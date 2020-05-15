if (DEFINED GTEST_PATH)
    link_directories(${GTEST_PATH}/lib)
endif (DEFINED GTEST_PATH)

# Compiling plugin sources again is not optimal but it's the easy way
add_executable(tests EXCLUDE_FROM_ALL
    test-main.cpp
    private-chat-test.cpp
    group-chat-test.cpp
    login-test.cpp
    test-transceiver.cpp
    libpurple-mock.cpp
    printout.cpp
    purple-events.cpp
    fixture.cpp

    ../tdlib-purple.cpp
    ../td-client.cpp
    ../transceiver.cpp
    ../account-data.cpp
    ../chat-info.cpp
    ../config.cpp
    ../client-utils.cpp
    ../format.cpp
)

target_compile_definitions(tests PRIVATE
    "TD_VERSION=\"${Td_VERSION}\";GIT_COMMIT=\"${GIT_COMMIT}\""
)
set_property(TARGET tests PROPERTY CXX_STANDARD 14)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tests PRIVATE gtest fmt::fmt Td::TdStatic glib-2.0)

if (DEFINED GTEST_PATH)
    target_include_directories(tests PRIVATE ${GTEST_PATH}/include)
    link_directories(${GTEST_PATH}/lib)
endif (DEFINED GTEST_PATH)

add_custom_target(run-tests ${CMAKE_CURRENT_BINARY_DIR}/tests DEPENDS tests)
