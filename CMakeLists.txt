cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(tdlib-purple VERSION 1.0 LANGUAGES CXX)

find_package(Td REQUIRED)
math(EXPR TDLIB_VERSION_NUMBER "10000*${Td_VERSION_MAJOR} + 100*${Td_VERSION_MINOR} + ${Td_VERSION_PATCH}")
if (${TDLIB_VERSION_NUMBER} GREATER 10605)
    message(FATAL_ERROR "tdlib version 1.6.5 or older is required")
endif (${TDLIB_VERSION_NUMBER} GREATER 10605)

set(NoPkgConfig FALSE CACHE BOOL "Do not use pkg-config")
set(NoWebp FALSE CACHE BOOL "Do not decode webp stickers")
set(NoBundledLottie FALSE CACHE BOOL "Do not use bundled rlottie library")
set(NoLottie FALSE CACHE BOOL "Disable animated sticker conversion")
set(NoTranslations FALSE CACHE BOOL "Disable translation support")
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
    sticker.cpp
)

# libpurple uses the deprecated glib-type `GParameter` and the deprecated glib-macro `G_CONST_RETURN`, which
# spams the console with useless warnings that we can do nothing about.
target_compile_definitions(telegram-tdlib PRIVATE GLIB_DISABLE_DEPRECATION_WARNINGS)
#target_compile_options(telegram-tdlib PRIVATE -Wmissing-declarations)

include_directories(${Purple_INCLUDE_DIRS} ${CMAKE_BINARY_DIR})
target_link_libraries(telegram-tdlib PRIVATE Td::TdStatic ${Purple_LIBRARIES})
    set_target_properties(telegram-tdlib PROPERTIES
        LINK_FLAGS "-Wl,--version-script=${CMAKE_SOURCE_DIR}/ld_version_script"
    )

if (NOT NoWebp)
    include_directories(${libwebp_INCLUDE_DIRS} ${libpng_INCLUDE_DIRS})
    target_link_libraries(telegram-tdlib PRIVATE ${libwebp_LIBRARIES} ${libpng_LIBRARIES})
endif (NOT NoWebp)

set_property(TARGET telegram-tdlib PROPERTY CXX_STANDARD 14)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(fmt)
target_compile_options(fmt PRIVATE -fPIC)
target_link_libraries(telegram-tdlib PRIVATE fmt::fmt)

if (NOT NoLottie)
    if (NOT NoBundledLottie)
        add_subdirectory(rlottie)
        target_compile_options(rlottie PRIVATE -fPIC)
        target_include_directories(telegram-tdlib PRIVATE rlottie/inc)
    endif (NOT NoBundledLottie)
    target_link_libraries(telegram-tdlib PRIVATE rlottie)
    # When building with cygwin, lack of LOT_BUILD would add dllimport attribute to function
    # prototypes from rlottie, causing them to have different name mangling from what they had when
    # building rlottie.
    target_compile_definitions(telegram-tdlib PRIVATE LOT_BUILD)
endif (NOT NoLottie)

if (NOT DEFINED SHARE_INSTALL_PREFIX)
    set(SHARE_INSTALL_PREFIX "share")
endif (NOT DEFINED SHARE_INSTALL_PREFIX)

if (NOT NoTranslations)
    find_package(Gettext REQUIRED)
    file(STRINGS po/LINGUAS _linguas)
    foreach(_lang ${_linguas})
        GETTEXT_PROCESS_PO_FILES(
            ${_lang} ALL
            PO_FILES "${CMAKE_SOURCE_DIR}/po/${_lang}.po"
        )
        install(
            FILES ${CMAKE_CURRENT_BINARY_DIR}/${_lang}.gmo
            DESTINATION "${SHARE_INSTALL_PREFIX}/locale/${_lang}/LC_MESSAGES"
            RENAME tdlib-purple.mo
        )
    endforeach()

    find_package(Intl REQUIRED)
    target_include_directories(telegram-tdlib PRIVATE ${Intl_INCLUDE_DIRS})
    target_link_libraries(telegram-tdlib PRIVATE ${Intl_LIBRARIES})
endif (NOT NoTranslations)

if (DEFINED STANDARD_LIBRARIES_EXTRA)
    set (CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} ${STANDARD_LIBRARIES_EXTRA}")
endif (DEFINED STANDARD_LIBRARIES_EXTRA)

install(TARGETS telegram-tdlib DESTINATION "${PURPLE_PLUGIN_DIR}")
install(FILES data/telegram16.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/16" RENAME telegram.png)
install(FILES data/telegram22.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/22" RENAME telegram.png)
install(FILES data/telegram48.png DESTINATION "${PURPLE_DATA_DIR}/pixmaps/pidgin/protocols/48" RENAME telegram.png)
install(FILES data/tdlib-purple.metainfo.xml DESTINATION "${SHARE_INSTALL_PREFIX}/metainfo")

add_subdirectory(test)
