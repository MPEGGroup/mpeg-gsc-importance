# additional target to perform clang-format run, requires clang-format

if( NOT TARGET clang-format )
  find_program(CLANG_FORMAT_PATH NAMES clang-format )
  if(DEFINED ENV{CC}) 
    get_filename_component(NAME $ENV{CC} NAME)
    if( ${NAME} STREQUAL "clang" )
      set( CLANG_FORMAT_PATH $ENV{CC}-format )
      message("  CLANG_FORMAT_PATH    =  ${CLANG_FORMAT_PATH} " )
    endif()
  endif()

  if( EXISTS  "${CLANG_FORMAT_PATH}" )
    file(GLOB_RECURSE ALL_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/source/app/*.cpp  
                                       ${CMAKE_CURRENT_SOURCE_DIR}/source/app/*.hpp )
    add_custom_target( clang-format COMMAND ${CLANG_FORMAT_PATH} -style=file -i ${ALL_SOURCE_FILES} )
  else()
      message("clang-format executable not found")
  endif()
endif()    
