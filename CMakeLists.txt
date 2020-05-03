cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(TdExample VERSION 1.0 LANGUAGES CXX)

find_package(Td REQUIRED)
set(CMAKE_CXX_FLAGS "-Wall -Werror")

if (${UNIX})
    execute_process(COMMAND git log --pretty=format:%h -n 1 OUTPUT_VARIABLE GIT_COMMIT)
    if (NOT "${GIT_COMMIT}" STREQUAL "")
        set(GIT_COMMIT "-${GIT_COMMIT}")
    endif (NOT "${GIT_COMMIT}" STREQUAL "")
else (${UNIX})
    set(GIT_COMMIT "")
endif (${UNIX})

include(FindPkgConfig)
pkg_check_modules(Purple REQUIRED purple)

add_library(td-purple SHARED
    tdlib-purple.cpp
    td-client.cpp
    transceiver.cpp
    account-data.cpp
    config.cpp
)

target_compile_definitions(td-purple PRIVATE
    "TD_VERSION=\"${Td_VERSION}\";GIT_COMMIT=\"${GIT_COMMIT}\""
)
include_directories(td-purple ${Purple_INCLUDE_DIRS})
target_link_libraries(td-purple PRIVATE Td::TdStatic ${Purple_LIBRARIES})
set_property(TARGET td-purple PROPERTY CXX_STANDARD 14)
