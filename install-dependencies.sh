#!/bin/bash

git clone https://github.com/nlohmann/json.git .json
cd .json
git checkout v3.11.2
cmake -S . -B build
cmake --build build
sudo cmake --install build

sudo apt update
sudo apt install libgpiod-dev libuv1-dev libspdlog-dev
