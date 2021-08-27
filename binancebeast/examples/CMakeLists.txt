cmake_minimum_required (VERSION 3.15)

project (examples C CXX)


SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}../../bin/)

include_directories("../../vcpkg/installed/x64-linux/include")
include_directories("../bblib/include")
include_directories("../../ordered_thread_pool")

LINK_DIRECTORIES("../../vcpkg/installed/x64-linux/lib")

add_executable (rest "rest.cpp")
add_executable (userdata "userdata.cpp")
add_executable (neworder "neworder.cpp")

set_target_properties(rest PROPERTIES CXX_STANDARD 17)
target_link_libraries(rest binancebeast -lssl -lboost_json -lcrypto -lpthread -ldl)

set_target_properties(userdata PROPERTIES CXX_STANDARD 17)
target_link_libraries(userdata binancebeast -lssl -lboost_json -lcrypto -lpthread -ldl)

set_target_properties(neworder PROPERTIES CXX_STANDARD 17)
target_link_libraries(neworder binancebeast -lssl -lboost_json -lcrypto -lpthread -ldl)