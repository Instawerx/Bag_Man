// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

/** Log category for AFL cosmetic-layer EDITOR tooling (the [ItemDefWire] batch logs here). */
AFLCOSMETICCOREEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogAFLCosmeticCoreEditor, Log, All);

class FAFLCosmeticCoreEditorModule : public IModuleInterface
{
};
