cmake_minimum_required(VERSION 3.17 FATAL_ERROR)

get_filename_component(MYNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
string(REPLACE " " "_" MYNAME ${MYNAME})
project(${MYNAME} C CXX)

# Building options
set(GSCM_VERSION_MAJOR            4 )
set(GSCM_VERSION_MINOR            0 )
set(CMAKE_CXX_STANDARD            23) 
set(CMAKE_CXX_STANDARD_REQUIRED   OFF)
set(CMAKE_CXX_EXTENSIONS          OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_COMPILER_WORKS      1)
set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE )
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type: Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
endif()

option( USE_CPU_ONLY "CPU only, disbale GPU rendering and display mode"   OFF ) 
option( USE_OPENMP   "Use OpenMP library if available"                    ON )
option( BUILD_IMP_SCORES   "Build executable for importance score calculation" ON )



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

## Open Multi-Processing (OpenMP)
message("USE_CPU_ONLY = ${USE_CPU_ONLY}")
message("USE_OPENMP   = ${USE_OPENMP}")
if( USE_OPENMP )
  find_package(OpenMP REQUIRED)
  if( OPENMP_FOUND )
    set( CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${OpenMP_C_FLAGS}"   )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}" )
  else()
    message( FATAL_ERROR "Package OpenMP required, but not found!, please install apt install libomp-dev libomp5 or brew install libomp-dev libomp5" )  
  endif()
endif()

message("BUILD_IMP_SCORES = ${BUILD_IMP_SCORES}")

# Cmake dependencies
include(${PROJECT_SOURCE_DIR}/dependencies/cmake/CPM.cmake)
include(${PROJECT_SOURCE_DIR}/dependencies/cmake/tools.cmake)
include(${PROJECT_SOURCE_DIR}/dependencies/cmake/clang-format.cmake)
include(${PROJECT_SOURCE_DIR}/dependencies/cmake/cppcheck.cmake)
include(${PROJECT_SOURCE_DIR}/dependencies/cmake/glm.cmake)
if ( NOT USE_CPU_ONLY )
  include(${PROJECT_SOURCE_DIR}/dependencies/cmake/glfw.cmake)
endif()
include(${PROJECT_SOURCE_DIR}/dependencies/cmake/fmtlib.cmake)
add_subdirectory(${PROJECT_SOURCE_DIR}/dependencies/pmbb-base libpmbb-base)
add_subdirectory(${PROJECT_SOURCE_DIR}/dependencies/pmbb-core libpmbb-core)
add_subdirectory(${PROJECT_SOURCE_DIR}/dependencies/pmbb-ivqm libpmbb-ivqm)

# Include 
include_directories(${PROJECT_SOURCE_DIR}/external/stb)
if( APPLE )
  if( USE_OPENMP )
    link_libraries(${OpenMP_CXX_LIBRARIES})
  endif()
endif()

# Build
add_subdirectory(${PROJECT_SOURCE_DIR}/source)


