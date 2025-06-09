// server/src/main.cpp
#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include "provisioning_service.h"
#include "database_manager.h"
#include "jwt_manager.h"

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    
    // Configuration de la base de données
    auto db_manager = std::make_shared<DatabaseManager>(
        "localhost",  // host
        "provisioning_user",  // user
        "provisioning_password",  // password
        "provisioning_db"  // database
    );
    
    if (!db_manager->connect()) {
        std::cerr << "Failed to connect to database" << std::endl;
        return;
    }
    
    // Configuration JWT
    auto jwt_manager = std::make_shared<JWTManager>("your-secret-key-here");
    
    // Service de provisionnement
    ProvisioningServiceImpl service(db_manager, jwt_manager);
    
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    
    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
