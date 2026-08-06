# ShantyTown — WATER, SWIM & DEGRADATION (design + scope)

**What this is:** the implementation scope for traversable water on ShantyTown, produced by a read-only
survey of source and assets. It carries **design and phasing**, which is why it lives in `Docs/design/` and
not in `Docs/ssot/`.

**What this is not:** a Tier 2 SSOT, and not a status board. **No claim here says anything is built.** Status
lives in `Docs/LIVE_TRACKER.html`; the laws cited live in [`Docs/DOCTRINE.md`](../DOCTRINE.md); the systems
this touches are `ssot/ai-bots.md`, `ssot/match-modes.md` and `ssot/map-build-system.md`, **cited, not
restated**.

**The ruling this exists to serve — R32:** *water is NOT lethal. It is traversable and playable. Entering
water starts a timer; health then degrades.* That supersedes the depth-lethality rule in
[`ShantyTown_BR_DESIGN.md`](ShantyTown_BR_DESIGN.md) §4 and the water half of its containment layer.

---

## 1. THE THREE FINDINGS THIS DOCUMENT EXISTS TO PRESERVE

Everything else here is design. **These three are facts that were expensive to establish and would otherwise
be rediscovered.**

### 1.1 THE GITIGNORE BLOCKER — the animations are unshippable today

```
.gitignore:125   /Content/Fab/                    →  0 tracked files
.gitignore:126   /Content/FreeAnimationLibrary/   →  0 tracked files
```

**Every swim animation this feature would use is gitignored and untracked.** Worse, the FreeAnimationLibrary
set targets `/Game/FreeAnimationLibrary/Demo/Characters/Mannequins/Meshes/SK_Mannequin` — **the pack's own
decoy skeleton, inside the ignored folder** — not the tracked `Characters/Heroes/Mannequin/` rig. The Motifect
sequences each carry their own `_Skeleton` asset and are not on the shared rig either.

**This is the THIRD instance of one pattern:**

| # | Instance | Ignored root |
|---|---|---|
| 1 | Parkour IK + retarget pair (still open) | `.gitignore:172` `/Content/ParkourAnimations/` |
| 2 | ShantyTown pack decoy blueprints and the absent `Landscape_Layers/` | (pack duplicates, not gitignore) |
| 3 | **These swim animations** | `.gitignore:125` and `:126` |

> **The pattern, stated so it is recognised on the fourth occurrence:** a marketplace pack ships its **own**
> copy of the mannequin skeleton and mesh. Assets authored against the pack resolve to the pack's copy, which
> looks correct in the editor and is invisible in a fresh clone. **Referencing a pack asset directly is
> always a clean-clone break.**

**RESOLUTION — retarget onto the tracked rig and bank the outputs.** Conform exactly to the **11 tracked
`*_Retargeted` assets** under `Content/Characters/Heroes/IRONICS/Animations/Retargeted/`, which are the
working precedent: tracked, clean, on the tracked rig, with zero references to any ignored pack. The pack is
then an **authoring-time** input only, and does not need un-ignoring.

### 1.2 THE CORRECTED GE PRECEDENT — do not reach for BeamTick

**`GE_AFL_Damage_BeamTick` and `GE_AFL_Damage_CutterTick` are the WRONG shape for this.** Both are
`EGameplayEffectDurationType::Instant`, applied repeatedly by an ability's own fire timer. They model *a
weapon firing repeatedly*, not *a condition persisting*.

**Water degradation is presence-driven: it starts on entry, ticks while present, and stops on exit.** The
right precedents are already in `AFLCombat`:

| Precedent | What it contributes |
|---|---|
| **`AFLGE_InExtractionZone`** | `Infinite`, and its comment states the shape exactly: *"presence has no fixed duration — lifetime is owned entirely by the zone"*. This is the enter/leave contract |
| **`AFLGE_OverdriveBuff`** | `Infinite` + `Period = 1.0` + `bExecutePeriodicEffectOnApplication = false` + SetByCaller magnitude, with the handle owned by a component |
| **`GE_AFL_Heat_Decay`** | `Infinite` + `Period` — the periodic-execution pattern at a higher rate |
| **`GE_AFL_Damage_BeamTick`** | Contributes **only** its ExecCalc routing: an `FGameplayEffectExecutionDefinition` with `CalculationClass = UAFLDamageExecCalc` |

**Recommended shape:** `Infinite` · `Period` (≈1 s) · `bExecutePeriodicEffectOnApplication = false` so the
first tick lands after a grace period rather than on contact · magnitude via **SetByCaller** so the owning
component sets the rate · damage routed through **`UAFLDamageExecCalc`**, never a direct health write (**A4**).

**Why this is recorded rather than left to judgment:** "conform to the existing periodic damage effect" points
at BeamTick by name, and BeamTick is not periodic. Its own header comment records that it *already* shipped
one bug of this class — a direct meta-attribute write that logged damage while health never moved. Reaching
for it here would import the wrong duration model into a system where the duration **is** the mechanic.

### 1.3 THE WORLD PARTITION REQUIREMENT — one always-loaded volume

> **ONE always-loaded water volume. Never several spatially-loaded ones.**

Two distinct failures, both silent:

- **A spatially-loaded volume that streams out mid-swim** drops the player from `MOVE_Swimming` into falling —
  **inside water geometry**, with no volume to re-enter. The player is not told; they simply begin drowning in
  a way nothing explains.
- **Several volumes create seams at their boundaries.** A player crossing from one to the next momentarily
  belongs to neither and exits swim for a frame.

**This is the same failure class as C6** — spawn-critical actors in a World Partition map must be non-spatial
or they are not in memory when needed. That law was earned on **this exact map**. A water volume has the same
property: it must exist for the whole play space at all times, or it is worse than absent, because its absence
is intermittent.

**Corollary:** the volume is sized to the **swimmable band** (§5), not to the 16 km × 16 km visual plane. The
plane is what the player sees; the volume is where the game happens.

---

## 2. THE WATER VOLUME

### 2.1 Recommended: `AAFLWaterVolume : APhysicsVolume`

UE engages `MOVE_Swimming` when a pawn overlaps an `APhysicsVolume` with `bWaterVolume = true`. **Zero such
volumes exist in the level today** — verified across all 639 external actor packages. The water is a
`StaticMeshActor` plane with no physics representation at all.

| Option | Trade-off |
|---|---|
| Hand-placed `APhysicsVolume` | Trivial to make, but `bWaterVolume` and the non-spatial flag are **placement properties an operator can forget** — and forgetting either is §1.3 |
| Water plugin body | Available and enabled. Brings buoyancy, surface rendering and underwater post-process — see §2.2 |
| **`AAFLWaterVolume : APhysicsVolume`** ⭐ | `bWaterVolume` and non-spatial loading become properties of the **TYPE**, set in the constructor. Carries the degradation knobs as `EditAnywhere` properties |

**Recommend the AFL class.** The two things that must never be wrong stop being things anyone has to remember.
This follows the deployables' precedent — lean actors carrying their own tuning knobs (**A10**) — and gives
the operator **one** thing to place rather than a volume plus a separate degradation trigger that could be
placed inconsistently with it.

### 2.2 The Water plugin is ENABLED — and deliberately not used

`Bag_Man.uproject` carries `"Water": Enabled = true`. It is not the recommendation here, for one reason:

> **A Water body would replace the existing 16 km static-mesh plane and its material — turning a gameplay task
> into an art change.**

The plane is authored content with an authored look. Swapping it for a Water body means re-establishing the
visual, and it drags in water zones whose interaction with World Partition streaming is more complex than the
single always-loaded volume §1.3 requires. **Recorded as available for a later art upgrade**, on its own
schedule, with its own sign-off. Also noted: a `PostProcessWaterEffect` actor already exists in the level, so
some underwater post-process may already be present.

---

## 3. THE MOVEMENT MODE — P-CONTROLS HOLDS

**No CMC subclass and no reparent is required.** The engine provides `MOVE_Swimming` natively once
`bWaterVolume` is true; nothing needs writing to make swimming happen.

| Concern | Where it goes |
|---|---|
| **Tuning** — `MaxSwimSpeed`, `Buoyancy`, `OutofWaterZ` | CDO defaults on the existing `UAFLCharacterMovementComponent`. These are currently **engine CDO defaults, not authored values** — nothing in that component mentions swim, buoyancy or water |
| **Entry/exit detection + the degradation handle** | A GameFeature-attached `UActorComponent` reading `GetCharacterMovement()->IsSwimming()` |

**That second row is P-CONTROLS satisfied exactly**, and it conforms to the existing family —
`AFLSprintMovementComponent`, `AFLSlideMovementComponent`, `AFLWallRunMovementComponent` all attach the same
way and read the stock CMC rather than replacing it.

**Module:** `AFLMovement`. **Lane:** C++ editor-closed, then an editor pass to attach via
`GameFeatureAction_AddComponents`.

**Open within this piece:** camera behaviour when submerged, and whether surface and submerged are one state
or two.

---

## 4. THE ANIM STATE

**Seven usable sequences (R35).** The other nine matching assets are skeletons, physics assets and skeletal
meshes — not animations.

| Have | Missing |
|---|---|
| `anim_SwimIdle` · `anim_Swim_Surface_Fwd` · `_Left` · `_Right` | **Entry** (dive / jump-in) |
| `swim_backstroke_Anim` · `swim_breaststroke_Anim` · `swim_freestyle_Anim` | **Exit** (climb-out / vault to land) |
| | **Submerged** idle and forward |
| | **Tread-water** |
| | **Turn-in-place** |

All seven are subject to §1.1 and must be retargeted before use.

### 4.1 Where the state belongs — the AFL layer, not the shared base

The locomotion state machine lives in **`ABP_Mannequin_Base`**, which drives **every character in the
project**. Editing it to add swim has the blast radius that `ssot/ai-bots.md` §3.4 records for the shared rig:
a regression appears on unrelated characters, in unrelated modes, and is found by someone who did not make the
change.

**Add swim at the AFL layer**, per **G1**'s order of preference — clone-and-harvest first, patch the shared
asset only where structure genuinely requires it. Keep the addition **additive**, exactly as the FBIK solve is
layered rather than re-pinned.

---

## 5. CONTAINMENT — WHAT REPLACES THE LETHAL SEAL

R32 removes the self-sealing boundary the BR brief relied on. Water is now traversable, so it no longer
contains the player by killing them.

| Option | Trade-off |
|---|---|
| Distance-scaled degradation alone | Readable and diegetic, but a full-health player can still cross — no guarantee, and **C5** demands one |
| Hard blocking volume beyond a swimmable band | Guaranteed, but a wall in open water reads as broken |
| BR-ring soft edge | Already scoped as the Zone system; wrong for non-BR rulesets and would duplicate it |

**RECOMMENDED: a swimmable band with distance-scaled degradation, backed by a blocking volume placed beyond
any survivable swim.**

- **The degradation is what the player experiences and learns.** It makes the water edge a risk decision with
  a readable gradient rather than an invisible fence.
- **The blocking volume is a correctness backstop they never reach alive.** **C5** makes containment a
  correctness requirement, not polish — and a mechanic that *usually* contains is not containment.

**The tuning target, stated as the design constraint:** *survivable near shore, unsurvivable far out.* A flat
rate cannot express that — it makes shallow water either free or lethal with nothing in between, which is the
"wall or escape" failure the whole design is trying to avoid.

---

## 6. BOTS — OPPORTUNISTIC, NOT BLOCKING (R34)

> **R34: bots are a temporary population measure until real player counts support matches. Gameplay and
> character capability are never limited to keep bots viable.**

Navmesh does not cover water, so bots cannot enter it. Under R34 that is **accepted**, not a blocker.

- **If a nav modifier is cheap once swim works, add it.** A swimmable nav area with a cost weighting would let
  bots path through water while preferring land — mirroring how a player treats it.
- **Otherwise bots stay on land and that is accepted.**

**This is a deliberate, recorded exception to `ssot/ai-bots.md` §8.2** (a bot must be indistinguishable from a
player to match logic). It is recorded there as a carve-out with its reasoning — an undocumented exception
reads as a defect to whoever finds it next.

**The cost is understood and accepted:** while bots cannot swim, water is a bot-free space, and in a
bot-populated match standing in it is advantageous. R34 accepts that because bots are transitional.

---

## 7. PHASED PLAN

Each phase is independently PIE-provable, smallest provable slice first.

| # | Phase | Lane · editor | Operator watches | Falsifiable assertion |
|---|---|---|---|---|
| 1 | `AAFLWaterVolume` + one placed non-spatial volume | C++ closed → editor open | Walk into water | Character enters `MOVE_Swimming`. **Fails if** the mode never changes |
| 2 | Swim tuning on the CMC CDO | Editor open | Swim around | Speed and buoyancy are authored values, not engine defaults |
| 3 | **WP streaming proof** | Editor open | Long traverse across cell boundaries | **Swim state never drops.** Fails if the player falls out of swim mid-traverse |
| 4 | Degradation component + GE | C++ closed → editor open | Enter water, wait, leave, re-enter | Health ticks on a period, stops on exit, resumes on re-entry; death routes through the normal path |
| 5 | **Retarget swim anims, bank outputs** | AIK / Blender, editor open | — | Retargeted assets are **tracked**, on the tracked rig, zero references to ignored packs |
| 6 | Swim anim state at the AFL layer | Editor open | Swim | Character animates swimming; **stock/Haywire characters unchanged** |
| 7 | Nav area + cost modifier *(opportunistic, R34)* | Editor open | Bot follows a player into water | Bot enters, swims, takes degradation, dies through the same path — **or is skipped** |
| 8 | Containment band + backstop | Editor open | Swim outward | Survivable near shore, unsurvivable far out, blocking volume never reached alive |

**Phase 3 must not be reordered or skipped.** It is cheap, and it is the only failure here that corrupts a
match rather than merely looking wrong. This map has already been bitten once by the non-spatial requirement.

**Phase 5 gates phase 6.** It is the only phase with a hard external dependency — the source animations are
currently unshippable (§1.1).

---

## 8. OPEN QUESTIONS

1. **Uniform rate or distance-scaled?** §5 recommends distance-scaled, because the design constraint
   (survivable near shore, unsurvivable far out) cannot be expressed by a flat rate. Not yet ruled.
2. **On exit: pause or reverse?** Pausing makes water damage cumulative across a match and turns repeated
   short crossings into an attrition mechanic. Reversing makes water a fully recoverable route and reduces the
   decision to a timing question. **Recommend pause-and-hold** — the risk should persist, or crossing carries
   no memory — but this changes how the whole system feels and is owed.
3. **Does the rate differ by ruleset?** A water death is elimination in SHOOTOUT and a few seconds in TURBO
   (`ssot/match-modes.md`), so an identical rate is a far larger commitment in SHOOTOUT.
   **Recommend one rate, letting the stakes differ naturally** — that keeps one mechanic players learn once,
   and matches the principle that the rulesets differ in win condition and respawn rather than in physics.
   **Flagged as owed.** TURBO has no code, so this can be deferred without blocking.
4. **Surface versus submerged** — one movement state or two, and what the camera does when submerged (§3).
5. **The grace period before the first tick.** `bExecutePeriodicEffectOnApplication = false` gives one free
   period; whether that is the right grace, or whether a longer explicit one is wanted before degradation
   begins, is untuned.

---

## 9. RELATED

- [`Docs/DOCTRINE.md`](../DOCTRINE.md) — **A4** no ability writes health directly · **A10** a deployable is a
  lean actor carrying its own knobs · **C5** containment is a correctness requirement · **C6** WP actors that
  must always be present are non-spatial · **G1** clone-and-harvest before patching a shared asset ·
  **N1** server authority.
- [`ShantyTown_BR_DESIGN.md`](ShantyTown_BR_DESIGN.md) — the map brief. Its §4 depth-lethality rule and the
  water half of its §3B containment layer are **superseded by R32**.
- `ssot/ai-bots.md` — §8.2, and the R34 carve-out recorded against it.
- `ssot/match-modes.md` — the two rulesets whose stakes differ in §8.3.
- `ssot/map-build-system.md` — the play-space sizing the swimmable band is measured against.
