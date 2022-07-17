FROM ubuntu:20.04

RUN apt-get update \
    && apt-get install -y cmake g++ libtool \
    && rm -rf /var/lib/apt/lists/*
