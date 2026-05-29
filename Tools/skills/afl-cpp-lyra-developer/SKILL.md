---
name: afl-cpp-lyra-developer
description: >
  Senior C++ engineer skill for the Lyra-derived UE5 project (filesystem
  Bag_Man, code prefix AFL, launch identity Ironics - Beta Lands V1.0).
  Specializes in extending Lyra rather than modifying it: GameFeature
  Plugins under Plugins/GameFeatures/AFL*/, GAS via Lyra's ASC on
  PlayerState, Lyra Experiences, AttributeSets, equipment/inventory,
  modular character framework. Use whenever the work involves extending
  Lyra base classes, authoring a GameplayAbility / GameplayEffect /
  AttributeSet / UActorComponent for AFL, creating or extending a
  GameFeature plugin, deciding content shape (/Game/ vs plugin) for
  asset references, or writing C++ that must remain merge-compatible
  with upstream Lyra. Paired with lyra-ue5-build-discipline (the
  rebuild's methodology and 22-trap catalog) — always loaded together.
---

# AFL C++ / Lyra Developer Skill

Senior C++ engineer for the AFL project. Job: **extend Lyra, don't
rewrite it** — subclass or add GameFeature plugins, never modify Lyra
base code, so upstream Lyra updates stay mergeable.

This skill governs C++ architecture decisions. Pair with
`lyra-ue5-build-discipline` (the methodology + 22-trap catalog) on every
sprint that touches code — they're designed to be loaded together.

**Identity Map**: see `lyra-ue5-build-discipline/SKILL.md` for the
canonical Bag_Man / AFL / Ironics - Beta Lands V1.0 name disambiguation.
Code identifiers in this skill use `Bag_Man` (filesystem), `AFL` (code
prefix). The launch identity `Ironics - Beta Lands V1.0` lives in
display fields only.

---

## AFL Lyra Architecture Rules (non-negotiable)

1. **Never modify Lyra base classes directly** — subclass instead. This
   preserves upstream Lyra mergeability.

2. **New features go in GameFeature Plugins** under
   `Plugins/GameFeatures/AFL<Feature>/`. Three exist on the project today:
   `AFLCombat`, `AFLCore`, `AFLMovement`. New features extend the pattern.

3. **Use LyraExperiences** to compose gameplay — never hardcoded
   GameModes. `B_Experience_BagMan` is the project's primary experience.

4. **GAS via Lyra's ASC** — `ULyraAbilitySystemComponent` lives on
   `ALyraPlayerState`, **not the pawn**. Never write a custom ASC, and
   never `FindComponentByClass<UAbilitySystemComponent>` on the player's
   own pawn (see `lyra-ue5-build-discipline` trap #3 — three confirmed
   instances led to BM-DEBT-AUDIT-001's unified fix at commit
   `d6942982`).

5. **Input via LyraInputConfig** — never legacy input bindings.

6. **UI via LyraHUDLayout** — CommonUI stack, not standalone HUDs.

7. **Content shape**: `/Game/` content cannot reference plugin content
   (per `AssetReferenceRestrictions` validator). Solution: put
   AFL-specific content in a GameFeature plugin where the references are
   legal. **Relocate the content, don't sever the references.** This is
   BM-DEBT-001's resolution pattern — moving content into a plugin makes
   the existing `/ShooterCore/` references legal rather than requiring
   the references to be eliminated. Plugin → plugin references are
   allowed; the validator only flags `/Game/` → plugin.

---

## ⚠️ Standing Hazard — UAFLHeroComponent

**DO NOT add `UAFLHeroComponent` (or `LAS_AFL_HeroComponents`) to this
project. This component is a bug-carrier from the prior build.**

The prior build authored a custom hero component that left Lyra's
`DefaultInputMappings` empty at runtime — Lyra's `InitializePlayerInput`
clears all mappings then repopulates from that array, which was empty
on a raw-C++ class. Every input binding silently died. The pawn looked
controllable in the editor and accepted no input in PIE for weeks of
work that was banked as ✅ on the basis of code-authored-not-feature-works.

This was the **founding failure mode** the Step-0 rebuild corrected. The
rebuild's discipline (✅ = operator-watched-runtime, canonical BP-child
character adoption) exists precisely because `UAFLHeroComponent` was the
trap that destroyed the whole prior build.

**If a future sprint surfaces a need for hero-component-shaped
functionality**, that's its own scoped decision with explicit risk
acknowledgment — not a casual "add a hero component" task. The
appropriate paths are almost always:
- Extend `ULyraHeroComponent` via a Blueprint child (canonical Lyra path)
- Compose the needed behavior through experience-side `AddComponents`
  targeting `LyraPlayerController` (per BM-0106's `UAFLHitConfirmComponent`
  pattern, commit `ab683344`)

See the standing hazard memory (`project_uaflherocomponent_standing_hazard`)
for full history. The Step-0 tracker records this lesson at multiple
checkpoints. Don't reintroduce the bug-carrier.

---

## GameFeature Plugin Structure (Canonical)

```
Plugins/GameFeatures/AFL<Feature>/
├── AFL<Feature>.uplugin
├── Source/AFL<Feature>/
│   ├── AFL<Feature>.Build.cs
│   ├── Public/
│   │   ├── Abilities/
│   │   ├── Components/
│   │   └── AttributeSets/
│   └── Private/
│       ├── Abilities/
│       └── AFL<Feature>Module.cpp
└── Content/
    ├── Abilities/
    ├── Data/
    └── Experiences/  (if the plugin owns experience definitions)
```

### Canonical `.uplugin` (anchored to `AFLCombat.uplugin`)

```json
{
  "FileVersion": 3,
  "Version": 1,
  "VersionName": "1.0",
  "FriendlyName": "AFL Combat",
  "Description": "AFL combat systems -- abilities, attributes, damage",
  "Category": "Game Features",
  "CreatedBy": "Ironics - Beta Lands V1.0",
  "CanContainContent": true,
  "ExplicitlyLoaded": true,
  "BuiltInInitialFeatureState": "Registered",
  "Plugins": [
    { "Name": "ModularGameplay", "Enabled": true },
    { "Name": "GameFeatures",    "Enabled": true }
  ]
}
```

Key fields:
- `CanContainContent: true` — required for .uassets, not just code.
- `ExplicitlyLoaded: true` — no auto-load; Experience composition controls activation.
- `BuiltInInitialFeatureState: "Registered"` — registered but not Active
  until Experience activates. *Active vs Registered is real:*
  `+GameFeaturesToEnable=` in `DefaultGame.ini` only registers — only
  Experience-side AddActions make things Active. See
  `lyra-ue5-build-discipline` trap #15.

### Canonical `.Build.cs`

```csharp
public class AFLCombat : ModuleRules
{
    public AFLCombat(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnforceIWYU = true;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine",
            "GameplayAbilities", "GameplayTags", "GameplayTasks",
            "LyraGame",          // Lyra base classes
            "ModularGameplay",
        });
    }
}
```

### Content-only GameFeature Plugins

A plugin can be **content-only** (no Source module) if it holds .uassets
but no new C++. The `.uplugin` omits the `Modules` array. This is the
right shape for plugins that exist purely to host content that needs to
be in a plugin per rule 7, separate from systems that own the C++. D's
Layer 1 (BM-DEBT-001) likely produces a content-only plugin.

---

## Extending Lyra Classes

### Custom LyraGameplayAbility

```cpp
// AFLAG_PulseFire.h
#pragma once
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AFLAG_PulseFire.generated.h"

UCLASS()
class AFLCOMBAT_API UAFLAG_PulseFire : public ULyraGameplayAbility
{
    GENERATED_BODY()
public:
    UAFLAG_PulseFire();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category="AFL|Pulse")
    float MaxRange = 5000.f;

    UPROPERTY(EditDefaultsOnly, Category="AFL|Pulse")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    UPROPERTY(EditDefaultsOnly, Category="AFL|Pulse|Tuning")
    TObjectPtr<UAFLPulseTuningDA> TuningData;
};
```

```cpp
// AFLAG_PulseFire.cpp
UAFLAG_PulseFire::UAFLAG_PulseFire()
{
    // Lyra standard: abilities are instanced per actor
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // FObjectFinder must be Succeeded()-guarded;
    // see lyra-ue5-build-discipline trap #2.
    static ConstructorHelpers::FObjectFinder<UAFLPulseTuningDA>
        TuningFinder(TEXT("/AFLCombat/Data/DA_AFL_PulseTuning"));
    if (TuningFinder.Succeeded())
    {
        TuningData = TuningFinder.Object;
    }
}
```

### ASC access pattern — the trap #3 lesson

**The pawn does not own the ASC.** `ULyraAbilitySystemComponent` lives
on `ALyraPlayerState`. Three project instances of looking it up on the
pawn led to BM-DEBT-AUDIT-001's unified fix.

**Own-pawn ASC (the player's own ability system):**

```cpp
// CORRECT -- own ASC lives on PlayerState
if (APlayerController* PC = Cast<APlayerController>(GetController()))
{
    if (APlayerState* PS = PC->PlayerState)
    {
        if (UAbilitySystemComponent* ASC =
                UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PS))
        {
            // use ASC
        }
    }
}

// WRONG -- has bitten this project 3+ times. Never write this for the
// player's own ASC:
// UAbilitySystemComponent* ASC =
//     Pawn->FindComponentByClass<UAbilitySystemComponent>();
```

### Hit-target ASC — a different lookup target (the caveat)

When the hit *target* may own its own ASC (e.g. melee hit on an AI actor
that follows a different ASC ownership pattern), the lookup is against
the target, not the player's PlayerState:

```cpp
// In a melee hit handler:
if (AActor* HitActor = Hit.GetActor())
{
    // This works because the hit actor's ASC is a different lookup target
    // than the *firing* pawn's ASC, which lives on PlayerState per rule 4
    // / trap #3. For the firing player's own ASC, use the
    // PC -> PlayerState -> GetAbilitySystemComponentFromActor pattern above.
    if (UAbilitySystemComponent* TargetASC =
            UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
    {
        // apply effect to target
    }
}
```

The distinction: **whose ASC are you looking up?**
- Player's own = `PC -> PlayerState -> GetAbilitySystemComponentFromActor`
- Hit target = lookup against the target actor directly

Don't generalize one to the other. Trap #3's prohibition is specifically
about the player-pawn lookup; target lookups are a different shape.

---

## Lyra Experience Setup

The Experience is the canonical composition layer. Actions on the
Experience asset determine what's added at runtime:

```
B_Experience_BagMan (ULyraExperienceDefinition):
  Default Pawn Class:        BP_HeroPawn_BagMan
  Actions:
    + AddAbilities:
        - AbilitySet:        /AFLCombat/Sets/DA_AFL_Combat_AbilitySet
        - Target:            ALyraPlayerState
    + AddComponents:
        - Component:         UAFLHitConfirmComponent
        - Target:            ALyraPlayerController
        - bClientComponent:  true
        - bServerComponent:  false
    + AddInputConfig:
        - InputConfig:       DA_AFL_BagMan_InputConfig
```

### The BM-PLUGIN-GRANT-LIFECYCLE workaround (load-bearing)

Plugin-side `GameFeatureAction_AddAbilities` does NOT reliably deliver
grants to `LyraPlayerState` in Bag_Man's experience lifecycle (confirmed
by the BM-DEBT-005-followup-prune distinguishing experiment, commit
`d6942982`).

**Canonical workaround:** mirror the `AddAbilities` action on the
**experience side** rather than relying on plugin-side action. This is
why `B_Experience_BagMan` carries the `AddAbilities` action directly
(commit `7101997f`) rather than depending on `AFLCombat`'s plugin-side
grants alone.

This is an **architectural open question** — not solved, worked around.
Affects every plugin-heavy phase (AFLEnergy, Extraction, Chaos, Online,
Backend). Investigation deferred until a phase where it becomes blocking.

See `lyra-ue5-build-discipline` trap #8 for the full diagnostic signature.

---

## AFL Custom AttributeSet

Lyra ships `ULyraHealthSet` and `ULyraCombatSet`. Extend with
AFL-specific attributes — don't replace.

```cpp
UCLASS(BlueprintType)
class AFLCOMBAT_API UAFLAttributeSet_Combat : public ULyraAttributeSet
{
    GENERATED_BODY()
public:
    ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, Health)
    ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, MaxHealth)
    ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, Heat)
    ATTRIBUTE_ACCESSORS(UAFLAttributeSet_Combat, MaxHeat)

    virtual void PreAttributeChange(
        const FGameplayAttribute& Attribute, float& NewValue) override;

    virtual void PostGameplayEffectExecute(
        const FGameplayEffectModCallbackData& Data) override;

private:
    UPROPERTY(BlueprintReadOnly, Category="AFL|Combat",
              ReplicatedUsing=OnRep_Health,
              meta=(AllowPrivateAccess="true"))
    FGameplayAttributeData Health;

    UPROPERTY(BlueprintReadOnly, Category="AFL|Combat",
              ReplicatedUsing=OnRep_MaxHealth,
              meta=(AllowPrivateAccess="true"))
    FGameplayAttributeData MaxHealth;

    UPROPERTY(BlueprintReadOnly, Category="AFL|Combat",
              ReplicatedUsing=OnRep_Heat,
              meta=(AllowPrivateAccess="true"))
    FGameplayAttributeData Heat;

    UPROPERTY(BlueprintReadOnly, Category="AFL|Combat",
              ReplicatedUsing=OnRep_MaxHeat,
              meta=(AllowPrivateAccess="true"))
    FGameplayAttributeData MaxHeat;

    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& Old);
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& Old);
    UFUNCTION() void OnRep_Heat(const FGameplayAttributeData& Old);
    UFUNCTION() void OnRep_MaxHeat(const FGameplayAttributeData& Old);
};
```

Rules:
- `ATTRIBUTE_ACCESSORS` for every attribute.
- `OnRep_*` for every replicated attribute, paired with
  `GAMEPLAYATTRIBUTE_REPNOTIFY` in the implementation.
- `PreAttributeChange` clamps values; `PostGameplayEffectExecute` handles
  side effects (zeroing meta attributes, broadcasting events).
- Damage as a **meta attribute** (not persistent) consumed in PostGEE —
  see `UAFLDamageExecCalc` pattern (BM-0102, commit `edfb6953`).

---

## Damage Pipeline (BM-0102 reference)

Runtime-verified pattern: Pulse → Health 100→82→64→46→28, exactly
−18/shot, no accumulation.

```
Pulse ability fires -> seeds GE Source.Damage + SetByCallers
  -> GE_AFL_Damage_Pulse (Executions-only, no direct Override)
  -> UAFLDamageExecCalc runs (9-step algorithm in lyra-ue5-build-discipline 8.3)
  -> Modifies Health meta
  -> PostGameplayEffectExecute zeros Damage meta + checks OverkillThreshold
  -> Broadcasts Event.Damage.Confirmed + Event.Damage.Overkill (if threshold)
  -> UAFLHitConfirmComponent (granted experience-side, target LyraPlayerController)
    subscribes via UGameplayMessageSubsystem and reacts (camera shake, hitmarker)
```

**Key constraint:** GameplayEffects that apply damage must route through
the ExecCalc (`Executions` array), **not** direct attribute modifiers.
Direct modifiers bypass armor mitigation, shield absorption, and overkill
detection. See `lyra-ue5-build-discipline` trap #9.

---

## Component Grant Patterns (the orphan trap)

A `UActorComponent` subclass with a self-subscribe lifecycle (registers
a `UGameplayMessageSubsystem` listener on `BeginPlay`) requires a grant.
Without one, the component compiles, ships, and does nothing at runtime.

The project has hit this pattern **twice confirmed** — see
`lyra-ue5-build-discipline` trap #19 and methodology.md's "Pillar 4 in
formation" worked example. A third instance triggers a full audit of
AFLCombat `UActorComponent` subclasses.

**Canonical grant paths:**

| Component role | Grant via | Target |
|---|---|---|
| Per-player feedback (camera shake, hitmarker) | Experience `AddComponents` | `LyraPlayerController` |
| Server-only authoritative state | Experience `AddComponents` | `LyraPlayerState` |
| Per-pawn behavior (lag-comp snapshots) | PawnData component list | The pawn class |
| Self-contained test actor | Ctor `CreateDefaultSubobject` | The actor itself |

**Rule:** author the grant in the **same sprint** as the component. The
grant is part of "the component works," not a follow-up.

---

## Platform Compile Guards

```cpp
#if PLATFORM_PS5
    // PS5-specific: DualSense haptics, Activities API
#elif PLATFORM_XSX
    // Xbox-specific: GameCore, Xbox Live
#elif PLATFORM_ANDROID || PLATFORM_IOS
    // Mobile: touch input, reduced quality settings
#endif

// Shipping guard -- never log sensitive data in Shipping
#if !UE_BUILD_SHIPPING
    UE_LOG(LogAFL, Verbose, TEXT("Debug: %s"), *DebugString);
#endif
```

---

## AFL Naming Conventions

| Type | Prefix | Example |
|---|---|---|
| GameFeature Plugin | `AFL` | `AFLCombat`, `AFLMovement` |
| Module | `AFL<Feature>` | `AFLCombat`, `AFLCore` |
| Ability class | `UAFLAG_<Name>` | `UAFLAG_PulseFire`, `UAFLAG_Dash` |
| Component | `UAFL<Name>Component` | `UAFLHitConfirmComponent` |
| AttributeSet | `UAFLAttributeSet_<Name>` | `UAFLAttributeSet_Combat` |
| GameplayEffect (BP) | `GE_AFL_<Effect>` | `GE_AFL_Damage_Pulse` |
| Data Asset | `DA_AFL_<Name>` | `DA_AFL_Combat_AbilitySet` |
| Blueprint | `B_<Name>` or `BP_<Name>_BagMan` | `B_Hero_BagMan`, `B_Experience_BagMan` |
| Pawn Data | `HeroData_BagMan`, `PawnData_<Name>` | |
| Input config | `DA_AFL_<Name>_InputConfig` | |

Content prefixes:
- `BagMan` suffix on Blueprints/DataAssets specific to the player
  character (e.g. `B_Hero_BagMan`, `HeroData_BagMan`,
  `B_Experience_BagMan`).
- Generic AFL systems use `AFL` only (e.g. `DA_AFL_Combat_AbilitySet` —
  not tied to BagMan specifically).

---

## When to Reach for the Discipline Catalog

`lyra-ue5-build-discipline` carries 22 catalogued anti-patterns from
this project's lived evidence. Consult its `references/traps-catalog.md`
when:

- Authoring a new GameplayAbility (traps #2, #3, #11)
- Authoring a new GameplayEffect (traps #5, #9)
- Authoring a new `UActorComponent` with self-subscribe (trap #19)
- Editing .uassets via MCP / Python (traps #1, #18)
- Building / running cook (traps #4, #12, #13)
- Adding GameFeature actions (traps #8, #15)
- Verifying anything in PIE (traps #7, #16, #17)
- Doing git operations on AFL work (traps #4, #21)
- Reading or banking tracker entries (trap #22)

The catalog is by-symptom and grep-tractable. Each entry cites the
BM-xxxx sprint and commit that earned it.

---

## Code Review Checklist (AFL Lyra-canonical)

Before any C++ PR merges:

- [ ] No direct modification of Lyra base classes (rule 1)
- [ ] New features in a GameFeature plugin (rule 2)
- [ ] No custom ASC (rule 4); player's own ASC accessed via PlayerState (trap #3)
- [ ] Hit-target ASC lookups appropriately use the target actor (not generalized from trap #3)
- [ ] All abilities `InstancedPerActor` + `LocalPredicted` (Lyra standard)
- [ ] ATTRIBUTE_ACCESSORS for new attributes; OnRep_* paired with REPNOTIFY
- [ ] No direct Health modification — route through `UAFLDamageExecCalc` (trap #9)
- [ ] Components self-subscribing on BeginPlay have a grant authored in
      the same sprint (trap #19)
- [ ] Platform guards for any platform-specific code
- [ ] AFL naming conventions followed
- [ ] FObjectFinder constructor code is `Succeeded()`-guarded (trap #2)
- [ ] No `UAFLHeroComponent` introduced (standing hazard — see warning section)
- [ ] If touching `/Game/` content that references plugin assets, content shape
      verified per rule 7 (relocate, don't sever)

---

## Cross-references

- **`lyra-ue5-build-discipline`** (Tools/skills/) — paired methodology
  and 22-trap catalog. Always loaded together. Carries the canonical
  Identity Map referenced from this skill's opening.
- **`afl-sprint-planner`** (Tools/skills/, *authored at D.0d — see git log*) —
  task format and estimation guide.
- **`afl-asset-pipeline`** (Tools/skills/, *authored at D.0b — see git log*) —
  DCC→UE5 workflow, naming, Git LFS, cook audit, redirector fixup.
- **`unreal-engine-expert`** (Tools/skills/, *authored at D.0c — see git log*) —
  broader AAA UE5 patterns; routes to project-specific AFL skills.
- **Master Build Document** (Docs/) — the project's SSOT and forward
  roadmap.
- **`BAG_MAN_LIVE_TRACKER.html`** (project root) — the live tracker;
  reconciled to git reality at HEAD `0b62bf01`.
- **`project_uaflherocomponent_standing_hazard`** (memory) — full
  history of the bug-carrier referenced in the Standing Hazard section.
