// Copyright C12 AI Gaming. All Rights Reserved.

#include "Movement/AFLCharacterMovementComponent.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "GameFramework/Character.h"           // ACharacter::GetCharacterMovement() in the saved-move hooks
#include "GameFramework/Pawn.h"
#include "GameFramework/PhysicsVolume.h"   // AFL_MOVEMODE instrumentation reads bWaterVolume off the current volume
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCharacterMovementComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Dashing, "State.Movement.Dashing");

// ============================================================================================
//  M1 PREDICTION SPINE  (Identity Movement AAA upgrade -- project_movement_aaa_upgrade)
//
//  The parkour desync bug came from traversal that direct-drove Velocity/MovementMode from a
//  component tick on BOTH server and client, OUTSIDE FSavedMove prediction -- so the two sides
//  latched different states and the CMC corrected forever ("stuck / pull / slide, jump to break
//  free"). The AAA-correct fix is to carry every movement INTENT through the CMC's own saved-move
//  channel: the owning client packs intent into the move's compressed flags, the server replays
//  the SAME flags, and both sides compute identical movement with no correction.
//
//  M1 lands the plumbing only: the flags, the saved-move, the client prediction data, and a
//  predicted sprint that reads a flag. The custom traversal PHYSICS (wall-run / climb / slide as
//  real PhysCustom sub-modes with gated detection) is M2 -- PhysCustom() below dispatches on the
//  sub-mode so M2 fills each arm without touching this spine. Nothing here is wired to the live
//  hero yet (the pawn still uses the stock CMC); wiring is the operator-watched reparent step.
// ============================================================================================

/** One saved move per simulated frame. The four AFL intent bits ride the base move's compressed
 *  flags (FLAG_Custom_0..3), so they replicate to the server and replay on the client for free. */
class FSavedMove_AFL : public FSavedMove_Character
{
	using Super = FSavedMove_Character;

public:
	uint8 bSavedWantsToSprint : 1;
	uint8 bSavedWantsWallRun  : 1;
	uint8 bSavedWantsClimb    : 1;
	uint8 bSavedWantsSlide    : 1;

	FSavedMove_AFL()
		: bSavedWantsToSprint(0), bSavedWantsWallRun(0), bSavedWantsClimb(0), bSavedWantsSlide(0)
	{
	}

	virtual void Clear() override
	{
		Super::Clear();
		bSavedWantsToSprint = 0;
		bSavedWantsWallRun  = 0;
		bSavedWantsClimb    = 0;
		bSavedWantsSlide    = 0;
	}

	/** Pack the intent into the compressed flags the server will replay from. */
	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Result = Super::GetCompressedFlags();
		if (bSavedWantsToSprint) { Result |= FLAG_Custom_0; }
		if (bSavedWantsWallRun)  { Result |= FLAG_Custom_1; }
		if (bSavedWantsClimb)    { Result |= FLAG_Custom_2; }
		if (bSavedWantsSlide)    { Result |= FLAG_Custom_3; }
		return Result;
	}

	/** Never fold two moves whose intent differs -- a combined move would lose the flag edge and the
	 *  server would replay the wrong intent, which is exactly the desync class M1 exists to kill. */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
	{
		const FSavedMove_AFL* New = static_cast<const FSavedMove_AFL*>(NewMove.Get());
		if (bSavedWantsToSprint != New->bSavedWantsToSprint) { return false; }
		if (bSavedWantsWallRun  != New->bSavedWantsWallRun)  { return false; }
		if (bSavedWantsClimb    != New->bSavedWantsClimb)    { return false; }
		if (bSavedWantsSlide    != New->bSavedWantsSlide)    { return false; }
		return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
	}

	/** Client: capture the CMC's live intent into this move at save time. */
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
		FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
		if (const UAFLCharacterMovementComponent* CMC = C ? Cast<UAFLCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
		{
			bSavedWantsToSprint = CMC->bWantsToSprint ? 1 : 0;
			bSavedWantsWallRun  = CMC->bWantsWallRun  ? 1 : 0;
			bSavedWantsClimb    = CMC->bWantsClimb    ? 1 : 0;
			bSavedWantsSlide    = CMC->bWantsSlide    ? 1 : 0;
		}
	}

	/** Client: restore this move's intent onto the CMC before it is replayed during a correction. */
	virtual void PrepMoveFor(ACharacter* C) override
	{
		Super::PrepMoveFor(C);
		if (UAFLCharacterMovementComponent* CMC = C ? Cast<UAFLCharacterMovementComponent>(C->GetCharacterMovement()) : nullptr)
		{
			CMC->bWantsToSprint = bSavedWantsToSprint != 0;
			CMC->bWantsWallRun  = bSavedWantsWallRun  != 0;
			CMC->bWantsClimb    = bSavedWantsClimb    != 0;
			CMC->bWantsSlide    = bSavedWantsSlide    != 0;
		}
	}
};

/** Hands the CMC our saved-move type so intent is carried through prediction. */
class FNetworkPredictionData_Client_AFL : public FNetworkPredictionData_Client_Character
{
	using Super = FNetworkPredictionData_Client_Character;

public:
	explicit FNetworkPredictionData_Client_AFL(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_AFL());
	}
};

UAFLCharacterMovementComponent::UAFLCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// DO NOT disable the component tick. UCharacterMovementComponent::TickComponent is what applies
	// gravity, falling, walking, and ground sweeps every frame -- the base ctor enables it and it is
	// LOAD-BEARING. An earlier copy-paste set bCanEverTick=false here (a passive-listener idiom), which
	// stranded the possessed hero with NO gravity/ground (WASD moved but jump went up and never came
	// down -- floating, no ground attachment). The dash tag-listener is event-driven
	// (RegisterGameplayTagEvent), NOT tick-driven, so this component needs no tick code of its own --
	// but it MUST keep the parent's movement tick. Leave PrimaryComponentTick to Super.

	// ---- SWIM TUNING (water phase 2; Docs/design/ShantyTown_Water_Swim_DESIGN.md) ----
	//
	// AUTHORED HERE AND NOT IN THE EDITOR, deliberately. MaxSwimSpeed and Buoyancy are
	// UPROPERTY(EditAnywhere, BlueprintReadWrite) -- NOT Config (CharacterMovementComponent.h:272, 380).
	// A native class CDO is not an asset and has no ini backing, so an editor-side CDO edit lives only in
	// memory and vanishes on restart: it would read as done and not be. The constructor is the only place
	// these become durable defaults.

	// 50% of the Pro pawn's 700 land reference. Unmistakably slower than running -- water should cost you
	// something -- while still crossing the ~370 m swimmable band in reasonable time, so the sea reads as
	// traversable rather than as a soft wall (R32).
	//
	// The engine default is 300, which at 43% is coincidentally close. THAT IS EXACTLY WHY THIS NEEDS
	// AUTHORING: right-ish by accident is not a decision, and nothing downstream can tell the difference
	// between a value someone chose and a value nobody touched.
	MaxSwimSpeed = 350.0f;

	// NEUTRAL BUOYANCY. At 1.0 the robot floats at the surface and stays visible, which is what R32's
	// traversable-water ruling needs -- water is a route, and a route you can see yourself on. Below 1.0 the
	// pawn sinks and water becomes a trap, which is the behaviour R32 retired.
	//
	// SET EXPLICITLY EVEN THOUGH IT EQUALS THE ENGINE DEFAULT. This is a RECORDED DECISION, not a
	// non-change: the next reader should find the value chosen and reasoned, not absent. It will NOT
	// serialise -- UE skips properties matching their default -- so the only trace it leaves is this line
	// and this comment. That is the point of writing it.
	Buoyancy = 1.0f;
}

void UAFLCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UAFLCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogAFLMovement, Verbose, TEXT("AFLCMC::BeginPlay on %s (role=%d)"),
		*GetNameSafe(GetOwner()), (int32)(GetOwner() ? GetOwner()->GetLocalRole() : ROLE_None));

	// Try a direct bind first — covers non-Lyra hosts and tests where the ASC is
	// already wired by BeginPlay. For real Lyra player pawns this short-circuits
	// (ASC not yet available) and falls through to the pawn-extension subscription
	// below, which fires later in the init-state lifecycle.
	TryBindToAbilitySystem();

	if (!CachedASC.IsValid())
	{
		RegisterWithLyraPawnExtension();
	}
}

void UAFLCharacterMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromAbilitySystem();
	if (bDashTuningActive)
	{
		RestoreDashTuning();
	}
	Super::EndPlay(EndPlayReason);
}

namespace
{
	/** Readable movement-mode name. Emitted alongside the raw value so a reader never has to map the enum.
	 *  The ordinals are NOT obvious and one of them is a trap: MOVE_Falling is 3 and MOVE_Swimming is 4, so
	 *  anyone grepping for "-> 3" expecting swim is actually watching FALLING -- which fires on every jump
	 *  and every step off a ledge, including the fall INTO water. That misreads as success. */
	const TCHAR* AFLMovementModeName(EMovementMode Mode)
	{
		switch (Mode)
		{
		case MOVE_None:       return TEXT("None");
		case MOVE_Walking:    return TEXT("Walking");
		case MOVE_NavWalking: return TEXT("NavWalking");
		case MOVE_Falling:    return TEXT("Falling");
		case MOVE_Swimming:   return TEXT("Swimming");
		case MOVE_Flying:     return TEXT("Flying");
		case MOVE_Custom:     return TEXT("Custom");
		default:              return TEXT("Unknown");
		}
	}
}

void UAFLCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	// Super FIRST. This override is an observer: the engine's mode-entry handling (and the CharacterOwner
	// notification it performs) must complete before anything is read, or the values logged describe a
	// half-applied transition.
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

#if !UE_BUILD_SHIPPING
	// Guarded to match this project's existing instrumentation convention (UAFLBattleRoyaleComponent uses
	// the same #if for its belief-state dump). Diagnostics carry a cost in a shipping build and none of
	// this is gameplay -- but it stays in Development and Test, which is where it is read.
	const APhysicsVolume* Volume = GetPhysicsVolume();

	UE_LOG(LogAFLMovement, Log,
		TEXT("AFL_MOVEMODE: owner=%s %s(%d) -> %s(%d) inWater=%d volume=%s MaxSwimSpeed=%.1f Buoyancy=%.2f vel=%.1f"),
		*GetNameSafe(GetOwner()),
		AFLMovementModeName(PreviousMovementMode), (int32)PreviousMovementMode,
		AFLMovementModeName(MovementMode),         (int32)MovementMode,
		(Volume && Volume->bWaterVolume) ? 1 : 0,
		*GetNameSafe(Volume),
		MaxSwimSpeed,
		Buoyancy,
		Velocity.Size());
#endif // !UE_BUILD_SHIPPING
}

void UAFLCharacterMovementComponent::OnUnregister()
{
	UnbindFromAbilitySystem();
	if (bDashTuningActive)
	{
		RestoreDashTuning();
	}
	Super::OnUnregister();
}

void UAFLCharacterMovementComponent::RegisterWithLyraPawnExtension()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	ULyraPawnExtensionComponent* PawnExt = ULyraPawnExtensionComponent::FindPawnExtensionComponent(OwnerPawn);
	if (!PawnExt)
	{
		// No Lyra extension — nothing to subscribe to. The direct BeginPlay bind
		// is the only path on non-Lyra pawns and has already run.
		UE_LOG(LogAFLMovement, Verbose,
			TEXT("AFLCharacterMovementComponent: no ULyraPawnExtensionComponent on %s; skipping deferred bind"),
			*GetNameSafe(OwnerPawn));
		return;
	}

	UE_LOG(LogAFLMovement, Verbose,
		TEXT("AFLCMC::RegisterWithLyraPawnExtension: subscribing on %s"),
		*GetNameSafe(OwnerPawn));

	// OnAbilitySystemInitialized_RegisterAndCall both subscribes AND fires immediately
	// if the ASC is already initialized — covers the race where the ASC bound between
	// the BeginPlay TryBind and this registration.
	PawnExt->OnAbilitySystemInitialized_RegisterAndCall(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(
			this, &UAFLCharacterMovementComponent::OnLyraAbilitySystemInitialized));
}

void UAFLCharacterMovementComponent::OnLyraAbilitySystemInitialized()
{
	UE_LOG(LogAFLMovement, Verbose,
		TEXT("AFLCMC::OnLyraAbilitySystemInitialized fired on %s"),
		*GetNameSafe(GetOwner()));
	TryBindToAbilitySystem();
}

void UAFLCharacterMovementComponent::TryBindToAbilitySystem()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwnerPawn);
	if (!ASC)
	{
		// ASC not yet bound. The pawn-extension subscription set up in BeginPlay
		// will retry this once Lyra finishes the init-state lifecycle and binds the
		// PlayerState's ASC to the pawn (FSimpleMulticastDelegate fired from
		// ULyraPawnExtensionComponent::InitializeAbilitySystem).
		UE_LOG(LogAFLMovement, Verbose,
			TEXT("AFLCharacterMovementComponent: ASC not yet available for %s; awaiting Lyra pawn-extension callback"),
			*GetNameSafe(OwnerPawn));
		return;
	}

	// Idempotent: if we're already bound to this exact ASC, no-op.
	if (CachedASC.Get() == ASC && DashTagChangedHandle.IsValid())
	{
		return;
	}

	// If we were bound to a different ASC (e.g. controller swap → fresh PlayerState),
	// unregister the old delegate before binding the new one.
	if (CachedASC.IsValid() && CachedASC.Get() != ASC)
	{
		UnbindFromAbilitySystem();
	}

	CachedASC = ASC;
	DashTagChangedHandle = ASC->RegisterGameplayTagEvent(
			TAG_State_Movement_Dashing, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UAFLCharacterMovementComponent::HandleDashTagChanged);

	UE_LOG(LogAFLMovement, Log,
		TEXT("AFLCharacterMovementComponent: bound dash tag listener on %s (ASC %s)"),
		*GetNameSafe(OwnerPawn), *GetNameSafe(ASC));
}

void UAFLCharacterMovementComponent::UnbindFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		if (DashTagChangedHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(TAG_State_Movement_Dashing, EGameplayTagEventType::NewOrRemoved)
				.Remove(DashTagChangedHandle);
		}
	}
	DashTagChangedHandle.Reset();
	CachedASC.Reset();
}

void UAFLCharacterMovementComponent::HandleDashTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0)
	{
		ApplyDashTuning();
	}
	else
	{
		RestoreDashTuning();
	}
}

void UAFLCharacterMovementComponent::ApplyDashTuning()
{
	if (bDashTuningActive)
	{
		return;
	}

	// Cache at dash ENTRY — captures any modifications applied by other systems
	// (leg-loss penalties, temporary buffs) up to the moment dash begins. This is
	// the §9.6 critical rule: restore must return to the real pre-dash state,
	// not hardcoded construction defaults.
	CachedGroundFriction = GroundFriction;
	CachedAirControl = AirControl;

	GroundFriction = DashGroundFriction;
	AirControl = DashAirControl;
	bDashTuningActive = true;

	UE_LOG(LogAFLMovement, Verbose,
		TEXT("Dash tuning applied: friction %.2f→%.2f, airControl %.2f→%.2f"),
		CachedGroundFriction, GroundFriction, CachedAirControl, AirControl);
}

void UAFLCharacterMovementComponent::RestoreDashTuning()
{
	if (!bDashTuningActive)
	{
		return;
	}

	GroundFriction = CachedGroundFriction;
	AirControl = CachedAirControl;
	bDashTuningActive = false;

	UE_LOG(LogAFLMovement, Verbose,
		TEXT("Dash tuning restored: friction→%.2f, airControl→%.2f"),
		GroundFriction, AirControl);
}

// ---- M1 prediction spine: CMC overrides (unwired until the hero uses this component) ----

FNetworkPredictionData_Client* UAFLCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr);

	if (ClientPredictionData == nullptr)
	{
		UAFLCharacterMovementComponent* Mutable = const_cast<UAFLCharacterMovementComponent*>(this);
		Mutable->ClientPredictionData = new FNetworkPredictionData_Client_AFL(*this);
		// Lyra-canonical smoothing distances (same values ULyraCharacterMovementComponent would inherit).
		Mutable->ClientPredictionData->MaxSmoothNetUpdateDist = 92.0f;
		Mutable->ClientPredictionData->NoSmoothNetUpdateDist  = 140.0f;
	}

	return ClientPredictionData;
}

void UAFLCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// Server + replay: reconstruct the exact intent the owning client had for this move.
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsWallRun  = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bWantsClimb    = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
	bWantsSlide    = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;
}

float UAFLCharacterMovementComponent::GetMaxSpeed() const
{
	// Base first -- ULyraCharacterMovementComponent::GetMaxSpeed() returns ~0 under Gameplay.MovementStopped.
	// Never sprint past that gate: a stopped pawn stays stopped.
	const float BaseMax = Super::GetMaxSpeed();
	if (bWantsToSprint && BaseMax > 0.0f && (IsMovingOnGround() || IsFalling()))
	{
		// Predicted on BOTH sides because bWantsToSprint comes from the compressed flags -- this replaces the
		// tag-driven MaxWalkSpeed swap (which mutated a replicated property outside reconciliation) once wired.
		return FMath::Max(BaseMax, SprintSpeed);
	}
	return BaseMax;
}

void UAFLCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	// M1: no custom physics yet. The dispatch exists so M2 fills wall-run / climb / slide as real
	// PhysCustom sub-modes without reworking the spine. Until then every arm falls through to Super,
	// which safely treats an unknown custom mode as no movement.
	switch (static_cast<EAFLCustomMoveMode>(CustomMovementMode))
	{
	case EAFLCustomMoveMode::WallRun: /* M2: PhysWallRun(DeltaTime, Iterations) */ break;
	case EAFLCustomMoveMode::Climb:   /* M2: PhysClimb(DeltaTime, Iterations)   */ break;
	case EAFLCustomMoveMode::Slide:   /* M2: PhysSlide(DeltaTime, Iterations)   */ break;
	default: break;
	}

	Super::PhysCustom(DeltaTime, Iterations);
}
