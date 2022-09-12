cmake_minimum_required(VERSION 3.5)
include(FetchContent)

project(oki_unit LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.0.1
)

FetchContent_MakeAvailable(Catch2)

add_executable(oki_unit oki_test_handle.cpp oki_test_container.cpp)
target_include_directories(oki_unit PRIVATE "../src/")
target_link_libraries(oki_unit PRIVATE Catch2::Catch2WithMain)
