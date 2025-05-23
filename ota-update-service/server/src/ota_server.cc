#include "ota_server.h"
#include "../common/include/logging.h"
#include <iostream>
#include <fstream>

namespace ota {

OtaServiceImpl::OtaServiceImpl(std::shared_ptr<FirmwareManager> firmware_manager,
                             std::shared_ptr<DbConnector> db_connector)
    : firmware_manager_(firmware_manager), db_connector_(db_connector) {}

grpc::Status OtaServiceImpl::CheckForUpdate(grpc::ServerContext* context,
                                          const CheckForUpdateRequest* request,
                                          CheckForUpdateResponse* response) {
    LOG_INFO("Requête de vérification de mise à jour reçue de " + request->device_id() +
             " (version actuelle: " + request->current_version() + ")");
    
    // Vérifier si une mise à jour est disponible pour ce dispositif
    auto update_info = firmware_manager_->CheckForUpdate(
        request->device_id(), 
        request->current_version(),
        request->hardware_model()
    );
    
    if (update_info.has_value()) {
        const auto& info = update_info.value();
        
        response->set_update_available(true);
        response->set_version(info.version);
        response->set_firmware_size(info.size);
        response->set_checksum(info.checksum);
        response->set_changelog(info.changelog);
        
        LOG_INFO("Mise à jour disponible: version " + info.version);
    } else {
        response->set_update_available(false);
        LOG_INFO("Aucune mise à jour disponible");
    }
    
    // Enregistrer l'interaction dans la base de données
    db_connector_->LogUpdateCheck(
        request->device_id(),
        request->current_version(),
        response->update_available(),
        response->update_available() ? response->version() : ""
    );
    
    return grpc::Status::OK;
}

grpc::Status OtaServiceImpl::DownloadFirmware(grpc::ServerContext* context,
                                            const DownloadFirmwareRequest* request,
                                            grpc::ServerWriter<FirmwareChunk>* writer) {
    LOG_INFO("Téléchargement du firmware demandé pour " + request->device_id() +
             " (version: " + request->version() + ")");
    
    // Obtenir le chemin du fichier firmware
    auto firmware_path = firmware_manager_->GetFirmwarePath(request->version());
    
    if (firmware_path.empty()) {
        LOG_ERROR("Firmware non trouvé pour la version " + request->version());
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Version de firmware non trouvée");
    }
    
    // Ouvrir le fichier firmware
    std::ifstream firmware_file(firmware_path, std::ios::binary);
    if (!firmware_file.is_open()) {
        LOG_ERROR("Impossible d'ouvrir le fichier firmware " + firmware_path);
        return grpc::Status(grpc::StatusCode::INTERNAL, "Erreur d'accès au fichier firmware");
    }
    
    // Enregistrer le début du téléchargement dans la base de données
    db_connector_->LogDownloadStart(
        request->device_id(),
        request->version()
    );
    
    // Envoyer le fichier en morceaux
    const size_t chunk_size = 64 * 1024;  // 64 KB par morceau
    std::vector<char> buffer(chunk_size);
    size_t offset = 0;
    
    while (firmware_file) {
        firmware_file.read(buffer.data(), chunk_size);
        size_t bytes_read = firmware_file.gcount();
        
        if (bytes_read > 0) {
            FirmwareChunk chunk;
            chunk.set_data(buffer.data(), bytes_read);
            chunk.set_offset(offset);
            
            if (!writer->Write(chunk)) {
                LOG_ERROR("Erreur lors de l'écriture du morceau de firmware");
                break;
            }
            
            offset += bytes_read;
        }
    }
    
    // Enregistrer la fin du téléchargement dans la base de données
    db_connector_->LogDownloadComplete(
        request->device_id(),
        request->version(),
        offset  // Nombre total d'octets envoyés
    );
    
    LOG_INFO("Téléchargement du firmware terminé: " + std::to_string(offset) + " octets envoyés");
    
    return grpc::Status::OK;
}

grpc::Status OtaServiceImpl::ReportUpdateStatus(grpc::ServerContext* context,
                                              const UpdateStatusRequest* request,
                                              UpdateStatusResponse* response) {
    LOG_INFO("Rapport de statut reçu de " + request->device_id() +
             " (version: " + request->version() + ", statut: " + 
             std::to_string(request->status()) + ")");
    
    // Enregistrer le statut de mise à jour dans la base de données
    db_connector_->LogUpdateStatus(
        request->device_id(),
        request->version(),
        request->status(),
        request->error_message()
    );
    
    response->set_received(true);
    return grpc::Status::OK;
}

OtaServer::OtaServer(const std::string& server_address,
                     std::shared_ptr<FirmwareManager> firmware_manager,
                     std::shared_ptr<DbConnector> db_connector)
    : firmware_manager_(firmware_manager), db_connector_(db_connector) {
    
    grpc::ServerBuilder builder;
    
    // Configurer le serveur sans authentification (pour la simplicité)
    // En production, il faudrait ajouter des credentials SSL/TLS
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    // Enregistrer le service
    OtaServiceImpl service(firmware_manager_, db_connector_);
    builder.RegisterService(&service);
    
    // Construire le serveur
    server_ = builder.BuildAndStart();
    
    LOG_INFO("Serveur OTA démarré sur " + server_address);
}

void OtaServer::Start() {
    if (server_) {
        LOG_INFO("Serveur OTA en attente de connexions...");
        server_->Wait();
    }
}

void OtaServer::Stop() {
    if (server_) {
        LOG_INFO("Arrêt du serveur OTA...");
        server_->Shutdown();
    }
}

} // namespace ota