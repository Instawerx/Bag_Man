// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"

AFLCOMBAT_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLCombat, Log, All);

class FAFLCombatModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
