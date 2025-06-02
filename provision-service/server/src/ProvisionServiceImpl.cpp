#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include <ctime>
#include <mysql/mysql.h>
#include <jwt-cpp/jwt.h>
#include "../include/db_handler.hpp"
#include "../include/jwt_handler.hpp"
#include "../include/ProvisionServiceImpl.hpp"
#include "provision.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace shadow_agent;

// Constructor: Initializes the database handler
ProvisionServiceImpl::ProvisionServiceImpl() {
    try {
        db = make_unique<DBHandler>();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error initializing DBHandler: " << e.what() << std::endl;
        throw; // Re-throw the exception to prevent the service from starting
    }
}

// Private function to get the current timestamp in "YYYY-MM-DD HH:MM:SS" format
string ProvisionServiceImpl::getCurrentTimestamp() {
    time_t now = time(nullptr);
    char buf[100];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buf);
}

// Register a new device
Status ProvisionServiceImpl::RegisterDevice(ServerContext* context, const DeviceInfo* request, RegisterDeviceResponse* response) {
    string currentTime = getCurrentTimestamp();

    // Allocate buffers for escaped strings
    char escaped_hostname[request->hostname().length()*2+1];
    char escaped_type[request->type().length()*2+1];
    char escaped_os_type[request->os_type().length()*2+1];
    char escaped_username[request->username().length()*2+1];
    char escaped_time[currentTime.length()*2+1];
    
    // Get MySQL connection
    MYSQL* connection = db->getConnection();
    
    // Escape input strings to prevent SQL injection
    mysql_real_escape_string(connection, escaped_hostname, request->hostname().c_str(), request->hostname().length());
    mysql_real_escape_string(connection, escaped_type, request->type().c_str(), request->type().length());
    mysql_real_escape_string(connection, escaped_os_type, request->os_type().c_str(), request->os_type().length());
    mysql_real_escape_string(connection, escaped_username, request->username().c_str(), request->username().length());
    mysql_real_escape_string(connection, escaped_time, currentTime.c_str(), currentTime.length());
    
    // Insert the device into the database with escaped values
    string query = "INSERT INTO devices (hostname, type, os_type, username, `current_time`) VALUES ('" +
                   string(escaped_hostname) + "', '" + string(escaped_type) + "', '" +
                   string(escaped_os_type) + "', '" + string(escaped_username) + "', '" +
                   string(escaped_time) + "')";

    if (!db->executeQuery(query)) {
        response->set_success(false);
        response->set_message("Database error: " + db->getLastError());
        return Status::OK;
    }

    // Retrieve the auto-incremented device ID
    long long device_id = mysql_insert_id(connection);

    if (device_id == 0) {
        response->set_success(false);
        response->set_message("Failed to retrieve the device ID");
        return Status::OK;
    }

    // Generate a token using the device ID
    string token = JWTUtils::CreateToken(to_string(device_id));

    // Update the token in the database
    string updateQuery = "UPDATE devices SET token = '" + token + "' WHERE id = " + to_string(device_id);
    if (!db->executeQuery(updateQuery)) {
        response->set_success(false);
        response->set_message("Failed to update token in the database: " + db->getLastError());
        return Status::OK;
    }

    // Set the response
    response->set_success(true);
    response->set_message("Device registered successfully with id " + to_string(device_id));
    response->set_token(token);
    response->set_id(static_cast<int32_t>(device_id)); 
    return Status::OK;
}

// Delete a device
Status ProvisionServiceImpl::DeleteDevice(ServerContext* context, const DeviceId* request, Response* response) {
    string device_id;
    if (!JWTUtils::ValidateToken(request->token(), device_id)) {
        response->set_success(false);
        response->set_message("Invalid or expired token");
        return Status::OK;
    }

    if (device_id != to_string(request->id())){
        response->set_success(false);
        response->set_message("Token does not match the device ID");
        return Status::OK;
    }
    string query = "DELETE FROM devices WHERE id = " + to_string(request->id());
    if (!db->executeQuery(query)) {
        response->set_success(false);
        response->set_message("Database error: " + db->getLastError());
        return Status::OK;
    }

    if (mysql_affected_rows(db->getConnection()) == 0) {
        response->set_success(false);
        response->set_message("Device not found");
        return Status::OK;
    }

    response->set_success(true);
    response->set_message("Device deleted successfully");
    return Status::OK;
}

// Update a device
Status ProvisionServiceImpl::UpdateDevice(ServerContext* context, const UpdateDeviceRequest* request, Response* response) {
    string device_id;
    if (!JWTUtils::ValidateToken(request->token(), device_id)) {
        response->set_success(false);
        response->set_message("Invalid or expired token");
        return Status::OK;
    }

    if (device_id != to_string(request->id())) {
        response->set_success(false);
        response->set_message("Token does not match the device ID");
        return Status::OK;
    }

    string currentTime = getCurrentTimestamp();  // Update the current time
    string query = "UPDATE devices SET hostname = '" + request->hostname() +
                   "', type = '" + request->type() + "', os_type = '" +
                   request->os_type() + "', username = '" + request->username() +
                   "', `current_time` = '" + currentTime + "' WHERE id = " + to_string(request->id());

    if (!db->executeQuery(query)) {
        response->set_success(false);
        response->set_message("Database error: " + db->getLastError());
        return Status::OK;
    }

    if (mysql_affected_rows(db->getConnection()) == 0) {
        response->set_success(false);
        response->set_message("Device not found");
        return Status::OK;
    }

    response->set_success(true);
    response->set_message("Device updated successfully");
    return Status::OK;
}

// List all devices
Status ProvisionServiceImpl::ListDevices(ServerContext* context, const ListDeviceRequest* request, DeviceList* response) {
    string device_id;
    if (!JWTUtils::ValidateToken(request->token(), device_id)) {
        return Status(StatusCode::UNAUTHENTICATED, "Invalid or expired token");
    }

    string query = "SELECT id, hostname, type, os_type, username, `current_time`, token FROM devices";
    MYSQL_RES* result = db->executeSelect(query);

    if (!result) {
        return Status(StatusCode::INTERNAL, "Database error: " + db->getLastError());
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        DeviceInfo* device = response->add_devices();
        device->set_id(atoi(row[0]));
        device->set_hostname(row[1]);
        device->set_type(row[2]);
        device->set_os_type(row[3]);
        device->set_username(row[4]);
        device->set_current_time(row[5]);
    }

    mysql_free_result(result);
    return Status::OK;
}

// Get a specific device by ID
Status ProvisionServiceImpl::GetDevice(ServerContext* context, const DeviceId* request, DeviceInfo* response) {
    string device_id;
    if (!JWTUtils::ValidateToken(request->token(), device_id)) {
        return Status(StatusCode::UNAUTHENTICATED, "Invalid or expired token");
    }

    if (device_id != to_string(request->id())) {
        return Status(StatusCode::PERMISSION_DENIED, "Token does not match the device ID");
    }

    string query = "SELECT id, hostname, type, os_type, username, `current_time`, token FROM devices WHERE id = " + 
                   to_string(request->id()); 

    MYSQL_RES* result = db->executeSelect(query);
    if (!result) {
        return Status(StatusCode::INTERNAL, "Database error: " + db->getLastError());
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return Status(StatusCode::NOT_FOUND, "Device not found");
    }

    response->set_id(atoi(row[0]));
    response->set_hostname(row[1]);
    response->set_type(row[2]);
    response->set_os_type(row[3]);
    response->set_username(row[4]);
    response->set_current_time(row[5]);
    mysql_free_result(result);
    return Status::OK;
}
