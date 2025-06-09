#pragma once

#include <grpcpp/grpcpp.h>
#include "provisioning.grpc.pb.h"
#include <memory>
#include <string>

class ProvisioningClient {
public:
    ProvisioningClient(std::shared_ptr<grpc::Channel> channel);
    
    bool Authenticate(const std::string& hostname, const std::string& password, std::string& jwt_token);
    bool AddDevice(const std::string& hostname, const std::string& password,
                   const std::string& user, const std::string& location,
                   const std::string& hardware_type, const std::string& os_type,
                   const std::string& serial_number, int& device_id, std::string& jwt_token);
    bool DeleteDevice(int device_id);
    bool UpdateDevice(int device_id, const std::string& user, const std::string& location,
                     const std::string& hardware_type, const std::string& os_type,
                     const std::string& ip_address, const std::string& serial_number);
    void GetAllDevices();
    bool GetDeviceById(int device_id);

private:
    std::unique_ptr<provisioning::ProvisioningService::Stub> stub_;
    std::string jwt_token_;
    
    grpc::ClientContext createContextWithAuth();
};

// client/src/provisioning_client.cpp
#include "provisioning_client.h"
#include <iostream>

ProvisioningClient::ProvisioningClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(provisioning::ProvisioningService::NewStub(channel)) {
}

bool ProvisioningClient::Authenticate(const std::string& hostname, const std::string& password, std::string& jwt_token) {
    provisioning::AuthRequest request;
    request.set_hostname(hostname);
    request.set_password(password);
    
    provisioning::AuthResponse response;
    grpc::ClientContext context;
    
    grpc::Status status = stub_->Authenticate(&context, request, &response);
    
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
                                  const std::string& serial_number, int& device_id, std::string& jwt_token) {
    provisioning::AddDeviceRequest request;
    request.set_hostname(hostname);
    request.set_password(password);
    request.set_user(user);
    request.set_location(location);
    request.set_hardware_type(hardware_type);
    request.set_os_type(os_type);
    request.set_serial_number(serial_number);
    
    provisioning::AddDeviceResponse response;
    grpc::ClientContext context;
    
    grpc::Status status = stub_->AddDevice(&context, request, &response);
    
    if (status.ok() && response.success()) {
        device_id = response.device_id();
        jwt_token = response.jwt_token();
        jwt_token_ = jwt_token;
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
    
    provisioning::DeleteDeviceResponse response;
    grpc::ClientContext context = createContextWithAuth();
    
    grpc::Status status = stub_->DeleteDevice(&context, request, &response);
    
    if (status.ok() && response.success()) {
        std::cout << "Device deleted successfully" << std::endl;
        return true;
    } else {
        std::cout << "Failed to delete device: " << response.error_message() << std::endl;
        return false;
    }
}

bool ProvisioningClient::UpdateDevice(int device_id, const std::string& user, const std::string& location,
                                     const std::string& hardware_type, const std::string& os_type,
                                     const std::string& ip_address, const std::string& serial_number) {
    provisioning::UpdateDeviceRequest request;
    request.set_device_id(device_id);
    
    provisioning::DeviceInfo* device_info = request.mutable_device_info();
    device_info->set_user(user);
    device_info->set_location(location);
    device_info->set_hardware_type(hardware_type);
    device_info->set_os_type(os_type);
    device_info->set_ip_address(ip_address);
    device_info->set_serial_number(serial_number);
    
    provisioning::UpdateDeviceResponse response;
    grpc::ClientContext context = createContextWithAuth();
    
    grpc::Status status = stub_->UpdateDevice(&context, request, &response);
    
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
    provisioning::GetDevicesResponse response;
    grpc::ClientContext context = createContextWithAuth();
    
    grpc::Status status = stub_->GetAllDevices(&context, request, &response);
    
    if (status.ok()) {
        std::cout << "=== All Devices ===" << std::endl;
        for (const auto& device : response.devices()) {
            std::cout << "ID: " << device.id() << std::endl;
            std::cout << "Hostname: " << device.hostname() << std::endl;
            std::cout << "User: " << device.user() << std::endl;
            std::cout << "Location: " << device.location() << std::endl;
            std::cout << "Hardware Type: " << device.hardware_type() << std::endl;
            std::cout << "OS Type: " << device.os_type() << std::endl;
            std::cout << "IP Address: " << device.ip_address() << std::endl;
            std::cout << "Serial Number: " << device.serial_number() << std::endl;
            std::cout << "Created At: " << device.created_at() << std::endl;
            std::cout << "Updated At: " << device.updated_at() << std::endl;
            std::cout << "---" << std::endl;
        }
    } else {
        std::cout << "Failed to get devices" << std::endl;
    }
}

bool ProvisioningClient::GetDeviceById(int device_id) {
    provisioning::GetDeviceByIdRequest request;
    request.set_device_id(device_id);
    
    provisioning::GetDeviceByIdResponse response;
    grpc::ClientContext context = createContextWithAuth();
    
    grpc::Status status = stub_->GetDeviceById(&context, request, &response);
    
    if (status.ok() && response.success()) {
        const auto& device = response.device();
        std::cout << "=== Device Information ===" << std::endl;
        std::cout << "ID: " << device.id() << std::endl;
        std::cout << "Hostname: " << device.hostname() << std::endl;
        std::cout << "User: " << device.user() << std::endl;
        std::cout << "Location: " << device.location() << std::endl;
        std::cout << "Hardware Type: " << device.hardware_type() << std::endl;
        std::cout << "OS Type: " << device.os_type() << std::endl;
        std::cout << "IP Address: " << device.ip_address() << std::endl;
        std::cout << "Serial Number: " << device.serial_number() << std::endl;
        std::cout << "Created At: " << device.created_at() << std::endl;
        std::cout << "Updated At: " << device.updated_at() << std::endl;
        return true;
    } else {
        std::cout << "Failed to get device: " << response.error_message() << std::endl;
        return false;
    }
}

grpc::ClientContext ProvisioningClient::createContextWithAuth() {
    grpc::ClientContext context;
    if (!jwt_token_.empty()) {
        context.AddMetadata("authorization", "Bearer " + jwt_token_);
    }
    return context;
}