#include <gtest/gtest.h>

class TestConfig : public ::testing::Environment
{
public:
    virtual void SetUp() override
    {
    }

    virtual void TearDown() override
    {
    }
};

int main(int argc, char * argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	::testing::AddGlobalTestEnvironment(new TestConfig);
	return RUN_ALL_TESTS();
}
