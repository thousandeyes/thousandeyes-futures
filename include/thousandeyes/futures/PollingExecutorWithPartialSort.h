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

#include <algorithm>
#include <chrono>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <vector>

#include <thousandeyes/futures/Executor.h>
#include <thousandeyes/futures/Waitable.h>

namespace thousandeyes {
namespace futures {

//! \brief An implementation of the #Executor that polls to determine when the
//! "watched" #Waitable instances become ready. This particular polling executor
//! also partially sorts the waitables left and right of their deadline median value.
//!
//! \note The PollingExecutorWithPartialSort dispatches the polling function via the TPollFunctor
//! functor and, subsequently, dispatches a ready #Waitable via the TDispatchFunctor
//! functor.
template <class TPollFunctor, class TDispatchFunctor>
class PollingExecutorWithPartialSort :
    public Executor,
    public std::enable_shared_from_this<
        PollingExecutorWithPartialSort<TPollFunctor, TDispatchFunctor>> {
public:
    //! \brief Constructs a #PollingExecutorWithPartialSort with default-constructed functors
    //! for polling and dispatching ready #Waitables
    //!
    //! \param q The polling timeout.
    PollingExecutorWithPartialSort(std::chrono::microseconds q) :
        q_(std::move(q)),
        pollFunc_(std::make_unique<TPollFunctor>()),
        dispatchFunc_(std::make_unique<TDispatchFunctor>())
    {}

    //! \brief Constructs a #PollingExecutorWithPartialSort with the given functors
    //! for polling and dispatching ready #Waitables
    //!
    //! \param q The polling timeout.
    //! \param pollFunc The functor used to dispatch the polling function.
    //! \param dispatchFunc The functor used to dispatch the ready #Waitables.
    PollingExecutorWithPartialSort(std::chrono::microseconds q,
                                   TPollFunctor&& pollFunc,
                                   TDispatchFunctor&& dispatchFunc) :
        q_(std::move(q)),
        pollFunc_(std::make_unique<TPollFunctor>(std::forward<TPollFunctor>(pollFunc))),
        dispatchFunc_(
            std::make_unique<TDispatchFunctor>(std::forward<TDispatchFunctor>(dispatchFunc)))
    {}

    ~PollingExecutorWithPartialSort()
    {
        stop();

        pollFunc_.reset();
        dispatchFunc_.reset();
    }

    PollingExecutorWithPartialSort(const PollingExecutorWithPartialSort& o) = delete;
    PollingExecutorWithPartialSort& operator=(const PollingExecutorWithPartialSort& o) = delete;

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
            cancel_(std::move(w), "Executor inactive");
            return;
        }

        (*pollFunc_)([this, keep = this->shared_from_this()]() { poll_(); });
    }

    void stop() override final
    {
        std::vector<std::unique_ptr<Waitable>> pending;
        {
            std::lock_guard<std::mutex> lock(mutex_);

            active_ = false;
            pending.swap(waitables_);
        }

        for (std::unique_ptr<Waitable>& w : pending) {
            cancel_(std::move(w), "Executor stoped");
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

    inline void poll_()
    {
        std::vector<std::unique_ptr<Waitable>> polling;
        polling.reserve(1000);

        while (true) {
            bool isPollerRunning;
            {
                std::lock_guard<std::mutex> lock(mutex_);

                std::move(waitables_.begin(), waitables_.end(), std::back_inserter(polling));
                waitables_.clear();

                if (!active_ || polling.empty()) {
                    isPollerRunning_ = false;
                }

                isPollerRunning = isPollerRunning_;
            }

            if (!isPollerRunning) {
                for (std::unique_ptr<Waitable>& w : polling) {
                    cancel_(std::move(w), "Executor stoped");
                }
                return;
            }

            auto middleIter = polling.begin() + polling.size() / 2;

            std::nth_element(polling.begin(),
                             middleIter,
                             polling.end(),
                             [](const auto& a, const auto& b) {
                                 return a->compare(*b) < std::chrono::milliseconds(0);
                             });

            std::for_each(polling.begin(), middleIter, [this](std::unique_ptr<Waitable>& w) {
                try {
                    if (w->wait(q_)) {
                        dispatch_(std::move(w), nullptr);
                    }
                }
                catch (...) {
                    dispatch_(std::move(w), std::current_exception());
                }
            });

            std::for_each(polling.begin(), polling.end(), [this](std::unique_ptr<Waitable>& w) {
                try {
                    if (w && w->wait(q_)) {
                        dispatch_(std::move(w), nullptr);
                    }
                }
                catch (...) {
                    dispatch_(std::move(w), std::current_exception());
                }
            });

            // Remove dispatched waitables
            polling.erase(std::remove_if(polling.begin(),
                                         polling.end(),
                                         std::logical_not<std::unique_ptr<Waitable>>()),
                          polling.end());
        }
    }

    const std::chrono::microseconds q_;

    std::mutex mutex_;
    std::vector<std::unique_ptr<Waitable>> waitables_;
    bool active_{true};
    bool isPollerRunning_{false};

    std::unique_ptr<TPollFunctor> pollFunc_;
    std::unique_ptr<TDispatchFunctor> dispatchFunc_;
};

} // namespace futures
} // namespace thousandeyes
