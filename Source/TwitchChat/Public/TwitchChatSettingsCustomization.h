
#pragma once

#if WITH_EDITOR
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "Input/Reply.h"

class FTwitchChatSettingsCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    FReply OnGenerateTokenClicked();
    TWeakObjectPtr<class UTwitchChatSettings> Settings;
};
#endif