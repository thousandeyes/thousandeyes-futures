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

#include <memory>
#include <mutex>

namespace thousandeyes {
namespace futures {

//! \brief Component for setting default shared pointer instances that have
//! a well-defined lifetime.
//!
//! \note The lifetime of default instances is determined by the lifetime of
//! the respective Default<>::Setter instances that are meant to be allocated
//! on the stack.
template<class T>
class Default {
public:
    struct Setter {
        Setter(std::shared_ptr<T> instance) :
            prevInstance_(std::move(instance))
        {
            std::lock_guard<std::mutex> lock(mutex_);
            defaultInstance_.swap(prevInstance_);
        }

        ~Setter()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            defaultInstance_.swap(prevInstance_);
        }

        Setter(const Setter&) = delete;
        Setter(Setter&&) = delete;
        Setter& operator= (const Setter&) = delete;
        Setter& operator= (Setter&&) = delete;

    private:
        std::shared_ptr<T> prevInstance_;
    };

    //! \brief Obtains the current default shared pointer instance that was
    //! previously set via the instantiation of a Default<>::Setter.
    //!
    //! \return The current default shared pointer instance.
    operator std::shared_ptr<T>() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return defaultInstance_;
    }

private:
    static std::mutex mutex_;
    static std::shared_ptr<T> defaultInstance_;
};

template<class T>
std::mutex Default<T>::mutex_;

template<class T>
std::shared_ptr<T> Default<T>::defaultInstance_;

} // namespace futures
} // namespace thousandeyes
