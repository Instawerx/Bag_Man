// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"

AFLCOMBATTESTS_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLCombatTests, Log, All);

class FAFLCombatTestsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
