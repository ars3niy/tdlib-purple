#ifndef _FILE_TRANSFER_H
#define _FILE_TRANSFER_H

#include "account-data.h"
#include "transceiver.h"

bool saveImage(int id, char **fileName);
void startDocumentUpload(int64_t chatId, const std::string &filename, PurpleXfer *xfer,
                         TdTransceiver &transceiver, TdAccountData &account,
                         TdTransceiver::ResponseCb response);
void uploadResponseError(PurpleXfer *xfer, const std::string &message, TdAccountData &account);
void startDocumentUploadProgress(int64_t chatId, PurpleXfer *xfer, const td::td_api::file &file,
                                 TdTransceiver &transceiver, TdAccountData &account,
                                 TdTransceiver::ResponseCb sendMessageResponse);
void startDownloadProgress(DownloadRequest &request, TdTransceiver &transceiver, TdAccountData &account);
void updateFileTransferProgress(const td::td_api::file &file, TdTransceiver &transceiver,
                                TdAccountData &account, TdTransceiver::ResponseCb sendMessageResponse);
void finishDownloadProgress(DownloadRequest &downloadReq, TdAccountData &account);

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

std::string makeDocumentDescription(const td::td_api::voiceNote *document);
std::string makeDocumentDescription(const td::td_api::videoNote *document);

#endif
