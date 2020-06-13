cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(TdExample VERSION 1.0 LANGUAGES CXX)

find_package(Td REQUIRED)

set(NoPkgConfig FALSE CACHE BOOL "Do not use pkg-config")
set(NoWebp FALSE CACHE BOOL "Do not decode webp stickers")
set(NoBundledLottie FALSE CACHE BOOL "Do not use bundled rlottie library")
set(API_ID 94575 CACHE STRING "API id")
set(API_HASH a3406de8d171bb422bb6ddf3bbd800e2 CACHE STRING "API hash")
set(STUFF "" CACHE STRING "")

if (NOT NoPkgConfig)
    include(FindPkgConfig)
    pkg_check_modules(Purple REQUIRED purple)
    if (NOT NoWebp)
        pkg_check_modules(libwebp libwebp)
        pkg_check_modules(libpng libpng)
    endif (NOT NoWebp)
    pkg_get_variable(PURPLE_PLUGIN_DIR purple plugindir)
    pkg_get_variable(PURPLE_DATA_DIR purple datarootdir)
endif (NOT NoPkgConfig)

if (NOT NoWebp)
    if ("${libwebp_LIBRARIES}" STREQUAL "")
        message(FATAL_ERROR "Webp library not found, build with -DNoWebp=TRUE to disable webp sticker decoding")
    endif ("${libwebp_LIBRARIES}" STREQUAL "")
    if ("${libpng_LIBRARIES}" STREQUAL "")
        message(FATAL_ERROR "libpng not found, build with -DNoWebp=TRUE to disable webp sticker decoding")
    endif ("${libpng_LIBRARIES}" STREQUAL "")
endif (NOT NoWebp)

configure_file(buildopt.h.in buildopt.h)
configure_file(config.cpp.in config.cpp)

add_library(telegram-tdlib SHARED
    tdlib-purple.cpp
    td-client.cpp
    transceiver.cpp
    account-data.cpp
    purple-info.cpp
    ${CMAKE_BINARY_DIR}/config.cpp
    client-utils.cpp
    format.cpp
)

include_directories(${Purple_INCLUDE_DIRS} ${CMAKE_BINARY_DIR})
target_link_libraries(telegram-tdlib PRIVATE Td::TdStatic ${Purple_LIBRARIES})

if (NOT NoWebp)
    include_directories(${libwebp_INCLUDE_DIRS} ${libpng_INCLUDE_DIRS})
    target_link_libraries(telegram-tdlib PRIVATE ${libwebp_LIBRARIES} ${libpng_LIBRARIES})
endif (NOT NoWebp)

set_property(TARGET telegram-tdlib PROPERTY CXX_STANDARD 14)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(fmt)
target_compile_options(fmt PRIVATE -fPIC)
target_link_libraries(telegram-tdlib PRIVATE fmt::fmt)

if (NOT NoBundledLottie)
    set(LOTTIE_MODULE OFF)
    add_subdirectory(rlottie)
endif (NOT NoBundledLottie)
target_link_libraries(telegram-tdlib PRIVATE rlottie)

if (NOT DEFINED SHARE_INSTALL_PREFIX)
    set(SHARE_INSTALL_PREFIX "share")
endif (NOT DEFINED SHARE_INSTALL_PREFIX)

install(TARGETS telegram-tdlib DESTINATION "${PURPLE_PLUGIN_DIR}")
install(FILES data/telegram16.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/16" RENAME telegram.png)
install(FILES data/telegram22.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/22" RENAME telegram.png)
install(FILES data/telegram48.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/48" RENAME telegram.png)
install(FILES data/tdlib-purple.metainfo.xml DESTINATION "${SHARE_INSTALL_PREFIX}/metainfo")

add_subdirectory(test)
