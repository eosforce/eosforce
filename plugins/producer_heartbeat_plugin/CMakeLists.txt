file(GLOB HEADERS "include/eosio/producer_heartbeat_plugin/*.hpp")
add_library( producer_heartbeat_plugin
             producer_heartbeat_plugin.cpp
             ${HEADERS} )

target_link_libraries( producer_heartbeat_plugin chain_plugin producer_plugin appbase fc )
target_include_directories( producer_heartbeat_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
