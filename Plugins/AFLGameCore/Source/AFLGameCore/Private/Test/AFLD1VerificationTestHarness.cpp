// Copyright C12 AI Gaming. All Rights Reserved.

#include "Test/AFLD1VerificationTestHarness.h"

#include "AFLGameCore.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Engine.h"                                 // GEngine->bUseFixedFrameRate (perf cap detection)
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"                                   // TActorIterator
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/CheatManagerDefines.h"             // UE_WITH_CHEAT_MANAGER -- WITHOUT THIS the macro is
                                                           // undefined, #if evaluates as 0, and the console command
                                                           // is silently compiled out. Build stays green. (B231)
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/LyraPlayerStart.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

namespace
{
	TWeakObjectPtr<UAFLD1VerificationTestHarness> GLiveD1VerifyRun;

	// Entry-point wait for a player character -- see RunInWorld.
	FTimerHandle GPawnWaitTimer;
	double       GPawnWaitDeadline = 0.0;
	const double kPawnWaitTimeout  = 30.0;

	const TCHAR* kDistrictLayer = TEXT("/Game/Maps/DataLayers/L_ShantyTown/District_Duel.District_Duel");

	/**
	 * D1 SPAWN membership is a TAG; D1 STRUCTURE (panels, bound volumes) is data layer membership.
	 *
	 * These two used to be the same test, and that was the defect: spawn points authored inside the runtime
	 * data layer did not exist until it finished streaming, so spawn selection raced streaming (fixed in
	 * 2b9afdb4). A fence is streamed geometry; a spawn point is match configuration and must be knowable the
	 * instant the match is decided. CollectDistrictActors therefore still asks ContainsDataLayer -- correctly,
	 * the panels really are in the layer -- while anything about STARTS asks for this tag instead.
	 *
	 * Config-defined in AFLCoreTags.ini, so requested by name rather than UE_DEFINE_GAMEPLAY_TAG_STATIC.
	 */
	bool IsD1Start(const ALyraPlayerStart* Start)
	{
		if (Start == nullptr)
		{
			return false;
		}
		static const FGameplayTag TagD1 =
			FGameplayTag::RequestGameplayTag(TEXT("AFL.Spawn.District.Duel"), /*ErrorIfNotFound=*/false);
		return TagD1.IsValid() && const_cast<ALyraPlayerStart*>(Start)->GetGameplayTags().HasTag(TagD1);
	}

	// R55 flipped exactly these three on the bound volumes, BLOCK -> IGNORE. Names from
	// Config/DefaultEngine.ini [/Script/Engine.CollisionProfile]:
	//   ECC_GameTraceChannel2 = Lyra_TraceChannel_Weapon
	//   ECC_GameTraceChannel3 = Lyra_TraceChannel_Weapon_Capsule
	//   ECC_GameTraceChannel4 = Lyra_TraceChannel_Weapon_Multi
	const ECollisionChannel kWeaponChannels[3] =
	{
		ECC_GameTraceChannel2, ECC_GameTraceChannel3, ECC_GameTraceChannel4
	};
	const TCHAR* kWeaponChannelNames[3] =
	{
		TEXT("Weapon"), TEXT("Weapon_Capsule"), TEXT("Weapon_Multi")
	};

	// Block-stated figures, kept ONLY as a cross-check against the derived enclosure. The harness
	// scores against the derived box; these exist so a divergence is printed rather than assumed away.
	constexpr float kStatedCentreX = -862.0f;
	constexpr float kStatedCentreY = 1981.0f;
	constexpr float kStatedCapZMin = 7235.0f;

	// Streaming settle. IsStreamingCompleted() returns TRUE when nothing is pending -- which is also
	// true when nothing was ever REQUESTED because no streaming source is in range. So quiescence alone
	// cannot distinguish "streamed and finished" from "never started", and the D2 wait polls for the
	// actors themselves, falling back to the timeout. (measured B241: bounds=0 with state=Activated)
	constexpr float kSettleTimeout    = 8.0f;
	constexpr float kPresenceTimeout  = 20.0f;   // generous -- a slow disk must not read as a missing district
	constexpr int32 kD1MinSettleFrames  = 3;
	constexpr float kPollLogInterval  = 2.0f;

	/** Seed = the block-stated D1 centre, at a Z between the floor (3793..5443) and the cap (7235). */
	const FVector kDefaultSeed(-862.0f, 1981.0f, 6400.0f);

	// Seal trial budgets. Apex of a single jump is ~0.92s, so Execute must outlast a double jump.
	constexpr float kD1PlaceSettle  = 0.4f;
	constexpr float kD1Execute      = 3.0f;
	constexpr float kD1Observe      = 1.4f;
	constexpr float kD1SecondJumpAt = 0.85f;

	constexpr float kD1StandoffVertical = 320.0f;    // adjacent -- vertical verbs need no runway
	constexpr float kD1StandoffRunup    = 1500.0f;   // wall-run / climb need speed on arrival

	/** A trial only counts as CONTAINED if the pawn actually got this close to the face it attacked.
	 *  Without this a pawn that never moved would score as containment -- the seal test's fail-open. */
	constexpr float kEngagedDistance = 420.0f;

	constexpr float kOutsideEpsilon = 25.0f;   // uu of slack before "outside" is called

	/** Clearance below the cap for the ground trace start. Must exceed the cap's own thickness (50 uu)
	 *  or the trace begins inside the cap and resolves ground to the roof. */
	constexpr float kCapClearance = 400.0f;

	constexpr int32 kPointsPerSide = 3;
	constexpr int32 kPerfFrames    = 180;      // ~3s at 60fps, sampled stationary inside the district

	float Percentile(TArray<float>& Sorted, float Pct)
	{
		if (Sorted.Num() == 0) { return 0.0f; }
		Sorted.Sort();
		const int32 Idx = FMath::Clamp(FMath::RoundToInt(Pct * (Sorted.Num() - 1)), 0, Sorted.Num() - 1);
		return Sorted[Idx];
	}
}

const TCHAR* UAFLD1VerificationTestHarness::SideName(ESide S)
{
	switch (S)
	{
	case ESide::XMinus: return TEXT("X-");
	case ESide::XPlus:  return TEXT("X+");
	case ESide::YMinus: return TEXT("Y-");
	case ESide::YPlus:  return TEXT("Y+");
	default:            return TEXT("?");
	}
}

const TCHAR* UAFLD1VerificationTestHarness::VerbName(EVerb V)
{
	switch (V)
	{
	case EVerb::DoubleJump:  return TEXT("DoubleJump");
	case EVerb::WallRunJump: return TEXT("WallRunJump");
	case EVerb::ClimbHeld:   return TEXT("ClimbHeld");
	case EVerb::GrabStack:   return TEXT("GrabStack");
	default:                 return TEXT("?");
	}
}

const TCHAR* UAFLD1VerificationTestHarness::SealResultName(ESealResult R)
{
	switch (R)
	{
	case ESealResult::Contained: return TEXT("CONTAINED");
	case ESealResult::Escaped:   return TEXT("ESCAPED");
	default:                     return TEXT("INCONCLUSIVE");
	}
}

const TCHAR* UAFLD1VerificationTestHarness::StateName(EDataLayerRuntimeState State)
{
	switch (State)
	{
	case EDataLayerRuntimeState::Unloaded:  return TEXT("Unloaded");
	case EDataLayerRuntimeState::Loaded:    return TEXT("Loaded");
	case EDataLayerRuntimeState::Activated: return TEXT("Activated");
	default:                                return TEXT("<unknown>");
	}
}

void UAFLD1VerificationTestHarness::RunInWorld(UWorld* World, const FVector& InSeedLocation)
{
	if (GLiveD1VerifyRun.IsValid())
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_TEST ABORT -- a D1.Verify run is already live. Stop PIE and retry."));
		return;
	}
	// WAIT FOR THE PAWN rather than abort on its absence.
	//
	// Driven from PIE by hand there is always a player character by the time the command is typed. Driven
	// from -ExecCmds there is never one: that fires at frame 0, before the world has spawned anybody, so the
	// harness aborted instantly and could not be run headlessly at all. That is a START-UP ORDERING fact,
	// not a misconfiguration -- so it is waited on here, at the entry point, while StartRun's own check
	// stays strict for every other caller.
	//
	// Bounded, and the bound is a real failure: past it there genuinely is no pawn and the run is refused.
	if (World && World->IsGameWorld() && UGameplayStatics::GetPlayerCharacter(World, 0) == nullptr)
	{
		if (GPawnWaitDeadline <= 0.0)
		{
			GPawnWaitDeadline = World->GetTimeSeconds() + kPawnWaitTimeout;
			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_TEST -- no player character yet; waiting up to %.0fs for one (expected when driven "
				     "from -ExecCmds, which fires at frame 0)."), kPawnWaitTimeout);
		}
		else if (World->GetTimeSeconds() >= GPawnWaitDeadline)
		{
			GPawnWaitDeadline = 0.0;
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_TEST ABORT -- no player character after %.0fs."), kPawnWaitTimeout);
			return;
		}

		const FVector Seed = InSeedLocation;
		TWeakObjectPtr<UWorld> WeakWorld(World);
		World->GetTimerManager().SetTimer(GPawnWaitTimer, FTimerDelegate::CreateLambda(
			[WeakWorld, Seed]()
			{
				if (UWorld* W = WeakWorld.Get())
				{
					UAFLD1VerificationTestHarness::RunInWorld(W, Seed);
				}
			}), 0.5f, /*bLoop=*/false);
		return;
	}
	GPawnWaitDeadline = 0.0;

	UAFLD1VerificationTestHarness* H = NewObject<UAFLD1VerificationTestHarness>();
	if (H->StartRun(World, InSeedLocation))
	{
		GLiveD1VerifyRun = H;
	}
}

bool UAFLD1VerificationTestHarness::StartRun(UWorld* World, const FVector& InSeedLocation)
{
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- no game world. Run from inside PIE."));
		return false;
	}
	// SetDataLayerRuntimeState is server-authoritative; a client run would be silently ignored and
	// look like a mystery FAIL at D2. Refuse loudly up front instead.
	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_TEST ABORT -- CLIENT window. Data layer activation is server-authoritative. Run in the HOST window."));
		return false;
	}

	ACharacter* C = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!C)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- no player character."));
		return false;
	}
	ULyraAbilitySystemComponent* A = Cast<ULyraAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(C));
	if (!A)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- pawn has no LyraAbilitySystemComponent."));
		return false;
	}

	const UDataLayerAsset* Asset = LoadObject<UDataLayerAsset>(nullptr, kDistrictLayer);
	if (!Asset)
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- could not load DataLayerAsset '%s'."), kDistrictLayer);
		return false;
	}
	if (!Asset->IsRuntime())
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_TEST ABORT -- '%s' is an EDITOR data layer; a state change would be silently ignored."), kDistrictLayer);
		return false;
	}
	UDataLayerManager* M = UDataLayerManager::GetDataLayerManager(World);
	if (!M || !M->GetDataLayerInstanceFromAsset(Asset))
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_TEST ABORT -- '%s' has no DataLayerInstance in this world. Wrong map?"), kDistrictLayer);
		return false;
	}

	WorldPtr      = World;
	Pawn          = C;
	ASC           = A;
	DistrictAsset = Asset;
	if (const UCapsuleComponent* Cap = C->GetCapsuleComponent())
	{
		CapsuleHalf = Cap->GetScaledCapsuleHalfHeight();
	}
	SeedLocation = InSeedLocation;
	bRunning = true;
	EnterPhase(EPhase::P_Seed);
	AddToRoot();

	const FVector Where = C->GetActorLocation();
	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST BEGIN -- D1 verification (netmode=%d capsuleHalf=%.0f). Assertions D1-D6 + S2 + PERF. ")
		TEXT("WATCH the boundary: you should see crates low, chain fence above, and NO grey cubes."),
		(int32)World->GetNetMode(), CapsuleHalf);
	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST[SEED] pawn spawned at (%.0f,%.0f,%.0f); pinning to seed (%.0f,%.0f,%.0f), %.0f uu away. ")
		TEXT("A data layer only streams where a STREAMING SOURCE is -- Activated alone loads nothing."),
		Where.X, Where.Y, Where.Z, SeedLocation.X, SeedLocation.Y, SeedLocation.Z,
		FVector::Dist(Where, SeedLocation));
	return true;
}

void UAFLD1VerificationTestHarness::PinPawnIfNeeded()
{
	if (!bPinPawn) { return; }
	ACharacter* C = Pawn.Get();
	if (!C) { return; }

	// Hold position outright rather than teleporting once: until the district streams there may be no
	// floor under the pawn, and a pawn falling out of the world takes the streaming source with it.
	C->SetActorLocation(SeedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* CMC = C->GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
	}
}

void UAFLD1VerificationTestHarness::FinishRun()
{
	ReleaseAllPending();
	bRunning = false;
	Phase    = EPhase::Done;
	UE_LOG(LogAFLGameCore, Display, TEXT("AFL_TEST COMPLETE"));
	GLiveD1VerifyRun = nullptr;
	RemoveFromRoot();
}

void UAFLD1VerificationTestHarness::EnterPhase(EPhase Next)
{
	Phase         = Next;
	PhaseElapsed  = 0.0f;
	PhaseFrames   = 0;
	LastPollLogAt = 0.0f;
}

bool UAFLD1VerificationTestHarness::SetDistrictState(EDataLayerRuntimeState State)
{
	UWorld* W = WorldPtr.Get();
	UDataLayerManager* M = W ? UDataLayerManager::GetDataLayerManager(W) : nullptr;
	return (M && DistrictAsset.IsValid()) ? M->SetDataLayerRuntimeState(DistrictAsset.Get(), State) : false;
}

EDataLayerRuntimeState UAFLD1VerificationTestHarness::EffectiveState() const
{
	UWorld* W = WorldPtr.Get();
	UDataLayerManager* M = W ? UDataLayerManager::GetDataLayerManager(W) : nullptr;
	const UDataLayerInstance* I = (M && DistrictAsset.IsValid())
		? M->GetDataLayerInstanceFromAsset(DistrictAsset.Get()) : nullptr;
	return I ? M->GetDataLayerInstanceEffectiveRuntimeState(I) : EDataLayerRuntimeState::Unloaded;
}

bool UAFLD1VerificationTestHarness::StreamingSettled() const
{
	if (PhaseFrames < kD1MinSettleFrames) { return false; }
	UWorld* W = WorldPtr.Get();
	const UWorldPartitionSubsystem* S = W ? W->GetSubsystem<UWorldPartitionSubsystem>() : nullptr;
	return S ? S->IsStreamingCompleted() : true;
}

void UAFLD1VerificationTestHarness::CollectDistrictActors()
{
	BoundActors.Reset();
	PanelActors.Reset();
	UnclassifiedCount = 0;

	UWorld* W = WorldPtr.Get();
	if (!W || !DistrictAsset.IsValid()) { return; }

	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || !A->ContainsDataLayer(DistrictAsset.Get())) { continue; }

		// A panel is identified by carrying an InstancedStaticMeshComponent; a bound volume by
		// blocking ECC_Pawn. Both are structural facts readable at runtime -- unlike the label, which
		// is editor-only and would never match.
		TArray<UInstancedStaticMeshComponent*> ISMs;
		A->GetComponents<UInstancedStaticMeshComponent>(ISMs);
		if (ISMs.Num() > 0)
		{
			PanelActors.Add(A);
			continue;
		}

		bool bBlocksPawn = false;
		TArray<UPrimitiveComponent*> Prims;
		A->GetComponents<UPrimitiveComponent>(Prims);
		for (const UPrimitiveComponent* P : Prims)
		{
			if (P && P->GetCollisionEnabled() != ECollisionEnabled::NoCollision
				&& P->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
			{
				bBlocksPawn = true;
				break;
			}
		}
		if (bBlocksPawn) { BoundActors.Add(A); }
		else             { ++UnclassifiedCount; }
	}
}

bool UAFLD1VerificationTestHarness::DeriveEnclosure()
{
	bEnclosureValid = false;
	if (BoundActors.Num() == 0) { return false; }

	FBox Box(ForceInit);
	for (const TWeakObjectPtr<AActor>& Weak : BoundActors)
	{
		if (const AActor* A = Weak.Get())
		{
			FVector Origin, Extent;
			A->GetActorBounds(false, Origin, Extent);
			Box += FBox(Origin - Extent, Origin + Extent);
		}
	}
	if (!Box.IsValid) { return false; }

	EnclosureMin    = Box.Min;
	EnclosureMax    = Box.Max;
	bEnclosureValid = true;
	return true;
}

void UAFLD1VerificationTestHarness::CollectPanelInstanceSamples(const AActor* PanelActor, TArray<FVector>& Out, int32 MaxSamples) const
{
	Out.Reset();
	if (!PanelActor) { return; }

	TArray<UInstancedStaticMeshComponent*> ISMs;
	const_cast<AActor*>(PanelActor)->GetComponents<UInstancedStaticMeshComponent>(ISMs);

	// Spread the samples across the ring rather than taking the first N, which would all sit on one
	// face and prove nothing about the other three.
	for (const UInstancedStaticMeshComponent* ISM : ISMs)
	{
		if (!ISM) { continue; }
		const int32 Count = ISM->GetInstanceCount();
		if (Count <= 0) { continue; }

		const int32 Want = FMath::Max(1, MaxSamples / FMath::Max(1, ISMs.Num()));
		const int32 Step = FMath::Max(1, Count / Want);
		for (int32 i = 0; i < Count && Out.Num() < MaxSamples; i += Step)
		{
			FTransform T;
			if (ISM->GetInstanceTransform(i, T, /*bWorldSpace=*/true))
			{
				Out.Add(T.GetLocation());
			}
		}
	}
}

bool UAFLD1VerificationTestHarness::IsOutsideEnclosure(const FVector& Loc, float& OutHowFar) const
{
	OutHowFar = 0.0f;
	if (!bEnclosureValid) { return false; }

	// Outside means beyond ANY face of the enclosure, including above the cap. Standing on top of the
	// cap is therefore outside -- which is correct: the pawn has left the closed volume.
	const float DX = FMath::Max(EnclosureMin.X - Loc.X, Loc.X - EnclosureMax.X);
	const float DY = FMath::Max(EnclosureMin.Y - Loc.Y, Loc.Y - EnclosureMax.Y);
	const float DZ = Loc.Z - EnclosureMax.Z;   // below the floor is not an escape route worth scoring
	OutHowFar = FMath::Max3(DX, DY, DZ);
	return OutHowFar > kOutsideEpsilon;
}

// ---------------------------------------------------------------------------------------------
// D6 -- the five render nothing yet still block. BOTH halves, and the blocking half is a live
// sweep rather than a property read, because a property that says BLOCK and geometry that does not
// is exactly the failure this assertion exists to catch.
// ---------------------------------------------------------------------------------------------
void UAFLD1VerificationTestHarness::RunD6_RenderAndBlock()
{
	UWorld* W = WorldPtr.Get();
	D6Rendering   = 0;
	D6NotBlocking = 0;
	if (!W) { bD6Pass = false; return; }

	for (const TWeakObjectPtr<AActor>& Weak : BoundActors)
	{
		const AActor* A = Weak.Get();
		if (!A) { continue; }

		bool bAnyVisible = false;
		TArray<UPrimitiveComponent*> Prims;
		const_cast<AActor*>(A)->GetComponents<UPrimitiveComponent>(Prims);
		for (const UPrimitiveComponent* P : Prims)
		{
			if (P && P->IsVisible() && !P->bHiddenInGame) { bAnyVisible = true; }
		}
		if (bAnyVisible) { ++D6Rendering; }

		// The blocking half: sweep a pawn-sized capsule at THIS volume and require it to be stopped by
		// THIS volume.
		//
		// Sweep along the volume's THINNEST axis over a SHORT span starting just outside its own
		// surface. A long fixed-axis sweep crosses the whole district and is stopped by whatever it
		// meets first -- the far wall, or a building -- which reads as "this actor does not block"
		// when the actor was simply never reached. (measured B241: the cap and the X+ wall both
		// reported notBlocking for exactly this reason.)
		FVector Origin, Extent;
		A->GetActorBounds(false, Origin, Extent);

		int32 ThinAxis = 0;
		if (Extent.Y < Extent[ThinAxis]) { ThinAxis = 1; }
		if (Extent.Z < Extent[ThinAxis]) { ThinAxis = 2; }
		FVector Axis = FVector::ZeroVector;
		Axis[ThinAxis] = 1.0f;

		const float Half   = Extent[ThinAxis] + CapsuleHalf + 60.0f;
		const FVector From = Origin - Axis * Half;
		const FVector To   = Origin + Axis * Half;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLD1_D6), false);
		if (Pawn.IsValid()) { Params.AddIgnoredActor(Pawn.Get()); }
		// Everything else in the district is irrelevant to "does THIS volume block a pawn".
		for (const TWeakObjectPtr<AActor>& Other : BoundActors) { if (Other.Get() && Other.Get() != A) { Params.AddIgnoredActor(Other.Get()); } }
		for (const TWeakObjectPtr<AActor>& Other : PanelActors) { if (Other.Get()) { Params.AddIgnoredActor(Other.Get()); } }
		FHitResult Hit;
		const bool bHit = W->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Pawn,
			FCollisionShape::MakeCapsule(34.0f, CapsuleHalf), Params);

		if (!bHit || Hit.GetActor() != A) { ++D6NotBlocking; }

		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[D6]   bound at (%.0f,%.0f,%.0f) rendering=%s pawnSweepBlocked=%s"),
			Origin.X, Origin.Y, Origin.Z,
			bAnyVisible ? TEXT("YES <-- should be NO") : TEXT("no"),
			(bHit && Hit.GetActor() == A) ? TEXT("yes") : TEXT("NO <-- seal gone"));
	}

	bD6Pass = (BoundActors.Num() > 0) && (D6Rendering == 0) && (D6NotBlocking == 0);
	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST[D6] RENDER/BLOCK bounds=%d rendering=%d (expected 0) notBlocking=%d (expected 0) %s"),
		BoundActors.Num(), D6Rendering, D6NotBlocking, bD6Pass ? TEXT("PASS") : TEXT("FAIL"));
}

// ---------------------------------------------------------------------------------------------
// D4 -- R55. TWO different results, both required: rounds must CROSS the volume, and panels must
// still TAKE the mark. This has never been exercised; if it fails, R55's flip did not take.
// ---------------------------------------------------------------------------------------------
void UAFLD1VerificationTestHarness::RunD4_Projectile()
{
	UWorld* W = WorldPtr.Get();
	D4ChannelsCrossed = 0;
	bD4PanelTookHit   = false;
	if (!W || !bEnclosureValid || BoundActors.Num() == 0) { bD4Pass = false; return; }

	const FVector Centre = (EnclosureMin + EnclosureMax) * 0.5f;
	const float   HalfX  = (EnclosureMax.X - EnclosureMin.X) * 0.5f;

	// Muzzle inside the district, target well beyond the far face. A round that stops at the boundary
	// is R55 not taking; a round that reaches the far point has crossed.
	const FVector From = FVector(Centre.X, Centre.Y, Centre.Z);
	const FVector To   = FVector(EnclosureMax.X + 2000.0f, Centre.Y, Centre.Z);

	for (int32 i = 0; i < 3; ++i)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLD1_D4), false);
		if (Pawn.IsValid()) { Params.AddIgnoredActor(Pawn.Get()); }
		FHitResult Hit;
		const bool bHit = W->LineTraceSingleByChannel(Hit, From, To, kWeaponChannels[i], Params);

		// Crossed = nothing blocked before the far face. If something DID block, name it: a bound
		// volume means R55 did not take; anything else is scenery and reported as such.
		const bool bBlockedByBound = bHit && BoundActors.ContainsByPredicate(
			[&Hit](const TWeakObjectPtr<AActor>& B) { return B.Get() == Hit.GetActor(); });
		const bool bCrossed = !bBlockedByBound;
		if (bCrossed) { ++D4ChannelsCrossed; }

		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[D4]   channel=%-15s crossedBoundary=%s blockedBy=%s at=%.0f uu"),
			kWeaponChannelNames[i], bCrossed ? TEXT("yes") : TEXT("NO <-- R55 DID NOT TAKE"),
			bHit ? (bBlockedByBound ? TEXT("BOUND VOLUME") : *Hit.GetActor()->GetName()) : TEXT("<nothing>"),
			bHit ? Hit.Distance : -1.0f);
	}

	// Second, different result: a panel must still register a hit, or there is nothing to decal.
	// Aim at REAL INSTANCES. The actor's bounds centroid is the empty middle of the ring.
	for (const TWeakObjectPtr<AActor>& Weak : PanelActors)
	{
		const AActor* P = Weak.Get();
		if (!P) { continue; }

		TArray<FVector> Samples;
		CollectPanelInstanceSamples(P, Samples, 8);
		int32 Hits = 0;

		for (const FVector& Inst : Samples)
		{
			// Short shot from just inboard of this instance, straight at it.
			const FVector Dir   = (Inst - Centre).GetSafeNormal2D();
			const FVector PFrom = Inst - Dir * 250.0f;
			const FVector PTo   = Inst + Dir * 60.0f;

			FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLD1_D4Panel), false);
			if (Pawn.IsValid()) { Params.AddIgnoredActor(Pawn.Get()); }
			FHitResult Hit;
			const bool bHit = W->LineTraceSingleByChannel(Hit, PFrom, PTo, ECC_GameTraceChannel2, Params);
			if (bHit && Hit.GetActor() == P) { ++Hits; }
		}
		if (Hits > 0) { bD4PanelTookHit = true; }

		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[D4]   panel %s instancesSampled=%d tookWeaponHit=%d/%d %s"),
			*P->GetName(), Samples.Num(), Hits, Samples.Num(),
			(Hits > 0) ? TEXT("yes") : TEXT("NO -- nothing to decal"));
	}

	bD4Pass = (D4ChannelsCrossed == 3) && bD4PanelTookHit;
	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST[D4] R55 channelsCrossed=%d/3 panelTookMark=%s %s -- both must hold: rounds cross, panels mark"),
		D4ChannelsCrossed, bD4PanelTookHit ? TEXT("yes") : TEXT("NO"), bD4Pass ? TEXT("PASS") : TEXT("FAIL"));
}

// ---------------------------------------------------------------------------------------------
// D5 -- panels must NOT block pawns, and must not be standable. A crate that blocks is a crate a
// player stands on, and that beats the cap ruling.
// ---------------------------------------------------------------------------------------------
void UAFLD1VerificationTestHarness::RunD5_PanelPawn()
{
	UWorld* W = WorldPtr.Get();
	D5Blocking     = 0;
	bD5CouldStand  = false;
	if (!W || PanelActors.Num() == 0) { bD5Pass = false; return; }

	// Radial direction for each instance is measured from the enclosure centre.
	const FVector Centre = (EnclosureMin + EnclosureMax) * 0.5f;

	int32 TotalSamples = 0;
	for (const TWeakObjectPtr<AActor>& Weak : PanelActors)
	{
		const AActor* P = Weak.Get();
		if (!P) { continue; }

		// REAL INSTANCES, not the ring's centroid. Sweeping the centroid tests the empty middle of the
		// arena and returns "does not block" no matter what the crates do. (B241)
		TArray<FVector> Samples;
		CollectPanelInstanceSamples(P, Samples, 8);
		TotalSamples += Samples.Num();

		const FCollisionShape Capsule = FCollisionShape::MakeCapsule(34.0f, CapsuleHalf);
		int32 BlockedHere = 0, StandableHere = 0;

		for (const FVector& Inst : Samples)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLD1_D5), false);
			if (Pawn.IsValid()) { Params.AddIgnoredActor(Pawn.Get()); }
			for (const TWeakObjectPtr<AActor>& B : BoundActors) { if (B.Get()) { Params.AddIgnoredActor(B.Get()); } }

			// (a) walk straight through this instance, along the ring's radial direction
			const FVector Dir = (Inst - Centre).GetSafeNormal2D();
			FHitResult ThroughHit;
			const bool bThrough = W->SweepSingleByChannel(ThroughHit, Inst - Dir * 300.0f, Inst + Dir * 300.0f,
				FQuat::Identity, ECC_Pawn, Capsule, Params);
			if (bThrough && ThroughHit.GetActor() == P) { ++BlockedHere; }

			// (b) can it be stood on? Drop onto this instance from above.
			FHitResult DownHit;
			const bool bDown = W->SweepSingleByChannel(DownHit, Inst + FVector(0, 0, 400.0f), Inst,
				FQuat::Identity, ECC_Pawn, Capsule, Params);
			if (bDown && DownHit.GetActor() == P) { ++StandableHere; }
		}

		D5Blocking += BlockedHere;
		if (StandableHere > 0) { bD5CouldStand = true; }

		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[D5]   panel %s instancesSampled=%d blocksPawn=%d/%d %s standable=%d/%d %s"),
			*P->GetName(), Samples.Num(), BlockedHere, Samples.Num(),
			BlockedHere > 0 ? TEXT("<-- would beat the cap") : TEXT(""),
			StandableHere, Samples.Num(),
			StandableHere > 0 ? TEXT("<-- would beat the cap") : TEXT(""));
	}

	// Zero samples means zero evidence. Refuse to call that a pass -- it is the shape of the bug this
	// assertion just had.
	if (TotalSamples == 0)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_TEST[D5] NO PANEL INSTANCES SAMPLED -- cannot assert anything. Treating as FAIL, not PASS."));
		bD5Pass = false;
		return;
	}
	bD5Pass = (D5Blocking == 0) && !bD5CouldStand;
	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST[D5] PANEL/PAWN panels=%d blocking=%d (expected 0) standable=%s %s"),
		PanelActors.Num(), D5Blocking, bD5CouldStand ? TEXT("YES") : TEXT("no"),
		bD5Pass ? TEXT("PASS") : TEXT("FAIL"));
}

// ---------------------------------------------------------------------------------------------
// S2 -- report only. Which start does the REAL selector pick, and is it inside D1? The fix depends
// on whether district spawns belong on the district layer, which is a scoping decision, not ours.
// ---------------------------------------------------------------------------------------------
void UAFLD1VerificationTestHarness::RunS2_Spawn()
{
	UWorld* W = WorldPtr.Get();
	if (!W) { return; }

	S2StartsTotal = S2StartsLyra = S2StartsInside = 0;
	for (TActorIterator<APlayerStart> It(W); It; ++It)
	{
		APlayerStart* S = *It;
		if (!S) { continue; }
		++S2StartsTotal;
		const bool bLyra = S->IsA(ALyraPlayerStart::StaticClass());
		if (bLyra) { ++S2StartsLyra; }

		float HowFar = 0.0f;
		const bool bInside = bEnclosureValid && !IsOutsideEnclosure(S->GetActorLocation(), HowFar);
		if (bInside) { ++S2StartsInside; }

		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[S2]   start %-42s lyra=%s loc=(%.0f,%.0f,%.0f) insideD1=%s outsideBy=%.0f uu"),
			*S->GetName(), bLyra ? TEXT("yes") : TEXT("no"),
			S->GetActorLocation().X, S->GetActorLocation().Y, S->GetActorLocation().Z,
			bInside ? TEXT("YES") : TEXT("no"), bInside ? 0.0f : HowFar);
	}

	// Ask the REAL selector, through the game mode, rather than reimplementing its rules here.
	FString ChosenName = TEXT("<none>");
	AGameModeBase* GM = W->GetAuthGameMode();
	APlayerController* PC = UGameplayStatics::GetPlayerController(W, 0);
	if (GM && PC)
	{
		if (AActor* Chosen = GM->ChoosePlayerStart(PC))
		{
			ChosenName = Chosen->GetName();
			float HowFar = 0.0f;
			bS2ChosenInside = bEnclosureValid && !IsOutsideEnclosure(Chosen->GetActorLocation(), HowFar);
			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_TEST[S2] CHOSEN start=%s loc=(%.0f,%.0f,%.0f) insideD1=%s outsideBy=%.0f uu"),
				*ChosenName,
				Chosen->GetActorLocation().X, Chosen->GetActorLocation().Y, Chosen->GetActorLocation().Z,
				bS2ChosenInside ? TEXT("YES") : TEXT("NO <-- spawns outside the sealed boundary"),
				bS2ChosenInside ? 0.0f : HowFar);
			bS2Ran = true;
		}
	}
	if (!bS2Ran)
	{
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_TEST[S2] could not query the selector (gameMode=%s pc=%s) -- candidate census above still stands."),
			GM ? TEXT("ok") : TEXT("null"), PC ? TEXT("ok") : TEXT("null"));
	}

	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST[S2] SPAWN CENSUS starts=%d (lyraStarts=%d -- only these are selector candidates) insideD1=%d outsideD1=%d chosen=%s"),
		S2StartsTotal, S2StartsLyra, S2StartsInside, S2StartsTotal - S2StartsInside, *ChosenName);
}

int32 UAFLD1VerificationTestHarness::CountStarts(int32& OutD1Starts) const
{
	OutD1Starts = 0;
	int32 Total = 0;
	UWorld* W = WorldPtr.Get();
	if (!W) { return 0; }

	for (TActorIterator<ALyraPlayerStart> It(W); It; ++It)
	{
		ALyraPlayerStart* S = *It;
		if (!S) { continue; }
		++Total;
		// D1 membership by TAG. Not the data layer -- starts deliberately live outside it (see IsD1Start),
		// and not the label, which is editor-only and would never match at runtime.
		if (IsD1Start(S))
		{
			++OutD1Starts;
		}
	}
	return Total;
}

// ---------------------------------------------------------------------------------------------
// S3 + S4 -- district ACTIVE. Does a spawn land inside D1, and do the side tags oppose?
// ---------------------------------------------------------------------------------------------
void UAFLD1VerificationTestHarness::RunS3S4_SideSpawn()
{
	UWorld* W = WorldPtr.Get();
	if (!W || !bEnclosureValid) { return; }

	const FVector Centre = (EnclosureMin + EnclosureMax) * 0.5f;
	static const FGameplayTag TagSide0 = FGameplayTag::RequestGameplayTag(TEXT("AFL.Spawn.Side.0"));
	static const FGameplayTag TagSide1 = FGameplayTag::RequestGameplayTag(TEXT("AFL.Spawn.Side.1"));

	int32 Total = CountStarts(S3D1Starts);
	S4Side0Count = S4Side1Count = 0;
	double Sum0 = 0.0, Sum1 = 0.0;

	for (TActorIterator<ALyraPlayerStart> It(W); It; ++It)
	{
		ALyraPlayerStart* S = *It;
		if (!IsD1Start(S)) { continue; }

		const FGameplayTagContainer& Tags = S->GetGameplayTags();
		const FVector L = S->GetActorLocation();
		float HowFar = 0.0f;
		const bool bInside = !IsOutsideEnclosure(L, HowFar);
		const TCHAR* Side = Tags.HasTag(TagSide0) ? TEXT("0") : (Tags.HasTag(TagSide1) ? TEXT("1") : TEXT("<none>"));

		if (Tags.HasTag(TagSide0)) { ++S4Side0Count; Sum0 += L.X; }
		if (Tags.HasTag(TagSide1)) { ++S4Side1Count; Sum1 += L.X; }

		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[S3]   D1 start %-42s side=%-6s loc=(%.0f,%.0f,%.0f) insideD1=%s distFromCentre=%.0f uu"),
			*S->GetName(), Side, L.X, L.Y, L.Z, bInside ? TEXT("YES") : TEXT("NO"),
			FVector::Dist2D(L, Centre));
	}

	S4Side0MeanX = (S4Side0Count > 0) ? (float)(Sum0 / S4Side0Count) : 0.0f;
	S4Side1MeanX = (S4Side1Count > 0) ? (float)(Sum1 / S4Side1Count) : 0.0f;

	// S4: opposing means the two side groups sit on OPPOSITE sides of the district centre. Both
	// clusters on one bank is a duel where one team spawns on top of the other.
	bS4Opposing = (S4Side0Count > 0) && (S4Side1Count > 0)
		&& (((S4Side0MeanX - Centre.X) * (S4Side1MeanX - Centre.X)) < 0.0f);

	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST[S4] SIDES side0=%d (meanX=%.0f) side1=%d (meanX=%.0f) centreX=%.0f -- opposingBanks=%s %s"),
		S4Side0Count, S4Side0MeanX, S4Side1Count, S4Side1MeanX, Centre.X,
		bS4Opposing ? TEXT("yes") : TEXT("NO"), bS4Opposing ? TEXT("PASS") : TEXT("FAIL"));

	// S3: ask the REAL selector.
	AGameModeBase* GM = W->GetAuthGameMode();
	APlayerController* PC = UGameplayStatics::GetPlayerController(W, 0);
	if (GM && PC)
	{
		if (AActor* Chosen = GM->ChoosePlayerStart(PC))
		{
			float HowFar = 0.0f;
			bS3ChosenInsideD1 = !IsOutsideEnclosure(Chosen->GetActorLocation(), HowFar);
			S3ChosenDist = FVector::Dist2D(Chosen->GetActorLocation(), Centre);
			bS3Ran = true;
			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_TEST[S3] ACTIVE totalLyraStarts=%d d1Starts=%d | CHOSEN=%s insideD1=%s distFromCentre=%.0f uu %s"),
				Total, S3D1Starts, *Chosen->GetName(),
				bS3ChosenInsideD1 ? TEXT("YES") : TEXT("NO"), S3ChosenDist,
				bS3ChosenInsideD1 ? TEXT("PASS") : TEXT("FAIL"));
		}
	}
	if (!bS3Ran)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_TEST[S3] could not query the selector (gameMode/PC missing)."));
	}

	// The side filter only engages when a side index exists. Without one it no-ops and EVERY start --
	// including the 27 unscoped BR starts -- stays a candidate, so a pass here would be luck.
	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST[S3] NOTE the side filter needs a side index from IAFLRoundRestartPolicy. If this mode ")
		TEXT("provides none, SideScoped falls back to ALL starts and a correct pick is chance, not scoping. ")
		TEXT("S5 is what actually proves the mechanism."));
}

// ---------------------------------------------------------------------------------------------
// S5 -- THE ONE THAT PROVES THE DESIGN. District unloaded: is the FENCE gone and are the STARTS still there?
//       (It used to assert the starts vanished. That was the abandoned approach -- see the header note.)
// ---------------------------------------------------------------------------------------------
void UAFLD1VerificationTestHarness::RunS5_Verify()
{
	S5TotalStarts = CountStarts(S5D1StartsVisible);
	CollectDistrictActors();
	S5StructureVisible = PanelActors.Num() + BoundActors.Num();
	bS5Ran = true;

	// Structure must be GONE (the layer scopes streamed geometry) and the starts must REMAIN, unchanged
	// from when the district was active (they are match configuration, not streamed content).
	const bool bStructureScoped = (S5StructureVisible == 0);
	const bool bStartsPersisted = (S3D1Starts > 0) && (S5D1StartsVisible == S3D1Starts);
	bS5Pass = bStructureScoped && bStartsPersisted;

	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST[S5] UNLOADED structureVisible=%d (expected 0) d1StartsVisible=%d (expected %d, unchanged) ")
		TEXT("totalLyraStarts=%d effectiveState=%s %s"),
		S5StructureVisible, S5D1StartsVisible, S3D1Starts,
		S5TotalStarts, StateName(EffectiveState()),
		bS5Pass ? TEXT("PASS") : TEXT("FAIL"));

	if (bS5Pass)
	{
		return;
	}

	if (S3D1Starts <= 0)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_TEST[S5] INCONCLUSIVE -- no D1 starts were counted even while the district was ACTIVE. ")
			TEXT("Either the map's starts carry no AFL.Spawn.District.Duel tag, or the tag is unregistered ")
			TEXT("(it is config-defined in AFLCoreTags.ini). Nothing downstream of this proves anything."));
		return;
	}

	if (!bStructureScoped)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_TEST[S5] THE DATA LAYER IS NOT SCOPING GEOMETRY -- %d district actor(s) are still present ")
			TEXT("with the district unloaded. The fence is supposed to be streamed content; if it survives an ")
			TEXT("unload, districts cannot be swapped and the enclosure means nothing."), S5StructureVisible);
	}

	if (!bStartsPersisted)
	{
		UE_LOG(LogAFLGameCore, Error,
			TEXT("AFL_TEST[S5] REGRESSION -- d1 starts went from %d to %d when the district unloaded. They are ")
			TEXT("supposed to be ALWAYS loaded (2b9afdb4): a start that disappears with its layer puts spawn ")
			TEXT("selection back in a race with streaming, which is the defect that made a 2v2 spawn ~150m ")
			TEXT("apart across the island. Check that the starts are still OUTSIDE the runtime data layer and ")
			TEXT("still carry AFL.Spawn.District.Duel."), S3D1Starts, S5D1StartsVisible);
	}
}

// ---------------------------------------------------------------------------------------------
// D3 -- the seal.
// ---------------------------------------------------------------------------------------------
void UAFLD1VerificationTestHarness::BuildSealTrials()
{
	Trials.Reset();
	for (int32 S = 0; S < (int32)ESide::MAX; ++S)
	{
		for (int32 P = 0; P < kPointsPerSide; ++P)
		{
			for (int32 V = 0; V < (int32)EVerb::MAX; ++V)
			{
				Trials.Add(FSealTrial{ (ESide)S, P, (EVerb)V });
			}
		}
	}
	UE_LOG(LogAFLGameCore, Display,
		TEXT("AFL_TEST[D3] SEAL PLAN %d trials = %d sides x %d points x %d verbs (DoubleJump, WallRunJump, ClimbHeld, GrabStack)"),
		Trials.Num(), (int32)ESide::MAX, kPointsPerSide, (int32)EVerb::MAX);
}

void UAFLD1VerificationTestHarness::PlaceForSealTrial()
{
	ACharacter* C = Pawn.Get();
	if (!C || !Trials.IsValidIndex(TrialIndex) || !bEnclosureValid) { return; }

	const FSealTrial& T = Trials[TrialIndex];
	const FVector Centre = (EnclosureMin + EnclosureMax) * 0.5f;
	const FVector Extent = (EnclosureMax - EnclosureMin) * 0.5f;

	const float Standoff = (T.Verb == EVerb::DoubleJump) ? kD1StandoffVertical : kD1StandoffRunup;

	// Spread the sample points along the attacked face at -1/3, 0, +1/3 of its length.
	const float Frac = (T.Point - (kPointsPerSide - 1) * 0.5f) / FMath::Max(1.0f, (float)kPointsPerSide);

	FVector Start = Centre;
	FRotator Face = FRotator::ZeroRotator;
	switch (T.Side)
	{
	case ESide::XMinus:
		Start = FVector(EnclosureMin.X + Standoff, Centre.Y + Frac * Extent.Y * 2.0f, 0.0f);
		Face  = FRotator(0.0f, 180.0f, 0.0f);
		break;
	case ESide::XPlus:
		Start = FVector(EnclosureMax.X - Standoff, Centre.Y + Frac * Extent.Y * 2.0f, 0.0f);
		Face  = FRotator(0.0f, 0.0f, 0.0f);
		break;
	case ESide::YMinus:
		Start = FVector(Centre.X + Frac * Extent.X * 2.0f, EnclosureMin.Y + Standoff, 0.0f);
		Face  = FRotator(0.0f, -90.0f, 0.0f);
		break;
	default:
		Start = FVector(Centre.X + Frac * Extent.X * 2.0f, EnclosureMax.Y - Standoff, 0.0f);
		Face  = FRotator(0.0f, 90.0f, 0.0f);
		break;
	}

	// Drop onto whatever floor is actually there. D1's grade swings 16.49 m, so a fixed Z would put
	// the pawn underground on one side of the perimeter and in the air on the other.
	//
	// THE TRACE MUST START BELOW THE CAP. Starting at EnclosureMax.Z - 50 puts the start point inside
	// the cap band, which resolves "ground" to the cap's own surface and stands the pawn ON THE ROOF --
	// outside the enclosure before the trial begins, so every trial scores ESCAPED without testing
	// anything. (measured B241: firstOutside Z=7377 on all 48 trials, capTop=7285)
	UWorld* W = WorldPtr.Get();
	float GroundZ = Centre.Z;
	if (W)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLD1_Ground), false);
		Params.AddIgnoredActor(C);
		for (const TWeakObjectPtr<AActor>& B : BoundActors) { if (B.Get()) { Params.AddIgnoredActor(B.Get()); } }
		FHitResult Hit;
		const FVector Top(Start.X, Start.Y, EnclosureMax.Z - kCapClearance);
		const FVector Bot(Start.X, Start.Y, EnclosureMin.Z - 500.0f);
		if (W->LineTraceSingleByChannel(Hit, Top, Bot, ECC_Visibility, Params))
		{
			GroundZ = Hit.ImpactPoint.Z;
		}
	}
	Start.Z = GroundZ + CapsuleHalf + 15.0f;

	// A trial that starts OUTSIDE the enclosure can only produce a false ESCAPED. Verify the start
	// point before committing to it, and refuse the trial rather than score it.
	float HowFar = 0.0f;
	bTrialPlacementValid = !IsOutsideEnclosure(Start, HowFar);
	if (!bTrialPlacementValid)
	{
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_TEST[D3] side=%-3s pt=%d verb=%-12s PLACEMENT INVALID -- start (%.0f,%.0f,%.0f) is %.0f uu ")
			TEXT("OUTSIDE the enclosure (groundZ=%.0f capTop=%.0f). Trial refused, NOT scored as an escape."),
			SideName(T.Side), T.Point, VerbName(T.Verb), Start.X, Start.Y, Start.Z, HowFar,
			GroundZ, EnclosureMax.Z);
	}

	static const FGameplayTag TagSprintR = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Movement.Sprint"));
	ReleaseAllPending();
	Release(TagSprintR);

	C->TeleportTo(Start, Face, false, true);
	if (UCharacterMovementComponent* CMC = C->GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
	}

	bEverOutside    = false;
	MaxOutsideDist  = 0.0f;
	FirstOutsideLoc = FVector::ZeroVector;
	PeakZ           = -BIG_NUMBER;
	ClosestWallDist = BIG_NUMBER;
	bVerbFired      = false;
	TagsAtPeak.Reset();
}

void UAFLD1VerificationTestHarness::DriveVerb(float DeltaTime)
{
	ACharacter* C = Pawn.Get();
	if (!C || !Trials.IsValidIndex(TrialIndex)) { return; }

	static const FGameplayTag TagJump   = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Jump"));
	static const FGameplayTag TagClimb  = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Climb"));
	static const FGameplayTag TagSprint = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Movement.Sprint"));
	static const FGameplayTag TagGrab   = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Grab"));

	const FSealTrial& T = Trials[TrialIndex];
	const float Time = PhaseElapsed;

	// Always drive movement INTO the face -- air control is what carries a pawn over an edge.
	const FVector Toward = C->GetActorForwardVector();
	C->AddMovementInput(Toward, 1.0f);

	for (int32 i = Pending.Num() - 1; i >= 0; --i)
	{
		if (Time >= Pending[i].At) { Release(Pending[i].Tag); Pending.RemoveAt(i); }
	}

	const bool bFirstFrame = (Time < DeltaTime * 1.5f);
	if (T.Verb != EVerb::DoubleJump && bFirstFrame) { Press(TagSprint); }

	const bool bNearWall = (ClosestWallDist < kEngagedDistance);

	switch (T.Verb)
	{
	case EVerb::DoubleJump:
		if (bFirstFrame)                                                  { PressHeld(TagJump, 0.20f); bVerbFired = true; }
		else if (Time >= kD1SecondJumpAt && Time < kD1SecondJumpAt + DeltaTime) { PressHeld(TagJump, 0.20f); }
		break;

	case EVerb::WallRunJump:
		// GA_AFL_WallRun_C has NO input tag -- it is induced by carrying speed into the wall. Only send
		// Jump once wall-run state actually appeared, so a plain jump cannot masquerade as a wall-jump.
		if (TagsAtPeak.Contains(TEXT("WallRun")) && Time > 0.3f && !bVerbFired)
		{
			PressHeld(TagJump, 0.20f); bVerbFired = true;
		}
		break;

	case EVerb::ClimbHeld:
		// Held to its limit: climb is bounded by input DURATION, not by height (R49), so the press is
		// held for the whole execute window rather than tapped.
		if (bNearWall && Time > 0.2f && !bVerbFired)
		{
			PressHeld(TagClimb, kD1Execute); bVerbFired = true;
		}
		break;

	case EVerb::GrabStack:
		// BEST EFFORT, and reported as such. Grab-and-stack needs a physics prop, a carry and a
		// release at height; scripting it reliably is beyond this harness. A negative result here is
		// NOT proof the route is closed -- see the D3 summary note.
		if (bNearWall && Time > 0.2f && !bVerbFired)
		{
			PressHeld(TagGrab, 0.60f); bVerbFired = true;
		}
		else if (bVerbFired && Time > 1.2f && Time < 1.2f + DeltaTime)
		{
			PressHeld(TagJump, 0.20f);
		}
		break;

	default:
		break;
	}
}

UAFLD1VerificationTestHarness::ESealResult UAFLD1VerificationTestHarness::ScoreSealTrial() const
{
	// A trial that began outside the enclosure cannot say anything about the seal.
	if (!bTrialPlacementValid) { return ESealResult::Inconclusive; }

	// THE QUESTION IS WHETHER THE PAWN GOT PAST -- not whether it ended standing on top. Any sample
	// outside the enclosure is an escape even if the pawn fell back inside afterwards.
	if (bEverOutside) { return ESealResult::Escaped; }

	// A seal test fails open: a pawn that never engaged trivially "stayed inside". Containment is
	// only credited when the verb fired AND the pawn actually reached the wall.
	if (!bVerbFired || ClosestWallDist > kEngagedDistance) { return ESealResult::Inconclusive; }

	return ESealResult::Contained;
}

void UAFLD1VerificationTestHarness::AdvanceSealTrial()
{
	++TrialIndex;
	if (!Trials.IsValidIndex(TrialIndex))
	{
		UE_LOG(LogAFLGameCore, Display, TEXT("AFL_TEST[D3] SEAL SUMMARY -----------------------------------"));
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[D3] trials=%d contained=%d ESCAPED=%d inconclusive=%d worstFrameDuringSeal=%.2f ms"),
			Trials.Num(), SealContained, SealEscapes, SealInconclusive, WorstFrameMsDuringSeal);

		// CONTAINMENT IS NOT REASSURANCE IF NOTHING GOT NEAR THE CEILING. Report the best height any
		// valid trial reached against the cap it is supposed to test, so a comfortable-looking
		// "0 escapes" cannot be quoted as "the cap holds" when every trial peaked 30 m below it.
		if (BestPeakZ > -BIG_NUMBER * 0.5f && bEnclosureValid)
		{
			const float ShortfallM = (EnclosureMax.Z - BestPeakZ) / 100.0f;
			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_TEST[D3] COVERAGE bestPeakZ=%.0f capTopZ=%.0f -- the highest any trial reached was %.1f m ")
				TEXT("BELOW the cap. %s"),
				BestPeakZ, EnclosureMax.Z, ShortfallM,
				(ShortfallM > 5.0f)
					? TEXT("The cap was therefore NEVER TESTED; 0 escapes says the walls held, not the ceiling.")
					: TEXT("Trials reached the cap band."));
		}
		if (SealInconclusive > 0)
		{
			UE_LOG(LogAFLGameCore, Warning,
				TEXT("AFL_TEST[D3] %d trial(s) INCONCLUSIVE -- the verb never fired or the pawn never reached the wall. ")
				TEXT("These are NOT containment. GrabStack is best-effort and is the likely source; a negative there ")
				TEXT("does not prove the route is closed."), SealInconclusive);
		}
		EnterPhase(EPhase::Perf_Place);
		return;
	}
	EnterPhase(EPhase::D3_Place);
}

// ---------------------------------------------------------------------------------------------
void UAFLD1VerificationTestHarness::Press(const FGameplayTag& Tag)
{
	if (ULyraAbilitySystemComponent* A = ASC.Get()) { A->AbilityInputTagPressed(Tag); }
}
void UAFLD1VerificationTestHarness::Release(const FGameplayTag& Tag)
{
	if (ULyraAbilitySystemComponent* A = ASC.Get()) { A->AbilityInputTagReleased(Tag); }
}
void UAFLD1VerificationTestHarness::PressHeld(const FGameplayTag& Tag, float HoldSeconds)
{
	// A same-frame press+release CANCELS a jump: ACharacter::Jump() only sets bPressedJump and
	// CMC::CheckJumpInput applies the impulse on the NEXT movement tick. (measured B234)
	Press(Tag);
	Pending.Add({ Tag, PhaseElapsed + HoldSeconds });
}
void UAFLD1VerificationTestHarness::ReleaseAllPending()
{
	for (const FPendingRelease& P : Pending) { Release(P.Tag); }
	Pending.Reset();
}

// ---------------------------------------------------------------------------------------------
void UAFLD1VerificationTestHarness::Tick(float DeltaTime)
{
	if (!bRunning) { return; }
	if (!WorldPtr.IsValid())
	{
		UE_LOG(LogAFLGameCore, Error, TEXT("AFL_TEST ABORT -- world went away mid-run."));
		FinishRun();
		return;
	}
	if (!Pawn.IsValid())
	{
		// The pawn dies during seal trials -- a fall off the boundary is exactly the kind of thing
		// these trials provoke. Losing it used to end the run 14 trials in; re-acquire and continue,
		// because a harness that dies partway through reports nothing about the 34 trials it skipped.
		ACharacter* Fresh = UGameplayStatics::GetPlayerCharacter(WorldPtr.Get(), 0);
		ULyraAbilitySystemComponent* FreshASC = Fresh
			? Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Fresh))
			: nullptr;
		if (!Fresh || !FreshASC)
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_TEST ABORT -- pawn went away mid-run and no replacement is available (trial %d of %d)."),
				TrialIndex + 1, Trials.Num());
			FinishRun();
			return;
		}
		Pawn = Fresh;
		ASC  = FreshASC;
		Pending.Reset();
		if (const UCapsuleComponent* Cap = Fresh->GetCapsuleComponent())
		{
			CapsuleHalf = Cap->GetScaledCapsuleHalfHeight();
		}
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AFL_TEST -- pawn was lost (likely a fall death) and has been RE-ACQUIRED; continuing at trial %d of %d. ")
			TEXT("The trial in flight is discarded, not scored."),
			TrialIndex + 1, Trials.Num());
		bTrialPlacementValid = false;   // whatever was in flight is void
	}

	PhaseElapsed += DeltaTime;
	++PhaseFrames;

	// Continuous sampling during a seal trial. This is the assertion -- scoring on the final resting
	// position is exactly the mistake Block 234 made.
	if (Phase == EPhase::D3_Execute || Phase == EPhase::D3_Observe)
	{
		const FVector Loc = Pawn->GetActorLocation();
		float HowFar = 0.0f;
		if (IsOutsideEnclosure(Loc, HowFar))
		{
			if (!bEverOutside) { FirstOutsideLoc = Loc; }
			bEverOutside   = true;
			MaxOutsideDist = FMath::Max(MaxOutsideDist, HowFar);
		}
		// Distance to the attacked face, so "never reached the wall" is distinguishable from "reached
		// it and was stopped" -- without this the two are the same low number.
		if (bEnclosureValid && Trials.IsValidIndex(TrialIndex))
		{
			float Gap = BIG_NUMBER;
			switch (Trials[TrialIndex].Side)
			{
			case ESide::XMinus: Gap = Loc.X - EnclosureMin.X; break;
			case ESide::XPlus:  Gap = EnclosureMax.X - Loc.X; break;
			case ESide::YMinus: Gap = Loc.Y - EnclosureMin.Y; break;
			default:            Gap = EnclosureMax.Y - Loc.Y; break;
			}
			ClosestWallDist = FMath::Min(ClosestWallDist, FMath::Abs(Gap));
		}
		if (Loc.Z > PeakZ)
		{
			PeakZ = Loc.Z;
			if (ULyraAbilitySystemComponent* A = ASC.Get())
			{
				FGameplayTagContainer Owned;
				A->GetOwnedGameplayTags(Owned);
				TagsAtPeak = Owned.ToStringSimple();
			}
		}
		WorstFrameMsDuringSeal = FMath::Max(WorstFrameMsDuringSeal, DeltaTime * 1000.0f);
	}

	PinPawnIfNeeded();

	switch (Phase)
	{
	case EPhase::P_Seed:
	{
		if (PhaseFrames <= 1)
		{
			bPinPawn = true;
			PinPawnIfNeeded();
		}
		// Give World Partition a moment to register the moved streaming source before asking it for
		// a baseline; otherwise D1 measures the pawn's old neighbourhood, not D1.
		else if (PhaseElapsed >= 1.5f)
		{
			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_TEST[SEED] pinned at (%.0f,%.0f,%.0f) -- streaming source in place, starting baseline."),
				SeedLocation.X, SeedLocation.Y, SeedLocation.Z);
			EnterPhase(EPhase::P_Unload);
		}
		break;
	}

	case EPhase::P_Unload:
	{
		const bool bAccepted = SetDistrictState(EDataLayerRuntimeState::Unloaded);
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[D1] UNLOAD requested -- accepted=%s effectiveState=%s"),
			bAccepted ? TEXT("true") : TEXT("FALSE"), StateName(EffectiveState()));
		EnterPhase(EPhase::P_UnloadSettle);
		break;
	}

	case EPhase::P_UnloadSettle:
		if (StreamingSettled() || PhaseElapsed >= kSettleTimeout) { EnterPhase(EPhase::D1_VerifyEmpty); }
		break;

	case EPhase::D1_VerifyEmpty:
	{
		CollectDistrictActors();
		D1BoundCount = BoundActors.Num();
		D1PanelCount = PanelActors.Num();
		const bool bPass = (D1BoundCount == 0 && D1PanelCount == 0);
		// NOTE the fail-open: absent is also what you get when the pawn is nowhere near D1. The seed
		// phase is what makes this a real baseline, and D2 is what proves the baseline meant anything --
		// if D2 also reads 0, D1's PASS proved nothing and the verdict says so.
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[D1] UNLOADED bounds=%d panels=%d unclassified=%d expected=0/0 %s -- effectiveState=%s ")
			TEXT("(meaningful only if D2 then finds them at this same location)"),
			D1BoundCount, D1PanelCount, UnclassifiedCount, bPass ? TEXT("PASS") : TEXT("FAIL"),
			StateName(EffectiveState()));
		EnterPhase(EPhase::D2_Activate);
		break;
	}

	case EPhase::D2_Activate:
	{
		const bool bAccepted = SetDistrictState(EDataLayerRuntimeState::Activated);
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[D2] ACTIVATE requested -- accepted=%s effectiveState=%s"),
			bAccepted ? TEXT("true") : TEXT("FALSE"), StateName(EffectiveState()));
		EnterPhase(EPhase::D2_Settle);
		break;
	}

	case EPhase::D2_Settle:
	{
		// Poll for the ACTORS, not merely for quiescence -- quiescence is also what "nothing was ever
		// requested" looks like. Proceed as soon as the district is fully present; otherwise keep
		// waiting to kPresenceTimeout and log progress, so a slow stream is visibly different from a
		// district that is never coming.
		CollectDistrictActors();
		const int32 B = BoundActors.Num();
		const int32 P = PanelActors.Num();

		if ((B >= 5 && P >= 2) || PhaseElapsed >= kPresenceTimeout)
		{
			EnterPhase(EPhase::D2_VerifyPresent);
			break;
		}
		if (PhaseElapsed - LastPollLogAt >= kPollLogInterval)
		{
			LastPollLogAt = PhaseElapsed;
			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_TEST[D2] waiting %.1fs -- bounds=%d panels=%d streamingCompleted=%s effectiveState=%s"),
				PhaseElapsed, B, P, StreamingSettled() ? TEXT("yes") : TEXT("no"), StateName(EffectiveState()));
		}
		break;
	}

	case EPhase::D2_VerifyPresent:
	{
		CollectDistrictActors();
		D2BoundCount = BoundActors.Num();
		D2PanelCount = PanelActors.Num();

		// Counts, not just a pass -- "five bounds and two panels" and "seven of something" are
		// different facts, and only the first one means the district streamed in whole.
		const bool bPass = (D2BoundCount == 5 && D2PanelCount == 2);
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[D2] ACTIVATED bounds=%d (expected 5) panels=%d (expected 2) unclassified=%d %s -- effectiveState=%s"),
			D2BoundCount, D2PanelCount, UnclassifiedCount, bPass ? TEXT("PASS") : TEXT("FAIL"),
			StateName(EffectiveState()));

		if (!DeriveEnclosure())
		{
			const FVector Where = Pawn.IsValid() ? Pawn->GetActorLocation() : FVector::ZeroVector;
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_TEST ABORT -- no bound volumes streamed in, so there is no enclosure to test against. ")
				TEXT("Everything downstream would be meaningless."));
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_TEST[D2] DIAGNOSIS -- state=%s (so the layer ACCEPTED activation) but 0 actors exist. ")
				TEXT("pawn=(%.0f,%.0f,%.0f) seed=(%.0f,%.0f,%.0f) streamingCompleted=%s waited=%.1fs. ")
				TEXT("Activated means PERMITTED to stream, not loaded: if the seed is not inside D1, its cells ")
				TEXT("are out of range of every streaming source and nothing will ever load. Re-run with an ")
				TEXT("explicit seed: afl.D1.Verify X Y Z"),
				StateName(EffectiveState()), Where.X, Where.Y, Where.Z,
				SeedLocation.X, SeedLocation.Y, SeedLocation.Z,
				StreamingSettled() ? TEXT("yes") : TEXT("no"), kPresenceTimeout);
			FinishRun();
			return;
		}

		// Release the anchor -- from here the pawn must be free to move or every seal trial would be
		// dragged back to the seed.
		bPinPawn = false;
		const FVector C = (EnclosureMin + EnclosureMax) * 0.5f;
		const FVector E = (EnclosureMax - EnclosureMin);
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[D2] ENCLOSURE derived centre=(%.0f,%.0f) size=(%.1f x %.1f m) capTopZ=%.0f floorZ=%.0f")
			TEXT(" | stated centre=(%.0f,%.0f) capZmin=%.0f | dCentre=(%.0f,%.0f)"),
			C.X, C.Y, E.X / 100.0f, E.Y / 100.0f, EnclosureMax.Z, EnclosureMin.Z,
			kStatedCentreX, kStatedCentreY, kStatedCapZMin,
			C.X - kStatedCentreX, C.Y - kStatedCentreY);

		BuildSealTrials();
		EnterPhase(EPhase::D6_RenderAndBlock);
		break;
	}

	case EPhase::D6_RenderAndBlock:
		RunD6_RenderAndBlock();
		EnterPhase(EPhase::D4_Projectile);
		break;

	case EPhase::D4_Projectile:
		RunD4_Projectile();
		EnterPhase(EPhase::D5_PanelPawn);
		break;

	case EPhase::D5_PanelPawn:
		RunD5_PanelPawn();
		TrialIndex = 0;
		EnterPhase(EPhase::D3_Place);
		break;

	case EPhase::D3_Place:
		if (PhaseFrames <= 1)                  { PlaceForSealTrial(); }
		else if (PhaseElapsed >= kD1PlaceSettle) { EnterPhase(EPhase::D3_Execute); }
		break;

	case EPhase::D3_Execute:
		DriveVerb(DeltaTime);
		if (PhaseElapsed >= kD1Execute) { ReleaseAllPending(); EnterPhase(EPhase::D3_Observe); }
		break;

	case EPhase::D3_Observe:
		if (PhaseElapsed >= kD1Observe) { EnterPhase(EPhase::D3_Score); }
		break;

	case EPhase::D3_Score:
	{
		const FSealTrial& T = Trials[TrialIndex];
		const ESealResult R = ScoreSealTrial();
		const FVector Final = Pawn->GetActorLocation();

		switch (R)
		{
		case ESealResult::Escaped:   ++SealEscapes; break;
		case ESealResult::Contained: ++SealContained; break;
		default:                     ++SealInconclusive; break;
		}
		if (bTrialPlacementValid) { BestPeakZ = FMath::Max(BestPeakZ, PeakZ); }

		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[D3] side=%-3s pt=%d verb=%-12s %s placement=%s | everOutside=%s maxOutside=%.0f uu firstOutside=(%.0f,%.0f,%.0f)")
			TEXT(" | finalLoc=(%.0f,%.0f,%.0f) peakZ=%.0f capTopZ=%.0f | closestWall=%.0f uu fired=%s | tags=%s"),
			SideName(T.Side), T.Point, VerbName(T.Verb), SealResultName(R),
			bTrialPlacementValid ? TEXT("ok") : TEXT("INVALID"),
			bEverOutside ? TEXT("YES") : TEXT("no"), MaxOutsideDist,
			FirstOutsideLoc.X, FirstOutsideLoc.Y, FirstOutsideLoc.Z,
			Final.X, Final.Y, Final.Z, PeakZ, EnclosureMax.Z,
			(ClosestWallDist < BIG_NUMBER * 0.5f) ? ClosestWallDist : -1.0f,
			bVerbFired ? TEXT("yes") : TEXT("no"),
			TagsAtPeak.IsEmpty() ? TEXT("<none>") : *TagsAtPeak);

		AdvanceSealTrial();
		break;
	}

	case EPhase::Perf_Place:
	{
		// Stand still at the district centre, facing the panelled face, and measure. Movement would
		// mix traversal cost into a number that is supposed to be about draw cost.
		if (PhaseFrames <= 1)
		{
			FrameMs.Reset();
			const FVector C = (EnclosureMin + EnclosureMax) * 0.5f;
			if (ACharacter* Ch = Pawn.Get())
			{
				Ch->TeleportTo(FVector(C.X, C.Y, C.Z), FRotator(0.0f, 0.0f, 0.0f), false, true);
				if (UCharacterMovementComponent* CMC = Ch->GetCharacterMovement()) { CMC->StopMovementImmediately(); }
			}
		}
		else if (PhaseElapsed >= 0.75f) { EnterPhase(EPhase::Perf_Sample); }
		break;
	}

	case EPhase::Perf_Sample:
	{
		FrameMs.Add(DeltaTime * 1000.0f);
		if (FrameMs.Num() >= kPerfFrames)
		{
			float Sum = 0.0f, Max = 0.0f, Min = BIG_NUMBER;
			for (float Ms : FrameMs) { Sum += Ms; Max = FMath::Max(Max, Ms); Min = FMath::Min(Min, Ms); }
			const float Mean = Sum / FMath::Max(1, FrameMs.Num());
			TArray<float> Sorted = FrameMs;
			const float P95 = Percentile(Sorted, 0.95f);

			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_TEST[PERF] inside district frames=%d mean=%.2f ms (%.0f fps) p95=%.2f ms min=%.2f ms max=%.2f ms | worstDuringSeal=%.2f ms"),
				FrameMs.Num(), Mean, Mean > 0.0f ? 1000.0f / Mean : 0.0f, P95, Min, Max, WorstFrameMsDuringSeal);

			// A real render load VARIES. A near-flat frame time is a cap -- t.MaxFPS, fixed frame rate,
			// or the editor throttling an unfocused viewport -- and the number then describes the
			// harness environment, not the district. Say so rather than let it be quoted as a cost.
			const float Spread = (Mean > 0.0f) ? (Max - Min) / Mean : 0.0f;
			static IConsoleVariable* MaxFPSVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
			const float MaxFPS = MaxFPSVar ? MaxFPSVar->GetFloat() : -1.0f;
			if (Spread < 0.05f)
			{
				UE_LOG(LogAFLGameCore, Warning,
					TEXT("AFL_TEST[PERF] NOT USABLE -- frame time varies by only %.1f%% across %d frames, which is a ")
					TEXT("CAP, not a load (t.MaxFPS=%.0f, fixedFrameRate=%s). Re-measure with the PIE window FOCUSED ")
					TEXT("and no frame limiter. Note the district's own geometry is ~1.1k instances / ~50k tris and ")
					TEXT("cannot plausibly cost this."),
					Spread * 100.0f, FrameMs.Num(), MaxFPS,
					(GEngine && GEngine->bUseFixedFrameRate) ? TEXT("ON") : TEXT("off"));
			}

			// The cost surface behind that number. 600 instances at 45 tris with ONE LOD and no Nanite
			// never cheapen with distance, so the instance/LOD/Nanite facts belong next to the timing.
			//
			// Nanite is read via HasValidNaniteData(), NOT IsNaniteEnabled(). The latter is
			// WITH_EDITORONLY_DATA (StaticMesh.h:909) and does not compile in Shipping -- but the swap is an
			// improvement rather than a workaround, because the two answer different questions:
			//   IsNaniteEnabled()    "should Nanite be BUILT for this mesh"  -- authoring intent
			//   HasValidNaniteData() "does this mesh HAVE valid Nanite render data" -- what the renderer does
			// This log line exists to explain a measured frame cost, so the renderer's truth is the one that
			// belongs in it. A mesh with the box ticked but no built data would have read "on" before and
			// still cost full price -- exactly the case this diagnostic is meant to catch.
			for (const TWeakObjectPtr<AActor>& Weak : PanelActors)
			{
				const AActor* P = Weak.Get();
				if (!P) { continue; }
				TArray<UInstancedStaticMeshComponent*> ISMs;
				const_cast<AActor*>(P)->GetComponents<UInstancedStaticMeshComponent>(ISMs);
				for (const UInstancedStaticMeshComponent* ISM : ISMs)
				{
					if (!ISM) { continue; }
					const UStaticMesh* Mesh = ISM->GetStaticMesh();
					UE_LOG(LogAFLGameCore, Display,
						TEXT("AFL_TEST[PERF]   ISM mesh=%s instances=%d LODs=%d nanite=%s%s"),
						Mesh ? *Mesh->GetName() : TEXT("<none>"),
						ISM->GetInstanceCount(),
						Mesh ? Mesh->GetNumLODs() : 0,
						(Mesh && Mesh->HasValidNaniteData()) ? TEXT("on") : TEXT("OFF"),
						(Mesh && Mesh->GetNumLODs() <= 1 && !Mesh->HasValidNaniteData())
							? TEXT("  <-- one LOD, no Nanite: never cheapens with distance") : TEXT(""));
				}
			}
			EnterPhase(EPhase::S2_Spawn);
		}
		break;
	}

	case EPhase::S2_Spawn:
		RunS2_Spawn();
		EnterPhase(EPhase::S3_SideSpawn);
		break;

	case EPhase::S3_SideSpawn:
		RunS3S4_SideSpawn();
		EnterPhase(EPhase::S5_UnloadReq);
		break;

	case EPhase::S5_UnloadReq:
	{
		const bool bAccepted = SetDistrictState(EDataLayerRuntimeState::Unloaded);
		UE_LOG(LogAFLGameCore, Display,
			TEXT("AFL_TEST[S5] UNLOAD requested -- accepted=%s effectiveState=%s"),
			bAccepted ? TEXT("true") : TEXT("FALSE"), StateName(EffectiveState()));
		EnterPhase(EPhase::S5_Settle);
		break;
	}

	case EPhase::S5_Settle:
	{
		// Poll for the STRUCTURE to actually GO, not the starts. The starts are match configuration and
		// never leave (see the S5 note in the header); waiting on them would burn the full timeout every
		// run and then report a false failure.
		CollectDistrictActors();
		{
			const int32 StructureNow = PanelActors.Num() + BoundActors.Num();
			if (StructureNow == 0 || PhaseElapsed >= kPresenceTimeout)
			{
				EnterPhase(EPhase::S5_Verify);
			}
			else if (PhaseElapsed - LastPollLogAt >= kPollLogInterval)
			{
				LastPollLogAt = PhaseElapsed;
				UE_LOG(LogAFLGameCore, Display,
					TEXT("AFL_TEST[S5] waiting %.1fs -- districtStructureVisible=%d effectiveState=%s"),
					PhaseElapsed, StructureNow, StateName(EffectiveState()));
			}
		}
		break;
	}

	case EPhase::S5_Verify:
		RunS5_Verify();
		EnterPhase(EPhase::Verdict);
		break;

	case EPhase::Verdict:
	{
		const bool bD1 = (D1BoundCount == 0 && D1PanelCount == 0);
		const bool bD2 = (D2BoundCount == 5 && D2PanelCount == 2);
		const bool bD3 = (SealEscapes == 0);

		FString Broke;
		if (!bD1)     { Broke += TEXT("D1(district not empty when unloaded) "); }
		if (!bD2)     { Broke += TEXT("D2(wrong actor counts) "); }
		if (!bD3)     { Broke += TEXT("D3(SEAL BREACHED) "); }
		if (!bD4Pass) { Broke += TEXT("D4(R55 -- rounds blocked or panels unmarked) "); }
		if (!bD5Pass) { Broke += TEXT("D5(panels block or are standable) "); }
		if (!bD6Pass) { Broke += TEXT("D6(rendering, or no longer blocking) "); }
		if (bS5Ran && !bS5Pass) { Broke += TEXT("S5(LAYER SCOPING BROKEN -- streamed-out starts still candidates) "); }
		if (bS3Ran && !bS3ChosenInsideD1) { Broke += TEXT("S3(spawn resolved OUTSIDE D1) "); }
		if (S4Side0Count + S4Side1Count > 0 && !bS4Opposing) { Broke += TEXT("S4(sides not on opposing banks) "); }

		// D3 PASSES ON "NO ESCAPES", WHICH IS ALSO WHAT THIN COVERAGE LOOKS LIKE. A run where two thirds
		// of the trials never engaged and nothing came within 20 m of the cap has not established that
		// the boundary holds -- it has established that the walls stop a double jump. Reporting that as
		// a clean PASS next to a COVERAGE line saying "the cap was NEVER TESTED" is the exact false
		// green this harness exists to prevent, so it is graded INCOMPLETE instead. (B241)
		const bool bCapTested = bEnclosureValid && (BestPeakZ > EnclosureMax.Z - 500.0f);
		const bool bCoverageThin = (SealInconclusive * 2 > Trials.Num()) || !bCapTested;

		if (Broke.IsEmpty() && !bCoverageThin)
		{
			UE_LOG(LogAFLGameCore, Display,
				TEXT("AFL_TEST VERDICT PASS -- D1=%d/%d D2=%d/%d seal contained=%d escaped=0 inconclusive=%d ")
				TEXT("R55 channelsCrossed=3/3 panelMarks=yes D5 clean D6 clean."),
				D1BoundCount, D1PanelCount, D2BoundCount, D2PanelCount, SealContained, SealInconclusive);
		}
		else if (Broke.IsEmpty())
		{
			UE_LOG(LogAFLGameCore, Warning,
				TEXT("AFL_TEST VERDICT INCOMPLETE -- every assertion that RAN passed (D1 D2 D4 D5 D6), but D3 did ")
				TEXT("not cover its own question: contained=%d escaped=0 inconclusive=%d of %d, capTested=%s ")
				TEXT("(bestPeak %.1f m below the cap). This grades THIS HARNESS's coverage, not the boundary: the ")
				TEXT("seal itself was verified by operator PIE test (B241) -- no escape, blocked invisibly. Do not ")
				TEXT("read this line as the seal being in doubt, and do not read a future PASS as replacing that test."),
				SealContained, SealInconclusive, Trials.Num(), bCapTested ? TEXT("yes") : TEXT("NO"),
				bEnclosureValid ? (EnclosureMax.Z - BestPeakZ) / 100.0f : 0.0f);
		}
		else
		{
			UE_LOG(LogAFLGameCore, Error,
				TEXT("AFL_TEST VERDICT FAIL -- broke: %s| D1 b=%d p=%d | D2 b=%d p=%d | D3 contained=%d escaped=%d inconclusive=%d ")
				TEXT("| D4 channels=%d/3 panelMark=%s | D5 blocking=%d standable=%s | D6 rendering=%d notBlocking=%d"),
				*Broke, D1BoundCount, D1PanelCount, D2BoundCount, D2PanelCount,
				SealContained, SealEscapes, SealInconclusive,
				D4ChannelsCrossed, bD4PanelTookHit ? TEXT("yes") : TEXT("no"),
				D5Blocking, bD5CouldStand ? TEXT("yes") : TEXT("no"),
				D6Rendering, D6NotBlocking);
		}

		if (bS2Ran && !bS2ChosenInside)
		{
			UE_LOG(LogAFLGameCore, Warning,
				TEXT("AFL_TEST[S2] FINDING -- the selector chose a start OUTSIDE the sealed boundary ")
				TEXT("(%d of %d starts lie inside D1). Reported only; the fix is a scoping decision."),
				S2StartsInside, S2StartsTotal);
		}
		FinishRun();
		break;
	}

	default:
		FinishRun();
		break;
	}
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLD1VerifyCmd(
		TEXT("afl.D1.Verify"),
		TEXT("D1 duel boundary verification: unloads/activates District_Duel and asserts D1-D6 (presence, seal, ")
		TEXT("R55 projectile-permeability, panel/pawn collision, hidden-but-blocking), plus the S2 spawn census ")
		TEXT("and frame time inside the district. Pins the pawn inside D1 first -- a data layer only streams ")
		TEXT("where a streaming source is. Optional args: X Y Z seed (defaults to the D1 centre). ")
		TEXT("Run in the HOST window; stop PIE after AFL_TEST COMPLETE."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				FVector Seed = kDefaultSeed;
				if (Args.Num() >= 3)
				{
					Seed = FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2]));
				}
				else if (Args.Num() != 0)
				{
					Ar.Logf(TEXT("afl.D1.Verify -- expected 0 or 3 args (X Y Z); got %d. Using the default seed."), Args.Num());
				}
				Ar.Logf(TEXT("afl.D1.Verify -- starting, seed=(%.0f,%.0f,%.0f) (see AFL_TEST lines in LogAFLGameCore)."),
					Seed.X, Seed.Y, Seed.Z);
				UAFLD1VerificationTestHarness::RunInWorld(World, Seed);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
