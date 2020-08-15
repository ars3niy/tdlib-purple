#include "file-transfer.h"
#include "config.h"
#include "client-utils.h"
#include "format.h"
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

void startDocumentUpload(int64_t chatId, const std::string &filename, PurpleXfer *xfer,
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

static void updateDocumentUploadProgress(const td::td_api::file &file, PurpleXfer *xfer, int64_t chatId,
                                         TdTransceiver &transceiver, TdAccountData &account,
                                         TdTransceiver::ResponseCb sendMessageResponse);

void startDocumentUploadProgress(int64_t chatId, PurpleXfer *xfer, const td::td_api::file &file,
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

static void updateDocumentUploadProgress(const td::td_api::file &file, PurpleXfer *upload, int64_t chatId,
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
            sendMessageRequest->chat_id_ = chatId;

            int64_t  chatId = sendMessageRequest->chat_id_;
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

void startInlineDownloadProgress(DownloadRequest &request, TdTransceiver &transceiver, TdAccountData &account)
{
    purple_debug_misc(config::pluginId, "Tracking download progress of file id %d: downloaded %d/%d\n",
        (int)request.fileId, (int)request.downloadedSize, (int)request.fileSize);

    char *tempFileName = NULL;
    int fd = g_file_open_tmp("tdlib_download_XXXXXX", &tempFileName, NULL);
    if (fd < 0)
        return;

    request.tempFileName = tempFileName;
    PurpleXfer *xfer = purple_xfer_new (account.purpleAccount, PURPLE_XFER_RECEIVE, request.message.sender.c_str());
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
        account.addFileTransfer(request.fileId, xfer, 0);
    } else
        remove(tempFileName);

    purple_xfer_unref(xfer);
    g_free(tempFileName);
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
    int64_t     chatId;
    if (account.getFileTransfer(file.id_, xfer, chatId)) {
        if (xfer && (purple_xfer_get_type(xfer) == PURPLE_XFER_SEND))
            updateDocumentUploadProgress(file, xfer, chatId, transceiver, account, sendMessageResponse);
    }

    updateDownloadProgress(file, xfer, account);
}

void finishInlineDownloadProgress(DownloadRequest &downloadReq, TdAccountData& account)
{
    PurpleXfer *download;
    int64_t     chatId;

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

std::string getDownloadPath(const td::td_api::Object *downloadResponse)
{
    if (!downloadResponse)
        purple_debug_misc(config::pluginId, "No response after downloading file\n");
    else if (downloadResponse->get_id() == td::td_api::file::ID) {
        const td::td_api::file &file = static_cast<const td::td_api::file &>(*downloadResponse);
        if (!file.local_)
            purple_debug_misc(config::pluginId, "No local file info after downloading\n");
        else if (!file.local_->is_downloading_completed_)
            purple_debug_misc(config::pluginId, "File not completely downloaded\n");
        else
            return file.local_->path_;
    } else
        purple_debug_misc(config::pluginId, "Unexpected response to downloading file: id %d\n",
                          (int)downloadResponse->get_id());

    return "";
}

static void standardDownloadResponse(TdAccountData *account, uint64_t requestId,
                                     td::td_api::object_ptr<td::td_api::Object> object)
{
    std::unique_ptr<DownloadRequest> request = account->getPendingRequest<DownloadRequest>(requestId);
    std::string                      path    = getDownloadPath(object.get());
    if (!request) return;

    PurpleXfer *download;
    int64_t     chatId;

    if (account->getFileTransfer(request->fileId, download, chatId)) {
        std::unique_ptr<DownloadData> data(static_cast<DownloadData *>(download->data));
        download->data = NULL;

        gchar *content = NULL;
        gsize fileSize = 0;
        GError *error  = NULL;

        if (!path.empty() && g_file_get_contents(path.c_str(), &content, &fileSize, &error)) {
            purple_xfer_ref(download);
            purple_xfer_set_bytes_sent(download, 0);
            purple_xfer_set_size(download, fileSize);
            purple_xfer_write_file(download, reinterpret_cast<guchar *>(content), fileSize);
            // write_file can cancel the transfer - ref above prevents it from being freed
            if (!purple_xfer_is_canceled(download)) {
                purple_xfer_set_completed(download, TRUE);
                purple_xfer_end(download);
            }
            purple_xfer_unref(download);
            account->removeFileTransfer(request->fileId);
        } else {
            if (error) {
                // Unlikely error message not worth translating
                std::string message = formatMessage("Failed to read {}: {}", {path, std::string(error->message)});
                purple_debug_misc(config::pluginId, "%s\n", message.c_str());
                purple_xfer_error(PURPLE_XFER_RECEIVE, account->purpleAccount, download->who, message.c_str());
                g_error_free(error);
            }
            if (path.empty())
                purple_debug_warning(config::pluginId, "Incomplete file in download response for %s\n",
                                     purple_xfer_get_local_filename(download));
            purple_xfer_cancel_remote(download);
        }

        g_free(content);
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
        TgMessageInfo message;
        std::unique_ptr<DownloadRequest> request = std::make_unique<DownloadRequest>(requestId, 0,
                                                        message, fileId, 0, "", nullptr,
                                                        nullptr);
        data->account->addPendingRequest<DownloadRequest>(requestId, std::move(request));
        // Start immediately, because standardDownloadResponse will call purple_xfer_write_file, which
        // will fail if purple_xfer_start hasn't been called
        purple_xfer_start(xfer, -1, NULL, 0);
    }
}

void requestStandardDownload(const TgMessageInfo &message, const std::string &fileName,
                             const td::td_api::file &file, TdTransceiver &transceiver, TdAccountData &account)
{
    PurpleXfer *xfer = purple_xfer_new (account.purpleAccount, PURPLE_XFER_RECEIVE, message.sender.c_str());
    purple_xfer_set_init_fnc(xfer, startStandardDownload);
    purple_xfer_set_cancel_recv_fnc(xfer, cancelDownload);
    purple_xfer_set_filename(xfer, fileName.c_str());
    purple_xfer_set_size(xfer, getFileSize(file));
    xfer->data = new DownloadData(account, transceiver);
    account.addFileTransfer(file.id_, xfer, 0);
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
    return formatMessage(_("voice note [{}]"), document->mime_type_);
}

std::string makeDocumentDescription(const td::td_api::videoNote *document)
{
    if (!document)
        // Unlikely error message not worth translating
        return "faulty voice note";
    return formatMessage(_("video note [{} s]"), document->duration_);
}

std::string getFileName(const td::td_api::voiceNote* document)
{
    td::Client::Response resp = td::Client::execute({0, td::td_api::make_object<td::td_api::getFileExtension>(document->mime_type_)});
    if (resp.object && (resp.object->get_id() == td::td_api::text::ID))
        return std::string(_("voiceNote")) + '.' + static_cast<const td::td_api::text &>(*resp.object).text_;
    return _("voiceNote");
}

std::string getFileName(const td::td_api::videoNote *document)
{
    return std::string(_("videoNote")) + ".avi";
}
