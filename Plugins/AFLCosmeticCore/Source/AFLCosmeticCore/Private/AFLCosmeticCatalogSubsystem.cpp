// Copyright C12 AI Gaming. All Rights Reserved.

#include "AFLCosmeticCatalogSubsystem.h"

#include "AFLCosmeticCatalog.h"
#include "AFLColorIdentityRegistry.h"
#include "AFLCosmeticCore.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"        // GEngine / GetWorldFromContextObject
#include "Engine/GameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLCosmeticCatalogSubsystem)

namespace
{
	// The AssetManager primary-asset TYPE the catalog registers under (matches
	// UAFLCosmeticCatalog::GetPrimaryAssetId + the DefaultGame.ini PrimaryAssetTypesToScan entry).
	const FPrimaryAssetType AFLCosmeticCatalogType(TEXT("AFLCosmeticCatalog"));

	// The color-identity registry's primary-asset type (matches UAFLColorIdentityRegistry::GetPrimaryAssetId
	// + its DefaultGame.ini PrimaryAssetTypesToScan entry).
	const FPrimaryAssetType AFLColorIdentityRegistryType(TEXT("AFLColorIdentityRegistry"));
}

void UAFLCosmeticCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadCatalog();
	LoadColorIdentityRegistry();
}

void UAFLCosmeticCatalogSubsystem::Deinitialize()
{
	Catalog = nullptr;
	ColorIdentityRegistry = nullptr;
	Super::Deinitialize();
}

void UAFLCosmeticCatalogSubsystem::LoadCatalog()
{
	UAssetManager& AssetManager = UAssetManager::Get();

	// Resolve the one catalog asset via its primary-asset type. Synchronous load -> the manifest is tiny
	// (FName/int rows + soft refs); the cosmetic ASSETS it points at stay soft and load on demand later.
	TArray<FPrimaryAssetId> Ids;
	AssetManager.GetPrimaryAssetIdList(AFLCosmeticCatalogType, Ids);

	if (Ids.Num() == 0)
	{
		// Not fatal: if the asset isn't authored / the type isn't registered, the subsystem is simply
		// not-ready (IsReady() false) and resolves nothing. Logged so a "catalog miss" is legible.
		UE_LOG(LogAFLCosmeticCore, Warning,
			TEXT("[Catalog] No AFLCosmeticCatalog primary asset found (register PrimaryAssetTypesToScan + author DA_AFL_CosmeticCatalog)."));
		return;
	}

	if (Ids.Num() > 1)
	{
		UE_LOG(LogAFLCosmeticCore, Warning,
			TEXT("[Catalog] %d AFLCosmeticCatalog assets found; expected ONE global catalog. Using the first (%s)."),
			Ids.Num(), *Ids[0].ToString());
	}

	const FSoftObjectPath Path = AssetManager.GetPrimaryAssetPath(Ids[0]);
	Catalog = Cast<UAFLCosmeticCatalog>(Path.TryLoad());

	if (Catalog)
	{
		Catalog->BuildLookup(); // ensure the accelerator is built (PostLoad also does, belt + suspenders)
		UE_LOG(LogAFLCosmeticCore, Log, TEXT("[Catalog] Loaded %s (%d entries)."),
			*Catalog->GetName(), Catalog->Entries.Num());
	}
	else
	{
		UE_LOG(LogAFLCosmeticCore, Warning, TEXT("[Catalog] Failed to load catalog asset at %s."), *Path.ToString());
	}
}

void UAFLCosmeticCatalogSubsystem::LoadColorIdentityRegistry()
{
	UAssetManager& AssetManager = UAssetManager::Get();

	// Resolve the one registry via its primary-asset type. Synchronous load -> tiny (tags + colors).
	TArray<FPrimaryAssetId> Ids;
	AssetManager.GetPrimaryAssetIdList(AFLColorIdentityRegistryType, Ids);

	if (Ids.Num() == 0)
	{
		// Not fatal: readers fall back to the default cyber colors until the registry is authored + scanned.
		UE_LOG(LogAFLCosmeticCore, Warning,
			TEXT("[ColorIdentity] No AFLColorIdentityRegistry primary asset found (register PrimaryAssetTypesToScan + author DA_AFL_ColorIdentityRegistry)."));
		return;
	}

	if (Ids.Num() > 1)
	{
		UE_LOG(LogAFLCosmeticCore, Warning,
			TEXT("[ColorIdentity] %d registries found; expected ONE. Using the first (%s)."),
			Ids.Num(), *Ids[0].ToString());
	}

	const FSoftObjectPath Path = AssetManager.GetPrimaryAssetPath(Ids[0]);
	ColorIdentityRegistry = Cast<UAFLColorIdentityRegistry>(Path.TryLoad());

	if (ColorIdentityRegistry)
	{
		UE_LOG(LogAFLCosmeticCore, Log, TEXT("[ColorIdentity] Loaded %s (%d identities)."),
			*ColorIdentityRegistry->GetName(), ColorIdentityRegistry->Identities.Num());
	}
	else
	{
		UE_LOG(LogAFLCosmeticCore, Warning, TEXT("[ColorIdentity] Failed to load registry asset at %s."), *Path.ToString());
	}
}

const FAFLCatalogEntry* UAFLCosmeticCatalogSubsystem::FindEntry(FName CosmeticId) const
{
	return Catalog ? Catalog->FindEntry(CosmeticId) : nullptr;
}

UPrimaryDataAsset* UAFLCosmeticCatalogSubsystem::ResolveAsset(FName CosmeticId) const
{
	const FAFLCatalogEntry* Entry = FindEntry(CosmeticId);
	if (!Entry)
	{
		return nullptr;
	}
	// Synchronous load of the soft ref (cooked + small). LoadSynchronous returns null if the ref is unset.
	return Entry->Asset.LoadSynchronous();
}

void UAFLCosmeticCatalogSubsystem::GetEntriesByType(EAFLCosmeticType Type, TArray<const FAFLCatalogEntry*>& OutEntries) const
{
	OutEntries.Reset();
	if (!Catalog)
	{
		return;
	}
	for (const FAFLCatalogEntry& Entry : Catalog->Entries)
	{
		if (Entry.Type == Type)
		{
			OutEntries.Add(&Entry);
		}
	}
}

void UAFLCosmeticCatalogSubsystem::GetPurchasableEntries(TArray<FAFLCatalogEntry>& OutEntries) const
{
	OutEntries.Reset();
	if (!Catalog)
	{
		return;
	}
	for (const FAFLCatalogEntry& Entry : Catalog->Entries)
	{
		// Purchasable = NOT GrantedFree (identity / free base / basic colors are owned by all, not shop items).
		if (Entry.Acquisition != EAFLAcquisition::GrantedFree)
		{
			OutEntries.Add(Entry); // by value -- small BlueprintType struct for the store grid.
		}
	}
}

bool UAFLCosmeticCatalogSubsystem::GetEntry(FName CosmeticId, FAFLCatalogEntry& OutEntry) const
{
	if (const FAFLCatalogEntry* Entry = FindEntry(CosmeticId))
	{
		OutEntry = *Entry;
		return true;
	}
	return false;
}

FText UAFLCosmeticCatalogSubsystem::GetEntryPriceText(const FAFLCatalogEntry& Entry)
{
	// Show the ACTUAL currency/currencies the entry charges (Fix 1): a dual-priced SPARK item reads
	// "10,000 V / 100,000 W" (pay-either), a Volts-only tier "16,000 V", a Watts-only item "100,000 W".
	// Grouped (thousands separators) via FText::AsNumber. GrantedFree / no price reads "Free".
	const bool bHasVolts = (Entry.PriceVolts > 0);
	const bool bHasWatts = (Entry.PriceWatts > 0);

	if (!bHasVolts && !bHasWatts)
	{
		return NSLOCTEXT("AFLStore", "Price_Free", "Free");
	}

	FText VoltsPart;
	if (bHasVolts)
	{
		VoltsPart = FText::Format(NSLOCTEXT("AFLStore", "Price_Volts", "{0} V"), FText::AsNumber(Entry.PriceVolts));
	}
	FText WattsPart;
	if (bHasWatts)
	{
		WattsPart = FText::Format(NSLOCTEXT("AFLStore", "Price_Watts", "{0} W"), FText::AsNumber(Entry.PriceWatts));
	}

	if (bHasVolts && bHasWatts)
	{
		// Pay-either (SPARK): both, separated, so the player sees the choice.
		return FText::Format(NSLOCTEXT("AFLStore", "Price_Either", "{0} / {1}"), VoltsPart, WattsPart);
	}
	return bHasVolts ? VoltsPart : WattsPart;
}

FGameplayTag UAFLCosmeticCatalogSubsystem::ResolveRarityTag(const FAFLCatalogEntry& Entry)
{
	// Designer-set tag wins; else map the enum so a card always resolves a rarity (additive fallback).
	if (Entry.RarityTag.IsValid())
	{
		return Entry.RarityTag;
	}
	const TCHAR* TagStr = TEXT("Cosmetic.Rarity.Common");
	switch (Entry.Rarity)
	{
	case EAFLCosmeticRarity::Uncommon:  TagStr = TEXT("Cosmetic.Rarity.Uncommon");  break;
	case EAFLCosmeticRarity::Rare:      TagStr = TEXT("Cosmetic.Rarity.Rare");      break;
	case EAFLCosmeticRarity::Epic:      TagStr = TEXT("Cosmetic.Rarity.Epic");      break;
	case EAFLCosmeticRarity::Legendary: TagStr = TEXT("Cosmetic.Rarity.Legendary"); break;
	case EAFLCosmeticRarity::Common:
	default:                            TagStr = TEXT("Cosmetic.Rarity.Common");    break;
	}
	// ErrorIfNotFound=false: these tags may not be registered in the ini yet; we still want a stable mapping
	// for the color/text switch below (which keys off the enum anyway). The tag is for BP-side comparisons.
	return FGameplayTag::RequestGameplayTag(FName(TagStr), /*ErrorIfNotFound=*/false);
}

FLinearColor UAFLCosmeticCatalogSubsystem::GetRarityColor(const FAFLCatalogEntry& Entry)
{
	// IRONICS cyber palette: Common=steel, Uncommon=green(#00ff88), Rare=cyan(#00f0ff),
	// Epic=magenta(#ff00aa), Legendary=gold(#ffd700). Keyed off the enum (the stable, always-present value).
	switch (Entry.Rarity)
	{
	case EAFLCosmeticRarity::Uncommon:  return FLinearColor(0.0f,   1.0f,   0.533f, 1.0f); // green
	case EAFLCosmeticRarity::Rare:      return FLinearColor(0.0f,   0.941f, 1.0f,   1.0f); // cyan
	case EAFLCosmeticRarity::Epic:      return FLinearColor(1.0f,   0.0f,   0.667f, 1.0f); // magenta
	case EAFLCosmeticRarity::Legendary: return FLinearColor(1.0f,   0.843f, 0.0f,   1.0f); // gold
	case EAFLCosmeticRarity::Common:
	default:                            return FLinearColor(0.478f, 0.478f, 0.588f, 1.0f); // steel (text-mute)
	}
}

FText UAFLCosmeticCatalogSubsystem::GetRarityText(const FAFLCatalogEntry& Entry)
{
	switch (Entry.Rarity)
	{
	case EAFLCosmeticRarity::Uncommon:  return NSLOCTEXT("AFLStore", "Rarity_Uncommon",  "UNCOMMON");
	case EAFLCosmeticRarity::Rare:      return NSLOCTEXT("AFLStore", "Rarity_Rare",      "RARE");
	case EAFLCosmeticRarity::Epic:      return NSLOCTEXT("AFLStore", "Rarity_Epic",      "EPIC");
	case EAFLCosmeticRarity::Legendary: return NSLOCTEXT("AFLStore", "Rarity_Legendary", "LEGENDARY");
	case EAFLCosmeticRarity::Common:
	default:                            return NSLOCTEXT("AFLStore", "Rarity_Common",    "COMMON");
	}
}

const FAFLColorIdentity* UAFLCosmeticCatalogSubsystem::FindColorIdentity(const FGameplayTag& IdentityTag) const
{
	// Lazy-load guard: the engine-startup AssetManager scan can run BEFORE the registry asset exists (or the
	// subsystem can init before the scan completes), leaving ColorIdentityRegistry null. If a resolve is asked
	// for and we have no registry yet, load it now (covers the startup-timing gap that left edges on fallback).
	if (!ColorIdentityRegistry)
	{
		const_cast<UAFLCosmeticCatalogSubsystem*>(this)->LoadColorIdentityRegistry();
	}

	const FAFLColorIdentity* Found = ColorIdentityRegistry ? ColorIdentityRegistry->FindIdentity(IdentityTag) : nullptr;

	// DIAGNOSTIC (afl.Cosmetic identity resolve) -- instrument-then-fix: log exactly what each resolve sees,
	// so a PIE log proves registry-loaded? tag-valid? found? per entry, instead of inferring from the screen.
	UE_LOG(LogAFLCosmeticCore, Log,
		TEXT("[IdentityResolve] tag=%s  registry=%s identities=%d  found=%s"),
		*IdentityTag.ToString(),
		ColorIdentityRegistry ? *ColorIdentityRegistry->GetName() : TEXT("NULL"),
		ColorIdentityRegistry ? ColorIdentityRegistry->Identities.Num() : -1,
		Found ? TEXT("YES") : TEXT("no(fallback)"));

	return Found;
}

bool UAFLCosmeticCatalogSubsystem::ResolveColorIdentity(const UObject* WorldContext, FGameplayTag IdentityTag, FAFLColorIdentity& OutIdentity)
{
	if (const UAFLCosmeticCatalogSubsystem* Sub = Get(WorldContext))
	{
		if (const FAFLColorIdentity* Found = Sub->FindColorIdentity(IdentityTag))
		{
			OutIdentity = *Found;
			return true;
		}
	}
	return false; // OutIdentity keeps its struct defaults (cyber blue/cyan) on miss.
}

FLinearColor UAFLCosmeticCatalogSubsystem::GetEntryPrimaryColor(const UObject* WorldContext, const FAFLCatalogEntry& Entry)
{
	// Resolve the entry's identity TAG -> registry -> PrimaryColor. ONE uniform path (no per-card derivation).
	FAFLColorIdentity Identity;
	if (ResolveColorIdentity(WorldContext, Entry.ColorIdentityTag, Identity))
	{
		return Identity.PrimaryColor;
	}
	// Fallback: the cyber-cyan-blue UI accent (also FAFLColorIdentity's PrimaryColor default).
	return FLinearColor(0.0f, 0.42f, 1.0f, 1.0f);
}

FLinearColor UAFLCosmeticCatalogSubsystem::GetEntryAccentColor(const UObject* WorldContext, const FAFLCatalogEntry& Entry)
{
	FAFLColorIdentity Identity;
	if (ResolveColorIdentity(WorldContext, Entry.ColorIdentityTag, Identity))
	{
		return Identity.AccentColor;
	}
	return FLinearColor(0.0f, 0.941f, 1.0f, 1.0f); // cyan accent default
}

UAFLCosmeticCatalogSubsystem* UAFLCosmeticCatalogSubsystem::Get(const UObject* WorldContext)
{
	if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr)
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UAFLCosmeticCatalogSubsystem>();
		}
	}
	return nullptr;
}
