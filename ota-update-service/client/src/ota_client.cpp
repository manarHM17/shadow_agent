#include "../include/ota_client.h"
#include <iostream>
#include <fstream>
#include <grpcpp/grpcpp.h>

namespace ota {

OTAClient::OTAClient(const std::string& server_address) {
    channel_ = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    stub_ = ota::OTAService::NewStub(channel_);
}

OTAClient::~OTAClient() {}

bool OTAClient::isConnected() {
    auto state = channel_->GetState(true);
    return state == GRPC_CHANNEL_READY;
}

bool OTAClient::registerDevice(const std::string& device_id, const std::string& device_type,
                              const std::string& current_version, const std::string& platform) {
    ota::DeviceRegistration request;
    request.set_device_id(device_id);
    request.set_device_type(device_type);
    request.set_current_version(current_version);
    request.set_platform(platform);
    
    ota::RegistrationResponse response;
    grpc::ClientContext context;
    
    // Timeout de 10 secondes
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(10);
    context.set_deadline(deadline);
    
    grpc::Status status = stub_->RegisterDevice(&context, request, &response);
    
    if (status.ok()) {
        std::cout << "Device registration: " << response.message() << std::endl;
        return response.success();
    } else {
        std::cerr << "Registration failed: " << status.error_message() << std::endl;
        return false;
    }
}

bool OTAClient::checkForUpdates(const std::string& device_id, const std::string& current_version,
                               int update_type, ota::UpdateResponse& update_info) {
    ota::UpdateRequest request;
    request.set_device_id(device_id);
    request.set_current_version(current_version);
    request.set_update_type(static_cast<ota::UpdateType>(update_type));
    
    grpc::ClientContext context;
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(10);
    context.set_deadline(deadline);
    
    grpc::Status status = stub_->CheckUpdate(&context, request, &update_info);
    
    if (status.ok()) {
        if (update_info.update_available()) {
            std::cout << "Update available: " << update_info.version() 
                      << " (" << update_info.description() << ")" << std::endl;
        } else {
            std::cout << "No updates available" << std::endl;
        }
        return true;
    } else {
        std::cerr << "Check update failed: " << status.error_message() << std::endl;
        return false;
    }
}

bool OTAClient::downloadUpdate(const std::string& device_id, const std::string& update_id,
                              const std::string& local_path) {
    ota::DownloadRequest request;
    request.set_device_id(device_id);
    request.set_update_id(update_id);
    request.set_chunk_offset(0);
    
    grpc::ClientContext context;
    // Timeout plus long pour le téléchargement
    auto deadline = std::chrono::system_clock::now() + std::chrono::minutes(5);
    context.set_deadline(deadline);
    
    std::unique_ptr<grpc::ClientReader<ota::FileChunk>> reader(
        stub_->DownloadUpdate(&context, request));
    
    std::ofstream file(local_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to create local file: " << local_path << std::endl;
        return false;
    }
    
    ota::FileChunk chunk;
    int64_t total_bytes = 0;
    
    std::cout << "Downloading update..." << std::endl;
    
    while (reader->Read(&chunk)) {
        file.write(chunk.data().c_str(), chunk.data().size());
        total_bytes += chunk.data().size();
        
        // Afficher le progrès
        if (total_bytes % (1024 * 1024) == 0) { // Chaque MB
            std::cout << "Downloaded: " << total_bytes / 1024 << " KB" << std::endl;
        }
        
        if (chunk.is_last()) {
            break;
        }
    }
    
    file.close();
    
    grpc::Status status = reader->Finish();
    if (status.ok()) {
        std::cout << "Download completed: " << total_bytes << " bytes" << std::endl;
        return true;
    } else {
        std::cerr << "Download failed: " << status.error_message() << std::endl;
        return false;
    }
}

bool OTAClient::confirmInstallation(const std::string& device_id, const std::string& update_id,
                                   bool success, const std::string& error_message) {
    ota::InstallationRequest request;
    request.set_device_id(device_id);
    request.set_update_id(update_id);
    request.set_success(success);
    request.set_error_message(error_message);
    
    ota::InstallationResponse response;
    grpc::ClientContext context;
    
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(10);
    context.set_deadline(deadline);
    
    grpc::Status status = stub_->ConfirmInstallation(&context, request, &response);
    
    if (status.ok()) {
        std::cout << "Installation confirmation: " << response.message() << std::endl;
        return response.success();
    } else {
        std::cerr << "Confirmation failed: " << status.error_message() << std::endl;
        return false;
    }
}

bool OTAClient::getDeviceStatus(const std::string& device_id, ota::DeviceStatusResponse& status) {
    ota::DeviceStatusRequest request;
    request.set_device_id(device_id);
    
    grpc::ClientContext context;
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(10);
    context.set_deadline(deadline);
    
    grpc::Status grpc_status = stub_->GetDeviceStatus(&context, request, &status);
    
    if (grpc_status.ok()) {
        return true;
    } else {
        std::cerr << "Get status failed: " << grpc_status.error_message() << std::endl;
        return false;
    }
}

} // namespace ota