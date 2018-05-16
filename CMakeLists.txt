#
# Top level compilation directory of cmake.
# cd cmake_path/build
# cmake -DCMAKE_PREFIX_PATH="qt_path" -G"Visual Studio 14 2015[Win64|ARM]" cmake_path
#

cmake_minimum_required(VERSION 3.8.2)

PROJECT(msp-studio)

# 1.全局环境变量设定
OPTION(DISPLAY_ENVIROMENT "Display current project's environments" OFF)
OPTION(CMAKE_LIBRARY_TEST "General bins instead of dlls for test"  OFF)
OPTION(ENABLE_UI "Enables the CCLive user interfaces" 				ON)
OPTION(COPY_DEPENDENCIES "Automaticaly copying all dependencies"	ON)

SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/doc/cmake)
IF(NOT CMAKE_BUILD_TYPE)
	SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Set build type to Debug") 
ENDIF()
IF(${CMAKE_C_COMPILER_ID} MATCHES "Clang" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
	SET(CMAKE_COMPILER_IS_CLANG TRUE)
ENDIF()

# 2.全局功能脚本引用
INCLUDE(default-functions)
get_archBits(_arc_suffix)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/rundir/${_arc_suffix}bit)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/rundir/${_arc_suffix}bit)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/rundir/${_arc_suffix}bit)

# 3.外部依赖模块路径
# gcc -I path -L path.
INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/dep*/win${_arc_suffix}/inc")
LINK_DIRECTORIES(   "${CMAKE_SOURCE_DIR}/dep*/win${_arc_suffix}/bin")

# 4.内部模块递归处理
SET(SUB_DIRIRECTORIS UI sub_module)
FOREACH(dir ${SUB_DIRIRECTORIS})
	add_subdirectory(${dir})
ENDFOREACH()

# 5.拷贝平台及库依赖
copy_dependence()