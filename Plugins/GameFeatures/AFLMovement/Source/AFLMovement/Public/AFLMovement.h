// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Logging/LogMacros.h"

AFLMOVEMENT_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLMovement, Log, All);

class FAFLMovementModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
