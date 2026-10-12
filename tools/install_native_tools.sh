#!/usr/bin/env bash
# tools/install_native_tools.sh: the native build's toolchain, user-local in
# $TOOLS_EXT (default C:/simgolf-tools), official releases only:
#   LLVM 23.1.3 (clang-cl, lld-link) from github.com/llvm/llvm-project
#   SDL2 2.32.10 VC development files from github.com/libsdl-org/SDL
# Also needed: the MSVC x86 libraries and a Windows SDK (Visual Studio 2022
# Build Tools), which clang-cl and lld-link find on their own.
set -euo pipefail
cd "$(dirname "$0")/.."
. tools/env.sh
cd "$TOOLS_EXT"
if [ ! -x llvm/bin/clang-cl.exe ]; then
  echo "LLVM 23.1.3"
  d=$(mktemp -d); gh release download llvmorg-23.1.3 -R llvm/llvm-project -D "$d" \
    -p 'clang+llvm-23.1.3-x86_64-pc-windows-msvc.tar.xz'
  tar -xf "$d"/clang+llvm-23.1.3-x86_64-pc-windows-msvc.tar.xz -C "$d"
  mv "$d/clang+llvm-23.1.3-x86_64-pc-windows-msvc" llvm && rm -rf "$d"
fi
if [ ! -f SDL2-2.32.10/include/SDL.h ]; then
  echo "SDL2 2.32.10"
  d=$(mktemp -d); gh release download release-2.32.10 -R libsdl-org/SDL -D "$d" -p 'SDL2-devel-2.32.10-VC.zip'
  (cd "$d" && unzip -q SDL2-devel-2.32.10-VC.zip) && mv "$d/SDL2-2.32.10" . && rm -rf "$d"
fi
echo "native toolchain ready in $TOOLS_EXT"
