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

# JWT
include_directories(/usr/local/include/jwt-cpp)

# OpenSSL
find_package(OpenSSL REQUIRED)

# Boost
find_package(Boost REQUIRED)

# json-cpp
include_directories(/usr/include/jsoncpp)

# Proto file paths
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../proto")
set(GENERATED_PROTO_PATH "${CMAKE_CURRENT_BINARY_DIR}")

# Generate proto files
set(PROVISION_PROTO "${PROTO_PATH}/provisioning.proto")
set(provision_proto_srcs "${GENERATED_PROTO_PATH}/provisioning.pb.cc")
set(provision_proto_hdrs "${GENERATED_PROTO_PATH}/provisioning.pb.h")
set(provision_grpc_srcs "${GENERATED_PROTO_PATH}/provisioning.grpc.pb.cc")
set(provision_grpc_hdrs "${GENERATED_PROTO_PATH}/provisioning.grpc.pb.h")

# Proto generation command
add_custom_command(
    OUTPUT ${provision_proto_srcs} ${provision_proto_hdrs} ${provision_grpc_srcs} ${provision_grpc_hdrs}
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${GENERATED_PROTO_PATH}"
         --cpp_out "${GENERATED_PROTO_PATH}"
         -I "${PROTO_PATH}"
         --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${PROVISION_PROTO}"
    DEPENDS "${PROVISION_PROTO}"
)

# Custom target for proto generation
add_custom_target(generate_protos_client
    DEPENDS 
        ${provision_proto_srcs}
        ${provision_proto_hdrs}
        ${provision_grpc_srcs}
        ${provision_grpc_hdrs}
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)
include_directories(${GENERATED_PROTO_PATH})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Executable
add_executable(provision_test 
    src/grpc_client.cpp
    src/ProvisionClientImpl.cpp
    src/ConfigManager.cpp
    ${provision_proto_srcs}
    ${provision_grpc_srcs}
)

# Add dependency on proto generation
add_dependencies(provision_test generate_protos_client)

# Link libraries
target_link_libraries(provision_test
    ${_GRPC_GRPCPP}
    protobuf::libprotobuf
    stdc++fs
)


