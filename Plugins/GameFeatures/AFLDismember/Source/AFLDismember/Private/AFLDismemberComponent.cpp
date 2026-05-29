// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLDismemberComponent.h"

#include "AFLDismember.h"
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
		// S4-04 SCOPE: detection only. The detach (head physics actor +
		// bone hide) is S4-05. This log is the PIE-verifiable proof that
		// the server-side victim-side listener fires on a head-overkill.
		UE_LOG(LogAFLDismember, Warning,
			TEXT("[AFLDismember] HEAD OVERKILL detected on %s (bone=%s, magnitude=%.1f) -- detach is S4-05"),
			*GetNameSafe(GetOwner()),
			*Payload.BoneName.ToString(),
			Payload.Magnitude);
	}
	else
	{
		UE_LOG(LogAFLDismember, Verbose,
			TEXT("[AFLDismember] Overkill on %s but bone=%s (not head) -- no head-pop"),
			*GetNameSafe(GetOwner()),
			*Payload.BoneName.ToString());
	}
}
