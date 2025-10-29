
# Determine the software version
set(VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)
add_custom_target( genversion
                   COMMAND ${CMAKE_COMMAND} -D OUTPUT=${CMAKE_CURRENT_BINARY_DIR}
                                            -D VERSION_NUMBER=v${GSCM_VERSION_MAJOR}.${GSCM_VERSION_MINOR}
                                            -P ${CMAKE_SOURCE_DIR}/dependencies/cmake/genversion.cmake
                   BYPRODUCTS ${VERSION_FILE}
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} )
add_custom_command( OUTPUT ${VERSION_FILE} COMMAND ${CMAKE_COMMAND} 
                    -E echo "Generating ${VERSION_FILE}" DEPENDS genversion )

########################### Main applications ###########################

LINK_DIRECTORIES( ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${GLFW_LIBRARY_DIR} )

file(GLOB CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp 
                    ${CMAKE_CURRENT_SOURCE_DIR}/util/*.cpp 
                    ${CMAKE_SOURCE_DIR}/dependencies/program-options-lite/*.cpp 
                    ${CMAKE_SOURCE_DIR}/dependencies/glad/src/glad.c)

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
endif()
create_executable( NAME       mpeg-gsc-metrics 
                   SOURCES    ${CPP_FILES} 
                   INCLUDES   ${CMAKE_SOURCE_DIR}/source
                              ${CMAKE_SOURCE_DIR}/source/util
                              ${CMAKE_SOURCE_DIR}/dependencies
                              ${CMAKE_SOURCE_DIR}/dependencies/glm
                              ${CMAKE_SOURCE_DIR}/dependencies/glad/include
                              ${GLFW_INCLUDE_DIR} 
                              ${fmtlib_SOURCE_DIR}/include
                              ${${LIB_PMBB_BASE_NAME}_SOURCE_DIR}/src
                              ${${LIB_PMBB_CORE_NAME}_SOURCE_DIR}/src
                              ${${LIB_PMBB_IVQM_NAME}_SOURCE_DIR}/src
                              ${CMAKE_CURRENT_BINARY_DIR}
                   LIBRARIES  ${LIBS} )  
