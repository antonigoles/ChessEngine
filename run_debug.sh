#!/bin/bash
set -e

echo "=== KOMPILACJA TRYBU DEBUG ==="
cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug

cmake --build build/debug

echo -e "\n=== URUCHAMIANIE PROGRAMU (DEBUG) ==="
./build/debug/ChessBot