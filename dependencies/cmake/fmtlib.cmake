set( DIR ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/fmtlib )
set(FMT_USE_CONSTEVAL OFF CACHE BOOL "Disable consteval" FORCE)
if(NOT EXISTS ${DIR})
  CPMAddPackage(NAME              fmt
                GITHUB_REPOSITORY fmtlib/fmt
                GIT_TAG           11.0.2
                SOURCE_DIR        ${DIR} 
                OPTIONS           "FMT_USE_CONSTEVAL OFF" )
else()
  add_subdirectory(${DIR} ${CMAKE_BINARY_DIR}/fmt_build)
endif()
if(TARGET fmt)
  if(MSVC)
    target_compile_options(fmt INTERFACE /wd4005)
  else()
    target_compile_options(fmt INTERFACE -Wno-macro-redefined)
  endif()
  target_compile_definitions(fmt INTERFACE FMT_USE_CONSTEVAL=0)
  target_compile_features(fmt INTERFACE cxx_std_17)
  set_target_properties(fmt PROPERTIES CXX_STANDARD 17)
endif()

set( fmtlib_SOURCE_DIR ${DIR})