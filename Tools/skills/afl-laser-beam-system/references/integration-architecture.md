# Integration Architecture — Beam FX wired the Lyra-canonical way

This is how the marketplace Niagara library connects to AFL's existing
authoritative combat. Read alongside `pack-inventory.md` (the param surface).

## The boundary

```
   AUTHORITATIVE HALF (already built, do not touch)        COSMETIC HALF (this skill)
   ───────────────────────────────────────────────        ──────────────────────────
   UAFLAG_Laser_Pulse / _Beam                              GameplayCue notifies
     client-predicted local trace ───────┐                  AAFLCueNotify_LaserBeam (looping)
     ServerSetReplicatedTargetData        │  predicted       AAFLCueNotify_LaserImpact (burst)
     lag-comp server re-trace             │  impact point         │
     UAFLDamageExecCalc (damage)          └────── cue params ──────┘ drives:
     heat / telemetry                        (Location, Normal,        User.Beam End  (Vector)
                                              SourceObject=weapon)      User.Color     (LinearColor)
```

Only two scalars cross the line: a **world-space point** and a **color**. Damage
never sees Niagara. Niagara never sees an attribute. If you ever find yourself
writing `ApplyGameplayEffect` inside a cue, or `SpawnSystemAttached` inside an
ability, stop — the layers have leaked.

## GameplayCue tags

Add to the AFL tags table (plugin-local `Config/Tags/`, same place as the existing
51-tag bundle):

```
GameplayCue.Weapon.Laser.Beam      # looping — the continuous beam (channeled weapons)
GameplayCue.Weapon.Laser.BeamFlash # burst   — single muzzle→impact flash (hitscan Pulse)
GameplayCue.Weapon.Laser.Impact    # burst   — sparks/decal at the hit point
GameplayCue.Weapon.Laser.Muzzle    # burst   — muzzle/charge orb (OrbType systems)
```

## CRITICAL — how the cue notify is DISCOVERED (a C++ class alone does NOT fire)

> The `AAFLCueNotify_*` classes below are **parent C++ classes only.** A bare C++
> notify class is **NOT discoverable by itself** in this project. To fire, each needs
> a tagged **`GCN_AFL_*.uasset`** parented to it, saved under a **scanned
> `GameplayCueNotifyPaths` folder**.

This is project-specific and load-bearing (confirmed in BAG MAN, 2026-05-31): Lyra's
`LyraGameplayCueManager` is **asset-scan + path-based, not C++-class-scanned** (a
deliberate perf optimization — it skips the full class-library scan). `DefaultGame.ini`
lists only:

```
+GameplayCueNotifyPaths=/Game/GameplayCueNotifies
+GameplayCueNotifyPaths=/Game/GameplayCues
```

So a `UGameplayCueNotify_*` subclass living in a code plugin's mount (e.g. `/AFLVFX/`)
is invisible to the manager — the path isn't scanned, and a bare C++ class carries no
GameplayCue tag binding. **The working AFL cues fire because they are `GCN_AFL_*`
*assets* in `/Game/GameplayCues/` (a scanned path), with the tag derived from the asset
name** (`DeriveGameplayCueTagFromAssetName` — the asset's `gameplay_cue_tag` reads empty
on the CDO; the name is the binding).

**The rule for every laser cue:**
1. The behavior lives in the C++ `AAFLCueNotify_*` parent class (in the always-on
   `AFLVFX` plugin — see why in the install doctrine).
2. Create a `GCN_AFL_Laser_<Thing>.uasset` **parented to that class**, named so its tag
   derives to `GameplayCue.Weapon.Laser.<Thing>`, **saved in `/Game/GameplayCues/`**.
   Fastest safe path: duplicate a working `GCN_AFL_Pulse_*` asset, rename, re-parent —
   the tag-derivation mechanism is inherited identically.
3. The ability emits the tag (`AddGameplayCue` / `K2_ExecuteGameplayCueWithParams`); the
   manager maps tag → the GCN asset → spawns it → runs the C++ behavior.

**No `DefaultGame.ini` edit is needed** if you follow this (asset in the already-scanned
`/Game/GameplayCues/`). Do NOT instead add the plugin path to `GameplayCueNotifyPaths` —
that diverges from the proven precedent for no benefit. If a cue won't fire on a clean
build, **suspect #1 is "no tagged GCN asset in a scanned path," not the C++.**

## Why a cue and not a spawn-in-ability

GameplayCues are GAS's replicated, predicted, net-decoupled cosmetic channel. Trigger
a cue on the locally-predicting client and it plays instantly; it also replicates to
everyone else with no extra netcode. Spawn Niagara directly in the ability and you
own all of that plumbing yourself and you couple gameplay to art. The cue is free
correctness.

## The weapon describes its own look (data-driven, no hardcoding)

The cue must not hardcode which Niagara system or color to use — different lasers look
different. The weapon supplies that. Define a tiny interface the weapon/equipment
implements, and have the cue read from the cue's `SourceObject`:

```cpp
// AFLVFX/Public/AFLLaserVisualProvider.h
UINTERFACE(BlueprintType)
class UAFLLaserVisualProvider : public UInterface { GENERATED_BODY() };

class IAFLLaserVisualProvider
{
    GENERATED_BODY()
public:
    /** The beam Niagara system for this weapon (e.g. NS_AFL_Laser_Twist). */
    virtual UNiagaraSystem* GetBeamSystem() const = 0;
    /** Optional muzzle/charge orb system (OrbType), or nullptr. */
    virtual UNiagaraSystem* GetMuzzleSystem() const = 0;
    /** Beam tint -> drives User.Color. */
    virtual FLinearColor GetBeamColor() const = 0;
    /** Socket on the weapon mesh the beam emits from. */
    virtual FName GetMuzzleSocketName() const = 0;
    /** Cosmetic max range for the local end-point trace (NOT the damage range). */
    virtual float GetCosmeticRange() const = 0;
};
```

Implement it on the AFL laser weapon's `ULyraEquipmentInstance` (or its cosmetic
config data asset). Now one generic cue serves every laser weapon, and adding a new
laser look is pure data — exactly the "configure, don't reinvent" goal.

## The looping beam cue (channeled weapons)

`AGameplayCueNotify_Actor` subclass. It owns the Niagara, attaches to the muzzle,
and updates the end-point each frame with a **cosmetic-only** trace (the damage trace
lives in the ability; this one just decides where the beam visually lands). Production
skeleton:

```cpp
// AFLVFX/Public/AFLCueNotify_LaserBeam.h
UCLASS()
class AAFLCueNotify_LaserBeam : public AGameplayCueNotify_Actor
{
    GENERATED_BODY()
public:
    AAFLCueNotify_LaserBeam();

    virtual bool OnActive_Implementation(AActor* Target,
        const FGameplayCueParameters& Params) override;
    virtual bool OnRemove_Implementation(AActor* Target,
        const FGameplayCueParameters& Params) override;
    virtual void Tick(float DeltaSeconds) override;

protected:
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> BeamNC = nullptr;
    UPROPERTY(Transient) TScriptInterface<IAFLLaserVisualProvider> Visual;
    UPROPERTY(EditDefaultsOnly, Category="AFL|Laser")
    FName BeamEndParam = TEXT("Beam End");   // matches User.Beam End
    UPROPERTY(EditDefaultsOnly, Category="AFL|Laser")
    FName ColorParam   = TEXT("Color");      // matches User.Color
    UPROPERTY(EditDefaultsOnly, Category="AFL|Laser")
    TEnumAsByte<ECollisionChannel> CosmeticTraceChannel = ECC_Visibility;
};
```

```cpp
// AFLVFX/Private/AFLCueNotify_LaserBeam.cpp  (essentials)
bool AAFLCueNotify_LaserBeam::OnActive_Implementation(AActor* Target,
    const FGameplayCueParameters& Params)
{
    Visual = TScriptInterface<IAFLLaserVisualProvider>(Params.SourceObject.Get());
    if (!Visual) return false;

    UNiagaraSystem* Sys = Visual->GetBeamSystem();
    USceneComponent* Mesh = /* weapon mesh from Visual / Target */;
    if (!Sys || !Mesh) return false;

    BeamNC = UNiagaraFunctionLibrary::SpawnSystemAttached(
        Sys, Mesh, Visual->GetMuzzleSocketName(),
        FVector::ZeroVector, FRotator::ZeroRotator,
        EAttachLocation::SnapToTarget, /*bAutoDestroy*/ false);

    if (BeamNC)
        BeamNC->SetVariableLinearColor(ColorParam, Visual->GetBeamColor());

    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(true);
    return true;
}

void AAFLCueNotify_LaserBeam::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!BeamNC || !Visual) return;

    USceneComponent* Mesh = BeamNC->GetAttachParent();
    const FName Socket = Visual->GetMuzzleSocketName();
    const FVector Start = Mesh->GetSocketLocation(Socket);
    const FVector Dir   = Mesh->GetSocketRotation(Socket).Vector();
    const FVector End   = Start + Dir * Visual->GetCosmeticRange();

    FHitResult Hit;
    FCollisionQueryParams QP(SCENE_QUERY_STAT(AFLLaserCosmetic), /*bTraceComplex*/ true);
    QP.AddIgnoredActor(GetInstigator());
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, Start, End, CosmeticTraceChannel, QP);

    // Drive User.Beam End — this is the whole marketplace contract.
    BeamNC->SetVariableVec3(BeamEndParam, bHit ? Hit.ImpactPoint : End);
}

bool AAFLCueNotify_LaserBeam::OnRemove_Implementation(AActor* Target,
    const FGameplayCueParameters& Params)
{
    if (BeamNC)
    {
        BeamNC->Deactivate();             // let ribbons fade
        BeamNC->SetAutoDestroy(true);
        BeamNC = nullptr;
    }
    SetActorTickEnabled(false);
    return true;
}
```

The two driver calls — `SetVariableVec3("Beam End", point)` and
`SetVariableLinearColor("Color", tint)` — ARE the marketplace integration. Everything
else is plumbing.

## The hitscan flash + impact (Pulse)

Pulse is instant, so there is no looping beam — a one-shot flash plus an impact burst.
Both are bursts driven entirely by cue params (no tick):

- `GameplayCue.Weapon.Laser.BeamFlash`: spawn the beam system, set `User.Beam End` to
  `Params.Location` (the confirmed impact), set color, auto-destroy after ~0.06–0.10s.
- `GameplayCue.Weapon.Laser.Impact`: spawn the spark system at `Params.Location`
  oriented to `Params.Normal`; optional decal.

`AGameplayCueNotify_Burst` (or `_Actor` with `bAutoDestroyOnRemove`) is the right base;
read `Params.Location` / `Params.Normal` in `OnExecute`.

## How the ability triggers it (the only lines that change in combat code)

The ability already computes a predicted local impact and a server-validated impact.
Feed the **predicted** one to the cue for instant feel; damage keeps using the server
one. Channeled beam:

```cpp
// On beam start (after CommitAbility), client-predicted:
FGameplayCueParameters Cue;
Cue.SourceObject = GetAFLLaserWeaponInstance();        // implements IAFLLaserVisualProvider
GetAbilitySystemComponentFromActorInfo()->AddGameplayCue(
    AFLGameplayTags::GameplayCue_Weapon_Laser_Beam, Cue);

// On beam end / ability end:
GetAbilitySystemComponentFromActorInfo()->RemoveGameplayCue(
    AFLGameplayTags::GameplayCue_Weapon_Laser_Beam);
```

Hitscan Pulse, per shot, using the predicted target data you already build:

```cpp
FGameplayCueParameters Cue;
Cue.SourceObject = GetAFLLaserWeaponInstance();
Cue.Location     = PredictedImpactPoint;   // from your local trace
Cue.Normal       = PredictedImpactNormal;
ASC->ExecuteGameplayCue(AFLGameplayTags::GameplayCue_Weapon_Laser_BeamFlash, Cue);
ASC->ExecuteGameplayCue(AFLGameplayTags::GameplayCue_Weapon_Laser_Impact,    Cue);
```

That is the entire change to the authoritative side: a few cue calls. No trace logic
moves, no damage logic moves.

## Mobile material variants (required for any shipped weapon)

The marketplace masters are PC-grade (translucent, panning fractal noise, some use
scene color/distortion). For each beam material a shipped weapon uses:

1. Create a `_Mobile` material **instance** of the master.
2. Drop scene-color/refraction/distortion features; cap texture-sample count.
3. Prefer additive over translucent where the look survives it (cheaper overdraw).
4. Verify on a mobile preview (`r.Mobile.*`) before marking the weapon ✅.

Drive both variants from the same Niagara via a quality-switched MID or a Niagara
scalability setting — never fork the Niagara system per platform.

## PIE acceptance — what earns the ✅

Per STEP-0 doctrine, ✅ = watched in PIE on a controllable pawn, not "compiles". Listen
server + 2 clients:

| # | Check | Pass condition |
|---|---|---|
| 1 | Channeled beam endpoint | Beam visually terminates ON the surface the crosshair is over; sweep across geometry → endpoint tracks the wall, no overshoot/undershoot |
| 2 | Muzzle attach | Beam start stays glued to the muzzle socket as the pawn moves/turns |
| 3 | Color | `User.Color` matches the weapon's configured tint on all clients |
| 4 | Hitscan flash | Pulse shot flashes muzzle→impact for one frame-burst then clears (no lingering beam) |
| 5 | Impact | Sparks appear at the impact point oriented to the surface normal |
| 6 | Replication | Remote clients see the beam/flash/impact, not just the firer |
| 7 | Damage independence | Target takes damage via ExecCalc even if the cosmetic trace and damage trace disagree slightly (proves the layers are decoupled) |
| 8 | Base control intact | After firing, the pawn still walks/looks/jumps (STEP-0 re-verify rule) |
| 9 | Mobile | Beam renders correctly in the mobile preview with the `_Mobile` material |

Only when 1–9 are watched green does the laser/beam upgrade count as done.
