/*
 * Copyright 2024 ThousandEyes, Inc.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 *
 * @author Marcus Nutzinger, https://github.com/manutzin-te
 */

#pragma once

#include <future>
#include <memory>

#include <thousandeyes/futures/Default.h>
#include <thousandeyes/futures/detail/ObservedFutureWithContinuation.h>
#include <thousandeyes/futures/Executor.h>

namespace thousandeyes {
namespace futures {

//! \brief Observes the input futures and calls the given continuation function once
//! it becomes ready.
//!
//! \param executor The object that waits for the given future to become ready.
//! \param timeLimit The maximum time to wait for the given future to become ready.
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note Any exceptions from the continuation or from the executor will be thrown
//! on the thread on which the continuation is scheduled.
//!
//! \sa WaitableTimedOutException
template <class TIn, class TFunc>
void observe(std::shared_ptr<Executor> executor,
             std::chrono::microseconds timeLimit,
             std::future<TIn> f,
             TFunc&& cont)
{
    executor->watch(std::make_unique<detail::ObservedFutureWithContinuation<TIn, TFunc>>(
        std::move(timeLimit),
        std::move(f),
        std::forward<TFunc>(cont)));
}

//! \brief Observes the input futures and calls the given continuation function once
//! it becomes ready.
//!
//! \param executor The object that waits for the given future to become ready.
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note If the total time for waiting the input future to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), an exception of type
//! WaitableTimedOutException will be thrown on the thread on which the continuation
//! is scheduled.
//!
//! \sa WaitableTimedOutException
template <class TIn, class TFunc>
void observe(std::shared_ptr<Executor> executor, std::future<TIn> f, TFunc&& cont)
{
    observe<TIn, TFunc>(std::move(executor),
                        std::chrono::hours(1),
                        std::move(f),
                        std::forward<TFunc>(cont));
}

//! \brief Observes the input futures and calls the given continuation function once
//! it becomes ready.
//!
//! \par This function uses the default Executor object to wait for the given futures
//! to become ready. If there isn't any default Executor object registered, this
//! function's behavior is undefined.
//!
//! \param timeLimit The maximum time to wait for the given future to become ready.
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note If the total time for waiting the input future to become ready exceeds the
//! given timeLimit, an exception of type WaitableTimedOutException will be thrown on
//! the thread on which the continuation is scheduled.
//!
//! \sa Default, WaitableTimedOutException
template <class TIn, class TFunc>
void observe(std::chrono::microseconds timeLimit, std::future<TIn> f, TFunc&& cont)
{
    observe<TIn, TFunc>(Default<Executor>(),
                        std::move(timeLimit),
                        std::move(f),
                        std::forward<TFunc>(cont));
}

//! \brief Observes the input futures and calls the given continuation function once
//! it becomes ready.
//!
//! \par This function uses the default Executor object to wait for the given futures
//! to become ready. If there isn't any default Executor object registered, this
//! function's behavior is undefined.
//!
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note If the total time for waiting the input future to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), an exception of
//! type WaitableTimedOutException will be thrown on the thread on which the
//! continuation is scheduled.
//!
//! \sa Default, WaitableTimedOutException
template <class TIn, class TFunc>
void observe(std::future<TIn> f, TFunc&& cont)
{
    return observe<TIn, TFunc>(std::chrono::hours(1), std::move(f), std::forward<TFunc>(cont));
}

} // namespace futures
} // namespace thousandeyes
