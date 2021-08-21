cmake_minimum_required (VERSION 3.15)

project (bbtest C CXX)

include_directories("../../vcpkg/installed/x64-linux/include")
include_directories("../bblib/include")

LINK_DIRECTORIES("../../vcpkg/installed/x64-linux/lib")

add_executable (bbtest "bbtest.cpp")


set_target_properties(bbtest PROPERTIES CXX_STANDARD 17)

target_link_libraries(bbtest binancebeast -lssl -lboost_json -lcrypto -lpthread -ldl)