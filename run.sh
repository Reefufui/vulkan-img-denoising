#!/bin/bash

cd shaders
sh compile_shaders.sh
cd ..

if [ -d "build" ]; then
    cd build &&
    make -j 5 &&
    cd .. &&
    ./build/vulkan_denoice $1
else
    mkdir build &&
    cd build &&
    cmake .. &&
    make -j 5 &&
    cd .. &&
    ./build/vulkan_denoice $1
fi
