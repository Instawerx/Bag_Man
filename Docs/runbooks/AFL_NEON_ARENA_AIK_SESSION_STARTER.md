# AFL :: NEON ARENA — AIK SESSION STARTER
**Phase 0 / Sprint 1 Bootstrap · v1.0 · for Claude Code agent in NeoStack AIK**

> This document is the operator runbook for the **first** AIK session against the AFL repo.
> It is the literal sequence of prompts that bootstraps the codebase from "freshly cloned Lyra" to "Pulse Carbine fires end-to-end with v1.1 server-authoritative architecture in place."
>
> Owner: Engineering Lead. Sign-off required: Studio Lead (after Stage 7 PIE smoke test).

---

## 0. BEFORE YOU OPEN AIK

Complete this checklist before pasting a single prompt. Skipping any item means the agent gets the wrong context and produces the wrong code.

### 0.1 Repo State
- [ ] AFL repo cloned, on branch `feature/phase-0-bootstrap`
- [ ] Lyra Starter Game already merged into `Source/LyraGame/` (this is the upstream we extend, never modify)
- [ ] `AFL_NEON_ARENA_MASTER_BUILD_v1.1.md` is present at the repo root (the agent will read it)
- [ ] `AFL_NEON_ARENA_LIVE_TRACKER_v1.1.html` open in your browser, on Phase 1 → Sprint 1

### 0.2 AIK Settings
- [ ] `Tools → Agent Chat` opens a chat panel
- [ ] **Agent: Claude Code** (NOT Gemini, NOT OpenRouter — Claude Code has the best Lyra awareness per AFL profile guidance)
- [ ] **Profile: AFL Blueprint & Gameplay** active
- [ ] Tools enabled in profile: Edit Blueprint, Behavior Tree, State Tree, Enhanced Input, Data Structures, Data Table
- [ ] Project Settings → AIK → no globally disabled tools

### 0.3 What to Attach (paperclip icon in AIK chat)
Attach these once at session start; they stay in context for the whole session:

| Attachment | Purpose |
|---|---|
| `AFL_NEON_ARENA_MASTER_BUILD_v1.1.md` | The contract. Sections §7, §8, §9, §13.1, §13.4, §13.5, §13.6 are the agent's reference. |
| `Source/LyraGame/AbilitySystem/Abilities/LyraGameplayAbility.h` | Base class for our abilities |
| `Source/LyraGame/Character/LyraHeroComponent.h` | Base class for `UAFLHeroComponent` (§9) |
| `Source/LyraGame/AbilitySystem/Attributes/LyraAttributeSet.h` | Pattern for `UAFLAttributeSet_Combat` (§8) |
| `Source/LyraGame/Player/LyraPlayerState.h` | Where the ASC lives (canonical) |
| `Source/LyraGame/GameModes/LyraExperienceDefinition.h` | Pattern for `B_LyraExperience_AFL_Arena` |

### 0.4 Operator Discipline
- **One stage at a time.** Do not paste Stage 2 until Stage 1's checkpoint passes.
- **Read every file the agent creates** before moving on. Sign off on the diff in your head, not just the AIK output panel.
- **Compile after every stage.** A green PIE check between stages is non-negotiable.
- **If the agent violates a §7/§8/§9 rule, STOP.** Do not "fix it later." Reset the stage with the recovery prompt in §4.

---

## 1. THE SESSION CONTRACT — paste first, only once

This single prompt sets the rules of engagement for the entire AIK session. Every subsequent prompt builds on the contract this establishes. Claude Code will internalize these constraints and apply them to every turn until you close the chat.

**Paste this prompt as the very first message. Do not modify.**

```text
═══════════════════════════════════════════════════════════════════════════
  AFL :: NEON ARENA — SESSION CONTRACT
═══════════════════════════════════════════════════════════════════════════

You are driving the Phase 0 bootstrap of the AFL NEON ARENA codebase — a
Lyra-based UE5 multiplayer extraction shooter targeting Win64, PS5, XSX,
iOS, and Android.

Read AFL_NEON_ARENA_MASTER_BUILD_v1.1.md (attached) END TO END before you
write a single line of code. The document is authoritative. When this
prompt and the master document conflict, the master document wins.

═══ NON-NEGOTIABLE RULES (enforced by code review and CI lint AFL-0215) ═══

RULE 1 — SERVER-AUTHORITATIVE HITSCAN (§7)
  ▸ NEVER call GetPlayerViewPoint on a dedicated server. It returns control
    rotation, not the client's camera, and silently desyncs.
  ▸ Hitscan abilities use the GAS TargetData pattern: client local trace
    → FGameplayAbilityTargetDataHandle → ServerSetReplicatedTargetData
    → server validation → server-only damage application.
  ▸ Server validation runs all 5 layers (§7.2): schema, geometry, lag-comp
    LOS re-trace, RoF/resource, telemetry emission on reject.
  ▸ Lag rewind uses SERVER-MEASURED RTT (PlayerState->ExactPing), never
    client-supplied. Hard cap MaxRewindSeconds = 0.2.

RULE 2 — DAMAGE PIPELINE (§8)
  ▸ NEVER modify Health (or any persistent attribute) directly from a
    UGameplayAbility.
  ▸ Damage flows through a Damage META-attribute on UAFLAttributeSet_Combat.
  ▸ Damage GEs run a UGameplayEffectExecutionCalculation (UAFLDamageExecCalc)
    that handles armor mitigation, shield stripping, and overkill tagging.
  ▸ SetByCaller magnitudes (Data.Damage.Headshot/Distance/Weakpoint) are
    set by the ability post-validation; ExecCalc reads them.

RULE 3 — LYRA INITIALIZATION CONTRACT (§9)
  ▸ NEVER grant abilities in BeginPlay, constructors, or PostInitializeComponents.
  ▸ Abilities live in ULyraAbilitySet data assets (DA_AFL_AbilitySet_*).
  ▸ Granting happens in UAFLHeroComponent::HandleChangeInitState at the
    InitState_DataInitialized phase — let Super:: do the work, do not
    duplicate it.
  ▸ PawnData (DA_AFL_PawnData_Hero_Default) flows through the
    LyraExperience definition. Not via GameMode constructors. Not via
    PIE override hacks.
  ▸ Input flows: Enhanced Input → ULyraInputConfig (IC_AFL_Default) →
    InputTags → ability activation. No direct InputAction → method binds.

RULE 4 — LYRA EXTENSION DISCIPLINE
  ▸ Never modify any file under Source/LyraGame/. Subclass or extend via
    GameFeature plugin under Plugins/GameFeatures/AFL<Feature>/.
  ▸ Every plugin has its own .uplugin, Build.cs, Source/, and Content/.
  ▸ Plugin Build.cs depends on "LyraGame" only when the plugin extends a
    Lyra class that lives in that module.

RULE 5 — NAMING (case-sensitive, validated by AFL-0109 CI lint)
  ▸ Plugins:        AFL<Feature>  e.g. AFLCore, AFLCombat, AFLMovement
  ▸ Abilities:      UAFLAG_*      e.g. UAFLAG_Laser_Pulse
  ▸ Components:     UAFLC_* / UAFL*Component
  ▸ AttributeSets:  UAFLAttributeSet_*  e.g. UAFLAttributeSet_Combat
  ▸ GameplayEffects:GE_AFL_*      e.g. GE_AFL_Damage_Pulse
  ▸ Data Assets:    DA_AFL_*      e.g. DA_AFL_PawnData_Hero_Default
  ▸ Blueprints:     BP_AFL*       e.g. BP_AFLCharacter_Base
  ▸ Materials:      M_AFL_*       (and matching M_AFL_*_Mobile variant)
  ▸ Niagara:        NS_AFL_*

RULE 6 — MULTI-PLATFORM AWARENESS
  ▸ Target: Win64, PS5, XSX, iOS, Android.
  ▸ No platform-specific code outside #if PLATFORM_* guards.
  ▸ Every visual material has a Mobile variant from day one (§4.3 of master).
  ▸ Niagara: GPU sim variants for any system >500 particles.

═══ HOW WE WORK ═══════════════════════════════════════════════════════════

▸ I will paste prompts in stages. Each stage has an explicit goal and an
  explicit acceptance test. Do not advance to the next stage on your own.
▸ At the end of each stage, output a "STAGE N COMPLETE" block listing:
    - Files created/modified
    - Compile status (you may run dotnet/UBT to check)
    - Any deviations from the master document, with reasoning
    - Next stage prerequisites
▸ If you encounter ambiguity, STOP and ask. Do not invent.
▸ If a request would violate any of Rules 1–6, REFUSE and explain which
  rule applies. I will rephrase the request.

═══ CONFIRM UNDERSTANDING ═════════════════════════════════════════════════

Before I send Stage 1, reply with a numbered checklist confirming you
have ingested:
  1. The five non-negotiable rules above
  2. The plugin map in master §3.2
  3. The canonical Pulse Carbine in master §13.1
  4. The validation doctrine in master §7
  5. The damage pipeline in master §8
  6. The Lyra initialization contract in master §9

Do NOT write any code yet. Just the confirmation checklist.
═══════════════════════════════════════════════════════════════════════════
```

### Operator action after the contract

Wait for the agent's confirmation checklist. Read it. If any of the six items is paraphrased loosely or contradicts the master document, **reset the chat and re-paste**. Do not proceed with a confused agent.

---

## 2. STAGE PROMPTS — paste in order

Each stage has: **Goal** · **Tasks covered** · **Prompt (copy-paste)** · **Acceptance test** · **Checkpoint** before next stage.

---

### STAGE 1 — `AFLCore` plugin scaffold + GameplayTags

**Goal**: A compiling, empty `AFLCore` plugin that other plugins can depend on. Project boots with the plugin enabled; no behavior change yet.

**Sprint tasks covered**: AFL-0101, AFL-0102

**Prompt:**

```text
═══ STAGE 1 — AFLCore plugin scaffold + GameplayTags ═══

GOAL: Create the AFLCore GameFeature plugin. Add the project's master
GameplayTag list. Project must compile and the plugin must show as
enabled in Edit → Plugins.

DELIVERABLES:

1. Plugins/GameFeatures/AFLCore/AFLCore.uplugin
   - GameFeatureType = "GameFeature"
   - Dependencies: ModularGameplay, GameFeatures, GameplayAbilities,
     GameplayTags, GameplayTasks
   - InitialState = "Active" (we want it loaded at boot for Phase 0)

2. Plugins/GameFeatures/AFLCore/Source/AFLCore/AFLCore.Build.cs
   - PCHUsage = UseExplicitOrSharedPCHs
   - bEnforceIWYU = true
   - PublicDependencyModuleNames: Core, CoreUObject, Engine,
     GameplayAbilities, GameplayTags, GameplayTasks, LyraGame,
     ModularGameplay, ModularGameplayActors

3. Plugins/GameFeatures/AFLCore/Source/AFLCore/Public/AFLCore.h
   Plugins/GameFeatures/AFLCore/Source/AFLCore/Private/AFLCore.cpp
   - Standard FDefaultGameModuleImpl pattern
   - DECLARE_LOG_CATEGORY_EXTERN(LogAFLCore, Log, All) in header
   - DEFINE_LOG_CATEGORY(LogAFLCore) in cpp

4. Config/Tags/AFLTags.ini  (in repo root Config/, NOT inside the plugin)
   - Paste the EXACT tag list from master §13.2, including:
     · State.* tags
     · Ability.Laser.* tags
     · Cooldown.* tags
     · Event.* including Event.Damage.Overkill
     · Data.Damage.Headshot / Distance / Weakpoint
     · Telemetry.Reject.* (Schema/Distance/Origin/Angle/LOS)
     · InputTag.* tags

5. Update Config/DefaultGame.ini:
   - Add AFLCore to [/Script/GameFeatures.GameFeaturesSubsystemSettings]
     +GameFeaturesToEnable=AFLCore
   - Register the AFLTags.ini in [/Script/GameplayTags.GameplayTagsList]
     +GameplayTagTableList=("/Game/AFL/Data/DT_AFLTags.uasset") IF you also
     create a data table; otherwise the +GameplayTagList= entries in
     AFLTags.ini are picked up automatically.

CONSTRAINTS:
  ▸ Do NOT add any C++ classes beyond the module class. AttributeSets,
    HeroComponent, abilities all come in later stages.
  ▸ Do NOT modify Source/LyraGame/.
  ▸ Use forward declarations in headers. Include implementation .h files
    only in .cpp.

ACCEPTANCE TEST:
  ▸ UnrealEditor compiles clean (Win64 Development).
  ▸ Editor opens; Plugins window shows "AFL Core" enabled.
  ▸ Project Settings → GameplayTags shows all tags from §13.2.
  ▸ Output log shows "LogAFLCore: Display: AFLCore module loaded" on boot
    (add this log line to StartupModule).

When complete, output "STAGE 1 COMPLETE" with the file list and any
notes.
```

**Checkpoint after Stage 1:**

1. From terminal: `./GenerateProjectFiles.sh` (or `.bat` on Windows) — should regenerate project with new plugin.
2. Build `AFLEditor` Development Win64 in Rider/VS — must succeed with zero warnings.
3. Open editor → check Plugins window for AFL Core (enabled, not crashed).
4. Output log: `grep "LogAFLCore" Saved/Logs/AFL.log` — must show the startup message.
5. Project Settings → GameplayTags → search "Event.Damage.Overkill" — must resolve.

If any check fails, jump to §4 Recovery before Stage 2.

---

### STAGE 2 — `AFLCombat` plugin + `UAFLAttributeSet_Combat` + `UAFLDamageExecCalc`

**Goal**: The damage pipeline foundation. No abilities yet — just the attribute set and the ExecCalc so when abilities arrive in Stage 5, they have the right plumbing to write into.

**Sprint tasks covered**: AFL-0103, AFL-0212

**Prompt:**

```text
═══ STAGE 2 — AFLCombat plugin + AttributeSet + ExecCalc ═══

GOAL: Stand up the AFLCombat plugin with the canonical attribute set
and damage execution calculation. NO abilities yet.

DELIVERABLES:

1. Plugins/GameFeatures/AFLCombat/AFLCombat.uplugin
   - Same metadata pattern as AFLCore
   - Plugin dependencies include AFLCore

2. Plugins/GameFeatures/AFLCombat/Source/AFLCombat/AFLCombat.Build.cs
   - Same dependencies as AFLCore plus AIModule (used by lag comp later)

3. Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Public/AFLCombatLog.h
   - DECLARE_LOG_CATEGORY_EXTERN(LogAFLCombat, Log, All)
   - DECLARE_LOG_CATEGORY_EXTERN(LogAFLTelemetry, Log, All)

4. Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Public/Attributes/
   AFLAttributeSet_Combat.h   and matching .cpp

   Subclass ULyraAttributeSet (NOT UAttributeSet — we extend Lyra's base).
   Attributes (each with ATTRIBUTE_ACCESSORS macro, OnRep functions for
   non-meta, REPLICATED via GetLifetimeReplicatedProps with COND_OwnerOnly):
     · Health, MaxHealth     (persistent)
     · Shield, MaxShield     (persistent)
     · Armor                 (persistent)
     · OverkillThreshold     (persistent)
     · Heat, MaxHeat         (persistent — used by Beam in S2)
     · Damage                (META — reset to 0 in PostGameplayEffectExecute)

   Override:
     · PreAttributeChange — clamp Health to [0, MaxHealth], Shield to
       [0, MaxShield], Heat to [0, MaxHeat]
     · PostGameplayEffectExecute — when Damage was the modified attribute,
       process and zero it (Damage routing handled by ExecCalc; this is
       the safety net for direct sets in tests/dev)

5. Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Public/Calc/
   AFLDamageExecCalc.h   and matching .cpp

   Subclass UGameplayEffectExecutionCalculation. Implement
   Execute_Implementation EXACTLY as specified in master §13.5:
     · Capture: Damage(source), Armor, Shield, Health, OverkillThreshold (target)
     · Read SetByCaller magnitudes: Data.Damage.Headshot, .Distance, .Weakpoint
     · Modified = RawDamage * Headshot * Distance * Weakpoint
     · Mitigation = Armor / (Armor + 100)
     · Effective = Modified * (1 - Mitigation)
     · ShieldDelta = -min(Shield, Effective)
     · HealthDelta = -(Effective + ShieldDelta)
     · Output AddOutputModifier for ShieldAttribute and HealthAttribute (Additive)
     · If HealthDelta < -OverkillThreshold (and OverkillThreshold > 0):
       Spec.AddDynamicAssetTag(Event.Damage.Overkill)

6. Plugins/GameFeatures/AFLCombat/Content/GE/GE_AFL_Damage_Pulse.uasset
   - GameplayEffect Blueprint
   - Duration Policy: Instant
   - Modifier: Damage attribute, Override, magnitude SetByCaller(none — actually
     uses CalculationOperation = Override with magnitude 18.0 from a
     ScalableFloat curve table, so designers can re-tune via CT_AFLWeapons)
   - Executions: UAFLDamageExecCalc

CONSTRAINTS:
  ▸ Health, Shield, Armor are Target-captured, NOT-snapshotted (so buffs
    applied between activation and execution are honored).
  ▸ Damage is Source-captured AND snapshotted (the moment of fire is what
    matters, not the moment of execution).
  ▸ All attribute properties use UPROPERTY(BlueprintReadOnly,
    Category="AFL|Attributes", ReplicatedUsing=OnRep_*, meta=(AllowPrivateAccess="true")).
  ▸ DO NOT add any methods that allow abilities to directly write Health.

ACCEPTANCE TEST:
  ▸ AFLCombat compiles clean.
  ▸ Editor → Project Settings → Gameplay Tags shows
    Event.Damage.Overkill resolves.
  ▸ Open GE_AFL_Damage_Pulse in editor — UAFLDamageExecCalc shows in
    the Executions list.
  ▸ Type `showdebug abilitysystem` in PIE — UAFLAttributeSet_Combat
    appears (no actor uses it yet, but the class registers).

When complete, output "STAGE 2 COMPLETE" with the file list.
```

**Checkpoint after Stage 2:**

1. Build `AFLCombat` clean.
2. In editor → Content Browser → Plugins/AFL Combat Content/GE/ — `GE_AFL_Damage_Pulse` visible, opens without errors.
3. Search the codebase: `grep -rn "Health = \|Health(\|SetHealth" Plugins/GameFeatures/AFL*/Source/` — should match ONLY the AttributeSet's own clamping in `PreAttributeChange`. Any other match is a Rule 2 violation; reset.

---

### STAGE 3 — Lag Compensation Subsystem (`UAFLLagCompensationWorldSubsystem` + per-pawn component)

**Goal**: The rewind machinery exists and ticks on a test pawn. No abilities use it yet, but the API is callable.

**Sprint tasks covered**: AFL-0211

**Prompt:**

```text
═══ STAGE 3 — Lag Compensation Subsystem ═══

GOAL: Implement the lag compensation system per master §7 (doctrine) and
§13.4 (header sketch). API must be callable from validation code in Stage 5.

DELIVERABLES:

1. Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Public/Lag/
   AFLLagCompensationComponent.h   and matching .cpp

   UCLASS(ClassGroup=(AFL), meta=(BlueprintSpawnableComponent))
   class AFLCOMBAT_API UAFLLagCompensationComponent : public UActorComponent

   Internals (private):
     struct FBoneSnapshot { FName Bone; FTransform XForm; };
     struct FFrameSnapshot { double ServerTime; TArray<FBoneSnapshot> Bones; };
     TArray<FFrameSnapshot> Ring;  // sized to TrackedFrameCount, default 64
     int32 Head = 0;

   UPROPERTY(EditDefaultsOnly, Category="AFL|LagComp")
     TArray<FName> TrackedBones = { "head", "neck_01", "spine_03", "spine_01",
       "pelvis", "upperarm_l", "upperarm_r", "thigh_l", "thigh_r",
       "lowerarm_l", "lowerarm_r", "calf_l", "calf_r",
       "hand_l", "hand_r", "foot_l", "foot_r",
       "clavicle_l", "clavicle_r", "neck_02", "head_top", "spine_02",
       "ball_l", "ball_r" };  // 24 bones; can be reduced to 8 if budget tight (master §7.4)

   UPROPERTY(EditDefaultsOnly, Category="AFL|LagComp")
     float HistorySeconds = 1.0f;

   UPROPERTY(EditDefaultsOnly, Category="AFL|LagComp")
     int32 TrackedFrameCount = 64;

   Lifecycle:
     · BeginPlay: register with UAFLLagCompensationWorldSubsystem
     · EndPlay: unregister
     · TickComponent: tick group TG_PostPhysics — capture current bone
       transforms into Ring[Head], advance Head with wraparound.
     · Use the owner's USkeletalMeshComponent (find via FindComponentByClass).
       If no skeletal mesh is found, log warning once and disable tick.

   Public API:
     void SampleAtTime(float DeltaSeconds, TArray<FTransform>& OutTransforms,
                       TArray<FName>& OutBoneNames) const;
     · Walks Ring backwards from Head, finds the two snapshots bracketing
       (now - DeltaSeconds), linearly interpolates bone transforms.
     · If DeltaSeconds is older than oldest snapshot, returns the oldest.

2. Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Public/Lag/
   AFLLagCompensationWorldSubsystem.h   and matching .cpp

   USTRUCT()
   struct FAFLLagSnapshot {
       GENERATED_BODY()
       TWeakObjectPtr<AActor> Target;
       TArray<FTransform> RestoreBoneTransforms;
       TArray<FName> RestoreBoneNames;
       bool bValid = false;
   };

   UCLASS()
   class AFLCOMBAT_API UAFLLagCompensationWorldSubsystem : public UWorldSubsystem

   Public API:
     FAFLLagSnapshot RewindWorldFor(AActor* Target, float DeltaSeconds);
     void RestoreWorld(const FAFLLagSnapshot& Snapshot);
     void RegisterComponent(UAFLLagCompensationComponent* Comp);
     void UnregisterComponent(UAFLLagCompensationComponent* Comp);

   Internals:
     UPROPERTY()
     TArray<TWeakObjectPtr<UAFLLagCompensationComponent>> Components;
     FCriticalSection RewindLock;

   RewindWorldFor implementation:
     · Acquire RewindLock (FScopeLock).
     · Find Target's UAFLLagCompensationComponent.
     · Call SampleAtTime(DeltaSeconds, ...) to get historical bone transforms.
     · Capture current transforms into FAFLLagSnapshot.RestoreBoneTransforms.
     · Apply historical transforms to the target's USkeletalMeshComponent
       via SetBoneTransformByName for each tracked bone.
     · Mark snapshot valid; return.
     · NB: lock is held for the entire rewind transaction; RestoreWorld releases.

   RestoreWorld implementation:
     · If !Snapshot.bValid, return.
     · Apply RestoreBoneTransforms back to the target's mesh in inverse order.
     · Release the RewindLock.

CONSTRAINTS:
  ▸ Subsystem ONLY exists on dedicated server and listen server hosts;
    early-out in ShouldCreateSubsystem if NetMode == NM_Client.
  ▸ Component ticks on server only — TickComponent guards with
    GetOwner()->HasAuthority().
  ▸ Performance budget: 200µs per RewindWorldFor on dedicated server target.
    Add SCOPE_CYCLE_COUNTER stat group AFL_LagComp around RewindWorldFor.

ACCEPTANCE TEST:
  ▸ Compiles clean.
  ▸ Stat group AFL_LagComp shows in `stat startfile` recordings.
  ▸ For sanity: temporarily attach UAFLLagCompensationComponent to
    BP_LyraCharacter (Lyra's default character), launch PIE as listen
    server, type `displayall AFLLagCompensationComponent Ring.Num`.
    Should grow to 64 then stay.

When complete, output "STAGE 3 COMPLETE".
```

**Checkpoint after Stage 3:**

1. Build clean.
2. PIE as listen server with the component temporarily on a Lyra character → Ring populates.
3. `stat unit` → no measurable hitch from component tick on a 4-character scene.
4. Remove the temporary attachment before Stage 4 (we'll add it through PawnData properly there).

---

### STAGE 4 — Lyra Initialization Contract (`UAFLHeroComponent` + PawnData/AbilitySet/InputConfig assets)

**Goal**: Plug the Lyra init choreography in the AFL way. After this stage, an AFL character spawns with NO abilities yet (we have no abilities to grant) but the *path* by which they will be granted is wired.

**Sprint tasks covered**: AFL-0214

**Prompt:**

```text
═══ STAGE 4 — Lyra Initialization Contract ═══

GOAL: Implement UAFLHeroComponent and author the PawnData / AbilitySet /
InputConfig data assets per master §9 and §13.6.

DELIVERABLES:

1. Plugins/GameFeatures/AFLCore/Source/AFLCore/Public/Components/
   AFLHeroComponent.h   and matching .cpp

   UCLASS(ClassGroup=(AFL), meta=(BlueprintSpawnableComponent))
   class AFLCORE_API UAFLHeroComponent : public ULyraHeroComponent

   Override HandleChangeInitState EXACTLY as in master §13.6:
     · Always call Super:: first (ability granting happens in Super at
       InitState_DataInitialized — DO NOT DUPLICATE).
     · On DesiredState == InitState_GameplayReady, hook AFL-specific
       gameplay-ready actions ONLY (HUD, energy registration, voice
       channel registration, etc.). Stub these as TODO comments — we
       wire them in later sprints.

   ABSOLUTE REQUIREMENT: DO NOT call ASC->GiveAbility anywhere in this
   class. The base class handles it via the AbilitySet path.

2. BP_AFLCharacter_Base — created via the AIK Edit Blueprint tool:
   - Parent class: ALyraCharacter
   - Component additions: UAFLHeroComponent (REPLACES the default
     ULyraHeroComponent — remove the parent's component, add ours)
   - NO logic in event graph. NO BeginPlay overrides. Empty Construction
     Script. The Blueprint exists only to compose components.
   - Save to: /Game/AFL/Pawns/BP_AFLCharacter_Base

3. DA_AFL_AbilitySet_Combat_Pulse — Data Asset of class ULyraAbilitySet:
   - GrantedGameplayAbilities: EMPTY for now (Stage 5 will add Pulse)
   - GrantedAttributes: one entry referencing UAFLAttributeSet_Combat
     with InitializationData = ID_AFL_Stats_Default (create as
     UDataTable of FAttributeMetaData rows: Health=100, MaxHealth=100,
     Shield=50, MaxShield=50, Armor=20, OverkillThreshold=50, Heat=0,
     MaxHeat=100)
   - Save to: /Game/AFL/AbilitySets/DA_AFL_AbilitySet_Combat_Pulse

4. IC_AFL_Default — Data Asset of class ULyraInputConfig:
   - NativeInputActions:
       InputTag.Movement.Move ↔ /Lyra/Input/Actions/IA_Move_KBM
       InputTag.Movement.Look ↔ /Lyra/Input/Actions/IA_Look_Mouse
   - AbilityInputActions: EMPTY for now (Stage 5 will add Fire)
   - Save to: /Game/AFL/Input/IC_AFL_Default

5. DA_AFL_TagRelationships — UDataAsset of class
   ULyraAbilityTagRelationshipMapping:
   - Empty for now; we'll add tag-based blocking/cancelling in later sprints
   - Save to: /Game/AFL/Data/DA_AFL_TagRelationships

6. DA_AFL_PawnData_Hero_Default — Data Asset of class ULyraPawnData:
   - PawnClass: BP_AFLCharacter_Base
   - InputConfig: IC_AFL_Default
   - AbilitySets: [DA_AFL_AbilitySet_Combat_Pulse]
   - TagRelationshipMapping: DA_AFL_TagRelationships
   - DefaultCameraMode: leave default (Lyra's third-person)
   - Save to: /Game/AFL/PawnData/DA_AFL_PawnData_Hero_Default

7. B_LyraExperience_AFL_Arena — Blueprint of class
   ULyraExperienceDefinition:
   - DefaultPawnData: DA_AFL_PawnData_Hero_Default
   - GameFeaturesToEnable: [AFLCore, AFLCombat]
   - Actions: empty for now
   - Save to: /Game/AFL/Experiences/B_LyraExperience_AFL_Arena

8. Add the lag-comp component to the character via AbilitySet's
   GrantedAttributes path — actually, since LagComp is a UActorComponent
   not an AttributeSet, add it via the experience instead:
   - Edit B_LyraExperience_AFL_Arena → Actions →
     GameFeatureAction_AddComponents:
       Target: ALyraCharacter
       Component: UAFLLagCompensationComponent
       SpawnActorCondition: ClientAndServer (it self-disables on client)

CONSTRAINTS:
  ▸ ZERO `GiveAbility` calls in the C++ added in this stage (verify with
    grep before finishing).
  ▸ ZERO BeginPlay overrides in BP_AFLCharacter_Base.
  ▸ All asset paths under /Game/AFL/ — no exceptions.

ACCEPTANCE TEST:
  ▸ Compiles clean.
  ▸ Open the WorldSettings of any test map, set DefaultGameplayExperience
    to B_LyraExperience_AFL_Arena, set DefaultPawnClass via experience.
  ▸ PIE as listen server → character spawns as BP_AFLCharacter_Base, has
    UAFLHeroComponent and UAFLLagCompensationComponent attached.
  ▸ `showdebug abilitysystem` → UAFLAttributeSet_Combat registered with
    initialization values from ID_AFL_Stats_Default.
  ▸ Output log: no errors about missing AbilitySets or PawnData.

When complete, output "STAGE 4 COMPLETE".
```

**Checkpoint after Stage 4:**

1. Build clean.
2. PIE on the test map → character spawns, ASC has the AttributeSet, no abilities.
3. **Crucial grep**: `grep -rn "GiveAbility" Plugins/GameFeatures/AFL*/Source/` — must return ZERO matches. Any match is a Rule 3 violation; reset.

---

### STAGE 5 — `UAFLAG_Laser_Pulse` canonical (the v2 implementation)

**Goal**: The Pulse Carbine fires, with full TargetData → server validation → ExecCalc damage flow. This is THE moment of truth: every architectural choice from Stages 1–4 either works here or doesn't.

**Sprint tasks covered**: AFL-0104, AFL-0105, AFL-0106

**Prompt:**

```text
═══ STAGE 5 — Pulse Carbine canonical (v2 server-authoritative) ═══

GOAL: Implement UAFLAG_Laser_Pulse EXACTLY as specified in master §13.1.
This is the canonical hitscan template; future weapons will be cloned
from this implementation.

DELIVERABLES:

1. Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Public/Abilities/
   AFLAG_Laser_Pulse.h   and matching .cpp

   IMPLEMENT VERBATIM from master §13.1. Do not "improve" the algorithm.
   Do not skip validation layers. Do not use GetPlayerViewPoint on the
   server path under any circumstance.

   Specifically verify in your output:
     · ActivateAbility splits client / server paths via IsLocallyControlled
     · Client path calls GetPlayerViewPoint LOCALLY (this is correct on client)
     · Server path binds AbilityTargetDataSetDelegate, then
       CallReplicatedTargetDataDelegatesIfSet for replays
     · ClientPredictAndSend opens FScopedPredictionWindow before
       ServerSetReplicatedTargetData
     · OnTargetDataReplicated runs ValidateTargetData FIRST. On reject,
       calls ConsumeClientReplicatedTargetData and EndAbility(true,true).
     · On accept, sets SetByCaller magnitudes for Headshot/Distance,
       applies GE_AFL_Damage_Pulse via MakeOutgoingSpec → ApplyToTarget.
     · ValidateTargetData implements all four checked layers:
         L1 schema/finite-float check
         L2a distance gate (Dist <= MaxTraceDistance + 200)
         L2b origin gate (claimed start within MaxOriginDeviation of pawn)
         L2c angular gate (angle <= MaxAngularDeviationDegrees)
         L3 lag-comp LOS via UAFLLagCompensationWorldSubsystem with
            ClampedRTT = min(MaxRewindSeconds, ExactPing*0.001*0.5)
     · Use ON_SCOPE_EXIT to guarantee RestoreWorld on early returns from
       the lag-comp block.

2. UPROPERTY defaults match master §13.1:
     · MaxTraceDistance = 9000.f
     · MaxAngularDeviationDegrees = 100.f
     · MaxOriginDeviation = 250.f
     · MaxRewindSeconds = 0.2f
     · bServerVerifyLineOfSight = true

3. Class config defaults:
     · InstancingPolicy   = InstancedPerActor
     · NetExecutionPolicy = LocalPredicted
     · NetSecurityPolicy  = ServerOnlyExecution
     · ReplicationPolicy  = ReplicateNo

4. Create input action and binding:
     · Create /Lyra/Input/Actions/IA_AFL_Fire (Enhanced Input action,
       ValueType Bool, triggered on Pressed)
     · Edit IC_AFL_Default → AbilityInputActions: add
       { InputAction: IA_AFL_Fire, InputTag: InputTag.Weapon.Fire }

5. Edit DA_AFL_AbilitySet_Combat_Pulse:
     · GrantedGameplayAbilities: add
       { Ability: UAFLAG_Laser_Pulse, InputTag: InputTag.Weapon.Fire }

6. Edit GE_AFL_Damage_Pulse:
     · DamageGE on UAFLAG_Laser_Pulse points to GE_AFL_Damage_Pulse
     · Confirm: GE_AFL_Damage_Pulse → Modifiers → Damage attribute, magnitude 18.0
     · Confirm: GE_AFL_Damage_Pulse → Executions → UAFLDamageExecCalc

7. ECC channel:
     · Create custom collision channel ECC_GameTraceChannel1 named
       "AFL_Weapon" in DefaultEngine.ini
     · Default response Block; Pawn = Block; Visibility = Block
     · Ability uses this channel for traces

CONSTRAINTS — ABSOLUTE:
  ▸ ZERO calls to GetPlayerViewPoint outside ClientPredictAndSend (the
    only place it's safe).
  ▸ ZERO direct Health writes — damage flows through GE → ExecCalc.
  ▸ ZERO trust of client TargetData without ValidateTargetData passing.
  ▸ Server lag rewind clamp uses PlayerState->ExactPing, never any
    client-supplied value.

ACCEPTANCE TEST:
  ▸ Compiles clean, zero warnings.
  ▸ PIE as listen server with 2 players (Network → Number of Players: 2).
    Player 1 fires at Player 2:
       · Player 2's Health drops by ~18 minus mitigation (Armor=20 →
         100/(100+20)*18 = 15)
       · Player 2's Shield strips first if non-zero
       · Hit telemetry log appears: "LogAFLCombat: Validation REJECTED"
         should NOT appear on legitimate hits.
  ▸ Cheat test: open Output Log, type
       `Cheat AFLForceTargetDataAtFakeOrigin 0 0 0`
    (you'll need to add this dev console command in Stage 6 — for now,
    this acceptance test simply verifies legitimate hits work; reject
    rate testing is a Stage 6 deliverable).
  ▸ Run `dumpticks` — confirm UAFLLagCompensationComponent ticks at
    TG_PostPhysics on the server character.

When complete, output "STAGE 5 COMPLETE" with:
  · The exact file diff for AFLAG_Laser_Pulse.cpp ValidateTargetData
    (so I can audit the validation order)
  · Confirmation that grep for "GetPlayerViewPoint" returns matches
    ONLY inside ClientPredictAndSend
  · Confirmation that grep for "Health.*=" inside Abilities/ returns
    ZERO matches
```

**Checkpoint after Stage 5 — THIS IS THE PHASE 0 CRITICAL POINT:**

1. Build clean.
2. **2-player PIE listen-server smoke test**: Player 1 shoots Player 2 → damage applies, mitigation correct.
3. **Audit greps** (operator runs these manually):
   ```
   grep -rn "GetPlayerViewPoint" Plugins/GameFeatures/AFL*/Source/
   ```
   Must return matches ONLY inside `ClientPredictAndSend()` — no other location.
   ```
   grep -rn "ASC->GiveAbility\|->GiveAbility(" Plugins/GameFeatures/AFL*/Source/
   ```
   Must return ZERO matches.
   ```
   grep -rn "->Health\|->SetHealth\|Health = " Plugins/GameFeatures/AFL*/Source/Abilities/
   ```
   Must return ZERO matches.
4. **Replication smoke test**: Open `Net.PktLag 80` console command → fire shots → still hits land within tolerance, no rubber-banding.
5. **Studio Lead awareness**: this is the moment to demo the working Pulse Carbine before continuing. Even though Sprint 1 isn't fully done, Phase 0 architecture is proven.

---

### STAGE 6 — Telemetry sink stub + CI lint hook

**Goal**: The reject-event telemetry pipeline is wired (writing to log only — Sprint 13 swaps in PlayFab). The CI rule that enforces our forbidden patterns is committed. After this, the foundation is locked.

**Sprint tasks covered**: AFL-0213, AFL-0215

**Prompt:**

```text
═══ STAGE 6 — Telemetry sink stub + CI lint ═══

GOAL: Wire the validation-reject telemetry path with a stub backend,
and commit the CI lint that enforces the §7/§8/§9 forbidden patterns.

DELIVERABLES:

1. Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Public/Telemetry/
   AFLTelemetrySink.h   and matching .cpp

   USTRUCT()
   struct AFLCOMBAT_API FAFLValidationRejectEvent {
       GENERATED_BODY()
       FString PlayerId;
       FName  AbilityTag;
       FGameplayTag RejectReason;  // Telemetry.Reject.Schema/Distance/Origin/Angle/LOS
       int32  ServerTick = 0;
       FVector ClaimedOrigin = FVector::ZeroVector;
       FVector ClaimedHit    = FVector::ZeroVector;
       float   MeasuredRTTms = 0.f;
   };

   UINTERFACE()
   class AFLCOMBAT_API UAFLTelemetrySink : public UInterface { GENERATED_BODY() };

   class AFLCOMBAT_API IAFLTelemetrySink {
       GENERATED_BODY()
   public:
       virtual void EmitValidationReject(const FAFLValidationRejectEvent& Event) = 0;
   };

   UCLASS()
   class AFLCOMBAT_API UAFLTelemetrySink_LogOnly : public UObject,
                                                   public IAFLTelemetrySink {
       GENERATED_BODY()
   public:
       virtual void EmitValidationReject(const FAFLValidationRejectEvent& Event) override;
       // Implementation: UE_LOG(LogAFLTelemetry, Warning, TEXT("REJECT %s ..."), ...)
   };

   UCLASS()
   class AFLCOMBAT_API UAFLTelemetrySubsystem : public UWorldSubsystem {
       GENERATED_BODY()
   public:
       virtual void Initialize(FSubsystemCollectionBase& Collection) override;
       void Emit(const FAFLValidationRejectEvent& Event);
       // Sets default sink to UAFLTelemetrySink_LogOnly. Sprint 13 will inject
       // a PlayFab-backed sink via SetSink.
       void SetSink(TScriptInterface<IAFLTelemetrySink> InSink);
   private:
       UPROPERTY()
       TScriptInterface<IAFLTelemetrySink> ActiveSink;
   };

2. Update UAFLAG_Laser_Pulse::ValidateTargetData:
   On each rejection branch, BEFORE returning false, populate an
   FAFLValidationRejectEvent and call:
     World->GetSubsystem<UAFLTelemetrySubsystem>()->Emit(Event);
   Use the appropriate Telemetry.Reject.* tag for each rejection cause.

3. .github/workflows/afl-forbidden-patterns.yml — new GitHub Actions
   workflow that runs on every PR:
     · Step 1: grep for forbidden patterns inside
       Plugins/GameFeatures/AFL*/Source/. Failures:
         - `GetPlayerViewPoint` outside .cpp files matching
           `*ClientPredictAndSend*` body (use ripgrep with -A context)
         - `->GiveAbility(` anywhere in Source/
         - `Health = ` or `SetHealth(` inside Source/*/Abilities/ paths
     · Step 2: For matches, dump the offending file:line and FAIL THE BUILD
       with a message pointing the author to master §7/§8/§9.

   Implementation hint: use ripgrep for clarity; example:
       rg -n "GetPlayerViewPoint" Plugins/GameFeatures/AFL*/Source/ \
         | rg -v "ClientPredictAndSend" \
         | tee forbidden_hits.txt
       if [ -s forbidden_hits.txt ]; then
         echo "::error::Rule 1 violation. See master §7."
         cat forbidden_hits.txt
         exit 1
       fi

4. Add a unit test stub:
   Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Private/Tests/
   AFLCombatTests.cpp
     · IMPLEMENT_SIMPLE_AUTOMATION_TEST: AFL.Combat.ExecCalc.MitigationCurve
       Verifies Armor=100 → 50% mitigation; Armor=0 → 0%; Armor=900 → 90%.
     · IMPLEMENT_SIMPLE_AUTOMATION_TEST: AFL.Combat.ExecCalc.ShieldStripsBeforeHealth
       100 damage with Shield=50 → Shield 50→0, Health -50.
     · These run via `Automation RunTests AFL.Combat` in editor cmd.

CONSTRAINTS:
  ▸ Telemetry sink interface MUST allow swap-out without recompile in
    Sprint 13 (that's why we use a UInterface + ActiveSink pointer).
  ▸ Log-only sink writes at LogAFLTelemetry Warning verbosity in
    Development; Verbose in Shipping (use #if UE_BUILD_SHIPPING guard).
  ▸ CI lint must run in <30 seconds — it's a grep, not a parse tree walk.

ACCEPTANCE TEST:
  ▸ Build clean.
  ▸ Cheat: in PIE, force a reject by spawning a fake target far from the
    player and calling ServerSetReplicatedTargetData manually via a dev
    console command (add `AFLDebugForceReject` in this stage). LogAFLTelemetry
    line appears with the right Telemetry.Reject.* tag.
  ▸ Run automation: `Automation RunTests AFL.Combat` in editor → both
    tests pass.
  ▸ Push a temporary commit that adds GetPlayerViewPoint into a server
    code path → CI fails with the master §7 reference message. Revert
    the commit.

When complete, output "STAGE 6 COMPLETE" with the CI workflow content
and confirmation that the deliberate-violation push was rejected by CI.
```

**Checkpoint after Stage 6:**

1. Build clean. Tests pass.
2. CI lint catches a deliberate violation (you push a doctored commit, CI rejects, you revert).
3. The repo is now self-policing for the v1.1 forbidden patterns.

---

### STAGE 7 — Wire-up & first PIE smoke test (Phase 0 sign-off)

**Goal**: Everything from Stages 1–6 plays together in a fresh PIE session. This is what the operator demos to Studio Lead for Phase 0 sign-off.

**Sprint tasks covered**: AFL-0107 (binding wrap-up), final Sprint 1 acceptance gate

**Prompt:**

```text
═══ STAGE 7 — Phase 0 Smoke Test & Sign-off ═══

GOAL: Verify the entire stack from input → ability → validation → ExecCalc
→ telemetry → CI lint, end-to-end. Produce a sign-off report.

OPERATOR ACTIONS (no agent code generation needed):

1. Create Test_Arena map (or use Lyra's L_DefaultMap):
   - Set GameMode override → ALyraGameMode
   - Set DefaultGameplayExperience override → B_LyraExperience_AFL_Arena
   - Add 2 Player Starts spaced 8m apart

2. Run PIE → Network: Run as Listen Server, Number of Players: 2.

3. Test matrix (record results in master tracker):
   ┌──────────────────────────────────────────┬─────────────────┐
   │ Test                                     │ Expected Result │
   ├──────────────────────────────────────────┼─────────────────┤
   │ Player 1 Pulse fires (LMB)               │ Tracer + audio  │
   │ Player 1 hits Player 2 directly          │ Health -15±1    │
   │ Player 2 Shield (set via cheat to 50)    │ Shield strips   │
   │   stripped before Health                 │   first         │
   │ Headshot multiplier (aim at head bone)   │ ~30 dmg pre-mit │
   │ Distance falloff (>2000u distance)       │ Reduced damage  │
   │ Wall LOS (shoot through cover)           │ Reject + telem  │
   │ ExactPing 80ms (Net.PktLag 80)           │ Hits still land │
   │ ExactPing 200ms                          │ Hits still land │
   │ ExactPing 400ms (above MaxRewind)        │ Some misses OK  │
   │ Fire rate exceeded (cooldown not done)   │ Server rejects  │
   │ CI lint catches deliberate violation     │ Build fails     │
   └──────────────────────────────────────────┴─────────────────┘

4. Generate Phase 0 sign-off artifact:
   AGENT: produce /Docs/AFL_Phase0_SignOff.md containing:
     · Date
     · Build SHA (run `git rev-parse HEAD`)
     · UE version + Lyra commit hash
     · Test matrix results (table format, Pass/Fail per row)
     · Architecture compliance audit:
         - GetPlayerViewPoint locations: <list each, confirm Client-only>
         - GiveAbility locations: <should be empty>
         - Direct Health writes in Abilities/: <should be empty>
     · Lag-comp budget measured (microseconds per RewindWorldFor):
         <run `stat AFL_LagComp` in PIE for 60s, capture average>
     · CI green status link
     · Open issues / deviations (if any)
     · Operator name + Studio Lead sign-off line

5. Push branch, open PR titled
   "AFL-0101..AFL-0215 :: Phase 0 Bootstrap (v1.1 architecture)"
   - PR description references master §7, §8, §9
   - Reviewers: Engineering Lead + Combat Lead (both required for AFLCombat)
   - Link to Phase0_SignOff.md

When complete, output "STAGE 7 COMPLETE" and "PHASE 0 SIGN-OFF READY".
```

**Final checkpoint:**

This is the demo. Studio Lead observes the test matrix runs, the CI green check, and signs `Phase0_SignOff.md`. Sprint 1 closes after this.

---

## 3. INTER-STAGE PROTOCOL

Use this loop between every stage:

```
┌──────────────────────────────────────────────────────────────┐
│  AGENT outputs "STAGE N COMPLETE"                            │
│           ▼                                                  │
│  OPERATOR runs the stage's Acceptance Test commands          │
│           ▼                                                  │
│  OPERATOR runs the stage's Checkpoint greps                  │
│           ▼                                                  │
│  ┌─ All pass ────► Commit. Move to Stage N+1.               │
│  └─ Any fail ───► Recovery (§4). Do NOT continue.           │
└──────────────────────────────────────────────────────────────┘
```

**Commit after every clean stage.** Each stage = one commit. Branch history reads like the master document. PR reviewer can audit stage-by-stage.

---

## 4. RECOVERY PLAYBOOK

When a stage fails an acceptance test, **do not let the agent "patch" the failure conversationally** — that's how architectural drift starts. Reset cleanly.

### Common failure modes and the right response

| Symptom | Likely cause | Response |
|---|---|---|
| Compile error: `GetPlayerViewPoint` undefined on server | Agent put it on server path (Rule 1 violation) | Reset stage with the prompt below. |
| `ASC->GiveAbility` shows up in grep | Agent bypassed AbilitySet (Rule 3 violation) | Reset stage. |
| Direct `Health -= N` in ability cpp | Agent skipped ExecCalc (Rule 2 violation) | Reset stage. |
| Lag-comp ticks on client | `HasAuthority` guard missing | Patch in place, re-test. |
| Tests pass but PIE hits don't register | Likely missing `ConsumeClientReplicatedTargetData` after validation | Patch in place. |
| Asset path under `/Game/` not `/Game/AFL/` | Naming/location violation | Move asset, fix references. |
| CI lint workflow doesn't fail on test violation | Workflow file syntax wrong | Fix `.yml`, re-test. |

### Stage Reset Prompt

When the agent has produced architecturally wrong code, paste this:

```text
HALT. The previous output violates Rule [N] (master §[7|8|9]).
Specifically: [one-sentence description of the violation].

Do the following in order:
1. Acknowledge the violation in one sentence.
2. List the files you produced or modified that contain the violation.
3. Revert your work (delete those files / undo edits).
4. Re-read master §13.1 (or §13.4/§13.5/§13.6 as relevant).
5. Wait. Do not regenerate code until I send the corrected stage prompt.
```

If the same violation recurs after a reset: close the AIK chat, reopen, re-paste the **Session Contract** (§1), then resume from the failed stage. State drift in long sessions is real; a clean session usually fixes it.

---

## 5. AFTER PHASE 0

Once Stage 7 signs off, Sprint 1 is `Done` and Sprint 2 starts. The remaining Sprint 1 tasks (AFL-0108 GitHub Actions PR validation, AFL-0109 naming validation, AFL-0110 greybox arena, AFL-0111 QA test plan template) are each small enough to drive with single-shot prompts using the same Session Contract — operators can write those themselves now that the foundation is locked.

For Sprint 2 onwards:
- Switch AIK profile when discipline changes (`AFL VFX & Materials` for VFX work, `AFL Animation` for anim).
- Re-paste an abbreviated Session Contract at the start of each new chat session — Claude Code does not retain state between sessions.
- The CI lint from Stage 6 is your guard rail; trust it and let it gate every PR.

---

## 6. ONE-PAGE QUICK REFERENCE (print this and tape it to the desk)

```
┌────────────────────────────────────────────────────────────────────┐
│                AFL :: NEON ARENA — AIK QUICK REF                   │
├────────────────────────────────────────────────────────────────────┤
│ Agent:    Claude Code           Profile:   AFL Blueprint & Gameplay│
│ Attach:   Master_v1.1.md + Lyra base headers (§0.3)                │
├────────────────────────────────────────────────────────────────────┤
│ THE THREE RULES YOU CANNOT VIOLATE                                 │
│                                                                    │
│  1. No GetPlayerViewPoint on server — TargetData only        (§7)  │
│  2. No direct Health writes — meta-attr + ExecCalc           (§8)  │
│  3. No BeginPlay GiveAbility — AbilitySet via HeroComponent  (§9)  │
│                                                                    │
│  CI lint enforces these. Code review re-enforces these.            │
├────────────────────────────────────────────────────────────────────┤
│ STAGE FLOW                                                         │
│  1 → AFLCore plugin + tags             [AFL-0101, 0102]            │
│  2 → AFLCombat + AttributeSet + ExecCalc [AFL-0103, 0212]          │
│  3 → Lag Compensation subsystem        [AFL-0211]                  │
│  4 → Lyra Init Contract assets         [AFL-0214]                  │
│  5 → Pulse Carbine canonical (v2)      [AFL-0104, 0105, 0106]      │
│  6 → Telemetry stub + CI lint          [AFL-0213, 0215]            │
│  7 → PIE smoke test + sign-off         [AFL-0107 + Sprint 1 gate]  │
│                                                                    │
│  Compile + test + grep audit between every stage.                  │
│  One stage = one commit.                                           │
├────────────────────────────────────────────────────────────────────┤
│ IF SOMETHING SMELLS OFF: STOP. Do not "patch forward."             │
│ Reset the stage (§4). Architectural drift kills schedules.         │
└────────────────────────────────────────────────────────────────────┘
```

---

**END OF AIK SESSION STARTER · v1.0**
