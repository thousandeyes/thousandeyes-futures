/*
 * Copyright 2019 ThousandEyes, Inc.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 *
 * @author Giannis Georgalis, https://github.com/ggeorgalis
 */

#pragma once

#include <future>
#include <memory>

#include <thousandeyes/futures/TimedWaitable.h>

namespace thousandeyes {
namespace futures {
namespace detail {

template<class TIn, class TOut, class TFunc>
class FutureWithContinuation : public TimedWaitable {
public:
    FutureWithContinuation(std::chrono::microseconds waitLimit,
                           std::future<TIn> f,
                           std::promise<TOut> p,
                           TFunc&& cont) :
        TimedWaitable(std::move(waitLimit)),
        f_(std::move(f)),
        p_(std::move(p)),
        cont_(std::forward<TFunc>(cont))
    {}

    FutureWithContinuation(const FutureWithContinuation& o) = delete;
    FutureWithContinuation& operator=(const FutureWithContinuation& o) = delete;

    FutureWithContinuation(FutureWithContinuation&& o) = default;
    FutureWithContinuation& operator=(FutureWithContinuation&& o) = default;

    bool timedWait(const std::chrono::microseconds& timeout) override
    {
        return f_.wait_for(timeout) == std::future_status::ready;
    }

    void dispatch(std::exception_ptr err) override
    {
        if (err) {
            p_.set_exception(err);
            return;
        }

        try {
            p_.set_value(cont_(std::move(f_)));
        }
        catch (...) {
            p_.set_exception(std::current_exception());
        }
    }

private:
    std::future<TIn> f_;
    std::promise<TOut> p_;
    TFunc cont_;
};

// Partial specialization for void output type

template<class TIn, class TFunc>
class FutureWithContinuation<TIn, void, TFunc> : public TimedWaitable {
public:
    FutureWithContinuation(std::chrono::microseconds waitLimit,
                           std::future<TIn> f,
                           std::promise<void> p,
                           TFunc&& cont) :
        TimedWaitable(std::move(waitLimit)),
        f_(std::move(f)),
        p_(std::move(p)),
        cont_(std::forward<TFunc>(cont))
    {}

    FutureWithContinuation(const FutureWithContinuation& o) = delete;
    FutureWithContinuation& operator=(const FutureWithContinuation& o) = delete;

    FutureWithContinuation(FutureWithContinuation&& o) = default;
    FutureWithContinuation& operator=(FutureWithContinuation&& o) = default;

    bool timedWait(const std::chrono::microseconds& timeout) override
    {
        return f_.wait_for(timeout) == std::future_status::ready;
    }

    void dispatch(std::exception_ptr err) override
    {
        if (err) {
            p_.set_exception(err);
            return;
        }

        try {
            cont_(std::move(f_));
            p_.set_value();
        }
        catch (...) {
            p_.set_exception(std::current_exception());
        }
    }

private:
    std::future<TIn> f_;
    std::promise<void> p_;
    TFunc cont_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
