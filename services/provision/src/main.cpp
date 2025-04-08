#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "ProvisionServiceImpl.h"

using namespace std;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using namespace shadow_agent;

int main() {
    string server_address("0.0.0.0:50051");
    ProvisionServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Server listening on " << server_address << endl;
    server->Wait();

    return 0;
}
