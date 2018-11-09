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
        if (t0_.joinable()) {
            t0_.join();
        }

        if (t1_.joinable()) {
            t1_.join();
        }
    }

    void operator()(std::function<void()> f)
    {
        std::lock_guard<std::mutex> lock(m_);

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
    std::mutex m_;

    bool usingT0_{ false };

    std::thread t0_;
    std::thread t1_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
