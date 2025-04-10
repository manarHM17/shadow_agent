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

        // Création de la table avec vérification détaillée
        const char* create_table_query = 
            "CREATE TABLE IF NOT EXISTS devices ("
            "id BIGINT AUTO_INCREMENT PRIMARY KEY,"
            "name VARCHAR(255) NOT NULL,"
            "type VARCHAR(255) NOT NULL,"
            "auth_key VARCHAR(255) NOT NULL,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ") ENGINE=InnoDB";

        if (mysql_query(conn, create_table_query)) {
            std::string error = mysql_error(conn);
            mysql_close(conn);
            throw std::runtime_error("Failed to create table: " + error);
        }

        // Vérifier que la table existe
        if (mysql_query(conn, "DESCRIBE devices")) {
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
