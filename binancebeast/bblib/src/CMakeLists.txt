cmake_minimum_required (VERSION 3.15)

project (bblib C CXX)

SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}../../../lib/)


include_directories("../../../vcpkg/installed/x64-linux/include")
include_directories("../include")
include_directories("../../../ordered_thread_pool")


add_library(binancebeast STATIC "BinanceBeast.cpp")


SET_TARGET_PROPERTIES(binancebeast PROPERTIES LINKER_LANGUAGE CXX)
SET_TARGET_PROPERTIES(binancebeast PROPERTIES CXX_STANDARD 17)

# target_link_libraries(binancebeast -lcpprest -lssl -lcrypto -lz -ldl)