# Cross-compile the Windows x86-64 build from Linux with llvm-mingw (the release toolchain, pinned in
# tools/installer/toolchains.json: llvm-mingw-20260922). Use its Linux-hosted build of the same version:
#   cmake -S . -B build/win-rel -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/llvm-mingw-cross.cmake \
#     -DLLVM_MINGW=/path/to/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64 -DCMAKE_EXE_LINKER_FLAGS=-static ...
# Tests run under Wine (CMAKE_CROSSCOMPILING_EMULATOR). README "Build for Windows" has the full commands.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
if(NOT LLVM_MINGW)
  set(LLVM_MINGW "$ENV{LLVM_MINGW}")
endif()
if(NOT LLVM_MINGW OR NOT EXISTS "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang")
  message(FATAL_ERROR "set LLVM_MINGW to an llvm-mingw folder (Linux-hosted), e.g. -DLLVM_MINGW=/opt/llvm-mingw-20260922-ucrt-ubuntu-22.04-x86_64")
endif()
# try_compile projects get the variable too
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LLVM_MINGW)
set(CMAKE_C_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-clang++")
set(CMAKE_RC_COMPILER "${LLVM_MINGW}/bin/x86_64-w64-mingw32-windres")
set(CMAKE_AR "${LLVM_MINGW}/bin/llvm-ar")
set(CMAKE_RANLIB "${LLVM_MINGW}/bin/llvm-ranlib")
# only the toolchain's own Windows libraries; SDL3 and the rest are built from pinned sources (TPHD_BUNDLED_DEPS)
set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW}/x86_64-w64-mingw32" ${TPHD_CROSS_EXTRA_ROOTS})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_CROSSCOMPILING_EMULATOR wine)
