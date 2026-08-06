// Copyright C12 AI Gaming. All Rights Reserved.

#include "Movement/AFLCharacterMovementComponent.h"

#include "AFLMovement.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "GameFramework/Pawn.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCharacterMovementComponent)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Movement_Dashing, "State.Movement.Dashing");

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
