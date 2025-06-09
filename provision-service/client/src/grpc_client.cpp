// client/src/main.cpp
#include <iostream>
#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include "../include/ProvisionClientImpl.h"
#include "../include/ConfigManager.h"

using namespace grpc;
using namespace std;

void showAuthMenu() {
    cout << "\n=== Authentification ===" << endl;
    cout << "1. Se connecter" << endl;
    cout << "2. S'enregistrer" << endl;
    cout << "0. Quitter" << endl;
    cout << "Choix: ";
}

void showMainMenu() {
    cout << "\n=== Menu Principal ===" << endl;
    cout << "1. Supprimer un dispositif" << endl;
    cout << "2. Mettre à jour un dispositif" << endl;
    cout << "3. Afficher tous les dispositifs" << endl;
    cout << "4. Afficher un dispositif par ID" << endl;
    cout << "5. Se déconnecter" << endl;
    cout << "0. Quitter" << endl;
    cout << "Choix: ";
}

bool authenticateUser(ProvisioningClient& client, string& jwt_token, int& current_device_id) {
    string hostname, password;
    
    cout << "\n=== Connexion ===" << endl;
    cout << "Hostname: ";
    getline(cin, hostname);
    cout << "Password: ";
    getline(cin, password);
    
    if (client.Authenticate(hostname, password, jwt_token)) {
        cout << "Connexion réussie!" << endl;
        
        // Try to load device ID from config
        string stored_device_id;
        if (ConfigManager::loadDeviceInfo(hostname, stored_device_id)) {
            current_device_id = stoi(stored_device_id);
        }
        return true;
    } else {
        cout << "Échec de la connexion!" << endl;
        return false;
    }
}

bool registerUser(ProvisioningClient& client, string& jwt_token, int& current_device_id) {
    string hostname, password, user, location, hardware_type, os_type;
    
    cout << "\n=== Nouveau Dispositif ===" << endl;
    cout << "Hostname: ";
    getline(cin, hostname);
    cout << "Password: ";
    getline(cin, password);
    cout << "User: ";
    getline(cin, user);
    cout << "Location: ";
    getline(cin, location);
    cout << "Hardware Type: ";
    getline(cin, hardware_type);
    cout << "OS Type: ";
    getline(cin, os_type);
    
    if (client.AddDevice(hostname, password, user, location, hardware_type,
                        os_type, current_device_id, jwt_token)) {
        cout << "Enregistrement réussi! ID: " << current_device_id << endl;
        return true;
    } else {
        cout << "Échec de l'enregistrement!" << endl;
        return false;
    }
}

void handleMainMenu(ProvisioningClient& client, const string& jwt_token, int current_device_id) {
    int choice;
    string user, location, hardware_type, os_type;
    int device_id;
    
    while (true) {
        showMainMenu();
        cin >> choice;
        cin.ignore();
        
        switch (choice) {
            case 1: {
                cout << "Device ID to delete: ";
                cin >> device_id;
                cin.ignore();
                
                client.DeleteDevice(device_id);
                break;
            }
            
            case 2: {
                cout << "Device ID to update (current: " << current_device_id << "): ";
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
                
                client.UpdateDevice(device_id, user, location, hardware_type, os_type);
                break;
            }
            
            case 3: {
                client.GetAllDevices();
                break;
            }
            
            case 4: {
                cout << "Device ID (current: " << current_device_id << "): ";
                cin >> device_id;
                cin.ignore();
                client.GetDeviceById(device_id);
                break;
            }
            
            case 5: {
                cout << "Déconnexion en cours..." << endl;
                return; // Return to authentication menu
            }
            
            case 0: {
                cout << "Au revoir!" << endl;
                exit(0);
            }
            
            default: {
                cout << "Choix invalide!" << endl;
                break;
            }
        }
    }
}

int main(int argc, char** argv) {
    string server_address = "localhost:50051";
    
    if (argc > 1) {
        server_address = argv[1];
    }
    
    // Create communication channel
    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    ProvisioningClient client(channel);
    
    int choice;
    string jwt_token;
    int current_device_id = -1;
    bool authenticated = false;
    
    cout << "\n=== Système de Provisionnement ===" << endl;
    
    while (true) {
        if (!authenticated) {
            showAuthMenu();
            cin >> choice;
            cin.ignore();
            
            switch (choice) {
                case 1: {
                    authenticated = authenticateUser(client, jwt_token, current_device_id);
                    break;
                }
                
                case 2: {
                    authenticated = registerUser(client, jwt_token, current_device_id);
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
        } else {
            // User is authenticated, show main menu
            handleMainMenu(client, jwt_token, current_device_id);
            // When returning from main menu, user is logged out
            authenticated = false;
            jwt_token.clear();
            current_device_id = -1;
            cout << "Retour au menu d'authentification..." << endl;
        }
    }
    
    return 0;
}