
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TwitchChatLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTwitchChatLibrary, Log, All);

UCLASS()
class TWITCHCHAT_API UTwitchChatLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
  
    UFUNCTION(BlueprintCallable, Category = "Twitch Chat")
    static void TwitchChat_Connect();

    UFUNCTION(BlueprintCallable, Category = "Twitch Chat")
    static void TwitchChat_Disconnect();


    UFUNCTION(BlueprintCallable, Category = "Twitch Chat")
    static void TwitchChat_ChangeChannel(const FString& NewChannel);


    UFUNCTION(BlueprintCallable, Category = "Twitch Chat")
    static bool TwitchChat_GetEmoteTexture(const FString& EmoteID, UTexture*& OutTexture);

    UFUNCTION(BlueprintCallable, Category = "Twitch Chat")
    static bool TwitchChat_GetEmoteTextureFromTables(const FString& EmoteID, UTexture*& OutTexture);
};
