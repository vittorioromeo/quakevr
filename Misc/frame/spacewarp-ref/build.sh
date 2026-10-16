#!/bin/sh
# Build spacewarp-ref; the binary lands next to this script.
set -e
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
