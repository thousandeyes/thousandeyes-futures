#pragma once

#include <chrono>
#include <exception>

namespace thousandeyes {
namespace futures {

//! \brief Interface to represent objects that can be waited on and get
//! dispatched.
class Waitable {
public:
    virtual ~Waitable() = default;

    //! \brief Waits, at most, the given amount of time to determine whether
    //! the object is ready (to be dispatched) or not.
    //!
    //! \param timeout The maximum time to wait until determining whether the object
    //! is ready or not.
    //!
    //! \return true if the object is ready and false otherwise.
    //!
    //! \throws std::exception if something exceptional happens during waiting.
    //!
    //! \note Once wait() returns true, subsequent invocations of wait() should
    //! also return true as soon as possible.
    virtual bool wait(const std::chrono::microseconds& timeout) = 0;

    //! \brief Dispatches the object, setting it to a finished state.
    //!
    //! \note Once the object is set to the "finished" state, no other method of the
    //! interface can be invoked. Subsequent invocation of any other method, including
    //! dispatch() should throw an exception or call std::terminate().
    virtual void dispatch(std::exception_ptr err = nullptr) = 0;
};

} // namespace futures
} // namespace thousandeyes
