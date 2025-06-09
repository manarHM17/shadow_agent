#include "../include/ProvisionClientImpl.h"
#include <iostream>

ProvisioningClient::ProvisioningClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(provisioning::ProvisioningService::NewStub(channel)) {
}

bool ProvisioningClient::Authenticate(const std::string& hostname, const std::string& password, 
                                    std::string& jwt_token) {
    std::string saved_hostname, saved_device_id;
    if (ConfigManager::loadDeviceInfo(saved_hostname, saved_device_id) && 
        saved_hostname == hostname) {
        std::cout << "Using saved device configuration (ID: " << saved_device_id << ")" << std::endl;
    }

    auto context = createContextWithAuth();
    
    provisioning::AuthRequest request;
    request.set_hostname(hostname);
    request.set_password(password);
    
    provisioning::AuthResponse response;
    
    grpc::Status status = stub_->Authenticate(&(*context), request, &response);
    
    if (status.ok() && response.success()) {
        jwt_token = response.jwt_token();
        jwt_token_ = jwt_token;
        std::cout << "Authentication successful" << std::endl;
        return true;
    } else {
        std::cout << "Authentication failed: " << response.error_message() << std::endl;
        return false;
    }
}

bool ProvisioningClient::AddDevice(const std::string& hostname, const std::string& password,
                                 const std::string& user, const std::string& location,
                                 const std::string& hardware_type, const std::string& os_type,
                                 int& device_id, std::string& jwt_token) {
    provisioning::AddDeviceRequest request;
    request.set_hostname(hostname);
    request.set_password(password);
    request.set_user(user);
    request.set_location(location);
    request.set_hardware_type(hardware_type);
    request.set_os_type(os_type);

    provisioning::AddDeviceResponse response;
    grpc::ClientContext context;
    
    grpc::Status status = stub_->AddDevice(&context, request, &response);
    
    if (status.ok() && response.success()) {
        device_id = response.device_id();
        jwt_token = response.jwt_token();
        jwt_token_ = jwt_token;
        
        // Save device info to config file
        if (ConfigManager::saveDeviceInfo(hostname, std::to_string(device_id))) {
            std::cout << "Device configuration saved successfully" << std::endl;
        } else {
            std::cout << "Warning: Failed to save device configuration" << std::endl;
        }
        
        std::cout << "Device added successfully with ID: " << device_id << std::endl;
        return true;
    } else {
        std::cout << "Failed to add device: " << response.error_message() << std::endl;
        return false;
    }
}

bool ProvisioningClient::DeleteDevice(int device_id) {
    provisioning::DeleteDeviceRequest request;
    request.set_device_id(device_id);
    request.set_jwt_token(jwt_token_); // Use stored JWT token

    provisioning::DeleteDeviceResponse response;
    auto context = std::make_unique<grpc::ClientContext>();
    
    grpc::Status status = stub_->DeleteDevice(&(*context), request, &response);
    
    if (status.ok() && response.success()) {
        std::cout << "Device deleted successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to delete device: " << response.error_message() << std::endl;
        return false;
    }
}

bool ProvisioningClient::UpdateDevice(int device_id, const std::string& user, 
                                    const std::string& location,
                                    const std::string& hardware_type, 
                                    const std::string& os_type) {
    provisioning::UpdateDeviceRequest request;
    request.set_device_id(device_id);
    request.set_jwt_token(jwt_token_); // Use stored JWT token
    
    auto device_info = request.mutable_device_info();
    device_info->set_user(user);
    device_info->set_location(location);
    device_info->set_hardware_type(hardware_type);
    device_info->set_os_type(os_type);

    provisioning::UpdateDeviceResponse response;
    auto context = std::make_unique<grpc::ClientContext>();
    
    grpc::Status status = stub_->UpdateDevice(&(*context), request, &response);
    
    if (status.ok() && response.success()) {
        std::cout << "Device updated successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to update device: " << response.error_message() << std::endl;
        return false;
    }
}

void ProvisioningClient::GetAllDevices() {
    provisioning::GetDevicesRequest request;
    request.set_jwt_token(jwt_token_); // Use stored jwt token

    provisioning::GetDevicesResponse response;
    auto context = std::make_unique<grpc::ClientContext>();
    
    grpc::Status status = stub_->GetAllDevices(&(*context), request, &response);
    
    if (status.ok()) {
        std::cout << "=== All Devices ===" << std::endl;
        for (const auto& device : response.devices()) {
            std::cout << "ID: " << device.id() << std::endl;
            std::cout << "Hostname: " << device.hostname() << std::endl;
            std::cout << "User: " << device.user() << std::endl;
            std::cout << "Location: " << device.location() << std::endl;
            std::cout << "Hardware Type: " << device.hardware_type() << std::endl;
            std::cout << "OS Type: " << device.os_type() << std::endl;
            std::cout << "Created At: " << device.created_at() << std::endl;
            std::cout << "Updated At: " << device.updated_at() << std::endl;
            std::cout << "---" << std::endl;
        }
    } else {
        std::cout << "Failed to get devices: " << status.error_message() << std::endl;
    }
}

bool ProvisioningClient::GetDeviceById(int device_id) {
    provisioning::GetDeviceByIdRequest request;
    request.set_device_id(device_id);
    
    provisioning::GetDeviceByIdResponse response;
    auto context = createContextWithAuth();
    
    grpc::Status status = stub_->GetDeviceById(&(*context), request, &response);
    
    if (status.ok() && response.success()) {
        const auto& device = response.device();
        std::cout << "=== Device Information ===" << std::endl;
        std::cout << "ID: " << device.id() << std::endl;
        std::cout << "Hostname: " << device.hostname() << std::endl;
        std::cout << "User: " << device.user() << std::endl;
        std::cout << "Location: " << device.location() << std::endl;
        std::cout << "Hardware Type: " << device.hardware_type() << std::endl;
        std::cout << "OS Type: " << device.os_type() << std::endl;
        std::cout << "Created At: " << device.created_at() << std::endl;
        std::cout << "Updated At: " << device.updated_at() << std::endl;
        return true;
    } else {
        std::cout << "Failed to get device: " << response.error_message() << std::endl;
        return false;
    }
}

std::unique_ptr<grpc::ClientContext> ProvisioningClient::createContextWithAuth() {
    auto context = std::make_unique<grpc::ClientContext>();
    if (!jwt_token_.empty()) {
        context->AddMetadata("authorization", "Bearer " + jwt_token_);
    }
    return context;
}