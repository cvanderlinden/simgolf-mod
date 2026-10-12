#!/usr/bin/env bash
# tools/build_native.sh: the native Windows build, build/native/golf.exe
# (port/CMakePresets.json "native": clang-cl x86, SDL2, see port/cmake/native.cmake).
# Needs the translated game in build/gen (tools/build_web.sh) and the toolchain
# from tools/install_native_tools.sh. A clean build takes about 30 s.
set -euo pipefail
cd "$(dirname "$0")/.."
. tools/env.sh
[ -f build/gen/golf/golf_protos.h ] || { echo "build/gen missing: run tools/build_web.sh first"; exit 3; }
[ -f build/native/CMakeCache.txt ] || (cd port && cmake --preset native >/dev/null)
cmake --build build/native
