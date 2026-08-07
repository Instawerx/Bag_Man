// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "AFLD1VerificationTestHarness.generated.h"

class AActor;
class ACharacter;
class APlayerStart;
class UDataLayerAsset;
class ULyraAbilitySystemComponent;
class UPrimitiveComponent;
enum class EDataLayerRuntimeState : uint8;

/**
 * UAFLD1VerificationTestHarness  (D1 duel boundary -- the assertions nobody has watched)
 *
 * Cheat-driven FSM behind `afl.D1.Verify` (the afl.DL.StreamTest / afl.DL.HeightTest registration
 * pattern). D1's seal is harness-asserted (2356 sweeps) and its collision is a property readback.
 * Both are real; neither is the project standard, and R55 in particular has NEVER been exercised --
 * three weapon channels were flipped BLOCK -> IGNORE and no shot has ever been fired at the boundary.
 *
 *   AFL_TEST[D1] ...   District_Duel Unloaded -> no boundary, no panels
 *   AFL_TEST[D2] ...   Activated -> five bound volumes AND both panel ISMs, counted separately
 *   AFL_TEST[D3] ...   SEAL -- movement stack driven at the boundary, scored on CROSSING
 *   AFL_TEST[D4] ...   R55 -- rounds cross the volume, panels take the mark (two results, both required)
 *   AFL_TEST[D5] ...   panels do NOT block pawns and cannot be stood on
 *   AFL_TEST[D6] ...   bound volumes render nothing yet still block
 *   AFL_TEST[S2] ...   which PlayerStart the real selector picks, and whether it is inside D1
 *   AFL_TEST[PERF] ... frame time inside the district + the ISM cost surface
 *   AFL_TEST VERDICT / AFL_TEST COMPLETE
 *
 * TWO CLASSIFIER LESSONS ARE BUILT IN, because both have already cost a block:
 *
 *  1. D3 ASKS WHETHER THE PAWN GOT PAST, NOT WHETHER IT ENDED ON TOP. The height harness scores
 *     CLEARED as "grounded at or above the wall top" (AFLBoundaryHeightTestHarness.cpp:325), which is
 *     the right question for measuring a wall and the WRONG one for a seal -- Block 234 scored a 14 m
 *     overshoot as a fail because the pawn did not finish standing up there. Here every tick is
 *     sampled and ANY sample outside the enclosure is an escape, whatever happens afterwards.
 *
 *  2. A SEAL TEST FAILS OPEN. Doing nothing produces "never left the volume", which reads as a PASS.
 *     So containment is only credited when the trial actually ENGAGED -- the verb fired and the pawn
 *     closed to within kEngagedDistance of the wall. Otherwise the trial is INCONCLUSIVE and says so.
 *     A test that cannot fail is not evidence, and a silent no-op is exactly how this project has
 *     produced false greens before.
 *
 * Honesty contract: actors are found by AActor::ContainsDataLayer, never by label -- SetActorLabel is
 * editor-only and a World Partition actor's runtime name is 'StaticMeshActor_UAID_<hash>'. The
 * enclosure box is DERIVED from the streamed bound volumes rather than hardcoded, and the derived
 * figures are logged next to the block-stated ones so a divergence is visible rather than assumed away.
 */
UCLASS()
class AFLGAMECORE_API UAFLD1VerificationTestHarness : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** Entry point for the console command. Refuses to start while a prior run is live.
	 *  SeedLocation is where the pawn is PINNED for the duration of the streaming phases -- see
	 *  bPinPawn. It must be inside D1, because a data layer only streams where a streaming source is. */
	static void RunInWorld(UWorld* World, const FVector& InSeedLocation);

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override { return bRunning; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return WorldPtr.Get(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAFLD1VerificationTestHarness, STATGROUP_Tickables); }
	//~ End FTickableGameObject

private:
	enum class EPhase : uint8
	{
		P_Seed,          // pin the pawn inside D1 FIRST -- without a streaming source in range, nothing
		                 // streams and every presence assertion passes or fails for the wrong reason
		P_Unload,        // force District_Duel Unloaded so D1 measures a real baseline
		P_UnloadSettle,
		D1_VerifyEmpty,
		D2_Activate,
		D2_Settle,
		D2_VerifyPresent,
		D6_RenderAndBlock,
		D4_Projectile,
		D5_PanelPawn,
		D3_Place,        // ---- seal trial loop ----
		D3_Execute,
		D3_Observe,
		D3_Score,
		Perf_Place,
		Perf_Sample,
		S2_Spawn,
		S3_SideSpawn,    // district ACTIVE: does a spawn resolve inside D1, and do the sides oppose?
		S5_UnloadReq,    // district UNLOADED: are the D1 starts invisible to the selector?
		S5_Settle,
		S5_Verify,
		Verdict,
		Done
	};

	/** Which face of the enclosure a trial attacks. Derived from actor position, never from a label. */
	enum class ESide : uint8 { XMinus, XPlus, YMinus, YPlus, MAX };

	/** The traversal verbs driven at the boundary. GrabStack is best-effort -- see DriveVerb. */
	enum class EVerb : uint8 { DoubleJump, WallRunJump, ClimbHeld, GrabStack, MAX };

	enum class ESealResult : uint8 { Contained, Escaped, Inconclusive };

	struct FSealTrial
	{
		ESide Side  = ESide::XMinus;
		int32 Point = 0;
		EVerb Verb  = EVerb::DoubleJump;
	};

	// ---- lifecycle ----
	bool StartRun(UWorld* World, const FVector& InSeedLocation);
	void FinishRun();
	void EnterPhase(EPhase Next);

	/** Hold the pawn at PinLocation with zero velocity. The pawn IS the streaming source; if it falls
	 *  out of the world (likely, since the floor may not have streamed yet) the anchor moves with it
	 *  and the district never loads. Pinning removes that failure mode entirely. */
	void PinPawnIfNeeded();

	// ---- world queries ----
	bool SetDistrictState(EDataLayerRuntimeState State);
	EDataLayerRuntimeState EffectiveState() const;
	bool StreamingSettled() const;

	/** Walks the world once and buckets District_Duel actors into bounds / panels / unclassified.
	 *  Unclassified is REPORTED, never silently dropped -- a miscount must be visible. */
	void CollectDistrictActors();

	/** Union of the bound volumes' bounds. False if no bound volume was found. */
	bool DeriveEnclosure();

	/** World locations of ACTUAL panel instances, sampled across the ring.
	 *
	 *  A panel is ONE ISM actor whose instances ring the perimeter, so GetActorBounds() returns the
	 *  centroid of that ring -- the empty middle of the arena, not any crate. Aiming there made D4
	 *  report occlusion and made D5 report "does not block" against thin air, which is a PASS earned
	 *  by testing nothing. Per-instance transforms are the only honest target. (B241) */
	void CollectPanelInstanceSamples(const AActor* PanelActor, TArray<FVector>& Out, int32 MaxSamples) const;

	/** True if Loc lies outside the derived enclosure (beyond any face, or above the cap).
	 *  OutHowFar receives the largest single-axis overshoot in uu. */
	bool IsOutsideEnclosure(const FVector& Loc, float& OutHowFar) const;

	// ---- assertions ----
	void RunD6_RenderAndBlock();
	void RunD4_Projectile();
	void RunD5_PanelPawn();
	void RunS2_Spawn();

	/** S3 + S4 -- with District_Duel active, does a spawn land inside D1, and do the two side tags
	 *  resolve to OPPOSING banks rather than the same one? */
	void RunS3S4_SideSpawn();

	/** S5 -- the assertion that proves the DESIGN. With the district unloaded, a D1 start must be
	 *  invisible to ULyraPlayerSpawningManagerComponent's TActorIterator. If a streamed-out start is
	 *  still a candidate, layer-scoping does not work and the whole approach needs rethinking. */
	void RunS5_Verify();

	/** Counts ALyraPlayerStart actors in the world, splitting D1 (District_Duel members) from the rest. */
	int32 CountStarts(int32& OutD1Starts) const;

	// ---- seal trial loop ----
	void BuildSealTrials();
	void PlaceForSealTrial();
	void DriveVerb(float DeltaTime);
	ESealResult ScoreSealTrial() const;
	void AdvanceSealTrial();

	// ---- input plumbing (mirrors the height harness; a same-frame press+release cancels a jump) ----
	void Press(const FGameplayTag& Tag);
	void Release(const FGameplayTag& Tag);
	void PressHeld(const FGameplayTag& Tag, float HoldSeconds);
	void ReleaseAllPending();
	struct FPendingRelease { FGameplayTag Tag; float At = 0.0f; };
	TArray<FPendingRelease> Pending;

	static const TCHAR* SideName(ESide S);
	static const TCHAR* VerbName(EVerb V);
	static const TCHAR* SealResultName(ESealResult R);
	static const TCHAR* StateName(EDataLayerRuntimeState State);

	// ---- state ----
	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<ACharacter> Pawn;
	TWeakObjectPtr<ULyraAbilitySystemComponent> ASC;
	TWeakObjectPtr<const UDataLayerAsset> DistrictAsset;

	TArray<TWeakObjectPtr<AActor>> BoundActors;   // AStaticMeshActor + Engine cube, blocks Pawn
	TArray<TWeakObjectPtr<AActor>> PanelActors;   // carries an InstancedStaticMeshComponent
	int32 UnclassifiedCount = 0;

	/** Derived enclosure, in world space. Min/Max are the OUTER extent of the bound volumes, so
	 *  "outside" is conservative and cannot false-positive an escape on wall thickness alone. */
	FVector EnclosureMin = FVector::ZeroVector;
	FVector EnclosureMax = FVector::ZeroVector;
	bool    bEnclosureValid = false;

	/** Where the pawn is pinned while the district streams. Defaults to the block-stated D1 centre;
	 *  overridable from the console so a wrong seed is a one-line retry rather than a rebuild. */
	FVector SeedLocation = FVector::ZeroVector;
	bool    bPinPawn     = false;
	float   LastPollLogAt = 0.0f;

	bool   bRunning     = false;
	EPhase Phase        = EPhase::P_Seed;
	float  PhaseElapsed = 0.0f;
	int32  PhaseFrames  = 0;
	float  CapsuleHalf  = 90.0f;

	// ---- recorded results (-1 / false = never ran, which the verdict names rather than hides) ----
	int32 D1BoundCount = -1;
	int32 D1PanelCount = -1;
	int32 D2BoundCount = -1;
	int32 D2PanelCount = -1;

	bool  bD6Pass = false;   int32 D6Rendering = 0;    int32 D6NotBlocking = 0;
	bool  bD4Pass = false;   int32 D4ChannelsCrossed = 0;  bool bD4PanelTookHit = false;
	bool  bD5Pass = false;   int32 D5Blocking = 0;     bool bD5CouldStand = false;

	// ---- seal trials ----
	TArray<FSealTrial> Trials;
	int32 TrialIndex = 0;
	int32 SealEscapes = 0;
	int32 SealContained = 0;
	int32 SealInconclusive = 0;

	/** Per-trial sampling. bEverOutside is the assertion; the rest exist so a result can be read. */
	bool    bEverOutside     = false;
	float   MaxOutsideDist   = 0.0f;
	FVector FirstOutsideLoc  = FVector::ZeroVector;
	float   PeakZ            = -BIG_NUMBER;
	float   ClosestWallDist  = BIG_NUMBER;
	bool    bVerbFired       = false;
	FString TagsAtPeak;

	/** False when the pawn could not be placed INSIDE the enclosure for this trial. Such a trial can
	 *  only produce a false ESCAPED, so it is refused rather than scored. */
	bool    bTrialPlacementValid = false;

	/** Highest peak reached in any VALID trial, and how far short of the cap it fell. Containment is
	 *  not reassurance if nothing ever got near the ceiling it is supposed to test. */
	float BestPeakZ = -BIG_NUMBER;

	// ---- perf ----
	TArray<float> FrameMs;
	float WorstFrameMsDuringSeal = 0.0f;

	// ---- spawn ----
	int32 S2StartsTotal = 0;
	int32 S2StartsLyra  = 0;
	int32 S2StartsInside = 0;
	bool  bS2ChosenInside = false;
	bool  bS2Ran = false;

	// ---- S3 / S4 / S5 (district spawns) ----
	int32 S3D1Starts       = -1;      // D1 starts visible while the district is ACTIVE
	int32 S3SideIndex      = -1;      // side index the selector actually queried (-1 = none -> filter no-ops)
	bool  bS3ChosenInsideD1 = false;
	float S3ChosenDist     = -1.0f;   // chosen start's distance from the district centre
	bool  bS3Ran           = false;

	int32 S4Side0Count = 0;
	int32 S4Side1Count = 0;
	float S4Side0MeanX = 0.0f;
	float S4Side1MeanX = 0.0f;
	bool  bS4Opposing  = false;

	// S5 ASSERTS THE OPPOSITE OF WHAT IT ORIGINALLY DID, deliberately.
	//
	// It was written to prove that data layer membership SCOPED SPAWNS -- unload the district, the D1 starts
	// vanish from the iterator. That approach was abandoned in 2b9afdb4: starts inside the runtime layer did
	// not exist until it finished streaming, so spawn selection raced streaming. Spawn points are now match
	// configuration (always loaded, AFL.Spawn.District.* tag) and only the FENCE is streamed content.
	//
	// So the invariant flipped. Unloading a district must now take the STRUCTURE and leave the STARTS:
	//   structure gone  -> the data layer still scopes streamed geometry
	//   starts present  -> spawn selection can never race streaming again
	// A run where the starts vanished would now be a REGRESSION, not a pass.
	int32 S5D1StartsVisible  = -1;    // MUST equal S3D1Starts -- starts are unaffected by layer state
	int32 S5StructureVisible = -1;    // MUST be 0 -- panels + bound volumes are streamed content
	int32 S5TotalStarts      = -1;
	bool  bS5Pass            = false;
	bool  bS5Ran             = false;
};
