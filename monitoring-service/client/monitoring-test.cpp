#include <iostream>
#include <thread>
#include <random>
#include <chrono>
#include <nlohmann/json.hpp>
#include <grpcpp/grpcpp.h>
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include "monitoring.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientReader;
using monitoring::MonitoringService;
using monitoring::DeviceID;
using monitoring::MonitoringResponse;

void sendMetricsToRabbitMQ(int32_t device_id) {
    try {
        auto channel = AmqpClient::Channel::Create("localhost");
        std::string queue_name = "metrics_" + std::to_string(device_id);
        channel->DeclareQueue(queue_name, false, true, false, false);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> cpu_dist(20, 100);
        std::uniform_int_distribution<> mem_dist(30, 100);
        std::uniform_int_distribution<> disk_dist(50, 95);
        std::uniform_int_distribution<> net_dist(0, 1);
        std::uniform_int_distribution<> svc_dist(0, 1);

        while (true) {
            nlohmann::json metrics;
            metrics["cpu_usage"] = std::to_string(cpu_dist(gen)) + "%";
            metrics["memory_usage"] = std::to_string(mem_dist(gen)) + "%";
            metrics["disk_usage_root"] = std::to_string(disk_dist(gen)) + "%";
            metrics["network_status"] = (net_dist(gen) == 0) ? "ok" : "unreachable";

            nlohmann::json services;
            services["mosquitto"] = (svc_dist(gen) == 0) ? "active" : "inactive";
            services["ssh"] = (svc_dist(gen) == 0) ? "active" : "inactive";
            metrics["services"] = services;

            std::string message = metrics.dump();
            channel->BasicPublish("", queue_name, AmqpClient::BasicMessage::Create(message));

            //std::cout << "[RabbitMQ] ➤ Message envoyé à la file : " << queue_name << std::endl;
            //std::cout << "[RabbitMQ] ➤ Contenu : " << message << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(10));
        }

    } catch (std::exception& e) {
        std::cerr << "❌ Erreur envoi RabbitMQ : " << e.what() << std::endl;
    }
}

void receiveAlertsFromServer(std::shared_ptr<Channel> channel, int32_t device_id) {
    auto stub = MonitoringService::NewStub(channel);
    DeviceID request;
    request.set_device_id(device_id);

    ClientContext context;
    MonitoringResponse response;
    std::unique_ptr<ClientReader<MonitoringResponse>> reader = stub->StreamMonitoringData(&context, request);

    while (reader->Read(&response)) {
        std::cout << "\n📢 Alerte reçue:\n";
        std::cout << "  ➤ Message   : " << response.alert_message() << std::endl;
        std::cout << "  ➤ Niveau    : " << response.alert_level() << std::endl;
        std::cout << "  ➤ Action    : " << response.recommended_action() << std::endl;
        std::cout << "  ➤ Timestamp : " << response.timestamp() << std::endl;
    }

    Status status = reader->Finish();
    if (!status.ok()) {
        std::cerr << "❌ Stream terminé avec erreur : " << status.error_message() << std::endl;
    }
}

int main() {
    int32_t device_id;
    std::cout << "🖥️ Entrez l'ID de l'appareil : ";
    std::cin >> device_id;

    std::string server_address = "localhost:50051";
    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());

    // Thread pour l’envoi des métriques
    std::thread metricsThread(sendMetricsToRabbitMQ, device_id);

    // Thread pour recevoir les alertes du serveur
    std::thread grpcThread(receiveAlertsFromServer, channel, device_id);

    metricsThread.join();
    grpcThread.join();

    return 0;
}