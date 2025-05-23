#ifndef DB_CONNECTOR_H
#define DB_CONNECTOR_H

#include <string>
#include <vector>
#include <memory>
#include <mysql/mysql.h>

namespace ota {

// Structure pour les statistiques d'un dispositif
struct DeviceStats {
    std::string device_id;
    std::string current_version;
    std::string hardware_model;
    std::string last_check_time;
    std::string last_update_time;
    int update_success_count;
    int update_failure_count;
};

class DbConnector {
public:
    DbConnector(const std::string& host, const std::string& user,
                const std::string& password, const std::string& database);
    ~DbConnector();

    // Initialiser la base de données
    bool Initialize();

    // Enregistrer une vérification de mise à jour
    bool LogUpdateCheck(const std::string& device_id, 
                        const std::string& current_version,
                        bool update_available,
                        const std::string& available_version);

    // Enregistrer le début d'un téléchargement
    bool LogDownloadStart(const std::string& device_id,
                          const std::string& version);

    // Enregistrer la fin d'un téléchargement
    bool LogDownloadComplete(const std::string& device_id,
                             const std::string& version,
                             uint64_t bytes_downloaded);

    // Enregistrer un changement de statut de mise à jour
    bool LogUpdateStatus(const std::string& device_id,
                         const std::string& version,
                         int status,
                         const std::string& error_message);

    // Obtenir les statistiques pour un dispositif
    std::vector<DeviceStats> GetDeviceStats(const std::string& device_id = "");

    // Vérifier la santé de la base de données
    bool CheckHealth();

private:
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    MYSQL* db_;

    // Exécuter une requête SQL simple
    bool ExecuteQuery(const std::string& query);
};

} // namespace ota

#endif // DB_CONNECTOR_H