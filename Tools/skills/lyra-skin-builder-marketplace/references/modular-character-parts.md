# Modular Character Parts Reference

Phase 4 of the pipeline. **Lyra ships a complete modular character parts
system** — `ULyraPawnComponent_CharacterParts`. Don't rewrite it. Extend it.

This reference covers using and extending Lyra's stock system for full
modular skin support (head/torso/legs/arms/accessories swappable).

---

## What Lyra Gives You Out of the Box

The stock system lives in `LyraGame/Cosmetics/` and contains:

```cpp
// Core component — sits on your pawn, server-replicated
class ULyraPawnComponent_CharacterParts : public UPawnComponent

// Per-part definition struct — describes one cosmetic piece
struct FLyraCharacterPart
{
    TSubclassOf<AActor> PartClass;       // The actor to spawn (mesh actor)
    FGameplayTag       SocketTag;        // Which socket / attach point
    ECharacterCustomizationCollisionMode CollisionMode;  // None / QueryOnly / etc.
};

// Animation layer selection driven by equipped parts
struct FLyraAnimLayerSelectionSet
{
    TArray<FLyraAnimLayerSelectionEntry> LayerRules;
    TSubclassOf<UAnimInstance>           DefaultLayer;

    TSubclassOf<UAnimInstance> SelectLayer(
        const FGameplayTagContainer& CosmeticTags) const;
};

// Controller-side component for client picks (UI selection → server replication)
class ULyraControllerComponent_CharacterParts : public UControllerComponent
```

**What this gives you for free:**
- Network replication of equipped parts
- Spawning/despawning of part actors
- Socket-based attachment to the pawn's mesh
- Animation layer linking based on equipped cosmetics
- Gameplay-tag driven part queries (e.g. "is wearing heavy armor?")

**What you still need to build:**
- The actual part actor classes (your meshes wrapped in an AActor)
- A DataTable / DataAsset catalog of all parts
- Storage of which player owns which parts (entitlement — Phase 9)
- UI for selecting and previewing parts (Phase 7)

---

## Part Actor — BP_<Project>CharacterPart_<PartName>

Each cosmetic piece is an AActor that contains a SkeletalMeshComponent (for
parts that follow body anim, like outfits) or a StaticMeshComponent (for
rigid attachments like helmets, backpacks).

### For Skinned Parts (Outfit, Hair, Accessories that deform)

```cpp
// <Project>CharacterPart_Skinned.h
UCLASS(Blueprintable)
class <PROJECT>GAME_API A<Project>CharacterPart_Skinned : public AActor
{
    GENERATED_BODY()

public:
    A<Project>CharacterPart_Skinned();

    /** Skeletal mesh skinned to SK_Mannequin */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetic")
    TObjectPtr<USkeletalMeshComponent> MeshComponent;

    /** Tags applied while this part is equipped (e.g. Cosmetic.Outfit.Heavy) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    FGameplayTagContainer AppliedTags;

    /** Optional animation layer to link onto the parent character */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    TSubclassOf<UAnimInstance> AnimLayerToLink;

    /** Called when attached to the parent pawn. Override for custom setup. */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Cosmetic")
    void OnAttachedToPawn(APawn* ParentPawn);
};
```

```cpp
// <Project>CharacterPart_Skinned.cpp
A<Project>CharacterPart_Skinned::A<Project>CharacterPart_Skinned()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false; // Replication is handled by the parts component

    MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(
        TEXT("MeshComponent"));
    RootComponent = MeshComponent;

    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetGenerateOverlapEvents(false);
    MeshComponent->SetCanEverAffectNavigation(false);
}

void A<Project>CharacterPart_Skinned::OnAttachedToPawn_Implementation(APawn* ParentPawn)
{
    if (!IsValid(ParentPawn)) { return; }

    ACharacter* ParentChar = Cast<ACharacter>(ParentPawn);
    if (!IsValid(ParentChar)) { return; }

    USkeletalMeshComponent* ParentMesh = ParentChar->GetMesh();
    if (!IsValid(ParentMesh)) { return; }

    // Critical: skinned parts must use the parent's leader pose so they
    // animate identically to the parent body without their own anim instance
    MeshComponent->SetLeaderPoseComponent(ParentMesh, true);
    MeshComponent->SetAnimInstanceClass(nullptr);

    // Match LOD with parent so distant characters drop part LODs together
    MeshComponent->bSyncAttachParentLOD = true;
}
```

### For Rigid Parts (Helmets, Backpacks, Weapons-as-Cosmetic)

```cpp
UCLASS(Blueprintable)
class <PROJECT>GAME_API A<Project>CharacterPart_Rigid : public AActor
{
    GENERATED_BODY()

public:
    A<Project>CharacterPart_Rigid();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cosmetic")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    FName AttachSocketName = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    FGameplayTagContainer AppliedTags;
};
```

Rigid parts attach via socket name (e.g. `head_socket` for helmets,
`spine_socket` for backpacks). The parts component handles the attach call
when the part spawns.

---

## Part Data Asset — DA_<Project>Part_<PartName>

Each part definition is a Data Asset so designers can create new parts
without C++. The asset wraps `FLyraCharacterPart`.

```cpp
UCLASS(BlueprintType)
class <PROJECT>GAME_API U<Project>CharacterPartDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** Display name for shop UI */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
    FText DisplayName;

    /** Description shown in shop tooltip */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
    FText Description;

    /** Thumbnail for shop grid */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
    TSoftObjectPtr<UTexture2D> Thumbnail;

    /** Slot this part occupies (head / torso / legs / etc.) — one part per slot */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    FGameplayTag SlotTag;

    /** Where to attach (socket on parent pawn) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    FGameplayTag SocketTag;

    /** The part actor class — soft ref so it doesn't load until equipped */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    TSoftClassPtr<AActor> PartClass;

    /** Mobile variant (different mesh / mat) — null falls back to PartClass */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    TSoftClassPtr<AActor> PartClassMobile;

    /** Rarity tier — drives shop display + pricing */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Economy")
    FGameplayTag RarityTag; // Cosmetic.Rarity.Common / Rare / Epic / Legendary

    /** Tags applied to the parent pawn while this is equipped */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    FGameplayTagContainer GrantedTags;

    /** GameFeature plugin name this part lives in (for live-ops parts) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
    FString OwningGameFeaturePlugin;

    /** Primary asset bundles — for selective loading */
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(
            FPrimaryAssetType(TEXT("<Project>CharacterPart")),
            GetFName());
    }
};
```

---

## Wiring the Parts Component on Your Pawn

In your character class (extending `ALyraCharacter`):

```cpp
UCLASS()
class <PROJECT>GAME_API A<Project>CosmeticCharacter : public ALyraCharacter
{
    GENERATED_BODY()

public:
    A<Project>CosmeticCharacter();

    /** Equip a part by data asset reference */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Cosmetics")
    void ServerEquipPart(U<Project>CharacterPartDefinition* PartDef);

    /** Remove a part by slot */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Cosmetics")
    void ServerUnequipSlot(FGameplayTag SlotTag);

protected:
    /** Lyra's stock parts component handles network rep + spawning */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<ULyraPawnComponent_CharacterParts> PartsComponent;

private:
    /** Map of currently equipped parts per slot — server authoritative */
    UPROPERTY(Replicated)
    TMap<FGameplayTag, TObjectPtr<U<Project>CharacterPartDefinition>> EquippedParts;

    void ApplyPart(U<Project>CharacterPartDefinition* PartDef);
    void RemovePart(FGameplayTag SlotTag);
};
```

```cpp
A<Project>CosmeticCharacter::A<Project>CosmeticCharacter()
{
    PartsComponent = CreateDefaultSubobject<ULyraPawnComponent_CharacterParts>(
        TEXT("PartsComponent"));
}

void A<Project>CosmeticCharacter::ServerEquipPart_Implementation(
    U<Project>CharacterPartDefinition* PartDef)
{
    if (!IsValid(PartDef)) { return; }

    // ENTITLEMENT CHECK — never trust the client
    if (!HasEntitlementForPart(PartDef))
    {
        UE_LOG(LogCosmetics, Warning,
            TEXT("Player attempted to equip %s without entitlement"),
            *PartDef->GetName());
        return;
    }

    // Remove existing part in same slot
    if (EquippedParts.Contains(PartDef->SlotTag))
    {
        RemovePart(PartDef->SlotTag);
    }

    EquippedParts.Add(PartDef->SlotTag, PartDef);
    ApplyPart(PartDef);
}

void A<Project>CosmeticCharacter::ApplyPart(
    U<Project>CharacterPartDefinition* PartDef)
{
    // Use platform-appropriate part class
    TSoftClassPtr<AActor> PartClass = PartDef->PartClass;
#if PLATFORM_ANDROID || PLATFORM_IOS
    if (!PartDef->PartClassMobile.IsNull())
    {
        PartClass = PartDef->PartClassMobile;
    }
#endif

    // Async load if needed; equip when ready
    if (!PartClass.IsValid())
    {
        UAssetManager::GetStreamableManager().RequestAsyncLoad(
            PartClass.ToSoftObjectPath(),
            FStreamableDelegate::CreateUObject(
                this,
                &A<Project>CosmeticCharacter::OnPartClassLoaded,
                PartDef));
        return;
    }

    // Construct the FLyraCharacterPart entry and hand to the parts component
    FLyraCharacterPart Entry;
    Entry.PartClass    = PartClass.Get();
    Entry.SocketTag    = PartDef->SocketTag;
    Entry.CollisionMode = ECharacterCustomizationCollisionMode::NoCollision;

    PartsComponent->AddCharacterPart(Entry);
}

void A<Project>CosmeticCharacter::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(A<Project>CosmeticCharacter, EquippedParts);
}
```

---

## Slot Tag Hierarchy

Define your cosmetic slots as a GameplayTag hierarchy in `DefaultGameplayTags.ini`:

```ini
[/Script/GameplayTags.GameplayTagsSettings]
+GameplayTagList=(Tag="Cosmetic.Slot.Head",         DevComment="Helmet, hat, mask")
+GameplayTagList=(Tag="Cosmetic.Slot.Torso",        DevComment="Body armor, outfit upper")
+GameplayTagList=(Tag="Cosmetic.Slot.Legs",         DevComment="Outfit lower, leg armor")
+GameplayTagList=(Tag="Cosmetic.Slot.Arms",         DevComment="Gauntlets, sleeves")
+GameplayTagList=(Tag="Cosmetic.Slot.Back",         DevComment="Backpack, cape")
+GameplayTagList=(Tag="Cosmetic.Slot.Face",         DevComment="Face decals, paint")
+GameplayTagList=(Tag="Cosmetic.Slot.Hair",         DevComment="Hair style")
+GameplayTagList=(Tag="Cosmetic.Slot.WeaponSkin",   DevComment="Weapon cosmetic overlay")

+GameplayTagList=(Tag="Cosmetic.Rarity.Common",     DevComment="Free / low-tier")
+GameplayTagList=(Tag="Cosmetic.Rarity.Rare",       DevComment="Mid-tier earnable")
+GameplayTagList=(Tag="Cosmetic.Rarity.Epic",       DevComment="Premium purchase")
+GameplayTagList=(Tag="Cosmetic.Rarity.Legendary",  DevComment="Top tier / event drops")

+GameplayTagList=(Tag="Cosmetic.Socket.HeadAttach",  DevComment="Helmet socket")
+GameplayTagList=(Tag="Cosmetic.Socket.BackAttach",  DevComment="Backpack socket")
+GameplayTagList=(Tag="Cosmetic.Socket.HipAttach",   DevComment="Holster socket")
```

---

## Animation Layer Linking — Parts That Change How You Move

For parts that should affect locomotion (e.g. heavy armor that slows the
character, magical robes with floating gait), use `FLyraAnimLayerSelectionSet`.

```cpp
// In your AnimBP base class
UPROPERTY(EditDefaultsOnly, Category = "Cosmetic Animation")
FLyraAnimLayerSelectionSet LocomotionLayerSelection;

// In the AnimBP's blueprint thread-safe update:
void U<Project>CharacterAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

    // Collect tags from all equipped parts on the owning pawn
    FGameplayTagContainer EquippedTags = GetEquippedCosmeticTags();

    // Pick the right anim layer based on tags
    TSubclassOf<UAnimInstance> NewLayer =
        LocomotionLayerSelection.SelectLayer(EquippedTags);

    if (NewLayer && NewLayer != CurrentLocomotionLayer)
    {
        CurrentLocomotionLayer = NewLayer;
        // Link the layer on game thread
        AsyncTask(ENamedThreads::GameThread, [this, NewLayer]()
        {
            GetOwningComponent()->LinkAnimClassLayers(NewLayer);
        });
    }
}
```

Example layer selection rules in the AnimBP defaults:
```
Default Layer:  ABP_<Project>_Locomotion_Default

Rules:
  - Required Tags: Cosmetic.Outfit.HeavyArmor   → ABP_<Project>_Locomotion_Heavy
  - Required Tags: Cosmetic.Outfit.Robes         → ABP_<Project>_Locomotion_Robes
  - Required Tags: Cosmetic.Outfit.Stealth       → ABP_<Project>_Locomotion_Crouchwalk
```

---

## Why Use Lyra's Parts System Instead of Rolling Your Own

Teams routinely re-implement a parts system from scratch and regret it.
Reasons to use Lyra's:

```
✓ Network replication of equipped parts is solved (server-authoritative)
✓ Spawn/despawn lifecycle is solved (no leaks)
✓ LeaderPoseComponent setup for skinned parts is solved
✓ Animation layer linking based on cosmetics is solved
✓ GameFeature plugin integration works out of the box
✓ Future Lyra updates patch this code, not yours

✗ Custom implementations almost always miss:
   - LOD parity with parent mesh
   - Proper cleanup on pawn destruction
   - Replication ordering bugs (part spawns before mesh ready)
   - Mobile memory churn from synchronous loads
```

---

## Verification Checklist — End of Phase 4

```
□ ULyraPawnComponent_CharacterParts wired on your character
□ Part data asset class defined with soft refs for part classes
□ At least one skinned part actor and one rigid part actor working
□ Equipping a part via server RPC works and replicates to clients
□ Skinned parts inherit LeaderPose from parent mesh (no anim duplication)
□ Rigid parts attach to correct socket via socket tag
□ Slot system enforces one-part-per-slot
□ Animation layer linking changes locomotion when heavy/stealth equipped
□ Removing a part frees the actor (no leaks; verify with Show Memory)
□ Mobile variant path tested on device (PartClassMobile loads correctly)
```
