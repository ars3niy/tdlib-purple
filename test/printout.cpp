#include "printout.h"

using namespace td::td_api;

std::string requestToString(const td::TlObject &req)
{
#define C(class) case class::ID: return #class;
    switch (req.get_id()) {
        C(acceptCall)
        C(acceptTermsOfService)
        C(addChatMember)
        C(addChatMembers)
        C(addContact)
        C(addCustomServerLanguagePack)
        C(addFavoriteSticker)
        C(addLocalMessage)
        C(addLogMessage)
        C(addNetworkStatistics)
        C(addProxy)
        C(addRecentSticker)
        C(addRecentlyFoundChat)
        C(addSavedAnimation)
        C(addStickerToSet)
        C(answerCallbackQuery)
        C(answerCustomQuery)
        C(answerInlineQuery)
        C(answerPreCheckoutQuery)
        C(answerShippingQuery)
        C(blockUser)
        C(canTransferOwnership)
        C(cancelDownloadFile)
        C(cancelUploadFile)
        C(changeImportedContacts)
        C(changePhoneNumber)
        C(changeStickerSet)
        C(checkAuthenticationBotToken)
        C(checkAuthenticationCode)
        C(checkAuthenticationPassword)
        C(checkChangePhoneNumberCode)
        C(checkChatInviteLink)
        C(checkChatUsername)
        C(checkCreatedPublicChatsLimit)
        C(checkDatabaseEncryptionKey)
        C(checkEmailAddressVerificationCode)
        C(checkPhoneNumberConfirmationCode)
        C(checkPhoneNumberVerificationCode)
        C(checkRecoveryEmailAddressCode)
        C(cleanFileName)
        C(clearAllDraftMessages)
        C(clearImportedContacts)
        C(clearRecentStickers)
        C(clearRecentlyFoundChats)
        C(close)
        C(closeChat)
        C(closeSecretChat)
        C(confirmQrCodeAuthentication)
        C(createBasicGroupChat)
        C(createCall)
        C(createChatInviteLink)
        C(createNewBasicGroupChat)
        C(createNewSecretChat)
        C(createNewStickerSet)
        C(createNewSupergroupChat)
        C(createPrivateChat)
        C(createSecretChat)
        C(createSupergroupChat)
        C(createTemporaryPassword)
        C(deleteAccount)
        C(deleteChat)
        C(deleteChatHistory)
        C(deleteChatMessagesFromUser)
        C(deleteChatReplyMarkup)
        C(deleteFile)
        C(deleteLanguagePack)
        C(deleteMessages)
        C(deletePassportElement)
        C(deleteProfilePhoto)
        C(deleteSavedCredentials)
        C(deleteSavedOrderInfo)
        C(destroy)
        C(disableProxy)
        C(discardCall)
        C(disconnectAllWebsites)
        C(disconnectWebsite)
        C(downloadFile)
        C(editCustomLanguagePackInfo)
        C(editInlineMessageCaption)
        C(editInlineMessageLiveLocation)
        C(editInlineMessageMedia)
        C(editInlineMessageReplyMarkup)
        C(editInlineMessageText)
        C(editMessageCaption)
        C(editMessageLiveLocation)
        C(editMessageMedia)
        C(editMessageReplyMarkup)
        C(editMessageSchedulingState)
        C(editMessageText)
        C(editProxy)
        C(enableProxy)
        C(finishFileGeneration)
        C(forwardMessages)
        C(getAccountTtl)
        C(getActiveLiveLocationMessages)
        C(getActiveSessions)
        C(getAllPassportElements)
        C(getApplicationConfig)
        C(getArchivedStickerSets)
        C(getAttachedStickerSets)
        C(getAuthorizationState)
        C(getAutoDownloadSettingsPresets)
        C(getBackgroundUrl)
        C(getBackgrounds)
        C(getBasicGroup)
        C(getBasicGroupFullInfo)
        C(getBlockedUsers)
        C(getCallbackQueryAnswer)
        C(getChat)
        C(getChatAdministrators)
        C(getChatEventLog)
        C(getChatHistory)
        C(getChatMember)
        C(getChatMessageByDate)
        C(getChatMessageCount)
        C(getChatNotificationSettingsExceptions)
        C(getChatPinnedMessage)
        C(getChatScheduledMessages)
        C(getChatStatisticsUrl)
        C(getChats)
        C(getConnectedWebsites)
        C(getContacts)
        C(getCountryCode)
        C(getCreatedPublicChats)
        C(getCurrentState)
        C(getDatabaseStatistics)
        C(getDeepLinkInfo)
        C(getEmojiSuggestionsUrl)
        C(getFavoriteStickers)
        C(getFile)
        C(getFileDownloadedPrefixSize)
        C(getFileExtension)
        C(getFileMimeType)
        C(getGameHighScores)
        C(getGroupsInCommon)
        C(getImportedContactCount)
        C(getInactiveSupergroupChats)
        C(getInlineGameHighScores)
        C(getInlineQueryResults)
        C(getInstalledStickerSets)
        C(getInviteText)
        C(getJsonString)
        C(getJsonValue)
        C(getLanguagePackInfo)
        C(getLanguagePackString)
        C(getLanguagePackStrings)
        C(getLocalizationTargetInfo)
        C(getLogStream)
        C(getLogTagVerbosityLevel)
        C(getLogTags)
        C(getLogVerbosityLevel)
        C(getLoginUrl)
        C(getLoginUrlInfo)
        C(getMapThumbnailFile)
        C(getMe)
        C(getMessage)
        C(getMessageLink)
        C(getMessageLinkInfo)
        C(getMessageLocally)
        C(getMessages)
        C(getNetworkStatistics)
        C(getOption)
        C(getPassportAuthorizationForm)
        C(getPassportAuthorizationFormAvailableElements)
        C(getPassportElement)
        C(getPasswordState)
        C(getPaymentForm)
        C(getPaymentReceipt)
        C(getPollVoters)
        C(getPreferredCountryLanguage)
        C(getProxies)
        C(getProxyLink)
        C(getPublicMessageLink)
        C(getPushReceiverId)
        C(getRecentInlineBots)
        C(getRecentStickers)
        C(getRecentlyVisitedTMeUrls)
        C(getRecoveryEmailAddress)
        C(getRemoteFile)
        C(getRepliedMessage)
        C(getSavedAnimations)
        C(getSavedOrderInfo)
        C(getScopeNotificationSettings)
        C(getSecretChat)
        C(getStickerEmojis)
        C(getStickerSet)
        C(getStickers)
        C(getStorageStatistics)
        C(getStorageStatisticsFast)
        C(getSuitableDiscussionChats)
        C(getSupergroup)
        C(getSupergroupFullInfo)
        C(getSupergroupMembers)
        C(getSupportUser)
        C(getTemporaryPasswordState)
        C(getTextEntities)
        C(getTopChats)
        C(getTrendingStickerSets)
        C(getUser)
        C(getUserFullInfo)
        C(getUserPrivacySettingRules)
        C(getUserProfilePhotos)
        C(getWebPageInstantView)
        C(getWebPagePreview)
        C(importContacts)
        C(joinChat)
        C(joinChatByInviteLink)
        C(loadChats)
        C(leaveChat)
        C(logOut)
        C(openChat)
        C(openMessageContent)
        C(optimizeStorage)
        C(parseTextEntities)
        C(pinChatMessage)
        C(pingProxy)
        C(processPushNotification)
        C(readAllChatMentions)
        C(readFilePart)
        C(recoverAuthenticationPassword)
        C(recoverPassword)
        C(registerDevice)
        C(registerUser)
        C(removeBackground)
        C(removeChatActionBar)
        C(removeContacts)
        C(removeFavoriteSticker)
        C(removeNotification)
        C(removeNotificationGroup)
        C(removeProxy)
        C(removeRecentHashtag)
        C(removeRecentSticker)
        C(removeRecentlyFoundChat)
        C(removeSavedAnimation)
        C(removeStickerFromSet)
        C(removeTopChat)
        C(reorderInstalledStickerSets)
        C(reportChat)
        C(reportSupergroupSpam)
        C(requestAuthenticationPasswordRecovery)
        C(requestPasswordRecovery)
        C(requestQrCodeAuthentication)
        C(resendAuthenticationCode)
        C(resendChangePhoneNumberCode)
        C(resendEmailAddressVerificationCode)
        C(resendMessages)
        C(resendPhoneNumberConfirmationCode)
        C(resendPhoneNumberVerificationCode)
        C(resendRecoveryEmailAddressCode)
        C(resetAllNotificationSettings)
        C(resetBackgrounds)
        C(resetNetworkStatistics)
        C(saveApplicationLogEvent)
        C(searchBackground)
        C(searchCallMessages)
        C(searchChatMembers)
        C(searchChatMessages)
        C(searchChatRecentLocationMessages)
        C(searchChats)
        C(searchChatsNearby)
        C(searchChatsOnServer)
        C(searchContacts)
        C(searchEmojis)
        C(searchHashtags)
        C(searchInstalledStickerSets)
        C(searchMessages)
        C(searchPublicChat)
        C(searchPublicChats)
        C(searchSecretMessages)
        C(searchStickerSet)
        C(searchStickerSets)
        C(searchStickers)
        C(sendBotStartMessage)
        C(sendCallDebugInformation)
        C(sendCallRating)
        C(sendChatAction)
        C(sendChatScreenshotTakenNotification)
        C(sendChatSetTtlMessage)
        C(sendCustomRequest)
        C(sendEmailAddressVerificationCode)
        C(sendInlineQueryResultMessage)
        C(sendMessage)
        C(sendMessageAlbum)
        C(sendPassportAuthorizationForm)
        C(sendPaymentForm)
        C(sendPhoneNumberConfirmationCode)
        C(sendPhoneNumberVerificationCode)
        C(setAccountTtl)
        C(setAlarm)
        C(setAuthenticationPhoneNumber)
        C(setAutoDownloadSettings)
        C(setBackground)
        C(setBio)
        C(setBotUpdatesStatus)
        C(setChatChatList)
        C(setChatClientData)
        C(setChatDescription)
        C(setChatDiscussionGroup)
        C(setChatDraftMessage)
        C(setChatLocation)
        C(setChatMemberStatus)
        C(setChatNotificationSettings)
        C(setChatPermissions)
        C(setChatPhoto)
        C(setChatSlowModeDelay)
        C(setChatTitle)
        C(setCustomLanguagePack)
        C(setCustomLanguagePackString)
        C(setDatabaseEncryptionKey)
        C(setFileGenerationProgress)
        C(setGameScore)
        C(setInlineGameScore)
        C(setLogStream)
        C(setLogTagVerbosityLevel)
        C(setLogVerbosityLevel)
        C(setName)
        C(setNetworkType)
        C(setOption)
        C(setPassportElement)
        C(setPassportElementErrors)
        C(setPassword)
        C(setPinnedChats)
        C(setPollAnswer)
        C(setProfilePhoto)
        C(setRecoveryEmailAddress)
        C(setScopeNotificationSettings)
        C(setStickerPositionInSet)
        C(setSupergroupStickerSet)
        C(setSupergroupUsername)
        C(setTdlibParameters)
        C(setUserPrivacySettingRules)
        C(setUsername)
        C(sharePhoneNumber)
        C(stopPoll)
        C(synchronizeLanguagePack)
        C(terminateAllOtherSessions)
        C(terminateSession)
        C(testCallBytes)
        C(testCallEmpty)
        C(testCallString)
        C(testCallVectorInt)
        C(testCallVectorIntObject)
        C(testCallVectorString)
        C(testCallVectorStringObject)
        C(testGetDifference)
        C(testNetwork)
        C(testProxy)
        C(testReturnError)
        C(testSquareInt)
        C(testUseUpdate)
        C(toggleChatDefaultDisableNotification)
        C(toggleChatIsMarkedAsUnread)
        C(toggleChatIsPinned)
        C(toggleSupergroupIsAllHistoryAvailable)
        C(toggleSupergroupSignMessages)
        C(transferChatOwnership)
        C(unblockUser)
        C(unpinChatMessage)
        C(upgradeBasicGroupChatToSupergroupChat)
        C(uploadFile)
        C(uploadStickerFile)
        C(validateOrderInfo)
        C(viewMessages)
        C(viewTrendingStickerSets)
        C(writeGeneratedFilePart)
    }
#undef C
    return "Id " + std::to_string(req.get_id());
}

std::string responseToString(const td::TlObject &object)
{
#define C(class) case class::ID: return #class;
    switch (object.get_id()) {
        C(accountTtl)
        C(address)
        C(animation)
        C(animations)
        C(audio)
        C(authenticationCodeInfo)
        
        C(authenticationCodeTypeTelegramMessage)
        C(authenticationCodeTypeSms)
        C(authenticationCodeTypeCall)
        C(authenticationCodeTypeFlashCall)
        
        C(authorizationStateWaitTdlibParameters)
        C(authorizationStateWaitEncryptionKey)
        C(authorizationStateWaitPhoneNumber)
        C(authorizationStateWaitCode)
        C(authorizationStateWaitOtherDeviceConfirmation)
        C(authorizationStateWaitRegistration)
        C(authorizationStateWaitPassword)
        C(authorizationStateReady)
        C(authorizationStateLoggingOut)
        C(authorizationStateClosing)
        C(authorizationStateClosed)
        C(autoDownloadSettings)
        C(autoDownloadSettingsPresets)
        C(background)
        
        C(backgroundFillSolid)
        C(backgroundFillGradient)
        
        C(backgroundTypeWallpaper)
        C(backgroundTypePattern)
        C(backgroundTypeFill)
        C(backgrounds)
        C(basicGroup)
        C(basicGroupFullInfo)
        C(botCommand)
        C(botInfo)
        C(call)
        C(callConnection)
        
        C(callDiscardReasonEmpty)
        C(callDiscardReasonMissed)
        C(callDiscardReasonDeclined)
        C(callDiscardReasonDisconnected)
        C(callDiscardReasonHungUp)
        C(callId)
        
        C(callProblemEcho)
        C(callProblemNoise)
        C(callProblemInterruptions)
        C(callProblemDistortedSpeech)
        C(callProblemSilentLocal)
        C(callProblemSilentRemote)
        C(callProblemDropped)
        C(callProtocol)
        
        C(callStatePending)
        C(callStateExchangingKeys)
        C(callStateReady)
        C(callStateHangingUp)
        C(callStateDiscarded)
        C(callStateError)
        C(callbackQueryAnswer)
        
        C(callbackQueryPayloadData)
        C(callbackQueryPayloadGame)
        
        C(canTransferOwnershipResultOk)
        C(canTransferOwnershipResultPasswordNeeded)
        C(canTransferOwnershipResultPasswordTooFresh)
        C(canTransferOwnershipResultSessionTooFresh)
        C(chat)
        
        C(chatActionTyping)
        C(chatActionRecordingVideo)
        C(chatActionUploadingVideo)
        C(chatActionRecordingVoiceNote)
        C(chatActionUploadingVoiceNote)
        C(chatActionUploadingPhoto)
        C(chatActionUploadingDocument)
        C(chatActionChoosingLocation)
        C(chatActionChoosingContact)
        C(chatActionStartPlayingGame)
        C(chatActionRecordingVideoNote)
        C(chatActionUploadingVideoNote)
        C(chatActionCancel)
        
        C(chatActionBarReportSpam)
        C(chatActionBarReportUnrelatedLocation)
        C(chatActionBarReportAddBlock)
        C(chatActionBarAddContact)
        C(chatActionBarSharePhoneNumber)
        C(chatAdministrator)
        C(chatAdministrators)
        C(chatEvent)
        
        C(chatEventMessageEdited)
        C(chatEventMessageDeleted)
        C(chatEventPollStopped)
        C(chatEventMessagePinned)
        C(chatEventMessageUnpinned)
        C(chatEventMemberJoined)
        C(chatEventMemberLeft)
        C(chatEventMemberInvited)
        C(chatEventMemberPromoted)
        C(chatEventMemberRestricted)
        C(chatEventTitleChanged)
        C(chatEventPermissionsChanged)
        C(chatEventDescriptionChanged)
        C(chatEventUsernameChanged)
        C(chatEventPhotoChanged)
        C(chatEventInvitesToggled)
        C(chatEventLinkedChatChanged)
        C(chatEventSlowModeDelayChanged)
        C(chatEventSignMessagesToggled)
        C(chatEventStickerSetChanged)
        C(chatEventLocationChanged)
        C(chatEventIsAllHistoryAvailableToggled)
        C(chatEventLogFilters)
        C(chatEvents)
        C(chatInviteLink)
        C(chatInviteLinkInfo)
        
        C(chatListMain)
        C(chatListArchive)
        C(chatLocation)
        C(chatMember)
        
        C(chatMemberStatusCreator)
        C(chatMemberStatusAdministrator)
        C(chatMemberStatusMember)
        C(chatMemberStatusRestricted)
        C(chatMemberStatusLeft)
        C(chatMemberStatusBanned)
        C(chatMembers)
        
        C(chatMembersFilterContacts)
        C(chatMembersFilterAdministrators)
        C(chatMembersFilterMembers)
        C(chatMembersFilterRestricted)
        C(chatMembersFilterBanned)
        C(chatMembersFilterBots)
        C(chatNearby)
        C(chatNotificationSettings)
        C(chatPermissions)
        C(chatPhoto)
        
        C(chatReportReasonSpam)
        C(chatReportReasonViolence)
        C(chatReportReasonPornography)
        C(chatReportReasonChildAbuse)
        C(chatReportReasonCopyright)
        C(chatReportReasonUnrelatedLocation)
        C(chatReportReasonCustom)
        
        C(chatTypePrivate)
        C(chatTypeBasicGroup)
        C(chatTypeSupergroup)
        C(chatTypeSecret)
        C(chats)
        C(chatsNearby)
        
        C(checkChatUsernameResultOk)
        C(checkChatUsernameResultUsernameInvalid)
        C(checkChatUsernameResultUsernameOccupied)
        C(checkChatUsernameResultPublicChatsTooMuch)
        C(checkChatUsernameResultPublicGroupsUnavailable)
        C(connectedWebsite)
        C(connectedWebsites)
        
        C(connectionStateWaitingForNetwork)
        C(connectionStateConnectingToProxy)
        C(connectionStateConnecting)
        C(connectionStateUpdating)
        C(connectionStateReady)
        C(contact)
        C(count)
        C(customRequestResult)
        C(databaseStatistics)
        C(date)
        C(datedFile)
        C(deepLinkInfo)
        
        C(deviceTokenFirebaseCloudMessaging)
        C(deviceTokenApplePush)
        C(deviceTokenApplePushVoIP)
        C(deviceTokenWindowsPush)
        C(deviceTokenMicrosoftPush)
        C(deviceTokenMicrosoftPushVoIP)
        C(deviceTokenWebPush)
        C(deviceTokenSimplePush)
        C(deviceTokenUbuntuPush)
        C(deviceTokenBlackBerryPush)
        C(deviceTokenTizenPush)
        C(document)
        C(draftMessage)
        C(emailAddressAuthenticationCodeInfo)
        C(emojis)
        C(encryptedCredentials)
        C(encryptedPassportElement)
        C(error)
        C(file)
        C(filePart)
        
        C(fileTypeNone)
        C(fileTypeAnimation)
        C(fileTypeAudio)
        C(fileTypeDocument)
        C(fileTypePhoto)
        C(fileTypeProfilePhoto)
        C(fileTypeSecret)
        C(fileTypeSecretThumbnail)
        C(fileTypeSecure)
        C(fileTypeSticker)
        C(fileTypeThumbnail)
        C(fileTypeUnknown)
        C(fileTypeVideo)
        C(fileTypeVideoNote)
        C(fileTypeVoiceNote)
        C(fileTypeWallpaper)
        C(formattedText)
        C(foundMessages)
        C(game)
        C(gameHighScore)
        C(gameHighScores)
        C(hashtags)
        C(httpUrl)
        C(identityDocument)
        C(importedContacts)
        C(inlineKeyboardButton)
        
        C(inlineKeyboardButtonTypeUrl)
        C(inlineKeyboardButtonTypeLoginUrl)
        C(inlineKeyboardButtonTypeCallback)
        C(inlineKeyboardButtonTypeCallbackGame)
        C(inlineKeyboardButtonTypeSwitchInline)
        C(inlineKeyboardButtonTypeBuy)
        
        C(inlineQueryResultArticle)
        C(inlineQueryResultContact)
        C(inlineQueryResultLocation)
        C(inlineQueryResultVenue)
        C(inlineQueryResultGame)
        C(inlineQueryResultAnimation)
        C(inlineQueryResultAudio)
        C(inlineQueryResultDocument)
        C(inlineQueryResultPhoto)
        C(inlineQueryResultSticker)
        C(inlineQueryResultVideo)
        C(inlineQueryResultVoiceNote)
        C(inlineQueryResults)
        
        C(inputBackgroundLocal)
        C(inputBackgroundRemote)
        
        C(inputCredentialsSaved)
        C(inputCredentialsNew)
        C(inputCredentialsAndroidPay)
        C(inputCredentialsApplePay)
        
        C(inputFileId)
        C(inputFileRemote)
        C(inputFileLocal)
        C(inputFileGenerated)
        C(inputIdentityDocument)
        
        C(inputInlineQueryResultAnimatedGif)
        C(inputInlineQueryResultAnimatedMpeg4)
        C(inputInlineQueryResultArticle)
        C(inputInlineQueryResultAudio)
        C(inputInlineQueryResultContact)
        C(inputInlineQueryResultDocument)
        C(inputInlineQueryResultGame)
        C(inputInlineQueryResultLocation)
        C(inputInlineQueryResultPhoto)
        C(inputInlineQueryResultSticker)
        C(inputInlineQueryResultVenue)
        C(inputInlineQueryResultVideo)
        C(inputInlineQueryResultVoiceNote)
        
        C(inputMessageText)
        C(inputMessageAnimation)
        C(inputMessageAudio)
        C(inputMessageDocument)
        C(inputMessagePhoto)
        C(inputMessageSticker)
        C(inputMessageVideo)
        C(inputMessageVideoNote)
        C(inputMessageVoiceNote)
        C(inputMessageLocation)
        C(inputMessageVenue)
        C(inputMessageContact)
        C(inputMessageGame)
        C(inputMessageInvoice)
        C(inputMessagePoll)
        C(inputMessageForwarded)
        
        C(inputPassportElementPersonalDetails)
        C(inputPassportElementPassport)
        C(inputPassportElementDriverLicense)
        C(inputPassportElementIdentityCard)
        C(inputPassportElementInternalPassport)
        C(inputPassportElementAddress)
        C(inputPassportElementUtilityBill)
        C(inputPassportElementBankStatement)
        C(inputPassportElementRentalAgreement)
        C(inputPassportElementPassportRegistration)
        C(inputPassportElementTemporaryRegistration)
        C(inputPassportElementPhoneNumber)
        C(inputPassportElementEmailAddress)
        C(inputPassportElementError)
        
        C(inputPassportElementErrorSourceUnspecified)
        C(inputPassportElementErrorSourceDataField)
        C(inputPassportElementErrorSourceFrontSide)
        C(inputPassportElementErrorSourceReverseSide)
        C(inputPassportElementErrorSourceSelfie)
        C(inputPassportElementErrorSourceTranslationFile)
        C(inputPassportElementErrorSourceTranslationFiles)
        C(inputPassportElementErrorSourceFile)
        C(inputPassportElementErrorSourceFiles)
        C(inputPersonalDocument)
        C(inputThumbnail)
        C(invoice)
        C(jsonObjectMember)
        
        C(jsonValueNull)
        C(jsonValueBoolean)
        C(jsonValueNumber)
        C(jsonValueString)
        C(jsonValueArray)
        C(jsonValueObject)
        C(keyboardButton)
        
        C(keyboardButtonTypeText)
        C(keyboardButtonTypeRequestPhoneNumber)
        C(keyboardButtonTypeRequestLocation)
        C(keyboardButtonTypeRequestPoll)
        C(labeledPricePart)
        C(languagePackInfo)
        C(languagePackString)
        
        C(languagePackStringValueOrdinary)
        C(languagePackStringValuePluralized)
        C(languagePackStringValueDeleted)
        C(languagePackStrings)
        C(localFile)
        C(localizationTargetInfo)
        C(location)
        
        C(logStreamDefault)
        C(logStreamFile)
        C(logStreamEmpty)
        C(logTags)
        C(logVerbosityLevel)
        
        C(loginUrlInfoOpen)
        C(loginUrlInfoRequestConfirmation)
        
        C(maskPointForehead)
        C(maskPointEyes)
        C(maskPointMouth)
        C(maskPointChin)
        C(maskPosition)
        C(message)
        
        C(messageText)
        C(messageAnimation)
        C(messageAudio)
        C(messageDocument)
        C(messagePhoto)
        C(messageExpiredPhoto)
        C(messageSticker)
        C(messageVideo)
        C(messageExpiredVideo)
        C(messageVideoNote)
        C(messageVoiceNote)
        C(messageLocation)
        C(messageVenue)
        C(messageContact)
        C(messageGame)
        C(messagePoll)
        C(messageInvoice)
        C(messageCall)
        C(messageBasicGroupChatCreate)
        C(messageSupergroupChatCreate)
        C(messageChatChangeTitle)
        C(messageChatChangePhoto)
        C(messageChatDeletePhoto)
        C(messageChatAddMembers)
        C(messageChatJoinByLink)
        C(messageChatDeleteMember)
        C(messageChatUpgradeTo)
        C(messageChatUpgradeFrom)
        C(messagePinMessage)
        C(messageScreenshotTaken)
        C(messageChatSetTtl)
        C(messageCustomServiceAction)
        C(messageGameScore)
        C(messagePaymentSuccessful)
        C(messagePaymentSuccessfulBot)
        C(messageContactRegistered)
        C(messageWebsiteConnected)
        C(messagePassportDataSent)
        C(messagePassportDataReceived)
        C(messageUnsupported)
        C(messageForwardInfo)
        
        C(messageForwardOriginUser)
        C(messageForwardOriginHiddenUser)
        C(messageForwardOriginChannel)
        C(messageLinkInfo)
        
        C(messageSchedulingStateSendAtDate)
        C(messageSchedulingStateSendWhenOnline)
        
        C(messageSendingStatePending)
        C(messageSendingStateFailed)
        C(messageSendOptions)
        C(messages)
        C(minithumbnail)
        C(networkStatistics)
        
        C(networkStatisticsEntryFile)
        C(networkStatisticsEntryCall)
        
        C(networkTypeNone)
        C(networkTypeMobile)
        C(networkTypeMobileRoaming)
        C(networkTypeWiFi)
        C(networkTypeOther)
        C(notification)
        C(notificationGroup)
        
        C(notificationGroupTypeMessages)
        C(notificationGroupTypeMentions)
        C(notificationGroupTypeSecretChat)
        C(notificationGroupTypeCalls)
        
        C(notificationSettingsScopePrivateChats)
        C(notificationSettingsScopeGroupChats)
        C(notificationSettingsScopeChannelChats)
        
        C(notificationTypeNewMessage)
        C(notificationTypeNewSecretChat)
        C(notificationTypeNewCall)
        C(notificationTypeNewPushMessage)
        C(ok)
        
        C(optionValueBoolean)
        C(optionValueEmpty)
        C(optionValueInteger)
        C(optionValueString)
        C(orderInfo)
        
        C(pageBlockTitle)
        C(pageBlockSubtitle)
        C(pageBlockAuthorDate)
        C(pageBlockHeader)
        C(pageBlockSubheader)
        C(pageBlockKicker)
        C(pageBlockParagraph)
        C(pageBlockPreformatted)
        C(pageBlockFooter)
        C(pageBlockDivider)
        C(pageBlockAnchor)
        C(pageBlockList)
        C(pageBlockBlockQuote)
        C(pageBlockPullQuote)
        C(pageBlockAnimation)
        C(pageBlockAudio)
        C(pageBlockPhoto)
        C(pageBlockVideo)
        C(pageBlockVoiceNote)
        C(pageBlockCover)
        C(pageBlockEmbedded)
        C(pageBlockEmbeddedPost)
        C(pageBlockCollage)
        C(pageBlockSlideshow)
        C(pageBlockChatLink)
        C(pageBlockTable)
        C(pageBlockDetails)
        C(pageBlockRelatedArticles)
        C(pageBlockMap)
        C(pageBlockCaption)
        
        C(pageBlockHorizontalAlignmentLeft)
        C(pageBlockHorizontalAlignmentCenter)
        C(pageBlockHorizontalAlignmentRight)
        C(pageBlockListItem)
        C(pageBlockRelatedArticle)
        C(pageBlockTableCell)
        
        C(pageBlockVerticalAlignmentTop)
        C(pageBlockVerticalAlignmentMiddle)
        C(pageBlockVerticalAlignmentBottom)
        C(passportAuthorizationForm)
        
        C(passportElementPersonalDetails)
        C(passportElementPassport)
        C(passportElementDriverLicense)
        C(passportElementIdentityCard)
        C(passportElementInternalPassport)
        C(passportElementAddress)
        C(passportElementUtilityBill)
        C(passportElementBankStatement)
        C(passportElementRentalAgreement)
        C(passportElementPassportRegistration)
        C(passportElementTemporaryRegistration)
        C(passportElementPhoneNumber)
        C(passportElementEmailAddress)
        C(passportElementError)
        
        C(passportElementErrorSourceUnspecified)
        C(passportElementErrorSourceDataField)
        C(passportElementErrorSourceFrontSide)
        C(passportElementErrorSourceReverseSide)
        C(passportElementErrorSourceSelfie)
        C(passportElementErrorSourceTranslationFile)
        C(passportElementErrorSourceTranslationFiles)
        C(passportElementErrorSourceFile)
        C(passportElementErrorSourceFiles)
        
        C(passportElementTypePersonalDetails)
        C(passportElementTypePassport)
        C(passportElementTypeDriverLicense)
        C(passportElementTypeIdentityCard)
        C(passportElementTypeInternalPassport)
        C(passportElementTypeAddress)
        C(passportElementTypeUtilityBill)
        C(passportElementTypeBankStatement)
        C(passportElementTypeRentalAgreement)
        C(passportElementTypePassportRegistration)
        C(passportElementTypeTemporaryRegistration)
        C(passportElementTypePhoneNumber)
        C(passportElementTypeEmailAddress)
        C(passportElements)
        C(passportElementsWithErrors)
        C(passportRequiredElement)
        C(passportSuitableElement)
        C(passwordState)
        C(paymentForm)
        C(paymentReceipt)
        C(paymentResult)
        C(paymentsProviderStripe)
        C(personalDetails)
        C(personalDocument)
        C(phoneNumberAuthenticationSettings)
        C(photo)
        C(photoSize)
        C(poll)
        C(pollOption)
        
        C(pollTypeRegular)
        C(pollTypeQuiz)
        C(profilePhoto)
        C(proxies)
        C(proxy)
        
        C(proxyTypeSocks5)
        C(proxyTypeHttp)
        C(proxyTypeMtproto)
        
        C(publicChatTypeHasUsername)
        C(publicChatTypeIsLocationBased)
        C(publicMessageLink)
        
        C(pushMessageContentHidden)
        C(pushMessageContentAnimation)
        C(pushMessageContentAudio)
        C(pushMessageContentContact)
        C(pushMessageContentContactRegistered)
        C(pushMessageContentDocument)
        C(pushMessageContentGame)
        C(pushMessageContentGameScore)
        C(pushMessageContentInvoice)
        C(pushMessageContentLocation)
        C(pushMessageContentPhoto)
        C(pushMessageContentPoll)
        C(pushMessageContentScreenshotTaken)
        C(pushMessageContentSticker)
        C(pushMessageContentText)
        C(pushMessageContentVideo)
        C(pushMessageContentVideoNote)
        C(pushMessageContentVoiceNote)
        C(pushMessageContentBasicGroupChatCreate)
        C(pushMessageContentChatAddMembers)
        C(pushMessageContentChatChangePhoto)
        C(pushMessageContentChatChangeTitle)
        C(pushMessageContentChatDeleteMember)
        C(pushMessageContentChatJoinByLink)
        C(pushMessageContentMessageForwards)
        C(pushMessageContentMediaAlbum)
        C(pushReceiverId)
        C(recoveryEmailAddress)
        C(remoteFile)
        
        C(replyMarkupRemoveKeyboard)
        C(replyMarkupForceReply)
        C(replyMarkupShowKeyboard)
        C(replyMarkupInlineKeyboard)
        
        C(richTextPlain)
        C(richTextBold)
        C(richTextItalic)
        C(richTextUnderline)
        C(richTextStrikethrough)
        C(richTextFixed)
        C(richTextUrl)
        C(richTextEmailAddress)
        C(richTextSubscript)
        C(richTextSuperscript)
        C(richTextMarked)
        C(richTextPhoneNumber)
        C(richTextIcon)
        C(richTextAnchor)
        C(richTexts)
        C(savedCredentials)
        C(scopeNotificationSettings)
        
        C(searchMessagesFilterEmpty)
        C(searchMessagesFilterAnimation)
        C(searchMessagesFilterAudio)
        C(searchMessagesFilterDocument)
        C(searchMessagesFilterPhoto)
        C(searchMessagesFilterVideo)
        C(searchMessagesFilterVoiceNote)
        C(searchMessagesFilterPhotoAndVideo)
        C(searchMessagesFilterUrl)
        C(searchMessagesFilterChatPhoto)
        C(searchMessagesFilterCall)
        C(searchMessagesFilterMissedCall)
        C(searchMessagesFilterVideoNote)
        C(searchMessagesFilterVoiceAndVideoNote)
        C(searchMessagesFilterMention)
        C(searchMessagesFilterUnreadMention)
        C(seconds)
        C(secretChat)
        
        C(secretChatStatePending)
        C(secretChatStateReady)
        C(secretChatStateClosed)
        C(session)
        C(sessions)
        C(shippingOption)
        C(sticker)
        C(stickerSet)
        C(stickerSetInfo)
        C(stickerSets)
        C(stickers)
        C(storageStatistics)
        C(storageStatisticsByChat)
        C(storageStatisticsByFileType)
        C(storageStatisticsFast)
        C(supergroup)
        C(supergroupFullInfo)
        
        C(supergroupMembersFilterRecent)
        C(supergroupMembersFilterContacts)
        C(supergroupMembersFilterAdministrators)
        C(supergroupMembersFilterSearch)
        C(supergroupMembersFilterRestricted)
        C(supergroupMembersFilterBanned)
        C(supergroupMembersFilterBots)
        C(tMeUrl)
        
        C(tMeUrlTypeUser)
        C(tMeUrlTypeSupergroup)
        C(tMeUrlTypeChatInvite)
        C(tMeUrlTypeStickerSet)
        C(tMeUrls)
        C(tdlibParameters)
        C(temporaryPasswordState)
        C(termsOfService)
        C(testBytes)
        C(testInt)
        C(testString)
        C(testVectorInt)
        C(testVectorIntObject)
        C(testVectorString)
        C(testVectorStringObject)
        C(text)
        C(textEntities)
        C(textEntity)
        
        C(textEntityTypeMention)
        C(textEntityTypeHashtag)
        C(textEntityTypeCashtag)
        C(textEntityTypeBotCommand)
        C(textEntityTypeUrl)
        C(textEntityTypeEmailAddress)
        C(textEntityTypePhoneNumber)
        C(textEntityTypeBold)
        C(textEntityTypeItalic)
        C(textEntityTypeUnderline)
        C(textEntityTypeStrikethrough)
        C(textEntityTypeCode)
        C(textEntityTypePre)
        C(textEntityTypePreCode)
        C(textEntityTypeTextUrl)
        C(textEntityTypeMentionName)
        
        C(textParseModeMarkdown)
        C(textParseModeHTML)
        
        C(topChatCategoryUsers)
        C(topChatCategoryBots)
        C(topChatCategoryGroups)
        C(topChatCategoryChannels)
        C(topChatCategoryInlineBots)
        C(topChatCategoryCalls)
        C(topChatCategoryForwardChats)
        
        C(updateAuthorizationState)
        C(updateNewMessage)
        C(updateMessageSendAcknowledged)
        C(updateMessageSendSucceeded)
        C(updateMessageSendFailed)
        C(updateMessageContent)
        C(updateMessageEdited)
        C(updateMessageViews)
        C(updateMessageContentOpened)
        C(updateMessageMentionRead)
        C(updateMessageLiveLocationViewed)
        C(updateNewChat)
        C(updateChatTitle)
        C(updateChatPhoto)
        C(updateChatPermissions)
        C(updateChatLastMessage)
        C(updateChatIsPinned)
        C(updateChatIsMarkedAsUnread)
        C(updateChatHasScheduledMessages)
        C(updateChatDefaultDisableNotification)
        C(updateChatReadInbox)
        C(updateChatReadOutbox)
        C(updateChatUnreadMentionCount)
        C(updateChatNotificationSettings)
        C(updateScopeNotificationSettings)
        C(updateChatActionBar)
        C(updateChatPinnedMessage)
        C(updateChatReplyMarkup)
        C(updateChatDraftMessage)
        C(updateChatOnlineMemberCount)
        C(updateNotification)
        C(updateNotificationGroup)
        C(updateActiveNotifications)
        C(updateHavePendingNotifications)
        C(updateDeleteMessages)
        C(updateUserChatAction)
        C(updateUserStatus)
        C(updateUser)
        C(updateBasicGroup)
        C(updateSupergroup)
        C(updateSecretChat)
        C(updateUserFullInfo)
        C(updateBasicGroupFullInfo)
        C(updateSupergroupFullInfo)
        C(updateServiceNotification)
        C(updateFile)
        C(updateFileGenerationStart)
        C(updateFileGenerationStop)
        C(updateCall)
        C(updateUserPrivacySettingRules)
        C(updateUnreadMessageCount)
        C(updateUnreadChatCount)
        C(updateOption)
        C(updateInstalledStickerSets)
        C(updateTrendingStickerSets)
        C(updateRecentStickers)
        C(updateFavoriteStickers)
        C(updateSavedAnimations)
        C(updateSelectedBackground)
        C(updateLanguagePackStrings)
        C(updateConnectionState)
        C(updateTermsOfService)
        C(updateUsersNearby)
        C(updateNewInlineQuery)
        C(updateNewChosenInlineResult)
        C(updateNewCallbackQuery)
        C(updateNewInlineCallbackQuery)
        C(updateNewShippingQuery)
        C(updateNewPreCheckoutQuery)
        C(updateNewCustomEvent)
        C(updateNewCustomQuery)
        C(updatePoll)
        C(updatePollAnswer)
        C(updates)
        C(user)
        C(userFullInfo)
        
        C(userPrivacySettingShowStatus)
        C(userPrivacySettingShowProfilePhoto)
        C(userPrivacySettingShowLinkInForwardedMessages)
        C(userPrivacySettingShowPhoneNumber)
        C(userPrivacySettingAllowChatInvites)
        C(userPrivacySettingAllowCalls)
        C(userPrivacySettingAllowPeerToPeerCalls)
        C(userPrivacySettingAllowFindingByPhoneNumber)
        
        C(userPrivacySettingRuleAllowAll)
        C(userPrivacySettingRuleAllowContacts)
        C(userPrivacySettingRuleAllowUsers)
        C(userPrivacySettingRuleAllowChatMembers)
        C(userPrivacySettingRuleRestrictAll)
        C(userPrivacySettingRuleRestrictContacts)
        C(userPrivacySettingRuleRestrictUsers)
        C(userPrivacySettingRuleRestrictChatMembers)
        C(userPrivacySettingRules)
        C(userProfilePhoto)
        C(userProfilePhotos)
        
        C(userStatusEmpty)
        C(userStatusOnline)
        C(userStatusOffline)
        C(userStatusRecently)
        C(userStatusLastWeek)
        C(userStatusLastMonth)
        
        C(userTypeRegular)
        C(userTypeDeleted)
        C(userTypeBot)
        C(userTypeUnknown)
        C(users)
        C(validatedOrderInfo)
        C(venue)
        C(video)
        C(videoNote)
        C(voiceNote)
        C(webPage)
        C(webPageInstantView)
    }
    return "Id " + std::to_string(object.get_id());
}
