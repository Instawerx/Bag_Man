// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLCombat.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAFLCombat);

#define LOCTEXT_NAMESPACE "AFLCombat"

void FAFLCombatModule::StartupModule()
{
	UE_LOG(LogAFLCombat, Log, TEXT("AFLCombat module loaded"));
}

void FAFLCombatModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAFLCombatModule, AFLCombat)
