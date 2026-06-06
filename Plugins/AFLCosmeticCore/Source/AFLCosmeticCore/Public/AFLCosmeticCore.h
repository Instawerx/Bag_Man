// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

/** Log category for the AFL cosmetic catalog / economy core (the [Catalog] subsystem logs here). */
AFLCOSMETICCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLCosmeticCore, Log, All);

class FAFLCosmeticCoreModule : public IModuleInterface
{
};
