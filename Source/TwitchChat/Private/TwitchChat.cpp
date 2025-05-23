
#include "TwitchChat.h"
#include "TwitchChatSettings.h"
#include "TwitchChatWindow.h"


#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "WebSocketsModule.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "PropertyEditorModule.h" 
#include "TwitchChatSettingsCustomization.h"
#endif

#define LOCTEXT_NAMESPACE "FTwitchChatModule"
static const FName TwitchChatTabName("TwitchChatWindow");

void FTwitchChatModule::StartupModule()
{
   
    FModuleManager::Get().LoadModuleChecked("WebSockets");

    static TSharedPtr<FSlateStyleSet> TwitchChatStyle;
    if (!TwitchChatStyle.IsValid())
    {
        TwitchChatStyle = MakeShareable(new FSlateStyleSet("TwitchChatStyle"));
        FString BaseDir = IPluginManager::Get().FindPlugin(TEXT("TwitchChat"))->GetBaseDir();
        TwitchChatStyle->SetContentRoot(BaseDir / TEXT("Resources"));
        TwitchChatStyle->Set(
            "TwitchChat.OpenPluginWindow.Small",
            new FSlateImageBrush(
                TwitchChatStyle->RootToContentDir(TEXT("TwitchChat_16x"), TEXT(".png")),
                FVector2D(16, 16)
            )
        );
        TwitchChatStyle->Set(
            "TwitchChat.OpenPluginWindow.Large",
            new FSlateImageBrush(
                TwitchChatStyle->RootToContentDir(TEXT("TwitchChat_40x"), TEXT(".png")),
                FVector2D(40, 40)
            )
        );
        FSlateStyleRegistry::RegisterSlateStyle(*TwitchChatStyle);
    }

#if WITH_EDITOR
    if (ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        Settings->RegisterSettings(
            "Project", "Plugins", "TwitchChat",
            LOCTEXT("RuntimeSettingsName", "Twitch Chat"),
            LOCTEXT("RuntimeSettingsDescription", "Configure Twitch Chat credentials."),
            GetMutableDefault<UTwitchChatSettings>()
        );
    }


    {
        FPropertyEditorModule& PropMod =
            FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropMod.RegisterCustomClassLayout(
            "TwitchChatSettings",
            FOnGetDetailCustomizationInstance::CreateStatic(&FTwitchChatSettingsCustomization::MakeInstance)
        );
        PropMod.NotifyCustomizationModuleChanged();
    }


    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
            {
                UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
                FToolMenuSection& Section = ToolsMenu->FindOrAddSection(
                    "Twitch",
                    LOCTEXT("TwitchHeading", "Twitch")
                );
                Section.AddMenuEntry(
                    "TwitchChatWindow",
                    LOCTEXT("TwitchChatMenuLabel", "Twitch Chat"),
                    LOCTEXT("TwitchChatMenuTip", "Open the Twitch Chat panel."),
                    FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Tabs.OutputLog"),
                    FUIAction(FExecuteAction::CreateStatic([]()
                        {
                            FGlobalTabmanager::Get()->TryInvokeTab(TwitchChatTabName);
                        }))
                );
            })
    );
#endif


    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        TwitchChatTabName,
        FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
            {
                return SNew(SDockTab)
                    .TabRole(ETabRole::NomadTab)[
                        SNew(STwitchChatWindow)
                    ];
            })
    )
        .SetDisplayName(LOCTEXT("TwitchChatTabTitle", "Twitch Chat"))
        .SetMenuType(ETabSpawnerMenuType::Hidden)
        .SetIcon(FSlateIcon(
            TwitchChatStyle->GetStyleSetName(),
            TEXT("TwitchChat.OpenPluginWindow.Small")
        ));
}

void FTwitchChatModule::ShutdownModule()
{
#if WITH_EDITOR
   
    if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
    {
        FPropertyEditorModule& PropMod =
            FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropMod.UnregisterCustomClassLayout("TwitchChatSettings");
        PropMod.NotifyCustomizationModuleChanged();
    }

   
    if (ISettingsModule* Settings = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
    {
        Settings->UnregisterSettings("Project", "Plugins", "TwitchChat");
    }
    if (FModuleManager::Get().IsModuleLoaded("ToolMenus"))
    {
        UToolMenus::UnregisterOwner(this);
    }
#endif

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TwitchChatTabName);
    if (auto Style = FSlateStyleRegistry::FindSlateStyle("TwitchChatStyle"))
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*Style);
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTwitchChatModule, TwitchChat)
