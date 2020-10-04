#ifndef _FILE_TRANSFER_H
#define _FILE_TRANSFER_H

#include "account-data.h"
#include "transceiver.h"

enum {
    FILE_DOWNLOAD_PRIORITY       = 1,
};

bool saveImage(int id, char **fileName);
void startDocumentUpload(ChatId chatId, const std::string &filename, PurpleXfer *xfer,
                         TdTransceiver &transceiver, TdAccountData &account,
                         TdTransceiver::ResponseCb response);
void uploadResponseError(PurpleXfer *xfer, const std::string &message, TdAccountData &account);
void startDocumentUploadProgress(ChatId chatId, PurpleXfer *xfer, const td::td_api::file &file,
                                 TdTransceiver &transceiver, TdAccountData &account,
                                 TdTransceiver::ResponseCb sendMessageResponse);
void startInlineDownloadProgress(DownloadRequest &request, TdTransceiver &transceiver, TdAccountData &account);
void updateFileTransferProgress(const td::td_api::file &file, TdTransceiver &transceiver,
                                TdAccountData &account, TdTransceiver::ResponseCb sendMessageResponse);
void finishInlineDownloadProgress(DownloadRequest &downloadReq, TdAccountData &account);

void requestStandardDownload(ChatId chatId, const TgMessageInfo &message, const std::string &fileName,
                             const td::td_api::file &file, TdTransceiver &transceiver, TdAccountData &account);
std::string getDownloadPath(const td::td_api::Object *downloadResponse);

unsigned getFileSize(const td::td_api::file &file);
unsigned getFileSizeKb(const td::td_api::file &file);

template<typename DocumentType>
std::string makeDocumentDescription(const DocumentType *document)
{
    if (!document)
        // Unlikely error message not worth translating
        return "(faulty file)";
    return document->file_name_ + " [" + document->mime_type_ + "]";
}

template<typename DocumentType>
std::string getFileName(const DocumentType *document)
{
    return document->file_name_;
}

std::string getFileName(const td::td_api::voiceNote *document);
std::string getFileName(const td::td_api::videoNote *document);

std::string makeDocumentDescription(const td::td_api::voiceNote *document);
std::string makeDocumentDescription(const td::td_api::videoNote *document);

#endif
