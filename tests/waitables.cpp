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
using std::shared_ptr;
using std::unique_ptr;
using std::weak_ptr;
using std::string;
using std::runtime_error;
using std::chrono::hours;
using std::chrono::minutes;
using std::chrono::seconds;
using std::chrono::milliseconds;
using std::chrono::microseconds;
using std::chrono::duration_cast;

using thousandeyes::futures::PollingExecutor;
using thousandeyes::futures::Waitable;
using thousandeyes::futures::WaitableTimedOutException;
using thousandeyes::futures::TimedWaitable;

using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Test;
using ::testing::Throw;
using ::testing::_;

namespace {

class WaitableMock : public Waitable {
public:
    MOCK_METHOD1(wait, bool(const std::chrono::microseconds& timeout));

    MOCK_METHOD1(dispatch, void(std::exception_ptr err));
};

class TimedWaitableMock : public TimedWaitable {
public:
    explicit TimedWaitableMock(microseconds timeout) :
        TimedWaitable(move(timeout))
    {}

    MOCK_METHOD1(timedWait, bool(const std::chrono::microseconds& timeout));

    MOCK_METHOD1(dispatch, void(std::exception_ptr err));
};

class Invoker {
public:
    MOCK_METHOD1(invoke, void(function<void()> f));
};

class DispatcherFunctor {
public:
    explicit DispatcherFunctor(shared_ptr<Invoker> invoker) :
        invoker_(move(invoker))
    {}

    void start() {}

    void stop() {}

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

class WaitablesTest : public Test {
public:
    WaitablesTest() :
        invoker_(make_shared<Invoker>()),
        poller_(make_shared<Executor>(milliseconds(10), invoker_))
    {}

protected:
    shared_ptr<Invoker> invoker_;
    shared_ptr<Executor> poller_;
};

TEST_F(WaitablesTest, DispatchWaitable)
{
    auto waitable = make_unique<WaitableMock>();

    EXPECT_CALL(*waitable, wait(microseconds(10000)))
        .WillOnce(Return(true));

    EXPECT_CALL(*waitable, dispatch(IsNull()))
        .Times(1);

    function<void()> f, g;
    EXPECT_CALL(*invoker_, invoke(_))
        .WillOnce(SaveArg<0>(&f))
        .WillOnce(SaveArg<0>(&g));

    poller_->watch(move(waitable));

    f(); // Poll
    g(); // Dispatch
}

TEST_F(WaitablesTest, ThrowingWaitable)
{
    auto waitable = make_unique<WaitableMock>();

    EXPECT_CALL(*waitable, wait(microseconds(10000)))
        .WillOnce(Throw(runtime_error("Oops!")));

    EXPECT_CALL(*waitable, dispatch(NotNull()))
        .Times(1);

    function<void()> f, g;
    EXPECT_CALL(*invoker_, invoke(_))
        .WillOnce(SaveArg<0>(&f))
        .WillOnce(SaveArg<0>(&g));

    poller_->watch(move(waitable));

    f(); // Poll
    g(); // Dispatch
}

TEST_F(WaitablesTest, DispatchTimedWaitable)
{
    auto waitable = make_unique<TimedWaitableMock>(hours(1821));

    EXPECT_CALL(*waitable, timedWait(microseconds(10000)))
        .WillOnce(Return(true));

    EXPECT_CALL(*waitable, dispatch(IsNull()))
        .Times(1);

    function<void()> f, g;
    EXPECT_CALL(*invoker_, invoke(_))
        .WillOnce(SaveArg<0>(&f))
        .WillOnce(SaveArg<0>(&g));

    poller_->watch(move(waitable));

    f(); // Poll
    g(); // Dispatch
}

TEST_F(WaitablesTest, TimedWaitableDoesntTimeout)
{
    auto waitable = make_unique<TimedWaitableMock>(milliseconds(20));

    EXPECT_CALL(*waitable, timedWait(microseconds(10000)))
        .Times(3)
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*waitable, dispatch(IsNull()))
        .Times(1);

    function<void()> f, g;
    EXPECT_CALL(*invoker_, invoke(_))
        .WillOnce(SaveArg<0>(&f))
        .WillOnce(SaveArg<0>(&g));

    EXPECT_EQ(false, waitable->wait(milliseconds(10)));
    EXPECT_EQ(true, waitable->wait(milliseconds(10)));

    poller_->watch(move(waitable));

    f(); // Poll
    g(); // Dispatch
}

TEST_F(WaitablesTest, ThrowingTimedWaitable)
{
    auto waitable = make_unique<TimedWaitableMock>(minutes(1822));

    EXPECT_CALL(*waitable, timedWait(microseconds(10000)))
        .WillOnce(Throw(runtime_error("Oops!")));

    EXPECT_CALL(*waitable, dispatch(NotNull()))
        .Times(1);

    function<void()> f, g;
    EXPECT_CALL(*invoker_, invoke(_))
        .WillOnce(SaveArg<0>(&f))
        .WillOnce(SaveArg<0>(&g));

    poller_->watch(move(waitable));

    f(); // Poll
    g(); // Dispatch
}

TEST_F(WaitablesTest, TimedWaitableThatExpires)
{
    auto waitable = make_unique<TimedWaitableMock>(milliseconds(30));

    EXPECT_CALL(*waitable, timedWait(microseconds(10000)))
        .Times(3)
        .WillRepeatedly(Return(false));

    EXPECT_CALL(*waitable, dispatch(NotNull()))
        .Times(1);

    function<void()> f, g;
    EXPECT_CALL(*invoker_, invoke(_))
        .WillOnce(SaveArg<0>(&f))
        .WillOnce(SaveArg<0>(&g));

    poller_->watch(move(waitable));

    f(); // Poll
    f(); // Poll
    f(); // Poll
    f(); // Poll
    g(); // Time-out
}

TEST_F(WaitablesTest, ExpiredTimedWaitable)
{
    auto waitable = make_unique<TimedWaitableMock>(milliseconds(0));

    EXPECT_CALL(*waitable, timedWait(microseconds(10000)))
        .Times(0);

    EXPECT_CALL(*waitable, dispatch(NotNull()))
        .Times(1);

    function<void()> f, g;
    EXPECT_CALL(*invoker_, invoke(_))
        .WillOnce(SaveArg<0>(&f))
        .WillOnce(SaveArg<0>(&g));

    poller_->watch(move(waitable));

    f(); // Poll
    g(); // Time-out
}

TEST_F(WaitablesTest, TimedWaitableThatExpiresForDifferentReason)
{
    auto waitable = make_unique<TimedWaitableMock>(minutes(1822));

    EXPECT_CALL(*waitable, timedWait(microseconds(10000)))
        .WillOnce(Return(false))
        .WillOnce(Throw(WaitableTimedOutException("Timed out!")));

    EXPECT_CALL(*waitable, dispatch(NotNull()))
        .Times(1);

    function<void()> f, g;
    EXPECT_CALL(*invoker_, invoke(_))
        .WillOnce(SaveArg<0>(&f))
        .WillOnce(SaveArg<0>(&g));

    poller_->watch(move(waitable));

    f(); // Poll
    g(); // Time-out
}

TEST_F(WaitablesTest, TimedWaitableThatExpiresWithZeroPoller)
{
    auto zeroPoller = make_shared<Executor>(milliseconds(0), invoker_);

    auto waitable = make_unique<TimedWaitableMock>(microseconds(3));

    EXPECT_CALL(*waitable, timedWait(microseconds(0)))
        .Times(3)
        .WillRepeatedly(Return(false));

    EXPECT_CALL(*waitable, dispatch(NotNull()))
        .Times(1);

    function<void()> f, g;
    EXPECT_CALL(*invoker_, invoke(_))
        .WillOnce(SaveArg<0>(&f))
        .WillOnce(SaveArg<0>(&g));

    zeroPoller->watch(move(waitable));

    f(); // Poll
    f(); // Poll
    f(); // Poll
    f(); // Poll
    g(); // Time-out
}
