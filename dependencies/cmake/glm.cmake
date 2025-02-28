set( DIR ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/glm )
if( NOT EXISTS ${DIR} )
  CPMAddPackage( NAME             glm
                GIT_REPOSITORY    https://github.com/g-truc/glm.git
                GIT_TAG           1.0.1
                SOURCE_DIR        ${DIR} 
                DOWNLOAD_ONLY     YES)
endif()
