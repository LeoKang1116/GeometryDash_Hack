cmake_minimum_required(VERSION 3.25)
project(TargetReplay VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TARGET_REPLAY_BUILD_MOD "Build the Geode mod" ON)
include(FetchContent)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nlohmann_json)

add_library(replay_core STATIC src/Replay.cpp)
set_target_properties(replay_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(replay_core PUBLIC src)
target_link_libraries(replay_core PUBLIC nlohmann_json::nlohmann_json)

enable_testing()
add_executable(replay_tests tests/ReplayTests.cpp)
target_link_libraries(replay_tests PRIVATE replay_core)
add_test(NAME replay_tests COMMAND replay_tests)

if(TARGET_REPLAY_BUILD_MOD)
    if(NOT GEODE_SDK)
        set(GEODE_SDK "$ENV{GEODE_SDK}")
    endif()
    if(NOT EXISTS "${GEODE_SDK}/CMakeLists.txt")
        message(FATAL_ERROR "Set GEODE_SDK to a Geode v5.8.2 SDK checkout, or disable TARGET_REPLAY_BUILD_MOD for core tests.")
    endif()
    add_subdirectory("${GEODE_SDK}" geode-sdk)
    add_library(${PROJECT_NAME} SHARED src/main.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE replay_core)
    setup_geode_mod(${PROJECT_NAME} DONT_INSTALL LINK_TYPE PRIVATE)
endif()
