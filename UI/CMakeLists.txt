# 
# cd cmake_path/build
# cmake -DCMAKE_PREFIX_PATH="qt_path;deps_path" -G"Visual Studio 14 2015" cmake_path
#

#----------------------------------------------------------------------------------
# 项目入口，此处cmake开始执行！
PROJECT(msp)

# 针对配置选项和环境变量的处理.
IF(ENABLE_UI)
	SET(FIND_MODE REQUIRED)
ELSE()
	SET(FIND_MODE QUIET)
ENDIF()

# Add default package search directories from $ENV{...}.
# get_archBits(_arc_suffix)
IF(DEFINED ENV{QTDI1R${_arc_suffix}} OR DEFINED ENV{DPDIR${_arc_suffix}})
	LIST(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR${_arc_suffix}};$ENV{DEPS_DIR${_arc_suffix}}")
elseif(DEFINED ENV{QTDIR} OR DEFINED ENV{DPDIR}})
	LIST(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR};$ENV{DPDIR}")
ENDIF()

IF(NOT CMAKE_PREFIX_PATH)
	FILE(GLOB   CMAKE_EXTERN_PATH "C:/Qt/Qt5.7.*/5.7/msvc201*"
	"${CMAKE_SOURCE_DIR}/dependenc*/win${_arc_suffix}")
	LIST(APPEND CMAKE_PREFIX_PATH ${CMAKE_EXTERN_PATH})
	MESSAGE("Add default search paths<Guess paths>: ${CMAKE_PREFIX_PATH}")
ENDIF()
#----------------------------------------------------------------------------------
# 针对Qt5工程通用配置,定位包.
# Find include files in the source and build directories.
SET(CMAKE_INCLUDE_CURRENT_DIR 	ON)
# Turn on automatic invocation of the moc files.
SET(CMAKE_AUTOMOC 				ON)
# Find the packages(libraries/headers dirs) of Qt5.
FIND_PACKAGE(Qt5 ${FIND_MODE}
	COMPONENTS Widgets Core Gui Network Xml OpenGL 
	LinguistTools WebChannel WebEngineWidgets )
#FIND_PACKAGE(Qt5Widgets 			${FIND_MODE})
IF(WIN32)
FIND_PACKAGE(Qt5AxContainer 		${FIND_MODE})
FIND_PACKAGE(Qt5AxBase 				${FIND_MODE})
ENDIF()
IF(		NOT CMAKE_PREFIX_PATH 
	OR 	NOT Qt5Widgets_DIR
	OR 	NOT Qt5LinguistTools_FOUND )
	IF (ENABLE_UI)
		MESSAGE(FATAL_ERROR "Failed to find Qt5![please cheCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}]")
	ELSE()
		MESSAGE(STATUS "Qt5 not found - UI disabled")
	ENDIF()
ENDIF()

#
# 针对FFmpeg通用配置,定位包.
FIND_PACKAGE(FFmpeg REQUIRED
	COMPONENTS avcodec avfilter avdevice avutil swscale avformat swresample postproc)
INCLUDE_DIRECTORIES(${FFmpeg_INCLUDE_DIR})

#
# 针对SDL2  通用配置,定位包.
FIND_PACKAGE(SDL2  REQUIRED)
INCLUDE_DIRECTORIES(${SDL2_INCLUDE_DIR})

#
# 针对libVLC通用配置,定位包.
FIND_PACKAGE(LIBVLC  REQUIRED)
INCLUDE_DIRECTORIES(${LIBVLC_INCLUDE_DIR})

# 外部依赖模块路径<传统模式>
# gcc -I path -L path.
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/inc")
LINK_DIRECTORIES(   "${PROJECT_SOURCE_DIR}/lib")
# 外部依赖模块名称<传统模式>
# gcc -ldependece.lib.
SET(EXTERN_LIBRARY dependece.lib)

#----------------------------------------------------------------------------------
# 针对GCC/G++/CLang/MSVC的配置项.
# Set compiler flags for c/c++. 
# you can use add_definitions("-Wall -ansi –pedantic –g”) to add flags and 
# remove_definitions(-DFOO -DBAR ...) to cancle flags
# CMAKE_BUILD_TYPE & BUILD_SHARED_LIBS
IF( 	CMAKE_COMPILER_IS_GNUCXX 
	OR 	CMAKE_COMPILER_IS_GNUCC
	OR 	CMAKE_COMPILER_IS_CLANG )
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-function -Werror-implicit-function-declaration -Wno-missing-field-initializers -fno-strict-aliasing")
	SET(CMAKE_C_FLAGS 	"${CMAKE_C_FLAGS} 	-Wall -Wextra -Wno-unused-function -Werror-implicit-function-declaration -Wno-missing-braces -Wno-missing-field-initializers  -std=gnu99 -fno-strict-aliasing")
	option(USE_LIBC++ "Use libc++ instead of libstdc++" ${APPLE})
	IF(USE_LIBC++)
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	ENDIF()
	SET(CMAKE_CXX_FLAGS_DEBUG 	"${CMAKE_CXX_FLAGS} -O0 -Wall -g -ggdb ")
	SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -Wall")
ELSEIF( MSVC )
	IF(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		STRING(REGEX REPLACE   "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	ELSE()
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	ENDIF()
	# Disable pointless constant condition warnings
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4127 /wd4201 /Zi")
		SET(CMAKE_EXE_LINKER_FLAGS    "/SAFESEH:NO")
	SET(CMAKE_SHARED_LINKER_FLAGS "/SAFESEH:NO")
	SET(CMAKE_STATIC_LINKER_FLAGS "/SAFESEH:NO")
	SET(CMAKE_MODULE_LINKER_FLAGS "/SAFESEH:NO")
	ADD_DEFINITIONS(-DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS)
ENDIF()

#----------------------------------------------------------------------------------
#
# 资源文件(自动转换)以及源文件的添加
SET(MSP_RC
	ui-forms/msp.qrc
)
SET(MSP_TS
	ui-forms/msp_ch.ts
	ui-forms/msp_en.ts
)
SET(MSP_UI 
	ui-forms/msp-mainwindow.ui
	ui-forms/msp-help.ui
)

# Generate code from ui files
QT5_WRAP_UI(UI_HEADERS 			${MSP_UI})
# Generate rules for building source files from the resources
QT5_ADD_RESOURCES(RC_SOURCES 	${MSP_RC})
# Translate the .ts files to .qm files.
QT5_ADD_TRANSLATION(TS_SOURCES 	${MSP_TS})
SOURCE_GROUP(ui    FILES ${MSP_UI} 	   ${MSP_RC} 	  ${MSP_TS}			 )
SOURCE_GROUP(ui-rc FILES ${UI_HEADERS} ${RC_SOURCES}  ${TS_SOURCES}		 )
SOURCE_GROUP(ui-rc FILES ${CMAKE_BINARY_DIR}/UI/msp_autogen/mocs_compilation.cpp )
LIST(APPEND ALL_SOURCES_AND_HEADERS ${UI_HEADERS} ${RC_SOURCES} ${TS_SOURCES}    )

#
# @@@Add [interface\\base		] module 
SET(MSP_BASE_HEADERS
	interface/msp.hpp 
)
SET(MSP_BASE_SOURCES
	interface/msp.cpp 
)
SOURCE_GROUP(interface 				FILES	${MSP_BASE_SOURCES} ${MSP_BASE_HEADERS})
LIST(APPEND ALL_SOURCES_AND_HEADERS 		${MSP_BASE_SOURCES} ${MSP_BASE_HEADERS})

#
# @@@Add [interface\\mainwindow	] module 
SET(MSP_INTERFACE_HEADERS 
	interface/msp-mainwindow.hpp 
)
SET(MSP_INTERFACE_SOURCES
	interface/msp-mainwindow.cpp 
)
SOURCE_GROUP(interface\\mainwindow	FILES  	${MSP_INTERFACE_SOURCES} ${MSP_INTERFACE_HEADERS})
LIST(APPEND ALL_SOURCES_AND_HEADERS 	  	${MSP_INTERFACE_SOURCES} ${MSP_INTERFACE_HEADERS})
#INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR})

#
# @@@Add [interface\\help		] module 
SET(MSP_HELP_HEADERS 
	interface/msp-help.hpp 
)
SET(MSP_HELP_SOURCES
	interface/msp-help.cpp 
)
SOURCE_GROUP(interface\\help 		FILES	${MSP_HELP_SOURCES} ${MSP_HELP_HEADERS})
LIST(APPEND ALL_SOURCES_AND_HEADERS 		${MSP_HELP_SOURCES} ${MSP_HELP_HEADERS})

#----------------------------------------------------------------------------------
#
# 构建目标文件,生成库或可执行文件
# ADD_LIBRARY(${PROJECT_NAME} [STATIC | SHARED | MODULE])
# ADD_LIBRARY(${PROJECT_NAME} ${ALL_SOURCES_AND_HEADERS})
ADD_EXECUTABLE(${PROJECT_NAME} ${ALL_SOURCES_AND_HEADERS})

# gcc -l libs.
# Add the Qt5 Widgets for linking
TARGET_LINK_LIBRARIES(${PROJECT_NAME} 
	Qt5::Widgets 
	Qt5::WinMain)
TARGET_LINK_LIBRARIES(${PROJECT_NAME}
	${FFmpeg_LIBRARY}
	${SDL2_LIBRARY}
	${LIBVLC_LIBRARY}
	${EXTERN_LIBRARY})

#----------------------------------------------------------------------------------
# 链接外部脚本或执行子目录下的CMakelist.txt
#INCLUDE(copy_deps_bins)
#ADD_SUBDIRECTORY(sub_module)
display_enviroments(${DISPLAY_ENVIROMENT})