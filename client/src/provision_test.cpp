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

// Fonction pour charger un fichier (par exemple, certificat)
string LoadFile(const string& filepath) {
    ifstream file(filepath, ios::in | ios::binary);
    if (!file) {
        throw runtime_error("Failed to open file: " + filepath);
    }
    return string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}

// Classe pour interagir avec le service Provision
class ProvisionClient {
private:
    unique_ptr<ProvisionService::Stub> stub_;
    string token_; // Stocker le token reçu lors de l'enregistrement

public:
    ProvisionClient(shared_ptr<Channel> channel)
        : stub_(ProvisionService::NewStub(channel)) {}

    // Méthode pour enregistrer un périphérique
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
                cout << "Token: " << response.token() << endl; // Afficher le token
                token_ = response.token(); // Stocker le token pour les autres requêtes
            }
            return response.success();
        } else {
            cout << "RPC failed: " << status.error_message() << endl;
            return false;
        }
    }

    // Méthode pour supprimer un périphérique
    bool DeleteDevice() {
        int32_t id;
        cout << "Enter device ID to delete: ";
        cin >> id;
        cin.ignore();

        if (token_.empty()) {
            cout << "Error: No token available. Please register a device first." << endl;
            return false;
        }

        DeviceId request;
        request.set_id(id);
        request.set_token(token_); // Ajouter le token à la requête

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

    // Méthode pour mettre à jour un périphérique
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

        if (token_.empty()) {
            cout << "Error: No token available. Please register a device first." << endl;
            return false;
        }

        UpdateDeviceRequest request;
        request.set_id(id);
        request.set_hostname(hostname);
        request.set_type(type);
        request.set_os_type(os_type);
        request.set_username(username);
        request.set_token(token_); // Ajouter le token à la requête

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

    // Méthode pour lister les périphériques
    bool ListDevices() {
        if (token_.empty()) {
            cout << "Error: No token available. Please register a device first." << endl;
            return false;
        }

        ListDeviceRequest request;
        request.set_token(token_); // Ajouter le token à la requête

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

    // Méthode pour obtenir un périphérique spécifique
    bool GetDevice() {
        int32_t id;
        cout << "Enter device ID to fetch: ";
        cin >> id;
        cin.ignore();

        if (token_.empty()) {
            cout << "Error: No token available. Please register a device first." << endl;
            return false;
        }

        DeviceId request;
        request.set_id(id);
        request.set_token(token_); // Ajouter le token à la requête

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

    // Utiliser une connexion insecure pour tester
    auto channel = grpc::CreateChannel(server_ip, grpc::InsecureChannelCredentials());
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
