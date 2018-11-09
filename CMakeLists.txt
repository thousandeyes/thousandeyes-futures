cmake_minimum_required(VERSION 3.11)

project(thousandeyes-futures VERSION 0.1 LANGUAGES CXX)

add_library(thousandeyes-futures INTERFACE)
add_library(thousandeyes::futures ALIAS thousandeyes-futures)

target_include_directories(thousandeyes-futures
INTERFACE
    ${PROJECT_SOURCE_DIR}/include
)

target_sources(thousandeyes-futures INTERFACE
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/Default.h
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
    ${PROJECT_SOURCE_DIR}/include/thousandeyes/futures/detail/typetraits.h
)

if(THOUSANDEYES_FUTURES_BUILD_TESTS)
    add_subdirectory(srctest)
endif()
