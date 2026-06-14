// Copyright C12 AI Gaming. AFL / BAG MAN.
#include "AFLCueNotify_ZoneSever.h"

#include "AFLDismemberCosmeticTarget.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCueNotify_ZoneSever)

namespace
{
	// [SV]/[CL1]/[CL2] net-mode tag (the AFLInteractionTestHarness pattern) -- proves OnActive fired on
	// the REMOTE client cold, not just the server. UE::GetPlayInEditorID() (NOT deprecated GPlayInEditorID).
	FString ZoneSeverNetTag(const UObject* WorldCtx)
	{
		const UWorld* W = WorldCtx ? WorldCtx->GetWorld() : nullptr;
		if (!W) { return TEXT("[??]"); }
		switch (W->GetNetMode())
		{
		case NM_DedicatedServer: return TEXT("[SV]");
		case NM_ListenServer:    return TEXT("[SV]");
		case NM_Client:          return FString::Printf(TEXT("[CL%d]"), static_cast<int32>(UE::GetPlayInEditorID()));
		case NM_Standalone:      return TEXT("[SP]");
		default:                 return TEXT("[??]");
		}
	}

	// Parse the zone LEAF from GameplayCue.Combat.Dismember.State.<Leaf> (e.g. -> "Head"). The component
	// maps leaf -> zone on its side (FName-based seam -> AFLVFX needs no AFLCore / EAFLBodyZone).
	FName ZoneLeafFromTag(const FGameplayTag& Tag)
	{
		if (!Tag.IsValid())
		{
			return NAME_None;
		}
		const FString Full = Tag.ToString();
		int32 DotIdx;
		if (Full.FindLastChar(TEXT('.'), DotIdx))
		{
			return FName(*Full.RightChop(DotIdx + 1));
		}
		return FName(*Full);
	}

	FGameplayTag ResolveCueTag(const FGameplayCueParameters& Parameters)
	{
		if (Parameters.OriginalTag.IsValid()) { return Parameters.OriginalTag; }
		if (Parameters.MatchedTagName.IsValid()) { return Parameters.MatchedTagName; }
		return FGameplayTag();
	}

	// Find the component on MyTarget implementing IAFLDismemberCosmeticTarget + route the hide/restore --
	// the EXACT beam pattern (ImplementsInterface + the Execute_ thunk; no concrete-type dependency).
	void RouteToCosmeticTarget(AActor* MyTarget, const FGameplayCueParameters& Parameters, bool bHide)
	{
		if (!MyTarget)
		{
			return;
		}
		const FName Leaf = ZoneLeafFromTag(ResolveCueTag(Parameters));
		const FString Net = ZoneSeverNetTag(MyTarget);

		for (UActorComponent* Comp : MyTarget->GetComponents())
		{
			if (Comp && Comp->GetClass()->ImplementsInterface(UAFLDismemberCosmeticTarget::StaticClass()))
			{
				if (bHide)
				{
					IAFLDismemberCosmeticTarget::Execute_ApplyZoneHideByLeaf(Comp, Leaf);
				}
				else
				{
					IAFLDismemberCosmeticTarget::Execute_ApplyZoneRestoreByLeaf(Comp, Leaf);
				}
				UE_LOG(LogTemp, Display,
					TEXT("%s ZoneSever_VFX %s leaf=%s -> interface %s on %s"),
					*Net, bHide ? TEXT("OnActive") : TEXT("OnRemove"), *Leaf.ToString(),
					bHide ? TEXT("hide") : TEXT("restore"), *GetNameSafe(MyTarget));
				return;
			}
		}
		UE_LOG(LogTemp, Warning,
			TEXT("%s ZoneSever_VFX %s leaf=%s -- no IAFLDismemberCosmeticTarget component on %s"),
			*Net, bHide ? TEXT("OnActive") : TEXT("OnRemove"), *Leaf.ToString(), *GetNameSafe(MyTarget));
	}
}

AAFLCueNotify_ZoneSever::AAFLCueNotify_ZoneSever()
{
	// Pure router cue: no tick, no spawned visual. Auto-destroy on remove like the beam (pool-safe).
	PrimaryActorTick.bCanEverTick = false;
	bAutoDestroyOnRemove = true;
}

bool AAFLCueNotify_ZoneSever::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// Cue ADDED -> hide the zone bone on THIS client (server + every remote, incl. late-join via the container).
	RouteToCosmeticTarget(MyTarget, Parameters, /*bHide=*/true);
	return Super::OnActive_Implementation(MyTarget, Parameters);
}

bool AAFLCueNotify_ZoneSever::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	// Cue REMOVED (server RemoveGameplayCue at reattach) -> un-hide on every client. Symmetric.
	RouteToCosmeticTarget(MyTarget, Parameters, /*bHide=*/false);
	return Super::OnRemove_Implementation(MyTarget, Parameters);
}
