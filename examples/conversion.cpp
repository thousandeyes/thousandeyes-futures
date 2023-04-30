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
#include <memory>
#include <string>

#include <thousandeyes/futures/DefaultExecutor.h>
#include <thousandeyes/futures/then.h>

using namespace std;
using namespace std::chrono;
using namespace thousandeyes::futures;

namespace {

template <class T>
future<T> getValueAsync(const T& value)
{
    return async(launch::async, [value]() { return value; });
}

} // namespace

int main(int argc, const char* argv[])
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    auto f = then(getValueAsync(1821), [](future<int> f) { return to_string(f.get()); });

    string result = f.get();

    cout << "Got result: " << result << endl;

    executor->stop();
}
