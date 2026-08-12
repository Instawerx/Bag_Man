# BAG MAN — DOCTRINE (Tier 1)

**What this is:** the laws. Content here changes **rarely**, and changing it requires justification.
**What this is not:** a status board (that is `LIVE_TRACKER`) and not a system design (that is `Docs/ssot/`).

> **THE LOAD-BEARING RULE OF THE DOC SYSTEM**
> A **Tier-2 SSOT may never contain a STATUS claim.** A **Tier-3 tracker may never contain a DESIGN decision.**
> `ShantyTown_BR_DESIGN.md §2` was a status claim living in a design doc — accurate when written, false four
> commits later. That is the failure this structure exists to prevent.

**How to read a row.** Every law carries its **source** and its **verification status**:
- **VERIFIED-ON-DISK** — re-checked against code/config/git at authoring time, with the citation.
- **UNVERIFIED** — durable and uncontested, but not mechanically checkable from disk (or not yet checked).

Sources are cited as `FILE:LINE`. Where a law appeared in more than one source, it has been **converged into one
canonical wording** and all sources are cited. Claims that disk contradicts were **excluded** and are recorded in
**Appendix A**.

Authored 2026-08-05 from `CLAUDE.md`, `Docs/BAG_MAN_MASTER_BUILD_v2.0.md`, the C++ header comments (the richest
documentation in this project), and the rescued-unique subset of the archived `Docs/CORE_GAME_CONCEPT.md`.

---

## 1. PROOF STANDARDS

The single most violated and most expensive category. Nothing here is negotiable.

| # | Law | Source | Verified |
|---|---|---|---|
| P1 | **✅ means demonstrated in PIE on a controllable pawn** — watched moving/firing on screen. **Never "compiles," never "committed."** A green checkbox is a thing you saw work. | `CLAUDE.md:17` | VERIFIED-ON-DISK — enforced throughout; every C++ block this session reported "COMPILES-ONLY, the run is the operator's" |
| P2 | **Never mark work done off a successful build alone.** Open PIE — listen server + 2 clients for anything networked — and watch it. | `CLAUDE.md:38` | UNVERIFIED (process) |
| P3 | **The live PIE / live terminal is the only source of truth.** Status marks and summaries are *reports*, not proof. | `CLAUDE.md:27` | UNVERIFIED (process) |
| P4 | **Replication is proven with 2+ clients, never on a listen-host alone.** A single-client test cannot surface a `FNetSerializeScriptStructCache` desync or a replicated-state divergence. | `CLAUDE.md:38`, `IRONICS_PROMOD_CHARACTER_SSOT.md:261` | UNVERIFIED (process) |
| P5 | **A validator must be shown to FAIL on known-bad input before its pass means anything.** | `IRONICS_PROMOD_CHARACTER_SSOT.md:274` | UNVERIFIED (process) |
| P6 | **A negative result from an unverified token is not evidence.** Absence of a grep hit proves nothing until the token itself is proven to match something. | `IRONICS_PROMOD_CHARACTER_SSOT.md:281` | UNVERIFIED (process) |
| P7 | **Canary before scaling.** One head, one sticker, one weapon, one map proven before any batch. | `IRONICS_PROMOD_CHARACTER_SSOT.md:266` | UNVERIFIED (process) |
| P8 | **A backend microservice must be standalone-green against a synthetic harness before any client integration.** No client, no UE build, no real server build — the real returned tuple is the green checkbox. | `BAG_MAN_MASTER_BUILD_v2.0.md:513`, `:1211` | UNVERIFIED |
| P9 | **Always read generated code before committing it. Always run it in PIE before pushing.** | `BAG_MAN_MASTER_BUILD_v2.0.md:2194` | UNVERIFIED (process) |
| P10 | **Definition of Done is hierarchical:** TASK = acceptance criteria + PR merged + CI green · SPRINT = demo + retro · PHASE = end gate signed off · LAUNCH = cert + stress + hotfix readiness. | `BAG_MAN_MASTER_BUILD_v2.0.md:1248-1254` | UNVERIFIED |

---

## 2. ARCHITECTURE LAWS

| # | Law | Source | Verified |
|---|---|---|---|
| A1 | **ASC lives on `PlayerState`. Never on the Character.** Persistent across respawn, multiplayer-safe. | `BAG_MAN_MASTER_BUILD_v2.0.md:226` **+** `CORE_GAME_CONCEPT.md:422` *(converged)* | **VERIFIED-ON-DISK** — `Source/LyraGame/Player/LyraPlayerState.h:62` `GetLyraAbilitySystemComponent()` |
| A2 | **Stay Lyra-canonical.** Characters and weapons are **Blueprint child + data assets** (PawnData, EquipmentDefinition, AbilitySet, InputConfig). **Never assemble a hero or weapon from raw C++.** The original project failure was an empty `DefaultInputMappings` from a raw-C++ hero component. | `CLAUDE.md:19` **+** `BAG_MAN_MASTER_BUILD_v2.0.md:135` **+** `CORE_GAME_CONCEPT.md:162` *(converged)* | UNVERIFIED (historical) |
| A3 | **Abilities are granted via AbilitySets** on PawnData/EquipmentDefinition — **never `GiveAbility` in `BeginPlay`/code.** Ability sets are granted at `InitState_DataInitialized`, nowhere else. Bypassing the InitState chain survives Experience swap and produces ghost abilities. | `CLAUDE.md:23` **+** `BAG_MAN_MASTER_BUILD_v2.0.md:915`, `:933`, `:973` **+** `CORE_GAME_CONCEPT.md:634` *(converged)* | **VERIFIED-ON-DISK** — `AFLBattleRoyaleComponent`/`AFLRoundManagerComponent` grant nothing in BeginPlay |
| A4 | **No ability modifies `Health` directly.** All damage flows through a meta-attribute and an execution calculation (`UAFLDamageExecCalc`). | `BAG_MAN_MASTER_BUILD_v2.0.md:831` | **VERIFIED-ON-DISK** — `AFLDamageExecCalc.cpp` is the sole damage path |
| A5 | **Cosmetics go through GameplayCues. Gameplay never touches Niagara; cosmetics never touch attributes.** | `CLAUDE.md:25` | UNVERIFIED |
| A6 | **Cooldowns are Cooldown GameplayEffects, never float variables or hand-rolled timers.** | `CLAUDE.md:25`, `:81` **+** `CORE_GAME_CONCEPT.md:596-611` *(converged)* | **VERIFIED-ON-DISK** — 12 `GE_AFL_Cooldown_*` classes in `AFLCombat/Public/Effects/` |
| A7 | **Use gameplay tags instead of booleans** for state. State is a tag on the ASC, not a bool on an actor. | `CORE_GAME_CONCEPT.md:457` *(rescued — unique)* | **VERIFIED-ON-DISK** — `State.*` tag namespace used throughout `AFLCombat` |
| A8 | **Input flows via `ULyraInputConfig` InputTags — never a direct `InputAction` binding to a character method.** | `BAG_MAN_MASTER_BUILD_v2.0.md:927`, `:934` | UNVERIFIED |
| A9 | **Bots run the same abilities as players; only the input trigger differs.** A weapon ability must accept a `GameplayEvent` trigger (`InputTag.Weapon.Fire`) alongside the player input path, or bots cannot fire it. | `CORE_GAME_CONCEPT.md:613-618` *(rescued — unique)* | **VERIFIED-ON-DISK** — `AFLAG_Hitscan_Base.cpp:53` *"BOT-FIRE PARITY (mandatory): ShooterCore BTS_Shoot sends GameplayEvent(InputTag.Weapon.Fire). Without a matching trigger bots can't fire"* |
| A10 | **A deployable is a lean replicated `AActor`, never a Pawn.** A Pawn is counted by round/team/alive logic and corrupts the match. Deployables self-grant the ASC + health stack to be destructible. | `AFLEMPDevice.h` header comment *(code-only; no `Docs/` coverage)* | **VERIFIED-ON-DISK** — `AFLEMPDevice.h`, `AFLDeployableBarrier` |
| A11 | **Full-Body IK is permanent doctrine.** Every character, current and future, is FBIK on the rigged **Manny/Quinn** skeleton bases. Not experimental, not per-character opt-in. | Operator ruling R5; assets `8367f0b8` | **VERIFIED-ON-DISK** — `CR_ProMod_FBIK` / `ABP_ProMod_FBIK_PP` / `IK_ProMod_FBIK`; `FullBodyIK` plugin `"Enabled": true` in `Bag_Man.uproject` |

---

## 3. NET-SAFETY LAWS

| # | Law | Source | Verified |
|---|---|---|---|
| N1 | **Server-authoritative everything. Client predicts; server decides.** | `BAG_MAN_MASTER_BUILD_v2.0.md:136` | UNVERIFIED (pervasive) |
| N2 | **The server NEVER calls `GetPlayerViewPoint`.** On a dedicated server it falls back to control rotation and lies about where the client was aiming. **Camera position is delivered via TargetData or it does not exist.** | `BAG_MAN_MASTER_BUILD_v2.0.md:789` | UNVERIFIED |
| N3 | **Never trust a client-supplied trace that produces a hit** without running it through lag-compensation rewind validation. | `BAG_MAN_MASTER_BUILD_v2.0.md:755`, `:763` | UNVERIFIED |
| N4 | **Rewind window is server-measured RTT, never client-claimed, hard-capped at 200 ms.** Callers compute `ServerTime = World->GetTimeSeconds() - ClampedRTT` where `ClampedRTT = min(serverRTT/2 + interp, 200ms)`. | `BAG_MAN_MASTER_BUILD_v2.0.md:805` + `AFLLagCompensationWorldSubsystem.h` | **VERIFIED-ON-DISK** — header documents exactly this formula |
| N5 | **The shooter's own pawn is excluded from rewind** — its trace already ran in its own client-local frame; rewinding it double-compensates. | `AFLLagCompensationWorldSubsystem.h` *(code-only)* | **VERIFIED-ON-DISK** |
| N6 | **Rewind is non-mutating.** Never write bone transforms onto a live `USkeletalMeshComponent` — the engine routes pose through the anim graph, so a direct write fights the next tick and corrupts the rest pose. Query the token instead. | `AFLLagCompensationWorldSubsystem.h` *(code-only)* | **VERIFIED-ON-DISK** |
| N7 | **Restore is scope-guarded and idempotent** (`ON_SCOPE_EXIT`); double-restore is safe, post-restore token queries are a programming error. | `BAG_MAN_MASTER_BUILD_v2.0.md:811` + `AFLLagCompensationWorldSubsystem.h` | **VERIFIED-ON-DISK** |
| N8 | **Any net-serialized struct lives in `AFLNetTypes` (Runtime, Default phase, NON-GameFeature) — never in a GameFeature module.** A GameFeature module can unload and take its `FNetSerializeScriptStructCache` registration with it, desyncing the cache and dropping connections. Single-client testing never surfaces this. | `IRONICS_PROMOD_CHARACTER_SSOT.md:129-132` | **VERIFIED-ON-DISK** — `AFLNetTypes` is a separate always-loaded plugin; `AFLCombat.Build.cs:42` documents the rule |
| N9 | **Never replicate beam particles.** Replicate the beam's start/end only; clients render locally. A beam's only inputs are `User.Beam End` (Vector) and `User.Color`. | `CLAUDE.md:69` **+** `CORE_GAME_CONCEPT.md:2765` *(converged; the replication clause was unique to the archived source)* | UNVERIFIED |
| N10 | **Dedicated authoritative servers. No peer-to-peer gameplay hosting** — required for anti-cheat, fairness and match integrity. | `CORE_GAME_CONCEPT.md:2410` *(rescued — unique as a law)* | UNVERIFIED |
| N11 | **The client never decides owned items, currencies, trades or unlocks.** The economy service is the source of truth. | `BAG_MAN_MASTER_BUILD_v2.0.md:159`, `:485` **+** `CORE_GAME_CONCEPT.md:2513` *(converged)* | UNVERIFIED |
| N12 | **Any P2P ownership transfer requires transactional locking + rollback protection + anti-dupe validation + escrow/confirm, ending in an atomic exchange.** | `CORE_GAME_CONCEPT.md:2553` *(rescued)*; also `IRONICS_PLAYER_FLOW.md:218-234` | UNVERIFIED — **not built** (`PLAYER_FLOW.md:172` "does not exist yet") |

---

## 4. THE ARSENAL TAG CONTRACT (LAW)

Established by `ab6e0594`. This lived **only in code and one commit message** before this document.

| # | Law | Verified |
|---|---|---|
| T1 | **Every weapon and movement ability blocks on `State.Match.Warmup` and `State.Match.Ended`.** These belong in the ability's `ActivationBlockedTags`. This is what makes the match-phase freeze real rather than advisory. | **VERIFIED-ON-DISK** — `AFLAG_Hitscan_Base.cpp:66-67` |
| T2 | **`State.Overheated` is the SINGLE heat tag.** `State.Weapon.Overheated` was a parallel, undeclared tag granted by nothing and gated on by two weapons — an inert gate. It is eliminated; repo-wide grep returns zero. | **VERIFIED-ON-DISK** — `AFLAG_Hitscan_Base.cpp:44`, `:70`; declared `AFLCombatTags.ini:49`; granted `AFLAttributeSet_Combat.cpp:283` |
| T3 | **Every weapon owns its own cooldown GE granting its own `Cooldown.Weapon.<Name>` tag.** A shared cooldown GE means one shot gates every weapon that shares it — GAS gates on the granted tag. | **VERIFIED-ON-DISK** — 12 `GE_AFL_Cooldown_*` classes |
| T4 | **No undeclared tags.** Every gameplay tag is declared in a `*Tags.ini` alongside its peers. An unregistered tag resolves empty: `SendGameplayEvent` silently no-fires and `MatchesTag` silently fails. | **VERIFIED-ON-DISK** — `AFLCombatTags.ini` |
| T5 | **The disable/lockout pattern is: a GE grants a `State.*` tag that sits in `ActivationBlockedTags` on the ability base.** Because the entire AFL weapon roster descends from one base, one tag disarms everything at once while leaving movement abilities untouched (a disabled player is disarmed but can still flee). | **VERIFIED-ON-DISK** — `AFLEMPDevice.h` DISABLE CONTRACT; same shape as the SMG overheat lockout |
| T6 | **A tag-gated cooldown/lockout is only real if something GRANTS the tag.** Verify the granting GE exists and is wired, not just that the ability blocks on it. | **VERIFIED-ON-DISK** — T2 is the counter-example that proves the rule |

> ⚠ **Owed (recorded, not a law):** the BP half of `ab6e0594` — repointing each `GA_AFL_<Name>` `CooldownGameplayEffectClass` to its new per-weapon GE, and updating the SMG overheat GE to grant `State.Overheated`. Until then the new GEs exist but are unused, and the SMG lockout is at risk. Tracked in Tier 3.

---

## 5. GAMEFEATURE RULES + ENGINE PARTITION

| # | Law | Source | Verified |
|---|---|---|---|
| G1 | **Lyra is the spine — extend it in an ORDER OF PREFERENCE, not under a prohibition.** ① **Clone and harvest** Lyra code into AFL modules wherever that is possible — the preferred route. ② **Patch Lyra core** where linkage or engine structure genuinely requires it (G2). AFL work lives in plugins under `Plugins/GameFeatures/` because that is the cleanest structure, **not** to preserve upstream mergeability. ⚠ **Upstream mergeability is NOT a constraint on this project** — the engine is frozen at revision, and the AAA-best option permanently overrides upstream purity. | `BAG_MAN_MASTER_BUILD_v2.0.md:135`, `:195`, `:1218` **+** `CORE_GAME_CONCEPT.md:162` *(converged)*; **amended by operator ruling R6** | **VERIFIED-ON-DISK** |
| G2 | **Patching Lyra core is the CORRECT FALLBACK when clone-and-harvest cannot work — not a violation and not an exception.** Where the engine structure forbids the clean route, patch it. Precedent: `ALyraPlayerBotController` carries no export macro, so a GameFeature C++ subclass **fails at LINK**; the correct fix was `MinimalAPI` + `UE_API` on the Lyra class. **Every core patch is recorded here and re-applied after an engine bump** — that record is the only obligation the patch creates. | commit `a9c8fdaf`; **operator ruling R6** | **VERIFIED-ON-DISK** |
| G3 | **Do not bypass Lyra's initialization choreography.** Bypassing breaks GameFeature hot-load and breaks Experience swap mid-match. Wait for the InitState chain — do not poll, do not use a timer. *(The source's third rationale, "creates merge conflicts on every upstream pull", was **retired by R6** — upstream mergeability is not a constraint on this project. The rule stands unchanged on the two runtime grounds.)* | `BAG_MAN_MASTER_BUILD_v2.0.md:894`, `:935`; rationale trimmed per **R6** | UNVERIFIED |
| G4 | **`/Game` content must not reference GameFeature content, and a GameFeature map must not reference another GameFeature's content** (AssetReferenceRestrictions). | `IRONICS_PROMOD_CHARACTER_SSOT.md:301` | UNVERIFIED |
| G5 | **A data asset with no consumer is inert.** Check for a reader before authoring config. | `IRONICS_PROMOD_CHARACTER_SSOT.md:300` | UNVERIFIED |
| G6 | **`AddComponents` matches by class INCLUDING subclasses.** A duplicated BP is a *sibling*, not a child — entries targeting the source class will not apply to it. | `IRONICS_PROMOD_CHARACTER_SSOT.md:297` | UNVERIFIED |
| G7 | **The backend microservice lives in a separate sibling repo (`Bag_Man_Backend`), never in the UE LFS repo.** Only deployed IDs cross back, as non-secret config. | `BAG_MAN_MASTER_BUILD_v2.0.md:515` | UNVERIFIED |
| G8 | **Separate GAMEPLAY from ONLINE SERVICES from LIVE CONTENT.** Tangling them makes updates dangerous. | `CORE_GAME_CONCEPT.md:2296` *(rescued)* | UNVERIFIED |
| G9 | **PRUNING: unused and unaffected Lyra content is pruned at the END of the project — never mid-flight.** This is safe precisely because upstream is owed nothing (G1): with no merge obligation, deleting what the game does not ship carries no future cost. Pruning early is what is dangerous — a system that looks unused mid-project is routinely a dependency of something not yet built. | operator ruling **R6** | UNVERIFIED (scheduled work) |

---

## 6. BUILD DISCIPLINE

| # | Law | Source | Verified |
|---|---|---|---|
| B1 | **The editor must be CLOSED for a C++ build.** Use `-WaitMutex`. | `AGENTS.md:50-51`, `CLAUDE.md:35` | **VERIFIED-ON-DISK** — enforced every build this session |
| B2 | **Build against the C: launcher engine** (`C:\Program Files\Epic Games\UE_5.6`), **not the D: source engine** — D: overwrites C: binaries and a C: `LyraEditor` rebuild is then owed before the editor reopens. | Operator standing instruction (Blocks 171-182) | **VERIFIED-ON-DISK** — project `EngineAssociation: 5.6`; editor binaries are launcher-pattern `UnrealEditor-*.dll` |
| B3 | **Canonical build command:** `Build.bat LyraEditor Win64 Development -Project="C:\Dev\Bag_Man\Bag_Man.uproject" -WaitMutex` | `AGENTS.md:57`, `CLAUDE.md:36` | **VERIFIED-ON-DISK** |
| B4 | **Multi-platform is a shipping requirement:** PC + PS5/XSX + iOS/Android. Every shipped material needs a PC master and a `_Mobile` instance; guard platform code with `#if PLATFORM_*`. | `CLAUDE.md:29`, `BAG_MAN_MASTER_BUILD_v2.0.md:269-273` | UNVERIFIED |
| B5 | **CI is real and enforcing.** `.github/workflows/afl-pr-build.yml` runs `AFL_Lint` (asset naming + skill registry) and **fails the PR** if `Docs/SKILLS_REGISTRY.md` is stale. | `.github/workflows/afl-pr-build.yml:90-100` | **VERIFIED-ON-DISK** |
| B6 | **`Docs/SKILLS_REGISTRY.md` is GENERATED, not authored, and its path is hardcoded.** Never hand-edit it; never move it without updating `skill_registry.py:337` and the workflow in the same commit. `Docs/BAG_MAN_MASTER_BUILD_v2.0.md` is linter-EXEMPT at `skill_registry.py:197` — moving it silently un-exempts it. | `Tools/AFL_Lint/skill_registry.py:197`, `:337`, `:412` | **VERIFIED-ON-DISK** |
| B7 | **Weapons and balance are DATA, not code.** Designers edit data assets; engineers do not change source for a balance pass. | `BAG_MAN_MASTER_BUILD_v2.0.md:878` **+** `CORE_GAME_CONCEPT.md:2693` *(converged)* | UNVERIFIED |

---

## 7. LANE DISCIPLINE

| # | Law | Source | Verified |
|---|---|---|---|
| L1 | **One layer at a time. Re-verify base control after each change.** | `CLAUDE.md:27` | UNVERIFIED (process) |
| L2 | **Ship vertical slices, not horizontal scaffolding.** Do not build all systems simultaneously; each phase produces a playable, testable artifact. | `BAG_MAN_MASTER_BUILD_v2.0.md:137`, `:1031` **+** `CORE_GAME_CONCEPT.md:2164` *(converged)* | UNVERIFIED |
| L3 | **PER-MAP DESIGN GATE (non-negotiable).** No map enters greybox build until its per-map design brief (`<MapName>_DESIGN.md`) exists and is **operator-approved**. Order: **brief → approval → greybox → telemetry → balance → art → PIE sign-off.** A re-sent brief is not approval; disk state is verified before build. | `CLAUDE.md:95` | **VERIFIED-ON-DISK** — `Arena_02_DESIGN.md` is held at exactly this gate (banked `a241cc49`, Gate still DRAFT) |
| L4 | **AIK owns in-editor asset authoring; Claude Code owns C++/Blender/backend with the editor closed; the Operator owns builds, PIE watches, socket placement and all design rulings.** Never switch lanes silently — flag the lane, why it moved, and why it structurally must. | `IRONICS_PROMOD_CHARACTER_SSOT.md:240-250` | UNVERIFIED (process) |
| L5 | **The agent issues ZERO MCP/editor automation into a running PIE.** The operator controls PIE; the agent reads `Saved/Logs` only after PIE has stopped. | `AGENTS.md:42` | **VERIFIED-ON-DISK** — enforced this session |
| L6 | **One block at a time. Do not stack blocks** before the prior one returns. | `IRONICS_PROMOD_CHARACTER_SSOT.md:250` | UNVERIFIED (process) |
| L7 | **Verify which level is open before any destructive level write.** Assuming the active `.umap` once wiped a finished map. | `feedback-verify-active-level-before-writes`; `Docs/runbooks/AFL_MapArtPass_MasterPlan.md:10` | **VERIFIED-ON-DISK** — every level write this session asserted `world.get_name()` first |
| L8 | **Disk is truth.** Enumerate what is actually present before acting on it; do not assume a skill, asset or system exists because a doc names it. | `CLAUDE.md:43` | UNVERIFIED (process) |

---

## 8. GIT DISCIPLINE

| # | Law | Source | Verified |
|---|---|---|---|
| GD1 | **Push to `personal` (Instawerx/Bag_Man).** That is the live durable remote for the current workflow. | `AGENTS.md:11`, `:62`; operator standing instruction | **VERIFIED-ON-DISK** — `git remote -v` |
| GD2 | **Commit source only. Never commit `Binaries/`, `Intermediate/`, `Saved/`, or DDC.** | `Tools/skills/afl-asset-pipeline` | **VERIFIED-ON-DISK** — `.gitignore` |
| GD3 | **All `.uasset`/`.umap`/binary art goes to Git LFS, no exceptions.** Verify pointer form before pushing — a raw binary in git objects is the failure mode. | `.gitattributes:1-5` | **VERIFIED-ON-DISK** — `*.uasset`, `*.umap`, `*.png` all `filter=lfs` |
| GD4 | **Per-file staging by explicit path.** Never `git add -A` / `git add .` — the working tree routinely holds unrelated in-flight work from other lanes. | operator standing instruction (Blocks 171-180) | **VERIFIED-ON-DISK** — enforced every commit this session |
| GD5 | **Immutable history:** never delete a completed task, never rewrite committed task wording, never renumber IDs. Supersede **forward** — append a correction, strike through the original, never substitute it. | `BAG_MAN_MASTER_BUILD_v2.0.md:14-18`, `:1948` | UNVERIFIED (process) |
| GD6 | **Typos, formatting and broken links may be silently corrected. Anything that changes MEANING gets a new version entry.** | `BAG_MAN_MASTER_BUILD_v2.0.md:22` | UNVERIFIED |
| GD7 | **Never commit parked or inert assets** unless the lane's acceptance proof passes. | `AGENTS.md:65` | UNVERIFIED |
| GD8 | **A commit that banks a design does not approve the build.** Say so explicitly in the message when committing an unapproved brief. | operator ruling, Block 180 | **VERIFIED-ON-DISK** — `a241cc49` |

---

## 9. NAMING

| # | Law | Source | Verified |
|---|---|---|---|
| NM1 | **`AFL` is the permanent internal C++/asset prefix. Only the player-facing title is BAG MAN.** Code does not rename. | `BAG_MAN_MASTER_BUILD_v2.0.md:141`, `:145` | **VERIFIED-ON-DISK** — modules `AFLCore`/`AFLCombat`/`AFLMovement`/`AFLVFX` |
| NM2 | **Class/asset prefixes:** `AFLAG_`/`UAFLAG_` abilities · `GE_AFL_*` effects · `NS_AFL_*` Niagara · `M_AFL_*_Master` / `MI_AFL_*` materials · `DA_AFL_*` data assets · `ED_AFL_*` equipment defs · `ID_AFL_*` item defs · `SK_`/`SM_` meshes · `ABP_`/`CR_`/`IK_` rigs. | `CLAUDE.md:102`, `BAG_MAN_MASTER_BUILD_v2.0.md:228-241` | **VERIFIED-ON-DISK** |
| NM3 | **Tag namespaces:** `State.*` state · `InputTag.*` input · `Cooldown.*` cooldowns · `GameplayCue.*` cues · `Event.*` verbs · `AFL.GamePhase.*` phases · `Data.*` SetByCaller magnitudes. | `CLAUDE.md:104`, `AFLCombatTags.ini` | **VERIFIED-ON-DISK** |
| NM4 | **NEW bridge-produced assets use the `BagMan_` prefix, not `AFL_`.** Narrow exceptions: dismember gibs keep `SM_AFL_`, and the already-shipped `B_AFL_*` shield/weapon family keeps its names (a shipped id is never renamed). | operator ruling; supersedes `MASTER_BUILD:240` | **VERIFIED-ON-DISK** — `SM_BagMan_Extractor_*`, `B_BagMan_CyberBarrier_BR` |
| NM5 | **A shipped ID is never renamed.** Internal codenames and player-facing tile names deliberately differ; renaming a `MapID`/DA/experience re-breaks host resolution. Read the registry, never the filename. | `Docs/reference/MAP_DISPLAY_NAME_REGISTRY.md:44-54` | **VERIFIED-ON-DISK** |
| NM6 | **The shipped artifact is canonical over the doc.** Where a shipped `*Tags.ini` and a spec disagree, the shipped file wins and the spec bends to it. Read the existing tag files before specifying tag names. | `BAG_MAN_MASTER_BUILD_v2.0.md:32`, `:44` | UNVERIFIED |

---

## 10. CONTENT & FEEL STANDARDS

| # | Law | Source | Verified |
|---|---|---|---|
| C1 | **Game feel before content.** A weak shot kills the game; missing skins do not. | `BAG_MAN_MASTER_BUILD_v2.0.md:134` | UNVERIFIED |
| C2 | **No generic reskins. Every weapon needs a distinct visual identity, skill expression and risk/reward profile — if two weapons feel similar, delete one.** | `CORE_GAME_CONCEPT.md:674` *(rescued — unique)* | UNVERIFIED |
| C3 | **Every map has exactly one memorable signature mechanic.** | `BAG_MAN_MASTER_BUILD_v2.0.md:160`, `:565` **+** `CORE_GAME_CONCEPT.md:2081` *(converged)* | UNVERIFIED |
| C4 | **TONE CEILING: chaotic robotic sports violence — NOT gore-heavy.** This is the only stated limit on the dismemberment system and it is load-bearing. | `CORE_GAME_CONCEPT.md:1825` *(rescued — unique)* | UNVERIFIED |
| C5 | **Containment is a correctness requirement, not polish.** If a player can run or fall off the play area anywhere, the map is not playable-AAA. Verify by sampling every reachable surface and asserting the reachable set is inside the boundary — then actively test dash/vault/grenade-jump at every seam. | operator ruling (ARCANEON sealing pass) | **VERIFIED-ON-DISK** — `AFL_MapArtPass_MasterPlan.md`; ARCANEON signed off `c2aa9192` |
| C6 | **Spawn-critical actors must be `ALyraPlayerStart`, not plain `APlayerStart`** — the AFL spawn manager only considers the former; plain starts are invisible to it and every pawn falls back to origin. **In a World Partition map they must also be non-spatial (always-loaded)** or they are not in memory at match start. | `AFLPlayerSpawningManagerComponent.cpp:181` | **VERIFIED-ON-DISK** — both failure modes hit and fixed at `ff28ad98` |

---

## 11. BANKED TRAPS

Each of these cost real time. They are recorded so they are paid for once.

| # | Trap | Source | Verified |
|---|---|---|---|
| X1 | **`unreal.Rotator` positional args are `(roll, pitch, yaw)`, NOT `(pitch, yaw, roll)`.** Always use keyword args. This single bug caused every wall-facing, panel-rotation and ramp-incline defect in the ARCANEON art pass. | operator session record | **VERIFIED-ON-DISK** — `Tools/ArtPass/arcaneon_p3.py` `T()` helper uses keyword args |
| X2 | **Cleanup lives entirely in `EndAbility` (end + cancel).** The looping cue's `OnRemove` is the one place Niagara/audio/shake actually stop. | `CLAUDE.md:83` | UNVERIFIED |
| X3 | **`AddDynamic` is NOT idempotent.** Guard every delegate bind or a double-bind double-counts the event (a death counted twice ends a round early). | `AFLRoundManagerComponent.cpp:643`, `AFLBattleRoyaleComponent.cpp` | **VERIFIED-ON-DISK** |
| X4 | **Population writes need JOIN COVERAGE.** A GameState component that writes state across "everyone who exists" does it once, at a phase edge — anyone arriving later is silently uncovered. Use `UAFLMatchPopulationComponent`'s two stages, and `SetLooseGameplayTagCount` (never `Add`) so overlapping writes are no-ops instead of a refcount of 2 that survives one removal. | `AFLMatchPopulationComponent.h` *(code-only)* | **VERIFIED-ON-DISK** — closed a 5-site bug class |
| X5 | **CACHE BEFORE SWEEPING.** A cached flag that later joiners read must be set *before* the sweep that uses it. | `AFLRoundManagerComponent.cpp:514-518` | **VERIFIED-ON-DISK** |
| X6 | **State applied at match start must be CLEARED at match end.** Leaving `State.Round.NoRespawn` set made an in-session restart begin with carried-over dead participants and let bot-fill over-add. | `AFLBattleRoyaleComponent.cpp` | **VERIFIED-ON-DISK** — fixed `c903af1c` |
| X7 | **`GameFeatureAction_AddComponents` properties read in PascalCase** (`ComponentList`, `ComponentClass`); snake_case returns zero properties. A bridge returning no properties may mean wrong case, not no properties. **`ComponentClass` must be set with a class object, not a `SoftClassPath`.** | `IRONICS_PROMOD_CHARACTER_SSOT.md:283` | **VERIFIED-ON-DISK** — hit live authoring `B_AFL_TeamSetup_Solo` |
| X8 | **Resolve LFS before any blob-content comparison** — HEAD blobs are ~130-byte pointers, not content. | `IRONICS_PROMOD_CHARACTER_SSOT.md:280` | **VERIFIED-ON-DISK** |
| X9 | **A raw file-copy of a `.umap` loads and converts but FATAL-crashes on save** (`Trailer != nullptr`, EditorBulkData). Use a proper `EditorAssetLibrary.duplicate_asset` to re-serialize bulk data. | Block 175 | **VERIFIED-ON-DISK** — crash then success at `c6db6c4d` |
| X10 | **Never duplicate a mesh or skeleton carrying operator-placed sockets**, and **every duplicate must point at its own assets** — a clone left aimed at its source is a live bug class. | `IRONICS_PROMOD_CHARACTER_SSOT.md:289-291` | UNVERIFIED |
| X11 | **Bone names differ per family and per side. Hardcode nothing — read them.** Verify weights on the TWIST bones; base bones read 0 even on a perfect bind. | `IRONICS_PROMOD_CHARACTER_SSOT.md:292` | UNVERIFIED |
| X12 | **Never use the editor bridge to create/duplicate a level or call `new_level`** — it destabilizes the editor (world becomes `None`) or kills it outright. Restart the editor after heavy level ops. | `feedback-ue-bridge-new-level-crash` | UNVERIFIED |
| X13 | **A validator must be shown to FAIL on known-bad input before its pass counts.** *Prevents:* an unexercised validator that returns pass unconditionally, converting "not checked" into "checked and clean" — the most expensive false negative available, because it retires the suspicion that would have found the defect. | `IRONICS_PROMOD_CHARACTER_SSOT.md:274` | UNVERIFIED |
| X14 | **Compare enum VALUES, never stringified names.** *Prevents:* a comparison that silently fails on display-name, namespace-prefix or alias differences and reports a mismatch that does not exist — or, worse, a match that does not exist. | `IRONICS_PROMOD_CHARACTER_SSOT.md:275` | UNVERIFIED |
| X15 | **`reload_packages` silently no-ops on a DIRTY package** — check the save return value first. *Prevents:* reading stale in-memory state while believing it was re-read from disk, so a failed write reads back as a successful one. | `IRONICS_PROMOD_CHARACTER_SSOT.md:276` | UNVERIFIED |
| X16 | **A component reads `None` immediately after `compile_blueprint`** — reload, then read. *Prevents:* concluding a component was not added when it was, and "fixing" it by adding it twice. | `IRONICS_PROMOD_CHARACTER_SSOT.md:277` | UNVERIFIED |
| X17 | **Asset-registry dependency queries return `[]` for freshly-created packages. Scan lag is INDISTINGUISHABLE from "not wired."** *Prevents:* treating an empty dependency result as proof of a missing reference, when it is proof of nothing until the registry has scanned. | `IRONICS_PROMOD_CHARACTER_SSOT.md:278-279` | UNVERIFIED |
| X18 | **A negative result from an UNVERIFIED token is not evidence of absence.** If the search term, tag, path or id was never confirmed to exist in the form searched for, "no results" measures the term, not the repo. *Prevents:* the whole class of confident wrong "it isn't there" conclusions. | `IRONICS_PROMOD_CHARACTER_SSOT.md:281` | **VERIFIED-ON-DISK** — the `ssot/character-system.md` §2.2 address-space finding is this trap in the affirmative |
| X19 | **Writes to a structured array (catalog rows, entry lists) must be WHOLE-ARRAY REASSIGNMENT.** In-place edits to an element wrapper do not persist and revert on reload. *Prevents:* an edit that appears to apply in-session and is gone after a reload, with no error at any point. | `IRONICS_PROMOD_CHARACTER_SSOT.md:288` | UNVERIFIED |
| X20 | **Verify the WIRED asset, not the canonically-named one.** A canonically-named asset can be wired to nothing while a differently-named one is live. *Prevents:* auditing the asset whose name matches the design doc and concluding the system is correct — while the asset actually in use is untouched. Corollary of **NM6**. | `IRONICS_PROMOD_CHARACTER_SSOT.md:293-294` | UNVERIFIED |
| X21 | **Never `new = src` on a struct — it ALIASES, and writing the copy RENAMES THE SOURCE ROW.** Deep-copy every field. *Prevents:* a "duplicate" operation that silently destroys the original it was cloned from. | `IRONICS_PROMOD_CHARACTER_SSOT.md:287` | UNVERIFIED |
| X22 | **Bracket/brace counting cannot validate a file containing prose. Only execution proves structure.** *Prevents:* a syntactic self-check that passes on a broken file (or fails on a sound one) because delimiters inside strings, comments and prose are counted as structure. | `IRONICS_PROMOD_CHARACTER_SSOT.md:282` | UNVERIFIED |
| X23 | **An inherited component CANNOT be deleted in a child Blueprint.** A variant that must REMOVE a component is therefore a duplicate — a **sibling**, not a child — which then triggers **G6**: component-adding actions targeting the source class will not apply to it. *Prevents:* a variant that spawns correctly and is silently missing every component the source class was granted. | `IRONICS_PROMOD_CHARACTER_SSOT.md:299` + **G6** | UNVERIFIED |
| X24 | **A REFERENCE-COUNT COLLAPSE PROVES CONTENT WAS REMOVED, NOT THAT THE REMOVED CONTENT WAS WANTED.** A census measures the *delta*; it cannot tell which side holds the value — the same numbers describe "work was destroyed" and "residue was discarded" equally well. **Establish which state is authoritative from the asset's HISTORY before classifying a shrink as damage.** *Prevents:* reverting a correct discard, or preserving residue, on the strength of a number that cannot distinguish them. Same family as **X18** — there, a negative result measured the search term rather than the repo; here, a delta measures the change rather than its worth. In both, a real measurement is read as an answer to a question it was never capable of answering. | Block 193 triage classified L_Arena_05's **496 → 21** reference collapse as LIKELY-DELETION. The counts were correct; the inference was inverted — HEAD held dead residue from an abandoned build and the working copy was the honest blank. Corrected at `3793b36c` (**R27**). | **VERIFIED-ON-DISK** — the misclassification and its correction are both in this repo's history |
| X25 | **A LEVEL SAVE DOES NOT REACH EXTERNAL ACTOR PACKAGES.** Under World Partition every actor lives in its own `__ExternalActors__` package. `save_current_level()` and `save_dirty_packages()` both return `True` while an edited actor's package keeps its old timestamp — the *level* saves, the *actor* does not. Only `save_packages([Actor->GetPackage()])` writes it. **Verify by file mtime or `git status`, never by the return value.** *Prevents:* reporting an edit as shipped while the bytes on disk are unchanged — and, worse, building the next step on a transform that was never written. Same family as **X15**: the API truthfully reports the operation it performed, not the one you meant. | Block 225 (ShantyTown water volume) and Block 230 (proof marker) — **hit twice**, the second time after the first was already known, which is why it is doctrine and not a block note | **VERIFIED-ON-DISK** — both detected by timestamp comparison, both resolved by explicit `save_packages` |
| X26 | **A BARE `except` AROUND AN EDITOR/BRIDGE CALL IS A SILENT FAILURE.** `try: comp.set_material(...) except Exception: pass` swallowed a failed material assignment; the asset path was wrong and nothing said so. If a call may legitimately fail, catch it and **log the exception**. If it must not fail, let it raise. *Prevents:* converting a loud error into a false green — the one failure mode this project cannot absorb, because it retires the suspicion that would have found the defect. Same shape as **X13**. | Block 229 — the proof marker shipped unstyled and was reported as styled | **VERIFIED-ON-DISK** — `GridMaterial` did not exist at the path used; surfaced only by a later binary/asset readback |
| X27 | **AN AUTHORED `Style` ON A `W_LyraButton_C` IS A SUGGESTION, NOT A SETTING.** `ULyraButtonBase::NativePreConstruct` fires the `UpdateButtonStyle` BlueprintImplementableEvent, and Lyra's `W_LyraButton` answers it with `Set Style = DefaultStyle` (or `ControllerInputStyle`). The per-instance `Style` is overwritten before the first paint. Set **`DefaultStyle`**, or do not inherit from `W_LyraButton`. **COROLLARY — DISK VERIFICATION PROVES PERSISTENCE, NEVER CONSUMPTION.** Before tuning any visual value, read the CONSUMER and confirm the asset you are editing is the one that reaches the renderer. *Prevents:* escalating a visual signal against an asset that is discarded at construct — three widths were written, verified on disk after reload each time, and correct every time, while the surface rendered Lyra's stock `ButtonStyle-Primary-M`. Same family as **X20**: there the canonically-named asset was not the wired one; here the authored property is not the consumed one. | Blocks on the IRONICS lobby axis; cause found by reading `UpdateStyle`'s graph and `DefaultStyle`'s value rather than the style asset | **VERIFIED-ON-DISK** — fixed at `3736a702`, operator-watched: Arc-Violet paints on the first click, both doors |
| X28 | **AUTHOR THE FLAG; NEVER FORCE IT AT RUNTIME.** `SetIsSelectable(true)` immediately before `SetIsSelected` masks the authored value: the WBP can lose the flag and runtime still works, so the drift is undetectable until a second surface without the force-set silently fails. Author `bSelectable`/`bToggleable` in the asset and `ensure` in code. **COROLLARY — `bToggleable=False` MAKES DESELECTION UNREACHABLE.** `UCommonButtonBase::SetIsSelected(false)` is gated on `bToggleable` (`CommonButtonBase.cpp:991`), so a non-toggleable button can be selected but never programmatically cleared. An exclusive axis needs `bToggleable=True` **plus** a re-assert on the handler's no-change path, because CommonUI toggles off before the handler runs and every `Select*` early-returns. *Prevents:* a fix that ships a new defect inside it — every chip that ever lit staying lit forever. | `AFLW_Lobby_Root.cpp` `RefreshAxisSelection`; force-set removed only after the paint was proven | **VERIFIED-ON-DISK** — removed at `5437f3b7`; the replacement `ensure` did not fire on the operator's watch |
| X29 | **`bToggleable` IS HALF A CONTRACT; `bTriggerClickedAfterSelection` DECIDES WHO WRITES LAST.** With it `False` (the default), `HandleButtonClicked` runs `NativeOnClicked()` **then** `SetIsSelected(!bSelected)` (`CommonButtonBase.cpp:1444-1445`) — so CommonUI re-reads whatever your handler wrote and inverts it. Any handler that sets selection state on the button just clicked is silently undone, and the symptom is asymmetric: deselecting a *different* button sticks, selecting the *clicked* button does not, so the choice appears one interaction late. **X29.1 — ASSERT THE OUTCOME, NOT THE ENABLING FLAG.** `bSelectable` is protected with no getter; `ensure(GetSelected() == bSelected)` compiles and is stronger regardless, because it holds whatever the reason the state failed to take. *Prevents:* auditing your own ordering when the handler was already correct. | `CommonButtonBase.cpp:1437-1446`; all twelve axis instances were authored `False` | **VERIFIED-ON-DISK** — flag set at `3736a702`; operator-watched, selection now lands immediately |
| X30 | **AUTO-WRAP CANNOT FIRE INSIDE AN UNBOUNDED SLOT.** `auto_wrap_text=True` on a widget in an `Automatic`-sized slot is inert: the slot grows to the text's desired width, so the text never meets the constraint that would make it fold. Wrap is half a fix — the allocation is the other half. *Prevents:* setting wrap, watching nothing change, and concluding wrap is not the problem. | The lobby tile row: wrap applied to the tiles was inert until the tile slots were changed from `Automatic` to `Fill` | **VERIFIED-ON-DISK** — banked by operator ruling; both halves shipped at `4269592e` / `5437f3b7` |

---

## APPENDIX A — CORRECTED CLAIMS (excluded from doctrine)

These appeared doctrine-shaped in a source but **disk contradicts them.** Recorded so they are not re-adopted.

| Claim | Source | What disk actually says |
|---|---|---|
| **Two-repo model** — an `afl-game` code repo plus an `afl-game-assets` LFS repo | `BAG_MAN_MASTER_BUILD_v2.0.md:248-249` | **FALSE — single repo.** `git remote -v` shows one repo (`Bag_Man`) with two remotes: `origin` (C12-Ai-Gaming) and `personal` (Instawerx). The same document contradicts itself at §1/§14.2, which describe a single repo with LFS migrated in place. |
| **`State.Dashing` / `Cooldown.Dash`** | `BAG_MAN_MASTER_BUILD_v2.0.md:1549`, `:1561` | **FALSE — disk has `State.Movement.Dashing`** in both C++ and the tag `.ini`. The source document itself flags this as stale at `:45`. |
| **`SM_AFL_` / `AFL_` prefix for all new assets** | `BAG_MAN_MASTER_BUILD_v2.0.md:240`, `:2167` | **SUPERSEDED.** New bridge-produced assets use `BagMan_`; narrow exceptions keep `SM_AFL_` (dismember gibs) and `B_AFL_*` (shipped shield/weapon family). See **NM4**. |
| **"No CI exists, so every 'enforced by CI lint' claim is aspirational"** | asserted during the Block 181 doctrine mine | **FALSE — that assertion was itself stale. CI EXISTS.** `.github/workflows/afl-pr-build.yml` + `afl-yolo-pr.yml`, 6 `AFL_Lint` steps, and a PR-failing staleness gate at `:99-100`. The lint doctrine at `MASTER_BUILD:99`, `:228`, `:266` is **TRUE and enforcing** — carried into **B5**. |
| **AFL-XXXX task IDs are unique and immutable** | `BAG_MAN_MASTER_BUILD_v2.0.md:17` vs `:449-474` | **SELF-VIOLATING** — `AFL-0801` is assigned twice (Sprint 8 and Sprint 10). The *law* is retained (**GD5**); the *source document* violates it. |
| **Document version "1.0 · Sprint 0 (pre-kickoff)"** | `BAG_MAN_MASTER_BUILD_v2.0.md:2198` | Contradicts its own v2.2 header at `:2`/`:28`. Footer is stale. |
| **"Don't build your own matchmaking — EOS solves this"** | `CORE_GAME_CONCEPT.md:2353` | **DELIBERATELY NOT CARRIED.** It contradicts the live League/Advancement and Match/Staking tracks. EOS supplies sessions/parties, not division ladders or staked match types. |
| **Folder/naming law** — `/Game/NeonGame/`, `BP_NeonRobotCharacter`, `M_Neon_Master` | `CORE_GAME_CONCEPT.md:334-343`, `:189`, `:308` | **WRONG VOCABULARY — not carried.** The *shape* of the rule (one owned content root; never dump into Lyra folders) is retained; the literal names are pre-project. |

---

## APPENDIX B — PROVENANCE OF THE ARCHIVED SOURCE

`Docs/CORE_GAME_CONCEPT.md` (2888 lines) is **archived** per operator ruling R2. It is an **unratified ChatGPT
transcript**: it ends at line 2889 with `https://chatgpt.com/share/69fd87d1-…`, contains **zero occurrences** of
`AFL`, `IRONICS`, `Bag Man`, `ProMod`, `Watts`, `Volts`, `Haywire` or `ARCANEON`, and landed in one bulk commit
(`3665d15b`). It was never reconciled against the codebase.

Before archival, its doctrine was reconciled against `CLAUDE.md` and `BAG_MAN_MASTER_BUILD_v2.0.md`. **Seven
statements existed nowhere else and were rescued into this document, re-grounded in project vocabulary:**
**A7** (tags not booleans) · **A9** (bot ability parity) · **C2** (no generic reskins) · **C4** (tone ceiling) ·
**N9** (beam replication clause) · **N10** (no P2P hosting) · **N12** (P2P trade safety) · plus **G8** (service
separation). Four were already covered and were converged rather than duplicated: ASC-on-PlayerState, never-trust-
clients, one-mechanic-per-map, vertical-slices.

---

## APPENDIX C — OPERATOR RULINGS BINDING THIS DOCUMENT

| # | Ruling |
|---|---|
| R1 | **MELEE IS CUT.** The game is **dual-mode**, not tri-mode. `IRONICS_MELEE_RULESET_SSOT.md` is superseded. `IRONICS_PROMOD_CHARACTER_SSOT.md` L3 wins over `IRONICS_GAME_MODES_SSOT.md` on this point. |
| R2 | `CORE_GAME_CONCEPT.md` is **ARCHIVED** — see Appendix B. |
| R3 | The Tier-3 tracker is **REBUILT from git log**, carrying forward only relevant notes. Not a merge of the five existing trackers. |
| R4 | Both loot docs merge into `ssot/economy-store.md`; `IRONICS_LOOT_CARRY_MODEL.md` v7 is archived **verbatim** as a decision record. |
| R5 | **FBIK is permanent doctrine** — all characters, current and future, on the rigged Manny/Quinn bases (`8367f0b8`). Carried as **A11**. |
| R6 | **Lyra extension is an ORDER OF PREFERENCE, not a prohibition:** ① clone-and-harvest into AFL modules where possible · ② patch Lyra core where linkage/engine structure requires it — the correct fallback, not a violation · ③ **upstream mergeability is NOT a constraint** (engine frozen at revision; AAA-best-option overrides upstream purity permanently) · ④ **pruning** of unused Lyra content happens at the END of the project. Carried as **G1**, **G2**, **G9**. |

---

## HOW TO CHANGE THIS DOCUMENT

1. A law changes only with an operator ruling. Record the ruling in **Appendix C**.
2. **Never delete a law.** Supersede it forward: strike it, add the replacement, cite both (**GD5**).
3. When disk contradicts a law, that is a **finding** — move it to **Appendix A** with what disk says. Do not
   silently drop it.
4. A statement that is a *status claim* does not belong here at all. It belongs in the Tier-3 tracker.
