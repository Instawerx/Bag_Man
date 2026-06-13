// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberComponent.h"

#include "AFLDismember.h"
#include "AFLDismemberTypes.h"
#include "AFLDismemberZoneSet.h"
#include "AFLDismemberedPart.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayEffect.h"
#include "HUD/AFLOverkillMessage.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDismemberComponent)

namespace
{
	// Native-define mirrors the AFLCoreTags.ini tag (Event.Damage.Overkill.AFL) to avoid
	// first-boot race -- same pattern as AFLHitConfirmComponent's TAG_Event_Damage_Confirmed.
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Damage_Overkill_AFL_Dismember, "Event.Damage.Overkill.AFL");
}

UAFLDismemberComponent::UAFLDismemberComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAFLDismemberComponent::BeginPlay()
{
	Super::BeginPlay();

	// Server-authoritative: the overkill broadcast is server-side, and dismemberment is
	// authoritative gameplay. No local-controller gate -- register on the server only.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(this);
		ListenerHandle = MessageSubsystem.RegisterListener(
			TAG_Event_Damage_Overkill_AFL_Dismember,
			this,
			&ThisClass::OnOverkill);
	}
}

void UAFLDismemberComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ListenerHandle.IsValid())
	{
		UGameplayMessageSubsystem::Get(this).UnregisterListener(ListenerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UAFLDismemberComponent::OnOverkill(FGameplayTag Channel, const FAFLOverkillMessage& Payload)
{
	// Victim-side filter: only react if I am the one who was overkilled.
	if (Payload.Target != GetOwner())
	{
		return;
	}

	if (ZoneSet.IsNull())
	{
		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] %s overkilled (bone=%s) but no ZoneSet assigned -- no dismemberment"),
			*GetNameSafe(GetOwner()), *Payload.BoneName.ToString());
		return;
	}

	ResolveAndSever(Payload.BoneName);
}

void UAFLDismemberComponent::ResolveAndSever(FName HitBone)
{
	// AAA: async-load the ZoneSet (soft), then resolve the bone -> row on the game thread.
	// RequestAsyncLoad keeps the asset off the hard-ref graph; the lambda runs once loaded.
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	const FSoftObjectPath ZonePath = ZoneSet.ToSoftObjectPath();

	TWeakObjectPtr<UAFLDismemberComponent> WeakThis(this);
	Streamable.RequestAsyncLoad(ZonePath, FStreamableDelegate::CreateLambda([WeakThis, HitBone, ZonePath]()
	{
		UAFLDismemberComponent* Self = WeakThis.Get();
		if (!Self || !Self->GetOwner() || !Self->GetOwner()->HasAuthority())
		{
			return;
		}

		const UAFLDismemberZoneSet* Set = Cast<UAFLDismemberZoneSet>(ZonePath.ResolveObject());
		if (!Set)
		{
			UE_LOG(LogAFLDismember, Warning, TEXT("[AFLDismember] ZoneSet failed to load -- no dismemberment"));
			return;
		}

		const FAFLDismemberZone* Row = Set->FindZoneForBone(HitBone);
		if (!Row)
		{
			UE_LOG(LogAFLDismember, Verbose,
				TEXT("[AFLDismember] Overkill on %s but bone=%s maps to no zone -- no dismemberment"),
				*GetNameSafe(Self->GetOwner()), *HitBone.ToString());
			return;
		}

		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] ZONE OVERKILL on %s -- bone=%s -> zone=%d severed=%s"),
			*GetNameSafe(Self->GetOwner()), *HitBone.ToString(),
			static_cast<int32>(Row->Zone), *Row->SeveredBone.ToString());

		// Copy the row -- the async prop/GE continuations in SeverZone outlive the
		// array-pointer Row returned by FindZoneForBone.
		Self->SeverZone(*Row);
	}));
}

void UAFLDismemberComponent::SeverZone(const FAFLDismemberZone& Row)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !GetWorld())
	{
		return;
	}

	// (1) Hide the severed bone. PBO_Term scales the bone to 0 (limb visibly disappears) and
	// terminates its physics body so no phantom collision remains. Server-side; shows in
	// single-PIE (server==client). The NetMulticast-replicated HideBone is AFL-0408 (deferred).
	USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMesh)
	{
		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] %s has no SkeletalMeshComponent -- cannot sever"), *GetNameSafe(Owner));
		return;
	}
	SkelMesh->HideBoneByName(Row.SeveredBone, PBO_Term);

	// (2) The severed-bone world transform = where the prop spawns.
	const FTransform SeverXform = SkelMesh->GetSocketTransform(Row.SeveredBone, RTS_World);

	UE_LOG(LogAFLDismember, Display,
		TEXT("[AFLDismember] Bone %s hidden on %s -- spawning prop"),
		*Row.SeveredBone.ToString(), *GetNameSafe(Owner));

	// (2/3) Async-load the (soft) prop class, then spawn at the transform + apply the pop impulse.
	if (!Row.PropClass.IsNull())
	{
		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		const FSoftObjectPath PropPath = Row.PropClass.ToSoftObjectPath();

		// Capture the spawn-time data by value (the row reference does not outlive the lambda).
		TWeakObjectPtr<UAFLDismemberComponent> WeakThis(this);
		const FVector2D ImpulseXY = Row.ImpulseXYRange;
		const float ImpulseZ = Row.ImpulseZ;

		Streamable.RequestAsyncLoad(PropPath, FStreamableDelegate::CreateLambda(
			[WeakThis, PropPath, SeverXform, ImpulseXY, ImpulseZ]()
		{
			UAFLDismemberComponent* Self = WeakThis.Get();
			if (!Self || !Self->GetOwner() || !Self->GetOwner()->HasAuthority() || !Self->GetWorld())
			{
				return;
			}

			UClass* PropClass = Cast<UClass>(PropPath.ResolveObject());
			if (!PropClass)
			{
				UE_LOG(LogAFLDismember, Warning, TEXT("[AFLDismember] prop class failed to load -- no prop"));
				return;
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.Owner = Self->GetOwner();

			AAFLDismemberedPart* Part =
				Self->GetWorld()->SpawnActor<AAFLDismemberedPart>(PropClass, SeverXform, SpawnParams);
			if (Part)
			{
				// Randomized pop -- matches the proven head (+-100 XY, +500 Z by default).
				Part->ApplyPopImpulse(FVector(
					FMath::FRandRange(ImpulseXY.X, ImpulseXY.Y),
					FMath::FRandRange(ImpulseXY.X, ImpulseXY.Y),
					ImpulseZ));
				UE_LOG(LogAFLDismember, Display,
					TEXT("[AFLDismember] Part prop spawned on %s -- pop + roll"),
					*GetNameSafe(Self->GetOwner()));
			}
		}));
	}

	UAbilitySystemComponent* VictimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);

	// (4) Fire the cosmetic cue (replicated via the victim ASC) if the row sets one.
	if (VictimASC && Row.CueTag.IsValid())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location     = SeverXform.GetLocation();
		CueParams.Instigator   = Owner;
		CueParams.SourceObject = Owner;
		VictimASC->ExecuteGameplayCue(Row.CueTag, CueParams);
	}

	// (5) Apply the consequence GE (grants State.Dismembered.<Zone>) if the row sets one --
	// EMPTY for lethal-cosmetic zones (head/torso = death, no surviving consequence). Async-load.
	if (VictimASC && !Row.ConsequenceGE.IsNull())
	{
		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		const FSoftObjectPath GEPath = Row.ConsequenceGE.ToSoftObjectPath();

		TWeakObjectPtr<UAFLDismemberComponent> WeakThis(this);
		TWeakObjectPtr<UAbilitySystemComponent> WeakASC(VictimASC);

		Streamable.RequestAsyncLoad(GEPath, FStreamableDelegate::CreateLambda([WeakThis, WeakASC, GEPath]()
		{
			UAFLDismemberComponent* Self = WeakThis.Get();
			UAbilitySystemComponent* ASC = WeakASC.Get();
			if (!Self || !ASC || !Self->GetOwner() || !Self->GetOwner()->HasAuthority())
			{
				return;
			}

			UClass* GEClass = Cast<UClass>(GEPath.ResolveObject());
			if (!GEClass)
			{
				UE_LOG(LogAFLDismember, Warning, TEXT("[AFLDismember] consequence GE failed to load"));
				return;
			}

			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(Self);
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GEClass, 1.0f, Context);
			if (Spec.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				UE_LOG(LogAFLDismember, Display,
					TEXT("[AFLDismember] consequence GE applied to %s"), *GetNameSafe(Self->GetOwner()));
			}
		}));
	}
}
