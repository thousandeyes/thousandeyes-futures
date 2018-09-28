cmake_minimum_required(VERSION 3.11)

project(thousandeyes-futures VERSION 0.1 LANGUAGES CXX)

add_library(thousandeyes-futures INTERFACE)
add_library(thousandeyes::futures ALIAS thousandeyes-futures)

target_include_directories(thousandeyes-futures
INTERFACE
    include
)

if(THOUSANDEYES_FUTURES_BUILD_TESTS)
    add_subdirectory(srctest)
endif()
