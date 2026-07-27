
set(VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)
add_custom_target( genversion
                   COMMAND ${CMAKE_COMMAND} -D OUTPUT=${CMAKE_CURRENT_BINARY_DIR}
                                            -D VERSION_NUMBER=v${GSCM_VERSION_MAJOR}.${GSCM_VERSION_MINOR}
                                            -P ${PROJECT_SOURCE_DIR}/dependencies/cmake/genversion.cmake
                   BYPRODUCTS ${VERSION_FILE}
                   WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} )

add_executable(importance-scores
  importanceScores.cpp
  rasterizer.cpp
  ${PROJECT_SOURCE_DIR}/dependencies/program-options-lite/program_options_lite.cpp
  ${VERSION_FILE})

add_dependencies(importance-scores genversion)

target_include_directories(importance-scores PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/util
  ${PROJECT_SOURCE_DIR}/dependencies
  ${PROJECT_SOURCE_DIR}/dependencies/glm
  ${CMAKE_CURRENT_BINARY_DIR})

if(USE_OPENMP)
  target_compile_definitions(importance-scores PRIVATE USE_OPENMP)
  target_link_libraries(importance-scores PRIVATE OpenMP::OpenMP_CXX)
endif()

install(TARGETS importance-scores DESTINATION bin)
