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

namespace
{
	// LEFT-ANCHORED parse of "Tier_League_Ruleset_Venue_Bracket". The bracket is the REMAINDER joined back, so
	// a BR bracket that carries an embedded underscore (BR_9 / BR_20 / BR_36) survives whole -- the old fixed
	// 5-token split turned BR_9 into a 6-token cell and rejected every Battle Royale queue. Returns false for a
	// staked (Volts/Watts) or malformed cell -- only unstaked LeaguePlay gets an offline fallback.
	bool ParseLeagueCell(const FString& QueueId, FString& OutLeague, FString& OutRuleset, FString& OutVenue, FString& OutBracket)
	{
		TArray<FString> P;
		QueueId.ParseIntoArray(P, TEXT("_"));
		if (P.Num() < 5 || P[0] != TEXT("LeaguePlay"))
		{
			return false;
		}
		OutLeague  = P[1];   // ProMod / Haywire
		OutRuleset = P[2];   // MatchPlay / BattleRoyale
		OutVenue   = P[3];   // Arena / Map
		OutBracket.Reset();
		for (int32 i = 4; i < P.Num(); ++i)
		{
			if (i > 4) { OutBracket += TEXT("_"); }
			OutBracket += P[i];
		}
		return !OutLeague.IsEmpty() && !OutRuleset.IsEmpty() && !OutVenue.IsEmpty() && !OutBracket.IsEmpty();
	}

	// Playlist-name token for a bracket: MatchPlay brackets pass through ("2v2"); BR brackets drop the
	// underscore ("BR_9" -> "BR9") to match the authored DA_AFL_ShantyTown_BR9_* / DA_AFL_ShantyTown_2v2_* names.
	FString PlaylistBracketToken(const FString& Bracket)
	{
		return Bracket.Replace(TEXT("_"), TEXT(""));
	}
}

FName UAFLLeagueBotFallbackSubsystem::ShantyTownFallbackFor(const FString& QueueId)
{
	FString League, Ruleset, Venue, Bracket;
	if (!ParseLeagueCell(QueueId, League, Ruleset, Venue, Bracket))
	{
		return NAME_None;
	}
	// ShantyTown carries an authored playlist for EVERY bracket+league -- 12 MatchPlay (1v1..8v8 x
	// ProMod/Haywire) and 3 BR (BR9/BR20/BR36 x league). This is the guaranteed fill for any uncovered
	// venue+bracket (operator ruling 2026-09-05: uncovered Arena brackets play on ShantyTown).
	return FName(*FString::Printf(TEXT("DA_AFL_ShantyTown_%s_%s"), *PlaylistBracketToken(Bracket), *League));
}

FName UAFLLeagueBotFallbackSubsystem::ResolvePlaylistAsset(const FString& QueueId)
{
	// QueueId: Tier_League_Ruleset_Venue_Bracket, e.g. LeaguePlay_ProMod_MatchPlay_Arena_2v2 or
	// LeaguePlay_Haywire_BattleRoyale_Map_BR_9. The client cannot read the server registry (R18 keeps
	// map/experience off the /queues wire), so this maps the cell to the authored ULyraUserFacingExperience
	// Definition. Returns the PREFERRED playlist (venue-specific Arena where authored); the caller falls back
	// to ShantyTownFallbackFor when it does not load, so EVERY unstaked LEAGUE cell fills.
	FString League, Ruleset, Venue, Bracket;
	if (!ParseLeagueCell(QueueId, League, Ruleset, Venue, Bracket))
	{
		return NAME_None;
	}

	// Battle Royale has only a ShantyTown (Map) venue -- straight to the guaranteed playlist.
	if (Ruleset == TEXT("BattleRoyale"))
	{
		return ShantyTownFallbackFor(QueueId);
	}

	// MatchPlay Arena has authored playlists only for 3v3 (Arena01) and 5v5/8v8 (Arena04). 1v1/2v2/4v4 -- and
	// any league variant that is unauthored (e.g. Arena-3v3-Haywire) -- fall through to ShantyTown below.
	if (Ruleset == TEXT("MatchPlay") && Venue == TEXT("Arena"))
	{
		if (Bracket == TEXT("3v3"))
		{
			return FName(*FString::Printf(TEXT("DA_AFL_Arena01_3v3_%s"), *League));
		}
		if (Bracket == TEXT("5v5") || Bracket == TEXT("8v8"))
		{
			return FName(*FString::Printf(TEXT("DA_AFL_Arena04_%s_%s"), *Bracket, *League));
		}
	}

	// MatchPlay Map, or an uncovered Arena bracket: the ShantyTown playlist of this bracket+league.
	return ShantyTownFallbackFor(QueueId);
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
	auto LoadPlaylist = [&AM](const FName Name) -> ULyraUserFacingExperienceDefinition*
	{
		if (Name.IsNone())
		{
			return nullptr;
		}
		const FPrimaryAssetId Id(FPrimaryAssetType(TEXT("LyraUserFacingExperienceDefinition")), Name);
		UObject* Obj = AM.GetPrimaryAssetObject(Id);
		if (!Obj)
		{
			const FSoftObjectPath Path = AM.GetPrimaryAssetPath(Id);
			Obj = Path.IsValid() ? Path.TryLoad() : nullptr;
		}
		return Cast<ULyraUserFacingExperienceDefinition>(Obj);
	};

	ULyraUserFacingExperienceDefinition* Playlist = LoadPlaylist(PlaylistName);
	if (!Playlist)
	{
		// UNIVERSAL SHANTYTOWN FALLBACK (ruling 1): an uncovered Arena bracket (1v1/2v2/4v4) or a missing
		// venue-specific league variant (e.g. Arena-3v3-Haywire) still fills -- host the guaranteed ShantyTown
		// playlist of this bracket+league instead of stranding the player.
		const FName ShantyFallback = ShantyTownFallbackFor(QueueId);
		if (ShantyFallback != PlaylistName)
		{
			Playlist = LoadPlaylist(ShantyFallback);
			if (Playlist)
			{
				UE_LOG(LogAFLGameCore, Log,
					TEXT("AFL_FALLBACK: %s -- preferred playlist %s unavailable, falling back to ShantyTown %s."),
					*QueueId, *PlaylistName.ToString(), *ShantyFallback.ToString());
			}
		}
	}
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
