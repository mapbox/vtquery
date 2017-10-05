cmake_minimum_required(VERSION 3.8)
project(hpp_skel LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED on)
# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/mason.cmake)

option(WERROR "Add -Werror flag to build (turns warnings into errors)" ON)

# configure optimization
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(OPTIMIZATION_FLAGS "-O0 -DDEBUG")
    message("-- Configuring debug build")
else()
    set(OPTIMIZATION_FLAGS "-O3 -DNDEBUG")
    message("-- Configuring release build")
endif()

# Note: -D_GLIBCXX_USE_CXX11_ABI=0 is needed to support mason packages that are precompiled libs
# Currently we only depend on a header only library, but this will help avoid issues when more libs are added via mason
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPTIMIZATION_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=0 -Wall -Wextra -pedantic -Wsign-compare -Wconversion -Wshadow")

if (WERROR)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()

# includes
mason_use(geometry VERSION 0.9.2 HEADER_ONLY)
mason_use(variant VERSION 1.1.4 HEADER_ONLY)
# mason_use(vtzero VERSION fa6682b HEADER_ONLY)
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/../spatial-algorithms/include) # todo: package in mason
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/../protozero/include) # todo: package in mason
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/../vtzero/include) # todo: package in mason
include_directories(SYSTEM ${MASON_PACKAGE_geometry_INCLUDE_DIRS})
include_directories(SYSTEM ${MASON_PACKAGE_variant_INCLUDE_DIRS})
include_directories(SYSTEM ${MASON_PACKAGE_vtzero_INCLUDE_DIRS})

# create vtquery script
file (GLOB VTQUERY_SOURCE vtquery/*.cpp)
add_executable(vtquery ${VTQUERY_SOURCE})

# link benchmark static library to the bench-tests binary so the bench tests know where to find the benchmark impl code
target_link_libraries(vtquery -lz ${CMAKE_CURRENT_SOURCE_DIR}/../spatial-algorithms/build/libspatialalgorithms.a)
