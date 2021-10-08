FROM ubuntu:20.04

RUN apt-get update && apt-get install --no-install-recommends -y \
    make=4.2.1-1.2 \
    cmake=3.16.3-1ubuntu1 \
    g++=4:9.3.0-1ubuntu2 \
    libassimp-dev=5.0.1~ds0-1build1 \
    libglfw3-dev=3.3.2-1 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
