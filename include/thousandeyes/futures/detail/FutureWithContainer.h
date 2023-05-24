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

template <class TContainer>
class FutureWithContainer : public TimedWaitable {
public:
    FutureWithContainer(std::chrono::microseconds waitLimit,
                        TContainer&& futures,
                        std::promise<typename std::decay<TContainer>::type> p) :
        TimedWaitable(std::move(waitLimit)),
        futures_(std::forward<TContainer>(futures)),
        p_(std::move(p))
    {}

    FutureWithContainer(const FutureWithContainer& o) = delete;
    FutureWithContainer& operator=(const FutureWithContainer& o) = delete;

    FutureWithContainer(FutureWithContainer&& o) = default;
    FutureWithContainer& operator=(FutureWithContainer&& o) = default;

    bool timedWait(const std::chrono::microseconds& timeout) override
    {
        for (const auto& f : futures_) {
            if (f.wait_for(timeout) != std::future_status::ready) {
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
            p_.set_value(std::move(futures_));
        }
        catch (...) {
            p_.set_exception(std::current_exception());
        }
    }

private:
    typename std::decay<TContainer>::type futures_;
    std::promise<typename std::decay<TContainer>::type> p_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
