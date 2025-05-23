

#if WITH_EDITOR
#include "TwitchChatSettingsCustomization.h"

#include "TwitchChatSettings.h"
#include "TwitchChatConnection.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IDetailCustomization> FTwitchChatSettingsCustomization::MakeInstance()
{
    return MakeShared<FTwitchChatSettingsCustomization>();
}


//Part in the settings needed to create the buttons

void FTwitchChatSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);
    if (Objects.Num())
    {
        Settings = Cast<UTwitchChatSettings>(Objects[0].Get());
    }

    IDetailCategoryBuilder& Cat = DetailBuilder.EditCategory("Settings");

    Cat.AddCustomRow(FText::FromString("Generate Token"))
        .NameContent()
        [
            SNew(STextBlock)
                .Text(FText::FromString("Generate Access Token"))
                .Font(IDetailLayoutBuilder::GetDetailFont())
        ]
        .ValueContent()
        [
            SNew(SButton)
                .Text(FText::FromString("Generate Token"))
                .OnClicked(FOnClicked::CreateSP(this, &FTwitchChatSettingsCustomization::OnGenerateTokenClicked))
        ];
}

FReply FTwitchChatSettingsCustomization::OnGenerateTokenClicked()
{
    if (Settings.IsValid())
    {
        FTwitchChatConnection::Get()->StartDeviceFlowInteractive();
    }
    return FReply::Handled();
}
#endif