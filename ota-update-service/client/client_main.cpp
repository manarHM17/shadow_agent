#include "ota_service.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <openssl/sha.h> 

class SimpleOTAClient {
private:
    std::unique_ptr<ota::OTAUpdateService::Stub> stub;
    int32_t device_id;
    bool running = false;

public:
    SimpleOTAClient(const std::string& server_address, int32_t device_id) 
        : device_id(device_id) {
        auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
        stub = ota::OTAUpdateService::NewStub(channel);
        
        std::cout << "OTA Client initialized with device_id: " << device_id << std::endl;
    }

    void StartPeriodicUpdateCheck(int period_minutes = 5) {
        running = true;
        std::thread([this, period_minutes]() {
            std::cout << "OTA Client started. Checking for updates every " << period_minutes << " minutes..." << std::endl;
            while (running) {
                this->CheckAndApplyUpdates();
                std::this_thread::sleep_for(std::chrono::minutes(period_minutes));
            }
        }).detach();
    }

    void Stop() {
        running = false;
    }
    
    void CheckAndApplyUpdates() {
        try {
            // 1. Vérifier les mises à jour
            ota::CheckUpdatesRequest request;
            request.set_device_id(device_id);
            request.set_current_version("1.0.0");
            
            ota::CheckUpdatesResponse response;
            grpc::ClientContext context;
            
            grpc::Status status = stub->CheckForUpdates(&context, request, &response);
            
            if (!status.ok()) {
                std::cerr << "Failed to check updates: " << status.error_message() << std::endl;
                return;
            }
            
            if (!response.has_updates()) {
                std::cout << "No updates available" << std::endl;
                return;
            }
            
            std::cout << "Found " << response.available_updates_size() << " updates" << std::endl;
            
            // 2. Télécharger et appliquer chaque mise à jour
            for (const auto& update : response.available_updates()) {
                std::cout << "Processing update: " << update.component_name() 
                         << " v" << update.version() << std::endl;
                
                if (DownloadAndApplyUpdate(update)) {
                    ReportStatus(update.component_name(), "SUCCESS", "");
                    std::cout << "Successfully applied update for " << update.component_name() << std::endl;
                } else {
                    ReportStatus(update.component_name(), "FAILED", "Application failed");
                    std::cerr << "Failed to apply update for " << update.component_name() << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in CheckAndApplyUpdates: " << e.what() << std::endl;
        }
    }
    
private:
    bool DownloadAndApplyUpdate(const ota::UpdateInfo& update) {
        try {
            // Télécharger
            ota::DownloadRequest dl_request;
            dl_request.set_device_id(device_id);
            dl_request.set_component_name(update.component_name());
            
            grpc::ClientContext context;
            auto reader = stub->DownloadUpdate(&context, dl_request);
            
            std::vector<char> file_data;
            ota::DownloadResponse chunk;
            
            std::cout << "Downloading " << update.component_name() << "..." << std::endl;
            
            while (reader->Read(&chunk)) {
                const std::string& data = chunk.data();
                file_data.insert(file_data.end(), data.begin(), data.end());
                
                // Afficher le progrès
                double progress = (double)chunk.current_size() / chunk.total_size() * 100;
                std::cout << "\rProgress: " << std::fixed << std::setprecision(1) 
                         << progress << "%" << std::flush;
            }
            std::cout << std::endl;
            
            grpc::Status status = reader->Finish();
            if (!status.ok()) {
                std::cerr << "Download failed: " << status.error_message() << std::endl;
                return false;
            }
            
            // Vérifier le checksum
            std::string calculated_checksum = CalculateChecksum(file_data);
            if (calculated_checksum != update.checksum()) {
                std::cerr << "Checksum mismatch!" << std::endl;
                return false;
            }
            
            // Appliquer la mise à jour
            return ApplyUpdate(update, file_data);
            
        } catch (const std::exception& e) {
            std::cerr << "Exception in DownloadAndApplyUpdate: " << e.what() << std::endl;
            return false;
        }
    }
    
    std::string GetRaspberryPiModel() {
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("Model") != std::string::npos) {
                return line.substr(line.find(":") + 2);
            }
        }
        return "unknown";
    }

    bool ApplyUpdate(const ota::UpdateInfo& update, const std::vector<char>& data) {
        try {
            // Backup current application
            std::string backup_dir = "/var/backups/ota/";
            std::filesystem::create_directories(backup_dir);
            
            std::string timestamp = std::to_string(std::time(nullptr));
            std::string backup_path = backup_dir + "app_" + timestamp + ".backup";
            
            if (std::filesystem::exists(update.target_path())) {
                std::filesystem::copy_file(
                    update.target_path(),
                    backup_path,
                    std::filesystem::copy_options::overwrite_existing
                );
            }

            // Stop service
            std::string stop_cmd = "sudo systemctl stop " + update.service_name();
            system(stop_cmd.c_str());

            // Apply update
            std::ofstream outfile(update.target_path(), std::ios::binary);
            if (!outfile) {
                std::cerr << "Failed to open target file" << std::endl;
                return false;
            }
            outfile.write(data.data(), data.size());
            outfile.close();

            // Set permissions
            std::string chmod_cmd = "chmod +x " + update.target_path();
            system(chmod_cmd.c_str());

            // Restart service
            std::string start_cmd = "sudo systemctl start " + update.service_name();
            system(start_cmd.c_str());

            // Verify service is running
            std::this_thread::sleep_for(std::chrono::seconds(2));
            if (system(("systemctl is-active " + update.service_name()).c_str()) != 0) {
                // Rollback if failed
                std::filesystem::copy_file(
                    backup_path,
                    update.target_path(),
                    std::filesystem::copy_options::overwrite_existing
                );
                system(start_cmd.c_str());
                return false;
            }

            return true;
        } catch (const std::exception& e) {
            std::cerr << "Update failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    void ReportStatus(const std::string& component, const std::string& status_str, 
                      const std::string& error_msg) {
        ota::StatusReport request;
        request.set_device_id(device_id);
        request.set_component_name(component);
        request.set_status(status_str);
        request.set_error_message(error_msg);
        
        ota::StatusResponse response;
        grpc::ClientContext context;
        
        stub->ReportStatus(&context, request, &response);
    }
    
    std::string CalculateChecksum(const std::vector<char>& data) {
        // Implémentation similaire à celle du serveur
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, data.data(), data.size());
        SHA256_Final(hash, &sha256);
        
        std::stringstream ss;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }
};

int main(int argc, char** argv) {
    std::string server_address = "localhost:50051";
    if (argc > 1) {
        server_address = argv[1];
    }

    int32_t device_id;
    std::cout << "Enter device_id (int): ";
    std::cin >> device_id;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    SimpleOTAClient client(server_address, device_id);
    client.StartPeriodicUpdateCheck(5);

    // Keep the main thread alive
    std::cout << "Press Ctrl+C to exit." << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::hours(24));
    }
    return 0;
}