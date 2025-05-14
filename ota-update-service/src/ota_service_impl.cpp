#include "ota_service_impl.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <jwt-cpp/jwt.h>
#include <filesystem>

namespace fs = std::filesystem;

// Constantes
const std::string DOWNLOAD_DIR = "/tmp/ota_updates/";
const size_t CHUNK_SIZE = 64 * 1024; // 64KB

OTAServiceImpl::OTAServiceImpl(std::shared_ptr<MenderClient> mender_client)
    : m_mender_client(mender_client) {
    // Créer le répertoire de téléchargement s'il n'existe pas
    fs::create_directories(DOWNLOAD_DIR);
}

grpc::Status OTAServiceImpl::CheckForUpdate(grpc::ServerContext* context,
                                         const ota::CheckUpdateRequest* request,
                                         ota::CheckUpdateResponse* response) {
    std::cout << "Check for update request from device: " << std:to_string(request->device_id()) << std::endl;
    
    // Vérifier l'identifiant de l'appareil
    if (!verifyDeviceId(request->device_id())) {
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Invalid device ID");
    }
    
    // Vérifier les mises à jour avec Mender
    std::string update_id;
    bool update_available = m_mender_client->checkForUpdates(
        request->device_id(), request->current_version(), update_id);
    
    response->set_update_available(update_available);
    
    if (update_available) {
        // Normalement, ces informations devraient être obtenues de Mender
        // Ici, nous remplissons avec des valeurs fictives
        response->set_new_version("v" + std::to_string(std::stoi(request->current_version().substr(1)) + 1));
        response->set_update_size(10 * 1024 * 1024); // Exemple: 10MB
        response->set_update_id(update_id);
        response->set_changelog("Bug fixes and performance improvements");
    }
    
    return grpc::Status::OK;
}

grpc::Status OTAServiceImpl::DownloadUpdate(grpc::ServerContext* context,
                                         const ota::DownloadRequest* request,
                                         grpc::ServerWriter<ota::DownloadChunk>* writer) {
    std::cout << "Download request for update: " << request->update_id() << std::endl;
    
    // Valider le token JWT
    if (!validateToken(request->jwt_token())) {
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid token");
    }
    
    // Vérifier l'identifiant de l'appareil
    if (!verifyDeviceId(request->device_id())) {
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Invalid device ID");
    }
    
    // Créer un ID d'installation unique
    std::string installation_id = request->device_id() + "_" + 
                                  request->update_id() + "_" + 
                                  std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    // Mettre à jour le statut
    updateStatus(installation_id, ota::StatusResponse::DOWNLOADING, 0, "Starting download");
    
    // Chemin du fichier pour le téléchargement
    std::string download_path = DOWNLOAD_DIR + request->update_id() + ".mender";
    
    // Télécharger l'artefact depuis Mender
    bool download_success = m_mender_client->downloadArtifact(request->update_id(), download_path);
    if (!download_success) {
        updateStatus(installation_id, ota::StatusResponse::FAILED, 0, "Download failed");
        return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to download update");
    }
    
    // Ouvrir le fichier et l'envoyer par morceaux
    std::ifstream file(download_path, std::ios::binary);
    if (!file.is_open()) {
        updateStatus(installation_id, ota::StatusResponse::FAILED, 0, "Failed to open downloaded file");
        return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to open downloaded file");
    }
    
    // Obtenir la taille du fichier
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Envoyer le fichier par morceaux
    std::vector<char> buffer(CHUNK_SIZE);
    int chunk_id = 0;
    size_t bytes_sent = 0;
    
    while (file && !file.eof()) {
        file.read(buffer.data(), CHUNK_SIZE);
        std::streamsize bytes_read = file.gcount();
        
        if (bytes_read > 0) {
            ota::DownloadChunk chunk;
            chunk.set_data(buffer.data(), bytes_read);
            chunk.set_chunk_id(chunk_id++);
            
            bytes_sent += bytes_read;
            bool is_last_chunk = bytes_sent >= file_size;
            chunk.set_last_chunk(is_last_chunk);
            
            // Mettre à jour le statut de progression
            int progress = static_cast<int>((static_cast<double>(bytes_sent) / file_size) * 100);
            updateStatus(installation_id, ota::StatusResponse::DOWNLOADING, progress, 
                        "Downloading: " + std::to_string(progress) + "%");
            
            if (!writer->Write(chunk)) {
                updateStatus(installation_id, ota::StatusResponse::FAILED, progress, "Connection lost");
                return grpc::Status(grpc::StatusCode::ABORTED, "Stream closed by client");
            }
        }
    }
    
    updateStatus(installation_id, ota::StatusResponse::VERIFYING, 100, "Download complete, verifying...");
    
    return grpc::Status::OK;
}

grpc::Status OTAServiceImpl::InstallUpdate(grpc::ServerContext* context,
                                        const ota::InstallRequest* request,
                                        ota::InstallResponse* response) {
    std::cout << "Install request for update: " << request->update_id() << std::endl;
    
    // Vérifier l'identifiant de l'appareil
    if (!verifyDeviceId(request->device_id())) {
        return grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "Invalid device ID");
    }
    
    // Créer un ID d'installation unique
    std::string installation_id = request->device_id() + "_" + 
                                  request->update_id() + "_" + 
                                  std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    // Mettre à jour le statut
    updateStatus(installation_id, ota::StatusResponse::INSTALLING, 0, "Starting installation");
    
    // Lancer l'installation dans un thread séparé
    std::thread installation_thread([this, request, installation_id]() {
        bool success = this->installFirmware(request->update_id(), request->device_id());
        
        if (success) {
            updateStatus(installation_id, ota::StatusResponse::COMPLETED, 100, "Installation successful");
            
            // Rapporter le succès à Mender
            m_mender_client->reportUpdateStatus(request->update_id(), request->device_id(), "success");
        } else {
            updateStatus(installation_id, ota::StatusResponse::FAILED, 0, "Installation failed");
            
            // Rapporter l'échec à Mender
            m_mender_client->reportUpdateStatus(request->update_id(), request->device_id(), "failure");
        }
    });
    installation_thread.detach();
    
    // Répondre immédiatement avec l'ID d'installation
    response->set_success(true);
    response->set_message("Installation started");
    response->set_installation_id(installation_id);
    
    return grpc::Status::OK;
}

grpc::Status OTAServiceImpl::GetUpdateStatus(grpc::ServerContext* context,
                                          const ota::StatusRequest* request,
                                          ota::StatusResponse* response) {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    
    auto it = m_update_statuses.find(request->installation_id());
    if (it == m_update_statuses.end()) {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Installation ID not found");
    }
    
    response->set_status(it->second.status);
    response->set_progress(it->second.progress);
    response->set_message(it->second.message);
    
    return grpc::Status::OK;
}

bool OTAServiceImpl::validateToken(const std::string& jwt_token) {
    try {
        // Clé secrète pour la vérification du token (à remplacer par votre propre clé)
        const std::string secret = "your_jwt_secret_key";
        
        // Vérifier le token
        auto decoded = jwt::decode(jwt_token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret})
            .with_issuer("ota_service");
        
        verifier.verify(decoded);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Token validation error: " << e.what() << std::endl;
        return false;
    }
}

bool OTAServiceImpl::verifyDeviceId(const std::string& device_id) {
    // Ici, vous devriez implémenter la vérification de l'appareil
    // Par exemple, vérifier dans une base de données si l'appareil est enregistré
    // Pour cet exemple, nous acceptons tous les appareils
    return !device_id.empty();
}

bool OTAServiceImpl::installFirmware(const std::string& update_id, const std::string& device_id) {
    // Simuler l'installation du firmware
    std::string firmware_path = DOWNLOAD_DIR + update_id + ".mender";
    
    // Vérifier que le fichier existe
    if (!fs::exists(firmware_path)) {
        return false;
    }
    
    // Simuler les étapes d'installation
    for (int i = 0; i <= 100; i += 10) {
        std::string installation_id = device_id + "_" + update_id + "_" + 
                                     std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        updateStatus(installation_id, ota::StatusResponse::INSTALLING, i, 
                    "Installing firmware: " + std::to_string(i) + "%");
        
        // Simuler le temps d'installation
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Simuler le redémarrage
    std::string installation_id = device_id + "_" + update_id + "_" + 
                                 std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    updateStatus(installation_id, ota::StatusResponse::REBOOTING, 100, "Rebooting device");
    
    // Simuler le temps de redémarrage
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    return true;
}

void OTAServiceImpl::updateStatus(const std::string& installation_id, 
                               ota::StatusResponse::Status status,
                               int progress, 
                               const std::string& message) {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    
    UpdateStatus& update_status = m_update_statuses[installation_id];
    update_status.status = status;
    update_status.progress = progress;
    update_status.message = message;
    
    std::cout << "Update status [" << installation_id << "]: " 
              << message << " (" << progress << "%)" << std::endl;
}