cmake_minimum_required(VERSION 3.15)
project(ota_service)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Protobuf
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using protobuf ${protobuf_VERSION}")

# Protobuf-compiler
set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)

# gRPC
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")
set(_GRPC_GRPCPP gRPC::grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

# MySQL
find_library(MYSQL_LIB mysqlclient REQUIRED)
include_directories(/usr/include/mysql)

include_directories(/usr/include)
include_directories(/usr/include/jsoncpp/json)
#find_package(nlohmann_json REQUIRED)
#include_directories(${nlohmann_json_INCLUDE_DIRS})

# JWT
include_directories(/usr/local/include/jwt-cpp)

# OpenSSL (since jwt depends on it)
find_package(OpenSSL REQUIRED)

# Boost
find_package(Boost REQUIRED)


# Proto file
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../proto")

# === ota_serviceing Service ===
set(ota_service_PROTO "${PROTO_PATH}/ota_service.proto")
set(ota_service_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/ota_service.pb.cc")
set(ota_service_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/ota_service.pb.h")
set(ota_service_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/ota_service.grpc.pb.cc")
set(ota_service_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/ota_service.grpc.pb.h")

add_custom_command(
    OUTPUT ${ota_service_proto_srcs} ${ota_service_proto_hdrs} ${ota_service_grpc_srcs} ${ota_service_grpc_hdrs}
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         -I "${PROTO_PATH}"
         --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${ota_service_PROTO}"
    DEPENDS "${ota_service_PROTO}"
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../client/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../common/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/../common/src)




# Executable for ota_serviceing Test
add_executable(ota_service
    src/main.cpp
    src/ota_client.cpp
    src/partition_manager.cpp
    src/update_manager.cpp
    ${ota_service_proto_srcs}
    ${ota_service_grpc_srcs}
)

# Link libraries for ota_service_test
target_link_libraries(ota_service
    ${_GRPC_GRPCPP}
    protobuf::libprotobuf
    ${MYSQL_LIB}
    pthread
    ${OPENSSL_LIBS}
    ota_common
)
