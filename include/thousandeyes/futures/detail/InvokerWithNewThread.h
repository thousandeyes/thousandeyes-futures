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

#include <functional>
#include <mutex>
#include <thread>
#include <utility>

namespace thousandeyes {
namespace futures {
namespace detail {

class InvokerWithNewThread {
public:
    ~InvokerWithNewThread()
    {
        stop();
    }

    void start() {}

    void stop()
    {
        if (t0_.joinable() && t0_.get_id() != std::this_thread::get_id()) {
            t0_.join();
        }

        if (t1_.joinable() && t1_.get_id() != std::this_thread::get_id()) {
            t1_.join();
        }
    }

    void operator()(std::function<void()> f)
    {
        if (!usingT0_ && t0_.joinable()) {
            t0_.join();
        }

        if (usingT0_ && t1_.joinable()) {
            t1_.join();
        }

        (!usingT0_ ? t0_ : t1_) = std::thread(std::move(f));

        usingT0_ = !usingT0_; // flip
    }

private:
    bool usingT0_{ false };

    std::thread t0_;
    std::thread t1_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
