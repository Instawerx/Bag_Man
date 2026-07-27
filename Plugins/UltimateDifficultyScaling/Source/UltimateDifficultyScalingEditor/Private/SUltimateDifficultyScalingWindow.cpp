// Copyright 2025, BlueprintsLab, All rights reserved

#include "SUltimateDifficultyScalingWindow.h"
#include "Widget/UDifficultyToolBridge.h"
#include "Widget/UDifficultySettingsBridge.h"

#include "DifficultyScalingConfig.h"
#include "DifficultyScalingSettings.h"
#include "DifficultyScalableComponent.h"

#include "AI/UDS_ApiKeyManager.h"
#include "AI/UDS_AIClient.h"

// Slate / web
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Framework/Application/SlateApplication.h"
#include "WebBrowserModule.h"
#include "SWebBrowser.h"
#include "IWebBrowserWindow.h"
#include "Async/Async.h"

// Editor / reflection
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

// Asset registry / content browser
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "DragAndDrop/AssetDragDropOp.h"

// Json
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

// Notifications
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "UltimateDifficultyScalingWindow"

// ─────────────────────────────────────────────────────────────────────────────
// Local helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
	FString SerializeObj(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	FString SerializeArr(const TArray<TSharedPtr<FJsonValue>>& Arr)
	{
		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Arr, Writer);
		return Out;
	}

	/** Escape a string so it can be embedded inside a single-quoted JS string literal. */
	FString EscapeForJsString(const FString& In)
	{
		FString S = In;
		S.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		S.ReplaceInline(TEXT("'"),  TEXT("\\'"));
		S.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		S.ReplaceInline(TEXT("\r"), TEXT(""));
		return S;
	}

	bool IsNumericProperty(FName TypeName, bool& bOutInteger)
	{
		bOutInteger = (TypeName == FIntProperty::StaticClass()->GetFName()
			|| TypeName == FInt64Property::StaticClass()->GetFName()
			|| TypeName == FByteProperty::StaticClass()->GetFName());
		return (TypeName == FFloatProperty::StaticClass()->GetFName()
			|| TypeName == FDoubleProperty::StaticClass()->GetFName()
			|| bOutInteger);
	}

	FBlueprintDifficultySettings* FindSettings(UDifficultyScalingConfig* Cfg, UBlueprint* BP)
	{
		if (!Cfg || !BP) return nullptr;
		const FSoftObjectPath Target(BP);
		return Cfg->BlueprintSettings.FindByPredicate(
			[&](const FBlueprintDifficultySettings& S) { return S.TargetBlueprint.ToSoftObjectPath() == Target; });
	}

	// ── AI value helpers ──
	bool ValidateNumericStr(const FString& Text, bool bInteger)
	{
		if (Text.IsEmpty()) return true;
		if (Text.Find(TEXT("-"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1) != INDEX_NONE) return false;
		int32 Dots = 0;
		for (TCHAR C : Text)
		{
			if (C == TEXT('.')) { if (bInteger) return false; ++Dots; }
			else if (!FChar::IsDigit(C) && C != TEXT('-')) return false;
		}
		return Dots <= 1;
	}

	FString NormalizeBoolStr(const FString& V)
	{
		const FString L = V.TrimStartAndEnd().ToLower();
		if (L == TEXT("true") || L == TEXT("1") || L == TEXT("yes") || L == TEXT("on")) return TEXT("true");
		return TEXT("false");
	}

	/** Robustly stringify a JSON value (the model may return numbers/bools instead of strings). */
	FString JsonValToStr(const TSharedPtr<FJsonValue>& V)
	{
		if (!V.IsValid()) return FString();
		switch (V->Type)
		{
		case EJson::String:  return V->AsString();
		case EJson::Boolean: return V->AsBool() ? TEXT("true") : TEXT("false");
		case EJson::Number:
		{
			const double D = V->AsNumber();
			if (FMath::IsNearlyEqual(D, FMath::RoundToDouble(D))) return FString::Printf(TEXT("%lld"), (int64)D);
			return FString::SanitizeFloat(D);
		}
		default: return FString();
		}
	}

	/** Coerce a raw model value to a valid value for the given spec, falling back when invalid. */
	FString CoerceValue(const TSharedPtr<FJsonObject>& Spec, const FString& Raw, const FString& Fallback)
	{
		FString Control = TEXT("text");
		Spec->TryGetStringField(TEXT("control"), Control);
		const FString Trimmed = Raw.TrimStartAndEnd();

		if (Control == TEXT("bool")) return NormalizeBoolStr(Trimmed);
		if (Control == TEXT("enum"))
		{
			const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
			if (Spec->TryGetArrayField(TEXT("options"), Options))
			{
				for (const TSharedPtr<FJsonValue>& O : *Options)
				{
					const FString Opt = O->AsString();
					if (Opt.Equals(Trimmed, ESearchCase::IgnoreCase)) return Opt;
				}
				if (!Fallback.IsEmpty()) return Fallback;
				if (Options->Num() > 0) return (*Options)[0]->AsString();
			}
			return Fallback;
		}
		if (Control == TEXT("numeric"))
		{
			bool bInteger = false; Spec->TryGetBoolField(TEXT("integer"), bInteger);
			if (ValidateNumericStr(Trimmed, bInteger)) return Trimmed;
			return Fallback.IsEmpty() ? TEXT("0") : Fallback;
		}
		return Raw;
	}

	TSharedPtr<FJsonObject> FindSpecByPath(const TArray<TSharedPtr<FJsonObject>>& Specs, const FString& Path)
	{
		for (const TSharedPtr<FJsonObject>& S : Specs)
		{
			FString P;
			if (S->TryGetStringField(TEXT("path"), P) && P == Path) return S;
		}
		return nullptr;
	}

	/** Pull the JSON object out of a model reply that may be fenced or have stray prose. */
	FString ExtractJsonObjectStr(const FString& Reply)
	{
		FString S = Reply;
		S.ReplaceInline(TEXT("```json"), TEXT(""));
		S.ReplaceInline(TEXT("```"), TEXT(""));
		int32 Start = INDEX_NONE, End = INDEX_NONE;
		if (S.FindChar(TEXT('{'), Start) && S.FindLastChar(TEXT('}'), End) && End > Start)
		{
			return S.Mid(Start, End - Start + 1);
		}
		return S.TrimStartAndEnd();
	}

	FString DescribeSpecLine(const TSharedPtr<FJsonObject>& Spec)
	{
		FString Path; Spec->TryGetStringField(TEXT("path"), Path);
		FString Control = TEXT("text"); Spec->TryGetStringField(TEXT("control"), Control);

		FString TypeDesc;
		if (Control == TEXT("bool")) TypeDesc = TEXT("boolean (\"true\" or \"false\")");
		else if (Control == TEXT("enum"))
		{
			const TArray<TSharedPtr<FJsonValue>>* Opt = nullptr;
			TArray<FString> O;
			if (Spec->TryGetArrayField(TEXT("options"), Opt)) for (const auto& V : *Opt) O.Add(V->AsString());
			TypeDesc = FString::Printf(TEXT("enum, exactly one of [%s]"), *FString::Join(O, TEXT(", ")));
		}
		else if (Control == TEXT("numeric"))
		{
			bool bInt = false; Spec->TryGetBoolField(TEXT("integer"), bInt);
			TypeDesc = bInt ? TEXT("integer number") : TEXT("decimal number");
		}
		else TypeDesc = TEXT("text");

		FString Line = FString::Printf(TEXT("- %s: %s"), *Path, *TypeDesc);
		const TArray<TSharedPtr<FJsonValue>>* Cur = nullptr;
		if (Spec->TryGetArrayField(TEXT("current"), Cur) && Cur->Num() > 0)
		{
			TArray<FString> C; for (const auto& V : *Cur) C.Add(JsonValToStr(V));
			Line += FString::Printf(TEXT(", current [%s]"), *FString::Join(C, TEXT(", ")));
		}
		return Line + TEXT("\n");
	}

	FString BuildUserPrompt(const FString& Intent, const TArray<FString>& DiffNames,
		const TArray<TSharedPtr<FJsonObject>>& Specs, bool bAllowAdd)
	{
		FString S;
		S += FString::Printf(TEXT("Designer intent: %s\n\n"), *Intent);
		S += FString::Printf(TEXT("Difficulties in order: %s  (%d values per variable)\n\n"),
			*FString::Join(DiffNames, TEXT(", ")), DiffNames.Num());

		S += TEXT("Variables to SET values for:\n");
		bool bAnyToSet = false, bAnyCandidate = false;
		for (const TSharedPtr<FJsonObject>& Spec : Specs)
		{
			bool bIsNew = false; Spec->TryGetBoolField(TEXT("isNew"), bIsNew);
			if (bIsNew) { bAnyCandidate = true; continue; }
			bAnyToSet = true; S += DescribeSpecLine(Spec);
		}
		if (!bAnyToSet) S += TEXT("(none added yet)\n");

		if (bAllowAdd && bAnyCandidate)
		{
			S += TEXT("\nYou MAY also add any of these available variables if they meaningfully affect difficulty (use the exact path as the key):\n");
			for (const TSharedPtr<FJsonObject>& Spec : Specs)
			{
				bool bIsNew = false; Spec->TryGetBoolField(TEXT("isNew"), bIsNew);
				if (bIsNew) S += DescribeSpecLine(Spec);
			}
		}

		S += TEXT("\nReturn ONLY a JSON object mapping each variable's exact name/path to an array of string values, e.g. {\"Health\":[\"100\",\"200\",\"400\"]}.");
		return S;
	}

	/** Validate the model's JSON against the specs → the onAiSuggestion payload (rows with current vs proposed). */
	TSharedRef<FJsonObject> BuildSuggestionRows(const TArray<TSharedPtr<FJsonObject>>& Specs,
		const TArray<FString>& DiffNames, const TSharedPtr<FJsonObject>& AiObj)
	{
		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> DiffArr;
		for (const FString& D : DiffNames) DiffArr.Add(MakeShared<FJsonValueString>(D));
		Payload->SetArrayField(TEXT("difficultyNames"), DiffArr);

		const int32 DiffCount = DiffNames.Num();
		TArray<TSharedPtr<FJsonValue>> Rows;

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : AiObj->Values)
		{
			TSharedPtr<FJsonObject> Spec = FindSpecByPath(Specs, Pair.Key);
			if (!Spec.IsValid()) continue; // ignore variables we didn't offer

			TArray<TSharedPtr<FJsonValue>> RawVals;
			if (Pair.Value.IsValid() && Pair.Value->Type == EJson::Array) RawVals = Pair.Value->AsArray();

			const TArray<TSharedPtr<FJsonValue>>* CurArr = nullptr;
			Spec->TryGetArrayField(TEXT("current"), CurArr);

			TArray<TSharedPtr<FJsonValue>> Proposed;
			for (int32 i = 0; i < DiffCount; ++i)
			{
				const FString Fallback = (CurArr && CurArr->IsValidIndex(i)) ? JsonValToStr((*CurArr)[i]) : FString();
				const FString Raw = RawVals.IsValidIndex(i) ? JsonValToStr(RawVals[i]) : Fallback;
				Proposed.Add(MakeShared<FJsonValueString>(CoerceValue(Spec, Raw, Fallback)));
			}

			TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
			FString Display = Pair.Key; Spec->TryGetStringField(TEXT("display"), Display);
			bool bIsNew = false; Spec->TryGetBoolField(TEXT("isNew"), bIsNew);
			FString Control = TEXT("text"); Spec->TryGetStringField(TEXT("control"), Control);
			Row->SetStringField(TEXT("name"), Pair.Key);
			Row->SetStringField(TEXT("display"), Display);
			Row->SetBoolField(TEXT("isNew"), bIsNew);
			Row->SetStringField(TEXT("control"), Control);
			const TArray<TSharedPtr<FJsonValue>>* Opt = nullptr;
			if (Spec->TryGetArrayField(TEXT("options"), Opt)) Row->SetArrayField(TEXT("options"), *Opt);
			bool bInt = false; if (Spec->TryGetBoolField(TEXT("integer"), bInt)) Row->SetBoolField(TEXT("integer"), bInt);
			if (CurArr) Row->SetArrayField(TEXT("current"), *CurArr);
			Row->SetArrayField(TEXT("proposed"), Proposed);
			Rows.Add(MakeShared<FJsonValueObject>(Row));
		}
		Payload->SetArrayField(TEXT("rows"), Rows);
		return Payload;
	}

	const TCHAR* AI_SYSTEM_PROMPT =
		TEXT("You are a game difficulty-balancing assistant for Unreal Engine. Given a designer's intent and a list of ")
		TEXT("variables with their types and difficulty levels, propose sensible per-difficulty values. Respond with ONLY ")
		TEXT("a JSON object (no markdown fences, no commentary) mapping each variable's exact name/path to an array of ")
		TEXT("values, one per difficulty in the given order. Numeric values must be valid numbers (integers have no decimal ")
		TEXT("point); booleans must be \"true\" or \"false\"; enum values must be exactly one of the listed options. The ")
		TEXT("array length must equal the number of difficulties. Only include variables you intend to set.");
}

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

void SUltimateDifficultyScalingWindow::Construct(const FArguments& InArgs)
{
	// Bridges must exist before the browser fires its first JS callback.
	ToolBridge = NewObject<UDifficultyToolBridge>();
	ToolBridge->AddToRoot();
	SettingsBridge = NewObject<UDifficultySettingsBridge>();
	SettingsBridge->AddToRoot();

	ChildSlot
	[
		SAssignNew(MainSwitcher, SWidgetSwitcher)
		.WidgetIndex(0)

		+ SWidgetSwitcher::Slot()
		[
			SAssignNew(ToolBrowser, SWebBrowser)
			.InitialURL(TEXT("about:blank"))
			.BrowserFrameRate(60)   // 60 fps so hover/UI feedback feels instant (chat-style 30 lags)
			.SupportsTransparency(false)
			.ShowControls(false)
			.ShowAddressBar(false)
			.OnLoadCompleted(FSimpleDelegate::CreateSP(this, &SUltimateDifficultyScalingWindow::OnToolBrowserLoaded))
		]

		+ SWidgetSwitcher::Slot()
		[
			SAssignNew(SettingsBrowser, SWebBrowser)
			.InitialURL(TEXT("about:blank"))
			.BrowserFrameRate(60)
			.SupportsTransparency(false)
			.ShowControls(false)
			.ShowAddressBar(false)
			.OnLoadCompleted(FSimpleDelegate::CreateSP(this, &SUltimateDifficultyScalingWindow::OnSettingsBrowserLoaded))
			.OnConsoleMessage(FOnConsoleMessageDelegate::CreateSP(this, &SUltimateDifficultyScalingWindow::OnSettingsBrowserConsoleMessage))
			.OnTitleChanged(FOnTextChanged::CreateSP(this, &SUltimateDifficultyScalingWindow::OnSettingsBrowserTitleChanged))
		]
	];

	ToolBridge->OwnerWidget = SharedThis(this);
	ToolBridge->BrowserRef  = ToolBrowser;
	if (ToolBrowser.IsValid())
	{
		ToolBrowser->BindUObject(TEXT("app"), ToolBridge);
	}
	SettingsBridge->OwnerWidget = SharedThis(this);
	SettingsBridge->BrowserRef  = SettingsBrowser;
	// Settings browser is bound lazily in OpenSettingsView (hidden-renderer reliability).

	if (FSlateApplication::IsInitialized())
	{
		if (ITextInputMethodSystem* TIMS = FSlateApplication::Get().GetTextInputMethodSystem())
		{
			if (ToolBrowser.IsValid())     ToolBrowser->BindInputMethodSystem(TIMS);
			if (SettingsBrowser.IsValid()) SettingsBrowser->BindInputMethodSystem(TIMS);
		}
	}

	LoadActiveConfig();

	// CEF init is async — defer the first LoadURL until the browser is ready.
	if (GEditor)
	{
		FTimerHandle DeferredLoad;
		GEditor->GetTimerManager()->SetTimer(DeferredLoad, [WeakSelf = TWeakPtr<SUltimateDifficultyScalingWindow>(SharedThis(this))]()
		{
			if (TSharedPtr<SUltimateDifficultyScalingWindow> Self = WeakSelf.Pin())
			{
				if (Self->ToolBrowser.IsValid())
					Self->ToolBrowser->LoadURL(UDifficultyToolBridge::BuildToolHtmlDataUri());
			}
		}, 0.3f, false);
	}
}

SUltimateDifficultyScalingWindow::~SUltimateDifficultyScalingWindow()
{
	if (ToolBridge)
	{
		ToolBridge->OwnerWidget.Reset();
		ToolBridge->BrowserRef.Reset();
		ToolBridge->RemoveFromRoot();
	}
	if (SettingsBridge)
	{
		SettingsBridge->OwnerWidget.Reset();
		SettingsBridge->BrowserRef.Reset();
		SettingsBridge->RemoveFromRoot();
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Browser callbacks
// ─────────────────────────────────────────────────────────────────────────────

void SUltimateDifficultyScalingWindow::OnToolBrowserLoaded()
{
	// On Mac, UE's WebBrowser is backed by Apple WKWebView, whose
	// didFinishNavigation callback fires from a WebKit IPC thread — NOT the
	// game thread. PushInitialState touches the asset registry, which asserts
	// game-thread-only. On Windows CEF dispatches this back to the game thread
	// itself, so the bug only repros on Mac. Marshal explicitly so it's safe
	// on both platforms.
	if (IsInGameThread())
	{
		PushInitialState();
		return;
	}
	TWeakPtr<SUltimateDifficultyScalingWindow> WeakSelf = StaticCastSharedRef<SUltimateDifficultyScalingWindow>(AsShared());
	AsyncTask(ENamedThreads::GameThread, [WeakSelf]()
	{
		if (TSharedPtr<SUltimateDifficultyScalingWindow> Self = WeakSelf.Pin())
			Self->PushInitialState();
	});
}

void SUltimateDifficultyScalingWindow::OnSettingsBrowserLoaded()
{
	// Same Mac WebKit thread-safety concern as OnToolBrowserLoaded — marshal.
	auto Run = [this]()
	{
		PushPluginInfoToJs();
		PushSettingsToJs();

		// Deep-link to a requested tab now that the page is actually ready (first-open path).
		if (!PendingSettingsTab.IsEmpty() && SettingsBrowser.IsValid())
		{
			SettingsBrowser->ExecuteJavascript(FString::Printf(
				TEXT("if(typeof focusTab==='function')focusTab(\"%s\")"), *PendingSettingsTab));
			PendingSettingsTab.Reset();
		}
	};
	if (IsInGameThread())
	{
		Run();
		return;
	}
	TWeakPtr<SUltimateDifficultyScalingWindow> WeakSelf = StaticCastSharedRef<SUltimateDifficultyScalingWindow>(AsShared());
	AsyncTask(ENamedThreads::GameThread, [WeakSelf]()
	{
		if (TSharedPtr<SUltimateDifficultyScalingWindow> Self = WeakSelf.Pin())
		{
			Self->PushPluginInfoToJs();
			Self->PushSettingsToJs();
			if (!Self->PendingSettingsTab.IsEmpty() && Self->SettingsBrowser.IsValid())
			{
				Self->SettingsBrowser->ExecuteJavascript(FString::Printf(
					TEXT("if(typeof focusTab==='function')focusTab(\"%s\")"), *Self->PendingSettingsTab));
				Self->PendingSettingsTab.Reset();
			}
		}
	});
}

void SUltimateDifficultyScalingWindow::OnSettingsBrowserConsoleMessage(const FString& Message, const FString& /*Source*/, int32 /*Line*/, EWebBrowserConsoleLogSeverity /*Severity*/)
{
	static const FString Prefix(TEXT("UDS_RPC:"));
	const int32 Idx = Message.Find(Prefix);
	if (Idx == INDEX_NONE) return;
	DispatchSettingsRpc(Message.RightChop(Idx + Prefix.Len()));
}

void SUltimateDifficultyScalingWindow::OnSettingsBrowserTitleChanged(const FText& Title)
{
	const FString Str = Title.ToString();
	static const FString Prefix(TEXT("UDS_RPC:"));
	if (!Str.StartsWith(Prefix)) return;
	DispatchSettingsRpc(Str.RightChop(Prefix.Len()));
}

void SUltimateDifficultyScalingWindow::DispatchSettingsRpc(const FString& Payload)
{
	// Mac WKWebView fires OnConsoleMessage / OnTitleChanged from its IPC thread.
	// Downstream we touch UObjects + the asset registry — all game-thread only.
	if (!IsInGameThread())
	{
		TWeakPtr<SUltimateDifficultyScalingWindow> WeakSelf = StaticCastSharedRef<SUltimateDifficultyScalingWindow>(AsShared());
		FString Captured = Payload;
		AsyncTask(ENamedThreads::GameThread, [WeakSelf, Captured]()
		{
			if (TSharedPtr<SUltimateDifficultyScalingWindow> Self = WeakSelf.Pin())
				Self->DispatchSettingsRpc(Captured);
		});
		return;
	}
	if (!SettingsBridge) return;

	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return;

	int32 Counter = -1;
	Obj->TryGetNumberField(TEXT("n"), Counter);
	if (Counter > 0 && Counter == LastSettingsRpcCounter) return; // dedupe console+title
	LastSettingsRpcCounter = Counter;

	FString Method;
	if (!Obj->TryGetStringField(TEXT("m"), Method)) return;
	const TArray<TSharedPtr<FJsonValue>>* ArgsArr = nullptr;
	Obj->TryGetArrayField(TEXT("a"), ArgsArr);
	auto StrArg = [&](int32 I) -> FString { return (ArgsArr && ArgsArr->IsValidIndex(I)) ? (*ArgsArr)[I]->AsString() : FString(); };
	auto IntArg = [&](int32 I) -> int32  { return (ArgsArr && ArgsArr->IsValidIndex(I)) ? (int32)(*ArgsArr)[I]->AsNumber() : 0; };

	const FString M = Method.ToLower();
	bool bMutated = false;
	if      (M == TEXT("closesettings"))   SettingsBridge->CloseSettings();
	else if (M == TEXT("openexternalurl")) SettingsBridge->OpenExternalUrl(StrArg(0));
	else if (M == TEXT("saveslot"))
	{
		TSharedPtr<FJsonObject> S;
		TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(StrArg(0));
		if (FJsonSerializer::Deserialize(R, S) && S.IsValid())
		{
			FUDS_ApiKeySlot Slot;
			S->TryGetNumberField(TEXT("slotIndex"),      Slot.SlotIndex);
			S->TryGetStringField(TEXT("name"),           Slot.Name);
			S->TryGetStringField(TEXT("provider"),       Slot.Provider);
			S->TryGetStringField(TEXT("apiKey"),         Slot.ApiKey);
			S->TryGetStringField(TEXT("customBaseURL"),  Slot.CustomBaseURL);
			S->TryGetStringField(TEXT("customModelName"),Slot.CustomModelName);
			S->TryGetStringField(TEXT("geminiModel"),    Slot.GeminiModel);
			S->TryGetStringField(TEXT("openaiModel"),    Slot.OpenAIModel);
			S->TryGetStringField(TEXT("claudeModel"),    Slot.ClaudeModel);
			S->TryGetStringField(TEXT("deepseekModel"),  Slot.DeepSeekModel);
			FUDS_ApiKeyManager::Get().SetSlot(Slot.SlotIndex, Slot);
			bMutated = true;
		}
	}
	else if (M == TEXT("setactiveslot")) { FUDS_ApiKeyManager::Get().SetActiveSlot(IntArg(0)); bMutated = true; }

	if (bMutated)
	{
		// Defer one frame — ExecuteJavascript synchronously from inside a CEF console/title
		// callback is dropped on Mac. Push the refreshed settings on the next tick.
		if (GEditor)
		{
			TWeakPtr<SUltimateDifficultyScalingWindow> WeakSelf(SharedThis(this));
			FTimerHandle T;
			GEditor->GetTimerManager()->SetTimer(T, [WeakSelf]()
			{
				if (TSharedPtr<SUltimateDifficultyScalingWindow> Self = WeakSelf.Pin()) Self->PushSettingsToJs();
			}, 0.05f, false);
		}
		else
		{
			PushSettingsToJs();
		}
	}
}

void SUltimateDifficultyScalingWindow::PushSettingsToJs()
{
	if (!SettingsBrowser.IsValid()) return;
	const FString Json = UDifficultySettingsBridge::SettingsToJson();
	SettingsBrowser->ExecuteJavascript(FString::Printf(
		TEXT("if(typeof onSettingsData==='function')onSettingsData('%s')"), *EscapeForJsString(Json)));
}

void SUltimateDifficultyScalingWindow::PushPluginInfoToJs()
{
	if (!SettingsBrowser.IsValid() || !SettingsBridge) return;
	const FString InfoJson = SettingsBridge->RequestPluginInfo();
	SettingsBrowser->ExecuteJavascript(FString::Printf(
		TEXT("if(typeof onPluginInfo==='function')onPluginInfo('%s')"), *EscapeForJsString(InfoJson)));
}

void SUltimateDifficultyScalingWindow::ExecOnTool(const FString& Js)
{
	if (ToolBrowser.IsValid()) ToolBrowser->ExecuteJavascript(Js);
}

// ─────────────────────────────────────────────────────────────────────────────
// Config asset
// ─────────────────────────────────────────────────────────────────────────────

UDifficultyScalingConfig* SUltimateDifficultyScalingWindow::LoadActiveConfig()
{
	const UDifficultyScalingSettings* Settings = GetDefault<UDifficultyScalingSettings>();
	if (Settings && !Settings->DifficultyConfigAsset.IsNull())
	{
		if (UDifficultyScalingConfig* Cfg = Cast<UDifficultyScalingConfig>(Settings->DifficultyConfigAsset.TryLoad()))
		{
			ConfigAsset = Cfg;
			return Cfg;
		}
	}
	ConfigAsset = nullptr;
	return nullptr;
}

void SUltimateDifficultyScalingWindow::GatherConfigAssets(TArray<FAssetData>& Out) const
{
	FAssetRegistryModule& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AR.Get().GetAssetsByClass(UDifficultyScalingConfig::StaticClass()->GetClassPathName(), Out, true);
}

void SUltimateDifficultyScalingWindow::SyncActiveConfigToProjectSettings()
{
	UDifficultyScalingSettings* Settings = GetMutableDefault<UDifficultyScalingSettings>();
	Settings->DifficultyConfigAsset = ConfigAsset.IsValid() ? FSoftObjectPath(ConfigAsset.Get()) : FSoftObjectPath();
	Settings->TryUpdateDefaultConfigFile();
}

void SUltimateDifficultyScalingWindow::SelectConfigInternal(const FString& ConfigPath)
{
	UDifficultyScalingConfig* Cfg = Cast<UDifficultyScalingConfig>(FSoftObjectPath(ConfigPath).TryLoad());
	ConfigAsset = Cfg;
	SyncActiveConfigToProjectSettings();
	SelectedBlueprint = nullptr;
	PushInitialState();
	PushVariableData();
}

void SUltimateDifficultyScalingWindow::CreateConfigInternal(const FString& Name)
{
	FString BaseName = Name.TrimStartAndEnd();
	if (BaseName.IsEmpty()) BaseName = TEXT("DA_DifficultyConfig");
	// Strip characters that aren't valid in object names.
	FString Sanitized;
	for (TCHAR C : BaseName)
	{
		if (FChar::IsAlnum(C) || C == TEXT('_')) Sanitized.AppendChar(C);
	}
	if (Sanitized.IsEmpty()) Sanitized = TEXT("DA_DifficultyConfig");

	// Ensure a unique package name under /Game.
	FString PackageName = FString::Printf(TEXT("/Game/%s"), *Sanitized);
	int32 Suffix = 1;
	while (FPackageName::DoesPackageExist(PackageName))
	{
		PackageName = FString::Printf(TEXT("/Game/%s_%d"), *Sanitized, Suffix++);
	}
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		PushToast(TEXT("Failed to create config package."), TEXT("error"));
		return;
	}
	Package->FullyLoad();

	UDifficultyScalingConfig* NewCfg = NewObject<UDifficultyScalingConfig>(
		Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
	if (!NewCfg)
	{
		PushToast(TEXT("Failed to create config asset."), TEXT("error"));
		return;
	}
	NewCfg->DifficultyNames = { TEXT("Easy"), TEXT("Normal"), TEXT("Hard") };

	FAssetRegistryModule::AssetCreated(NewCfg);
	Package->MarkPackageDirty();

	// Persist to disk so the soft-path reference resolves after restart.
	const FString FileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, NewCfg, *FileName, SaveArgs);

	ConfigAsset = NewCfg;
	SyncActiveConfigToProjectSettings();
	SelectedBlueprint = nullptr;
	PushInitialState();
	PushVariableData();
	PushToast(FString::Printf(TEXT("Created %s"), *AssetName), TEXT("success"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Difficulty names
// ─────────────────────────────────────────────────────────────────────────────

void SUltimateDifficultyScalingWindow::AddDifficultyNameInternal(const FString& Name)
{
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	if (!Cfg) return;

	Cfg->DifficultyNames.Add(Name.IsEmpty() ? FString::Printf(TEXT("Difficulty %d"), Cfg->DifficultyNames.Num() + 1) : Name);
	// Keep every variable's value array aligned with the new column count.
	for (FBlueprintDifficultySettings& BPS : Cfg->BlueprintSettings)
	{
		for (FScaledVariableData& Var : BPS.ScaledVariables)
		{
			const FString Fill = Var.ScaledValuesAsString.Num() > 0 ? Var.ScaledValuesAsString.Last() : TEXT("0");
			Var.ScaledValuesAsString.Add(Fill);
		}
	}
	Cfg->MarkPackageDirty();
	PushInitialState();
	PushVariableData();
}

void SUltimateDifficultyScalingWindow::RenameDifficultyInternal(int32 Index, const FString& Name)
{
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	if (!Cfg || !Cfg->DifficultyNames.IsValidIndex(Index)) return;
	Cfg->DifficultyNames[Index] = Name;
	Cfg->MarkPackageDirty();
	PushInitialState();
	PushVariableData();
}

void SUltimateDifficultyScalingWindow::RemoveDifficultyNameInternal(int32 Index)
{
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	if (!Cfg || !Cfg->DifficultyNames.IsValidIndex(Index)) return;
	Cfg->DifficultyNames.RemoveAt(Index);
	for (FBlueprintDifficultySettings& BPS : Cfg->BlueprintSettings)
	{
		for (FScaledVariableData& Var : BPS.ScaledVariables)
		{
			if (Var.ScaledValuesAsString.IsValidIndex(Index))
			{
				Var.ScaledValuesAsString.RemoveAt(Index);
			}
		}
	}
	Cfg->MarkPackageDirty();
	PushInitialState();
	PushVariableData();
}

// ─────────────────────────────────────────────────────────────────────────────
// Blueprints
// ─────────────────────────────────────────────────────────────────────────────

void SUltimateDifficultyScalingWindow::RequestInitialStateInternal()
{
	LoadActiveConfig();
	PushInitialState();
}

void SUltimateDifficultyScalingWindow::RefreshListInternal()
{
	LoadActiveConfig();
	SelectedBlueprint = nullptr;
	PushBlueprintList();
	PushVariableData();
}

bool SUltimateDifficultyScalingWindow::AddBlueprintToConfig(UBlueprint* Blueprint)
{
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	if (!Cfg || !Blueprint) return false;

	const FSoftObjectPath Target(Blueprint);
	const bool bExists = Cfg->BlueprintSettings.ContainsByPredicate(
		[&](const FBlueprintDifficultySettings& S) { return S.TargetBlueprint.ToSoftObjectPath() == Target; });
	if (bExists) return false;

	Cfg->BlueprintSettings.Add(FBlueprintDifficultySettings{ TSoftObjectPtr<UBlueprint>(Blueprint) });
	return true;
}

void SUltimateDifficultyScalingWindow::AddBlueprintsInternal()
{
	if (!ConfigAsset.IsValid())
	{
		PushToast(TEXT("Pick or create a Difficulty Config first."), TEXT("error"));
		return;
	}
	if (!GEditor) return;

	// Defer one tick: opening a modal asset dialog directly inside a CEF bridge
	// callback can reenter the browser message loop. Run it off that stack instead.
	FTimerHandle T;
	GEditor->GetTimerManager()->SetTimer(T, [WeakSelf = TWeakPtr<SUltimateDifficultyScalingWindow>(SharedThis(this))]()
	{
		TSharedPtr<SUltimateDifficultyScalingWindow> Self = WeakSelf.Pin();
		if (!Self.IsValid() || !Self->ConfigAsset.IsValid()) return;

		FContentBrowserModule& CBM = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		FOpenAssetDialogConfig Config;
		Config.DialogTitleOverride = LOCTEXT("PickBlueprints", "Add Blueprints to Difficulty Scaling");
		Config.bAllowMultipleSelection = true;
		Config.AssetClassNames.Add(UBlueprint::StaticClass()->GetClassPathName());

		const TArray<FAssetData> Selected = CBM.Get().CreateModalOpenAssetDialog(Config);

		int32 Added = 0;
		for (const FAssetData& Data : Selected)
		{
			if (UBlueprint* BP = Cast<UBlueprint>(Data.GetAsset()))
			{
				if (Self->AddBlueprintToConfig(BP)) ++Added;
			}
		}
		if (Added > 0)
		{
			Self->ConfigAsset->MarkPackageDirty();
			Self->PushBlueprintList();
			Self->PushToast(FString::Printf(TEXT("Added %d Blueprint(s)."), Added), TEXT("success"));
		}
	}, 0.01f, false);
}

void SUltimateDifficultyScalingWindow::RemoveBlueprintInternal(const FString& BlueprintPath)
{
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	if (!Cfg) return;

	const FSoftObjectPath Target(BlueprintPath);
	Cfg->BlueprintSettings.RemoveAll(
		[&](const FBlueprintDifficultySettings& S) { return S.TargetBlueprint.ToSoftObjectPath() == Target; });
	Cfg->MarkPackageDirty();

	if (SelectedBlueprint.IsValid() && FSoftObjectPath(SelectedBlueprint.Get()) == Target)
	{
		SelectedBlueprint = nullptr;
	}
	PushBlueprintList();
	PushVariableData();
}

void SUltimateDifficultyScalingWindow::ClearAllInternal()
{
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	if (!Cfg) return;
	Cfg->BlueprintSettings.Empty();
	Cfg->MarkPackageDirty();
	SelectedBlueprint = nullptr;
	PushBlueprintList();
	PushVariableData();
}

void SUltimateDifficultyScalingWindow::AddComponentInternal(const FString& BlueprintPath)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(FSoftObjectPath(BlueprintPath).TryLoad());
	if (!Blueprint) return;

	if (!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf<AActor>())
	{
		PushToast(TEXT("DifficultyScalableComponent can only be added to Actor Blueprints."), TEXT("error"));
		return;
	}

	// Already has one?
	if (Blueprint->SimpleConstructionScript)
	{
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(UDifficultyScalableComponent::StaticClass()))
			{
				PushToast(TEXT("That Blueprint already has a DifficultyScalableComponent."), TEXT("info"));
				return;
			}
		}

		USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(
			UDifficultyScalableComponent::StaticClass(), FName("DifficultyScalableComponent"));
		if (NewNode)
		{
			Blueprint->SimpleConstructionScript->AddNode(NewNode);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			PushBlueprintList();
			PushToast(TEXT("Added DifficultyScalableComponent."), TEXT("success"));
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Variable editor — selection + tree
// ─────────────────────────────────────────────────────────────────────────────

void SUltimateDifficultyScalingWindow::SelectBlueprintInternal(const FString& BlueprintPath)
{
	if (BlueprintPath.IsEmpty())
	{
		SelectedBlueprint = nullptr;
	}
	else
	{
		SelectedBlueprint = Cast<UBlueprint>(FSoftObjectPath(BlueprintPath).TryLoad());
	}
	PushVariableData();
}

FProperty* SUltimateDifficultyScalingWindow::ResolvePropertyByPath(const FString& DottedPath) const
{
	if (!SelectedBlueprint.IsValid() || DottedPath.IsEmpty()) return nullptr;

	UStruct* CurrentStruct = SelectedBlueprint->GeneratedClass;
	TArray<FString> Parts;
	DottedPath.ParseIntoArray(Parts, TEXT("."));

	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		if (!CurrentStruct) return nullptr;
		FProperty* Prop = FindFProperty<FProperty>(CurrentStruct, *Parts[i]);
		if (!Prop) return nullptr;
		if (i == Parts.Num() - 1) return Prop;

		if (FStructProperty* SP = CastField<FStructProperty>(Prop))
		{
			CurrentStruct = SP->Struct;
		}
		else if (FObjectProperty* OP = CastField<FObjectProperty>(Prop))
		{
			CurrentStruct = OP->PropertyClass;
		}
		else
		{
			return nullptr;
		}
	}
	return nullptr;
}

bool SUltimateDifficultyScalingWindow::IsVariablePathValid(const FString& VariablePath) const
{
	return ResolvePropertyByPath(VariablePath) != nullptr;
}

FString SUltimateDifficultyScalingWindow::GetSmartDefaultForProperty(FProperty* Property) const
{
	if (!Property) return TEXT("0");

	UEnum* EnumClass = nullptr;
	if (FEnumProperty* EP = CastField<FEnumProperty>(Property)) EnumClass = EP->GetEnum();
	else if (FByteProperty* BP = CastField<FByteProperty>(Property)) EnumClass = BP->Enum;

	if (EnumClass && EnumClass->NumEnums() > 1)
	{
		return EnumClass->GetDisplayNameTextByIndex(0).ToString();
	}
	if (Property->IsA<FBoolProperty>()) return TEXT("false");
	if (Property->IsA<FStrProperty>() || Property->IsA<FNameProperty>() || Property->IsA<FTextProperty>()) return TEXT("");
	return TEXT("0");
}

TSharedPtr<FJsonObject> SUltimateDifficultyScalingWindow::MakeControlDescriptor(const FString& VariablePath, FName StoredPropertyType) const
{
	TSharedPtr<FJsonObject> Desc = MakeShared<FJsonObject>();
	FProperty* Prop = ResolvePropertyByPath(VariablePath);

	// Enum (live property only — orphaned paths can't enumerate options)
	UEnum* EnumClass = nullptr;
	if (Prop)
	{
		if (FEnumProperty* EP = CastField<FEnumProperty>(Prop)) EnumClass = EP->GetEnum();
		else if (FByteProperty* BP = CastField<FByteProperty>(Prop)) EnumClass = BP->Enum;
	}
	if (EnumClass)
	{
		Desc->SetStringField(TEXT("control"), TEXT("enum"));
		TArray<TSharedPtr<FJsonValue>> Opts;
		for (int32 j = 0; j < EnumClass->NumEnums() - 1; ++j)
		{
			Opts.Add(MakeShared<FJsonValueString>(EnumClass->GetDisplayNameTextByIndex(j).ToString()));
		}
		Desc->SetArrayField(TEXT("options"), Opts);
		return Desc;
	}

	const FName TypeName = Prop ? Prop->GetClass()->GetFName() : StoredPropertyType;
	bool bInteger = false;
	if (TypeName == FBoolProperty::StaticClass()->GetFName())
	{
		Desc->SetStringField(TEXT("control"), TEXT("bool"));
	}
	else if (IsNumericProperty(TypeName, bInteger))
	{
		Desc->SetStringField(TEXT("control"), TEXT("numeric"));
		Desc->SetBoolField(TEXT("integer"), bInteger);
	}
	else
	{
		Desc->SetStringField(TEXT("control"), TEXT("text"));
	}
	return Desc;
}

UStruct* SUltimateDifficultyScalingWindow::ResolveContainerStructForPath(const FString& NodePath, UStruct*& OutStopAtStruct) const
{
	OutStopAtStruct = nullptr;
	if (!SelectedBlueprint.IsValid()) return nullptr;
	UClass* GenClass = SelectedBlueprint->GeneratedClass;

	if (NodePath.IsEmpty())
	{
		// The "(Self)" root: gate inherited variables here only.
		OutStopAtStruct = bShowInheritedVariables ? nullptr : (GenClass ? GenClass->GetSuperClass() : nullptr);
		return GenClass;
	}

	FProperty* Prop = ResolvePropertyByPath(NodePath);
	if (!Prop) return nullptr;
	if (FStructProperty* SP = CastField<FStructProperty>(Prop)) return SP->Struct;
	if (FObjectProperty* OP = CastField<FObjectProperty>(Prop))
	{
		if (OP->PropertyClass->IsChildOf(UActorComponent::StaticClass())) return OP->PropertyClass;
	}
	return nullptr;
}

void SUltimateDifficultyScalingWindow::BuildChildNodes(UStruct* TargetStruct, const FString& CurrentPath, UStruct* StopAtStruct, TArray<TSharedPtr<FJsonValue>>& Out) const
{
	if (!TargetStruct || TargetStruct == StopAtStruct) return;

	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	FBlueprintDifficultySettings* BPS = FindSettings(Cfg, SelectedBlueprint.Get());

	for (TFieldIterator<FProperty> It(TargetStruct, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;

		const FString DisplayName = Prop->GetDisplayNameText().ToString();
		const FString NewPath = CurrentPath.IsEmpty() ? Prop->GetName() : FString::Printf(TEXT("%s.%s"), *CurrentPath, *Prop->GetName());

		bool bExpandable = false;
		if (CastField<FStructProperty>(Prop))
		{
			bExpandable = true;
		}
		else if (FObjectProperty* OP = CastField<FObjectProperty>(Prop))
		{
			if (OP->PropertyClass->IsChildOf(UActorComponent::StaticClass())) bExpandable = true;
		}

		const bool bScaled = BPS && BPS->ScaledVariables.ContainsByPredicate(
			[&](const FScaledVariableData& V) { return V.VariableName.ToString() == NewPath; });

		TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
		Node->SetStringField(TEXT("name"), DisplayName);
		Node->SetStringField(TEXT("path"), NewPath);
		Node->SetBoolField(TEXT("expandable"), bExpandable);
		Node->SetBoolField(TEXT("addable"), !bExpandable);
		Node->SetBoolField(TEXT("scaled"), bScaled);
		Out.Add(MakeShared<FJsonValueObject>(Node));
	}

	// Inherited properties (walk up until StopAtStruct or UObject), matching the old tree.
	UStruct* Super = TargetStruct->GetSuperStruct();
	if (Super && Super != UObject::StaticClass())
	{
		BuildChildNodes(Super, CurrentPath, StopAtStruct, Out);
	}
}

void SUltimateDifficultyScalingWindow::RequestTreeChildrenInternal(const FString& NodePath, int32 RequestId)
{
	TArray<TSharedPtr<FJsonValue>> Children;
	UStruct* StopAt = nullptr;
	if (UStruct* Container = ResolveContainerStructForPath(NodePath, StopAt))
	{
		BuildChildNodes(Container, NodePath, StopAt, Children);
	}

	const FString Json = SerializeArr(Children);
	ExecOnTool(FString::Printf(
		TEXT("if(typeof onTreeChildren==='function')onTreeChildren(%d,%s)"), RequestId, *Json));
}

void SUltimateDifficultyScalingWindow::AddVariableInternal(const FString& FullPath)
{
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	FBlueprintDifficultySettings* BPS = FindSettings(Cfg, SelectedBlueprint.Get());
	if (!Cfg || !BPS) return;

	FProperty* Property = ResolvePropertyByPath(FullPath);
	if (!Property)
	{
		PushToast(TEXT("Could not resolve that property."), TEXT("error"));
		return;
	}

	// Skip duplicates.
	const bool bExists = BPS->ScaledVariables.ContainsByPredicate(
		[&](const FScaledVariableData& V) { return V.VariableName.ToString() == FullPath; });
	if (bExists) return;

	FScaledVariableData NewData;
	NewData.VariableName = FName(*FullPath);
	NewData.PropertyType = Property->GetClass()->GetFName();

	const FString DefaultValue = GetSmartDefaultForProperty(Property);
	for (int32 i = 0; i < Cfg->DifficultyNames.Num(); ++i)
	{
		NewData.ScaledValuesAsString.Add(DefaultValue);
	}

	BPS->ScaledVariables.Add(NewData);
	Cfg->MarkPackageDirty();
	PushVariableData();
}

void SUltimateDifficultyScalingWindow::RemoveVariableInternal(const FString& VariableName)
{
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	FBlueprintDifficultySettings* BPS = FindSettings(Cfg, SelectedBlueprint.Get());
	if (!Cfg || !BPS) return;

	const FName VarName(*VariableName);
	BPS->ScaledVariables.RemoveAll(
		[&](const FScaledVariableData& V) { return V.VariableName == VarName; });
	Cfg->MarkPackageDirty();
	PushVariableData();
}

void SUltimateDifficultyScalingWindow::SetValueInternal(const FString& VariableName, int32 DifficultyIndex, const FString& Value)
{
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	FBlueprintDifficultySettings* BPS = FindSettings(Cfg, SelectedBlueprint.Get());
	if (!Cfg || !BPS || DifficultyIndex < 0) return;

	const FName VarName(*VariableName);
	if (FScaledVariableData* Var = BPS->ScaledVariables.FindByPredicate(
		[&](const FScaledVariableData& V) { return V.VariableName == VarName; }))
	{
		while (!Var->ScaledValuesAsString.IsValidIndex(DifficultyIndex))
		{
			Var->ScaledValuesAsString.Add(FString());
		}
		Var->ScaledValuesAsString[DifficultyIndex] = Value;
		Cfg->MarkPackageDirty();
	}
}

void SUltimateDifficultyScalingWindow::CleanOrphansInternal()
{
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	FBlueprintDifficultySettings* BPS = FindSettings(Cfg, SelectedBlueprint.Get());
	if (!Cfg || !BPS) return;

	const int32 Original = BPS->ScaledVariables.Num();
	BPS->ScaledVariables.RemoveAll(
		[&](const FScaledVariableData& V) { return !IsVariablePathValid(V.VariableName.ToString()); });
	const int32 Removed = Original - BPS->ScaledVariables.Num();

	if (Removed > 0)
	{
		Cfg->MarkPackageDirty();
		PushVariableData();
		PushToast(FString::Printf(TEXT("Removed %d orphaned variable(s)."), Removed), TEXT("success"));
	}
	else
	{
		PushToast(TEXT("No orphaned variables found."), TEXT("info"));
	}
}

void SUltimateDifficultyScalingWindow::SetShowInheritedInternal(bool bShow)
{
	bShowInheritedVariables = bShow;
	PushVariableData();
}

// ─────────────────────────────────────────────────────────────────────────────
// JS push helpers
// ─────────────────────────────────────────────────────────────────────────────

void SUltimateDifficultyScalingWindow::PushInitialState()
{
	if (!ToolBrowser.IsValid()) return;

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	// Available config assets
	TArray<FAssetData> Configs;
	GatherConfigAssets(Configs);
	TArray<TSharedPtr<FJsonValue>> ConfigArr;
	for (const FAssetData& Data : Configs)
	{
		TSharedPtr<FJsonObject> C = MakeShared<FJsonObject>();
		C->SetStringField(TEXT("path"), Data.GetSoftObjectPath().ToString());
		C->SetStringField(TEXT("name"), Data.AssetName.ToString());
		ConfigArr.Add(MakeShared<FJsonValueObject>(C));
	}
	Root->SetArrayField(TEXT("configs"), ConfigArr);

	const bool bHasConfig = ConfigAsset.IsValid();
	Root->SetStringField(TEXT("activeConfig"), bHasConfig ? FSoftObjectPath(ConfigAsset.Get()).ToString() : TEXT(""));
	Root->SetStringField(TEXT("activeConfigName"), bHasConfig ? ConfigAsset->GetName() : TEXT(""));

	TArray<TSharedPtr<FJsonValue>> DiffNames;
	if (bHasConfig)
	{
		for (const FString& N : ConfigAsset->DifficultyNames)
		{
			DiffNames.Add(MakeShared<FJsonValueString>(N));
		}
	}
	Root->SetArrayField(TEXT("difficultyNames"), DiffNames);

	// Blueprint list
	TArray<TSharedPtr<FJsonValue>> Blueprints;
	if (bHasConfig)
	{
		for (const FBlueprintDifficultySettings& BPS : ConfigAsset->BlueprintSettings)
		{
			UBlueprint* BP = BPS.TargetBlueprint.LoadSynchronous();
			if (!BP) continue;

			bool bHasComponent = false;
			if (BP->SimpleConstructionScript)
			{
				for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
				{
					if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(UDifficultyScalableComponent::StaticClass()))
					{
						bHasComponent = true;
						break;
					}
				}
			}

			TSharedPtr<FJsonObject> B = MakeShared<FJsonObject>();
			B->SetStringField(TEXT("path"), BPS.TargetBlueprint.ToString());
			B->SetStringField(TEXT("name"), FPaths::GetBaseFilename(BPS.TargetBlueprint.ToString()));
			B->SetBoolField(TEXT("hasComponent"), bHasComponent);
			Blueprints.Add(MakeShared<FJsonValueObject>(B));
		}
	}
	Root->SetArrayField(TEXT("blueprints"), Blueprints);

	ExecOnTool(FString::Printf(
		TEXT("if(typeof onInitialState==='function')onInitialState(%s)"), *SerializeObj(Root)));
}

void SUltimateDifficultyScalingWindow::PushBlueprintList()
{
	if (!ToolBrowser.IsValid()) return;

	TArray<TSharedPtr<FJsonValue>> Blueprints;
	if (UDifficultyScalingConfig* Cfg = ConfigAsset.Get())
	{
		for (const FBlueprintDifficultySettings& BPS : Cfg->BlueprintSettings)
		{
			UBlueprint* BP = BPS.TargetBlueprint.LoadSynchronous();
			if (!BP) continue;

			bool bHasComponent = false;
			if (BP->SimpleConstructionScript)
			{
				for (USCS_Node* Node : BP->SimpleConstructionScript->GetAllNodes())
				{
					if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(UDifficultyScalableComponent::StaticClass()))
					{
						bHasComponent = true;
						break;
					}
				}
			}

			TSharedPtr<FJsonObject> B = MakeShared<FJsonObject>();
			B->SetStringField(TEXT("path"), BPS.TargetBlueprint.ToString());
			B->SetStringField(TEXT("name"), FPaths::GetBaseFilename(BPS.TargetBlueprint.ToString()));
			B->SetBoolField(TEXT("hasComponent"), bHasComponent);
			Blueprints.Add(MakeShared<FJsonValueObject>(B));
		}
	}

	ExecOnTool(FString::Printf(
		TEXT("if(typeof onBlueprintList==='function')onBlueprintList(%s)"), *SerializeArr(Blueprints)));
}

void SUltimateDifficultyScalingWindow::PushVariableData()
{
	if (!ToolBrowser.IsValid()) return;

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();

	Root->SetStringField(TEXT("blueprint"),
		SelectedBlueprint.IsValid() ? FSoftObjectPath(SelectedBlueprint.Get()).ToString() : TEXT(""));

	TArray<TSharedPtr<FJsonValue>> DiffNames;
	if (Cfg)
	{
		for (const FString& N : Cfg->DifficultyNames) DiffNames.Add(MakeShared<FJsonValueString>(N));
	}
	Root->SetArrayField(TEXT("difficultyNames"), DiffNames);

	const int32 DiffCount = Cfg ? Cfg->DifficultyNames.Num() : 0;

	// Scaled variables for the selected blueprint
	TArray<TSharedPtr<FJsonValue>> ScaledVars;
	FBlueprintDifficultySettings* BPS = FindSettings(Cfg, SelectedBlueprint.Get());
	if (BPS)
	{
		for (const FScaledVariableData& Var : BPS->ScaledVariables)
		{
			const FString VarName = Var.VariableName.ToString();

			TSharedPtr<FJsonObject> VarObj = MakeControlDescriptor(VarName, Var.PropertyType);
			VarObj->SetStringField(TEXT("name"), VarName);
			VarObj->SetBoolField(TEXT("valid"), IsVariablePathValid(VarName));

			TArray<TSharedPtr<FJsonValue>> Values;
			for (int32 i = 0; i < DiffCount; ++i)
			{
				const FString V = Var.ScaledValuesAsString.IsValidIndex(i) ? Var.ScaledValuesAsString[i] : FString();
				Values.Add(MakeShared<FJsonValueString>(V));
			}
			VarObj->SetArrayField(TEXT("values"), Values);

			ScaledVars.Add(MakeShared<FJsonValueObject>(VarObj));
		}
	}
	Root->SetArrayField(TEXT("scaledVars"), ScaledVars);

	// Top-level available-variable tree nodes (Self + components); children load lazily.
	TArray<TSharedPtr<FJsonValue>> Tree;
	if (SelectedBlueprint.IsValid() && Cfg)
	{
		TSharedPtr<FJsonObject> SelfNode = MakeShared<FJsonObject>();
		SelfNode->SetStringField(TEXT("name"), SelectedBlueprint->GetName() + TEXT(" (Self)"));
		SelfNode->SetStringField(TEXT("path"), TEXT(""));
		SelfNode->SetBoolField(TEXT("expandable"), true);
		SelfNode->SetBoolField(TEXT("addable"), false);
		SelfNode->SetBoolField(TEXT("scaled"), false);
		Tree.Add(MakeShared<FJsonValueObject>(SelfNode));

		if (SelectedBlueprint->SimpleConstructionScript)
		{
			for (USCS_Node* Node : SelectedBlueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (!Node || Node->GetVariableName() == FName(TEXT("DefaultSceneRoot")) || !Node->ComponentTemplate) continue;

				TSharedPtr<FJsonObject> CompNode = MakeShared<FJsonObject>();
				CompNode->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
				CompNode->SetStringField(TEXT("path"), Node->GetVariableName().ToString());
				CompNode->SetBoolField(TEXT("expandable"), true);
				CompNode->SetBoolField(TEXT("addable"), false);
				CompNode->SetBoolField(TEXT("scaled"), false);
				Tree.Add(MakeShared<FJsonValueObject>(CompNode));
			}
		}
	}
	Root->SetArrayField(TEXT("tree"), Tree);

	ExecOnTool(FString::Printf(
		TEXT("if(typeof onVariableData==='function')onVariableData(%s)"), *SerializeObj(Root)));
}

void SUltimateDifficultyScalingWindow::PushToast(const FString& Message, const FString& Kind)
{
	// Native editor notification (visible regardless of which view is active).
	FNotificationInfo Info(FText::FromString(Message));
	Info.ExpireDuration = Kind == TEXT("error") ? 6.0f : 3.5f;
	Info.bFireAndForget = true;
	if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
	{
		Item->SetCompletionState(Kind == TEXT("error") ? SNotificationItem::CS_Fail : SNotificationItem::CS_Success);
	}

	// Also notify the page (in-panel toast).
	if (ToolBrowser.IsValid())
	{
		ExecOnTool(FString::Printf(TEXT("if(typeof onToast==='function')onToast('%s','%s')"),
			*EscapeForJsString(Message), *Kind));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// AI suggest / apply
// ─────────────────────────────────────────────────────────────────────────────

void SUltimateDifficultyScalingWindow::BuildAiVariableSpecs(bool bAllowAdd, TArray<TSharedPtr<FJsonObject>>& OutSpecs) const
{
	OutSpecs.Reset();
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	if (!Cfg || !SelectedBlueprint.IsValid()) return;

	FBlueprintDifficultySettings* BPS = FindSettings(Cfg, SelectedBlueprint.Get());
	const int32 DiffCount = Cfg->DifficultyNames.Num();

	TSet<FString> Scaled;
	if (BPS)
	{
		for (const FScaledVariableData& Var : BPS->ScaledVariables)
		{
			const FString Name = Var.VariableName.ToString();
			Scaled.Add(Name);

			TSharedPtr<FJsonObject> Spec = MakeControlDescriptor(Name, Var.PropertyType);
			Spec->SetStringField(TEXT("path"), Name);
			Spec->SetStringField(TEXT("display"), Name);
			Spec->SetBoolField(TEXT("isNew"), false);
			TArray<TSharedPtr<FJsonValue>> Cur;
			for (int32 i = 0; i < DiffCount; ++i)
			{
				Cur.Add(MakeShared<FJsonValueString>(Var.ScaledValuesAsString.IsValidIndex(i) ? Var.ScaledValuesAsString[i] : FString()));
			}
			Spec->SetArrayField(TEXT("current"), Cur);
			OutSpecs.Add(Spec);
		}
	}

	if (!bAllowAdd) return;

	// Gather addable leaf candidates: the Blueprint's own variables + each component's own variables.
	int32 Budget = 50;
	auto AddLeaves = [&](UStruct* Struct, const FString& Prefix)
	{
		if (!Struct) return;
		for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::ExcludeSuper); It && Budget > 0; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;

			bool bContainer = false;
			if (CastField<FStructProperty>(Prop)) bContainer = true;
			else if (FObjectProperty* OP = CastField<FObjectProperty>(Prop))
			{
				if (OP->PropertyClass->IsChildOf(UActorComponent::StaticClass())) bContainer = true;
			}
			if (bContainer) continue; // only leaves are addable

			const FString Path = Prefix.IsEmpty() ? Prop->GetName() : FString::Printf(TEXT("%s.%s"), *Prefix, *Prop->GetName());
			if (Scaled.Contains(Path)) continue;

			TSharedPtr<FJsonObject> Spec = MakeControlDescriptor(Path, Prop->GetClass()->GetFName());
			Spec->SetStringField(TEXT("path"), Path);
			Spec->SetStringField(TEXT("display"), Prop->GetDisplayNameText().ToString());
			Spec->SetBoolField(TEXT("isNew"), true);
			Spec->SetArrayField(TEXT("current"), TArray<TSharedPtr<FJsonValue>>());
			OutSpecs.Add(Spec);
			--Budget;
		}
	};

	AddLeaves(SelectedBlueprint->GeneratedClass, TEXT(""));
	if (SelectedBlueprint->SimpleConstructionScript)
	{
		for (USCS_Node* Node : SelectedBlueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (Budget <= 0) break;
			if (!Node || Node->GetVariableName() == FName(TEXT("DefaultSceneRoot")) || !Node->ComponentClass) continue;
			AddLeaves(Node->ComponentClass, Node->GetVariableName().ToString());
		}
	}
}

void SUltimateDifficultyScalingWindow::AiSuggestInternal(const FString& Intent, bool bAllowAdd)
{
	if (!ToolBrowser.IsValid()) return;

	auto PushErr = [&](const FString& Msg, bool bNeedsKey)
	{
		ExecOnTool(FString::Printf(TEXT("if(typeof onAiError==='function')onAiError('%s',%s)"),
			*EscapeForJsString(Msg), bNeedsKey ? TEXT("true") : TEXT("false")));
	};

	if (!FUDS_ApiKeyManager::Get().HasAnyConfiguredSlot())
	{
		PushErr(TEXT("No API key set up yet. Open Settings -> AI and add your provider key."), true);
		return;
	}

	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	if (!Cfg || !SelectedBlueprint.IsValid()) { PushErr(TEXT("Select a Blueprint first."), false); return; }
	if (Cfg->DifficultyNames.Num() == 0)      { PushErr(TEXT("Add at least one difficulty first."), false); return; }

	TArray<TSharedPtr<FJsonObject>> Specs;
	BuildAiVariableSpecs(bAllowAdd, Specs);

	int32 NumToSet = 0;
	for (const TSharedPtr<FJsonObject>& Spec : Specs)
	{
		bool bNew = false; Spec->TryGetBoolField(TEXT("isNew"), bNew);
		if (!bNew) ++NumToSet;
	}
	if (!bAllowAdd && NumToSet == 0)
	{
		PushErr(TEXT("Add some variables to the table first so AI can fill them - or use 'Suggest what to scale'."), false);
		return;
	}
	if (Specs.Num() == 0) { PushErr(TEXT("No editable variables found on this Blueprint."), false); return; }

	const TArray<FString> DiffNames = Cfg->DifficultyNames;
	const FString System = AI_SYSTEM_PROMPT;
	const FString User   = BuildUserPrompt(Intent, DiffNames, Specs, bAllowAdd);

	ExecOnTool(TEXT("if(typeof onAiThinking==='function')onAiThinking(true)"));

	TWeakPtr<SUltimateDifficultyScalingWindow> WeakSelf(SharedThis(this));
	FUDS_AIClient::Send(System, User, [WeakSelf, Specs, DiffNames](bool bOk, const FString& Reply, const FString& Error)
	{
		TSharedPtr<SUltimateDifficultyScalingWindow> Self = WeakSelf.Pin();
		if (!Self.IsValid()) return;

		Self->ExecOnTool(TEXT("if(typeof onAiThinking==='function')onAiThinking(false)"));

		auto Err = [&](const FString& Msg)
		{
			Self->ExecOnTool(FString::Printf(TEXT("if(typeof onAiError==='function')onAiError('%s',false)"), *EscapeForJsString(Msg)));
		};

		if (!bOk) { Err(Error.IsEmpty() ? TEXT("The AI request failed.") : Error); return; }

		const FString JsonStr = ExtractJsonObjectStr(Reply);
		TSharedPtr<FJsonObject> AiObj;
		TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(JsonStr);
		if (!FJsonSerializer::Deserialize(R, AiObj) || !AiObj.IsValid())
		{
			Err(TEXT("The AI didn't return valid JSON. Try again, or rephrase your request."));
			return;
		}

		TSharedRef<FJsonObject> Payload = BuildSuggestionRows(Specs, DiffNames, AiObj);
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (!Payload->TryGetArrayField(TEXT("rows"), Rows) || Rows->Num() == 0)
		{
			Err(TEXT("The AI didn't return values for any known variable. Try rephrasing."));
			return;
		}

		FString Out;
		TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Payload, W);
		Self->ExecOnTool(FString::Printf(TEXT("if(typeof onAiSuggestion==='function')onAiSuggestion(%s)"), *Out));
	});
}

void SUltimateDifficultyScalingWindow::ApplyAiSuggestionInternal(const FString& SuggestionJson)
{
	UDifficultyScalingConfig* Cfg = ConfigAsset.Get();
	FBlueprintDifficultySettings* BPS = FindSettings(Cfg, SelectedBlueprint.Get());
	if (!Cfg || !BPS) { PushToast(TEXT("Select a Blueprint first."), TEXT("error")); return; }

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(SuggestionJson);
	if (!FJsonSerializer::Deserialize(R, Root) || !Root.IsValid()) return;

	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!Root->TryGetArrayField(TEXT("rows"), Rows)) return;

	const int32 DiffCount = Cfg->DifficultyNames.Num();
	int32 Applied = 0, Added = 0;

	for (const TSharedPtr<FJsonValue>& RowVal : *Rows)
	{
		const TSharedPtr<FJsonObject> Row = RowVal->AsObject();
		if (!Row.IsValid()) continue;
		FString Name;
		if (!Row->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) continue;

		const TArray<TSharedPtr<FJsonValue>>* Vals = nullptr;
		Row->TryGetArrayField(TEXT("values"), Vals);

		// Add the variable if it isn't scaled yet (scan / auto-pick mode).
		FScaledVariableData* Var = BPS->ScaledVariables.FindByPredicate(
			[&](const FScaledVariableData& V) { return V.VariableName.ToString() == Name; });
		if (!Var)
		{
			FProperty* Prop = ResolvePropertyByPath(Name);
			if (!Prop) continue; // can't resolve → skip
			FScaledVariableData NewVar;
			NewVar.VariableName = FName(*Name);
			NewVar.PropertyType = Prop->GetClass()->GetFName();
			const FString Def = GetSmartDefaultForProperty(Prop);
			for (int32 i = 0; i < DiffCount; ++i) NewVar.ScaledValuesAsString.Add(Def);
			BPS->ScaledVariables.Add(NewVar);
			Var = &BPS->ScaledVariables.Last();
			++Added;
		}

		TSharedPtr<FJsonObject> Spec = MakeControlDescriptor(Name, Var->PropertyType);
		for (int32 i = 0; i < DiffCount; ++i)
		{
			while (!Var->ScaledValuesAsString.IsValidIndex(i)) Var->ScaledValuesAsString.Add(FString());
			const FString Fallback = Var->ScaledValuesAsString[i];
			const FString Raw = (Vals && Vals->IsValidIndex(i)) ? JsonValToStr((*Vals)[i]) : Fallback;
			Var->ScaledValuesAsString[i] = CoerceValue(Spec, Raw, Fallback);
		}
		++Applied;
	}

	if (Applied > 0)
	{
		Cfg->MarkPackageDirty();
		PushVariableData();
		const FString Suffix = Added > 0 ? FString::Printf(TEXT(" (%d new)"), Added) : FString();
		PushToast(FString::Printf(TEXT("Applied AI values to %d variable(s)%s."), Applied, *Suffix), TEXT("success"));
	}
	else
	{
		PushToast(TEXT("Nothing to apply."), TEXT("info"));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Settings/info view switching
// ─────────────────────────────────────────────────────────────────────────────

void SUltimateDifficultyScalingWindow::OpenSettingsView()
{
	if (MainSwitcher.IsValid()) MainSwitcher->SetActiveWidgetIndex(1);

	// Lazy-load on first show; bind the bridge right before navigation (Mac reliability).
	if (!bSettingsBrowserLoadKicked && SettingsBrowser.IsValid() && GEditor)
	{
		bSettingsBrowserLoadKicked = true;
		FTimerHandle T;
		GEditor->GetTimerManager()->SetTimer(T, [WeakSelf = TWeakPtr<SUltimateDifficultyScalingWindow>(SharedThis(this))]()
		{
			TSharedPtr<SUltimateDifficultyScalingWindow> Self = WeakSelf.Pin();
			if (!Self.IsValid() || !Self->SettingsBrowser.IsValid()) return;
			if (Self->SettingsBridge)
			{
				Self->SettingsBrowser->BindUObject(TEXT("bridge"), Self->SettingsBridge);
			}
			Self->SettingsBrowser->LoadURL(UDifficultySettingsBridge::BuildSettingsHtmlDataUri());
		}, 0.2f, false);
	}
}

void SUltimateDifficultyScalingWindow::OpenSettingsViewToTab(const FString& TabName)
{
	// Remembered so OnSettingsBrowserLoaded can re-fire it on the first (lazy) load,
	// when the page isn't ready yet for the immediate call below.
	PendingSettingsTab = TabName;
	OpenSettingsView();
	if (SettingsBrowser.IsValid())
	{
		SettingsBrowser->ExecuteJavascript(FString::Printf(
			TEXT("if(typeof focusTab==='function')focusTab(\"%s\")"), *TabName));
	}
}

void SUltimateDifficultyScalingWindow::CloseSettingsView()
{
	if (MainSwitcher.IsValid()) MainSwitcher->SetActiveWidgetIndex(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Drag & drop (best-effort — native picker is the reliable add path)
// ─────────────────────────────────────────────────────────────────────────────

FReply SUltimateDifficultyScalingWindow::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FAssetDragDropOp> Op = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (Op.IsValid())
	{
		for (const FAssetData& AssetData : Op->GetAssets())
		{
			if (AssetData.GetClass() && AssetData.GetClass()->IsChildOf(UBlueprint::StaticClass()))
			{
				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}

FReply SUltimateDifficultyScalingWindow::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FAssetDragDropOp> Op = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (Op.IsValid() && ConfigAsset.IsValid())
	{
		int32 Added = 0;
		for (const FAssetData& AssetData : Op->GetAssets())
		{
			if (UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset()))
			{
				if (AddBlueprintToConfig(BP)) ++Added;
			}
		}
		if (Added > 0)
		{
			ConfigAsset->MarkPackageDirty();
			PushBlueprintList();
			PushToast(FString::Printf(TEXT("Added %d Blueprint(s)."), Added), TEXT("success"));
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
