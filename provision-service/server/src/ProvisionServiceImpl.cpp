#include "server/include/ProvisionServiceImpl.h"
#include <iostream>
#include <regex>

ProvisioningServiceImpl::ProvisioningServiceImpl(std::shared_ptr<DBHandler> db_manager,
                                               std::shared_ptr<JWTUtils> jwt_manager)
    : db_manager_(db_manager), jwt_manager_(jwt_manager) {
    try {
        DBHandler db;
        std::cout << "✓ Service de provisionnement initialisé avec succès" << std::endl;
    } catch (const std::runtime_error& e) {
        std::cerr << "✗ Erreur critique: Impossible d'initialiser la base de données: " << e.what() << std::endl;
    }
}

// ============================================================================
// AUTHENTIFICATION
// ============================================================================

grpc::Status ProvisioningServiceImpl::Authenticate(grpc::ServerContext* context,
                                                  const provisioning::AuthRequest* request,
                                                  provisioning::AuthResponse* response) {
    std::cout << "🔐 Tentative de connexion pour: " << request->hostname() << std::endl;
    
    try {
        // Validation des données d'entrée
        if (request->hostname().empty()) {
            response->set_success(false);
            response->set_error_message("Le nom d'hôte ne peut pas être vide");
            std::cout << "✗ Connexion refusée: Nom d'hôte manquant" << std::endl;
            return grpc::Status::OK;
        }
        
        if (request->password().empty()) {
            response->set_success(false);
            response->set_error_message("Le mot de passe ne peut pas être vide");
            std::cout << "✗ Connexion refusée: Mot de passe manquant" << std::endl;
            return grpc::Status::OK;
        }
        
        // Vérification de l'authentification
        bool auth_success = db_manager_->authenticateDevice(request->hostname(), request->password());
        
        if (auth_success) {
            try {
                DeviceData device = db_manager_->getDeviceByHostname(request->hostname());
                std::string token = jwt_manager_->CreateToken(request->hostname(), std::to_string(device.id));
                
                response->set_success(true);
                response->set_jwt_token(token);
                std::cout << "✓ Connexion réussie pour: " << request->hostname() 
                         << " (ID: " << device.id << ")" << std::endl;
            } catch (const std::exception& e) {
                response->set_success(false);
                response->set_error_message("Erreur lors de la génération du token de connexion");
                std::cout << "✗ Erreur token pour: " << request->hostname() << std::endl;
            }
        } else {
            response->set_success(false);
            response->set_error_message("Nom d'hôte ou mot de passe incorrect");
            std::cout << "✗ Connexion refusée: Identifiants invalides pour " << request->hostname() << std::endl;
        }
        
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Erreur critique lors de l'authentification: " << e.what() << std::endl;
        response->set_success(false);
        response->set_error_message("Erreur technique temporaire. Veuillez réessayer");
        return grpc::Status::OK;
    }
}

// ============================================================================
// AJOUT DE DISPOSITIF (ENREGISTREMENT)
// ============================================================================

grpc::Status ProvisioningServiceImpl::AddDevice(grpc::ServerContext* context,
                                               const provisioning::AddDeviceRequest* request,
                                               provisioning::AddDeviceResponse* response) {
    std::cout << "📱 Tentative d'enregistrement: " << request->hostname() << std::endl;
    
    try {
        // Validation des données obligatoires
        if (request->hostname().empty()) {
            response->set_success(false);
            response->set_error_message("Le nom d'hôte est obligatoire");
            return grpc::Status::OK;
        }
        
        if (request->password().empty()) {
            response->set_success(false);
            response->set_error_message("Le mot de passe est obligatoire");
            return grpc::Status::OK;
        }
        
        if (request->user().empty()) {
            response->set_success(false);
            response->set_error_message("Le nom d'utilisateur est obligatoire");
            return grpc::Status::OK;
        }
        
        // Vérifier si le dispositif existe déjà
        if (db_manager_->hostnameExists(request->hostname())) {
            response->set_success(false);
            response->set_error_message("Ce dispositif est déjà enregistré. Utilisez la connexion");
            std::cout << "✗ Enregistrement refusé: " << request->hostname() << " existe déjà" << std::endl;
            return grpc::Status::OK;
        }
        
        // Créer le nouveau dispositif
        DeviceData device;
        device.hostname = request->hostname();
        device.password_hash = request->password();
        device.user = request->user();
        device.location = request->location().empty() ? "Non spécifié" : request->location();
        device.hardware_type = request->hardware_type().empty() ? "Non spécifié" : request->hardware_type();
        device.os_type = request->os_type().empty() ? "Non spécifié" : request->os_type();
        
        int device_id = db_manager_->addDevice(device);
        
        if (device_id > 0) {
            try {
                std::string token = jwt_manager_->CreateToken(device.hostname, std::to_string(device_id));
                response->set_success(true);
                response->set_device_id(device_id);
                response->set_jwt_token(token);
                std::cout << "✓ Dispositif enregistré avec succès: " << request->hostname() 
                         << " (ID: " << device_id << ")" << std::endl;
            } catch (const std::exception& e) {
                response->set_success(false);
                response->set_error_message("Dispositif créé mais erreur de connexion automatique");
                std::cout << "⚠ Dispositif créé mais erreur token: " << request->hostname() << std::endl;
            }
        } else {
            response->set_success(false);
            response->set_error_message("Impossible d'enregistrer le dispositif. Vérifiez vos informations");
            std::cout << "✗ Échec enregistrement: " << request->hostname() << std::endl;
        }
        
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Erreur critique lors de l'enregistrement: " << e.what() << std::endl;
        response->set_success(false);
        response->set_error_message("Erreur technique temporaire. Veuillez réessayer");
        return grpc::Status::OK;
    }
}

// ============================================================================
// RÉCUPÉRATION DE TOUS LES DISPOSITIFS
// ============================================================================

grpc::Status ProvisioningServiceImpl::GetAllDevices(grpc::ServerContext* context,
                                                   const provisioning::GetDevicesRequest* request,
                                                   provisioning::GetDevicesResponse* response) {
    std::cout << "📋 Demande de liste des dispositifs" << std::endl;
    
    try {
        // Validation du token JWT
        if (request->jwt_token().empty()) {
            std::cout << "✗ Accès refusé: Token manquant" << std::endl;
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Vous devez être connecté pour accéder à cette fonction");
        }
        
        std::string device_id, hostname;
        if (!jwt_manager_->ValidateToken(request->jwt_token(), hostname, device_id)) {
            std::cout << "✗ Accès refusé: Token invalide" << std::endl;
            return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Votre session a expiré. Veuillez vous reconnecter");
        }
        
        // Récupération des dispositifs
        std::vector<DeviceData> devices = db_manager_->getAllDevices();
        std::cout << "✓ " << devices.size() << " dispositif(s) trouvé(s)" << std::endl;
        
        for (const auto& device : devices) {
            provisioning::DeviceInfo* device_info = response->add_devices();
            device_info->set_id(device.id);
            device_info->set_hostname(device.hostname);
            device_info->set_user(device.user);
            device_info->set_location(device.location);
            device_info->set_hardware_type(device.hardware_type);
            device_info->set_os_type(device.os_type);
            device_info->set_created_at(device.created_at);
            device_info->set_updated_at(device.updated_at);
        }
        
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Erreur lors de la récupération des dispositifs: " << e.what() << std::endl;
        return grpc::Status(grpc::StatusCode::INTERNAL, "Erreur technique temporaire. Veuillez réessayer");
    }
}

// ============================================================================
// RÉCUPÉRATION D'UN DISPOSITIF PAR ID
// ============================================================================

grpc::Status ProvisioningServiceImpl::GetDeviceById(grpc::ServerContext* context,
                                                   const provisioning::GetDeviceByIdRequest* request,
                                                   provisioning::GetDeviceByIdResponse* response) {
    std::cout << "🔍 Recherche dispositif ID: " << request->device_id() << std::endl;
    
    try {
        // Validation du token JWT
        if (!validateJWTFromContext(context)) {
            std::cout << "✗ Accès refusé: Token invalide ou manquant" << std::endl;
            response->set_success(false);
            response->set_error_message("Votre session a expiré. Veuillez vous reconnecter");
            return grpc::Status::OK;
        }
        
        // Validation de l'ID
        if (request->device_id() <= 0) {
            response->set_success(false);
            response->set_error_message("ID de dispositif invalide");
            return grpc::Status::OK;
        }
        
        // Recherche du dispositif
        DeviceData device = db_manager_->getDeviceById(request->device_id());
        
        if (device.id == 0) {
            response->set_success(false);
            response->set_error_message("Aucun dispositif trouvé avec cet ID");
            std::cout << "✗ Dispositif non trouvé: ID " << request->device_id() << std::endl;
        } else {
            response->set_success(true);
            provisioning::DeviceInfo* device_info = response->mutable_device();
            device_info->set_id(device.id);
            device_info->set_hostname(device.hostname);
            device_info->set_user(device.user);
            device_info->set_location(device.location);
            device_info->set_hardware_type(device.hardware_type);
            device_info->set_os_type(device.os_type);
            device_info->set_created_at(device.created_at);
            device_info->set_updated_at(device.updated_at);
            std::cout << "✓ Dispositif trouvé: " << device.hostname << std::endl;
        }
        
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Erreur lors de la recherche: " << e.what() << std::endl;
        response->set_success(false);
        response->set_error_message("Erreur technique temporaire. Veuillez réessayer");
        return grpc::Status::OK;
    }
}

// ============================================================================
// SUPPRESSION DE DISPOSITIF
// ============================================================================

grpc::Status ProvisioningServiceImpl::DeleteDevice(grpc::ServerContext* context,
                                                 const provisioning::DeleteDeviceRequest* request,
                                                 provisioning::DeleteDeviceResponse* response) {
    std::cout << "🗑️ Tentative suppression dispositif ID: " << request->device_id() << std::endl;
    
    try {
        // Validation du token JWT
        if (request->jwt_token().empty()) {
            response->set_success(false);
            response->set_error_message("Vous devez être connecté pour supprimer un dispositif");
            return grpc::Status::OK;
        }
        
        std::string device_id, hostname;
        if (!jwt_manager_->ValidateToken(request->jwt_token(), hostname, device_id)) {
            response->set_success(false);
            response->set_error_message("Votre session a expiré. Veuillez vous reconnecter");
            std::cout << "✗ Suppression refusée: Token invalide" << std::endl;
            return grpc::Status::OK;
        }
        
        // Validation de l'ID
        if (request->device_id() <= 0) {
            response->set_success(false);
            response->set_error_message("ID de dispositif invalide");
            return grpc::Status::OK;
        }
        
        // Vérifier que le dispositif existe avant suppression
        DeviceData existing_device = db_manager_->getDeviceById(request->device_id());
        if (existing_device.id == 0) {
            response->set_success(false);
            response->set_error_message("Le dispositif à supprimer n'existe pas");
            std::cout << "✗ Suppression impossible: Dispositif ID " << request->device_id() << " introuvable" << std::endl;
            return grpc::Status::OK;
        }
        
        // Suppression
        bool success = db_manager_->deleteDevice(request->device_id());
        
        if (success) {
            response->set_success(true);
            std::cout << "✓ Dispositif supprimé: " << existing_device.hostname 
                     << " (ID: " << request->device_id() << ")" << std::endl;
        } else {
            response->set_success(false);
            response->set_error_message("Impossible de supprimer le dispositif. Veuillez réessayer");
            std::cout << "✗ Échec suppression dispositif ID: " << request->device_id() << std::endl;
        }
        
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Erreur lors de la suppression: " << e.what() << std::endl;
        response->set_success(false);
        response->set_error_message("Erreur technique temporaire. Veuillez réessayer");
        return grpc::Status::OK;
    }
}

// ============================================================================
// MISE À JOUR DE DISPOSITIF
// ============================================================================

grpc::Status ProvisioningServiceImpl::UpdateDevice(grpc::ServerContext* context,
                                                const provisioning::UpdateDeviceRequest* request,
                                                provisioning::UpdateDeviceResponse* response) {
    std::cout << "✏️ Tentative mise à jour dispositif ID: " << request->device_id() << std::endl;
    
    try {
        // Validation du token JWT
        if (request->jwt_token().empty()) {
            response->set_success(false);
            response->set_error_message("Vous devez être connecté pour modifier un dispositif");
            return grpc::Status::OK;
        }
        
        std::string device_id, hostname;
        if (!jwt_manager_->ValidateToken(request->jwt_token(), hostname, device_id)) {
            response->set_success(false);
            response->set_error_message("Votre session a expiré. Veuillez vous reconnecter");
            std::cout << "✗ Mise à jour refusée: Token invalide" << std::endl;
            return grpc::Status::OK;
        }
        
        // Validation de l'ID
        if (request->device_id() <= 0) {
            response->set_success(false);
            response->set_error_message("ID de dispositif invalide");
            return grpc::Status::OK;
        }
        
        // Vérifier que le dispositif existe
        DeviceData existing_device = db_manager_->getDeviceById(request->device_id());
        if (existing_device.id == 0) {
            response->set_success(false);
            response->set_error_message("Le dispositif à modifier n'existe pas");
            std::cout << "✗ Mise à jour impossible: Dispositif ID " << request->device_id() << " introuvable" << std::endl;
            return grpc::Status::OK;
        }
        
        // Validation des données à mettre à jour
        if (request->device_info().user().empty()) {
            response->set_success(false);
            response->set_error_message("Le nom d'utilisateur ne peut pas être vide");
            return grpc::Status::OK;
        }
        
        // Préparation des nouvelles données
        DeviceData updated_device;
        updated_device.user = request->device_info().user();
        updated_device.location = request->device_info().location().empty() ? "Non spécifié" : request->device_info().location();
        updated_device.hardware_type = request->device_info().hardware_type().empty() ? "Non spécifié" : request->device_info().hardware_type();
        updated_device.os_type = request->device_info().os_type().empty() ? "Non spécifié" : request->device_info().os_type();
        
        // Mise à jour
        bool success = db_manager_->updateDevice(request->device_id(), updated_device);
        
        if (success) {
            response->set_success(true);
            std::cout << "✓ Dispositif mis à jour: " << existing_device.hostname 
                     << " (ID: " << request->device_id() << ")" << std::endl;
        } else {
            response->set_success(false);
            response->set_error_message("Impossible de mettre à jour le dispositif. Veuillez réessayer");
            std::cout << "✗ Échec mise à jour dispositif ID: " << request->device_id() << std::endl;
        }
        
        return grpc::Status::OK;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Erreur lors de la mise à jour: " << e.what() << std::endl;
        response->set_success(false);
        response->set_error_message("Erreur technique temporaire. Veuillez réessayer");
        return grpc::Status::OK;
    }
}

// ============================================================================
// UTILITAIRES PRIVÉS
// ============================================================================

bool ProvisioningServiceImpl::validateJWTFromContext(grpc::ServerContext* context) {
    const auto& metadata = context->client_metadata();
    auto auth_header = metadata.find("authorization");
    
    if (auth_header == metadata.end()) {
        std::cout << "⚠ Token JWT manquant dans les métadonnées" << std::endl;
        return false;
    }
    
    std::string token = std::string(auth_header->second.data(), auth_header->second.size());
    
    // Supprimer le préfixe "Bearer " si présent
    if (token.substr(0, 7) == "Bearer ") {
        token = token.substr(7);
    }
    
    std::string device_id, hostname;
    bool valid = jwt_manager_->ValidateToken(token, hostname, device_id);
    
    if (!valid) {
        std::cout << "⚠ Token JWT invalide ou expiré" << std::endl;
    }
    
    return valid;
}