// Copyright 2025, BlueprintsLab, All rights reserved
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UDifficultySettingsBridge.generated.h"

class SUltimateDifficultyScalingWindow;
class SWebBrowser;

/**
 * Settings/Info bridge — backs settings.html (Info tab + "Get the full Copilot" upsell).
 *   JS calls: window.ue.bridge.<method>(args)
 *
 * The settings browser lives in a hidden SWidgetSwitcher slot and is bound lazily,
 * so JS also emits a console.log/document.title RPC fallback (see the host widget's
 * DispatchSettingsRpc) for Mac reliability — exactly like CodeExplainer's settings.
 */
UCLASS()
class UDifficultySettingsBridge : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<SUltimateDifficultyScalingWindow> OwnerWidget;
	TWeakPtr<SWebBrowser> BrowserRef;

	UFUNCTION() void    CloseSettings();
	UFUNCTION() void    OpenExternalUrl(const FString& Url);
	UFUNCTION() FString RequestPluginInfo();   // returns JSON string
	UFUNCTION() FString LoadAllSettings();     // returns JSON: { slots, activeSlot, providers }

	// Mutations arrive via the console/title RPC fallback (handled in the host widget),
	// so they are NOT UFUNCTIONs here — that avoids the double-apply on Windows.
	static FString SettingsToJson();           // shared builder used by host push + the pull above

	static FString BuildSettingsHtmlDataUri();
};
