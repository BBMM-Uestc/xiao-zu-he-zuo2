add_executable(logicsim_tests
    TestMain.cpp
)

target_link_libraries(logicsim_tests
    PRIVATE
        logic_model
        logic_simcore
        logic_components
        logic_fileio
)

target_compile_features(logicsim_tests PRIVATE cxx_std_20)

add_test(
    NAME LogicSimCoreTests
    COMMAND logicsim_tests
)
