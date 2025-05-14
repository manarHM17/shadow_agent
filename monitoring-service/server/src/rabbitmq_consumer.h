#pragma once

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <nlohmann/json.hpp>

class RabbitMQConsumer {
public:
    // Callback for when hardware metrics are received
    using HardwareMetricsCallback = std::function<void(const std::string& device_id,
                                                     const nlohmann::json& metrics)>;
    
    // Callback for when software metrics are received
    using SoftwareMetricsCallback = std::function<void(const std::string& device_id,
                                                     const nlohmann::json& metrics)>;
    
    RabbitMQConsumer(const std::string& hostname, int port,
                    const std::string& username, const std::string& password,
                    const std::string& hw_queue_name, const std::string& sw_queue_name);
    ~RabbitMQConsumer();
    
    // Initialize connection and start consumers
    bool start(HardwareMetricsCallback hw_callback, SoftwareMetricsCallback sw_callback);
    
    // Stop consumers and close connection
    void stop();
    
private:
    std::string hostname_;
    int port_;
    std::string username_;
    std::string password_;
    std::string hw_queue_name_;
    std::string sw_queue_name_;
    
    // Callback functions
    HardwareMetricsCallback hw_callback_;
    SoftwareMetricsCallback sw_callback_;
    
    // Connection state
    amqp_connection_state_t hw_conn_;
    amqp_connection_state_t sw_conn_;
    int hw_channel_;
    int sw_channel_;
    
    // Consumer threads
    std::thread hw_thread_;
    std::thread sw_thread_;
    std::atomic<bool> running_;
    
    // Helper for checking AMQP responses
    bool checkAMQPResponse(amqp_rpc_reply_t x, const char* context);
    
    // Thread functions
    void consumeHardwareMetrics();
    void consumeSoftwareMetrics();
    
    // Connect to RabbitMQ
    amqp_connection_state_t connectToRabbitMQ(const std::string& queue_name, int& channel);
};