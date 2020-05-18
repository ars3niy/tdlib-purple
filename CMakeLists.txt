cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(TdExample VERSION 1.0 LANGUAGES CXX)

find_package(Td REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")

if (${UNIX})
    execute_process(COMMAND git log --pretty=format:%h -n 1 OUTPUT_VARIABLE GIT_COMMIT)
    if (NOT "${GIT_COMMIT}" STREQUAL "")
        set(GIT_COMMIT "-${GIT_COMMIT}")
    endif (NOT "${GIT_COMMIT}" STREQUAL "")
else (${UNIX})
    set(GIT_COMMIT "")
endif (${UNIX})

if (NOT DEFINED NoPkgConfig OR NOT NoPkgConfig)
    include(FindPkgConfig)
    pkg_check_modules(Purple REQUIRED purple)
    pkg_get_variable(PURPLE_PLUGIN_DIR purple plugindir)
    pkg_get_variable(PURPLE_DATA_DIR purple datarootdir)
endif (NOT DEFINED NoPkgConfig OR NOT NoPkgConfig)

add_library(telegram-tdlib SHARED
    tdlib-purple.cpp
    td-client.cpp
    transceiver.cpp
    account-data.cpp
    chat-info.cpp
    config.cpp
    client-utils.cpp
    format.cpp
)

target_compile_definitions(telegram-tdlib PRIVATE
    "TD_VERSION=\"${Td_VERSION}\";GIT_COMMIT=\"${GIT_COMMIT}\""
)
include_directories(telegram-tdlib ${Purple_INCLUDE_DIRS})
target_link_libraries(telegram-tdlib PRIVATE Td::TdStatic ${Purple_LIBRARIES})
set_property(TARGET telegram-tdlib PROPERTY CXX_STANDARD 14)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(fmt)
target_compile_options(fmt PRIVATE -fPIC)
target_link_libraries(telegram-tdlib PRIVATE fmt::fmt)

if (NOT DEFINED SHARE_INSTALL_PREFIX)
    set(SHARE_INSTALL_PREFIX "share")
endif (NOT DEFINED SHARE_INSTALL_PREFIX)

install(TARGETS telegram-tdlib DESTINATION "${PURPLE_PLUGIN_DIR}")
install(FILES data/telegram16.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/16" RENAME telegram.png)
install(FILES data/telegram22.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/22" RENAME telegram.png)
install(FILES data/telegram48.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/48" RENAME telegram.png)
install(FILES data/tdlib-purple.metainfo.xml DESTINATION "${SHARE_INSTALL_PREFIX}/metainfo")

add_subdirectory(test)
