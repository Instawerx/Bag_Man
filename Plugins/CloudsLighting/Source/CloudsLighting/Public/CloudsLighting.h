// Copyright (c) 2021-2024 by Lukerrr

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCloudsLightingModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
