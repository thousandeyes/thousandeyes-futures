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
#include <thread>

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
using std::this_thread::sleep_for;

using thousandeyes::futures::PollingExecutor;
using thousandeyes::futures::Waitable;
using thousandeyes::futures::WaitableTimedOutException;
using thousandeyes::futures::TimedWaitable;

using ::testing::Return;
using ::testing::Test;
using ::testing::Throw;
using ::testing::_;

namespace {

class TimedWaitableMock : public TimedWaitable {
public:
    explicit TimedWaitableMock(microseconds timeout) :
        TimedWaitable(move(timeout))
    {}

    MOCK_METHOD1(timedWait, bool(const std::chrono::microseconds& timeout));

    MOCK_METHOD1(dispatch, void(std::exception_ptr err));
};

} // namespace

TEST(TimedWaitableTest, Ready)
{
    auto waitable = make_unique<TimedWaitableMock>(hours(1821));

    EXPECT_CALL(*waitable, timedWait(microseconds(10000)))
        .WillOnce(Return(true));

    auto result = waitable->wait(microseconds(10000));

    EXPECT_EQ(result, true);
}

TEST(TimedWaitableTest, NotReadyNotExpired)
{
    auto waitable = make_unique<TimedWaitableMock>(milliseconds(30));

    EXPECT_CALL(*waitable, timedWait(microseconds(10000)))
        .WillOnce(Return(false))
        .WillOnce(Return(false))
        .WillOnce(Return(false));

    EXPECT_EQ(false, waitable->wait(milliseconds(10)));
    EXPECT_EQ(false, waitable->wait(milliseconds(10)));
    EXPECT_EQ(false, waitable->wait(milliseconds(10)));
}

TEST(TimedWaitableTest, ErrorDuringWait)
{
    auto waitable = make_unique<TimedWaitableMock>(minutes(1822));

    EXPECT_CALL(*waitable, timedWait(microseconds(10000)))
        .WillOnce(Throw(runtime_error("Oops!")));

    EXPECT_THROW(waitable->wait(milliseconds(10)),
                 runtime_error);
}

TEST(TimedWaitableTest, ExpiredAndNotReady)
{
    auto waitable = make_unique<TimedWaitableMock>(milliseconds(30));

    EXPECT_CALL(*waitable, timedWait(microseconds(10000)))
        .WillOnce(Return(false));

    EXPECT_CALL(*waitable, timedWait(microseconds(0)))
        .WillOnce(Return(false));

    EXPECT_EQ(false, waitable->wait(milliseconds(10)));

    sleep_for(milliseconds(40));

    EXPECT_THROW(waitable->wait(milliseconds(10)),
                 WaitableTimedOutException);
}

TEST(TimedWaitableTest, ExpiredAndReady)
{
    auto waitable = make_unique<TimedWaitableMock>(milliseconds(30));

    EXPECT_CALL(*waitable, timedWait(microseconds(10000)))
        .WillOnce(Return(false));

    EXPECT_CALL(*waitable, timedWait(microseconds(0)))
        .WillOnce(Return(true));

    EXPECT_EQ(false, waitable->wait(milliseconds(10)));

    sleep_for(milliseconds(40));

    EXPECT_EQ(true, waitable->wait(milliseconds(10)));
}
