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
#include <exception>
#include <utility>

namespace thousandeyes {
namespace futures {

//! \brief Exception thrown by the Waitable objects when there's some error.
//!
//! \sa Waitable
class WaitableWaitException : public std::runtime_error {
public:
    explicit WaitableWaitException(const std::string& reason) : std::runtime_error(reason)
    {}
};

//! \brief Utility function to convert a time-point to an epoch timestamp.
//!
//! \param t The timepoint to convert to an epoch timestamp.
//!
//! \returns the time of milliseconds since the Epoch.
template <class TClock, class TDuration>
std::chrono::milliseconds toEpochTimestamp(const std::chrono::time_point<TClock, TDuration>& t)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch());
}

//! \brief Interface to represent objects that can be waited on,
//! ordered by deadline, expire and, finally, get dispatched.
class Waitable {
public:
    //! \brief Creates a Waitable object with the given deadline.
    //!
    //! \param deadline The deadline after which the object is considered
    //! expired in number of ms since the Epoch.
    explicit Waitable(std::chrono::milliseconds epochDeadline) :
        epochDeadline_(std::move(epochDeadline))
    {}

    Waitable() = default;

    virtual ~Waitable() = default;

    //! \brief Waits, at most, the given amount of time to determine whether
    //! the object is ready or not.
    //!
    //! \param q The maximum time to wait until determining whether the object
    //! is ready or not.
    //!
    //! \return true if the object is ready and false otherwise.
    //!
    //! \throw #WaitableWaitException if an error occurs at/during waiting.
    //!
    //! \note Once wait() returns true, subsequent invocations of wait() should
    //! also return true as soon as possible.
    virtual bool wait(const std::chrono::microseconds& q) = 0;

    //! \brief Dispatches the object, setting it to a finished state.
    //!
    //! \note Once the object is set to the "finished" state, no other method of the
    //! interface can be invoked. Subsequent invocation of any other method, including
    //! dispatch() should throw an exception or call std::terminate().
    virtual void dispatch(std::exception_ptr err = nullptr) = 0;

    //! \brief Compares two #Waitable objects with respect to their deadlines.
    //!
    //! \param other the object to compare to the current one.
    //!
    //! \returns < 0 if the current object has a shorter deadline, > 0
    //! if the current object has a longer deadline and = 0 if the
    //! deadlines are equal.
    inline std::chrono::milliseconds compare(const Waitable& other) const
    {
        return epochDeadline_ - other.epochDeadline_;
    }

    //! \brief Returns the current object's timeout with respect to the given timestamp.
    //!
    //! \param epochTimestamp The current timestamp in number of ms since the Epoch.
    //!
    //! \return the milliseconds until the object's deadline exceeds the given timestamp.
    inline std::chrono::milliseconds timeout(const std::chrono::milliseconds& epochTimestamp) const
    {
        return epochDeadline_ - epochTimestamp;
    }

    //! \brief Checks whether the object's deadline has been exceeded.
    //!
    //! \param epochTimestamp The current timestamp in number of ms since the Epoch.
    //!
    //! \return true if the object's deadline has been exceeded and false otherwise.
    inline bool expired(const std::chrono::milliseconds& epochTimestamp) const
    {
        return epochTimestamp >= epochDeadline_;
    }

private:
    std::chrono::milliseconds epochDeadline_{0};
};

} // namespace futures
} // namespace thousandeyes
