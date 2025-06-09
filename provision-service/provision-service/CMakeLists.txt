cmake_minimum_required(VERSION 3.15)
project(provisioning_service)

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

# JWT
include_directories(/usr/local/include/jwt-cpp)

# OpenSSL (since jwt depends on it)
find_package(OpenSSL REQUIRED)

# Boost
find_package(Boost REQUIRED)

# json-cpp
include_directories(/usr/include/jsoncpp)

# Proto file
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/proto")

# === provisioninging Service ===
set(provisioning_PROTO "${PROTO_PATH}/provisioning.proto")
set(provisioning_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/provisioning.pb.cc")
set(provisioning_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/provisioning.pb.h")
set(provisioning_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/provisioning.grpc.pb.cc")
set(provisioning_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/provisioning.grpc.pb.h")

add_custom_command(
    OUTPUT ${provisioning_proto_srcs} ${provisioning_proto_hdrs} ${provisioning_grpc_srcs} ${provisioning_grpc_hdrs}
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         -I "${PROTO_PATH}"
         --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${provisioning_PROTO}"
    DEPENDS "${provisioning_PROTO}"
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/client/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/server/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common/include)

include_directories(${CMAKE_CURRENT_BINARY_DIR})



# Executable for provisioninging Test
add_executable(provisioning_service
    common/src/database_manager.cpp 
    common/src/jwt_manager.cpp
    server/src/provisioning_service.cpp    
    server/src/main.cpp
    ${provisioning_proto_srcs}
    ${provisioning_grpc_srcs}
    ${provisioning_proto_hdrs}
    ${provisioning_grpc_hdrs}   
)

# Link libraries for provisioning_test
target_link_libraries(provisioning_service
    ${_GRPC_GRPCPP}
    protobuf::libprotobuf
    ${MYSQL_LIB}
    pthread
    OpenSSL::SSL
    OpenSSL::Crypto
    crypt
    gRPC::grpc++_reflection
)
# Executable for provisioninging Test
add_executable(provisioning_test
    common/src/database_manager.cpp 
    common/src/jwt_manager.cpp
    client/src/provisioning_client.cpp
    client/src/main.cpp
    ${provisioning_proto_srcs}
    ${provisioning_grpc_srcs}
    ${provisioning_proto_hdrs}
    ${provisioning_grpc_hdrs}   
)

# Link libraries for provisioning_test
target_link_libraries(provisioning_test
    ${_GRPC_GRPCPP}
    protobuf::libprotobuf
    ${MYSQL_LIB}
    pthread
    OpenSSL::SSL
    OpenSSL::Crypto
    crypt
)
