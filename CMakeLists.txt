cmake_minimum_required(VERSION 3.28)

project(UnrealVoxelSim.Voxel.Solid VERSION 0.1.0 LANGUAGES CXX)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME UnrealVoxelSimBuild
    GITHUB_REPOSITORY UnrealVoxelSim/Build.CMake
    GIT_TAG main
)

include("${UnrealVoxelSimBuild_SOURCE_DIR}/UnrealVoxelSim.Build.CMake.cmake")

UnrealVoxelSimAddInternalDependency(
    NAME UnrealVoxelSimVoxelApi
    GITHUB_REPOSITORY UnrealVoxelSim/Voxel.Api
)

UnrealVoxelSimAddInternalDependency(
    NAME UnrealVoxelSimVoxelSolidApi
    GITHUB_REPOSITORY UnrealVoxelSim/Voxel.Solid.Api
)

if(UNREALVOXELSIM_BUILD_TESTS AND BUILD_TESTING AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    UnrealVoxelSimAddInternalDependency(
        NAME UnrealVoxelSimEventsInMemory
        GITHUB_REPOSITORY UnrealVoxelSim/Events.InMemory
    )

    UnrealVoxelSimAddInternalDependency(
        NAME UnrealVoxelSimVoxelChunked
        GITHUB_REPOSITORY UnrealVoxelSim/Voxel.Chunked
    )
endif()

UnrealVoxelSimConfigureModule()
