# BAG MAN — MASTER BUILD DOCUMENT
**Single Source of Truth · v2.0 · IMMUTABLE HISTORY EDITION**

> *Competitive cyber arena shooter with extraction stakes, robot dismemberment spectacle, and a social lounge that turns spectators into customers. Codename AFL. Studio: C12 AI Gaming.*

---

## 🔒 SSOT CHARTER — READ BEFORE EDITING

This document is the **single source of truth** for the BAG MAN build. As of v2.0 it operates under an **immutable-history protocol**:

| What you can do | What you cannot do |
|---|---|
| ✅ Add new sections, tasks, sprints, risks, appendix entries | ❌ Delete completed tasks or historical sections |
| ✅ Mark tasks `✅ COMPLETE` when done (with date and commit SHA) | ❌ Remove or rewrite the original wording of any committed task |
| ✅ Add `🔄 SUPERSEDED BY` pointer when a task is replaced by a better approach | ❌ Edit a task's acceptance criteria after it's been worked on |
| ✅ Append new versions (v2.1, v2.2…) with explicit changelog entries | ❌ Renumber existing AFL-XXXX task IDs |
| ✅ Add new chapter sections (§16, §17…) | ❌ Renumber existing chapters |

**The principle**: this document accumulates institutional memory. Six months from now, an engineer should be able to read the doc and understand not just what we built, but **what we tried, what we learned, and what we deliberately set aside**. Deleting completed work erases that lineage.

**The exception**: typos, formatting fixes, and broken links can be silently corrected. Anything that changes meaning gets a new version entry.

---

## 📜 VERSION CHANGELOG (append-only)

### v2.2 — Sprint 3 dash tag schema reconciled to b171bd17 namespace convention
**Date**: 2026-05-12 · **Tagged**: `ssot-v2.2` (pending commit) · **Supersedes tag list in v2.1 §9.6**

- **Meaning-changing correction** — not a typo fix. The original §9.6 tag list (authored 2026-05-12 in commit `ee7d54ee`) named tags `State.Dashing`, `Cooldown.Dash`, and `Ability.Dash.Active`. Those names conflicted with the shipped 51-tag bundle in `Plugins/GameFeatures/AFLCore/Config/Tags/AFLCoreTags.ini` (commit `b171bd17`, AFL-0102), which uses subscope convention `State.Movement.*`, `Cooldown.Movement.*`, `Ability.Movement.*`.
- **Reconciliation rule**: shipped 51-tag file is canonical. SSOT §9.6 tag list bends to match it. Original §9.6 text below is updated **inline** because no Sprint 3 runtime code has been written against the bad names yet (Phase 2 implementation has not begun) — but this changelog entry is the load-bearing audit trail for the rename, per SSOT charter "Anything that changes meaning gets a new version entry."
- **Reconciled tag list (Sprint 3 dash)**:
  - `InputTag.Movement.Dash` — already exists (AFL-0102)
  - `Cooldown.Movement.Dash` — already exists (AFL-0102, was `Cooldown.Dash` in v2.1 §9.6)
  - `State.Movement.Dashing` — already exists (AFL-0102, was `State.Dashing` in v2.1 §9.6)
  - `Ability.Movement.Dash` — already exists (AFL-0102, replaces the v2.1 `Ability.Dash.Active` which was dropped as redundant — Lyra's GA `ActivationOwnedTags` pattern uses the activation-identity tag itself, not a separate `*.Active` subscope)
  - `State.Invulnerable` — **NEW** (cross-cutting, no subscope; follows the `State.Death` precedent. Used by dash i-frame AND spawn protection AFL-0506 AND potential future ability-cast invulnerability)
  - `Cue.Movement.Dash.Activated` — **NEW**
  - `Cue.Movement.Dash.Trail` — **NEW**
- **Net AFL-0301a delta**: 3 new tags (`State.Invulnerable` + 2 cue tags). Down from the originally-specced 7-tag bundle. Four of the original seven already exist in the shipped 51-tag file under the correct subscope namespace.
- **Cross-cutting state-tag precedent**: `State.Death` (cross-cutting, no subscope) establishes the pattern for character-state tags that are not owned by any single system. `State.Invulnerable` follows that pattern. Match-phase tags `State.Match.Warmup` / `State.Match.Ended` (specced in GameMode spec, not yet shipped) further confirm the precedent.
- **`Cue.*` block**: AFL-0301a introduces the first `Cue.*` namespace tags. AFLCoreTags.ini gets a new `; ─── Cue ───` section header to match the existing block-comment style.
- **Process correction noted**: the v2.1 tag list was authored without first grepping `AFLCoreTags.ini`. This v2.2 entry is the audit trail for that miss. Going forward, schema-change work must read the existing `*Tags.ini` files before specifying tag names.
- **Known v2.0 drift (NOT corrected in this commit — scoped out)**: §5 Sprint 3 task table (lines ~361–369: AFL-0303 cites `Cooldown.Dash`, AFL-0306 cites `State.Dashing`) and §13.2 GameplayTag Skeleton (an illustrative `AFLTags.ini` paste-block, lines ~1512–1546) both use the pre-shipping draft tag names without the `Movement.` subscope. These pre-date v2.1 and are not part of this reconciliation's scope. The shipped 51-tag bundle in `AFLCoreTags.ini` is canonical; §9.6 (this section) is the load-bearing Sprint 3 contract. The §5 and §13.2 entries are stale reference text and will be addressed in a separate, scoped cleanup commit before the relevant tickets enter implementation. Flagged here rather than silently rewritten to preserve the immutable-history protocol around task-table wording.

### v2.1 — Sprint 3 architecture correction: Dash Movement Contract; UAFLAttributeSet_Movement deferred
**Date**: 2026-05-12 · **Tagged**: `ssot-v2.1` (pending commit)

- **§9.6 AFL Dash Movement Contract** added — locks Sprint 3 movement as a dash gameplay contract (GA + GE + CMC tag-response + local camera modifier), not as a standalone movement AttributeSet. Authority for all Sprint 3 movement work.
- **§9.7 `UAFLAttributeSet_Movement` (Deferred)** added — formally defers the movement attribute set to Sprint 4, when leg-loss, overdrive, or other movement composition systems create a real consumer.
- **Reason for correction**: the earlier Sprint 3 plan assumed `UAFLAttributeSet_Movement` was the foundation work for Sprint 3 Phase 2. Review of the tracker's Sprint 3 ticket list (AFL-0301 through AFL-0309) confirmed Sprint 3 is ability-, tag-, and CMC-driven with no Sprint 3 consumer for persistent movement attributes. Authoring the AttributeSet now would be premature architecture and violate scope discipline. Earlier `UAFL_ATTRIBUTESET_MOVEMENT_SPEC.md` work is preserved as Sprint 4 reference material, not discarded.
- **New schema-final sub-task**: AFL-0301a (dash tag schema) lands standalone before AFL-0302 runtime implementation, following the same pattern established by AFL-0608 / AFL-0908.
- Section renumbering: none — §9.6 and §9.7 appended to the existing §9 chapter without disturbing §9.1–§9.5.

### v2.0 — Bag Man rebrand, SSOT charter, completed work captured, workflow skills baked in
**Date**: 2026-05-10 · **Tagged**: `ssot-v2.0`

- **Rebrand**: Working title NEON ARENA dropped. Official title is now **BAG MAN**. Internal code prefix `AFL` retained throughout codebase.
- **SSOT Charter** added (this section + the immutable-history rules above).
- **§14 COMPLETED WORK LOG** added — captures all environment setup, planning, and infrastructure work completed through 2026-05-10. Every entry timestamped + tagged in git.
- **§15 AI WORKFLOW & SKILLS INFRASTRUCTURE** added — formalizes the AFL skill suite (Blender bridge, Lyra skin builder, expert game designer, plus the existing AFL-cpp-lyra, AFL-build-operator, AFL-sprint-planner, AFL-asset-pipeline, AFL-neostack-task-writer, AFL-ui-hud-design, AFL-qa-build-recovery) as authoritative tooling for Phase 0+ work.
- **§16 CROSS-DISCIPLINE BRIDGES** added — documents the Blender↔UE5 round-trip workflow, the NeoStack genAI → AFL mesh cleanup pipeline, and the image-to-level / heightmap workflow.
- **Sprint task additions**: AFL-0216 (Blender MCP bridge bootstrap), AFL-0217 (genAI mesh validation pipeline), AFL-0218 (skill registry CI check), placed in Sprint 2 alongside existing AFL-0211-0215.
- Section renumbering: nothing — appended only. Original §14 (Live Build Tracker) → kept as §17. Original §15 (Final Directive) → kept as §18.

### v1.1 — Bag-Man amendments (architecture corrections before Sprint 1)
**Date**: 2026-05-08 · **Tagged**: in repo at first commit

(a) Replaced broken Pulse Carbine appendix with TargetData-driven canonical version; (b) Added §7 hitscan validation doctrine, §8 damage pipeline, §9 Lyra init contract; (c) Added Sprint 2 tasks AFL-0211/0212/0213/0214/0215; (d) Restructured Sprint 11 around AFL-1100 PlayFab→Lambda→GameLift tentpole; pulled EOS Matchmaking out of scope; (e) Added R-09 to risk register; (f) Tightened code review forbidden-pattern list.

### v1.0 — Initial Senior Architect Sign-off Edition
**Date**: 2026-05-08 (earlier same day) · superseded by v1.1 architecture corrections.

---

## 0. HOW TO READ THIS DOCUMENT

This is the **single source of truth** for the build. It is paired with the **Live Build Tracker** (HTML artifact) which the team uses daily to mark task progress.

| You are... | Read these sections |
|---|---|
| Studio Lead / Producer | §1, §2, §3, §10, §14, §17, §18 |
| Engineering Lead (any) | §3, §6, §7, §8, §9, §13, §15, §16 |
| Combat / Gameplay Engineer | §5 (your phase), §7, §8, §9, §13.1, §13.4, §13.5 |
| Online / Backend Engineer | §5 (P3), §10, §13.3 |
| Art / Animation / VFX | §4 pipeline, §5 phase deliverables, §11, §15.1 (Blender), §16 |
| Tech Artist (cross-discipline) | §4, §15 entire, §16 entire |
| QA Lead | §6, §7 (Layer 5 telemetry), §10, §12, §17 |
| New hire onboarding | §1 → §3 → §7 → §8 → §9 → §14 (status) → your phase in §5 |

Every sprint has an explicit **START GATE** (entry criteria) and **END GATE** (exit criteria with QA sign-off). No sprint advances without its end gate passing.

**Authoritative sections — NO violations permitted, enforced by code review and CI lint (AFL-0215):**
- §7 Server-Authoritative Hitscan Validation
- §8 Damage Pipeline & ExecCalc
- §9 Lyra Initialization Contract

---

## 1. PROJECT IDENTITY

```
Title:           BAG MAN
Code Prefix:     AFL (retained throughout C++ namespace)
Studio:          C12 AI Gaming
GitHub Org:      C12-Ai-Gaming
Genre:           Competitive arena shooter + light extraction
Engine:          Unreal Engine 5.6 (Lyra Starter Game base)
Targets:         Win64 (Steam/EGS) → PS5 → XSX → iOS → Android
Match Length:    8–12 minutes
Player Count:    12–20 per match
Online Stack:    Epic Online Services (EOS) + Steamworks + PlayFab backend
Servers:         Dedicated authoritative (AWS GameLift fleet, Linux)
Anti-Cheat:      Easy Anti-Cheat (EAC via EOS)
Source Control:  GitHub (Git LFS, 250 GB allocated to C12-Ai-Gaming org)
CI/CD:           GitHub Actions
PM Tool:         Internal AFL task system (task IDs: AFL-XXXX, immutable)
AI Workflow:     Claude Code (subscription auth) via NeoStack AIK in-editor
                 + 11 AFL skills (§15) covering full discipline matrix
DCC Pipeline:    Blender ↔ UE5 round-trip via blender_mcp (§16)
                 NeoStack genAI (Tripo, Meshy) → AFL mesh validation (§16)
Project Path:    C:\Dev\Bag_Man (Windows dev workstation)
Repo Location:   C:\Dev\Bag_Man (local) → github.com/C12-Ai-Gaming/Bag_Man (deferred push)
```

### 1.1 Studio Identity Pillars (non-negotiable)
1. **Game feel before content.** A weak shot kills the game; missing skins do not.
2. **Lyra is the spine — extend, don't edit.** All work lives in GameFeature plugins.
3. **Server authoritative everything.** Client predicts; server decides.
4. **Spectacle as identity.** Neon, dismemberment, head physics — every match has shareable moments.
5. **Ship vertical slices, not horizontal scaffolding.** Each phase produces a playable, testable artifact.

### 1.2 Naming Heritage
The title evolved through three phases:
- **AFL** — original internal codename (retained as C++ module/class prefix forever).
- **NEON ARENA** — working title used during v1.0–v1.1 architecture planning (dropped in v2.0).
- **BAG MAN** — official title as of v2.0, used in all UI, marketing, and external comms.

Code does not rename. `UAFLAG_Laser_Pulse`, `AFLCore`, `AFLCombat` etc. stay. Only the player-facing title is BAG MAN.

---

## 2. THE NORTH STAR: WHAT "DONE" LOOKS LIKE AT LAUNCH

A shipped game state where:

- **20-player match** spawns and runs to completion on dedicated servers across NA/EU/APAC with <100ms p95 input latency.
- **5 launch weapons** (Pulse, Beam, Ricochet, Nova, Singularity) all server-authoritative, predicted, with distinct identity — no two weapons share VFX or feel.
- **Robot dismemberment system** with 6 hit zones and physics-driven head props that survive >5s with audio reactions.
- **Energy economy + extraction loop** is the core retention engine. Players carry energy, decide when to extract, and convert it to meta currency on success.
- **Stress-Object Mode** — chaos game mode shipped as a launch-day differentiator.
- **Social Lounge** — spectator + mini-games + cosmetic flexing, hosted on cheap low-tick lounge servers.
- **Battle Pass + Store + Inventory** — PlayFab-backed, transaction-safe, no client-side ownership trust.
- **6 maps** at launch, each with one signature mechanic (gravity, walls, jump pads, etc.).
- **Replay + KillCam + Highlight Generator** wired in from day one (it's a content engine, not a feature).
- **EAC integrated, telemetry flowing, live-ops dashboard operational** before public launch.

If we ship the above, Phase 5 (Live Service) sustains the game indefinitely.

---

## 3. ARCHITECTURE OVERVIEW

### 3.1 The Five Layers (and what each owns)

```
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 5 — LIVE OPS                                             │
│  Battle pass · Store rotations · Events · Analytics · Ranked    │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 4 — CONTENT                                              │
│  Maps · Weapons · Skins · Cosmetics · Audio · VFX libraries     │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 3 — ONLINE SERVICES                                      │
│  EOS (auth/match/sessions) · PlayFab (inventory) · Steam        │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 2 — GAME LOOP                                            │
│  Energy economy · Extraction · Overdrive · Stress-Object · GMs  │
├─────────────────────────────────────────────────────────────────┤
│  LAYER 1 — GAME FEEL (THE FOUNDATION)                           │
│  Movement · Weapons · GAS · Dismemberment · Hit feedback        │
└─────────────────────────────────────────────────────────────────┘
```

**Hard rule:** A layer cannot ship until the layer below it is passing all acceptance criteria. Layer 1 must feel AAA before Layer 2 begins.

### 3.2 GameFeature Plugin Map

All AFL-specific work lives in plugins under `Plugins/GameFeatures/`. This preserves upstream Lyra mergeability.

| Plugin | Owns | Phase introduced |
|---|---|---|
| `AFLCore` | Shared types, GameplayTags, base classes | P1 S1 |
| `AFLCombat` | All weapon abilities (`AFLAG_Laser_*`), damage GEs | P1 S2 |
| `AFLMovement` | Dash, Blink, glide tuning, movement abilities | P1 S3 |
| `AFLDismember` | Dismemberment component, head physics, sparks | P1 S4 |
| `AFLEnergy` | Energy attribute set, pickups, overdrive | P2 S1 |
| `AFLExtraction` | Extraction zones, channeling, reward conversion | P2 S2 |
| `AFLChaos` | Stress-object mode, carrier mechanics | P2 S4 |
| `AFLOnline` | EOS integration, matchmaking, parties | P3 S1 |
| `AFLBackend` | PlayFab inventory, currency, progression | P3 S3 |
| `AFLLounge` | Social lounge, mini-games, betting tokens | P4 S3 |
| `AFLLiveOps` | Battle pass, store, events, ranked | P5 S1 |

### 3.3 GAS Architecture (canonical)

```
[ APlayerController (input) ]
            ↓
[ ALyraPlayerState ] ── owns ──▶ [ ULyraAbilitySystemComponent ]
            │                              │
            ▼                              ▼
[ ALyraCharacter (avatar) ]     [ AttributeSets · Tags · Effects · Abilities ]
            │
            ├─▶ UAFLDismemberComponent
            ├─▶ UAFLEnergyCollectorComponent
            └─▶ ULyraEquipmentManagerComponent (Lyra-owned)
```

**ASC lives on `PlayerState`.** Persistent across respawn. Multiplayer-safe. **Never put ASC on the Character.**

### 3.4 Naming Conventions (enforced by linter in CI)

| Prefix | Type |
|---|---|
| `AFLAG_` | GameplayAbility (e.g., `AFLAG_Laser_Pulse`) |
| `AFLGE_` | GameplayEffect (e.g., `AFLGE_Damage_Pulse`) |
| `AFLAS_` | AttributeSet (e.g., `AFLAS_Combat`) |
| `AFLC_` | Component (e.g., `UAFLEnergyCollectorComponent`) |
| `BP_AFL` | Blueprint actor (e.g., `BP_AFLEnergyPickup`) |
| `DA_AFL` | Data Asset (e.g., `DA_AFLPulseAbilitySet`) |
| `M_AFL` / `MI_AFL` | Material / Material Instance |
| `NS_AFL` | Niagara System |
| `SK_AFL` / `SM_AFL` | Skeletal / Static Mesh |
| `AM_AFL<Char>_<Action>` | AnimMontage |

---

## 4. CONTENT PIPELINE (DCC → Engine)

### 4.1 Source Control Discipline
- **Code repo**: `afl-game` (GitHub) — protected `main`, working branch `develop`, feature branches per task ID.
- **Asset repo**: Git LFS on `afl-game-assets` for `.uasset` >1MB, `.fbx`, `.wav`, `.exr`, `.psd`.
- **Branching**: `feature/AFL-XXXX-short-name`, `bugfix/AFL-XXXX-...`, `hotfix/AFL-XXXX-...`.
- **PR rule**: Every PR references the AFL task ID in title. CI runs `RunUAT BuildCookRun` validation per PR for Win64 Development.

### 4.2 Asset Acceptance Pipeline
```
DCC (Maya/Blender/Substance)
    ↓ (export with AFL FBX preset)
Validation script (naming, scale, pivot, materials)
    ↓
UE5 Import (auto-LOD, mobile compression variant)
    ↓
Asset Audit (collisions, lightmaps, Nanite eligibility)
    ↓
PR review → merge to develop
```

**No asset enters the project without passing the validation script.** It runs in CI; failed assets block PR merge.

### 4.3 Multi-Platform Material Strategy
Every shippable material has **two variants**:
- `M_AFL_<Name>` — full quality (PC/Console)
- `M_AFL_<Name>_Mobile` — simplified (iOS/Android), no Lumen-dependent nodes, ≤32 instructions

This is enforced by the asset validator.

---

## 5. PHASE & SPRINT BREAKDOWN

We run **2-week sprints**. Each phase has explicit entry and exit gates.

```
PHASE 1 — THE FUN TEST          (Sprints 1–6,   Weeks 1–12)   GO/NO-GO at end
PHASE 2 — CORE GAME LOOP        (Sprints 7–10,  Weeks 13–20)
PHASE 3 — ONLINE & BACKEND      (Sprints 11–14, Weeks 21–28)  parallel content begins S13
PHASE 4 — CONTENT & POLISH      (Sprints 15–20, Weeks 29–40)
PHASE 5 — LIVE SERVICE LAUNCH   (Sprints 21–24, Weeks 41–48)  → SHIP

TOTAL: 48 weeks (12 months) to launch
```

---

### PHASE 1 — THE FUN TEST  *(Sprints 1–6 · 12 weeks)*

**Phase Goal**: A 4-player local-network match where movement and laser combat feel AAA. If this isn't fun, nothing else matters.

**START GATE (entry criteria)**
- [ ] Lyra Starter Game cloned, builds clean on Win64 Development
- [ ] GitHub repo + Git LFS configured, GitHub Actions CI green on empty PR
- [ ] AFL custom AIK profiles installed on all engineer machines
- [ ] Project naming/folder conventions documented (this doc, §3.4)
- [ ] All licensed dev kits (PS5, XSX) on shelf; mobile dev devices ordered

**END GATE (Phase 1 ships when ALL pass)**
- [ ] 4-player listen-server match runs at locked 60 fps on mid-tier PC
- [ ] Pulse Carbine + Prism Beam both feel responsive, predicted, server-validated via TargetData (see §7)
- [ ] Dash ability fluid, no rubber-banding, replicated correctly
- [ ] Dismemberment system: head pops on headshot, rolls >3s with physics+audio
- [ ] One playable arena map (Arena_01) with all combat surfaces
- [ ] Internal team playtest: ≥80% of players say "this feels good" in survey
- [ ] Crash-free for 30 minutes of continuous play across 8 sessions
- [ ] **Hitscan validation reject rate < 1% on legitimate clients across 30/80/200 ms RTT cohorts** (see §7, §AFL-0144 telemetry)
- [ ] **Damage pipeline routes through `UAFLDamageExecCalc` — zero direct `Health` modifications from any ability** (see §8)
- [ ] **All abilities granted via `ULyraPawnData` ability sets — zero `BeginPlay` granting in code review audit** (see §9)

#### Sprint 1 — Foundations & Pulse Skeleton  *(Weeks 1–2)*

**Sprint Goal**: Project scaffolded, Lyra extended cleanly, Pulse Carbine fires and applies damage end-to-end.

| ID | Title | Discipline | Est | Owner |
|---|---|---|---|---|
| AFL-0101 | Stand up `AFLCore` GameFeature plugin (uplugin, Build.cs, module) | Engineering | M | Eng Lead |
| AFL-0102 | Define core GameplayTags table (`Ability.*`, `State.*`, `Event.*`) | Engineering | S | Eng |
| AFL-0103 | Create `UAFLAS_Combat` AttributeSet (Health/Shield/Heat) with `ATTRIBUTE_ACCESSORS` | Engineering | M | Eng |
| AFL-0104 | Stand up `AFLCombat` plugin; create `UAFLAG_Laser_Pulse` extending `ULyraGameplayAbility` with **client-predicted TargetData → server validation pattern** (see §7, §10.1) | Engineering | XL | Eng |
| AFL-0105 | Create `GE_AFL_Damage_Pulse` (Instant, sets `Damage` meta-attribute = 18) wired to `UAFLDamageExecCalc`; create `GE_AFL_EnergyGain_Small` | Engineering | M | Eng |
| AFL-0106 | Hitscan: client local trace (camera-aligned), pack `FGameplayAbilityTargetDataHandle`, ship via `ServerSetReplicatedTargetData`. **No server-side `GetPlayerViewPoint`** | Engineering | L | Eng |
| AFL-0107 | Bind Pulse to `InputTag.Weapon.Fire` via `ULyraInputConfig` | Engineering | S | Eng |
| AFL-0108 | GitHub Actions: Win64 Development build + cook on every PR to `develop` | DevOps | M | DevOps |
| AFL-0109 | Asset naming validation script (Python, runs in CI) | DevOps | M | DevOps |
| AFL-0110 | Greybox arena `Arena_01_Greybox` (60×60m, four ramps, two cover walls) | Art | L | Level Designer |
| AFL-0111 | QA test plan template + Pulse smoke test pass criteria | QA | S | QA Lead |

**Definition of Sprint 1 Done**: Player presses LMB → server applies damage → opponent's health drops → no exceptions in editor or packaged Development build.

#### Sprint 2 — Pulse Polish + Beam Foundation  *(Weeks 3–4)*

**Sprint Goal**: Pulse feels great. Beam exists with continuous damage and heat.

| ID | Title | Discipline | Est |
|---|---|---|---|
| AFL-0201 | `NS_AFL_PulseBeam` Niagara — thick emissive trail, taper at impact | VFX | M |
| AFL-0202 | `NS_AFL_HitSpark` impact effect with sparks + radial bloom | VFX | M |
| AFL-0203 | `M_AFL_NeonMaster` master material with `Mobile` variant | VFX | M |
| AFL-0204 | Hit confirmation: crosshair flash, 80ms screen shake, hitmarker UMG widget | UI/Engineering | M |
| AFL-0205 | `SFX_AFL_PulseShot` + `SFX_AFL_HitConfirm` audio integration via MetaSounds | Audio | M |
| AFL-0206 | `UAFLAG_Laser_Beam` channeling ability — tick damage every 0.1s, 1.2 dmg/tick | Engineering | L |
| AFL-0207 | Heat system: Add `Heat`/`MaxHeat` to `AFLAS_Combat`; `State.Overheated` tag prevents fire | Engineering | M |
| AFL-0208 | `NS_AFL_PrismBeam` continuous beam Niagara w/ heat-driven flicker | VFX | L |
| AFL-0209 | Recoil/spread tuning on Pulse (data-driven via `DA_AFLPulseTuning`) | Design | S |
| AFL-0210 | QA: Pulse + Beam + Heat regression matrix on Win64 Dev | QA | M |
| AFL-0211 | **`UAFLLagCompensationWorldSubsystem` + per-pawn `UAFLLagCompensationComponent`** — 60Hz hitbox snapshot ring buffer, 1s history, `RewindWorldFor` / `RestoreWorld` API. See §7 Layer 3 | Engineering | XL |
| AFL-0212 | **`UAFLDamageExecCalc` + `UAFLAttributeSet_Combat` rework** — `Damage` meta-attribute, armor/shield mitigation, overkill threshold tag emission. See §8 | Engineering | L |
| AFL-0213 | **Telemetry hook** — emit validation reject events to PlayFab Player Streams (stub-mode in P1 writing to local `LogAFLTelemetry`, wired to PlayFab in S13). See §7 Layer 5 | Engineering | M |
| AFL-0214 | **Lyra init contract**: author `DA_AFL_PawnData_Hero_Default`, `IC_AFL_Default`, `DA_AFL_AbilitySet_Combat_Pulse`, and `UAFLHeroComponent` extending `ULyraHeroComponent`. Wire ability granting through ability set on `InitState_DataInitialized`. See §9 | Engineering | L |
| AFL-0215 | **CI rule**: lint Pass that fails any PR containing `GiveAbility` outside an `AbilitySet`, any direct `Health` write from a `GameplayAbility`, or any server-context `GetPlayerViewPoint` | DevOps | M |
| AFL-0216 | **Blender MCP bridge bootstrap** — install `blender_mcp` connector on dev workstations, validate Blender↔Claude Code round-trip with a test asset (kitbash one Lyra prop, modify in Blender via MCP, reimport to UE5 with locked FBX settings). See §16.1 | Tech Art / Engineering | L |
| AFL-0217 | **GenAI mesh validation pipeline** — author `tools/afl_mesh_validator.py` that ingests Tripo/Meshy outputs, runs AFL-conformant checks (poly budget per LOD, naming, materials, collision, sockets), and produces a pass/fail manifest. See §16.2 | Tech Art / DevOps | L |
| AFL-0218 | **Skill registry CI check** — workflow that fails any PR whose AIK profile references an undocumented skill. Source of truth for "documented" is `/Docs/SKILLS_REGISTRY.md` (auto-generated from §15). See §15.4 | DevOps | S |

#### Sprint 3 — Movement: Dash + Mobility Tuning  *(Weeks 5–6)*

**Sprint Goal**: Dash feels cyberpunk-skater, not RPG-roll. Movement is the skill ceiling.

| ID | Title | Discipline | Est |
|---|---|---|---|
| AFL-0301 | Stand up `AFLMovement` plugin | Engineering | S |
| AFL-0302 | `UAFLAG_Dash` — `LaunchCharacter` impulse 1500u, 0.12s duration, 3s cooldown | Engineering | M |
| AFL-0303 | `AFLGE_DashCooldown` with `Cooldown.Dash` tag | Engineering | S |
| AFL-0304 | Movement tuning pass: lower friction during dash, raised air control to 0.6 | Engineering/Design | M |
| AFL-0305 | `NS_AFL_DashTrail` motion blur burst + chromatic aberration pulse | VFX | M |
| AFL-0306 | Optional 0.1s i-frame window via `State.Dashing` tag (immune to damage GEs) | Engineering | S |
| AFL-0307 | Replication validation: dash on listen server with 4 clients, 0 desync over 50 trials | QA/Engineering | M |
| AFL-0308 | `SFX_AFL_DashWoosh` doppler audio | Audio | S |
| AFL-0309 | Camera FOV pulse during dash (110° → 95° → back over 0.3s) | Engineering | S |

#### Sprint 4 — Dismemberment System (signature feature)  *(Weeks 7–8)*

**Sprint Goal**: Headshots pop heads. Heads roll. Heads are funny. This is the viral feature.

| ID | Title | Discipline | Est |
|---|---|---|---|
| AFL-0401 | Stand up `AFLDismember` plugin | Engineering | S |
| AFL-0402 | `UAFLDismemberComponent` on `BP_AFLPlayerCharacter` — tracks 6 hit zones (Head, L/R Arm, L/R Leg, Torso) | Engineering | L |
| AFL-0403 | Hit zone detection from `FHitResult.BoneName` → maps to enum `EAFLBodyZone` | Engineering | M |
| AFL-0404 | `BP_AFLDismemberedHead` physics actor — spawns on detach, RB enabled, audio component | Engineering/Animation | M |
| AFL-0405 | Robot character rig with breakable sockets at all 6 zones | Animation/Art | L |
| AFL-0406 | `NS_AFL_BodyPartSparks` — sparks + wires + emissive at detach point | VFX | M |
| AFL-0407 | Head VO bank: 12 lines ("dignity compromised", "where am I going", etc.) | Audio | M |
| AFL-0408 | Replicated dismemberment events (client cosmetic spawn from server-driven multicast) | Engineering | M |
| AFL-0409 | Arm-loss penalty: weapon recoil ×1.5 if armless. Leg-loss: speed ×0.5 + crawl anim | Engineering/Design | L |
| AFL-0410 | Performance: pooled head props, max 8 active globally | Engineering | M |
| AFL-0411 | QA: Headshot test matrix at 5 distances × 4 weapons | QA | M |

#### Sprint 5 — Arena_01 Production + VFX Polish  *(Weeks 9–10)*

**Sprint Goal**: One real production-quality map. Combat readability locked in.

| ID | Title | Discipline | Est |
|---|---|---|---|
| AFL-0501 | `Arena_01_Production` — replace greybox with neon-cyber art pass (modular kit) | Art/Level Design | XL |
| AFL-0502 | Lighting pass: emissive-driven, low ambient, neon accents at sightlines | Art (Lighting) | L |
| AFL-0503 | Mobile material variants for all Arena_01 assets | VFX | L |
| AFL-0504 | Beam visibility tuning vs. background (no laser ever lost in environment color) | VFX/Design | M |
| AFL-0505 | Player silhouette readability — rim light shader on `M_AFL_RobotMaster` | Art | M |
| AFL-0506 | Spawn point logic + spawn protection (1.5s `State.Invulnerable`) | Engineering | M |
| AFL-0507 | Arena collision audit; no exploitable geometry; bounds volume | QA/Level Design | M |

#### Sprint 6 — Phase 1 Polish, Profiling, GO/NO-GO  *(Weeks 11–12)*

**Sprint Goal**: Phase 1 ships at quality. Internal playtest + go/no-go decision.

| ID | Title | Discipline | Est |
|---|---|---|---|
| AFL-0601 | Profiling pass: Unreal Insights, lock 60fps on RTX 3060 / Ryzen 5 baseline | Engineering | L |
| AFL-0602 | Niagara budget audit: ≤500 active particles per beam, GPU sim mandatory | VFX/Engineering | M |
| AFL-0603 | Hit feedback final pass: subwoofer thump, controller rumble, screen flash | Audio/Engineering | M |
| AFL-0604 | Internal playtest #1 — 4-player session, post-test survey | All | M |
| AFL-0605 | Bug bash week — every team member files 5+ tickets | All | M |
| AFL-0606 | Phase 1 retrospective + Phase 2 sprint planning workshop | All | S |
| AFL-0607 | **GO/NO-GO REVIEW** with Studio Lead | Studio Lead | S |

**🚨 GO/NO-GO criteria**: All Phase 1 End Gate items pass + playtest sentiment ≥80% positive on game feel. If NO-GO, Sprint 6.5 inserted for game feel rework before Phase 2 begins.

---

### PHASE 2 — CORE GAME LOOP  *(Sprints 7–10 · 8 weeks)*

**Phase Goal**: The retention loop. Players fight → drop energy → collect → risk extraction → cash out. The "one more match" engine.

**END GATE**
- [ ] Energy drops on overload, magnetically attracts to players, accumulates correctly
- [ ] Overdrive Mode triggers at threshold with visible buff state
- [ ] Extraction zones spawn, channel, reward, fail under damage
- [ ] Stress-Object mode playable end-to-end
- [ ] One full match (warmup → active → extraction window → end) runs cleanly

#### Sprint 7 — Energy Economy  *(Weeks 13–14)*
- AFL-0701 — `AFLEnergy` plugin + `UAFLAS_Energy` (CarriedEnergy, MaxEnergy, OverdriveThreshold)
- AFL-0702 — `UAFLEnergyCollectorComponent` on character, tracks carry + capacity
- AFL-0703 — `BP_AFLEnergyPickup` actor (3 tiers: Small/Medium/Large, +10/+25/+50)
- AFL-0704 — Magnetic attraction within 500u via `VInterpTo`, value-driven glow intensity
- AFL-0705 — `UAFLAG_Energy_Pickup` on overlap → applies `AFLGE_EnergyGain`
- AFL-0706 — Overload event replaces death: drops 70% carried energy as pickups, fast respawn
- AFL-0707 — `UAFLAG_Overdrive` activates at threshold; `AFLGE_OverdriveBuff` (+25% damage, +15% speed)
- AFL-0708 — Energy meter UMG widget (center-bottom, pulses at threshold)

#### Sprint 8 — Extraction System  *(Weeks 15–16)*
- AFL-0801 — `AFLExtraction` plugin
- AFL-0802 — `BP_AFLExtractionZone` with states: Inactive/Active/Contested
- AFL-0803 — `UAFLAG_Extract` channel ability, 6s, interrupted by damage
- AFL-0804 — Match-flow timer in custom GameMode triggers extraction windows (every 2.5min)
- AFL-0805 — `AFLGE_ExtractionReward` — converts CarriedEnergy → MetaCurrency (off-GAS, posts to backend)
- AFL-0806 — Extraction UI: timer, zone status, contested indicator
- AFL-0807 — Audio escalation during extraction (rising synth, pitch-shifts on damage taken)
- AFL-0808 — Failure state: drop all carried energy on interrupt

#### Sprint 9 — Custom GameMode + Match Flow  *(Weeks 17–18)*
- AFL-0901 — `ALyraGameMode_AFLExtractionArena` extends `ALyraGameMode`
- AFL-0902 — Match phases: Warmup (30s) → Active (8min) → Extraction Window (2min) → End
- AFL-0903 — `DA_AFLExperience_ExtractionArena` LyraExperience definition
- AFL-0904 — Match HUD: phase indicator, time remaining, scoreboard
- AFL-0905 — End-of-match summary screen (kills, energy extracted, MVP)
- AFL-0906 — Player respawn timing: 3s base, +1s per recent death (anti-spam)
- AFL-0907 — Listen server scaling test: 8 players, 12 minutes, no leaks

#### Sprint 10 — Stress-Object Chaos Mode  *(Weeks 19–20)*
- AFL-0801 — `AFLChaos` plugin + `BP_AFLStressObject` carrier object
- AFL-0802 — Pickup logic: any player within radius can grab; instant drop on damage taken
- AFL-0803 — Carrier multipliers: 2× score, 1.5× extraction reward, +30% damage taken
- AFL-0804 — Object instability: glows brighter over time, repositions if held >45s
- AFL-0805 — Squishy physics simulation, audio chirps/squeaks on bounce
- AFL-0806 — `ALyraGameMode_AFLChaosObject` mode variant
- AFL-0807 — Phase 2 internal playtest #2

---

### PHASE 3 — ONLINE & BACKEND  *(Sprints 11–14 · 8 weeks)*

**Phase Goal**: Move off listen server. Real matchmaking, real backend, real economy integrity.

**END GATE**
- [ ] EOS authentication + sessions + matchmaking working cross-platform
- [ ] Dedicated server fleet on AWS GameLift, region-aware
- [ ] PlayFab inventory + currency authoritative; client never trusted
- [ ] Easy Anti-Cheat integrated and bypass-tested
- [ ] 20-player match runs stable for 12 minutes end-to-end on dedicated server

#### Sprint 11 — Backend Microservice Tentpole + EOS Foundation  *(Weeks 21–22)*

**Sprint Goal**: The PlayFab→Lambda→GameLift glue microservice exists, is tested in isolation, and can spin up a dedicated server from a synthetic match payload. EOS is integrated for voice/friends/EAC ONLY — **EOS Matchmaking is explicitly out of scope** (see §10).

| ID | Title | Discipline | Est |
|---|---|---|---|
| AFL-1100 | **TENTPOLE — Author PlayFab→Lambda→GameLift CDK stack** with synthetic match payload test harness. Lambda receives PlayFab `OnMatchFound` webhook, calls `CreateGameSession` + `CreatePlayerSessions`, returns IP/Port/`PlayerSessionId`. Must pass standalone test before any client integration. See §10 | Engineering / DevOps | XXL |
| AFL-1101 | `AFLOnline` plugin scaffolding + EOS SDK integration (identity, voice, friends, EAC ONLY) | Engineering | M |
| AFL-1102 | EOS authentication flow (Steam/Epic ticket → EOS user → cross-platform identity bridge) | Engineering | M |
| AFL-1103 | EOS Voice Chat (proximity in-match, lobby in lounge) | Engineering | M |
| AFL-1104 | EOS Friends + Parties (party rosters travel with PlayFab matchmaking ticket) | Engineering | M |
| AFL-1105 | Friend list + invite UMG widgets | UI | M |
| AFL-1106 | Presence integration (cross-platform rich presence) | Engineering | S |
| AFL-1107 | **Sprint exit**: integration test — PlayFab ticket created → Lambda fires → GameLift session created → IP returned → CI integration test green end-to-end | QA / DevOps | L |

#### Sprint 12 — Dedicated Servers + GameLift  *(Weeks 23–24)*
- AFL-1201 — Linux dedicated server build target in UE5
- AFL-1202 — Docker container packaging for dedicated server
- AFL-1203 — AWS GameLift fleet setup (us-east-1 first region) with FleetIQ spot-instance config
- AFL-1204 — **PlayFab Matchmaking ticket flow** (client → PlayFab queue → Lambda → GameLift via S11 microservice). EOS is voice/friends/EAC ONLY at runtime. See §10
- AFL-1205 — Server tick rate config (30Hz match, 10Hz lounge)
- AFL-1206 — Server health metrics → CloudWatch
- AFL-1207 — Connection migration on server crash (graceful disconnect)
- AFL-1208 — Easy Anti-Cheat integration via EOS

#### Sprint 13 — PlayFab Backend  *(Weeks 25–26)*
- AFL-1301 — `AFLBackend` plugin + PlayFab SDK
- AFL-1302 — Player account auto-link (EOS ID → PlayFab account)
- AFL-1303 — Authoritative inventory: items, currency, cosmetics
- AFL-1304 — Match-end transaction: server posts results to PlayFab CloudScript
- AFL-1305 — Currency awarding on extraction success (server-validated only)
- AFL-1306 — Anti-tamper: HMAC-signed match results, replay attack protection
- AFL-1307 — **Wire AFL-0213 telemetry stub to live PlayFab Player Streams** — validation rejects, angular-velocity anomalies, headshot ratios stream to dashboards. See §7 Layer 5
- AFL-1308 — *(Parallel content begins)* Weapon #3 — Ricochet Lancer (`UAFLAG_Laser_Ricochet`)

#### Sprint 14 — Hardening + 20-Player Stress Test  *(Weeks 27–28)*
- AFL-1401 — 20-player stress test on production-spec server
- AFL-1402 — Bandwidth audit per player (target: <128 kbit/s up, <512 kbit/s down)
- AFL-1403 — Replication relevancy + dormancy on energy pickups
- AFL-1404 — Anti-cheat bypass attempts (red team week)
- AFL-1405 — Crash analytics dashboard (PlayFab → custom Grafana)
- AFL-1406 — Region failover test (kill us-east-1, verify EU pickup)
- AFL-1407 — Phase 3 retrospective + Phase 4 planning

---

### PHASE 4 — CONTENT & POLISH  *(Sprints 15–20 · 12 weeks)*

**Phase Goal**: Fill the game with content. 5 weapons → 12. 1 map → 6. Skin system. Replay/KillCam. Lounge.

**END GATE**
- [ ] All 12 weapons shipped, each with distinct identity validated by playtest
- [ ] 6 maps each with one signature mechanic
- [ ] Skin system + 6 skins per weapon launch SKU
- [ ] Replay system + KillCam + Highlight Generator working
- [ ] Social Lounge operational with mini-games

#### Sprint 15 — Weapons #4–#5 (Nova Burst + Singularity Cannon)  *(Weeks 29–30)*
- Cone-trace shotgun-laser; charge-and-release devastator
- Tasks: AFL-1501..1510 (full weapon checklist per AFL conventions)

#### Sprint 16 — Map_02 + Skin System Foundation  *(Weeks 31–32)*
- Map_02 with moving laser walls signature mechanic
- Master skin material with reactive layers (heat, streak, low health)
- Skin equip/preview backend wired to PlayFab
- Tasks: AFL-1601..1610

#### Sprint 17 — Replay + KillCam + Highlights  *(Weeks 33–34)*
- Unreal Replay system enabled on dedicated servers
- Auto-generated highlight clips on multikills
- Killcam after death (3s replay from killer POV)
- Streamer mode UI (hide sensitive UI)
- Tasks: AFL-1701..1708

#### Sprint 18 — Maps_03 + _04 + Weapons #6–#9  *(Weeks 35–36)*
- 4 mid-tier weapons: Chain-laser, melt rifle, energy pistol, mine projector
- Maps with gravity shifts, energy storms
- Tasks: AFL-1801..1815

#### Sprint 19 — Social Lounge + Mini-Games  *(Weeks 37–38)*
- `AFLLounge` plugin
- Lounge map with spectator walls
- Head-kicking mini-game (uses dismemberment heads)
- Beam basketball, hover races
- Betting tokens (non-monetary, cosmetic XP)
- Tasks: AFL-1901..1912

#### Sprint 20 — Maps_05 + _06 + Weapons #10–#12  *(Weeks 39–40)*
- 3 final weapons (chaos/utility roles)
- Maps with vertical rails, shrinking extraction zones
- Final content lock for launch
- Tasks: AFL-2001..2015

---

### PHASE 5 — LIVE SERVICE LAUNCH  *(Sprints 21–24 · 8 weeks)*

**Phase Goal**: Battle pass, store, ranked, analytics, cert, **SHIP**.

**END GATE = LAUNCH READINESS**
- [ ] Battle Pass S1 fully authored, tracked, rewarded
- [ ] Store rotation system + 3 launch-week rotations queued
- [ ] Ranked mode operational with MMR + season rewards
- [ ] Telemetry validated against 12 KPI dashboards
- [ ] Console cert pass (TRC for PS5, XR for Xbox)
- [ ] Marketing build delivered to PR partner 4 weeks before launch
- [ ] Public stress test (10K concurrent) passed

#### Sprint 21 — Battle Pass + Store  *(Weeks 41–42)*
- `AFLLiveOps` plugin
- Battle Pass S1: 100 tiers, 50 rewards, XP curve
- Store rotation engine + admin dashboard
- Bundle system

#### Sprint 22 — Ranked + Progression  *(Weeks 43–44)*
- MMR algorithm (Glicko-2 derivative)
- Season reset logic
- Ranked rewards
- Player progression dashboard

#### Sprint 23 — Cert + Compliance  *(Weeks 45–46)*
- PS5 TRC pass
- Xbox XR pass
- Mobile store compliance (iOS App Store, Google Play)
- GDPR/CCPA compliance audit

#### Sprint 24 — Launch Week  *(Weeks 47–48)*
- Public stress test
- Live ops war room setup
- Day 0 hotfix readiness
- **🚀 LAUNCH**

---

## 6. WORKFLOW

### 6.1 Daily Cadence

```
09:00  Standup           (15 min, async-friendly via Discord thread)
       — Yesterday / Today / Blockers per discipline track
09:15  Engineering deep work (no meetings until 12:00)
12:00  Lunch + open desk (informal pair programming)
13:00  Discipline-specific syncs (rotating: art, anim, eng)
14:00  Deep work
17:00  PR review window (60 min, all engineers)
18:00  EOD — push WIP branches; CI builds overnight Win64+PS5+XSX
```

### 6.2 Sprint Cadence (2 weeks)

```
Day 1   Sprint Planning    (2h, all team)
Day 1   Sprint Brief published in PM tool + Discord pinned
Day 5   Mid-sprint check    (30 min, leads only)
Day 10  Bug bash morning + Sprint Demo afternoon (1h)
Day 10  Sprint Retro        (45 min, blameless)
Day 10  GO/NO-GO on phase end gates if applicable
```

### 6.3 NeoStack AIK + Claude Code Integration Workflow

This is how engineers actually use Claude inside Unreal day-to-day.

```
┌──────────────────────────────────────────────────────────────────┐
│  ENGINEER OPENS UNREAL EDITOR                                    │
│  Tools → Agent Chat                                              │
│  Active Profile: AFL Blueprint & Gameplay (or VFX, Animation)    │
└──────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                  Pull AFL task from PM tool
                              │
                              ▼
        ┌─────────────────────────────────────────┐
        │  Decide: in-editor (AIK) or terminal    │
        │  (Claude Code CLI)?                     │
        └─────────────────────────────────────────┘
            │                                │
   In-editor BP/asset work        C++ / build / refactor work
            │                                │
            ▼                                ▼
   AIK prompt with context        Claude Code in repo terminal
   attachment (parent BP,         working against feature branch
   anim graph, BT)                                │
            │                                ▼
            ▼                       Branch: feature/AFL-XXXX
   Generated asset                       │
   placed in correct                     ▼
   plugin folder                  Compile + smoke test
            │                                │
            ▼                                ▼
   Manual playtest in PIE      Push → PR with AFL-XXXX in title
            │                                │
            └────────────────┬───────────────┘
                             ▼
                  GitHub Actions CI runs:
                  - Compile Win64 Development
                  - Run validation scripts
                  - Cook content
                  - Block merge if any fail
                             │
                             ▼
                   PR review → merge to develop
                             │
                             ▼
                  Nightly cooked builds: Win64, PS5, XSX
                  Distributed via UnrealGameSync / S3
```

### 6.4 AIK Prompt Template (use this verbatim)

When dispatching tasks to Claude via AIK, format the prompt as:

```
[Task ID] AFL-XXXX
[Context] This task is part of [Feature], [Sprint N].
[Acceptance criteria — paste from task]
[Lyra base class to extend]: e.g., ULyraGameplayAbility
[Conventions]: AFL naming, GameFeature plugin location, replication policy.
[Platform notes]: Platform guards required if applicable.
[Attached]: <parent BP / data asset / referenced material>
```

The AFL custom AIK profiles already inject project context — but explicit prompts produce better output every time.

### 6.5 Code Review Standards

Every PR requires:
- [ ] AFL task ID in title and branch name
- [ ] Linked acceptance criteria checked off in PR description
- [ ] Min 1 reviewer (2 for `AFLCore`, `AFLCombat`, `AFLOnline`, `AFLBackend` plugins)
- [ ] CI green: compile, validation, cook, lint
- [ ] No new compiler warnings
- [ ] Replication notes if multiplayer-relevant
- [ ] Mobile material variant if visual asset

**FORBIDDEN PATTERNS (auto-reject by lint, see AFL-0215)**:
- ❌ Granting abilities outside a `ULyraAbilitySet` (e.g., `ASC->GiveAbility(...)` in `BeginPlay` or character constructor) — abilities flow through `ULyraPawnData` → `ULyraHeroComponent::HandleChangeInitState(InitState_DataInitialized)`. See §9.
- ❌ Modifying `Health` (or any persistent attribute) directly from a `UGameplayAbility`. Damage MUST flow through the `Damage` meta-attribute and `UAFLDamageExecCalc`. See §8.
- ❌ Calling `GetPlayerViewPoint` in any code path that may execute on a dedicated server. Camera position is client-authoritative; the server receives it via `FGameplayAbilityTargetDataHandle`. See §7.
- ❌ Trusting any client-supplied trace without running it through `UAFLLagCompensationWorldSubsystem::RewindWorldFor` validation when the trace produces a hit. See §7 Layer 3.

---

---

## 7. SERVER-AUTHORITATIVE HITSCAN VALIDATION DOCTRINE

**This section supersedes any earlier wording about "server validation" of hitscan weapons.** Every hitscan ability in `AFLCombat` (Pulse, Beam, Ricochet, and any future addition) must follow this five-layer validation model. No exceptions.

### 7.1 The Pattern: Client-Builds, Server-Validates, Server-Applies

```
┌──────────────────────────┐         ┌──────────────────────────┐
│         CLIENT           │         │       DEDICATED SERVER   │
│                          │         │                          │
│ 1. ActivateAbility()     │         │ 1. ActivateAbility()     │
│ 2. CommitAbility()       │         │ 2. CommitAbility()       │
│ 3. GetPlayerViewPoint()  │         │ 3. Bind delegate via     │
│    (camera-authoritative)│         │    AbilityTargetData     │
│ 4. LineTrace local        ├────────►│    SetDelegate           │
│ 5. Pack hit into          │  Repli- │ 4. Receive Target Data   │
│    FGameplayAbility       │  cated  │ 5. ValidateTargetData()  │
│    TargetDataHandle       │         │    ─ schema/bounds       │
│ 6. ServerSetReplicated    │         │    ─ distance/angle      │
│    TargetData()           │         │    ─ origin proximity    │
│ 7. Spawn cosmetic FX      │         │    ─ lag-comp re-trace   │
│    (tracer, muzzle)       │         │ 6. ApplyGE_Damage_Pulse  │
│ 8. EndAbility (predicted) │         │    via ExecCalc (§8)     │
│                          │         │ 7. Emit telemetry        │
│                          │         │ 8. EndAbility            │
└──────────────────────────┘         └──────────────────────────┘
```

**Hard rule**: the server NEVER calls `GetPlayerViewPoint`. On a dedicated server it falls back to control rotation and lies about where the client was aiming. Camera position is delivered via TargetData or it does not exist.

### 7.2 The Five Validation Layers

Ordered cheap → expensive. Reject as early as possible.

| Layer | What it does | Cost | Rejects |
|---|---|---|---|
| **1. Schema & Bounds** | Validates `FGameplayAbilityTargetDataHandle` count, finite floats, valid actor, freshness window (≤500ms after activation prediction key opened) | ~Free | Malformed / replay packets |
| **2. Geometry Gates** | Distance ≤ `MaxRange + tolerance`; trace origin within `MaxOriginDeviation` of pawn capsule (250cm); angle pawn-forward → shot-direction ≤ `MaxAngularDeviationDegrees` (100°, accounts for Lyra control rig pitch + body twist) | ~Free | Teleport-shot, shoot-from-elsewhere |
| **3. Lag-Compensated LOS Re-trace** | `UAFLLagCompensationWorldSubsystem` rewinds target hitboxes to `min(serverMeasuredRTT/2 + interp, 200ms)`, re-runs trace from claimed origin, compares hit actor | Moderate | Aimbot through walls, ping-spoofing for free rewind |
| **4. Rate-of-Fire & Resource** | Server-side cooldown GE with `ReplicateToOwnerOnly`; `CommitAbility` rejects when cooldown tag still active. Server attribute set is authoritative, client predicts down on rejection | ~Free | RoF hacks, ammo/heat bypass |
| **5. Telemetry & Adjudication** | Every reject emits `{player, ability, reject_reason, server_tick, claimed_origin, claimed_hit, measured_rtt}` to PlayFab Player Streams. Dashboards on reject rate (>2% = soft flag), aim angular velocity, wall-bang LOS rejects, headshot ratio anomalies | Async | Aimbots that pass kernel-level EAC |

### 7.3 Anti-Ping-Spoofing

Rewind window is **server-measured RTT**, not client-claimed. Hard cap of 200ms. A client claiming 800ms RTT to get extra rewind gets 200ms regardless. Past 200ms they play without lag comp by design — and EAC flags artificially-induced latency separately.

### 7.4 Rewind Implementation Notes

- `UAFLLagCompensationWorldSubsystem` (UWorldSubsystem) owns the rewind transaction lock. Only one ability can rewind world state at a time per server tick.
- `UAFLLagCompensationComponent` (per-pawn, `TG_PostPhysics` tick) writes a snapshot ring buffer: 60 snapshots × 24 bones at 60Hz = ~1 second of history per pawn (~1.5KB).
- `RewindWorldFor(target, dt)` returns an `FAFLLagSnapshot`; `RestoreWorld(snapshot)` undoes the move. Wrap in `ON_SCOPE_EXIT` to guarantee restoration on early return.
- Critical section: snapshots are immutable once written; pawns moved during rewind are restored in inverse order to preserve attached actor relativity.
- Performance budget: 200µs per rewind on dedicated server target hardware. Profile in Sprint 6 before phase exit.

### 7.5 What Goes Where

| Concern | Lives in |
|---|---|
| Client local trace + cosmetic FX | `UAFLAG_Laser_*::ClientPredictAndSend()` |
| TargetData transport | GAS (`ServerSetReplicatedTargetData` / `AbilityTargetDataSetDelegate`) |
| Schema/bounds/geometry validation | `UAFLAG_Laser_*::ValidateTargetData()` |
| Lag rewind | `UAFLLagCompensationWorldSubsystem` + per-pawn component |
| Damage calculation | `UAFLDamageExecCalc` + `UAFLAttributeSet_Combat` (§8) |
| Cooldown enforcement | Server-side cooldown GE (`ReplicateToOwnerOnly`) |
| Telemetry | `IAFLTelemetrySink` interface, stub backend in P1, PlayFab in S13 |

---

## 8. DAMAGE PIPELINE & EXECCALC STANDARD

**This section supersedes any earlier reference to abilities applying instant `-N Health`.** No `UGameplayAbility` in this codebase modifies `Health` directly. All damage flows through a meta-attribute and an execution calculation.

### 8.1 Attribute Layout (`UAFLAttributeSet_Combat`)

| Attribute | Type | Replication | Purpose |
|---|---|---|---|
| `Health` | Persistent | OwnerOnly | Current HP |
| `MaxHealth` | Persistent | OwnerOnly | Cap, mutable via buffs |
| `Shield` | Persistent | OwnerOnly | Stripped before Health |
| `MaxShield` | Persistent | OwnerOnly | Cap |
| `Armor` | Persistent | OwnerOnly | Reciprocal-curve mitigation input |
| `OverkillThreshold` | Persistent | OwnerOnly | Damage above this triggers `Event.Damage.Overkill` (dismemberment hook) |
| `Damage` | **Meta** | None | Reset to 0 every `PostGameplayEffectExecute` after processing |

### 8.2 Pipeline Flow

```
Ability             GE                   ExecCalc                  Persistent attrs
─────────           ─────────            ──────────                ─────────────────
Pulse hits ───►  GE_AFL_Damage_Pulse ─► UAFLDamageExecCalc ─►  Shield delta
                 (SetByCaller magnitudes:                        Health delta
                   .Headshot, .Distance,                         Tag emission
                   .Weakpoint)                                   (Event.Damage.Overkill)
```

### 8.3 `UAFLDamageExecCalc::Execute_Implementation`

1. Capture: `Damage` (source meta), `Armor`, `Shield`, `Health` (target persistent)
2. Read SetByCaller magnitudes from spec: `HeadshotMult`, `DistanceFalloff`, `WeakpointMult`
3. `RawDamage = Damage * HeadshotMult * WeakpointMult * DistanceFalloff`
4. `Mitigation = Armor / (Armor + 100)` *(reciprocal curve, designer-tunable per `DT_AFL_DamageCurves`)*
5. `EffectiveDamage = RawDamage * (1 - Mitigation)`
6. Strip Shield first: `ShieldDelta = -min(Shield, EffectiveDamage)`
7. Remainder hits Health: `HealthDelta = -(EffectiveDamage + ShieldDelta)`
8. Output `FGameplayModifierEvaluatedData` for both `ShieldAttribute` and `HealthAttribute`
9. If `HealthDelta < -OverkillThreshold`: `Spec.AddAssetTag(Event.Damage.Overkill)` — `AFLDismember` system listens for this in `OnGameplayEffectAppliedToTarget`

### 8.4 SetByCaller Magnitudes (set by ability, read by ExecCalc)

| Magnitude tag | Set by | Read by |
|---|---|---|
| `Data.Damage.Headshot` | Ability examines `Hit->BoneName == "head"` post-validation | ExecCalc |
| `Data.Damage.Distance` | Ability computes from `Hit->Distance` and weapon `DA_AFLPulseTuning` falloff curve | ExecCalc |
| `Data.Damage.Weakpoint` | Dismemberment system reads from socketed `WeakpointTag` on detached limb | ExecCalc |

### 8.5 Designer Workflow

All numeric tuning lives in `DT_AFL_WeaponDamage`, `DT_AFL_DamageCurves`, `DT_AFL_HeadshotMultipliers`. Designers edit data assets — engineers do not change source for balance passes. Hot-reload during PIE via the data table observer.

### 8.6 Why This Matters

Without ExecCalc:
- Shield/armor logic gets duplicated across every ability
- Overkill detection gets baked into damage application instead of reactive tags
- Designers can't tune balance without engineers
- Future systems (lifesteal, damage shielding, reflective armor) require touching every ability

With ExecCalc: one place to change, one place to break, one place to test.

---

## 9. LYRA INITIALIZATION CONTRACT

**This section supersedes any earlier statement about ability granting on `BeginPlay`.** Lyra has a deliberate initialization choreography. We respect it. Bypassing it breaks GameFeature plugin hot-loading, breaks Experience swap mid-match (e.g., for chaos mode), and creates merge conflicts every time we pull upstream Lyra.

### 9.1 The Authoritative Flow

```
LyraExperienceDefinition (B_LyraExperience_AFL_Arena)
     │
     ├─ selects ULyraPawnData (DA_AFL_PawnData_Hero_Default)
     │       │
     │       ├─ PawnClass: BP_AFLCharacter_Base (no logic in BP)
     │       ├─ InputConfig: IC_AFL_Default (InputTag → IA_*)
     │       ├─ AbilitySets: [DA_AFL_AbilitySet_Combat_Pulse,
     │       │                DA_AFL_AbilitySet_Movement_Default,
     │       │                DA_AFL_AbilitySet_HUD]
     │       └─ TagRelationshipMapping: DA_AFL_TagRelationships
     │
     └─ activated by GameMode via URL options / matchmaking ticket payload

ULyraPawnExtensionComponent (on pawn) coordinates LoadState chain:
     InitState_Spawned
       → InitState_DataAvailable (PawnData arrived)
         → InitState_DataInitialized  ◄── ABILITY SETS GRANTED HERE
           → InitState_GameplayReady
```

### 9.2 What Goes Where

| Concern | Lives in |
|---|---|
| Pawn class | `BP_AFLCharacter_Base` extends `ALyraCharacter` (no logic — just component composition) |
| Initialization coordination | `ULyraPawnExtensionComponent` (Lyra's, untouched) |
| Ability granting + input binding | `UAFLHeroComponent` extends `ULyraHeroComponent` — overrides `HandleChangeInitState(InitState_DataInitialized)` |
| Ability sets | `ULyraAbilitySet` data assets per archetype/weapon (`DA_AFL_AbilitySet_*`) |
| Input mapping | `ULyraInputConfig` (`IC_AFL_Default`) — InputTag-driven, NOT direct InputAction binding |
| Per-archetype configuration | `ULyraPawnData` (`DA_AFL_PawnData_Hero_*`) |
| Match/mode flavor | `ULyraExperienceDefinition` (`B_LyraExperience_AFL_Arena`, `B_LyraExperience_AFL_Chaos`, etc.) |

### 9.3 The Three Forbidden Patterns

1. **No `BeginPlay` ability granting.** If you find yourself writing `ASC->GiveAbility(...)` outside an `AbilitySet`, stop. The ability belongs in `DA_AFL_AbilitySet_*` and gets granted via `UAFLHeroComponent`.
2. **No direct character class input binding.** Inputs flow Enhanced Input → `ULyraInputConfig` InputTags → ability activation. Never bind an `InputAction` directly to a character method.
3. **No PawnData lookup in random places.** Read it via `ULyraPawnExtensionComponent::GetPawnData<UAFLPawnData>()`. If the component reports "not yet ready," wait for the InitState chain — don't poll, don't `GetWorld()->GetTimerManager()`.

### 9.4 Custom AFL Initialization

When AFL needs to wire something at init time (e.g., spawn HUD widgets, register player with energy economy, register voice channel):

```cpp
// AFLCore/Public/Components/AFLHeroComponent.h
UCLASS()
class AFLCORE_API UAFLHeroComponent : public ULyraHeroComponent
{
    GENERATED_BODY()
protected:
    virtual void HandleChangeInitState(
        UGameFrameworkComponentManager* Manager,
        FGameplayTag CurrentState,
        FGameplayTag DesiredState) override;
};

// AFLCore/Private/Components/AFLHeroComponent.cpp
void UAFLHeroComponent::HandleChangeInitState(
    UGameFrameworkComponentManager* Manager,
    FGameplayTag CurrentState,
    FGameplayTag DesiredState)
{
    Super::HandleChangeInitState(Manager, CurrentState, DesiredState);
    if (DesiredState == LyraGameplayTags::InitState_GameplayReady)
    {
        // AFL-specific gameplay-ready hooks ONLY (HUD, energy registration, etc.)
        // Ability granting already happened in Super at InitState_DataInitialized.
    }
}
```

### 9.5 Experience Swap (chaos mode, lounge transitions)

Because all gameplay lives in plugins, switching from arena to chaos mode is just an Experience swap:
- Arena → Chaos: server triggers experience change → all pawns re-init through PawnData chain → chaos abilities granted → arena abilities removed cleanly. No state corruption.
- This is why `BeginPlay` granting is forbidden: it bypasses the InitState chain and survives Experience swap, leading to ghost abilities.

### 9.6 AFL Dash Movement Contract

**Authoritative for**: Sprint 3 (W5–W6) — AFL-0301, AFL-0301a, AFL-0302, AFL-0303, AFL-0304, AFL-0306, AFL-0309.

#### Purpose

Sprint 3 movement ships as a **dash gameplay contract**, not as a standalone movement AttributeSet.

This section replaces the earlier Sprint 3 assumption that `UAFLAttributeSet_Movement` should be authored first. That approach was deferred because Sprint 3 has no valid consumer requiring persistent movement attributes. The actual Sprint 3 tracker scope is dash-focused and is best implemented through a **GameplayAbility + GameplayEffects + CharacterMovementComponent tag-response** architecture. `UAFLAttributeSet_Movement` is deferred to Sprint 4 (see §9.7), when leg-loss (AFL-0409), overdrive (AFL-0707), or other movement composition systems create a real consumer.

#### Scope decision

**In scope for Sprint 3** — five runtime collaborators:
1. `UAFLGameplayAbility_Dash` (AFL-0302)
2. `AFLGE_DashCooldown` (AFL-0303)
3. `AFLGE_Dash_Active` (AFL-0306 — duration window granting `State.Movement.Dashing` and optional `State.Invulnerable`)
4. `UAFLCharacterMovementComponent` (AFL-0304 — listens to `State.Movement.Dashing`, swaps friction/air-control)
5. `UAFLCameraModifier_DashFOV` (AFL-0309 — local-only FOV pulse)

Covers: dash impulse execution, cooldown enforcement, dash-active state window, friction/air-control tuning during dash, optional i-frame support, local dash camera response.

**Out of scope for Sprint 3** (explicit deferrals):
- `UAFLAttributeSet_Movement` (see §9.7)
- multi-charge dash
- directional dash variants beyond input/facing resolution
- stamina systems
- wall-jump / double-jump
- dash-cancel mid-flight
- permanent movement stat composition
- VFX/audio polish beyond their declared tasks (AFL-0305 VFX, AFL-0308 audio, AFL-0307 QA replication — owned by their discipline tracks)

#### Core architecture

Dash is implemented through a GAS-native contract:

- **GameplayAbility** owns activation, validation, dash launch
- **Cooldown GE** owns cooldown state
- **Dash-Active GE** owns dash-active tags and timed removal (GAS rollback replaces hand-rolled timer + restore logic)
- **CharacterMovementComponent** owns movement-response behavior while dash tags are active
- **Camera modifier** owns local-only FOV response
- **AbilitySet grant path** remains Lyra-canonical through PawnData → AbilitySet (§9.1, §9.4), never `BeginPlay`

This keeps movement authority inside CMC, gameplay state inside GAS, and timed cleanup inside GE lifecycle.

#### Authority model

Dash uses **predicted client activation with server-authoritative validation**.

| Side | Behavior |
|---|---|
| **Client (owner)** | Input via `InputTag.Movement.Dash` → predicts activation → predicts cooldown GE + dash-active GE → computes dash direction → calls `LaunchCharacter` → local CMC reacts to `State.Movement.Dashing` → local camera modifier reacts |
| **Server** | Validates cooldown + blocked-tag rules → applies cooldown GE authoritatively → applies dash-active GE authoritatively → applies authoritative `LaunchCharacter` → replicated movement resolves via standard CMC replication |
| **Remote client** | Receives replicated dash-active GE → receives replicated velocity via CMC → simulated proxy reacts via same `State.Movement.Dashing` tag path → **no remote camera modification** |

#### Gameplay rule: no Sprint 3 Movement AttributeSet

Sprint 3 does **not** introduce `UAFLAttributeSet_Movement`. The earlier proposed AttributeSet had no Sprint 3 consumer; introducing it here would violate the project's scope discipline (§3.1: "Ship vertical slices, not horizontal scaffolding"). See §9.7 for the deferred plan.

#### Dash ability contract

**Class**: `UAFLGameplayAbility_Dash` extending `ULyraGameplayAbility`.

**Responsibilities**: validate activation; commit cooldown; apply dash-active GE; compute dash direction; call `LaunchCharacter`; fire activation cue(s); end cleanly after activation.

**Network policy**: `InstancedPerActor`, `LocalPredicted`, server-authoritative validation and correction.

**Direction resolution** (in order):
1. Current movement input vector (`GetLastMovementInputVector`)
2. Actor forward vector fallback (`GetActorForwardVector`)

Direction is flattened to the XY plane for dash launch. Vertical velocity is preserved separately (gravity continues to operate; airborne dashes maintain Z velocity).

**Movement primitive**: dash uses `LaunchCharacter`, **not** `AddImpulse` and **not** direct velocity assignment. Required because `LaunchCharacter`:
- participates correctly in Lyra/CMC network movement
- preserves authoritative movement reconciliation
- avoids bypassing the normal character movement replication path

#### Ground / air rule (LOCKED)

Sprint 3 dash is **air-dash allowed**.

- Dash may activate while airborne.
- Dash does not introduce a second charge system.
- Cooldown remains the limiter.
- `LaunchCharacter` preserves current Z velocity because dash is planar and does not overwrite vertical motion.

This is locked for Sprint 3 because it supports mobility expression without introducing extra state complexity.

#### Gameplay Effects

**`AFLGE_DashCooldown`**

| Property | Value |
|---|---|
| Policy | `Duration` |
| Duration | `FScalableFloat 3.0s` |
| Granted tags | `Cooldown.Movement.Dash` |
| Modifiers | (none — tag-only) |

Sole source of dash cooldown state. The GA does not own cooldown through timers.

**`AFLGE_Dash_Active`**

| Property | Value |
|---|---|
| Policy | `Duration` |
| Duration | `FScalableFloat 0.12s` |
| Granted tags | `State.Movement.Dashing` (always); `State.Invulnerable` (optional, configurable) |
| Modifiers | (none in Sprint 3 base — friction/air-control swap is CMC-side, not GE-modifier-side) |
| Stacking | None (overlapping dashes prevented by cooldown) |

**Invulnerability rule (LOCKED)**: i-frame support is **configurable**, not assumed as the only valid Sprint 3 mode.
- Base Sprint 3 contract requires `State.Movement.Dashing`.
- Optional i-frame behavior may be enabled through the GE/tag configuration without rewriting the GA.
- Damage GE `ApplicationTagRequirements.IgnoreTags` updates to honor `State.Invulnerable` **only if** i-frame is formally enabled in Sprint 3. If i-frame stays optional and not yet enabled, the tag is reserved but the damage GE behavior is not forced until that toggle is chosen.

This keeps PvP balance tuning flexible.

#### Character movement contract

**Class**: `UAFLCharacterMovementComponent` extending `UCharacterMovementComponent`.

**Responsibilities**: bind to dash-active gameplay tag changes; enter dash-movement state on `State.Movement.Dashing` add; restore baseline movement state on `State.Movement.Dashing` removal.

**Runtime behavior**:
- On dash start: cache current `GroundFriction` and `AirControl` values, then reduce friction and raise air-control.
- On dash end: restore the cached values.

**Sprint 3 tuning targets** (initial):
- Dash friction: `2.0`
- Dash air-control: `0.6`

**Critical rule**: values are cached at **dash entry**, not at component construction. This guarantees restore returns to the real pre-dash state, including future systems such as leg-loss penalties, temporary buffs, or other movement modifiers that may exist when dash begins.

#### Camera contract

**Class**: `UAFLCameraModifier_DashFOV`.

**Responsibilities**: local-only dash FOV pulse; react to `State.Movement.Dashing`; never replicate to non-owning viewers.

**Sprint 3 behavior**: FOV pulse during dash (target: 110→95→back over 0.3s per AFL-0309); local player only; no remote spectator/other-player camera changes.

The camera response is cosmetic only and does not represent gameplay state.

#### Grant path contract

Dash is granted through the Lyra-authoritative initialization path (§9.1, §9.4):

```
PawnData → AbilitySet → Ability grant
```

Sprint 3 dash must **never** be granted through `BeginPlay`, ad-hoc `GiveAbility` calls, or manual bootstrap outside the Lyra experience/pawn-data path. This is enforced by the §9.3 forbidden-patterns rule and CI lint (AFL-0215).

**Required asset pattern**: `DA_AFL_AbilitySet_Movement_Dash`
- Grants `UAFLGameplayAbility_Dash`
- Binds `InputTag.Movement.Dash`
- Does **not** grant a movement AttributeSet in Sprint 3

#### Activation blocked-tag contract

Sprint 3 dash must be blocked when any of the following tags are present:
- `State.Match.Warmup`
- `State.Match.Ended`
- `State.Extracting`

Additionally: if a hard-disable control-state tag (stun/root/knockdown equivalent) already exists in the project schema, dash must respect it. No new hard-disable tag is introduced here unless already present elsewhere in the schema.

#### Tag schema (AFL-0301a — schema-final before runtime)

Sprint 3 dash requires the following tags. The subscope convention follows the shipped 51-tag bundle in `Plugins/GameFeatures/AFLCore/Config/Tags/AFLCoreTags.ini` (AFL-0102, commit `b171bd17`).

**Already present in the shipped 51-tag bundle (no new file edits required):**
- `InputTag.Movement.Dash`
- `Cooldown.Movement.Dash`
- `State.Movement.Dashing`
- `Ability.Movement.Dash` — used as the GA's activation-identity tag and (via `ActivationOwnedTags`) as the implicit "this ability is active" tag. No separate `Ability.Dash.Active` tag is needed; the Lyra GA pattern uses the activation-identity tag itself.

**New tags added by AFL-0301a (3 tags) — standalone commit before any GA/GE/CMC code, same pattern as AFL-0608 / AFL-0908:**
- `State.Invulnerable` — cross-cutting, no subscope (follows `State.Death` precedent). Reserved/available; gameplay-effect impact gated on the i-frame toggle. Also reserved for spawn protection (AFL-0506) and potential future ability-cast invulnerability.
- `Cue.Movement.Dash.Activated` — one-shot activation cue.
- `Cue.Movement.Dash.Trail` — looping trail cue.

A schema-final task must land before dash runtime implementation begins so the dash code does not introduce tag drift later.

#### Acceptance matrix (T1–T8)

Sprint 3 dash closes only when the contract is proven through the following acceptance gates:

| Gate | Validates |
|---|---|
| **T1** | Fresh dash activation: dash launches; `State.Movement.Dashing` appears for active window; `Cooldown.Movement.Dash` appears for cooldown window; friction/air-control swap during dash and restore after |
| **T2** | Dash blocked during active cooldown: second dash attempt fails cleanly |
| **T3** | Cooldown boundary: blocked before expiry; succeeds after expiry |
| **T4** | If i-frame mode is enabled: damage during active dash window is ignored |
| **T5** | Multiplayer visibility/replication: replicated dash position and behavior remain consistent across clients (AFL-0307 owns 4-client × 50-trial validation) |
| **T6** | Warmup blocked: dash cannot activate during `State.Match.Warmup` |
| **T7** | Extraction blocked: dash cannot activate while `State.Extracting` |
| **T8** | Dash restoration integrity: post-dash movement state restores to actual pre-dash values, not hardcoded spawn defaults |

#### Testing rule

Sprint 3 must ship with a sibling test module (`AFLMovementTests`, mirroring Sprint 1.5's `AFLCombatTests` precedent).

Because Sprint 3 is **not** an AttributeSet sprint, the required automated test surface is **dash contract validation**, not movement AttributeSet validation. The Sprint 3 automated suite proves:
- dash activation rules
- cooldown tag application/removal
- dash-active tag application/removal
- blocked activation states
- direction resolution behavior
- optional invulnerability behavior (when enabled)
- clean authority/correction behavior where applicable

This preserves the project rule (established Sprint 1.5) that every gameplay plugin with contract logic ships with automated contract tests from day one.

### 9.7 `UAFLAttributeSet_Movement` (Deferred)

**Status**: deferred from Sprint 3 to Sprint 4.

**Reason**: Sprint 3 has no valid consumer requiring persistent movement attribute composition. Sprint 3 dash mechanics are fully expressible through GA + GE + CMC tag-response (§9.6) and an AttributeSet would be dead architecture during Sprint 3.

**Introduction trigger**: `UAFLAttributeSet_Movement` will be introduced only when movement composition has real consumers requiring persistent attribute state. Known candidates:
- **AFL-0409** (Sprint 4) — leg-loss penalty: speed ×0.5 + crawl. First real `MoveSpeedMultiplier` consumer.
- **AFL-0707** (Sprint 7) — Overdrive buff: speed +15%. Stacks multiplicatively with leg-loss via `MoveSpeedMultiplier`.

**Reference material preserved**: the earlier `UAFL_ATTRIBUTESET_MOVEMENT_SPEC.md` work is preserved as Sprint 4 reference, not discarded. When AFL-0409 implementation begins, that spec becomes the starting point for the AttributeSet schema (subject to fresh review against the then-current consumer set).

**Sprint 3 does not authorize implementation of `UAFLAttributeSet_Movement`.**

---

## 10. RISK REGISTER

| Risk | Severity | Mitigation | Owner |
|---|---|---|---|
| Game feel insufficient at Phase 1 GO/NO-GO | 🔴 Critical | Insert Sprint 6.5 polish; do not advance to P2 until ≥80% playtest sentiment | Studio Lead |
| **Backend triangle (PlayFab + Lambda + GameLift) integration delays Phase 3** | 🔴 Critical | **Sprint 11 tentpole AFL-1100 ships standalone microservice with synthetic test harness BEFORE any client integration. EOS Matchmaking removed from scope; EOS reduced to voice/friends/EAC** | Online Lead |
| **R-09: Hitscan validation tuning false-positive rate** | 🔴 Critical | Sprint 6 stress test: 30/80/200ms RTT cohorts × 1000 shots/min synthetic clients. Reject rate must be <1% on legitimate clients before phase exit. Telemetry from S2 stub feeds the tuning data | Combat Lead |
| Console cert failure | 🟠 High | Begin TRC/XR audit in S15, monthly compliance review | Producer |
| Anti-cheat bypass discovered post-launch | 🟠 High | Red team week S14; bug bounty program at launch; telemetry adjudication (§7.5) live from Phase 3 | Online Lead |
| Mobile performance below 60fps target | 🟠 High | Mobile material variants enforced from S2; profiling Sprint dedicated | Tech Art |
| Lag compensation rewind cost > 200µs/shot on dedicated server | 🟠 High | Profile in Sprint 6; if budget exceeded, reduce snapshot bone count from 24 → 8 (capsule + head + 6 limb roots) | Combat Lead |
| Asset pipeline blocking content team in P4 | 🟡 Medium | Pipeline tooling shipped before P4; content team bottleneck audit S14 | Pipeline Eng |
| Lyra upstream merge conflict | 🟡 Medium | All work in plugins, no Lyra base class edits, code review enforces; quarterly merge cadence | Eng Lead |
| Team burnout in P4 content crunch | 🟠 High | Hard 40-hour cap; carryover acceptance over crunch | Studio Lead |

---

## 11. TEAM ROLES (recommended minimum)

| Role | Count | Responsibilities |
|---|---|---|
| Studio Lead | 1 | Vision, GO/NO-GO calls, external partners |
| Engineering Lead | 1 | Architecture, code review, AFLCore/Combat/Online plugins |
| Engineers (gameplay) | 2–3 | Abilities, components, game modes |
| Engineer (online/backend) | 1 | EOS, PlayFab, dedicated servers |
| Engineer (DevOps) | 1 | CI/CD, build farm, deployment |
| Tech Artist | 1 | Materials, VFX optimization, mobile variants |
| VFX Artist | 1 | Niagara, beam systems, dismemberment effects |
| Level Designer / Environment Artist | 1–2 | Maps, modular kits |
| Animator | 1 | Robot rigs, montages, dismemberment anims |
| Audio Designer | 1 | MetaSounds, weapon SFX, head VO bank |
| QA Lead | 1 | Test plans, sprint sign-off, regression matrices |
| QA Testers | 2 | Daily smoke tests, bug bash facilitation |
| Producer | 1 | Sprint coordination, risk tracking, this document upkeep |

**Total: 14–17 people** for a 12-month build to launch quality.

---

## 12. DEFINITION OF DONE (HIERARCHY)

```
TASK DONE        → Acceptance criteria checked + PR merged + CI green
SPRINT DONE      → All committed tasks done OR explicitly carried over
                   + Sprint Demo passed + Retro held
PHASE DONE       → All sprint goals met + Phase End Gate criteria pass
                   + Studio Lead sign-off
LAUNCH DONE      → Phase 5 End Gate + cert pass + 10K concurrent stress test
                   + Day 0 hotfix readiness confirmed
```

---

## 13. APPENDIX — REFERENCE MATERIALS

### 13.1 Pulse Carbine — Canonical Reference Implementation (v2, server-authoritative)

> **REPLACES** the v1 appendix that called `GetPlayerViewPoint` on the server. v1 was broken: `GetPlayerViewPoint` on a dedicated server falls back to control rotation and produces a server trace from the wrong origin at the wrong angle. v2 uses the GAS TargetData pattern. Every hitscan ability in `AFLCombat` follows this template.

**`AFLCombat/Public/Abilities/AFLAG_Laser_Pulse.h`**
```cpp
#pragma once

#include "Abilities/LyraGameplayAbility.h"
#include "AFLAG_Laser_Pulse.generated.h"

UCLASS()
class AFLCOMBAT_API UAFLAG_Laser_Pulse : public ULyraGameplayAbility
{
    GENERATED_BODY()

public:
    UAFLAG_Laser_Pulse();

protected:
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    /** Bound on server to the delegate that fires when client TargetData replicates in. */
    void OnTargetDataReplicated(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);

    /** Local-predicted client trace; produces TargetData and ships it to server. */
    void ClientPredictAndSend();

    /** Authoritative validation. Returns true only if every gate (§7) passes. */
    bool ValidateTargetData(const FGameplayAbilityTargetDataHandle& Data) const;

    /** ExecCalc-driven damage GE. NEVER modifies Health directly — sets Damage meta-attribute. (§8) */
    UPROPERTY(EditDefaultsOnly, Category = "AFL|Pulse")
    TSubclassOf<UGameplayEffect> DamageGE;

    UPROPERTY(EditDefaultsOnly, Category = "AFL|Pulse")
    float MaxTraceDistance = 9000.f;

    UPROPERTY(EditDefaultsOnly, Category = "AFL|Validation")
    float MaxAngularDeviationDegrees = 100.f;

    UPROPERTY(EditDefaultsOnly, Category = "AFL|Validation")
    float MaxOriginDeviation = 250.f;

    UPROPERTY(EditDefaultsOnly, Category = "AFL|Validation")
    float MaxRewindSeconds = 0.2f;

    UPROPERTY(EditDefaultsOnly, Category = "AFL|Validation")
    bool bServerVerifyLineOfSight = true;
};
```

**`AFLCombat/Private/Abilities/AFLAG_Laser_Pulse.cpp`**
```cpp
#include "Abilities/AFLAG_Laser_Pulse.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Lag/AFLLagCompensationComponent.h"
#include "Lag/AFLLagCompensationWorldSubsystem.h"
#include "AFLCombatLog.h"

UAFLAG_Laser_Pulse::UAFLAG_Laser_Pulse()
{
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    NetSecurityPolicy  = EGameplayAbilityNetSecurityPolicy::ServerOnlyExecution;
    ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateNo;
}

void UAFLAG_Laser_Pulse::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }

    if (ActorInfo->IsLocallyControlled())
    {
        ClientPredictAndSend();
    }
    else
    {
        // SERVER PATH: bind delegate; client TargetData replication will fire it.
        const FPredictionKey Key = ActivationInfo.GetActivationPredictionKey();
        ASC->AbilityTargetDataSetDelegate(Handle, Key)
           .AddUObject(this, &UAFLAG_Laser_Pulse::OnTargetDataReplicated);

        // If TargetData arrived before we bound, fire it now.
        ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, Key);
    }
}

void UAFLAG_Laser_Pulse::ClientPredictAndSend()
{
    const FGameplayAbilityActorInfo* AI = GetCurrentActorInfo();
    APlayerController* PC = AI ? AI->PlayerController.Get() : nullptr;
    UAbilitySystemComponent* ASC = AI ? AI->AbilitySystemComponent.Get() : nullptr;
    AActor* Avatar = AI ? AI->AvatarActor.Get() : nullptr;
    if (!PC || !ASC || !Avatar) { return; }

    // SAFE: GetPlayerViewPoint on client returns the actual camera transform.
    // NEVER call this on the dedicated server.
    FVector CamLoc; FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    const FVector End = CamLoc + (CamRot.Vector() * MaxTraceDistance);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(AFLPulseTrace), true, Avatar);
    Params.bReturnPhysicalMaterial = true;

    FHitResult Hit;
    GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_GameTraceChannel1, Params);

    auto* Payload = new FGameplayAbilityTargetData_SingleTargetHit(Hit);
    FGameplayAbilityTargetDataHandle DataHandle;
    DataHandle.Add(Payload);

    // Open prediction window so client visuals/audio fire NOW for responsiveness.
    // Damage and authoritative hit registration are server-only.
    FScopedPredictionWindow Scoped(ASC, IsPredictingClient());
    ASC->ServerSetReplicatedTargetData(
        CurrentSpecHandle,
        CurrentActivationInfo.GetActivationPredictionKey(),
        DataHandle,
        FGameplayTag(),
        ASC->ScopedPredictionKey
    );

    // Cosmetic-only client effects (tracer, muzzle flash, audio) belong here or in
    // a Niagara/montage ability task. Do NOT apply damage on the client.
    EndAbility(CurrentSpecHandle, AI, CurrentActivationInfo, true, false);
}

void UAFLAG_Laser_Pulse::OnTargetDataReplicated(
    const FGameplayAbilityTargetDataHandle& Data,
    FGameplayTag /*Tag*/)
{
    const FGameplayAbilityActorInfo* AI = GetCurrentActorInfo();
    UAbilitySystemComponent* ASC = AI ? AI->AbilitySystemComponent.Get() : nullptr;
    if (!ASC || !HasAuthority(&CurrentActivationInfo)) { return; }

    if (!ValidateTargetData(Data))
    {
        UE_LOG(LogAFLCombat, Warning,
               TEXT("[Pulse] Validation REJECTED for %s — telemetry emitted"),
               *GetNameSafe(AI->AvatarActor.Get()));
        // AFL-0213 telemetry: emit reject event. Stub in P1; PlayFab Player Streams in S13.
        ASC->ConsumeClientReplicatedTargetData(
            CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
        EndAbility(CurrentSpecHandle, AI, CurrentActivationInfo, true, true);
        return;
    }

    for (int32 i = 0; i < Data.Num(); ++i)
    {
        const FHitResult* Hit = Data.Get(i)->GetHitResult();
        if (!Hit || !Hit->GetActor()) { continue; }

        UAbilitySystemComponent* TargetASC =
            UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Hit->GetActor());
        if (!TargetASC) { continue; }

        FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
        Ctx.AddHitResult(*Hit);
        Ctx.AddInstigator(AI->OwnerActor.Get(), AI->AvatarActor.Get());

        const FGameplayEffectSpecHandle Spec =
            ASC->MakeOutgoingSpec(DamageGE, GetAbilityLevel(), Ctx);

        if (Spec.IsValid())
        {
            // Set headshot/distance/weakpoint magnitudes for ExecCalc to read. (§8)
            const bool bHeadshot = Hit->BoneName == FName(TEXT("head"));
            Spec.Data->SetSetByCallerMagnitude(
                FGameplayTag::RequestGameplayTag(TEXT("Data.Damage.Headshot")),
                bHeadshot ? 2.0f : 1.0f);

            const float DistanceFalloff = FMath::GetMappedRangeValueClamped(
                FVector2D(2000.f, MaxTraceDistance), FVector2D(1.0f, 0.6f), Hit->Distance);
            Spec.Data->SetSetByCallerMagnitude(
                FGameplayTag::RequestGameplayTag(TEXT("Data.Damage.Distance")),
                DistanceFalloff);

            // ExecCalc reads Damage meta-attribute, applies armor/shield mitigation,
            // writes final delta to Health. We never touch Health directly.
            ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
        }
    }

    ASC->ConsumeClientReplicatedTargetData(
        CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
    EndAbility(CurrentSpecHandle, AI, CurrentActivationInfo, true, false);
}

bool UAFLAG_Laser_Pulse::ValidateTargetData(const FGameplayAbilityTargetDataHandle& Data) const
{
    const FGameplayAbilityActorInfo* AI = GetCurrentActorInfo();
    AActor* Avatar = AI ? AI->AvatarActor.Get() : nullptr;
    APlayerController* PC = AI ? AI->PlayerController.Get() : nullptr;
    if (!Avatar || !PC) { return false; }

    UAFLLagCompensationWorldSubsystem* LagSys = nullptr;
    if (UWorld* W = GetWorld())
    {
        LagSys = W->GetSubsystem<UAFLLagCompensationWorldSubsystem>();
    }

    for (int32 i = 0; i < Data.Num(); ++i)
    {
        const FHitResult* Hit = Data.Get(i)->GetHitResult();
        if (!Hit) { return false; }

        // LAYER 1: schema/bounds
        if (!FMath::IsFinite(Hit->Location.X) ||
            !FMath::IsFinite(Hit->Location.Y) ||
            !FMath::IsFinite(Hit->Location.Z)) { return false; }

        // LAYER 2: distance gate
        const float Dist = FVector::Dist(Avatar->GetActorLocation(), Hit->Location);
        if (Dist > MaxTraceDistance + 200.f) { return false; }

        // LAYER 2: trace origin gate
        const FVector ClaimedStart = Hit->TraceStart;
        if (FVector::Dist(ClaimedStart, Avatar->GetActorLocation()) > MaxOriginDeviation)
        {
            return false;
        }

        // LAYER 2: angular gate
        const FVector ShotDir = (Hit->Location - ClaimedStart).GetSafeNormal();
        const float AngleDeg = FMath::RadiansToDegrees(
            FMath::Acos(FVector::DotProduct(Avatar->GetActorForwardVector(), ShotDir)));
        if (AngleDeg > MaxAngularDeviationDegrees) { return false; }

        // LAYER 3: lag-compensated LOS verify
        if (bServerVerifyLineOfSight && LagSys && Hit->GetActor())
        {
            // Use SERVER-MEASURED RTT, not client-supplied. Hard cap at MaxRewindSeconds.
            const float ExactPingMs = PC->PlayerState ? PC->PlayerState->ExactPing : 0.f;
            const float ClampedRTT = FMath::Min(MaxRewindSeconds, (ExactPingMs * 0.001f) * 0.5f);

            FAFLLagSnapshot Snap = LagSys->RewindWorldFor(Hit->GetActor(), ClampedRTT);
            ON_SCOPE_EXIT { LagSys->RestoreWorld(Snap); };

            FHitResult Verify;
            FCollisionQueryParams P(SCENE_QUERY_STAT(AFLPulseVerify), true, Avatar);
            GetWorld()->LineTraceSingleByChannel(
                Verify, ClaimedStart, Hit->Location, ECC_GameTraceChannel1, P);

            if (Verify.GetActor() != Hit->GetActor()) { return false; }
        }
    }
    return true;
}
```

**Why this version is canonical:**
- Camera is read on the client where it's authoritative; never on the server where it isn't.
- TargetData traverses the GAS replication path; the server never trusts client state without validation.
- All five validation layers (§7) execute before any GE applies.
- Damage routes through ExecCalc (§8); zero direct attribute writes.
- Lag compensation is server-measured-RTT capped, immune to ping spoofing.

Use this as the template for `UAFLAG_Laser_Beam`, `UAFLAG_Laser_Ricochet`, `UAFLAG_Laser_Nova`, and `UAFLAG_Laser_Singularity`.

### 13.2 GameplayTag Skeleton (paste into `Config/Tags/AFLTags.ini`)

```ini
[/Script/GameplayTags.GameplayTagsList]
+GameplayTagList=(Tag="State.Overdrive",DevComment="Player in overdrive buff state")
+GameplayTagList=(Tag="State.Extracting",DevComment="Channeling extraction")
+GameplayTagList=(Tag="State.Overheated",DevComment="Weapon overheated, blocks fire")
+GameplayTagList=(Tag="State.Dashing",DevComment="During dash i-frame")
+GameplayTagList=(Tag="State.Invulnerable",DevComment="Spawn protection or scripted")

+GameplayTagList=(Tag="Ability.Laser.Pulse")
+GameplayTagList=(Tag="Ability.Laser.Beam")
+GameplayTagList=(Tag="Ability.Laser.Ricochet")
+GameplayTagList=(Tag="Ability.Laser.Nova")
+GameplayTagList=(Tag="Ability.Laser.Singularity")
+GameplayTagList=(Tag="Ability.Movement.Dash")
+GameplayTagList=(Tag="Ability.Movement.Blink")
+GameplayTagList=(Tag="Ability.Objective.Extract")

+GameplayTagList=(Tag="Cooldown.Dash")
+GameplayTagList=(Tag="Cooldown.Blink")

+GameplayTagList=(Tag="Event.Hit")
+GameplayTagList=(Tag="Event.Headshot")
+GameplayTagList=(Tag="Event.Elimination")
+GameplayTagList=(Tag="Event.EnergyPickup")
+GameplayTagList=(Tag="Event.Extraction.Start")
+GameplayTagList=(Tag="Event.Extraction.Complete")
+GameplayTagList=(Tag="Event.Extraction.Failed")
+GameplayTagList=(Tag="Event.Damage.Overkill",DevComment="Damage exceeded OverkillThreshold; dismember system listens")

+GameplayTagList=(Tag="Data.Damage.Headshot",DevComment="SetByCaller: headshot multiplier")
+GameplayTagList=(Tag="Data.Damage.Distance",DevComment="SetByCaller: distance falloff multiplier")
+GameplayTagList=(Tag="Data.Damage.Weakpoint",DevComment="SetByCaller: weakpoint multiplier")

+GameplayTagList=(Tag="Telemetry.Reject.Schema")
+GameplayTagList=(Tag="Telemetry.Reject.Distance")
+GameplayTagList=(Tag="Telemetry.Reject.Origin")
+GameplayTagList=(Tag="Telemetry.Reject.Angle")
+GameplayTagList=(Tag="Telemetry.Reject.LOS")

+GameplayTagList=(Tag="InputTag.Weapon.Fire")
+GameplayTagList=(Tag="InputTag.Weapon.Secondary")
+GameplayTagList=(Tag="InputTag.Movement.Dash")
+GameplayTagList=(Tag="InputTag.Movement.Jump")
```

### 13.3 GitHub Actions skeleton (`.github/workflows/pr-validate.yml`)

```yaml
name: PR Validate
on:
  pull_request:
    branches: [develop, main]

jobs:
  validate-win64:
    runs-on: [self-hosted, windows, ue5]
    steps:
      - uses: actions/checkout@v4
        with:
          lfs: true
      - name: Asset naming validation
        run: python scripts/validate_assets.py
      - name: Compile Win64 Development
        shell: cmd
        run: |
          "%UE5_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
            -project="%CD%\AFL.uproject" ^
            -noP4 -platform=Win64 -clientconfig=Development ^
            -build -cook -nostage
      - name: Run unit tests
        shell: cmd
        run: |
          "%UE5_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
            "%CD%\AFL.uproject" -ExecCmds="Automation RunTests AFL;Quit" ^
            -unattended -nopause -testexit="Automation Test Queue Empty"
```

### 13.4 Lag Compensation Subsystem (header sketch)

```cpp
// AFLCombat/Public/Lag/AFLLagCompensationWorldSubsystem.h
#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "AFLLagCompensationWorldSubsystem.generated.h"

USTRUCT()
struct FAFLLagSnapshot
{
    GENERATED_BODY()
    TWeakObjectPtr<AActor> Target;
    TArray<FTransform> RestoreBoneTransforms;
    TArray<FName> RestoreBoneNames;
    bool bValid = false;
};

UCLASS()
class AFLCOMBAT_API UAFLLagCompensationWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    /** Rewinds Target's hitboxes to (now - DeltaSeconds), returns snapshot for restore. */
    FAFLLagSnapshot RewindWorldFor(AActor* Target, float DeltaSeconds);

    /** Restores world state from a previously taken snapshot. */
    void RestoreWorld(const FAFLLagSnapshot& Snapshot);

    /** Registered per-pawn at PostInitializeComponents. */
    void RegisterComponent(class UAFLLagCompensationComponent* Comp);
    void UnregisterComponent(class UAFLLagCompensationComponent* Comp);

private:
    UPROPERTY()
    TArray<TWeakObjectPtr<UAFLLagCompensationComponent>> Components;

    FCriticalSection RewindLock;  // one rewind transaction at a time
};

// AFLCombat/Public/Lag/AFLLagCompensationComponent.h
UCLASS(ClassGroup=(AFL), meta=(BlueprintSpawnableComponent))
class AFLCOMBAT_API UAFLLagCompensationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UAFLLagCompensationComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    /** Returns interpolated bone transforms for (now - DeltaSeconds). */
    void SampleAtTime(float DeltaSeconds, TArray<FTransform>& OutTransforms,
                      TArray<FName>& OutBoneNames) const;

private:
    struct FBoneSnapshot { FName Bone; FTransform XForm; };
    struct FFrameSnapshot { double ServerTime; TArray<FBoneSnapshot> Bones; };

    TCircularBuffer<FFrameSnapshot> Ring{ 64 };  // ~1.07s @ 60Hz
    int32 Head = 0;

    UPROPERTY(EditDefaultsOnly, Category="AFL|LagComp")
    TArray<FName> TrackedBones;  // 24 bones default; tunable to 8 if budget tight

    UPROPERTY(EditDefaultsOnly, Category="AFL|LagComp")
    float HistorySeconds = 1.0f;
};
```

Implementation lives in `AFLCombat/Private/Lag/`. Tick group = `TG_PostPhysics`. Ring buffer is per-component, not shared. World subsystem holds the rewind transaction lock and orchestrates multi-pawn rewind for AOE abilities (Nova Burst hits multiple targets — all rewound to same `t = now - dt` for consistency).

### 13.5 Damage ExecCalc Skeleton

```cpp
// AFLCombat/Public/Calc/AFLDamageExecCalc.h
#pragma once
#include "GameplayEffectExecutionCalculation.h"
#include "AFLDamageExecCalc.generated.h"

UCLASS()
class AFLCOMBAT_API UAFLDamageExecCalc : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()
public:
    UAFLDamageExecCalc();
    virtual void Execute_Implementation(
        const FGameplayEffectCustomExecutionParameters& Params,
        FGameplayEffectCustomExecutionOutput& Output) const override;
};

// AFLCombat/Private/Calc/AFLDamageExecCalc.cpp
struct FAFLDamageStatics
{
    DECLARE_ATTRIBUTE_CAPTUREDEF(Damage);
    DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
    DECLARE_ATTRIBUTE_CAPTUREDEF(Shield);
    DECLARE_ATTRIBUTE_CAPTUREDEF(Health);
    DECLARE_ATTRIBUTE_CAPTUREDEF(OverkillThreshold);

    FAFLDamageStatics()
    {
        DEFINE_ATTRIBUTE_CAPTUREDEF(UAFLAttributeSet_Combat, Damage,            Source, true);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UAFLAttributeSet_Combat, Armor,             Target, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UAFLAttributeSet_Combat, Shield,            Target, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UAFLAttributeSet_Combat, Health,            Target, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UAFLAttributeSet_Combat, OverkillThreshold, Target, false);
    }
};
static const FAFLDamageStatics& DamageStatics() { static FAFLDamageStatics S; return S; }

UAFLDamageExecCalc::UAFLDamageExecCalc()
{
    RelevantAttributesToCapture.Add(DamageStatics().DamageDef);
    RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
    RelevantAttributesToCapture.Add(DamageStatics().ShieldDef);
    RelevantAttributesToCapture.Add(DamageStatics().HealthDef);
    RelevantAttributesToCapture.Add(DamageStatics().OverkillThresholdDef);
}

void UAFLDamageExecCalc::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& Params,
    FGameplayEffectCustomExecutionOutput& Output) const
{
    const FGameplayEffectSpec& Spec = Params.GetOwningSpec();
    FAggregatorEvaluateParameters EvalParams;
    EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

    float RawDamage = 0.f, Armor = 0.f, Shield = 0.f, Health = 0.f, OverkillT = 0.f;
    Params.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().DamageDef,            EvalParams, RawDamage);
    Params.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorDef,             EvalParams, Armor);
    Params.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ShieldDef,            EvalParams, Shield);
    Params.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().HealthDef,            EvalParams, Health);
    Params.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().OverkillThresholdDef, EvalParams, OverkillT);

    const float Headshot = Spec.GetSetByCallerMagnitude(
        FGameplayTag::RequestGameplayTag(TEXT("Data.Damage.Headshot")), false, 1.f);
    const float Distance = Spec.GetSetByCallerMagnitude(
        FGameplayTag::RequestGameplayTag(TEXT("Data.Damage.Distance")), false, 1.f);
    const float Weakpoint = Spec.GetSetByCallerMagnitude(
        FGameplayTag::RequestGameplayTag(TEXT("Data.Damage.Weakpoint")), false, 1.f);

    const float Modified = RawDamage * Headshot * Distance * Weakpoint;
    const float Mitigation = Armor / (Armor + 100.f);
    const float Effective = Modified * (1.f - Mitigation);

    const float ShieldDelta = -FMath::Min(Shield, Effective);
    const float Remainder   = Effective + ShieldDelta;  // ShieldDelta is negative
    const float HealthDelta = -Remainder;

    if (ShieldDelta < 0.f)
    {
        Output.AddOutputModifier(FGameplayModifierEvaluatedData(
            DamageStatics().ShieldProperty, EGameplayModOp::Additive, ShieldDelta));
    }
    if (HealthDelta < 0.f)
    {
        Output.AddOutputModifier(FGameplayModifierEvaluatedData(
            DamageStatics().HealthProperty, EGameplayModOp::Additive, HealthDelta));
    }

    if (HealthDelta < -OverkillT && OverkillT > 0.f)
    {
        // Tag emission consumed by AFLDismember subsystem in OnGameplayEffectAppliedToTarget.
        // Note: AddDynamicAssetTag is the surfaced API on the spec at this point.
        const_cast<FGameplayEffectSpec&>(Spec).AddDynamicAssetTag(
            FGameplayTag::RequestGameplayTag(TEXT("Event.Damage.Overkill")));
    }
}
```

### 13.6 PawnData / HeroComponent / AbilitySet Wiring (Lyra-correct)

```cpp
// AFLCore/Public/Components/AFLHeroComponent.h
#pragma once
#include "Character/LyraHeroComponent.h"
#include "AFLHeroComponent.generated.h"

UCLASS(ClassGroup=(AFL), meta=(BlueprintSpawnableComponent))
class AFLCORE_API UAFLHeroComponent : public ULyraHeroComponent
{
    GENERATED_BODY()
protected:
    virtual void HandleChangeInitState(
        UGameFrameworkComponentManager* Manager,
        FGameplayTag CurrentState,
        FGameplayTag DesiredState) override;
};

// AFLCore/Private/Components/AFLHeroComponent.cpp
#include "Components/AFLHeroComponent.h"
#include "LyraGameplayTags.h"

void UAFLHeroComponent::HandleChangeInitState(
    UGameFrameworkComponentManager* Manager,
    FGameplayTag CurrentState,
    FGameplayTag DesiredState)
{
    // Super grants ability sets at InitState_DataInitialized — DO NOT duplicate that here.
    Super::HandleChangeInitState(Manager, CurrentState, DesiredState);

    if (DesiredState == LyraGameplayTags::InitState_GameplayReady)
    {
        // AFL-specific gameplay-ready hooks ONLY:
        //   - register pawn with energy economy subsystem
        //   - bind to extraction beacon notifications
        //   - spawn HUD widgets owned by this hero
        //   - register voice channel
        // NEVER call ASC->GiveAbility here. Abilities live in DA_AFL_AbilitySet_*.
    }
}
```

**Asset wiring** (configured in editor, NOT in code):

```
Content/AFL/Pawns/
  BP_AFLCharacter_Base                  (extends ALyraCharacter, UAFLHeroComponent attached)

Content/AFL/PawnData/
  DA_AFL_PawnData_Hero_Default          (ULyraPawnData)
    PawnClass:               BP_AFLCharacter_Base
    InputConfig:             IC_AFL_Default
    AbilitySets:             [DA_AFL_AbilitySet_Combat_Pulse,
                              DA_AFL_AbilitySet_Movement_Default,
                              DA_AFL_AbilitySet_HUD]
    TagRelationshipMapping:  DA_AFL_TagRelationships

Content/AFL/Input/
  IC_AFL_Default                        (ULyraInputConfig)
    NativeInputActions: [InputTag.Movement.Move ↔ IA_Move,
                         InputTag.Movement.Look ↔ IA_Look]
    AbilityInputActions: [InputTag.Weapon.Fire   ↔ IA_Fire,
                          InputTag.Movement.Dash ↔ IA_Dash,
                          InputTag.Movement.Jump ↔ IA_Jump]

Content/AFL/AbilitySets/
  DA_AFL_AbilitySet_Combat_Pulse        (ULyraAbilitySet)
    GrantedGameplayAbilities: [{ Ability: UAFLAG_Laser_Pulse,
                                  InputTag: InputTag.Weapon.Fire }]
    GrantedAttributes:        [{ AttributeSet: UAFLAttributeSet_Combat,
                                  InitializationData: ID_AFL_Stats_Default }]
    GrantedGameplayEffects:   []

Content/AFL/Experiences/
  B_LyraExperience_AFL_Arena            (ULyraExperienceDefinition)
    DefaultPawnData: DA_AFL_PawnData_Hero_Default
    GameFeaturesToEnable: [AFLCore, AFLCombat, AFLMovement, AFLDismember]
```

This is the **only** path by which a player gets the Pulse Carbine. No `BeginPlay`. No constructor `GiveAbility`. No Blueprint event graph fiddling.

---

## 14. COMPLETED WORK LOG (immutable, append-only)

This section captures everything completed on the BAG MAN build. Entries are appended chronologically. Once marked `✅ COMPLETE`, an entry stays here forever — never deleted, never silently rewritten. If a completed approach is later replaced by a better one, the original entry stays and a new entry is appended noting the supersession.

**Why this exists**: Six months from now, when someone asks "did we ever try X?" or "why didn't we go with approach Y?", this log answers without requiring git archaeology. It's institutional memory in append-only form.

### 14.1 Pre-Production Planning & Architecture (v1.0 → v1.1)

| Date | Item | Status | Notes / Artifacts |
|---|---|---|---|
| 2026-05-08 | Master Build Document v1.0 drafted | ✅ COMPLETE | Senior Architect sign-off edition, 5 phases / 24 sprints / 48 weeks |
| 2026-05-08 | Live Build Tracker v1.0 generated (HTML, localStorage state) | ✅ COMPLETE | Cyberpunk neon aesthetic, per-task checkboxes |
| 2026-05-08 | v1.1 architecture amendments (Bag-Man corrections) | ✅ COMPLETE | TargetData hitscan pattern, ExecCalc damage pipeline, Lyra init contract, PlayFab→Lambda→GameLift tentpole; pulled EOS Matchmaking from scope |
| 2026-05-08 | AIK Session Starter doc authored (1,029 lines, 7 stages) | ✅ COMPLETE | Stage prompts for AFLCore → telemetry → CI lint with checkpoint audits |

### 14.2 Environment Bootstrap (2026-05-08 → 2026-05-10)

| Date | Item | Status | Notes / Artifacts |
|---|---|---|---|
| 2026-05-08 | Project codename "Bag_Man" established as .uproject filename | ✅ COMPLETE | `Bag_Man.uproject` (EngineAssociation 5.6) |
| 2026-05-08 | Project relocated from OneDrive to `C:\Dev\Bag_Man` | ✅ COMPLETE | 56,836 MB / 20,918 files moved via robocopy /MOVE /MT:8 |
| 2026-05-08 | .NET 8 SDK installed (UE 5.6 UBT dependency) | ✅ COMPLETE | aka.ms/dotnet/8.0 official installer |
| 2026-05-08 | Visual Studio project files generated for UE 5.6 | ✅ COMPLETE | `UnrealBuildTool -projectfiles`, 26.56s, no errors; iOS/Android SDK gaps deferred |
| 2026-05-08 | Claude Code 2.1.133 installed natively on Windows | ✅ COMPLETE | `C:\Users\tabor\.local\bin\claude.exe`, doctor reports OK |
| 2026-05-08 | Claude Code authenticated to C12 AI Gaming studio org | ✅ COMPLETE | Subscription auth (Premium seat), not API key |
| 2026-05-08 | v1.1 documentation staged into `Docs/` | ✅ COMPLETE | `BAG_MAN_MASTER_BUILD_v2.0.md` (this doc), `AFL_NEON_ARENA_LIVE_TRACKER_v1.1.html`, `AFL_NEON_ARENA_AIK_SESSION_STARTER.md` (older filenames retained until v2.0 doc set replaces them) |
| 2026-05-08 | Git repo initialized with Unreal-aware `.gitignore` | ✅ COMPLETE | Excludes `Binaries/`, `Saved/`, `Intermediate/`, `DerivedDataCache/`, `.vs/`, `.vsconfig`, etc. |
| 2026-05-08 | Baseline commit + cleanup commit on `main` | ✅ COMPLETE | `f841d361` (baseline) ← `3adf23ec` (cleanup) |
| 2026-05-08 | Safety tags placed | ✅ COMPLETE | `baseline-environment-validated`, `baseline-pre-lfs-migration` |
| 2026-05-10 | `.gitattributes` authored for 36 binary extensions → LFS | ✅ COMPLETE | Standard Unreal LFS tracking set |
| 2026-05-10 | LFS migration imported 19,945 files (54.56 GB) into LFS storage | ✅ COMPLETE | `git lfs migrate import --everything --include=…`, fsck OK |
| 2026-05-10 | Reflog expired + `git gc --prune=now` reclaimed 51 GB | ✅ COMPLETE | Pack dropped from 51.86 GB to 7 MB; total .git from 106 GB to ~55 GB |
| 2026-05-10 | `git fsck --full` clean (28,673 objects) | ✅ COMPLETE | All commits and trees verified |
| 2026-05-10 | `git lfs fsck` OK (19,945 pointers ↔ 54.56 GB objects) | ✅ COMPLETE | Every LFS pointer in tree has matching object |
| 2026-05-10 | GitHub Pro account confirmed; C12-Ai-Gaming org LFS allocation: **250 GB** | ✅ COMPLETE | Storage + bandwidth via GitHub data packs |
| 2026-05-10 | Phase B (GitHub remote + first push) **deferred** | ⏸ DEFERRED | ~10 Mbps bandwidth → 54 GB first push estimated 12+ hours. Push gated on (a) faster connection OR (b) Sprint 1 task AFL-0108 requiring remote |

### 14.3 SSOT & Documentation (2026-05-10)

| Date | Item | Status | Notes / Artifacts |
|---|---|---|---|
| 2026-05-10 | Title finalized: **BAG MAN** (NEON ARENA dropped) | ✅ COMPLETE | AFL code prefix retained |
| 2026-05-10 | SSOT charter authored (immutable-history protocol) | ✅ COMPLETE | This section + the charter at top of doc |
| 2026-05-10 | Master Build Document v2.0 SSOT edition produced | ✅ COMPLETE | This document |
| 2026-05-10 | Live Build Tracker v2.0 updated to BAG MAN branding + completed-work view | ✅ COMPLETE | `BAG_MAN_LIVE_TRACKER_v2.0.html` |
| 2026-05-10 | AFL skill suite formalized in §15 (11 skills documented) | ✅ COMPLETE | Blender bridge, Lyra skin builder, expert game designer, plus 8 existing AFL skills |

### 14.4 Phase 0 / Sprint 1 — Bootstrap (in progress)

| Task ID | Title | Status | Date / Notes |
|---|---|---|---|
| AFL-0101 | Bootstrap `AFLCore` GameFeature plugin | ⬜ NOT STARTED | Gated on AIK session contract paste |
| AFL-0102 | Define core gameplay tags in `Config/Tags/AFLTags.ini` | ⬜ NOT STARTED | |
| AFL-0103 | `UAFLAttributeSet_Combat` (Health, Shield, Armor, Heat, Energy, MovementSpeed) | ⬜ NOT STARTED | Stage 2 of session starter |
| AFL-0104 | `UAFLAG_Laser_Pulse` ability with TargetData pattern | ⬜ NOT STARTED | Canonical reference in §13.1 |
| AFL-0105 | `GE_AFL_Damage_Pulse` Instant GE wired to `UAFLDamageExecCalc` | ⬜ NOT STARTED | |
| AFL-0106 | Hitscan TargetData replication path | ⬜ NOT STARTED | |
| AFL-0107 | Sprint 1 PIE smoke test sign-off | ⬜ NOT STARTED | End gate of Sprint 1 |
| AFL-0108 | GitHub Actions CI on PR to `develop` | ⬜ NOT STARTED | First task requiring remote — push-gating |
| AFL-0109 | Naming validation CI lint | ⬜ NOT STARTED | |
| AFL-0110 | Greybox arena (Arena_01) | ⬜ NOT STARTED | |
| AFL-0111 | QA test plan template | ⬜ NOT STARTED | |

**Sprint 1 START GATE**: ✅ All preconditions met. Environment validated, docs staged, AIK ready. Cleared to begin.

### 14.5 How to Update This Log

When you complete a task or milestone:

1. Pick the right subsection (14.1 architecture, 14.2 environment, 14.3 SSOT, 14.4 Sprint 1, etc.). Add a new subsection 14.N if your work doesn't fit.
2. Append a row. Never edit an existing row's wording — only its status field, and only to move it forward (`⬜ NOT STARTED` → `🔄 IN PROGRESS` → `✅ COMPLETE`, or → `⏸ DEFERRED`, or → `❌ ABANDONED`).
3. Include date, commit SHA if relevant, and a one-line note pointing at the artifact (file path, PR number, doc reference).
4. If your completion supersedes an earlier approach: leave the earlier row, add a `🔄 SUPERSEDED BY [new task]` note in its row, then add the new row.

**Status legend**:
- ⬜ NOT STARTED — defined, not begun
- 🔄 IN PROGRESS — actively being worked
- ✅ COMPLETE — done, validated, committed
- ⏸ DEFERRED — paused with explicit reason; expected to resume
- ❌ ABANDONED — explicitly chosen not to pursue (with reasoning in notes)
- 🔄 SUPERSEDED BY — replaced by a later, better approach (pointer in notes)

---

## 15. AI WORKFLOW & SKILLS INFRASTRUCTURE

BAG MAN is built with a **multi-skill agent workflow**. The codebase is too large and the discipline matrix too broad for a single generalist agent. Each skill is a focused, project-aware module that Claude Code (via NeoStack AIK) loads when relevant to the task at hand.

### 15.1 The Eleven AFL Skills

The skills are organized by discipline. Each one has its own SKILL.md at a known path and is loaded automatically by Claude Code when its trigger description matches the task.

| # | Skill | Discipline | Primary Use |
|---|---|---|---|
| 1 | **afl-cpp-lyra-developer** | C++ Engineering | Lyra-correct C++ extension (GAS abilities, GameFeature plugins, AttributeSets, ExecCalcs). Enforces §7/§8/§9. |
| 2 | **afl-neostack-task-writer** | Engineering / AI Workflow | Writes high-quality AIK prompts that produce AFL-conformant Blueprints, Materials, Behavior Trees. |
| 3 | **afl-sprint-planner** | Production / Project Mgmt | Decomposes features into AFL-XXXX tasks, writes sprint briefs, estimates effort, tracks blockers. |
| 4 | **afl-build-operator** | DevOps / Build | UAT BuildCookRun pipelines, GitHub Actions, multi-platform packaging, dedicated server builds. |
| 5 | **afl-asset-pipeline** | Tech Art / Pipeline | DCC tool export settings, FBX/USD import, LOD generation, texture compression per platform. |
| 6 | **afl-qa-build-recovery** | QA / DevOps | Crash triage, broken-build recovery, regression matrices, platform cert (TRC/XR). |
| 7 | **afl-ui-hud-design** | UI Engineering | Lyra CommonUI stack, UMG widgets, activatable widgets, multi-platform input routing. |
| 8 | **afl-blender-bridge** ⭐ NEW v2.0 | Tech Art / 3D | Blender↔UE5 round-trip via `blender_mcp`. Kitbash, retexture, dress, audit, AAA-clean modular assets. Composes with NeoStack genAI (Tripo, Meshy). Image-to-level blockouts. Heightmap landscapes. |
| 9 | **lyra-skin-builder-marketplace** ⭐ NEW v2.0 | Character / Cosmetics | Lyra-foundation reskinning pipeline. Mesh swap, IK retargeting to SK_Mannequin, modular character parts, in-game cosmetic marketplace, GameFeature plugins for live-ops skin drops, server-authoritative entitlement. |
| 10 | **expert-game-designer** ⭐ NEW v2.0 | Game Design / Visual Direction | Apple-Glass-inspired UI aesthetic, level/environment design direction, character/creature concept, Midjourney + NeoStack AIK prompt pipelines, design systems, palettes, typography. |
| 11 | **unreal-engine-expert** | C++ Engineering (general UE5) | AAA-level general UE5 expertise: rendering (Lumen, Nanite, Niagara), gameplay systems, AI, networking, optimization, animation. Pairs with afl-cpp-lyra for AFL-specific work. |

### 15.2 Skill Composition Patterns

Skills are not used in isolation — they compose. A few representative workflows:

**Workflow A — "Build me a new weapon (Ricochet Lancer)"**
1. `afl-sprint-planner` decomposes the feature into Sprint 13 tasks (AFL-1308 already exists).
2. `afl-cpp-lyra-developer` writes `UAFLAG_Laser_Ricochet` extending `ULyraGameplayAbility`, following the canonical TargetData pattern from §13.1.
3. `afl-neostack-task-writer` writes the AIK prompts that turn the spec into compiled code.
4. `afl-qa-build-recovery` defines the regression test matrix.

**Workflow B — "Modular cyberpunk hallway kit for Arena_01"**
1. `expert-game-designer` produces concept direction + reference imagery + Midjourney prompts.
2. `afl-blender-bridge` generates kitbash variants in Blender via MCP (signage decals, wall panels, lighting fixtures).
3. `afl-asset-pipeline` enforces FBX export settings (locked transforms, smoothing groups, lightmap UVs).
4. UE5 reimport via blender_mcp manifest; lit by `afl-ui-hud-design` for HUD/screen displays in-world.

**Workflow C — "AI-generated boss mesh from Tripo, get it ship-ready"**
1. `afl-blender-bridge` ingests the raw Tripo output, runs AAA-cleanup (decimate, retopo to budget, UV unwrap, material assignment, socket placement).
2. `lyra-skin-builder-marketplace` rigs to SK_Mannequin via IK retargeting if humanoid.
3. `afl-asset-pipeline` validates against the AFL conformance manifest before import.
4. `afl-cpp-lyra-developer` wires Gameplay Ability hooks if interactive.

**Workflow D — "New cosmetic skin drop for live-ops Week 3"**
1. `lyra-skin-builder-marketplace` handles the mesh swap + IK retargeting + GameFeature plugin scaffold.
2. `expert-game-designer` produces the shop UI mockups in Apple-Glass aesthetic.
3. `afl-ui-hud-design` implements the marketplace activatable widget in CommonUI.
4. `afl-build-operator` packages the GameFeature plugin for hot deployment.

### 15.3 Skill Locations on Disk

```
/mnt/skills/user/
  ├─ afl-cpp-lyra-developer/SKILL.md
  ├─ afl-neostack-task-writer/SKILL.md
  ├─ afl-sprint-planner/SKILL.md
  ├─ afl-build-operator/SKILL.md
  ├─ afl-asset-pipeline/SKILL.md
  ├─ afl-qa-build-recovery/SKILL.md
  ├─ afl-ui-hud-design/SKILL.md
  ├─ afl-blender-bridge/SKILL.md          ⭐ added v2.0
  ├─ lyra-skin-builder-marketplace/SKILL.md  ⭐ added v2.0
  ├─ expert-game-designer/SKILL.md           ⭐ added v2.0
  └─ unreal-engine-expert/SKILL.md
```

### 15.4 Skill Governance — AFL-0218 CI Check

The skill registry is **authoritative**. A PR is rejected by CI if:
- It references a skill not listed in §15.1
- It modifies a skill's behavior without bumping its version in SKILL.md
- It introduces a workflow that bypasses skill conventions (e.g., direct AIK prompt that violates §7/§8/§9 instead of going through `afl-cpp-lyra-developer`)

The CI workflow is `AFL-0218` (Sprint 2) and reads `/Docs/SKILLS_REGISTRY.md`, which is auto-generated from this section.

### 15.5 When to Add a New Skill

Add a new skill when:
- A discipline workflow occurs more than 3 times across separate sprints
- A specific technical pattern (e.g. Niagara VFX authoring, audio middleware integration) consumes >2 hours per occurrence and follows a stable pattern
- A cross-discipline bridge (like Blender↔UE5) requires more than 5 conventions/rules to operate correctly

Don't add a skill for:
- One-off tasks
- Pure delegation ("write me the same kind of code I asked for last time") — that's what session context is for
- Things that are still being discovered (skills should encode hard-won conventions, not exploration)

---

## 16. CROSS-DISCIPLINE BRIDGES

Bridges are the formal handoffs between disciplines. They turn art→engineering, design→engineering, and AI-gen→game-ready into repeatable workflows instead of one-off heroics.

### 16.1 Blender ↔ UE5 Round-Trip (afl-blender-bridge)

**Tool**: `blender_mcp` — an MCP server that lets Claude Code drive Blender via Python in-process. Operates on the running Blender instance, no file shuffling.

**Required setup** (Sprint 2 task **AFL-0216**):
1. Install Blender 4.x (LTS) on each dev workstation
2. Install `blender_mcp` connector following its docs
3. Add `blender_mcp` to the Claude Code MCP config
4. Validate via test asset: import any `SM_Lyra_Cube_01.uasset` placeholder → modify in Blender via MCP (add bevel, paint a vertex group) → re-export FBX → reimport to UE5 → verify visual difference

**Locked FBX export settings** (every Blender → UE5 trip uses these):
```
Scale:              1.0 (apply transforms before export)
Forward Axis:       -Y
Up Axis:            Z
Apply Modifiers:    True
Smoothing:          Face
Bake Anim:          False (for static meshes)
Use Tangent Space:  True
```

**Reimport manifest**: every Blender→UE5 round-trip writes `_AFL_REIMPORT_MANIFEST.json` next to the FBX, capturing:
- Source Blender file SHA
- Export timestamp + operator name
- Modifier stack state
- LOD count + poly budget per LOD
- Socket positions (by name)
- Material slot assignments

UE5's `afl_reimport.py` editor utility reads the manifest, validates against AFL conformance, and either imports or rejects with a structured error.

**What this enables**:
- Kitbash variants without leaving Claude Code's command flow
- Retexture passes driven by `expert-game-designer` mockups
- Asset audits (find every mesh over 50k tris in a folder)
- Image-driven level blockouts (concept art → grayscale heightmap → modular kit population)

### 16.2 NeoStack genAI → AFL Mesh Validation Pipeline (AFL-0217)

**Inputs**: Raw outputs from Tripo (image-to-3D, text-to-3D), Meshy, or similar genAI mesh tools. These produce visually plausible meshes that are usually **not game-ready**: bad topology, no LODs, no collision, no sockets, baked materials with no texture-set discipline.

**Pipeline**:

```
                                      ┌──────────────────────────┐
[Tripo/Meshy raw .glb/.fbx]    ───►   │ afl_mesh_validator.py    │
                                      │  ─ Poly budget check     │
                                      │  ─ Naming convention     │
                                      │  ─ UV coverage           │
                                      │  ─ Material slot count   │
                                      │  ─ Bone/socket sniff     │
                                      └──────────┬───────────────┘
                                                 │
                                ┌────────────────┴────────────────┐
                                ▼                                 ▼
                          [PASS manifest]                  [FAIL report]
                                │                                 │
                                ▼                                 ▼
                  ┌──────────────────────────┐      ┌──────────────────────────┐
                  │ afl-blender-bridge       │      │ Reject + send to         │
                  │  AAA-clean pipeline:     │      │ tech artist with         │
                  │  ─ Decimate to budget    │      │ specific failure list    │
                  │  ─ Retopo if humanoid    │      └──────────────────────────┘
                  │  ─ UV unwrap             │
                  │  ─ Material assignment   │
                  │  ─ LOD0/1/2/3 generation │
                  │  ─ Socket placement      │
                  │  ─ Collision generation  │
                  │  ─ Export with manifest  │
                  └──────────┬───────────────┘
                             │
                             ▼
                  ┌──────────────────────────┐
                  │ UE5 reimport             │
                  │ (afl_reimport.py)        │
                  └──────────────────────────┘
```

**Conformance budgets** (per asset class):
| Class | LOD0 tris | LOD3 tris | Texture max | Material slots |
|---|---|---|---|---|
| Hero character | 80,000 | 8,000 | 4K | 8 |
| Heavy weapon | 25,000 | 3,000 | 2K | 4 |
| Environment hero prop | 15,000 | 1,500 | 2K | 4 |
| Modular kit piece | 5,000 | 500 | 1K (tileable) | 2 |
| Decal | 200 | — | 1K (alpha) | 1 |

These are enforced by the validator. AFL-0217 ships the validator + budgets table as a DataTable for designer tuning.

### 16.3 Image → UE5 Landscape Heightmap

For terrain-heavy maps in Phase 4, the workflow:

1. **Source image** — grayscale concept art, real-world DEM, or AI-generated heightmap (Midjourney with `--tile` + `--ar 1:1` + height-map prompt phrasing).
2. **Blender preprocess** (via `afl-blender-bridge`):
   - Resize to 1009×1009 or 2017×2017 (UE5 recommended powers-of-2 + 1)
   - Apply gaussian blur to remove genAI noise
   - Output 16-bit grayscale PNG
3. **UE5 Landscape import** — Tools → Landscape → New → Import from File.
4. **Material setup** — auto-blend slope/altitude using Lyra's `M_Terrain_Master`.
5. **Decal pass** — `afl-blender-bridge` generates road/path decals from a second-pass image.

This is documented as a workflow rather than a sprint task because it's used ad-hoc per map.

### 16.4 Concept Art → Modular Kit (expert-game-designer × afl-blender-bridge)

For Phase 4 arena maps:

1. `expert-game-designer` produces concept art + Midjourney variations + a written design intent.
2. Concept gets distilled into a **kit manifest**: list of modular pieces required (walls, floors, lighting, signage, hero props).
3. `afl-blender-bridge` either kitbashes from Lyra's existing modular content OR generates new pieces via Tripo→validation pipeline.
4. Pieces meet AFL naming conventions before import (`SM_AFL_<MapName>_<Category>_<Variant>`).
5. Level designer assembles in UE5 using Modeling Tools' modular workflow.

The bridge eliminates the "weeks of art bottleneck" that used to gate level production in pre-AI pipelines.

---

## 17. THE LIVE BUILD TRACKER

A separate **interactive HTML dashboard** (sibling artifact to this document) gives the team:

- ✅ Per-task checkboxes with persistent state
- 📊 Phase + sprint progress bars
- 🚨 Blocker registry
- 🎯 GO/NO-GO gate visibility
- 📅 Today/This-Week views

**Open the tracker daily.** Standup happens against the tracker, not against memory.

---

## 18. FINAL DIRECTIVE

> Build the foundation correctly. Layer 1 (Game Feel) is the only layer where "good enough" is unacceptable. Every other layer can be iterated on after launch — game feel cannot.

> Do not start Phase 2 until Phase 1's End Gate is signed off by the Studio Lead. Skipping a gate is the single most expensive mistake this project can make.

> NeoStack AIK + Claude Code is a force multiplier, not a replacement for engineering judgment. Always read generated code before committing it. Always run it in PIE before pushing. Always reference the AFL task ID.

---

**Document version:** 1.0
**Last updated:** Sprint 0 (pre-kickoff)
**Next review:** End of Sprint 1
**Maintained by:** Producer + Engineering Lead
