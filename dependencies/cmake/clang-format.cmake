# Additional target to format project sources. Requires clang-format.

if(NOT TARGET clang-format)
  find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)

  if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE FORMAT_SOURCE_FILES CONFIGURE_DEPENDS
      ${PROJECT_SOURCE_DIR}/source/*.cpp
      ${PROJECT_SOURCE_DIR}/source/*.hpp)

    add_custom_target(clang-format
      COMMAND ${CLANG_FORMAT_EXECUTABLE} -style=file -i ${FORMAT_SOURCE_FILES}
      COMMENT "Formatting C++ sources"
      VERBATIM)
  else()
    message(STATUS "clang-format executable not found; clang-format target disabled")
  endif()
endif()