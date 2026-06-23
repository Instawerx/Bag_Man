#include "WeaponAlignmentStudioMode.h"
#include "WeaponAlignmentToolkit.h"
#include "Toolkits/ToolkitManager.h"

#define LOCTEXT_NAMESPACE "WeaponAlignmentStudio"

const FEditorModeID UWeaponAlignmentStudioMode::ModeID = TEXT("WeaponAlignmentStudioMode");

UWeaponAlignmentStudioMode::UWeaponAlignmentStudioMode()
{
	Info = FEditorModeInfo(
		ModeID,
		LOCTEXT("ModeName", "Weapon Alignment Studio"),
		FSlateIcon(),
		true,
		500
	);
}

void UWeaponAlignmentStudioMode::CreateToolkit()
{
	// Base Enter() calls this, then Toolkit->Init(Owner->GetToolkitHost(), this).
	Toolkit = MakeShareable(new FWeaponAlignmentToolkit());
}

void UWeaponAlignmentStudioMode::Exit()
{
	if (Toolkit.IsValid())
	{
		FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
		Toolkit.Reset();
	}

	Super::Exit();
}

#undef LOCTEXT_NAMESPACE
