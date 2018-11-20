#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <memory>

#include <thousandeyes/futures/DefaultExecutor.h>
#include <thousandeyes/futures/then.h>

using namespace std;
using namespace std::chrono;
using namespace thousandeyes::futures;

namespace {

template<class T>
future<T> getValueAsync(const T& value)
{
    return std::async(std::launch::async, [value]() {
        return value;
    });
}

} // namespace

int main(int argc, const char* argv[])
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

    string result = f.get();

    cout << "Got result: " << result << endl;

    executor->stop();
}
