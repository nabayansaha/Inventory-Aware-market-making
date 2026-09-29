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
    src/simulation/ExternalOrderManager.cpp
    src/simulation/TraderBridge.cpp
    src/net/TcpJsonServer.cpp
    src/analysis/Statistics.cpp
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
        tests/test_montecarlo.cpp
        tests/test_oms.cpp
    )
    target_link_libraries(mm_tests PRIVATE mm_core Catch2::Catch2WithMain)
    include(Catch)
    catch_discover_tests(mm_tests)
endif()

add_executable(market_maker src/main.cpp)
target_link_libraries(market_maker PRIVATE mm_core)

if(MM_BUILD_UI)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)

    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.9
    )
    FetchContent_Declare(
        implot
        GIT_REPOSITORY https://github.com/epezent/implot.git
        GIT_TAG v0.16
    )
    FetchContent_MakeAvailable(imgui implot)

    add_library(imgui_lib STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui_lib PUBLIC glfw OpenGL::GL)

    add_library(implot_lib STATIC
        ${implot_SOURCE_DIR}/implot.cpp
        ${implot_SOURCE_DIR}/implot_items.cpp
    )
    target_include_directories(implot_lib PUBLIC ${implot_SOURCE_DIR})
    target_link_libraries(implot_lib PUBLIC imgui_lib)

    target_sources(market_maker PRIVATE
        src/ui/TradingTerminal.cpp
        src/ui/Charts.cpp
        src/ui/OrderBookView.cpp
        src/ui/EventTape.cpp
    )
    target_compile_definitions(market_maker PRIVATE MM_HAS_UI=1 GL_SILENCE_DEPRECATION=1)
    target_link_libraries(market_maker PRIVATE imgui_lib implot_lib)

    if(APPLE)
        target_link_libraries(market_maker PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    endif()
endif()
