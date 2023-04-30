cmake_minimum_required(VERSION 3.11)

project(thousandeyes-futures VERSION 0.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_GENERATOR MATCHES "Make|Ninja")
    # These generators support generating a compilation database in the build
    # tree named "compile_commands.json" which can be used by LLVM-based
    # tooling.
    set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
endif()

add_library(thousandeyes-futures INTERFACE)
add_library(thousandeyes::futures ALIAS thousandeyes-futures)

target_include_directories(thousandeyes-futures
INTERFACE
    ${PROJECT_SOURCE_DIR}/include
)

target_sources(thousandeyes-futures INTERFACE
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/Default.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/DefaultExecutor.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/Executor.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/PollingExecutor.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/TimedWaitable.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/Waitable.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/all.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/then.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/util.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/detail/FutureWithChaining.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/detail/FutureWithContainer.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/detail/FutureWithContinuation.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/detail/FutureWithForwarding.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/detail/FutureWithIterators.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/detail/FutureWithTuple.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/detail/InvokerWithNewThread.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/detail/InvokerWithSingleThread.h
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/detail/typetraits.h
)

if(THOUSANDEYES_FUTURES_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(THOUSANDEYES_FUTURES_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
