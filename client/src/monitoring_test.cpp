#include <iostream>
#include <filesystem>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <iomanip>
#include <grpcpp/grpcpp.h>
#include "monitoring.pb.h"
#include "monitoring.grpc.pb.h"
#include <SimpleAmqpClient/SimpleAmqpClient.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using monitoring::MonitoringService;
using monitoring::Empty;
using monitoring::MonitoringResponse;
namespace fs = std::filesystem;

# obtenir le dernier fichier json 
std::string getLatestJsonFile(const std::string& directory) {
    std::string latestFile;
    std::chrono::system_clock::time_point latestTime;

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.path().extension() == ".json") {
            auto ftime = fs::last_write_time(entry);
            if (latestFile.empty() || ftime > latestTime) {
                latestTime = ftime;
                latestFile = entry.path().string();
            }
        }
    }

    return latestFile;
}


void sendToRabbitMQ(const std::string& message) {
    amqp_connection_state_t conn = amqp_new_connection();
    amqp_socket_t* socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        std::cerr << "Erreur lors de la création du socket RabbitMQ" << std::endl;
        return;
    }

    if (amqp_socket_open(socket, "localhost", 5672)) {
        std::cerr << "Erreur de connexion au broker RabbitMQ" << std::endl;
        return;
    }

    amqp_rpc_reply_t login_reply = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");
    if (login_reply.reply_type != AMQP_RESPONSE_NORMAL) {
        std::cerr << "Erreur de connexion RabbitMQ : " << amqp_error_string(login_reply.reply_type) << std::endl;
        return;
    }
    

    amqp_channel_open(conn, 1);
    amqp_get_rpc_reply(conn);

    amqp_basic_publish(conn, 1, amqp_cstring_bytes(""), amqp_cstring_bytes("time_queue"), 0, 0, NULL, amqp_cstring_bytes(message.c_str()));

    amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn);
}

void periodicSender() {
    while (true) {
        std::time_t now = std::time(nullptr);
        std::string time_str = std::ctime(&now);
        time_str.pop_back();  // Enlever le saut de ligne de la fin de la chaîne
        sendToRabbitMQ(time_str);
        std::cout << "[Client] Heure envoyée : " << time_str << std::endl;
        std::this_thread::sleep_for(std::chrono::minutes(1));  // Envoi toutes les minutes
    }
}

void startGrpcStream(std::shared_ptr<Channel> channel) {
    std::unique_ptr<TimeService::Stub> stub = TimeService::NewStub(channel);
    Empty request;
    ClientContext context;
    std::unique_ptr<grpc::ClientReader<DayStateResponse>> reader(stub->StreamDayState(&context, request));
    
    DayStateResponse response;
    while (reader->Read(&response)) {
        std::cout << "[Client] 🔔 État de la journée changé : " << response.state() << std::endl;
    }
    Status status = reader->Finish();
    if (!status.ok()) {
        std::cerr << "Stream terminé avec erreur : " << status.error_message() << std::endl;
    }
}

int main() {
    // Démarrer un thread pour l'envoi périodique des données à RabbitMQ
    std::thread senderThread(periodicSender);
    
    // Connexion et démarrage du stream gRPC
    string server_ip = "localhost:50051";

    // Utiliser une connexion insecure pour tester
    auto channel = grpc::CreateChannel(server_ip, grpc::InsecureChannelCredentials());
    ProvisionClient client(channel);

    // Joindre le thread d'envoi
    senderThread.join();
    return 0;
}
