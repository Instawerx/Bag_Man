// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLCore.h"

#include "Cook/AFLCookedAssetRegistry.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAFLCore);

#define LOCTEXT_NAMESPACE "AFLCore"

void FAFLCoreModule::StartupModule()
{
	UE_LOG(LogAFLCore, Log, TEXT("AFLCore module loaded"));

	// Sweep every string-referenced asset path declared via AFL_COOKED_ASSET and log an ERROR
	// for any that would be absent from a packaged build. See Cook/AFLCookedAssetRegistry.h --
	// this class of defect cannot reproduce in PIE, and has shipped three times.
	//
	// AFLCore is a GameFeature, so this module starts on GameFeature load rather than engine
	// init. If the engine is already up we sweep immediately; otherwise we wait for
	// OnPostEngineInit, since the editor path needs the asset registry and config to be live.
	if (GEngine != nullptr)
	{
		FAFLCookedAssetRegistry::ValidateAll(TEXT("AFLCore startup"));
	}
	else
	{
		PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda([]()
		{
			FAFLCookedAssetRegistry::ValidateAll(TEXT("post engine init"));
		});
	}
}

void FAFLCoreModule::ShutdownModule()
{
	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAFLCoreModule, AFLCore)
