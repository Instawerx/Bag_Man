// Copyright C12 AI Gaming. All Rights Reserved.

// IRONICS start-loop capture (operator-approved storyboard 2026-09-01).
//
// WHY THIS SHAPE: Sequencer/MRQ authoring through the editor bridge access-violates (banked law),
// so the loop is captured with proven primitives only -- an in-PIE cinematic DIRECTOR that stages
// the shot, drives one camera through the six approved beats at a FIXED 60Hz timestep, and dumps
// numbered frames via FScreenshotRequest (the afl.Creator.Shot lineage). ffmpeg assembles the
// seamless 10s loop offline. Deterministic by construction: frame 0 == frame 600 because nothing
// in the stage is simulated randomness -- pads spin at fixed rates with periods that divide 10s.
//
// Stage subjects are the game's own display language: AAFLDisplayPedestal spawn pads floating the
// REAL hero weapons (Ripsaw / Aria / Scarlett / Hand Cannon XT), and uncontrolled hero pawns
// idling on their own AnimBPs for the robot beats. Everything spawns transient -- nothing saves.

#if !UE_BUILD_SHIPPING

#include "AFLCombat.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h" // TActorIterator (match centroid)
#include "Misc/App.h"
#include "UnrealClient.h"
#include "UObject/SoftObjectPath.h"

namespace AFLCine
{
	struct FBeat
	{
		int32 StartFrame;
		int32 EndFrame;
		FVector CamA;      // absolute (anchor-relative) when TargetIndex < 0
		FVector CamB;
		FRotator Rot;      // used when TargetIndex < 0
		float Fov;
		int32 TargetIndex; // 0..3 = staged pad, 10..13 = staged pawn, -1 = fixed rotation
		float OrbitDist;   // targeted beats: camera orbits the SNAPPED subject (v7 lesson --
		float OrbitDegA;   // anchor-relative cams hovered hundreds of units above the real ground)
		float OrbitDegB;
		float OrbitHeight;
	};

	// The approved storyboard, anchored on BR_Spawn_04 (1093,-3844,3967) -- verified playable ground.
	static const FVector GAnchor(1093.f, -3844.f, 3967.f);
	static const FBeat GBeats[] = {
		{   0, 110, { -1500, -1500,  460 }, { -1250, -1250,  430 }, { -10,  45, 0 }, 70.f, -1, 0, 0, 0, 0 },      // wide over the stacks
		{ 110, 215, {0,0,0}, {0,0,0}, {0,0,0}, 45.f, 10, 460.f, 200.f, 232.f, 190.f },                            // IRONICS hero pawn -- high angle over the rim
		{ 215, 305, {0,0,0}, {0,0,0}, {0,0,0}, 42.f,  0, 500.f, 140.f, 174.f, 210.f },                            // RIPSAW pad -- clears the FX column
		{ 305, 400, {0,0,0}, {0,0,0}, {0,0,0}, 45.f,  1, 520.f, 288.f, 322.f, 200.f },                            // ARIA pad
		{ 400, 495, {0,0,0}, {0,0,0}, {0,0,0}, 48.f,  3, 480.f,  60.f,  96.f, 170.f },                            // hand-cannon XT
		{ 495, 560, {0,0,0}, {0,0,0}, {0,0,0}, 42.f,  2, 490.f, 188.f, 224.f, 220.f },                            // SCARLETT pad
		{ 560, 600, { -1250, -1250,  430 }, { -1500, -1500,  460 }, { -10,  45, 0 }, 70.f, -1, 0, 0, 0, 0 },      // seam: reverse wide -> frame 0 lock
	};
	// Beat coordinates are OFFSETS from the anchor; resolved at run time.

	// The stage: spawn-pad hero subjects (REAL weapons on the game's own display fixtures) + idle pawns.
	struct FPadSpawn { FVector Loc; float Yaw; const TCHAR* CosmeticId; };
	// v8 lesson: scattered snaps put pads inside walled alleys -- the WHOLE stage now clusters on
	// the open plaza (v6-verified clean at ~z3875 around the anchor), orbits sweep free air.
	static const FPadSpawn GPads[] = {
		{ { -300,  200, 0 },  150.f, TEXT("AFL.Weapon.Ripsaw") },
		{ { -150, -350, 0 },  300.f, TEXT("AFL.Weapon.Aria") },
		{ {  250,  300, 0 },  200.f, TEXT("AFL.Weapon.Scarlett") },
		{ {  350, -250, 0 },   80.f, TEXT("AFL.Weapon.HandCannon.IRONICS.XT") },
	};
	static const FVector GPawns[] = { { 0, 80, 0 }, { -450, -100, 0 }, { 150, 500, 0 }, { 500, 50, 0 } };

	/** Snap a stage offset to real ground near the anchor (blind spawns buried the v1 stage). */
	static FVector GroundSnap(UWorld* World, const FVector& Offset)
	{
		const FVector Probe = GAnchor + Offset;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Probe + FVector(0, 0, 2000.f), Probe - FVector(0, 0, 4000.f), ECC_Visibility))
		{
			return Hit.ImpactPoint + FVector(0, 0, 4.f);
		}
		return Probe;
	}

	struct FCaptureState
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<ACameraActor> Camera;
		TArray<TWeakObjectPtr<AActor>> Staged;
		FString OutDir;
		int32 Frame = 0;
		bool bCapture = true;
		int32 SettleTicks = 0;      // counts AFTER the world starts rendering
		bool bStageBuilt = false;   // stage + camera spawn after settle (WP cells loaded by then)
		double RealStart = 0.0;
		TArray<FVector> PadSpots;   // snapped stage positions -- hero beats LOOK AT these
		TArray<FVector> PawnSpots;
	};
	static TSharedPtr<FCaptureState> GState;

	static void BuildStage(UWorld* World, FCaptureState& State)
	{
		// Spawn pads by class path (AFLHub is not a link dependency of AFLCombat).
		UClass* PadClass = FSoftClassPath(TEXT("/Script/AFLHub.AFLDisplayPedestal")).ResolveClass();
		for (const FPadSpawn& P : GPads)
		{
			if (!PadClass) break;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* Pad = World->SpawnActor<AActor>(PadClass, GroundSnap(World, P.Loc), FRotator(0.f, P.Yaw, 0.f), Params);
			if (Pad)
			{
				FProperty* IdProp = PadClass->FindPropertyByName(TEXT("CosmeticId"));
				if (FNameProperty* NameProp = CastField<FNameProperty>(IdProp))
				{
					NameProp->SetPropertyValue_InContainer(Pad, FName(P.CosmeticId));
				}
				State.Staged.Add(Pad);
				State.PadSpots.Add(Pad->GetActorLocation() + FVector(0, 0, 130.f)); // aim at the floating item
				UE_LOG(LogAFLCombat, Log, TEXT("AFL_CINE: staged pad %s at %s."), P.CosmeticId, *Pad->GetActorLocation().ToCompactString());
			}
		}
		// Hero pawns: the game's own hero BP idling on its AnimBP (uncontrolled = ambient life).
		UClass* HeroClass = FSoftClassPath(TEXT("/AFLBagMan/Characters/B_Hero_BagMan_Pro.B_Hero_BagMan_Pro_C")).ResolveClass();
		if (!HeroClass)
		{
			HeroClass = LoadClass<AActor>(nullptr, TEXT("/AFLBagMan/Characters/B_Hero_BagMan_Pro.B_Hero_BagMan_Pro_C"));
		}
		int32 PawnYaw = 0;
		for (const FVector& Loc : GPawns)
		{
			if (!HeroClass) break;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* Pawn = World->SpawnActor<AActor>(HeroClass, GroundSnap(World, Loc) + FVector(0, 0, 90.f), FRotator(0.f, (PawnYaw++ * 97.f), 0.f), Params);
			if (Pawn)
			{
				State.Staged.Add(Pawn);
				State.PawnSpots.Add(Pawn->GetActorLocation() + FVector(0, 0, 40.f)); // chest height
			}
		}
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_CINE: stage built (%d actors)."), State.Staged.Num());
	}

	static void EndCapture(bool bRestoreOnly)
	{
		FApp::SetUseFixedTimeStep(false);
		if (GState.IsValid())
		{
			for (TWeakObjectPtr<AActor>& A : GState->Staged)
			{
				if (AActor* Actor = A.Get()) { Actor->Destroy(); }
			}
			if (ACameraActor* Cam = GState->Camera.Get()) { Cam->Destroy(); }
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_CINE: capture %s."), bRestoreOnly ? TEXT("STAGE CLEARED") : TEXT("COMPLETE -- run encode_loop.sh on the frames"));
		GState.Reset();
	}

	static bool TickCapture(float /*Delta*/)
	{
		if (!GState.IsValid())
		{
			return false;
		}
		FCaptureState& S = *GState;
		UWorld* World = S.World.Get();
		if (!World)
		{
			EndCapture(false);
			return false;
		}
		// PHASE 1 -- SETTLE: v1 burned its 600 frames under the loading screen / WP streaming. Wait
		// a real-time settle (12s) so the experience, streaming and lighting are up, THEN build the
		// stage and start the frame clock.
		if (S.SettleTicks >= 0)
		{
			++S.SettleTicks;
			// A real settle needs a POSSESSED world: player controller present AND 12s of streaming.
			if (!UGameplayStatics::GetPlayerController(World, 0) || FPlatformTime::Seconds() - S.RealStart < 12.0)
			{
				return true;
			}
			S.SettleTicks = -1;
			BuildStage(World, S);
			FActorSpawnParameters CamParams;
			CamParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			S.Camera = World->SpawnActor<ACameraActor>(GAnchor + GBeats[0].CamA, GBeats[0].Rot, CamParams);
			if (S.bCapture)
			{
				FApp::SetUseFixedTimeStep(true);
				FApp::SetFixedDeltaTime(1.0 / 60.0);
			}
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_CINE: settled -- stage up, frame clock starts."));
		}
		ACameraActor* Cam = S.Camera.Get();
		if (!Cam || S.Frame > 600)
		{
			EndCapture(false);
			return false;
		}
		// Drive the camera along the beat.
		for (const FBeat& B : GBeats)
		{
			if (S.Frame >= B.StartFrame && S.Frame < B.EndFrame)
			{
				const float T = float(S.Frame - B.StartFrame) / float(B.EndFrame - B.StartFrame);
				FVector LookAt = FVector::ZeroVector;
				if (B.TargetIndex >= 10 && S.PawnSpots.IsValidIndex(B.TargetIndex - 10)) { LookAt = S.PawnSpots[B.TargetIndex - 10]; }
				else if (B.TargetIndex >= 0 && S.PadSpots.IsValidIndex(B.TargetIndex))   { LookAt = S.PadSpots[B.TargetIndex]; }
				FVector CamLoc;
				if (!LookAt.IsZero())
				{
					// ORBIT the snapped subject (v7 lesson: anchor-relative cams sat far above the
					// real ground). Distance/height are subject-relative; the angle drifts gently.
					const float Deg = FMath::Lerp(B.OrbitDegA, B.OrbitDegB, T);
					const float Rad = FMath::DegreesToRadians(Deg);
					CamLoc = LookAt + FVector(B.OrbitDist * FMath::Cos(Rad), B.OrbitDist * FMath::Sin(Rad), B.OrbitHeight);
					Cam->SetActorLocation(CamLoc);
					Cam->SetActorRotation((LookAt - CamLoc).Rotation());
				}
				else
				{
					CamLoc = GAnchor + FMath::Lerp(B.CamA, B.CamB, T);
					Cam->SetActorLocation(CamLoc);
					Cam->SetActorRotation(B.Rot);
				}
				Cam->GetCameraComponent()->SetFieldOfView(B.Fov);
				break;
			}
		}
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			PC->SetViewTargetWithBlend(Cam, 0.f);
		}
		if (S.bCapture && S.Frame >= 0)
		{
			FScreenshotRequest::RequestScreenshot(
				FString::Printf(TEXT("%s/frame.%04d.png"), *S.OutDir, S.Frame), false, false);
		}
		++S.Frame;
		return true; // keep ticking
	}

	static FAutoConsoleCommandWithWorldAndArgs GCineLoop(
		TEXT("afl.Cine.StartLoop"),
		TEXT("Capture the 10s start loop: afl.Cine.StartLoop [outdir] [stageonly]. Stages pads+pawns, drives the 6 approved beats at fixed 60Hz, dumps 600 numbered frames."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game))
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_CINE: run inside PIE or a -game session on L_ShantyTown."));
				return;
			}
			if (GState.IsValid())
			{
				EndCapture(true);
			}
			GState = MakeShared<FCaptureState>();
			GState->World = World;
			GState->OutDir = Args.Num() > 0 ? Args[0] : FPaths::ProjectSavedDir() / TEXT("CineLoop");
			GState->bCapture = !(Args.Num() > 1 && Args[1].StartsWith(TEXT("stage"), ESearchCase::IgnoreCase));
			IFileManager::Get().MakeDirectory(*GState->OutDir, true);

			GState->RealStart = FPlatformTime::Seconds();
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickCapture), 0.f);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_CINE: %s -> %s"),
				GState->bCapture ? TEXT("CAPTURING 600 frames") : TEXT("STAGE ONLY (no frames)"), *GState->OutDir);
		}));


	// ================= MATCHPLAY DRONE CAPTURE (operator ask: action + overhead + swooping) =========
	// Live bot combat (the experience's own BotFill), a single drone camera cycling four shot types,
	// always aimed at the SMOOTHED action centroid. No loop constraint -- promo footage.

	struct FMatchState
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<ACameraActor> Camera;
		FString OutDir;
		int32 Frame = 0;
		int32 TotalFrames = 2700;
		int32 BotTopUp = 0;
		int32 BotsAdded = 0;
		double RealStart = 0.0;
		bool bRolling = false;
		FVector Centroid = FVector::ZeroVector;
		FVector TargetGoal = FVector::ZeroVector; // cluster recompute lands here; Centroid glides to it
		bool bCentroidInit = false;
		float ArmFrac = 1.f; // spring-arm fraction: snaps in on a blocking hit, eases back out
	};
	static TSharedPtr<FMatchState> GMatch;

	// The action target is the densest CONTROLLED-pawn cluster, not the roster mean -- a BR
	// roster spreads across the whole town (the mean lands in empty lanes) and late-game
	// corpses linger as pawns (the controller filter drops them).
	static FVector ComputeActionTarget(UWorld* World, const FVector& Prev, bool bHavePrev)
	{
		TArray<FVector, TInlineAllocator<64>> P;
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetController() != nullptr)
			{
				P.Add(It->GetActorLocation());
			}
		}
		if (P.Num() == 0)
		{
			return Prev;
		}
		const float ClusterR2 = FMath::Square(1200.f);
		int32 BestIdx = 0;
		float BestScore = -FLT_MAX;
		for (int32 i = 0; i < P.Num(); ++i)
		{
			int32 N = 0;
			for (int32 j = 0; j < P.Num(); ++j)
			{
				if (FVector::DistSquared(P[i], P[j]) < ClusterR2)
				{
					++N;
				}
			}
			// Continuity tiebreak: between equal fights, stay on the one the drone already covers.
			const float Score = N * 10000.f - (bHavePrev ? FVector::Dist(P[i], Prev) * 0.1f : 0.f);
			if (Score > BestScore)
			{
				BestScore = Score;
				BestIdx = i;
			}
		}
		FVector Sum = FVector::ZeroVector;
		int32 N = 0;
		for (const FVector& Q : P)
		{
			if (FVector::DistSquared(P[BestIdx], Q) < ClusterR2)
			{
				Sum += Q;
				++N;
			}
		}
		return Sum / N;
	}

	static void EndMatchCapture()
	{
		FApp::SetUseFixedTimeStep(false);
		if (GMatch.IsValid())
		{
			if (ACameraActor* Cam = GMatch->Camera.Get()) { Cam->Destroy(); }
		}
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_CINE: MATCH capture complete."));
		GMatch.Reset();
	}

	static bool TickMatch(float /*Delta*/)
	{
		if (!GMatch.IsValid())
		{
			return false;
		}
		FMatchState& S = *GMatch;
		UWorld* World = S.World.Get();
		if (!World)
		{
			EndMatchCapture();
			return false;
		}
		const double Elapsed = FPlatformTime::Seconds() - S.RealStart;
		if (!S.bRolling)
		{
			// Settle: controller + 14s, then top-up bots one per tick, then 10s of battle warmup.
			if (!UGameplayStatics::GetPlayerController(World, 0) || Elapsed < 14.0)
			{
				return true;
			}
			if (S.BotsAdded < S.BotTopUp)
			{
				if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
				{
					PC->ConsoleCommand(TEXT("AddPlayerBot"));
				}
				++S.BotsAdded;
				return true;
			}
			if (Elapsed < 26.0)
			{
				return true;
			}
			S.bRolling = true;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			S.Centroid = ComputeActionTarget(World, GAnchor, false);
			S.TargetGoal = S.Centroid;
			S.bCentroidInit = true;
			S.Camera = World->SpawnActor<ACameraActor>(S.Centroid + FVector(0, 0, 900.f), FRotator(-80.f, 0, 0), Params);
			FApp::SetUseFixedTimeStep(true);
			FApp::SetFixedDeltaTime(1.0 / 60.0);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_CINE: MATCH rolling -- %d frames, drone program up."), S.TotalFrames);
		}
		ACameraActor* Cam = S.Camera.Get();
		if (!Cam || S.Frame >= S.TotalFrames)
		{
			EndMatchCapture();
			return false;
		}
		// Smoothed action target: recompute the cluster goal every half second, but GLIDE toward it
		// every frame -- v3 stepped the look-at once per 30 frames, smearing exactly those frames
		// with a one-frame rotation whip (a twice-a-second judder in the encoded reel).
		if (S.Frame % 30 == 0)
		{
			S.TargetGoal = ComputeActionTarget(World, S.Centroid, S.bCentroidInit);
		}
		S.Centroid = FMath::Lerp(S.Centroid, S.TargetGoal, 0.02f);
		// Drone program: 4 shot types, 7s (420-frame) shots, base bearing walks 73 deg per shot.
		const int32 Shot = S.Frame / 420;
		const float T = float(S.Frame % 420) / 420.f;
		const float Base = FMath::DegreesToRadians(Shot * 73.f);
		FVector CamLoc;
		switch (Shot % 4)
		{
		// v3: every path lives above the palm/roof canopy (v1 rammed rooftops, v2's clamp pinched
		// into fronds). Variety comes from radius/speed/pattern, not altitude.
		case 0: // OVERHEAD slow orbit, steep look-down
		{
			const float A = Base + T * 0.9f;
			CamLoc = S.Centroid + FVector(900.f * FMath::Cos(A), 900.f * FMath::Sin(A), 1250.f);
			break;
		}
		case 1: // SWOOP: long dive PAST the fight (lateral offset -- crossing the zenith flips the
		{       // look-at ~180 deg in a second and smears the whole low pass)
			const FVector Dir(FMath::Cos(Base), FMath::Sin(Base), 0.f);
			const FVector Perp(-Dir.Y, Dir.X, 0.f);
			const float Along = (1.f - 2.f * T) * 1400.f;
			const float H = 750.f + (1400.f - 750.f) * FMath::Square(2.f * T - 1.f);
			CamLoc = S.Centroid + Dir * Along + Perp * 700.f + FVector(0, 0, H);
			break;
		}
		case 2: // ACTION ARC: tighter, faster orbit
		{
			const float A = Base + T * 1.2f;
			CamLoc = S.Centroid + FVector(900.f * FMath::Cos(A), 900.f * FMath::Sin(A), 850.f);
			break;
		}
		default: // WIDE establishing orbit
		{
			const float A = Base + T * 0.5f;
			CamLoc = S.Centroid + FVector(1600.f * FMath::Cos(A), 1600.f * FMath::Sin(A), 1400.f);
			break;
		}
		}
		// Spring-arm collision clamp against WORLD-STATIC only (buildings/terrain, never pawns):
		// v1 proved fixed-height orbits below the roofline ram the stilt town's split-level geometry.
		// Guaranteeing the LookAt->camera segment is clear keeps the fight framed at any height.
		const FVector LookAt = S.Centroid + FVector(0, 0, 80.f);
		float DesiredFrac = 1.f;
		{
			FHitResult Hit;
			FCollisionQueryParams Q(SCENE_QUERY_STAT(AFLCineDrone), false);
			FCollisionObjectQueryParams Obj;
			Obj.AddObjectTypesToQuery(ECC_WorldStatic);
			if (World->LineTraceSingleByObjectType(Hit, LookAt, CamLoc, Obj, Q))
			{
				const float Full = FVector::Dist(LookAt, CamLoc);
				// Floor at half arm: a stray palm frond crossing the sight line must never drag the
				// camera into the canopy (the v2 failure) -- this clamp is a safety, not a framing tool.
				DesiredFrac = Full > 1.f ? FMath::Clamp((Hit.Distance - 60.f) / Full, 0.5f, 1.f) : 1.f;
			}
		}
		// Snap in on a hit (never clip through), ease back out when clear.
		S.ArmFrac = DesiredFrac < S.ArmFrac ? DesiredFrac : FMath::Min(1.f, FMath::Lerp(S.ArmFrac, DesiredFrac, 0.12f));
		CamLoc = LookAt + (CamLoc - LookAt) * S.ArmFrac;
		Cam->SetActorLocation(CamLoc);
		Cam->SetActorRotation((LookAt - CamLoc).Rotation());
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			PC->SetViewTargetWithBlend(Cam, 0.f);
		}
		FScreenshotRequest::RequestScreenshot(
			FString::Printf(TEXT("%s/frame.%04d.png"), *S.OutDir, S.Frame), false, false);
		++S.Frame;
		return true;
	}

	static FAutoConsoleCommandWithWorldAndArgs GCineMatch(
		TEXT("afl.Cine.Match"),
		TEXT("Matchplay drone capture over live bots: afl.Cine.Match [outdir] [seconds=45] [topUpBots=0]. Overhead / swoop / action-arc / wide shots tracking the fight."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game))
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_CINE: run Match inside PIE or a -game session."));
				return;
			}
			if (GMatch.IsValid())
			{
				EndMatchCapture();
			}
			GMatch = MakeShared<FMatchState>();
			GMatch->World = World;
			GMatch->OutDir = Args.Num() > 0 ? Args[0] : FPaths::ProjectSavedDir() / TEXT("CineMatch");
			GMatch->TotalFrames = 60 * FMath::Clamp(Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 45, 5, 300);
			GMatch->BotTopUp = FMath::Clamp(Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 0, 0, 64);
			GMatch->RealStart = FPlatformTime::Seconds();
			IFileManager::Get().MakeDirectory(*GMatch->OutDir, true);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickMatch), 0.f);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_CINE: MATCH armed -> %s (%d frames, +%d bots)."),
				*GMatch->OutDir, GMatch->TotalFrames, GMatch->BotTopUp);
		}));

	static FAutoConsoleCommand GCineStop(
		TEXT("afl.Cine.Stop"),
		TEXT("Stop/clear the start-loop capture stage."),
		FConsoleCommandDelegate::CreateLambda([]() { EndCapture(true); }));
}

#endif // !UE_BUILD_SHIPPING
