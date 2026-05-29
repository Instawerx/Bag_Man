// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberComponent.h"

#include "AFLDismember.h"
#include "AFLDismemberedHead.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "HUD/AFLOverkillMessage.h"
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLDismemberComponent)

namespace
{
	// Native-define mirrors the AFLCoreTags.ini tag (Event.Damage.Overkill.AFL)
	// to avoid first-boot race -- same pattern as AFLHitConfirmComponent's
	// TAG_Event_Damage_Confirmed. File-local suffix per HYG-001/002.
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Damage_Overkill_AFL_Dismember, "Event.Damage.Overkill.AFL");
}

UAFLDismemberComponent::UAFLDismemberComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Defaulted in ctor so the slice ships without BP wiring; BP children
	// may override to a custom head asset later (real head mesh = polish).
	HeadActorClass = AAFLDismemberedHead::StaticClass();
}

void UAFLDismemberComponent::BeginPlay()
{
	Super::BeginPlay();

	// Server-authoritative: the overkill broadcast is server-side, and
	// dismemberment is authoritative gameplay. No local-controller gate
	// (unlike UAFLHitConfirmComponent) -- register on the server only.
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

	// Head-zone check. S4-A0 confirmed the Manny head bone is "head".
	if (Payload.BoneName == FName(TEXT("head")))
	{
		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] HEAD OVERKILL detected on %s (bone=%s, magnitude=%.1f) -- detaching head"),
			*GetNameSafe(GetOwner()),
			*Payload.BoneName.ToString(),
			Payload.Magnitude);

		// S4-05a: detach the head. Reach the owner's skeletal mesh (registry
		// lookup -- works for the dummy's private Mesh subobject and for
		// ALyraCharacter pawns later) and hide the head bone. HideBoneByName
		// scales the bone to 0 (head visibly disappears); PBO_Term also
		// terminates its physics body so no phantom head-capsule collision
		// remains. S4-05b spawns the rolling head prop at the head transform.
		//
		// Server-side, shows in single-player PIE (server==client). The
		// multiplayer-correct version (NetMulticast wrapper so clients also
		// run HideBoneByName) is deferred to when dismemberment goes on real
		// pawns in real net play -- HideBoneByName does NOT replicate.
		if (USkeletalMeshComponent* SkelMesh =
				GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
		{
			SkelMesh->HideBoneByName(FName(TEXT("head")), PBO_Term);

			UE_LOG(LogAFLDismember, Display,
				TEXT("[AFLDismember] Head bone hidden on %s (S4-05a) -- rolling prop is S4-05b"),
				*GetNameSafe(GetOwner()));

			// S4-05b: spawn the rolling head prop at the head's world transform.
			// Server-side; shows in single-player PIE (server==client). The
			// physics sphere simulates + auto-destroys after 5s. Multiplayer-
			// correct replicated physics is deferred (snapshot replication is
			// good enough cosmetically; deterministic physics is a later lift).
			const FTransform HeadXform =
				SkelMesh->GetSocketTransform(FName(TEXT("head")), RTS_World);

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.Owner = GetOwner();

			UClass* HeadClass = HeadActorClass.Get();
			if (HeadClass && GetWorld())
			{
				AAFLDismemberedHead* HeadActor =
					GetWorld()->SpawnActor<AAFLDismemberedHead>(HeadClass, HeadXform, SpawnParams);
				if (HeadActor && HeadActor->GetSphereMesh())
				{
					HeadActor->GetSphereMesh()->AddImpulse(
						FVector(FMath::FRandRange(-100.f, 100.f),
								FMath::FRandRange(-100.f, 100.f),
								500.f),
						NAME_None, /*bVelChange=*/false);
					UE_LOG(LogAFLDismember, Display,
						TEXT("[AFLDismember] Head prop spawned on %s (S4-05b) -- pop + roll"),
						*GetNameSafe(GetOwner()));
				}
			}
		}
		else
		{
			UE_LOG(LogAFLDismember, Warning,
				TEXT("[AFLDismember] %s has no SkeletalMeshComponent -- cannot detach head"),
				*GetNameSafe(GetOwner()));
		}
	}
	else
	{
		UE_LOG(LogAFLDismember, Verbose,
			TEXT("[AFLDismember] Overkill on %s but bone=%s (not head) -- no head-pop"),
			*GetNameSafe(GetOwner()),
			*Payload.BoneName.ToString());
	}
}
