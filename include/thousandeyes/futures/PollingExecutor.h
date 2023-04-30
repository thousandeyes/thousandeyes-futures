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
template <class TPollFunctor, class TDispatchFunctor>
class PollingExecutor :
    public Executor,
    public std::enable_shared_from_this<PollingExecutor<TPollFunctor, TDispatchFunctor>> {
public:
    //! \brief Constructs a #PollingExecutor with default-constructed functors
    //! for polling and dispatching ready #Waitables
    //!
    //! \param q The polling timeout.
    PollingExecutor(std::chrono::microseconds q) :
        q_(std::move(q)),
        pollFunc_(std::make_unique<TPollFunctor>()),
        dispatchFunc_(std::make_unique<TDispatchFunctor>())
    {}

    //! \brief Constructs a #PollingExecutor with the given functors
    //! for polling and dispatching ready #Waitables
    //!
    //! \param q The polling timeout.
    //! \param pollFunc The functor used to dispatch the polling function.
    //! \param dispatchFunc The functor used to dispatch the ready #Waitables.
    PollingExecutor(std::chrono::microseconds q,
                    TPollFunctor&& pollFunc,
                    TDispatchFunctor&& dispatchFunc) :
        q_(std::move(q)),
        pollFunc_(std::make_unique<TPollFunctor>(std::forward<TPollFunctor>(pollFunc))),
        dispatchFunc_(
            std::make_unique<TDispatchFunctor>(std::forward<TDispatchFunctor>(dispatchFunc)))
    {}

    ~PollingExecutor()
    {
        stop();

        pollFunc_.reset();
        dispatchFunc_.reset();
    }

    PollingExecutor(const PollingExecutor& o) = delete;
    PollingExecutor& operator=(const PollingExecutor& o) = delete;

    void watch(std::unique_ptr<Waitable> w) override final
    {
        bool isActive;
        {
            std::lock_guard<std::mutex> lock(mutex_);

            isActive = active_;

            if (isActive) {
                waitables_.push(std::move(w));

                if (isPollerRunning_) {
                    return;
                }

                isPollerRunning_ = true;
            }
        }

        if (!isActive) {
            cancel_(std::move(w), "Executor inactive");
            return;
        }

        (*pollFunc_)([this, keep = this->shared_from_this()]() {
            while (true) {
                std::unique_ptr<Waitable> w;
                {
                    std::lock_guard<std::mutex> lock(mutex_);

                    if (waitables_.empty() || !active_) {
                        isPollerRunning_ = false;
                        break;
                    }

                    w = std::move(waitables_.front());
                    waitables_.pop();
                }

                try {
                    if (!w->wait(q_)) {
                        std::lock_guard<std::mutex> lock(mutex_);
                        waitables_.push(std::move(w));
                        continue;
                    }

                    dispatch_(std::move(w), nullptr);
                }
                catch (...) {
                    dispatch_(std::move(w), std::current_exception());
                }
            }
        });
    }

    void stop() override final
    {
        std::queue<std::unique_ptr<Waitable>> pending;
        {
            std::lock_guard<std::mutex> lock(mutex_);

            active_ = false;
            pending.swap(waitables_);
        }

        while (!pending.empty()) {
            cancel_(std::move(pending.front()), "Executor stoped");
            pending.pop();
        }
    }

private:
    inline void dispatch_(std::unique_ptr<Waitable> w, std::exception_ptr error)
    {
        // Using shared_ptr to enable copy-ability of the lambda, otherwise the
        // dispatchFunc_ would not be able to accept it as function<void()>
        std::shared_ptr<Waitable> wShared = std::move(w);
        (*dispatchFunc_)(
            [w = std::move(wShared), error = std::move(error)]() { w->dispatch(error); });
    }

    inline void cancel_(std::unique_ptr<Waitable> w, const std::string& message)
    {
        auto error = std::make_exception_ptr(WaitableWaitException(message));
        dispatch_(std::move(w), std::move(error));
    }

    const std::chrono::microseconds q_;

    std::mutex mutex_;
    std::queue<std::unique_ptr<Waitable>> waitables_;
    bool active_{true};
    bool isPollerRunning_{false};

    std::unique_ptr<TPollFunctor> pollFunc_;
    std::unique_ptr<TDispatchFunctor> dispatchFunc_;
};

} // namespace futures
} // namespace thousandeyes
