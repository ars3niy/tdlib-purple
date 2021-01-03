#ifndef _SUPERGROUP_TEST_H
#define _SUPERGROUP_TEST_H

#include "fixture.h"

class SupergroupTest: public CommTest {
protected:
    const int32_t     groupId             = 700;
    const int64_t     groupChatId         = -7000;
    const std::string groupChatTitle      = "Title";
    const std::string groupChatPurpleName = "chat" + std::to_string(groupChatId);

    void loginWithSupergroup(object_ptr<supergroupFullInfo> fullInfo = nullptr,
                             object_ptr<chatMembers> recentMembers = nullptr,
                             object_ptr<chatMembers> administrators = nullptr);
};

#endif
