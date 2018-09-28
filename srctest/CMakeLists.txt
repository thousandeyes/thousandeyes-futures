
cmake_minimum_required(VERSION 3.11)

find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

add_library(thousandeyes-testing INTERFACE)
add_library(thousandeyes::testing ALIAS thousandeyes-testing)

target_include_directories(thousandeyes-testing
INTERFACE
    ${GMOCK_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(thousandeyes-testing
INTERFACE
    ${GTEST_BOTH_LIBRARIES}
    ${GMOCK_BOTH_LIBRARIES}
)

function(add_testcase _file)
    get_filename_component(test_name ${_file} NAME_WE)
    set(_target futures-test-${test_name})

    add_executable(${_target} ${_file})

    target_link_libraries(${_target}
                          thousandeyes::futures
                          thousandeyes::testing)

    add_test(NAME ${_target} COMMAND $<TARGET_FILE:${_target}>)
endfunction(add_testcase)

add_testcase(future.cpp)
add_testcase(waitables.cpp)
