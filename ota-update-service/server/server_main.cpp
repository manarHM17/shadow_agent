// server_main.cpp
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>
#include "grpc_service_impl.h"

void RunServer() {
    std::string server_address("0.0.0.0:50051");

    auto ota_service = std::make_unique<OTAUpdateService>(
        "/home/manar/IOTSHADOW/ota-update-service/server/updates/applications"
    );

    if (!ota_service->InitializeDatabase()) {
        std::cerr << "Failed to initialize database" << std::endl;
        return;
    }

    OTAUpdateServiceImpl service(std::move(ota_service));

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "OTA Update Server listening on " << server_address << std::endl;

    server->Wait();
}

int main() {
    RunServer();
    return 0;
}