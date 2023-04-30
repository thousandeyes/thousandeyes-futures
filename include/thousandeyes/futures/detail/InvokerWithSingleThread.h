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

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

namespace thousandeyes {
namespace futures {
namespace detail {

class InvokerWithSingleThread {
public:
    InvokerWithSingleThread() : state_(std::make_shared<State>())
    {
        std::thread([s = state_]() {
            std::unique_lock<std::mutex> lock(s->m);

            while (s->active) {
                s->cv.wait(lock, [&s]() { return !s->active || !s->fs.empty(); });

                while (!s->fs.empty()) {
                    std::function<void()> f = std::move(s->fs.front());
                    s->fs.pop();

                    lock.unlock();
                    f();

                    // Ensure f is destroyed before re-acquiring the lock
                    f = std::function<void()>{};
                    lock.lock();
                }
            }
        }).detach();
    }

    ~InvokerWithSingleThread()
    {
        bool wasActive;
        {
            std::lock_guard<std::mutex> lock(state_->m);

            wasActive = state_->active;
            state_->active = false;
        }

        if (wasActive) {
            state_->cv.notify_one();
        }
    }

    void operator()(std::function<void()> f)
    {
        bool wasEmpty;
        {
            std::lock_guard<std::mutex> lock(state_->m);

            wasEmpty = state_->fs.empty();
            state_->fs.push(std::move(f));
        }

        if (wasEmpty) {
            state_->cv.notify_one();
        }
    }

private:
    struct State {
        std::mutex m;
        std::condition_variable cv;
        bool active{true};
        std::queue<std::function<void()>> fs;
    };

    std::shared_ptr<State> state_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
