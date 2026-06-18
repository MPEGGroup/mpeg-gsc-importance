
# Determine the software version
set(VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)
add_custom_target( genversion
                   COMMAND ${CMAKE_COMMAND} -D OUTPUT=${CMAKE_CURRENT_BINARY_DIR}
                                            -D VERSION_NUMBER=v${GSCM_VERSION_MAJOR}.${GSCM_VERSION_MINOR}
                                            -P ${PROJECT_SOURCE_DIR}/dependencies/cmake/genversion.cmake
                   BYPRODUCTS ${VERSION_FILE}
                   WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} )

########################### Main applications ###########################

LINK_DIRECTORIES( ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${GLFW_LIBRARY_DIR} )

file(GLOB COMMON_CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/metric.cpp 
                    ${CMAKE_CURRENT_SOURCE_DIR}/rasterizer.cpp 
                    ${CMAKE_CURRENT_SOURCE_DIR}/rasterizerHardware.cpp 
                    ${CMAKE_CURRENT_SOURCE_DIR}/rasterizerSoftware.cpp 
                    ${CMAKE_CURRENT_SOURCE_DIR}/util/*.cpp 
                    ${PROJECT_SOURCE_DIR}/dependencies/program-options-lite/*.cpp 
                    ${PROJECT_SOURCE_DIR}/dependencies/glad/src/glad.c)

set(MPEG_GSC_METRICS_MAIN
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

set(IMPORTANCE_SCORES_MAIN
    ${CMAKE_CURRENT_SOURCE_DIR}/importanceScores.cpp)

find_package(Threads REQUIRED)
SET( LIBS Threads::Threads)
if ( NOT USE_CPU_ONLY ) 
  list(APPEND LIBS ${GLFW_LIBRARIES})
else()
  message("CPU only mode, disabling GLFW linking")
  if (WIN32)
    list (APPEND LIBS gdi32 user32)
  else()
    list (APPEND LIBS dl)
  endif()
endif()

list(APPEND LIBS ${LIB_PMBB_IVQM_NAME})
list(APPEND LIBS ${LIB_PMBB_CORE_NAME})
list(APPEND LIBS ${LIB_PMBB_BASE_NAME})
list(APPEND LIBS fmt::fmt)

if(USE_OPENMP)
  list(APPEND LIBS OpenMP::OpenMP_CXX)
  if(APPLE)
    list(APPEND LIBS omp)
  endif()
endif()

set(COMMON_INCLUDES
  ${PROJECT_SOURCE_DIR}/source
  ${PROJECT_SOURCE_DIR}/source/util
  ${PROJECT_SOURCE_DIR}/dependencies
  ${PROJECT_SOURCE_DIR}/dependencies/glm
  ${PROJECT_SOURCE_DIR}/dependencies/glad/include
  ${GLFW_INCLUDE_DIR} 
  ${fmtlib_SOURCE_DIR}/include
  ${${LIB_PMBB_BASE_NAME}_SOURCE_DIR}/src
  ${${LIB_PMBB_CORE_NAME}_SOURCE_DIR}/src
  ${${LIB_PMBB_IVQM_NAME}_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
)

create_executable( NAME       mpeg-gsc-metrics 
                   SOURCES    ${COMMON_CPP_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
                   INCLUDES   ${COMMON_INCLUDES}
                   LIBRARIES  ${LIBS} ) 

IF(BUILD_IMP_SCORES)
  create_executable( NAME       importance-scores 
                     SOURCES    ${COMMON_CPP_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/importanceScores.cpp
                     INCLUDES   ${COMMON_INCLUDES}
                     LIBRARIES  ${LIBS} )  
ENDIF()
