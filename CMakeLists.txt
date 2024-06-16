include(${CMAKE_SOURCE_DIR}/cmake/pr_common.cmake)

option(VFILESYSTEM_STATIC "Build as static library?" OFF)
option(LINK_COMMON_LIBS_STATIC "Link to common Pragma libraries statically?"
       OFF)

if(${VFILESYSTEM_STATIC})
    set(LIB_TYPE STATIC)
else()
    set(LIB_TYPE SHARED)
endif()

set(PROJ_NAME vfilesystem)
pr_add_library(${PROJ_NAME} ${LIB_TYPE})

pr_add_dependency(${PROJ_NAME} sharedutils TARGET PUBLIC)
pr_add_external_dependency(${PROJ_NAME} liblzma HEADER_ONLY)
pr_add_external_dependency(${PROJ_NAME} bzip2 HEADER_ONLY)

pr_add_headers(${PROJ_NAME} "include/")
if(WIN32)
	# We don't need the .c-files on Windows, so we use a custom filter.
	pr_add_sources(${PROJ_NAME} "src/" FILTER "src/*.cpp")
else()
	pr_add_sources(${PROJ_NAME} "src/")
endif()

pr_add_compile_definitions(${PROJ_NAME} -DVFILESYSTEM_DLL)

if(${LINK_COMMON_LIBS_STATIC})
    pr_add_compile_definitions(${PROJ_NAME} -DMUTIL_STATIC -DSHUTIL_STATIC)
endif()

if(${VFILESYSTEM_STATIC})
    pr_add_compile_definitions(${PROJ_NAME} -DVFILESYSTEM_STATIC)
endif()

if(WIN32)
    target_link_libraries(${PROJ_NAME} PRIVATE shlwapi.lib)
endif()

pr_finalize(${PROJ_NAME})
