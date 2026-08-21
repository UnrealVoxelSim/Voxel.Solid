find_package(GTest CONFIG REQUIRED)

add_executable(
    ${PROJECT_NAME}.Test
    UnrealVoxelSim/Voxel/Solid/ControllerTest.cpp
)

target_link_libraries(
    ${PROJECT_NAME}.Test
    PRIVATE
        ${PROJECT_NAME}
        UnrealVoxelSim.Events.InMemory
        UnrealVoxelSim.Voxel.Chunked
        GTest::gtest_main
)

UnrealVoxelSimConfigureTarget(${PROJECT_NAME}.Test)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}.Test)
