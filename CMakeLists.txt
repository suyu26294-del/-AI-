cmake_minimum_required(VERSION 3.18)
project(edge_cloud_ai_scheduler VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" ON)

add_library(common_lib
  common/src/logger.cpp
  common/src/time_utils.cpp
  common/src/message.cpp
  common/src/socket_utils.cpp
)
target_include_directories(common_lib PUBLIC common/include)
target_link_libraries(common_lib PUBLIC pthread)

add_executable(cloud_server
  cloud_server/src/main.cpp
  cloud_server/src/cloud_server.cpp
)
target_include_directories(cloud_server PRIVATE cloud_server/include)
target_link_libraries(cloud_server PRIVATE common_lib)

add_executable(edge_node
  edge_node/src/main.cpp
  edge_node/src/edge_node.cpp
)
target_include_directories(edge_node PRIVATE edge_node/include)
target_link_libraries(edge_node PRIVATE common_lib)

add_executable(device_client
  device_client/src/main.cpp
  device_client/src/device_client.cpp
)
target_include_directories(device_client PRIVATE device_client/include)
target_link_libraries(device_client PRIVATE common_lib)

add_executable(multi_device_simulator
  simulator/src/main.cpp
)
target_link_libraries(multi_device_simulator PRIVATE common_lib)

if(BUILD_TESTS)
  enable_testing()
  add_test(
    NAME integration_smoke
    COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${CMAKE_SOURCE_DIR} python3 ${CMAKE_SOURCE_DIR}/tests/integration_simulation_test.py
  )
endif()
