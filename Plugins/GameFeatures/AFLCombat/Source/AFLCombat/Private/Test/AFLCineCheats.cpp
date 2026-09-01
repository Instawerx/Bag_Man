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
#include "Misc/App.h"
#include "UnrealClient.h"
#include "UObject/SoftObjectPath.h"

namespace AFLCine
{
	struct FBeat
	{
		int32 StartFrame;
		int32 EndFrame;
		FVector CamA;      // camera start
		FVector CamB;      // camera end (per-beat drift/push-in)
		FRotator Rot;
		float Fov;
	};

	// The approved storyboard, camera moves around the ShantyTown core (BR-spawn band, z ~4000).
	static const FBeat GBeats[] = {
		{   0, 120, { -2600, -2600, 4420 }, { -2200, -2200, 4380 }, {  -8,  45, 0 }, 70.f }, // wide firefight
		{ 120, 228, {   980,   980, 4090 }, {   890,   900, 4075 }, {  -2, 210, 0 }, 35.f }, // IRONICS hero
		{ 228, 312, {  1640,  1340, 4070 }, {  1580,  1300, 4060 }, {  -4, 150, 0 }, 28.f }, // RIPSAW hero
		{ 312, 408, {  -940,  1840, 4110 }, {  -860,  1760, 4095 }, {  -6, 300, 0 }, 40.f }, // SIMULARENT + ARIA
		{ 408, 504, {   340, -1440, 4050 }, {   260, -1340, 4045 }, {   2,  80, 0 }, 55.f }, // hand-cannon alley
		{ 504, 600, {  2240,  -640, 4130 }, { -2600, -2600, 4420 }, { -10, 200, 0 }, 33.f }, // SCARLETT -> loop pullback
	};

	// The stage: spawn-pad hero subjects (REAL weapons on the game's own display fixtures) + idle pawns.
	struct FPadSpawn { FVector Loc; float Yaw; const TCHAR* CosmeticId; };
	static const FPadSpawn GPads[] = {
		{ { 1500, 1180, 4020 },  150.f, TEXT("AFL.Weapon.Ripsaw") },
		{ { -760, 1620, 4040 },  300.f, TEXT("AFL.Weapon.Aria") },
		{ { 2100, -780, 4080 },  200.f, TEXT("AFL.Weapon.Scarlett") },
		{ {  180, -1240, 4000 },  80.f, TEXT("AFL.Weapon.HandCannon.IRONICS.XT") },
	};
	static const FVector GPawns[] = { { 830, 830, 4020 }, { -980, 1960, 4040 }, { -2000, -1800, 4000 }, { 500, -1100, 4000 } };

	struct FCaptureState
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<ACameraActor> Camera;
		TArray<TWeakObjectPtr<AActor>> Staged;
		FString OutDir;
		int32 Frame = 0;
		bool bCapture = true;
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
			AActor* Pad = World->SpawnActor<AActor>(PadClass, P.Loc, FRotator(0.f, P.Yaw, 0.f), Params);
			if (Pad)
			{
				FProperty* IdProp = PadClass->FindPropertyByName(TEXT("CosmeticId"));
				if (FNameProperty* NameProp = CastField<FNameProperty>(IdProp))
				{
					NameProp->SetPropertyValue_InContainer(Pad, FName(P.CosmeticId));
				}
				State.Staged.Add(Pad);
				UE_LOG(LogAFLCombat, Log, TEXT("AFL_CINE: staged pad %s."), P.CosmeticId);
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
			AActor* Pawn = World->SpawnActor<AActor>(HeroClass, Loc, FRotator(0.f, (PawnYaw++ * 97.f), 0.f), Params);
			if (Pawn)
			{
				State.Staged.Add(Pawn);
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
		ACameraActor* Cam = S.Camera.Get();
		if (!World || !Cam || S.Frame > 600)
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
				Cam->SetActorLocation(FMath::Lerp(B.CamA, B.CamB, T));
				Cam->SetActorRotation(B.Rot);
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
			if (!World || World->WorldType != EWorldType::PIE)
			{
				UE_LOG(LogAFLCombat, Warning, TEXT("AFL_CINE: run inside PIE on L_ShantyTown."));
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

			BuildStage(World, *GState);

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GState->Camera = World->SpawnActor<ACameraActor>(AFLCine::GBeats[0].CamA, AFLCine::GBeats[0].Rot, Params);

			if (GState->bCapture)
			{
				FApp::SetUseFixedTimeStep(true);
				FApp::SetFixedDeltaTime(1.0 / 60.0);
			}
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickCapture), 0.f);
			UE_LOG(LogAFLCombat, Display, TEXT("AFL_CINE: %s -> %s"),
				GState->bCapture ? TEXT("CAPTURING 600 frames") : TEXT("STAGE ONLY (no frames)"), *GState->OutDir);
		}));

	static FAutoConsoleCommand GCineStop(
		TEXT("afl.Cine.Stop"),
		TEXT("Stop/clear the start-loop capture stage."),
		FConsoleCommandDelegate::CreateLambda([]() { EndCapture(true); }));
}

#endif // !UE_BUILD_SHIPPING
