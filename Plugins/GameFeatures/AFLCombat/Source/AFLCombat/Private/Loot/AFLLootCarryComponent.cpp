// Copyright C12 AI Gaming. All Rights Reserved.

#include "Loot/AFLLootCarryComponent.h"

#include "AFLCombat.h"
#include "Character/LyraHealthComponent.h"
#include "Cosmetics/AFLWalletComponent.h"     // the BANKED twin -- extract banks the at-risk pool into it
#include "Engine/StaticMesh.h"                 // C1: the per-form gib mesh (cheat LoadObject + the form-name log)
#include "Materials/MaterialInterface.h"       // PRESENTATION: FAFLCarriedForm::GibMaterial (TObjectPtr UPROPERTY full type)
#include "Engine/World.h"
#include "GameFramework/CheatManagerDefines.h" // UE_WITH_CHEAT_MANAGER -- without it the #if reads 0 + the cmd compiles out SILENTLY
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"     // GetFirstPlayerController (the gate-4 dev cheat)
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"                // FAutoConsoleCommandWithWorldArgsAndOutputDevice
#include "Loot/AFLLootCarryPickup.h"
#include "Messages/AFLHitConfirmMessage.h"     // FAFLHitConfirmMessage (AFLCore damage verb)
#include "Messages/LyraVerbMessage.h"          // FLyraVerbMessage (the extraction broadcast payload)
#include "Net/UnrealNetwork.h"                 // DOREPLIFETIME
#include "NativeGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLLootCarryComponent)

// The two hooks Phase A subscribes to -- both already broadcast by shipped code (UAFLDamageExecCalc /
// UAFLAG_Extract). Native-define per the file convention so module init never races the tag-ini scan.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Damage_Confirmed_Carry, "Event.Damage.Confirmed");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Extraction_Complete_Carry, "Event.Extraction.Complete");

static TAutoConsoleVariable<float> CVarAFLLootDropPercent(
	TEXT("afl.Loot.DropPercent"),
	33.0f,
	TEXT("Percent of the carried-loot pool scattered on each confirmed hit (death scatters the remainder)."));

UAFLLootCarryComponent::UAFLLootCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Mirror the wallet rail: a plain UActorComponent has no replicated base -> WE enable replication or
	// CarriedValue never reaches clients (the wallet's "compiles but doesn't replicate" trap). Pool changes
	// are collect/scatter/extract-rare -> no tick.
	SetIsReplicatedByDefault(true);

	// Default the scatter pickup to the Phase-A pickup (same module; a BP child can override).
	ScatterPickupClass = AAFLLootCarryPickup::StaticClass();
}

void UAFLLootCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAFLLootCarryComponent, CarriedValue);
	DOREPLIFETIME(UAFLLootCarryComponent, CarriedPartsValue);
	DOREPLIFETIME(UAFLLootCarryComponent, CarriedPartsCount);
}

void UAFLLootCarryComponent::Collect(int32 Value)
{
	// Back-compat (the proven Phase A/B caches call this): collect as the generic CUBE form -- ScatterPickupClass,
	// no gib mesh -- routed through the form-aware Collect so the single-form path buckets like any other form.
	FAFLCarriedForm CubeForm;
	CubeForm.ScatterForm = ScatterPickupClass;
	Collect(Value, CubeForm);
}

void UAFLLootCarryComponent::Collect(int32 Value, const FAFLCarriedForm& Form)
{
	AActor* Owner = GetOwner();
	if (Value <= 0 || !Owner || !Owner->HasAuthority())
	{
		return;
	}
	CommitCarriedDelta(+Value);   // the replicated rail -- UNCHANGED (the HUD + the bank total)
	BucketValue(Form, Value);     // the server-only provenance ledger -- form-accurate scatter (additive)
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s collected +%d (%s) -> pool %d, ledger %d bucket(s)"),
		*GetNameSafe(Owner), Value, Form.GibMesh ? *Form.GibMesh->GetName() : TEXT("cube"),
		CarriedValue, Ledger.Num());
}

void UAFLLootCarryComponent::BucketValue(const FAFLCarriedForm& Form, int32 Value)
{
	// Find-or-add by FORM identity (scatter class + gib mesh). Bounded (~4 forms: cube, head/arm/leg gib), so a
	// linear scan is right. Keeps sum(Ledger) == CarriedValue (Collect just committed the same +Value to the rail).
	const TSubclassOf<AAFLLootCarryPickup> FormClass = Form.ScatterForm ? Form.ScatterForm : ScatterPickupClass;
	for (FAFLCarriedForm& Bucket : Ledger)
	{
		if (Bucket.ScatterForm == FormClass && Bucket.GibMesh == Form.GibMesh && Bucket.GibMaterial == Form.GibMaterial
			&& Bucket.GibSkinColor == Form.GibSkinColor)
		{
			Bucket.Value += Value;
			return;
		}
	}
	FAFLCarriedForm& NewBucket = Ledger.AddDefaulted_GetRef();
	NewBucket.ScatterForm = FormClass;
	NewBucket.GibMesh = Form.GibMesh;
	NewBucket.GibMaterial = Form.GibMaterial;
	NewBucket.GibSkinColor = Form.GibSkinColor;
	NewBucket.Value = Value;
}

FAFLCarriedForm UAFLLootCarryComponent::MakeLimbForm(UStaticMesh* GibMesh, UMaterialInterface* GibMaterial,
	UAFLSkinColorAsset* GibSkinColor) const
{
	// C2: the dismember scatter form = the cube's recoverable pickup (ScatterPickupClass) WEARING the limb gib
	// mesh (applied per-spawn by SpawnFormPickups -> AAFLLootCarryPickup::SetVisualMesh). A null GibMesh degrades
	// to the plain cube. C3 builds this from the live limb's own LimbGibMesh/HeadGibMesh -- no hardcoded path here.
	// PRESENTATION: GibMaterial = the victim's slot-1 MIC so the scattered gib reads as WHOSE it is (skinned).
	FAFLCarriedForm Form;
	Form.ScatterForm = ScatterPickupClass;
	Form.GibMesh = GibMesh;
	Form.GibMaterial = GibMaterial;
	Form.GibSkinColor = GibSkinColor;
	return Form;
}

void UAFLLootCarryComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;   // pool mutation + scatter are authority ops; the int replicates down to clients via OnRep
	}

	// Death -> scatter the remainder (the same OnDeathStarted the head loot-box's owner-death-vanish uses).
	if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Owner))
	{
		Health->OnDeathStarted.AddDynamic(this, &UAFLLootCarryComponent::HandleDeathStarted);
	}

	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem& Messages = UGameplayMessageSubsystem::Get(World);
		// Confirmed hit on me -> scatter a portion (the Event.Damage.Confirmed the carry-drop consumes).
		DamageListenerHandle = Messages.RegisterListener<FAFLHitConfirmMessage>(TAG_Event_Damage_Confirmed_Carry,
			[this](FGameplayTag C, const FAFLHitConfirmMessage& M) { HandleDamageConfirmed(C, M); });
		// I completed an extraction -> bank the pool to Watts (UAFLAG_Extract already broadcasts this).
		ExtractListenerHandle = Messages.RegisterListener<FLyraVerbMessage>(TAG_Event_Extraction_Complete_Carry,
			[this](FGameplayTag C, const FLyraVerbMessage& M) { HandleExtractionComplete(C, M); });
	}
}

void UAFLLootCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Owner))
		{
			Health->OnDeathStarted.RemoveDynamic(this, &UAFLLootCarryComponent::HandleDeathStarted);
		}
	}
	if (DamageListenerHandle.IsValid())
	{
		DamageListenerHandle.Unregister();
	}
	if (ExtractListenerHandle.IsValid())
	{
		ExtractListenerHandle.Unregister();
	}
	Super::EndPlay(EndPlayReason);
}

void UAFLLootCarryComponent::HandleDamageConfirmed(FGameplayTag /*Channel*/, const FAFLHitConfirmMessage& Message)
{
	if (Message.Target != GetOwner())
	{
		return;   // someone else's hit
	}
	// V7-1: a confirmed hit drops ONE WHOLE part token (indivisible -- order arbitrary, V7-3 sorts smallest-first),
	// BESIDE the fungible cache %-scatter below. A part can't fragment -> one head stays one head across hits.
	ScatterOnePart();
	const int32 Pool = CarriedValue;
	if (Pool <= 0)
	{
		return;
	}
	const float Pct = FMath::Clamp(CVarAFLLootDropPercent.GetValueOnGameThread(), 0.0f, 100.0f);
	const int32 Drop = FMath::Clamp(FMath::RoundToInt(Pool * Pct / 100.0f), 0, Pool);
	if (Drop > 0)
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s hit -> scatter %d/%d (%.0f%%)"),
			*GetNameSafe(GetOwner()), Drop, Pool, Pct);
		ScatterValue(Drop);
	}
}

void UAFLLootCarryComponent::HandleDeathStarted(AActor* /*OwningActor*/)
{
	// V7-1: death drops ALL whole part tokens (indivisible -- one pickup per part), beside the cache remainder.
	ScatterAllParts();
	const int32 Pool = CarriedValue;
	if (Pool > 0)
	{
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s died -> scatter remainder %d"),
			*GetNameSafe(GetOwner()), Pool);
		ScatterValue(Pool);
	}
}

void UAFLLootCarryComponent::HandleExtractionComplete(FGameplayTag /*Channel*/, const FLyraVerbMessage& Message)
{
	if (Message.Instigator != GetOwner())
	{
		return;   // someone else's extraction
	}
	// V7-4: extraction COUNT -- bank BOTH the fungible cache rail (CarriedValue) AND the carried PART tokens' fixed
	// values, then clear both. Pre-V7-4 only the rail banked, so a head/limb a player carried to extraction had its
	// value silently DROPPED (the tokens live on CarriedParts, beside the rail). Sum the indivisible tokens here.
	int32 PartSum = 0;
	for (const FAFLCarriedPart& Part : CarriedParts)
	{
		PartSum += Part.FixedValue;
	}
	const int32 Pool = CarriedValue;
	const int32 Total = Pool + PartSum;
	if (Total <= 0)
	{
		return;   // nothing carried (rail empty AND no part tokens)
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerState* PS = Pawn ? Pawn->GetPlayerState() : nullptr;
	UAFLWalletComponent* Wallet = PS ? PS->FindComponentByClass<UAFLWalletComponent>() : nullptr;
	if (Wallet)
	{
		// Bank the at-risk pools into the banked wallet -- PER-PLAYER (the wallet is on the EXTRACTOR's PlayerState;
		// the project has no team-score system, only cosmetic teams). Then zero BOTH so a second extract can't
		// double-bank (mirrors the rail's CommitCarriedDelta + Ledger.Reset, now extended to the token track).
		Wallet->EarnWattsAuthority(Total, TEXT("loot-extract"));
		if (Pool > 0)
		{
			CommitCarriedDelta(-Pool);
		}
		Ledger.Reset();        // cache provenance banked (sum-invariant: rail now 0)
		CarriedParts.Empty();  // V7-4: part tokens banked -> cleared
		RecomputeCarriedAggregates();   // surface the cleared total/count to the owner's HUD
		UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s extracted -> +%d Watts (%d cache + %d parts), pools cleared"),
			*GetNameSafe(GetOwner()), Total, Pool, PartSum);
	}
	else
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOOTCARRY: %s extracted but no wallet found -- %d not banked"),
			*GetNameSafe(GetOwner()), Total);
	}
}

void UAFLLootCarryComponent::ScatterValue(int32 Amount)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}
	Amount = FMath::Min(Amount, CarriedValue);
	if (Amount <= 0)
	{
		return;
	}

	// Remove from the replicated rail FIRST (authoritative), then DRAIN the server-only ledger in collection
	// order -- each drained chunk spawns ITS form (cube vs limb gib), summing to Amount. The rail delta is
	// identical to the pre-ledger behavior, so the proven value path (and afl.LootCarry.Test.Run) is unchanged.
	CommitCarriedDelta(-Amount);

	int32 Remaining = Amount;
	while (Remaining > 0 && Ledger.Num() > 0)
	{
		FAFLCarriedForm& Bucket = Ledger[0];   // FIFO = collection order (oldest-collected scatters first)
		const int32 Take = FMath::Min(Bucket.Value, Remaining);
		SpawnFormPickups(Bucket.ScatterForm, Bucket.GibMesh, Bucket.GibMaterial, Bucket.GibSkinColor, Take);
		Bucket.Value -= Take;
		Remaining -= Take;
		if (Bucket.Value <= 0)
		{
			Ledger.RemoveAt(0);
		}
	}

	// Desync safety: a ledger that ever under-counts the rail must never silently swallow value -- scatter the
	// remainder as the default cube. Does not fire while the sum-invariant holds (it always should).
	if (Remaining > 0)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOOTCARRY: ledger under-counted by %d -- scattering as cube"), Remaining);
		SpawnFormPickups(ScatterPickupClass, nullptr, nullptr, nullptr, Remaining);
	}
}

void UAFLLootCarryComponent::SpawnFormPickups(TSubclassOf<AAFLLootCarryPickup> Form, UStaticMesh* GibMesh, UMaterialInterface* GibMaterial,
	UAFLSkinColorAsset* GibSkinColor, int32 Value)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || Value <= 0)
	{
		return;
	}
	const TSubclassOf<AAFLLootCarryPickup> SpawnClass = Form ? Form : ScatterPickupClass;
	if (!SpawnClass)
	{
		UE_LOG(LogAFLCombat, Warning, TEXT("AFL_LOOTCARRY: no scatter form -- %d not scattered"), Value);
		return;
	}
	// Quantize this form's value into K recoverable chunks (the proven Phase-A spread), each pickup wearing the
	// form's gib mesh if set (else the pickup's own cube). Deterministic VALUE per chunk; only POSITION jitters.
	const int32 ChunkSize = 50;
	const int32 K = FMath::Clamp(FMath::CeilToInt(Value / static_cast<float>(ChunkSize)), 1, 8);
	const FVector Base = Owner->GetActorLocation();
	int32 Remaining = Value;
	for (int32 i = 0; i < K && Remaining > 0; ++i)
	{
		const int32 Per = (i == K - 1) ? Remaining : FMath::Max(1, Value / K);
		Remaining -= Per;
		const FVector Loc = Base + FVector(FMath::RandRange(-150.0f, 150.0f), FMath::RandRange(-150.0f, 150.0f), 40.0f);
		const FTransform SpawnTM(FRotator::ZeroRotator, Loc);
		// CACHE SPAWN-RACE FIX (mirrors the part fix in 7c6effc6): deferred spawn so value/mesh/material + the arm
		// delay are set BEFORE BeginPlay registers the collect. A plain SpawnActor armed the overlap first -> a cube
		// spawned point-blank on a stationary dropper self-collected the same frame with its DEFAULT value (50), not
		// Per. The 1.5s arm-delay then lets the cube land + present (value-fungible, low-risk; the harness asserts
		// pool deltas, not immediate re-collect, so it holds 10/10).
		AAFLLootCarryPickup* Pickup = World->SpawnActorDeferred<AAFLLootCarryPickup>(
			*SpawnClass, SpawnTM, /*Owner=*/nullptr, /*Instigator=*/nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Pickup)
		{
			Pickup->SetValue(Per);
			if (GibMesh)
			{
				Pickup->SetVisualMesh(GibMesh);   // per-spawn form (the limb gib in C2/C3; the test sphere here)
			}
			if (GibMaterial)
			{
				Pickup->SetVisualMaterial(GibMaterial);   // PRESENTATION (layer 1): the victim's slot-1 base MIC
			}
			if (GibSkinColor)
			{
				Pickup->SetVisualSkinColor(GibSkinColor);   // PRESENTATION (layer 2): the victim's finish color params on top
			}
			Pickup->SetArmDelay(1.5f);   // present-before-collectible -- the same beat the part scatter uses
			Pickup->FinishSpawning(SpawnTM);
		}
	}
}

void UAFLLootCarryComponent::CollectPart(const FAFLCarriedPart& Part)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || Part.FixedValue <= 0)
	{
		return;
	}
	// One WHOLE indivisible token on the server-only part track -- it NEVER enters the chunking rail, so it can't
	// fragment (the residue is designed out). Caches stay on CarriedValue; parts are tokens beside it.
	CarriedParts.Add(Part);
	RecomputeCarriedAggregates();   // surface the new total/count to the owner's HUD
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s collected PART token (owner=%d zone=%d value=%d) -> %d part(s)"),
		*GetNameSafe(Owner), Part.OwnerPlayerId, static_cast<int32>(Part.OriginZone), Part.FixedValue, CarriedParts.Num());
}

int32 UAFLLootCarryComponent::ResolvePlayerId(const AActor* Actor)
{
	if (!Actor)
	{
		return INDEX_NONE;
	}
	const APlayerState* PS = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Actor))                 { PS = Pawn->GetPlayerState(); }
	else if (const AController* Ctrl = Cast<AController>(Actor)) { PS = Ctrl->PlayerState; }
	else                                                        { PS = Cast<APlayerState>(Actor); }
	return PS ? PS->GetPlayerId() : INDEX_NONE;
}

void UAFLLootCarryComponent::ScatterOnePart()
{
	if (CarriedParts.Num() == 0)
	{
		return;   // no parts -> nothing here (the cache %-scatter is separate)
	}
	// V7-3: drop the SMALLEST part first -- sort ascending by FixedValue so [0] is the cheapest token. Legs (16)
	// bleed first under sustained fire; the head (160) clings, dropping LAST. Selection/order only -- the token data
	// + every other path are untouched (ScatterAllParts drops the whole set, order-independent).
	CarriedParts.Sort([](const FAFLCarriedPart& A, const FAFLCarriedPart& B) { return A.FixedValue < B.FixedValue; });
	const FAFLCarriedPart Part = CarriedParts[0];
	CarriedParts.RemoveAt(0);
	RecomputeCarriedAggregates();   // surface the reduced total/count to the owner's HUD
	SpawnPartPickup(Part);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s hit -> drop 1 whole part (zone=%d value=%d) -> %d left"),
		*GetNameSafe(GetOwner()), static_cast<int32>(Part.OriginZone), Part.FixedValue, CarriedParts.Num());
}

void UAFLLootCarryComponent::ScatterAllParts()
{
	if (CarriedParts.Num() == 0)
	{
		return;
	}
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s death -> drop ALL %d whole part(s)"),
		*GetNameSafe(GetOwner()), CarriedParts.Num());
	for (const FAFLCarriedPart& Part : CarriedParts)
	{
		SpawnPartPickup(Part);
	}
	CarriedParts.Empty();
	RecomputeCarriedAggregates();   // surface the cleared total/count to the owner's HUD
}

void UAFLLootCarryComponent::SpawnPartPickup(const FAFLCarriedPart& Part)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !ScatterPickupClass)
	{
		return;
	}
	// One token = one pickup -- INDIVISIBLE (no quantizer). The pickup wears the gib + carries the token's
	// {owner, zone, value} so it re-collects as a part (CollectPart) and V7-2 can owner-check it.
	const FVector Loc = Owner->GetActorLocation() +
		FVector(FMath::RandRange(-150.0f, 150.0f), FMath::RandRange(-150.0f, 150.0f), 40.0f);
	const FTransform SpawnTM(FRotator::ZeroRotator, Loc);
	// DEFERRED spawn (the box-not-head fix): InitPartToken sets the identity (owner/zone/value/gib) AND the overlap
	// arm-delay BEFORE BeginPlay registers the collect. A plain SpawnActor runs BeginPlay first -> the pickup spawns
	// ARMED on top of the stationary dropper, its overlap fires immediately, and it self-collects as a DEFAULT +50
	// cube (losing the gib, the 160 value, and the owner id) before InitPartToken ever runs -- then SpawnActor
	// returns null (destroyed mid-spawn) so the init is skipped entirely (0 "applied gib form" in the 04:36 log).
	// Deferred guarantees the token + the inert-until-armed delay are in place when the overlap first registers.
	AAFLLootCarryPickup* Pickup = World->SpawnActorDeferred<AAFLLootCarryPickup>(
		*ScatterPickupClass, SpawnTM, /*Owner=*/nullptr, /*Instigator=*/nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Pickup)
	{
		Pickup->InitPartToken(Part.OwnerPlayerId, Part.OriginZone, Part.FixedValue, Part.GibMesh, Part.GibMaterial, Part.GibSkinColor);
		Pickup->FinishSpawning(SpawnTM);
	}
}

void UAFLLootCarryComponent::CommitCarriedDelta(int32 Delta)
{
	// The single authority commit point (mirrors UAFLWalletComponent::CommitMutation, minus persistence -- the
	// at-risk pool is ephemeral by design). Listen-host applies locally here (OnRep does not fire on authority)
	// -> broadcast for the host HUD too; remote clients fire the same broadcast from OnRep_CarriedValue.
	CarriedValue = FMath::Max(0, CarriedValue + Delta);
	OnCarriedValueChanged.Broadcast(CarriedValue);
}

void UAFLLootCarryComponent::OnRep_CarriedValue()
{
	// Remote clients: the pool replicated in -- the owner's HUD updates from here (event-driven, mirrors the
	// wallet's OnRep_Balance).
	OnCarriedValueChanged.Broadcast(CarriedValue);
}

void UAFLLootCarryComponent::RecomputeCarriedAggregates()
{
	// Authority: re-derive the replicated PART aggregate from the server-only token list + broadcast for the
	// listen-host's own HUD (OnRep doesn't fire on authority). Remote clients fire the same delegate from the OnReps
	// below. The token LIST (owner/zone/identity) stays server-only -- only these two ints replicate (mirrors the
	// CarriedValue rail: aggregate-only net surface).
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	int32 Sum = 0;
	for (const FAFLCarriedPart& Part : CarriedParts)
	{
		Sum += Part.FixedValue;
	}
	CarriedPartsValue = Sum;
	CarriedPartsCount = CarriedParts.Num();
	OnCarriedPartsChanged.Broadcast(CarriedPartsValue, CarriedPartsCount);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s carried aggregate -> %d cache + %d parts (%d tokens) = %d TOTAL (auth=1)"),
		*GetNameSafe(GetOwner()), CarriedValue, CarriedPartsValue, CarriedPartsCount, GetCarriedTotal());
}

void UAFLLootCarryComponent::OnRep_CarriedPartsValue()
{
	// Remote clients: the part-value aggregate replicated in -- refresh the owner's HUD (event-driven). Value+count
	// change together (every token has FixedValue>0), so this + OnRep_CarriedPartsCount may both fire one frame; the
	// HUD refresh is idempotent (reads the getters).
	OnCarriedPartsChanged.Broadcast(CarriedPartsValue, CarriedPartsCount);
	UE_LOG(LogAFLCombat, Display, TEXT("AFL_LOOTCARRY: %s carried aggregate OnRep -> %d parts (%d tokens) = %d TOTAL (auth=0 CLIENT)"),
		*GetNameSafe(GetOwner()), CarriedPartsValue, CarriedPartsCount, GetCarriedTotal());
}

void UAFLLootCarryComponent::OnRep_CarriedPartsCount()
{
	OnCarriedPartsChanged.Broadcast(CarriedPartsValue, CarriedPartsCount);
}

#if UE_WITH_CHEAT_MANAGER
namespace
{
	// Phase A gate-4 proof in ISOLATION. The real extraction zone is match-phase-gated (it dispenses only while
	// AFL.GamePhase.Playing.ExtractionWindow is open), so this dev cheat broadcasts the SAME message
	// UAFLAG_Extract sends on a real channel-complete (AFLAG_Extract.cpp:244 -- FLyraVerbMessage with
	// Verb + Instigator = the host hero) -> UAFLLootCarryComponent::HandleExtractionComplete banks the carried
	// pool to Watts + zeroes it. Proves the listener hook without driving the match spine. Remove post-Phase-A.
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLootTestExtractCmd(
		TEXT("afl.Loot.TestExtract"),
		TEXT("Phase A: broadcast Event.Extraction.Complete for the host hero -> banks the carried loot pool to Watts (proves the extract hook; no match window needed). HOST only."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				if (!World || World->GetNetMode() == NM_Client)
				{
					Ar.Log(TEXT("afl.Loot.TestExtract -- HOST window only (the extraction broadcast is an authority op)."));
					return;
				}
				APlayerController* PC = World->GetFirstPlayerController();
				APawn* Pawn = PC ? PC->GetPawn() : nullptr;
				if (!Pawn)
				{
					Ar.Log(TEXT("afl.Loot.TestExtract -- no possessed pawn."));
					return;
				}
				// Mirror UAFLAG_Extract::BroadcastExtractionEvent exactly (Verb + Instigator = the channeling pawn).
				FLyraVerbMessage Message;
				Message.Verb = TAG_Event_Extraction_Complete_Carry;
				Message.Instigator = Pawn;
				UGameplayMessageSubsystem::Get(World).BroadcastMessage(Message.Verb, Message);
				Ar.Logf(TEXT("afl.Loot.TestExtract -- broadcast Event.Extraction.Complete for %s (watch AFL_LOOTCARRY)."), *Pawn->GetName());
			}));

	// Phase C1 DIFFERENTIATION proof: collect TWO distinct scatter FORMS into the host pool -- the default cube
	// and a TEST sphere (engine basic shape -> visually distinct, no new asset) -- so a following scatter/death
	// drains the ledger and spawns TWO VISUALLY DISTINCT forms (cube + sphere), proving form-accurate provenance.
	// Collect-only: the operator then drives the EXISTING damage/death/extract to watch the form-accurate drain.
	// HOST only. Remove post-Phase-C (the real limb-gib forms arrive in C2/C3).
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLootCarryTestFormsCmd(
		TEXT("afl.LootCarry.TestForms"),
		TEXT("Phase C1: collect a cube form + a TEST sphere form into the host pool (proves form-accurate scatter on the next damage/death drain). HOST only."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				if (!World || World->GetNetMode() == NM_Client)
				{
					Ar.Log(TEXT("afl.LootCarry.TestForms -- HOST window only (Collect is an authority op)."));
					return;
				}
				APlayerController* PC = World->GetFirstPlayerController();
				APawn* Pawn = PC ? PC->GetPawn() : nullptr;
				UAFLLootCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<UAFLLootCarryComponent>() : nullptr;
				if (!Carry)
				{
					Ar.Log(TEXT("afl.LootCarry.TestForms -- no possessed pawn with a UAFLLootCarryComponent."));
					return;
				}
				// Form 1: the default CUBE (no gib mesh -> the pickup's own cube body).
				FAFLCarriedForm CubeForm;
				CubeForm.ScatterForm = AAFLLootCarryPickup::StaticClass();
				Carry->Collect(150, CubeForm);
				// Form 2: a TEST SPHERE (engine basic shape -> a distinct silhouette from the cube; stands in for
				// the C2 limb gib and exercises the exact per-spawn-mesh path the real gib will use).
				FAFLCarriedForm SphereForm;
				SphereForm.ScatterForm = AAFLLootCarryPickup::StaticClass();
				SphereForm.GibMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
				Carry->Collect(150, SphereForm);
				Ar.Logf(TEXT("afl.LootCarry.TestForms -- collected cube(150) + sphere(150) for %s (pool=%d). Now damage/die to watch TWO DISTINCT forms scatter; afl.Loot.TestExtract to bank+clear."),
					*Pawn->GetName(), Carry->GetCarriedValue());
			}));

	// V7-2 deterministic 2-client REATTACH test: force the HOST pawn to DROP all its carried part tokens (the
	// death-drop without dying), so a part it collected from ANOTHER player hits the ground and THAT player can
	// walk over it and RECLAIM it (the owner-reattach-after-collect) -- without relying on the carrier being killed.
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLootCarryDropPartsCmd(
		TEXT("afl.LootCarry.DropParts"),
		TEXT("V7-2: force the HOST pawn to drop ALL its carried part tokens as recoverable pickups (so each part's OWNER can reclaim it). HOST only."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& /*Args*/, UWorld* World, FOutputDevice& Ar)
			{
				if (!World || World->GetNetMode() == NM_Client)
				{
					Ar.Log(TEXT("afl.LootCarry.DropParts -- HOST window only (scatter is an authority op)."));
					return;
				}
				APlayerController* PC = World->GetFirstPlayerController();
				APawn* Pawn = PC ? PC->GetPawn() : nullptr;
				UAFLLootCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<UAFLLootCarryComponent>() : nullptr;
				if (!Carry)
				{
					Ar.Log(TEXT("afl.LootCarry.DropParts -- no possessed pawn with a UAFLLootCarryComponent."));
					return;
				}
				Carry->ForceDropAllParts();
				Ar.Logf(TEXT("afl.LootCarry.DropParts -- %s dropped all carried part tokens as pickups. Now have each part's OWNER (e.g. CLIENT 1) walk over their own head/limb to RECLAIM it -- the owner-reattach-after-collect."),
					*Pawn->GetName());
			}));

	// Phase C2 REAL-GIB proof: collect a cube + a REAL limb-gib form [arm|leg|head] (the actual dismember gib mesh
	// from /Game/BagMan/Characters/Dismember/) so a following scatter/death shows the scattered loot READING AS THE
	// REAL LIMB GIB -- the form the C1 sphere stood in for, flowing through the SAME C1 ledger + form-accurate
	// scatter path via Carry->MakeLimbForm. C3 (the migration) routes dismember-collect to MakeLimbForm with the
	// LIVE limb's own gib mesh; this cheat hardcodes the verified paths only for the isolated watch. HOST only.
	// Remove post-Phase-C (with the TestForms sphere cheat).
	FAutoConsoleCommandWithWorldArgsAndOutputDevice GAFLLootCarryTestLimbFormCmd(
		TEXT("afl.LootCarry.TestLimbForm"),
		TEXT("Phase C2: collect a cube + a REAL limb-gib form [arm|leg|head] (default arm) into the host pool -> scatter/die to watch it spawn as the real limb gib (not a cube). HOST only."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
			{
				if (!World || World->GetNetMode() == NM_Client)
				{
					Ar.Log(TEXT("afl.LootCarry.TestLimbForm -- HOST window only (Collect is an authority op)."));
					return;
				}
				APlayerController* PC = World->GetFirstPlayerController();
				APawn* Pawn = PC ? PC->GetPawn() : nullptr;
				UAFLLootCarryComponent* Carry = Pawn ? Pawn->FindComponentByClass<UAFLLootCarryComponent>() : nullptr;
				if (!Carry)
				{
					Ar.Log(TEXT("afl.LootCarry.TestLimbForm -- no possessed pawn with a UAFLLootCarryComponent."));
					return;
				}
				// Pick the gib by arg (the verified /Game/BagMan/Characters/Dismember/ gib meshes; default arm).
				const FString Which = Args.Num() > 0 ? Args[0].ToLower() : TEXT("arm");
				FString GibPath;
				if (Which == TEXT("leg"))       { GibPath = TEXT("/Game/BagMan/Characters/Dismember/SM_AFL_RobotLeg_Gib.SM_AFL_RobotLeg_Gib"); }
				else if (Which == TEXT("head")) { GibPath = TEXT("/Game/BagMan/Characters/Dismember/SM_AFL_RobotHead_Gib.SM_AFL_RobotHead_Gib"); }
				else                            { GibPath = TEXT("/Game/BagMan/Characters/Dismember/SM_AFL_RobotArm_Gib.SM_AFL_RobotArm_Gib"); }
				UStaticMesh* Gib = LoadObject<UStaticMesh>(nullptr, *GibPath);
				if (!Gib)
				{
					Ar.Logf(TEXT("afl.LootCarry.TestLimbForm -- could not load gib %s"), *GibPath);
					return;
				}
				// Cube baseline + the REAL limb form (carry-pickup wearing the gib mesh) -> two distinct forms.
				FAFLCarriedForm CubeForm;
				CubeForm.ScatterForm = AAFLLootCarryPickup::StaticClass();
				Carry->Collect(150, CubeForm);
				Carry->Collect(150, Carry->MakeLimbForm(Gib));
				Ar.Logf(TEXT("afl.LootCarry.TestLimbForm -- collected cube(150) + %s gib(150) for %s (pool=%d). Damage/die to watch the REAL %s gib scatter (not a cube); afl.Loot.TestExtract to bank+clear."),
					*Which, *Pawn->GetName(), Carry->GetCarriedValue(), *Which);
			}));
}
#endif // UE_WITH_CHEAT_MANAGER
