#include "rabbitmq_sender.h"
#include <iostream>
#include <amqp_framing.h>
#include <nlohmann/json.hpp>
#include <amqp_tcp_socket.h>

RabbitMQSender::RabbitMQSender(const std::string& hostname, int port,
                             const std::string& username, const std::string& password,
                             const std::string& hw_queue_name, const std::string& sw_queue_name)
    : hostname_(hostname), port_(port), username_(username), password_(password),
      hw_queue_name_(hw_queue_name), sw_queue_name_(sw_queue_name),
      conn_(nullptr), channel_(1), connected_(false) {
}

RabbitMQSender::~RabbitMQSender() {
    disconnect();
}

bool RabbitMQSender::connect() {
    // Create connection
    conn_ = amqp_new_connection();
    if (!conn_) {
        std::cerr << "Failed to create AMQP connection" << std::endl;
        return false;
    }
    
    // Create socket
    amqp_socket_t* socket = amqp_tcp_socket_new(conn_);
    if (!socket) {
        std::cerr << "Failed to create TCP socket" << std::endl;
        return false;
    }
    
    // Open socket
    int status = amqp_socket_open(socket, hostname_.c_str(), port_);
    if (status != AMQP_STATUS_OK) {
    std::cerr << "Failed to open TCP socket: " << status << std::endl;
    amqp_destroy_connection(conn_);
    conn_ = nullptr;
    return false;
    }

    
    // Login
    amqp_rpc_reply_t reply = amqp_login(conn_, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
                                       username_.c_str(), password_.c_str());
    if (!checkAMQPResponse(reply, "Logging in")) {
        return false;
    }
    
    // Open channel
    amqp_channel_open(conn_, channel_);
    reply = amqp_get_rpc_reply(conn_);
    if (!checkAMQPResponse(reply, "Opening channel")) {
        return false;
    }
    
    // Declare hardware queue
    amqp_queue_declare(conn_, channel_, amqp_cstring_bytes(hw_queue_name_.c_str()),
                      0, 1, 0, 0, amqp_empty_table);
    reply = amqp_get_rpc_reply(conn_);
    if (!checkAMQPResponse(reply, "Declaring hardware queue")) {
        return false;
    }
    
    // Declare software queue
    amqp_queue_declare(conn_, channel_, amqp_cstring_bytes(sw_queue_name_.c_str()),
                      0, 1, 0, 0, amqp_empty_table);
    reply = amqp_get_rpc_reply(conn_);
    if (!checkAMQPResponse(reply, "Declaring software queue")) {
        return false;
    }
    
    connected_ = true;
    return true;
}

void RabbitMQSender::disconnect() {
if (conn_) {
    if (connected_) {
        amqp_channel_close(conn_, channel_, AMQP_REPLY_SUCCESS);
        amqp_connection_close(conn_, AMQP_REPLY_SUCCESS);
    }
    amqp_destroy_connection(conn_);
    conn_ = nullptr;
    connected_ = false;
}
}

bool RabbitMQSender::sendHardwareMetrics(const MetricsCollector::HardwareMetrics& metrics) {
    if (!connected_) {
        if (!connect()) {
            std::cerr << "Reconnect failed" << std::endl;
            return false;
        }
    }

    std::string message = serializeHardwareMetrics(metrics);
    return sendMessage(hw_queue_name_, message);
}

bool RabbitMQSender::sendSoftwareMetrics(const MetricsCollector::SoftwareMetrics& metrics) {
    if (!connected_ && !connect()) {
        std::cerr << "Reconnect failed" << std::endl;
        return false;
    }
    
    std::string message = serializeSoftwareMetrics(metrics);
    return sendMessage(sw_queue_name_, message);
}

bool RabbitMQSender::sendMessage(const std::string& queue_name, const std::string& message) {
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2; // persistent delivery
    
    // Publish message
    int status = amqp_basic_publish(conn_, channel_,
                                  amqp_cstring_bytes(""), // exchange
                                  amqp_cstring_bytes(queue_name.c_str()), // routing key
                                  0, // mandatory
                                  0, // immediate
                                  &props,
                                  amqp_cstring_bytes(message.c_str()));
    amqp_rpc_reply_t reply = amqp_get_rpc_reply(conn_);
    if (!checkAMQPResponse(reply, "Publishing message")) {
        return false;
    }

    
    if (status != AMQP_STATUS_OK) {
        std::cerr << "Failed to publish message to " << queue_name << ": " << status << std::endl;
        return false;
    }
    
    return true;
}

std::string RabbitMQSender::serializeHardwareMetrics(const MetricsCollector::HardwareMetrics& metrics) {
    nlohmann::json json;
    json["device_id"] = metrics.device_id;
    json["readable_date"] = metrics.readable_date;
    json["cpu_usage"] = metrics.cpu_usage;
    json["memory_usage"] = metrics.memory_usage;
    json["disk_usage"] = metrics.disk_usage_root;
    json["usb_state"] = metrics.usb_data;
    json["gpio_state"] = metrics.gpio_state;
    json["kernel_version"] = metrics.kernel_version;
    json["hardware_model"] = metrics.hardware_model;
    json["firmware_version"] = metrics.firmware_version;
    return json.dump();
}

std::string RabbitMQSender::serializeSoftwareMetrics(const MetricsCollector::SoftwareMetrics& metrics) {
    nlohmann::json json;
    json["device_id"] = metrics.device_id;
    json["readable_date"] = metrics.readable_date;
    json["ip_address"] = metrics.ip_address;
    json["uptime"] = metrics.uptime;
    json["network_status"] = metrics.network_status;
    json["os_version"] = metrics.os_version;
    // Serialize applications
    nlohmann::json apps = nlohmann::json::array();
    for (const auto& [name, version] : metrics.applications) {
        apps.push_back({{"name", name}, {"version", version}});
    }
    json["applications"] = apps;
    // Serialize services
    nlohmann::json services;
    for (const auto& [name, status] : metrics.services) {
        services[name] = status;
    }
    json["services"] = services;
    return json.dump();
}

bool RabbitMQSender::checkAMQPResponse(amqp_rpc_reply_t x, const char* context) {
    switch (x.reply_type) {
        case AMQP_RESPONSE_NORMAL:
            return true;
            
        case AMQP_RESPONSE_NONE:
            std::cerr << context << ": missing RPC reply" << std::endl;
            break;
            
        case AMQP_RESPONSE_LIBRARY_EXCEPTION:
            std::cerr << context << ": " << amqp_error_string2(x.library_error) << std::endl;
            break;
            
        case AMQP_RESPONSE_SERVER_EXCEPTION:
            switch (x.reply.id) {
                case AMQP_CONNECTION_CLOSE_METHOD: {
                    amqp_connection_close_t* m = static_cast<amqp_connection_close_t*>(x.reply.decoded);
                    std::cerr << context << ": server connection error " << m->reply_code << ", message: "
                              << std::string(static_cast<char*>(m->reply_text.bytes), m->reply_text.len) << std::endl;
                    break;
                }
                case AMQP_CHANNEL_CLOSE_METHOD: {
                    amqp_channel_close_t* m = static_cast<amqp_channel_close_t*>(x.reply.decoded);
                    std::cerr << context << ": server channel error " << m->reply_code << ", message: "
                              << std::string(static_cast<char*>(m->reply_text.bytes), m->reply_text.len) << std::endl;
                    break;
                }
                default:
                    std::cerr << context << ": unknown server error, method id " << x.reply.id << std::endl;
                    break;
            }
            break;
    }
    
    return false;
}