#pragma once

#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <type_traits>

namespace thousandeyes {
namespace futures {

//! \brief Convenience function for obtaining a ready future that contains
//! the given value.
//!
//! \param value The value to make the future ready with.
template<class T>
std::future<typename std::decay<T>::type> fromValue(T&& value)
{
    using Output = typename std::decay<T>::type;

    std::promise<Output> result;
    result.set_value(std::forward<T>(value));
    return result.get_future();
}

//! \brief Convenience function for obtaining a ready future that throws
//! the exception stored in the given #std::exception_ptr.
//!
//! \param exc The exception to make the future ready with.
template<class T>
std::future<typename std::decay<T>::type> fromException(std::exception_ptr exc)
{
    using Output = typename std::decay<T>::type;

    std::promise<Output> result;
    result.set_exception(std::move(exc));
    return result.get_future();
}

} // namespace futures
} // namespace thousandeyes
