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
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../proto")
set(GENERATED_PROTO_PATH "${CMAKE_CURRENT_BINARY_DIR}")

# === provisioning Service ===
set(provisioning_PROTO "${PROTO_PATH}/provisioning.proto")
set(provisioning_proto_srcs "${GENERATED_PROTO_PATH}/provisioning.pb.cc")
set(provisioning_proto_hdrs "${GENERATED_PROTO_PATH}/provisioning.pb.h")
set(provisioning_grpc_srcs "${GENERATED_PROTO_PATH}/provisioning.grpc.pb.cc")
set(provisioning_grpc_hdrs "${GENERATED_PROTO_PATH}/provisioning.grpc.pb.h")

# Generate protobuf files
add_custom_command(
    OUTPUT ${provisioning_proto_srcs} ${provisioning_proto_hdrs} ${provisioning_grpc_srcs} ${provisioning_grpc_hdrs}
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${GENERATED_PROTO_PATH}"
         --cpp_out "${GENERATED_PROTO_PATH}"
         -I "${PROTO_PATH}"
         --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${provisioning_PROTO}"
    DEPENDS "${provisioning_PROTO}"
)

# Add custom target for generated files
add_custom_target(generate_protos
    DEPENDS 
        ${provisioning_proto_srcs}
        ${provisioning_proto_hdrs}
        ${provisioning_grpc_srcs}
        ${provisioning_grpc_hdrs}
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)
include_directories(${GENERATED_PROTO_PATH})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Executable
add_executable(provisioning_service
    ../common/src/db_handler.cpp 
    ../common/src/jwt_handler.cpp
    src/grpc_server.cpp
    src/ProvisionServiceImpl.cpp
    ${provisioning_proto_srcs}
    ${provisioning_grpc_srcs}
)

# Add dependency on proto generation
add_dependencies(provisioning_service generate_protos)

# Link libraries
target_link_libraries(provisioning_service
    ${_GRPC_GRPCPP}
    protobuf::libprotobuf
    ${MYSQL_LIB}
    pthread
    ${OPENSSL_LIBS}
)
