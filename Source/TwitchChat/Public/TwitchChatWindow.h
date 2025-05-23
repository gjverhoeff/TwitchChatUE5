// TwitchChatWindow.h

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"                // FActiveTimerHandle
#include "Widgets/SBoxPanel.h"              // SHorizontalBox, SVerticalBox
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "AnimatedTexture2D.h"
#include "TwitchChatMessage.h"
#include "TwitchChatConnection.h"
#include "TwitchChatSettings.h"
#include "EmoteInfo.h"

class STwitchChatWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STwitchChatWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
    EActiveTimerReturnType HandleAnimationTick(double InCurrentTime, float InDeltaTime);
    ~STwitchChatWindow();

private:
    // UI
    TSharedPtr<STextBlock>                               ChannelLabel;
    TSharedPtr<SButton>                                  SetChannelButton;
    TSharedPtr<SButton>                                  ConnectButton;
    TSharedPtr<SButton>                                  DisconnectButton;
    TSharedPtr<SButton>                                  ClearButton;
    TSharedPtr<SListView<TSharedPtr<FTwitchChatMessage>>> ListView;

    // Data
    TArray<TSharedPtr<FTwitchChatMessage>> Messages;
    FDelegateHandle                 MessageHandle;

    // Animation‐timer handle
    TSharedPtr<FActiveTimerHandle> AnimationTimerHandle;

    // Emote brushes
    TMap<FString, TSharedPtr<FSlateBrush>> EmoteBrushes;

    // Cached width for wrapping
    float ChatPanelWidth = 0.f;

    // Helpers & callbacks
    FText       GetChannelLabel() const;
    FSlateColor GetConnectionColor() const;
    void        LoadEmoteBrushes();

    FReply OnSetChannelClicked();
    void   OnConnectClicked();
    void   OnDisconnectClicked();
    void   OnClearClicked();
    void   HandleIncoming(const FTwitchChatMessage& Msg);

    TSharedRef<ITableRow> OnGenerateRow(
        TSharedPtr<FTwitchChatMessage> Item,
        const TSharedRef<STableViewBase>& OwnerTable) const;
};
