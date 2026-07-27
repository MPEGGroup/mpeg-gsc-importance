cmake_minimum_required(VERSION 3.17 FATAL_ERROR)

get_filename_component(MYNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" MYNAME ${MYNAME})
project(${MYNAME} CXX)

# Building options
set(GSCM_VERSION_MAJOR            4 )
set(GSCM_VERSION_MINOR            0 )
set(CMAKE_CXX_STANDARD            23) 
set(CMAKE_CXX_STANDARD_REQUIRED   ON)
set(CMAKE_CXX_EXTENSIONS          OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type: Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
endif()

option(USE_OPENMP "Use OpenMP for parallel viewpoint processing" ON)

include(${PROJECT_SOURCE_DIR}/dependencies/cmake/clang-format.cmake)
include(${PROJECT_SOURCE_DIR}/dependencies/cmake/cppcheck.cmake)
include(${PROJECT_SOURCE_DIR}/dependencies/cmake/CPM.cmake)
include(${PROJECT_SOURCE_DIR}/dependencies/cmake/glm.cmake)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if (MSVC)
    set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   /MD")
    set(CMAKE_C_FLAGS_DEBUG     "${CMAKE_C_FLAGS_DEBUG}     /MDd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   /MDd")
    set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS}  /NODEFAULTLIB:LIBCMT")
    add_compile_options(/utf-8)
elseif(APPLE)
  add_definitions(-DOS_MACOS)
endif()

message("USE_OPENMP = ${USE_OPENMP}")
if(USE_OPENMP)
  find_package(OpenMP REQUIRED COMPONENTS CXX)
endif()

add_subdirectory(source)


