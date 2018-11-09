#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <queue>

#include <thousandeyes/futures/Executor.h>
#include <thousandeyes/futures/Waitable.h>

namespace thousandeyes {
namespace futures {

//! \brief An implementation of the #Executor that polls to determine when the
//! "watched" #Waitable instances become ready.
//!
//! \note The PollingExecutor dispatches the polling function via the TPollFunctor
//! functor and, subsequently, dispatches a ready #Waitable via the TDispatchFunctor
//! functor.
template<class TPollFunctor, class TDispatchFunctor>
class PollingExecutor :
    public Executor,
    public std::enable_shared_from_this<PollingExecutor<TPollFunctor, TDispatchFunctor>> {
public:

    //! \brief Constructs a #PollingExecutor with default-constructed functors
    //! for polling and dispatching ready #Waitables
    //!
    //! \param q The polling frequancy.
    PollingExecutor(std::chrono::microseconds q) :
        q_(std::move(q))
    {}

    //! \brief Constructs a #PollingExecutor with the given functors
    //! for polling and dispatching ready #Waitables
    //!
    //! \param q The polling frequancy.
    //! \param pollFunc The functor used to dispatch the polling function.
    //! \param dispatchFunc The functor used to dispatch the ready #Waitables.
    PollingExecutor(std::chrono::microseconds q,
                    TPollFunctor&& pollFunc,
                    TDispatchFunctor&& dispatchFunc) :
        q_(std::move(q)),
        pollFunc_(std::forward<TPollFunctor>(pollFunc)),
        dispatchFunc_(std::forward<TDispatchFunctor>(dispatchFunc))
    {}

    ~PollingExecutor()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        while (!waitables_.empty()) {
            waitables_.pop();
        }
    }

    PollingExecutor(const PollingExecutor& o) = delete;
    PollingExecutor& operator=(const PollingExecutor& o) = delete;

    void watch(std::unique_ptr<Waitable> w) override final
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            waitables_.push(move(w));

            if (isPollerRunning_) {
                return;
            }

            isPollerRunning_ = true;
        }

        // shared_from_this() is thread-safe
        auto keep = this->shared_from_this();

        pollFunc_([this, keep]() {

            while (true) {

                std::unique_ptr<Waitable> w;
                {
                    std::lock_guard<std::mutex> lock(mutex_);

                    if (waitables_.empty()) {
                        isPollerRunning_ = false;
                        break;
                    }

                    w = move(waitables_.front());
                    waitables_.pop();
                }

                bool ready = true;
                std::exception_ptr error = nullptr;

                try {
                    ready = w->wait(q_);
                }
                catch (...) {
                    error = std::current_exception();
                }

                if (!ready) {
                    std::lock_guard<std::mutex> lock(mutex_);

                    waitables_.push(move(w));
                    continue;
                }

                // Using shared_ptr to enable copy-ability of the lambda, otherwise the
                // dispatchFunc_ would not be able to accept it as function<void()>
                std::shared_ptr<Waitable> wShared = std::move(w);
                dispatchFunc_([error, wShared]() {
                    wShared->dispatch(error);
                });
            }
        });
    }

private:
    const std::chrono::microseconds q_;

    std::mutex mutex_;
    std::queue<std::unique_ptr<Waitable>> waitables_;
    bool isPollerRunning_{ false };

    typename std::decay<TPollFunctor>::type pollFunc_;
    typename std::decay<TDispatchFunctor>::type dispatchFunc_;
};

} // namespace futures
} // namespace thousandeyes
