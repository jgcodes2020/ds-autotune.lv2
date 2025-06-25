cmake_minimum_required(VERSION 3.28.3 FATAL_ERROR)
project(
    dsat
    DESCRIPTION "My attempt to design an autotune plugin."
    LANGUAGES CXX
)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# default build type
get_property(is_multiconfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT is_multiconfig AND NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting default build type to Debug")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build configuration for CMake." FORCE)
endif()

# libraries
include(FetchContent)

# download lvtk off GitHub and wrangle it into working
FetchContent_Declare(lvtk
    GIT_REPOSITORY "https://gitlab.com/lvtk/lvtk.git"
    GIT_TAG "c2abd213602ff3ebcb513755b5f4d23f0b7492ab"
)

# wrangle xtensor into working
FetchContent_Declare(xtl
    GIT_REPOSITORY "https://github.com/xtensor-stack/xtl.git"
    GIT_TAG "3634f2ded19e0cf38208c8b86cea9e1d7c8e397d"
    FIND_PACKAGE_ARGS
)
FetchContent_Declare(xtensor
    GIT_REPOSITORY "https://github.com/xtensor-stack/xtensor.git"
    GIT_TAG "3634f2ded19e0cf38208c8b86cea9e1d7c8e397d"
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(lvtk xtensor)

FetchContent_GetProperties(lvtk SOURCE_DIR lvtk_srcdir)

find_package(FFTW3 REQUIRED)

add_library(lvtk_lib INTERFACE IMPORTED)
target_include_directories(lvtk_lib INTERFACE "${lvtk_srcdir}/include")
add_library(lvtk::lvtk ALIAS lvtk_lib)

include("cmake/Lv2Validation.cmake")

add_subdirectory(src)
