#!/bin/bash
set -e

echo "=== KOMPILACJA TRYBU PRODUCTION (RELEASE) ==="
cmake -B build/release -DCMAKE_BUILD_TYPE=Release

cmake --build build/release

echo -e "\n=== URUCHAMIANIE PROGRAMU (PRODUCTION) ==="
./build/release/ChessBot l