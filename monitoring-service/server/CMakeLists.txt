cmake_minimum_required(VERSION 3.15)
project(monitoring_service)

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
#jwt 
include_directories(/usr/local/include/jwt-cpp)
#openSSL puisque jwt depend d'elle
find_package(OpenSSL REQUIRED)
# json-cpp
find_package(jsoncpp REQUIRED)
include_directories(/usr/include/jsoncpp)

# RabbitMQ C library
find_package(PkgConfig REQUIRED)
pkg_check_modules(RABBITMQ REQUIRED librabbitmq)
find_package(Boost REQUIRED)

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

# Proto file
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../proto")

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

add_executable(monitoring_service 
    src/server.cpp
    src/rabbitmq_consumer.cpp
    src/metrics_analyzer.cpp
    src/alert_manager.cpp
    ${monitoring_proto_srcs}
    ${monitoring_grpc_srcs}

    ${monitoring_proto_hdrs}
    ${monitoring_grpc_hdrs}  )  

target_link_libraries(monitoring_service
    ${_GRPC_GRPCPP}
    protobuf::libprotobuf
    ${MYSQL_LIB}
    pthread
    OpenSSL::SSL
    OpenSSL::Crypto
    ${RABBITMQ_LIB} 
    SimpleAmqpClient 
    boost_system 
    pthread 
    ${RABBITMQ_LIBRARIES}
    jsoncpp
    )
