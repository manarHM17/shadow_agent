cmake_minimum_required(VERSION 3.10)
project(ota_update_service)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Trouver les packages nécessaires
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
include_directories(/usr/include/jsoncpp)

# Obtenir les chemins du compilateur protobuf et du plugin gRPC C++
set(PROTOC ${Protobuf_PROTOC_EXECUTABLE})
set(GRPC_CPP_PLUGIN $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

# Générer les fichiers source et header à partir des fichiers proto
set(PROTO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Vérifier que le répertoire proto existe
if(NOT EXISTS ${PROTO_SRC_DIR})
  file(MAKE_DIRECTORY ${PROTO_SRC_DIR})
endif()

# Proto file
set(PROTO_FILES
  ${PROTO_SRC_DIR}/ota_service.proto
)

# Générer les fichiers .cc et .h à partir des fichiers .proto
foreach(proto_file ${PROTO_FILES})
  get_filename_component(proto_name ${proto_file} NAME_WE)
  add_custom_command(
    OUTPUT "${PROTO_OUTPUT_DIR}/${proto_name}.pb.cc"
           "${PROTO_OUTPUT_DIR}/${proto_name}.pb.h"
           "${PROTO_OUTPUT_DIR}/${proto_name}.grpc.pb.cc"
           "${PROTO_OUTPUT_DIR}/${proto_name}.grpc.pb.h"
    COMMAND ${PROTOC}
    ARGS --grpc_out=${PROTO_OUTPUT_DIR}
         --cpp_out=${PROTO_OUTPUT_DIR}
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         -I${PROTO_SRC_DIR}
         ${proto_file}
    DEPENDS ${proto_file}
    COMMENT "Running C++ protocol buffer compiler on ${proto_file}"
    VERBATIM
  )
endforeach()

# Inclure les répertoires d'en-têtes
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${Protobuf_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIR}
  ${CURL_INCLUDE_DIRS}
)

# Sources pour la bibliothèque de service OTA
set(OTA_SERVICE_LIB_SRCS
  src/mender_client.cpp
  src/ota_service_impl.cpp
  src/ota_service.pb.cc
  src/ota_service.grpc.pb.cc
)

# Ajouter la bibliothèque de service OTA
add_library(ota_service_lib ${OTA_SERVICE_LIB_SRCS})
target_link_libraries(ota_service_lib
  ${PROTOBUF_LIBRARIES}
  gRPC::grpc++
  gRPC::grpc++_reflection
  ${OPENSSL_LIBRARIES}
  ${CURL_LIBRARIES}
  pthread
)

# Exécutable du serveur
add_executable(ota_server src/server.cpp)
target_link_libraries(ota_server ota_service_lib)

# Exécutable du client
add_executable(ota_client src/mender_client.cpp
               src/ota_service.pb.cc
               src/ota_service.grpc.pb.cc)
target_link_libraries(ota_client
  ${PROTOBUF_LIBRARIES}
  gRPC::grpc++
  gRPC::grpc++_reflection
  ${OPENSSL_LIBRARIES}
  pthread
)
