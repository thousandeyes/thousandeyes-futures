#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include <thousandeyes/futures/PollingExecutor.h>
#include <thousandeyes/futures/detail/InvokerWithNewThread.h>
#include <thousandeyes/futures/detail/InvokerWithSingleThread.h>

namespace thousandeyes {
namespace futures {

using DefaultExecutor = PollingExecutor<detail::InvokerWithNewThread,
                                        detail::InvokerWithSingleThread>;

} // namespace futures
} // namespace thousandeyes
