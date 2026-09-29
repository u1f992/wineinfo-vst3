# syntax=docker/dockerfile:1.7
FROM ubuntu:24.04

RUN dpkg --add-architecture i386 && \
    apt-get update && apt-get install --yes --no-install-recommends \
    ca-certificates=20240203 \
    git=1:2.43.0-1ubuntu7.3 \
    cmake=3.28.3-1build7 \
    clang-format=1:18.0-59~exp2 \
    libvulkan1=1.3.275.0-1build1 \
    mesa-vulkan-drivers=25.2.8-0ubuntu0.24.04.2 \
    mingw-w64=11.0.1-3build1 \
    ninja-build=1.11.1-2 \
    xauth=1:1.1.2-1build1 \
    xvfb=2:21.1.12-1ubuntu1.8 \
    && rm -rf /var/lib/apt/lists/*

ADD --chmod=644 --checksum=sha256:d965d646defe94b3dfba6d5b4406900ac6c81065428bf9d9303ad7a72ee8d1b8 \
    https://dl.winehq.org/wine-builds/winehq.key /etc/apt/keyrings/winehq-archive.key
ADD --chmod=644 --checksum=sha256:b5962429ece1f831c9f713c13f9f0d26bb367117ef56706648ff385f004779fb \
    https://dl.winehq.org/wine-builds/ubuntu/dists/noble/winehq-noble.sources /etc/apt/sources.list.d/winehq-noble.sources

RUN apt-get update && apt-get install --yes --install-recommends \
    winehq-stable=10.0.0.0~noble-1 \
    wine-stable=10.0.0.0~noble-1 \
    wine-stable-amd64=10.0.0.0~noble-1 \
    wine-stable-i386:i386=10.0.0.0~noble-1 \
    && rm -rf /var/lib/apt/lists/*
