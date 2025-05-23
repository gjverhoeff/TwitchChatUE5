
#include "TwitchChatConnection.h"
#include "TwitchChatSettings.h"
#include "TwitchChatMessage.h"
#include "WebSocketsModule.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY(LogTwitchChat);

TSharedRef<FTwitchChatConnection> FTwitchChatConnection::Get()
{
    static TSharedRef<FTwitchChatConnection> Instance = MakeShared<FTwitchChatConnection>();
    return Instance;
}

FTwitchChatConnection::FTwitchChatConnection() {}
FTwitchChatConnection::~FTwitchChatConnection()
{
    Disconnect();
}

void FTwitchChatConnection::StartDeviceFlowInteractive()
{
   
    bAutoConnect = false;
    StartDeviceFlow();
}


bool FTwitchChatConnection::Connect(
    const FString& InUser,
    const FString& /*InToken*/,
    const FString& InChannel,
    int32 /*InPort*/
)
{
    Disconnect();
    BotLogin = InUser;
    ChannelLogin = InChannel.ToLower();
    BeginAuthFlow();
    return true;
}

void FTwitchChatConnection::BeginAuthFlow()
{
    const UTwitchChatSettings* S = GetDefault<UTwitchChatSettings>();
    FString ConfigToken = S->AccessToken.TrimStartAndEnd();

    if (ConfigToken.IsEmpty())
    {
   
        bAutoConnect = true;
        StartDeviceFlow();
    }
    else
    {
    
        bAutoConnect = false;

        OAuthToken = ConfigToken.StartsWith(TEXT("oauth:"))
            ? ConfigToken.Mid(6)
            : ConfigToken;

     
        QueryUserId(BotLogin, [this](const FString& Id) { BotUserId = Id; bGotBotId = true; TrySubscribe(); });
        QueryUserId(ChannelLogin, [this](const FString& Id) { BroadcasterUserId = Id; bGotBroadcasterId = true; TrySubscribe(); });
        SetupWebSocket();
    }
}

void FTwitchChatConnection::StartDeviceFlow()
{
    const UTwitchChatSettings* S = GetDefault<UTwitchChatSettings>();
    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(TEXT("https://id.twitch.tv/oauth2/device"));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));


    FString Body = FString::Printf(TEXT("client_id=%s&scopes=user%%3Aread%%3Achat"), *S->ClientId);
    Req->SetContentAsString(Body);

    Req->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr /*R*/, FHttpResponsePtr Resp, bool bOK)
        {
            if (!bOK || !Resp.IsValid() || Resp->GetResponseCode() != 200)
            {
                UE_LOG(LogTwitchChat, Error,
                    TEXT("Device flow start failed: %s"),
                    Resp.IsValid() ? *Resp->GetContentAsString() : TEXT("no-response"));
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Resp->GetContentAsString());
            if (!FJsonSerializer::Deserialize(Reader, Json))
                return;

         
            DeviceCode = Json->GetStringField(TEXT("device_code"));
            UserCode = Json->GetStringField(TEXT("user_code"));
            VerificationUri = Json->GetStringField(TEXT("verification_uri"));
            PollIntervalSeconds = Json->GetIntegerField(TEXT("interval"));
            DeviceFlowExpiry = FPlatformTime::Seconds() + Json->GetNumberField(TEXT("expires_in"));

            UE_LOG(LogTwitchChat, Log,
                TEXT("Device auth: go to %s and enter code %s"),
                *VerificationUri, *UserCode);

            AsyncTask(ENamedThreads::GameThread, [this]()
                {
                    FPlatformProcess::LaunchURL(*VerificationUri, nullptr, nullptr);
                });

         
            PollDeviceToken();
        }
    );
    Req->ProcessRequest();
}



void FTwitchChatConnection::PollDeviceToken()
{
    const UTwitchChatSettings* S = GetDefault<UTwitchChatSettings>();
    Async(EAsyncExecution::Thread, [this, S]()
        {
            while (FPlatformTime::Seconds() < DeviceFlowExpiry)
            {
                FPlatformProcess::Sleep(PollIntervalSeconds);

                auto Req = FHttpModule::Get().CreateRequest();
                Req->SetURL(TEXT("https://id.twitch.tv/oauth2/token"));
                Req->SetVerb(TEXT("POST"));
                Req->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));

                FString Body = FString::Printf(
                    TEXT("client_id=%s&client_secret=%s&grant_type=urn:ietf:params:oauth:grant-type:device_code&device_code=%s"),
                    *S->ClientId, *S->ClientSecret, *DeviceCode
                );
                Req->SetContentAsString(Body);

                FEvent* Done = FPlatformProcess::GetSynchEventFromPool(true);
                bool bSuccess = false;

                Req->OnProcessRequestComplete().BindLambda(
                    [this, &bSuccess, Done](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
                    {
                        if (bOK && Resp.IsValid() && Resp->GetResponseCode() == 200)
                        {
                            TSharedPtr<FJsonObject> J;
                            TSharedRef<TJsonReader<>> R =
                                TJsonReaderFactory<>::Create(Resp->GetContentAsString());
                            if (FJsonSerializer::Deserialize(R, J))
                            {
                                OAuthToken = J->GetStringField(TEXT("access_token"));
                                UE_LOG(LogTwitchChat, Log, TEXT("Device flow succeeded; token acquired"));

                                if (UTwitchChatSettings* M = GetMutableDefault<UTwitchChatSettings>())
                                {
                                    M->AccessToken = OAuthToken;
                                    M->RefreshToken = J->GetStringField(TEXT("refresh_token"));
                                    M->SaveConfig();  

                                    UE_LOG(LogTwitchChat, Log, TEXT("Tokens saved to DefaultEditorPerProjectUserSettings.ini"));
                                }
                                bSuccess = true;
                            }
                        }
                        Done->Trigger();
                    }
                );
                Req->ProcessRequest();
                Done->Wait();
                FPlatformProcess::ReturnSynchEventToPool(Done);

                if (bSuccess)
                {
                    if (bAutoConnect)
                    {
                        AsyncTask(ENamedThreads::GameThread, [this]()
                            {
                                QueryUserId(BotLogin, [this](const FString& Id) { BotUserId = Id; bGotBotId = true; TrySubscribe(); });
                                QueryUserId(ChannelLogin, [this](const FString& Id) { BroadcasterUserId = Id; bGotBroadcasterId = true; TrySubscribe(); });
                                SetupWebSocket();
                            });
                    }
                    break;
                }
            }
        });
}


void FTwitchChatConnection::Disconnect()
{
    if (Socket.IsValid())
    {
        Socket->Close();
        Socket.Reset();
    }
    bSubscribed = false;
    BotUserId.Empty();
    BroadcasterUserId.Empty();
    SessionId.Empty();
    bGotBotId = bGotBroadcasterId = bGotWelcome = false;
}

void FTwitchChatConnection::QueryUserId(const FString& Login, TFunction<void(const FString&)> Callback)
{
    const UTwitchChatSettings* S = GetDefault<UTwitchChatSettings>();
    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(FString::Printf(TEXT("https://api.twitch.tv/helix/users?login=%s"), *Login));
    Req->SetVerb(TEXT("GET"));
    Req->SetHeader(TEXT("Client-Id"), S->ClientId);
    Req->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + OAuthToken);

    Req->OnProcessRequestComplete().BindLambda(
        [this, Login, Callback](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
        {
            if (!bOK || !Resp.IsValid())
            {
                Callback(FString());
                return;
            }

            int32 Code = Resp->GetResponseCode();
            if (Code == 401)
            {
                // Token expired → refresh & retry
                RefreshOAuthToken([this, Login, Callback](bool bRefreshed)
                    {
                        if (bRefreshed)
                            QueryUserId(Login, Callback);
                        else
                            Callback(FString());
                    });
                return;
            }

            if (Code == 200)
            {
                FString Out;
                TSharedPtr<FJsonObject> J;
                TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
                if (FJsonSerializer::Deserialize(R, J) && J->HasField(TEXT("data")))
                {
                    const auto& Arr = J->GetArrayField(TEXT("data"));
                    if (Arr.Num() > 0)
                        Out = Arr[0]->AsObject()->GetStringField(TEXT("id"));
                }
                Callback(Out);
            }
            else
            {
                Callback(FString());
            }
        }
    );
    Req->ProcessRequest();
}


void FTwitchChatConnection::TrySubscribe()
{
    if (bSubscribed || !bGotBotId || !bGotBroadcasterId || !bGotWelcome)
        return;

    const UTwitchChatSettings* S = GetDefault<UTwitchChatSettings>();
    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(TEXT("https://api.twitch.tv/helix/eventsub/subscriptions"));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Client-Id"), S->ClientId);
    Req->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + OAuthToken);
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("type"), TEXT("channel.chat.message"));
    Root->SetStringField(TEXT("version"), TEXT("1"));
    TSharedPtr<FJsonObject> Cond = MakeShared<FJsonObject>();
    Cond->SetStringField(TEXT("broadcaster_user_id"), BroadcasterUserId);
    Cond->SetStringField(TEXT("user_id"), BotUserId);
    Root->SetObjectField(TEXT("condition"), Cond);
    TSharedPtr<FJsonObject> Trans = MakeShared<FJsonObject>();
    Trans->SetStringField(TEXT("method"), TEXT("websocket"));
    Trans->SetStringField(TEXT("session_id"), SessionId);
    Root->SetObjectField(TEXT("transport"), Trans);

    FString Body;
    TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(Root.ToSharedRef(), W);
    Req->SetContentAsString(Body);

    Req->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
        {
            int32 Code = Resp.IsValid() ? Resp->GetResponseCode() : -1;
            if (bOK && (Code == 200 || Code == 202))
            {
                UE_LOG(LogTwitchChat, Log, TEXT("Subscribed to channel.chat.message"));
                bSubscribed = true;
            }
            else if (Code == 401)
            {
                // Token expired → refresh & retry
                RefreshOAuthToken([this](bool bRefreshed)
                    {
                        if (bRefreshed)
                            TrySubscribe();
                    });
            }
            else
            {
                UE_LOG(LogTwitchChat, Error,
                    TEXT("Subscription failed (%d): %s"),
                    Code,
                    Resp.IsValid() ? *Resp->GetContentAsString() : TEXT("no-response")
                );
            }
        }
    );
    Req->ProcessRequest();
}



void FTwitchChatConnection::HandleWebSocketMessage(const FString& MsgJson)
{
  
    Async(EAsyncExecution::Thread, [this, MsgJson]()
        {
         
            TSharedPtr<FJsonObject> Root;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(MsgJson);
            if (!FJsonSerializer::Deserialize(Reader, Root) || !Root->HasField(TEXT("metadata")))
            {
                return;
            }

            const auto& Meta = Root->GetObjectField(TEXT("metadata"));
            const FString MessageType = Meta->GetStringField(TEXT("message_type"));

      
            if (MessageType == TEXT("session_welcome"))
            {
                SessionId = Root->GetObjectField(TEXT("payload"))->GetObjectField(TEXT("session"))->GetStringField(TEXT("id"));
                bGotWelcome = true;
                TrySubscribe();
                return;
            }

       
            if (MessageType != TEXT("notification") ||
                Meta->GetStringField(TEXT("subscription_type")) != TEXT("channel.chat.message"))
            {
                return;
            }

            const auto& Evt = Root->GetObjectField(TEXT("payload"))->GetObjectField(TEXT("event"));

         
            FTwitchChatMessage M;
            M.RawPayload = MsgJson;


            if (Evt->HasField(TEXT("chatter_user_name")))
                M.UserName = Evt->GetStringField(TEXT("chatter_user_name"));
            else if (Evt->HasField(TEXT("user_name")))
                M.UserName = Evt->GetStringField(TEXT("user_name"));


            if (Evt->HasField(TEXT("message")))
                M.Message = Evt->GetObjectField(TEXT("message"))->GetStringField(TEXT("text"));
            else if (Evt->HasField(TEXT("text")))
                M.Message = Evt->GetStringField(TEXT("text"));

      
            TArray<FString> EmoteIds;
            TArray<FIntPoint>  EmoteRanges;

            if (Evt->HasField(TEXT("message")))
            {
                const auto& Frags = Evt->GetObjectField(TEXT("message"))->GetArrayField(TEXT("fragments"));
                int32 Cursor = 0;
                for (auto& FragVal : Frags)
                {
                    const auto& FragObj = FragVal->AsObject();
                    const FString Text = FragObj->GetStringField(TEXT("text"));
                    if (FragObj->GetStringField(TEXT("type")) == TEXT("emote") && FragObj->HasField(TEXT("emote")))
                    {
                        const FString EmId = FragObj->GetObjectField(TEXT("emote"))->GetStringField(TEXT("id"));
                        EmoteIds.Add(EmId);
                        EmoteRanges.Add(FIntPoint(Cursor, Cursor + Text.Len()));
                        DownloadEmoteIfNeeded(EmId);
                    }
                    Cursor += Text.Len();
                }
            }
            else if (Evt->HasField(TEXT("emotes")))
            {
                for (auto& Val : Evt->GetArrayField(TEXT("emotes")))
                {
                    const auto& Obj = Val->AsObject();
                    const FString EmId = Obj->GetStringField(TEXT("id"));
                    int32 B = Obj->GetIntegerField(TEXT("begin"));
                    int32 E = Obj->GetIntegerField(TEXT("end"));
                    EmoteIds.Add(EmId);
                    EmoteRanges.Add(FIntPoint(B, E));
                    DownloadEmoteIfNeeded(EmId);
                }
            }

            M.EmoteIds = MoveTemp(EmoteIds);
            M.EmoteRanges = MoveTemp(EmoteRanges);

     
            if (Evt->HasField(TEXT("color")) && !Evt->GetStringField(TEXT("color")).IsEmpty())
            {
                M.UserColor = FLinearColor(FColor::FromHex(Evt->GetStringField(TEXT("color"))));
            }
            else
            {
                M.UserColor = FLinearColor(FColor::FromHex(TEXT("#6441A4")));
            }

         
            const UTwitchChatSettings* Settings = GetDefault<UTwitchChatSettings>();
            if (Settings->AutoDownloadEmotes && M.EmoteIds.Num() > 0)
            {
                double TimeoutSecs = Settings->EmoteRenderTimeoutSeconds;            
                double Deadline = FPlatformTime::Seconds() + TimeoutSecs;
                AllEmotesDownloaded(M.EmoteIds, Deadline);                         
            }

         
            AsyncTask(ENamedThreads::GameThread, [this, M = MoveTemp(M)]()
                {
                    OnMessage.Broadcast(M);
                });
        });
}




void FTwitchChatConnection::SetupWebSocket()
{
    const UTwitchChatSettings* S = GetDefault<UTwitchChatSettings>();

    
    int32 KA = FMath::Clamp(S->KeepaliveTimeoutSeconds, 10, 600);

    
    FString URL = FString::Printf(
        TEXT("wss://eventsub.wss.twitch.tv/ws?keepalive_timeout_seconds=%d"),
        KA
    );


    Socket = FWebSocketsModule::Get().CreateWebSocket(URL, TEXT(""));

    Socket->OnConnected().AddLambda([]()
        {
            UE_LOG(LogTwitchChat, Log, TEXT("WS connected; awaiting session_welcome"));
        });

    Socket->OnConnectionError().AddLambda([](const FString& Err)
        {
            UE_LOG(LogTwitchChat, Error, TEXT("WS error: %s"), *Err);
        });

    Socket->OnClosed().AddLambda([](int32 Code, const FString& Reason, bool)
        {
            UE_LOG(LogTwitchChat, Warning, TEXT("WS closed: %d (%s)"), Code, *Reason);
        });

    Socket->OnMessage().AddLambda([this](const FString& Msg)
        {
            HandleWebSocketMessage(Msg);
        });

   
    Socket->Connect();
}



bool FTwitchChatConnection::DownloadEmoteIfNeeded(const FString& EmoteId)
{
    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FetchedEmotes"));
    const FString Path = FPaths::Combine(Dir, EmoteId + TEXT(".png"));
    if (FPaths::FileExists(Path))
        return true;

    IFileManager::Get().MakeDirectory(*Dir, /*Tree=*/true);
    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(FString::Printf(
        TEXT("https://static-cdn.jtvnw.net/emoticons/v2/%s/default/dark/3.0"),
        *EmoteId
    ));
    Req->SetVerb(TEXT("GET"));
    Req->OnProcessRequestComplete().BindLambda(
        [Path](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
        {
            if (bOK && Resp.IsValid()
                && EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
            {
                FFileHelper::SaveArrayToFile(Resp->GetContent(), *Path);
            }
        }
    );
    return Req->ProcessRequest();
}

bool FTwitchChatConnection::AllEmotesDownloaded(
    const TArray<FString>& EmoteIds,
    double Deadline
)
{
    while (FPlatformTime::Seconds() < Deadline)
    {
        bool bAll = true;
        for (const FString& Id : EmoteIds)
        {
            const FString P = FPaths::Combine(
                FPaths::ProjectSavedDir(), TEXT("FetchedEmotes"), Id + TEXT(".png")
            );
            if (!FPaths::FileExists(P))
            {
                bAll = false;
                break;
            }
        }
        if (bAll)
            return true;
        FPlatformProcess::Sleep(0.1f);
    }
    return false;
}
void FTwitchChatConnection::RefreshOAuthToken(TFunction<void(bool)> OnComplete)
{
    const UTwitchChatSettings* S = GetDefault<UTwitchChatSettings>();
    if (S->RefreshToken.IsEmpty())
    {
        UE_LOG(LogTwitchChat, Error, TEXT("No refresh token available"));
        OnComplete(false);
        return;
    }

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(TEXT("https://id.twitch.tv/oauth2/token"));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
    FString Body = FString::Printf(
        TEXT("grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s"),
        *S->RefreshToken, *S->ClientId, *S->ClientSecret
    );
    Req->SetContentAsString(Body);

    Req->OnProcessRequestComplete().BindLambda(
        [this, OnComplete](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
        {
            if (bOK && Resp.IsValid() && Resp->GetResponseCode() == 200)
            {
                TSharedPtr<FJsonObject> J;
                TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
                if (FJsonSerializer::Deserialize(R, J))
                {
                    if (UTwitchChatSettings* M = GetMutableDefault<UTwitchChatSettings>())
                    {
                        if (J->HasField(TEXT("access_token")))
                        {
                            M->AccessToken = J->GetStringField(TEXT("access_token"));
                            OAuthToken = M->AccessToken;
                        }
                        if (J->HasField(TEXT("refresh_token")))
                        {
                            M->RefreshToken = J->GetStringField(TEXT("refresh_token"));
                        }
                        M->SaveConfig();  
                        UE_LOG(LogTwitchChat, Log, TEXT("Tokens saved to INI after refresh."));
                        OnComplete(true);
                        return;
                    }
                }
            }
            UE_LOG(LogTwitchChat, Error,
                TEXT("Failed to refresh OAuth token: %s"),
                Resp.IsValid() ? *Resp->GetContentAsString() : TEXT("no-response")
            );
            OnComplete(false);
        }
    );
    Req->ProcessRequest();
}
