add_library(logic_model STATIC
    CircuitDocument.cpp
)

target_include_directories(logic_model
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(logic_model PUBLIC cxx_std_20)
