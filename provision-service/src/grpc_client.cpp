#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <grpcpp/grpcpp.h>
#include "provision.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace shadow_agent;

// Define the absolute path for token.txt
const string TOKEN_FILE_PATH = "/home/manar/IOTSHADOW/provision-service/token/token.txt";

// Vérifie si un fichier existe
bool fileExists(const std::string& path) {
    bool exists = std::filesystem::exists(path);
    std::cout << "[DEBUG] Checking if file exists: " << path << " -> " << (exists ? "Exists" : "Does not exist") << std::endl;
    return exists;
}

// Charger un fichier en tant que chaîne
string LoadFile(const string& filepath) {
    ifstream file(filepath, ios::in | ios::binary);
    if (!file) {
        throw runtime_error("Failed to open file: " + filepath);
    }
    return string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}

// Sauvegarder l'ID et le token dans un fichier
void SaveToken(int32_t device_id, const string& token) {
    ofstream tokenFile(TOKEN_FILE_PATH);
    if (!tokenFile.is_open()) {
        throw runtime_error("Failed to open " + TOKEN_FILE_PATH + " for writing");
    }
    tokenFile << device_id << ":" << token;
    tokenFile.close();
    std::cout << "[DEBUG] Saved to " << TOKEN_FILE_PATH << ": " << device_id << ":" << token << std::endl;
}

// Charger l'ID depuis un fichier
int32_t GetIdFromFile() {
    ifstream tokenFile(TOKEN_FILE_PATH);
    if (!tokenFile.is_open()) {
        throw runtime_error("No token file found at " + TOKEN_FILE_PATH + ". Please register a device first.");
    }
    string line;
    getline(tokenFile, line);
    tokenFile.close();

    if (line.empty()) {
        throw runtime_error("Token file " + TOKEN_FILE_PATH + " is empty.");
    }

    size_t pos = line.find(':');
    if (pos == string::npos) {
        throw runtime_error("Invalid format in " + TOKEN_FILE_PATH + ". Expected 'id:token'. Found: " + line);
    }

    string id_str = line.substr(0, pos);
    try {
        return stoi(id_str);
    } catch (const std::exception& e) {
        throw runtime_error("Invalid device ID in " + TOKEN_FILE_PATH + ": " + string(e.what()));
    }
}

// Charger le token depuis un fichier
string GetTokenFromFile() {
    ifstream tokenFile(TOKEN_FILE_PATH);
    if (!tokenFile.is_open()) {
        throw runtime_error("No token file found at " + TOKEN_FILE_PATH + ". Please register a device first.");
    }
    string line;
    getline(tokenFile, line);
    tokenFile.close();

    if (line.empty()) {
        throw runtime_error("Token file " + TOKEN_FILE_PATH + " is empty.");
    }

    size_t pos = line.find(':');
    if (pos == string::npos) {
        throw runtime_error("Invalid format in " + TOKEN_FILE_PATH + ". Expected 'id:token'. Found: " + line);
    }

    return line.substr(pos + 1);
}

// Classe client pour le service Provision
class ProvisionClient {
private:
    unique_ptr<ProvisionService::Stub> stub_;

public:
    ProvisionClient(shared_ptr<Channel> channel)
        : stub_(ProvisionService::NewStub(channel)) {}

    bool RegisterDevice() {
        if (fileExists(TOKEN_FILE_PATH)) {
            cout << "Device already registered. Retrieving info using token...\n";

            int32_t id;
            string token;
            try {
                id = GetIdFromFile();
                token = GetTokenFromFile();
            } catch (const runtime_error& e) {
                cerr << "Error reading " << TOKEN_FILE_PATH << ": " << e.what() << endl;
                return false;
            }

            DeviceId request;
            request.set_id(id);
            request.set_token(token);

            DeviceInfo response;
            ClientContext context;

            Status status = stub_->GetDevice(&context, request, &response);
            if (status.ok()) {
                cout << "\nDevice already registered:\n"
                     << "ID: " << response.id()
                     << "\nHostname: " << response.hostname()
                     << "\nType: " << response.type()
                     << "\nOS Type: " << response.os_type()
                     << "\nUsername: " << response.username()
                     << "\nCurrent Time: " << response.current_time()
                     << endl;
                return true;
            } else {
                cerr << "Failed to retrieve device info: " << status.error_message() << endl;
                return false;
            }
        }

        // If token.txt doesn't exist, proceed with registration
        cout << "No token found. Registering new device...\n";
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
                cout << "Device registered successfully!\n"
                     << "ID: " << response.id() << "\n"
                     << "Token: " << response.token() << endl;
                try {
                    SaveToken(response.id(), response.token()); // Save ID and token
                } catch (const runtime_error& e) {
                    cerr << "Failed to save token: " << e.what() << endl;
                    return false;
                }
            }
            return response.success();
        } else {
            cerr << "RPC failed: " << status.error_message() << endl;
            return false;
        }
    }

    bool DeleteDevice() {
        int32_t id;
        string token;
        try {
            id = GetIdFromFile();
            token = GetTokenFromFile();
        } catch (const runtime_error& e) {
            cerr << "Error: " << e.what() << endl;
            return false;
        }

        cout << "Confirm deletion of device ID " << id << "? (y/n): ";
        string confirmation;
        getline(cin, confirmation);
        if (confirmation != "y" && confirmation != "Y") {
            cout << "Deletion cancelled." << endl;
            return false;
        }

        DeviceId request;
        request.set_id(id);
        request.set_token(token);

        Response response;
        ClientContext context;

        Status status = stub_->DeleteDevice(&context, request, &response);
        if (status.ok()) {
            cout << "Response: " << response.message() << endl;
            if (response.success()) {
                // Remove token.txt on successful deletion
                if (remove(TOKEN_FILE_PATH.c_str()) != 0) {
                    cerr << "Warning: Failed to delete " << TOKEN_FILE_PATH << endl;
                } else {
                    cout << "[DEBUG] Deleted " << TOKEN_FILE_PATH << endl;
                }
            }
            return response.success();
        } else {
            cerr << "RPC failed: " << status.error_message() << endl;
            return false;
        }
    }

    bool UpdateDevice() {
        int32_t id;
        string token;
        try {
            id = GetIdFromFile();
            token = GetTokenFromFile();
        } catch (const runtime_error& e) {
            cerr << "Error: " << e.what() << endl;
            return false;
        }

        string hostname, type, os_type, username;
        cout << "Enter new hostname: ";
        getline(cin, hostname);
        cout << "Enter new type: ";
        getline(cin, type);
        cout << "Enter new OS type: ";
        getline(cin, os_type);
        cout << "Enter new username: ";
        getline(cin, username);

        UpdateDeviceRequest request;
        request.set_id(id);
        request.set_hostname(hostname);
        request.set_type(type);
        request.set_os_type(os_type);
        request.set_username(username);
        request.set_token(token);

        Response response;
        ClientContext context;

        Status status = stub_->UpdateDevice(&context, request, &response);
        if (status.ok()) {
            cout << "Response: " << response.message() << endl;
            return response.success();
        } else {
            cerr << "RPC failed: " << status.error_message() << endl;
            return false;
        }
    }

    bool ListDevices() {
        string token;
        try {
            token = GetTokenFromFile();
        } catch (const runtime_error& e) {
            cerr << "Error: " << e.what() << endl;
            return false;
        }

        ListDeviceRequest request;
        request.set_token(token);

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
                     << "\n\n";
            }
            return true;
        } else {
            cerr << "RPC failed: " << status.error_message() << endl;
            return false;
        }
    }

    bool GetDevice() {
        int32_t id;
        string token;
        try {
            id = GetIdFromFile();
            token = GetTokenFromFile();
        } catch (const runtime_error& e) {
            cerr << "Error: " << e.what() << endl;
            return false;
        }

        DeviceId request;
        request.set_id(id);
        request.set_token(token);

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
            cerr << "RPC failed: " << status.error_message() << endl;
            return false;
        }
    }
};

void displayMenu() {
    cout << "\nProvision Service Client\n"
         << "1. Register Device\n"
         << "2. Delete Device\n"
         "3. Update Device\n"
         "4. List Devices\n"
         "5. Get Device\n"
         "0. Exit\n"
         "Choose an option: ";
}

int main() {
    const char* addr = getenv("SERVER_ADDR");
    string target = addr ? addr : "127.0.0.1:50051";

    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    ProvisionClient client(channel);

    int choice;
    do {
        displayMenu();
        cin >> choice;
        cin.ignore();  // Ignore newline after numeric input

        switch (choice) {
            case 1: client.RegisterDevice(); break;
            case 2: client.DeleteDevice(); break;
            case 3: client.UpdateDevice(); break;
            case 4: client.ListDevices(); break;
            case 5: client.GetDevice(); break;
            case 0: cout << "Exiting...\n"; break;
            default: cout << "Invalid option!\n";
        }
    } while (choice != 0);

    return 0;
}