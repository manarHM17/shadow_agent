#include "ota_client.h"
#include "../../common/include/logging.h"
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <fstream>
#include <thread>
#include <chrono>


// Global variables for signal handling
static bool g_running = true;
static std::unique_ptr<OTAClient> g_ota_client = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    LOG_INFO("Received signal: " + std::to_string(signal));
    g_running = false;
    
    if (g_ota_client) {
        g_ota_client->stop();
    }
}

// Display usage information
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "OTA Client for Raspberry Pi firmware updates\n\n"
              << "Options:\n"
              << "  -h, --help                 Show this help message\n"
              << "  -c, --config FILE          Configuration file path (default: /etc/ota/client.conf)\n"
              << "  -s, --server ADDRESS       OTA server address (default: localhost:50051)\n"
              << "  -i, --client-id ID         Client identifier (default: auto-generated)\n"
              << "  -m, --hardware-model MODEL Hardware model (default: auto-detect)\n"
              << "  -t, --temp-dir PATH        Temporary directory (default: /tmp/ota)\n"
              << "  -v, --verbose              Enable verbose logging\n"
              << "  -d, --daemon               Run as daemon\n"
              << "  --check-now                Check for updates immediately and exit\n"
              << "  --status                   Show current status and exit\n"
              << "  --cancel-update            Cancel current update if in progress\n"
              << "  --rollback                 Perform system rollback\n"
              << "  --check-interval SECONDS   Update check interval (default: 3600)\n"
              << "  --status-interval SECONDS  Status report interval (default: 300)\n"
              << std::endl;
}

// Load configuration from file
bool loadConfigFromFile(const std::string& config_file, ClientConfig& config) {
    LOG_INFO("Loading configuration from: " + config_file);
    
    std::ifstream file(config_file);
    if (!file.is_open()) {
        LOG_WARNING("Configuration file not found: " + config_file);
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        size_t equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            continue;
        }
        
        std::string key = line.substr(0, equals_pos);
        std::string value = line.substr(equals_pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Parse configuration values
        if (key == "server_address") {
            config.server_address = value;
        } else if (key == "device_id") {
            config.device_id = std::stoi(value);
        } else if (key == "hardware_model") {
            config.hardware_model = value;
        } else if (key == "temp_directory") {
            config.temp_directory = value;
        } else if (key == "check_interval_seconds") {
            config.check_interval_seconds = std::stoi(value);
        } else if (key == "status_report_interval_seconds") {
            config.status_report_interval_seconds = std::stoi(value);
        } else if (key == "max_retry_attempts") {
            config.max_retry_attempts = std::stoi(value);
        } else if (key == "retry_delay_seconds") {
            config.retry_delay_seconds = std::stoi(value);
        }
    }
    
    file.close();
    LOG_INFO("Configuration loaded successfully");
    return true;
}

// Status callback function
void statusCallback(const std::string& message, UpdateState state) {
    std::string state_str;
    switch (state) {
        case UpdateState::IDLE: state_str = "IDLE"; break;
        case UpdateState::DOWNLOADING: state_str = "DOWNLOADING"; break;
        case UpdateState::DOWNLOAD_COMPLETED: state_str = "DOWNLOAD_COMPLETED"; break;
        case UpdateState::VERIFYING: state_str = "VERIFYING"; break;
        case UpdateState::INSTALLING: state_str = "INSTALLING"; break;
        case UpdateState::REBOOT_REQUIRED: state_str = "REBOOT_REQUIRED"; break;
        case UpdateState::UPDATE_SUCCESS: state_str = "UPDATE_SUCCESS"; break;
        case UpdateState::UPDATE_FAILED: state_str = "UPDATE_FAILED"; break;
        case UpdateState::ROLLBACK_IN_PROGRESS: state_str = "ROLLBACK_IN_PROGRESS"; break;
        case UpdateState::ROLLBACK_COMPLETED: state_str = "ROLLBACK_COMPLETED"; break;
        default: state_str = "UNKNOWN"; break;
    }
    
    LOG_INFO("Status Update: [" + state_str + "] " + message);
    
    // If running interactively, also print to console
    if (!isatty(STDOUT_FILENO)) {
        std::cout << "[" << state_str << "] " << message << std::endl;
    }
}

// Display current client status
bool showStatus(OTAClient& client) {
    UpdateProgress progress = client.getUpdateProgress();
    
    std::cout << "OTA Client Status:" << std::endl;
    std::cout << "==================" << std::endl;
    
    std::string state_str;
    switch (progress.state) {
        case UpdateState::IDLE: state_str = "IDLE"; break;
        case UpdateState::DOWNLOADING: state_str = "DOWNLOADING"; break;
        case UpdateState::DOWNLOAD_COMPLETED: state_str = "DOWNLOAD_COMPLETED"; break;
        case UpdateState::VERIFYING: state_str = "VERIFYING"; break;
        case UpdateState::INSTALLING: state_str = "INSTALLING"; break;
        case UpdateState::REBOOT_REQUIRED: state_str = "REBOOT_REQUIRED"; break;
        case UpdateState::UPDATE_SUCCESS: state_str = "UPDATE_SUCCESS"; break;
        case UpdateState::UPDATE_FAILED: state_str = "UPDATE_FAILED"; break;
        case UpdateState::ROLLBACK_IN_PROGRESS: state_str = "ROLLBACK_IN_PROGRESS"; break;
        case UpdateState::ROLLBACK_COMPLETED: state_str = "ROLLBACK_COMPLETED"; break;
        default: state_str = "UNKNOWN"; break;
    }
    
    std::cout << "State: " << state_str << std::endl;
    std::cout << "Operation: " << progress.current_operation << std::endl;
    std::cout << "Progress: " << progress.progress_percentage << "%" << std::endl;
    
    if (!progress.error_message.empty()) {
        std::cout << "Error: " << progress.error_message << std::endl;
    }
    
    return true;
}

// Run as daemon
bool daemonize() {
    pid_t pid = fork();
    
    if (pid < 0) {
        LOG_ERROR("Failed to fork daemon process");
        return false;
    }
    
    if (pid > 0) {
        // Parent process exits
        exit(EXIT_SUCCESS);
    }
    
    // Child process continues
    if (setsid() < 0) {
        LOG_ERROR("Failed to create new session");
        return false;
    }
    
    // Change working directory to root
    if (chdir("/") < 0) {
        LOG_ERROR("Failed to change working directory");
        return false;
    }
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    LOG_INFO("Running as daemon");
    return true;
}

int main(int argc, char* argv[]) {
    // Default configuration
    ClientConfig config;
    config.server_address = "localhost:50051";
    config.device_id = 0;
    config.hardware_model = "";  // Auto-detect
    config.temp_directory = "/tmp/ota";
    config.check_interval_seconds = 3600;     // 1 hour
    config.status_report_interval_seconds = 300;  // 5 minutes
    config.max_retry_attempts = 3;
    config.retry_delay_seconds = 60;
    
    // Command line options
    std::string config_file = "/etc/ota/client.conf";
    bool verbose = false;
    bool run_daemon = false;
    bool check_now = false;
    bool show_status_only = false;
    bool cancel_update = false;
    bool perform_rollback = false;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'c'},
        {"server", required_argument, 0, 's'},
        {"device-id", required_argument, 0, 'i'},
        {"hardware-model", required_argument, 0, 'm'},
        {"temp-dir", required_argument, 0, 't'},
        {"verbose", no_argument, 0, 'v'},
        {"daemon", no_argument, 0, 'd'},
        {"check-now", no_argument, 0, 1000},
        {"status", no_argument, 0, 1001},
        {"cancel-update", no_argument, 0, 1002},
        {"rollback", no_argument, 0, 1003},
        {"check-interval", required_argument, 0, 1004},
        {"status-interval", required_argument, 0, 1005},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "hc:s:i:m:t:vd", long_options, nullptr)) != -1) {
        switch (c) {
            case 'h':
                printUsage(argv[0]);
                return EXIT_SUCCESS;
            case 'c':
                config_file = optarg;
                break;
            case 's':
                config.server_address = optarg;
                break;
            case 'i':
                config.device_id = std::stoi(optarg);
                break;
            case 'm':
                config.hardware_model = optarg;
                break;
            case 't':
                config.temp_directory = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'd':
                run_daemon = true;
                break;
            case 1000:
                check_now = true;
                break;
            case 1001:
                show_status_only = true;
                break;
            case 1002:
                cancel_update = true;
                break;
            case 1003:
                perform_rollback = true;
                break;
            case 1004:
                config.check_interval_seconds = std::stoi(optarg);
                break;
            case 1005:
                config.status_report_interval_seconds = std::stoi(optarg);
                break;
            case '?':
                printUsage(argv[0]);
                return EXIT_FAILURE;
            default:
                break;
        }
    }
    
    // Initialize logging
    if (verbose) {
        Logger::getInstance().setLevel(LogLevel::DEBUG);
    } else {
        Logger::getInstance().setLevel(LogLevel::INFO);
    }
    
    // Load configuration file if it exists
    loadConfigFromFile(config_file, config);
    
    LOG_INFO("Starting OTA Client");
    LOG_INFO("Device ID: " + std::to_string(config.device_id));
    LOG_INFO("Server: " + config.server_address);
    
    // Create OTA client
    g_ota_client = std::make_unique<OTAClient>(config);
    
    // Initialize client
    if (!g_ota_client->initialize()) {
        LOG_ERROR("Failed to initialize OTA client");
        return EXIT_FAILURE;
    }
    
    // Set status callback
    g_ota_client->setStatusCallback(statusCallback);
    
    // Handle special commands
    if (show_status_only) {
        return showStatus(*g_ota_client) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    
    if (cancel_update) {
        LOG_INFO("Cancelling current update...");
        bool result = g_ota_client->cancelCurrentUpdate();
        return result ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    
    if (perform_rollback) {
        LOG_INFO("Performing system rollback...");
        bool result = g_ota_client->performRollback();
        return result ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    
    if (check_now) {
        LOG_INFO("Checking for updates now...");
        bool result = g_ota_client->checkForUpdatesNow();
        return result ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    
    // Run as daemon if requested
    if (run_daemon) {
        if (!daemonize()) {
            LOG_ERROR("Failed to daemonize");
            return EXIT_FAILURE;
        }
    }
    
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGHUP, signalHandler);
    
    // Start the OTA client
    g_ota_client->start();
    
    LOG_INFO("OTA Client is running. Press Ctrl+C to stop.");
    
    // Main loop
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    LOG_INFO("Shutting down OTA Client...");
    g_ota_client->stop();
    g_ota_client.reset();
    
    LOG_INFO("OTA Client stopped.");
    return EXIT_SUCCESS;
}
