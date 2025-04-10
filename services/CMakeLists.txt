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

#openSSL puisque jwt depend d'elle
find_package(OpenSSL REQUIRED)

# jwt-cpp
add_library(jwt-cpp INTERFACE) # jwt-cpp est une bibliothèque d'interface (header-only)
target_include_directories(jwt-cpp INTERFACE /usr/local/include) # Inclure jwt-cpp
#puisque jwt necessite l utilisation de json file on doit inclure picojson
target_include_directories(jwt-cpp INTERFACE /usr/local/include)
# Proto file
set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../proto")
set(PROTO_FILE "${PROTO_PATH}/provision.proto")

# Generated sources
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/provision.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/provision.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/provision.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/provision.grpc.pb.h")

# Generate gRPC and Protobuf files
add_custom_command(
    OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         -I "${PROTO_PATH}"
         --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
         "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Executable
add_executable(provision_service 
    provision/include/ProvisionServiceImpl.h
    main.cpp
    provision/src/ProvisionServiceImpl.cpp
    ${hw_proto_srcs}
    ${hw_grpc_srcs}
    ${hw_proto_hdrs}     # Ajout des headers protobuf
    ${hw_grpc_hdrs}      # Ajout des headers gRPC
)

# Link libraries
target_link_libraries(provision_service
    ${_GRPC_GRPCPP}
    protobuf::libprotobuf
    ${MYSQL_LIB}
    pthread
    jwt-cpp
    OpenSSL::SSL
    OpenSSL::Crypto
)
