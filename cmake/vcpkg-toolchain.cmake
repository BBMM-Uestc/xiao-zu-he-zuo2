# Resolve vcpkg from a user-defined VCPKG_ROOT first, then from the vcpkg copy
# installed with Visual Studio 2022. The wrapper lets a fresh checkout build
# without requiring a machine-wide environment variable.

set(_logicsim_vcpkg_candidates)

if(DEFINED ENV{VCPKG_ROOT})
    list(APPEND _logicsim_vcpkg_candidates "$ENV{VCPKG_ROOT}")
endif()

if(CMAKE_HOST_WIN32)
    foreach(_visual_studio_edition IN ITEMS Community Professional Enterprise BuildTools)
        list(APPEND _logicsim_vcpkg_candidates
            "C:/Program Files/Microsoft Visual Studio/2022/${_visual_studio_edition}/VC/vcpkg"
        )
    endforeach()
endif()

list(APPEND _logicsim_vcpkg_candidates "C:/dev/vcpkg")

foreach(_candidate IN LISTS _logicsim_vcpkg_candidates)
    if(EXISTS "${_candidate}/scripts/buildsystems/vcpkg.cmake")
        set(VCPKG_ROOT "${_candidate}" CACHE PATH "vcpkg installation")
        message(STATUS "Using vcpkg from: ${_candidate}")
        include("${_candidate}/scripts/buildsystems/vcpkg.cmake")
        return()
    endif()
endforeach()

message(FATAL_ERROR
    "vcpkg was not found. Set VCPKG_ROOT or install the Visual Studio C++ "
    "vcpkg component."
)
