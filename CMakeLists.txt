cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(TdExample VERSION 1.0 LANGUAGES CXX)

find_package(Td 1.6.0 REQUIRED)

add_executable(td_example td_example.cpp)
target_link_libraries(td_example PRIVATE Td::TdStatic)
set_property(TARGET td_example PROPERTY CXX_STANDARD 14)
