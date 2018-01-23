
# display the cmake enviroments for debug.
FUNCTION(get_archBits arg)
IF(CMAKE_SIZEOF_VOID_P EQUAL 8)
	SET(${arg} 64 PARENT_SCOPE)
ELSE()
	SET(${arg} 32 PARENT_SCOPE)
ENDIF()
ENDFUNCTION()

# display the cmake enviroments for debug.
FUNCTION(display_enviroments arg)
IF(arg)
MESSAGE(STATUS "-----------------------------------------------------------")
MESSAGE(STATUS "01 CMAKE_VERSION        	= ${CMAKE_VERSION}"				)
MESSAGE(STATUS "02 PROJECT_SOURCE_DIR		= ${PROJECT_SOURCE_DIR}"		)
MESSAGE(STATUS "03 PROJECT_BINARY_DIR		= ${PROJECT_BINARY_DIR}"		)
MESSAGE(STATUS "04 CMAKE_SOURCE_DIR  		= ${CMAKE_SOURCE_DIR}"			)
MESSAGE(STATUS "05 CMAKE_BINARY_DIR  		= ${CMAKE_BINARY_DIR}"			)
MESSAGE(STATUS "06 CMAKE_CURRENT_SOURCE_DIR	= ${CMAKE_CURRENT_SOURCE_DIR}"	)
MESSAGE(STATUS "07 CMAKE_CURRENT_BINARY_DIR	= ${CMAKE_CURRENT_BINARY_DIR}"	)
MESSAGE(STATUS "08 CMAKE_INCLUDE_PATH		= ${CMAKE_INCLUDE_PATH}"		)
MESSAGE(STATUS "09 CMAKE_LIBRARY_PATH		= ${CMAKE_LIBRARY_PATH}"		)
MESSAGE(STATUS "10 LIBRARY_OUTPUT_PATH		= ${LIBRARY_OUTPUT_PATH}"		)
MESSAGE(STATUS "11 EXECUTABLE_OUTPUT_PATH	= ${EXECUTABLE_OUTPUT_PATH}"	)
MESSAGE(STATUS "12 PROJECT_NAME         	= ${PROJECT_NAME}"				)
MESSAGE(STATUS "13 BUILD_SHARED_LIBS		= ${BUILD_SHARED_LIBS}"			)
MESSAGE(STATUS "14 CMAKE_C_FLAGS         	= ${CMAKE_C_FLAGS}"				)
MESSAGE(STATUS "15 CMAKE_CXX_FLAGS       	= ${CMAKE_CXX_FLAGS}"			)
MESSAGE(STATUS "16 CMAKE_SYSTEM          	= ${CMAKE_SYSTEM}"				)
MESSAGE(STATUS "17 CMAKE_SYSTEM_PROCESSOR	= ${CMAKE_SYSTEM_PROCESSOR}"	)
MESSAGE(STATUS "18 WIN32 = ${WIN32} UNIX = ${UNIX} APPLE = ${APPLE}"		)
MESSAGE(STATUS "19 CMAKE_MODULE_PATH		= ${CMAKE_MODULE_PATH}"			)
MESSAGE(STATUS "20 CMAKE_PREFIX_PATH		= ${CMAKE_PREFIX_PATH}"			)
MESSAGE(STATUS "21 CMAKE_BUILD_TYPE 		= ${CMAKE_BUILD_TYPE}"			)
MESSAGE(STATUS "22 CMAKE_CURRENT_LIST_FILE	= ${CMAKE_CURRENT_LIST_FILE}"	)
MESSAGE(STATUS "23 CMAKE_CURRENT_LIST_LINE	= ${CMAKE_CURRENT_LIST_LINE}"	)
MESSAGE(STATUS "24 CMAKE_COMPILER_IS_CLANG	= ${CMAKE_COMPILER_IS_CLANG}"	)
MESSAGE(STATUS "25 CMAKE_SIZEOF_VOID_P		= ${CMAKE_SIZEOF_VOID_P}"		)
MESSAGE(STATUS "26 CMAKE_INSTALL_PREFIX   	= ${CMAKE_INSTALL_PREFIX}"		)
MESSAGE(STATUS "-----------------------------------------------------------")
ENDIF()
ENDFUNCTION()

FUNCTION(copy_dependence)

IF(NOT COPY_DEPENDENCIES OR COPIED_DEPENDENCIES)
	return()
ENDIF()

get_archBits(_bin_suffix)

IF(MSVC)	
FILE(GLOB FFmpeg_BIN_FILES	
	MESSAGE("---${FFmpeg_avcodec_INCLUDE_DIR}")
	"${FFmpeg_avcodec_INCLUDE_DIR}/../../bin/avcodec-*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../../bin${_bin_suffix}/avcodec-*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin${_bin_suffix}/avcodec-*.dll"

	"${FFmpeg_avformat_INCLUDE_DIR}/../../bin/avformat-*.dll"
	"${FFmpeg_avformat_INCLUDE_DIR}/../../bin${_bin_suffix}/avformat-*.dll"
	"${FFmpeg_avformat_INCLUDE_DIR}/bin${_bin_suffix}/avformat-*.dll"

	"${FFmpeg_avutil_INCLUDE_DIR}/../../bin/avutil-*.dll"
	"${FFmpeg_avutil_INCLUDE_DIR}/../../bin${_bin_suffix}/avutil-*.dll"
	"${FFmpeg_avutil_INCLUDE_DIR}/../bin${_bin_suffix}/avutil-*.dll"

	"${FFmpeg_avdevice_INCLUDE_DIR}/../../bin/avdevice-*.dll"
	"${FFmpeg_avdevice_INCLUDE_DIR}/../../bin${_bin_suffix}/avdevice-*.dll"
	"${FFmpeg_avdevice_INCLUDE_DIR}/../bin${_bin_suffix}/avdevice-*.dll"

	"${FFmpeg_avfilter_INCLUDE_DIR}/../../bin/avfilter-*.dll"
	"${FFmpeg_avfilter_INCLUDE_DIR}/../../bin${_bin_suffix}/avfilter-*.dll"
	"${FFmpeg_avfilter_INCLUDE_DIR}/../../bin${_bin_suffix}/avfilter-*.dll"

	"${FFmpeg_postproc_INCLUDE_DIR}/../../bin/postproc-*.dll"
	"${FFmpeg_postproc_INCLUDE_DIR}/../../bin${_bin_suffix}/postproc-*.dll"
	"${FFmpeg_postproc_INCLUDE_DIR}/../bin${_bin_suffix}/postproc-*.dll"

	"${FFmpeg_swscale_INCLUDE_DIR}/../../bin/swscale-*.dll"
	"${FFmpeg_swscale_INCLUDE_DIR}/../bin${_bin_suffix}/swscale-*.dll"
	"${FFmpeg_swscale_INCLUDE_DIR}/../../bin${_bin_suffix}/swscale-*.dll"

	"${FFmpeg_swresample_INCLUDE_DIR}/../../bin/swresample-*.dll"
	"${FFmpeg_swresample_INCLUDE_DIR}/../../bin${_bin_suffix}/swresample-*.dll"
	"${FFmpeg_swresample_INCLUDE_DIR}/../bin${_bin_suffix}/swresample-*.dll"

	"${FFmpeg_avcodec_INCLUDE_DIR}/../../bin/libopus*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../../bin/opus*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin/libopus*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin/opus*.dll"

	"${FFmpeg_avcodec_INCLUDE_DIR}/../../bin/libogg*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../../bin/libvorbis*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin/libogg*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin/libvorbis*.dll"

	"${FFmpeg_avcodec_INCLUDE_DIR}/../../bin/libvpx*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin/libvpx*.dll"

	"${FFmpeg_avcodec_INCLUDE_DIR}/../../bin${_bin_suffix}/libopus*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../../bin${_bin_suffix}/opus*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin${_bin_suffix}/libopus*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin${_bin_suffix}/opus*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../../bin/libbz2*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../../bin/zlib*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin/libbz2*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin/zlib*.dll"

	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin${_bin_suffix}/libbz2*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin${_bin_suffix}/zlib*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin${_bin_suffix}/libbz2*.dll"
	"${FFmpeg_avcodec_INCLUDE_DIR}/../bin${_bin_suffix}/zlib*.dll"
	)
FILE(GLOB SDL2_BIN_FILES
	"${SDL2_INCLUDE_DIR}/../../bin/SDL*.dll")
	
FILE(GLOB X264_BIN_FILES
	"${X264_INCLUDE_DIR}/../bin${_bin_suffix}/libx264-*.dll"
	"${X264_INCLUDE_DIR}/../bin/libx264-*.dll"
	"${X264_INCLUDE_DIR}/bin/libx264-*.dll"
	"${X264_INCLUDE_DIR}/bin${_bin_suffix}/libx264-*.dll")

FILE(GLOB FREETYPE_BIN_FILES
	"${FREETYPE_INCLUDE_DIR_ft2build}/../../bin${_bin_suffix}/libfreetype*-*.dll"
	"${FREETYPE_INCLUDE_DIR_ft2build}/../../bin/libfreetype*-*.dll"
	"${FREETYPE_INCLUDE_DIR_ft2build}/../bin${_bin_suffix}/libfreetype*-*.dll"
	"${FREETYPE_INCLUDE_DIR_ft2build}/../bin/libfreetype*-*.dll"
	"${FREETYPE_INCLUDE_DIR_ft2build}/bin/libfreetype*-*.dll"
	"${FREETYPE_INCLUDE_DIR_ft2build}/bin${_bin_suffix}/libfreetype*-*.dll")

FILE(GLOB LIBFDK_BIN_FILES
	"${Libfdk_INCLUDE_DIR}/../bin${_bin_suffix}/libfdk*-*.dll"
	"${Libfdk_INCLUDE_DIR}/../bin/libfdk*-*.dll"
	"${Libfdk_INCLUDE_DIR}/bin/libfdk*-*.dll"
	"${Libfdk_INCLUDE_DIR}/bin${_bin_suffix}/libfdk*-*.dll")

FILE(GLOB SSL_BIN_FILES
	"${SSL_INCLUDE_DIR}/../bin${_bin_suffix}/ssleay32*.dll"
	"${SSL_INCLUDE_DIR}/../bin${_bin_suffix}/libeay32*.dll"
	"${SSL_INCLUDE_DIR}/../bin/ssleay32*.dll"
	"${SSL_INCLUDE_DIR}/../bin/libeay32*.dll"
	"${SSL_INCLUDE_DIR}/bin${_bin_suffix}/ssleay32*.dll"
	"${SSL_INCLUDE_DIR}/bin${_bin_suffix}/libeay32*.dll"
	"${SSL_INCLUDE_DIR}/bin/ssleay32*.dll"
	"${SSL_INCLUDE_DIR}/bin/libeay32*.dll")

FILE(GLOB CURL_BIN_FILES
	"${CURL_INCLUDE_DIR}/../build/Win${_bin_suffix}/VC12/DLL Release - DLL Windows SSPI/libcurl.dll"
	"${CURL_INCLUDE_DIR}/../bin${_bin_suffix}/libcurl*.dll"
	"${CURL_INCLUDE_DIR}/../bin${_bin_suffix}/curl*.dll"
	"${CURL_INCLUDE_DIR}/../bin/libcurl*.dll"
	"${CURL_INCLUDE_DIR}/../bin/curl*.dll"
	"${CURL_INCLUDE_DIR}/bin${_bin_suffix}/libcurl*.dll"
	"${CURL_INCLUDE_DIR}/bin${_bin_suffix}/curl*.dll"
	"${CURL_INCLUDE_DIR}/bin/libcurl*.dll"
	"${CURL_INCLUDE_DIR}/bin/curl*.dll"
	)

IF (ZLIB_LIB)
	GET_FILENAME_COMPONENT(ZLIB_BIN_PATH ${ZLIB_LIB} PATH)
ENDIF()
FILE(GLOB ZLIB_BIN_FILES
	"${ZLIB_BIN_PATH}/zlib*.dll")

IF (NOT ZLIB_BIN_FILES)
	FILE(GLOB ZLIB_BIN_FILES
		"${ZLIB_INCLUDE_DIR}/../bin${_bin_suffix}/zlib*.dll"
		"${ZLIB_INCLUDE_DIR}/../bin/zlib*.dll"
		"${ZLIB_INCLUDE_DIR}/bin${_bin_suffix}/zlib*.dll"
		"${ZLIB_INCLUDE_DIR}/bin/zlib*.dll"
		)
ENDIF()

IF (CMAKE_CONFIGURATION_TYPES MATCHES "Debug")
	FILE(GLOB QT_DEBUG_BIN_FILES
		"${Qt5Core_DIR}/../../../bin/Qt5Cored.dll"
		"${Qt5Core_DIR}/../../../bin/Qt5Guid.dll"
		"${Qt5Core_DIR}/../../../bin/Qt5Widgetsd.dll"
		"${Qt5Core_DIR}/../../../bin/libGLESv2d.dll"
		"${Qt5Core_DIR}/../../../bin/libEGLd.dll")
	FILE(GLOB QT_DEBUG_PLAT_BIN_FILES
		"${Qt5Core_DIR}/../../../plugins/platforms/qwindowsd.dll")
ENDIF()

IF (CMAKE_CONFIGURATION_TYPES MATCHES "Rel")
	FILE(GLOB QT_BIN_FILES
		"${Qt5Core_DIR}/../../../bin/Qt5Core.dll"
		"${Qt5Core_DIR}/../../../bin/Qt5Gui.dll"
		"${Qt5Core_DIR}/../../../bin/Qt5Widgets.dll"
		"${Qt5Core_DIR}/../../../bin/libGLESv2.dll"
		"${Qt5Core_DIR}/../../../bin/libEGL.dll")
	FILE(GLOB QT_PLAT_BIN_FILES
		"${Qt5Core_DIR}/../../../plugins/platforms/qwindows.dll")
ENDIF()

FILE(GLOB QT_ICU_BIN_FILES
	"${Qt5Core_DIR}/../../../bin/icu*.dll")

	
SET(ALL_BASE_BIN_FILES
	${FFmpeg_BIN_FILES}
	${SDL2_BIN_FILES}
	${X264_BIN_FILES}
	${CURL_BIN_FILES}
	${SSL_BIN_FILES}
	${ZLIB_BIN_FILES}
	${LIBFDK_BIN_FILES}
	${FREETYPE_BIN_FILES}
	${QT_ICU_BIN_FILES})

SET(ALL_REL_BIN_FILES
	${QT_BIN_FILES})

SET(ALL_DBG_BIN_FILES
	${QT_DEBUG_BIN_FILES})

SET(ALL_PLATFORM_BIN_FILES)

SET(ALL_PLATFORM_REL_BIN_FILES
	${QT_PLAT_BIN_FILES})

SET(ALL_PLATFORM_DBG_BIN_FILES
	${QT_DEBUG_PLAT_BIN_FILES})

FOREACH(item
		ALL_BASE_BIN_FILES ALL_REL_BIN_FILES ALL_DBG_BIN_FILES
		ALL_PLATFORM_BIN_FILES ALL_PLATFORM_REL_BIN_FILES ALL_PLATFORM_DBG_BIN_FILES)
	IF(${item})
		list(REMOVE_DUPLICATES ${item})
	ENDIF()
	FOREACH(i ${${item}})
		MESSAGE("${i}")
	ENDFOREACH()
ENDFOREACH()

MESSAGE(STATUS "FFmpeg FILEs: ${FFmpeg_BIN_FILES}")
MESSAGE(STATUS "SDL2 FILEs: ${SDL2_BIN_FILES}")
MESSAGE(STATUS "x264 FILEs: ${X264_BIN_FILES}")
MESSAGE(STATUS "Libfdk FILEs: ${LIBFDK_BIN_FILES}")
MESSAGE(STATUS "Freetype FILEs: ${FREETYPE_BIN_FILES}")
MESSAGE(STATUS "curl FILEs: ${CURL_BIN_FILES}")
MESSAGE(STATUS "ssl FILEs: ${SSL_BIN_FILES}")
MESSAGE(STATUS "zlib FILEs: ${ZLIB_BIN_FILES}")
MESSAGE(STATUS "QT Debug FILEs: ${QT_DEBUG_BIN_FILES}")
MESSAGE(STATUS "QT Debug Platform FILEs: ${QT_DEBUG_PLAT_BIN_FILES}")
MESSAGE(STATUS "QT Release FILEs: ${QT_BIN_FILES}")
MESSAGE(STATUS "QT Release Platform FILEs: ${QT_PLAT_BIN_FILES}")
MESSAGE(STATUS "QT ICU FILEs: ${QT_ICU_BIN_FILES}")

# dependencies destination dirs.
SET(DEST_COPY_PATH ${CMAKE_BINARY_DIR}/app_dependencies)

FOREACH(BinFile ${ALL_BASE_BIN_FILES})
	MESSAGE(STATUS "copying ${BinFile} to ${DEST_COPY_PATH}/exec${_bin_suffix}")
	FILE(COPY "${BinFile}" DESTINATION "${DEST_COPY_PATH}/exec${_bin_suffix}/")
ENDFOREACH()

FOREACH(BinFile ${ALL_REL_BIN_FILES})
	MESSAGE(STATUS "copying ${BinFile} to ${DEST_COPY_PATH}/exec${_bin_suffix}r")
	FILE(COPY "${BinFile}" DESTINATION "${DEST_COPY_PATH}/exec${_bin_suffix}r/")
ENDFOREACH()

FOREACH(BinFile ${ALL_DBG_BIN_FILES})
	MESSAGE(STATUS "copying ${BinFile} to ${DEST_COPY_PATH}/exec${_bin_suffix}d")
	FILE(COPY "${BinFile}" DESTINATION "${DEST_COPY_PATH}/exec${_bin_suffix}d/")
ENDFOREACH()

FOREACH(BinFile ${ALL_PLATFORM_BIN_FILES})
	make_directory("${DEST_COPY_PATH}/exec${_bin_suffix}/platforms")
	FILE(COPY "${BinFile}" DESTINATION "${DEST_COPY_PATH}/exec${_bin_suffix}/platforms/")
ENDFOREACH()

FOREACH(BinFile ${ALL_PLATFORM_REL_BIN_FILES})
	make_directory("${DEST_COPY_PATH}/exec${_bin_suffix}r/platforms")
	FILE(COPY "${BinFile}" DESTINATION "${DEST_COPY_PATH}/exec${_bin_suffix}r/platforms/")
ENDFOREACH()

FOREACH(BinFile ${ALL_PLATFORM_DBG_BIN_FILES})
	make_directory("${DEST_COPY_PATH}/exec${_bin_suffix}d/platforms")
	FILE(COPY "${BinFile}" DESTINATION "${DEST_COPY_PATH}/exec${_bin_suffix}d/platforms/")
ENDFOREACH()

SET(COPIED_DEPENDENCIES TRUE CACHE BOOL "Dependencies have been copied, SET to false to copy again" FORCE)
ENDIF()
ENDFUNCTION()
