/*
 * Copyright 2019 ThousandEyes, Inc.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 *
 * @author Giannis Georgalis, https://github.com/ggeorgalis
 */

#include <functional>
#include <future>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <memory>

#include <thousandeyes/futures/DefaultExecutor.h>
#include <thousandeyes/futures/all.h>
#include <thousandeyes/futures/then.h>

using namespace std;
using namespace std::chrono;
using namespace thousandeyes::futures;

namespace {

template<class T>
future<T> getValueAfter(const T& value, const milliseconds& t)
{
    promise<T> p;
    auto result = p.get_future();

    // Note: ~std::future of futures returned by std::async() may block until ready
    thread([p=move(p), value, t]() mutable {
        this_thread::sleep_for(t);
        p.set_value_at_thread_exit(value);
    }).detach();

    return result;
}

} // namespace

int main(int argc, const char* argv[])
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAfter(1820, milliseconds(0)), [](future<int> f) {
        return f.get() + 1;
    });

    auto g = all(move(f), getValueAfter(1820, hours(2)));

    auto h = then(milliseconds(100), move(g), [](future<tuple<future<int>, future<int>>> f){
        cout << "This will never get called" << endl;
    });

    try {
        h.get();
        cout << "This will never get printed" << endl;
    }
    catch (const WaitableTimedOutException& e) {
        cout << "Got exception: " << e.what() << endl;
    }

    auto j = all(milliseconds(100),
                 getValueAfter(1820, hours(2)),
                 getValueAfter(1820, milliseconds(1)));

    auto k = then(move(j), [](future<tuple<future<int>, future<int>>> f){
        try {
            auto result = f.get();
            cout << "This will never get printed" << endl;
        }
        catch (const WaitableTimedOutException& e) {
            cout << "Got exception: " << e.what() << endl;
        }
    });

    k.get();

    executor->stop();
}
