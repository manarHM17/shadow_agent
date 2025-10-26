# IOTSHADOW - IoT Device Management Platform 

## Overview

IOTSHADOW is a comprehensive IoT device management platform that consists of three main microservices:

- **Provision Service**: Handles device registration, authentication, and configuration
- **Monitoring Service**: Collects and analyzes device metrics, provides real-time alerts
- **OTA Update Service**: Manages over-the-air software updates for devices

## System Architecture

```
<img width="3964" height="1644" alt="image" src="https://github.com/user-attachments/assets/348cef6a-8aa6-4b0a-adbb-b4390608e158" />

```

## Services

### 1. Provision Service
- Device registration and authentication
- JWT-based security
- Configuration management
- [More details](./provision-service/README.md)

### 2. Monitoring Service
- Hardware/software metrics collection
- Real-time alerts
- Performance analysis
- [More details](./monitoring-service/README.md)
<img width="2124" height="988" alt="image" src="https://github.com/user-attachments/assets/a1b11167-92f6-46ca-bcb6-834dc2764e3a" />


### 3. OTA Update Service
- Remote software updates
- Version management
- Update status tracking
- [More details](./ota-update-service/README.md)
<img width="4164" height="1044" alt="image" src="https://github.com/user-attachments/assets/5dcf4910-fbf7-4b63-aa9f-dbb234893c86" />

## Infrastructure

All services use shared infrastructure defined in `docker-compose.yml`:

- **MySQL**: Central database for all services
- **RabbitMQ**: Message broker for monitoring service
- **phpMyAdmin**: Database management interface

## Prerequisites

- Docker and Docker Compose
- C++ 17 or higher
- gRPC 1.62.1 or higher
- OpenSSL 3.0 or higher
- CMake 3.10 or higher
- Boost 1.83.0 or higher

## Quick Start

1. Start the infrastructure:
```bash
docker-compose up -d
```

2. Build and start each service:
```bash
# Build Provision Service
cd provision-service
mkdir build && cd build
cmake .. && make

# Build Monitoring Service
cd ../../monitoring-service
mkdir build && cd build
cmake .. && make

# Build OTA Update Service
cd ../../ota-update-service
mkdir build && cd build
cmake .. && make
```

3. Access Services:
- Provision Service: gRPC on port 50051
- Monitoring Service: gRPC on port 50052
- OTA Update Service: gRPC on port 50053
- RabbitMQ Management
- phpMyAdmin

## Configuration

Each service has its own configuration files and requirements. Please refer to individual service READMEs for detailed setup instructions.

## Development

- Use the provided Docker environment for consistent development
- Follow C++ coding standards
- Submit PRs with appropriate tests
- Update documentation as needed

## Security

- JWT authentication for device communication
- TLS/SSL encryption for gRPC connections
- Secure password storage
- Rate limiting and access controls
