// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"

AFLCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLCore, Log, All);

class FAFLCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Set only when this module started before engine init and had to defer the cook sweep. */
	FDelegateHandle PostEngineInitHandle;
};
