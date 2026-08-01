// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

#include "AFLHolsterComponent.generated.h"

class UGameplayEffect;
class ULyraEquipmentManagerComponent;
class USkeletalMeshComponent;
class USceneComponent;

/**
 * UAFLHolsterComponent  (Movement Overhaul — Phase 3: the holster SSOT)
 *
 * Single source of truth for "is the weapon holstered". Replaces the two ad-hoc "hide the weapon actor" hacks
 * (UAFLClimbMovementComponent + UAFLInteractionComponent) that today stomp each other because each tracks its
 * own hidden-actor list. Here a **reason refcount** composes them: any of climb / grab / melee / manual can
 * hold a HolsterReason.* tag; the weapon stays holstered until ALL reasons clear.
 *
 * Server-authoritative: AddHolsterReason/RemoveHolsterReason mutate the reason set on the server, which flips
 * the replicated bHolstered, applies/removes GE_AFL_State_Holstered (grants State.Weapon.Holstered -> Fire/ADS/
 * Reload block on it), and reattaches the spawned weapon actor(s) hand<->back socket. Clients mirror the
 * reattach via OnRep_Holstered (cosmetic). Unlike the old hide-hacks, the weapon is VISIBLE on the back.
 */
UCLASS(ClassGroup = (AFL), Blueprintable, meta = (BlueprintSpawnableComponent))
class AFLMOVEMENT_API UAFLHolsterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAFLHolsterComponent();

	/** Server-auth: hold a holster reason (HolsterReason.*). Weapon stays holstered while ANY reason is held. */
	UFUNCTION(BlueprintCallable, Category = "AFL|Holster") void AddHolsterReason(FGameplayTag Reason);
	UFUNCTION(BlueprintCallable, Category = "AFL|Holster") void RemoveHolsterReason(FGameplayTag Reason);

	UFUNCTION(BlueprintPure, Category = "AFL|Holster") bool IsHolstered() const { return bHolstered; }

	/**
	 * Fired on the server AND every client whenever the holster state changes (after the weapon actor reattach).
	 * Implement in BPC_AFL_Holster to relink the mesh's anim layer — weapon layer <-> ABP_UnarmedAnimLayers — so
	 * the HANDS match the holstered weapon (Lyra keys the pose off the linked layer, not the actor), and/or to
	 * play a draw/holster montage. Cosmetic; runs on all machines so every proxy poses correctly.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "AFL|Holster")
	void OnHolsterStateChanged(bool bNowHolstered);

	/** Character-mesh socket the weapon reattaches to when holstered (v1: all weapons -> back; hip-for-pistol later). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Holster") FName BackSocket = TEXT("weapon_holster_back");

	/** Infinite GE granting State.Weapon.Holstered (blocks Fire/ADS/Reload). BP child sets it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AFL|Holster") TSubclassOf<UGameplayEffect> HolsteredEffectClass;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Holstered)
	bool bHolstered = false;

	UFUNCTION()
	void OnRep_Holstered();

	/** Server: recompute bHolstered from the reason set; on change, apply/remove the GE + reattach. */
	void UpdateHolsterState();

	/** Client+server: reattach the spawned weapon actor(s) to the back socket (holster) or their cached hand attach (draw). */
	void ApplyHolsterVisual(bool bNowHolstered);

	ULyraEquipmentManagerComponent* GetEquipmentManager() const;
	USkeletalMeshComponent* GetOwnerMesh() const;

	/** Reasons currently holding the weapon holstered (server only). */
	TSet<FGameplayTag> HolsterReasons;

	/** Handle to the applied State.Weapon.Holstered GE (server). */
	FActiveGameplayEffectHandle HolsteredGEHandle;

	/** Original attach of each weapon actor, cached at holster so the draw restores it exactly. */
	struct FCachedAttach
	{
		TWeakObjectPtr<USceneComponent> Parent;
		FName Socket = NAME_None;
		FTransform RelativeTransform = FTransform::Identity;
	};
	TMap<TWeakObjectPtr<AActor>, FCachedAttach> CachedAttachments;
};
