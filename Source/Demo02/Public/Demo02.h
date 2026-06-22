// Demo02.h — 游戏模块头文件
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FDemo02Module : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
