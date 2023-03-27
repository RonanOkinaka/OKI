cmake_minimum_required(VERSION 3.19)

project(oki LANGUAGES CXX)

# Only if we're the top-level project should the tests + examples get built
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    add_subdirectory(test)
    add_subdirectory(examples)
endif()
