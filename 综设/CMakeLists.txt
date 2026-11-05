cmake_minimum_required(VERSION 3.20)

project(logisim_lite VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LL_BUILD_TESTS "Build the logisim-lite tests" ON)
option(LL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# ---------------------------------------------------------------------------
# Common warning / language settings
# ---------------------------------------------------------------------------
function(ll_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /Zc:__cplusplus /utf-8 /EHsc)
        if(LL_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(LL_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# ll_model  (owner: D -- reference implementation bundled for local testing)
# ---------------------------------------------------------------------------
add_library(ll_model STATIC
    model/src/logic_value.cpp
    model/src/circuit.cpp
    model/src/document.cpp
)
target_include_directories(ll_model PUBLIC model/include)
ll_apply_warnings(ll_model)

# ---------------------------------------------------------------------------
# ll_component  (owner: C -- reference implementation bundled for local testing)
# ---------------------------------------------------------------------------
add_library(ll_component STATIC
    component/src/geometry.cpp
    component/src/registry.cpp
    component/src/builtin.cpp
)
target_include_directories(ll_component PUBLIC component/include)
target_link_libraries(ll_component PUBLIC ll_model)
ll_apply_warnings(ll_component)

# ---------------------------------------------------------------------------
# ll_sim  (owner: B -- the deliverable of this repository)
# ---------------------------------------------------------------------------
add_library(ll_sim STATIC
    sim/src/netlist.cpp
    sim/src/engine.cpp
    sim/src/waveform.cpp
)
target_include_directories(ll_sim PUBLIC sim/include)
target_link_libraries(ll_sim PUBLIC ll_component ll_model)
ll_apply_warnings(ll_sim)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(LL_BUILD_TESTS)
    enable_testing()

    add_executable(ll_tests
        tests/test_main.cpp
        tests/logic_value_test.cpp
        tests/netlist_test.cpp
        tests/engine_test.cpp
        tests/sequential_test.cpp
        tests/diagnostic_test.cpp
        tests/waveform_test.cpp
        tests/subcircuit_test.cpp
        tests/stress_test.cpp
    )
    target_include_directories(ll_tests PRIVATE tests)
    target_link_libraries(ll_tests PRIVATE ll_sim ll_component ll_model)
    ll_apply_warnings(ll_tests)

    add_test(NAME ll_tests COMMAND ll_tests)
endif()
