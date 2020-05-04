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

//! \brief A Waitable Interface to represent objects whose wait() method
//! can throw a #WaitableTimedOutException if the object is not ready
//! and the #Waitable object's deadline has been exceeded.
class TimedWaitable : public Waitable {
public:
    //! \brief Creates a Waitable object whose wait() method can throw a
    //! #WaitableTimedOutException if the object is not ready and the
    //! given timeout has passed. In those cases the object's expired
    //! method should return true.
    //!
    //! \param timeout The timeout after which the object is considered
    //! expired.
    explicit TimedWaitable(std::chrono::microseconds timeout) :
        Waitable(toEpochTimestamp(std::chrono::steady_clock::now() + timeout))
    {}

    //! \brief Waits, at most, the given amount of time to determine whether
    //! the object is ready or not.
    //!
    //! \param q The maximum time to wait until determining whether the object
    //! is ready or not.
    //!
    //! \return true if the object is ready and false otherwise.
    //!
    //! \throw #WaitableTimedOutException if not ready and deadline was exceeded.
    //!
    //! \note Objects that inherit the TimedWaitable Interface should override
    //! timedWait() to implement their specific waiting logic.
    //!
    //! \sa timedWait()
    bool wait(const std::chrono::microseconds& q) override final
    {
        if (!expired(toEpochTimestamp(std::chrono::steady_clock::now()))) {
            return timedWait(q);
        }

        if (timedWait(std::chrono::microseconds(0))) {
            return true;
        }

        throw WaitableTimedOutException("Wait limit exceeded");
    }

    //! \brief Waits, at most, the given amount of time to determine whether
    //! the object is ready or not.
    //!
    //! \param timeout The maximum time to wait until determining whether the object
    //! is ready or not.
    //!
    //! \return true if the object is fulfilled and false otherwise.
    //!
    //! \throws std::exception if something exceptional happens during waiting.
    //!
    //! \note If timedWait() returns true, subsequent invocations of timedWait()
    //! should also return true as soon as possible.
    virtual bool timedWait(const std::chrono::microseconds& timeout) = 0;

protected:
    std::chrono::microseconds getTimeout() const
    {
        return timeout(toEpochTimestamp(std::chrono::steady_clock::now()));
    }
};

} // namespace futures
} // namespace thousandeyes
