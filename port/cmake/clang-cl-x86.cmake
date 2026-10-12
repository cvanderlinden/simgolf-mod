# Toolchain for the native Windows build (simgolf mod, task-2): clang-cl and
# lld-link from the official LLVM release, 32-bit x86 (the translated game
# keeps x86 addresses and stores pointers in 32-bit slots), the MSVC CRT and
# Windows SDK of the installed Visual Studio (found by clang-cl and lld-link
# themselves). LLVM location: SIMGOLF_LLVM, default C:/simgolf-tools/llvm.
if(DEFINED ENV{SIMGOLF_LLVM})
  set(SIMGOLF_LLVM "$ENV{SIMGOLF_LLVM}")
else()
  set(SIMGOLF_LLVM "C:/simgolf-tools/llvm")
endif()
set(CMAKE_C_COMPILER "${SIMGOLF_LLVM}/bin/clang-cl.exe")
set(CMAKE_CXX_COMPILER "${SIMGOLF_LLVM}/bin/clang-cl.exe")
set(CMAKE_LINKER "${SIMGOLF_LLVM}/bin/lld-link.exe")
set(CMAKE_AR "${SIMGOLF_LLVM}/bin/llvm-lib.exe")
set(CMAKE_RC_COMPILER "${SIMGOLF_LLVM}/bin/llvm-rc.exe")
set(CMAKE_MT "${SIMGOLF_LLVM}/bin/llvm-mt.exe")
set(CMAKE_C_COMPILER_TARGET i686-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET i686-pc-windows-msvc)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)   # static CRT: no VC++ redistributable needed
