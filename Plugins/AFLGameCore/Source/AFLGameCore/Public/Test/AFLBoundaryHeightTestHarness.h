// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "AFLBoundaryHeightTestHarness.generated.h"

class AActor;
class ACharacter;
class ULyraAbilitySystemComponent;

/**
 * UAFLBoundaryHeightTestHarness  (district boundary height measurement)
 *
 * Cheat-driven FSM behind `afl.DL.HeightTest` (the afl.DL.StreamTest / afl.Interaction.Test.Run
 * registration pattern). Measures the maximum wall height each traversal verb in the ProMod stack
 * can clear, so district boundary height is a MEASURED spec input rather than a guess.
 *
 *   AFL_TEST[Hn] ...            per-trial: verb, wall height, peak feet Z, verdict
 *   AFL_TEST HEIGHT SUMMARY     per-verb maxima
 *   AFL_TEST VERDICT ...        overall max + recommended boundary height
 *   AFL_TEST COMPLETE           end marker
 *
 * Rig: AFL_HR_Wall_03m .. _14m, 1 m steps, on the Scaffold_HeightRig RUNTIME data layer. The harness
 * activates that layer itself (proven mechanism, Block 231) and unloads it at the end, so the rig is
 * absent from normal play.
 *
 * TWO CONSTRAINTS DISCOVERED IN THE ABILITY SET -- the FSM is shaped by them, not by assumption:
 *  - GA_AFL_WallRun_C has NO input tag. It cannot be pressed; it is induced by carrying speed into a
 *    wall. The harness therefore drives sprint + a velocity vector at the wall and reads whether
 *    wall-run state appeared, rather than "activating" it.
 *  - GA_AFL_WallJump_C shares InputTag.Jump with GA_Hero_Jump_C. The same press means different
 *    things depending on state, so the harness NEVER infers the verb from the input it sent -- it
 *    samples the owned gameplay tags at peak height and reports what was actually active.
 *
 * Honesty contract: CLEARED requires the pawn's FEET (capsule bottom) to finish at or above the
 * wall's top Z *and* be in a grounded movement mode. An ability that fires and gains no height is a
 * FAIL, and anything that ends between the two is AMBIGUOUS and flagged for a human look -- it is
 * never rounded into a pass.
 */
UCLASS()
class AFLGAMECORE_API UAFLBoundaryHeightTestHarness : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	static void RunInWorld(UWorld* World);

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override { return bRunning; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return WorldPtr.Get(); }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAFLBoundaryHeightTestHarness, STATGROUP_Tickables); }
	//~ End FTickableGameObject

private:
	enum class EVerb : uint8 { Jump, DoubleJump, Vault, Climb, WallRunJump, MAX };
	enum class EStage : uint8 { ActivateRig, Place, Execute, Observe, Score, Advance, Summarise, Done };
	enum class EResult : uint8 { Cleared, Failed, Ambiguous };

	struct FWall
	{
		TWeakObjectPtr<AActor> Actor;
		float TopZ    = 0.0f;
		float BaseZ   = 0.0f;
		int32 Metres  = 0;
	};

	bool StartRun(UWorld* World);
	void FinishRun();

	bool CollectRig();
	void PlaceForTrial();
	void DriveVerb(float DeltaTime);
	EResult ScoreTrial(float& OutPeakFeetZ, FString& OutTagsAtPeak) const;
	void AdvanceTrial();

	void SetRigState(bool bActivated);
	void Press(const FGameplayTag& Tag);
	void Release(const FGameplayTag& Tag);

	/** Press now and schedule the release HoldSeconds later. A same-frame press+release CANCELS a jump:
	 *  ACharacter::Jump() only sets bPressedJump, and CMC::CheckJumpInput applies the impulse on the
	 *  NEXT movement tick -- releasing first clears the flag and no jump ever occurs. (measured B234) */
	void PressHeld(const FGameplayTag& Tag, float HoldSeconds);
	void ReleaseAllPending();

	struct FPendingRelease { FGameplayTag Tag; float At = 0.0f; };
	TArray<FPendingRelease> Pending;

	static const TCHAR* VerbName(EVerb V);
	static const TCHAR* ResultName(EResult R);

	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<ACharacter> Pawn;
	TWeakObjectPtr<ULyraAbilitySystemComponent> ASC;

	TArray<FWall> Walls;          // ascending by height
	bool   bRunning      = false;
	EStage Stage         = EStage::ActivateRig;
	float  StageElapsed  = 0.0f;

	int32  VerbIndex     = 0;
	int32  WallIndex     = 0;
	int32  ConsecFails   = 0;

	float  PeakFeetZ     = -BIG_NUMBER;
	FString PeakTags;
	float  CapsuleHalf   = 90.0f;

	/** Horizontal approach tracking. Without this, "never jumped" and "never reached the wall" both
	 *  look like a low peak Z, and the first failed run could not distinguish them. */
	float  StartY        = 0.0f;
	float  ClosestGapY   = BIG_NUMBER;
	bool   bFiredThisTrial = false;

	/** Highest wall CLEARED per verb, in metres. 0 = cleared nothing. */
	int32  BestMetres[(uint8)EVerb::MAX] = { 0 };
	int32  AmbiguousCount = 0;
};
