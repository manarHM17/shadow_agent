// server/main_server.cpp
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <fstream>
#include "../include/ota_server.h"
#include "../include/database_manager.h"
#include "../include/file_manager.h"

// Fonction pour créer des exemples de mises à jour (Niveau 1)
void createSampleUpdates(ota::DatabaseManager& db, ota::FileManager& fm) {
    std::cout << "Creating sample updates for Level 1..." << std::endl;
    
    // 1. Fichier de configuration d'exemple
    std::string config_content = R"({
  "app_name": "iot_device_v2.1",
  "log_level": "INFO",
  "mqtt_broker": "mqtt.example.com",
  "mqtt_port": 1883,
  "update_interval": 30,
  "sensors": {
    "temperature": true,
    "humidity": true,
    "pressure": false
  },
  "thresholds": {
    "temp_max": 35.0,
    "temp_min": 10.0,
    "humidity_max": 80.0
  }
})";

    std::vector<uint8_t> config_data(config_content.begin(), config_content.end());
    std::string config_update_id = "config_update_v2.1_001";
    
    if (fm.storeUpdateFile(config_update_id, 0, config_data)) {
        ota::UpdateInfo config_update;
        config_update.update_id = config_update_id;
        config_update.version = "2.1.0";
        config_update.description = "Configuration update: New MQTT settings and sensor thresholds";
        config_update.file_path = "/tmp/ota_updates/config/" + config_update_id;
        config_update.checksum = fm.calculateChecksum(config_data);
        config_update.file_size = config_data.size();
        config_update.update_type = 0; // CONFIG_FILE
        
        db.addUpdate(config_update);
        std::cout << "✓ Config update created: " << config_update_id << std::endl;
    }
    
    // 2. Application conteneurisée (script de démarrage)
    std::string app_script = R"(#!/bin/bash
# IoT Application Startup Script v1.5
echo "Starting IoT Device Application v1.5..."

# Export environment variables
export APP_VERSION="1.5.0"
export LOG_LEVEL="INFO"
export MQTT_ENABLED="true"

# Start application container
docker run -d --name iot_app \
  --restart unless-stopped \
  -e APP_VERSION=$APP_VERSION \
  -e LOG_LEVEL=$LOG_LEVEL \
  -e MQTT_ENABLED=$MQTT_ENABLED \
  -v /etc/iot/config.json:/app/config.json \
  -v /var/log/iot:/app/logs \
  iot-device:v1.5.0

echo "IoT Application started successfully"
)";

    std::vector<uint8_t> app_data(app_script.begin(), app_script.end());
    std::string app_update_id = "app_update_v1.5_001";
    
    if (fm.storeUpdateFile(app_update_id, 1, app_data)) {
        ota::UpdateInfo app_update;
        app_update.update_id = app_update_id;
        app_update.version = "1.5.0";
        app_update.description = "Application update: New container version with enhanced logging";
        app_update.file_path = "/tmp/ota_updates/apps/" + app_update_id;
        app_update.checksum = fm.calculateChecksum(app_data);
        app_update.file_size = app_data.size();
        app_update.update_type = 1; // APPLICATION
        
        db.addUpdate(app_update);
        std::cout << "✓ Application update created: " << app_update_id << std::endl;
    }
    
    // 3. Service systemd
    std::string service_content = R"([Unit]
Description=IoT Device Monitor Service v2.0
After=network.target docker.service
Requires=docker.service

[Service]
Type=notify
ExecStart=/usr/local/bin/iot-monitor
ExecReload=/bin/kill -HUP $MAINPID
ExecStop=/bin/kill -TERM $MAINPID
Restart=always
RestartSec=10
User=iot
Group=iot

# Resource limits
MemoryLimit=256M
CPUQuota=50%

# Environment
Environment=IOT_MODE=production
Environment=IOT_LOG_LEVEL=info
Environment=IOT_METRICS_ENABLED=true

[Install]
WantedBy=multi-user.target
)";

    std::vector<uint8_t> service_data(service_content.begin(), service_content.end());
    std::string service_update_id = "service_update_v2.0_001";
    
    if (fm.storeUpdateFile(service_update_id, 2, service_data)) {
        ota::UpdateInfo service_update;
        service_update.update_id = service_update_id;
        service_update.version = "2.0.0";
        service_update.description = "SystemD service update: Added resource limits and metrics";
        service_update.file_path = "/tmp/ota_updates/services/" + service_update_id;
        service_update.checksum = fm.calculateChecksum(service_data);
        service_update.file_size = service_data.size();
        service_update.update_type = 2; // SYSTEMD_SERVICE
        
        db.addUpdate(service_update);
        std::cout << "✓ SystemD service update created: " << service_update_id << std::endl;
    }
    
    std::cout << "Sample updates created successfully for Level 1 OTA!" << std::endl;
}

void printUsage() {
    std::cout << "\n=== OTA Service - Niveau 1 ===\n";
    std::cout << "Types de mises à jour supportées:\n";
    std::cout << "• Fichiers de configuration (JSON, XML, YAML)\n";
    std::cout << "• Applications métier conteneurisées\n";
    std::cout << "• Services systemd\n\n";
    
    std::cout << "Configuration BDD MySQL requise:\n";
    std::cout << "Host: localhost, User: ota_user, DB: ota_service\n\n";
    
    std::cout << "Endpoints gRPC disponibles:\n";
    std::cout << "• RegisterDevice - Enregistrer un dispositif\n";
    std::cout << "• CheckUpdate - Vérifier les mises à jour\n";
    std::cout << "• DownloadUpdate - Télécharger une mise à jour\n";
    std::cout << "• ConfirmInstallation - Confirmer l'installation\n";
    std::cout << "• GetDeviceStatus - Statut du dispositif\n\n";
}

int main(int argc, char** argv) {
    printUsage();
    
    // Configuration de la base de données
    std::string db_host = "localhost";
    std::string db_user = "ota_user";
    std::string db_password = "ota_password";
    std::string db_name = "ota_service";
    std::string server_address = "0.0.0.0:50051";
    
    // Traitement des arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--db-host" && i + 1 < argc) {
            db_host = argv[++i];
        } else if (arg == "--db-user" && i + 1 < argc) {
            db_user = argv[++i];
        } else if (arg == "--db-password" && i + 1 < argc) {
            db_password = argv[++i];
        } else if (arg == "--db-name" && i + 1 < argc) {
            db_name = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            server_address = "0.0.0.0:" + std::string(argv[++i]);
        }
    }
    
    // Initialisation du service OTA
    auto service = std::make_unique<ota::OTAServiceImpl>();
    
    if (!service->initialize(db_host, db_user, db_password, db_name)) {
        std::cerr << "Failed to initialize OTA service" << std::endl;
        return 1;
    }
    
    // Créer des exemples de mises à jour pour le niveau 1
    ota::DatabaseManager sample_db;
    ota::FileManager sample_fm;
    
    if (sample_db.connect(db_host, db_user, db_password, db_name)) {
        createSampleUpdates(sample_db, sample_fm);
    }
    
    // Configuration du serveur gRPC
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());
    
    // Augmenter les limites de message pour les gros fichiers
    builder.SetMaxReceiveMessageSize(100 * 1024 * 1024); // 100MB
    builder.SetMaxSendMessageSize(100 * 1024 * 1024);    // 100MB
    
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    
    if (!server) {
        std::cerr << "Failed to start gRPC server" << std::endl;
        return 1;
    }
    
    std::cout << "\n🚀 OTA Service started successfully!" << std::endl;
    std::cout << "📡 Server listening on " << server_address << std::endl;
    std::cout << "🏥 Database: " << db_host << "/" << db_name << std::endl;
    std::cout << "\n📋 Ready to handle Level 1 OTA updates:" << std::endl;
    std::cout << "   ⚙️  Configuration files" << std::endl;
    std::cout << "   📦 Containerized applications" << std::endl;
    std::cout << "   🔧 SystemD services" << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server..." << std::endl;
    
    // Attendre l'arrêt du serveur
    server->Wait();
    
    return 0;
}