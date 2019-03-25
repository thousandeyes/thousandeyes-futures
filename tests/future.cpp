#include <atomic>
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
#include <thousandeyes/futures/util.h>
#include <thousandeyes/futures/DefaultExecutor.h>

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
using thousandeyes::futures::DefaultExecutor;
using thousandeyes::futures::Executor;
using thousandeyes::futures::Waitable;
using thousandeyes::futures::then;
using thousandeyes::futures::all;
using thousandeyes::futures::fromValue;

namespace detail = thousandeyes::futures::detail;

using ::testing::Range;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Test;
using ::testing::TestWithParam;
using ::testing::Throw;
using ::testing::_;

namespace {

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

    return std::async(std::launch::async, [value]() {
        sleep_for(microseconds(dist(gen)));
        return value;
    });
}

template<class T, class TException>
future<T> getExceptionAsync()
{
    return std::async(std::launch::async, []() {
        throw TException();
        return T{};
    });
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

TEST_F(FutureTest, ThenWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAsync(1821), [](future<int> f) {
        return to_string(f.get());
    });

    auto g = then(getValueAsync(1822), [](future<int> f) {
        return to_string(f.get());
    });

    EXPECT_EQ("1821", f.get());
    EXPECT_EQ("1822", g.get());

    executor->stop();
}

TEST_F(FutureTest, IdentityChainingThenWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAsync(1821), [](future<int> f) {
        return f;
    });

    EXPECT_EQ(1821, f.get());

    executor->stop();
}

TEST_F(FutureTest, ChainingThenWithoutException)
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

TEST_F(FutureTest, ThenWithoutExceptionMultipleFutures)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    vector<future<string>> f;
    for (int i = 0; i < 1821; ++i) {
        f.push_back(then(getValueAsync(i), [](future<int> f) {
            return to_string(f.get());
        }));
    }

    for (int i = 0; i < 1821; ++i) {
        EXPECT_EQ(to_string(i), f[i].get());
    }

    executor->stop();
}

TEST_F(FutureTest, ThenWithExceptionInInputPromise)
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
        return to_string(f.get());
    });

    EXPECT_THROW(f.get(), SomeKindOfError);

    executor->stop();
}

TEST_F(FutureTest, ChainingThenWithExceptionInInputPromise)
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

TEST_F(FutureTest, ThenWithExceptionInOutputPromise)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto p = make_shared<promise<int>>();

    thread([p]() {
        p->set_value_at_thread_exit(1821);
    }).detach();

    auto f = then(p->get_future(), [](future<int> f) {
        throw SomeKindOfError();
        return to_string(f.get());
    });

    EXPECT_THROW(f.get(), SomeKindOfError);

    executor->stop();
}

TEST_F(FutureTest, ChainingThenWithExceptionInOutputPromiseLvl0)
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

TEST_F(FutureTest, ChainingThenWithExceptionInOutputPromiseLvl1)
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

TEST_F(FutureTest, ChainingThenWithExceptionInOutputPromiseLvl2)
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

TEST_F(FutureTest, ThenWithExceptionInOutputPromiseMultipleFutures)
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

TEST_F(FutureTest, ContainerAllSum)
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
        for (auto& fi: futures) {
            currSum += fi.get();
        }
        return currSum;
    });

    EXPECT_EQ(targetSum, f.get());

    executor->stop();
}

TEST_F(FutureTest, ContainerViaIteratorsAllSum)
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

    executor->stop();
}

TEST_F(FutureTest, EmptyContainerAll)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    vector<future<string>> futures;

    auto result = all(move(futures)).get();
    EXPECT_EQ(0U, result.size());

    executor->stop();
}

TEST_F(FutureTest, ContainerAllWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    executor->stop();
}

TEST_F(FutureTest, EmptyArrayAll)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    array<future<string>, 0> futures;

    auto result = all(move(futures)).get();
    EXPECT_EQ(0U, result.size());

    executor->stop();
}

TEST_F(FutureTest, ArrayAllWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    executor->stop();
}

TEST_P(FutureTest, ArrayAllWithExceptionWithParam)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    executor->stop();
}

INSTANTIATE_TEST_SUITE_P(ArrayAllWithExceptionInNthInputPromise,
                         FutureTest,
                         Range(0, 1821, 100));

TEST_F(FutureTest, TupleAllWithExplicitTupleWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto t = all(make_tuple(getValueAsync(1821),
                            getValueAsync(string("1822")),
                            getValueAsync(true))).get();

    EXPECT_EQ(get<0>(t).get(), 1821);
    EXPECT_EQ(get<1>(t).get(), "1822");
    EXPECT_EQ(get<2>(t).get(), true);

    executor->stop();
}

TEST_F(FutureTest, TupleOfTwoAllWithSameType)
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

TEST_F(FutureTest, TupleAllWithoutException)
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

TEST_F(FutureTest, TupleAllWithContinuationWithoutException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    executor->stop();
}

TEST_F(FutureTest, TupleAllWithException)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    return then(move(h), [](future<int> g) {
        return recFunc2(move(g));
    });
}

future<int> recFunc2(future<int> f)
{
    auto count = f.get();

    if (count == 10) {
        return fromValue(1821);
    }

    auto h = std::async(std::launch::async, []() {
        sleep_for(milliseconds(1));
    });

    return then(move(h), [count](future<void> g) {
        g.get();
        return recFunc1(count);
    });
}

} // namespace

TEST_F(FutureTest, MutuallyRecursiveFunctionsCreateDependentFutures)
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = recFunc1(0);

    EXPECT_EQ(1821, f.get());

    executor->stop();
}
