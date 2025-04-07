FROM ubuntu:latest

RUN apt-get update && apt-get install -y \
    clang \
    make \
    cmake \
    libc6-dev \
    libgtk-3-dev

COPY . /app
WORKDIR /app

RUN mkdir build && cd build && \
    cmake .. && \
    make

WORKDIR /app/build
