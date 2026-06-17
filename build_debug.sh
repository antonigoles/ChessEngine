#!/bin/bash
set -e

echo "=== KOMPILACJA TRYBU DEBUG ==="
cmake -B build/debug-mcts -DCMAKE_BUILD_TYPE=Debug

cmake --build build/debug-mcts