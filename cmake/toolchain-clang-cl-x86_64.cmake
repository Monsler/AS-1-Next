# Cross-compile toolchain: Linux host -> Windows x86_64 with clang-cl + lld
# against a real MSVC CRT + Windows SDK fetched by `xwin splat` (JUCE 8 does
# not support MinGW, but clang-cl is an officially supported Windows compiler).
#
# Required cache variables:
#   XWIN_DIR        - xwin splat output containing crt/ and sdk/
#   LLVM_TOOLS_DIR  - directory with llvm-rc / llvm-lib / llvm-mt
# Usage:
#   cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-clang-cl-x86_64.cmake \
#         -DXWIN_DIR=... -DLLVM_TOOLS_DIR=...
#   cmake --build build-win --target AS1Next_VST3
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(XWIN_DIR "" CACHE PATH "xwin splat output (crt/ + sdk/)")
set(LLVM_TOOLS_DIR "/usr/bin" CACHE PATH "directory with llvm-rc/llvm-lib/llvm-mt")
# Re-evaluated inside try_compile projects, which don't inherit the cache —
# forward our variables so the checks above don't fail there.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES XWIN_DIR LLVM_TOOLS_DIR)
if(NOT IS_DIRECTORY "${XWIN_DIR}")
    message(FATAL_ERROR "Pass -DXWIN_DIR=<xwin splat output>")
endif()

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_LINKER lld-link)
set(CMAKE_AR "${LLVM_TOOLS_DIR}/llvm-lib")
set(CMAKE_RC_COMPILER "${LLVM_TOOLS_DIR}/llvm-rc")
set(CMAKE_MT "${LLVM_TOOLS_DIR}/llvm-mt")

foreach(inc crt/include sdk/include/ucrt sdk/include/um sdk/include/shared sdk/include/winrt sdk/include/cppwinrt)
    string(APPEND CMAKE_C_FLAGS_INIT " /imsvc\"${XWIN_DIR}/${inc}\"")
    string(APPEND CMAKE_CXX_FLAGS_INIT " /imsvc\"${XWIN_DIR}/${inc}\"")
    string(APPEND CMAKE_RC_FLAGS_INIT " -I\"${XWIN_DIR}/${inc}\"")
endforeach()

foreach(kind EXE SHARED MODULE STATIC)
    string(APPEND CMAKE_${kind}_LINKER_FLAGS_INIT
           " /libpath:\"${XWIN_DIR}/crt/lib/x64\" /libpath:\"${XWIN_DIR}/sdk/lib/um/x64\" /libpath:\"${XWIN_DIR}/sdk/lib/ucrt/x64\"")
endforeach()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
