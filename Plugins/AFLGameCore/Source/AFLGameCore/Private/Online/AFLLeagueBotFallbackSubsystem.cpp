// Copyright C12 AI Gaming. All Rights Reserved.

#include "Online/AFLLeagueBotFallbackSubsystem.h"

#include "AFLGameCore.h"                                   // LogAFLGameCore
#include "AFLMatchmakingSubsystem.h"                       // OnLeagueFallbackDue / NotifyFallbackFailed (AFLOnline)
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"                                  // UWorld::ServerTravel (offline host)
#include "GameModes/LyraUserFacingExperienceDefinition.h" // the playlist asset (LyraGame)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLeagueBotFallbackSubsystem)

void UAFLLeagueBotFallbackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// InitializeDependency forces the matchmaking subsystem to exist + be initialized before we bind, so the
	// bind cannot miss it to subsystem-creation ordering.
	UAFLMatchmakingSubsystem* MM = Cast<UAFLMatchmakingSubsystem>(
		Collection.InitializeDependency(UAFLMatchmakingSubsystem::StaticClass()));
	if (MM)
	{
		FallbackHandle = MM->OnLeagueFallbackDue.AddUObject(this, &UAFLLeagueBotFallbackSubsystem::HandleLeagueFallbackDue);
	}
}

void UAFLLeagueBotFallbackSubsystem::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UAFLMatchmakingSubsystem* MM = GI->GetSubsystem<UAFLMatchmakingSubsystem>())
		{
			MM->OnLeagueFallbackDue.Remove(FallbackHandle);
		}
	}
	FallbackHandle.Reset();
	Super::Deinitialize();
}

FName UAFLLeagueBotFallbackSubsystem::ResolvePlaylistAsset(const FString& QueueId)
{
	// QueueId: Tier_League_Ruleset_Venue_Bracket, e.g. LeaguePlay_ProMod_MatchPlay_Map_3v3. The client cannot
	// read the server registry (R18 keeps map/experience off the /queues wire), so this maps the cell to the
	// matching authored ULyraUserFacingExperienceDefinition (which carries the map + experience). A missing
	// asset is handled by the caller (the load returns null), so an over-broad guess is safe. LEAGUE MatchPlay
	// team cells only -- BR and staked never reach here.
	TArray<FString> P;
	QueueId.ParseIntoArray(P, TEXT("_"));
	if (P.Num() != 5 || P[0] != TEXT("LeaguePlay") || P[2] != TEXT("MatchPlay"))
	{
		return NAME_None;
	}
	const FString& League = P[1];  // ProMod / Haywire
	const FString& Venue = P[3];    // Arena / Map
	const FString& Bracket = P[4];  // 1v1 .. 8v8

	if (Venue == TEXT("Map"))
	{
		// Map venue -> ShantyTown; one playlist per bracket+league (all 12 authored).
		return FName(*FString::Printf(TEXT("DA_AFL_ShantyTown_%s_%s"), *Bracket, *League));
	}
	if (Venue == TEXT("Arena"))
	{
		// Arena's concrete map varies by bracket: 3v3 -> Arena01, 5v5/8v8 -> Arena04. Others have no playlist.
		if (Bracket == TEXT("3v3"))
		{
			return FName(*FString::Printf(TEXT("DA_AFL_Arena01_3v3_%s"), *League));
		}
		if (Bracket == TEXT("5v5") || Bracket == TEXT("8v8"))
		{
			return FName(*FString::Printf(TEXT("DA_AFL_Arena04_%s_%s"), *Bracket, *League));
		}
	}
	return NAME_None;
}

void UAFLLeagueBotFallbackSubsystem::HandleLeagueFallbackDue(const FString& QueueId)
{
	UGameInstance* GI = GetGameInstance();
	UAFLMatchmakingSubsystem* MM = GI ? GI->GetSubsystem<UAFLMatchmakingSubsystem>() : nullptr;

	auto Fail = [MM](const FText& Reason)
	{
		UE_LOG(LogAFLGameCore, Warning, TEXT("AFL_FALLBACK: %s"), *Reason.ToString());
		if (MM)
		{
			MM->NotifyFallbackFailed(Reason);
		}
	};

	const FName PlaylistName = ResolvePlaylistAsset(QueueId);
	if (PlaylistName.IsNone())
	{
		Fail(NSLOCTEXT("AFL", "FallbackNoPlaylist",
			"No offline match is available for this queue yet. Try another, or wait for live players."));
		return;
	}

	// Playlists are scanned PrimaryAssets (Config/DefaultGame.ini) and AlwaysCook, so they resolve in a
	// packaged client. Load synchronously -- the player is already waiting on a spinner.
	UAssetManager& AM = UAssetManager::Get();
	const FPrimaryAssetId PlaylistId(FPrimaryAssetType(TEXT("LyraUserFacingExperienceDefinition")), PlaylistName);
	UObject* Obj = AM.GetPrimaryAssetObject(PlaylistId);
	if (!Obj)
	{
		const FSoftObjectPath Path = AM.GetPrimaryAssetPath(PlaylistId);
		Obj = Path.IsValid() ? Path.TryLoad() : nullptr;
	}
	ULyraUserFacingExperienceDefinition* Playlist = Cast<ULyraUserFacingExperienceDefinition>(Obj);
	if (!Playlist)
	{
		Fail(FText::Format(NSLOCTEXT("AFL", "FallbackNoAsset", "No offline match backs {0} yet."),
			FText::FromString(QueueId)));
		return;
	}

	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		Fail(NSLOCTEXT("AFL", "FallbackNoWorld", "Could not start an offline match right now."));
		return;
	}

	// Resolve the map package from the playlist's MapID (a "Map" primary asset).
	const FString MapPackage = AM.GetPrimaryAssetPath(Playlist->MapID).GetLongPackageName();
	if (MapPackage.IsEmpty())
	{
		Fail(FText::Format(NSLOCTEXT("AFL", "FallbackNoMap", "No map backs {0} yet."), FText::FromString(QueueId)));
		return;
	}

	// Build an OFFLINE standalone travel URL -- no ?listen, so no remote clients and no S12-E PreLogin. This
	// mirrors UAFLGameLiftHostSubsystem's ServerTravel host (the AFL-canonical host path) rather than
	// CommonSession, keeping this self-contained in AFLGameCore with no CommonUser dependency and no reliance
	// on the (unexported) ULyraUserFacingExperienceDefinition::CreateHostingRequest. ?Experience= is where
	// LyraGameMode reads the gameplay experience; the playlist's ExtraArgs carry FieldSize/District/etc.
	FString TravelURL = MapPackage;
	TravelURL += FString::Printf(TEXT("?Experience=%s"), *Playlist->ExperienceID.PrimaryAssetName.ToString());
	for (const TPair<FString, FString>& Arg : Playlist->ExtraArgs)
	{
		if (Arg.Key.Equals(TEXT("Experience"), ESearchCase::IgnoreCase))
		{
			continue;  // Experience is set explicitly above; never double it
		}
		TravelURL += FString::Printf(TEXT("?%s=%s"), *Arg.Key, *Arg.Value);
	}

	// Standalone frontend world -> ServerTravel replaces it with the match map; UAFLBotFillComponent fills
	// the field on the non-authoritative host.
	UE_LOG(LogAFLGameCore, Log, TEXT("AFL_FALLBACK: hosting offline bot match for %s -> %s"), *QueueId, *TravelURL);
	World->ServerTravel(TravelURL, /*bAbsolute*/ true);
}
