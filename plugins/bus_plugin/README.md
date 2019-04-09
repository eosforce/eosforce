# bus-plugin
## Description   
grpc_client_plugin provide a function to send grpc request  
those plugin just provide function to get a grpc request and send a grpc request,the Specific application scenarios require secondary development.  
**Currently the plugin only work with [official eosforce repository](https://github.com/eosforce/eosforce).**
## Installation
### Install `grpc`
grpc_plugins rely on [grpc](https://github.com/grpc/grpc)
```bash
git clone https://github.com/grpc/grpc
cd grpc
git submodule update --init --recursive
```
Grpc directory in the same directory of eosforce
### Embed `grpc_server_plugin` `grpc_client_plugin` into `nodeos`
1. Get `grpc_server_plugin`  `grpc_client_plugin`source code.
```bash
git clone https://gitlab.com/eosforce/bus-plugin.git
cd bus-plugin
git submodule update --init --recursive
```
```cmake
...
add_subdirectory(mongo_db_plugin)
add_subdirectory(login_plugin)
add_subdirectory(login_plugin)
add_subdirectory(grpc_client_plugin) # add this line.
...
```
3. Add following line to `programs/nodeos/CMakeLists.txt`.

```cmake
target_link_libraries( ${NODE_EXECUTABLE_NAME}
        PRIVATE appbase
        PRIVATE -Wl,${whole_archive_flag} login_plugin               -Wl,${no_whole_archive_flag}
        PRIVATE -Wl,${whole_archive_flag} history_plugin             -Wl,${no_whole_archive_flag}
        ...
        # add this line.
        PRIVATE -Wl,${whole_archive_flag} grpc_client_plugin       -Wl,${no_whole_archive_flag}
        ...
```
## Usage
The usage of  `grpc_client_plugin` is simple  
--grpc-client-address       grpc-client-address string.grcp server bind ip and port.

