#!/bin/bash

cmake -S . -B build
cmake --build build
sudo cmake --install build

sudo systemctl enable fanshim-driver.service
sudo systemctl start fanshim-driver.service
