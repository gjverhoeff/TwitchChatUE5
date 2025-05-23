#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/DataTable.h"
#include "TwitchChatSettings.generated.h"


UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "Twitch Chat"))
class TWITCHCHAT_API UTwitchChatSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UTwitchChatSettings();

 
    UPROPERTY(EditAnywhere, Config, Category = "Settings", meta = (DisplayName = "Channel"))
    FString LastChannel;

    UPROPERTY(EditAnywhere, Config, Category = "Settings", meta = (DisplayName = "Username"))
    FString UserName;


    UPROPERTY(EditAnywhere, Config, Category = "Settings", meta = (DisplayName = "Client ID"))
    FString ClientId;

    UPROPERTY(EditAnywhere, Config, Category = "Settings", meta = (DisplayName = "Client Secret"))
    FString ClientSecret;

    UPROPERTY(EditAnywhere, Config, Category = "Settings", meta = (DisplayName = "Access Token"))
    FString AccessToken;

    UPROPERTY(EditAnywhere, Config, Category = "Settings", meta = (DisplayName = "Refresh Token"))
    FString RefreshToken;

    UPROPERTY(EditAnywhere, Config, Category = "Settings", meta = (DisplayName = "Keepalive Timeout Seconds", ClampMin = "10", ClampMax = "600"))
    int32 KeepaliveTimeoutSeconds = 30;

    UPROPERTY(EditAnywhere, Config, Category = "Settings", meta = (DisplayName = "Port", ClampMin = "1", ClampMax = "65535"))
    int32 Port = 6667;

    UPROPERTY(Config)
    bool bUseEventSub = true;

    UPROPERTY(EditAnywhere, Config, Category = "Emotes", meta = (DisplayName = "Auto Download Emotes"))
    bool AutoDownloadEmotes = true;

    UPROPERTY(EditAnywhere, Config, Category = "Emotes", meta = (
        DisplayName = "Emote Download Timeout Seconds",
        ClampMin = "1", ClampMax = "60"
         ))
     int32 EmoteRenderTimeoutSeconds = 5;

    UPROPERTY(EditAnywhere, Category = "Emotes", meta = (DisplayName = "Global Emote Table"))
    UDataTable* GlobalEmoteTable = nullptr;

    UPROPERTY(EditAnywhere, Category = "Emotes", meta = (DisplayName = "Channel Emote Table"))
    UDataTable* ChannelEmoteTable = nullptr;

    UPROPERTY(EditAnywhere, Config, Category = "Editor Window", meta = (DisplayName = "Max Messages", ClampMin = "0"))
    int32 MaxMessages = 20;
};
