#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Allow script to be launched from any directory
SOURCE=${BASH_SOURCE[0]}
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
#echo "SOURCE is '$SOURCE'"
#echo "SCRIPT_DIR is '$SCRIPT_DIR"
pushd $SCRIPT_DIR/.. > /dev/null

# Determine the operating system
unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     machine=Linux;;
    Darwin*)    machine=Mac;;
    *)          machine="UNKNOWN:${unameOut}"
esac
#echo ${machine}

if [ "$machine" == "Mac" ]; then
    echo "Mac build script not implemented yet"
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then

    echo "Building GCC Debug"
    cmake -S . -B build/gcc/debug -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++
    ninja -C build/gcc/debug

    echo "Building GCC Release"
    cmake -S . -B build/gcc/release -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++
    ninja -C build/gcc/release

    echo "Building Clang Debug"
    cmake -S . -B build/clang/debug -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
    ninja -C build/clang/debug

    echo "Building Clang Release"
    cmake -S . -B build/clang/release -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
    ninja -C build/clang/release
fi

# return to original directory
popd > /dev/null
