cmake_minimum_required(VERSION 3.15)
project(iot_shadow_client)

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
#jwt 
include_directories(/usr/local/include/jwt-cpp)
#openSSL puisque jwt depend d'elle
find_package(OpenSSL REQUIRED)
# RabbitMQ
# Boost (for RabbitMQ wrapper)
find_package(Boost REQUIRED)

# RabbitMQ C library
find_package(PkgConfig REQUIRED)
pkg_check_modules(RABBITMQ REQUIRED librabbitmq)

# SimpleAmqpClient (RabbitMQ wrapper)
find_library(SIMPLE_AMQP_CLIENT SimpleAmqpClient REQUIRED)
link_directories(/usr/local/lib)

set(RABBITMQ_LIB ${SIMPLE_AMQP_CLIENT})
message(STATUS "Using RabbitMQ lib: ${RABBITMQ_LIB}")
# Include directories
include_directories(
    ${Boost_INCLUDE_DIRS}
    ${RABBITMQ_INCLUDE_DIRS}
)

find_library(RABBITMQ_LIB SimpleAmqpClient REQUIRED)
include_directories(/usr/include) 
#json
find_package(nlohmann_json REQUIRED)

# Proto files path
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

#=== Monitoring Service ===
set(MONITORING_PROTO "${PROTO_PATH}/monitoring.proto")
set(monitoring_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/monitoring.pb.cc")
set(monitoring_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/monitoring.pb.h")
set(monitoring_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/monitoring.grpc.pb.cc")
set(monitoring_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/monitoring.grpc.pb.h")

add_custom_command(
   OUTPUT ${monitoring_proto_srcs} ${monitoring_proto_hdrs} ${monitoring_grpc_srcs} ${monitoring_grpc_hdrs}
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         -I "${PROTO_PATH}"
         --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${MONITORING_PROTO}"
    DEPENDS "${MONITORING_PROTO}" )

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Executable
# add_executable(provision_test
#     #src/monitoring_test.cpp
#     src/provision_test.cpp
#     ${provision_proto_srcs}
#     ${provision_grpc_srcs}

#     ${provision_proto_hdrs}
#     ${provision_grpc_hdrs}

# )


# # Link libraries
# target_link_libraries(provision_test
#     ${_GRPC_GRPCPP}
#     protobuf::libprotobuf
#     ${Boost_LIBRARIES}
#     ${RABBITMQ_LIB}
#     nlohmann_json::nlohmann_json
#     OpenSSL::SSL
#     OpenSSL::Crypto
# )
# Executable
add_executable(monitoring_test
    src/monitoring_test.cpp
    ${monitoring_proto_srcs}
    ${monitoring_grpc_srcs}

    ${monitoring_proto_hdrs}
    ${monitoring_grpc_hdrs}

)


# Link libraries
target_link_libraries(monitoring_test
    ${_GRPC_GRPCPP}
    protobuf::libprotobuf
    ${Boost_LIBRARIES}
    ${RABBITMQ_LIB}
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    ${RABBITMQ_LIB} 
    SimpleAmqpClient 
    boost_system 
    pthread 
    ${RABBITMQ_LIBRARIES}
)

