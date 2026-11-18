# Optional Linux-hosted Windows x64 build. The normal Visual Studio build is
# unchanged. Obtain Microsoft's SDK/CRT with accepted terms before using this.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(_llvm "$ENV{SIDECAR_LLVM_ROOT}")
set(_xwin "$ENV{SIDECAR_XWIN_ROOT}")
if(NOT EXISTS "${_llvm}/bin/clang-cl" OR NOT EXISTS "${_xwin}/crt/include")
  message(FATAL_ERROR "Set SIDECAR_LLVM_ROOT and SIDECAR_XWIN_ROOT to the cloud toolchain roots")
endif()
set(CMAKE_CXX_COMPILER "${_llvm}/bin/clang-cl")
set(CMAKE_C_COMPILER "${_llvm}/bin/clang-cl")
set(CMAKE_LINKER "${_llvm}/bin/lld-link")
set(CMAKE_AR "${_llvm}/bin/llvm-lib")
set(CMAKE_RC_COMPILER "${_llvm}/bin/llvm-rc")
set(CMAKE_MT "${_llvm}/bin/llvm-mt")
# xwin omits debug CRT libraries by default. Compiler probes must use Release.
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
# This project has no C++ modules and does not need clang-scan-deps.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_FLAGS_INIT "/clang:-fms-compatibility-version=19.44 -imsvc\"${_xwin}/crt/include\" -imsvc\"${_xwin}/sdk/include/ucrt\" -imsvc\"${_xwin}/sdk/include/shared\" -imsvc\"${_xwin}/sdk/include/um\" -imsvc\"${_xwin}/sdk/include/winrt\" -imsvc\"${_xwin}/sdk/include/cppwinrt\"")
set(CMAKE_EXE_LINKER_FLAGS_INIT "/libpath:\"${_xwin}/crt/lib/x86_64\" /libpath:\"${_xwin}/sdk/lib/ucrt/x86_64\" /libpath:\"${_xwin}/sdk/lib/um/x86_64\"")
set(CMAKE_RC_FLAGS_INIT "/I\"${_xwin}/crt/include\" /I\"${_xwin}/sdk/include/ucrt\" /I\"${_xwin}/sdk/include/shared\" /I\"${_xwin}/sdk/include/um\"")
set(CMAKE_FIND_ROOT_PATH "${_xwin}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
