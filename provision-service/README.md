# IoT Device Provisioning Service

A gRPC-based service for managing IoT device provisioning, authentication, and configuration management.

## Features

- Device registration and authentication
- JWT-based secure communication
- Device management (add, update, delete)
- Configuration persistence
- Multi-device support
- MySQL database backend

## Prerequisites

- C++ 17 or higher
- gRPC 1.62.1 or higher
- MySQL Server 8.0 or higher
- OpenSSL 3.0 or higher
- CMake 3.10 or higher
- Boost 1.83.0 or higher

## Project Structure

```
provision-service/
├── client/
│   ├── include/
│   │   ├── ProvisionClientImpl.h
│   │   └── ConfigManager.h
│   ├── src/
│   │   ├── ProvisionClientImpl.cpp
│   │   ├── ConfigManager.cpp
│   │   └── grpc_client.cpp
│   └── config/
│       └── config.txt
├── server/
│   ├── include/
│   │   └── ProvisionServiceImpl.h
│   └── src/
│       ├── ProvisionServiceImpl.cpp
│       └── grpc_server.cpp
├── common/
│   ├── include/
│   │   ├── db_handler.h
│   │   └── jwt_handler.h
│   └── src/
│       ├── db_handler.cpp
│       └── jwt_handler.cpp
└── proto/
    └── provisioning.proto
```

## Building the Project

1. Create build directories:
```bash
mkdir -p server/build client/build
```

2. Build the server:
```bash
cd server/build
cmake ..
make
```

3. Build the client:
```bash
cd client/build
cmake ..
make
```

## Database Setup

1. Install MySQL Server:
```bash
sudo apt install mysql-server
```

2. Create the database:
```sql
CREATE DATABASE IOTSHADOW;
```

## Running the Service

1. Start the server:
```bash
cd server/build
./provisioning_service
```

2. Run the client:
```bash
cd client/build
./provision_test
```

## Usage

### Device Registration
```bash
./provision_test
# Choose option 2 for device registration
# Follow the prompts to enter device information
```

### Device Management
- Option 1: Authenticate device
- Option 2: Register new device
- Option 3: Delete device
- Option 4: Update device information
- Option 5: List all devices
- Option 6: Get device by ID

## Configuration

Device configurations are stored in:
- Client: `client/config/config.txt`
- Server: MySQL database

## Security

- JWT-based authentication
- Secure password hashing
- Unique device identification
- Connection limiting per device

## Error Handling

The service includes comprehensive error handling for:
- Database connection issues
- Authentication failures
- Invalid device operations
- Configuration file access

