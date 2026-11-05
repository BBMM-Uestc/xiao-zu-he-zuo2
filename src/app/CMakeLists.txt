add_executable(LogicSim WIN32
    LogicSimApp.cpp
    MainFrame.cpp
)

target_include_directories(LogicSim
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(LogicSim
    PRIVATE
        logic_model
        logic_simcore
        logic_components
        logic_editor
        logic_fileio
        wx::core
        wx::base
)

target_compile_features(LogicSim PRIVATE cxx_std_20)

set_target_properties(LogicSim PROPERTIES
    OUTPUT_NAME "LogicSim"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
