# Base image (assumed to be Ubuntu 22.04 based on the context)
FROM ubuntu:22.04

# Install basic dependencies and build tools, including Boost
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    libssl-dev \
    libmysqlclient-dev \
    autoconf \
    automake \
    libtool \
    make \
    g++ \
    unzip \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Clone gRPC
RUN git clone --recurse-submodules -b v1.60.0 https://github.com/grpc/grpc /opt/grpc \
    && cd /opt/grpc \
    && mkdir -p cmake/build

# Clone jwt-cpp
RUN git clone https://github.com/Thalhammer/jwt-cpp.git /opt/jwt-cpp \
    && cd /opt/jwt-cpp \
    && mkdir build

# Set working directory
WORKDIR /app

# Copy project files
COPY . /app

# Create build directory and build the project
RUN mkdir -p build && cd build \
    && cmake .. \
    && make