#pragma once

#include <memory>

namespace thousandeyes {
namespace futures {
class Waitable;
} // namespace futures
} // namespace thousandeyes

namespace thousandeyes {
namespace futures {

//! \brief Interface for the component that is responsible for eventually executing
//! a #Waitable when it becomes ready.
class Executor {
public:
    virtual ~Executor() = default;

    //! \brief Watches the given #Waitable and eventually executes when it is ready.
    //!
    //! \param w The #Waitable instance to monitor and dispatch when ready.
    //!
    //! \note A #Waitable is ready only when its wait() method returns true or
    //! when it throws.
    virtual void watch(std::unique_ptr<Waitable> w) = 0;
};

} // namespace futures
} // namespace thousandeyes
