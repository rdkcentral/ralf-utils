cmake_minimum_required(VERSION 3.12)
cmake_policy(VERSION 3.12)

project(test_project CXX)

find_package(libralf REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_project testbuild.cpp)

target_link_libraries(test_project ralf-utils::libralf)
