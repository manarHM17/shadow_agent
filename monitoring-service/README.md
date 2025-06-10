# IOTSHADOW Monitoring Service

## Architecture Overview

This service implements a monitoring solution for IoT devices using a client-server architecture with the following components:

- **Client**: Collects hardware and software metrics locally via a shell script (scheduled with cron), and sends the data (with device ID) to two RabbitMQ queues.
- **Server**: Consumes data from RabbitMQ, stores it in two MySQL tables, analyzes the metrics, and sends alerts to clients via gRPC streaming.

---

## Workflow

### 1. Client Side

- **Metrics Collection**:  
  - A shell script (`collect_metrics.sh`) is executed periodically (e.g., via cron) on the client device.
  - The script collects hardware and software metrics (CPU, memory, disk, USB, GPIO, OS version, applications, services, etc.) and saves them as JSON files in a local logs directory.

- **Data Sending**:  
  - The client application reads the latest metrics files, parses them, and sends the data (including the device ID) to two RabbitMQ queues:
    - `hardware_metrics`
    - `software_metrics`

### 2. Server Side

- **Data Consumption**:  
  - The server application listens to the two RabbitMQ queues.
  - For each message received, it:
    - Stores the data in the corresponding MySQL table (`hardware_info` or `software_info`).
    - Passes the data to the metrics analyzer.

- **Analysis & Alerting**:  
  - The metrics analyzer checks for threshold violations or abnormal states.
  - If an alert condition is detected, an alert is sent to the corresponding client via a gRPC streaming message.

---

## Database

- **MySQL Database**:  
  - Database: `IOTSHADOW`
  - Tables:
    - `hardware_info`: Stores hardware metrics.
    - `software_info`: Stores software metrics.

---

## Technologies Used

- **RabbitMQ**: Message broker for decoupling client and server.
- **MySQL**: Persistent storage for all metrics.
- **gRPC**: Real-time alert streaming from server to client.
- **C++**: Core client and server logic.
- **Bash**: Metrics collection script on client.

---

## Typical Data Flow

1. **Client (cron/script)** → Collects metrics → Writes JSON files
2. **Client (C++ app)** → Reads JSON → Sends to RabbitMQ (hardware/software queues)
3. **Server** → Consumes from RabbitMQ → Stores in MySQL → Analyzes → Sends gRPC alerts to client

---

## Setup Notes

- Ensure RabbitMQ and MySQL are running and accessible.
- Create the `IOTSHADOW` database in MySQL before starting the server.
- The server will auto-create the required tables if they do not exist.
- Configure cron on the client to run `collect_metrics.sh` at the desired interval.

---

## Example Cron Entry (Client)

```
* * * * * /path/to/monitoring-service/client/scripts/collect_metrics.sh
```


