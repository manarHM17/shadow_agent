#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib> // Pour system()

#include <grpcpp/grpcpp.h>
#include "monitoring.grpc.pb.h"

#include "metrics_collector.h"
#include "rabbitmq_sender.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::Status;
using namespace monitoring;

class MonitoringClient {
public:
    MonitoringClient(std::shared_ptr<Channel> channel, 
                    const std::string& amqp_uri,
                    const std::string& hardware_queue,
                    const std::string& software_queue)
        : stub_(MonitoringService::NewStub(channel)),
          metrics_collector_(std::make_unique<MetricsCollector>("../../client/logs")), 
          rabbitmq_sender_(std::make_unique<RabbitMQSender>(
              "localhost", 5672, "guest", "guest", hardware_queue, software_queue)),
          running_(false) {
            // Connect to RabbitMQ
            if (!rabbitmq_sender_->connect()) {
                throw std::runtime_error("Failed to connect to RabbitMQ");}
            }

    // Register device with the monitoring server
std::string RegisterDevice() {
    DeviceInfo device_info;
    device_info.set_device_id(metrics_collector_->getDeviceId());

    ClientContext* context = new ClientContext();
    std::unique_ptr<ClientReader<Alert>> reader(
        stub_->RegisterDevice(context, device_info));

    // Move the reader to a shared_ptr to safely pass it into the thread
    auto shared_reader = std::shared_ptr<ClientReader<Alert>>(std::move(reader));

    alert_thread_ = std::thread([this, shared_reader, context]() {
        Alert alert;
        while (shared_reader->Read(&alert)) {
            this->ProcessAlert(alert);
        }
        Status status = shared_reader->Finish();
        if (!status.ok()) {
            std::cerr << "Alert stream failed: " << status.error_message() << std::endl;
        }
        delete context;  // Clean up context
    });

    return metrics_collector_->getDeviceId();
}


    // Start monitoring and sending metrics
    void StartMonitoring(const std::string& device_id) {
        running_ = true;
        
        metrics_thread_ = std::thread([this, device_id]() {
            while (running_) {
                try {
                    // Collect and send hardware metrics
                    auto [hw_file, sw_file] = metrics_collector_->collectMetrics();
                    auto hw_metrics = metrics_collector_->parseHardwareMetrics(hw_file);
                    auto sw_metrics = metrics_collector_->parseSoftwareMetrics(sw_file);
                    
                    rabbitmq_sender_->sendHardwareMetrics(hw_metrics);
                    rabbitmq_sender_->sendSoftwareMetrics(sw_metrics);
                    
                    // Send status update (simplified heartbeat)
                    SendStatusUpdate("Metrics collected and sent");
                    
                } catch (const std::exception& e) {
                    std::cerr << "Error in metrics collection: " << e.what() << std::endl;
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(60));
            }
        });
    }

    void StopMonitoring() {
        running_ = false;
        if (metrics_thread_.joinable()) {
            metrics_thread_.join();
        }
        if (alert_thread_.joinable()) {
            alert_thread_.join();
        }
    }

private:
    void SendStatusUpdate(const std::string& message) {
        StatusUpdate update;
        update.set_device_id(metrics_collector_->getDeviceId());
        update.set_message(message);
        
        StatusResponse response;
        ClientContext context;
        
        Status status = stub_->SendStatusUpdate(&context, update, &response);
        
        if (!status.ok()) {
            std::cerr << "Failed to send status update: " << status.error_message() << std::endl;
        }
    }

    void ExecuteCorrectiveCommand(const std::string& cmds) {
        if (!cmds.empty()) {
            std::istringstream iss(cmds);
            std::string cmd;
            while (std::getline(iss, cmd, ';')) {
                if (!cmd.empty()) {
                    std::cout << "[INFO] Executing corrective command: " << cmd << std::endl;
                    int ret = system(cmd.c_str());
                    if (ret != 0) {
                        std::cerr << "[ERROR] Command failed with code: " << ret << std::endl;
                    }
                }
            }
        }
    }

    void ProcessAlert(const Alert& alert) {
        std::cout << "=== ALERT RECEIVED ===" << std::endl;
        std::cout << "Type: " << alert.alert_type() << std::endl;
        std::cout << "Severity: " << Alert::Severity_Name(alert.severity()) << std::endl;
        std::cout << "Description: " << alert.description() << std::endl;
        std::cout << "Recommended Action: " << alert.recommended_action() << std::endl;
        std::cout << "Timestamp: " << alert.timestamp() << std::endl;
        if (!alert.corrective_command().empty()) {
            std::cout << "Corrective Command(s): " << alert.corrective_command() << std::endl;
            ExecuteCorrectiveCommand(alert.corrective_command());
        }
        std::cout << "=====================" << std::endl;
    }

    std::unique_ptr<MonitoringService::Stub> stub_;
    std::unique_ptr<MetricsCollector> metrics_collector_;
    std::unique_ptr<RabbitMQSender> rabbitmq_sender_;
    std::atomic<bool> running_;
    std::thread metrics_thread_;
    std::thread alert_thread_;
};

int main(int argc, char** argv) {
    try {
        std::string server_address = "localhost:50051";
        std::string amqp_uri = "amqp://guest:guest@localhost:5672";
        std::string hardware_queue = "hardware_metrics";
        std::string software_queue = "software_metrics";
        

        MonitoringClient client(
            grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()),
            amqp_uri,
            hardware_queue,
            software_queue
        );

        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::string device_id = client.RegisterDevice();
        if (device_id.empty()) {
            std::cerr << "Failed to register device" << std::endl;
            return 1;
        }

        std::cout << "Successfully registered device: " << device_id << std::endl;
        client.StartMonitoring(device_id);
        
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
