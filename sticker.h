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
    TgMessageInfo m_message;
public:
    const std::string inputFileName;
    const ChatId chatId;
    StickerConversionThread(PurpleAccount *purpleAccount, const std::string &filename,
                            ChatId chatId, TgMessageInfo &&message)
    : AccountThread(purpleAccount), m_message(std::move(message)), inputFileName(filename),
        chatId(chatId) {}
    StickerConversionThread(PurpleAccount *purpleAccount, const std::string &filename,
                            ChatId chatId, const TgMessageInfo *message)
    : AccountThread(purpleAccount), inputFileName(filename), chatId(chatId)
    {
        if (message)
            m_message.assign(*message);
    }

    const std::string &getOutputFileName() const { return m_outputFileName; }
    const std::string &getErrorMessage()   const { return m_errorMessage; }
    const TgMessageInfo &message()         const { return m_message; }

    static void setCallback(Callback callback);
};

#endif
