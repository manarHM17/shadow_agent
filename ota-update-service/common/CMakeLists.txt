cmake_minimum_required(VERSION 3.15)
project(ota_service)

find_package(OpenSSL REQUIRED)


# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Configuration commune
add_library(ota_common
    src/checksum.cc
    src/logging.cc
)

target_include_directories(ota_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# OpenSSL pour le checksum
find_package(OpenSSL REQUIRED)
target_link_libraries(ota_common
    PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto
)