FROM ubuntu:24.04

RUN dpkg --add-architecture i386 && \
    apt-get update && apt-get install --yes --no-install-recommends \
    ca-certificates=20240203 \
    git=1:2.43.0-1ubuntu7.3 \
    cmake=3.28.3-1build7 \
    clang-format=1:18.0-59~exp2 \
    mingw-w64=11.0.1-3build1 \
    ninja-build=1.11.1-2 \
    wine=9.0~repack-4build3 \
    && rm -rf /var/lib/apt/lists/*
