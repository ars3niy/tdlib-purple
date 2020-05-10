cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(TdExample VERSION 1.0 LANGUAGES CXX)

find_package(Td REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

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
pkg_get_variable(PURPLE_PLUGIN_DIR purple plugindir)
pkg_get_variable(PURPLE_DATA_DIR purple datarootdir)

add_library(telegram-tdlib SHARED
    tdlib-purple.cpp
    td-client.cpp
    transceiver.cpp
    account-data.cpp
    chat-info.cpp
    config.cpp
)

target_compile_definitions(telegram-tdlib PRIVATE
    "TD_VERSION=\"${Td_VERSION}\";GIT_COMMIT=\"${GIT_COMMIT}\""
)
include_directories(telegram-tdlib ${Purple_INCLUDE_DIRS})
target_link_libraries(telegram-tdlib PRIVATE Td::TdStatic ${Purple_LIBRARIES})
set_property(TARGET telegram-tdlib PROPERTY CXX_STANDARD 14)

install(TARGETS telegram-tdlib LIBRARY DESTINATION "${PURPLE_PLUGIN_DIR}")
install(FILES icons/telegram16.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/16" RENAME telegram.png)
install(FILES icons/telegram22.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/22" RENAME telegram.png)
install(FILES icons/telegram48.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/48" RENAME telegram.png)

add_subdirectory(test)
