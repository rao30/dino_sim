#!/usr/bin/env bash
# Build dino_c on Linux (Google Colab / Ubuntu). Run from the repo root.
set -euo pipefail
cd "$(dirname "$0")/.."

if command -v apt-get >/dev/null 2>&1; then
  apt-get update -qq
  DEBIAN_FRONTEND=noninteractive apt-get install -y -qq cmake g++ ninja-build >/dev/null
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel --target dino_c dino_tests
python3 train/test_c_env.py
echo "colab setup ok: $(ls -1 build/libdino_c.so build/dino_c.so 2>/dev/null | head -1)"
