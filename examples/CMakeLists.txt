cmake_minimum_required(VERSION 3.11)

function(add_example _file)
    get_filename_component(test_name ${_file} NAME_WE)
    set(_target example-${test_name})

    add_executable(${_target} ${_file})
    target_link_libraries(${_target} PRIVATE thousandeyes::futures)
    set_target_properties(${_target} PROPERTIES CXX_STANDARD 14)
endfunction(add_example)

add_example(conversion.cpp)
add_example(chaining.cpp)
add_example(sum.cpp)
