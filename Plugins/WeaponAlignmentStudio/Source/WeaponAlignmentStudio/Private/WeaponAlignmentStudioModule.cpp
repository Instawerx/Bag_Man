#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "WeaponAlignmentStudioMode.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "ToolMenus.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "WeaponAlignmentStudio"

class FWeaponAlignmentStudioStyle
{
public:
	static void Register()
	{
		StyleInstance = MakeShareable(new FSlateStyleSet("WeaponAlignmentStudioStyle"));

		const FString PluginDir = IPluginManager::Get()
			.FindPlugin(TEXT("WeaponAlignmentStudio"))->GetBaseDir();
		StyleInstance->SetContentRoot(PluginDir / TEXT("Resources"));

		// Icon is optional — a missing PNG is silently ignored at runtime
		const FVector2D Icon40(40.f, 40.f);
		StyleInstance->Set(
			"WeaponAlignmentStudio.OpenStudio",
			new FSlateImageBrush(
				StyleInstance->RootToContentDir(TEXT("WeaponAlignmentStudio_40x.png")), Icon40));

		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}

	static void Unregister()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		StyleInstance.Reset();
	}

	static TSharedPtr<FSlateStyleSet> StyleInstance;
};

TSharedPtr<FSlateStyleSet> FWeaponAlignmentStudioStyle::StyleInstance;

// ---------------------------------------------------------------------------

class FWeaponAlignmentStudioModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FWeaponAlignmentStudioStyle::Register();

		// UEdMode subclasses self-register via UAssetEditorSubsystem::RegisterEditorModes()
		// which discovers all UEdMode CDOs at editor startup — no FEditorModeRegistry call needed.

		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
			{
				UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
				FToolMenuSection& Section = Menu->FindOrAddSection("AFL");
				Section.AddMenuEntry(
					"WeaponAlignmentStudio",
					LOCTEXT("MenuLabel", "Weapon Alignment Studio"),
					LOCTEXT("MenuTooltip", "Open the AFL Weapon IK Alignment Studio"),
					FSlateIcon("WeaponAlignmentStudioStyle", "WeaponAlignmentStudio.OpenStudio"),
					FUIAction(FExecuteAction::CreateLambda([]()
					{
						GLevelEditorModeTools().ActivateMode(UWeaponAlignmentStudioMode::ModeID);
					}))
				);
			})
		);
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		FWeaponAlignmentStudioStyle::Unregister();
	}
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWeaponAlignmentStudioModule, WeaponAlignmentStudio)
