#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "provision.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace shadow_agent;

class ProvisionClient {
private:
    unique_ptr<ProvisionService::Stub> stub_;

public:
    ProvisionClient(shared_ptr<Channel> channel)
        : stub_(ProvisionService::NewStub(channel)) {}

    bool ListDevices() {
        ListDeviceRequest request;
        DeviceList response;
        ClientContext context;

        Status status = stub_->ListDevices(&context, request, &response);
        if (status.ok()) {
            cout << "\nList of devices:\n";
            for (const auto& device : response.devices()) {
                cout << "ID: " << device.id() 
                     << "\n  Hostname: " << device.hostname()
                     << "\n  Type: " << device.type()
                     << "\n  OS Type: " << device.os_type()
                     << "\n  Username: " << device.username()
                     << "\n  Current Time: " << device.current_time()
                     << "\n";
            }
            return true;
        } else {
            cout << "RPC failed: " << status.error_message() << endl;
            return false;
        }
    }
};

int main() {
    string server_ip = "localhost:50051";

    // Créer un canal non sécurisé
    auto channel = grpc::CreateChannel(server_ip, grpc::InsecureChannelCredentials());
    ProvisionClient client(channel);

    // Tester une requête
    cout << "Attempting to list devices without SSL..." << endl;
    if (!client.ListDevices()) {
        cout << "Connection failed as expected (server requires SSL)." << endl;
    } else {
        cout << "Connection succeeded (unexpected, server should require SSL)." << endl;
    }

    return 0;
}