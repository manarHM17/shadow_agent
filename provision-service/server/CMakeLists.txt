cmake_minimum_required(VERSION 3.15)
project(provision_service)

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
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../proto")

# === Provisioning Service ===
set(PROVISION_PROTO "${PROTO_PATH}/provision.proto")
set(provision_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/provision.pb.cc")
set(provision_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/provision.pb.h")
set(provision_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/provision.grpc.pb.cc")
set(provision_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/provision.grpc.pb.h")

add_custom_command(
    OUTPUT ${provision_proto_srcs} ${provision_proto_hdrs} ${provision_grpc_srcs} ${provision_grpc_hdrs}
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         -I "${PROTO_PATH}"
         --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${PROVISION_PROTO}"
    DEPENDS "${PROVISION_PROTO}"
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})



# Executable for Provisioning Test
add_executable(provision_service
    src/db_handler.cpp 
    src/jwt_handler.cpp
    src/grpc_server.cpp
    src/ProvisionServiceImpl.cpp
    ${provision_proto_srcs}
    ${provision_grpc_srcs}
    ${provision_proto_hdrs}
    ${provision_grpc_hdrs}   
)

# Link libraries for provision_test
target_link_libraries(provision_service
    ${_GRPC_GRPCPP}
    protobuf::libprotobuf
    ${MYSQL_LIB}
    pthread
    ${OPENSSL_LIBS}
)
