#pragma once

#include <future>
#include <memory>
#include <type_traits>
#include <tuple>

#include <thousandeyes/futures/detail/FutureWithContainer.h>
#include <thousandeyes/futures/detail/FutureWithTuple.h>
#include <thousandeyes/futures/detail/FutureWithIterators.h>
#include <thousandeyes/futures/detail/typetraits.h>

#include <thousandeyes/futures/Default.h>
#include <thousandeyes/futures/Executor.h>

namespace thousandeyes {
namespace futures {

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in the given
//! container become ready.
//!
//! \param executor The object that waits for the given futures to become ready.
//! \param timeLimit The maximum time to wait for all the given futures to become ready.
//! \param futures The container that contains all the input futures.
//!
//! \note If the total time for waiting the input futures to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \sa WaitableTimedOutException
//!
//! \return An std::future<TContainer> that contains all the input futures, where
//! all the contained futures are ready.
template<class TContainer>
std::future<typename std::decay<TContainer>::type> all(std::shared_ptr<Executor> executor,
                                                       std::chrono::microseconds timeLimit,
                                                       TContainer&& futures)
{
    std::promise<typename std::decay<TContainer>::type> p;

    auto result = p.get_future();

    executor->watch(std::make_unique<detail::FutureWithContainer<TContainer>>(
        std::move(timeLimit),
        std::forward<TContainer>(futures),
        std::move(p)
    ));

    return result;
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in the given
//! container become ready.
//!
//! \param executor The object that waits for the given futures to become ready.
//! \param futures The container that contains all the input futures.
//!
//! \note If the total time for waiting the input futures to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \sa WaitableTimedOutException
//!
//! \return An std::future<TContainer> that contains all the input futures, where
//! all the contained futures are ready.
template<class TContainer>
std::future<typename std::decay<TContainer>::type> all(std::shared_ptr<Executor> executor,
                                                       TContainer&& futures)
{
    return all<TContainer>(std::move(executor),
                           std::chrono::hours(1),
                           std::forward<TContainer>(futures));
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in the given
//! container become ready. This function uses the default Executor
//! object to wait for the given futures to become ready. If there isn't any default
//! Executor object registered, this function's behavior is undefined.
//!
//! \param timeLimit The maximum time to wait for all the given futures to become ready.
//! \param futures The container that contains all the input futures.
//!
//! \note If the total time for waiting the input futures to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \sa Default, WaitableTimedOutException
//!
//! \return An std::future<TContainer> that contains all the input futures, where
//! all the contained futures are ready.
template<class TContainer>
std::future<typename std::decay<TContainer>::type> all(std::chrono::microseconds timeLimit,
                                                       TContainer&& futures)
{
    return all<TContainer>(Default<Executor>(),
                           std::move(timeLimit),
                           std::forward<TContainer>(futures));
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in the given
//! container become ready. This function uses the default Executor
//! object to wait for the given futures to become ready. If there isn't any default
//! Executor object registered, this function's behavior is undefined.
//!
//! \param futures The container that contains all the input futures.
//!
//! \note If the total time for waiting the input futures to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \sa Default, WaitableTimedOutException
//!
//! \return An std::future<TContainer> that contains all the input futures, where
//! all the contained futures are ready.
template<class TContainer>
std::future<typename std::decay<TContainer>::type> all(TContainer&& futures)
{
    return all<TContainer>(std::chrono::hours(1),
                           std::forward<TContainer>(futures));
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in the given
//! tuple become ready.
//!
//! \param executor The object that waits for the given futures to become ready.
//! \param timeLimit The maximum time to wait for all the given futures to become ready.
//! \param futures The input futures as a tuple.
//!
//! \note If the total time for waiting the input futures to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \sa WaitableTimedOutException
//!
//! \return An std::future<std::tuple> that contains all the input futures, where
//! all the contained futures are ready.
template<typename... Args>
std::future<std::tuple<std::future<Args>...>> all(std::shared_ptr<Executor> executor,
                                                  std::chrono::microseconds timeLimit,
                                                  std::tuple<std::future<Args>...> futures)
{
    std::promise<std::tuple<std::future<Args>...>> p;

    auto result = p.get_future();

    executor->watch(std::make_unique<detail::FutureWithTuple<Args...>>(
        std::move(timeLimit),
        std::move(futures),
        std::move(p)
    ));

    return result;
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in the given
//! tuple become ready.
//!
//! \param executor The object that waits for the given futures to become ready.
//! \param futures The input futures as a tuple.
//!
//! \note If the total time for waiting the input futures to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \sa WaitableTimedOutException
//!
//! \return An std::future<std::tuple> that contains all the input futures, where
//! all the contained futures are ready.
template<typename... Args>
std::future<std::tuple<std::future<Args>...>> all(std::shared_ptr<Executor> executor,
                                                  std::tuple<std::future<Args>...> futures)
{
    return all<Args...>(std::move(executor),
                        std::chrono::hours(1),
                        std::move(futures));
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in the given
//! tuple become ready. This function uses the default Executor
//! object to wait for the given futures to become ready. If there isn't any default
//! Executor object registered, this function's behavior is undefined.
//!
//! \param timeLimit The maximum time to wait for all the given futures to become ready.
//! \param futures The input futures as a tuple.
//!
//! \note If the total time for waiting the input futures to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \sa Default, WaitableTimedOutException
//!
//! \return An std::future<std::tuple> that contains all the input futures, where
//! all the contained futures are ready.
template<typename... Args>
std::future<std::tuple<std::future<Args>...>> all(std::chrono::microseconds timeLimit,
                                                  std::tuple<std::future<Args>...> futures)
{
    return all<Args...>(Default<Executor>(),
                        std::move(timeLimit),
                        std::move(futures));
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in the given
//! tuple become ready. This function uses the default Executor
//! object to wait for the given futures to become ready. If there isn't any default
//! Executor object registered, this function's behavior is undefined.
//!
//! \param futures The input futures as a tuple.
//!
//! \note If the total time for waiting the input futures to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \sa Default, WaitableTimedOutException
//!
//! \return An std::future<std::tuple> that contains all the input futures, where
//! all the contained futures are ready.
template<typename... Args>
std::future<std::tuple<std::future<Args>...>> all(std::tuple<std::future<Args>...> futures)
{
    return all<Args...>(std::chrono::hours(1),
                        std::move(futures));
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures given as
//! arguments become ready.
//!
//! \param executor The object that waits for the given futures to become ready.
//! \param timeLimit The maximum time to wait for all the given futures to become ready.
//! \param futures... The input futures as variable arguments.
//!
//! \note If the total time for waiting the input futures to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \sa WaitableTimedOutException
//!
//! \return An std::future<std::tuple> that contains all the input futures, where
//! all the contained futures are ready.
template<typename Arg, typename... Args>
std::future<std::tuple<std::future<Arg>, std::future<Args>...>> all(
    std::shared_ptr<Executor> executor,
    std::chrono::microseconds timeLimit,
    std::future<Arg> future,
    std::future<Args>... futures
)
{
    using Tuple = std::tuple<std::future<Arg>, std::future<Args>...>;

    return all<Arg, Args...>(executor,
                             std::move(timeLimit),
                             Tuple{ std::move(future), std::move(futures)... });
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures given as
//! arguments become ready.
//!
//! \param executor The object that waits for the given futures to become ready.
//! \param futures... The input futures as variable arguments.
//!
//! \note If the total time for waiting the input futures to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \sa WaitableTimedOutException
//!
//! \return An std::future<std::tuple> that contains all the input futures, where
//! all the contained futures are ready.
template<typename Arg, typename... Args>
std::future<std::tuple<std::future<Arg>, std::future<Args>...>> all(
    std::shared_ptr<Executor> executor,
    std::future<Arg> future,
    std::future<Args>... futures
)
{
    return all<Arg, Args...>(std::move(executor),
                             std::chrono::hours(1),
                             std::move(future),
                             std::move(futures)...);
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures given as
//! arguments become ready. This function uses the default Executor
//! object to wait for the given futures to become ready. If there isn't any default
//! Executor object registered, this function's behavior is undefined.
//!
//! \param timeLimit The maximum time to wait for all the given futures to become ready.
//! \param futures... The input futures as variable arguments.
//!
//! \note If the total time for waiting the input futures to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \sa Default, WaitableTimedOutException
//!
//! \return An std::future<std::tuple> that contains all the input futures, where
//! all the contained futures are ready.
template<typename Arg, typename... Args>
std::future<std::tuple<std::future<Arg>, std::future<Args>...>> all(std::chrono::microseconds timeLimit,
                                                                    std::future<Arg> future,
                                                                    std::future<Args>... futures)
{
    return all<Arg, Args...>(Default<Executor>(),
                             std::move(timeLimit),
                             std::move(future),
                             std::move(futures)...);
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures given as
//! arguments become ready. This function uses the default Executor
//! object to wait for the given futures to become ready. If there isn't any default
//! Executor object registered, this function's behavior is undefined.
//!
//! \param futures... The input futures as variable arguments.
//!
//! \note If the total time for waiting the input futures to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \sa Default, WaitableTimedOutException
//!
//! \return An std::future<std::tuple> that contains all the input futures, where
//! all the contained futures are ready.
template<typename Arg, typename... Args>
std::future<std::tuple<std::future<Arg>, std::future<Args>...>> all(std::future<Arg> future,
                                                                    std::future<Args>... futures)
{
    return all<Arg, Args...>(Default<Executor>(),
                             std::chrono::hours(1),
                             std::move(future),
                             std::move(futures)...);
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in range
//! [first, last) become ready.
//!
//! \param executor The object that waits for the given futures to become ready.
//! \param timeLimit The maximum time to wait for all the given futures to become ready.
//! \param first The first ForwardIterator of the range [first, last).
//! \param last A ForwardIterator that marks the end of the range [first, last).
//!
//! \note If the total time for waiting the input futures to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \note The original containers, from which first and last are obtained, have to stay
//! alive and stable (in the same memory address) until the client code finishes extracting
//! all the results from the futures in [first, last) or until the continuations
//! attached to the resulting future, using then(), finish processing.
//!
//! \sa then(), WaitableTimedOutException
//!
//! \return A std::future<std::tuple> with the input ForwardIterators, where all the
//! futures in range [first, last) are ready.
template<class TForwardIterator>
std::future<std::tuple<TForwardIterator, TForwardIterator>> all(
    std::shared_ptr<Executor> executor,
    std::chrono::microseconds timeLimit,
    TForwardIterator first,
    TForwardIterator last
)
{
    std::promise<std::tuple<TForwardIterator, TForwardIterator>> p;

    auto result = p.get_future();

    executor->watch(std::make_unique<detail::FutureWithIterators<TForwardIterator>>(
        std::move(timeLimit),
        first,
        last,
        std::move(p)
    ));

    return result;
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in range
//! [first, last) become ready.
//!
//! \param executor The object that waits for the given futures to become ready.
//! \param first The first ForwardIterator of the range [first, last).
//! \param last A ForwardIterator that marks the end of the range [first, last).
//!
//! \note If the total time for waiting the input futures to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \note The original containers, from which first and last are obtained, have to stay
//! alive and stable (in the same memory address) until the client code finishes extracting
//! all the results from the futures in [first, last) or until the continuations
//! attached to the resulting future, using then(), finish processing.
//!
//! \sa then(), WaitableTimedOutException
//!
//! \return A std::future<std::tuple> with the input ForwardIterators, where all the
//! futures in range [first, last) are ready.
template<class TForwardIterator>
std::future<std::tuple<TForwardIterator, TForwardIterator>> all(
    std::shared_ptr<Executor> executor,
    TForwardIterator first,
    TForwardIterator last
)
{
    return all<TForwardIterator>(std::move(executor),
                                 std::chrono::hours(1),
                                 first,
                                 last);
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in range
//! [first, last) become ready. This function uses the default Executor
//! object to wait for the given futures to become ready. If there isn't any default
//! Executor object registered, this function's behavior is undefined.
//!
//! \param timeLimit The maximum time to wait for all the given futures to become ready.
//! \param first The first ForwardIterator of the range [first, last).
//! \param last A ForwardIterator that marks the end of the range [first, last).
//!
//! \note If the total time for waiting the input futures to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \note The original containers, from which first and last are obtained, have to stay
//! alive and stable (in the same memory address) until the client code finishes extracting
//! all the results from the futures in [first, last) or until the continuations
//! attached to the resulting future, using then(), finish processing.
//!
//! \sa then(), Default, WaitableTimedOutException
//!
//! \return A std::future<std::tuple> with the input ForwardIterators, where all the
//! futures in range [first, last) are ready.
template<class TForwardIterator>
std::future<std::tuple<TForwardIterator, TForwardIterator>> all(std::chrono::microseconds timeLimit,
                                                                TForwardIterator first,
                                                                TForwardIterator last)
{
    return all<TForwardIterator>(Default<Executor>(),
                                 std::move(timeLimit),
                                 first,
                                 last);
}

//! \brief Creates a future that becomes ready when all the input futures become ready.
//!
//! \par The resulting future becomes ready when all the futures in range
//! [first, last) become ready. This function uses the default Executor
//! object to wait for the given futures to become ready. If there isn't any default
//! Executor object registered, this function's behavior is undefined.
//!
//! \param first The first ForwardIterator of the range [first, last).
//! \param last A ForwardIterator that marks the end of the range [first, last).
//!
//! \note If the total time for waiting the input futures to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \note The original containers, from which first and last are obtained, have to stay
//! alive and stable (in the same memory address) until the client code finishes extracting
//! all the results from the futures in [first, last) or until the continuations
//! attached to the resulting future, using then(), finish processing.
//!
//! \sa then(), Default, WaitableTimedOutException
//!
//! \return A std::future<std::tuple> with the input ForwardIterators, where all the
//! futures in range [first, last) are ready.
template<class TForwardIterator>
std::future<std::tuple<TForwardIterator, TForwardIterator>> all(TForwardIterator first,
                                                                TForwardIterator last)
{
    return all<TForwardIterator>(Default<Executor>(),
                                 std::chrono::hours(1),
                                 first,
                                 last);
}

} // namespace futures
} // namespace thousandeyes
