cmake_minimum_required(VERSION 3.19)
include(FetchContent)

# Fetch and set up Catch2 dependency
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.0.1
)
FetchContent_MakeAvailable(Catch2)

# Express source files for unit testing [target: oki_unit]
add_executable(oki_unit
    oki_test_component.cpp
    oki_test_container.cpp
    oki_test_handle.cpp
    oki_test_observer.cpp
    oki_test_system.cpp
    oki_test_type_erasure.cpp
)

# Express external dependencies
target_include_directories(oki_unit PRIVATE "../src")
target_link_libraries(oki_unit PRIVATE Catch2::Catch2WithMain)

# Describe compiler features
target_compile_features(oki_unit PUBLIC cxx_std_17)
set_target_properties(oki_unit PROPERTIES CXX_EXTENSIONS OFF)

# Finally, register the unit tests
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(oki_unit)
