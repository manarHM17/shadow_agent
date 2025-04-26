#include <iostream>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <grpcpp/grpcpp.h>
#include "monitoring.pb.h"
#include "monitoring.grpc.pb.h"
#include <amqp.h>
#include <amqp_tcp_socket.h>

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

void analyzeAndSendAlert(int32_t device_id, const std::string& json_str) {
    try {
        auto json = nlohmann::json::parse(json_str);
        std::cout << "[Serveur] Métriques reçues du device " << device_id << ": " << json_str << std::endl;

        MonitoringResponse resp;
        bool alert_needed = false;

        // Analyse CPU
        if (json.contains("cpu_usage")) {
            float cpu = parse_percentage(json["cpu_usage"]);
            if (cpu > 80.0) {
                resp.set_alert_message("Utilisation CPU élevée");
                resp.set_alert_level("WARNING");
                resp.set_recommended_action("Fermer les applications inutiles ou redémarrer le système");
                alert_needed = true;
            }
        }

        // Analyse mémoire
        if (json.contains("memory_usage")) {
            float mem = parse_percentage(json["memory_usage"]);
            if (mem > 80.0) {
                resp.set_alert_message("Utilisation mémoire élevée");
                resp.set_alert_level("WARNING");
                resp.set_recommended_action("Vérifier les processus actifs");
                alert_needed = true;
            }
        }

        // Analyse disque
        if (json.contains("disk_usage_root")) {
            float disk = parse_percentage(json["disk_usage_root"]);
            if (disk > 90.0) {
                resp.set_alert_message("Espace disque insuffisant");
                resp.set_alert_level("CRITICAL");
                resp.set_recommended_action("Nettoyer le disque ou ajouter plus d’espace");
                alert_needed = true;
            }
        }

        // Analyse réseau
        if (json.contains("network_status") && json["network_status"] == "unreachable") {
            resp.set_alert_message("Le réseau est injoignable");
            resp.set_alert_level("CRITICAL");
            resp.set_recommended_action("Vérifier la connexion réseau");
            alert_needed = true;
        }

        // Analyse services
        if (json.contains("services") && json["services"].is_object()) {
            auto services = json["services"];
            if (services.contains("mosquitto") && services["mosquitto"] == "inactive") {
                resp.set_alert_message("Mosquitto inactif");
                resp.set_alert_level("WARNING");
                resp.set_recommended_action("Redémarrer Mosquitto");
                alert_needed = true;
            } else if (services.contains("ssh") && services["ssh"] == "inactive") {
                resp.set_alert_message("SSH inactif");
                resp.set_alert_level("WARNING");
                resp.set_recommended_action("Redémarrer SSH");
                alert_needed = true;
            }
        }

        // Si une alerte est nécessaire
        if (alert_needed) {
            time_t now = time(nullptr);
            resp.set_timestamp(std::to_string(now));

            std::lock_guard<std::mutex> lock(mtx);
            if (active_writers.count(device_id)) {
                active_writers[device_id]->Write(resp);
                std::cout << ">> Alerte envoyée à " << device_id << " : " << resp.alert_message() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Erreur JSON : " << e.what() << std::endl;
    }
}
void consumeMetrics(int32_t device_id) {
    std::string queue_name = "metrics_" + std::to_string(device_id);
    amqp_connection_state_t conn = amqp_new_connection();
    amqp_socket_t* socket = amqp_tcp_socket_new(conn);
    if (!socket || amqp_socket_open(socket, "localhost", 5672)) {
        std::cerr << "[Thread-" << device_id << "] Échec connexion RabbitMQ\n";
        return;
    }

    if (amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest").reply_type != AMQP_RESPONSE_NORMAL) {
        std::cerr << "[Thread-" << device_id << "] Échec login\n";
        return;
    }

    amqp_channel_open(conn, 1);
    amqp_get_rpc_reply(conn);

    amqp_queue_declare(conn, 1, amqp_cstring_bytes(queue_name.c_str()), 0, 1, 0, 0, amqp_empty_table);
    amqp_basic_consume(conn, 1, amqp_cstring_bytes(queue_name.c_str()), amqp_empty_bytes, 0, 1, 0, amqp_empty_table);

    std::cout << "[Thread-" << device_id << "] Démarré pour consommer la file : " << queue_name << "\n";

    while (true) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (active_devices.find(device_id) == active_devices.end()) {
                std::cout << "[Thread-" << device_id << "] Fin de consommation, stream terminé.\n";
                break;
            }
        }

        amqp_envelope_t envelope;
        amqp_maybe_release_buffers(conn);
        auto res = amqp_consume_message(conn, &envelope, NULL, 0);

        if (res.reply_type == AMQP_RESPONSE_NORMAL) {
            std::string msg((char*)envelope.message.body.bytes, envelope.message.body.len);
            analyzeAndSendAlert(device_id, msg);
            amqp_destroy_envelope(&envelope);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn);
}

class MonitoringServiceImpl final : public MonitoringService::Service {
public:
    Status StreamMonitoringData(ServerContext* context,
                                const DeviceID* request,
                                ServerWriter<MonitoringResponse>* writer) override {
        int32_t device_id = request->device_id();
        std::cout << "[Serveur] Nouveau stream : device_id = " << device_id << std::endl;

        {
            std::lock_guard<std::mutex> lock(mtx);
            active_writers[device_id] = writer;
            active_devices.insert(device_id);
            if (consumer_threads.count(device_id) == 0) {
                consumer_threads[device_id] = std::thread(consumeMetrics, device_id);
            }
        }

        while (!context->IsCancelled()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        {
            std::lock_guard<std::mutex> lock(mtx);
            active_writers.erase(device_id);
            active_devices.erase(device_id);
        }

        std::cout << "[Serveur] Fin du stream : device_id = " << device_id << std::endl;
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
    server->Wait();
}

int main() {
    runServer();
    return 0;
}
