if(NOT DEFINED ENV{EMSDK})
    message(FATAL_ERROR "EMSDK environment variable not set")
endif()
if(NOT DEFINED ENV{VCPKG_ROOT})
    message(FATAL_ERROR "VCPKG_ROOT environment variable not set")
endif()

include("$ENV{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")