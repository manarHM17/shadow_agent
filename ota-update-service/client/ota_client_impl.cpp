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
        for (const auto& entry : std::filesystem::directory_iterator("/opt")) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                // Extract app_name and version from filename like app_name_001
                std::string app_name = filename;
                std::string version = "0";
                size_t last_underscore = filename.rfind('_');
                if (last_underscore != std::string::npos && last_underscore + 1 < filename.size()) {
                    app_name = filename.substr(0, last_underscore);
                    version = filename.substr(last_underscore + 1);
                }

                ota::CheckUpdatesRequest request;
                request.set_device_id(device_id);
                request.set_app_name(app_name);
                request.set_current_version(version);

                ota::CheckUpdatesResponse response;
                grpc::ClientContext context;
                grpc::Status status = stub->CheckForUpdates(&context, request, &response);

                if (!status.ok()) {
                    std::cerr << "Failed to check updates for " << app_name << ": " << status.error_message() << std::endl;
                    continue;
                }

                if (!response.has_updates()) {
                    std::cout << "No updates available for " << app_name << std::endl;
                    continue;
                }

                std::cout << "Found " << response.available_updates_size() << " updates for " << app_name << std::endl;

                for (const auto& update : response.available_updates()) {
                    std::cout << "Processing update: " << update.app_name()
                              << " v" << update.version() << std::endl;

                    if (DownloadAndApplyUpdate(update)) {
                        ReportStatus(update.app_name(), "SUCCESS", "");
                        std::cout << "Successfully applied update for " << update.app_name() << std::endl;
                    } else {
                        ReportStatus(update.app_name(), "FAILED", "Application failed");
                        std::cerr << "Failed to apply update for " << update.app_name() << std::endl;
                    }
                }
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
        dl_request.set_app_name(update.app_name());

        grpc::ClientContext context;
        auto reader = stub->DownloadUpdate(&context, dl_request);

        std::vector<char> file_data;
        ota::DownloadResponse chunk;

        std::cout << "Downloading " << update.app_name() << "..." << std::endl;

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
        std::string target_path = "/opt/" + update.app_name() + "_" + update.version();
        if (std::filesystem::exists(target_path)) {
            std::string backup_path = target_path + ".backup";
            std::filesystem::copy_file(target_path, backup_path,
                                       std::filesystem::copy_options::overwrite_existing);
        }

        std::ofstream outfile(target_path, std::ios::binary);
        if (!outfile.is_open()) {
            std::cerr << "Failed to open target file: " << target_path << std::endl;
            return false;
        }

        outfile.write(data.data(), data.size());
        outfile.close();

        std::string chmod_cmd = "chmod +x " + target_path;
        system(chmod_cmd.c_str());

        // Remove all other versioned binaries for this app except the current one
        std::string prefix = update.app_name() + "_";
        for (const auto& entry : std::filesystem::directory_iterator("/opt")) {
            if (entry.is_regular_file()) {
                std::string fname = entry.path().filename().string();
                if (fname.rfind(prefix, 0) == 0 && fname != (update.app_name() + "_" + update.version())) {
                    try {
                        std::filesystem::remove(entry.path());
                    } catch (...) {
                        // Ignore errors
                    }
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in ApplyUpdate: " << e.what() << std::endl;
        return false;
    }
}

void SimpleOTAClient::ReportStatus(const std::string& app_name, const std::string& status_str,
                                   const std::string& error_msg) {
    ota::StatusReport request;
    request.set_device_id(device_id);
    request.set_app_name(app_name);
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