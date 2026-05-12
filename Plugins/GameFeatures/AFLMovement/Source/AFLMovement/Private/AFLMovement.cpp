// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLMovement.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAFLMovement);

#define LOCTEXT_NAMESPACE "AFLMovement"

void FAFLMovementModule::StartupModule()
{
	UE_LOG(LogAFLMovement, Log, TEXT("AFLMovement module loaded"));
}

void FAFLMovementModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAFLMovementModule, AFLMovement)
