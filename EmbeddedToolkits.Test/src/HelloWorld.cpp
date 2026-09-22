#include <gtest/gtest.h>
#include "St16c550Driver.h"

TEST(HelloWorldTest, BasicAssertions) {
    // Expect two strings to be equal.
    EXPECT_STREQ("hello", "hello");
    // Expect equality.
    EXPECT_EQ(7 * 6, 42);
}