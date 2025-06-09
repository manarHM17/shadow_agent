cmake_minimum_required(VERSION 3.17)
project(ota_service_service)

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

# OpenSSL (since jwt depends on it)
find_package(OpenSSL REQUIRED)


# json-cpp
include_directories(/usr/include/jsoncpp)

# Proto file
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../proto")
set(GENERATED_PROTO_PATH "${CMAKE_CURRENT_BINARY_DIR}")

# === ota_service Service ===
set(ota_service_PROTO "${PROTO_PATH}/ota_service.proto")
set(ota_service_proto_srcs "${GENERATED_PROTO_PATH}/ota_service.pb.cc")
set(ota_service_proto_hdrs "${GENERATED_PROTO_PATH}/ota_service.pb.h")
set(ota_service_grpc_srcs "${GENERATED_PROTO_PATH}/ota_service.grpc.pb.cc")
set(ota_service_grpc_hdrs "${GENERATED_PROTO_PATH}/ota_service.grpc.pb.h")

# Generate protobuf files
add_custom_command(
    OUTPUT ${ota_service_proto_srcs} ${ota_service_proto_hdrs} ${ota_service_grpc_srcs} ${ota_service_grpc_hdrs}
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${GENERATED_PROTO_PATH}"
         --cpp_out "${GENERATED_PROTO_PATH}"
         -I "${PROTO_PATH}"
         --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${ota_service_PROTO}"
    DEPENDS "${ota_service_PROTO}"
)

# Add custom target for generated files
add_custom_target(generate_protos
    DEPENDS 
        ${ota_service_proto_srcs}
        ${ota_service_proto_hdrs}
        ${ota_service_grpc_srcs}
        ${ota_service_grpc_hdrs}
)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${GENERATED_PROTO_PATH})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Executable
add_executable(ota_test
    ${CMAKE_CURRENT_SOURCE_DIR}/ota_client_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/client_main.cpp
    ${ota_service_proto_srcs}
    ${ota_service_grpc_srcs}
)

# Add dependency on proto generation
add_dependencies(ota_test generate_protos)

# Link libraries
target_link_libraries(ota_test
    ${_GRPC_GRPCPP}
    protobuf::libprotobuf
    pthread
    ${OPENSSL_LIBS}
)
