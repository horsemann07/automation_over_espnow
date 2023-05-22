
# The following lines of boilerplate have to be in your project's CMakeLists
# # in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
add_compile_options(-fdiagnostics-color=always)

# This example uses an extra component for common functions such as Wi-Fi and Ethernet connection.
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

set(CMAKE_C_CLANG_TIDY_EXECUTABLE /usr/bin/clang-tidy)

#set(CMAKE_C_CLANG_TIDY "clang-tidy;-header-filter=${CMAKE_SOURCE_DIR}/components/wifi_manager*")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(get-started)
