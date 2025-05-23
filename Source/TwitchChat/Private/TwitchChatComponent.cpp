#include "TwitchChatComponent.h"
#include "TwitchChatConnection.h"
#include "Async/Async.h"

UTwitchChatComponent::UTwitchChatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTwitchChatComponent::BeginPlay()
{
    Super::BeginPlay();
    MessageHandle = FTwitchChatConnection::Get()
        ->OnMessage.AddUObject(this, &UTwitchChatComponent::HandleIncoming);
}

void UTwitchChatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FTwitchChatConnection::Get()->OnMessage.Remove(MessageHandle);
    Super::EndPlay(EndPlayReason);
}

void UTwitchChatComponent::HandleIncoming(const FTwitchChatMessage& Msg)
{
   
    TWeakObjectPtr<UTwitchChatComponent> WeakThis(this);

    Async(EAsyncExecution::TaskGraphMainThread, [WeakThis, Msg]()
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            FBP_TwitchChatMessage BP;

        
            BP.UserName = Msg.UserName;
            BP.Message = Msg.Message;
            BP.UserColor = Msg.UserColor;

           
            BP.EmoteIds = Msg.EmoteIds;

        
            BP.EmoteOccurrences = Msg.EmoteIds;


            const FString* Val = nullptr;
            Val = Msg.Tags.Find(TEXT("subscriber"));
            BP.bSubscriber = (Val && *Val == TEXT("1"));
            Val = Msg.Tags.Find(TEXT("vip"));
            BP.bVip = (Val && *Val == TEXT("1"));
            Val = Msg.Tags.Find(TEXT("mod"));
            BP.bMod = (Val && *Val == TEXT("1"));
            Val = Msg.Tags.Find(TEXT("turbo"));
            BP.bTurbo = (Val && *Val == TEXT("1"));

    
            BP.Bits = 0;
            if (const FString* V = Msg.Tags.Find(TEXT("bits")))
            {
                BP.Bits = FCString::Atoi(**V);
            }

       
            BP.TmiSentTs = 0;
            if (const FString* V = Msg.Tags.Find(TEXT("tmi-sent-ts")))
            {
                BP.TmiSentTs = FCString::Atoi64(**V);
            }

         
            auto AssignTag = [&](const TCHAR* Key, FString& Out)
                {
                    if (const FString* V = Msg.Tags.Find(Key))
                    {
                        Out = *V;
                    }
                };
            AssignTag(TEXT("id"), BP.Id);
            AssignTag(TEXT("reply-parent-msg-id"), BP.ReplyParentMsgId);
            AssignTag(TEXT("reply-parent-user-id"), BP.ReplyParentUserId);
            AssignTag(TEXT("reply-parent-display-name"), BP.ReplyParentDisplayName);
            AssignTag(TEXT("reply-parent-msg-body"), BP.ReplyParentMsgBody);
            AssignTag(TEXT("user-id"), BP.UserIdTag);
            AssignTag(TEXT("user-type"), BP.UserType);

            BP.RawPayload = Msg.RawPayload;
       
            WeakThis->OnChatMessageReceived.Broadcast(BP);
        });
}
