#include "../include/ota_server.h"
#include <iostream>
#include <chrono>
#include <random>
using namespace std;
using namespace grpc;
using namespace ota;

#define CHUNK_SIZE 32768 // 32 KB

namespace ota {

OTAServiceImpl::OTAServiceImpl() : initialized_(false) {
    db_manager_ = make_unique<DatabaseManager>();
    file_manager_ = make_unique<FileManager>();
}

OTAServiceImpl::~OTAServiceImpl() {}

bool OTAServiceImpl::initialize(const string& db_host, const string& db_user,
                               const string& db_password, const string& db_name) {
    if (!db_manager_->connect(db_host, db_user, db_password, db_name)) {
        cerr << "Failed to connect to database" << endl;
        return false;
    }
    
    initialized_ = true;
    cout << "OTA Service initialized successfully" << endl;
    return true;
}

Status OTAServiceImpl::RegisterDevice(ServerContext* context,
                                          const DeviceRegistration* request,
                                          RegistrationResponse* response) {
    if (!initialized_) {
        response->set_success(false);
        response->set_message("Service not initialized");
        return Status::OK;
    }
    
    DeviceInfo device;
    device.device_id = request->device_id();
    device.device_type = request->device_type();
    device.current_version = request->current_version();
    device.platform = request->platform();
    device.status = ota_common::DeviceStatus::ONLINE; // Fixed: use ota_common namespace
    
    bool success = db_manager_->registerDevice(device);
    response->set_success(success);
    response->set_message(success ? "Device registered successfully" : "Failed to register device");
    
    cout << "Device registration: " << request->device_id() 
              << " - " << (success ? "SUCCESS" : "FAILED") << endl;
    
    return Status::OK;
}

Status OTAServiceImpl::CheckUpdate(ServerContext* context,
                                       const UpdateRequest* request,
                                       UpdateResponse* response) {
    if (!initialized_) {
        response->set_update_available(false);
        return Status::OK;
    }
    
    cout << "Checking update for device: " << request->device_id() << endl;
    
    // Récupérer les informations du dispositif
    DeviceInfo device = db_manager_->getDevice(request->device_id());
    if (device.device_id == 0) {
        response->set_update_available(false);
        return Status::OK;
    }
    
    // Chercher la dernière mise à jour disponible
    // Fixed: Cast request->update_type() to ota_common::UpdateType
    ota::UpdateType update_type = static_cast<ota::UpdateType>(request->update_type());
    UpdateInfo latest_update = db_manager_->getLatestUpdate(device.device_type, update_type);
    
    if (latest_update.update_id == 0) {
        response->set_update_available(false);
        return Status::OK;
    }
    
    // Vérifier si une mise à jour est nécessaire
    bool needs_update = needsUpdate(request->current_version(), latest_update.version);
    
    response->set_update_available(needs_update);
    if (needs_update) {
        response->set_update_id(latest_update.update_id);
        response->set_version(latest_update.version);
        response->set_description(latest_update.description);
        response->set_file_size(latest_update.file_size);
        response->set_checksum(latest_update.checksum);
        response->set_update_type(static_cast<UpdateType>(latest_update.update_type));
        
        cout << "Update available: " << latest_update.version 
                  << " for device " << request->device_id() << endl;
    }
    
    return Status::OK;
}

Status OTAServiceImpl::DownloadUpdate(ServerContext* context,
                                          const DownloadRequest* request,
                                          ServerWriter<FileChunk>* writer) {
    if (!initialized_) {
        return Status(StatusCode::UNAVAILABLE, "Service not initialized");
    }
    
    cout << "Download request for update: " << request->update_id() 
              << " from device: " << request->device_id() << endl;
    
    // Marquer le dispositif comme en cours de mise à jour
    db_manager_->updateDeviceStatus(request->device_id(), ota::DeviceStatus::UPDATING);
    
    // Trouver le fichier de mise à jour
    string file_path;
    // Fixed: Use ota_common::UpdateType enum values
    for (int type = static_cast<int>(ota_common::UpdateType::CONFIG_FILE); 
         type <= static_cast<int>(ota_common::UpdateType::SYSTEMD_SERVICE); 
         ++type) {
        ota::UpdateType update_type = static_cast<ota::UpdateType>(type);
        UpdateInfo update = db_manager_->getLatestUpdate("", update_type);
        if (update.update_id == request->update_id()) {
            file_path = update.file_path;
            break;
        }
    }
    
    if (file_path.empty() || !file_manager_->fileExists(file_path)) {
        db_manager_->updateDeviceStatus(request->device_id(), ota::DeviceStatus::ERROR);
        return Status(StatusCode::NOT_FOUND, "Update file not found");
    }
    
    // Streaming du fichier par chunks
    int64_t offset = request->chunk_offset();
    vector<uint8_t> chunk_data;
    
    while (true) {
        bool success = file_manager_->readFileChunk(file_path, offset, CHUNK_SIZE, chunk_data);
        if (!success || chunk_data.empty()) {
            break;
        }
        
        FileChunk chunk;
        chunk.set_data(chunk_data.data(), chunk_data.size());
        chunk.set_offset(offset);
        chunk.set_is_last(chunk_data.size() < CHUNK_SIZE);
        
        if (!writer->Write(chunk)) {
            db_manager_->updateDeviceStatus(request->device_id(), ota::DeviceStatus::ERROR);
            return Status(StatusCode::ABORTED, "Failed to send chunk");
        }
        
        offset += chunk_data.size();
        
        if (chunk.is_last()) {
            break;
        }
    }
    
    cout << "Download completed for device: " << request->device_id() << endl;
    return Status::OK;
}

Status OTAServiceImpl::ConfirmInstallation(ServerContext* context,
                                               const InstallationRequest* request,
                                               InstallationResponse* response) {
    if (!initialized_) {
        response->set_success(UpdateStatus::FAILED);
        response->set_message("Service not initialized");
        return Status::OK;
    }
    
    cout << "Installation confirmation from device: " << request->device_id()
              << " - " << (request->status() ? "SUCCESS" : "FAILED") << endl;
    
    // Enregistrer l'installation
    InstallationRecord record;
    record.device_id = request->device_id();
    record.update_id = request->update_id();
    // Fixed: Use proper enum values instead of bool
    record.success = (request->status() == ota::UpdateStatus::SUCCESS) ? ota_common::UpdateStatus::SUCCESS : ota_common::UpdateStatus::FAILED;
    record.error_message = request->error_message();

    bool db_success = db_manager_->recordInstallation(record);

    // Mettre à jour le statut du dispositif
    // Fixed: Use proper enum values instead of bool
    ota_common::DeviceStatus new_status = (request->status() == ota::UpdateStatus::SUCCESS) ? ota_common::DeviceStatus::ONLINE : ota_common::DeviceStatus::ERROR;
    db_manager_->updateDeviceStatus(request->device_id(), static_cast<ota::DeviceStatus>(new_status));

    response->set_success(db_success);
    response->set_message(db_success ? "Installation recorded" : "Failed to record installation");

    return Status::OK;
}

Status OTAServiceImpl::GetDeviceStatus(ServerContext* context,
                                           const DeviceStatusRequest* request,
                                           DeviceStatusResponse* response) {
    if (!initialized_) {
        return Status(StatusCode::UNAVAILABLE, "Service not initialized");
    }
    
    DeviceInfo device = db_manager_->getDevice(request->device_id());
    
    if (device.device_id == 0) {
        return Status(StatusCode::NOT_FOUND, "Device not found");
    }
    
    response->set_device_id(device.device_id);
    response->set_current_version(device.current_version);
    response->set_last_update(device.last_seen);
    response->set_status(static_cast<DeviceStatus>(device.status));
    
    return Status::OK;
}

bool OTAServiceImpl::needsUpdate(const string& current_version, 
                               const string& latest_version) {
    // Comparaison simple de versions (peut être améliorée pour semantic versioning)
    return current_version != latest_version;
}

string OTAServiceImpl::generateUpdateId() {
    auto now = chrono::system_clock::now();
    auto timestamp = chrono::duration_cast<chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1000, 9999);
    
    return "update_" + to_string(timestamp) + "_" + to_string(dis(gen));
}

}
