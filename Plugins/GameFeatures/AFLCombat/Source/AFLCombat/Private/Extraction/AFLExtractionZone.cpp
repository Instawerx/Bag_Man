// Copyright C12 AI Gaming. All Rights Reserved.

#include "Extraction/AFLExtractionZone.h"

#include "AFLCombat.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Effects/AFLGE_InExtractionZone.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLExtractionZone)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_AFL_GamePhase_ExtractionWindow_Zone, "AFL.GamePhase.Playing.ExtractionWindow");

namespace
{
	// The cycle-1 disc material (MI_AFL_Elec_Cylinder_Inst) is UV-distortion driven and exposes NO
	// emissive scalar (verified: Thickness/Mul_VN/etc., no brightness param). So the breathing swap
	// drives what every material honors -- mesh VISIBILITY (hidden when Inactive, shown when Active) --
	// plus, IF the BP child later swaps in a material that DOES expose this param, an emissive scale.
	// Visibility is the cycle-1 truth; the emissive dim/bright is the documented material-upgrade seam.
	const FName NAME_EmissiveParam(TEXT("EmissiveStrength"));
}

AAFLExtractionZone::AAFLExtractionZone()
{
	PrimaryActorTick.bCanEverTick = false; // pure overlap dispenser.

	// Pickup net posture minus motion: replicated from birth (BP visuals on every client),
	// static, no dormancy (always relevant -- ZoneState now mutates), low update rate.
	bReplicates = true;
	SetNetUpdateFrequency(2.0f);

	ZoneSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ZoneSphere"));
	SetRootComponent(ZoneSphere);
	ZoneSphere->InitSphereRadius(300.0f);
	ZoneSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ZoneSphere->SetCollisionObjectType(ECC_WorldStatic);
	ZoneSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	ZoneSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	ZoneEffectClass = UAFLGE_InExtractionZone::StaticClass();
	WindowPhaseTag = TAG_AFL_GamePhase_ExtractionWindow_Zone;
}

void AAFLExtractionZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAFLExtractionZone, ZoneState);
}

void AAFLExtractionZone::BeginPlay()
{
	Super::BeginPlay();

	// Resolve the glow disc (BP child roots a "GlowDisc" StaticMeshComponent) + make a MID so the
	// emissive swap is per-instance. Runs on all machines (visual).
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetName().Contains(TEXT("GlowDisc")))
		{
			GlowDiscComp = Mesh;
			break;
		}
	}
	if (!GlowDiscComp && Meshes.Num() > 0)
	{
		GlowDiscComp = Meshes[0]; // fall back to whatever mesh the child provides
	}
	if (GlowDiscComp && GlowDiscComp->GetMaterial(0))
	{
		GlowMID = GlowDiscComp->CreateDynamicMaterialInstance(0);
	}
	OnRep_ZoneState(); // paint the initial (Inactive/dim) state on every machine

	if (HasAuthority())
	{
		ZoneSphere->OnComponentBeginOverlap.AddDynamic(this, &AAFLExtractionZone::OnZoneBeginOverlap);
		ZoneSphere->OnComponentEndOverlap.AddDynamic(this, &AAFLExtractionZone::OnZoneEndOverlap);

		// Observe the extraction-window phase. THE LYRA PHASE WALL (see AFLMatchPhaseComponent.cpp):
		// the C++ WhenPhase* overloads aren't LYRAGAME_API-exported and the K2_ UFUNCTION equivalents
		// are PROTECTED -- so register the observers REFLECTIVELY via ProcessEvent (the mechanism a BP
		// node compiles to; ignores access control + the export boundary). K2_WhenPhaseStartsOrIsActive
		// fires immediately if the window is ALREADY open (a late-spawned zone syncs free). The dynamic
		// delegates bind our UFUNCTION callbacks.
		if (ULyraGamePhaseSubsystem* PhaseSub = UWorld::GetSubsystem<ULyraGamePhaseSubsystem>(GetWorld()))
		{
			struct FK2WhenPhaseParams
			{
				FGameplayTag PhaseTag;
				EPhaseTagMatchType MatchType = EPhaseTagMatchType::ExactMatch;
				FLyraGamePhaseTagDynamicDelegate WhenPhase;
			};
			auto RegisterObserver = [&](const TCHAR* FnName, FName CallbackName)
			{
				if (UFunction* Fn = PhaseSub->FindFunction(FnName))
				{
					FK2WhenPhaseParams Params;
					Params.PhaseTag = WindowPhaseTag;
					Params.MatchType = EPhaseTagMatchType::ExactMatch;
					Params.WhenPhase.BindUFunction(this, CallbackName);
					PhaseSub->ProcessEvent(Fn, &Params);
				}
			};
			RegisterObserver(TEXT("K2_WhenPhaseStartsOrIsActive"), GET_FUNCTION_NAME_CHECKED(AAFLExtractionZone, HandleWindowPhaseActive));
			RegisterObserver(TEXT("K2_WhenPhaseEnds"), GET_FUNCTION_NAME_CHECKED(AAFLExtractionZone, HandleWindowPhaseEnded));
		}
	}
}

void AAFLExtractionZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A despawning zone must not strand State.InExtractionZone (the grab EndPlay-funnel lesson).
	RemoveAllZoneEffects();
	Super::EndPlay(EndPlayReason);
}

void AAFLExtractionZone::SetZoneActive(bool bActive)
{
	if (!HasAuthority())
	{
		return;
	}
	const EAFLZoneState NewState = bActive ? EAFLZoneState::Active : EAFLZoneState::Inactive;
	if (ZoneState == NewState)
	{
		return;
	}
	ZoneState = NewState;
	OnRep_ZoneState(); // authority paints immediately; clients via replication.

	if (bActive)
	{
		// Window opened: dispense to everyone already standing inside (overlap-begin only fires on
		// transitions, so seed the present set -- the same already-inside pattern as the old BeginPlay).
		TArray<AActor*> Inside;
		ZoneSphere->GetOverlappingActors(Inside, APawn::StaticClass());
		for (AActor* PawnActor : Inside)
		{
			TryDispenseTo(PawnActor);
		}
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_EXTRACT: zone %s ACTIVE (%d inside seeded)."), *GetName(), Inside.Num());
	}
	else
	{
		// Window closed: sweep every live handle -> State.InExtractionZone drops on each channeler
		// -> the GA's existing HandleZoneTagChanged self-cancels -> Failed, energy retained
		// (leg-3 semantics verbatim; ZERO new GA code).
		RemoveAllZoneEffects();
		UE_LOG(LogAFLCombat, Log, TEXT("AFL_EXTRACT: zone %s INACTIVE (handles swept; mid-channel attempts cancel)."), *GetName());
	}
}

void AAFLExtractionZone::OnRep_ZoneState()
{
	const bool bActive = (ZoneState == EAFLZoneState::Active);
	if (GlowDiscComp)
	{
		// Material-agnostic breathing: the disc is hidden while Inactive, shown while Active.
		GlowDiscComp->SetVisibility(bActive, /*bPropagateToChildren=*/true);
	}
	if (GlowMID)
	{
		// Bonus emissive scale IF the (upgraded) material exposes the param -- a no-op on the cycle-1
		// MI (which has none), so visibility alone carries the cycle-1 read.
		GlowMID->SetScalarParameterValue(NAME_EmissiveParam, bActive ? BrightEmissive : DimEmissive);
	}
}

void AAFLExtractionZone::HandleWindowPhaseActive(const FGameplayTag& /*PhaseTag*/)
{
	SetZoneActive(true);
}

void AAFLExtractionZone::HandleWindowPhaseEnded(const FGameplayTag& /*PhaseTag*/)
{
	SetZoneActive(false);
}

void AAFLExtractionZone::TryDispenseTo(AActor* PawnActor)
{
	if (!HasAuthority() || ZoneState != EAFLZoneState::Active || !PawnActor || !PawnActor->IsA<APawn>()
		|| ZoneEffectHandles.Contains(PawnActor))
	{
		return;
	}
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PawnActor);
	if (!ASC || !ZoneEffectClass)
	{
		return;
	}

	// Apply-to-self through the target's ASC (the death-burst rail shape).
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(this, this);
	const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(ZoneEffectClass, 1.0f, Context);
	if (SpecHandle.IsValid())
	{
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		if (Handle.IsValid())
		{
			ZoneEffectHandles.Add(PawnActor, Handle);
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_EXTRACT: %s entered active zone %s -> State.InExtractionZone applied."),
				*GetNameSafe(PawnActor), *GetName());
		}
	}
}

void AAFLExtractionZone::RemoveAllZoneEffects()
{
	for (TPair<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle>& Pair : ZoneEffectHandles)
	{
		if (Pair.Key.IsValid() && Pair.Value.IsValid())
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pair.Key.Get()))
			{
				ASC->RemoveActiveGameplayEffect(Pair.Value);
			}
		}
	}
	ZoneEffectHandles.Empty();
}

void AAFLExtractionZone::OnZoneBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	// Guarded on state==Active inside TryDispenseTo: walking in while Inactive dispenses nothing.
	TryDispenseTo(OtherActor);
}

void AAFLExtractionZone::OnZoneEndOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (!HasAuthority() || !OtherActor)
	{
		return;
	}
	FActiveGameplayEffectHandle Handle;
	if (ZoneEffectHandles.RemoveAndCopyValue(OtherActor, Handle) && Handle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor))
		{
			ASC->RemoveActiveGameplayEffect(Handle);
			UE_LOG(LogAFLCombat, Log, TEXT("AFL_EXTRACT: %s left zone %s -> State.InExtractionZone removed."),
				*GetNameSafe(OtherActor), *GetName());
		}
	}
}
