cmake_minimum_required(VERSION 3.16)
project(lightweight_edge_cloud_ai LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(common STATIC
  common/src/logger.cpp
  common/src/protocol.cpp
  common/src/socket_utils.cpp
  common/src/thread_pool.cpp
)
target_include_directories(common PUBLIC common/include)
target_link_libraries(common PUBLIC Threads::Threads)

add_library(device_core STATIC device_client/src/device_client.cpp)
target_include_directories(device_core PUBLIC device_client/include)
target_link_libraries(device_core PUBLIC common)

add_executable(cloud_server
  cloud_server/src/main.cpp
  cloud_server/src/cloud_server.cpp
)
target_include_directories(cloud_server PRIVATE cloud_server/include)
target_link_libraries(cloud_server PRIVATE common)

add_executable(edge_node
  edge_node/src/main.cpp
  edge_node/src/edge_node.cpp
)
target_include_directories(edge_node PRIVATE edge_node/include)
target_link_libraries(edge_node PRIVATE common)

add_executable(device_client device_client/src/main.cpp)
target_link_libraries(device_client PRIVATE device_core)

add_executable(device_simulator simulator/src/main.cpp)
target_link_libraries(device_simulator PRIVATE device_core)

enable_testing()
add_test(NAME protocol_unit COMMAND device_client --self-test)
