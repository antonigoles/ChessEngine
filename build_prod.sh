#!/bin/bash
set -e

echo "=== KOMPILACJA TRYBU PRODUCTION (RELEASE) ==="
cmake -B build/release -DCMAKE_BUILD_TYPE=Release

cmake --build build/release