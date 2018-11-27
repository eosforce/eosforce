#list( APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/libraries/fc/GitVersionGen" )
#include( GetGitRevisionDescription )
#get_git_head_revision(GIT_REFSPEC EOS_GIT_REVISION_SHA)
#get_git_unix_timestamp(EOS_GIT_REVISION_UNIX_TIMESTAMP)
#git_describe(EOS_GIT_REVISION_DESCRIPTION --tags)
if(NOT EOS_GIT_REVISION_DESCRIPTION)
    set(EOS_GIT_REVISION_DESCRIPTION "unknown")
endif(NOT EOS_GIT_REVISION_DESCRIPTION)

file(GLOB HEADERS
        "include/eosio/utilities/*.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/rand_cfg.hpp")

set(sources
   key_conversion.cpp
   string_escape.cpp
   tempdir.cpp
   words.cpp
   rand.cpp
   ${HEADERS})

#configure_file("${CMAKE_CURRENT_SOURCE_DIR}/git_revision.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/git_revision.cpp" @ONLY)
#list(APPEND sources "${CMAKE_CURRENT_BINARY_DIR}/git_revision.cpp")


include(CheckSymbolExists)
include(CheckCXXSourceCompiles)

# Check for ways to obtain entropy
check_symbol_exists(getentropy "unistd.h" HAVE_GETENTROPY)
check_symbol_exists(getentropy "sys/random.h" HAVE_GETENTROPY_RAND)

check_cxx_source_compiles("
	#include <unistd.h>  /* for syscall */
	#include <sys/syscall.h>  /* for SYS_getrandom */
	int main() {
		syscall(SYS_getrandom, nullptr, 0, 0);
		return 0;
	}
" HAVE_SYS_GETRANDOM)

check_cxx_source_compiles("
	#include <sys/types.h>
	#include <sys/sysctl.h>
	int main() {
		static const int name[2] = {CTL_KERN, KERN_ARND};
		sysctl(name, 2, nullptr, nullptr, nullptr, 0);
		return 0;
	}
" HAVE_SYSCTL_ARND)

add_library( eos_utilities
             ${sources}
             ${HEADERS} )
target_link_libraries( eos_utilities fc WAST WASM )
target_include_directories( eos_utilities
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
                                   "${CMAKE_CURRENT_SOURCE_DIR}/../wasm-jit/Include" )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/rand_cfg.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/rand_cfg.hpp)

if (USE_PCH)
  set_target_properties(eos_utilities PROPERTIES COTIRE_ADD_UNITY_BUILD FALSE)
  cotire(eos_utilities)
endif(USE_PCH)
