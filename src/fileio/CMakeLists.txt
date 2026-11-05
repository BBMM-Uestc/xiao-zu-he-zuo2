add_library(logic_fileio STATIC
    CircuitFile.cpp
)

target_include_directories(logic_fileio
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(logic_fileio
    PUBLIC
        logic_model
    PRIVATE
        nlohmann_json::nlohmann_json
)

target_compile_features(logic_fileio PUBLIC cxx_std_20)
