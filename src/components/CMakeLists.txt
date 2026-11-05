add_library(logic_components STATIC
    ComponentCatalog.cpp
)

target_include_directories(logic_components
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(logic_components
    PUBLIC
        logic_model
)

target_compile_features(logic_components PUBLIC cxx_std_20)
