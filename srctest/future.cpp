#include <array>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <thread>
#include <tuple>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thousandeyes/futures/all.h>
#include <thousandeyes/futures/then.h>
#include <thousandeyes/futures/PollingExecutor.h>

using std::array;
using std::bind;
using std::function;
using std::future;
using std::future_status;
using std::promise;
using std::map;
using std::make_shared;
using std::make_unique;
using std::make_tuple;
using std::shared_ptr;
using std::unique_ptr;
using std::exception;
using std::reference_wrapper;
using std::runtime_error;
using std::string;
using std::lock_guard;
using std::mutex;
using std::thread;
using std::tuple;
using std::get;
using std::to_string;
using std::vector;
using std::mt19937;
using std::uniform_int_distribution;
using std::chrono::hours;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::milliseconds;
using std::chrono::microseconds;
using std::chrono::duration_cast;
using std::this_thread::sleep_for;

using thousandeyes::futures::Default;
using thousandeyes::futures::Executor;
using thousandeyes::futures::PollingExecutor;
using thousandeyes::futures::Waitable;
using thousandeyes::futures::then;
using thousandeyes::futures::all;

namespace detail = thousandeyes::futures::detail;

using ::testing::Range;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Test;
using ::testing::TestWithParam;
using ::testing::Throw;
using ::testing::_;

namespace {

class NewThreadInvoker {
public:
    ~NewThreadInvoker()
    {
        lock_guard<mutex> lock(m_);

        for (thread& t: threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void operator()(function<void()> f)
    {
        lock_guard<mutex> lock(m_);

        threads_.push_back(thread(move(f)));
    }

private:
    mutex m_;
    vector<thread> threads_;
};

struct InlineInvoker {
    void operator()(function<void()> f)
    {
        f();
    }
};

class NewThreadPollingExecutor : public PollingExecutor<reference_wrapper<NewThreadInvoker>,
                                                        InlineInvoker> {
public:
    NewThreadPollingExecutor(std::chrono::microseconds q, NewThreadInvoker& t) :
        PollingExecutor(std::move(q), t, InlineInvoker())
    {}
};

class SomeKindOfError : public runtime_error {
public:
    SomeKindOfError() :
        runtime_error("Some Kind Of Error")
    {}
};

template<class T>
future<T> getValueAsync(const T& value)
{
    static mt19937 gen;
    static uniform_int_distribution<int> dist(5, 50000);

    try {
        promise<T> p;
        auto result = p.get_future();

        thread(bind([value](promise<T>& p) {
            sleep_for(microseconds(dist(gen)));
            p.set_value_at_thread_exit(value);
        }, move(p))).detach();

        return result;
    }
    catch (const exception&) {
        // We probably exhasted all the treads - return ready future
        promise<T> np;
        np.set_value(value);
        return np.get_future();
    }
}

template<class T, class TException>
future<T> getExceptionAsync()
{
    try {
        promise<T> p;
        auto result = p.get_future();

       thread(bind([](promise<T>& p) {
            try {
                throw TException();
            }
            catch (...) {
                p.set_exception_at_thread_exit(std::current_exception());
            }
        }, move(p))).detach();

        return result;
    }
    catch (const exception&) {
        // We probably exhasted all the treads - return ready future
        try {
            throw TException();
        }
        catch (...) {
            promise<T> np;
            np.set_exception(std::current_exception());
            return np.get_future();
        }
    }
}

// blocking_then() implementation for comparisons
template<class TIn, class TFunc>
std::future<
    typename std::result_of<
        typename std::decay<TFunc>::type(std::future<TIn>)
    >::type
> blocking_then(std::future<TIn> f, TFunc&& cont)
{
    using TOut = typename std::result_of<
            typename std::decay<TFunc>::type(std::future<TIn>)
        >::type;

    std::promise<TOut> p;

    try {
        f.wait();
        p.set_value(cont(std::move(f)));
    }
    catch (...) {
        p.set_exception(std::current_exception());
    }
    return p.get_future();
}

// unbounded_then() implementation for comparisons
template<class TIn, class TOut, class TFunc>
class FutureWaiter {
public:
    FutureWaiter(std::future<TIn> f,
                 std::promise<TOut> p,
                 TFunc&& cont) :
        f_(std::move(f)),
        p_(std::move(p)),
        cont_(std::move(cont))
    {}

    FutureWaiter(const FutureWaiter& o) = delete;

    FutureWaiter(FutureWaiter&& o) = default;
    FutureWaiter& operator=(FutureWaiter&& o) = default;

    void operator()()
    {
        try {
            f_.wait();
            p_.set_value(cont_(std::move(f_)));
        }
        catch (...) {
            p_.set_exception(std::current_exception());
        }
    }

private:
    std::future<TIn> f_;
    std::promise<TOut> p_;
    TFunc cont_;
};

template<class TIn, class TOut, class TFunc>
class AtThreadExitFutureWaiter {
public:
    AtThreadExitFutureWaiter(std::future<TIn> f,
                             std::promise<TOut> p,
                             TFunc&& cont) :
        f_(std::move(f)),
        p_(std::move(p)),
        cont_(std::move(cont))
    {}

    AtThreadExitFutureWaiter(const AtThreadExitFutureWaiter& o) = delete;

    AtThreadExitFutureWaiter(AtThreadExitFutureWaiter&& o) = default;
    AtThreadExitFutureWaiter& operator=(AtThreadExitFutureWaiter&& o) = default;

    void operator()()
    {
        try {
            f_.wait();
            p_.set_value_at_thread_exit(cont_(std::move(f_)));
        }
        catch (...) {
            p_.set_exception_at_thread_exit(std::current_exception());
        }
    }

private:
    std::future<TIn> f_;
    std::promise<TOut> p_;
    TFunc cont_;
};

template<class TIn, class TFunc>
std::future<
    typename std::result_of<
        typename std::decay<TFunc>::type(std::future<TIn>)
    >::type
> unbounded_then(std::future<TIn> f, TFunc&& cont)
{
    using TOut = typename std::result_of<
            typename std::decay<TFunc>::type(std::future<TIn>)
        >::type;

    std::promise<TOut> p;

    auto result = p.get_future();

    thread(AtThreadExitFutureWaiter<TIn, TOut, TFunc>(std::move(f),
                                                      std::move(p),
                                                      std::move(cont))).detach();
    return result;
}

} // namespace

class FutureTest : public TestWithParam<int> {
protected:
    FutureTest() = default;
};

TEST_F(FutureTest, SetValueAtExitSanityCheck)
{
    auto p = make_shared<promise<int>>();

    thread([p]() {
        p->set_value_at_thread_exit(1821);
    }).detach();

    future<int> f = p->get_future();
    future_status status = f.wait_for(milliseconds(1821));

    ASSERT_EQ(future_status::ready, status);
    EXPECT_EQ(1821, f.get());
}

TEST_F(FutureTest, BlockingThenWithoutException)
{
    auto p = make_shared<promise<int>>();

    thread([p]() {
        p->set_value_at_thread_exit(1821);
    }).detach();

    auto f = blocking_then(p->get_future(), [](future<int> f) {
        return to_string(f.get());
    });

    EXPECT_EQ("1821", f.get());
}

TEST_F(FutureTest, BlockingThenWithExceptionInInputPromise)
{
    auto p = make_shared<promise<int>>();

    thread([p]() {
        try {
            throw SomeKindOfError();
            p->set_value_at_thread_exit(1821);
        }
        catch (...) {
            p->set_exception_at_thread_exit(std::current_exception());
        }
    }).detach();

    auto f = blocking_then(p->get_future(), [](future<int> f) {
        return to_string(f.get());
    });

    EXPECT_THROW(f.get(), SomeKindOfError);
}

TEST_F(FutureTest, BlockingThenWithExceptionInOutputPromise)
{
    auto p = make_shared<promise<int>>();

    thread([p]() {
        p->set_value_at_thread_exit(1821);
    }).detach();

    auto f = blocking_then(p->get_future(), [](future<int> f) {
        throw SomeKindOfError();
        return to_string(f.get());
    });

    EXPECT_THROW(f.get(), SomeKindOfError);
}

TEST_F(FutureTest, UnboundedThenWithoutException)
{
    auto p = make_shared<promise<int>>();

    thread([p]() {
        p->set_value_at_thread_exit(1821);
    }).detach();

    auto f = unbounded_then(p->get_future(), [](future<int> f) {
        return to_string(f.get());
    });

    EXPECT_EQ("1821", f.get());
}

TEST_F(FutureTest, UnboundedThenWithExceptionInInputPromise)
{
    auto p = make_shared<promise<int>>();

    thread([p]() {
        try {
            throw SomeKindOfError();
            p->set_value_at_thread_exit(1821);
        }
        catch (...) {
            p->set_exception_at_thread_exit(std::current_exception());
        }
    }).detach();

    auto f = unbounded_then(p->get_future(), [](future<int> f) {
        return to_string(f.get());
    });

    EXPECT_THROW(f.get(), SomeKindOfError);
}

TEST_F(FutureTest, UnboundedThenWithExceptionInOutputPromise)
{
    auto p = make_shared<promise<int>>();

    thread([p]() {
        p->set_value_at_thread_exit(1821);
    }).detach();

    auto f = unbounded_then(p->get_future(), [](future<int> f) {
        throw SomeKindOfError();
        return to_string(f.get());
    });

    EXPECT_THROW(f.get(), SomeKindOfError);
}

TEST_F(FutureTest, PollingThenWithoutException)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    auto f = then(getValueAsync(1821), [](future<int> f) {
        return to_string(f.get());
    });

    auto g = then(getValueAsync(1822), [](future<int> f) {
        return to_string(f.get());
    });

    EXPECT_EQ("1821", f.get());
    EXPECT_EQ("1822", g.get());
}

TEST_F(FutureTest, PollingIdentityChainingThenWithoutException)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    auto f = then(getValueAsync(1821), [](future<int> f) {
        return f;
    });

    EXPECT_EQ(1821, f.get());
}

TEST_F(FutureTest, PollingChainingThenWithoutException)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    auto f = then(getValueAsync(1821), [](future<int> f) {
        auto first = to_string(f.get());
        return then(getValueAsync(string("1822")), [first](future<string> f) {
            auto second = f.get();
            return then(getValueAsync(1823), [first, second](future<int> f) {
                return first + '_' + second + '_' + to_string(f.get());
            });
        });
    });

    EXPECT_EQ("1821_1822_1823", f.get());
}

TEST_F(FutureTest, PollingThenWithoutExceptionMultipleFutures)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    vector<future<string>> f;
    for (int i = 0; i < 1821; ++i) {
        f.push_back(then(getValueAsync(i), [](future<int> f) {
            return to_string(f.get());
        }));
    }

    for (int i = 0; i < 1821; ++i) {
        EXPECT_EQ(to_string(i), f[i].get());
    }
}

TEST_F(FutureTest, PollingThenWithExceptionInInputPromise)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);
    Default<Executor>::Setter pollerSetter(poller);

    auto p = make_shared<promise<int>>();

    thread([p]() {
        try {
            throw SomeKindOfError();
            p->set_value_at_thread_exit(1821);
        }
        catch (...) {
            p->set_exception_at_thread_exit(std::current_exception());
        }
    }).detach();

    auto f = then(p->get_future(), [](future<int> f) {
        return to_string(f.get());
    });

    EXPECT_THROW(f.get(), SomeKindOfError);
}

TEST_F(FutureTest, PollingChainingThenWithExceptionInInputPromise)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    auto p = make_shared<promise<int>>();

    thread([p]() {
        try {
            throw SomeKindOfError();
            p->set_value_at_thread_exit(1821);
        }
        catch (...) {
            p->set_exception_at_thread_exit(std::current_exception());
        }
    }).detach();

    auto f = then(p->get_future(), [](future<int> f) {
        auto first = to_string(f.get());
        return then(getValueAsync(string("1822")), [first](future<string> f) {
            auto second = f.get();
            return then(getValueAsync(1823), [first, second](future<int> f) {
                return first + '_' + second + '_' + to_string(f.get());
            });
        });
    });

    EXPECT_THROW(f.get(), SomeKindOfError);
}

TEST_F(FutureTest, PollingThenWithExceptionInOutputPromise)
{
    NewThreadInvoker ti;
    Default<Executor>::Setter pollerSetter(
        make_shared<NewThreadPollingExecutor>(milliseconds(10), ti)
    );

    auto p = make_shared<promise<int>>();

    thread([p]() {
        p->set_value_at_thread_exit(1821);
    }).detach();

    auto f = then(p->get_future(), [](future<int> f) {
        throw SomeKindOfError();
        return to_string(f.get());
    });

    EXPECT_THROW(f.get(), SomeKindOfError);
}

TEST_F(FutureTest, PollingChainingThenWithExceptionInOutputPromiseLvl0)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    auto f = then(getValueAsync(1821), [](future<int> f) {
        throw SomeKindOfError();
        auto first = to_string(f.get());
        return then(getValueAsync(string("1822")), [first](future<string> f) {
            auto second = f.get();
            return then(getValueAsync(1823), [first, second](future<int> f) {
                return first + '_' + second + '_' + to_string(f.get());
            });
        });
    });

    EXPECT_THROW(f.get(), SomeKindOfError);
}

TEST_F(FutureTest, PollingChainingThenWithExceptionInOutputPromiseLvl1)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    auto f = then(getValueAsync(1821), [](future<int> f) {
        auto first = to_string(f.get());
        return then(getValueAsync(string("1822")), [first](future<string> f) {
            throw SomeKindOfError();
            auto second = f.get();
            return then(getValueAsync(1823), [first, second](future<int> f) {
                return first + '_' + second + '_' + to_string(f.get());
            });
        });
    });

    EXPECT_THROW(f.get(), SomeKindOfError);
}

TEST_F(FutureTest, PollingChainingThenWithExceptionInOutputPromiseLvl2)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    auto f = then(getValueAsync(1821), [](future<int> f) {
        auto first = to_string(f.get());
        return then(getValueAsync(string("1822")), [first](future<string> f) {
            auto second = f.get();
            return then(getValueAsync(1823), [first, second](future<int> f) {
                throw SomeKindOfError();
                return first + '_' + second + '_' + to_string(f.get());
            });
        });
    });

    EXPECT_THROW(f.get(), SomeKindOfError);
}

TEST_F(FutureTest, PollingThenWithExceptionInOutputPromiseMultipleFutures)
{
    NewThreadInvoker ti;
    Default<Executor>::Setter pollerSetter(
        make_shared<NewThreadPollingExecutor>(milliseconds(10), ti)
    );

    vector<future<string>> f;
    for (int i = 0; i < 1821; ++i) {
        f.push_back(then(getValueAsync(i), [](future<int> f) {
            throw SomeKindOfError();
            return to_string(f.get());
        }));
    }

    for (int i = 0; i < 1821; ++i) {
        EXPECT_THROW(f[i].get(), SomeKindOfError);
    }
}

INSTANTIATE_TEST_CASE_P(DISABLED_BenchmarkPollingThenWithDifferentQ,
                        FutureTest,
                        Range(1, 1000, 10));

TEST_F(FutureTest, pollingContainerAllSum)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    int targetSum = 0;

    vector<future<int>> futures;
    for (int i = 0; i < 1821; ++i) {
        targetSum += i;
        futures.push_back(getValueAsync(i));
    }

    auto f = then(all(move(futures)), [](future<vector<future<int>>> f) {
        int currSum = 0;
        auto futures = f.get();
        for (auto& fi: futures) {
            currSum += fi.get();
        }
        return currSum;
    });

    EXPECT_EQ(targetSum, f.get());
}

TEST_F(FutureTest, PollingContainerViaIteratorsAllSum)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    int targetSum = 0;

    vector<future<int>> futures;
    for (int i = 0; i < 1821; ++i) {
        targetSum += i;
        futures.push_back(getValueAsync(i));
    }

    // WARNING, WARNING, WARNING:
    // vector<future<int>> futures has to stay alive until the all() future becomes ready
    auto f = then(all(futures.begin(), futures.end()),
                  [](future<tuple<vector<future<int>>::iterator,
                                  vector<future<int>>::iterator>> f) {
        int currSum = 0;
        auto range = f.get();
        for (auto iter = get<0>(range); iter != get<1>(range); ++iter) {
            currSum += iter->get();
        }
        return currSum;
    });

    EXPECT_EQ(targetSum, f.get());
}

TEST_F(FutureTest, PollingEmptyContainerAll)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    vector<future<string>> futures;

    auto result = all(move(futures)).get();
    EXPECT_EQ(0U, result.size());
}

TEST_F(FutureTest, PollingContainerAllWithoutException)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    vector<future<string>> futures;
    for (int i = 0; i < 1821; ++i) {
        futures.push_back(then(getValueAsync(i), [](future<int> f) {
            return to_string(f.get());
        }));
    }

    auto f = all(move(futures)).get();

    for (int i = 0; i < 1821; ++i) {
        EXPECT_EQ(to_string(i), f[i].get());
    }
}

TEST_F(FutureTest, PollingEmptyArrayAll)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    array<future<string>, 0> futures;

    auto result = all(move(futures)).get();
    EXPECT_EQ(0U, result.size());
}

TEST_F(FutureTest, PollingArrayAllWithoutException)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    array<future<string>, 1821> futures;
    for (int i = 0; i < 1821; ++i) {
        futures[i] = then(getValueAsync(i), [](future<int> f) {
            return to_string(f.get());
        });
    }

    auto f = all(move(futures)).get();

    for (int i = 0; i < 1821; ++i) {
        EXPECT_EQ(to_string(i), f[i].get());
    }
}

TEST_P(FutureTest, PollingArrayAllWithExceptionWithParam)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    array<future<string>, 1821> futures;
    for (int i = 0; i < 1821; ++i) {

        if (i == GetParam()) {
            futures[i] = getExceptionAsync<string, SomeKindOfError>();
        }
        else {
            futures[i] = then(getValueAsync(i), [](future<int> f) {
                return to_string(f.get());
            });
        }
    }

    auto f = all(move(futures)).get();

    for (int i = 0; i < 1821; ++i) {
        if (i == GetParam()) {
            EXPECT_THROW(f[i].get(), SomeKindOfError);
        }
        else {
            EXPECT_EQ(to_string(i), f[i].get());
        }
    }
}

INSTANTIATE_TEST_CASE_P(PollingArrayAllWithExceptionInNthInputPromise,
                        FutureTest,
                        Range(0, 1821, 100));

TEST_F(FutureTest, PollingTupleAllWithExplicitTupleWithoutException)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    auto t = all(make_tuple(getValueAsync(1821),
                            getValueAsync(string("1822")),
                            getValueAsync(true))).get();

    EXPECT_EQ(get<0>(t).get(), 1821);
    EXPECT_EQ(get<1>(t).get(), "1822");
    EXPECT_EQ(get<2>(t).get(), true);
}

TEST_F(FutureTest, PollingTupleAllWithoutException)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    auto f0 = getValueAsync(1821);
    auto f1 = getValueAsync(string("1822"));
    auto f2 = getValueAsync(true);

    auto t = all(move(f0), move(f1), move(f2)).get();

    EXPECT_EQ(get<0>(t).get(), 1821);
    EXPECT_EQ(get<1>(t).get(), "1822");
    EXPECT_EQ(get<2>(t).get(), true);
}

TEST_F(FutureTest, PollingTupleAllWithContinuationWithoutException)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    auto f0 = getValueAsync(1821);
    auto f1 = getValueAsync(string("1822"));
    auto f2 = getValueAsync(true);

    auto f = then(all(move(f0), move(f1), move(f2)),
                  [](future<tuple<future<int>, future<string>, future<bool>>> f) {
                      future<int> f0;
                      future<string> f1;
                      future<bool> f2;
                      std::tie(f0, f1, f2) = f.get();

                      return to_string(f0.get()) + '_' + f1.get() + '_' +
                        (f2.get() ? "true" : "false");
                  });

    EXPECT_EQ(f.get(), "1821_1822_true");
}

TEST_F(FutureTest, PollingTupleAllWithException)
{
    NewThreadInvoker ti;
    auto poller = make_shared<NewThreadPollingExecutor>(milliseconds(10), ti);

    Default<Executor>::Setter pollerSetter(poller);

    auto t0 = all(getExceptionAsync<int, SomeKindOfError>(),
                  getValueAsync(string("1822")),
                  getValueAsync(true)).get();

    EXPECT_THROW(get<0>(t0).get(), SomeKindOfError);
    EXPECT_EQ(get<1>(t0).get(), "1822");
    EXPECT_EQ(get<2>(t0).get(), true);

    auto t1 = all(getValueAsync(1821),
                  getExceptionAsync<string, SomeKindOfError>(),
                  getValueAsync(true)).get();

    EXPECT_EQ(get<0>(t1).get(), 1821);
    EXPECT_THROW(get<1>(t1).get(), SomeKindOfError);
    EXPECT_EQ(get<2>(t1).get(), true);

    auto t2 = all(getValueAsync(1821),
                  getValueAsync(string("1822")),
                  getExceptionAsync<bool, SomeKindOfError>()).get();

    EXPECT_EQ(get<0>(t2).get(), 1821);
    EXPECT_EQ(get<1>(t2).get(), "1822");
    EXPECT_THROW(get<2>(t2).get(), SomeKindOfError);
}
