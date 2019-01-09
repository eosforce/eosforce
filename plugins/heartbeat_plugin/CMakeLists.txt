file(GLOB HEADERS "include/eosio/heartbeat_plugin/*.hpp")

add_library( heartbeat_plugin
             heartbeat_plugin.cpp
             ${HEADERS}
           )

target_link_libraries( heartbeat_plugin chain_plugin http_client_plugin appbase eosio_chain )
target_include_directories( heartbeat_plugin
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../chain_interface/include" )
