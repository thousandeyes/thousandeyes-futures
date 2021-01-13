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

#include <functional>
#include <mutex>
#include <thread>
#include <utility>

namespace thousandeyes {
namespace futures {
namespace detail {

class InvokerWithNewThread {
public:
    void operator()(std::function<void()> f)
    {
        std::thread(std::move(f)).detach();
    }
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes
