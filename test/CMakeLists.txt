if (DEFINED GTEST_PATH)
    link_directories(${GTEST_PATH}/lib)
endif (DEFINED GTEST_PATH)

add_executable(tests EXCLUDE_FROM_ALL
    test-main.cpp
    testsuite.cpp
)

set_property(TARGET tests PROPERTY CXX_STANDARD 14)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tests PRIVATE Td::TdStatic gtest)

if (DEFINED GTEST_PATH)
    target_include_directories(tests PRIVATE ${GTEST_PATH}/include)
    link_directories(${GTEST_PATH}/lib)
endif (DEFINED GTEST_PATH)

add_custom_target(run-tests ${CMAKE_CURRENT_BINARY_DIR}/tests DEPENDS tests)
