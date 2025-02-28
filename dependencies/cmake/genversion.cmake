# Generate a version file from a template based on the VCS version
# - Determine the current version string
#    => If git is unavailable, fallback to specified string
# - Test if it is the same as the cached version
# - If same, do nothing, otherwise generate output
set(VERSION_HPP   ${OUTPUT}/version.hpp)
set(VERSION_CPP   ${OUTPUT}/version.cpp)
set(VERSION_IN    ${OUTPUT}/version.cpp.in)
set(VERSION_CACHE ${OUTPUT}/version.cache)
set(VERSION_EXTRA "" )

if(EXISTS ${VERSION_CACHE})
  file(READ ${VERSION_CACHE} VERSION_CACHED)
endif()  
if(NOT EXISTS ${VERSION_HPP})
  file(WRITE ${VERSION_HPP} "#pragma once\n\nnamespace gscm {\nextern const char version[];\n}\n" )
endif()
if(NOT EXISTS ${VERSION_IN})
  file(WRITE ${VERSION_IN} "#include \"version.hpp\"\n\nnamespace gscm {\nconst char version[] = \"@VERSION@\";\n}\n")
endif()

find_package(Git)
if(NOT GIT_EXECUTABLE)
  set(VERSION "${VERSION_NUMBER}${VERSION_EXTRA}")
else()
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --long --always 
    OUTPUT_VARIABLE VERSION
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE )
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    OUTPUT_VARIABLE BRANCH
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE ) 
  execute_process(
    COMMAND ${GIT_EXECUTABLE} status --porcelain
    OUTPUT_VARIABLE STATUS
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE )
  set(VERSION "${VERSION_NUMBER}-t${VERSION}-b${BRANCH}${VERSION_EXTRA}" )
  if ( NOT "${STATUS}" STREQUAL "" ) 
    set(VERSION "${VERSION}-uncommited-changes" )
  endif()
endif()

if(NOT VERSION_CACHED STREQUAL VERSION )
  configure_file(${VERSION_IN} ${VERSION_CPP})
  file(WRITE ${VERSION_CACHE} ${VERSION})
endif()

message("GSCM version: ${VERSION}${VERSION_EXTRA}")