#include "WeaponAlignmentToolkit.h"
#include "WeaponAlignmentDataAsset.h"
#include "WeaponAlignmentStudioLibrary.h"
#include "SWeaponAlignmentViewport.h"

// Slate
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Styling/AppStyle.h"
#include "HAL/PlatformApplicationMisc.h"

// Asset picker
#include "PropertyCustomizationHelpers.h"

// Mesh types
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

// Dialog
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "WeaponAlignmentStudio"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace WAS
{
	static FText FormatTransform(const FTransform& T)
	{
		const FVector L = T.GetLocation();
		const FRotator R = T.GetRotation().Rotator();
		return FText::Format(
			LOCTEXT("TFmt", "Loc ({0:.1f}, {1:.1f}, {2:.1f})  Rot ({3:.1f}, {4:.1f}, {5:.1f})"),
			FText::AsNumber(L.X), FText::AsNumber(L.Y), FText::AsNumber(L.Z),
			FText::AsNumber(R.Pitch), FText::AsNumber(R.Yaw), FText::AsNumber(R.Roll));
	}

	static FText FormatVector(const FVector& V)
	{
		return FText::Format(
			LOCTEXT("VFmt", "({0:.1f}, {1:.1f}, {2:.1f})"),
			FText::AsNumber(V.X), FText::AsNumber(V.Y), FText::AsNumber(V.Z));
	}
}

// ---------------------------------------------------------------------------
// FWeaponAlignmentToolkit
// ---------------------------------------------------------------------------

FWeaponAlignmentToolkit::FWeaponAlignmentToolkit()  = default;
FWeaponAlignmentToolkit::~FWeaponAlignmentToolkit() = default;

void FWeaponAlignmentToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost,
                                   TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);
	ToolkitWidget = BuildToolkitWidget();
}

FText FWeaponAlignmentToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Weapon Alignment Studio");
}

FString FWeaponAlignmentToolkit::GetCharacterMeshPath() const
{
	return ChosenCharacterMesh.IsValid()
		? ChosenCharacterMesh.ToSoftObjectPath().ToString()
		: FString();
}

FString FWeaponAlignmentToolkit::GetWeaponMeshPath() const
{
	return ChosenWeaponMesh.IsValid()
		? ChosenWeaponMesh.ToSoftObjectPath().ToString()
		: FString();
}

FString FWeaponAlignmentToolkit::GetLoadAssetPath() const
{
	return LoadAssetPath;
}

// ---------------------------------------------------------------------------
// UI Builders
// ---------------------------------------------------------------------------

TSharedRef<SWidget> FWeaponAlignmentToolkit::BuildToolkitWidget()
{
	SAssignNew(Viewport3D, SWeaponAlignmentViewport);

	// Refresh the offset readout labels whenever a gizmo moves in the viewport.
	Viewport3D->OnOffsetsChanged.AddSP(this, &FWeaponAlignmentToolkit::RefreshTransformReadouts);

	return SNew(SSplitter)
		.Orientation(Orient_Horizontal)

		// Left: control panel
		+ SSplitter::Slot().Value(0.33f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot().Padding(0)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 4)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title", "AFL Weapon Alignment Studio"))
					.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				]
				+ SVerticalBox::Slot().AutoHeight() [ SNew(SSeparator) ]
				+ SVerticalBox::Slot().AutoHeight() [ BuildMeshPickerSection()      ]
				+ SVerticalBox::Slot().AutoHeight() [ SNew(SSeparator)              ]
				+ SVerticalBox::Slot().AutoHeight() [ BuildLoadSection()            ]
				+ SVerticalBox::Slot().AutoHeight() [ SNew(SSeparator)              ]
				+ SVerticalBox::Slot().AutoHeight() [ BuildBonePickerSection()      ]
				+ SVerticalBox::Slot().AutoHeight() [ SNew(SSeparator)              ]
				+ SVerticalBox::Slot().AutoHeight() [ BuildTransformReadoutSection()]
				+ SVerticalBox::Slot().AutoHeight() [ SNew(SSeparator)              ]
				+ SVerticalBox::Slot().AutoHeight() [ BuildSaveSection()            ]
			]
		]

		// Right: 3-D preview
		+ SSplitter::Slot().Value(0.67f)
		[
			Viewport3D.ToSharedRef()
		];
}

// ---------------------------------------------------------------------------

TSharedRef<SWidget> FWeaponAlignmentToolkit::BuildMeshPickerSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 6, 8, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MeshSection", "Meshes"))
			.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		]

		// Character SKM
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4, 8, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CharLabel", "Character Skeletal Mesh"))
			.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 6)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(USkeletalMesh::StaticClass())
			.ObjectPath(this, &FWeaponAlignmentToolkit::GetCharacterMeshPath)
			.OnObjectChanged(FOnSetObject::CreateSP(this, &FWeaponAlignmentToolkit::OnCharacterMeshSelected))
			.AllowClear(true)
		]

		// Weapon static mesh
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4, 8, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WeaponLabel", "Weapon Static Mesh"))
			.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 6)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UStaticMesh::StaticClass())
			.ObjectPath(this, &FWeaponAlignmentToolkit::GetWeaponMeshPath)
			.OnObjectChanged(FOnSetObject::CreateSP(this, &FWeaponAlignmentToolkit::OnWeaponMeshSelected))
			.AllowClear(true)
		];
}

// ---------------------------------------------------------------------------

TSharedRef<SWidget> FWeaponAlignmentToolkit::BuildLoadSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 6, 8, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LoadSection", "Load Existing Alignment"))
			.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LoadHint", "Pick a saved asset to load its offsets onto the gizmos."))
			.Font(FAppStyle::GetFontStyle("SmallFont"))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 6)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UWeaponAlignmentDataAsset::StaticClass())
			.ObjectPath(this, &FWeaponAlignmentToolkit::GetLoadAssetPath)
			.OnObjectChanged(FOnSetObject::CreateSP(this, &FWeaponAlignmentToolkit::OnLoadAssetSelected))
			.AllowClear(true)
		];
}

// ---------------------------------------------------------------------------

TSharedRef<SWidget> FWeaponAlignmentToolkit::BuildBonePickerSection()
{
	auto MakeBoneRow = [&](FText Label, FName& BoneRef,
	    TFunction<void(const FText&, ETextCommit::Type)> OnCommit) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 3)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				.MinDesiredWidth(140.f)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 3, 8, 3)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([&BoneRef]() { return FText::FromName(BoneRef); })
				.OnTextCommitted(FOnTextCommitted::CreateLambda(OnCommit))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
			];
	};

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 6, 8, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BoneSection", "Bone Names"))
			.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		]

		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeBoneRow(
				LOCTEXT("LHBone", "Left Hand Grip"),
				LeftHandBoneName,
				[this](const FText& T, ETextCommit::Type C) { OnLeftHandBoneCommitted(T, C); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeBoneRow(
				LOCTEXT("RHBone", "Right Hand Grip"),
				RightHandBoneName,
				[this](const FText& T, ETextCommit::Type C) { OnRightHandBoneCommitted(T, C); })
		]

		// Snap buttons: move each hand handle onto its named bone.
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4, 8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 2, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.OnClicked(this, &FWeaponAlignmentToolkit::OnSnapLeftHandClicked)
				.ToolTipText(LOCTEXT("SnapLTip", "Move the left-hand gizmo onto the named left-hand bone."))
				[ SNew(STextBlock).Text(LOCTEXT("SnapL", "Snap L → bone"))
				  .Font(FAppStyle::GetFontStyle("SmallFont")) ]
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(2, 0, 0, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.OnClicked(this, &FWeaponAlignmentToolkit::OnSnapRightHandClicked)
				.ToolTipText(LOCTEXT("SnapRTip", "Move the right-hand gizmo onto the named right-hand bone."))
				[ SNew(STextBlock).Text(LOCTEXT("SnapR", "Snap R → bone"))
				  .Font(FAppStyle::GetFontStyle("SmallFont")) ]
			]
		]

		// Closest-bone readouts (refreshed on gizmo move).
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 2)
		[
			SAssignNew(ClosestBoneReadout, STextBlock)
			.Text(LOCTEXT("ClosestInit", "Closest bones: drag a hand gizmo to query"))
			.Font(FAppStyle::GetFontStyle("SmallFont"))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
			.AutoWrapText(true)
		];
}

// ---------------------------------------------------------------------------

TSharedRef<SWidget> FWeaponAlignmentToolkit::BuildTransformReadoutSection()
{
	auto MakeReadout = [&](FText Label,
	    TSharedPtr<STextBlock>& OutWidget) -> TSharedRef<SWidget>
	{
		SAssignNew(OutWidget, STextBlock)
			.Text(LOCTEXT("NoData", "--"))
			.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
			.AutoWrapText(true);

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 0)
			[
				SNew(STextBlock)
				.Text(Label)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
				.Font(FAppStyle::GetFontStyle("SmallFont"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4)
			[ OutWidget.ToSharedRef() ];
	};

	// Editable X/Y/Z location row. Getter reads the live value; Setter pushes it
	// back through the viewport (which repositions the handle + rebuilds gizmos).
	auto MakeXYZRow = [this](FText Label,
	    TFunction<FVector()> Get, TFunction<void(const FVector&)> Set) -> TSharedRef<SWidget>
	{
		auto Axis = [Get, Set](int32 Index) -> TSharedRef<SWidget>
		{
			return SNew(SNumericEntryBox<float>)
				.AllowSpin(true)
				.MinDesiredValueWidth(48.f)
				.Value_Lambda([Get, Index]() { return TOptional<float>((float)Get()[Index]); })
				.OnValueCommitted_Lambda([Get, Set, Index](float NewVal, ETextCommit::Type)
				{
					FVector V = Get();
					V[Index] = NewVal;
					Set(V);
				});
		};

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 1, 4, 1)
			[
				SNew(STextBlock).Text(Label)
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.MinDesiredWidth(70.f)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(1, 1) [ Axis(0) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(1, 1) [ Axis(1) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(1, 1, 8, 1) [ Axis(2) ];
	};

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 6, 8, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ReadoutSection", "Live Offsets (gizmo ↔ numeric)"))
			.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		]

		// Text readouts (full transform incl. rotation, at a glance)
		+ SVerticalBox::Slot().AutoHeight()
		[ MakeReadout(LOCTEXT("LHGrip",  "L Hand Grip — weapon-relative"), LeftHandReadout)  ]
		+ SVerticalBox::Slot().AutoHeight()
		[ MakeReadout(LOCTEXT("RHGrip",  "R Hand Grip — weapon-relative"), RightHandReadout) ]
		+ SVerticalBox::Slot().AutoHeight()
		[ MakeReadout(LOCTEXT("LElbow",  "L Elbow Pole — world"),           LeftElbowReadout) ]
		+ SVerticalBox::Slot().AutoHeight()
		[ MakeReadout(LOCTEXT("RElbow",  "R Elbow Pole — world"),           RightElbowReadout)]

		// Editable location grids (X Y Z) — type exact positions
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 6, 8, 1)
		[
			SNew(STextBlock).Text(LOCTEXT("NumericHdr", "Edit Locations (cm)"))
			.Font(FAppStyle::GetFontStyle("SmallFont"))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeXYZRow(LOCTEXT("LHandLoc", "L Hand"),
				[this]() { return Viewport3D.IsValid() ? Viewport3D->GetLiveLeftHandTransform().GetLocation() : FVector::ZeroVector; },
				[this](const FVector& V)
				{
					if (!Viewport3D.IsValid()) return;
					FTransform T = Viewport3D->GetLiveLeftHandTransform(); T.SetLocation(V);
					Viewport3D->SetHandGripOffset(SWeaponAlignmentViewport::EHandle::LeftHand, T);
				})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeXYZRow(LOCTEXT("RHandLoc", "R Hand"),
				[this]() { return Viewport3D.IsValid() ? Viewport3D->GetLiveRightHandTransform().GetLocation() : FVector::ZeroVector; },
				[this](const FVector& V)
				{
					if (!Viewport3D.IsValid()) return;
					FTransform T = Viewport3D->GetLiveRightHandTransform(); T.SetLocation(V);
					Viewport3D->SetHandGripOffset(SWeaponAlignmentViewport::EHandle::RightHand, T);
				})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeXYZRow(LOCTEXT("LElbowLoc", "L Elbow"),
				[this]() { return Viewport3D.IsValid() ? Viewport3D->GetLiveLeftElbowLocation() : FVector::ZeroVector; },
				[this](const FVector& V)
				{
					if (Viewport3D.IsValid())
						Viewport3D->SetElbowWorldLocation(SWeaponAlignmentViewport::EHandle::LeftElbow, V);
				})
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeXYZRow(LOCTEXT("RElbowLoc", "R Elbow"),
				[this]() { return Viewport3D.IsValid() ? Viewport3D->GetLiveRightElbowLocation() : FVector::ZeroVector; },
				[this](const FVector& V)
				{
					if (Viewport3D.IsValid())
						Viewport3D->SetElbowWorldLocation(SWeaponAlignmentViewport::EHandle::RightElbow, V);
				})
		]

		// Copy-as-JSON (for pasting live offsets to Claude / chat)
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 6, 8, 4)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.OnClicked(this, &FWeaponAlignmentToolkit::OnCopyJsonClicked)
			.ToolTipText(LOCTEXT("CopyJsonTip", "Copy the current live offsets to the clipboard as JSON."))
			[
				SNew(STextBlock).Text(LOCTEXT("CopyJsonBtn", "Copy Live Offsets as JSON"))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
			]
		];
}

// ---------------------------------------------------------------------------

TSharedRef<SWidget> FWeaponAlignmentToolkit::BuildSaveSection()
{
	auto MakeTextRow = [&](FText Label, FString& Ref,
	    TFunction<void(const FText&, ETextCommit::Type)> OnCommit,
	    FText Hint = FText()) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 3)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				.MinDesiredWidth(90.f)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 3, 8, 3)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([&Ref]() { return FText::FromString(Ref); })
				.OnTextCommitted(FOnTextCommitted::CreateLambda(OnCommit))
				.HintText(Hint)
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
			];
	};

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 6, 8, 2)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SaveSection", "Bake to Data Asset"))
			.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
		]

		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeTextRow(
				LOCTEXT("AssetName", "Asset Name"),
				AssetSaveName,
				[this](const FText& T, ETextCommit::Type C) { OnAssetNameCommitted(T, C); })
		]

		+ SVerticalBox::Slot().AutoHeight()
		[
			MakeTextRow(
				LOCTEXT("SavePath", "Content Path"),
				AssetSavePath,
				[this](const FText& T, ETextCommit::Type C) { OnSavePathCommitted(T, C); },
				LOCTEXT("PathHint", "/Game/Weapons/AlignmentData"))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 10)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ButtonColorAndOpacity(FLinearColor(0.05f, 0.35f, 0.05f))
			.OnClicked(this, &FWeaponAlignmentToolkit::OnBakeClicked)
			.ToolTipText(LOCTEXT("BakeTip",
				"Compute weapon-relative IK offsets from the current gizmo positions "
				"and save a UWeaponAlignmentDataAsset to the specified content path."))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BakeBtn", "Bake & Save Alignment Asset"))
				.Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
			]
		];
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void FWeaponAlignmentToolkit::OnCharacterMeshSelected(const FAssetData& AssetData)
{
	ChosenCharacterMesh = TSoftObjectPtr<USkeletalMesh>(AssetData.ToSoftObjectPath());
	if (Viewport3D.IsValid())
	{
		Viewport3D->SetCharacterMesh(Cast<USkeletalMesh>(AssetData.GetAsset()));
		Viewport3D->RebuildGizmos();
	}
}

void FWeaponAlignmentToolkit::OnWeaponMeshSelected(const FAssetData& AssetData)
{
	ChosenWeaponMesh = TSoftObjectPtr<UStaticMesh>(AssetData.ToSoftObjectPath());
	if (Viewport3D.IsValid())
	{
		Viewport3D->SetWeaponMesh(Cast<UStaticMesh>(AssetData.GetAsset()));
		Viewport3D->RebuildGizmos();
	}
}

void FWeaponAlignmentToolkit::OnLoadAssetSelected(const FAssetData& AssetData)
{
	LoadAssetPath = AssetData.IsValid() ? AssetData.ToSoftObjectPath().ToString() : FString();

	UWeaponAlignmentDataAsset* Asset = Cast<UWeaponAlignmentDataAsset>(AssetData.GetAsset());
	if (Asset && Viewport3D.IsValid())
	{
		// Pull bone names back into the UI too, then position handles.
		LeftHandBoneName  = Asset->LeftHandBoneName.IsNone()  ? LeftHandBoneName  : Asset->LeftHandBoneName;
		RightHandBoneName = Asset->RightHandBoneName.IsNone() ? RightHandBoneName : Asset->RightHandBoneName;
		Viewport3D->LoadFromAsset(Asset);
	}
}

void FWeaponAlignmentToolkit::OnLeftHandBoneCommitted(const FText& T, ETextCommit::Type)
{
	LeftHandBoneName = FName(*T.ToString());
}

void FWeaponAlignmentToolkit::OnRightHandBoneCommitted(const FText& T, ETextCommit::Type)
{
	RightHandBoneName = FName(*T.ToString());
}

void FWeaponAlignmentToolkit::OnAssetNameCommitted(const FText& T, ETextCommit::Type)
{
	AssetSaveName = T.ToString();
}

void FWeaponAlignmentToolkit::OnSavePathCommitted(const FText& T, ETextCommit::Type)
{
	AssetSavePath = T.ToString();
}

void FWeaponAlignmentToolkit::RefreshTransformReadouts()
{
	if (!Viewport3D.IsValid()) { return; }

	if (LeftHandReadout.IsValid())
		LeftHandReadout->SetText(WAS::FormatTransform(Viewport3D->GetLiveLeftHandTransform()));
	if (LeftElbowReadout.IsValid())
		LeftElbowReadout->SetText(WAS::FormatVector(Viewport3D->GetLiveLeftElbowLocation()));
	if (RightHandReadout.IsValid())
		RightHandReadout->SetText(WAS::FormatTransform(Viewport3D->GetLiveRightHandTransform()));
	if (RightElbowReadout.IsValid())
		RightElbowReadout->SetText(WAS::FormatVector(Viewport3D->GetLiveRightElbowLocation()));

	// Closest-bone diagnostics for each hand (only meaningful with a char mesh).
	if (ClosestBoneReadout.IsValid())
	{
		float LDist = 0.f, RDist = 0.f;
		const FName LBone = Viewport3D->GetClosestBoneToHandle(
			SWeaponAlignmentViewport::EHandle::LeftHand, LDist);
		const FName RBone = Viewport3D->GetClosestBoneToHandle(
			SWeaponAlignmentViewport::EHandle::RightHand, RDist);

		if (LBone.IsNone() && RBone.IsNone())
		{
			ClosestBoneReadout->SetText(
				LOCTEXT("ClosestNone", "Closest bones: (assign a character mesh)"));
		}
		else
		{
			ClosestBoneReadout->SetText(FText::Format(
				LOCTEXT("ClosestFmt", "Closest:  L→ {0} ({1} cm)   R→ {2} ({3} cm)"),
				FText::FromName(LBone), FText::AsNumber(FMath::RoundToInt(LDist)),
				FText::FromName(RBone), FText::AsNumber(FMath::RoundToInt(RDist))));
		}
	}
}

// ---------------------------------------------------------------------------
// Bake & Save
// ---------------------------------------------------------------------------

FReply FWeaponAlignmentToolkit::OnBakeClicked()
{
	if (!Viewport3D.IsValid()
	    || !Viewport3D->GetCharacterMeshComponent()
	    || !Viewport3D->GetWeaponActor())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("NoMesh", "Assign both a character and a weapon mesh before baking."));
		return FReply::Handled();
	}

	// Gather the live gizmo offsets into the shared bake-params struct, then
	// delegate to the same library function that AIK / Claude call headlessly.
	FWeaponAlignmentBakeParams Params;
	Params.LeftHandGripWeaponRelative  = Viewport3D->GetLiveLeftHandTransform();
	Params.RightHandGripWeaponRelative = Viewport3D->GetLiveRightHandTransform();
	Params.LeftElbowPoleWorld  = Viewport3D->GetLiveLeftElbowLocation();
	Params.RightElbowPoleWorld = Viewport3D->GetLiveRightElbowLocation();
	Params.LeftHandBoneName    = LeftHandBoneName;
	Params.RightHandBoneName   = RightHandBoneName;
	Params.WeaponLabel         = ChosenWeaponMesh.IsValid()
	                              ? ChosenWeaponMesh.GetAssetName()
	                              : TEXT("Unknown");

	const FString CharPath = ChosenCharacterMesh.IsValid()
		? ChosenCharacterMesh.ToSoftObjectPath().ToString() : FString();

	UWeaponAlignmentDataAsset* Created = nullptr;
	FString Error;
	const bool bOK = UWeaponAlignmentStudioLibrary::BakeAlignmentAsset(
		CharPath, Params, AssetSavePath, AssetSaveName, Created, Error);

	if (!bOK)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("BakeFail", "Bake failed:\n{0}"), FText::FromString(Error)));
		return FReply::Handled();
	}

	FMessageDialog::Open(EAppMsgType::Ok,
		FText::Format(LOCTEXT("SaveOK", "Alignment asset saved:\n{0}"),
		              FText::FromString(AssetSavePath / AssetSaveName)));

	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Copy live offsets to clipboard as JSON (for Claude / chat round-trips)
// ---------------------------------------------------------------------------

FReply FWeaponAlignmentToolkit::OnCopyJsonClicked()
{
	if (!Viewport3D.IsValid()) { return FReply::Handled(); }

	auto Xf = [](const FTransform& T) -> FString
	{
		const FVector L = T.GetLocation();
		const FRotator R = T.GetRotation().Rotator();
		return FString::Printf(
			TEXT("{\"location\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f},")
			TEXT("\"rotation\":{\"pitch\":%.3f,\"yaw\":%.3f,\"roll\":%.3f}}"),
			L.X, L.Y, L.Z, R.Pitch, R.Yaw, R.Roll);
	};
	auto V3 = [](const FVector& V) -> FString
	{
		return FString::Printf(TEXT("{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f}"), V.X, V.Y, V.Z);
	};

	const FString Json = FString::Printf(
		TEXT("{\n")
		TEXT("  \"weaponLabel\": \"%s\",\n")
		TEXT("  \"leftHandBone\": \"%s\",\n")
		TEXT("  \"rightHandBone\": \"%s\",\n")
		TEXT("  \"leftHandGripWeaponRelative\": %s,\n")
		TEXT("  \"rightHandGripWeaponRelative\": %s,\n")
		TEXT("  \"leftElbowPoleWorld\": %s,\n")
		TEXT("  \"rightElbowPoleWorld\": %s\n")
		TEXT("}"),
		ChosenWeaponMesh.IsValid() ? *ChosenWeaponMesh.GetAssetName() : TEXT("Unknown"),
		*LeftHandBoneName.ToString(),
		*RightHandBoneName.ToString(),
		*Xf(Viewport3D->GetLiveLeftHandTransform()),
		*Xf(Viewport3D->GetLiveRightHandTransform()),
		*V3(Viewport3D->GetLiveLeftElbowLocation()),
		*V3(Viewport3D->GetLiveRightElbowLocation()));

	FPlatformApplicationMisc::ClipboardCopy(*Json);

	FMessageDialog::Open(EAppMsgType::Ok,
		LOCTEXT("CopiedJson", "Live offsets copied to clipboard as JSON."));

	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Bone snapping
// ---------------------------------------------------------------------------

FReply FWeaponAlignmentToolkit::OnSnapLeftHandClicked()
{
	if (Viewport3D.IsValid())
	{
		if (!Viewport3D->SnapHandToBone(
				SWeaponAlignmentViewport::EHandle::LeftHand, LeftHandBoneName))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("SnapLFail", "Could not snap: bone '{0}' not found on the character skeleton."),
				FText::FromName(LeftHandBoneName)));
		}
	}
	return FReply::Handled();
}

FReply FWeaponAlignmentToolkit::OnSnapRightHandClicked()
{
	if (Viewport3D.IsValid())
	{
		if (!Viewport3D->SnapHandToBone(
				SWeaponAlignmentViewport::EHandle::RightHand, RightHandBoneName))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("SnapRFail", "Could not snap: bone '{0}' not found on the character skeleton."),
				FText::FromName(RightHandBoneName)));
		}
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
