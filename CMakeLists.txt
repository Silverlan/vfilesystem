cmake_minimum_required(VERSION 3.12)

set(PROJ_NAME vfilesystem)
project(${PROJ_NAME} C CXX)

set(CMAKE_CXX_STANDARD 20)

set(DEPENDENCY_GLM_INCLUDE "" CACHE PATH "Path to GLM include directory.")
set(DEPENDENCY_SHAREDUTILS_INCLUDE "" CACHE PATH "Path to sharedutils include directory.")
set(DEPENDENCY_SHAREDUTILS_LIBRARY "" CACHE FILEPATH "Path to sharedutils library.")
set(DEPENDENCY_MATHUTIL_INCLUDE "" CACHE PATH "Path to mathutil include directory.")
set(DEPENDENCY_MATHUTIL_LIBRARY "" CACHE FILEPATH "Path to mathutil library. Required if this is compiled as a shared library.")
set(DEPENDENCY_BZIP2_INCLUDE "" CACHE PATH "Path to bzip2 include directory.")
option(VFILESYSTEM_STATIC "Build as static library?" OFF)
option(LINK_COMMON_LIBS_STATIC "Link to common Pragma libraries statically?" OFF)

if(${VFILESYSTEM_STATIC})
	set(LIB_TYPE STATIC)
	add_definitions(-DVFILESYSTEM_STATIC)
else()
	set(LIB_TYPE SHARED)
endif()

if(${LINK_COMMON_LIBS_STATIC})
	add_definitions(-DMUTIL_STATIC)
	add_definitions(-DSHUTIL_STATIC)
endif()

add_definitions(-DVFILESYSTEM_DLL)
add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)

function(def_vs_filters FILE_LIST)
	foreach(source IN LISTS FILE_LIST)
	    get_filename_component(source_path "${source}" PATH)
	    string(REPLACE "${CMAKE_CURRENT_LIST_DIR}" "" source_path_relative "${source_path}")
	    string(REPLACE "/" "\\" source_path_msvc "${source_path_relative}")
	    source_group("${source_path_msvc}" FILES "${source}")
	endforeach()
endfunction(def_vs_filters)

file(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)
add_library(${PROJ_NAME} ${LIB_TYPE} ${SRC_FILES})
if(WIN32)
	target_compile_options(${PROJ_NAME} PRIVATE /wd4251)
	target_compile_options(${PROJ_NAME} PRIVATE /wd4996)
endif()
def_vs_filters("${SRC_FILES}")

target_link_libraries(${PROJ_NAME} ${DEPENDENCY_SHAREDUTILS_LIBRARY})
target_link_libraries(${PROJ_NAME} ${DEPENDENCY_MATHUTIL_LIBRARY})

if(WIN32)
	target_link_libraries(${PROJ_NAME} shlwapi.lib)
endif()

set_target_properties(${PROJ_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(${PROJ_NAME} PRIVATE ${DEPENDENCY_GLM_INCLUDE})
target_include_directories(${PROJ_NAME} PRIVATE ${DEPENDENCY_SHAREDUTILS_INCLUDE})
target_include_directories(${PROJ_NAME} PRIVATE ${DEPENDENCY_MATHUTIL_INCLUDE})
target_include_directories(${PROJ_NAME} PRIVATE ${DEPENDENCY_BZIP2_INCLUDE})
