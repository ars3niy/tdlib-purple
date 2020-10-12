#ifndef _RECEIVING_H
#define _RECEIVING_H

#include "account-data.h"
#include "transceiver.h"
#include <purple.h>

std::string makeNoticeWithSender(const td::td_api::chat &chat, const TgMessageInfo &message,
                                 const char *noticeText, PurpleAccount *account);
std::string getMessageText(const td::td_api::formattedText &text);
std::string makeInlineImageText(int imgstoreId);
void showMessageText(TdAccountData &account, const td::td_api::chat &chat, const TgMessageInfo &message,
                     const char *text, const char *notification, uint32_t extraFlags = 0);
void showMessageTextIm(TdAccountData &account, const char *purpleUserName, const char *text,
                       const char *notification, time_t timestamp, PurpleMessageFlags flags);
void showChatNotification(TdAccountData &account, const td::td_api::chat &chat,
                          const char *notification, PurpleMessageFlags extraFlags = (PurpleMessageFlags)0);
void showChatNotification(TdAccountData &account, const td::td_api::chat &chat,
                          const char *notification, time_t timestamp, PurpleMessageFlags extraFlags);
void showGenericFileInline(const td::td_api::chat &chat, const TgMessageInfo &message,
                           const std::string &filePath, const std::string &fileDescription,
                           TdAccountData &account);
void showDownloadedFileInline(ChatId chatId, TgMessageInfo &message,
                              const std::string &filePath, const char *caption,
                              const std::string &fileDescription,
                              td::td_api::object_ptr<td::td_api::file> thumbnail,
                              TdTransceiver &transceiver, TdAccountData &account);
void showMessage(const td::td_api::chat &chat, IncomingMessage &fullMessage,
                 TdTransceiver &transceiver, TdAccountData &account);
void showMessages(std::vector<IncomingMessage>& messages, TdTransceiver &transceiver,
                  TdAccountData &account);

struct FileInfo {
    const td::td_api::file *file;
    std::string name;
    std::string caption;
    std::string description;
    bool        secret;
};

const td::td_api::file *selectPhotoSize(PurpleAccount *account, const td::td_api::messagePhoto &photo);
void getFileFromMessage(const IncomingMessage &fullMessage, FileInfo &result);

void makeFullMessage(const td::td_api::chat &chat, td::td_api::object_ptr<td::td_api::message> message,
                     IncomingMessage &fullMessage, const TdAccountData &account);
bool isMessageReady(const IncomingMessage &fullMessage, const TdAccountData &account);
void fetchExtras(IncomingMessage &fullMessage, TdTransceiver &transceiver, TdAccountData &account,
                 TdTransceiver::ResponseCb2 onFetchReply);
void checkMessageReady(const IncomingMessage *message, TdTransceiver &transceiver,
                       TdAccountData &account);

#endif

