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

#include <thousandeyes/futures/Waitable.h>

using std::function;
using std::make_shared;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::weak_ptr;
using std::chrono::milliseconds;

using thousandeyes::futures::Waitable;

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::Test;
using ::testing::Throw;

namespace {

class WaitableMock : public Waitable {
public:
    WaitableMock(milliseconds epochDeadline) : Waitable(move(epochDeadline))
    {}

    MOCK_METHOD1(wait, bool(const std::chrono::microseconds& timeout));

    MOCK_METHOD1(dispatch, void(std::exception_ptr err));
};

} // namespace

TEST(WaitableTest, Compare)
{
    WaitableMock w0{milliseconds(0)};
    WaitableMock w1{milliseconds(10)};

    EXPECT_EQ(milliseconds(-10), w0.compare(w1));
    EXPECT_EQ(milliseconds(10), w1.compare(w0));
}

TEST(WaitableTest, Timeout)
{
    WaitableMock w{milliseconds(1821)};

    EXPECT_EQ(milliseconds(1821), w.timeout(milliseconds(0)));
    EXPECT_EQ(milliseconds(1822), w.timeout(milliseconds(-1)));
    EXPECT_EQ(milliseconds(3642), w.timeout(milliseconds(-1821)));
    EXPECT_EQ(milliseconds(1), w.timeout(milliseconds(1820)));
    EXPECT_EQ(milliseconds(-1), w.timeout(milliseconds(1822)));
}

TEST(WaitableTest, Expired)
{
    WaitableMock w{milliseconds(1821)};

    EXPECT_FALSE(w.expired(milliseconds(0)));
    EXPECT_FALSE(w.expired(milliseconds(-1)));
    EXPECT_FALSE(w.expired(milliseconds(-1821)));
    EXPECT_FALSE(w.expired(milliseconds(1820)));
    EXPECT_TRUE(w.expired(milliseconds(1821)));
    EXPECT_TRUE(w.expired(milliseconds(1822)));
    EXPECT_TRUE(w.expired(milliseconds(3642)));
}
