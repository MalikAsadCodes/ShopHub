cmake_minimum_required(VERSION 3.19)

project(ShopHub
    VERSION 2.0.0
    DESCRIPTION "ShopHub - e-commerce desktop application"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 6.4 or newer. Install Qt with the "MinGW" or "MSVC" kit on Windows.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Sql Network Gui Widgets Test)
qt_standard_project_setup()

option(SHOPHUB_BUILD_TESTS "Build the automated tests" ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src/core)
add_subdirectory(src/app)

if(SHOPHUB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
