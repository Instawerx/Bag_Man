// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLLoadoutDisplayPawn.h"

#include "Cosmetics/AFLSkinColorComponent.h"                 // pawn-side color holder (the fan-out target)
#include "Cosmetics/LyraCharacterPartTypes.h"                // FLyraCharacterPart + ECharacterCustomizationCollisionMode
#include "Equipment/LyraEquipmentManagerComponent.h"         // weapon-visual equip target (MinimalAPI, UE_API ctor)
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLoadoutDisplayPawn)

AAFLLoadoutDisplayPawn::AAFLLoadoutDisplayPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// LOAD-BEARING: a LOCAL non-replicated actor has HasAuthority()==true even on a client. That is what makes
	// FLyraCharacterPartList::AddEntry spawn the robot directly (its guard is GetOwner()->HasAuthority()) AND
	// UAFLSkinColorComponent::SetSkinColor apply directly. No server, no replication, no OnRep needed.
	bReplicates = false;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	// NEVER possessed -> no ASC, no ability grant (the ASC-gated AFLCombat grant has no target here).
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;
	// Cosmetic display pawn -> ALWAYS spawn (in-editor drop AND runtime), never blocked by capsule encroachment
	// against the armory geometry ("spawn failed because of actor location").
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetCollisionProfileName(TEXT("NoCollision"));
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->GravityScale = 0.f; // inert -- no controller drives it; keep it exactly where spawned
		Move->SetMovementMode(MOVE_None);
	}

	// Pawn-side cosmetic color/facemask/beam holder (exported AFL component) -- Refresh*ForPawn pushes here.
	SkinColorComp = CreateDefaultSubobject<UAFLSkinColorComponent>(TEXT("SkinColor"));

	// Weapon-visual equip target so the pod can show the equipped weapon (RefreshWeaponForPawn equips onto this).
	CreateDefaultSubobject<ULyraEquipmentManagerComponent>(TEXT("EquipmentManager"));

	// Default content (overridable): the invisible driving mesh the robot part copy-poses from.
	DrivingMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny_Invis.SKM_Manny_Invis")));
}

void AAFLLoadoutDisplayPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Reflection-add ULyraPawnComponent_CharacterParts (module-private to LyraGame -- no LYRAGAME_API, so it can
	// neither be CreateDefaultSubobject'd nor referenced in C++). Look it up by /Script path + NewObject + register.
	// This is the component whose AddCharacterPart spawns the robot child actor + wires its copy-pose to our mesh.
	if (!FindCharacterPartsComponent())
	{
		UClass* PartsClass = FindObject<UClass>(nullptr, TEXT("/Script/LyraGame.LyraPawnComponent_CharacterParts"));
		if (!PartsClass)
		{
			PartsClass = LoadObject<UClass>(nullptr, TEXT("/Script/LyraGame.LyraPawnComponent_CharacterParts"));
		}
		if (PartsClass)
		{
			if (UActorComponent* PartsComp = NewObject<UActorComponent>(this, PartsClass, TEXT("PawnCosmetics")))
			{
				PartsComp->RegisterComponent();
			}
		}
	}
}

void AAFLLoadoutDisplayPawn::BeginPlay()
{
	Super::BeginPlay();

	ApplyDrivingMesh();

	// PLACED in a scene (e.g. the armory) with no external driver -> apply a default body so the robot is VISIBLE
	// standalone. The loadout's ApplySelectionToDisplayPawn overrides this with the player's real identity; a bare
	// placed pawn (armory STEP 2) shows this fallback (set it to B_AFL_Robot_IRONICS) standing on its own.
	if (!DefaultBodyClass.IsNull())
	{
		if (UClass* BodyCls = DefaultBodyClass.LoadSynchronous())
		{
			SetRobotBody(BodyCls);
		}
	}
}

void AAFLLoadoutDisplayPawn::ApplyDrivingMesh()
{
	// The robot part copy-poses from this driving mesh (SKM_Manny_Invis renders invisibly via its material, so
	// only the robot is seen). No AnimBP -> ref-pose (acceptable for the de-risk slice; an idle can drop in later).
	// RE-APPLIED after SetRobotBody: the character-parts add/remove RESETS GetMesh() to null (proven -- comp is
	// SKM_Manny_Invis at BeginPlay, then "No SkeletalMesh" at equip), killing the weapon sockets + the copy-pose.
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}
	USkeletalMesh* SM = DrivingMesh.IsNull() ? nullptr : DrivingMesh.LoadSynchronous();
	if (SM)
	{
		MeshComp->SetSkeletalMeshAsset(SM); // UE5.6 API -- SetSkeletalMesh is the deprecated wrapper
		// LOAD-BEARING: the driver is invisible + off in a SceneCapture, so it can skip pose ticks. Force it to
		// ALWAYS tick pose+bones so the copy-posed robot animates AND the weapon sockets (weapon_r) stay resolvable.
		MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	if (!DrivingAnimClass.IsNull())
	{
		if (UClass* AnimCls = DrivingAnimClass.LoadSynchronous())
		{
			MeshComp->SetAnimInstanceClass(AnimCls);
		}
	}
}

UActorComponent* AAFLLoadoutDisplayPawn::FindCharacterPartsComponent() const
{
	// Walk the super-chain by NAME -- the runtime comp may be a BP subclass, and the unexported base can't be IsA'd.
	TInlineComponentArray<UActorComponent*> Comps(this);
	for (UActorComponent* Comp : Comps)
	{
		if (!Comp) { continue; }
		for (const UClass* C = Comp->GetClass(); C; C = C->GetSuperClass())
		{
			if (C->GetName() == TEXT("LyraPawnComponent_CharacterParts"))
			{
				return Comp;
			}
		}
	}
	return nullptr;
}

void AAFLLoadoutDisplayPawn::SetRobotBody(TSubclassOf<AActor> RobotPartClass)
{
	if (!RobotPartClass)
	{
		return;
	}
	// IDEMPOTENT: the remove+add below is a full robot re-spawn -> skip if the body is already this class (store-
	// preview + revert call this freely; only a real change should swap). BeginPlay's default set seeds it.
	if (RobotPartClass == CurrentRobotClass)
	{
		return;
	}
	UActorComponent* PartsComp = FindCharacterPartsComponent();
	if (!PartsComp)
	{
		return;
	}

	// AddCharacterPart(FLyraCharacterPart) + RemoveAllCharacterParts are UFUNCTIONs on the module-private comp ->
	// callable via the reflected thunk (ProcessEvent), exactly like UAFLCharacterPartSelectorComponent does on the
	// CONTROLLER's comp. We target the DISPLAY pawn's OWN pawn-side comp (the selector's ResolveBodyForPawn adds to
	// the controller's POSSESSED pawn, so it cannot be reused for a display pawn). Remove-then-add = idempotent.
	UFunction* RemoveAllFn = PartsComp->FindFunction(FName(TEXT("RemoveAllCharacterParts")));
	UFunction* AddFn = PartsComp->FindFunction(FName(TEXT("AddCharacterPart")));
	if (!AddFn)
	{
		return;
	}
	if (RemoveAllFn)
	{
		PartsComp->ProcessEvent(RemoveAllFn, nullptr);
	}
	// The ProcessEvent arg buffer MUST include the RETURN slot: AddCharacterPart returns FLyraCharacterPartHandle
	// (LyraPawnComponent_CharacterParts.h:138). A params-ONLY struct makes ProcessEvent write the handle PAST the
	// buffer -> STACK CORRUPTION (it faulted at GetLyraPlayerState reading 0x400 right after this ran; 1a only got
	// away with it because GetPreviewPawn returned immediately). Include ReturnValue so the layout matches the thunk.
	struct FAddCharacterPartArgs { FLyraCharacterPart NewPart; FLyraCharacterPartHandle ReturnValue; };
	FAddCharacterPartArgs Args;
	Args.NewPart.PartClass = RobotPartClass;
	Args.NewPart.SocketName = NAME_None;
	Args.NewPart.CollisionMode = ECharacterCustomizationCollisionMode::NoCollision;
	PartsComp->ProcessEvent(AddFn, &Args);
	CurrentRobotClass = RobotPartClass; // track the applied body for the idempotency guard above

	// The character-parts add (and the RemoveAll above) RESET GetMesh() to null -> re-apply the driving mesh so the
	// equipped weapon's socket (weapon_r) resolves + the robot keeps a copy-pose leader. Root cause of the weapon
	// attaching at the origin instead of the hand (proven via SkinDiag: comp set at BeginPlay, null at equip).
	ApplyDrivingMesh();
}
