cmake_minimum_required(VERSION 3.20)
project(market_maker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MM_BUILD_TESTS "Build unit tests" ON)
option(MM_BUILD_UI "Build Dear ImGui UI" ON)

include(FetchContent)
include(CTest)

set(MM_CORE_SOURCES
    src/model/Model.cpp
    src/model/Utility.cpp
    src/model/PriceProcess.cpp
    src/dp/Policy.cpp
    src/dp/DPSolver.cpp
    src/simulation/MarketEvent.cpp
    src/simulation/MarketSimulator.cpp
)

add_library(mm_core STATIC ${MM_CORE_SOURCES})
target_include_directories(mm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MM_BUILD_TESTS)
    FetchContent_Declare(
        catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.4
    )
    FetchContent_MakeAvailable(catch2)

    add_executable(mm_tests
        tests/test_model.cpp
        tests/test_dp.cpp
        tests/test_accounting.cpp
        tests/test_poisson.cpp
    )
    target_link_libraries(mm_tests PRIVATE mm_core Catch2::Catch2WithMain)
    include(Catch)
    catch_discover_tests(mm_tests)
endif()

# Placeholder executable until later phases wire the full app.
add_executable(market_maker src/main.cpp)
target_link_libraries(market_maker PRIVATE mm_core)
