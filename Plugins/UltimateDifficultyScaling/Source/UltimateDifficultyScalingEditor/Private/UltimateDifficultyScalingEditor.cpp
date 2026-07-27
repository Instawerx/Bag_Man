// Copyright 2025, BlueprintsLab, All rights reserved

#include "UltimateDifficultyScalingEditor.h"
#include "SUltimateDifficultyScalingWindow.h" // Include our new window widget
#include "Widgets/Docking/SDockTab.h"
#include "UltimateDifficultyScalingStyle.h"
#include "UltimateDifficultyScalingCommands.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"

// Needed for Project Settings
#include "ISettingsModule.h"
#include "DifficultyScalingSettings.h"

// CEF / web browser — initialized up front so the panel's SWebBrowser is ready.
#include "WebBrowserModule.h"


static const FName UltimateDifficultyScalingTabName("UltimateDifficultyScaling");

#define LOCTEXT_NAMESPACE "FUltimateDifficultyScalingModule"

// <-- FIX: Moved this function above StartupModule so it's declared before use.
TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		[
			SNew(SUltimateDifficultyScalingWindow)
		];
}


void FUltimateDifficultyScalingModule::StartupModule()
{
	// CEF initializes asynchronously; touch the module now so the browser backend
	// is spun up before the panel tries to host an SWebBrowser.
	IWebBrowserModule::Get();

	FUltimateDifficultyScalingStyle::Initialize();
	FUltimateDifficultyScalingStyle::ReloadTextures();

	FUltimateDifficultyScalingCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FUltimateDifficultyScalingCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FUltimateDifficultyScalingModule::PluginButtonClicked),
		FCanExecuteAction());

	// Register the tab spawner for our window
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(UltimateDifficultyScalingTabName, FOnSpawnTab::CreateStatic(&OnSpawnPluginTab)) // <-- FIX: Changed NewStatic to CreateStatic
		.SetDisplayName(FText::FromString("Ultimate Difficulty Scaling"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUltimateDifficultyScalingModule::RegisterMenus));

	// Register our custom settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "Ultimate Difficulty Scaling",
			FText::FromString("Ultimate Difficulty Scaling"),
			FText::FromString("Configure the settings for the difficulty scaling plugin."),
			GetMutableDefault<UDifficultyScalingSettings>()
		);
	}
}

void FUltimateDifficultyScalingModule::ShutdownModule()
{
	// Unregister custom settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Ultimate Difficulty Scaling");
	}

	// Unregister tab spawner
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UltimateDifficultyScalingTabName);

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FUltimateDifficultyScalingStyle::Shutdown();
	FUltimateDifficultyScalingCommands::Unregister();
}

void FUltimateDifficultyScalingModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(UltimateDifficultyScalingTabName);
}

void FUltimateDifficultyScalingModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FUltimateDifficultyScalingCommands::Get().PluginAction, PluginCommands);
		}
	}
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FUltimateDifficultyScalingCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUltimateDifficultyScalingModule, UltimateDifficultyScalingEditor)