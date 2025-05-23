
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TwitchChatMessage.h"
#include "TwitchChatComponent.generated.h"


USTRUCT(BlueprintType)
struct FBP_TwitchChatMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Username"))
    FString               UserName;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Message"))
    FString               Message;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "User Color"))
    FLinearColor          UserColor;


    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Emote IDs"))
    TArray<FString>       EmoteIds;

    
    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Emote Occurrences"))
    TArray<FString>       EmoteOccurrences;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Subscriber"))
    bool                   bSubscriber;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "VIP"))
    bool                   bVip;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Mod"))
    bool                   bMod;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Turbo"))
    bool                   bTurbo;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Bits"))
    int32                  Bits;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Timestamp"))
    int64                  TmiSentTs;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Message ID"))
    FString               Id;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Reply Parent Msg Id"))
    FString               ReplyParentMsgId;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Reply Parent User Id"))
    FString               ReplyParentUserId;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Reply Parent Display Name"))
    FString               ReplyParentDisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Reply Parent Msg Body"))
    FString               ReplyParentMsgBody;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "User Id"))
    FString               UserIdTag;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "User Type"))
    FString               UserType;

    UPROPERTY(BlueprintReadOnly, Category = "Twitch Chat Message", Meta = (DisplayName = "Raw Payload"))
    FString RawPayload;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnTwitchChatMessage,
    const FBP_TwitchChatMessage&, ChatMessage
);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TWITCHCHAT_API UTwitchChatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTwitchChatComponent();

 
    UPROPERTY(BlueprintAssignable, Category = "Twitch Chat", Meta = (DisplayName = "On New Chat Message"))
    FOnTwitchChatMessage OnChatMessageReceived;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void HandleIncoming(const FTwitchChatMessage& Msg);
    FDelegateHandle MessageHandle;
};
