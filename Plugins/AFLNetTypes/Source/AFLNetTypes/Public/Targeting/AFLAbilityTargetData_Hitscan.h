// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/HitResult.h"

#include "AFLAbilityTargetData_Hitscan.generated.h"

class FArchive;
class UPackageMap;


/**
 * FAFLAbilityTargetData_Hitscan
 *
 * AFL-0106 client-built hitscan payload. The firing client traces locally from
 * the camera, packs the resulting FHitResult plus the claimed view origin and
 * aim direction into this struct, and ships it to the server through
 * UAbilitySystemBlueprintLibrary::ServerSetReplicatedTargetData. The server's
 * authoritative damage path reads it back through the OnTargetDataReady
 * delegate and applies the damage GE.
 *
 * Why a custom subclass (vs FGameplayAbilityTargetData_SingleTargetHit):
 *   - We must replicate ClaimedViewOrigin + ClaimedAimDirection. The server is
 *     forbidden from reading the player's viewpoint (master doc §7, AFL-0215
 *     lint), so the claimed-origin is the ONLY source of truth for §7.2 layer-2
 *     geometry validation (distance, origin proximity, angle from pawn forward)
 *     and for the AFL-0211 lag-comp re-trace.
 *   - NetSerialize quantises the floats to keep the per-shot replicated payload
 *     tight (matches Lyra's existing SingleTargetHit overload pattern).
 *   - AddTargetDataToContext copies the claimed origin into the effect context
 *     so downstream listeners (UI hit markers, telemetry) can read it without
 *     plumbing the full target-data struct through their delegates.
 *
 * WHY THIS LIVES IN AFLNetTypes (an always-loaded, non-GameFeature module) and
 * NOT in the AFLCombat GameFeature where it originated:
 *   FGameplayAbilityTargetDataHandle replicates a custom target-data struct's
 *   TYPE as a one-byte index into FNetSerializeScriptStructCache. That cache is
 *   built ONCE, lazily, at first ASC-globals access, by walking every loaded
 *   UScriptStruct deriving from FGameplayAbilityTargetData (AbilitySystemGlobals.cpp
 *   InitForType) -- and is never rebuilt. A struct that lives in a GameFeature
 *   (ExplicitlyLoaded, activated at experience-load time) is not loaded when the
 *   cache is first built, so it is absent / differently-indexed on one endpoint.
 *   The result over a real connection: the receiver can't resolve the index
 *   ("Could not find script struct at idx N"), the RPC byte-stream mismatches,
 *   and the connection is dropped. Single-client (listen-host, client==server)
 *   never serializes over a wire, so it never surfaces. Lyra avoids this by
 *   keeping FLyraGameplayAbilityTargetData_SingleTargetHit in the always-loaded
 *   LyraGame module; we keep AFL's equivalent here so the AFLCombat GameFeature
 *   and Lyra base both stay clean. Every networked AFL weapon's net-types go here.
 *
 * Server validation (schema/bounds/geometry/lag-comp) lands in AFL-0211 and
 * AFL-0213. This task wires the transport and a telemetry stub only.
 */
USTRUCT()
struct AFLNETTYPES_API FAFLAbilityTargetData_Hitscan : public FGameplayAbilityTargetData_SingleTargetHit
{
	GENERATED_BODY()

	FAFLAbilityTargetData_Hitscan() = default;

	/** Camera-space origin at the moment the client predicted the shot. */
	UPROPERTY()
	FVector_NetQuantize ClaimedViewOrigin = FVector::ZeroVector;

	/** Unit aim direction at the moment the client predicted the shot. */
	UPROPERTY()
	FVector_NetQuantizeNormal ClaimedAimDirection = FVector::ForwardVector;

	/**
	 * Client-measured angular velocity of the aim direction (deg/sec) at the
	 * moment of fire. AFL-0213 telemetry compares this against the per-pawn
	 * budget; values above the budget produce a `hitscan_reject reason=ang`
	 * log line but DO NOT discard the shot until UAFLLagCompensationWorldSubsystem
	 * (AFL-0211) is in. Default 0 = no measurement; treat as below budget.
	 */
	UPROPERTY()
	float AimAngularVelocityDegPerSec = 0.0f;

	virtual void AddTargetDataToContext(FGameplayEffectContextHandle& Context, bool bIncludeActorArray) const override;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FAFLAbilityTargetData_Hitscan::StaticStruct();
	}

	virtual FString ToString() const override
	{
		return TEXT("FAFLAbilityTargetData_Hitscan");
	}
};

template<>
struct TStructOpsTypeTraits<FAFLAbilityTargetData_Hitscan> : public TStructOpsTypeTraitsBase2<FAFLAbilityTargetData_Hitscan>
{
	enum
	{
		// Required for FGameplayAbilityTargetDataHandle net replication to
		// dispatch the right serializer for our subclass. Without this the
		// handle falls back to the parent struct and loses our payload.
		WithNetSerializer = true,
	};
};
