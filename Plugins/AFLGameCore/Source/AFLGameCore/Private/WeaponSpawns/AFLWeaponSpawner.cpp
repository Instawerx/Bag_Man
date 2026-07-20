// Copyright C12 AI Gaming. AFL / BAG MAN.
#include "WeaponSpawns/AFLWeaponSpawner.h"

#include "WeaponSpawns/AFLWeaponSpawnRegistry.h"
#include "AFLGameCore.h"                     // LogAFLGameCore
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"                        // APawn (GetNameSafe on the pickup pawn)
#include "Equipment/LyraPickupDefinition.h"           // ULyraWeaponPickupDefinition
#include "Inventory/LyraInventoryItemDefinition.h"    // full type for the TSubclassOf bool check (matches stock)

#include UE_INLINE_GENERATED_CPP_BY_NAME(AFLWeaponSpawner)

AAFLWeaponSpawner::AAFLWeaponSpawner()
{
	// Inherit the stock spawner defaults (pad/collision/cooldown/replication built in the base ctor).
	// Nothing to add: the weapon is chosen at runtime from WeaponIntent, never baked on the instance.
}

void AAFLWeaponSpawner::BeginPlay()
{
	// OPT-IN GATE. This class now sits BETWEEN B_WeaponSpawner and ALyraWeaponSpawner (B_WeaponSpawner is
	// reparented to extend it), so EVERY spawner in the game -- stock B_WeaponSpawner, B_AbilitySpawner, and
	// AFL markers alike -- runs this BeginPlay. A spawner with NO WeaponIntent is a plain stock pad: run
	// ALyraWeaponSpawner::BeginPlay VERBATIM and take zero AFL code path (no registry, no binding, no
	// logging). Stock ShooterCore content behaves byte-for-byte as before.
	if (!WeaponIntent.IsValid())
	{
		Super::BeginPlay();
		return;
	}

	// AFL marker (WeaponIntent set): a null WeaponDefinition here is EXPECTED (resolved at runtime), so the
	// stock BeginPlay's "no valid weapon definition" Error would be a false alarm. Bypass it by running the
	// grandparent AActor::BeginPlay() directly; the ONE meaningful thing the stock BeginPlay does -- set
	// CoolDownTime from the def -- is re-done in ApplyDefinition once the def resolves. (Verified vs
	// ALyraWeaponSpawner::BeginPlay: it does only Super::BeginPlay + the CoolDownTime/error block.)
	AActor::BeginPlay();

	UAFLWeaponSpawnRegistry* Registry = GetRegistry();
	UE_LOG(LogAFLGameCore, Log, TEXT("AAFLWeaponSpawner::BeginPlay '%s' intent='%s' registry=%s active=%d"),
		*GetNameSafe(this), *WeaponIntent.ToString(), Registry ? TEXT("FOUND") : TEXT("NULL"),
		(Registry && Registry->IsActive()) ? 1 : 0);

	if (Registry)
	{
		Registry->OnReady.AddUObject(this, &AAFLWeaponSpawner::HandleRegistryReady);
		Registry->OnCleared.AddUObject(this, &AAFLWeaponSpawner::HandleRegistryCleared);
		bBoundToRegistry = true;

		if (Registry->IsActive())
		{
			// Common case: the experience activated the GameFeature before this cell streamed in.
			ResolveAndApply();
		}
		else
		{
			// Stay inert and wait for OnReady (this marker pre-dates activation).
			UE_LOG(LogAFLGameCore, Log, TEXT("AAFLWeaponSpawner '%s': registry not active yet -> bound OnReady, waiting for broadcast."),
				*GetNameSafe(this));
		}
	}
	else
	{
		// No registry at all in this world -> we genuinely cannot resolve. WARN (never a silent no-op) and
		// stay inert. Names the actor + tag so it is trivially diagnosable.
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AAFLWeaponSpawner '%s': no UAFLWeaponSpawnRegistry in world; cannot resolve WeaponIntent '%s' -- spawner stays inert."),
			*GetNameSafe(this), *WeaponIntent.ToString());
	}
}

void AAFLWeaponSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bBoundToRegistry)
	{
		if (UAFLWeaponSpawnRegistry* Registry = GetRegistry())
		{
			Registry->OnReady.RemoveAll(this);
			Registry->OnCleared.RemoveAll(this);
		}
		bBoundToRegistry = false;
	}

	Super::EndPlay(EndPlayReason);
}

void AAFLWeaponSpawner::AttemptPickUpWeapon_Implementation(APawn* Pawn)
{
	// DIAGNOSTIC (AFL markers only -- gated on WeaponIntent so the stock pads that now inherit this class stay
	// silent). Names WHY the grant does/doesn't fire: the stock guards (authority + bIsWeaponAvailable + a
	// valid InventoryItemDefinition) BEFORE Super calls the BlueprintImplementableEvent GiveWeapon.
	if (WeaponIntent.IsValid())
	{
		const bool bAuthority  = (GetLocalRole() == ROLE_Authority);
		const bool bHasItemDef = WeaponDefinition && (WeaponDefinition->InventoryItemDefinition != nullptr);
		UE_LOG(LogAFLGameCore, Log,
			TEXT("AAFLWeaponSpawner::AttemptPickUpWeapon '%s' pawn='%s' authority=%d available=%d weaponDef=%s hasItemDef=%d -> %s"),
			*GetNameSafe(this), *GetNameSafe(Pawn), bAuthority ? 1 : 0, bIsWeaponAvailable ? 1 : 0,
			*GetNameSafe(WeaponDefinition), bHasItemDef ? 1 : 0,
			(bAuthority && bIsWeaponAvailable && bHasItemDef) ? TEXT("guards PASS -> Super (-> GiveWeapon)") : TEXT("guards FAIL -> no GiveWeapon"));
	}

	Super::AttemptPickUpWeapon_Implementation(Pawn);
}

void AAFLWeaponSpawner::ResolveAndApply()
{
	UAFLWeaponSpawnRegistry* Registry = GetRegistry();
	ULyraWeaponPickupDefinition* Def = Registry ? Registry->Resolve(WeaponIntent) : nullptr;

	if (!Def)
	{
		// The tag is set but nothing in the active table matches it -> a DATA error the designer must see,
		// not a silent no-op. Name the actor + the tag so it is trivially fixable.
		UE_LOG(LogAFLGameCore, Warning,
			TEXT("AAFLWeaponSpawner '%s': WeaponIntent '%s' did not resolve to any weapon in the active spawn table -- spawner stays inert."),
			*GetNameSafe(this), *WeaponIntent.ToString());
		return;
	}

	UE_LOG(LogAFLGameCore, Log, TEXT("AAFLWeaponSpawner '%s': RESOLVED intent '%s' -> %s"),
		*GetNameSafe(this), *WeaponIntent.ToString(), *GetNameSafe(Def));
	ApplyDefinition(Def);
}

void AAFLWeaponSpawner::ApplyDefinition(ULyraWeaponPickupDefinition* Def)
{
	WeaponDefinition = Def;   // inherited protected member

	// The one meaningful action the stock BeginPlay performs (we bypassed it -- see BeginPlay).
	if (Def && Def->InventoryItemDefinition)
	{
		CoolDownTime = Def->SpawnCoolDownSeconds;
	}

	RefreshPad();
}

void AAFLWeaponSpawner::RevertToInert()
{
	WeaponDefinition = nullptr;
	if (WeaponMesh)
	{
		WeaponMesh->SetStaticMesh(nullptr);
	}
}

void AAFLWeaponSpawner::RefreshPad()
{
	// Mirror of ALyraWeaponSpawner::OnConstruction's mesh setup, re-driven at runtime after the def
	// resolves (OnConstruction ran at spawn with a null def and set nothing). Kept in sync with those 3
	// stock lines. The END result is a pad showing the ACTUAL weapon -- no UI degradation once each AFL
	// WeaponPickupData carries a real DisplayMesh.
	if (WeaponDefinition && WeaponDefinition->DisplayMesh && WeaponMesh)
	{
		WeaponMesh->SetStaticMesh(WeaponDefinition->DisplayMesh);
		WeaponMesh->SetRelativeLocation(WeaponDefinition->WeaponMeshOffset);
		WeaponMesh->SetRelativeScale3D(WeaponDefinition->WeaponMeshScale);
	}
}

UAFLWeaponSpawnRegistry* AAFLWeaponSpawner::GetRegistry() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UAFLWeaponSpawnRegistry>() : nullptr;
}

void AAFLWeaponSpawner::HandleRegistryReady()
{
	// The GameFeature just activated and the table is available -> resolve now.
	UE_LOG(LogAFLGameCore, Log, TEXT("AAFLWeaponSpawner '%s': registry OnReady fired -> resolving."), *GetNameSafe(this));
	ResolveAndApply();
}

void AAFLWeaponSpawner::HandleRegistryCleared()
{
	// The GameFeature deactivated -> the AFL weapon is gone; go dark so we never display/grant a stale one.
	RevertToInert();
}
