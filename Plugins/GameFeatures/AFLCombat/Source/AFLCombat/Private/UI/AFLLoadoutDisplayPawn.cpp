// Copyright C12 AI Gaming. All Rights Reserved.

#include "UI/AFLLoadoutDisplayPawn.h"

#include "Cosmetics/AFLSkinColorComponent.h"                 // pawn-side color holder (the fan-out target)
#include "Cosmetics/LyraCharacterPartTypes.h"                // FLyraCharacterPart + ECharacterCustomizationCollisionMode
#include "Equipment/LyraEquipmentManagerComponent.h"         // weapon-visual equip target (MinimalAPI, UE_API ctor)
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequenceBase.h" // DrivingIdleAnim -- the kiosk's looping unarmed idle
#include "HAL/IConsoleManager.h"        // afl.Loadout.BodyYaw
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLoadoutDisplayPawn)

// The robot's INITIAL facing: relative yaw applied to the driving mesh (parts hang off GetMesh, so
// this turns the whole rendered body + weapon sockets together -- the same axis CreatorRotatePreview
// spins). The preview camera is fixed on +X by pod design; this yaw is what faces the hero into it.
static TAutoConsoleVariable<float> CVarLoadoutBodyYaw(TEXT("afl.Loadout.BodyYaw"), 225.f,
	TEXT("Initial preview-body yaw (deg, mesh-relative). Applied on each dress; spin adds on top. ")
	TEXT("225 = visor + chest emblem square to the +X preview camera (yaw-sweep contact sheet, 2026-08-28)."));

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

	// Relaxed UNARMED idle (operator ruling 2026-08-28) -- the Lyra-canonical unarmed stance, looping
	// single-node on the driving mesh; the robot copy-poses it. Replaces the raw ref-pose.
	DrivingIdleAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(
		TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Unarmed/MM_Unarmed_Idle_Ready.MM_Unarmed_Idle_Ready")));
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
		// Face the hero into the fixed +X preview camera (cvar-tuned; re-applied per dress, so a
		// body swap resets any user spin to the canonical facing).
		MeshComp->SetRelativeRotation(FRotator(0.f, CVarLoadoutBodyYaw.GetValueOnGameThread(), 0.f));
		// LOAD-BEARING: the driver is invisible + off in a SceneCapture, so it can skip pose ticks. Force it to
		// ALWAYS tick pose+bones so the copy-posed robot animates AND the weapon sockets (weapon_r) stay resolvable.
		MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	// POSE: a looping single-node idle wins over the AnimBP path (operator-ruled relaxed unarmed
	// stance -- a kiosk pose needs no state machine). Falls back to DrivingAnimClass, then ref-pose.
	if (!DrivingIdleAnim.IsNull())
	{
		if (UAnimSequenceBase* Idle = DrivingIdleAnim.LoadSynchronous())
		{
			MeshComp->PlayAnimation(Idle, /*bLooping*/ true);
			return;
		}
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
	// EVERY early-out SAYS SO (AFL-3214 parity debugging law): this function returning silently is
	// exactly how a naked display pawn shipped -- the caller records "applied" and nothing retries.
	if (!RobotPartClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AFLDisplayPawn] SetRobotBody: null class on %s -- nothing to wear."), *GetName());
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
		UE_LOG(LogTemp, Warning, TEXT("[AFLDisplayPawn] SetRobotBody: %s has NO CharacterParts component -- cannot dress."), *GetName());
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
		UE_LOG(LogTemp, Warning, TEXT("[AFLDisplayPawn] SetRobotBody: AddCharacterPart not found on %s."), *GetNameSafe(PartsComp));
		return;
	}
	if (RemoveAllFn)
	{
		PartsComp->ProcessEvent(RemoveAllFn, nullptr);
		// The pawn now verifiably wears NOTHING. Clear the guard here, or a failed Add below leaves
		// it holding the OLD class -- and a switch BACK to that class then early-outs at the
		// idempotency check into a permanently naked pawn (the A->B->A hard-fail). The success path
		// re-records it below.
		CurrentRobotClass = nullptr;
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

	// GUARD ONLY A DRESS THAT ACTUALLY HAPPENED. The spawned part attaches as an attached actor;
	// if it is not there, recording CurrentRobotClass would poison the idempotency guard and this
	// pawn would stay naked forever with no retry and no log (measured: cacs=0 attached=0 while the
	// apply loop reported success every frame).
	int32 SpawnedCount = 0;
	TArray<AActor*> AttachedAfter;
	GetAttachedActors(AttachedAfter);
	for (const AActor* Attached : AttachedAfter)
	{
		if (Attached && Attached->GetClass()->IsChildOf(RobotPartClass))
		{
			++SpawnedCount;
		}
	}
	if (SpawnedCount == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[AFLDisplayPawn] SetRobotBody: AddCharacterPart(%s) spawned NOTHING on %s -- guard left open for retry."),
			*RobotPartClass->GetName(), *GetName());
	}
	else
	{
		CurrentRobotClass = RobotPartClass; // track the applied body for the idempotency guard above
		UE_LOG(LogTemp, Log, TEXT("[AFLDisplayPawn] SetRobotBody: %s wearing %s (parts=%d)."),
			*GetName(), *RobotPartClass->GetName(), SpawnedCount);
	}

	// The character-parts add (and the RemoveAll above) RESET GetMesh() to null -> re-apply the driving mesh so the
	// equipped weapon's socket (weapon_r) resolves + the robot keeps a copy-pose leader. Root cause of the weapon
	// attaching at the origin instead of the hand (proven via SkinDiag: comp set at BeginPlay, null at equip).
	ApplyDrivingMesh();
}
