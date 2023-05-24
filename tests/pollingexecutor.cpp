/*
 * Copyright 2019 ThousandEyes, Inc.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 *
 * @author Giannis Georgalis, https://github.com/ggeorgalis
 */

#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thousandeyes/futures/PollingExecutor.h>
#include <thousandeyes/futures/TimedWaitable.h>
#include <thousandeyes/futures/Waitable.h>

using std::function;
using std::make_shared;
using std::make_unique;
using std::move;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::weak_ptr;
using std::chrono::duration_cast;
using std::chrono::hours;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::minutes;
using std::chrono::seconds;

using thousandeyes::futures::PollingExecutor;
using thousandeyes::futures::TimedWaitable;
using thousandeyes::futures::Waitable;
using thousandeyes::futures::WaitableTimedOutException;

using ::testing::_;
using ::testing::AtLeast;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Test;
using ::testing::Throw;

namespace {

class WaitableMock : public Waitable {
public:
    MOCK_METHOD1(wait, bool(const std::chrono::microseconds& timeout));

    MOCK_METHOD1(dispatch, void(std::exception_ptr err));
};

class Invoker {
public:
    MOCK_METHOD1(invoke, void(function<void()> f));
};

class DispatcherFunctor {
public:
    explicit DispatcherFunctor(shared_ptr<Invoker> invoker) : invoker_(move(invoker))
    {}

    void operator()(function<void()> f)
    {
        invoker_->invoke(move(f));
    }

private:
    shared_ptr<Invoker> invoker_;
};

class Executor : public PollingExecutor<DispatcherFunctor, DispatcherFunctor> {
public:
    Executor(milliseconds q, shared_ptr<Invoker> d) :
        PollingExecutor(move(q), DispatcherFunctor(d), DispatcherFunctor(d))
    {}
};

} // namespace

class PollingExecutorTest : public Test {
public:
    PollingExecutorTest() :
        invoker_(make_shared<Invoker>()),
        poller_(make_shared<Executor>(milliseconds(10), invoker_))
    {}

protected:
    shared_ptr<Invoker> invoker_;
    shared_ptr<Executor> poller_;
};

TEST_F(PollingExecutorTest, DispatchWaitable)
{
    auto waitable = make_unique<WaitableMock>();

    EXPECT_CALL(*waitable, wait(microseconds(10000))).WillOnce(Return(true));

    EXPECT_CALL(*waitable, dispatch(IsNull())).Times(1);

    function<void()> f, g;
    EXPECT_CALL(*invoker_, invoke(_)).WillOnce(SaveArg<0>(&f)).WillOnce(SaveArg<0>(&g));

    poller_->watch(move(waitable));

    f(); // Poll
    g(); // Dispatch
}

TEST_F(PollingExecutorTest, ThrowingWaitable)
{
    auto waitable = make_unique<WaitableMock>();

    EXPECT_CALL(*waitable, wait(microseconds(10000))).WillOnce(Throw(runtime_error("Oops!")));

    EXPECT_CALL(*waitable, dispatch(NotNull())).Times(1);

    function<void()> f, g;
    EXPECT_CALL(*invoker_, invoke(_)).WillOnce(SaveArg<0>(&f)).WillOnce(SaveArg<0>(&g));

    poller_->watch(move(waitable));

    f(); // Poll
    g(); // Dispatch
}
