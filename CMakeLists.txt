cmake_minimum_required (VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules/")
project (solid_queue)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -O2 -ggdb -Wall -Wextra -Werror -pedantic")
enable_testing()
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(example)

# === distclean target ============                  
ADD_CUSTOM_TARGET(distclean 
    COMMENT "Cleaning for source distribution")
	
set_property(TARGET distclean PROPERTY FOLDER "Helpers")

if (${CMAKE_GENERATOR} MATCHES Makefile)
	add_custom_command (TARGET distclean
						COMMAND make clean
	)
endif (${CMAKE_GENERATOR} MATCHES Makefile)

SET(DISTCLEANED_FILES
   ${DISTCLEANED_FILES}
   cmake.depends
   cmake.check_depends
   CMakeCache.txt
   cmake.check_cache
   Makefile
   *~
)

SET(DISTCLEANED_DIRS
   ${DISTCLEANED_DIRS}
   CMakeFiles
)

SET(DISTCLEANED_NOREC
    ${DISTCLEANED_NOREC}
    ${CMAKE_SOURCE_DIR}/install
    ${CMAKE_SOURCE_DIR}/install_manifest.txt
    ${CMAKE_SOURCE_DIR}/src/bin
)

# Для Windows не надо. Там работаем через Visual Studio
if (UNIX)
	FOREACH (DSTCLEANELEM ${DISTCLEANED_DIRS})
	   ADD_CUSTOM_COMMAND(
	    COMMAND find ${CMAKE_BINARY_DIR} -type d -name \"${DSTCLEANELEM}\" -print0 | xargs -0 rm -rf
	    TARGET  distclean
	  )
	ENDFOREACH (DSTCLEANELEM)

	FOREACH (DSTCLEANELEM ${DISTCLEANED_FILES})
	   ADD_CUSTOM_COMMAND(
	    COMMAND find ${CMAKE_BINARY_DIR} -type f -name \"${DSTCLEANELEM}\" -delete
	    TARGET  distclean
	  )
	ENDFOREACH (DSTCLEANELEM)

	FOREACH (DSTCLEANELEM ${DISTCLEANED_NOREC})
	   ADD_CUSTOM_COMMAND(
	    COMMAND rm -rf ${DSTCLEANELEM}
	    TARGET  distclean
	  )
	ENDFOREACH (DSTCLEANELEM)
endif(UNIX)