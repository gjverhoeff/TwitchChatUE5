
#pragma once

#include "CoreMinimal.h"
#include "IWebSocket.h"
#include "Delegates/Delegate.h"
#include "TwitchChatMessage.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTwitchChat, Log, All);
DECLARE_MULTICAST_DELEGATE_OneParam(FTwitchChatMessageDelegate, const FTwitchChatMessage&);


class FTwitchChatConnection : public TSharedFromThis<FTwitchChatConnection>
{
public:
    FTwitchChatConnection();
    ~FTwitchChatConnection();

  
    static TSharedRef<FTwitchChatConnection> Get();


    bool Connect(const FString& InUser,
        const FString& InToken,
        const FString& InChannel,
        int32 InPort);

    
    void Disconnect();


    bool IsConnected() const { return Socket.IsValid() && bSubscribed; }

 
    FTwitchChatMessageDelegate OnMessage;

  
    void StartDeviceFlowInteractive();

private:

    void BeginAuthFlow();

    void StartDeviceFlow();

    void SetupWebSocket();

    void PollDeviceToken();


    void QueryUserId(const FString& Login, TFunction<void(const FString&)> Callback);


    void TrySubscribe();


    void HandleWebSocketMessage(const FString& Msg);


    bool DownloadEmoteIfNeeded(const FString& EmoteId);
    bool AllEmotesDownloaded(const TArray<FString>& EmoteIds, double Deadline);

    TSharedPtr<IWebSocket> Socket;
    bool bSubscribed = false;

 
    FString BotLogin;
    FString ChannelLogin;
    FString OAuthToken;
    FString BotUserId;
    FString BroadcasterUserId;
    FString SessionId;
    bool bGotBotId = false;
    bool bGotBroadcasterId = false;
    bool bGotWelcome = false;

    // Device flow
    FString DeviceCode;
    FString UserCode;
    FString VerificationUri;
    int32  PollIntervalSeconds = 5;
    double DeviceFlowExpiry = 0.0;

    bool bAutoConnect = false;

    void RefreshOAuthToken(TFunction<void(bool)> OnComplete);
};
