# `thousandeyes::futures`
> Continuations for C++ std::future<> types

`thousandeyes::futures` is a self-contained, header-only, cross-platform library for attaching continuations to `std::future` types and adapting multiple `std::future` objects into a single `std::future` object.

The library is tested on and should work on the following platforms:
* MacOS with clang 7.0 or newer
* Linux with g++ 5.5.0 or newer
* Windows with VS 2017 or newer

Currently, the library requires a C++14-capable compiler. Nonetheless, it can be ported to C++11 if projects that want to use the library cannot be upgraded to C++14.

## Table of Contents

* [Getting started](#getting-started)
  * [The Executor](#the-executor)
  * [Using the Executor](#using-the-executor)
  * [Attaching continuations](#attaching-continuations)
  * [Getting results](#getting-results)
  * [Stopping executors](#stopping-executors)
* [Motivation](#motivation)
* [Features](#features)
* [Examples](#examples)
  * [Running the Examples](#running-the-examples)
* [Tests](#tests)
  * [Running the Tests](#running-the-tests)
* [Using thousandeyes::futures in Existing Projects](#using-thousandeyesfutures-in-existing-projects)
  * [Cmake](#cmake)
  * [Conan.io package](#conanio-package)
  * [Publishing the library](#publishing-the-library)
* [Performance](#performance)
  * [Comparing to other methods](#comparing-to-other-methods)
  * [Comparing to other Executors](#comparing-to-other-executors)
  * [Discussion](#discussion)
* [Specialized Use Cases](#specialized-use-cases)
  * [Setting and handling timeouts](#setting-and-handling-timeouts)
  * [Implementing alternative executors](#implementing-alternative-executors)
  * [Implementing alternative invokers for the PollingExecutor](#implementing-alternative-invokers-for-the-pollingexecutor)
  * [Using the library with boost::asio](#using-the-library-with-boostasio)
  * [Using iterator adapters](#using-iterator-adapters)
* [Contributing](#contributing)
* [Licensing](#licensing)

## Getting started

The following program attaches a continuation to the future returned by the `getRandomNumber()` function. The attached continuation gets called only when the input future is ready (i.e., the `future<int>::get()` does not block).

```c++
future<int> getRandomNumber()
{
    return std::async(std::launch::async, []() {
        return 4; // chosen by fair dice roll.
                  // guaranteed to be random.
    });
}

int main(int argc, const char* argv[])
{
    // 1. create the executor used for waiting and setting the value on futures.
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));

    // 2. set the executor as the default executor for the current scope.
    Default<Executor>::Setter execSetter(executor);

    // 3. attach a continuation that gets called only when the given future is ready.
    auto f = then(getRandomNumber(), [](future<int> f) {
        return to_string(f.get()); // f is ready - f.get() does not block
    });

    // 4. the resulting future becomes ready when the continuation produces a result.
    string result = f.get(); // result == "4"

    // 5. stop the executor and cancel all pending continuations.
    executor->stop();
}
```

There are five concepts/aspects of the `thousandeyes::futures` library that can be seen in the above example:
1. Creating an `Executor`
2. Setting an `Executor` instance as the scope's default executor
3. Attaching continuations using the `thousandeyes::futures::then()` function
4. Extracting the final value from the resulting, top-level `std::future` object
5. Stopping an `Executor`

### The `Executor`

The `Executor` is the component responsible for waiting on and providing values to the `std::future` objects that are handled by the `thousandeyes::futures` library. Before attaching continuations to `std::future` types, an instance of a concrete implementation of the `Executor` interface has to be created as a shared pointer (`shared_ptr<Executor>`).

The library provides a simple, default implementation of the `Executor` interface called `DefaultExecutor`. Clients of the library, however, may choose to implement and use their own executor(s) if the provided `DefaultExecutor` is not a good fit for the project (see section [Performance](#performance)).

The `DefaultExecutor` uses two `std::thread` threads: one thread that polls all the active `std::future` objects that have continuations attached to them and one that is used to invoke the continuations once the futures become ready. It polls the active `std::future` objects with the timeout given in the component's constructor.

In the above example, the polling timeout is 10 ms, which means that `DefaultExecutor` will wait for 10 ms for each `std::future` to get ready. If a specific `std::future` gets ready before the timeout expires, it gets "dispatched" - meaning that the attached continuation gets invoked (on another thread). If the `std::future` is not ready after the given timeout, the `DefaultExecutor` will start waiting on the next `std::future` object and will come back to the first one after it finishes with all the others (i.e., when it dispaches or when the timeout expires on them).

### Using the `Executor`

When attaching continuations to `std::future` objects, an `Executor` instance has to be specified and be responsible for monitoring and dispatching them.

`Executor` instances can be specified either by passing a `shared_ptr<Executor>` instance as a first argument to the `thousandeyes::futures::then()` function or by setting a `shared_ptr<Executor>` instance as the default `Executor` for the current scope. The latter can be seen in the example above where the `executor` object is passed into the `Default<Executor>::Setter`'s constructor.

Then, the lifetime of the registration of the specified default `Executor` is tied to the lifetime of the `Default<Executor>::Setter` object. When the `Setter` object goes out of scope the default `Executor` is reset back to its previously registered value. This is more clearly shown in the example below.

```c++
future<int> h()
{
    return then(getRandomNumber(), [](future<int> f) {
        return 1821;
    });
}

int main(int argc, const char* argv)
{
    auto e1 = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(e1);

    auto f = then(getRandomNumber(), [](future<int> f) { // uses e1
        return "e1";
    });

    auto g = h(); // then() in "h" uses e1

    auto e2 = make_shared<DefaultExecutor>(milliseconds(1821));
    {
        Default<Executor>::Setter execSetter(e2);

        auto f2 = then(getRandomNumber(), [](future<int> f) { // uses e2
            return 1821;
        });

        auto g2 = h(); // then() in "h" uses e2
    }

    auto k = then(getRandomNumber(), [](future<int> f) { // uses e1 again
        return "e1 again";
    });

    e2.reset(); // e2's lifetime ends here
}
```

### Attaching continuations

Attaching continuations to `std::future` objects is achieved via the `thousandeyes::futures::then()` function. As mentioned above, `then()` can optionally accept a `shared_ptr<Executor>` instance as its first argument. Nonetheless, the main arguments the function accepts are the following:
* The input future object (`std::future`)
* The continuation function that accepts the input future as its argument and can either return a value or an `std::future` of a value

The input future object is passed to the `then()` function by value, which means that from that point onwards `then()` owns the object. Subsequently, the continuation function is invoked by the `Executor` (the `DefaultExecutor` in the examples) only when the input `std::future` object is ready. The ownership of the *ready* input future object is transferred to the continuation function and, thus, it is passed to it by value.

Inside the continuation function, the `std::future::get()` method never blocks, it either returns the stored result immediately or throws the stored exception depending on whether the original input future object became ready with a value or with an exception. If the continuation function's return type is a non-`std::future` type `T`, then the return value of the `then()` function is an `std::future<T>` object that becomes ready with the result of the continuation function or an exception, if the continuation function happens to `throw` when invoked. If the continuation function's return type is an `std::future<U>` type, then the return value of the `then()` function is equivalent to the resulting `std::future<U>` object itself. More specifically, `then()` returns an `std::future<U>` object that becomes ready when the continuation's `std::future<U>` object becomes ready or, if the continuation function happens to `throw` when invoked, it contains the thrown exception.

This allows client code to easily chain `then()` invocations inside arbitrarily nested continuation functions and receive the final result only when all the `std::future` objects that participate in the chain have become ready. This is best shown in the excerpt below:

```c++
auto f = then(getValueAsync(1821), [](future<int> f) {
    auto first = to_string(f.get());
    return then(getValueAsync(string("1822")), [first](future<string> f) {
        auto second = f.get();
        return then(getValueAsync(1823), [first, second](future<int> f) {
            return first + '_' + second + '_' + to_string(f.get());
        });
    });
});

string result = f.get(); // result == "1821_1822_1823"
```

When the intermediate `std::future` objects of a calculation are independent, even when they cannot be determined at compile-time, the `thousandeyes::futures::all()` adapter can be used to create an `std::future` object that gets ready when all the input futures become ready. The resulting `std::future` object, in turn, can be passed into `then()` in order to process and aggregate the individual results. This can be seen in the example below:

```c++
vector<future<int>> futures;
for (int i = 0; i < 1821; ++i) {
    futures.push_back(getValueAsync(i));
}

auto f = then(all(move(futures)), [](future<vector<future<int>>> f) {
    auto futures = f.get();

    return accumulate(futures.begin(), futures.end(), 0, [](int sum, future<int>& f) {
        return sum + f.get();
    });
});

int result = f.get(); // result == 0 + 1 + 2 + ... + 1820
```

In the above example, inside the continuation, both the top-level future (`std::future<vector<std::future<int>>>`) and all the individual futures contained within the `std::vector` of its stored value (`std::future<int>`) are guaranteed to be in the *ready* state. Therefore, all the `std::future:get()` calls above do not block.

### Getting results

As also mentioned in the subsections above, the result of the `then()` function is an `std::future<T>` object whose type `T` depends on the return type of the continuation function. The result of the `all()` function is either a "future of a vector of futures" object (`std::future<std::vector<std::future<T>>>`), when its input is an `std::vector<std::future<T>>` object, or a "future of a tuple of futures" object if its input is a tuple or multiple, variable, arguments.

The potential return types of the `then()` function are summarized in the table below:

| Continuation Return Type  | `then()` Return Type                     |
| ------------------------- | ---------------------------------------- |
| `T`                       | `std::future<T>`                         |
| `std::future<T>`          | `std::future<T>`                         |

The potential return types of the `all()` function are summarized in the table below:

| `all()` Argument Type(s)                          | `all()` Return Type                                            |
| ------------------------------------------------- | -------------------------------------------------------------- |
| `std::vector<std::future<T>>`                     | `std::future<std::vector<std::future<T>>>`                     |
| `std::tuple<std::future<T>, std::future<U>, ...>` | `std::future<std::tuple<std::future<T>, std::future<U>, ...>>` |
| `std::future<T>, std::future<U>, ...`             | `std::future<std::tuple<std::future<T>, std::future<U>, ...>>` |

Calling the `std::future::get()` method of an `std::future` object returned by the `then()` function throws an exception under the following conditions:
1. When the continuation function throws an exception `E` when invoked
2. When the continuation function returns an `std::future` object that becomes ready with an exception `E`
3. When the `Executor` object is stopped
4. When the total wait time on the input future exceeds the library's `Time Limit`

In the first two cases, `std::future::get()` re-throws the same the exception `E`, whereas in the last two cases it throws a `WaitableWaitException` and its subclass, `WaitableTimedOutException`, respectively.

Calling the `std::future::get()` method of an `std::future` object returned by the `all()` function throws only when the associated `Executor` object is stopped and when the total wait time on an input future exceeds the library's `Time Limit` (see section [Setting and handling the time limit](#setting-and-handling-the-time-limit)). In these cases, the method throws a `WaitableWaitException` and its subclass, `WaitableTimedOutException`, respectively.

### Stopping executors

Explicitly stopping an `Executor` instance does two things:
1. It cancels all pending wait operations on all the associated input `std::future` objects
2. It sets the `Executor` object into an invalid state where it cannot be used anymore to wait on and dispatch `std::future` objects

The `DefaultExecutor` implementation, used in all the examples above, (a) sets the `WaitableWaitException` on all the pending `std::future` objects associated with it and (b) joins the two threads that are used to poll and dispatch the associated `std::future` objects respectively. For more information on providing concrete `Executor` implementations, see section [Implementing alternative Executors](#implementing-alternative-executors).

## Motivation

The C++11/14/17 standard library includes the `std::future` type for returning results asynchronously to clients. The API of the standard `std::future` type, however, is very limited and does not provide support for attaching continuations or combining/adapting existing future objects. This limitation makes it very difficult to use `std::future` extensively in projects, since it is tedious and error-prone to effectively parallelize and reuse components that need to consume, transform and combine multiple asynchronous results.

Prior to implementing the `thousandeyes::futures` library, a few existing libraries were evaluated as potential alternatives to `std::future`. Despite their maturity, flexibility and feature-completeness they were not deemed as a good fit for our projects.

First of all, the [`boost::future` type](https://www.boost.org/doc/libs/1_68_0/doc/html/thread/synchronization.html#thread.synchronization.futures) was not a good fit since it relied on the specific implementations of the `boost::thread` library’s components, which, in turn, meant that dependent projects would have to replace standard components such as `std::thread`, `std::mutex`, and `std::chrono` with their boost equivalents.

The [`folly::future` library](https://github.com/facebook/folly/tree/master/folly/futures), created by Facebook, was not deemed a good fit since it relied on many of the other folly sub-libraries and we did not want to add the whole `folly` library as a dependency to our projects.

Finally, other open source future implementations on github.com were deemed not a good fit since they implemented their own, unique future type and were not very mature and well-tested.

## Features

The proposed solution, named `thousandeyes::futures`, is a small, independent header-only library with the following features:
1. Provides all its functionality on top of the standard, widely-supported `std::future` type
2. Does not have any external dependencies
3. Does not need building - client code should only `#include` its headers
4. Is efficient and well-tested
5. Is easy to extend and support many different use-cases
6. Achieves good trade-offs between implementation simplicity, efficient use of resources and time-to-detect-ready lag

## Examples

The library includes a few examples that showcase its use. The source code of the examples is available under the `examples` folder. Some of the examples and excerpts of them are also explained in the sections above.

### Running the Examples

The examples can be compiled with the following commands:

```sh
$ mkdir build
$ cd build
$ cmake -DTHOUSANDEYES_FUTURES_BUILD_EXAMPLES=ON ..
$ cmake --build . --config Debug
```

 Note that `cmake` version `3.11` is required for building the examples.

 Then, the executables of all the examples will be created under the `build/examples/Debug` folder.

## Tests

`thousandeyes::futures` uses [`googletest` and `googlemock`](https://github.com/google/googletest) for its tests. The provided tests assure that the library and its building blocks work correctly under many different usage scenarios. The source code of the tests is available under the `tests` folder.

### Running the Tests

The tests can be compiled with the following commands:

```sh
$ mkdir build
$ cd build
$ cmake -DTHOUSANDEYES_FUTURES_BUILD_TESTS=ON ..
$ cmake --build . --config Debug
```

Note that `cmake` version `3.11` is required for building the tests.

Then, the executables for all the available tests will be created under the `build/tests/Debug` folder. They can invoked (individually) manually or all together, at once via the `ctest` command:

```sh
$ ctest -C Debug -V
```

## Using `thousandeyes::futures` in Existing Projects

The simplest and most direct way to use the library is to copy it into an existing project under, e.g., the `thousandeyes-futures` folder, and then add its `include` sub-folder into the project's include path. E.g.: `-I./thousandeyes-futures/include` or `/I.\thousandeyes-futures\include`.

Moreover, the following sub-sections describe alternative ways to integrate the library into existing projects that either use `cmake` and/or the `conan.io` package manager (`vpkg` coming soon).

### Cmake

In projects using `cmake` the library can be found using the `thousandeyes-futures_ROOT` variable when generating the build files via the `cmake` generate command.

E.g.:

```sh
$ cmake -G Xcode -Dthousandeyes-futures_ROOT=/path/to/thousandeyes-futures -DBOOST_ROOT=${BOOST_ROOT}
```

Alternatively, the library can be copied in a sub-folder (or a git submodule), e.g., under the directory `thousandeyes-futures`, and then added to `cmake` via the `add_directory()` macro in the top-level `CMakeLists.txt` file.

E.g.:

```cmake
add_directory(thousandeyes-futures)
```

In any case, then, the library can be set as a target link library with the alias `thousandeyes::futures`. For example, the following command adds the library as a private dependency to the `my-target` target:

```cmake
target_link_libraries(my-target PRIVATE thousandeyes::futures)
```

### Conan.io package

The library can be built and published into a `conan.io` -compatible repository. Regardless of where it is published, the library can be integrated into the existing project using the project's build system and the macros/variables generated by `conan`.

When the configured `conan` generator is `cmake`, the library can be integrated either via the `conan`-generated cmake macros or, alternatively, via the `find_package()` macro that imports the target library with the `thousandeyes::futures` alias:

```cmake
find_package(ThousandEyesFutures)
```

Subsequently, the library can be added as link target to the other `cmake` targets. For example, the following command adds the library as a public dependency to the `my-target` target:

```cmake
target_link_libraries(my-target PUBLIC thousandeyes::futures)
```

#### Publishing the library

The library can be published to a `conan.io` repository. The first step to doing that is making sure that the internal or external repository is set as a `conan` `remote`.

For example the following command adds the server `http://conanrepo.example.com:9300` as conan remote called `origin`:

```sh
$ conan remote add origin http://conanrepo.example.com:9300
```

Then, the library can be published as the `thousandeyes-futures` package via the `upload` command.

For example the following commands bundle the library as a conan package and publish it to the `origin` remote under the `user/channel` namespace:

```sh
$ cd thousandeyes-futures
$ conan create . user/channel
$ conan upload -r origin thousandeyes-futures/0.1@user/channel
```

Subsequently, the published package can be required from the project via a `conanfile.py`:

```python
from conans import ConanFile

class MyConanFile(ConanFile):
    def requirements(self):
        self.requires("thousandeyes-futures/0.1@user/channel")
```

Or a `conanfile.txt`:

```ini
 [requires]
 thousandeyes-futures/0.1@user/channel
```

## Performance

This subsection includes the results and a discussion about the performance of the default `thousandeyes::futures::then()` implementation. In this context "default implementation" refers to the the implementation of `thousandeyes::futures::then()` using the provided `DefaultExecutor`, which, in turn, is the default, concrete implementation of the `Executor` component based on the `PollingExecutor` strategy.

After testing and benchmarking the default implementation (a) against other, more direct approaches for detecting when the set of active futures become ready and (b) against other implementations of the `Executor` component, it appears that the `DefaultExecutor` with a `q` value of 10 ms achieves a good balance between efficient use of resources and raw performance.

### Comparing to other methods

The proposed, default implementation of `then()`, using the `DefaultExecutor` (`default_then()`), was benchmarked against the following alternative implementations:

1. A `blocking_then()` implementation that eagerly calls `future::get()` and blocks to get the result before moving to the next future (serves as the baseline)
2. An `unbounded_then()` implenentation that creates a new thread per-invocation that waits for the result via `future::wait()`
3. An `asio_then()` implementation that dispatches a function via `boost::asio::io_context::post()` per-invocation, which, in turn, waits for the result via `future::wait()` and uses 50 threads to run `boost::asio::io_context::run()`

Whereas the other approaches (apart from `blocking_then()`) use many threads, the `default_then()` implementation uses at most two threads. One thread for polling all the active futures for completion status and one thread for dispatching the continuations.

Also all the futures that were used for the benchmark were independent - i.e., they did not depend on other futures to complete before completing themselves. The code that benchmarked the different methods was equivalent to the following:

```c++
template<class T>
future<T> getValueAsync(int i)
{
    promise<T> p;
    auto result = p.get_future();

    ioService.post([p=move(p), i]() {
        int g = 500000 - i * 5;
        sleep_for(microseconds(g));
        p.set_value(i);
    });

    return result;
}

void useCase()
{
    vector<future<string>> f;
    for (int i = 0; i < 1900; ++i) {
        f.push_back(then(getValueAsync(i), [](future<int> f) {
            return to_string(f.get());
        }));
    }

    for (int i = 0; i < 1900; ++i) {
        auto result = f[i].get();
        assert(result == to_string(i));
    }
}
```

The testing system was a 2016 MacBook Pro with 2.4 GHz Intel Core i7 CPU and 16 GB 1867 MHz LPDDR3 RAM. All the programs used a `boost::asio::io_context`-based thread-pool with 100 threads for the implementation of the `getValueAsync()` function, since macOS caps the number of threads that can be instantiated. On the testing system, after about 2,000 threads the `thread::thread()` constructor throwed an exception with the message:
> thread constructor failed: Resource temporarily unavailable

In general, among the aforementioned implementations, `unbounded_then()`, `asio_then()` and `default_then(q = 10ms)` exhibit very similar performance characteristics. The results are summarized in the table below:

| Implementation            | Result (`total` / `user` / `system` / `%cpu`) |
| ------------------------- | --------------------------------------------- |
| `blocking_then()`         | `15:46.81`/ `0.14s` / `0.19s` / `0%`          |
| `unbounded_then()`        | `00:09.52`/ `0.23s` / `0.24s` / `4%`          |
| `asio_then()`             | `00:09.50`/ `0.04s` / `0.06s` / `1%`          |
| `default_then(q = 0ms)`   | `00:09.45`/ `1.72s` / `8.12s` / `104%`        |
| `default_then(q = 1ms)`   | `00:09.48`/ `0.20s` / `0.30s` / `5%`          |
| `default_then(q = 10ms)`  | `00:09.48`/ `0.06s` / `0.08s` / `1%`          |
| `default_then(q = 500ms)` | `00:09.53`/ `0.04s` / `0.05s` / `0%`          |

### Comparing to other Executors

The `DefaultExecutor` was subsequently tested against different `Executor` implementations under two different use cases. The first use case, like before, only generates independent futures. The second use case, on the other hand, generates a long chain of interdependent futures where a specific future becomes ready only when all the futures generated after it become ready.

Specifically, the first use case, that utilizes `std::async()` to simulate asynchronous work and sleeps for a time period uniformely distributed in the `[5, 5_000_000]` `usec` interval can be seen below:

```c++
template<class T>
future<T> getValueAsync(const T& value)
{
    static mt19937 gen;
    static uniform_int_distribution<int> dist(5, 5000000);

    return std::async(std::launch::async, [value]() {
        this_thread::sleep_for(microseconds(dist(gen)));
        return value;
    });
}

void usecase0()
{
    vector<future<string>> results;
    for (int i = 0; i < 1900; ++i) {
        results.push_back(then(getValueAsync(i), [](future<int> f) {
            return to_string(f.get());
        }));
    }

    for (int i = 0; i < 1900; ++i) {
        auto result = results[i].get();
        assert(result == to_string(i));
    }
}
```

The second use case generates a long chain of interdependent futures by using two pseudo mutually recursive functions `recFunc1` and `recFunc2`. The first function generates a future that is ready when the future returned from the second function becomes ready, which in turn becomes ready when the future returned by the first function becomes ready. Specifically, the code for the second use case can be seen below:

```c++
bool usecase1()
{
    auto f = recFunc1(0);
    int result = f.get();
    assert(result == 1821);
}

future<int> recFunc1(int count)
{
    auto h = std::async(std::launch::async, [count]() {
        this_thread::sleep_for(milliseconds(1));
        return count + 1;
    });

    return then(move(h), [](future<int> g) {
        return recFunc2(move(g));
    });
}

future<int> recFunc2(future<int> f)
{
    auto count = f.get();

    if (count == 100) {
        return fromValue(1821);
    }

    auto h = std::async(std::launch::async, []() {
        this_thread::sleep_for(milliseconds(1));
    });

    return then(move(h), [count](future<void> g) {
        g.get();
        return recFunc1(count);
    });
}
```

The two `Executor` implementations that were benchmarked against the `DefaultExecutor` were the `BlockingExecutor` (baseline) and the `UnboundedExecutor` (see also section [Implementing alternative Executors](#implementing-alternative-executors)).

Whereas for the first use case (independent futures) the `DefaultExecutor` of any `q != 0` value behaves similarly to `UnboundedExecutor`, the second use case exposes a pathological case for the `DefaultExecutor`. When all active futures are interdependent, `DefaultExecutor` hits its theoretical worst-case performance. This can be easily seen in the table below:

| `Executor` Implementation    | Result 0 (`total` / `user` / `system` / `%cpu`) | Result 1 (`total` / `user` / `system` / `%cpu`) |
| ---------------------------- | ----------------------------------------------- | ----------------------------------------------- |
| `BlockingExecutor`           | `77:31.90`/ `0.20s` / `0.33s` / `0%`            | See `note 1`                                    |
| `UnboundedExecutor`          | `00:5.696`/ `0.50s` / `0.58s` / `18%`           | `00:0.301`/ `0.03s` / `0.06s` / `30%`           |
| `DefaultExecutor(q = 0ms)`   | `00:5.168`/ `5.28s` / `0.17s` / `105%`          | `00:0.283`/ `0.27s` / `0.03s` / `105%`          |
| `DefaultExecutor(q = 1ms)`   | `00:5.141`/ `0.29s` / `0.26s` / `10%`           | `00:26.71`/ `0.40s` / `0.43s` / `3%`            |
| `DefaultExecutor(q = 10ms)`  | `00:5.156`/ `0.23s` / `0.23s` / `8%`            | `03:47.96`/ `0.45s` / `0.46s` / `0%`            |
| `DefaultExecutor(q = 100ms)` | `00:5.252`/ `0.24s` / `0.22s` / `8%`            | `33:52.91`/ `0.50s` / `0.46s` / `0%`            |
| `DefaultExecutor(q = 500ms)` | `00:5.415`/ `0.24s` / `0.23s` / `8%`            | See `note 2`                                    |

* `note 1`: Since the futures are interdependent, the `BlockingExecutor` cannot complete
* `note 2`: It takes too long to finish, about `5 * DefaultExecutor(q = 100ms)`

The full source code used for this benchmark can be seen in `examples/executors.cpp`.

### Discussion

When the active futures are independent, the theoretical time-to-detect-ready lag, i.e., the time period from the exact moment the future becomes ready until the moment it is dispatched, is `q * O(N)`, where `N` is the number of active futures associated with a specific `Executor` instance. The worst possible delay can happen if the future becomes ready immediately after it is polled and, then, all the other active futures associated with the same `Executor` do not get ready when polled.

When the active futures do not complete independently, the theoretical time-to-detect-ready lag of `DefaultExecutor` increases to `q * O(N^2)`, where `N` is the number of active interdependent futures. The second use case of the previous subsection (see [Comparing to other Executors](#comparing-to-other-executors)) achieves the worst possible delay by creating a long chain of active futures where each future depends on the future generated after it.

Regardless of the aforementioned extreme cases, the `DefaultExecutor` with a `q` value of 10 ms appears to be a very good compromise between raw, real-world performance and resource utilization. In typical usage scenarios, where there will be a few hundred `std::future` instances active at any given time, mostly independent, the worst possible time-to-detect-ready lag will only be a few seconds. Moreover, the proposed implementation allows for easily scaling the monitoring and dispatching of the active futures. In usage scenarios where the number of active futures is orders of magnitute bigger, the active futures can be distributed over many different `Executor` instances.

The way the `thousandeyes::futures` library is currently used in internal projects, a few seconds of delay is perfectly fine since the main goal is increasing the parallelization potential of the underlying system and not make measurements. The proposed library achieves that goal with very modest cpu and memory requirements.

Nonetheless, `thousandeyes::futures` should not be used for measuring events via continuations, especially if those measurements need millisecond (or better) accuracy. For example, measurements like the following would not provide the required accuracy when using the `DefaultExecutor`:

```c++
Stopwatch<steady_clock> sw;
then(connect(host, port, use_future), [sw] {
    out << "It took " << sw.elapsed() << " to connect to host" << endl;
});
```

## Specialized Use Cases

The `thousandeyes::futures` library provides overloads for its `then()` and `all()` functions (a) for explicitly specifying the `Executor` instance that will be used to monitor the input `std::future` and dispatch the attached continuation and (b) for setting timeouts after which the library gives up waiting for the input future(s) to become ready.

Moreover, following the performance discussion in the previous section, the library can be extended to support more specialized use cases by either providing completely different concrete implementations of the `Executor` interface or by providing different invokers for the existing `PollingExecutor`.

### Setting and handling timeouts

Both the `thousandeyes::futures::then()` and `thousandeyes::futures::all()` functions accept a `timeLimit` value as their first or second argument, depending on whether the `Executor` is given explicitly or not. This value sets a timeout after which the input (given) future(s) stop being monitored and the resulting (output) future becomes ready with the `thousandeyes::futures::WaitableTimedOutException` exception.

Specifically, the signatures of the functions that include the `timeLimit` parameter are the following:

```c++
future<...> then(std::shared_ptr<Executor> executor, std::chrono::microseconds timeLimit, ...);
future<...> then(std::chrono::microseconds timeLimit, ...);
```

```c++
future<...> all(std::shared_ptr<Executor> executor, std::chrono::microseconds timeLimit, ...);
future<...> all(std::chrono::microseconds timeLimit, ...);
```

Therefore, when a timeout is specified when attaching a continuation to an input `std::future` object, if the latter is not ready after `dt`, where `dt >= timeLimit`, then the resulting (output) `std::future` object will become ready with the library's `WaitableTimedOutException` exception.

This behavior is clearly shown in the example below:

```c++
void timeout0()
{
    future<void> f = sleepAsync(hours(5));

    auto g = then(milliseconds(100), move(f), [](future<void> f){
        f.get(); // This will never get called
    });

    try {
        g.get(); // This will throw a timeout exception
    }
    catch (const WaitableTimedOutException& e) {
        cout << "Got exception: " << e.what() << endl;
    }
}
```

On the other hand, when a timeout is specified at the `all()` adapter, if any of the input `std::future` objects is not ready after `dt`, where `dt >= timeLimit`, then the resulting (output) `std::future` object will become ready with the library's `WaitableTimedOutException` exception.

Again, this behavior is clearly shown in the example below:

```c++
void timeout1()
{
    auto f = all(milliseconds(100), sleepAsync(milliseconds(0)), sleepAsync(hours(5)));

    auto g = then(move(f), [](future<tuple<future<void>, future<void>>> f){
        try {
            f.get(); // This will throw a timeout exception
        }
        catch (const WaitableTimedOutException& e) {
            cout << "Got exception: " << e.what() << endl;
        }
    });

    g.get(); // This will not throw since the exception was handled in the continuation
}
```

If no explicit `timeLimit` is given by the client code, the library's `then()` and `all()` functions implicitly set their `timeLimit` to one hour.

A full example that showcases timeouts when using the library can be found in `examples/timeout.cpp`.

### Implementing alternative executors

As also mentioned in the sections above, the `Executor` is responsible for monitoring all the input futures until they become ready. As soon as the `Executor` detects that a future is ready, it is responsible for dispatching it. All the aforementioned input future objects are adapted by the `then()` and `all()` functions to objects that implement the library's `Waitable` interface.

The `Waitable` interface is defined as follows:

```c++
class Waitable {
public:
    virtual ~Waitable() = default;

    virtual bool wait(const std::chrono::microseconds& timeout) = 0;

    virtual void dispatch(std::exception_ptr err = nullptr) = 0;
};
```

The interface's `wait()` method is equivalent to the `std::future::wait_for()` method and returns `true` if the `Waitable` object is ready for dispatching and `false` otherwise. The `dispatch()` method with a `nullptr` argument is equivalent to the `std::future::set_value()` method, whereas with a non-`nullptr` argument, it is equivalent to the `std::future::set_exception()` method.

Then, an `Executor` receives `Waitable` objects to monitor via its `watch()` method. `Executor` should also define a `stop()` method for suspending its normal operation and dispatching all non-ready `Waitable` objects with a `WaitableWaitException` exception.

Specifically, the interface of the `Executor` component is defined as follows:

```c++
class Executor {
public:
    virtual ~Executor() = default;

    virtual void watch(std::unique_ptr<Waitable> w) = 0;

    virtual void stop() = 0;
};
```

An example of a simple, limited but complete and fully conforming `Executor` is the `BlockingExecutor` which can be implemented as follows:

```c++
class BlockingExecutor : public Executor {
public:
    void watch(std::unique_ptr<Waitable> w)
    {
        try {
           while (active_) {
               if (!w->wait(minutes(1))) {
                   continue;
               }

                w->dispatch();
                return;
           }

           w->dispatch(make_exception_ptr(WaitableWaitException("Executor stoped")));
        }
        catch (...) {
            w->dispatch(current_exception());
        }
    }

    void stop()
    {
        active_ = false;
    }

private:
    atomic<bool> active_{ true };
};
```

The `BlockingExecutor` above has the following characteristics:
1. A shared `BlockingExecutor` object is thread-safe
2. It monitors the given `Waitable` objects until they become ready
3. It dispatches the monitored `Waitable` objects without an error if they do not `throw`
4. It dispatches the monitored `Waitable` objects with an error if an exception is thrown
5. The invocation of its `stop()` method results in (eventually) halting the monitoring of the `Waitable` objects and dispatching them with the `WaitableWaitException` exception

The `UnboundedExecutor`, below, has the same characteristics:

```c++
class UnboundedExecutor : public Executor {
public:
    void watch(std::unique_ptr<Waitable> w)
    {
        lock_guard<mutex> lock(threadListMutex_);

        threads_.emplace_back([this, w=move(w)]() {
            try {
                while (active_) {
                    if (!w->wait(minutes(1))) {
                        continue;
                    }

                    w->dispatch();
                    return;
                }

                w->dispatch(make_exception_ptr(WaitableWaitException("Executor stoped")));
            }
            catch (...) {
                w->dispatch(current_exception());
            }
        });
    }

    void stop()
    {
        active_ = false;

        lock_guard<mutex> lock(threadListMutex_);

        for (auto& t: threads_) {
            if (t.joinable() && t.get_id() != this_thread::get_id()) {
                t.join();
            }
        }
    }

private:
    atomic<bool> active_{ true };

    mutex threadListMutex_;
    vector<thread> threads_;
};
```

Aparently, the `UnboundedExecutor` is a much more powerful executor, able to handle more complex `std::future` dependencies (see [Comparing to other Executors](#comparing-to-other-executors)) at the expense of utilizing more resources.

A full example that implements and uses the executors above can be found in `examples/executors.cpp`.

### Implementing alternative invokers for the `PollingExecutor`

The library's `DefaultExecutor` is based on the `PollingExecutor` strategy, which is implemented as a template class with the actual functors that invoke the polling and dispatching threads as template parameters.

The `PollingExecutor` is defined as follows:

```c++
template<class TPollFunctor, class TDispatchFunctor>
class PollingExecutor : public Executor {
public:
    PollingExecutor(std::chrono::microseconds q);

    PollingExecutor(std::chrono::microseconds q,
                    TPollFunctor&& pollFunc,
                    TDispatchFunctor&& dispatchFunc);
};
```

Then, the `DefaultExecutor`, used in all the examples and tests within the `thousandeyes::futures` library, is defined as follows:

```c++
using DefaultExecutor = PollingExecutor<detail::InvokerWithNewThread,
                                        detail::InvokerWithSingleThread>;
```

As seen in section [Performance](#performance), the `DefaultExecutor` with a reasonable `q` parameter, achieves very good real-world performance with very low resource utilization. The invokers, used by the `DefaultExecutor`, however, may not be appropriate for every project, especially when a project is using its own thread-pools or uses the thread-pools of a third-party library. In those cases, the `PollingExecutor` can be integrated via a custom implementation of the `Invoker` concept.

An `Invoker` implementation must be a functor with `start()` and `stop()` methods like below:

```c++
struct Invoker {
    void start();
    void stop();
    void operator()(std::function<void()> f);
};
```

A real world `Invoker` that enables the `PollingExecutor` to use `boost::asio`-based thread-pools can be simply defined as follows:

```c++
class AsioInvoker {
public:
    explicit AsioInvoker(boost::asio::io_service& ioService) :
        ioService_(ioService)
    {}

    inline void start() {}

    inline void stop() {}

    inline void operator()(std::function<void()> f)
    {
        ioService_.post(std::move(f));
    }

private:
    boost::asio::io_service& ioService_;
};
```

The way the `AsioInvoker` above can be used by the `PollingExecutor` is shown in a complete example in the next subsection (see [Using the library with boost::asio](#using-the-library-with-boostasio)).

The implementation of the invokers used by the `DefaultExecutor` can be seen in the following source files:
* `detail/InvokerWithNewThread.h`
* `detail/InvokerWithSingleThread.h`

### Using the library with `boost::asio`

As mentioned before, the library's `PollingExecutor` can be easily extended to use other third party threads and thread-pools for the polling the input futures and invoking the continuations.

This is the full implementation of the `DefaultPollingExecutor` that uses `boost::asio::io_service`-based thread-pool to start the monitoring thread and dispatch the continuations:

```c++
#pragma once

#include <chrono>
#include <functional>
#include <utility>

#include <boost/asio/io_service.hpp>

#include <thousandeyes/futures/PollingExecutor.h>

class AsioInvoker {
public:
    explicit AsioInvoker(boost::asio::io_service& ioService) :
        ioService_(ioService)
    {}

    inline void start() {}

    inline void stop() {}

    inline void operator()(std::function<void()> f)
    {
        ioService_.post(std::move(f));
    }

private:
    boost::asio::io_service& ioService_;
};

class DefaultPollingExecutor :
    public thousandeyes::futures::PollingExecutor<AsioInvoker, AsioInvoker> {
public:
    DefaultPollingExecutor(std::chrono::microseconds q,
                           boost::asio::io_service& pollingIoService,
                           boost::asio::io_service& dispatchIoService) :
        PollingExecutor(std::move(q),
                        detail::AsioInvoker(pollingIoService),
                        detail::AsioInvoker(dispatchIoService))
    {}
};
```

Moreover, `boost::asio` can return `std::future` objects from all its asynchronous operations by using the `boost::asio::use_future` directive. In the example below, both `t` and `endpoints` are `std::future<>` objects:

```c++
    boost::asio::io_service io;

    boost::asio::steady_timer timer(io);

    timer.expires_after(seconds(1));
    future<void> t = timer.async_wait(boost::asio::use_future);

    boost::asio::udp::resolver resolver(io);

    auto endpoints = resolver.async_resolve(boost::asio::udp::v4(),
                                            "host.example.com",
                                            "daytime",
                                            boost::asio::use_future);
```

A more complete example is provided by `boost::asio` at the link below:
https://www.boost.org/doc/libs/1_66_0/doc/html/boost_asio/example/cpp11/futures/daytime_client.cpp

Of course, all `std::future<>` objects, returned by `boost::asio` methods and functions can be used by `thousandeyes::futures` via its `then()` and `all()` functions.

### Using iterator adapters

In certain use-cases it may be impossible to move the ownership of a container of futures to the library. Furthermore a container may want to encapsulate `std::future` objects as members in its internal structures. For those cases, the `thousandeyes::futures::all()` function has an overload that accepts iterators to futures as arguments.

Then, iterator adapters can be used to adapt iterators to internal structures to iterators to futures as required by the library. The complete example below, which uses the `boost::iterator::transform_iterator` type shows how everything must be set up for these particular use cases:

```c++
#include <functional>
#include <future>
#include <string>
#include <memory>

#include <boost/iterator/transform_iterator.hpp>

#include <thousandeyes/futures/DefaultExecutor.h>
#include <thousandeyes/futures/all.h>
#include <thousandeyes/futures/then.h>

using boost::transform_iterator;
using boost::make_transform_iterator;

using namespace std;
using namespace std::chrono;

using namespace thousandeyes::futures;

template<class T>
future<T> getValueAsync(const T& value)
{
    return std::async(std::launch::async, [value]() {
        return value;
    });
}

int main(int argc, const char* argv[])
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    typedef tuple<string, future<int>, string, bool> FancyStruct;

    int targetSum = 0;

    vector<FancyStruct> fancyStructs;
    for (int i = 0; i < 1821; ++i) {
        targetSum += i;
        fancyStructs.push_back(make_tuple("A user-friendly name",
                                          getValueAsync(i),
                                          "a-usr-friednly-id-" + to_string(i),
                                          false));
    }

    auto convert = [](const FancyStruct& s) -> const future<int>& {
        return get<1>(s);
    };

    typedef transform_iterator<function<const future<int>&(const FancyStruct&)>,
                               vector<FancyStruct>::iterator> Iter;

    Iter first(fancyStructs.begin(), convert);
    Iter last(fancyStructs.end(), convert);

    // Note: vector<FancyStruct> has to stay alive until the all() future becomes ready
    auto f = then(all(first, last), [](future<tuple<Iter, Iter>> f) {
        int sum = 0;
        auto range = f.get();
        for (auto iter = get<0>(range); iter != get<1>(range); ++iter) {
            const auto& baseIter = iter.base();
            int i = get<1>(*baseIter).get();
            sum += i;
        }
        return sum;
    });

    int result = f.get(); // result == 0 + 1 + 2 + ... + 1820

    executor->stop();
}
```

## Contributing

If you'd like to contribute, please fork the repository and use a feature branch. Pull requests are welcome.

## Licensing

The code in this project is licensed under the MIT license.
