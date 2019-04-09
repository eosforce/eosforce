cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
#set(protobuf_MODULE_COMPATIBLE TRUE)
#find_package(protobuf CONFIG REQUIRED)
#message(STATUS "Using protobuf ${protobuf_VERSION}")
#set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
#set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
#find_package(grpc CONFIG REQUIRED)
#message(STATUS "Using gRPC ${gRPC_VERSION}")
#set(_GRPC_GRPCPP_UNSECURE grpc::grpc++_unsecure)
#set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc::grpc_cpp_plugin>)


  add_subdirectory(../../externals/grpc ${CMAKE_CURRENT_BINARY_DIR}/grpc EXCLUDE_FROM_ALL)
  message(STATUS "client Using gRPC via add_subdirectory.")

  set(_PROTOBUF_LIBPROTOBUF libprotobuf)
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
  set(_GRPC_GRPCPP_UNSECURE grpc++_unsecure)
  set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)


get_filename_component(hw_proto "./include/protos/relay_commit.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/relay_commit.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/relay_commit.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/relay_commit.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/relay_commit.grpc.pb.h")
add_custom_command(
      OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${hw_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}"
      DEPENDS "${hw_proto}")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")


file(GLOB HEADERS "include/eosio/grpc_plugin/*.hpp")
include_directories("/usr/local/include/grpcpp")
include_directories("/usr/local/include/google/protobuf")
#LINK_LIBRARIES("/usr/local/lib/libgrpc++.so" "/usr/local/lib/libgrpc++.so.1")
add_library( bus_plugin
             bus_plugin.cpp
             relay_commit.grpc.pb.cc
             relay_commit.pb.cc
             ${HEADERS} )

target_link_libraries( bus_plugin appbase chain_plugin eosio_chain fc ${_GRPC_GRPCPP_UNSECURE} ${_PROTOBUF_LIBPROTOBUF})
target_include_directories( bus_plugin PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )


