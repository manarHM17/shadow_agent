#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <grpcpp/grpcpp.h>
#include "monitoring.grpc.pb.h"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// Variables globales pour la gestion de l'état du client
std::atomic<bool> shutdown_requested(false);
std::mutex mtx;
std::condition_variable cv;

// Chemin des logs - à adapter selon votre environnement
const std::string LOGS_DIRECTORY = "/home/manar/monitoring-service/client/logs";

// Configuration
const int METRICS_PUBLISH_INTERVAL_SEC = 30;     // Intervalle de publication des métriques
const int MAX_RECONNECT_ATTEMPTS = 5;            // Nombre max de tentatives de reconnexion
const int RECONNECT_DELAY_BASE_SEC = 2;          // Délai de base entre tentatives (backoff exponentiel)

/**
 * Trouve le fichier JSON le plus récent avec un préfixe donné dans un répertoire
 */
std::string getLatestJsonFile(const std::string& directory, const std::string& prefix) {
    try {
        std::string latest_file;
        auto latest_time = fs::file_time_type::min();
        
        if (!fs::exists(directory) || !fs::is_directory(directory)) {
            std::cerr << "[Client] Erreur: Le répertoire " << directory << " n'existe pas.\n";
            return "";
        }

        for (const auto& entry : fs::directory_iterator(directory)) {
            try {
                if (entry.is_regular_file() && 
                    entry.path().extension() == ".json" &&
                    entry.path().filename().string().find(prefix) != std::string::npos) {
                    auto ftime = fs::last_write_time(entry);
                    if (ftime > latest_time) {
                        latest_time = ftime;
                        latest_file = entry.path().string();
                    }
                }
            } catch (const fs::filesystem_error& e) {
                std::cerr << "[Client] Erreur lors de l'accès au fichier " << entry.path().string() 
                          << ": " << e.what() << "\n";
            }
        }

        if (latest_file.empty()) {
            std::cerr << "[Client] Aucun fichier avec le préfixe '" << prefix << "' trouvé dans " << directory << "\n";
        } else {
            std::cout << "[Client] Fichier le plus récent trouvé: " << latest_file << "\n";
        }

        return latest_file;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "[Client] Erreur lors de la recherche de fichiers: " << e.what() << "\n";
        return "";
    }
}

/**
 * Lit le contenu d'un fichier avec gestion d'erreurs
 */
std::string readFileContent(const std::string& file_path) {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "[Client] Erreur: Impossible d'ouvrir le fichier " << file_path << "\n";
            return "";
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        // Vérifier si le JSON est valide
        auto content = buffer.str();
        try {
            nlohmann::json::parse(content);
            return content;
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "[Client] Erreur: Le fichier " << file_path << " contient un JSON invalide: " 
                      << e.what() << "\n";
            return "";
        }
    } catch (const std::exception& e) {
        std::cerr << "[Client] Erreur lors de la lecture du fichier " << file_path << ": " << e.what() << "\n";
        return "";
    }
}

/**
 * Crée un canal RabbitMQ avec gestion des tentatives de reconnexion
 */
AmqpClient::Channel::ptr_t createRabbitMQChannel() {
    int attempts = 0;
    while (attempts < MAX_RECONNECT_ATTEMPTS && !shutdown_requested) {
        try {
            AmqpClient::Channel::OpenOpts opts;
            opts.host = "localhost";
            opts.port = 5672;
            opts.auth = AmqpClient::Channel::OpenOpts::BasicAuth{"guest", "guest"};
            auto channel = AmqpClient::Channel::Open(opts);
            
            // S'assurer que les queues existent
            channel->DeclareQueue("hardware_metrics_queue", false, true, false, false);
            channel->DeclareQueue("software_metrics_queue", false, true, false, false);
            
            std::cout << "[Client] Connexion à RabbitMQ établie avec succès\n";
            return channel;
        } catch (const std::exception& e) {
            attempts++;
            int delay = RECONNECT_DELAY_BASE_SEC * (1 << (attempts - 1)); // Backoff exponentiel
            std::cerr << "[Client] Tentative " << attempts << "/" << MAX_RECONNECT_ATTEMPTS 
                      << " de connexion à RabbitMQ échouée: " << e.what() 
                      << ". Nouvelle tentative dans " << delay << " secondes...\n";
            
            std::this_thread::sleep_for(std::chrono::seconds(delay));
        }
    }
    
    throw std::runtime_error("Impossible de se connecter à RabbitMQ après plusieurs tentatives");
}

/**
 * Thread qui publie périodiquement les métriques dans RabbitMQ
 */
void publishMetrics() {
    std::cout << "[Client] Démarrage du thread de publication des métriques\n";
    
    while (!shutdown_requested) {
        try {
            auto channel = createRabbitMQChannel();
            
            // Boucle principale de publication
            while (!shutdown_requested) {
                try {
                    // Chercher les derniers fichiers JSON
                    std::string hardware_file = getLatestJsonFile(LOGS_DIRECTORY, "hardware_metrics_");
                    std::string software_file = getLatestJsonFile(LOGS_DIRECTORY, "software_metrics_");

                    if (!hardware_file.empty()) {
                        std::string hardware_json = readFileContent(hardware_file);
                        if (!hardware_json.empty()) {
                            channel->BasicPublish("", "hardware_metrics_queue", 
                                                 AmqpClient::BasicMessage::Create(hardware_json));
                            std::cout << "[Client] Métriques hardware publiées dans RabbitMQ.\n";
                        }
                    }

                    if (!software_file.empty()) {
                        std::string software_json = readFileContent(software_file);
                        if (!software_json.empty()) {
                            channel->BasicPublish("", "software_metrics_queue", 
                                                 AmqpClient::BasicMessage::Create(software_json));
                            std::cout << "[Client] Métriques software publiées dans RabbitMQ.\n";
                        }
                    }

                    // Attendre avant la prochaine publication
                    for (int i = 0; i < METRICS_PUBLISH_INTERVAL_SEC && !shutdown_requested; i++) {
                        std::this_thread::sleep_for(1s);
                    }
                } catch (const AmqpClient::ChannelException& e) {
                    std::cerr << "[Client] Erreur RabbitMQ: " << e.what() << ". Reconnexion...\n";
                    break; // Sortir de la boucle interne pour recréer le canal
                } catch (const std::exception& e) {
                    std::cerr << "[Client] Erreur lors de la publication: " << e.what() << "\n";
                    std::this_thread::sleep_for(5s);
                }
            }
        } catch (const std::exception& e) {
            if (!shutdown_requested) {
                std::cerr << "[Client] Erreur critique: " << e.what() << "\n";
                std::this_thread::sleep_for(10s);
            }
        }
    }
    
    std::cout << "[Client] Thread de publication terminé\n";
}

/**
 * Thread qui écoute les alertes via gRPC
 */
void listenGrpcAlerts() {
    std::cout << "[Client] Démarrage du thread d'écoute des alertes\n";
    
    while (!shutdown_requested) {
        try {
            // Créer le stub gRPC
            auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
            auto stub = monitoring::MonitoringService::NewStub(channel);
            
            monitoring::DeviceID request;
            request.set_device_id(1);  // ID de l'appareil

            std::cout << "[Client] Connexion au serveur de monitoring...\n";
            
            // Attendre que le serveur soit disponible
            auto state = channel->GetState(true);
            if (state != GRPC_CHANNEL_READY) {
                auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
                if (!channel->WaitForConnected(deadline)) {
                    std::cerr << "[Client] Serveur gRPC non disponible. Nouvelle tentative dans 5 secondes...\n";
                    std::this_thread::sleep_for(5s);
                    continue;
                }
            }
            
            grpc::ClientContext context;
            monitoring::MonitoringResponse response;
            std::cout << "[Client] Début du streaming des alertes...\n";
            
            auto reader = stub->StreamMonitoringData(&context, request);
            
            // Lire les alertes en continu
            while (reader->Read(&response)) {
                std::cout << "\n[ALERTE] " << response.alert_message()
                          << " | Niveau: " << response.alert_level()
                          << " | Action: " << response.recommended_action()
                          << " | Timestamp: " << response.timestamp() << "\n\n";
            }
            
            auto status = reader->Finish();
            if (!status.ok()) {
                std::cerr << "[Client] Erreur gRPC: " << status.error_message() << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "[Client] Erreur de connexion gRPC: " << e.what() << "\n";
        }
        
        if (!shutdown_requested) {
            std::cerr << "[Client] Connexion au serveur perdue. Reconnexion dans 5 secondes...\n";
            std::this_thread::sleep_for(5s);
        }
    }
    
    std::cout << "[Client] Thread d'écoute terminé\n";
}

/**
 * Gestionnaire de signaux pour arrêt propre
 */
void handleSignals() {
    std::string input;
    while (true) {
        std::getline(std::cin, input);
        if (input == "quit" || input == "exit") {
            std::cout << "[Client] Arrêt demandé, fermeture en cours...\n";
            shutdown_requested = true;
            cv.notify_all();
            break;
        }
    }
}

int main() {
    try {
        std::cout << "[Client] Démarrage du client de monitoring...\n";
        
        // Vérifier l'existence du répertoire de logs
        if (!fs::exists(LOGS_DIRECTORY)) {
            std::cerr << "[Client] ERREUR: Le répertoire de logs " << LOGS_DIRECTORY << " n'existe pas!\n";
            std::cerr << "[Client] Veuillez créer le répertoire ou modifier le chemin dans le code.\n";
            return 1;
        }
        
        // Démarrer les threads
        std::thread signal_thread(handleSignals);
        std::thread publisher(publishMetrics);
        std::thread alertListener(listenGrpcAlerts);
        
        // Attendre la fin des threads
        if (publisher.joinable()) {
            publisher.join();
        }
        
        if (alertListener.joinable()) {
            alertListener.join();
        }
        
        if (signal_thread.joinable()) {
            signal_thread.join();
        }
        
        std::cout << "[Client] Arrêt normal du client\n";
    } catch (const std::exception& e) {
        std::cerr << "[Client] Erreur fatale: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}