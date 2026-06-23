#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

class SWeaponAlignmentViewport;
class USkeletalMesh;
class UStaticMesh;
struct FAssetData;
class STextBlock;

/**
 * Dockable toolkit (Slate panel) for the Weapon Alignment Studio editor mode.
 * Left side: controls (mesh pickers, bone names, readouts, save).
 * Right side: SWeaponAlignmentViewport (the 3-D preview).
 */
class WEAPONALIGNMENTSTUDIO_API FWeaponAlignmentToolkit : public FModeToolkit
{
public:
	FWeaponAlignmentToolkit();
	virtual ~FWeaponAlignmentToolkit() override;

	//~ FModeToolkit
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost,
	                  TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual FName               GetToolkitFName()    const override { return FName("WeaponAlignmentStudio"); }
	virtual FText               GetBaseToolkitName() const override;
	virtual TSharedPtr<SWidget> GetInlineContent()   const override { return ToolkitWidget; }

	/** Bound to viewport OnOffsetsChanged — refreshes the offset readout labels. */
	void RefreshTransformReadouts();

	/** Path getter for the character SKM entry box attribute binding. */
	FString GetCharacterMeshPath() const;
	/** Path getter for the weapon static mesh entry box attribute binding. */
	FString GetWeaponMeshPath() const;
	/** Path getter for the load-existing-asset entry box attribute binding. */
	FString GetLoadAssetPath() const;

private:
	// Builders
	TSharedRef<SWidget> BuildToolkitWidget();
	TSharedRef<SWidget> BuildMeshPickerSection();
	TSharedRef<SWidget> BuildLoadSection();
	TSharedRef<SWidget> BuildBonePickerSection();
	TSharedRef<SWidget> BuildTransformReadoutSection();
	TSharedRef<SWidget> BuildSaveSection();

	// Callbacks
	void     OnCharacterMeshSelected(const FAssetData& AssetData);
	void     OnWeaponMeshSelected(const FAssetData& AssetData);
	void     OnLoadAssetSelected(const FAssetData& AssetData);
	void     OnLeftHandBoneCommitted(const FText& Text, ETextCommit::Type CommitType);
	void     OnRightHandBoneCommitted(const FText& Text, ETextCommit::Type CommitType);
	void     OnAssetNameCommitted(const FText& Text, ETextCommit::Type CommitType);
	void     OnSavePathCommitted(const FText& Text, ETextCommit::Type CommitType);
	FReply   OnBakeClicked();
	FReply   OnCopyJsonClicked();
	FReply   OnSnapLeftHandClicked();
	FReply   OnSnapRightHandClicked();

	// Widgets
	TSharedPtr<SWidget>                  ToolkitWidget;
	TSharedPtr<SWeaponAlignmentViewport> Viewport3D;
	TSharedPtr<STextBlock>               LeftHandReadout;
	TSharedPtr<STextBlock>               LeftElbowReadout;
	TSharedPtr<STextBlock>               RightHandReadout;
	TSharedPtr<STextBlock>               RightElbowReadout;
	TSharedPtr<STextBlock>               ClosestBoneReadout;

	// State
	TSoftObjectPtr<USkeletalMesh> ChosenCharacterMesh;
	TSoftObjectPtr<UStaticMesh>   ChosenWeaponMesh;
	FString LoadAssetPath;
	FName   LeftHandBoneName  = FName("hand_l");
	FName   RightHandBoneName = FName("hand_r");
	FString AssetSaveName     = TEXT("DA_WeaponAlignment_New");
	FString AssetSavePath     = TEXT("/Game/Weapons/AlignmentData");
};
