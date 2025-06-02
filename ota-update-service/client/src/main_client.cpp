#include "../include/ota_client.h"
#include "../include/update_manager.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

std::atomic<bool> running{true};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ". Shutting down gracefully..." << std::endl;
    running = false;
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <server_address> <device_id> [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --check-interval <seconds>  Check for updates interval (default: 300)\n";
    std::cout << "  --one-shot                  Check once and exit\n";
    std::cout << "  --manual-update <type> <file> <target>  Manual update\n";
    std::cout << "    Types: config, app, service\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " localhost:50051 device001\n";
    std::cout << "  " << program_name << " localhost:50051 device001 --check-interval 60\n";
    std::cout << "  " << program_name << " localhost:50051 device001 --one-shot\n";
    std::cout << "  " << program_name << " localhost:50051 device001 --manual-update config /tmp/new_config.json /etc/app/config.json\n";
}

int main(int argc, char* argv[]) {
    // Installation des gestionnaires de signaux
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string server_address = argv[1];
    std::string device_id = argv[2];
    
    // Parse command line arguments
    int check_interval = 300; // 5 minutes par défaut
    bool one_shot = false;
    bool manual_update = false;
    std::string update_type, update_file, update_target;
    
    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--check-interval" && i + 1 < argc) {
            check_interval = std::stoi(argv[++i]);
        } else if (arg == "--one-shot") {
            one_shot = true;
        } else if (arg == "--manual-update" && i + 3 < argc) {
            manual_update = true;
            update_type = argv[++i];
            update_file = argv[++i];
            update_target = argv[++i];
        }
    }
    
    std::cout << "OTA Client Starting..." << std::endl;
    std::cout << "Server: " << server_address << std::endl;
    std::cout << "Device ID: " << device_id << std::endl;
    
    // Initialiser l'UpdateManager avec des callbacks
    ota::UpdateManager update_manager;
    
    // Callback pour le progrès
    update_manager.setProgressCallback([](int progress) {
        std::cout << "Progress: " << progress << "%" << std::endl;
    });
    
    // Callback pour les logs
    update_manager.setLogCallback([](const std::string& message) {
        std::cout << "[LOG] " << message << std::endl;
    });
    
    // Mode manuel
    if (manual_update) {
        std::cout << "Manual update mode: " << update_type << std::endl;
        
        bool success = false;
        if (update_type == "config") {
            success = update_manager.installConfigUpdate(update_file, update_target);
        } else if (update_type == "app") {
            success = update_manager.installApplicationUpdate(update_file, update_target);
        } else if (update_type == "service") {
            success = update_manager.installSystemdServiceUpdate(update_file, update_target);
        } else {
            std::cerr << "ERROR: Unknown update type: " << update_type << std::endl;
            return 1;
        }
        
        if (success) {
            std::cout << "Manual update completed successfully!" << std::endl;
            return 0;
        } else {
            std::cout << "Manual update failed!" << std::endl;
            return 1;
        }
    }
    
    // Initialiser le client OTA
    ota::OTAClient client(server_address);
    
    if (!client.Connect()) {
        std::cerr << "ERROR: Failed to connect to OTA server" << std::endl;
        return 1;
    }
    
    std::cout << "Connected to OTA server successfully" << std::endl;
    
    // Enregistrer le périphérique
    if (!client.RegisterDevice(device_id, "1.0.0")) {
        std::cerr << "ERROR: Failed to register device" << std::endl;
        return 1;
    }
    
    std::cout << "Device registered successfully" << std::endl;
    
    // Boucle principale
    while (running) {
        try {
            std::cout << "Checking for updates..." << std::endl;
            
            // Vérifier les mises à jour disponibles
            auto updates = client.CheckForUpdates(device_id);
            
            if (updates.empty()) {
                std::cout << "No updates available" << std::endl;
            } else {
                std::cout << "Found " << updates.size() << " update(s) available" << std::endl;
                
                // Traiter chaque mise à jour
                for (const auto& update : updates) {
                    std::cout << "Processing update: " << update.package_name 
                              << " v" << update.version << std::endl;
                    
                    // Télécharger la mise à jour
                    std::string download_path = "/tmp/" + update.package_name + "_" + update.version + ".update";
                    
                    if (!client.DownloadUpdate(update.update_id, download_path)) {
                        std::cerr << "ERROR: Failed to download update " << update.update_id << std::endl;
                        continue;
                    }
                    
                    std::cout << "Update downloaded to: " << download_path << std::endl;
                    
                    // Vérifier le checksum
                    if (!update_manager.verifyChecksum(download_path, update.checksum)) {
                        std::cerr << "ERROR: Checksum verification failed for update " << update.update_id << std::endl;
                        continue;
                    }
                    
                    // Installer la mise à jour selon le type
                    bool install_success = false;
                    
                    if (update.update_type == "CONFIG") {
                        std::string target_path = "/etc/" + update.package_name + ".json";
                        install_success = update_manager.installConfigUpdate(download_path, target_path);
                    } else if (update.update_type == "APPLICATION") {
                        install_success = update_manager.installApplicationUpdate(download_path, update.package_name);
                    } else if (update.update_type == "SERVICE") {
                        install_success = update_manager.installSystemdServiceUpdate(download_path, update.package_name);
                    } else {
                        std::cerr << "ERROR: Unknown update type: " << update.update_type << std::endl;
                        continue;
                    }
                    
                    // Rapporter le statut de l'installation
                    if (install_success) {
                        std::cout << "Update installed successfully: " << update.package_name << std::endl;
                        client.ReportInstallationStatus(update.update_id, "SUCCESS", "Update installed successfully");
                    } else {
                        std::cerr << "ERROR: Failed to install update: " << update.package_name << std::endl;
                        client.ReportInstallationStatus(update.update_id, "FAILED", "Installation failed");
                    }
                    
                    // Nettoyer le fichier temporaire
                    std::remove(download_path.c_str());
                }
            }
            
            if (one_shot) {
                std::cout << "One-shot mode: exiting after check" << std::endl;
                break;
            }
            
            // Attendre avant la prochaine vérification
            std::cout << "Waiting " << check_interval << " seconds before next check..." << std::endl;
            
            for (int i = 0; i < check_interval && running; i++) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Exception in main loop: " << e.what() << std::endl;
            
            // Attendre avant de retry
            for (int i = 0; i < 30 && running; i++) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    
    std::cout << "OTA Client shutting down..." << std::endl;
    client.Disconnect();
    
    return 0;
}