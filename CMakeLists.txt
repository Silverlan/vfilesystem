include(${CMAKE_SOURCE_DIR}/cmake/pr_common.cmake)

option(VFILESYSTEM_STATIC "Build as static library?" OFF)
option(VFILESYSTEM_STORE_FILE_INDEX_CACHE_PATHS "Store paths in file index cache items?" OFF)
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
pr_add_dependency(${PROJ_NAME} bz2 TARGET)
pr_add_dependency(${PROJ_NAME} lzma TARGET)
pr_add_dependency(${PROJ_NAME} efsw TARGET)

pr_init_module(${PROJ_NAME})

pr_add_compile_definitions(${PROJ_NAME} -DVFILESYSTEM_DLL)

if(${VFILESYSTEM_STORE_FILE_INDEX_CACHE_PATHS})
    pr_add_compile_definitions(${PROJ_NAME} -DVFILESYSTEM_STORE_FILE_INDEX_CACHE_PATHS)
endif()

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
