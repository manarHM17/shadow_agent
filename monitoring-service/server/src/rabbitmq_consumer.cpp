#include "rabbitmq_consumer.h"
#include <iostream>
#include <amqp_framing.h>

RabbitMQConsumer::RabbitMQConsumer(const std::string& hostname, int port,
                                 const std::string& username, const std::string& password,
                                 const std::string& hw_queue_name, const std::string& sw_queue_name)
    : hostname_(hostname), port_(port), username_(username), password_(password),
      hw_queue_name_(hw_queue_name), sw_queue_name_(sw_queue_name),
      hw_conn_(nullptr), sw_conn_(nullptr), hw_channel_(1), sw_channel_(1),
      running_(false) {
}

RabbitMQConsumer::~RabbitMQConsumer() {
    stop();
}

bool RabbitMQConsumer::start(HardwareMetricsCallback hw_callback, SoftwareMetricsCallback sw_callback) {
    if (running_) {
        std::cerr << "RabbitMQ consumer already running" << std::endl;
        return false;
    }
    
    hw_callback_ = hw_callback;
    sw_callback_ = sw_callback;
    
    running_ = true;
    
    // Start consumer threads
    hw_thread_ = std::thread(&RabbitMQConsumer::consumeHardwareMetrics, this);
    sw_thread_ = std::thread(&RabbitMQConsumer::consumeSoftwareMetrics, this);
    
    return true;
}

void RabbitMQConsumer::stop() {
    if (running_) {
        running_ = false;
        
        // Wait for threads to finish
        if (hw_thread_.joinable()) {
            hw_thread_.join();
        }
        
        if (sw_thread_.joinable()) {
            sw_thread_.join();
        }
        
        // Close connections
        if (hw_conn_) {
            amqp_channel_close(hw_conn_, hw_channel_, AMQP_REPLY_SUCCESS);
            amqp_connection_close(hw_conn_, AMQP_REPLY_SUCCESS);
            amqp_destroy_connection(hw_conn_);
            hw_conn_ = nullptr;
        }
        
        if (sw_conn_) {
            amqp_channel_close(sw_conn_, sw_channel_, AMQP_REPLY_SUCCESS);
            amqp_connection_close(sw_conn_, AMQP_REPLY_SUCCESS);
            amqp_destroy_connection(sw_conn_);
            sw_conn_ = nullptr;
        }
    }
}

void RabbitMQConsumer::consumeHardwareMetrics() {
    // Connect to RabbitMQ
    hw_conn_ = connectToRabbitMQ(hw_queue_name_, hw_channel_);
    if (!hw_conn_) {
        std::cerr << "Failed to connect to RabbitMQ for hardware metrics" << std::endl;
        return;
    }
    
    // Set up basic consume
    amqp_basic_consume(hw_conn_, hw_channel_, amqp_cstring_bytes(hw_queue_name_.c_str()),
                     amqp_empty_bytes, 0, 0, 0, amqp_empty_table);
    amqp_rpc_reply_t res = amqp_get_rpc_reply(hw_conn_);
    if (!checkAMQPResponse(res, "Starting hardware metrics consumer")) {
        return;
    }
    
    std::cout << "Started consuming hardware metrics from queue " << hw_queue_name_ << std::endl;
    
    // Consume messages
    while (running_) {
        amqp_envelope_t envelope;
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        amqp_maybe_release_buffers(hw_conn_);
        res = amqp_consume_message(hw_conn_, &envelope, &timeout, 0);
        
        if (res.reply_type == AMQP_RESPONSE_NORMAL) {
            // Extract message
            std::string message(static_cast<char*>(envelope.message.body.bytes), envelope.message.body.len);
            
            try {
                // Parse JSON
                nlohmann::json json = nlohmann::json::parse(message);
                
                // Extract device ID
                std::string device_id = json["device_id"];
                
                // Call callback
                hw_callback_(device_id, json);
            } catch (const std::exception& e) {
                std::cerr << "Error processing hardware metrics: " << e.what() << std::endl;
            }
            
            // Acknowledge message
            amqp_basic_ack(hw_conn_, hw_channel_, envelope.delivery_tag, 0);
            
            // Clean up
            amqp_destroy_envelope(&envelope);
        } else if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION &&
                  res.library_error == AMQP_STATUS_TIMEOUT) {
            // Timeout, just continue
        } else {
            // Error
            std::cerr << "Error consuming hardware metrics: " << res.reply_type << std::endl;
            break;
        }
    }
    
    std::cout << "Hardware metrics consumer stopped" << std::endl;
}

void RabbitMQConsumer::consumeSoftwareMetrics() {
    // Connect to RabbitMQ
    sw_conn_ = connectToRabbitMQ(sw_queue_name_, sw_channel_);
    if (!sw_conn_) {
        std::cerr << "Failed to connect to RabbitMQ for software metrics" << std::endl;
        return;
    }
    
    // Set up basic consume
    amqp_basic_consume(sw_conn_, sw_channel_, amqp_cstring_bytes(sw_queue_name_.c_str()),
                     amqp_empty_bytes, 0, 0, 0, amqp_empty_table);
    amqp_rpc_reply_t res = amqp_get_rpc_reply(sw_conn_);
    if (!checkAMQPResponse(res, "Starting software metrics consumer")) {
        return;
    }
    
    std::cout << "Started consuming software metrics from queue " << sw_queue_name_ << std::endl;
    
    // Consume messages
    while (running_) {
        amqp_envelope_t envelope;
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        amqp_maybe_release_buffers(sw_conn_);
        res = amqp_consume_message(sw_conn_, &envelope, &timeout, 0);
        
        if (res.reply_type == AMQP_RESPONSE_NORMAL) {
            // Extract message
            std::string message(static_cast<char*>(envelope.message.body.bytes), envelope.message.body.len);
            
            try {
                // Parse JSON
                nlohmann::json json = nlohmann::json::parse(message);
                
                // Extract device ID
                std::string device_id = json["device_id"];
                
                // Call callback
                sw_callback_(device_id, json);
            } catch (const std::exception& e) {
                std::cerr << "Error processing software metrics: " << e.what() << std::endl;
            }
            
            // Acknowledge message
            amqp_basic_ack(sw_conn_, sw_channel_, envelope.delivery_tag, 0);
            
            // Clean up
            amqp_destroy_envelope(&envelope);
        } else if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION &&
                  res.library_error == AMQP_STATUS_TIMEOUT) {
            // Timeout, just continue
        } else {
            // Error
            std::cerr << "Error consuming software metrics: " << res.reply_type << std::endl;
            break;
        }
    }
    
    std::cout << "Software metrics consumer stopped" << std::endl;
}

amqp_connection_state_t RabbitMQConsumer::connectToRabbitMQ(const std::string& queue_name, int& channel) {
    // Create connection
    amqp_connection_state_t conn = amqp_new_connection();
    if (!conn) {
        std::cerr << "Failed to create AMQP connection" << std::endl;
        return nullptr;
    }
    
    // Create socket
    amqp_socket_t* socket = amqp_tcp_socket_new(conn);
    if (!socket) {
        std::cerr << "Failed to create TCP socket" << std::endl;
        amqp_destroy_connection(conn);
        return nullptr;
    }
    
    // Open socket
    int status = amqp_socket_open(socket, hostname_.c_str(), port_);
    if (status != AMQP_STATUS_OK) {
        std::cerr << "Failed to open TCP socket: " << status << std::endl;
        amqp_destroy_connection(conn);
        return nullptr;
    }
    
    // Login
    amqp_rpc_reply_t reply = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
                                       username_.c_str(), password_.c_str());
    if (!checkAMQPResponse(reply, "Logging in")) {
        amqp_destroy_connection(conn);
        return nullptr;
    }
    
    // Open channel
    amqp_channel_open(conn, channel);
    reply = amqp_get_rpc_reply(conn);
    if (!checkAMQPResponse(reply, "Opening channel")) {
        amqp_destroy_connection(conn);
        return nullptr;
    }
    
    // Declare queue
    amqp_queue_declare(conn, channel, amqp_cstring_bytes(queue_name.c_str()),
                      0, 1, 0, 0, amqp_empty_table);
    reply = amqp_get_rpc_reply(conn);
    if (!checkAMQPResponse(reply, "Declaring queue")) {
        amqp_destroy_connection(conn);
        return nullptr;
    }
    
    return conn;
}

bool RabbitMQConsumer::checkAMQPResponse(amqp_rpc_reply_t x, const char* context) {
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