#include "provisioning_service.h"
#include <iostream>
#include <regex>

ProvisioningServiceImpl::ProvisioningServiceImpl(std::shared_ptr<DatabaseManager> db_manager,
                                               std::shared_ptr<JWTManager> jwt_manager)
    : db_manager_(db_manager), jwt_manager_(jwt_manager) {
}

grpc::Status ProvisioningServiceImpl::Authenticate(grpc::ServerContext* context,
                                                  const provisioning::AuthRequest* request,
                                                  provisioning::AuthResponse* response) {
    try {
        bool auth_success = db_manager_->authenticateDevice(request->hostname(), request->password());
        
        if (auth_success) {
            std::string token = jwt_manager_->generateToken(request->hostname());
            response->set_success(true);
            response->set_jwt_token(token);
        } else {
            response->set_success(false);
            response->set_error_message("Erreur d'authentification");
        }
        
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Erreur interne du serveur");
    }
}

grpc::Status ProvisioningServiceImpl::GetAllDevices(grpc::ServerContext* context,
                                                   const provisioning::GetDevicesRequest* request,
                                                   provisioning::GetDevicesResponse* response) {
    try {
        std::vector<DeviceData> devices = db_manager_->getAllDevices();
        
        for (const auto& device : devices) {
            provisioning::DeviceInfo* device_info = response->add_devices();
            device_info->set_id(device.id);
            device_info->set_hostname(device.hostname);
            device_info->set_user(device.user);
            device_info->set_location(device.location);
            device_info->set_hardware_type(device.hardware_type);
            device_info->set_os_type(device.os_type);
            device_info->set_ip_address(device.ip_address);
            device_info->set_serial_number(device.serial_number);
            device_info->set_created_at(device.created_at);
            device_info->set_updated_at(device.updated_at);
        }
        
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Erreur interne du serveur");
    }
}

grpc::Status ProvisioningServiceImpl::GetDeviceById(grpc::ServerContext* context,
                                                   const provisioning::GetDeviceByIdRequest* request,
                                                   provisioning::GetDeviceByIdResponse* response) {
    try {
        DeviceData device = db_manager_->getDeviceById(request->device_id());
        
        if (device.id == 0) {
            response->set_success(false);
            response->set_error_message("ID introuvable");
        } else {
            response->set_success(true);
            provisioning::DeviceInfo* device_info = response->mutable_device();
            device_info->set_id(device.id);
            device_info->set_hostname(device.hostname);
            device_info->set_user(device.user);
            device_info->set_location(device.location);
            device_info->set_hardware_type(device.hardware_type);
            device_info->set_os_type(device.os_type);
            device_info->set_ip_address(device.ip_address);
            device_info->set_serial_number(device.serial_number);
            device_info->set_created_at(device.created_at);
            device_info->set_updated_at(device.updated_at);
        }
        
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Erreur interne du serveur");
    }
}

bool ProvisioningServiceImpl::validateJWTFromContext(grpc::ServerContext* context) {
    const auto& metadata = context->client_metadata();
    auto auth_header = metadata.find("authorization");
    
    if (auth_header == metadata.end()) {
        return false;
    }
    
    std::string token = std::string(auth_header->second.data(), auth_header->second.size());
    
    // Remove "Bearer " prefix if present
    if (token.substr(0, 7) == "Bearer ") {
        token = token.substr(7);
    }
    
    return jwt_manager_->validateToken(token);
}

std::string ProvisioningServiceImpl::getClientIP(grpc::ServerContext* context) {
    std::string peer = context->peer();
    
    // Extract IP from peer string (format: "ipv4:x.x.x.x:port" or "ipv6:[::1]:port")
    std::regex ip_regex(R"(ipv[46]:(.+):(\d+))");
    std::smatch match;
    
    if (std::regex_search(peer, match, ip_regex)) {
        std::string ip = match[1].str();
        // Remove brackets for IPv6
        if (ip.front() == '[' && ip.back() == ']') {
            ip = ip.substr(1, ip.length() - 2);
        }
        return ip;
    }
    
    return "unknown";
}


grpc::Status ProvisioningServiceImpl::AddDevice(grpc::ServerContext* context,
                                               const provisioning::AddDeviceRequest* request,
                                               provisioning::AddDeviceResponse* response) {
    try {
        // Vérifier si le hostname existe déjà
        if (db_manager_->hostnameExists(request->hostname())) {
            response->set_success(false);
            response->set_error_message("Dispositif déjà enregistré");
            return grpc::Status::OK;
        }
        
        // Créer le dispositif
        DeviceData device;
        device.hostname = request->hostname();
        device.password_hash = request->password();
        device.user = request->user();
        device.location = request->location();
        device.hardware_type = request->hardware_type();
        device.os_type = request->os_type();
        device.ip_address = request->ip_address().empty() ? getClientIP(context) : request->ip_address();
        device.serial_number = request->serial_number();
        
        int device_id = db_manager_->addDevice(device);
        
        if (device_id > 0) {
            std::string token = jwt_manager_->generateToken(request->hostname(), device_id);
            response->set_success(true);
            response->set_device_id(device_id);
            response->set_jwt_token(token);
        } else {
            response->set_success(false);
            response->set_error_message("Échec de l'ajout du dispositif");
        }
        
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Erreur interne du serveur");
    }
}

grpc::Status ProvisioningServiceImpl::DeleteDevice(grpc::ServerContext* context,
                                                  const provisioning::DeleteDeviceRequest* request,
                                                  provisioning::DeleteDeviceResponse* response) {
    try {
        // Vérifier si le dispositif existe
        DeviceData device = db_manager_->getDeviceById(request->device_id());
        if (device.id == 0) {
            response->set_success(false);
            response->set_error_message("ID introuvable");
            return grpc::Status::OK;
        }
        
        bool success = db_manager_->deleteDevice(request->device_id());
        response->set_success(success);
        
        if (!success) {
            response->set_error_message("Échec de la suppression");
        }
        
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Erreur interne du serveur");
    }
}

grpc::Status ProvisioningServiceImpl::UpdateDevice(grpc::ServerContext* context,
                                                  const provisioning::UpdateDeviceRequest* request,
                                                  provisioning::UpdateDeviceResponse* response) {
    try {
        // Vérifier si le dispositif existe
        DeviceData existing_device = db_manager_->getDeviceById(request->device_id());
        if (existing_device.id == 0) {
            response->set_success(false);
            response->set_error_message("ID introuvable");
            return grpc::Status::OK;
        }
        
        // Mettre à jour les informations
        DeviceData updated_device;
        updated_device.user = request->device_info().user();
        updated_device.location = request->device_info().location();
        updated_device.hardware_type = request->device_info().hardware_type();
        updated_device.os_type = request->device_info().os_type();
        updated_device.ip_address = request->device_info().ip_address();
        updated_device.serial_number = request->device_info().serial_number();
        
        bool success = db_manager_->updateDevice(request->device_id(), updated_device);
        response->set_success(success);
        
        if (!success) {
            response->set_error_message("Échec de la mise à jour");
        }
        
        return grpc::Status::OK;
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, "Erreur interne du serveur");
    }
}