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

#include <thousandeyes/futures/TimedWaitable.h>

namespace thousandeyes {
namespace futures {
namespace detail {

template <class TIn, class TFunc>
class ObservedFutureWithContinuation : public TimedWaitable {
public:
    ObservedFutureWithContinuation(std::chrono::microseconds waitLimit,
                                   std::future<TIn> f,
                                   TFunc&& cont) :
        TimedWaitable(std::move(waitLimit)),
        f_(std::move(f)),
        cont_(std::forward<TFunc>(cont))
    {}

    ObservedFutureWithContinuation(const ObservedFutureWithContinuation& o) = delete;
    ObservedFutureWithContinuation& operator=(const ObservedFutureWithContinuation& o) = delete;

    ObservedFutureWithContinuation(ObservedFutureWithContinuation&& o) = default;
    ObservedFutureWithContinuation& operator=(ObservedFutureWithContinuation&& o) = default;

    bool timedWait(const std::chrono::microseconds& timeout) override
    {
        return f_.wait_for(timeout) == std::future_status::ready;
    }

    void dispatch(std::exception_ptr err) override
    {
        if (err) {
            std::rethrow_exception(err);
        }
        else {
            cont_(std::move(f_));
        }
    }

private:
    std::future<TIn> f_;
    TFunc cont_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
