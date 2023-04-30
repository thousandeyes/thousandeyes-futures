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
#include <tuple>
#include <utility>

#include <thousandeyes/futures/TimedWaitable.h>

namespace thousandeyes {
namespace futures {
namespace detail {

template <int N, class T>
struct TupleItemsWaitFor {
    bool operator()(T& t, const std::chrono::microseconds& timeout)
    {
        if (std::get<N>(t).wait_for(timeout) != std::future_status::ready) {
            return false;
        }
        return TupleItemsWaitFor<N - 1, T>()(t, timeout);
    }
};

template <class T>
struct TupleItemsWaitFor<0, T> {
    bool operator()(T& t, const std::chrono::microseconds& timeout)
    {
        return std::get<0>(t).wait_for(timeout) == std::future_status::ready;
    }
};

template <typename... Args>
class FutureWithTuple : public TimedWaitable {
public:
    FutureWithTuple(std::chrono::microseconds waitLimit,
                    std::tuple<std::future<Args>...> futures,
                    std::promise<std::tuple<std::future<Args>...>> p) :
        TimedWaitable(std::move(waitLimit)),
        futures_(std::move(futures)),
        p_(std::move(p))
    {}

    FutureWithTuple(const FutureWithTuple& o) = delete;
    FutureWithTuple& operator=(const FutureWithTuple& o) = delete;

    FutureWithTuple(FutureWithTuple&& o) = default;
    FutureWithTuple& operator=(FutureWithTuple&& o) = default;

    bool timedWait(const std::chrono::microseconds& timeout) override
    {
        return TupleItemsWaitFor<sizeof...(Args) - 1, std::tuple<std::future<Args>...>>()(futures_,
                                                                                          timeout);
    }

    void dispatch(std::exception_ptr err) override
    {
        if (err) {
            p_.set_exception(err);
            return;
        }

        try {
            p_.set_value(std::move(futures_));
        }
        catch (...) {
            p_.set_exception(std::current_exception());
        }
    }

private:
    std::tuple<std::future<Args>...> futures_;
    std::promise<std::tuple<std::future<Args>...>> p_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
