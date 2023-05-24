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
#include <memory>
#include <mutex>
#include <thread>

#include <thousandeyes/futures/detail/InvokerWithNewThread.h>
#include <thousandeyes/futures/detail/InvokerWithSingleThread.h>
#include <thousandeyes/futures/PollingExecutor.h>

namespace thousandeyes {
namespace futures {

using DefaultExecutor =
    PollingExecutor<detail::InvokerWithNewThread, detail::InvokerWithSingleThread>;

} // namespace futures
} // namespace thousandeyes
