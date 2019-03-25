#include <functional>
#include <future>
#include <iostream>
#include <numeric>
#include <string>
#include <memory>

#include <thousandeyes/futures/DefaultExecutor.h>
#include <thousandeyes/futures/all.h>
#include <thousandeyes/futures/then.h>
#include <thousandeyes/futures/util.h>

using namespace std;
using namespace std::chrono;
using namespace thousandeyes::futures;

namespace {

future<int> recFunc1(int count);
future<int> recFunc2(future<int> f);

future<int> recFunc1(int count)
{
    cout << string(count, ' ') << "Func1" << endl;

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

    cout << string(count, ' ') << "Func2" << endl;

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

int main(int argc, const char* argv[])
{
    // Use small `q` for polling since use-case creates a large chain of dependent futures
    auto executor = make_shared<DefaultExecutor>(milliseconds(1));
    Default<Executor>::Setter execSetter(executor);

    auto f = recFunc1(0);

    int result = f.get();

    cout << "Got result: " << result << endl;

    executor->stop();
}
