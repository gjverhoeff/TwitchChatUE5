
#include "TwitchChatSettings.h"
#include "UObject/ConstructorHelpers.h"

UTwitchChatSettings::UTwitchChatSettings()
{
    // Default emote tables when non are set by user in settings
    static ConstructorHelpers::FObjectFinder<UDataTable> GlobalDT(
        TEXT("/TwitchChat/Emotes/GlobalEmoteTable.GlobalEmoteTable"));
    if (GlobalDT.Succeeded())
    {
        GlobalEmoteTable = GlobalDT.Object;
    }
    static ConstructorHelpers::FObjectFinder<UDataTable> ChannelDT(
        TEXT("/TwitchChat/Emotes/ChannelEmoteTable.ChannelEmoteTable"));
    if (ChannelDT.Succeeded())
    {
        ChannelEmoteTable = ChannelDT.Object;
    }

    MaxMessages = 20;
}
