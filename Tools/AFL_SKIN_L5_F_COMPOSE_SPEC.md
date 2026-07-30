# AFL Robot Skin — L5 Runtime Color (Option F: COMPOSITION) — C++ BUILD SPEC v3

**v3 = FULLY TYPED (UAFLSkinColorAsset, ZERO reflection anywhere) + the two convergence paths stated as BOTH-REQUIRED (part-BeginPlay AND component-OnRep-push), source-grounded. For operator review BEFORE build. Editor closed, UBT, `Result: Succeeded`.**

## What v3 nails down vs v2
- **DECISION applied — zero reflection:** color lives in a BagMan `UAFLSkinColorAsset` with C++-typed `TMap`s + direct getters. No `FindFProperty`, no runtime lookup that can miss. Compile-time-guaranteed read. (Also retires the `ULyraTeamDisplayAsset` type-name reuse — cosmetic color is now a properly-named BagMan type.)
- **CONFIRM A honest framing — the two-channel race is RELOCATED, not eliminated.** Part-BeginPlay self-color is NOT sufficient alone. BOTH paths are LOAD-BEARING:
  - **Part-BeginPlay self-color** → covers **part-arrives-second** (color already replicated when the part spawns).
  - **Component-OnRep-push-to-existing-parts** → covers **color-arrives-second** (part already spawned with null/stale color; OnRep re-pushes to it). **This is the backstop that makes color-after-part converge — load-bearing, not insurance.**
  - Both are idempotent (create-once MID, re-set same params) → safe to fire redundantly when both land close together.
- **CONFIRM B resolved (source):** the part attaches TO the pawn (`NewObject<UChildActorComponent>(pawn)` in `SpawnActorForEntry`), so **the pawn + its `UAFLSkinColorComponent` exist before the part's BeginPlay** → the component is GUARANTEED PRESENT and findable. The only unknown at part-BeginPlay is whether `SkinColor`'s VALUE has replicated — which is the race the OnRep-push backstops.

## F depends on ZERO unexported Lyra symbols AND ZERO reflection
- Find parts: `GetOwner()->GetComponents<UChildActorComponent>()` → `GetChildActor()` → `Cast<AAFLCharacterPartActor>` (engine + our class).
- Apply: engine `ENGINE_API` `UMeshComponent::GetNumMaterials/GetMaterial/CreateAndSetMaterialInstanceDynamic/SetXParameterValue` (confirmed exported).
- Color data: `UAFLSkinColorAsset` (BagMan, C++-typed maps, direct getters) — no reflection, no Lyra type.
- Lyra `OnCharacterPartsChanged`: NOT used. Lyra `GetCharacterPartActors`/`ApplyToActor`: NOT used (unexported).

---

## CLASS 0 — `UAFLSkinColorAsset : public UPrimaryDataAsset` (BagMan; the color param bag, typed)
```cpp
// Header
UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLSkinColorAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|SkinColor")
    TMap<FName, float> ScalarParameters;        // e.g. EmissiveStrength, EdgeGlowMagnitude

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|SkinColor")
    TMap<FName, FLinearColor> ColorParameters;  // EmissiveColor/2/3, TeamColor, EdgeGlowColor

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AFL|SkinColor")
    TMap<FName, TObjectPtr<UTexture>> TextureParameters; // (optional; LogoTexture if ever color-driven)

    // Direct typed getters -- ZERO reflection.
    const TMap<FName, float>& GetScalars() const { return ScalarParameters; }
    const TMap<FName, FLinearColor>& GetColors() const { return ColorParameters; }
    const TMap<FName, TObjectPtr<UTexture>>& GetTextures() const { return TextureParameters; }
};
```
- The 6 `TDA_*` color assets become `UAFLSkinColorAsset` instances (Blue/Green/Purple/Pink/Red param sets). Authored as data assets.

## CLASS 1 — `AAFLCharacterPartActor : public AActor` (the robot BPs reparent to this)
```cpp
// Header
UCLASS()
class AFLCOMBAT_API AAFLCharacterPartActor : public AActor
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;

    // FIX 1 (a) -- OWN-YOUR-MID: cache of the MIDs WE created, keyed by (mesh, slot). ApplySkinColor writes
    // ONLY to MIDs in this cache -- never to a foreign MID another system put in the slot (e.g. the
    // hit-flash / HitPosition0 path on the body mesh). Per-part-instance: the part is destroyed+respawned
    // on mark-change, so a fresh part = a fresh empty cache = correct (no stale cross-life MID).
    UPROPERTY(Transient)
    TMap<TObjectPtr<UMeshComponent>, FAFLSkinMIDSlots> OwnedMIDs;   // FAFLSkinMIDSlots = { TMap<int32,MID> }

public:
    // Apply a typed color asset to THIS part's mesh comps. Engine-only, OWN-MID (create-once, write-only-ours), null-safe.
    // Public so the component's OnRep-push can also call it on already-spawned parts.
    void ApplySkinColor(const UAFLSkinColorAsset* ColorAsset);
};

// Per-mesh owned-MID slot map (USTRUCT so it can live in a UPROPERTY TMap value).
USTRUCT()
struct FAFLSkinMIDSlots
{
    GENERATED_BODY()
    UPROPERTY(Transient)
    TMap<int32, TObjectPtr<UMaterialInstanceDynamic>> SlotMIDs;
};
```
```cpp
// Impl
#include "Cosmetics/AFLSkinColorComponent.h"   // to resolve the pawn's component
#include "Cosmetics/AFLSkinColorAsset.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Pawn.h"

void AAFLCharacterPartActor::BeginPlay()
{
    Super::BeginPlay();
    // PATH 1 of 2 (covers PART-ARRIVES-SECOND): resolve the owning pawn's color component and self-apply
    // the CURRENT color. CONFIRM B: the part is a child-actor attached to the pawn, so the pawn + its
    // UAFLSkinColorComponent exist before this BeginPlay -> the component is present. If SkinColor's VALUE
    // hasn't replicated yet (color-arrives-second), GetSkinColor() is null -> we no-op here, and the
    // component's OnRep-push (PATH 2) applies when the color arrives. BOTH paths required.
    if (AActor* PawnActor = GetParentActor())   // child-actor's parent = the owning pawn
    {
        if (UAFLSkinColorComponent* ColorComp = PawnActor->FindComponentByClass<UAFLSkinColorComponent>())
        {
            ApplySkinColor(ColorComp->GetSkinColor());   // null -> ApplySkinColor early-returns (guard)
        }
    }
}

void AAFLCharacterPartActor::ApplySkinColor(const UAFLSkinColorAsset* ColorAsset)
{
    if (ColorAsset == nullptr) { return; }      // GUARD: no color -> never touch materials -> no MID
    TArray<UMeshComponent*> Meshes;
    GetComponents<UMeshComponent>(Meshes);
    for (UMeshComponent* Mesh : Meshes)
    {
        if (!IsValid(Mesh)) { continue; }
        FAFLSkinMIDSlots& Slots = OwnedMIDs.FindOrAdd(Mesh);   // our cache for this mesh
        const int32 N = Mesh->GetNumMaterials();
        for (int32 i = 0; i < N; ++i)
        {
            // FIX 1 (a) -- OWN-YOUR-MID. Use the MID WE created+cached for this slot. We do NOT reuse a
            // foreign MID found in the slot (e.g. one the hit-flash/HitPosition0 path created) -- writing
            // skin params onto someone else's MID, or theirs stomping ours, is the collision we avoid.
            UMaterialInstanceDynamic* MID = Slots.SlotMIDs.FindRef(i);
            // (Re)create if: we never made one, OR the slot no longer holds OUR MID (a foreign system
            // replaced it -> we re-establish ours; create-once otherwise -> no duplicate, no leak).
            if (!IsValid(MID) || Mesh->GetMaterial(i) != MID)
            {
                MID = Mesh->CreateAndSetMaterialInstanceDynamic(i);   // ENGINE_API
                Slots.SlotMIDs.Add(i, MID);                            // cache OURS
            }
            if (!MID) { continue; }
            for (const TPair<FName,float>& KV : ColorAsset->GetScalars())  { MID->SetScalarParameterValue(KV.Key, KV.Value); }
            for (const TPair<FName,FLinearColor>& KV : ColorAsset->GetColors()) { MID->SetVectorParameterValue(KV.Key, FVector(KV.Value)); }
            for (const TPair<FName,TObjectPtr<UTexture>>& KV : ColorAsset->GetTextures()) { MID->SetTextureParameterValue(KV.Key, KV.Value); }
        }
    }
}
```

## CLASS 2 — `UAFLSkinColorComponent : public UActorComponent` (on the pawn; replicates color)
```cpp
// Header
UCLASS(meta=(BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLSkinColorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UAFLSkinColorComponent();
    UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category="AFL|Cosmetics")
    void SetSkinColor(UAFLSkinColorAsset* NewColor);
    UAFLSkinColorAsset* GetSkinColor() const { return SkinColor; }   // part reads this on BeginPlay (PATH 1)
protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;
    UPROPERTY(ReplicatedUsing=OnRep_SkinColor)
    TObjectPtr<UAFLSkinColorAsset> SkinColor = nullptr;
    UFUNCTION() void OnRep_SkinColor();
    void ReapplyColorToAllParts();   // PATH 2: push current color to already-spawned parts
};
```
```cpp
// Impl
UAFLSkinColorComponent::UAFLSkinColorComponent()
{
    SetIsReplicatedByDefault(true);   // CONFIRM-B(repl): no replicated base -> WE set it (do not omit)
    PrimaryComponentTick.bCanEverTick = false;
}
void UAFLSkinColorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(UAFLSkinColorComponent, SkinColor);
}
void UAFLSkinColorComponent::BeginPlay()
{
    Super::BeginPlay();
    ReapplyColorToAllParts();   // reconcile: if parts + color both already present (late-join), apply now
}
void UAFLSkinColorComponent::SetSkinColor(UAFLSkinColorAsset* NewColor)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        SkinColor = NewColor;
        ReapplyColorToAllParts();   // listen-host: OnRep doesn't fire on authority -> apply locally now
    }
}
// PATH 2 of 2 (covers COLOR-ARRIVES-SECOND): when the color value replicates in, push to already-spawned
// parts that read null/stale at their BeginPlay. LOAD-BEARING -- without this, color-after-part desyncs.
void UAFLSkinColorComponent::OnRep_SkinColor() { ReapplyColorToAllParts(); }

void UAFLSkinColorComponent::ReapplyColorToAllParts()
{
    if (SkinColor == nullptr) { return; }
    if (AActor* Owner = GetOwner())
    {
        TArray<UChildActorComponent*> CACs;
        Owner->GetComponents<UChildActorComponent>(CACs);
        for (UChildActorComponent* CAC : CACs)
        {
            // EDIT 1 FILTER (by-construction): only OUR body parts.
            if (AAFLCharacterPartActor* Part = Cast<AAFLCharacterPartActor>(CAC ? CAC->GetChildActor() : nullptr))
            {
                Part->ApplySkinColor(SkinColor);   // idempotent (create-once MID); safe if also self-colored
            }
        }
    }
}
```

## CLASS 3 — `UAFLSkinColorControllerComponent : public UControllerComponent` (persistent home)
`UControllerComponent` is `ENGINE_API`. Holds `UPROPERTY() TObjectPtr<UAFLSkinColorAsset> PersistentSkinColor;` (authority), binds the public `AController::OnPossessedPawnChanged` (engine delegate, `AddDynamic` linkable -- NOT reflection), re-pushes on possess -> survives respawn.
```cpp
void UAFLSkinColorControllerComponent::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
        if (AController* C = GetController<AController>())
        {
            C->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChanged);
            if (APawn* P = GetPawn<APawn>()) { PushToPawn(P); }
        }
}
void UAFLSkinColorControllerComponent::OnPossessedPawnChanged(APawn*, APawn* New) { if (New) PushToPawn(New); }
void UAFLSkinColorControllerComponent::SetPersistentSkinColor(UAFLSkinColorAsset* C)
{
    if (GetOwner() && GetOwner()->HasAuthority()) { PersistentSkinColor = C; if (APawn* P=GetPawn<APawn>()) PushToPawn(P); }
}
void UAFLSkinColorControllerComponent::PushToPawn(APawn* P) const
{
    if (P) if (UAFLSkinColorComponent* SC = P->FindComponentByClass<UAFLSkinColorComponent>()) SC->SetSkinColor(PersistentSkinColor);
}
```

---

## CONFIRM A — BOTH convergence paths are REQUIRED + idempotent (the honest framing)
The two-channel race (FastArray part vs UPROPERTY color) is RELOCATED into these two paths, NEITHER sufficient alone:
| Race order | Covered by | Why required |
|---|---|---|
| **Part arrives second** (color already replicated) | **PATH 1: part `BeginPlay` self-color** | reads the present color, applies on spawn |
| **Color arrives second** (part already spawned, read null) | **PATH 2: component `OnRep_SkinColor` → push to existing parts** | the part already ran BeginPlay with null; ONLY OnRep re-applies. LOAD-BEARING. |
| Both ~same frame | either/both fire | idempotent (create-once MID, re-set same params) -> redundant fire harmless |
- **Both paths are in the code above** (part BeginPlay calls `ApplySkinColor(GetSkinColor())`; component OnRep calls `ReapplyColorToAllParts()`). Stated explicitly: part-BeginPlay alone is NOT sufficient; the OnRep-push is the backstop for color-after-part, not insurance.

## CONFIRM B — read-timing resolves (source-grounded)
- `SpawnActorForEntry`: `NewObject<UChildActorComponent>(OwnerComponent->GetOwner())` + `SetupAttachment(pawn mesh)` + `RegisterComponent()` -> the part **attaches TO the pawn**. The pawn (and thus its `UAFLSkinColorComponent`, a ctor/spawn-time component) **exists before the part spawns**.
- Part resolves the component: `GetParentActor()` (child-actor's owner = pawn) -> `FindComponentByClass<UAFLSkinColorComponent>()`. **GUARANTEED present at part BeginPlay.**
- The ONLY unknown at part BeginPlay is whether `SkinColor`'s VALUE has replicated. If not -> `GetSkinColor()` null -> part no-ops -> **PATH 2 (OnRep-push) applies when it arrives.** Covered.

## CONFIRM 4 + FIX 1 — create-once AND own-your-MID (two DIFFERENT properties, both covered)
- **Create-once (no duplicate MIDs):** we cache the MID per (mesh, slot) and only create when absent -> repeated PATH 1 + PATH 2 calls reuse -> no leak, idempotent in cost.
- **FIX 1 (a) OWN-YOUR-MID (no foreign collision):** we write ONLY to MIDs in our `OwnedMIDs` cache, created by us via `CreateAndSetMaterialInstanceDynamic`. We do NOT `Cast<MID>` whatever is in the slot and write to it -- because the body mesh has a foreign MID path (the hit-flash / `HitPosition0`), and writing skin params onto its MID (or having it stomp ours) is a real, single-client-invisible collision. The check `Mesh->GetMaterial(i) != MID` detects if a foreign system replaced our MID in the slot and re-establishes ours. So: we never write to a MID we didn't make, and a foreign MID in the slot triggers re-creation of ours rather than a write to theirs.
  - **Why (a) not (b):** I could not prove (b) ("nothing else MIDs these slots") -- `HitPosition0` is exposed on the body master and the hit/number-pop paths are BP-driven (unreadable in full), so a negative proof is fragile + future-breaking (a new status tint would silently collide). (a) is safe-by-construction regardless of what any other system does, now or later. Marginal cost = one `TMap` per part.
  - **Lifetime:** `OwnedMIDs` is per-part-instance + `Transient`. The part is destroyed+respawned on mark-change -> fresh part = fresh empty cache -> no stale cross-life MID. Correct by the same respawn mechanism that drives PATH 1.
  - **Caveat (acknowledged, benign):** if the hit-flash and our skin both want the SAME mesh slot's color simultaneously, last-writer-wins per-param -- but they write DIFFERENT params (skin: EmissiveColor*/TeamColor/LogoTexture; hit-flash: HitPosition0), and we now each own our MID, so there is no shared-MID stomp. (If they ever shared a param, that'd be a design conflict to resolve separately -- not the case here.)

## CONFIRM 5 — respawn + late-join converge (source-grounded)
- **Mark-change respawn:** `PostReplicatedChange` destroy+respawns the part -> new `AAFLCharacterPartActor::BeginPlay` (PATH 1) self-colors from the already-replicated pawn color. (If color changed same frame, PATH 2 also fires.) New part colored. ✓
- **Late-join:** joiner gets pawn -> `SkinColor` replicates -> OnRep (PATH 2) + component BeginPlay reconcile; parts replicate+spawn -> each part BeginPlay (PATH 1). Multiple convergence points, none order-dependent. ✓
- **Respawn-persistence:** controller `PersistentSkinColor` re-pushed each possess -> new pawn component -> parts color. ✓

---

## BUILD CHECKLIST (operator verify post-build)
- [ ] `UAFLSkinColorAsset : UPrimaryDataAsset` — typed maps + direct getters. ZERO reflection.
- [ ] `AAFLCharacterPartActor : AActor` — `BeginPlay` PATH 1 self-color via `GetParentActor()->FindComponentByClass`; `ApplySkinColor` engine-only create-once MID + null-guard.
- [ ] `UAFLSkinColorComponent : UActorComponent` — ctor `SetIsReplicatedByDefault(true)`; Super + `DOREPLIFETIME(SkinColor)`; `SkinColor` ReplicatedUsing=OnRep; OnRep PATH 2 push; `ReapplyColorToAllParts` `Cast<AAFLCharacterPartActor>` filter; `SetSkinColor` authority + listen-host local apply.
- [ ] `UAFLSkinColorControllerComponent : UControllerComponent` — persistent color, `OnPossessedPawnChanged` AddDynamic (engine delegate), possess re-push, authority-gated.
- [ ] BOTH PATH 1 (part BeginPlay) AND PATH 2 (component OnRep) present + idempotent.
- [ ] ZERO unexported Lyra symbols; ZERO reflection (no FindFProperty anywhere). Link clean.
- [ ] Build: editor closed, PowerShell UBT, `Result: Succeeded`.

## POST-BUILD (editor) — reparent + per-robot verify
- [ ] Reparent the 6 `B_AFL_Robot_*` BPs to `AAFLCharacterPartActor`. Verify EACH by disk read (parent class = AAFLCharacterPartActor).
- [ ] **Re-confirm single-client render on at least one robot post-reparent** (a reparent that silently broke a mesh/ref would otherwise surface only at the wire proof — cheap insurance).
- [ ] Author the 5 `UAFLSkinColorAsset` color assets (Blue/Green/Purple/Pink/Red param sets) -- or migrate the existing TDA values.

## Wire proof (after build) — F v3
ARIA×Green, 2 clients: convergence + Race A (color live) + **Race B watched on a REMOTE client for the respawn default-material flash** (PATH 1 self-color should beat the visible frame; flash -> add a first-tick/deferred apply) + Race C (late-join). Real latency, both-client shots.

## Shared Lyra: byte-clean, never forked. ZERO unexported symbols, ZERO reflection. (E rejected; reflection-bind rejected.)
