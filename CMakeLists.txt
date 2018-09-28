cmake_minimum_required(VERSION 3.11)

project(thousandeyes-futures VERSION 0.1 LANGUAGES CXX)

add_library(thousandeyes-futures INTERFACE)
add_library(thousandeyes::futures ALIAS thousandeyes-futures)

target_sources(thousandeyes-futures INTERFACE
    include/thousandeyes/futures/Default.h
    include/thousandeyes/futures/Executor.h
    include/thousandeyes/futures/PollingExecutor.h
    include/thousandeyes/futures/TimedWaitable.h
    include/thousandeyes/futures/Waitable.h
    include/thousandeyes/futures/all.h
    include/thousandeyes/futures/then.h
    include/thousandeyes/futures/util.h
    include/thousandeyes/detail/FutureWithChaining.h
    include/thousandeyes/detail/FutureWithContainer.h
    include/thousandeyes/detail/FutureWithContinuation.h
    include/thousandeyes/detail/FutureWithForwarding.h
    include/thousandeyes/detail/FutureWithIterators.h
    include/thousandeyes/detail/FutureWithTuple.h
    include/thousandeyes/detail/typetraits.h
)

target_include_directories(thousandeyes-futures
INTERFACE
    include
)

if(THOUSANDEYES_FUTURES_BUILD_TESTS)
    add_subdirectory(srctest)
endif()
