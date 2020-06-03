#include "test-transceiver.h"
#include "printout.h"
#include <gtest/gtest.h>

using namespace td::td_api;

void TestTransceiver::send(td::Client::Request &&request)
{
    ASSERT_EQ(expectedRequestId, request.id);
    expectedRequestId++;
    std::cout << "Received: " << requestToString(*request.function) << std::endl;
    m_requests.push(std::move(request));
}

uint64_t TestTransceiver::verifyRequest(const Function &request)
{
    m_lastRequestIds.clear();
    verifyRequestImpl(request);
    if (!m_requests.empty()) {
        m_lastRequestIds.push_back(m_requests.front().id);
        m_requests.pop();
    }
    verifyNoRequests();
    return m_lastRequestIds.empty() ? 0 : m_lastRequestIds.back();
}

void TestTransceiver::verifyRequests(std::initializer_list<td::td_api::object_ptr<td::td_api::Function>> requests)
{
    m_lastRequestIds.clear();
    for (auto &pReq: requests) {
        verifyRequestImpl(*pReq);
        if (!m_requests.empty()) {
            m_lastRequestIds.push_back(m_requests.front().id);
            m_requests.pop();
        }
    }
    verifyNoRequests();
}

void TestTransceiver::addTimeout(guint interval, GSourceFunc function, gpointer data)
{
    m_timers.emplace_back();
    m_timers.back().function = function;
    m_timers.back().data = data;
}

void TestTransceiver::runTimeouts()
{
    for (const TimerInfo &timer: m_timers)
        while (timer.function(timer.data)) ;

    m_timers.clear();
}

#define COMPARE(param) ASSERT_EQ(expected.param, actual.param)

static void compare(const setTdlibParameters &actual, const setTdlibParameters &expected)
{
    COMPARE(parameters_->database_directory_);
}

static void compare(const checkDatabaseEncryptionKey &actual, const checkDatabaseEncryptionKey &expected)
{
    COMPARE(encryption_key_);
}

static void compare(const setAuthenticationPhoneNumber &actual, const setAuthenticationPhoneNumber &expected)
{
    COMPARE(phone_number_);
    COMPARE(settings_ != nullptr);
}

static void compare(const viewMessages &actual, const viewMessages &expected)
{
    COMPARE(chat_id_);
    COMPARE(message_ids_.size());
    for (size_t i = 0; i < actual.message_ids_.size(); i++)
        COMPARE(message_ids_[i]);
    COMPARE(force_read_);
}

static void compare(const downloadFile &actual, const downloadFile &expected)
{
    COMPARE(file_id_);
    COMPARE(priority_);
    COMPARE(offset_);
    COMPARE(limit_);
    COMPARE(synchronous_);
}

static void compare(const object_ptr<sendMessageOptions> &actual, const object_ptr<sendMessageOptions> &expected)
{
    ASSERT_EQ(nullptr, actual) << "not supported";
    ASSERT_EQ(nullptr, expected) << "not supported";
}

static void compare(const object_ptr<ReplyMarkup> &actual, const object_ptr<ReplyMarkup> &expected)
{
    ASSERT_EQ(nullptr, actual) << "not supported";
    ASSERT_EQ(nullptr, expected) << "not supported";
}

static void compare(const object_ptr<formattedText> &actual, const object_ptr<formattedText> &expected)
{
    ASSERT_EQ(expected != nullptr, actual != nullptr);
    if (!actual) return;

    ASSERT_EQ(expected->text_, actual->text_);
    ASSERT_TRUE(actual->entities_.empty()) << "not supported";
    ASSERT_TRUE(expected->entities_.empty()) << "not supported";
}

static void compare(const inputMessageText &actual,
                    const inputMessageText &expected)
{
    compare(actual.text_, expected.text_);
    COMPARE(disable_web_page_preview_);
    COMPARE(clear_draft_);
}

static void compare(const object_ptr<InputFile> &actual, const object_ptr<InputFile> &expected)
{
    ASSERT_EQ(expected != nullptr, actual != nullptr);
    if (!actual) return;
    ASSERT_EQ(expected->get_id(), actual->get_id());

    switch (actual->get_id()) {
        case td::td_api::inputFileLocal::ID:
            ASSERT_EQ(static_cast<const inputFileLocal &>(*expected).path_,
                      static_cast<const inputFileLocal &>(*actual).path_);
            break;
        case td::td_api::inputFileId::ID:
            ASSERT_EQ(static_cast<const inputFileId &>(*expected).id_,
                      static_cast<const inputFileId &>(*actual).id_);
            break;
        default:
            ASSERT_TRUE(false) << "not supported";
    }
}

static void compare(const inputMessageDocument &actual,
                    const inputMessageDocument &expected)
{
    compare(actual.document_, expected.document_);
    ASSERT_EQ(nullptr, expected.thumbnail_) << "not supported";
    ASSERT_EQ(nullptr, actual.thumbnail_) << "not supported";
    compare(actual.caption_, expected.caption_);
}

static void compare(const inputMessagePhoto &actual, const inputMessagePhoto &expected,
                    std::vector<std::string> &m_inputPhotoPaths)
{
    ASSERT_EQ(nullptr, expected.thumbnail_) << "not supported";
    ASSERT_EQ(nullptr, actual.thumbnail_) << "not supported";
    COMPARE(added_sticker_file_ids_.size());
    for (unsigned i = 0; i < actual.added_sticker_file_ids_.size(); i++)
        COMPARE(added_sticker_file_ids_[i]);
    COMPARE(width_);
    COMPARE(height_);
    compare(actual.caption_, expected.caption_);
    COMPARE(ttl_);

    COMPARE(photo_ != nullptr);
    if (actual.photo_) {
        COMPARE(photo_->get_id());
        if (actual.photo_->get_id() == inputFileLocal::ID)
            m_inputPhotoPaths.push_back(static_cast<const inputFileLocal &>(*actual.photo_).path_);
    }
}

static void compare(const object_ptr<InputMessageContent> &actual,
                    const object_ptr<InputMessageContent> &expected,
                    std::vector<std::string> &m_inputPhotoPaths)
{
    ASSERT_EQ(expected != nullptr, actual != nullptr);
    if (!actual) return;

    ASSERT_EQ(expected->get_id(), actual->get_id());
    switch (actual->get_id()) {
        case inputMessageText::ID:
            compare(static_cast<const inputMessageText &>(*actual), static_cast<const inputMessageText &>(*expected));
            break;
        case inputMessagePhoto::ID:
            compare(static_cast<const inputMessagePhoto &>(*actual), static_cast<const inputMessagePhoto &>(*expected),
                    m_inputPhotoPaths);
            break;
        case inputMessageDocument::ID:
            compare(static_cast<const inputMessageDocument &>(*actual), static_cast<const inputMessageDocument &>(*expected));
            break;
        default:
            ASSERT_TRUE(false) << "Unsupported input message content";
    }
}

static void compare(const sendMessage &actual, const sendMessage &expected,
                    std::vector<std::string> &m_inputPhotoPaths)
{
    COMPARE(chat_id_);
    COMPARE(reply_to_message_id_);

    compare(actual.options_,               expected.options_);
    compare(actual.reply_markup_,          expected.reply_markup_);
    compare(actual.input_message_content_, expected.input_message_content_, m_inputPhotoPaths);
}

static void compare(const getBasicGroupFullInfo &actual, const getBasicGroupFullInfo &expected)
{
    COMPARE(basic_group_id_);
}

static void compare(const joinChatByInviteLink &actual, const joinChatByInviteLink &expected)
{
    COMPARE(invite_link_);
}

static void compare(const contact &actual, const contact &expected)
{
    COMPARE(phone_number_);
    COMPARE(first_name_);
    COMPARE(last_name_);
    COMPARE(vcard_);
    COMPARE(user_id_);
}

static void compare(const importContacts &actual, const importContacts &expected)
{
    COMPARE(contacts_.size());

    for (size_t i = 0; i < actual.contacts_.size(); i++)
        compare(*actual.contacts_[i], *expected.contacts_[i]);
}

static void compare(const addContact &actual, const addContact &expected)
{
    compare(*actual.contact_, *expected.contact_);
    COMPARE(share_phone_number_);
}

static void compare(const createPrivateChat &actual, const createPrivateChat &expected)
{
    COMPARE(user_id_);
    COMPARE(force_);
}

static void compare(const checkAuthenticationCode &actual, const checkAuthenticationCode &expected)
{
    COMPARE(code_);
}

static void compare(const registerUser &actual, const registerUser &expected)
{
    COMPARE(first_name_);
    COMPARE(last_name_);
}

static void compare(const getMessage &actual, const getMessage &expected)
{
    COMPARE(chat_id_);
    COMPARE(message_id_);
}

static void compare(const sendChatAction &actual, const sendChatAction &expected)
{
    COMPARE(chat_id_);
    COMPARE(action_ != nullptr);
    if (actual.action_) {
        COMPARE(action_->get_id());
    }
}

static void compare(const proxyTypeHttp &actual, const proxyTypeHttp &expected)
{
    COMPARE(username_);
    COMPARE(password_);
    COMPARE(http_only_);
}

static void compare(const proxyTypeSocks5 &actual, const proxyTypeSocks5 &expected)
{
    COMPARE(username_);
    COMPARE(password_);
}

static void compare(const addProxy &actual, const addProxy &expected)
{
    COMPARE(server_);
    COMPARE(port_);
    COMPARE(enable_);
    COMPARE(type_ != nullptr);
    if (actual.type_ != nullptr) {
        COMPARE(type_->get_id());
        switch (actual.type_->get_id()) {
            case proxyTypeHttp::ID:
                compare(static_cast<const proxyTypeHttp &>(*actual.type_),
                        static_cast<const proxyTypeHttp &>(*expected.type_));
                break;
            case proxyTypeSocks5::ID:
                compare(static_cast<const proxyTypeSocks5 &>(*actual.type_),
                        static_cast<const proxyTypeSocks5 &>(*expected.type_));
                break;
            default:
                ASSERT_TRUE(false) << "Unsupported proxy type";
        }
    }
}

static void compare(const removeProxy &actual, const removeProxy &expected)
{
    COMPARE(proxy_id_);
}

static void compare(const deleteChatHistory &actual, const deleteChatHistory &expected)
{
    COMPARE(chat_id_);
    COMPARE(remove_from_chat_list_);
    COMPARE(revoke_);
}

static void compare(const removeContacts &actual, const removeContacts &expected)
{
    COMPARE(user_ids_.size());
    for (unsigned i = 0; i < actual.user_ids_.size(); i++)
        COMPARE(user_ids_[i]);
}

static void compare(const leaveChat &actual, const leaveChat &expected)
{
    COMPARE(chat_id_);
}

static void compare(const deleteSupergroup &actual, const deleteSupergroup &expected)
{
    COMPARE(supergroup_id_);
}

static void compare(const checkAuthenticationPassword &actual, const checkAuthenticationPassword &expected)
{
    COMPARE(password_);
}

static void compare(const uploadFile &actual, const uploadFile &expected)
{
    compare(actual.file_, expected.file_);

    COMPARE(file_type_ != nullptr);
    if (actual.file_type_) {
        COMPARE(file_type_->get_id());
    }

    COMPARE(priority_);
}

static void compare(const closeSecretChat &actual, const closeSecretChat &expected)
{
    COMPARE(secret_chat_id_);
}

static void compare(const getSupergroupFullInfo &actual, const getSupergroupFullInfo &expected)
{
    COMPARE(supergroup_id_);
}

static void compare(const cancelDownloadFile &actual, const cancelDownloadFile &expected)
{
    COMPARE(file_id_);
    COMPARE(only_if_pending_);
}

static void compareRequests(const Function &actual, const Function &expected,
                            std::vector<std::string> &m_inputPhotoPaths)
{
    ASSERT_EQ(expected.get_id(), actual.get_id()) << "Wrong request type: got " <<
        requestToString(actual) << " expected " << requestToString(expected);

#define C(class) case class::ID: \
    compare(static_cast<const class &>(actual), static_cast<const class &>(expected)); \
    break;

    switch (actual.get_id()) {
        C(setTdlibParameters)
        C(checkDatabaseEncryptionKey)
        C(setAuthenticationPhoneNumber)
        case getContacts::ID: break;
        case getChats::ID: break;
        C(viewMessages)
        C(downloadFile)
        case sendMessage::ID:
            compare(static_cast<const sendMessage &>(actual), static_cast<const sendMessage &>(expected),
                    m_inputPhotoPaths);
            break;
        C(getBasicGroupFullInfo)
        C(joinChatByInviteLink)
        C(importContacts)
        C(addContact)
        C(createPrivateChat)
        C(checkAuthenticationCode)
        C(registerUser)
        C(getMessage)
        C(sendChatAction)
        C(addProxy)
        case disableProxy::ID: break; // no data fields
        case getProxies::ID: break; // no data fields
        C(removeProxy)
        C(deleteChatHistory)
        C(removeContacts)
        C(leaveChat)
        C(deleteSupergroup)
        C(checkAuthenticationPassword)
        C(uploadFile)
        C(closeSecretChat)
        C(getSupergroupFullInfo)
        C(cancelDownloadFile)
        default: ASSERT_TRUE(false) << "Unsupported request " << requestToString(actual);
    }
}

void TestTransceiver::verifyRequestImpl(const Function &request)
{
    ASSERT_FALSE(m_requests.empty()) << "Missing request: expected " << requestToString(request);

    std::cout << "Received request " << m_requests.front().id << ": " << requestToString(*m_requests.front().function) << "\n";
    compareRequests(*m_requests.front().function, request, m_inputPhotoPaths);
}

void TestTransceiver::verifyNoRequests()
{
    ASSERT_TRUE(m_requests.empty()) << "Unexpected request: " << requestToString(*m_requests.front().function);
}

void TestTransceiver::update(object_ptr<Object> object)
{
    std::cout << "Sending update: " << responseToString(*object) << "\n";
    receive({0, std::move(object)});
}

void TestTransceiver::reply(object_ptr<Object> object)
{
    ASSERT_FALSE(m_lastRequestIds.empty()) << "No requests to reply to";
    reply(m_lastRequestIds.front(), std::move(object));
    m_lastRequestIds.erase(m_lastRequestIds.begin());
}

void TestTransceiver::reply(uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object)
{
    std::cout << "Replying to request " << requestId << ": " << responseToString(*object) << "\n";
    receive({requestId, std::move(object)});
}

namespace td {
namespace td_api {

object_ptr<user> makeUser(std::int32_t id_, std::string const &first_name_,
                          std::string const &last_name_,
                          std::string const &phone_number_,
                          object_ptr<UserStatus> &&status_)
{
    return make_object<user>(
        id_, first_name_, last_name_, "", phone_number_, std::move(status_),
        nullptr,
        false, // is_contact
        false,
        false,
        false,
        "",
        false,
        true,
        make_object<userTypeRegular>(),
        ""
    );
}

object_ptr<chat> makeChat(std::int64_t id_,
                          object_ptr<ChatType> &&type_,
                          std::string const &title_,
                          object_ptr<message> &&last_message_,
                          std::int32_t unread_count_,
                          std::int64_t last_read_inbox_message_id_,
                          std::int64_t last_read_outbox_message_id_)
{
    return make_object<chat>(
        id_,
        std::move(type_),
        nullptr,
        title_,
        nullptr,
        make_object<chatPermissions>(true, true, true, true, true, false, false, false),
        std::move(last_message_),
        0,
        false,
        unread_count_ > 0,
        false,
        false,
        true,
        true,
        false,
        false,
        unread_count_,
        last_read_inbox_message_id_,
        last_read_outbox_message_id_,
        0,
        make_object<chatNotificationSettings>(true, 0, true, "default", true, false, true, false, true, false),
        nullptr,
        0,
        0,
        nullptr,
        ""
    );
}

object_ptr<updateChatChatList> makeUpdateChatListMain(int64_t chatId)
{
    return make_object<updateChatChatList>(chatId, make_object<chatListMain>());
}

object_ptr<message> makeMessage(std::int64_t id_, std::int32_t sender_user_id_, std::int64_t chat_id_,
                                bool is_outgoing_, std::int32_t date_, object_ptr<MessageContent> &&content_)
{
    return make_object<message>(
        id_,
        sender_user_id_,
        chat_id_,
        nullptr,
        nullptr,
        is_outgoing_,
        false,
        true,
        true,
        true,
        false,
        false,
        date_,
        0,
        nullptr,
        0,
        0,
        0,
        0,
        "",
        0,
        0,
        "",
        std::move(content_),
        nullptr
    );
}

object_ptr<messageText> makeTextMessage(const std::string &text)
{
    return make_object<messageText>(
        make_object<formattedText>(text, std::vector<object_ptr<textEntity>>()),
        nullptr
    );
}

object_ptr<photo> makePhotoRemote(int32_t fileId, unsigned size, unsigned width, unsigned height)
{
    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, size, size,
            make_object<localFile>("", true, true, false, false, 0, 0, 0),
            make_object<remoteFile>("beh", "bleh", false, true, size)
        ),
        width, height
    ));
    return make_object<photo>(false, nullptr, std::move(sizes));
}

object_ptr<photo> makePhotoLocal(int32_t fileId, unsigned size, const std::string &path,
                                 unsigned width, unsigned height)
{
    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, size, size,
            make_object<localFile>(path, true, true, false, true, 0, size, size),
            make_object<remoteFile>("beh", "bleh", false, true, size)
        ),
        width, height
    ));
    return make_object<photo>(false, nullptr, std::move(sizes));
}

object_ptr<photo> makePhotoUploading(int32_t fileId, unsigned size, unsigned uploaded, const std::string &path,
                                     unsigned width, unsigned height)
{
    EXPECT_TRUE(uploaded < size);

    std::vector<object_ptr<photoSize>> sizes;
    sizes.push_back(make_object<photoSize>(
        "whatever",
        make_object<file>(
            fileId, size, size,
            make_object<localFile>(path, true, true, false, true, 0, size, size),
            make_object<remoteFile>("beh", "bleh", false, false, uploaded)
        ),
        width, height
    ));
    return make_object<photo>(false, nullptr, std::move(sizes));
}

}
}
