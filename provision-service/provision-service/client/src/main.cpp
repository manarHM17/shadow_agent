// client/src/main.cpp
#include <iostream>
#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "provisioning_client.h"
using namespace grpc ;
using namespace std ;

void showMenu() {
    cout << "\n=== Menu de Provisionnement ===" << endl;
    cout << "1. Authentifier un dispositif" << endl;
    cout << "2. Ajouter un nouveau dispositif" << endl;
    cout << "3. Supprimer un dispositif" << endl;
    cout << "4. Mettre à jour un dispositif" << endl;
    cout << "5. Afficher tous les dispositifs" << endl;
    cout << "6. Afficher un dispositif par ID" << endl;
    cout << "0. Quitter" << endl;
    cout << "Choix: ";
}

int main(int argc, char** argv) {
    string server_address = "localhost:50051";
    
    if (argc > 1) {
        server_address = argv[1];
    }
    
    // Créer le canal de communication
    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    ProvisioningClient client(channel);
    
    int choice;
    string hostname, password, user, location, hardware_type, os_type, serial_number, ip_address;
    int device_id;
    string jwt_token;
    
    while (true) {
        showMenu();
        cin >> choice;
        cin.ignore(); // Ignorer le caractère newline
        
        switch (choice) {
            case 1: {
                cout << "Hostname: ";
                getline(cin, hostname);
                cout << "Password: ";
                getline(cin, password);
                
                client.Authenticate(hostname, password, jwt_token);
                break;
            }
            
            case 2: {
                cout << "Hostname: ";
                getline(cin, hostname);
                cout << "Password: ";
                getline(cin, password);
                cout << "User: ";
                getline(cin, user);
                cout << "Location: ";
                getline(cin, location);
                cout << "Hardware Type (ex: Raspberry Pi 4): ";
                getline(cin, hardware_type);
                cout << "OS Type (ex: Raspbian, Ubuntu): ";
                getline(cin, os_type);
                cout << "Serial Number: ";
                getline(cin, serial_number);
                
                client.AddDevice(hostname, password, user, location, hardware_type, os_type, serial_number, device_id, jwt_token);
                break;
            }
            
            case 3: {
                cout << "Device ID à supprimer: ";
                cin >> device_id;
                client.DeleteDevice(device_id);
                break;
            }
            
            case 4: {
                cout << "Device ID à mettre à jour: ";
                cin >> device_id;
                cin.ignore();
                
                cout << "User: ";
                getline(cin, user);
                cout << "Location: ";
                getline(cin, location);
                cout << "Hardware Type: ";
                getline(cin, hardware_type);
                cout << "OS Type: ";
                getline(cin, os_type);
                cout << "IP Address: ";
                getline(cin, ip_address);
                cout << "Serial Number: ";
                getline(cin, serial_number);
                
                client.UpdateDevice(device_id, user, location, hardware_type, os_type, ip_address, serial_number);
                break;
            }
            
            case 5: {
                client.GetAllDevices();
                break;
            }
            
            case 6: {
                cout << "Device ID: ";
                cin >> device_id;
                client.GetDeviceById(device_id);
                break;
            }
            
            case 0: {
                cout << "Au revoir!" << endl;
                return 0;
            }
            
            default: {
                cout << "Choix invalide!" << endl;
                break;
            }
        }
    }
    
    return 0;
