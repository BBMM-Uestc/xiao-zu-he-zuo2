cmake_minimum_required(VERSION 3.24)

# Keep vcpkg's mutable cache inside the ignored build output. These directories
# must exist when the vcpkg toolchain runs during project().
file(MAKE_DIRECTORY
    "${CMAKE_CURRENT_LIST_DIR}/out/cache/vcpkg-downloads"
    "${CMAKE_CURRENT_LIST_DIR}/out/cache/vcpkg-binaries"
    "${CMAKE_CURRENT_LIST_DIR}/out/cache/vcpkg-registries"
)

project(LogicSim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(wxWidgets CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_subdirectory(src/model)
add_subdirectory(src/simcore)
add_subdirectory(src/components)
add_subdirectory(src/editor)
add_subdirectory(src/fileio)
add_subdirectory(src/app)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
