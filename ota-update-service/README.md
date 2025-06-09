# OTA Update Service

This project provides a simple Over-The-Air (OTA) update system for applications running on devices (e.g., Raspberry Pi). The system uses gRPC for communication between the server and clients, and MySQL for update metadata storage.

## Features

- Server stores application binaries and metadata (name, version, checksum, path).
- Client checks for updates based on the version extracted from the binary filename (e.g., `my_app_001`).
- Updates are downloaded and applied automatically.
- Only the latest versioned binary is kept on the client.
- Status reporting from client to server.

## Directory Structure

```
ota-update-service/
├── client/           # OTA client source code
├── server/           # OTA server source code
├── proto/            # gRPC .proto definitions
├── test/             # Example/test applications
```

## Usage

### 1. Build the Server

```sh
cd server
mkdir build && cd build
cmake ..
make
```

### 2. Build the Client

```sh
cd ../client
mkdir build && cd build
cmake ..
make
```

### 3. Prepare Application Binaries

- Build your application (e.g., `print_hello.cpp`) with version in the filename:
  ```sh
  g++ -o print_hello_001 ../test/print_hello.cpp
  ```
- Place binaries on the server in:
  ```
  server/updates/app/<app_name>/<app_name>_<version>
  ```

### 4. Insert Update Metadata in MySQL

Insert a row for each version:
```sql
INSERT INTO updates (app_name, version, file_path, checksum)
VALUES ('print_hello', '001', '/absolute/path/to/print_hello_001', '<SHA256_CHECKSUM>');
```
Calculate checksum:
```sh
sha256sum /absolute/path/to/print_hello_001
```

### 5. Run the Server

```sh
cd server/build
./ota_service
```

### 6. Deploy and Run the Client

- Copy the initial binary to `/opt` on the device:
  ```sh
  sudo cp print_hello_001 /opt/print_hello_001
  sudo chmod +x /opt/print_hello_001
  ```
- Run the client:
  ```sh
  cd client/build
  ./ota_test
  ```

## How It Works

- The client scans `/opt` for binaries named like `app_name_version`.
- It extracts `app_name` and `version` from the filename.
- It asks the server if a newer version is available.
- If yes, it downloads and installs `/opt/app_name_<new_version>`, removing older versions.
- The client reports update status to the server.

## Notes

- The client and server must have network access to each other.
- The client must have write permissions to `/opt`.
- The server must have correct file paths and checksums in the database.

## Example

1. Place `/opt/print_hello_001` on the client.
2. Add `/server/updates/app/print_hello/print_hello_002` on the server and insert its metadata in MySQL.
3. The client will detect and apply the update, resulting in `/opt/print_hello_002` on the client.

---

**For more details, see the code and comments in each module.**
