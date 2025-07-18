cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

#=========================================================================================================================================
# library PUT-Multimedia-Building-Blocks - Base
#=========================================================================================================================================
set(LIB_PMBB_BASE_NAME "pmbb-base")
set(LIB_PMBB_BASE_NAME "pmbb-base" PARENT_SCOPE)

project("${LIB_PMBB_BASE_NAME}")
add_library(${PROJECT_NAME} OBJECT "")
include(./TargetSources.cmake)
target_compile_features   (${PROJECT_NAME} PRIVATE cxx_std_17)
target_include_directories(${PROJECT_NAME} PRIVATE ${fmtlib_SOURCE_DIR}/include)

