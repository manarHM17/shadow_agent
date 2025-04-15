#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <stdexcept>
#include <grpcpp/grpcpp.h>
#include "provision.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace shadow_agent;


std::string LoadFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

class ProvisionClient {
private:
    unique_ptr<ProvisionService::Stub> stub_;

public:
    ProvisionClient(shared_ptr<Channel> channel)
        : stub_(ProvisionService::NewStub(channel)) {}

    bool RegisterDevice() {
        string hostname, type, os_type, username;
        cout << "Enter hostname: ";
        getline(cin, hostname);
        cout << "Enter device type: ";
        getline(cin, type);
        cout << "Enter OS type: ";
        getline(cin, os_type);
        cout << "Enter username: ";
        getline(cin, username);

        DeviceInfo request;
        request.set_hostname(hostname);
        request.set_type(type);
        request.set_os_type(os_type);
        request.set_username(username);

        RegisterDeviceResponse response;
        ClientContext context;

        Status status = stub_->RegisterDevice(&context, request, &response);
        if (status.ok()) {
            cout << "Response: " << response.message() << endl;
            if (response.success()) {
                cout << "Device registered successfully!" << endl;
            }
            return response.success();
        } else {
            cout << "RPC failed: " << status.error_message() << endl;
            return false;
        }
    }

    bool DeleteDevice() {
        int32_t id;
        cout << "Enter device ID to delete: ";
        cin >> id;
        cin.ignore();

        DeviceId request;
        request.set_id(id);

        Response response;
        ClientContext context;

        Status status = stub_->DeleteDevice(&context, request, &response);
        if (status.ok()) {
            cout << "Response: " << response.message() << endl;
            return response.success();
        } else {
            cout << "RPC failed: " << status.error_message() << endl;
            return false;
        }
    }

    bool UpdateDevice() {
        int32_t id;
        string hostname, type, os_type, username;
        
        cout << "Enter device ID to update: ";
        cin >> id;
        cin.ignore();
        
        cout << "Enter new hostname: ";
        getline(cin, hostname);
        cout << "Enter new type: ";
        getline(cin, type);
        cout << "Enter new OS type: ";
        getline(cin, os_type);
        cout << "Enter new username: ";
        getline(cin, username);

        UpdateDeviceRequest request; // Utiliser UpdateDeviceRequest au lieu de DeviceInfo
        request.set_id(id);
        request.set_hostname(hostname);
        request.set_type(type);
        request.set_os_type(os_type);
        request.set_username(username);

        Response response;
        ClientContext context;

        Status status = stub_->UpdateDevice(&context, request, &response);
        if (status.ok()) {
            cout << "Response: " << response.message() << endl;
            return response.success();
        } else {
            cout << "RPC failed: " << status.error_message() << endl;
            return false;
        }
    }

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

    bool GetDevice() {
        int32_t id;
        cout << "Enter device ID to fetch: ";
        cin >> id;
        cin.ignore();

        DeviceId request;
        request.set_id(id);
        DeviceInfo response;
        ClientContext context;

        Status status = stub_->GetDevice(&context, request, &response);
        if (status.ok()) {
            cout << "\nDevice details:\n"
                 << "ID: " << response.id()
                 << "\nHostname: " << response.hostname()
                 << "\nType: " << response.type()
                 << "\nOS Type: " << response.os_type()
                 << "\nUsername: " << response.username()
                 << "\nCurrent Time: " << response.current_time()
                 << endl;
            return true;
        } else {
            cout << "RPC failed: " << status.error_message() << endl;
            return false;
        }
    }
};

void displayMenu() {
    cout << "\nProvision Service Client\n"
         << "1. Register Device\n"
         << "2. Delete Device\n"
         << "3. Update Device\n"
         << "4. List Devices\n"
         << "5. Get Device\n"
         << "0. Exit\n"
         << "Choose an option: ";
}

int main() {
    string server_ip = "localhost:50051";
    // Charger le certificat du serveur
    string server_cert = LoadFile("../server.crt"); // Chemin vers le certificat du serveur

    // Configurer les options SSL/TLS
    grpc::SslCredentialsOptions ssl_opts;
    ssl_opts.pem_root_certs = server_cert;

    // Créer un canal sécurisé avec les credentials SSL
    auto channel = grpc::CreateChannel(server_ip, grpc::SslCredentials(ssl_opts));
    ProvisionClient client(channel);


    int choice;
    do {
        displayMenu();
        cin >> choice;
        cin.ignore();

        switch (choice) {
            case 1:
                client.RegisterDevice();
                break;
            case 2:
                client.DeleteDevice();
                break;
            case 3:
                client.UpdateDevice();
                break;
            case 4:
                client.ListDevices();
                break;
            case 5:
                client.GetDevice();
                break;
            case 0:
                cout << "Exiting...\n";
                break;
            default:
                cout << "Invalid option!\n";
        }
    } while (choice != 0);

    return 0;
}
