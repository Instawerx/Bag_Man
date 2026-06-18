# IRONICS — Loot-Carry Model (collect-vs-carry-object split) — v3

Finalized decision record. **v3 amendment (below) adds the PHYSICAL-OBJECT LIFECYCLE layer and records
Phase A as SHIPPED with an architecture pivot.** v2's 3 locked operator decisions are still in force.
Governs how collected loot is held (and Phase-4 harvest too).

**v3 amendment (2026-06-18):**
- **Phase A SHIPPED (`4d4cba28`) — the value layer pivoted to a WALLET-RAIL int, not inventory.** PIE proved
  Lyra inventory is *discrete-instance-count* (`GetTotalItemCountByDefinition` counts instances, never sums a
  stack count — `LyraInventoryManagerComponent.cpp:233`). Per `afl-cpp-lyra-developer` +
  `lyra-skin-builder-marketplace`, fungible currency-like value belongs in a **wallet-rail replicated `int32`**
  (the at-risk twin of `UAFLWalletComponent`), **not** an inventory item. `UAFLLootCarryComponent` ships this;
  `ID_AFL_Loot` was dropped. So the **`ID_AFL_Loot` / `AddItemDefinition` framing in STEP 1.1-1.2, "The
  inventory cache," and the Phase-A bullet is the decision trail — superseded by the shipped int.** The
  value-layer *behavior* (collect→pool, portion-on-hit, remainder-on-death, extract→Watts) is unchanged and
  PIE-proven; only its *storage* changed (int rail, not inventory stack).
- **NEW: the PHYSICAL-OBJECT LIFECYCLE layer (STEP 2B below).** A loot object has a visible physical presence
  *distinct from its value*: collected loot HIDES its physical form (invisible-in-cache); scattered loot
  REAPPEARS as a recoverable physical form. **Decision 1: dismember loot reappears as the LIMB MESH** (a
  severed-limb gib), not a generic orb. The generic hide/respawn hook lands in **Phase B** (mechanism); the
  dismember-specific limb-mesh form lands in **Phase C** (the dismember migration supplies the form).

**Locked decisions (this revision):**
1. **Fungible value-item.** The cache is a single `ID_AFL_Loot` (value + stack) — collected loot adds to a
   carried-Watts-value pool; the **wallet = banked-safe**, the **cache = carried-at-risk**, **extract = the
   bridge**.
2. **Portion-on-hit, remainder-on-death.** A confirmed hit scatters a **portion** (~25-50% start, CVar)
   of the carried pool as recoverable world loot; **death scatters the full remainder.** Principle locked,
   number deferred (playtest).
3. **CARRY keeps a short collect-channel.** CARRY is now a brief **stand-and-channel** (interruptible —
   damage / moving cancels it, exposing you), not hand-occupy. INSTANT stays walk-over-instant. So
   INSTANT-vs-CARRY stays meaningful (channel friction, not occupied hands).

Prior-pass decisions still in force: design this before Phase-4 harvest; collected loot → regular Lyra
inventory (read-confirmed wired); drop-on-damage is the risk mechanic.

---

## STEP 1 — the reads (what exists + what's active)

**1. Lyra inventory — ACTIVE.** `ULyraInventoryManagerComponent` is added to the **Controller** by
`LAS_ShooterGame_StandardComponents` (in `B_Experience_BagMan`'s ActionSets: SharedInput / **StandardComponents**
/ StandardHUD; `ShooterCore` in GameFeaturesToEnable). BagMan **weapons are inventory items**
(`ID_BagMan_PulseCarbine` / `Beam`, `LyraInventoryItemDefinition`); the loadout path (`AFLAG_GrantLoadout`)
does `Controller->FindComponentByClass<ULyraInventoryManagerComponent>()` → `AddItemDefinition(ItemDef, count)`.
`AddItemDefinition` is C++-exported; `AddItemToSlot`/quickbar is BlueprintCallable-only (thin BP event).

**2. IPickupable — bridge exists but discovery-gated.** `ALyraWorldCollectable : IInteractableTarget,
IPickupable` → `GetPickupInventory() → FInventoryPickup → inventory`, **but reached via the
`GA_Interact`/`WaitForInteractableTargets` discovery that's ShooterExplorer-demo-only in BagMan.** So the
inventory is live; the IPickupable *world-pickup* flow is not. We drive `AddItemDefinition` directly.

**3. Current hand-attach/carry-pose (what LOOT leaves).** `UAFLGrabbableComponent` + `GA_AFL_Grab` →
`UAFLInteractionComponent::GrabActor`: attaches to `hand_r`, plays a carry-pose montage, holsters the weapon.
Hand-occupied — stays only for *map objects*.

**4. Damage hook — ALREADY WIRED.** `UAFLDamageExecCalc` broadcasts `Event.Damage.Confirmed` (server,
`EffectiveDamage>0`); `UAFLInteractionComponent::HandleDamageConfirmed` already force-drops the hand-carried
object on a confirmed hit. The cache scatter hooks the **same** signal. (Death: `ULyraHealthComponent`
`OnDeathStarted`, already used by the head loot-box's owner-death-vanish.)

**5. Dismember branch.** `OnGrabbedBy` → owner (controller-match) → `RestoreZone` reattach (**no Watts**);
enemy → `EarnWattsAuthority(+value)`. The owner-branch is **"restore my body," not loot.**

---

## STEP 2 — the model

### Two kinds of grabbable (de-conflated)

| | **COLLECT (loot)** | **CARRY-OBJECT (map object)** |
|---|---|---|
| Destination | the **inventory cache** (fungible `ID_AFL_Loot`) | the hero's **`hand_r`** (existing grab) |
| Hand-occupied? | **No** — move + shoot | Yes — carry-pose, weapon holstered |
| Friction | INSTANT = walk-over; **CARRY = short collect-channel** | n/a |
| At-risk? | **Yes** — portion-on-hit, remainder-on-death | (existing per-object `bDropOnDamage`) |
| Banked when | **extract** (→ wallet Watts) | n/a |

### The inventory cache — FUNGIBLE (decision 1)
Collected loot adds to a **single fungible `ID_AFL_Loot`** `LyraInventoryItemDefinition` carrying an
`InventoryFragment_AFLLootValue` (a **Watts-equivalent value**, held as the item's **stack count**). One
stack = the carried-at-risk value pool. Driven by `AddItemDefinition(ID_AFL_Loot, value)` (the proven
loadout API), **server-auth**, no discovery. Loot-typed so drop/extract act on it only — never the weapon.
The hero is **not hand-occupied**. **Wallet = banked/safe; cache = carried/at-risk; extract = the bridge.**

### Routing (collect vs carry-object)
Extend the proven `UAFLGrabbableComponent` with `EAFLGrabKind { CarryObject (default), CollectLoot }`.
`CarryObject` (default) = today's hand grab, so every existing map-object grabbable is **untouched**.
`CollectLoot` routes to the collect path (channel-or-instant → add to the fungible cache + consume).
One enum on the existing component; no second interaction system.

### CARRY collect-channel — NEW mechanic (decision 3)
CARRY is a brief **timed, interruptible channel**, not hand-occupy:
- Walk up → **start channel** → stand/hold for `ChannelDuration` (tune-able, ~1-2 s start) → **complete**
  → loot to the cache.
- **Interrupts (cancel, no loot, expose you):** taking a confirmed hit; moving beyond a small radius from
  the start point. (Open: should *firing* interrupt? — default no; you can shoot while channeling, but a
  hit cancels.)
- **Feedback:** a channel progress indicator (HUD radial / bar over the object). INSTANT = no channel
  (walk-over, instant).
- This restores the INSTANT-vs-CARRY distinction as **channel friction** (you stand exposed briefly for the
  higher-value cache) instead of occupied hands.

> **HARVEST cousin (forward-link, decision-1 intent).** This collect-channel is the same shape as Phase-4
> **harvest-over-time** — *stand-and-do-something, interruptible, with progress feedback.* **Opportunity:
> a shared `UAFLChannelComponent`** (start / tick / complete / interrupt + the progress broadcast) that
> **both** the CARRY-channel and HARVEST consume — so Phase 4 builds on it instead of reinventing.
> Not built this pass; flagged so Phase B authors the channel substrate *generically* with Phase 4 in mind.

### Drop-on-damage — portion-on-hit, remainder-on-death (decision 2)
A new **`UAFLLootCarryComponent`** (pawn/PlayerState, server-auth) owns the carried pool's at-risk behavior:
- **On `Event.Damage.Confirmed` (carrier hit):** scatter a **portion** — `afl.Loot.DropPercent` (default in
  ~25-50%) of the current carried value — as recoverable world loot. A brief grace/cooldown so one burst
  isn't N separate scatters.
- **On `OnDeathStarted` (carrier dies):** scatter the **full remainder** of the carried value.
- **Scatter spawn (from a fungible pool):** quantize the dropped value into **K recoverable value-pickups**
  (e.g. ~50-value chunks → 100 dropped = 2 pickups) on the **proven `AAFLLootCacheInstant` overlap-collect
  substrate** (walk-over, INSTANT). **Recoverable by ANYONE** — including the original carrier if they
  survive and walk back (it's the risk: your loot is on the ground, contestable). Each scattered pickup,
  when collected, adds its value to the collector's *own* carried-at-risk cache (the loop closes).

### Extraction (the bank)
The existing extract sink (`AAFLExtractionZone` / the extract ability) **converts the carried `ID_AFL_Loot`
stack → wallet Watts** (`EarnWattsAuthority`) and clears it. The safe→banked transition; until then the loot
rides at-risk. (This replaces "Watts on grab" with "Watts on extract.")

### Economy tie
The model **is** the risk/reward axis: collect freely + keep moving, but everything carried is **at-risk**
until extract — bleed a portion per hit, lose the rest on death. The old hand-occupy risk is replaced by
drop-on-damage risk + the CARRY channel-exposure. Deterministic values throughout (no rolled acquisition).

---

## STEP 2B — the PHYSICAL-OBJECT LIFECYCLE (the visible-object layer)

Phase A proved the **value** layer (collect→pool, scatter, extract). This is its pair: a loot object also has
a **physical presence** on screen, distinct from its value, with its own lifecycle.

### The lifecycle
- **Collect** → value to the carried pool (Phase A, proven) **AND hide/despawn the physical presence** — the
  object is now *in the cache*, invisible (not in hands, not trailing the carrier).
- **Scatter** (portion-on-hit / remainder-on-death) → **respawn a physical RECOVERABLE form** carrying its
  value, on the ground, collectable by anyone.
- **Re-collect** → hides again. The physical form tracks the value: visible on the ground, hidden while pooled.

This mirrors the **proven head loot-box** (`BP_AFL_HeadLootBox` + the dismember `RestoreZone`-style cue): a
persistent object whose mesh is hidden/shown by a server-auth state, *not* destroyed-and-rebuilt — the
hide/respawn precedent to cite.

### The generic HOOK (Phase B's domain — the MECHANISM)
`UAFLLootCarryComponent` exposes a **generic physical-representation hook**: collect fires *"hide this loot's
physical form,"* scatter fires *"spawn this loot's physical recoverable form."* The component owns the
**mechanism** (hide-on-collect / respawn-on-scatter, tied to the value pool); it does **not** know what the
form *is*. **The consumer supplies the form per loot-type** — the same component-owns-mechanism /
consumer-supplies-specifics generalization the loot abstraction uses throughout (`IAFLLootable`,
`OnOwnerRetrieved`, the director config).

### Per-type physical forms
| Loot type | Physical form | Collect | Scatter | Phase |
|---|---|---|---|---|
| **Cache loot** | `AAFLLootCarryPickup` (the pickup actor) | actor **destroyed** | component **spawns** K pickups | **A (done)** / B |
| **Dismember loot** | the **LIMB GIB MESH** (`SM_AFL_Robot{Arm,Leg}_Gib` / head gib) | **HIDE** the gib mesh (value→pool, mesh invisible) | gib **reappears** as a recoverable severed limb | **C** |

- **Cache loot** (Phase A/B): the form is the pickup actor — already destroys-on-collect / respawns-on-scatter
  (Phase A proven). **No change**; it's the generic hook's default form.
- **Dismember loot** (Phase C, **decision 1**): the form is the **limb gib mesh** (arm/leg/head). Collect
  **hides** the gib mesh (the value goes to the pool; the *mesh* doesn't destroy — it hides, like the head
  loot-box hides bones via the cue). Scatter makes it **reappear as a recoverable limb gib** — it reads as a
  *severed limb on the ground*, **not a generic orb**. Re-collect hides it again.

### Phase placement (build the hook in B *knowing* C needs limb-mesh)
- **Phase B** builds the **generic hide/respawn hook** (the mechanism) into `UAFLLootCarryComponent`, with the
  cache pickup as the default form but the hook **parameterized** so a consumer can supply a different physical
  form. Don't hardcode the cube pickup as the only form.
- **Phase C** (dismember migration) **supplies the LIMB GIB MESH** as the physical form for dismember loot,
  wiring collect→hide-gib / scatter→show-gib through the Phase-B hook.

---

## Migrating the SHIPPED loot (re-watch, not rewire-beside)

| Shipped thing | Today | Migrates to | Care |
|---|---|---|---|
| **CARRY cache** (`AAFLLootCacheCarry`) | hand-occupied grab → **+200 Watts on grab** | `CollectLoot` + **collect-channel** → cache (at-risk); Watts on **extract** | **HIGH** |
| **INSTANT cache** (`AAFLLootCacheInstant`) | walk-over → **+50 Watts** | overlap-collect → cache (value, not Watts) | MED |
| **Head/limb loot-box** (enemy) | hand-occupied grab → **+Watts instant** | `CollectLoot` → cache (**carried-at-risk**, see change below) | **HIGH** |
| **Head/limb loot-box** (owner) | grab → `RestoreZone` reattach | **UNCHANGED** | — |
| Map objects (stress object, throwables) | hand-occupied carry | **UNCHANGED** (`CarryObject` default) | none |

### ⚠ INTENTIONAL BEHAVIOR CHANGE — enemy dismember-loot is now CARRIED-AT-RISK
Under the fungible cache, the resolved fork ("enemy-retrieved head/limb → inventory cache") **changes
shipped behavior**: an enemy who grabs a severed **head no longer instant-banks +160**; the head's value
(160 / limb 20) is **added to their carried-at-risk pool** — droppable on damage, lost on death, banked
only on **extract**. This is arguably *better* (the killer must survive + extract the head's value, at
risk — the head is a prize they have to carry out), but it **is a change to proven Phase-3 behavior**, so
it is **stated here and re-watched in Phase C, not silent.** The **owner-reattach branch is unchanged**
(body-restoration, never inventory; controller-match still routes it).

---

## STEP 3 — phased build plan (each ends watched; ⚠ = re-touches SHIPPED code)

**C++** = build-gated (operator-closed build); **data/BP** = editor.

- **Phase A — the cache loop in isolation (no shipped code).** [C++ + data]
  `ID_AFL_Loot` + `InventoryFragment_AFLLootValue`; `UAFLLootCarryComponent` (server-auth):
  `Collect(value)` → `AddItemDefinition`; `ScatterPortion` on `Event.Damage.Confirmed`; `ScatterAll` on
  `OnDeathStarted`; `ExtractAll()` → `EarnWattsAuthority` + clear. + the **INSTANT walk-over collect**.
  Prove on a **new test collectible**: collect → cache grows; confirmed hit → portion scatters as
  recoverable pickups; death → remainder scatters; extract → Watts banked + cache cleared. *No shipped code.*
- **Phase B ⚠ — routing + the CARRY collect-channel + the physical hook + migrate the caches.** [C++ + data]
  `EAFLGrabKind` on `UAFLGrabbableComponent` (default `CarryObject`). Author the **`UAFLChannelComponent`**
  (generic, HARVEST-ready) + the CARRY collect-channel. Author the **generic physical-representation hook**
  (STEP 2B — hide-on-collect / respawn-on-scatter, *parameterized* form, cache pickup as default, **knowing
  Phase C supplies the limb mesh**). Migrate **INSTANT + CARRY** caches to `CollectLoot` (INSTANT walk-over,
  CARRY channel → cache, not Watts). **Re-watch the Phase-3 cache gates** + the new channel + drop/extract;
  confirm **map-object grab still hand-occupies** (the stress object).
- **Phase C ⚠ — migrate the dismember enemy-collect + the LIMB-MESH physical form.** [C++]
  Enemy head/limb → `Collect` (the carried-at-risk pool); **owner-reattach unchanged**. Supply the **limb gib
  mesh** as the physical form through the Phase-B hook: collect → **hide** the gib mesh; scatter → the gib
  **reappears as a recoverable severed limb** (decision 1, not a generic orb). **Re-watch:** enemy head/limb
  value is now **carried-at-risk, not instant-banked** (the stated behavior change) + the limb mesh
  hides-on-collect / reappears-on-scatter; owner reattach still works.
- **Phase D — tuning + the recovery loop.** [data/CVar + watch]
  `afl.Loot.DropPercent` (the 25-50% pass) + the grace/cooldown; **2-client** scatter-recovery (another
  player collects your dropped loot); balance pass → write the carried-loot values into
  `IRONICS_ECONOMY_SPEC.md` (same as head/limb 160/20).
- **(Phase-4 HARVEST — later.)** Harvest node uses the **shared `UAFLChannelComponent`** (from Phase B) +
  deposits yield via `UAFLLootCarryComponent::Collect`. No model rework.

---

## Open (deferred numbers / tuning — principles are locked)
1. **Channel duration** + whether **firing** interrupts (default: no; only a hit/move cancels). Playtest.
2. **`afl.Loot.DropPercent`** exact value (25-50% start) + the grace/cooldown window. Playtest.
3. **Scatter quantization** — chunk size for the K recoverable pickups (~50-value start).
4. **Channel feedback widget** — radial vs bar (a HUD pass, not blocking the loop).
