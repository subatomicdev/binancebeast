cmake_minimum_required (VERSION 3.15)

project (tests C CXX)


SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}../../bin/)

include_directories("../../vcpkg/installed/x64-linux/include")
include_directories("../bblib/include")
include_directories("../../ordered_thread_pool")

LINK_DIRECTORIES("../../vcpkg/installed/x64-linux/lib")

add_executable (firstbuildtest "firstbuildtest.cpp")
add_executable (testrest "testrest.cpp")


set_target_properties(firstbuildtest PROPERTIES CXX_STANDARD 17)
target_link_libraries(firstbuildtest binancebeast -lssl -lboost_json -lcrypto -lpthread -ldl)

set_target_properties(testrest PROPERTIES CXX_STANDARD 17)
target_link_libraries(testrest binancebeast -lssl -lboost_json -lcrypto -lpthread -ldl)