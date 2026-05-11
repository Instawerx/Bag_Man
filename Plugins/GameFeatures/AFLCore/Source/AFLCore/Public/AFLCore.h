// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"

AFLCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLCore, Log, All);

class FAFLCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
