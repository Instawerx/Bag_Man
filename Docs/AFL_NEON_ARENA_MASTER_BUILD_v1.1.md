# AFL :: NEON ARENA — MASTER BUILD DOCUMENT
**Senior Architect Sign-off Edition — v1.1 (post Bag-Man amendments)**

> *"Competitive cyber sports chaos. High-mobility neon laser combat with extraction stakes, robot dismemberment spectacle, and a social lounge that turns spectators into customers."*

> **v1.1 changes** (locked before Sprint 1): (a) Replaced broken Pulse Carbine appendix with TargetData-driven canonical version; (b) Added §7 hitscan validation doctrine, §8 damage pipeline, §9 Lyra init contract; (c) Added Sprint 2 tasks AFL-0211/0212/0213/0214/0215; (d) Restructured Sprint 11 around AFL-1100 PlayFab→Lambda→GameLift tentpole; pulled EOS Matchmaking out of scope; (e) Added R-09 to risk register; (f) Tightened code review forbidden-pattern list.

---

## 0. HOW TO READ THIS DOCUMENT

This is the **single source of truth** for the build. It is paired with the **Live Build Tracker** (HTML artifact) which the team uses daily to mark task progress.

| You are... | Read these sections |
|---|---|
| Studio Lead / Producer | §1, §2, §3, §10, §14, §15 |
| Engineering Lead (any) | §3, §6, §7, §8, §9, §13 |
| Combat / Gameplay Engineer | §5 (your phase), §7, §8, §9, §13.1, §13.4, §13.5 |
| Online / Backend Engineer | §5 (P3), §10, §13.3 |
| Art / Animation / VFX | §4 pipeline, §5 phase deliverables, §11 |
| QA Lead | §6, §7 (Layer 5 telemetry), §10, §12, §14 |
| New hire onboarding | §1 → §3 → §7 → §8 → §9 in order, then your phase in §5 |

Every sprint has an explicit **START GATE** (entry criteria) and **END GATE** (exit criteria with QA sign-off). No sprint advances without its end gate passing.

**Authoritative sections — NO violations permitted, enforced by code review and CI lint (AFL-0215):**
- §7 Server-Authoritative Hitscan Validation
- §8 Damage Pipeline & ExecCalc
- §9 Lyra Initialization Contract

---

## 1. PROJECT IDENTITY

```
Codename:        AFL
Working Title:   NEON ARENA
Genre:           Competitive arena shooter + light extraction
Engine:          Unreal Engine 5.5+ (Lyra Starter Game base)
Targets:         Win64 (Steam/EGS) → PS5 → XSX → iOS → Android
Match Length:    8–12 minutes
Player Count:    12–20 per match
Online Stack:    Epic Online Services (EOS) + Steamworks + PlayFab backend
Servers:         Dedicated authoritative (AWS GameLift fleet, Linux)
Anti-Cheat:      Easy Anti-Cheat (EAC via EOS)
Source Control:  GitHub (Git LFS for binary assets)
CI/CD:           GitHub Actions
PM Tool:         AFL custom internal tool (task IDs: AFL-XXXX)
AI Workflow:     Claude Code via NeoStack AIK in-editor + Claude Code CLI
```

### 1.1 Studio Identity Pillars (non-negotiable)
1. **Game feel before content.** A weak shot kills the game; missing skins do not.
2. **Lyra is the spine — extend, don't edit.** All work lives in GameFeature plugins.
3. **Server authoritative everything.** Client predicts; server decides.
4. **Spectacle as identity.** Neon, dismemberment, head physics — every match has shareable moments.
5. **Ship vertical slices, not horizontal scaffolding.** Each phase produces a playable, testable artifact.

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

## 14. THE LIVE BUILD TRACKER

A separate **interactive HTML dashboard** (sibling artifact to this document) gives the team:

- ✅ Per-task checkboxes with persistent state
- 📊 Phase + sprint progress bars
- 🚨 Blocker registry
- 🎯 GO/NO-GO gate visibility
- 📅 Today/This-Week views

**Open the tracker daily.** Standup happens against the tracker, not against memory.

---

## 15. FINAL DIRECTIVE

> Build the foundation correctly. Layer 1 (Game Feel) is the only layer where "good enough" is unacceptable. Every other layer can be iterated on after launch — game feel cannot.

> Do not start Phase 2 until Phase 1's End Gate is signed off by the Studio Lead. Skipping a gate is the single most expensive mistake this project can make.

> NeoStack AIK + Claude Code is a force multiplier, not a replacement for engineering judgment. Always read generated code before committing it. Always run it in PIE before pushing. Always reference the AFL task ID.

---

**Document version:** 1.0
**Last updated:** Sprint 0 (pre-kickoff)
**Next review:** End of Sprint 1
**Maintained by:** Producer + Engineering Lead
