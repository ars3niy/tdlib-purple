cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(TdExample VERSION 1.0 LANGUAGES CXX)

find_package(Td 1.6.0 REQUIRED)
set(CMAKE_CXX_FLAGS "-DTD_VERSION=\\\"${Td_VERSION}\\\" -Wall -Werror")

include(FindPkgConfig)
pkg_check_modules(Purple REQUIRED purple)

add_library(td-purple SHARED
    tdlib-purple.cpp
    td-client.cpp
    config.cpp
)

include_directories(td-purple ${Purple_INCLUDE_DIRS})
target_link_libraries(td-purple PRIVATE Td::TdStatic ${Purple_LIBRARIES})
set_property(TARGET td-purple PROPERTY CXX_STANDARD 14)
