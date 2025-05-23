

#include "TwitchChatWindow.h"
#include "Widgets/SWidget.h"                  


#include "Widgets/SBoxPanel.h"                  

// Layout widgets
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"

// Common Slate widgets
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"


#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture2D.h"
#include "Rendering/Texture2DResource.h"
#include "Slate/SlateTextures.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateDynamicImageBrush.h"

// Async
#include "Async/Async.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogTwitchChatWindow, Log, All);
#define LOCTEXT_NAMESPACE "TwitchChatWindow"

static UTexture2D* LoadTextureFromDisk(const FString& FullPath)
{
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *FullPath))
    {
        return nullptr;
    }

    IImageWrapperModule& ImgMod =
        FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    TSharedPtr<IImageWrapper> Wrapper =
        ImgMod.CreateImageWrapper(EImageFormat::PNG);

    if (!Wrapper.IsValid() ||
        !Wrapper->SetCompressed(FileData.GetData(), FileData.Num()))
    {
        return nullptr;
    }

    TArray<uint8> RawBGRA;
    if (!Wrapper->GetRaw(ERGBFormat::BGRA, 8, RawBGRA))
    {
        return nullptr;
    }

    const int32 W = Wrapper->GetWidth();
    const int32 H = Wrapper->GetHeight();
    UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
    if (!Tex)
    {
        return nullptr;
    }

    Tex->AddToRoot();
    Tex->SRGB = true;
    Tex->UpdateResource();

    void* MipData = Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(MipData, RawBGRA.GetData(), RawBGRA.Num());
    Tex->GetPlatformData()->Mips[0].BulkData.Unlock();

    return Tex;
}


STwitchChatWindow::~STwitchChatWindow()
{
    if (AnimationTimerHandle.IsValid())
    {
        UnRegisterActiveTimer(AnimationTimerHandle.ToSharedRef());
    }
    FTwitchChatConnection::Get()->OnMessage.Remove(MessageHandle);
}

void STwitchChatWindow::Construct(const FArguments& /*InArgs*/)
{
    // Channel label
    SAssignNew(ChannelLabel, STextBlock)
        .Text(this, &STwitchChatWindow::GetChannelLabel)
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12));

    // Buttons
    SAssignNew(SetChannelButton, SButton)
        .Text(LOCTEXT("SetChannel", "Set Channel"))
        .OnClicked_Lambda([this]() { return OnSetChannelClicked(); });

    SAssignNew(ConnectButton, SButton)
        .Text(LOCTEXT("Connect", "Connect"))
        .OnClicked_Lambda([this]() { OnConnectClicked(); return FReply::Handled(); });

    SAssignNew(DisconnectButton, SButton)
        .Text(LOCTEXT("Disconnect", "Disconnect"))
        .OnClicked_Lambda([this]() { OnDisconnectClicked(); return FReply::Handled(); });

    SAssignNew(ClearButton, SButton)
        .Text(LOCTEXT("Clear", "Clear"))
        .OnClicked_Lambda([this]() { OnClearClicked(); return FReply::Handled(); });

    // Load table‐based emotes
    LoadEmoteBrushes();

    // Build layout
    ChildSlot
        [
            SNew(SVerticalBox)

                // Top row
                + SVerticalBox::Slot().AutoHeight().Padding(2)
                [
                    SNew(SHorizontalBox)

                        // Status dot
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
                        [SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(this, &STwitchChatWindow::GetConnectionColor)
                        .Padding(0)
                        [SNew(SBox).WidthOverride(8).HeightOverride(8)]
                        ]

                        // Channel name
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [ChannelLabel.ToSharedRef()]

                        // Spacer
                        + SHorizontalBox::Slot().FillWidth(1)[SNew(SSpacer)]

                        // Buttons
                        + SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SetChannelButton.ToSharedRef()]
                        + SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[ConnectButton.ToSharedRef()]
                        + SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[DisconnectButton.ToSharedRef()]
                        + SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[ClearButton.ToSharedRef()]
                ]

                // Chat list
                + SVerticalBox::Slot().FillHeight(1).Padding(2)
                [
                    SAssignNew(ListView, SListView<TSharedPtr<FTwitchChatMessage>>)
                        .ListItemsSource(&Messages)
                        .OnGenerateRow(this, &STwitchChatWindow::OnGenerateRow)
                ]
        ];

    // Subscribe delegate
    MessageHandle = FTwitchChatConnection::Get()
        ->OnMessage.AddSP(this, &STwitchChatWindow::HandleIncoming);

    // Start per-frame animation ticker
    AnimationTimerHandle = RegisterActiveTimer(
        0.0,
        FWidgetActiveTimerDelegate::CreateSP(this, &STwitchChatWindow::HandleAnimationTick)
    );
}


EActiveTimerReturnType STwitchChatWindow::HandleAnimationTick(double /*Now*/, float DeltaTime)
{
    bool bNeedsRefresh = false;

    for (const auto& Pair : EmoteBrushes)
    {
        auto DynBrush = StaticCastSharedPtr<FSlateDynamicImageBrush>(Pair.Value);
        if (DynBrush.IsValid())
        {
            if (auto Anim = Cast<UAnimatedTexture2D>(DynBrush->GetResourceObject()))
            {
                Anim->Tick(DeltaTime);
                DynBrush->SetResourceObject(Anim);
                bNeedsRefresh = true;
            }
        }
    }

    if (bNeedsRefresh && ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
    return EActiveTimerReturnType::Continue;
}


void STwitchChatWindow::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    ChatPanelWidth = AllottedGeometry.GetLocalSize().X;
}

//-----------------------------------------------------------------------------
// Helpers & callbacks
//-----------------------------------------------------------------------------
FText STwitchChatWindow::GetChannelLabel() const
{
    return FText::Format(
        LOCTEXT("ChannelFmt", "Channel: {0}"),
        FText::FromString(GetMutableDefault<UTwitchChatSettings>()->LastChannel)
    );
}

FSlateColor STwitchChatWindow::GetConnectionColor() const
{
    return FTwitchChatConnection::Get()->IsConnected()
        ? FSlateColor(FLinearColor::Green)
        : FSlateColor(FLinearColor::Red);
}

void STwitchChatWindow::LoadEmoteBrushes()
{
    EmoteBrushes.Empty();
    if (auto Settings = GetMutableDefault<UTwitchChatSettings>())
    {
        auto LoadTable = [&](UDataTable* Table)
            {
                if (!Table) return;
                for (auto& Row : Table->GetRowMap())
                {
                    auto Info = reinterpret_cast<const FEmoteInfo*>(Row.Value);
                    if (auto Res = Info->Texture.LoadSynchronous())
                    {
                        if (auto Anim = Cast<UAnimatedTexture2D>(Res))
                        {
                            auto Brush = MakeShared<FSlateDynamicImageBrush>(
                                FName(*Info->Id), Info->Size, FLinearColor::White,
                                ESlateBrushTileType::NoTile, ESlateBrushImageType::FullColor
                            );
                            Brush->SetResourceObject(Anim);
                            EmoteBrushes.Add(Info->Id, Brush);
                        }
                        else if (auto Tex = Cast<UTexture2D>(Res))
                        {
                            EmoteBrushes.Add(Info->Id, MakeShared<FSlateImageBrush>(Tex, Info->Size));
                        }
                    }
                }
            };
        LoadTable(Settings->GlobalEmoteTable);
        LoadTable(Settings->ChannelEmoteTable);
    }
}

FReply STwitchChatWindow::OnSetChannelClicked()
{
#if WITH_EDITOR
    if (auto M = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        M->ShowViewer("Project", "Plugins", "TwitchChat");
    }
#endif
    return FReply::Handled();
}

void STwitchChatWindow::OnConnectClicked()
{
    if (auto S = GetMutableDefault<UTwitchChatSettings>())
    {
        Async(EAsyncExecution::Thread, [S]()
            {
                FTwitchChatConnection::Get()
                    ->Connect(S->UserName, S->AccessToken, S->LastChannel, S->Port);
            });
    }
}

void STwitchChatWindow::OnDisconnectClicked()
{
    FTwitchChatConnection::Get()->Disconnect();
}

void STwitchChatWindow::OnClearClicked()
{
    Messages.Empty();
    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

void STwitchChatWindow::HandleIncoming(const FTwitchChatMessage& Msg)
{
    if (const auto S = GetDefault<UTwitchChatSettings>())
    {
        int32 Max = S->MaxMessages;
        if (Max <= 0) { Messages.Empty(); }
        else
        {
            Messages.Add(MakeShared<FTwitchChatMessage>(Msg));
            if (Messages.Num() > Max)
            {
                Messages.RemoveAt(0, Messages.Num() - Max);
            }
        }
    }
    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
        if (Messages.Num() > 0) ListView->ScrollToBottom();
    }
}

TSharedRef<ITableRow> STwitchChatWindow::OnGenerateRow(
    TSharedPtr<FTwitchChatMessage> Item,
    const TSharedRef<STableViewBase>& OwnerTable
) const
{
    struct FSeg { FString Id; int32 Start, End; };
    TArray<FSeg> Segs;
    for (int32 i = 0; i < Item->EmoteIds.Num(); ++i)
    {
        Segs.Add({ Item->EmoteIds[i],Item->EmoteRanges[i].X,Item->EmoteRanges[i].Y });
    }
    Segs.Sort([](auto& A, auto& B) { return A.Start < B.Start; });

    auto WrapAt = [this]() { return FMath::Max(0.f, ChatPanelWidth - 80.f); };
    TSharedRef<SWrapBox> Box = SNew(SWrapBox).UseAllottedSize(true);

    // username prefix
    Box->AddSlot().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
                .Text(FText::FromString(Item->UserName + TEXT(": ")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                .ColorAndOpacity(Item->UserColor)
                .WrapTextAt_Lambda(WrapAt)
        ];

    const FString& Txt = Item->Message;
    int32 Cursor = 0;

    for (const auto& Seg : Segs)
    {
        if (Seg.Start > Cursor)
        {
            FString Pre = Txt.Mid(Cursor, Seg.Start - Cursor);
            Box->AddSlot().VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Text(FText::FromString(Pre)).WrapTextAt_Lambda(WrapAt)
                ];
        }

        // lookup brush
        TSharedPtr<FSlateBrush> Brush = EmoteBrushes.FindRef(Seg.Id);
        if (!Brush)
        {
            // fallback to disk
            const FString Path = FPaths::ProjectSavedDir() / TEXT("FetchedEmotes") / (Seg.Id + TEXT(".png"));
            if (FPaths::FileExists(Path))
            {
                if (UTexture2D* Tex = LoadTextureFromDisk(Path))
                {
                    auto NewB = MakeShared<FSlateImageBrush>(Tex, FVector2D(Tex->GetSizeX(), Tex->GetSizeY()));
                    const_cast<STwitchChatWindow*>(this)->EmoteBrushes.Add(Seg.Id, NewB);
                    Brush = NewB;
                }
            }
        }

        if (Brush)
        {
            FVector2D Orig = Brush->ImageSize;
            float Scale = 14.f / Orig.Y;
            FVector2D Sz = Orig * Scale;
            Box->AddSlot().VAlign(VAlign_Center)
                [
                    SNew(SBox).WidthOverride(Sz.X).HeightOverride(Sz.Y)
                        [
                            SNew(SImage).Image(Brush.Get())
                        ]
                ];
        }

        Cursor = Seg.End + 1;
    }

    if (Cursor < Txt.Len())
    {
        FString Tail = Txt.Mid(Cursor);
        Box->AddSlot().VAlign(VAlign_Center)
            [
                SNew(STextBlock).Text(FText::FromString(Tail)).WrapTextAt_Lambda(WrapAt)
            ];
    }

    return SNew(STableRow<TSharedPtr<FTwitchChatMessage>>, OwnerTable)[Box];
}
#undef LOCTEXT_NAMESPACE
