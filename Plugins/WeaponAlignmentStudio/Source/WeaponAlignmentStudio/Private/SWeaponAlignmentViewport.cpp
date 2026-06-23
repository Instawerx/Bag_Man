#include "SWeaponAlignmentViewport.h"
#include "WeaponAlignmentDataAsset.h"
#include "WeaponAlignmentStudioLibrary.h"
#include "AdvancedPreviewScene.h"
#include "AssetEditorModeManager.h"
#include "EditorModeManager.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "InteractiveToolsContext.h"
#include "InteractiveGizmoManager.h"
#include "BaseGizmos/TransformProxy.h"
#include "BaseGizmos/CombinedTransformGizmo.h"
#include "ToolContextInterfaces.h"  // EToolContextTransformGizmoMode
#include "EditorViewportClient.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "WeaponAlignmentStudio"

// Owner key for gizmos created by this viewport (passed to Create3AxisTransformGizmo /
// DestroyAllGizmosByOwner). Address is unique and stable per viewport instance.
static const void* GizmoOwnerKey(const SWeaponAlignmentViewport* V) { return V; }

// ---------------------------------------------------------------------------
// Viewport client — orbit camera, no grid. Input/render/tick all forward into
// the ModeManager (FEditorModeTools) automatically via the base class, which is
// what makes the ITF gizmos live in THIS viewport.
// ---------------------------------------------------------------------------

class FWeaponAlignmentViewportClient : public FEditorViewportClient
{
public:
	FWeaponAlignmentViewportClient(
		FEditorModeTools* InModeTools,
		FAdvancedPreviewScene* InPreviewScene,
		const TSharedRef<SEditorViewport>& InViewport)
		: FEditorViewportClient(InModeTools, InPreviewScene, InViewport)
	{
		SetViewModes(VMI_Lit, VMI_Lit);
		SetViewportType(LVT_Perspective);
		bSetListenerPosition = false;
		EngineShowFlags.SetDynamicShadows(true);
		EngineShowFlags.SetGrid(false);

		SetInitialViewTransform(
			LVT_Perspective,
			FVector(-220.f, 40.f, 130.f),
			FRotator(-12.f, 18.f, 0.f),
			/*OrthoZoom=*/1000.f);
	}
};

// ---------------------------------------------------------------------------
// SWeaponAlignmentViewport
// ---------------------------------------------------------------------------

void SWeaponAlignmentViewport::Construct(const FArguments& InArgs)
{
	PreviewScene = MakeShareable(new FAdvancedPreviewScene(
		FAdvancedPreviewScene::ConstructionValues().SetEditor(true)));

	// Mode manager bound to our preview scene. This single object provides the
	// ITF context (input router, tool/gizmo managers, queries + transactions API).
	ModeManager = MakeShared<FAssetEditorModeManager>();
	ModeManager->SetPreviewScene(PreviewScene.Get());

	// Build the viewport widget — calls MakeEditorViewportClient() below.
	SEditorViewport::Construct(SEditorViewport::FArguments());

	SpawnPreviewActors();
}

SWeaponAlignmentViewport::~SWeaponAlignmentViewport()
{
	DestroyGizmos();

	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
	}
	ModeManager.Reset();
}

TSharedRef<FEditorViewportClient> SWeaponAlignmentViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FWeaponAlignmentViewportClient(
		ModeManager.Get(), PreviewScene.Get(), SharedThis(this)));
	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SWeaponAlignmentViewport::MakeViewportToolbar()
{
	return SNullWidget::NullWidget;
}

FAdvancedPreviewScene* SWeaponAlignmentViewport::GetPreviewScene() const
{
	return PreviewScene.Get();
}

// ---------------------------------------------------------------------------

void SWeaponAlignmentViewport::SpawnPreviewActors()
{
	UWorld* World = PreviewScene->GetWorld();
	if (!ensure(World)) { return; }

	FActorSpawnParameters Params;
	Params.ObjectFlags = RF_Transient;

	// Character
	AActor* CharActor = World->SpawnActor<AActor>(Params);
	CharacterMeshComp = NewObject<USkeletalMeshComponent>(CharActor, TEXT("CharMesh"));
	CharActor->SetRootComponent(CharacterMeshComp);
	CharacterMeshComp->RegisterComponent();

	// Weapon (offset so it doesn't overlap the character)
	WeaponActor    = World->SpawnActor<AActor>(Params);
	WeaponMeshComp = NewObject<UStaticMeshComponent>(WeaponActor, TEXT("WeaponMesh"));
	WeaponActor->SetRootComponent(WeaponMeshComp);
	WeaponMeshComp->RegisterComponent();
	WeaponActor->SetActorLocation(FVector(60.f, 15.f, 110.f));

	// Four draggable handle components. Hand handles parent under the weapon
	// (so their transform is naturally weapon-relative); elbow handles parent
	// under the character root (world-ish, refined to shoulder space at bake).
	auto MakeHandle = [&](AActor* Owner, const TCHAR* Name, FVector LocalLoc) -> USceneComponent*
	{
		USceneComponent* H = NewObject<USceneComponent>(Owner, Name);
		H->SetupAttachment(Owner->GetRootComponent());
		H->RegisterComponent();
		H->SetRelativeLocation(LocalLoc);
		return H;
	};

	LeftHandHandle   = MakeHandle(WeaponActor, TEXT("LeftHandHandle"),   FVector(0.f,  -8.f, 0.f));
	RightHandHandle  = MakeHandle(WeaponActor, TEXT("RightHandHandle"),  FVector(0.f,   8.f, 0.f));
	LeftElbowHandle  = MakeHandle(CharActor,   TEXT("LeftElbowHandle"),  FVector(-15.f, -25.f, 90.f));
	RightElbowHandle = MakeHandle(CharActor,   TEXT("RightElbowHandle"), FVector(-15.f,  25.f, 90.f));
}

// ---------------------------------------------------------------------------
// Gizmos
// ---------------------------------------------------------------------------

void SWeaponAlignmentViewport::RebuildGizmos()
{
	DestroyGizmos();

	if (!ModeManager.IsValid()) { return; }
	UInteractiveGizmoManager* GM = ModeManager->GetInteractiveToolsContext()->GizmoManager;
	if (!GM) { return; }

	// Helper: spawn a proxy bound to a handle, a gizmo driving the proxy, and a
	// transform-changed lambda that recomputes the corresponding live offset.
	auto Wire = [&](USceneComponent* Handle, UTransformProxy*& OutProxy,
	                TFunction<void(const FTransform&)> OnChanged)
	{
		if (!Handle) { return; }

		OutProxy = NewObject<UTransformProxy>(GetTransientPackage());
		OutProxy->AddComponent(Handle);

		UCombinedTransformGizmo* Gizmo = GM->Create3AxisTransformGizmo(
			const_cast<void*>(GizmoOwnerKey(this)));
		if (!Gizmo)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("WeaponAlignmentStudio: gizmo creation returned null."));
			return;
		}

		// CRITICAL: our standalone FAssetEditorModeManager has WidgetMode == WM_None
		// (no EdMode drives it). With bUseContextGizmoMode left at its default true,
		// the gizmo reads NoGizmo from the context on its first Tick (which fires once
		// the viewport gains focus on the first click) and hides every handle forever.
		// Opt out of context-driven mode and pin to Combined — same approach the
		// engine's Chaos Cloth editor preview viewport uses.
		Gizmo->bUseContextGizmoMode       = false;
		Gizmo->bUseContextCoordinateSystem = false;
		Gizmo->ActiveGizmoMode            = EToolContextTransformGizmoMode::Combined;

		Gizmo->SetActiveTarget(OutProxy);

		OutProxy->OnTransformChanged.AddLambda(
			[this, OnChanged](UTransformProxy*, FTransform NewWorld)
			{
				OnChanged(NewWorld);
				OnOffsetsChanged.Broadcast();
			});
	};

	const FTransform WeaponXf = WeaponActor
		? WeaponActor->GetActorTransform() : FTransform::Identity;

	Wire(LeftHandHandle, LeftHandProxy, [this, WeaponXf](const FTransform& W)
	{
		LiveLeftHandTransform = W.GetRelativeTransform(WeaponXf);
	});
	Wire(RightHandHandle, RightHandProxy, [this, WeaponXf](const FTransform& W)
	{
		LiveRightHandTransform = W.GetRelativeTransform(WeaponXf);
	});
	Wire(LeftElbowHandle, LeftElbowProxy, [this](const FTransform& W)
	{
		LiveLeftElbowLocation = W.GetLocation();
	});
	Wire(RightElbowHandle, RightElbowProxy, [this](const FTransform& W)
	{
		LiveRightElbowLocation = W.GetLocation();
	});

	// Seed the live values from the handles' current positions so the readout
	// isn't "--" before the user touches anything.
	if (LeftHandHandle)
		LiveLeftHandTransform  = LeftHandHandle->GetComponentTransform().GetRelativeTransform(WeaponXf);
	if (RightHandHandle)
		LiveRightHandTransform = RightHandHandle->GetComponentTransform().GetRelativeTransform(WeaponXf);
	if (LeftElbowHandle)
		LiveLeftElbowLocation  = LeftElbowHandle->GetComponentLocation();
	if (RightElbowHandle)
		LiveRightElbowLocation = RightElbowHandle->GetComponentLocation();

	OnOffsetsChanged.Broadcast();
}

void SWeaponAlignmentViewport::DestroyGizmos()
{
	if (ModeManager.IsValid())
	{
		if (UInteractiveGizmoManager* GM = ModeManager->GetInteractiveToolsContext()->GizmoManager)
		{
			GM->DestroyAllGizmosByOwner(const_cast<void*>(GizmoOwnerKey(this)));
		}
	}
	LeftHandProxy = RightHandProxy = LeftElbowProxy = RightElbowProxy = nullptr;
}

// ---------------------------------------------------------------------------
// Load a previously-baked asset back onto the handles
// ---------------------------------------------------------------------------

void SWeaponAlignmentViewport::LoadFromAsset(UWeaponAlignmentDataAsset* Asset)
{
	if (!Asset) { return; }

	// Hand handles are parented under the weapon actor, so their RELATIVE transform
	// is exactly the stored weapon-relative grip offset.
	if (LeftHandHandle)
		LeftHandHandle->SetRelativeTransform(Asset->LeftHandGripOffset);
	if (RightHandHandle)
		RightHandHandle->SetRelativeTransform(Asset->RightHandGripOffset);

	// Elbow handles live in character/component space; stored values are
	// shoulder-local, so convert back via the ref skeleton (needs a char mesh).
	USkeletalMesh* CharMesh = CharacterMeshComp ? CharacterMeshComp->GetSkeletalMeshAsset() : nullptr;
	if (CharMesh)
	{
		if (LeftElbowHandle)
		{
			const FVector W = UWeaponAlignmentStudioLibrary::ShoulderLocalToWorldElbow(
				CharMesh, Asset->LeftElbowPoleOffset, FName("lowerarm_l"));
			LeftElbowHandle->SetWorldLocation(W);
		}
		if (RightElbowHandle)
		{
			const FVector W = UWeaponAlignmentStudioLibrary::ShoulderLocalToWorldElbow(
				CharMesh, Asset->RightElbowPoleOffset, FName("lowerarm_r"));
			RightElbowHandle->SetWorldLocation(W);
		}
	}

	// Recreate gizmos on the repositioned handles and refresh the readout.
	RebuildGizmos();
}

// ---------------------------------------------------------------------------
// Numeric setters (two-way with the GUI fields)
// ---------------------------------------------------------------------------

void SWeaponAlignmentViewport::SetHandGripOffset(EHandle Which, const FTransform& WeaponRelative)
{
	// Hand handles are parented under the weapon, so relative == weapon-relative.
	USceneComponent* H = (Which == EHandle::LeftHand) ? LeftHandHandle : RightHandHandle;
	if (H)
	{
		H->SetRelativeTransform(WeaponRelative);
		RebuildGizmos();
	}
}

void SWeaponAlignmentViewport::SetElbowWorldLocation(EHandle Which, const FVector& WorldLoc)
{
	USceneComponent* H = (Which == EHandle::LeftElbow) ? LeftElbowHandle : RightElbowHandle;
	if (H)
	{
		H->SetWorldLocation(WorldLoc);
		RebuildGizmos();
	}
}

// ---------------------------------------------------------------------------
// Bone snapping + closest-bone query
// ---------------------------------------------------------------------------

namespace
{
	// Component-space position of a named bone in the ref pose.
	bool RefBoneComponentLocation(USkeletalMesh* Mesh, FName Bone, FVector& Out)
	{
		if (!Mesh) { return false; }
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		const int32 Idx = Ref.FindBoneIndex(Bone);
		if (Idx == INDEX_NONE) { return false; }
		Out = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Idx).GetLocation();
		return true;
	}
}

USceneComponent* SWeaponAlignmentViewport_HandleFor(
	SWeaponAlignmentViewport::EHandle Which,
	USceneComponent* LH, USceneComponent* RH, USceneComponent* LE, USceneComponent* RE)
{
	switch (Which)
	{
	case SWeaponAlignmentViewport::EHandle::LeftHand:   return LH;
	case SWeaponAlignmentViewport::EHandle::RightHand:  return RH;
	case SWeaponAlignmentViewport::EHandle::LeftElbow:  return LE;
	case SWeaponAlignmentViewport::EHandle::RightElbow: return RE;
	}
	return nullptr;
}

bool SWeaponAlignmentViewport::SnapHandToBone(EHandle Which, FName BoneName)
{
	USceneComponent* H = SWeaponAlignmentViewport_HandleFor(
		Which, LeftHandHandle, RightHandHandle, LeftElbowHandle, RightElbowHandle);
	USkeletalMesh* Mesh = CharacterMeshComp ? CharacterMeshComp->GetSkeletalMeshAsset() : nullptr;
	if (!H || !Mesh) { return false; }

	FVector BoneCompLoc;
	if (!RefBoneComponentLocation(Mesh, BoneName, BoneCompLoc)) { return false; }

	// Ref-pose component space == preview component world (component at origin).
	const FVector WorldLoc = CharacterMeshComp->GetComponentTransform().TransformPosition(BoneCompLoc);
	H->SetWorldLocation(WorldLoc);

	RebuildGizmos();
	return true;
}

FName SWeaponAlignmentViewport::GetClosestBoneToHandle(EHandle Which, float& OutDistance) const
{
	OutDistance = 0.f;
	USceneComponent* H = SWeaponAlignmentViewport_HandleFor(
		Which, LeftHandHandle, RightHandHandle, LeftElbowHandle, RightElbowHandle);
	USkeletalMesh* Mesh = CharacterMeshComp ? CharacterMeshComp->GetSkeletalMeshAsset() : nullptr;
	if (!H || !Mesh) { return NAME_None; }

	const FTransform CompXf = CharacterMeshComp->GetComponentTransform();
	const FVector HandleComp = CompXf.InverseTransformPosition(H->GetComponentLocation());

	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const int32 Num = Ref.GetNum();

	FName Best = NAME_None;
	float BestSq = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Num; ++i)
	{
		const FVector BoneLoc =
			FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, i).GetLocation();
		const float DSq = FVector::DistSquared(BoneLoc, HandleComp);
		if (DSq < BestSq)
		{
			BestSq = DSq;
			Best = Ref.GetBoneName(i);
		}
	}

	OutDistance = FMath::Sqrt(BestSq);
	return Best;
}

// ---------------------------------------------------------------------------
// Mesh setters
// ---------------------------------------------------------------------------

void SWeaponAlignmentViewport::SetCharacterMesh(USkeletalMesh* NewMesh)
{
	if (CharacterMeshComp)
	{
		CharacterMeshComp->SetSkeletalMeshAsset(NewMesh);
		CharacterMeshComp->RecreateRenderState_Concurrent();
	}
}

void SWeaponAlignmentViewport::SetWeaponMesh(UStaticMesh* NewMesh)
{
	if (WeaponMeshComp)
	{
		WeaponMeshComp->SetStaticMesh(NewMesh);
		WeaponMeshComp->RecreateRenderState_Concurrent();
	}
}

void SWeaponAlignmentViewport::SetWeaponTransform(const FTransform& T)
{
	if (WeaponActor)
	{
		WeaponActor->SetActorTransform(T);
	}
}

#undef LOCTEXT_NAMESPACE
