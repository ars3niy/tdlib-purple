#include "file-transfer.h"
#include "config.h"
#include "client-utils.h"
#include "format.h"
#include "receiving.h"
#include "sticker.h"
#include "purple-info.h"
#include <unistd.h>

enum {
    FILE_UPLOAD_PRIORITY = 1,
};

bool saveImage(int id, char **fileName)
{
    *fileName = NULL;
    char *tempFileName = NULL;

    PurpleStoredImage *psi = purple_imgstore_find_by_id (id);
    if (!psi) {
        purple_debug_misc(config::pluginId, "Failed to send image: id %d not found\n", id);
        return false;
    }
    int fd = g_file_open_tmp("tdlib_upload_XXXXXX", &tempFileName, NULL);
    if (fd < 0) {
        purple_debug_misc(config::pluginId, "Failed to send image: could not create temporary file\n");
        return false;
    }
    ssize_t len = write(fd, purple_imgstore_get_data (psi), purple_imgstore_get_size (psi));
    close(fd);
    if (len != (ssize_t)purple_imgstore_get_size(psi)) {
        purple_debug_misc(config::pluginId, "Failed to send image: could not write temporary file\n");
        remove(tempFileName);
        g_free(tempFileName);
        return false;
    }

    *fileName = tempFileName;
    return true;
}

void startDocumentUpload(ChatId chatId, const std::string &filename, PurpleXfer *xfer,
                         TdTransceiver &transceiver, TdAccountData &account,
                         TdTransceiver::ResponseCb response)
{
    auto uploadRequest = td::td_api::make_object<td::td_api::uploadFile>();
    uploadRequest->file_ = td::td_api::make_object<td::td_api::inputFileLocal>(filename);
    uploadRequest->file_type_ = td::td_api::make_object<td::td_api::fileTypeDocument>();
    uploadRequest->priority_ = FILE_UPLOAD_PRIORITY;
    purple_xfer_ref(xfer);
    uint64_t requestId = transceiver.sendQuery(std::move(uploadRequest), response);
    account.addPendingRequest<UploadRequest>(requestId, xfer, chatId);
}

static void updateDocumentUploadProgress(const td::td_api::file &file, PurpleXfer *xfer, ChatId chatId,
                                         TdTransceiver &transceiver, TdAccountData &account,
                                         TdTransceiver::ResponseCb sendMessageResponse);

void startDocumentUploadProgress(ChatId chatId, PurpleXfer *xfer, const td::td_api::file &file,
                                 TdTransceiver &transceiver, TdAccountData &account,
                                 TdTransceiver::ResponseCb sendMessageResponse)
{
    if (purple_xfer_is_canceled(xfer)) {
        // Someone managed to cancel the upload REAL fast
        auto cancelRequest = td::td_api::make_object<td::td_api::cancelUploadFile>(file.id_);
        transceiver.sendQuery(std::move(cancelRequest), nullptr);
        purple_xfer_unref(xfer);
    } else {
        purple_debug_misc(config::pluginId, "Got file id %d for uploading %s\n", (int)file.id_,
                            purple_xfer_get_local_filename(xfer));
        account.addFileTransfer(file.id_, xfer, chatId);
        updateDocumentUploadProgress(file, xfer, chatId, transceiver, account, sendMessageResponse);
    }
}

void uploadResponseError(PurpleXfer *xfer, const std::string &message, TdAccountData &account)
{
    purple_xfer_cancel_remote(xfer);
    purple_xfer_error(purple_xfer_get_type(xfer), account.purpleAccount,
                      purple_xfer_get_remote_user(xfer), message.c_str());
    purple_xfer_unref(xfer);
}

static void updateDocumentUploadProgress(const td::td_api::file &file, PurpleXfer *upload, ChatId chatId,
                                         TdTransceiver &transceiver, TdAccountData &account,
                                         TdTransceiver::ResponseCb sendMessageResponse)
{
    size_t fileSize = purple_xfer_get_size(upload);

    if (file.remote_) {
        if (file.remote_->is_uploading_active_) {
            if (purple_xfer_get_status(upload) != PURPLE_XFER_STATUS_STARTED) {
                purple_debug_misc(config::pluginId, "Started uploading %s\n", purple_xfer_get_local_filename(upload));
                purple_xfer_start(upload, -1, NULL, 0);
            }
            size_t bytesSent = std::max(0, file.remote_->uploaded_size_);
            purple_xfer_set_bytes_sent(upload, std::min(fileSize, bytesSent));
            purple_xfer_update_progress(upload);
        } else if (file.local_ && (file.remote_->uploaded_size_ == file.local_->downloaded_size_)) {
            purple_debug_misc(config::pluginId, "Finishing uploading %s\n", purple_xfer_get_local_filename(upload));
            purple_xfer_set_bytes_sent(upload, fileSize);
            purple_xfer_set_completed(upload, TRUE);
            purple_xfer_end(upload);
            purple_xfer_unref(upload);
            account.removeFileTransfer(file.id_);
            auto sendMessageRequest = td::td_api::make_object<td::td_api::sendMessage>();
            auto content = td::td_api::make_object<td::td_api::inputMessageDocument>();
            content->caption_ = td::td_api::make_object<td::td_api::formattedText>();
            content->document_ = td::td_api::make_object<td::td_api::inputFileId>(file.id_);
            sendMessageRequest->input_message_content_ = std::move(content);
            sendMessageRequest->chat_id_ = chatId.value();

            uint64_t requestId = transceiver.sendQuery(std::move(sendMessageRequest), sendMessageResponse);
            account.addPendingRequest<SendMessageRequest>(requestId, chatId, nullptr);
        }
    } else {
        purple_xfer_cancel_remote(upload);
        purple_xfer_unref(upload);
        account.removeFileTransfer(file.id_);
    }
}

struct DownloadData {
    TdAccountData *account;
    TdTransceiver *transceiver;

    DownloadData(TdAccountData &account, TdTransceiver &transceiver)
    : account(&account), transceiver(&transceiver) {}
};

static void nop(PurpleXfer *xfer)
{
}

static void cancelDownload(PurpleXfer *xfer)
{
    std::unique_ptr<DownloadData> data(static_cast<DownloadData *>(xfer->data));
    xfer->data = NULL;
    if (!data) return;

    int32_t fileId;
    if (data->account->getFileIdForTransfer(xfer, fileId)) {
        purple_debug_misc(config::pluginId, "Cancelling download of %s (file id %d)\n",
                            purple_xfer_get_local_filename(xfer), fileId);
        auto cancelRequest = td::td_api::make_object<td::td_api::cancelDownloadFile>();
        cancelRequest->file_id_ = fileId;
        cancelRequest->only_if_pending_ = false;
        data->transceiver->sendQuery(std::move(cancelRequest), nullptr);
        data->account->removeFileTransfer(fileId);
    }
}

static void finishInlineDownloadProgress(DownloadRequest &downloadReq, TdAccountData& account)
{
    PurpleXfer *download;
    ChatId      chatId;

    if (account.getFileTransfer(downloadReq.fileId, download, chatId)) {
        std::unique_ptr<DownloadData> data(static_cast<DownloadData *>(download->data));
        download->data = NULL;
        purple_xfer_set_bytes_sent(download, downloadReq.fileSize);
        purple_xfer_set_completed(download, TRUE);
        purple_xfer_end(download);
        account.removeFileTransfer(downloadReq.fileId);
    }

    if (downloadReq.tempFd >= 0) {
        close(downloadReq.tempFd);
        downloadReq.tempFd = -1;
    }
    if (!downloadReq.tempFileName.empty()) {
        remove(downloadReq.tempFileName.c_str());
        downloadReq.tempFileName.clear();
    }
}

static void inlineDownloadResponse(uint64_t requestId,
                                   td::td_api::object_ptr<td::td_api::Object> object,
                                   TdTransceiver &transceiver, TdAccountData &account)
{
    std::unique_ptr<DownloadRequest> request = account.getPendingRequest<DownloadRequest>(requestId);

    if (request) {
        std::string path = getDownloadPath(object);
        finishInlineDownloadProgress(*request, account);
        IncomingMessage *pendingMessage = account.pendingMessages.findPendingMessage(request->chatId, request->message.id);

        if (pendingMessage) {
            // Quick download response while message still in PendingMessageQueue
            const td::td_api::file *replacementFile = nullptr;

            if (pendingMessage->message && pendingMessage->message->content_ &&
                (pendingMessage->message->content_->get_id() == td::td_api::messageSticker::ID) &&
                isStickerAnimated(path))
            {
                if (shouldConvertAnimatedSticker(pendingMessage->messageInfo, account.purpleAccount)) {
                    StickerConversionThread *thread;
                    thread = new StickerConversionThread(account.purpleAccount, path, getChatId(*pendingMessage->message),
                                                         &pendingMessage->messageInfo);
                    thread->startThread();
                } else
                    replacementFile = pendingMessage->thumbnail.get();
            }

            if (replacementFile)
                // TODO: if thumbnail already downloaded, mark ready and don't download
                downloadFileInline(replacementFile->id_, request->chatId, request->message,
                                   request->fileDescription, nullptr, transceiver, account);
            else {
                pendingMessage->inlineDownloadComplete = true;
                pendingMessage->inlineDownloadedFilePath = path;
                checkMessageReady(pendingMessage, transceiver, account);
                pendingMessage = nullptr;
            }
        } else {
            // Message no longer in PendingMessageQueue
            if (!path.empty())
                showDownloadedFileInline(request->chatId, request->message, path, NULL,
                                         request->fileDescription, std::move(request->thumbnail),
                                         transceiver, account);
        }
    }
}

static void startInlineDownloadProgress(DownloadRequest &request, TdTransceiver &transceiver,
                                        TdAccountData &account)
{
    purple_debug_misc(config::pluginId, "Tracking download progress of file id %d: downloaded %d/%d\n",
        (int)request.fileId, (int)request.downloadedSize, (int)request.fileSize);

    char *tempFileName = NULL;
    int fd = g_file_open_tmp("tdlib_download_XXXXXX", &tempFileName, NULL);
    if (fd < 0)
        return;

    request.tempFileName = tempFileName;
    std::string who = getDownloadXferPeerName(request.chatId, request.message, account);
    PurpleXfer *xfer = purple_xfer_new (account.purpleAccount, PURPLE_XFER_RECEIVE, who.c_str());
    purple_xfer_set_init_fnc(xfer, nop);
    purple_xfer_set_cancel_recv_fnc(xfer, nop);
    purple_xfer_set_filename(xfer, request.fileDescription.c_str());
    purple_xfer_request_accepted(xfer, tempFileName);
    purple_xfer_set_size(xfer, request.fileSize);
    purple_xfer_set_bytes_sent(xfer, request.downloadedSize);

    purple_xfer_ref(xfer);

    if (request.downloadedSize) {
        close(fd);
        // purple_xfer_start will create file passed to purple_xfer_request_accepted and fail the
        // transfer if could not be created. Thus we do our best to give it path to a temporary file
        // that it will be able to create. If it somehow does fail then cancel handler isn't set yet
        // so the actual download won't be cancelled.
        purple_xfer_start(xfer, -1, NULL, 0);
    } else
        request.tempFd = fd;

    if (!purple_xfer_is_canceled(xfer)) {
        // purple_xfer_start didn't fail to re-open the temporary file, so now really start tracking
        // the progress
        xfer->data = new DownloadData(account, transceiver);
        purple_xfer_set_cancel_recv_fnc(xfer, cancelDownload);
        account.addFileTransfer(request.fileId, xfer, ChatId::invalid);
    } else
        remove(tempFileName);

    purple_xfer_unref(xfer);
    g_free(tempFileName);
}

static void handleLongInlineDownload(uint64_t requestId, TdTransceiver &transceiver,
                                     TdAccountData &account)
{
    DownloadRequest *pRequest = account.findPendingRequest<DownloadRequest>(requestId);
    if (pRequest) {
        const char *option = purple_account_get_string(account.purpleAccount, AccountOptions::DownloadBehaviour,
                                                       AccountOptions::DownloadBehaviourDefault());
        if (!strcmp(option, AccountOptions::DownloadBehaviourHyperlink))
            // We didn't want inline downloads, but got one anyway because it's image or sticker.
            // At least don't get the fake file transfer going, because that tends to get bitlbee
            // and spectrum in trouble.
            startInlineDownloadProgress(*pRequest, transceiver, account);

        IncomingMessage *pendingMessage = account.pendingMessages.findPendingMessage(pRequest->chatId, pRequest->message.id);
        if (pendingMessage) {

            pendingMessage->inlineDownloadTimeout = true;
            std::vector<IncomingMessage> readyMessages;
            checkMessageReady(pendingMessage, transceiver, account, &readyMessages);
            pendingMessage = nullptr;

            // Now after "Downloading..." notification has been displayed (which may have been
            // accompanied by file caption, if any, in which case it needs reply source if it was a
            // reply), we can move reply source from no-longer-pending IncomingMessage onto
            // DownloadRequest, so that citation can be displayed again when displaying hyperlink.
            // If the message is a reply but fetching reply source hasn't produced a response yet
            // at this point, a successful such response may technically yet come in which case we
            // will lose the reply source. But this is extremely unlikely, and not even a problem.
            for (IncomingMessage &pendingMessage: readyMessages)
                if (pendingMessage.message && (getId(*pendingMessage.message) == pRequest->message.id)) {
                    pRequest->message.repliedMessage = std::move(pendingMessage.repliedMessage);
                    pRequest->thumbnail = std::move(pendingMessage.thumbnail);
                }
        }
    }
}

void downloadFileInline(int32_t fileId, ChatId chatId, TgMessageInfo &message,
                        const std::string &fileDescription,
                        td::td_api::object_ptr<td::td_api::file> thumbnail,
                        TdTransceiver &transceiver, TdAccountData &account)
{
    td::td_api::object_ptr<td::td_api::downloadFile> downloadReq =
        td::td_api::make_object<td::td_api::downloadFile>();
    downloadReq->file_id_     = fileId;
    downloadReq->priority_    = FILE_DOWNLOAD_PRIORITY;
    downloadReq->offset_      = 0;
    downloadReq->limit_       = 0;
    downloadReq->synchronous_ = true;

    uint64_t requestId = transceiver.sendQuery(
        std::move(downloadReq),
        [&transceiver, &account](uint64_t reqId, td::td_api::object_ptr<td::td_api::Object> object) {
            inlineDownloadResponse(reqId, std::move(object), transceiver, account);
        });
    std::unique_ptr<DownloadRequest> request = std::make_unique<DownloadRequest>(requestId, chatId,
                                               message, fileId, 0, fileDescription, thumbnail.release());

    account.addPendingRequest(requestId, std::move(request));
    transceiver.setQueryTimer(requestId,
                              [&transceiver, &account](uint64_t reqId, td::td_api::object_ptr<td::td_api::Object>) {
                                  handleLongInlineDownload(reqId, transceiver, account);
                              }, 1, false);
}

static void updateDownloadProgress(const td::td_api::file &file, PurpleXfer *xfer, TdAccountData &account)
{
    DownloadRequest *downloadReq = account.findDownloadRequest(file.id_);
    if (!downloadReq)
        return;

    unsigned fileSize       = getFileSize(file);
    int32_t  downloadedSize = std::max(0, file.local_ ? file.local_->downloaded_size_ : 0);

    if (xfer) {
        purple_xfer_set_size(xfer, fileSize);

        if ((downloadedSize != 0) && (downloadReq->downloadedSize == 0)) {
            // For "inline" file downloads with fake-file-name PurpleXfer tracking progress,
            // both if below should evaluate to true - close the fake file and start transfer
            // (which reopens the fake file).
            // For downloads using PurpleXfer in standard way, both if should evaluate to false:
            // purple_xfer_start is called when downloadFile request is sent.
            if (downloadReq->tempFd >= 0)
                close(downloadReq->tempFd);
            downloadReq->tempFd = -1;
            if (purple_xfer_get_status(xfer) != PURPLE_XFER_STATUS_STARTED)
                purple_xfer_start(xfer, -1, NULL, 0);
        }

        purple_xfer_set_bytes_sent(xfer, downloadedSize);
        purple_xfer_update_progress(xfer);
    }

    downloadReq->fileSize = fileSize;
    downloadReq->downloadedSize = downloadedSize;
}

void updateFileTransferProgress(const td::td_api::file &file, TdTransceiver &transceiver,
                                TdAccountData &account, TdTransceiver::ResponseCb sendMessageResponse)
{
    PurpleXfer *xfer = NULL;
    ChatId      chatId;
    if (account.getFileTransfer(file.id_, xfer, chatId)) {
        if (xfer && (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND))
            updateDocumentUploadProgress(file, xfer, chatId, transceiver, account, sendMessageResponse);
    }

    updateDownloadProgress(file, xfer, account);
}

std::string getDownloadPath(const td::td_api::object_ptr<td::td_api::Object> &downloadResponse)
{
    if (downloadResponse && (downloadResponse->get_id() == td::td_api::file::ID)) {
        const td::td_api::file &file = static_cast<const td::td_api::file &>(*downloadResponse);
        if (!file.local_)
            purple_debug_warning(config::pluginId, "No local file info after downloading\n");
        else if (!file.local_->is_downloading_completed_)
            purple_debug_warning(config::pluginId, "File not completely downloaded\n");
        else
            return file.local_->path_;
    } else {
        std::string message = getDisplayedError(downloadResponse);
        purple_debug_warning(config::pluginId, "Error downloading file: %s\n", message.c_str());
    }

    return "";
}

struct DownloadWrapup {
    PurpleXfer *download;
    FILE       *tdlibFile;
    std::string tdlibPath;
};

static gboolean wrapupDownload(void *data)
{
    DownloadWrapup *wrapupData = static_cast<DownloadWrapup *>(data);
    unsigned chunkSize = AccountThread::isSingleThread() ? 10 : 1048576;

    bool last = false;
    if (!purple_xfer_is_canceled(wrapupData->download)) {
        if (purple_xfer_get_bytes_sent(wrapupData->download) + chunkSize >= purple_xfer_get_size(wrapupData->download)) {
            last = true;
            chunkSize = purple_xfer_get_size(wrapupData->download) - purple_xfer_get_bytes_sent(wrapupData->download);
        }

        uint8_t *buf = new uint8_t[chunkSize];
        unsigned bytesRead = fread(buf, 1, chunkSize, wrapupData->tdlibFile);
        if (bytesRead < chunkSize) {
            // Unlikely error message not worth translating
            std::string message = formatMessage("Failed to download {}: error reading {} after {} bytes",
                                                {purple_xfer_get_local_filename(wrapupData->download),
                                                wrapupData->tdlibPath,
                                                std::to_string(purple_xfer_get_bytes_sent(wrapupData->download) + bytesRead)});
            purple_debug_warning(config::pluginId, "%s\n", message.c_str());
            purple_xfer_error(PURPLE_XFER_RECEIVE, purple_xfer_get_account(wrapupData->download),
                              wrapupData->download->who, message.c_str());
            last = true;
        }

        purple_xfer_write_file(wrapupData->download, buf, bytesRead);
        delete[] buf;

        if (last) {
            purple_xfer_set_completed(wrapupData->download, TRUE);
            purple_xfer_end(wrapupData->download);
        }
    } else
        last = true;

    if (last) {
        purple_xfer_unref(wrapupData->download);
        fclose(wrapupData->tdlibFile);
        delete wrapupData;
        return G_SOURCE_REMOVE;
    } else
        return G_SOURCE_CONTINUE;
}

static void standardDownloadResponse(TdAccountData *account, uint64_t requestId,
                                     td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<DownloadRequest> request = account->getPendingRequest<DownloadRequest>(requestId);
    std::string                      path    = getDownloadPath(object);
    if (!request) return;

    PurpleXfer *download;
    ChatId      chatId;

    if (account->getFileTransfer(request->fileId, download, chatId)) {
        std::unique_ptr<DownloadData> data(static_cast<DownloadData *>(download->data));
        download->data = NULL;
        account->removeFileTransfer(request->fileId);

        FILE *f = NULL;
        if (!path.empty())
            f = fopen(path.c_str(), "r");

        if (f) {
            purple_xfer_set_bytes_sent(download, 0);
            long fileSize;
            if (fseek(f, 0, SEEK_END) == 0) {
                fileSize = ftell(f);
                if (fileSize >= 0)
                    purple_xfer_set_size(download, fileSize);
                fseek(f, 0, SEEK_SET);
            }

            DownloadWrapup *idleData = new DownloadWrapup;
            idleData->download = download;
            idleData->tdlibFile = f;
            idleData->tdlibPath = path;
            purple_xfer_ref(download);
            if (AccountThread::isSingleThread()) {
                while (wrapupDownload(idleData) == G_SOURCE_CONTINUE) ;
            } else
                g_idle_add(wrapupDownload, idleData);
        } else {
            if (!path.empty()) {
                // Unlikely error message not worth translating
                std::string message = formatMessage("Failed to open {}: {}", {path, std::string(strerror(errno))});
                purple_debug_misc(config::pluginId, "%s\n", message.c_str());
                purple_xfer_error(PURPLE_XFER_RECEIVE, account->purpleAccount, download->who, message.c_str());
            }
            if (path.empty())
                purple_debug_warning(config::pluginId, "Incomplete file in download response for %s\n",
                                     purple_xfer_get_local_filename(download));
            purple_xfer_cancel_remote(download);
        }
    }
}

static void startStandardDownload(PurpleXfer *xfer)
{
    DownloadData *data = static_cast<DownloadData *>(xfer->data);
    if (!data) return;

    int32_t fileId;
    if (data->account->getFileIdForTransfer(xfer, fileId)) {
        td::td_api::object_ptr<td::td_api::downloadFile> downloadReq =
            td::td_api::make_object<td::td_api::downloadFile>();
        downloadReq->file_id_     = fileId;
        downloadReq->priority_    = FILE_DOWNLOAD_PRIORITY;
        downloadReq->offset_      = 0;
        downloadReq->limit_       = 0;
        downloadReq->synchronous_ = true;

        uint64_t requestId = data->transceiver->sendQuery(std::move(downloadReq),
                                                          [account=data->account](uint64_t requestId, td::td_api::object_ptr<td::td_api::Object> object) {
                                                              standardDownloadResponse(account, requestId, std::move(object));
                                                          });
        TgMessageInfo messageInfo;
        std::unique_ptr<DownloadRequest> request = std::make_unique<DownloadRequest>(requestId,
                                                        ChatId::invalid,
                                                        messageInfo, fileId, 0, "", nullptr);
        data->account->addPendingRequest(requestId, std::move(request));
        // Start immediately, because standardDownloadResponse will call purple_xfer_write_file, which
        // will fail if purple_xfer_start hasn't been called
        purple_xfer_start(xfer, -1, NULL, 0);
    }
}

void requestStandardDownload(ChatId chatId, const TgMessageInfo &message, const std::string &fileName,
                             const td::td_api::file &file, TdTransceiver &transceiver, TdAccountData &account)
{
    std::string who = getDownloadXferPeerName(chatId, message, account);
    PurpleXfer *xfer = purple_xfer_new (account.purpleAccount, PURPLE_XFER_RECEIVE, who.c_str());
    purple_xfer_set_init_fnc(xfer, startStandardDownload);
    purple_xfer_set_cancel_recv_fnc(xfer, cancelDownload);
    purple_xfer_set_filename(xfer, fileName.c_str());
    purple_xfer_set_size(xfer, getFileSize(file));
    xfer->data = new DownloadData(account, transceiver);
    account.addFileTransfer(file.id_, xfer, ChatId::invalid);
    purple_xfer_request(xfer);
}

unsigned getFileSize(const td::td_api::file &file)
{
    int32_t size = file.size_;
    if (size == 0)
        size = file.expected_size_;

    if (size <= 0)
        return 0;
    else
        return size;
}

unsigned getFileSizeKb(const td::td_api::file &file)
{
    return getFileSize(file)/1024;
}

std::string makeDocumentDescription(const td::td_api::voiceNote *document)
{
    if (!document)
        // Unlikely error message not worth translating
        return "faulty voice note";
    // TRANSLATOR: In-line document type. Argument will be a mime type.
    return formatMessage(_("voice note [{}]"), document->mime_type_);
}

std::string makeDocumentDescription(const td::td_api::videoNote *document)
{
    if (!document)
        // Unlikely error message not worth translating
        return "faulty voice note";
    // TRANSLATOR: In-line document type. Argument will be a duration.
    return formatMessage(_("video note [{}]"), formatDuration(document->duration_));
}

std::string getFileName(const td::td_api::voiceNote* document)
{
    td::Client::Response resp = td::Client::execute({0, td::td_api::make_object<td::td_api::getFileExtension>(document->mime_type_)});
    if (resp.object && (resp.object->get_id() == td::td_api::text::ID)) {
        // TRANSLATOR: Filename. Keep it short, and as few special characters as possible.
        return std::string(_("voiceNote")) + '.' + static_cast<const td::td_api::text &>(*resp.object).text_;
    }
    return _("voiceNote");
}

std::string getFileName(const td::td_api::videoNote *document)
{
    // TRANSLATOR: Filename. Keep it short, and as few special characters as possible.
    return std::string(_("videoNote")) + ".avi";
}
