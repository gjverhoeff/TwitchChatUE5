#include "TwitchChatLibrary.h"
#include "TwitchChatConnection.h"
#include "TwitchChatSettings.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture.h"
#include "RenderUtils.h"           
#include "TextureResource.h"     
#include "AnimatedTexture2D.h"
#include "EmoteInfo.h"

DEFINE_LOG_CATEGORY(LogTwitchChatLibrary);


bool UTwitchChatLibrary::TwitchChat_GetEmoteTextureFromTables(const FString& EmoteID, UTexture*& OutTexture)
{
    UE_LOG(LogTwitchChatLibrary, Log, TEXT("LookupFromTables('%s')"), *EmoteID);
    OutTexture = nullptr;

    const UTwitchChatSettings* Settings = GetDefault<UTwitchChatSettings>();
    if (!Settings)
    {
        UE_LOG(LogTwitchChatLibrary, Warning, TEXT("No settings!"));
        return false;
    }

    struct FLookup { UDataTable* Table; const TCHAR* Name; };
    const TArray<FLookup> Lookups = {
        { Settings->GlobalEmoteTable,  TEXT("GlobalEmoteTable") },
        { Settings->ChannelEmoteTable, TEXT("ChannelEmoteTable") }
    };

    for (auto& L : Lookups)
    {
        int32 Count = L.Table ? L.Table->GetRowMap().Num() : 0;
        UE_LOG(LogTwitchChatLibrary, Log, TEXT("  Scanning %s (%d rows)"), L.Name, Count);

        if (L.Table)
        {
            const FString Context = TEXT("EmoteLookup");
            if (const FEmoteInfo* Row = L.Table->FindRow<FEmoteInfo>(*EmoteID, Context, true))
            {
                UObject* Loaded = Row->Texture.LoadSynchronous();
                if (Loaded)
                {
                    OutTexture = Cast<UTexture>(Loaded);
                    UE_LOG(LogTwitchChatLibrary, Log, TEXT("    Found '%s' in %s"), *EmoteID, L.Name);
                    return true;
                }
                else
                {
                    UE_LOG(LogTwitchChatLibrary, Warning, TEXT("    Row existed but Load failed in %s"), L.Name);
                }
            }
        }
    }

    UE_LOG(LogTwitchChatLibrary, Log, TEXT("  Not in any table"));
    return false;
}

bool UTwitchChatLibrary::TwitchChat_GetEmoteTexture(const FString& EmoteID, UTexture*& OutTexture)
{
    UE_LOG(LogTwitchChatLibrary, Log, TEXT("GetEmoteTexture('%s')"), *EmoteID);
    OutTexture = nullptr;

    // 1) Emote tables (could already be a UAnimatedTexture2D or UTexture2D)
    if (TwitchChat_GetEmoteTextureFromTables(EmoteID, OutTexture))
    {
        // OutTexture is already set to a valid UTexture*
        return true;
    }  // ← **This closing brace was missing** and is what lets the compiler see the rest as part of the function, not inside that if

    // 2) On-disk fallback, sync read then async convert via UpdateTextureRegions
    const FString Path = FPaths::ProjectSavedDir() / TEXT("FetchedEmotes") / (EmoteID + TEXT(".png"));
    if (!FPaths::FileExists(Path))
    {
        UE_LOG(LogTwitchChatLibrary, Warning, TEXT("  File not found: %s"), *Path);
        return false;
    }

    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *Path))
    {
        UE_LOG(LogTwitchChatLibrary, Error, TEXT("  LoadFileToArray failed for %s"), *Path);
        return false;
    }

    IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::PNG);
    if (!Wrapper.IsValid() || !Wrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        UE_LOG(LogTwitchChatLibrary, Error, TEXT("  PNG decode failed for %s"), *Path);
        return false;
    }

    TArray<uint8> RawBGRA;
    if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, RawBGRA))
    {
        UE_LOG(LogTwitchChatLibrary, Error, TEXT("  PNG→BGRA failed for %s"), *Path);
        return false;
    }

    const int32 W = Wrapper->GetWidth();
    const int32 H = Wrapper->GetHeight();

    // Create the transient texture
    UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
    if (!Tex)
    {
        UE_LOG(LogTwitchChatLibrary, Error, TEXT("  CreateTransient failed for %s"), *Path);
        return false;
    }
    Tex->AddToRoot();
    Tex->SRGB = true;
    Tex->UpdateResource();  // initialize RHI-side resource

    // Async update: heap-allocate region + pixel data
    auto* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, W, H);
    uint8* DataCopy = static_cast<uint8*>(FMemory::Malloc(RawBGRA.Num()));
    FMemory::Memcpy(DataCopy, RawBGRA.GetData(), RawBGRA.Num());

    Tex->UpdateTextureRegions(
        0,              // MipIndex
        1,              // NumRegions
        Region,         // pointer to our region
        W * 4,          // SrcPitch (bytes per row)
        4,              // SrcBpp   (bytes per pixel)
        DataCopy,       // pixel data
        // cleanup lambda on RHI thread
        [](uint8* InData, const FUpdateTextureRegion2D* InRegion)
        {
            FMemory::Free(InData);
            delete InRegion;
        }
    );

    OutTexture = Tex;
    UE_LOG(LogTwitchChatLibrary, Log, TEXT("  Returning async PNG texture (%dx%d) for %s"), W, H, *Path);
    return true;
}




void UTwitchChatLibrary::TwitchChat_Connect()
{
    UE_LOG(LogTwitchChatLibrary, Log, TEXT("TwitchChat_Connect called"));
    if (UTwitchChatSettings* S = GetMutableDefault<UTwitchChatSettings>())
    {
        FTwitchChatConnection::Get()->Connect(S->UserName, S->AccessToken, S->LastChannel, S->Port);
    }
}

void UTwitchChatLibrary::TwitchChat_Disconnect()
{
    UE_LOG(LogTwitchChatLibrary, Log, TEXT("TwitchChat_Disconnect called"));
    FTwitchChatConnection::Get()->Disconnect();
}

void UTwitchChatLibrary::TwitchChat_ChangeChannel(const FString& NewChannel)
{
    UE_LOG(LogTwitchChatLibrary, Log, TEXT("TwitchChat_ChangeChannel called: %s"), *NewChannel);
    FTwitchChatConnection::Get()->Disconnect();

    if (UTwitchChatSettings* S = GetMutableDefault<UTwitchChatSettings>())
    {
        S->LastChannel = NewChannel;
        S->SaveConfig();
    }
}
