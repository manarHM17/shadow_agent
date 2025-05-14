#pragma once
#include <string>
#include <vector>
#include <memory>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <cstdint>  // pour int32_t

class MenderClient {
public:
    MenderClient(const std::string& mender_server_url, const std::string& tenant_token);
    ~MenderClient();

    bool authenticate(int32_t device_id, const std::string& key);
    bool checkForUpdates(int32_t device_id, const std::string& current_version, std::string& update_id);
    bool downloadArtifact(const std::string& update_id, const std::string& download_path);
    bool reportUpdateStatus(const std::string& deployment_id, int32_t device_id, const std::string& status);

private:
    std::string m_server_url;
    std::string m_tenant_token; // token de mender server
    std::string m_auth_token;   // token après l'authentification
    CURL* m_curl;

    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
    bool performRequest(const std::string& url, const std::string& method, 
                        const nlohmann::json& data, nlohmann::json& response);
};
