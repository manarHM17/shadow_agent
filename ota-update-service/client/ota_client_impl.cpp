#include "ota_client_impl.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <openssl/sha.h>

SimpleOTAClient::SimpleOTAClient(const std::string& server_address, int32_t user_device_id) {
    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    stub = ota::OTAUpdateService::NewStub(channel);
    device_id = user_device_id;
    std::cout << "OTA Client initialized with device_id: " << device_id << std::endl;
}

void SimpleOTAClient::CheckAndApplyUpdates() {
    try {
        ota::CheckUpdatesRequest request;
        request.set_device_id(device_id);
        request.set_current_version("1.0.0");

        ota::CheckUpdatesResponse response;
        grpc::ClientContext context;

        grpc::Status status = stub->CheckForUpdates(&context, request, &response);

        if (!status.ok()) {
            std::cerr << "Failed to check updates: " << status.error_message() << std::endl;
            return;
        }

        if (!response.has_updates()) {
            std::cout << "No updates available" << std::endl;
            return;
        }

        std::cout << "Found " << response.available_updates_size() << " updates" << std::endl;

        for (const auto& update : response.available_updates()) {
            std::cout << "Processing update: " << update.component_name()
                      << " v" << update.version() << std::endl;

            if (DownloadAndApplyUpdate(update)) {
                ReportStatus(update.component_name(), "SUCCESS", "");
                std::cout << "Successfully applied update for " << update.component_name() << std::endl;
            } else {
                ReportStatus(update.component_name(), "FAILED", "Application failed");
                std::cerr << "Failed to apply update for " << update.component_name() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in CheckAndApplyUpdates: " << e.what() << std::endl;
    }
}

bool SimpleOTAClient::DownloadAndApplyUpdate(const ota::UpdateInfo& update) {
    try {
        ota::DownloadRequest dl_request;
        dl_request.set_device_id(device_id);
        dl_request.set_component_name(update.component_name());

        grpc::ClientContext context;
        auto reader = stub->DownloadUpdate(&context, dl_request);

        std::vector<char> file_data;
        ota::DownloadResponse chunk;

        std::cout << "Downloading " << update.component_name() << "..." << std::endl;

        while (reader->Read(&chunk)) {
            const std::string& data = chunk.data();
            file_data.insert(file_data.end(), data.begin(), data.end());

            double progress = (double)chunk.current_size() / chunk.total_size() * 100;
            std::cout << "\rProgress: " << std::fixed << std::setprecision(1)
                      << progress << "%" << std::flush;
        }
        std::cout << std::endl;

        grpc::Status status = reader->Finish();
        if (!status.ok()) {
            std::cerr << "Download failed: " << status.error_message() << std::endl;
            return false;
        }

        std::string calculated_checksum = CalculateChecksum(file_data);
        if (calculated_checksum != update.checksum()) {
            std::cerr << "Checksum mismatch!" << std::endl;
            return false;
        }

        return ApplyUpdate(update, file_data);

    } catch (const std::exception& e) {
        std::cerr << "Exception in DownloadAndApplyUpdate: " << e.what() << std::endl;
        return false;
    }
}

bool SimpleOTAClient::ApplyUpdate(const ota::UpdateInfo& update, const std::vector<char>& data) {
    try {
        if (!update.service_name().empty()) {
            std::string stop_cmd = "sudo systemctl stop " + update.service_name();
            system(stop_cmd.c_str());
        }

        if (std::filesystem::exists(update.target_path())) {
            std::string backup_path = update.target_path() + ".backup";
            std::filesystem::copy_file(update.target_path(), backup_path,
                                       std::filesystem::copy_options::overwrite_existing);
        }

        std::ofstream outfile(update.target_path(), std::ios::binary);
        if (!outfile.is_open()) {
            std::cerr << "Failed to open target file: " << update.target_path() << std::endl;
            return false;
        }

        outfile.write(data.data(), data.size());
        outfile.close();

        if (!update.is_config()) {
            std::string chmod_cmd = "chmod +x " + update.target_path();
            system(chmod_cmd.c_str());
        }

        if (!update.service_name().empty()) {
            if (update.is_service()) {
                system("sudo systemctl daemon-reload");
                std::string enable_cmd = "sudo systemctl enable " + update.service_name();
                system(enable_cmd.c_str());
            }

            std::string restart_cmd = "sudo systemctl restart " + update.service_name();
            if (system(restart_cmd.c_str()) != 0) {
                std::cerr << "Failed to restart service: " << update.service_name() << std::endl;
                return false;
            }

            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::string status_cmd = "systemctl is-active " + update.service_name() + " > /dev/null 2>&1";
            if (system(status_cmd.c_str()) != 0) {
                std::cerr << "Service failed to start: " << update.service_name() << std::endl;
                return false;
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in ApplyUpdate: " << e.what() << std::endl;
        return false;
    }
}

void SimpleOTAClient::ReportStatus(const std::string& component, const std::string& status_str,
                                   const std::string& error_msg) {
    ota::StatusReport request;
    request.set_device_id(device_id);
    request.set_component_name(component);
    request.set_status(status_str);
    request.set_error_message(error_msg);

    ota::StatusResponse response;
    grpc::ClientContext context;

    stub->ReportStatus(&context, request, &response);
}

std::string SimpleOTAClient::CalculateChecksum(const std::vector<char>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.data(), data.size());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}