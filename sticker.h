#ifndef _STICKER_H
#define _STICKER_H

#include "client-utils.h"

void showWebpSticker(const td::td_api::chat &chat, const TgMessageInfo &message,
                     const std::string &filePath, const std::string &fileDescription,
                     TdAccountData &account);

class StickerConversionThread: public AccountThread {
private:
    std::string   m_errorMessage;
    std::string   m_outputFileName;
    void run() override;

    static Callback g_callback;
    void callback(PurpleTdClient *tdClient) override;
public:
    const std::string inputFileName;
    const ChatId chatId;
    const TgMessageInfo message;
    StickerConversionThread(PurpleAccount *purpleAccount, const std::string &filename,
                            ChatId chatId, TgMessageInfo &&message)
    : AccountThread(purpleAccount), inputFileName(filename), chatId(chatId),
      message(std::move(message)) {}

    const std::string &getOutputFileName() const { return m_outputFileName; }
    const std::string &getErrorMessage()   const { return m_errorMessage; }

    static void setCallback(Callback callback);
};

#endif
