#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "TwitchChatMessage.generated.h"

USTRUCT(BlueprintType)
struct FTwitchChatMessage
{
    GENERATED_BODY()

    UPROPERTY() FString               UserName;
    UPROPERTY() FString               Message;
    UPROPERTY() FLinearColor          UserColor = FLinearColor::White;
    UPROPERTY() TArray<FString>       EmoteIds;
    UPROPERTY() TArray<FIntPoint>     EmoteRanges;
    UPROPERTY() TMap<FString, FString> Tags;
    UPROPERTY() FString RawPayload;
};
