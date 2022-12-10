cmake_minimum_required(VERSION 3.5)
include(FetchContent)

project(flappy LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

find_package(OpenGL REQUIRED)

add_executable(flappy flappy_bird.cpp)
target_include_directories(flappy PRIVATE "../src/")

target_link_libraries(flappy PRIVATE glfw ${OPENGL_LIBRARIES})
