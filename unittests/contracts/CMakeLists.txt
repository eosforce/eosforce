# will be implictly used for any compilation unit if not overrided by SYSTEM_INCLUDE_FOLDERS parameter
# these directories go as -isystem <dir> to avoid warnings from code of third-party libraries

set(STANDARD_INCLUDE_FOLDERS ${CMAKE_SOURCE_DIR}/contracts ${CMAKE_BINARY_DIR}/contracts ${CMAKE_SOURCE_DIR}/unittests/contracts ${CMAKE_SOURCE_DIR}/externals/magic_get/include)

add_subdirectory(deferred_test)
