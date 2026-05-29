// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"

AFLDISMEMBER_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLDismember, Log, All);

class FAFLDismemberModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
