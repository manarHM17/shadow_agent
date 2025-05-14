#include <grpcpp/grpcpp.h>
#include <memory>
#include <iostream>
#include <string>

#include "monitoring.grpc.pb.h"
#include "rabbitmq_consumer.h"
#include "metrics_analyzer.h"
#include "alert_manager.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerWriter;
using grpc::Status;

class MonitoringServiceImpl final : public monitoring::MonitoringService::Service {
public:
    explicit MonitoringServiceImpl(AlertManager* alert_manager)
        : alert_manager_(alert_manager) {}

    Status RegisterDevice(ServerContext* context,
                         const monitoring::DeviceInfo* request,
                         ServerWriter<monitoring::Alert>* writer) override {
        std::string device_id = request->device_id();
        std::cout << "Registering device: " << device_id << std::endl;

        // Register the device with AlertManager using raw pointer
        alert_manager_->registerDevice(device_id, writer);

        // Keep the stream open until client disconnects or context is cancelled
        while (!context->IsCancelled()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Unregister device when stream is closed
        alert_manager_->unregisterDevice(device_id);
        return Status::OK;
    }

    Status SendStatusUpdate(ServerContext* context,
                           const monitoring::StatusUpdate* request,
                           monitoring::StatusResponse* response) override {
        std::string device_id = request->device_id();
        std::string message = request->message();

        std::cout << "Status update from device " << device_id << ": " << message << std::endl;

        response->set_success(true);
        response->set_message("Status update received");

        return Status::OK;
    }

private:
    AlertManager* alert_manager_;
};

void RunServer(const std::string& rabbitmq_host, int rabbitmq_port,
               const std::string& rabbitmq_username, const std::string& rabbitmq_password,
               const std::string& hw_queue, const std::string& sw_queue,
               const std::string& thresholds_path, const std::string& grpc_address) {
    AlertManager alert_manager;
    MetricsAnalyzer metrics_analyzer(&alert_manager, thresholds_path);
    RabbitMQConsumer rabbitmq_consumer(
        rabbitmq_host, rabbitmq_port,
        rabbitmq_username, rabbitmq_password,
        hw_queue, sw_queue
    );

    auto hw_callback = [&metrics_analyzer](const std::string& device_id, const nlohmann::json& metrics) {
        metrics_analyzer.processHardwareMetrics(device_id, metrics);
    };

    auto sw_callback = [&metrics_analyzer](const std::string& device_id, const nlohmann::json& metrics) {
        metrics_analyzer.processSoftwareMetrics(device_id, metrics);
    };

    if (!rabbitmq_consumer.start(hw_callback, sw_callback)) {
        std::cerr << "Failed to start RabbitMQ consumer" << std::endl;
        return;
    }

    MonitoringServiceImpl service(&alert_manager);
    ServerBuilder builder;
    builder.AddListeningPort(grpc_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << grpc_address << std::endl;

    server->Wait();

    rabbitmq_consumer.stop();
}

int main(int argc, char** argv) {
    std::string rabbitmq_host = "localhost";
    int rabbitmq_port = 5672;
    std::string rabbitmq_username = "guest";
    std::string rabbitmq_password = "guest";
    std::string hw_queue = "hardware_metrics";
    std::string sw_queue = "software_metrics";
    std::string thresholds_path = "thresholds.json";
    std::string grpc_address = "0.0.0.0:50051";

    RunServer(rabbitmq_host, rabbitmq_port, rabbitmq_username, rabbitmq_password,
              hw_queue, sw_queue, thresholds_path, grpc_address);

    return 0;
}