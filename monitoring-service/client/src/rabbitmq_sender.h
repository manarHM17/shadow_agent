#pragma once
#include <string>
#include <amqp.h>
#include <nlohmann/json.hpp>
#include "metrics_collector.h"

class RabbitMQSender {
public:
    RabbitMQSender(const std::string& hostname, int port,
                   const std::string& username, const std::string& password,
                   const std::string& hw_queue_name, const std::string& sw_queue_name);
    ~RabbitMQSender();

    bool connect();
    void disconnect();

    bool sendHardwareMetrics(const MetricsCollector::HardwareMetrics& metrics);
    bool sendSoftwareMetrics(const MetricsCollector::SoftwareMetrics& metrics);

private:
    bool sendMessage(const std::string& queue_name, const std::string& message);
    std::string serializeHardwareMetrics(const MetricsCollector::HardwareMetrics& metrics);
    std::string serializeSoftwareMetrics(const MetricsCollector::SoftwareMetrics& metrics);
    bool checkAMQPResponse(amqp_rpc_reply_t x, const char* context);

    std::string hostname_, username_, password_;
    int port_;
    std::string hw_queue_name_, sw_queue_name_;
    amqp_connection_state_t conn_;
    int channel_;
    bool connected_;
};
