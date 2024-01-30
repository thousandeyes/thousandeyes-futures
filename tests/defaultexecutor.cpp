/*
 * Copyright 2019 ThousandEyes, Inc.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 *
 * @author Giannis Georgalis, https://github.com/ggeorgalis
 */

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thousandeyes/futures/all.h>
#include <thousandeyes/futures/DefaultExecutor.h>
#include <thousandeyes/futures/observe.h>
#include <thousandeyes/futures/then.h>
#include <thousandeyes/futures/util.h>

using std::array;
using std::bind;
using std::condition_variable;
using std::exception;
using std::function;
using std::future;
using std::future_status;
using std::get;
using std::lock_guard;
using std::make_shared;
using std::make_tuple;
using std::make_unique;
using std::map;
using std::move;
using std::mt19937;
using std::mutex;
using std::promise;
using std::reference_wrapper;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::thread;
using std::to_string;
using std::tuple;
using std::uniform_int_distribution;
using std::unique_lock;
using std::unique_ptr;
using std::vector;
using std::chrono::duration_cast;
using std::chrono::hours;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::minutes;
using std::chrono::seconds;
using std::this_thread::sleep_for;

using thousandeyes::futures::all;
using thousandeyes::futures::Default;
using thousandeyes::futures::DefaultExecutor;
using thousandeyes::futures::Executor;
using thousandeyes::futures::fromException;
using thousandeyes::futures::fromValue;
using thousandeyes::futures::observe;
using thousandeyes::futures::then;
using thousandeyes::futures::Waitable;
using thousandeyes::futures::WaitableWaitException;

namespace detail = thousandeyes::futures::detail;

using ::testing::_;
using ::testing::UnorderedElementsAre;
using ::testing::Range;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Test;
using ::testing::TestWithParam;
using ::testing::Throw;

namespace {

class SomeKindOfError : public runtime_error {
public:
    SomeKindOfError() : runtime_error("Some Kind Of Error")
    {}
};

future<void> getValueAsync()
{
    return std::async(std::launch::async, []() { sleep_for(milliseconds(1)); });
}

template <class T>
future<T> getValueAsync(const T& value)
{
    static mutex m;
    static mt19937 gen;
    static uniform_int_distribution<int> dist(5, 50000);

    return std::async(std::launch::async, [value]() {
        microseconds delay;
        {
            lock_guard<mutex> lock(m);
            delay = microseconds(dist(gen));
        }

        sleep_for(delay);
        return value;
    });
}

template <class T, class TException>
future<T> getExceptionAsync()
{
    return std::async(std::launch::async, []() {
        throw TException();
        return T{};
    });
}

template <class TException>
future<void> getExceptionAsync()
{
    return std::async(std::launch::async, []() { throw TException(); });
}

} // namespace

class DefaultExecutorTest : public TestWithParam<int> {
protected:
    DefaultExecutorTest() = default;
};

TEST_F(DefaultExecutorTest, SetValueAtExitSanityCheck)
{
    auto p = make_shared<promise<int>>();

    thread([p]() { p->set_value_at_thread_exit(1821); }).detach();

    future<int> f = p->get_future();
    future_status status = f.wait_for(milliseconds(1821));

    ASSERT_EQ(future_status::ready, status);
    EXPECT_EQ(1821, f.get());
}

TEST_F(DefaultExecutorTest, ThenWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAsync(1821), [](future<int> f) { return to_string(f.get()); });

    auto g = then(getValueAsync(1822), [](future<int> f) { return to_string(f.get()); });

    EXPECT_EQ("1821", f.get());
    EXPECT_EQ("1822", g.get());

    executor->stop();
}

TEST_F(DefaultExecutorTest, ObserveWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    mutex mtx;
    condition_variable cv;
    vector<int> result;

    auto recordResult = [&](int num) {
        lock_guard<mutex> lock(mtx);

        result.push_back(num);
        cv.notify_one();
    };

    observe(getValueAsync(1821), [&](future<int> f) { recordResult(f.get()); });

    observe(getValueAsync(1822), [&](future<int> f) { recordResult(f.get()); });

    {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [&] { return result.size() == 2; });
    }

    EXPECT_THAT(result, UnorderedElementsAre(1821, 1822));

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenWithException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getExceptionAsync<int, SomeKindOfError>(),
                  [](future<int> f) { return to_string(f.get()); });

    EXPECT_THROW(f.get(), SomeKindOfError);

    executor->stop();
}

using DefaultExecutorDeathTest = DefaultExecutorTest;

TEST_F(DefaultExecutorDeathTest, ObserveWithException)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");

    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    mutex mtx;
    condition_variable cv;

    // This will never become true, as the continuation will rethrow the exception
    // thrown by the future passed to observe(). It only serves as barrier to block
    // the execution until observe() throws.
    bool success = false;

    EXPECT_DEATH(
        {
            observe(getExceptionAsync<SomeKindOfError>(), [](future<void> f) {
                f.get();

                FAIL() << "As f.get() throws this code should never be executed";
            });

            unique_lock<mutex> lock(mtx);
            cv.wait(lock, [&]{ return success; });
        },
        _);

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenWithVoidInputWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAsync(), [](future<void> f) {
        f.get();
        return string("OK");
    });

    EXPECT_EQ("OK", f.get());

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenWithVoidInputWithException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getExceptionAsync<SomeKindOfError>(), [](future<void> f) {
        EXPECT_THROW(f.get(), SomeKindOfError);
        throw SomeKindOfError();
        return "OK";
    });

    EXPECT_THROW(f.get(), SomeKindOfError);

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenWithVoidOutputWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAsync(1821), [](future<int> f) { EXPECT_EQ(1821, f.get()); });

    EXPECT_NO_THROW(f.get());

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenWithVoidOutputWithException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getExceptionAsync<int, SomeKindOfError>(), [](future<int> f) {
        EXPECT_THROW(f.get(), SomeKindOfError);
        throw SomeKindOfError();
    });

    EXPECT_THROW(f.get(), SomeKindOfError);

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenWithVoidInputAndOutputWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAsync(), [](future<void> f) { EXPECT_NO_THROW(f.get()); });

    EXPECT_NO_THROW(f.get());

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenWithVoidInputAndOutputWithException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getExceptionAsync<SomeKindOfError>(), [](future<void> f) {
        EXPECT_THROW(f.get(), SomeKindOfError);
        throw SomeKindOfError();
    });

    EXPECT_THROW(f.get(), SomeKindOfError);

    executor->stop();
}

TEST_F(DefaultExecutorTest, IdentityChainingThenWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAsync(1821), [](future<int> f) { return f; });

    EXPECT_EQ(1821, f.get());

    executor->stop();
}

TEST_F(DefaultExecutorTest, ForwardingThenWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAsync(), [](future<void> f) { return fromValue(); });

    EXPECT_NO_THROW(f.get());

    executor->stop();
}

TEST_F(DefaultExecutorTest, ForwardingThenWithException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAsync(), [](future<void> f) {
        return fromException<void>(make_exception_ptr(SomeKindOfError{}));
    });

    EXPECT_THROW(f.get(), SomeKindOfError);

    executor->stop();
}

TEST_F(DefaultExecutorTest, ChainingThenWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    executor->stop();
}

TEST_F(DefaultExecutorTest, ChainingWithVoidOutputWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAsync(1821), [](future<int> f) {
        auto first = to_string(f.get());
        return then(getValueAsync(string("1822")), [first](future<string> f) {
            auto second = f.get();
            return then(getValueAsync(1823), [first, second](future<int> f) {});
        });
    });

    EXPECT_NO_THROW(f.get());

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenWithoutExceptionMultipleFutures)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    vector<future<string>> f;
    for (int i = 0; i < 1821; ++i) {
        f.push_back(then(getValueAsync(i), [](future<int> f) { return to_string(f.get()); }));
    }

    for (int i = 0; i < 1821; ++i) {
        EXPECT_EQ(to_string(i), f[i].get());
    }

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenWithExceptionInInputPromise)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    auto f = then(p->get_future(), [](future<int> f) { return to_string(f.get()); });

    EXPECT_THROW(f.get(), SomeKindOfError);

    executor->stop();
}

TEST_F(DefaultExecutorTest, ChainingThenWithExceptionInInputPromise)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenWithExceptionInOutputPromise)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto p = make_shared<promise<int>>();

    thread([p]() { p->set_value_at_thread_exit(1821); }).detach();

    auto f = then(p->get_future(), [](future<int> f) {
        throw SomeKindOfError();
        return to_string(f.get());
    });

    EXPECT_THROW(f.get(), SomeKindOfError);

    executor->stop();
}

TEST_F(DefaultExecutorTest, ChainingThenWithExceptionInOutputPromiseLvl0)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    executor->stop();
}

TEST_F(DefaultExecutorTest, ChainingThenWithExceptionInOutputPromiseLvl1)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    executor->stop();
}

TEST_F(DefaultExecutorTest, ChainingThenWithExceptionInOutputPromiseLvl2)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenWithExceptionInOutputPromiseMultipleFutures)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    executor->stop();
}

TEST_F(DefaultExecutorTest, ContainerAllSum)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    int targetSum = 0;

    vector<future<int>> futures;
    for (int i = 0; i < 1821; ++i) {
        targetSum += i;
        futures.push_back(getValueAsync(i));
    }

    auto f = then(all(move(futures)), [](future<vector<future<int>>> f) {
        int currSum = 0;
        auto futures = f.get();
        for (auto& fi : futures) {
            currSum += fi.get();
        }
        return currSum;
    });

    EXPECT_EQ(targetSum, f.get());

    executor->stop();
}

TEST_F(DefaultExecutorTest, ContainerViaIteratorsAllSum)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    int targetSum = 0;

    vector<future<int>> futures;
    for (int i = 0; i < 1821; ++i) {
        targetSum += i;
        futures.push_back(getValueAsync(i));
    }

    // WARNING, WARNING, WARNING:
    // vector<future<int>> futures has to stay alive until the all() future becomes ready
    auto f =
        then(all(futures.begin(), futures.end()),
             [](future<tuple<vector<future<int>>::iterator, vector<future<int>>::iterator>> f) {
                 int currSum = 0;
                 auto range = f.get();
                 for (auto iter = get<0>(range); iter != get<1>(range); ++iter) {
                     currSum += iter->get();
                 }
                 return currSum;
             });

    EXPECT_EQ(targetSum, f.get());

    executor->stop();
}

TEST_F(DefaultExecutorTest, EmptyContainerAll)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    vector<future<string>> futures;

    auto result = all(move(futures)).get();
    EXPECT_EQ(0U, result.size());

    executor->stop();
}

TEST_F(DefaultExecutorTest, ContainerAllWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    vector<future<string>> futures;
    for (int i = 0; i < 1821; ++i) {
        futures.push_back(then(getValueAsync(i), [](future<int> f) { return to_string(f.get()); }));
    }

    auto f = all(move(futures)).get();

    for (int i = 0; i < 1821; ++i) {
        EXPECT_EQ(to_string(i), f[i].get());
    }

    executor->stop();
}

TEST_F(DefaultExecutorTest, EmptyArrayAll)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    array<future<string>, 0> futures;

    auto result = all(move(futures)).get();
    EXPECT_EQ(0U, result.size());

    executor->stop();
}

TEST_F(DefaultExecutorTest, ArrayAllWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    array<future<string>, 1821> futures;
    for (int i = 0; i < 1821; ++i) {
        futures[i] = then(getValueAsync(i), [](future<int> f) { return to_string(f.get()); });
    }

    auto f = all(move(futures)).get();

    for (int i = 0; i < 1821; ++i) {
        EXPECT_EQ(to_string(i), f[i].get());
    }

    executor->stop();
}

TEST_P(DefaultExecutorTest, ArrayAllWithExceptionWithParam)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    array<future<string>, 1821> futures;
    for (int i = 0; i < 1821; ++i) {
        if (i == GetParam()) {
            futures[i] = getExceptionAsync<string, SomeKindOfError>();
        }
        else {
            futures[i] = then(getValueAsync(i), [](future<int> f) { return to_string(f.get()); });
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

    executor->stop();
}

INSTANTIATE_TEST_SUITE_P(ArrayAllWithExceptionInNthInputPromise,
                         DefaultExecutorTest,
                         Range(0, 1821, 100));

TEST_F(DefaultExecutorTest, TupleAllWithExplicitTupleWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto t =
        all(make_tuple(getValueAsync(1821), getValueAsync(string("1822")), getValueAsync(true)))
            .get();

    EXPECT_EQ(get<0>(t).get(), 1821);
    EXPECT_EQ(get<1>(t).get(), "1822");
    EXPECT_EQ(get<2>(t).get(), true);

    executor->stop();
}

TEST_F(DefaultExecutorTest, TupleOfTwoAllWithSameType)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f0 = getValueAsync(1821);
    auto f1 = getValueAsync(1822);

    auto t = all(move(f0), move(f1)).get();

    EXPECT_EQ(get<0>(t).get(), 1821);
    EXPECT_EQ(get<1>(t).get(), 1822);

    executor->stop();
}

TEST_F(DefaultExecutorTest, TupleAllWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f0 = getValueAsync(1821);
    auto f1 = getValueAsync(string("1822"));
    auto f2 = getValueAsync(true);

    auto t = all(move(f0), move(f1), move(f2)).get();

    EXPECT_EQ(get<0>(t).get(), 1821);
    EXPECT_EQ(get<1>(t).get(), "1822");
    EXPECT_EQ(get<2>(t).get(), true);

    executor->stop();
}

TEST_F(DefaultExecutorTest, TupleAllWithContinuationWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f0 = getValueAsync(1821);
    auto f1 = getValueAsync(string("1822"));
    auto f2 = getValueAsync(true);

    auto f =
        then(all(move(f0), move(f1), move(f2)),
             [](future<tuple<future<int>, future<string>, future<bool>>> f) {
                 future<int> f0;
                 future<string> f1;
                 future<bool> f2;
                 std::tie(f0, f1, f2) = f.get();

                 return to_string(f0.get()) + '_' + f1.get() + '_' + (f2.get() ? "true" : "false");
             });

    EXPECT_EQ(f.get(), "1821_1822_true");

    executor->stop();
}

TEST_F(DefaultExecutorTest, TupleAllWithException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto t0 = all(getExceptionAsync<int, SomeKindOfError>(),
                  getValueAsync(string("1822")),
                  getValueAsync(true))
                  .get();

    EXPECT_THROW(get<0>(t0).get(), SomeKindOfError);
    EXPECT_EQ(get<1>(t0).get(), "1822");
    EXPECT_EQ(get<2>(t0).get(), true);

    auto t1 =
        all(getValueAsync(1821), getExceptionAsync<string, SomeKindOfError>(), getValueAsync(true))
            .get();

    EXPECT_EQ(get<0>(t1).get(), 1821);
    EXPECT_THROW(get<1>(t1).get(), SomeKindOfError);
    EXPECT_EQ(get<2>(t1).get(), true);

    auto t2 = all(getValueAsync(1821),
                  getValueAsync(string("1822")),
                  getExceptionAsync<bool, SomeKindOfError>())
                  .get();

    EXPECT_EQ(get<0>(t2).get(), 1821);
    EXPECT_EQ(get<1>(t2).get(), "1822");
    EXPECT_THROW(get<2>(t2).get(), SomeKindOfError);

    executor->stop();
}

namespace {
future<int> recFunc1(int count);
future<int> recFunc2(future<int> f);

future<int> recFunc1(int count)
{
    auto h = std::async(std::launch::async, [count]() {
        sleep_for(milliseconds(1));
        return count + 1;
    });

    return then(move(h), [](future<int> g) { return recFunc2(move(g)); });
}

future<int> recFunc2(future<int> f)
{
    auto count = f.get();

    if (count == 10) {
        return fromValue(1821);
    }

    auto h = std::async(std::launch::async, []() { sleep_for(milliseconds(1)); });

    return then(move(h), [count](future<void> g) {
        g.get();
        return recFunc1(count);
    });
}

} // namespace

TEST_F(DefaultExecutorTest, MutuallyRecursiveFunctionsCreateDependentFutures)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = recFunc1(0);

    EXPECT_EQ(1821, f.get());

    executor->stop();
}

TEST_F(DefaultExecutorTest, ThenAfterStop)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    executor->stop();

    auto f = then(getValueAsync(1821), [](future<int> f) { return to_string(f.get()); });

    auto g = then(getValueAsync(1822), [](future<int> f) { return to_string(f.get()); });

    EXPECT_THROW(f.get(), WaitableWaitException);
    EXPECT_THROW(g.get(), WaitableWaitException);
}
