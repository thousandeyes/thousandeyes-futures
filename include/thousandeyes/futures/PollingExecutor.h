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
#include <iterator>
#include <memory>
#include <mutex>
#include <vector>

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
        pollFunc_(std::make_unique<TPollFunctor>(
            std::forward<TPollFunctor>(pollFunc)
        )),
        dispatchFunc_(std::make_unique<TDispatchFunctor>(
            std::forward<TDispatchFunctor>(dispatchFunc)
        ))
    {}

    ~PollingExecutor()
    {
        stop();
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
                waitables_.push_back(std::move(w));

                if (isPollerRunning_) {
                    return;
                }

                isPollerRunning_ = true;
            }
        }

        if (!isActive) {
            dispatch_(move(w),
                      std::make_exception_ptr(WaitableWaitException("Executor inactive")));
            return;
        }

        (*pollFunc_)([this, keep=this->shared_from_this()]() {
            poll_();
        });
    }

    void stop() override final
    {
        bool wasActive;
        std::vector<std::unique_ptr<Waitable>> pending;
        {
            std::lock_guard<std::mutex> lock(mutex_);

            wasActive = active_;
            active_ = false;
            pending.swap(waitables_);
        }

        if (!wasActive) {
            return;
        }

        auto error = std::make_exception_ptr(WaitableWaitException("Executor stoped"));
        for (std::unique_ptr<Waitable>& w: pending) {
            dispatch_(move(w), error);
        }

        pollFunc_.reset();
        dispatchFunc_.reset();
    }

private:
    inline void dispatch_(std::unique_ptr<Waitable> w, std::exception_ptr error)
    {
        // Using shared_ptr to enable copy-ability of the lambda, otherwise the
        // dispatchFunc_ would not be able to accept it as function<void()>
        std::shared_ptr<Waitable> wShared = std::move(w);
        (*dispatchFunc_)([w=std::move(wShared), error=std::move(error)]() {
            w->dispatch(error);
        });
    }

    inline void poll_()
    {
        // Kept sorted by deadline
        std::vector<std::unique_ptr<Waitable>> polling;

        while (true) {

            std::vector<std::unique_ptr<Waitable>> newWaitables;
            {
                std::lock_guard<std::mutex> lock(mutex_);

                if ((polling.empty() && waitables_.empty()) || !active_) {
                    isPollerRunning_ = false;

                    std::move(polling.begin(),
                              polling.end(),
                              std::back_inserter(waitables_));
                    break;
                }

                newWaitables.swap(waitables_);
            }

            for (auto& nw: newWaitables) {
                auto iter = std::upper_bound(polling.begin(),
                                             polling.end(),
                                             nw,
                                             [](const auto& a, const auto& b) {
                    return a->compare(*b) <= std::chrono::milliseconds(0);
                });
                polling.insert(iter, std::move(nw));
            }

            for (std::unique_ptr<Waitable>& w: polling) {
                try {
                    if (w->wait(q_)) {
                        dispatch_(std::move(w), nullptr);
                    }
                }
                catch (...) {
                    dispatch_(std::move(w), std::current_exception());
                }
            }

            // Remove dispatched waitables
            auto iter = std::remove_if(polling.begin(), polling.end(), [](const auto& w) {
                return !w;
            });
            polling.erase(iter, polling.end());
        }
    }

    const std::chrono::microseconds q_;

    std::mutex mutex_;
    std::vector<std::unique_ptr<Waitable>> waitables_;
    bool active_{ true };
    bool isPollerRunning_{ false };

    std::unique_ptr<TPollFunctor> pollFunc_;
    std::unique_ptr<TDispatchFunctor> dispatchFunc_;
};

} // namespace futures
} // namespace thousandeyes
