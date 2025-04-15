#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "/home/manar/IoT_shadow/services/db/db_handler.hpp"
#include "provision.grpc.pb.h"
#include "../include/ProvisionServiceImpl.h"
#include <ctime>
#include <mysql/mysql.h>

using namespace std;
using namespace grpc;
using namespace shadow_agent;



ProvisionServiceImpl::ProvisionServiceImpl() : db(make_unique<DBHandler>()) {}

string ProvisionServiceImpl::getCurrentTimestamp() {
    time_t now = time(nullptr);
    char buf[100];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buf);
}

Status ProvisionServiceImpl::RegisterDevice(ServerContext* context, const DeviceInfo* request, RegisterDeviceResponse* response) {
    string currentTime = getCurrentTimestamp();  // Obtenir l'heure actuelle
    string query = "INSERT INTO devices (hostname, type, os_type, username, `current_time`) VALUES ('" +
                   request->hostname() + "', '" + request->type() + "', '" +
                   request->os_type() + "', '" + request->username() + "', '" +
                   currentTime + "')";

    if (!db->executeQuery(query)) {
        response->set_success(false);
        response->set_message(db->getLastError());
        return Status::OK;
    }


    response->set_success(true);
    response->set_message("Device registered successfully");
    return Status::OK;
}

Status ProvisionServiceImpl::DeleteDevice(ServerContext* context, const DeviceId* request, Response* response) {
    std::string device_id;

    string query = "DELETE FROM devices WHERE id = " + to_string(request->id());
    if (!db->executeQuery(query)) {
        response->set_success(false);
        response->set_message(db->getLastError());
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

Status ProvisionServiceImpl::UpdateDevice(ServerContext* context, const UpdateDeviceRequest* request, Response* response) {
    std::string device_id;



    string currentTime = getCurrentTimestamp();  // Mettre à jour l'heure actuelle
    string query = "UPDATE devices SET hostname = '" + request->hostname() +
                   "', type = '" + request->type() + "', os_type = '" +
                   request->os_type() + "', username = '" + request->username() +
                   "', `current_time` = '" + currentTime + "' WHERE id = " + to_string(request->id());

    if (!db->executeQuery(query)) {
        response->set_success(false);
        response->set_message(db->getLastError());
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

Status ProvisionServiceImpl::ListDevices(ServerContext* context, const ListDeviceRequest* request, DeviceList* response) {
    // Vérifier le token

    string query = "SELECT id, hostname, type, os_type, username, `current_time` FROM devices";
    MYSQL_RES* result = db->executeSelect(query);

    if (!result) {
        return Status(StatusCode::INTERNAL, db->getLastError());
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

Status ProvisionServiceImpl::GetDevice(ServerContext* context, const DeviceId* request, DeviceInfo* response) {

    string query = "SELECT id, hostname, type, os_type, username, `current_time` FROM devices WHERE id = " + 
                   to_string(request->id()); 

    MYSQL_RES* result = db->executeSelect(query);
    if (!result) {
        return Status(StatusCode::INTERNAL, db->getLastError());
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        mysql_free_result(result);
        return Status(StatusCode::NOT_FOUND, "Device not found");
    }

    response->set_id(atoi(row[0]));  // Conversion correcte de chaîne en int32
    response->set_hostname(row[1]);
    response->set_type(row[2]);
    response->set_os_type(row[3]);
    response->set_username(row[4]);
    response->set_current_time(row[5]);

    mysql_free_result(result);
    return Status::OK;
}