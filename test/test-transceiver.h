#ifndef _TEST_TRANSCEIVER_H
#define _TEST_TRANSCEIVER_H

#include "transceiver.h"
#include <queue>
#include <vector>

class TestTransceiver: public ITransceiverBackend {
public:
    void send(td::Client::Request &&request) override;
    void addTimeout(guint interval, GSourceFunc function, gpointer data) override;
    void runTimeouts();

    // Check that given requests, and no others, have been received, and clear the queue
    uint64_t verifyRequest(const td::td_api::Function &request);
    void verifyRequests(std::initializer_list<td::td_api::object_ptr<td::td_api::Function>> requests);
    void verifyNoRequests();

    void update(td::td_api::object_ptr<td::td_api::Object> object);

    // Replies to the first non-replied request from the last verifyRequest(s) batch, or fails the
    // test case if there is no such request
    void reply(td::td_api::object_ptr<td::td_api::Object> object);
    void reply(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object);

    const std::string &getInputPhotoPath(unsigned index) { return m_inputPhotoPaths.at(index); }
private:
    struct TimerInfo {
        GSourceFunc function;
        gpointer data;
    };

    std::queue<td::Client::Request> m_requests;
    std::vector<uint64_t>           m_lastRequestIds;
    uint64_t                        expectedRequestId = 1;
    std::vector<std::string>        m_inputPhotoPaths;
    std::vector<TimerInfo>          m_timers;

    void verifyRequestImpl(const td::td_api::Function &request);
};

// Put the following functions in td::td_api namespace so that tdlib types can be used without
// namespace in function prototypes, while avoiding "using namespace..." in this header.
namespace td {
namespace td_api {

object_ptr<user> makeUser(std::int32_t id_, std::string const &first_name_,
                          std::string const &last_name_,
                          std::string const &phone_number_,
                          object_ptr<UserStatus> &&status_);

object_ptr<chat> makeChat(std::int64_t id_,
                          object_ptr<ChatType> &&type_,
                          std::string const &title_,
                          object_ptr<message> &&last_message_,
                          std::int32_t unread_count_,
                          std::int64_t last_read_inbox_message_id_,
                          std::int64_t last_read_outbox_message_id_);

object_ptr<message> makeMessage(std::int64_t id_, std::int32_t sender_user_id_, std::int64_t chat_id_,
                                bool is_outgoing_, std::int32_t date_, object_ptr<MessageContent> &&content_);

object_ptr<messageText> makeTextMessage(const std::string &text);

object_ptr<photo> makePhotoRemote(int32_t fileId, unsigned size, unsigned width, unsigned height);
object_ptr<photo> makePhotoLocal(int32_t fileId, unsigned size, const std::string &path,
                                 unsigned width, unsigned height);
object_ptr<photo> makePhotoUploading(int32_t fileId, unsigned size, unsigned uploaded, const std::string &path,
                                     unsigned width, unsigned height);

}
}

#endif
