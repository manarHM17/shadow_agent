# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(OTAService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(OpenSSL REQUIRED)

# MySQL
pkg_check_modules(MYSQL REQUIRED mysqlclient)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Generate protobuf and gRPC files
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_FILE "${PROTO_PATH}/ota_service.proto")
set(GENERATED_PROTOBUF_PATH "${CMAKE_CURRENT_BINARY_DIR}")

file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH})

# Custom command to generate protobuf files
add_custom_command(
    OUTPUT "${GENERATED_PROTOBUF_PATH}/ota_service.pb.cc"
           "${GENERATED_PROTOBUF_PATH}/ota_service.pb.h"
           "${GENERATED_PROTOBUF_PATH}/ota_service.grpc.pb.cc"
           "${GENERATED_PROTOBUF_PATH}/ota_service.grpc.pb.h"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out "${GENERATED_PROTOBUF_PATH}"
         --cpp_out "${GENERATED_PROTOBUF_PATH}"
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         -I "${PROTO_PATH}"
         "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
)

# Create protobuf library
add_library(ota_proto
    "${GENERATED_PROTOBUF_PATH}/ota_service.pb.cc"
    "${GENERATED_PROTOBUF_PATH}/ota_service.grpc.pb.cc"
)

target_link_libraries(ota_proto
    gRPC::grpc++
    protobuf::libprotobuf
)

target_include_directories(ota_proto PUBLIC ${GENERATED_PROTOBUF_PATH})

# Server executable
add_executable(ota_server
    server/src/main_server.cpp
    server/src/ota_server.cpp
    server/src/database_manager.cpp
    server/src/file_manager.cpp
)

target_link_libraries(ota_server
    ota_proto
    gRPC::grpc++
    protobuf::libprotobuf
    ${MYSQL_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

target_include_directories(ota_server PRIVATE ${MYSQL_INCLUDE_DIRS})
target_compile_options(ota_server PRIVATE ${MYSQL_CFLAGS_OTHER})

# Client executable
add_executable(ota_client
    client/src/main_client.cpp
    client/src/ota_client.cpp
    client/src/update_manager.cpp
)

target_link_libraries(ota_client
    ota_proto
    gRPC::grpc++
    protobuf::libprotobuf
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

target_include_directories(ota_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include
)

# Set compile options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Install targets
install(TARGETS ota_server ota_client
    RUNTIME DESTINATION bin
)
