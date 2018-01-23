#
# This module defines:
#
# FFmpeg_FOUND, if false, do not try to link to FFmpeg
# FFmpeg_LIBRARY, the name of the library to link against
# FFmpeg_INCLUDE_DIR, where to find FFmpeg.h
#

#----------------------------------------------------------------------------------
#
FUNCTION(dp_find_package mode component cmpt_header)	

	# set the default search paths on linux or MacOS.
	SET(SEARCH_PATH
	/Library/Frameworks ~/Library/Frameworks ~/work
	/usr/local /usr	/sw  /opt /opt/local /opt/csw )	

	# adjust multi components and independent package.
	SET(SEARCH_HDRS "${component}/${cmpt_header}" "lib${component}/${cmpt_header}" "${cmpt_header}")
	SET(SEARCH_LIBS "${component}"   			  "lib${component}")
	IF		(${mode} STREQUAL ${component})		
		SET(component)
		SET(link)
	ELSE()		
		SET(link "_")		
	ENDIF()
	
	# find headers, eg.gcc -I 
	FIND_PATH(${mode}_${component}${link}INCLUDE_DIR
		NAMES
			${SEARCH_HDRS}
		HINTS
			ENV DepsPath${_lib_suffix}
			ENV DepsPath
			${DepsPath${_lib_suffix}}
			${DepsPath}
		PATHS
			${SEARCH_PATH}
		PATH_SUFFIXES 
			inc incs include includes inc/sdl2 include/sdl2)

	# find libraries, eg.gcc -L
	FIND_LIBRARY(${mode}_${component}${link}LIBRARY
		NAMES
			${SEARCH_LIBS}
		HINTS
			ENV DepsPath${_lib_suffix}
			ENV DepsPath			
			${DepsPath${_lib_suffix}}
			${DepsPath}			
		PATHS
			${SEARCH_PATH}
		PATH_SUFFIXES
			lib${_lib_suffix} lib bin${_lib_suffix} bin
			../lib${_lib_suffix} ../lib ../bin${_lib_suffix} ../bin)
	FIND_LIBRARY(${mode}_${component}core${link}LIBRARY
		NAMES
			"${component}core" "lib${component}core"
		HINTS
			ENV DepsPath${_lib_suffix}
			ENV DepsPath			
			${DepsPath${_lib_suffix}}
			${DepsPath}			
		PATHS
			${SEARCH_PATH}
		PATH_SUFFIXES
			lib${_lib_suffix} lib bin${_lib_suffix} bin
			../lib${_lib_suffix} ../lib ../bin${_lib_suffix} ../bin)		
	# cache variable update upper string symbols	
	STRING(TOUPPER "${mode}_${component}${link}INCLUDE_DIR" 	U_MODE_CMPT_INCLUDE_DIR)
	STRING(TOUPPER "${mode}_${component}${link}LIBRARY"	  		U_MODE_CMPT_LIBRARY)
	SET(${U_MODE_CMPT_INCLUDE_DIR}	${${mode}_${component}${link}INCLUDE_DIR}	PARENT_SCOPE)
	SET(${U_MODE_CMPT_LIBRARY}		${${mode}_${component}${link}LIBRARY}		PARENT_SCOPE)
	
	MARK_AS_ADVANCED(${mode}_${component}${link}INCLUDE_DIR ${mode}_${component}${link}LIBRARY)
	
	# set package find flag to true or false.
	if(${mode}_${component}${link}INCLUDE_DIR AND ${mode}_${component}${link}LIBRARY)
		STRING(TOUPPER "${mode}_${component}FOUND" U_MODE_CMPT_FOUND)
		SET(${mode}_${component}FOUND 	TRUE PARENT_SCOPE)	
		SET(${U_MODE_CMPT_FOUND}		TRUE PARENT_SCOPE)
		# update ${mode}_INCLUDE_DIR
		LIST(APPEND ${mode}_INCLUDE_DIR ${${mode}_${component}${link}INCLUDE_DIR})
		LIST(REMOVE_DUPLICATES ${mode}_INCLUDE_DIR)
		SET(${mode}_INCLUDE_DIR "${${mode}_INCLUDE_DIR}" PARENT_SCOPE)
		# update ${mode}_LIBRARY
		LIST(APPEND ${mode}_LIBRARY 	${${mode}_${component}${link}LIBRARY} ${${mode}_${component}core${link}LIBRARY})
		LIST(REMOVE_DUPLICATES ${mode}_LIBRARY)
		SET(${mode}_LIBRARY "${${mode}_LIBRARY}" PARENT_SCOPE)		
	else()
		MESSAGE("--${${mode}_${component}${link}INCLUDE_DIR},${${mode}_${component}${link}LIBRARY}")
	endif()
	
ENDFUNCTION()

#
MACRO(do_find_package mode)
	IF( NOT ${mode}_FIND_COMPONENTS)
		#MESSAGE(STATUS "${mode} is searched as an independent package!")
		dp_find_package(${mode} vlc vlc.h)
	ELSE()
		FOREACH(cmpt_item ${${mode}_FIND_COMPONENTS})
			IF		(cmpt_item STREQUAL ${mode})
				dp_find_package(${mode} vlc vlc.h)			
			ELSE()
				MESSAGE(FATAL_ERROR "Unknown cmpt requested: ${cmpt_item}")
			ENDIF()
		ENDFOREACH()
	ENDIF()
ENDMACRO()

#----------------------------------------------------------------------------------
# 查找入口
SET(MODE 	    "LIBVLC")
SET(${MODE}_FOUND  FALSE)
SET(${MODE}_LIBRARY)
SET(${MODE}_INCLUDE_DIR )

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	SET(_lib_suffix 64)
else()
	SET(_lib_suffix 32)
endif()

do_find_package(${MODE})

FIND_PACKAGE(PkgConfig QUIET)
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(${MODE} REQUIRED_VARS ${MODE}_LIBRARY ${MODE}_INCLUDE_DIR)
#----------------------------------------------------------------------------------

