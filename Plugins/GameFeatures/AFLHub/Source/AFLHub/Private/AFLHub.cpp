// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLHub.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAFLHub);

#define LOCTEXT_NAMESPACE "AFLHub"

void FAFLHubModule::StartupModule()
{
	UE_LOG(LogAFLHub, Log, TEXT("AFLHub module loaded"));
}

void FAFLHubModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAFLHubModule, AFLHub)
