
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "WebSocketsModule.h"  

class FTwitchChatModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};