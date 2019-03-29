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

#include <thousandeyes/futures/TimedWaitable.h>

namespace thousandeyes {
namespace futures {
namespace detail {

template<class TForwardIterator>
class FutureWithIterators : public TimedWaitable {
public:
    FutureWithIterators(std::chrono::microseconds waitLimit,
                        TForwardIterator firstIter,
                        TForwardIterator lastIter,
                        std::promise<std::tuple<TForwardIterator, TForwardIterator>> p) :
        TimedWaitable(std::move(waitLimit)),
        range_(firstIter, lastIter),
        p_(std::move(p))
    {}

    FutureWithIterators(const FutureWithIterators& o) = delete;
    FutureWithIterators& operator=(const FutureWithIterators& o) = delete;

    FutureWithIterators(FutureWithIterators&& o) = default;
    FutureWithIterators& operator=(FutureWithIterators&& o) = default;

    bool timedWait(const std::chrono::microseconds& timeout) override
    {
        TForwardIterator end = std::get<1>(range_);
        for (TForwardIterator iter = std::get<0>(range_); iter != end; ++iter) {
            if (iter->wait_for(timeout) != std::future_status::ready) {
                return false;
            }
        }
        return true;
    }

    void dispatch(std::exception_ptr err) override
    {
        if (err) {
            p_.set_exception(err);
            return;
        }

        try {
            p_.set_value(std::move(range_));
        }
        catch (...) {
            p_.set_exception(std::current_exception());
        }
    }

private:
    std::tuple<TForwardIterator, TForwardIterator> range_;
    std::promise<std::tuple<TForwardIterator, TForwardIterator>> p_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
