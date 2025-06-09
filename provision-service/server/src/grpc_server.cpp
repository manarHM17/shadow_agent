#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include "../include/ProvisionServiceImpl.h"
#include <fstream>
using namespace std;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::SslServerCredentialsOptions;
using grpc::Status;
using namespace provisioning;

// Fonction pour charger les fichiers de certificat et de clé
std::string LoadFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

int main() {
    std::cout << "Starting the server..." << std::endl;
    string server_address("0.0.0.0:50051");

    // Create shared dependencies
    auto db_manager = std::make_shared<DBHandler>();
    auto jwt_manager = std::make_shared<JWTUtils>();

    // Create service with dependencies
    ProvisioningServiceImpl serviceProvision(db_manager, jwt_manager);
    
    // Build and start server
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&serviceProvision);

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Server listening on " << server_address << " with insecure connection" << endl;

    server->Wait();
    return 0;
}
