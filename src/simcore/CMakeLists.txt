add_library(logic_simcore STATIC
    SimulationEngine.cpp
)

target_include_directories(logic_simcore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(logic_simcore
    PUBLIC
        logic_model
)

target_compile_features(logic_simcore PUBLIC cxx_std_20)
