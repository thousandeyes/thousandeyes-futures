#include <atomic>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <thousandeyes/futures/Executor.h>
#include <thousandeyes/futures/DefaultExecutor.h>
#include <thousandeyes/futures/then.h>
#include <thousandeyes/futures/util.h>

using namespace std;
using namespace std::chrono;
using namespace thousandeyes::futures;

// --- Executors --- //

namespace executors {

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

// Note that the reference implementation of `UnboundedExecutor2` executor is provided
// here for completeness and does NOT work, unless we we extend the Waitable interface
// to include an extra `dispatchAtThreadExit()` method. Currently, we haven't included
// that method since using detached threads is not a very good idea anyway.
class UnboundedExecutor2 : public Executor {
public:
    void watch(std::unique_ptr<Waitable> w)
    {
        if (!active_) {
            w->dispatch(make_exception_ptr(WaitableWaitException("Executor stoped")));
            return;
        }

        thread([w=move(w)]() {

            try {
                while (!w->wait(hours(1))) {}
                w->dispatch(); // dispatchAtThreadExit()
            }
            catch (...) {
                w->dispatch(current_exception()); // dispatchAtThreadExit()
            }
        }).detach();
    }

    void stop()
    {
        active_ = false;

        // ¯\_(ツ)_/¯
    }

private:
    atomic<bool> active_{ true };
};

} // namespace executors

// --- Private utility functions --- //

namespace {

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

template<class T, class TException>
future<T> getExceptionAsync()
{
    return std::async(std::launch::async, []() {
        throw TException();
        return T{};
    });
}

future<int> recFunc1(int count);
future<int> recFunc2(future<int> f);

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

} // namespace

// --- Use cases that utilize thousandeyes-futures --- //

bool usecase0()
{
    vector<future<string>> results;
    for (int i = 0; i < 1900; ++i) {
        results.push_back(then(getValueAsync(i), [](future<int> f) {
            return to_string(f.get());
        }));
    }

    for (int i = 0; i < 1900; ++i) {
        if (to_string(i) != results[i].get()) {
            return false;
        }
    }

    return true;
}

bool usecase1()
{
    auto f = recFunc1(0);

    int result = f.get();

    if (result != 1821) {
        return false;
    }

    return true;
}

// --- main --- //

void runUseCases(const string& useCaseName)
{
    map<string, function<bool()>> allUseCases{
        { "0", &usecase0 },
        { "1", &usecase1 },
    };

    if (useCaseName == "all") {
        for (auto& e: allUseCases) {
            cout << "Runing use case \"" << e.first << "\" ... " << flush;
            auto ok = e.second();
            cout << (ok ? "OK" : "ERROR") << endl;
        }
        return;
    }

    if (auto f = allUseCases[useCaseName]) {
        cout << "Runing use case \"" << useCaseName << "\" ... " << flush;
        auto ok = f();
        cout << (ok ? "OK" : "ERROR") << endl;
        return;
    }

    cerr << "Non-existent use case: " << useCaseName << endl;
}

int main(int argc, const char* argv[])
{
    map<string, shared_ptr<Executor>> allExecutors{
        { "blocking", make_shared<executors::BlockingExecutor>() },
        { "unbounded", make_shared<executors::UnboundedExecutor>() },
        { "default0", make_shared<DefaultExecutor>(milliseconds(0)) },
        { "default1", make_shared<DefaultExecutor>(milliseconds(1)) },
        { "default10", make_shared<DefaultExecutor>(milliseconds(10)) },
        { "default100", make_shared<DefaultExecutor>(milliseconds(100)) },
        { "default500", make_shared<DefaultExecutor>(milliseconds(500)) },
    };

    string executorName = "all";
    string useCaseName = "all";

    if (argc >= 2) {
        executorName = argv[1];
    }

    if (argc >= 3) {
        useCaseName = argv[2];
    }

    if (executorName == "all") {
        for (auto& e: allExecutors) {

            // Skip blocking executor, unless requested explicitly
            if (e.first == "blocking") {
                continue;
            }

            Default<Executor>::Setter execSetter(e.second);
            cout << "- Using executor: " << e.first << endl;
            runUseCases(useCaseName);
        }
    }
    else if (auto exec = allExecutors[executorName]) {
        Default<Executor>::Setter execSetter(exec);
        cout << "- Using executor: " << executorName << endl;
        runUseCases(useCaseName);
    }
    else {
        cerr << "- Non-existent executor: " << executorName << endl;
    }

    for (auto& e: allExecutors) {
        if (e.second) {
            e.second->stop();
        }
    }
}

// System: Darwin Kernel Version 18.2.0: root:xnu-4903.241.1~1/RELEASE_X86_64 x86_64
// Results when running time ./a.out executor usecase for every combination:
//
// blocking, 0: 0.20s user 0.33s system 0% cpu 1:17:31.90 total
// blocking, 1: Cannot run with blocking executor because futures depend on each other
//
// unbounded, 0: 0.50s user 0.58s system 18% cpu 5.696 total
// unbounded, 1: 0.03s user 0.06s system 30% cpu 0.301 total
//
// default0, 0: 5.28s user 0.17s system 105% cpu 5.168 total
// default0, 1: 0.27s user 0.03s system 105% cpu 0.283 total
//
// default1, 0: 0.29s user 0.26s system 10% cpu 5.141 total
// default1, 1: 0.40s user 0.43s system 3% cpu 26.714 total
//
// default10, 0: 0.23s user 0.23s system 8% cpu 5.156 total
// default10, 1: 0.45s user 0.46s system 0% cpu 3:47.96 total
//
// default100, 0: 0.24s user 0.22s system 8% cpu 5.252 total
// default100, 1: 0.50s user 0.46s system 0% cpu 33:52.91 total
//
// default500, 0: 0.24s user 0.23s system 8% cpu 5.415 total
// default500, 1: (probably 5 X 33:52.91 total)
