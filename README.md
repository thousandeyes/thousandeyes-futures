# `thousandeyes::futures`
> Continuations for C++11 std::future<> types

`thousandeyes::futures` is a self-contained, header-only, cross-platform library for attaching continuations to `std::future` types and adapting multiple `std::future` objects into a single `std::future` object.

The library is tested on and should work on the following platforms:
* MacOS with clang 7.0 or newer
* Linux with g++ 5.5.0 or newer
* Windows with VS 2017 or newer

Currently, the library requires a C++14-capable compiler, however, the plan is to target C++11 before open-sourcing it.

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
  * [Conan.io Package](#conanio-package)
  * [Pre-published Conan Package](#pre-published-conan-package)
  * [Custom Conan Package](#custom-conan-package)
* [Performance](#performance)
  * [Methodology](#methodology)
  * [Results](#results)
  * [Discussion](#discussion)
* [Specialized Use Cases](#specialized-use-cases)
  * [Setting and handling the time limit](#setting-and-handling-the-time-limit)
  * [Implementing alternative Executors](#implementing-alternative-executors)
  * [Implementing alternative dispatchers for the PollingExecutor&lt;&gt;](#implementing-alternative-dispatchers-for-the-pollingexecutor)
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

The library provides a simple, default implementation of the `Executor` interface called `DefaultExecutor`. Clients of the library, however, may choose to implement and use their own executor(s) if the provided `DefaultExecutor` is not a good fit for the project (see section ???).

The `DefaultExecutor` uses two `std::thread` threads: one thread to poll all the active `std::future` objects that have continuations attached to them and one that is used to invoke the continuations once the futures become ready. It polls the active `std::future` objects with the timeout given in component's constructor.

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
4. When the total wait time on the input future exceeds the library's `Time Limit` (see section ???)

In the first two cases, `std::future::get()` re-throws the same the exception `E`, whereas in the last two cases it throws a `WaitableWaitException` and its subclass, `WaitableTimedOutException`, respectively.

Calling the `std::future::get()` method of an `std::future` object returned by the `all()` function throws only when the associated `Executor` object is stopped and when the total wait time on an input future exceeds the library's `Time Limit` (see section ???). In these cases, the method throws a `WaitableWaitException` and its subclass, `WaitableTimedOutException`, respectively.

### Stopping executors

Explicitly stopping an `Executor` instance does two things:
1. It cancels all pending wait operations on all the associated input `std::future` objects
2. It sets the `Executor` object into an invalid state where it cannot be used anymore to wait on and dispatch `std::future` objects

The `DefaultExecutor` implementation, used in all the examples above, (a) sets the `WaitableWaitException` on all the pending `std::future` objects associated with it and (b) joins the two threads that are used to poll and dispatch the associated `std::future` objects respectively. For more information about the implementation of the `DefaultExecutor`, see section ???.

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

### Conan.io Package

There are two alternatives for obtaining and using the library via the `conan.io` package manager:
1. Use the published package directly from `conan-center` (TODO: Upload the library there ???)
2. Build, publish and use a `thousandeyes-futures` package from an internal conan server

Regardless of which of the above two alternatives is chosen, the library can be integrated into the existing project using the project's build system and the macros/variables generated by `conan`.

When the configured `conan` generator is `cmake`, the library can be integrated either via the `conan`-generated cmake macros or, alternatively, via the `find_package()` macro that imports the target library with the `thousandeyes::futures` alias:

```cmake
find_package(ThousandEyesFutures)
```

Subsequently, the library can be added as link target to the other `cmake` targets. For example, the following command adds the library as a private dependency to the `my-target` target:

```cmake
target_link_libraries(my-target PRIVATE thousandeyes::futures)
```

#### Pre-published Conan Package

The `thousandeyes::futures` library can be obtained from the `conan-center` remote, simply by requiring it from its stable channel.

If the project uses a `conanfile.py` file, the library can be required as follows:

```python
from conans import ConanFile

class MyConanFile(ConanFile):
    def requirements(self):
        self.requires("thousandeyes-futures/0.1@jgeorgal/stable")
```

If the project uses a `conanfile.txt`, the equivalent statement is the following:

```ini
 [requires]
 thousandeyes-futures/0.1@jgeorgal/stable
```

#### Custom Conan Package

Alternatively the library can be published to an internal `conan.io` server. The first step to doing that is making sure that the internal server is set as a `conan` `remote`.

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

After testing and benchmarking `thousandeyes::futures::then()` implementation against other more specialized implementations that provide more direct approaches to achieve the same result, it seems that the `DefaultExecutor` with a `q` value of 10 ms achieves a good balance between efficient use of resources and raw performance against the other, specialized `then()` implementation.

### Methodology

The proposed, default implementation of `then()`, using the `DefaultExecutor`, was benchmarked against the following alternative implementations:

1. A `blocking_then()` implementation that eagerly calls `future::get()` and blocks to get the result (serves as the baseline for benchmarks)
2. An `unbounded_then()` implenentation that creates a new thread per-invocation that waits for the result via `future::wait()`
3. An `asio_then()` implementation that dispatches a function via `boost::asio::io_context::post()` per-invocation, which, in turn, waits for the result via `future::wait()` and uses 50 threads to run `boost::asio::io_context::run()`
4. The proposed, default polling `then()` implementation that polls all the active futures for completion using only one thread (and zero threads if there are no futures active)

TODO: include here the code for the aforementioned alternative `then()` implementations ???

All the implementations above were executed and timed on MacOS (TODO: Run also on windows ???).

All the benchmark programs used a `boost::asio::io_context`-based thread-pool for the implementation of the `getValueAsync()` function, since macOS caps the number of threads that can be instantiated. On the testing system, after about 2,000 threads the `thread::thread()` constructor throwed an exception with the following message:

> thread constructor failed: Resource temporarily unavailable

### Results

In general, among the aforementioned implementations, `unbounded_then()`, `asio_then()` and `polling_then(q = 10ms)` achieve very similar performance. Among those, however, the `polling_then(q = 10ms)` implementation uses the fewest resources: (at-most) one thread for polling.

The results are sumarized in the table below:

| Implementation            | Result (`total` / `user` / `system` / `%cpu`) |
| ------------------------- | --------------------------------------------- |
| `blocking_then()`         | `15:46.81`/ `0.14s` / `0.19s` / `0%`          |
| `unbounded_then()`        | `00:09.52`/ `0.23s` / `0.24s` / `4%`          |
| `asio_then()`             | `00:09.50`/ `0.04s` / `0.06s` / `1%`          |
| `polling_then(q = 0ms)`   | `00:09.45`/ `1.72s` / `8.12s` / `104%`        |
| `polling_then(q = 1ms)`   | `00:09.48`/ `0.20s` / `0.30s` / `5%`          |
| `polling_then(q = 10ms)`  | `00:09.48`/ `0.06s` / `0.08s` / `1%`          |
| `polling_then(q = 500ms)` | `00:09.53`/ `0.04s` / `0.05s` / `0%`          |

TODO: Run `asio_then()` with more/less threads and benchmark ???

The raw results from timing the aforementioned implementations can be seen below:

1. blocking_then (baseline)
```sh
time ./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test --gtest_also_run_disabled_tests --gtest_filter='FutureTest.DISABLED_benchmarkBlockingThen'
FutureTest.DISABLED_benchmarkBlockingThen (946795 ms)
./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test    0.14s user 0.19s system 0% cpu 15:46.81 total
```

2. unbounded_then
```sh
time ./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test --gtest_also_run_disabled_tests --gtest_filter='FutureTest.DISABLED_benchmarkUnboundedThen'
FutureTest.DISABLED_benchmarkUnboundedThen (9497 ms)
./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test    0.23s user 0.24s system 4% cpu 9.526 total
```

3. asio_then
```sh
time ./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test --gtest_also_run_disabled_tests --gtest_filter='FutureTest.DISABLED_benchmarkThen'
FutureTest.DISABLED_benchmarkThen (9484 ms)
./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test    0.04s user 0.06s system 1% cpu 9.502 total
```

4. polling_then (with q = 0ms)
```sh
time ./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test --gtest_also_run_disabled_tests --gtest_filter='FutureTest.DISABLED_benchmarkPollingThen'
FutureTest.DISABLED_benchmarkPollingThen (9438 ms)
./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test    1.72s user 8.12s system 104% cpu 9.459 total
```

5. polling_then (with q = 1ms)
```sh
time ./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test --gtest_also_run_disabled_tests --gtest_filter='FutureTest.DISABLED_benchmarkPollingThen'
FutureTest.DISABLED_benchmarkPollingThen (9459 ms)
./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test    0.20s user 0.30s system 5% cpu 9.482 total
```

6. polling_then (with q = 10ms)
```sh
time ./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test --gtest_also_run_disabled_tests --gtest_filter='FutureTest.DISABLED_benchmarkPollingThen'
FutureTest.DISABLED_benchmarkPollingThen (9474 ms)
./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test    0.06s user 0.08s system 1% cpu 9.489 total
```

7. polling_then (with q = 50ms)
```sh
time ./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test --gtest_also_run_disabled_tests --gtest_filter='FutureTest.DISABLED_benchmarkPollingThen'
FutureTest.DISABLED_benchmarkPollingThen (9491 ms)
./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test    0.05s user 0.08s system 1% cpu 9.510 total
```

8. polling_then (with q = 100ms)
```sh
time ./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test --gtest_also_run_disabled_tests --gtest_filter='FutureTest.DISABLED_benchmarkPollingThen'
FutureTest.DISABLED_benchmarkPollingThen (9490 ms)
./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test    0.05s user 0.07s system 1% cpu 9.506 total
```

9. polling_then (with q = 200ms)
```sh
time ./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test --gtest_also_run_disabled_tests --gtest_filter='FutureTest.DISABLED_benchmarkPollingThen'
FutureTest.DISABLED_benchmarkPollingThen (9481 ms)
./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test    0.04s user 0.07s system 1% cpu 9.505 total
```

10. polling_then (with q >= 500ms)
```sh
time ./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test --gtest_also_run_disabled_tests --gtest_filter='FutureTest.DISABLED_benchmarkPollingThen'
FutureTest.DISABLED_benchmarkPollingThen (9512 ms)
./buildenterprise/common/unibrow/srctest/Debug/unibrow-test-future-test    0.04s user 0.05s system 0% cpu 9.533 total
```

### Discussion

The theoretical time-to-detect-ready lag, i.e., the time period from the exact moment the future becomes ready until the moment it is dispatched, is `O(q * N)`, where `N` is the number of active futures associated with a specific `Executor` instance. The worst possible delay, asymptotically `q * N`, can happen if the future becomes ready immediately after it is polled and, then, all the other active futures associated with the same `Executor` do not get ready when polled.

As mentioned before, a `q` value of 10 ms seems to be a very good compromise between raw, real-world performance and cpu utilization. In usage scenarios, where there will be only a few hundred `std::futures` active at any given time, the worst possible time-to-detect-ready lag will only be a few seconds. Moreover, the proposed `then()`'s implementation is scalable. In usage scenarios where the number of active futures is an order of magnitute bigger, the active futures can be distributed over many different `Executor` instances.

The way the `thousandeyes::futures` library is currently used in internal projects, even a few seconds of delay is perfectly fine since the main goal is increasing the parallelization potential of the underlying system and not make measurements. The proposed library achieves that goal with very few resources.

Nonetheless, `thousandeyes::futures` should not be used for measuring events via continuations, especially if those measurements need millisecond (or better) accuracy. For example, measurements like the following would not provide the required accuracy:

```c++
Stopwatch<steady_clock> sw;
then(connect(host, port, use_future), [sw] {
    out << "It took " << sw.elapsed() << " to connect to host" << endl;
});
```

## Specialized Use Cases

TODO ???

### Setting and handling the time limit

TODO ???

### Implementing alternative `Executor`s

TODO ???

### Implementing alternative dispatchers for the `PollingExecutor<>`

TODO ???

### Using the library with `boost::asio`

TODO ???

1. Add implementation of `Executor` using `boost::asio::io_context`
2. Add implementation of `PollingExecutor<>` using `boost::asio::io_context`
3. Implement one or more of `boost::asio`'s examples using the library instead of callbacks

### Using iterator adapters

TODO ???

```c++
#include <functional>
#include <future>
#include <string>
#include <memory>

#include <boost/iterator/transform_iterator.hpp>

#include <thousandeyes/futures/DefaultExecutor.h>
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
