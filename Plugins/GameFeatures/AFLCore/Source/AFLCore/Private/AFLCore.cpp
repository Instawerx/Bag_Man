// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLCore.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAFLCore);

#define LOCTEXT_NAMESPACE "AFLCore"

void FAFLCoreModule::StartupModule()
{
	UE_LOG(LogAFLCore, Log, TEXT("AFLCore module loaded"));
}

void FAFLCoreModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAFLCoreModule, AFLCore)
