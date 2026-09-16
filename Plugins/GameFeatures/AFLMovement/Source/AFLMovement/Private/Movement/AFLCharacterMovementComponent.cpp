// Copyright C12 AI Gaming. All Rights Reserved.

#include "Movement/AFLCharacterMovementComponent.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "GameFramework/Character.h"           // ACharacter::GetCharacterMovement() in the saved-move hooks
#include "GameFramework/Pawn.h"
#include "GameFramework/PhysicsVolume.h"   // AFL_MOVEMODE instrumentation reads bWaterVolume off the current volume
#include "Components/CapsuleComponent.h"       // capsule dims for wall-run / ground traces
#include "Engine/World.h"                       // line traces for wall-run detection
#include "HAL/IConsoleManager.h"                // afl.Move.* live-tuning cvars
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCharacterMovementComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Dashing, "State.Movement.Dashing");

// ===================================================================================================
//  afl.Move.*  — LIVE movement-feel tuning (M3). Every knob is a console variable so the operator can
//  dial the super-agile-robot feel DURING a 2-player test on real clients, with no recook. A feel value
//  of -1 is a SENTINEL meaning "keep the pawn's authored (CDO) value"; any other value overrides it.
//  Ship defaults are a deliberate, moderate super step (double-jump + strong air control); everything
//  else defaults to sentinel so the proven base feel is unchanged until the operator tunes it.
// ===================================================================================================
namespace AFLMove
{
	static int32 Tuning = 1;
	static FAutoConsoleVariableRef CVarTuning(TEXT("afl.Move.Tuning"), Tuning,
		TEXT("Master switch for afl.Move.* feel tuning (1=on, 0=leave the CMC entirely authored)."), ECVF_Default);

	// --- aerial feel ---
	static float AirControl = 0.8f;   // super default (authored hero is ~0.35): strong mid-air steering
	static FAutoConsoleVariableRef CVarAirControl(TEXT("afl.Move.AirControl"), AirControl,
		TEXT("Air control 0..1. Super default 0.8; -1 keeps authored."), ECVF_Default);

	static int32 JumpMaxCount = 2;    // super default: double-jump
	static FAutoConsoleVariableRef CVarJumpCount(TEXT("afl.Move.JumpCount"), JumpMaxCount,
		TEXT("Air-jump count. Super default 2 (double-jump); -1 keeps authored."), ECVF_Default);

	static float JumpZ = -1.f;
	static FAutoConsoleVariableRef CVarJumpZ(TEXT("afl.Move.JumpZ"), JumpZ,
		TEXT("Jump Z velocity. -1 keeps authored."), ECVF_Default);

	static float GravityScale = -1.f;
	static FAutoConsoleVariableRef CVarGravityScale(TEXT("afl.Move.GravityScale"), GravityScale,
		TEXT("Gravity scale. -1 keeps authored."), ECVF_Default);

	static float FallingLateralFriction = -1.f;
	static FAutoConsoleVariableRef CVarFLF(TEXT("afl.Move.FallingLateralFriction"), FallingLateralFriction,
		TEXT("Air lateral friction (lower = more momentum kept). -1 keeps authored."), ECVF_Default);

	static float BrakingDecelFalling = -1.f;
	static FAutoConsoleVariableRef CVarBDF(TEXT("afl.Move.BrakingDecelFalling"), BrakingDecelFalling,
		TEXT("Air braking deceleration. -1 keeps authored."), ECVF_Default);

	// --- ground feel (sentinel by default so the proven sprint/walk stays intact) ---
	static float WalkSpeed = -1.f;
	static FAutoConsoleVariableRef CVarWalkSpeed(TEXT("afl.Move.WalkSpeed"), WalkSpeed,
		TEXT("Base MaxWalkSpeed. -1 keeps authored (proven sprint multiplies this)."), ECVF_Default);

	static float MaxAccel = -1.f;
	static FAutoConsoleVariableRef CVarMaxAccel(TEXT("afl.Move.MaxAccel"), MaxAccel,
		TEXT("MaxAcceleration. -1 keeps authored."), ECVF_Default);

	// --- predicted SLIDE (crouch while sprinting) ---
	static int32 SlideEnabled = 1;
	static FAutoConsoleVariableRef CVarSlideEnabled(TEXT("afl.Move.Slide.Enabled"), SlideEnabled,
		TEXT("Predicted momentum slide (1=on)."), ECVF_Default);
	static float SlideFriction = 0.4f;
	static FAutoConsoleVariableRef CVarSlideFriction(TEXT("afl.Move.Slide.Friction"), SlideFriction,
		TEXT("Ground friction while sliding (low = long slide)."), ECVF_Default);
	static float SlideBraking = 400.f;
	static FAutoConsoleVariableRef CVarSlideBraking(TEXT("afl.Move.Slide.Braking"), SlideBraking,
		TEXT("Walking braking deceleration while sliding."), ECVF_Default);
	static float SlideMinSpeed = 250.f;
	static FAutoConsoleVariableRef CVarSlideMinSpeed(TEXT("afl.Move.Slide.MinSpeed"), SlideMinSpeed,
		TEXT("Below this 2D speed the slide releases to normal friction."), ECVF_Default);

	// --- predicted WALL-RUN + WALL-JUMP ---
	static int32 WallRunEnabled = 1;
	static FAutoConsoleVariableRef CVarWREnabled(TEXT("afl.Move.WallRun.Enabled"), WallRunEnabled,
		TEXT("Predicted wall-run + wall-jump (1=on). Master kill-switch for the marquee verb."), ECVF_Default);
	static float WallRunSpeed = 850.f;
	static FAutoConsoleVariableRef CVarWRSpeed(TEXT("afl.Move.WallRun.Speed"), WallRunSpeed,
		TEXT("Along-wall run speed."), ECVF_Default);
	static float WallRunMaxTime = 1.5f;
	static FAutoConsoleVariableRef CVarWRMaxTime(TEXT("afl.Move.WallRun.MaxTime"), WallRunMaxTime,
		TEXT("Max seconds of a single wall-run before it drops."), ECVF_Default);
	static float WallRunMinSpeed = 250.f;
	static FAutoConsoleVariableRef CVarWRMinSpeed(TEXT("afl.Move.WallRun.MinSpeed"), WallRunMinSpeed,
		TEXT("Min 2D speed to START a wall-run (must be actively moving)."), ECVF_Default);
	static float WallRunStick = 150.f;
	static FAutoConsoleVariableRef CVarWRStick(TEXT("afl.Move.WallRun.Stick"), WallRunStick,
		TEXT("Inward bias that keeps the pawn attached to the wall."), ECVF_Default);
	static float WallRunGravity = 0.30f;
	static FAutoConsoleVariableRef CVarWRGravity(TEXT("afl.Move.WallRun.Gravity"), WallRunGravity,
		TEXT("Fraction of gravity applied while wall-running (0 = float, 1 = full)."), ECVF_Default);
	static float WallRunMaxWallAngle = 35.f;
	static FAutoConsoleVariableRef CVarWRAngle(TEXT("afl.Move.WallRun.MaxWallAngle"), WallRunMaxWallAngle,
		TEXT("Max wall tilt from vertical (deg) that still counts as a runnable wall."), ECVF_Default);
	static float WallRunReentry = 0.35f;
	static FAutoConsoleVariableRef CVarWRReentry(TEXT("afl.Move.WallRun.Reentry"), WallRunReentry,
		TEXT("Cooldown (s) after leaving a wall before another wall-run can start."), ECVF_Default);
	static float WallRunTraceDist = 75.f;
	static FAutoConsoleVariableRef CVarWRTrace(TEXT("afl.Move.WallRun.TraceDist"), WallRunTraceDist,
		TEXT("Side-trace reach for finding a runnable wall."), ECVF_Default);
	static float WallRunMinGroundClear = 60.f;
	static FAutoConsoleVariableRef CVarWRClear(TEXT("afl.Move.WallRun.MinGroundClear"), WallRunMinGroundClear,
		TEXT("Min clearance above ground to start/keep a wall-run (stops ground-level latching)."), ECVF_Default);
	static float WallJumpAway = 520.f;
	static FAutoConsoleVariableRef CVarWJAway(TEXT("afl.Move.WallRun.JumpAway"), WallJumpAway,
		TEXT("Wall-jump launch speed away from the wall."), ECVF_Default);
	static float WallJumpUp = 560.f;
	static FAutoConsoleVariableRef CVarWJUp(TEXT("afl.Move.WallRun.JumpUp"), WallJumpUp,
		TEXT("Wall-jump launch speed upward."), ECVF_Default);

	inline float Pick(float CVarVal, float Authored) { return CVarVal >= 0.f ? CVarVal : Authored; }
}

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
	// REPARENT FIX (2026-09-15): once the hero uses THIS CMC, its dash tag-listener and the pawn's separate
	// UAFLDashMovementComponent BOTH bound State.Movement.Dashing and BOTH wrote GroundFriction/AirControl —
	// a double-cache whose restore chain could strand the pawn at dash friction. The proven dash component
	// stays the sole writer of the dash friction/air-control; this CMC listener now only TRACKS the state so
	// ApplyMovementTuning() knows to yield AirControl to the dash component while dashing.
	bDashTuningActive = true;
}

void UAFLCharacterMovementComponent::RestoreDashTuning()
{
	// See ApplyDashTuning(): flag-only now; the dash component restores the actual friction/air-control.
	bDashTuningActive = false;
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
	if (static_cast<EAFLCustomMoveMode>(CustomMovementMode) == EAFLCustomMoveMode::WallRun)
	{
		PhysWallRun(DeltaTime, Iterations);
		return;
	}
	Super::PhysCustom(DeltaTime, Iterations);
}

// ================================ M2/M3 feel + predicted traversal ================================

bool UAFLCharacterMovementComponent::IsDashingNow() const
{
	if (bDashTuningActive) { return true; }
	if (const UAbilitySystemComponent* ASC = CachedASC.Get())
	{
		return ASC->HasMatchingGameplayTag(TAG_State_Movement_Dashing);
	}
	return false;
}

void UAFLCharacterMovementComponent::CacheAuthoredTuning()
{
	if (bAuthoredTuningCached) { return; }
	Authored_AirControl              = AirControl;
	Authored_JumpZVelocity          = JumpZVelocity;
	Authored_GravityScale           = GravityScale;
	Authored_FallingLateralFriction = FallingLateralFriction;
	Authored_BrakingDecelFalling    = BrakingDecelerationFalling;
	Authored_GroundFriction         = GroundFriction;
	Authored_BrakingDecelWalking    = BrakingDecelerationWalking;
	Authored_JumpMaxCount           = CharacterOwner ? CharacterOwner->JumpMaxCount : 1;
	bAuthoredTuningCached = true;
}

void UAFLCharacterMovementComponent::ApplyMovementTuning(float /*DeltaTime*/)
{
	if (AFLMove::Tuning == 0) { return; }
	CacheAuthoredTuning();

	const bool bDashing = IsDashingNow();
	const bool bSliding = (AFLMove::SlideEnabled != 0) && bWantsSlide && IsMovingOnGround()
		&& Velocity.Size2D() >= AFLMove::SlideMinSpeed;

	// Aerial + jump — nothing else writes these, so apply unconditionally (cvar, else authored).
	JumpZVelocity              = AFLMove::Pick(AFLMove::JumpZ, Authored_JumpZVelocity);
	GravityScale               = AFLMove::Pick(AFLMove::GravityScale, Authored_GravityScale);
	FallingLateralFriction     = AFLMove::Pick(AFLMove::FallingLateralFriction, Authored_FallingLateralFriction);
	BrakingDecelerationFalling = AFLMove::Pick(AFLMove::BrakingDecelFalling, Authored_BrakingDecelFalling);
	if (CharacterOwner)
	{
		CharacterOwner->JumpMaxCount = (AFLMove::JumpMaxCount >= 0) ? AFLMove::JumpMaxCount : Authored_JumpMaxCount;
	}

	// Air control + ground friction: yield to the proven dash component while it owns them during a dash.
	if (!bDashing)
	{
		AirControl                 = AFLMove::Pick(AFLMove::AirControl, Authored_AirControl);
		GroundFriction             = bSliding ? AFLMove::SlideFriction : Authored_GroundFriction;
		BrakingDecelerationWalking = bSliding ? AFLMove::SlideBraking  : Authored_BrakingDecelWalking;
	}

	// Optional ground-speed overrides (sentinel by default so the proven sprint/walk feel is untouched).
	if (AFLMove::WalkSpeed >= 0.f) { MaxWalkSpeed    = AFLMove::WalkSpeed; }
	if (AFLMove::MaxAccel  >= 0.f) { MaxAcceleration = AFLMove::MaxAccel; }
}

void UAFLCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (WallRunReentryCooldown > 0.f)
	{
		WallRunReentryCooldown = FMath::Max(0.f, WallRunReentryCooldown - DeltaTime);
	}

	// Apply the feel cvars BEFORE the move runs so this frame uses the tuned values (predicted-safe:
	// every value is deterministic given the same cvars on server + client).
	ApplyMovementTuning(DeltaTime);

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAFLCharacterMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	Super::PhysFalling(deltaTime, Iterations);

	// After the normal fall, try to latch onto a wall. Deterministic (uses saved-move Velocity/Acceleration
	// + world geometry), so client prediction and server replay agree.
	if (MovementMode == MOVE_Falling)
	{
		TryEnterWallRun();
	}
}

bool UAFLCharacterMovementComponent::FindWallRunWall(FVector& OutNormal, FVector& OutPoint) const
{
	const UWorld* World = GetWorld();
	if (!World || !CharacterOwner || !UpdatedComponent) { return false; }

	FVector Fwd = Velocity; Fwd.Z = 0.f;
	if (Fwd.SizeSquared() < 1.f) { Fwd = Acceleration; Fwd.Z = 0.f; }
	if (Fwd.SizeSquared() < 1.f) { Fwd = CharacterOwner->GetActorForwardVector(); Fwd.Z = 0.f; }
	Fwd = Fwd.GetSafeNormal();
	if (Fwd.IsNearlyZero()) { return false; }

	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
	const float CapR = CharacterOwner->GetCapsuleComponent() ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.f;
	const float Reach = AFLMove::WallRunTraceDist + CapR;
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	const float MaxNormalZ = FMath::Sin(FMath::DegreesToRadians(AFLMove::WallRunMaxWallAngle));

	FCollisionQueryParams Params(FName(TEXT("AFLWallRunScan")), false, CharacterOwner);
	bool bFound = false;
	float BestDist = TNumericLimits<float>::Max();
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		FHitResult Hit;
		const FVector End = Loc + Right * (Reach * static_cast<float>(Side));
		if (World->LineTraceSingleByChannel(Hit, Loc, End, ECC_Visibility, Params))
		{
			if (FMath::Abs(Hit.Normal.Z) <= MaxNormalZ && Hit.Distance < BestDist)
			{
				BestDist  = Hit.Distance;
				OutNormal = Hit.Normal;
				OutPoint  = Hit.ImpactPoint;
				bFound = true;
			}
		}
	}
	return bFound;
}

bool UAFLCharacterMovementComponent::TryEnterWallRun()
{
	if (AFLMove::Tuning == 0 || AFLMove::WallRunEnabled == 0) { return false; }
	if (WallRunReentryCooldown > 0.f) { return false; }
	if (!CharacterOwner || !UpdatedComponent || !GetWorld()) { return false; }

	// INTENTIONAL only: must be actively moving AND inputting — never auto-latch (fixes "climbs when not needed").
	if (Velocity.Size2D() < AFLMove::WallRunMinSpeed) { return false; }
	if (Acceleration.SizeSquared() < 1.f) { return false; }

	// Clear of the ground (not about to land).
	{
		const float CapHH = CharacterOwner->GetCapsuleComponent() ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
		const FVector Loc = UpdatedComponent->GetComponentLocation();
		FHitResult Down;
		FCollisionQueryParams P(FName(TEXT("AFLWallRunGround")), false, CharacterOwner);
		if (GetWorld()->LineTraceSingleByChannel(Down, Loc, Loc - FVector(0.f, 0.f, CapHH + AFLMove::WallRunMinGroundClear), ECC_Visibility, P))
		{
			return false;
		}
	}

	FVector N, Pt;
	if (!FindWallRunWall(N, Pt)) { return false; }

	WallRunNormal = N;
	WallRunTimeRemaining = AFLMove::WallRunMaxTime;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(EAFLCustomMoveMode::WallRun));
	UE_LOG(LogAFLMovement, Verbose, TEXT("AFL_WALLRUN: enter n=%s spd=%.0f"), *N.ToCompactString(), Velocity.Size2D());
	return true;
}

void UAFLCharacterMovementComponent::PhysWallRun(float DeltaTime, int32 /*Iterations*/)
{
	if (DeltaTime < 1e-6f) { return; }
	if (!CharacterOwner || !UpdatedComponent || !GetWorld() || AFLMove::WallRunEnabled == 0)
	{
		ExitWallRun(false); return;
	}

	// Jump off the wall.
	if (CharacterOwner->bPressedJump)
	{
		DoWallJump(); return;
	}

	WallRunTimeRemaining -= DeltaTime;
	if (WallRunTimeRemaining <= 0.f) { ExitWallRun(false); return; }

	const FVector Loc = UpdatedComponent->GetComponentLocation();
	const float CapR  = CharacterOwner->GetCapsuleComponent() ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.f;
	const float CapHH = CharacterOwner->GetCapsuleComponent() ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;

	// Re-confirm the wall (trace toward -normal).
	{
		FHitResult WallHit;
		FCollisionQueryParams P(FName(TEXT("AFLWallRunHold")), false, CharacterOwner);
		if (!GetWorld()->LineTraceSingleByChannel(WallHit, Loc, Loc - WallRunNormal * (AFLMove::WallRunTraceDist + CapR + 10.f), ECC_Visibility, P))
		{
			ExitWallRun(false); return;
		}
		WallRunNormal = WallHit.Normal;
	}

	// Dropped to the floor -> normal falling/landing.
	{
		FHitResult Down;
		FCollisionQueryParams P(FName(TEXT("AFLWallRunFloor")), false, CharacterOwner);
		if (GetWorld()->LineTraceSingleByChannel(Down, Loc, Loc - FVector(0.f, 0.f, CapHH + AFLMove::WallRunMinGroundClear * 0.5f), ECC_Visibility, P))
		{
			ExitWallRun(false); return;
		}
	}

	// Along-wall tangent in our direction of travel.
	FVector Dir = Velocity; Dir.Z = 0.f;
	if (Dir.SizeSquared() < 1.f) { Dir = Acceleration; Dir.Z = 0.f; }
	FVector Tangent = FVector::VectorPlaneProject(Dir, WallRunNormal).GetSafeNormal();
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector::VectorPlaneProject(CharacterOwner->GetActorForwardVector(), WallRunNormal).GetSafeNormal();
	}
	if (Tangent.IsNearlyZero()) { ExitWallRun(false); return; }

	// Velocity: run along the wall, slight gravity, inward stick to stay attached.
	FVector NewVel = Tangent * AFLMove::WallRunSpeed;
	NewVel += -WallRunNormal * AFLMove::WallRunStick;
	const float DownV = Velocity.Z + GetGravityZ() * AFLMove::WallRunGravity * DeltaTime;
	NewVel.Z = FMath::Max(DownV, -AFLMove::WallRunSpeed);
	Velocity = NewVel;

	const FVector Delta = Velocity * DeltaTime;
	FHitResult MoveHit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, MoveHit);
	if (MoveHit.bBlockingHit)
	{
		HandleImpact(MoveHit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - MoveHit.Time, MoveHit.Normal, MoveHit, true);
	}

	UpdateComponentVelocity();
}

void UAFLCharacterMovementComponent::DoWallJump()
{
	FVector Launch = WallRunNormal * AFLMove::WallJumpAway + FVector::UpVector * AFLMove::WallJumpUp;

	FVector Dir = Velocity; Dir.Z = 0.f;
	const FVector Tangent = FVector::VectorPlaneProject(Dir, WallRunNormal).GetSafeNormal();
	if (!Tangent.IsNearlyZero())
	{
		Launch += Tangent * FMath::Min(Velocity.Size2D(), AFLMove::WallRunSpeed) * 0.5f;
	}
	Velocity = Launch;
	if (CharacterOwner) { CharacterOwner->bPressedJump = false; } // consume the press
	ExitWallRun(true);
}

void UAFLCharacterMovementComponent::ExitWallRun(bool bFromJump)
{
	WallRunReentryCooldown = FMath::Max(AFLMove::WallRunReentry, 0.05f);
	WallRunTimeRemaining = 0.f;
	if (MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EAFLCustomMoveMode::WallRun))
	{
		SetMovementMode(MOVE_Falling);
	}
	UE_LOG(LogAFLMovement, Verbose, TEXT("AFL_WALLRUN: exit jump=%d"), bFromJump ? 1 : 0);
}
