#include <gtest/gtest.h>
#include "test-transceiver.h"

class CommTest: public testing::Test {
};

TEST_F(CommTest, dummy)
{
	EXPECT_EQ(4, 2*2);
}
