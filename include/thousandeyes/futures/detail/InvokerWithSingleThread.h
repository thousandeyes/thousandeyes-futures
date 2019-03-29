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

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

namespace thousandeyes {
namespace futures {
namespace detail {

class InvokerWithSingleThread {
public:
    ~InvokerWithSingleThread()
    {
        stop();
    }

    void start()
    {
        t_ = std::thread([this]() {
            while (true) {
                std::function<void()> f;
                {
                    std::unique_lock<std::mutex> lock(m_);

                    while (fs_.empty() && active_) {
                        cv_.wait(lock);
                    }

                    if (!active_) {
                        break;
                    }

                    f = std::move(fs_.front());
                    fs_.pop();
                }

                f();
            }
        });
    }

    void stop()
    {
        bool wasActive;
        {
            std::lock_guard<std::mutex> lock(m_);

            wasActive = active_;
            active_ = false;
        }

        if (wasActive) {
            cv_.notify_one();
        }

        if (t_.joinable() && t_.get_id() != std::this_thread::get_id()) {
            t_.join();
        }
    }

    void operator()(std::function<void()> f)
    {
        bool wasEmpty;
        {
            std::lock_guard<std::mutex> lock(m_);

            wasEmpty = fs_.empty();
            fs_.push(std::move(f));
        }

        if (wasEmpty) {
            cv_.notify_one();
        }
    }

private:
    std::mutex m_;
    std::condition_variable cv_;
    std::thread t_;

    bool active_{ true };

    std::queue<std::function<void()>> fs_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
