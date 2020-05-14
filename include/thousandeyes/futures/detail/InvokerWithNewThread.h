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
        std::lock_guard<std::mutex> lock(m_);

        for (auto& t: ts_) {
            if (t.joinable() && t.get_id() != std::this_thread::get_id()) {
                t.join();
            }
        }
    }

    void operator()(std::function<void()> f)
    {
        std::lock_guard<std::mutex> lock(m_);

        auto iter = ts_.begin();
        while (iter != ts_.end()) {
            if (iter->joinable() && iter->get_id() != std::this_thread::get_id()) {
                iter->join();
                iter = ts_.erase(iter);
            }
            else {
                ++iter;
            }
        }

        ts_.push_back(std::thread(std::move(f)));
    }

private:
    std::mutex m_;
    std::vector<std::thread> ts_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
