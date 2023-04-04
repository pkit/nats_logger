#!/bin/bash

sudo apt-get -q -y install libprotobuf-c-dev dpkg-dev wget

wget -qO- "https://github.com/Kitware/CMake/releases/download/v3.26.1/cmake-3.26.1-linux-x86_64.tar.gz" \
    | sudo tar --strip-components=1 -xz -C /usr/local

