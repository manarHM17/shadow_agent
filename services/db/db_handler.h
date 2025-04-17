#pragma once
#include <mysql/mysql.h>
#include <string>
#include <memory>
#include <stdexcept>

class DBHandler {
public:
    // Constructeur : initialise automatiquement la connexion
    DBHandler() {
        // Initialisation de MySQL
        conn = mysql_init(nullptr);
        if (!conn) {
            throw std::runtime_error("MySQL init failed");
        }
    
        // Établissement de la connexion initiale sans sélectionner de base de données
        if (!mysql_real_connect(conn, 
            "localhost",     // host
            "iotuser",      // username
            "iot2025",      // password
            nullptr,        // pas de base de données sélectionnée
            3306,          // port
            nullptr,       // unix socket
            0             // client flag
        )) {
            mysql_close(conn);  // Nettoyage en cas d'échec
            throw std::runtime_error(mysql_error(conn));
        }

        // Création de la base de données si elle n'existe pas
        if (mysql_query(conn, "CREATE DATABASE IF NOT EXISTS shadow_agent")) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("Failed to create database: " + error);
        }

        // Sélection de la base de données
        if (mysql_select_db(conn, "shadow_agent")) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("Failed to select database: " + error);
        }

        // Création de la table `devices`
        const char* create_devices_table_query = 
            "CREATE TABLE IF NOT EXISTS devices ("
            "id BIGINT AUTO_INCREMENT PRIMARY KEY,"
            "hostname VARCHAR(255) NOT NULL,"
            "type VARCHAR(255) NOT NULL,"
            "os_type VARCHAR(255) NOT NULL,"
            "username VARCHAR(255) NOT NULL,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ") ENGINE=InnoDB";

        if (mysql_query(conn, create_devices_table_query)) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("Failed to create devices table: " + error);
        }

        // Création de la table `monitoring`
        const char* create_monitoring_table_query = 
            "CREATE TABLE IF NOT EXISTS monitoring ("
            "id BIGINT AUTO_INCREMENT PRIMARY KEY,"
            "device_id VARCHAR(255) NOT NULL,"
            "timestamp BIGINT NOT NULL,"
            "cpu_usage FLOAT NOT NULL,"
            "memory_total_mb INT NOT NULL,"
            "memory_used_mb INT NOT NULL,"
            "disk_usage_root VARCHAR(10) NOT NULL,"
            "uptime VARCHAR(255) NOT NULL,"
            "usb_devices TEXT NOT NULL,"
            "ip_address VARCHAR(45) NOT NULL,"
            "network_status VARCHAR(20) NOT NULL,"
            "services_status JSON NOT NULL,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ") ENGINE=InnoDB";

        if (mysql_query(conn, create_monitoring_table_query)) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("Failed to create monitoring table: " + error);
        }

        // Vérifier que les tables existent
        if (mysql_query(conn, "DESCRIBE devices") || mysql_query(conn, "DESCRIBE monitoring")) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("Table verification failed: " + error);
        }
    }

    // Destructeur : ferme automatiquement la connexion
    ~DBHandler() {
        if (conn) {
            mysql_close(conn);
        }
    }

    // Empêcher la copie de l'objet
    DBHandler(const DBHandler&) = delete;
    DBHandler& operator=(const DBHandler&) = delete;

    // Méthodes d'accès
    MYSQL* getConnection() { 
        clearPreviousResults();  // Nettoie seulement les buffers de résultats
        return conn; 
    }

    // Méthodes utilitaires avec gestion sécurisée des résultats
    bool executeQuery(const std::string& query) {
        clearPreviousResults();  // Nettoie les anciens résultats en mémoire
        return mysql_query(conn, query.c_str()) == 0;
    }

    // Méthode sécurisée pour les requêtes SELECT
    MYSQL_RES* executeSelect(const std::string& query) {
        clearPreviousResults();  // Nettoie les anciens résultats
        if (mysql_query(conn, query.c_str()) != 0) {
            return nullptr;
        }
        return mysql_store_result(conn);
    }

    std::string getLastError() {
        return mysql_error(conn);
    }

    // Méthode pour insérer des données dans la table `monitoring`
    bool insertMonitoringData(const std::string& device_id, long timestamp, float cpu_usage, int memory_total_mb,
                              int memory_used_mb, const std::string& disk_usage_root, const std::string& uptime,
                              const std::string& usb_devices, const std::string& ip_address,
                              const std::string& network_status, const std::string& services_status_json) {
        std::string query = "INSERT INTO monitoring (device_id, timestamp, cpu_usage, memory_total_mb, memory_used_mb, "
                            "disk_usage_root, uptime, usb_devices, ip_address, network_status, services_status) VALUES ('" +
                            device_id + "', " + std::to_string(timestamp) + ", " + std::to_string(cpu_usage) + ", " +
                            std::to_string(memory_total_mb) + ", " + std::to_string(memory_used_mb) + ", '" +
                            disk_usage_root + "', '" + uptime + "', '" + usb_devices + "', '" + ip_address + "', '" +
                            network_status + "', '" + services_status_json + "')";
        return executeQuery(query);
    }

private:
    MYSQL* conn;

    // Nettoie uniquement les buffers de résultats en mémoire
    // N'affecte PAS les données dans la base MySQL
    void clearPreviousResults() {
        MYSQL_RES* result;
        while ((result = mysql_store_result(conn)) != nullptr) {
            mysql_free_result(result);  // Libère seulement la mémoire
        }
    }
};
