if(NOT DEFINED ENV{VCPKG_ROOT})
    message(FATAL_ERROR "VCPKG_ROOT environment variable not set")
endif()

include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")