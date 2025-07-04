#include "grpc_service_impl.h"
#include <fstream>
#include <algorithm>

OTAUpdateServiceImpl::OTAUpdateServiceImpl(std::unique_ptr<OTAUpdateService> service)
    : ota_service(std::move(service)) {}

grpc::Status OTAUpdateServiceImpl::CheckForUpdates(grpc::ServerContext* context,
                                const ota::CheckUpdatesRequest* request,
                                ota::CheckUpdatesResponse* response) {
    try {
        if (!ota_service) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "OTA service not initialized");
        }
        int32_t device_id = request->device_id();
        std::string app_name = request->app_name();
        std::string current_version = request->current_version();

        auto updates = ota_service->GetAvailableUpdates(device_id, app_name, current_version);
        response->set_has_updates(!updates.empty());
        for (const auto& update : updates) {
            auto* update_info = response->add_available_updates();
            update_info->set_app_name(update.app_name);
            update_info->set_version(update.version);
            update_info->set_checksum(update.checksum);
            update_info->set_target_path("/opt/" + update.app_name);
        }
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status OTAUpdateServiceImpl::DownloadUpdate(grpc::ServerContext* context,
                               const ota::DownloadRequest* request,
                               grpc::ServerWriter<ota::DownloadResponse>* writer) {
    try {
        if (!ota_service) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "OTA service not initialized");
        }
        std::vector<char> file_data;
        int32_t device_id = request->device_id();
        if (!ota_service->DownloadUpdate(device_id, request->app_name(), file_data)) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Update not found");
        }
        const size_t chunk_size = 64 * 1024;
        size_t total_size = file_data.size();
        size_t sent = 0;
        while (sent < total_size) {
            ota::DownloadResponse response;
            size_t current_chunk = std::min(chunk_size, total_size - sent);
            response.set_data(std::string(file_data.data() + sent, current_chunk));
            response.set_total_size(total_size);
            response.set_current_size(sent + current_chunk);
            if (!writer->Write(response)) {
                return grpc::Status(grpc::StatusCode::ABORTED, "Failed to send chunk");
            }
            sent += current_chunk;
        }
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status OTAUpdateServiceImpl::ReportStatus(grpc::ServerContext* context,
                             const ota::StatusReport* request,
                             ota::StatusResponse* response) {
    try {
        if (!ota_service) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "OTA service not initialized");
        }
        UpdateStatus status;
        status.device_id = request->device_id();
        status.app_name = request->app_name();
        status.status = request->status();
        status.error_message = request->error_message();
        status.target_version = request->version();

        bool success = ota_service->ReportUpdateStatus(status);
        response->set_success(success);
        response->set_message(success ? "Status updated" : "Failed to update status");
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}