#include "tg-utils.h"
#include "config.h"
#include <purple.h>
#include <unistd.h>

struct MessagePart {
    bool        isImage;
    int         imageId;
    std::string text;
};

static void parseMessage(const char *message, std::vector<MessagePart> &parts)
{
    // TODO: recognize images
    parts.resize(1);
    parts.back().isImage = false;
    parts.back().text = message;
}

void transmitMessage(int64_t chatId, const char *message, TdTransceiver &transceiver)
{
    std::vector<MessagePart> parts;
    parseMessage(message, parts);
    for (const MessagePart &input: parts) {
        td::td_api::object_ptr<td::td_api::sendMessage> sendMessageRequest = td::td_api::make_object<td::td_api::sendMessage>();
        sendMessageRequest->chat_id_ = chatId;

        if (input.isImage) {
            PurpleStoredImage *psi = purple_imgstore_find_by_id (input.imageId);
            if (!psi) {
                purple_debug_misc(config::pluginId, "Failed to send image: id %d not found\n", input.imageId);
                continue;
            }
            char *fileName;
            int fd = g_file_open_tmp("tdlib_upload_XXXXXXXX", &fileName, NULL);
            if (fd < 0) {
                purple_debug_misc(config::pluginId, "Failed to send image: could not create temporary file\n");
                continue;
            }
            write(fd, purple_imgstore_get_data (psi), purple_imgstore_get_size (psi));
            close(fd);

            td::td_api::object_ptr<td::td_api::sendMessage> sendMessageRequest = td::td_api::make_object<td::td_api::sendMessage>();
            sendMessageRequest->chat_id_ = chatId;
            td::td_api::object_ptr<td::td_api::inputMessagePhoto> content = td::td_api::make_object<td::td_api::inputMessagePhoto>();
            content->photo_ = td::td_api::make_object<td::td_api::inputFileLocal>(fileName);
            content->caption_ = td::td_api::make_object<td::td_api::formattedText>();
            content->caption_->text_ = input.text;

            sendMessageRequest->input_message_content_ = std::move(content);
            purple_debug_misc(config::pluginId, "Sending photo %s\n", fileName);
            g_free(fileName);
        } else {
            td::td_api::object_ptr<td::td_api::inputMessageText> content = td::td_api::make_object<td::td_api::inputMessageText>();
            content->text_ = td::td_api::make_object<td::td_api::formattedText>();
            content->text_->text_ = message;
            sendMessageRequest->input_message_content_ = std::move(content);
        }

        // successive updateFile follow, can show transfer dialog with progress
        // when one with is_uploading_active comes, can delete temproray file
        // and if not is_uploading_completed, show error message
        // Or updateMessageContent, or updateMessageSendSucceeded

        transceiver.sendQuery(std::move(sendMessageRequest), nullptr);
    }
}
