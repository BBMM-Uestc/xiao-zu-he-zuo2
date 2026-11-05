add_library(logic_editor STATIC
    CircuitCanvas.cpp
)

target_include_directories(logic_editor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(logic_editor
    PUBLIC
        logic_model
    PRIVATE
        wx::core
        wx::base
)

target_compile_features(logic_editor PUBLIC cxx_std_20)
