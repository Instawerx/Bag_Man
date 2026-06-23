#pragma once

#include "CoreMinimal.h"
#include "Tools/UEdMode.h"
#include "WeaponAlignmentStudioMode.generated.h"

/**
 * UEdMode whose only job is to host the Weapon Alignment Studio toolkit (the
 * dockable Slate panel). All gizmo / preview logic lives in the panel's
 * SWeaponAlignmentViewport, which owns its own preview-scene mode manager.
 */
UCLASS()
class WEAPONALIGNMENTSTUDIO_API UWeaponAlignmentStudioMode : public UEdMode
{
	GENERATED_BODY()

public:
	static const FEditorModeID ModeID;

	UWeaponAlignmentStudioMode();

	//~ UEdMode
	virtual void CreateToolkit() override;
	virtual void Exit() override;
	virtual bool UsesToolkits() const override { return true; }
};
