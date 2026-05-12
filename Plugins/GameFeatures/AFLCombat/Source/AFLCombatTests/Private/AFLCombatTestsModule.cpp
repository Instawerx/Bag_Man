// Copyright C12 AI Gaming. All Rights Reserved.
// Module aggregator for the AFLCombatTests module. Tests auto-register via
// IMPLEMENT_SIMPLE_AUTOMATION_TEST in AFLDamageExecCalcSpec.cpp.

#include "AFLCombatTests.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAFLCombatTests);

#define LOCTEXT_NAMESPACE "AFLCombatTests"

void FAFLCombatTestsModule::StartupModule()
{
	UE_LOG(LogAFLCombatTests, Log, TEXT("AFLCombatTests module loaded"));
}

void FAFLCombatTestsModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAFLCombatTestsModule, AFLCombatTests)
