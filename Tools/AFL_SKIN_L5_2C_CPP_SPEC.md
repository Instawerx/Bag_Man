# AFL Robot Skin — L5 Runtime Color Selection (Shape 2c) — C++ BUILD SPEC

**For operator review BEFORE build. Pillar replication code — must be airtight. Editor closed, UBT, `Result: Succeeded`.**

## Goal
Independent runtime **color** selection (`EmissiveColor*`/`TeamColor`/`EdgeGlow`) riding the **same canonical CharacterParts foundation** as the MARK, **race-safe on the wire** (no ping-dependent skin-desync), **shared Lyra source byte-clean** (subclass, never fork). Mark = which part actor (existing replicated FastArray). Color = a replicated selector on a BagMan-owned pawn-component subclass, applied per-client.

---

## REVIEW CONFIRMATIONS (4) — grounded in engine source, addressed in this spec

### CONFIRM 1 — `ULyraTeamDisplayAsset` is TYPE-REUSE ONLY, ZERO team-system coupling (NOT Option T)
**Source-verified (`LyraTeamDisplayAsset.cpp`):** `ApplyToActor`/`ApplyToMeshComponent`/`ApplyToMaterial` touch **NO** `ULyraTeamSubsystem`, **NO** team ID, **NO** `ServerCreateTeam`/team registration. They are pure param-bag application: iterate `ScalarParameters`/`ColorParameters`/`TextureParameters` and `SetXParameterValueOnMaterials`. The ONLY team-subsystem reference in the whole class is in `PostEditChangeProperty` (`#if WITH_EDITOR`, **editor-time only** — fires when you edit the asset in the editor; never at runtime).
- **Therefore:** `SkinColor` is a **data-asset REFERENCE we read params off and apply via `ApplyToMeshComponent`** — we use the type as a *param container only*. We do **NOT** call `ServerCreateTeam`, do **NOT** assign team IDs, do **NOT** register with `ULyraTeamSubsystem`, do **NOT** use `AsyncAction_ObserveTeamColors`. **No team-system coupling whatsoever.** Option T stays rejected; this is just type-reuse of a convenient param bag.
- *(Optional purity: if even the type-name association is unwanted, we could later define `UAFLSkinColorAsset` with the same 3 maps + an `ApplyToMeshComponent`. Not needed for correctness — the runtime path is team-coupling-free as-is — but noted. For now: reuse `ULyraTeamDisplayAsset` as a pure param bag.)*

### CONFIRM 2 — MID lifecycle: create-ONCE, reuse-thereafter (idempotent in COST, not just values) ✅ + guard
**Source-verified (`ApplyToMeshComponent`):** it does `UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MaterialInterface); if (!DynamicMaterial) { DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(MaterialIndex); }`.
- **First apply:** slot holds the static MI → cast fails → creates ONE MID, sets it on the slot.
- **Subsequent applies (same part actor):** slot now holds THAT MID → cast **succeeds** → **reuses it**, only sets params. **No new MID. No leak. No hitch.** Repeated firing (part+color+possess landing together) = re-set the same params on the same MID = genuinely cheap.
- **On mark-change respawn:** the OLD part actor is destroyed (its MID GC'd with it); the NEW part actor starts with the static MI → first re-apply creates ONE MID on the new actor. Correct — one MID per live part, not accumulating.
- **THE GUARD (spec'd in `ReapplyColorToAllParts`):** `if (SkinColor == nullptr) return;` BEFORE touching any material — so we **never create a MID when there's no color to apply** (no empty-MID churn during the color-not-yet-replicated window). MID is created only when there's a real color to set.
- **Conclusion:** redundant firing is harmless in **cost** (reuse) AND **values** (idempotent). The race-safety is free, not just logically free.

### CONFIRM 3 — possess-order closed BY DESIGN (authority-side push in the same flow), nets = insurance only
The persistent color is pushed **on AUTHORITY** as part of the **same possess flow** that re-adds parts: `AFL_OnPossessedPawnChanged` (authority-gated) calls `PawnComp->SetSkinColor(PersistentSkinColor)` on the new pawn's component. `SetSkinColor` sets the **replicated** `SkinColor` server-side → it replicates to all clients **naturally alongside** the FastArray parts (both are server-set during possess, both replicate down). **The CLIENT never depends on a local ordering** — it just receives `SkinColor` (→ OnRep) and parts (→ OnCharacterPartsChanged) in whatever order the network delivers, and the idempotent fn converges them. The *server* sets both during possess; the *client* is order-independent by construction (two replicated values + one convergent apply). The base-vs-subclass delegate bind order only affects **when the server pushes color relative to parts on the server** — and since both are server-side replicated state, the client converges regardless. **Nets (idempotency + self-color) are belt-and-suspenders, not the mechanism.**

### CONFIRM 4 — INITIAL REPLICATION / LATE-JOIN explicitly covered
A client **joining a match in progress** where the pawn ALREADY has a color + mark set server-side:
- On join, the pawn + its `UAFLPawnComponent_CharacterParts` replicate to the joiner. **`SkinColor`'s initial value replicates** → **`OnRep_SkinColor` fires on the joiner** (RepNotify fires for the initial value, not just changes). The **FastArray parts replicate** → **`OnCharacterPartsChanged` fires** as parts spawn on the joiner. Both call `ReapplyColorToAllParts()` → **the joiner converges to the existing mark×color.**
- Plus `BeginPlay`'s `ReapplyColorToAllParts()` (covers parts-already-present-at-BeginPlay) AND the part's self-color BeginPlay (covers part-spawns-with-color-already-replicated). **Three independent convergence points for the late-joiner**, none order-dependent.
- **This is race test C** (late-join) in the wire proof. Distinct from "color changed while watching" — a real separate 2-client path, explicitly handled.

## Grounding (confirmed from engine source)
- `ULyraPawnComponent_CharacterParts`: ctor calls `SetIsReplicatedByDefault(true)`; `GetLifetimeReplicatedProps` does `DOREPLIFETIME(ThisClass, CharacterPartList)` (the FastArray). 
- `OnCharacterPartsChanged` (BlueprintAssignable, `FLyraSpawnedCharacterPartsChanged`) fires from `BroadcastChanged()` **after** all `SpawnActorForEntry` complete in `PostReplicatedAdd`/`PostReplicatedChange` — so spawned part actors + their `SpawnedComponent` are constructed and reachable via `GetCharacterPartActors()` at hook time.
- `PostReplicatedChange` does **DestroyActorForEntry + SpawnActorForEntry** (destroy+respawn) on any entry change → a NEW part actor with DEFAULT materials until color re-applies.
- `GetCharacterPartActors()` returns spawned part actors (iterates `CharacterPartList.Entries[].SpawnedComponent->GetChildActor()`).
- Controller `ULyraControllerComponent_CharacterParts::OnPossessedPawnChanged(Old,New)` is **authority-only**, re-adds parts to the new pawn → the per-possess re-push hook (survives respawn).

---

## COMPONENT 1 — `UAFLPawnComponent_CharacterParts : public ULyraPawnComponent_CharacterParts`
**Location:** `Plugins/GameFeatures/AFLCombat/.../Public+Private/Cosmetics/` (or AFLCore — wherever the AFL cosmetic code lives; BagMan-owned, NOT LyraGame). Shared Lyra untouched.

### Header
```cpp
#pragma once
#include "Cosmetics/LyraPawnComponent_CharacterParts.h"
#include "AFLPawnComponent_CharacterParts.generated.h"

class ULyraTeamDisplayAsset; // reuse Lyra's param-bag type as the color carrier (ScalarParameters/ColorParameters/TextureParameters)

UCLASS(meta=(BlueprintSpawnableComponent))
class UAFLPawnComponent_CharacterParts : public ULyraPawnComponent_CharacterParts
{
    GENERATED_BODY()
public:
    UAFLPawnComponent_CharacterParts(const FObjectInitializer& OI = FObjectInitializer::Get());

    // AUTHORITY-ONLY: set the active color selection (server). Replicates to all clients.
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AFL|Cosmetics")
    void SetSkinColor(ULyraTeamDisplayAsset* NewColor);

    UFUNCTION(BlueprintPure, Category="AFL|Cosmetics")
    ULyraTeamDisplayAsset* GetSkinColor() const { return SkinColor; }

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    // The color selection (a ULyraTeamDisplayAsset = name->value param bag). Soft? No: hard ref is fine,
    // it's a tiny data asset; replicates as an object ref. Replicated so every client knows the choice.
    UPROPERTY(ReplicatedUsing=OnRep_SkinColor)
    TObjectPtr<ULyraTeamDisplayAsset> SkinColor = nullptr;

    UFUNCTION()
    void OnRep_SkinColor();

    // CORRECTNESS 1: the ONE idempotent re-apply. Reads CURRENT SkinColor + CURRENT spawned parts,
    // applies whatever is present. Safe no-op if SkinColor==null OR zero parts spawned.
    // Called by BOTH OnRep_SkinColor AND the OnCharacterPartsChanged handler. Idempotent: a 2nd/3rd
    // call re-applies the same values harmlessly. THIS is what makes it race-PROOF.
    void ReapplyColorToAllParts();

    UFUNCTION()
    void HandleCharacterPartsChanged(ULyraPawnComponent_CharacterParts* Comp);
};
```

### Implementation (key bodies)
```cpp
UAFLPawnComponent_CharacterParts::UAFLPawnComponent_CharacterParts(const FObjectInitializer& OI)
    : Super(OI)
{
    // Base already SetIsReplicatedByDefault(true); inherited. (No need to repeat, but harmless to assert.)
}

void UAFLPawnComponent_CharacterParts::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);          // keeps CharacterPartList replicating
    DOREPLIFETIME(UAFLPawnComponent_CharacterParts, SkinColor);
}

void UAFLPawnComponent_CharacterParts::BeginPlay()
{
    Super::BeginPlay();
    // Bind the per-client parts-changed hook (fires on every client after parts spawn/respawn).
    OnCharacterPartsChanged.AddDynamic(this, &UAFLPawnComponent_CharacterParts::HandleCharacterPartsChanged);
    // Cover the case where parts already exist by BeginPlay (late join / initial replication already arrived):
    ReapplyColorToAllParts();
}

void UAFLPawnComponent_CharacterParts::SetSkinColor(ULyraTeamDisplayAsset* NewColor)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        SkinColor = NewColor;
        // Server is also a client (listen host): apply locally now; OnRep won't fire on the authority.
        ReapplyColorToAllParts();
    }
}

void UAFLPawnComponent_CharacterParts::OnRep_SkinColor()
{
    // Color channel arrived (possibly before OR after the parts). Re-apply to whatever parts exist now.
    ReapplyColorToAllParts();
}

void UAFLPawnComponent_CharacterParts::HandleCharacterPartsChanged(ULyraPawnComponent_CharacterParts* /*Comp*/)
{
    // Parts channel changed (spawn/respawn, possibly before OR after the color). Apply current color.
    ReapplyColorToAllParts();
}

// CORRECTNESS 1: single idempotent function (both hooks call it). CORRECTNESS 2: MID-safe.
// Called after BroadcastChanged (post-spawn) AND on OnRep AND on BeginPlay. Idempotent in VALUES and COST.
void UAFLPawnComponent_CharacterParts::ReapplyColorToAllParts()
{
    // GUARD (CONFIRM 2): never touch materials -> never create a MID -> when no color is set yet.
    // This is the safe no-op for the "color not replicated yet / zero parts" race windows.
    if (SkinColor == nullptr) { return; }
    for (AActor* PartActor : GetCharacterPartActors())          // current spawned parts (may be empty -> loop no-ops)
    {
        if (!IsValid(PartActor)) { continue; }
        // ApplyToActor -> ApplyToMeshComponent: CREATE-ONCE the MID (Cast<MID> reuses if already created),
        // then SetXParameterValue. Repeated calls REUSE the same MID (no leak/hitch). Source-verified.
        SkinColor->ApplyToActor(PartActor, /*bIncludeChildActors=*/true);
    }
}
```

**Why this is race-PROOF (CORRECTNESS 1):**
- Two channels: parts (FastArray) + `SkinColor` (UPROPERTY). They arrive in any order, each fires its hook.
- **Both hooks call the SAME `ReapplyColorToAllParts()`**, which reads *current* state and applies whatever is present:
  - Parts-first: `HandleCharacterPartsChanged` runs (color maybe null -> no-op), then `OnRep_SkinColor` runs -> applies. ✓
  - Color-first: `OnRep_SkinColor` runs (zero parts -> loop no-ops), then `HandleCharacterPartsChanged` runs on spawn -> applies. ✓
  - Redundant re-fire: re-applies identical values, harmless (idempotent). ✓
- No "apply on A, re-apply on B" divergent paths — one function, two callers.

---

## COMPONENT 2 — controller-side persistent color home (survives respawn)
**Where the SELECTION lives** (authority, persists across pawn death — like the part selection). Two options:

**Option α (preferred, minimal):** a BagMan controller component
`UAFLControllerComponent_CharacterParts : public ULyraControllerComponent_CharacterParts` that holds
`UPROPERTY() TObjectPtr<ULyraTeamDisplayAsset> PersistentSkinColor;` (authority-only, NOT replicated — it's selection state, the pawn component replicates the active value), and on possess pushes it to the pawn component.

```cpp
// Override the possess hook (or add our own bind). The base OnPossessedPawnChanged is private; bind our own:
void UAFLControllerComponent_CharacterParts::BeginPlay()
{
    Super::BeginPlay();   // base binds its OnPossessedPawnChanged (re-adds parts)
    if (HasAuthority())
        if (AController* C = GetController<AController>())
            C->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::AFL_OnPossessedPawnChanged);
}

void UAFLControllerComponent_CharacterParts::AFL_OnPossessedPawnChanged(APawn* Old, APawn* New)
{
    // After the base re-adds parts to New, push the persistent color onto the new pawn's AFL component.
    if (New)
        if (auto* PawnComp = New->FindComponentByClass<UAFLPawnComponent_CharacterParts>())
            PawnComp->SetSkinColor(PersistentSkinColor);   // authority -> replicates -> all clients re-apply
}

// AUTHORITY: the selection API the game/UI calls
void UAFLControllerComponent_CharacterParts::SetPersistentSkinColor(ULyraTeamDisplayAsset* Color)
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        PersistentSkinColor = Color;
        if (APawn* P = GetPawn<APawn>())
            if (auto* PawnComp = P->FindComponentByClass<UAFLPawnComponent_CharacterParts>())
                PawnComp->SetSkinColor(Color);
    }
}
```
**Respawn-survival:** `PersistentSkinColor` lives on the Controller (survives pawn death). On each possess, `AFL_OnPossessedPawnChanged` re-pushes it to the new pawn's `UAFLPawnComponent_CharacterParts` → `SetSkinColor` → replicates → all clients re-apply. **Same lifecycle as the part selection** (proven by Block B respawn).

**⚠️ ORDER NOTE for the operator:** confirm `AFL_OnPossessedPawnChanged` runs AFTER the base class's part re-add (so parts exist when we push color). Both bind to `OnPossessedPawnChanged`; AddDynamic order = bind order. Since we `Super::BeginPlay()` first (base binds first), base fires first (parts re-added), then ours (color pushed). If engine delegate ordering is not guaranteed, the **idempotent re-apply + part-self-color (below) cover it anyway** — but flag to verify.

---

## CORRECTNESS 2 — destroy+respawn timing + part-self-color INSURANCE
Source confirms `OnCharacterPartsChanged`/`BroadcastChanged` fires AFTER `SpawnActorForEntry` completes (part actor + `SpawnedComponent` exist, reachable via `GetCharacterPartActors()`). So the hook hits a constructed part. **BUT** `UChildActorComponent.GetChildActor()` construction vs the child actor's own `BeginPlay`/material settle can be a frame-sensitive surface on the wire. **Belt-and-suspenders (SPEC IN):**

**The body part actor self-colors on its own BeginPlay** from a value it can read off the owning pawn component:
```cpp
// In the robot body part actor (the B_AFL_Robot_* actor, or a small C++ base for it):
void AAFLCharacterPartActor::BeginPlay()
{
    Super::BeginPlay();
    // Self-apply current color if the owning pawn already has one (covers hook-races-spawn).
    if (auto* PawnComp = GetOwningAFLPawnComponent())   // walk up attach chain to the pawn's UAFLPawnComponent_CharacterParts
        if (ULyraTeamDisplayAsset* Color = PawnComp->GetSkinColor())
            Color->ApplyToActor(this, true);
}
```
→ Even if the component hook races the spawn, the part **self-colors** when it begins play from the already-replicated `SkinColor`. Combined with the component's idempotent re-apply = **two independent guarantees the part ends up colored**, neither order-dependent.

**Operator decision to confirm:** is the part-self-color needed, or is `OnCharacterPartsChanged`-post-spawn timing guaranteed sufficient? **Spec recommends INCLUDING it** (pillar = belt-and-suspenders; the cost is ~6 lines and it removes the last frame-timing risk). If the body part is a pure-BP actor (`B_AFL_Robot_*`), this can be a BP `BeginPlay` node instead of C++ — either is fine; the point is the part self-applies.

---

## Replication checklist (operator verify post-build)
- [ ] `UAFLPawnComponent_CharacterParts` inherits `SetIsReplicatedByDefault(true)` (from base) — confirm component replicates.
- [ ] `GetLifetimeReplicatedProps` calls `Super::` AND `DOREPLIFETIME(UAFLPawnComponent_CharacterParts, SkinColor)`.
- [ ] `SkinColor` is `ReplicatedUsing=OnRep_SkinColor`.
- [ ] `OnRep_SkinColor` + `HandleCharacterPartsChanged` BOTH call `ReapplyColorToAllParts()` (the one idempotent fn).
- [ ] `SetSkinColor`/`SetPersistentSkinColor` are `BlueprintAuthorityOnly` / `HasAuthority()`-gated.
- [ ] Listen-host: `SetSkinColor` applies locally on authority (OnRep doesn't fire on the server).
- [ ] The BagMan pawn (`B_Hero_BagMan`) uses `UAFLPawnComponent_CharacterParts` (swap the cosmetics component class) and the controller uses `UAFLControllerComponent_CharacterParts` — wire via the experience's AddComponents (BagMan-owned) — confirm these REPLACE the stock Lyra cosmetic components, not duplicate.
- [ ] Build: editor closed, PowerShell UBT, `Result: Succeeded`.

## What stays the same (unchanged, proven)
- The MARK still = which part actor (`B_AFL_Robot_<mark>` via `AddCharacterPart`), replicated FastArray. (Mark could later also become a param on a generic part — out of scope; mark-as-part is proven.)
- The color VALUES = a `ULyraTeamDisplayAsset` (e.g. `TDA_AFL_<Color>`) carrying `ColorParameters`(EmissiveColor*/TeamColor/EdgeGlowColor) + `ScalarParameters`(EmissiveStrength*/EdgeGlowMagnitude). Reuses the proven `ApplyToActor` local primitive — now driven race-safe per-client.
- Shared `M_Mannequin`/`MF_logo`/`SK_Mannequin`/`FLyraCharacterPart`/all Lyra source: **byte-clean, never forked.**

## The wire proof (after build)
2-client session, non-default combo **ARIA mark × Green color**:
1. Server: `AddCharacterPart(ARIA-mark part)` + `SetPersistentSkinColor(TDA_AFL_Green)`.
2. **Both clients** converge to ARIA-mark + Green (identical), pawn controls on both.
3. **Race test A (color on already-spawned pawn):** change color server-side on a live pawn → both clients re-color (OnRep re-applies).
4. **Race test B (mark change forces respawn):** change the mark part → PostReplicatedChange destroy+respawn → both clients land on new mark STILL Green (HandleCharacterPartsChanged + part-self-color re-apply).
5. **Race test C (LATE-JOIN):** with the pawn already ARIA×Green server-side, a SECOND client CONNECTS to the in-progress session → the joiner converges to ARIA×Green (initial OnRep_SkinColor + OnCharacterPartsChanged as parts replicate in + BeginPlay re-apply + part-self-color). Distinct from "color changed while watching."
6. Proven ON THE WIRE (real 2-client), not single-PIE-once.
