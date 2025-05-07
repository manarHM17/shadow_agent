#include <iostream>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <grpcpp/grpcpp.h>
#include "monitoring.pb.h"
#include "monitoring.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;
using monitoring::MonitoringService;
using monitoring::DeviceID;
using monitoring::MonitoringResponse;

std::mutex mtx;
std::map<int32_t, ServerWriter<MonitoringResponse>*> active_writers;
std::map<int32_t, std::thread> consumer_threads;
std::unordered_set<int32_t> active_devices;
std::condition_variable cv;
std::atomic<bool> shutdown_requested(false);

// Fonction utilitaire pour parser un pourcentage (ex: "78%" → 78.0f)
float parse_percentage(const nlohmann::json& value) {
    if (value.is_string()) {
        std::string val = value.get<std::string>();
        size_t pos = val.find('%');
        if (pos != std::string::npos) {
            val.erase(pos);
        }
        try {
            return std::stof(val);
        } catch (...) {
            return 0.0f;
        }
    } else if (value.is_number()) {
        return value.get<float>();
    }
    return 0.0f;
}

void analyzeAndSendAlert(int32_t device_id, const std::string& json_str, const std::string& queue) {
    try {
        auto json = nlohmann::json::parse(json_str);
        std::cout << "[Serveur] Métriques reçues du device " << device_id << " depuis " << queue << ": " << json_str << std::endl;

        MonitoringResponse resp;
        bool alert_needed = false;

        // Log the values of all relevant metrics
        std::cout << "[DEBUG] Analyse pour device " << device_id << ":\n";
        
        // Analyse CPU
        if (json.contains("cpu_usage")) {
            float cpu = parse_percentage(json["cpu_usage"]);
            std::cout << "  ➤ CPU Usage: " << cpu << "% (Threshold: >80%)\n";
            if (cpu > 80.0) {
                resp.set_alert_message("Utilisation CPU élevée");
                resp.set_alert_level("WARNING");
                resp.set_recommended_action("Fermer les applications inutiles ou redémarrer le système");
                alert_needed = true;
            }
        } else {
            std::cout << "  ➤ CPU Usage: Not present in JSON\n";
        }

        // Analyse mémoire
        if (json.contains("memory_usage")) {
            float mem = parse_percentage(json["memory_usage"]);
            std::cout << "  ➤ Memory Usage: " << mem << "% (Threshold: >80%)\n";
            if (mem > 80.0) {
                resp.set_alert_message("Utilisation mémoire élevée");
                resp.set_alert_level("WARNING");
                resp.set_recommended_action("Vérifier les processus actifs");
                alert_needed = true;
            }
        } else {
            std::cout << "  ➤ Memory Usage: Not present in JSON\n";
        }

        // Analyse disque
        if (json.contains("disk_usage_root")) {
            float disk = parse_percentage(json["disk_usage_root"]);
            std::cout << "  ➤ Disk Usage: " << disk << "% (Threshold: >90%)\n";
            if (disk > 90.0) {
                resp.set_alert_message("Espace disque insuffisant");
                resp.set_alert_level("CRITICAL");
                resp.set_recommended_action("Nettoyer le disque ou ajouter plus d'espace");
                alert_needed = true;
            }
        } else {
            std::cout << "  ➤ Disk Usage: Not present in JSON\n";
        }

        // Analyse réseau
        if (json.contains("network_status")) {
            std::string network_status = json["network_status"];
            std::cout << "  ➤ Network Status: " << network_status << " (Trigger if: unreachable)\n";
            if (network_status == "unreachable") {
                resp.set_alert_message("Le réseau est injoignable");
                resp.set_alert_level("CRITICAL");
                resp.set_recommended_action("Vérifier la connexion réseau");
                alert_needed = true;
            }
        } else {
            std::cout << "  ➤ Network Status: Not present in JSON\n";
        }

        // Analyse services
        if (json.contains("services") && json["services"].is_object()) {
            auto services = json["services"];
            if (services.contains("mosquitto")) {
                std::string mosquitto_status = services["mosquitto"];
                std::cout << "  ➤ Mosquitto Status: " << mosquitto_status << " (Trigger if: inactive)\n";
                if (mosquitto_status == "inactive") {
                    resp.set_alert_message("Mosquitto inactif");
                    resp.set_alert_level("WARNING");
                    resp.set_recommended_action("Redémarrer Mosquitto");
                    alert_needed = true;
                }
            }
            if (services.contains("ssh")) {
                std::string ssh_status = services["ssh"];
                std::cout << "  ➤ SSH Status: " << ssh_status << " (Trigger if: inactive)\n";
                if (ssh_status == "inactive") {
                    resp.set_alert_message("SSH inactif");
                    resp.set_alert_level("WARNING");
                    resp.set_recommended_action("Redémarrer SSH");
                    alert_needed = true;
                }
            }
        } else {
            std::cout << "  ➤ Services: Not present in JSON\n";
        }

        // Si une alerte est nécessaire
        if (alert_needed) {
            time_t now = time(nullptr);
            resp.set_timestamp(std::to_string(now));

            std::lock_guard<std::mutex> lock(mtx);
            if (active_writers.count(device_id)) {
                if (active_writers[device_id]->Write(resp)) {
                    std::cout << ">> Alerte envoyée à " << device_id << " : " << resp.alert_message() << std::endl;
                } else {
                    std::cerr << ">> Échec d'envoi d'alerte à " << device_id << std::endl;
                }
            }
        } else {
            std::cout << "[Serveur] Aucune alerte générée pour device " << device_id << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Serveur] Erreur JSON : " << e.what() << std::endl;
    }
}

// Fonction pour créer un nouveau canal RabbitMQ avec gestion des erreurs
AmqpClient::Channel::ptr_t createChannel() {
    const int MAX_RETRIES = 5;
    int retry_count = 0;
    
    while (retry_count < MAX_RETRIES) {
        try {
            AmqpClient::Channel::OpenOpts opts;
            opts.host = "localhost";
            opts.port = 5672;
            opts.auth = AmqpClient::Channel::OpenOpts::BasicAuth{"guest", "guest"};
            auto channel = AmqpClient::Channel::Open(opts);
            return channel;
        } catch (const std::exception& e) {
            retry_count++;
            std::cerr << "[RabbitMQ] Tentative de connexion " << retry_count << " échouée: " << e.what() << std::endl;
            if (retry_count >= MAX_RETRIES) {
                throw;
            }
            // Attente exponentielle avant nouvelle tentative
            std::this_thread::sleep_for(std::chrono::seconds(1 << retry_count));
        }
    }
    throw std::runtime_error("Impossible de se connecter à RabbitMQ après plusieurs tentatives");
}

void consumeMetrics(const std::string& queue, int32_t device_id) {
    try {
        // Flag pour suivre si nous sommes toujours actifs
        bool is_active = true;
        
        while (is_active && !shutdown_requested) {
            // Vérifier si ce device est toujours actif
            {
                std::lock_guard<std::mutex> lock(mtx);
                is_active = (active_devices.find(device_id) != active_devices.end());
            }

            if (!is_active) {
                std::cout << "[Thread-" << device_id << "] Fin de consommation, stream terminé.\n";
                break;
            }
            
            try {
                // Création d'un nouveau canal pour chaque cycle de consommation
                // pour éviter les problèmes avec les delivery tags
                auto channel = createChannel();
                
                // S'assurer que la file existe
                channel->DeclareQueue(queue, false, true, false, false);
                std::cout << "[Thread-" << device_id << "] Connecté à la file : " << queue << "\n";
                
                // Utiliser noAck=true pour éviter d'avoir à acquitter explicitement les messages
                // Cela évite les erreurs PRECONDITION_FAILED - unknown delivery tag
                std::string consumer_tag = channel->BasicConsume(queue, "", true);  // noAck=true
                
                // Durée maximale de consommation sur ce canal avant d'en créer un nouveau
                const auto MAX_CONSUME_DURATION = std::chrono::minutes(5);
                auto start_time = std::chrono::steady_clock::now();
                
                // Boucle de consommation de messages
                while (is_active && !shutdown_requested) {
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        is_active = (active_devices.find(device_id) != active_devices.end());
                    }
                    
                    if (!is_active) break;
                    
                    // Vérifier si on doit rafraîchir la connexion
                    auto now = std::chrono::steady_clock::now();
                    if ((now - start_time) > MAX_CONSUME_DURATION) {
                        std::cout << "[Thread-" << device_id << "] Rafraîchissement périodique de la connexion\n";
                        break;  // Sortir de la boucle pour recréer un canal
                    }
                    
                    try {
                        // Essayer de consommer un message avec un timeout
                        AmqpClient::Envelope::ptr_t envelope;
                        bool message_received = channel->BasicConsumeMessage(consumer_tag, envelope, 500);
                        
                        if (message_received) {
                            std::string msg = envelope->Message()->Body();
                            
                            // Analyser et envoyer l'alerte si nécessaire
                            analyzeAndSendAlert(device_id, msg, queue);
                            
                            // Pas besoin de BasicAck car noAck=true dans BasicConsume
                        }
                    } catch (const AmqpClient::ChannelException& e) {
                        std::cerr << "[Thread-" << device_id << "] Erreur de canal RabbitMQ : " << e.what() << std::endl;
                        break;  // Sortir de la boucle pour recréer un canal
                    }
                }
                
                // Nettoyage avant de recréer un canal
                try {
                    channel->BasicCancel(consumer_tag);
                } catch (...) {
                    // Ignorer les erreurs lors de l'annulation
                }
                
            } catch (const std::exception& e) {
                std::cerr << "[Thread-" << device_id << "] Erreur dans ConsumeMetrics : " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));  // Attendre avant de réessayer
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Thread-" << device_id << "] Erreur fatale dans ConsumeMetrics : " << e.what() << std::endl;
    }
    
    std::cout << "[Thread-" << device_id << "] Thread consommateur terminé." << std::endl;
}

class MonitoringServiceImpl final : public MonitoringService::Service {
public:
    Status StreamMonitoringData(ServerContext* context,
                               const DeviceID* request,
                               ServerWriter<MonitoringResponse>* writer) override {
        int32_t device_id = request->device_id();
        std::cout << "[Serveur] Nouveau stream : device_id = " << device_id << std::endl;

        // Mutex pour protéger les structures partagées
        std::unique_lock<std::mutex> lock(mtx);
        active_writers[device_id] = writer;
        active_devices.insert(device_id);
        
        // Créer des threads consommateurs si nécessaire
        if (consumer_threads.count(device_id) == 0) {
            // Créer un thread pour chaque queue
            consumer_threads[device_id] = std::thread(consumeMetrics, "software_metrics_queue", device_id);
            consumer_threads[device_id + 1000] = std::thread(consumeMetrics, "hardware_metrics_queue", device_id);
        }
        lock.unlock();

        // Rester actif tant que le client est connecté
        while (!context->IsCancelled() && !shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Nettoyage lors de la déconnexion
        lock.lock();
        std::cout << "[Serveur] Fin du stream : device_id = " << device_id << std::endl;
        active_writers.erase(device_id);
        active_devices.erase(device_id);
        
        // Signaler que le device est inactif pour que les threads se terminent
        cv.notify_all();
        lock.unlock();
        
        // Attendre que les threads se terminent
        if (consumer_threads.count(device_id)) {
            if (consumer_threads[device_id].joinable()) {
                consumer_threads[device_id].join();
            }
            consumer_threads.erase(device_id);
        }
        
        if (consumer_threads.count(device_id + 1000)) {
            if (consumer_threads[device_id + 1000].joinable()) {
                consumer_threads[device_id + 1000].join();
            }
            consumer_threads.erase(device_id + 1000);
        }

        return Status::OK;
    }
};

void runServer() {
    std::string server_address("0.0.0.0:50051");
    MonitoringServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[Serveur] Écoute sur : " << server_address << std::endl;
    
    // Signal handler pour arrêter proprement
    std::thread signal_thread([&server]() {
        std::string input;
        while (true) {
            std::getline(std::cin, input);
            if (input == "quit" || input == "exit") {
                std::cout << "[Serveur] Arrêt demandé, fermeture en cours..." << std::endl;
                shutdown_requested = true;
                cv.notify_all();
                server->Shutdown();
                break;
            }
        }
    });
    
    server->Wait();
    
    if (signal_thread.joinable()) {
        signal_thread.join();
    }
}

int main() {
    try {
        runServer();
    } catch (const std::exception& e) {
        std::cerr << "Erreur fatale: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}