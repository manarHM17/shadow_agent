#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include "mender_client.hpp"
#include "ota_service_impl.hpp"

void RunServer(const std::string& server_address, 
               const std::string& mender_url,
               const std::string& tenant_token) {
    // Créer le client Mender
    auto mender_client = std::make_shared<MenderClient>(mender_url, tenant_token);
    
    // Créer l'implémentation du service OTA
    OTAServiceImpl service(mender_client);
    
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    
    grpc::ServerBuilder builder;
    
    // Configurer les options de sécurité (SSL/TLS)
    grpc::SslServerCredentialsOptions ssl_opts;
    ssl_opts.pem_root_certs = "";
    
    grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp;
    // Charger les certificats et clés à partir de fichiers
    std::ifstream cert_file("server.crt");
    std::string server_cert((std::istreambuf_iterator<char>(cert_file)),
                             std::istreambuf_iterator<char>());
    
    std::ifstream key_file("server.key");
    std::string server_key((std::istreambuf_iterator<char>(key_file)),
                            std::istreambuf_iterator<char>());
    
    pkcp.private_key = server_key;
    pkcp.cert_chain = server_cert;
    ssl_opts.pem_key_cert_pairs.push_back(pkcp);
    
    // Ajouter l'adresse d'écoute avec les credentials SSL
    builder.AddListeningPort(server_address, grpc::SslServerCredentials(ssl_opts));
    
    // Enregistrer le service
    builder.RegisterService(&service);
    
    // Construire et démarrer le serveur
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    
    // Attendre que le serveur se termine
    server->Wait();
}

int main(int argc, char** argv) {
    // Configuration par défaut
    std::string server_address = "0.0.0.0:50051";
    std::string mender_url = "https://localhost:443";
    std::string tenant_token = "eyJhbGciOiJSUzI1NiIsImtpZCI6MCwidHlwIjoiSldUIn0.eyJqdGkiOiIxNjE1ZTRjYy04ZDgwLTQ3YzQtODU1MC1lY2QzMTMzYzQzZjciLCJzdWIiOiIzNDE2YWFhYi04NjViLTQwNGEtOTVhNy1lNWZhN2ZhYzlkZjEiLCJleHAiOjE3Nzg1MDE4MjgsImlhdCI6MTc0Njk2NTgyOCwibWVuZGVyLnVzZXIiOnRydWUsImlzcyI6Im1lbmRlci51c2VyYWRtIiwic2NwIjoibWVuZGVyLioiLCJuYmYiOjE3NDY5NjU4Mjh9.TL8zcBaWncl2DsEDfUDwMUpLavVEXp3HqA_7dX_PQk0mpV__LZ8beTwA6ztSR9NNKCupPZ6MfD85sT37OybAebMO1cmehcdaI0mHYedWsB3AHqR4KDeARr168akR4kEMTWpSXexHfyPy72GxNEAq-nkjWIIndpOuSoh2GMrIH-EqDuw8v_VPZx-LTTvcpfz4-6ic0adnx8QNwaHtPwusaCsdQ91bLXLRgRHvNcgF_Drlyrk5ld7ipCW6w8A_fCACB4Sea60XdQG0DeLZawRtyCheojlgEU795c7b-wIoBLYU9xkaYuNoPmxrpCEO0x-MmtimyBwAbkpejpolebw90w";
    
    // Analyser les arguments de ligne de commande
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--address" && i + 1 < argc) {
            server_address = argv[++i];
        } else if (arg == "--mender-url" && i + 1 < argc) {
            mender_url = argv[++i];
        } else if (arg == "--tenant-token" && i + 1 < argc) {
            tenant_token = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n"
                      << "Options:\n"
                      << "  --address ADDRESS       Server address (default: 0.0.0.0:50051)\n"
                      << "  --mender-url URL        Mender server URL (default: https://hosted.mender.io)\n"
                      << "  --tenant-token TOKEN    Mender tenant token\n"
                      << "  --help                  Show this help message\n";
            return 0;
        }
    }
    
    RunServer(server_address, mender_url, tenant_token);
    
    return 0;
}