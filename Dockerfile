FROM ubuntu:latest

RUN apt-get update && apt-get install -y clang make libc6-dev
COPY src /app
WORKDIR /app

RUN make link-peer-listen
