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
#include <memory>

#include <thousandeyes/futures/DefaultExecutor.h>
#include <thousandeyes/futures/all.h>
#include <thousandeyes/futures/then.h>

using namespace std;
using namespace std::chrono;
using namespace thousandeyes::futures;

namespace {

template<class T>
future<T> getValueAsync(const T& value)
{
    return async(launch::async, [value]() {
        return value;
    });
}

} // namespace

int main(int argc, const char* argv[])
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

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

    int result = f.get();

    cout << "Got result: " << result << endl;

    executor->stop();
}
