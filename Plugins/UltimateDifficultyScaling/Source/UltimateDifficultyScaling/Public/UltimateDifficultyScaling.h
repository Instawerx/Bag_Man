// Copyright 2025, BlueprintsLab, All rights reserved

#pragma once

#include "Modules/ModuleManager.h"

class FUltimateDifficultyScalingModule : public IModuleInterface
{
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};