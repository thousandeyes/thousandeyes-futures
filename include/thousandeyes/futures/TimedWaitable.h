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
#include <string>

#include <thousandeyes/futures/Waitable.h>

namespace thousandeyes {
namespace futures {

//! \brief Exception thrown by the TimedWaitable objects when they time out.
//!
//! \sa TimedWaitable
class WaitableTimedOutException : public WaitableWaitException {
public:
    explicit WaitableTimedOutException(const std::string& error) :
        WaitableWaitException(error)
    {}
};

//! \brief A Waitable Interface to represent objects that can be waited on, that
//! can time-out and, finally, that can get dispatched.
class TimedWaitable : public Waitable {
public:
    //! \brief Creates a TimedWaitable object whose wait() method throws a
    //! WaitableTimedOutException if it has been called with timeouts that add
    //! up to or exceed the object's waitLimit.
    //!
    //! \param waitLimit The maximum time that the wait() method's timeouts
    //! can add up to before the object is considered timed-out.
    explicit TimedWaitable(std::chrono::microseconds waitLimit) :
        waitLimit_(std::move(waitLimit))
    {}

    //! \throws WaitableTimedOutException if the object has timed-out as a result
    //! of multiple invocations of the wait() method with timeouts that add
    //! up to or exceed the object's waitLimit.
    //!
    //! \note Objects that inherit the TimedWaitable Interface should override
    //! timedWait() to implement their specific waiting logic.
    //!
    //! \sa timedWait()
    bool wait(const std::chrono::microseconds& timeout) override final
    {
        if (waitLimit_ <= std::chrono::microseconds(0)) {
            throw WaitableTimedOutException("Wait limit exceeded");
        }

        if (timedWait(timeout)) {
            return true;
        }

        waitLimit_ -= std::max(timeout, std::chrono::microseconds(1));
        return false;
    }

    //! \brief Waits, at most, the given amount of time to determine whether
    //! the object is fulfilled or not.
    //!
    //! \param timeout The maximum time to wait until determining whether the object
    //! is fulfilled or not.
    //!
    //! \return true if the object is fulfilled and false otherwise.
    //!
    //! \throws std::exception if something exceptional happens during waiting.
    //!
    //! \note If timedWait() returns true, subsequent invocations of timedWait()
    //! should also return true as soon as possible.
    virtual bool timedWait(const std::chrono::microseconds& timeout) = 0;

protected:
    std::chrono::microseconds getWaitLimit() const
    {
        return waitLimit_;
    }

private:
    std::chrono::microseconds waitLimit_{ 0 };
};

} // namespace futures
} // namespace thousandeyes
