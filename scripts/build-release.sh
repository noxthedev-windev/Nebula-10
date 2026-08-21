#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$root" -B "$root/build" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/build" --config Release --parallel
ctest --test-dir "$root/build" --output-on-failure
