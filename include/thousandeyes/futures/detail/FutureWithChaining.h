#pragma once

#include <future>
#include <memory>

#include <thousandeyes/futures/Executor.h>
#include <thousandeyes/futures/TimedWaitable.h>
#include <thousandeyes/futures/detail/FutureWithForwarding.h>

namespace thousandeyes {
namespace futures {
namespace detail {

template<class TIn, class TOut, class TFunc>
class FutureWithChaining : public TimedWaitable {
public:
    FutureWithChaining(std::chrono::microseconds waitLimit,
                       std::weak_ptr<Executor> executor,
                       std::future<TIn> f,
                       std::promise<TOut> p,
                       TFunc&& cont) :
        TimedWaitable(std::move(waitLimit)),
        executor_(std::move(executor)),
        f_(std::move(f)),
        p_(std::move(p)),
        cont_(std::move(cont))
    {}

    FutureWithChaining(const FutureWithChaining& o) = delete;
    FutureWithChaining& operator=(const FutureWithChaining& o) = delete;

    FutureWithChaining(FutureWithChaining&& o) = default;
    FutureWithChaining& operator=(FutureWithChaining&& o) = default;

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
            if (auto e = executor_.lock()) {
                e->watch(std::make_unique<FutureWithForwarding<TOut>>(getWaitLimit(),
                                                                      cont_(std::move(f_)),
                                                                      std::move(p_)));
            }
            else {
                throw WaitableWaitException("No executor available");
            }
        }
        catch (...) {
            p_.set_exception(std::current_exception());
        }
    }

private:
    std::weak_ptr<Executor> executor_;
    std::future<TIn> f_;
    std::promise<TOut> p_;
    TFunc cont_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
