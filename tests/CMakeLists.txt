cmake_minimum_required(VERSION 3.11)

# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(CMAKE_CXX_STANDARD 14)

# Add googletest directly to our build, which defines the gtest/gmock and gtest_main targets
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

function(add_testcase _file)
    get_filename_component(test_name ${_file} NAME_WE)
    set(_target futures-test-${test_name})

    add_executable(${_target} ${_file})

    target_link_libraries(${_target}
                          thousandeyes::futures
                          gtest
                          gmock
                          gtest_main)

    add_test(NAME ${_target} COMMAND $<TARGET_FILE:${_target}>)
endfunction(add_testcase)

add_testcase(defaultexecutor.cpp)
add_testcase(pollingexecutor.cpp)
add_testcase(waitable.cpp)
add_testcase(timedwaitable.cpp)
