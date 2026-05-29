// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismember.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAFLDismember);

#define LOCTEXT_NAMESPACE "AFLDismember"

void FAFLDismemberModule::StartupModule()
{
	UE_LOG(LogAFLDismember, Log, TEXT("AFLDismember module loaded"));
}

void FAFLDismemberModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAFLDismemberModule, AFLDismember)
