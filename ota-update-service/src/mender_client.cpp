#include "mender_client.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

MenderClient::MenderClient(const std::string& mender_server_url, const std::string& tenant_token)
    : m_server_url(mender_server_url), m_tenant_token(tenant_token), m_auth_token("") {
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    m_curl = curl_easy_init();
    
    if (!m_curl) {
        throw std::runtime_error("Failed to initialize curl");
    }
}

MenderClient::~MenderClient() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
    curl_global_cleanup();
}

bool MenderClient::authenticate(int32_t device_id, const std::string& key) {
    nlohmann::json auth_data = {
        {"id", device_id},
        {"device_key", key}
    };
    
    nlohmann::json response;
    bool success = performRequest(m_server_url + "/api/devices/v1/authentication/auth_requests", 
                                "POST", auth_data, response);
    
    if (success && response.contains("token")) {
        m_auth_token = response["token"].get<std::string>();
        return true;
    }
    
    return false;
}

bool MenderClient::checkForUpdates(int32_t device_id, 
                                  const std::string& current_version,
                                  std::string& update_id) {
    nlohmann::json device_info = {
        {"device_id", device_id},
        {"artifact_name", current_version},
        {"device_type", "raspberry-pi"}
    };
    
    nlohmann::json response;
    bool success = performRequest(m_server_url + "/api/devices/v1/deployments/device/deployments/next", 
                                "POST", device_info, response);
    
    if (success && !response.empty()) {
        if (response.contains("id")) {
            update_id = response["id"].get<std::string>();
            return true;
        }
    }
    
    return false;
}

bool MenderClient::downloadArtifact(const std::string& update_id, const std::string& download_path) {
    if (m_auth_token.empty()) {
        std::cerr << "Not authenticated. Call authenticate() first." << std::endl;
        return false;
    }
    
    CURL* download_curl = curl_easy_init();
    if (!download_curl) {
        std::cerr << "Failed to initialize curl for download" << std::endl;
        return false;
    }
    
    std::string url = m_server_url + "/api/devices/v1/deployments/device/deployments/" + update_id + "/download";
    
    std::ofstream output_file(download_path, std::ios::binary);
    if (!output_file.is_open()) {
        std::cerr << "Failed to open file for writing: " << download_path << std::endl;
        curl_easy_cleanup(download_curl);
        return false;
    }
    
    curl_easy_setopt(download_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(download_curl, CURLOPT_WRITEFUNCTION, 
        [](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
            std::ofstream* file = static_cast<std::ofstream*>(userdata);
            file->write(ptr, size * nmemb);
            return size * nmemb;
        });
    curl_easy_setopt(download_curl, CURLOPT_WRITEDATA, &output_file);
    
    struct curl_slist* headers = NULL;
    std::string auth_header = "Authorization: Bearer " + m_auth_token;
    headers = curl_slist_append(headers, auth_header.c_str());
    curl_easy_setopt(download_curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(download_curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(download_curl);
    output_file.close();
    
    return (res == CURLE_OK);
}

bool MenderClient::reportUpdateStatus(const std::string& deployment_id, 
                                    int32_t device_id,
                                    const std::string& status) {
    nlohmann::json status_data = {
        {"device_id", device_id},
        {"deployment_id", deployment_id},
        {"status", status}
    };
    
    nlohmann::json response;
    return performRequest(m_server_url + "/api/devices/v1/deployments/device/deployments/" + 
                        deployment_id + "/status",
                        "PUT", status_data, response);
}

size_t MenderClient::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* response = static_cast<std::string*>(userdata);
    response->append(ptr, size * nmemb);
    return size * nmemb;
}

bool MenderClient::performRequest(const std::string& url, const std::string& method, 
                                const nlohmann::json& data, nlohmann::json& response) {
    curl_easy_reset(m_curl);
    
    std::string response_string;
    std::string data_string;
    
    if (!data.empty()) {
        data_string = data.dump();
    }
    
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response_string);
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    if (!m_tenant_token.empty()) {
        std::string tenant_header = "X-MEN-Tenant: " + m_tenant_token;
        headers = curl_slist_append(headers, tenant_header.c_str());
    }
    
    if (!m_auth_token.empty()) {
        std::string auth_header = "Authorization: Bearer " + m_auth_token;
        headers = curl_slist_append(headers, auth_header.c_str());
    }
    
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, headers);
    
    if (method == "POST") {
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, data_string.c_str());
    } else if (method == "PUT") {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, data_string.c_str());
    }
    
    CURLcode res = curl_easy_perform(m_curl);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
    
    long http_code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    if (http_code >= 200 && http_code < 300) {
        try {
            if (!response_string.empty()) {
                response = nlohmann::json::parse(response_string);
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse JSON response: " << e.what() << std::endl;
            return false;
        }
    }
    
    std::cerr << "HTTP error: " << http_code << std::endl;
    return false;
}