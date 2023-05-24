find_package(Threads)

function(add_example _file)
    if(NOT _file)
        message(FATAL_ERROR "You must provide a '_file''")
    endif(NOT _file)

    if(NOT TARGET examples)
        add_custom_target(examples)
    endif()

    get_filename_component(test_name ${_file} NAME_WE)
    set(_target example-${test_name})

    add_executable(${_target} ${_file})

    target_link_libraries(${_target}
                          PRIVATE ${CMAKE_THREAD_LIBS_INIT}
                          PRIVATE thousandeyes::futures)

    add_dependencies(examples ${_target})
endfunction(add_example)

add_example(chaining.cpp)
add_example(conversion.cpp)
add_example(executors.cpp)
add_example(recursive.cpp)
add_example(sum.cpp)
add_example(timeout.cpp)
