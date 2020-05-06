#include "test-transceiver.h"
#include "tdlib-purple.h"
#include <gtest/gtest.h>

class CommTest: public testing::Test {
public:
    CommTest();
    ~CommTest();

private:
    const std::string phoneNumber = "1234567";
    TestTransceiver   tgl;
    PurplePlugin      purplePlugin;
    PurpleAccount    *account;
};

CommTest::CommTest()
{
    tgprpl_set_test_backend(&tgl);
    purple_init_plugin(&purplePlugin);
    account = purple_account_new(phoneNumber.c_str(), NULL);
    ((PurplePluginProtocolInfo *)purplePlugin.info->extra_info)->login(account);
}

CommTest::~CommTest()
{
    purple_account_destroy(account);
}

TEST_F(CommTest, dummy)
{
	EXPECT_EQ(4, 2*2);
}
