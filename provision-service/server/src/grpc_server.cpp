#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include "../include/ProvisionServiceImpl.hpp"
#include <fstream>
using namespace std;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::SslServerCredentialsOptions;
using grpc::Status;
using namespace shadow_agent;

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

    // Commenter la partie SSL pour tester une connexion insecure
    /*
    string server_cert = LoadFile("/home/manar/IoT_shadow/services/server.crt"); // Chemin vers le certificat
    string server_key = LoadFile("/home/manar/IoT_shadow/services/server.key");  // Chemin vers la clé privée

    SslServerCredentialsOptions::PemKeyCertPair key_cert_pair = {server_key, server_cert};
    SslServerCredentialsOptions ssl_opts;
    ssl_opts.pem_key_cert_pairs.push_back(key_cert_pair);

    auto server_credentials = grpc::SslServerCredentials(ssl_opts);
    */

    // Utiliser une connexion insecure pour tester
    auto server_credentials = grpc::InsecureServerCredentials();

    ProvisionServiceImpl serviceProvision;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, server_credentials); // Utiliser les credentials insecure
    builder.RegisterService(&serviceProvision);

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Server listening on " << server_address << " with insecure connection" << endl;

    server->Wait();
    return 0;
}
