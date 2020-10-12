#include "receiving.h"
#include "client-utils.h"
#include "format.h"
#include "purple-info.h"
#include "file-transfer.h"
#include "config.h"

std::string makeNoticeWithSender(const td::td_api::chat &chat, const TgMessageInfo &message,
                                 const char *noticeText, PurpleAccount *account)
{
    std::string prefix = getSenderDisplayName(chat, message, account);
    if (!prefix.empty())
        prefix += ": ";
    return prefix + noticeText;
}

std::string getMessageText(const td::td_api::formattedText &text)
{
    char *newText = purple_markup_escape_text(text.text_.c_str(), text.text_.size());
    std::string result(newText);
    g_free(newText);
    return result;
}

std::string makeInlineImageText(int imgstoreId)
{
    return "\n<img id=\"" + std::to_string(imgstoreId) + "\">";
}

static PurpleMessageFlags getNotificationFlags(PurpleMessageFlags extraFlags)
{
    unsigned flags = (extraFlags & PURPLE_MESSAGE_ERROR) | (extraFlags & PURPLE_MESSAGE_NO_LOG);
    if (flags == 0)
        flags = PURPLE_MESSAGE_SYSTEM;

    return (PurpleMessageFlags)flags;
}

void showMessageTextIm(TdAccountData &account, const char *purpleUserName, const char *text,
                       const char *notification, time_t timestamp, PurpleMessageFlags flags)
{
    PurpleConversation *conv = NULL;

    if (text) {
        if (flags & PURPLE_MESSAGE_SEND) {
            // serv_got_im seems to work for messages sent from another client, but not for
            // echoed messages from this client. Therefore, this (code snippet from facebook plugin).
            conv = getImConversation(account.purpleAccount, purpleUserName);
            purple_conv_im_write(purple_conversation_get_im_data(conv),
                                 purple_account_get_name_for_display(account.purpleAccount),
                                 text, flags, timestamp);
        } else {
            serv_got_im(purple_account_get_connection(account.purpleAccount), purpleUserName, text,
                        flags, timestamp);
        }
    }

    if (notification) {
        if (conv == NULL)
            conv = getImConversation(account.purpleAccount, purpleUserName);
        purple_conv_im_write(purple_conversation_get_im_data(conv), purpleUserName, notification,
                             getNotificationFlags(flags), timestamp);
    }
}

static void showMessageTextChat(TdAccountData &account, const td::td_api::chat &chat,
                                const TgMessageInfo &message, const char *text,
                                const char *notification, PurpleMessageFlags flags)
{
    // Again, doing what facebook plugin does
    int purpleId = account.getPurpleChatId(getId(chat));
    PurpleConvChat *conv = getChatConversation(account, chat, purpleId);

    if (text) {
        if (flags & PURPLE_MESSAGE_SEND) {
            if (conv)
                purple_conv_chat_write(conv, purple_account_get_name_for_display(account.purpleAccount),
                                       text, flags, message.timestamp);
        } else {
            if (purpleId != 0)
                serv_got_chat_in(purple_account_get_connection(account.purpleAccount), purpleId,
                                 message.incomingGroupchatSender.empty() ? "someone" : message.incomingGroupchatSender.c_str(),
                                 flags, text, message.timestamp);
        }
    }

    if (notification) {
        if (conv)
            // Protocol plugins mostly use who="" for such messages, but this currently causes problems
            // with Spectrum. Use some non-empty string. Pidgin will ignore the who parameter for
            // notification messages.
            purple_conv_chat_write(conv, " ", notification, getNotificationFlags(flags), message.timestamp);
    }
}

static std::string quoteMessage(const td::td_api::message *message, TdAccountData &account)
{
    const td::td_api::user *originalAuthor = nullptr;
    if (message)
        originalAuthor = account.getUser(getSenderUserId(*message));

    std::string originalName;
    if (originalAuthor)
        originalName = account.getDisplayName(*originalAuthor);
    else {
        // message == NULL means it could not be fetched, or took too long to fetch
        // TRANSLATOR: In-line placeholder if the original author of a quote is unknown. Is at the beginning of the line if and only if you make it so, see "<b>&bt {} wrote:"...
        originalName = _("Unknown user");
    }

    std::string text;
    if (!message || !message->content_) {
        // TRANSLATOR: In-chat placeholder when something unknown is being replied to.
        text = _("[message unavailable]");
    } else switch (message->content_->get_id()) {
        case td::td_api::messageText::ID: {
            const td::td_api::messageText &messageText = static_cast<const td::td_api::messageText &>(*message->content_);
            if (messageText.text_)
                text = getMessageText(*messageText.text_);
            else
                text = "";
            break;
        }
        case td::td_api::messagePhoto::ID: {
            const td::td_api::messagePhoto &photo = static_cast<const td::td_api::messagePhoto &>(*message->content_);
            // TRANSLATOR: In-line placeholder when a photo is being replied to.
            text = _("[photo]");
            if (photo.caption_)
                text += " " + photo.caption_->text_;
            break;
        }
        case td::td_api::messageDocument::ID: {
            const td::td_api::messageDocument &document = static_cast<const td::td_api::messageDocument &>(*message->content_);
            if (document.document_) {
                // TRANSLATOR: In-line placeholder when a file is being replied to. Arguments will be the file name and MIME type (e.g. "application/gzip")
                text = formatMessage(_("[file: {0} ({1})]"), {document.document_->file_name_,
                                                           document.document_->mime_type_});
            } else {
                // Not supposed to be possible, but just in case
                text = "[file]";
            }
            if (document.caption_)
                text += " " + document.caption_->text_;
            break;
        }
        case td::td_api::messageVideo::ID: {
            const td::td_api::messageVideo &video = static_cast<const td::td_api::messageVideo &>(*message->content_);
            if (video.video_) {
                // TRANSLATOR: In-line placeholder when a video is being replied to. Argument will be the file name.
                text = formatMessage(_("[video: {}]"), video.video_->file_name_);
            } else {
                // Not supposed to be possible, but just in case
                text = "[video]";
            }
            if (video.caption_)
                text += " " + video.caption_->text_;
            break;
        }
        case td::td_api::messageSticker::ID:
            // TRANSLATOR: In-line placeholder when a sticker is being replied to.
            text = _("[sticker]");
            break;
        default:
            text = '[' + getUnsupportedMessageDescription(*message->content_) + ']';
            break;
    }

    for (unsigned i = 0; i < text.size(); i++)
        if (text[i] == '\n') text[i] = ' ';

    // TRANSLATOR: In-chat notification of a reply. Arguments will be username and the original text or description thereof. Please preserve the HTML.
    return formatMessage(_("<b>&gt; {0} wrote:</b>\n&gt; {1}"), {originalName, text});
}

void showMessageText(TdAccountData &account, const td::td_api::chat &chat, const TgMessageInfo &message,
                     const char *text, const char *notification, uint32_t extraFlags)
{
    PurpleMessageFlags directionFlag = message.outgoing ? PURPLE_MESSAGE_SEND : PURPLE_MESSAGE_RECV;
    PurpleMessageFlags flags = (PurpleMessageFlags) (extraFlags | directionFlag);
    if (message.outgoing && !message.sentLocally)
        flags = (PurpleMessageFlags) (flags | PURPLE_MESSAGE_REMOTE_SEND);

    std::string newText;
    if (message.repliedMessageId.valid())
        newText = quoteMessage(message.repliedMessage.get(), account);
    if (!message.forwardedFrom.empty()) {
        if (!newText.empty())
            newText += "\n";
        // TRANSLATOR: In-chat notification of forward. Argument will be a username. Please preserve the HTML.
        newText += formatMessage(_("<b>Forwarded from {}:</b>"), message.forwardedFrom);
    }
    if (text) {
        if (!newText.empty())
            newText += "\n";
        newText += text;
    }
    if (!newText.empty())
        text = newText.c_str();

    const td::td_api::user *privateUser = account.getUserByPrivateChat(chat);
    if (privateUser) {
        std::string userName;

        // If the chat is in contact list telegram-wise, there should be a buddy for it.
        // If not, libpurple won't be able to translate buddy name to alias, so use display name
        // instead of idXXXXXXXXX
        if (isChatInContactList(chat, privateUser))
            userName = getPurpleBuddyName(*privateUser);
        else
            userName = account.getDisplayName(*privateUser);
        showMessageTextIm(account, userName.c_str(), text, notification, message.timestamp, flags);
    }

    SecretChatId secretChatId = getSecretChatId(chat);
    if (secretChatId.valid()) {
        std::string userName = getSecretChatBuddyName(secretChatId);
        showMessageTextIm(account, userName.c_str(), text, notification, message.timestamp, flags);
    }

    if (getBasicGroupId(chat).valid() || getSupergroupId(chat).valid())
        showMessageTextChat(account, chat, message, text, notification, flags);
}

void showChatNotification(TdAccountData &account, const td::td_api::chat &chat,
                          const char *notification, time_t timestamp, PurpleMessageFlags extraFlags)
{
    TgMessageInfo messageInfo;
    messageInfo.type = TgMessageInfo::Type::Other;
    messageInfo.timestamp = timestamp;
    messageInfo.outgoing = true;
    showMessageText(account, chat, messageInfo, NULL, notification, extraFlags);
}

void showChatNotification(TdAccountData &account, const td::td_api::chat &chat,
                          const char *notification, PurpleMessageFlags extraFlags)
{
    showChatNotification(account, chat, notification,
                         (extraFlags & PURPLE_MESSAGE_NO_LOG) ? 0 : time(NULL), extraFlags);
}

void showGenericFileInline(const td::td_api::chat &chat, const TgMessageInfo &message,
                           const std::string &filePath, const std::string &fileDescription,
                           TdAccountData &account)
{
    if (filePath.find('"') != std::string::npos) {
        std::string notice = makeNoticeWithSender(chat, message, "Cannot show file: path contains quotes",
                                                    account.purpleAccount);
        showMessageText(account, chat, message, NULL, notice.c_str());
    } else {
        std::string text = "<a href=\"file://" + filePath + "\">" + fileDescription + "</a>";
        showMessageText(account, chat, message, text.c_str(), NULL);
    }
}

const td::td_api::file *selectPhotoSize(PurpleAccount *account, const td::td_api::messagePhoto &photo)
{
    unsigned                     sizeLimit        = getAutoDownloadLimitKb(account);
    const td::td_api::photoSize *selectedSize     = nullptr;
    bool                         selectedFileSize = 0;
    if (photo.photo_)
        for (const auto &newSize: photo.photo_->sizes_)
            if (newSize && newSize->photo_) {
                unsigned fileSize            = getFileSizeKb(*newSize->photo_);
                bool     isWithinLimit       = isSizeWithinLimit(fileSize, sizeLimit);
                bool     selectedWithinLimit = isSizeWithinLimit(selectedFileSize, sizeLimit);
                if (!selectedSize ||
                    (!selectedWithinLimit && (isWithinLimit || (fileSize < selectedFileSize))) ||
                    (selectedWithinLimit && isWithinLimit && (newSize->width_ > selectedSize->width_)))
                {
                    selectedSize = newSize.get();
                    selectedFileSize = fileSize;
                }
            }

    if (selectedSize)
        purple_debug_misc(config::pluginId, "Selected size %dx%d for photo\n",
                          (int)selectedSize->width_, (int)selectedSize->height_);
    else
        purple_debug_warning(config::pluginId, "No file found for a photo\n");

    return selectedSize ? selectedSize->photo_.get() : nullptr;
}

void makeFullMessage(const td::td_api::chat &chat, td::td_api::object_ptr<td::td_api::message> message,
                     IncomingMessage &fullMessage, const TdAccountData &account)
{
    if (!message) {
        fullMessage.message = nullptr;
        return;
    }

    fullMessage.repliedMessage = nullptr;
    fullMessage.selectedPhotoSizeId = 0;
    fullMessage.repliedMessageFailed = false;

    const char *option = purple_account_get_string(account.purpleAccount, AccountOptions::DownloadBehaviour,
                                                   AccountOptions::DownloadBehaviourDefault());
    fullMessage.standardDownloadConfigured = (strcmp(option, AccountOptions::DownloadBehaviourHyperlink) != 0);
    fullMessage.inlineFileSizeLimit = getAutoDownloadLimitKb(account.purpleAccount);

    TgMessageInfo &messageInfo = fullMessage.messageInfo;
    messageInfo.type             = TgMessageInfo::Type::Other;
    messageInfo.incomingGroupchatSender = getIncomingGroupchatSenderPurpleName(chat, *message, account);
    messageInfo.timestamp        = message->date_;
    messageInfo.outgoing         = message->is_outgoing_;
    messageInfo.sentLocally      = (message->sending_state_ != nullptr);
    messageInfo.repliedMessageId = getReplyMessageId(*message);

    if (message->forward_info_)
        messageInfo.forwardedFrom = getForwardSource(*message->forward_info_, account);

    if (message && message->content_) {
        if (message->content_->get_id() == td::td_api::messagePhoto::ID) {
            messageInfo.type = TgMessageInfo::Type::Photo;
            const td::td_api::messagePhoto &photo = static_cast<const td::td_api::messagePhoto &>(*message->content_);
            const td::td_api::file *file = selectPhotoSize(account.purpleAccount, photo);
            if (file)
                fullMessage.selectedPhotoSizeId = file->id_;
        } else if (message->content_->get_id() == td::td_api::messageSticker::ID)
            messageInfo.type = TgMessageInfo::Type::Sticker;
    }

    fullMessage.message = std::move(message);
}

static const td::td_api::file *getSelectedPhotoSize(const IncomingMessage &fullMessage,
                                                    const td::td_api::messagePhoto &photo)
{
    if (photo.photo_)
        for (const auto &newSize: photo.photo_->sizes_)
            if (newSize && newSize->photo_ && (newSize->photo_->id_ == fullMessage.selectedPhotoSizeId))
                return newSize->photo_.get();

    return nullptr;
}

static bool isInlineDownload(const IncomingMessage &fullMessage,
                             const td::td_api::MessageContent &content,
                             const td::td_api::chat &chat)
{
    return (content.get_id() == td::td_api::messagePhoto::ID) ||
           (content.get_id() == td::td_api::messageSticker::ID) ||
           !fullMessage.standardDownloadConfigured || !chat.type_ ||
           ((chat.type_->get_id() != td::td_api::chatTypePrivate::ID) &&
            (chat.type_->get_id() != td::td_api::chatTypeSecret::ID));
}

static bool inlineDownloadNeedAutoDl(const IncomingMessage &fullMessage,
                                     const td::td_api::file &file)
{
    unsigned fileSize = getFileSizeKb(file);

    return !((file.local_ && file.local_->is_downloading_completed_)) &&
           isSizeWithinLimit(fileSize, fullMessage.inlineFileSizeLimit);
}

static bool isFileMessageReady(const IncomingMessage &fullMessage, ChatId chatId,
                               const td::td_api::MessageContent &content,
                               const td::td_api::file &file, const TdAccountData &account)
{
    const td::td_api::chat *chat = account.getChat(chatId);
    if (!chat)
        return true;

    if (isInlineDownload(fullMessage, content, *chat)) {
        // File will be shown inline
        // Files above limit will either be ignored (in which case, message is ready)
        // or requested (in which case, don't try do display in order)
        return !inlineDownloadNeedAutoDl(fullMessage, file);
    } else
        // Standard libpurple transfer will be used, nothing to postpone
        return true;
}

void getFileFromMessage(const IncomingMessage &fullMessage, FileInfo &result)
{
    result.file = nullptr;
    result.caption = "";
    result.secret = false;
    if (!fullMessage.message || !fullMessage.message->content_)
        return;
    const td::td_api::message &message = *fullMessage.message;

    switch (message.content_->get_id()) {
        case td::td_api::messagePhoto::ID: {
            const td::td_api::messagePhoto &photo = static_cast<const td::td_api::messagePhoto &>(*message.content_);
            result.file = getSelectedPhotoSize(fullMessage, photo);
            result.name = ""; // will not be needed - inline download only
            if (photo.caption_) result.caption = photo.caption_->text_;
            // TRANSLATOR: File-type, used to describe what is being downloaded, in sentences like "Downloading photo" or "Ignoring photo download".
            result.description = _("photo");
            result.secret = photo.is_secret_;
            break;
        }
        case td::td_api::messageDocument::ID: {
            const td::td_api::messageDocument &document = static_cast<const td::td_api::messageDocument &>(*message.content_);
            result.file = document.document_ ? document.document_->document_.get() : nullptr;
            if (document.caption_) result.caption = document.caption_->text_;
            result.name = getFileName(document.document_.get());
            result.description = makeDocumentDescription(document.document_.get());
            break;
        }
        case td::td_api::messageVideo::ID: {
            const td::td_api::messageVideo &video = static_cast<const td::td_api::messageVideo &>(*message.content_);
            result.file = video.video_ ? video.video_->video_.get() : nullptr;
            if (video.caption_) result.caption = video.caption_->text_;
            result.name = getFileName(video.video_.get());
            result.description = makeDocumentDescription(video.video_.get());
            result.secret = video.is_secret_;
            break;
        }
        case td::td_api::messageAnimation::ID: {
            const td::td_api::messageAnimation &animation = static_cast<const td::td_api::messageAnimation &>(*message.content_);
            result.file = animation.animation_ ? animation.animation_->animation_.get() : nullptr;
            if (animation.caption_) result.caption = animation.caption_->text_;
            result.name = getFileName(animation.animation_.get());
            result.description = makeDocumentDescription(animation.animation_.get());
            result.secret = animation.is_secret_;
            break;
        }
        case td::td_api::messageAudio::ID: {
            const td::td_api::messageAudio &audio = static_cast<const td::td_api::messageAudio &>(*message.content_);
            result.file = audio.audio_ ? audio.audio_->audio_.get() : nullptr;
            if (audio.caption_) result.caption = audio.caption_->text_;
            result.name = getFileName(audio.audio_.get());
            result.description = makeDocumentDescription(audio.audio_.get());
            break;
        }
        case td::td_api::messageVoiceNote::ID: {
            const td::td_api::messageVoiceNote &audio = static_cast<const td::td_api::messageVoiceNote &>(*message.content_);
            result.file = audio.voice_note_ ? audio.voice_note_->voice_.get() : nullptr;
            if (audio.caption_) result.caption = audio.caption_->text_;
            result.name = getFileName(audio.voice_note_.get());
            result.description = makeDocumentDescription(audio.voice_note_.get());
            break;
        }
        case td::td_api::messageVideoNote::ID: {
            const td::td_api::messageVideoNote &video = static_cast<const td::td_api::messageVideoNote &>(*message.content_);
            result.file = video.video_note_ ? video.video_note_->video_.get() : nullptr;
            result.name = getFileName(video.video_note_.get());
            result.description = makeDocumentDescription(video.video_note_.get());
            result.secret = video.is_secret_;
            break;
        }
        case td::td_api::messageSticker::ID: {
            const td::td_api::messageSticker &sticker = static_cast<const td::td_api::messageSticker &>(*message.content_);
            result.file = sticker.sticker_ ? sticker.sticker_->sticker_.get() : nullptr;
            result.name = ""; // will not be needed - inline download only
            // TRANSLATOR: File-type, used to describe what is being downloaded, in sentences like "Downloading photo" or "Ignoring photo download".
            result.description = _("sticker");
            break;
        }
    }
}

bool isMessageReady(const IncomingMessage &fullMessage, const TdAccountData &account)
{
    if (!fullMessage.message) return true;
    const td::td_api::message &message = *fullMessage.message;
    ChatId chatId = getChatId(message);

    if (getReplyMessageId(message).valid() && !fullMessage.repliedMessage &&
        !fullMessage.repliedMessageFailed)
    {
        return false;
    }

    if (!message.content_) return true;

    return true; // dead code below

    FileInfo fileInfo;
    getFileFromMessage(fullMessage, fileInfo);
    // For stickers, will wait for sticker download to display the message, but not for animated sticker conversion
    if (fileInfo.file && !isFileMessageReady(fullMessage, chatId, *message.content_, *fileInfo.file, account))
        return false;

    return true;
}

void fetchExtras(IncomingMessage &fullMessage, TdTransceiver &transceiver, TdAccountData &account,
                 TdTransceiver::ResponseCb2 onFetchReply)
{
    if (!fullMessage.message) return;
    const td::td_api::message &message = *fullMessage.message;
    MessageId messageId      = getId(message);
    MessageId replyMessageId = getReplyMessageId(message);
    ChatId    chatId         = getChatId(message);
    const td::td_api::chat *chat = account.getChat(chatId);

    if (replyMessageId.valid()) {
        purple_debug_misc(config::pluginId, "Fetching message %" G_GINT64_FORMAT " which message %" G_GINT64_FORMAT " replies to\n",
                        replyMessageId.value(), messageId.value());
        auto getMessageReq = td::td_api::make_object<td::td_api::getMessage>();
        getMessageReq->chat_id_    = chatId.value();
        getMessageReq->message_id_ = replyMessageId.value();
        transceiver.sendQueryWithTimeout(std::move(getMessageReq), onFetchReply, 1);
    }

    FileInfo fileInfo;
    getFileFromMessage(fullMessage, fileInfo);
    if (fileInfo.file && message.content_ && chat && isInlineDownload(fullMessage, *message.content_, *chat) &&
        inlineDownloadNeedAutoDl(fullMessage, *fileInfo.file))
    {
    }
}
