# IRONICS — Loot-Carry Model (collect-vs-carry-object split) — v7

Finalized decision record. **v5 amendment (below) is the OVERLAP-PICKUP pivot — AUTO-PICKUP (walk-over) for body
parts + all loot EXCEPT map objects, superseding the collect-channel; v4 recorded Phase B SHIPPED + resolved the
Phase-C provenance fork; v3 added the PHYSICAL-OBJECT LIFECYCLE + recorded Phase A SHIPPED (wallet-rail).** v2's
3 locked operator decisions stand **EXCEPT Locked Decision 3** (the collect-channel) — superseded by v5. Governs
how collected loot is held (and Phase-4 harvest too).

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

**v4 amendment (2026-06-17) — Phase B SHIPPED + the Phase-C PROVENANCE resolution:**
- **Phase B SHIPPED (`47d80847`).** Routing (`EAFLGrabKind`), the collect-channel, and the cache migration are
  PIE-proven. **Architecture correction to STEP 2B / STEP 3:** the collect-channel shipped as a **GAS base
  ability** (`UAFLGameplayAbility_Channel` → `UAFLAG_CollectChannel`), **not** a `UAFLChannelComponent` — per
  `unreal-engine-expert`, GAS owns timed, interruptible, predicted/replicated activations (the WaitDelay clock,
  the lock-GE, the cancel funnel) for free; a component would re-roll that machinery. **Phase-4 HARVEST
  subclasses this ability**, not a component. The collect-channel is **move-CANCEL, not movement-locked**
  (Decision 3): moving cancels it; it does not freeze the hero.
- **The Phase-B physical hook landed as `ScatterPickupClass` + consume-on-collect** (Decision B): the CARRY
  cache is **destroyed** on collect and the value's recoverable form **respawns** on scatter —
  `hide-and-show` collapsed to `destroy-and-respawn` because a *fungible int pool can't track an individual
  hidden object*. The generic `ScatterPickupClass` seam is the parameterized form-supply hook STEP 2B promised.
- **Phase C provenance RESOLVED (skills-grounded — see STEP 2C below).** The ⚠ constraint Phase B recorded —
  *a single fungible int can't scatter dismember-value as a LIMB while cache-value scatters as a CUBE* — is
  resolved by a **thin, SERVER-ONLY provenance ledger** beside the proven int rail. The bulk value stays
  fungible (Phase A's wallet-rail decision STANDS); a bounded form→value ledger drives form-accurate scatter,
  and because **scatter is authority-only the ledger needs no replication at all.**

**v5 amendment (2026-06-18) — the OVERLAP-PICKUP pivot (B+C reframe; operator-confirmed interaction model):**
A C3 PIE watch surfaced two failures the spawn-logs hid: **(1)** a knocked-loose head reappears as a **CUBE**,
not a head; **(2)** a CLIENT can't pick up an enemy head — the physics-prop head **SHOVES** the pawn and **"G"
doesn't fire**. Root cause: **the GRAB/CHANNEL is the wrong mechanism for body parts.** Operator-confirmed model:
**AUTO-PICKUP (walk-over) for body parts + ALL loot EXCEPT map objects; the deliberate GRAB is reserved for map
objects (movable obstacles) + the OWNER-reattach.** This **supersedes Locked Decision 3** (the collect-channel)
for loot.
- **Enemy-collect = OVERLAP.** Reuse the proven `UAFLOverlapCollectComponent` (extracted from the energy pickup,
  worn by the INSTANT cache) — a **QueryOnly sphere that overlaps pawns with ZERO physics shove** → fires
  `OnCollected` → `TryGrant` → +pool + despawn. Fixes **(2)** for free (no grab, no shove). Skills:
  `afl-cpp-lyra-developer` (*extend the proven substrate, don't rewrite*) + `ue5-interaction-ik-expert` (passive
  overlap-trigger for auto-collect vs the deliberate, IK-driven manipulation that is the grab).
- **Owner-reattach = the GRAB — DELIBERATE + owner-only** (preserved: "owner = a deliberate press"). **align #2
  nuance:** the overlap is **ENEMY-ONLY** — the owner walking over does **NOT** auto-reattach (reattach stays a
  deliberate grab). Enemy-only gating: the overlap's `IsViableCollector` consults the grant's
  `ResolveRetrievalMode` (the C3 SSOT, same-module) → viable **iff `EnemyCollect`**; `OwnerReattach`/`Ineligible`
  → not viable → the owner's head persists for the deliberate grab.
- **The C3 grab-ROUTER + the collect-CHANNEL are SUPERSEDED.** The `IAFLLootRetrievalRouter` **interface** + the
  grab-ability channel-routing **DROP** (the overlap handles enemy-collect; the grab reverts to the proven
  owner-reattach via `TryGrant`'s owner-seam). **`ResolveRetrievalMode` STAYS on the grant** (the overlap
  consults it). The channel **BASE** (`UAFLGameplayAbility_Channel`) **SURVIVES for Phase-4 HARVEST**; only the
  loot USE of it (`UAFLAG_CollectChannel` + `UAFLGE_Channel`) is retired. *(With the overlap auto-collecting the
  enemy at proximity, an enemy never reaches the grab on a body part — "grab reserved for map objects" holds in
  practice; an explicit owner-only grab gate can be re-added if ever needed.)*
- **Caches → overlap too** (align #1): the **CARRY cache drops its channel** + wears the overlap (converging with
  the INSTANT cache — both walk-over auto-collect; the higher CARRY value is the only difference).
- **Collision (align #3):** the head/limb **physics body overlaps the PAWN** (no shove) while **world-blocking**
  (the death-tumble survives); the separate overlap **sphere** is the collect trigger. (`ECC_Pawn` → Overlap on
  the gib body; Block world.)
- **WHAT CARRIES OVER (mechanism-agnostic, proven):** the carried pool (A), the C1 provenance ledger, the C2
  limb-gib form, C3's value-side (`Configure→CarryToExtractEnergy`, `MakeLimbForm`, despawn-on-collect). **ONLY
  the collection TRIGGER changes** (overlap, not grab-channel). No work is lost.
- **BUG-1 (the cubes) = a SCALE bug, fixed here:** the scattered gib applies correctly but renders at the cube's
  `0.3` scale (~6 cm — *smaller than the 30 cm cubes*, so it's lost = "boxes not heads"). `ApplyVisualMesh`
  resets the pickup's scale to native (`1.0`) when a gib applies (the cube keeps `0.3`) → full-size scattered
  head/limb. (If full-size is STILL a cube, `SetVisualMesh` isn't landing → deeper diagnosis; an instrument log
  rides this increment to confirm.)

**v6 amendment (2026-06-18) — the DISCRETE-PART-TOKEN model (operator-approved; the coherent dismember-loot spine):**
Two PIE findings drove it: **(1)** the value-chunk quantizer scatters a 160-head as `ceil(160/50)=4` head-gibs
("4 heads from 1"); **(2)** the fungible pool drops owner-identity at collect, so a scattered-after-collect part is
loot-only (can't be reattached). BOTH are data-model limits, not laws. v6 makes a DISMEMBER PART a DISCRETE TOKEN
(owner-id + origin-zone + unit-value + gib mesh/material) that flows freely — collected, lost, reattached, lost,
collected — with value crystallizing ONLY at extraction (COUNT the parts you got away with). This supersedes the
per-form fungible VALUE for PARTS; **caches stay fungible** (cube currency, value-chunked, unchanged). The
discreteness also DISSOLVES the partial-drop tension (a part is indivisible -> hits eject WHOLE parts, never a
fraction). The proven spine — C1 server-only ledger, C2 gib-form, the presentation pass (material/pop/landing),
the Static-cue reattach (`c295de2c`) — ALL carries over; v6 enriches the token + the quantizer + the routing.
Additive. **See STEP 2E for the whole model + the F1-F4 phased plan.** Supersedes the E4 fungible full-loop intent
(the loop is now token-based) but keeps E1-E3 as shipped (scale fix, overlap, the cue reattach).

**v7 amendment (2026-06-19) — the PART-TOKEN model (parts PRIMARY, INDIVISIBLE):** F1's PIE watch exposed the
residue's ROOT -- deriving the part COUNT from VALUE (`round(Value/UnitValue)`) + a value-based drain = a
partial-value scatter ("53 of a head") that FRAGMENTS across sustained combat hits (4 hits -> 4 partial heads).
v7 makes a part PRIMARY + INDIVISIBLE: a token IS one whole part with a FIXED value -- no quantizer, no derive,
no value-math, so the residue is DESIGNED OUT (not patched). It ABSORBS F2 (identity = the token's owner +
SPECIFIC zone) and F4 (drop = whole tokens, SMALLEST-first; the head clings hardest) into ONE model. **F1 is
SUPERSEDED -- NOT committed** (subsumed by indivisibility; no quantizer at all now). Value-at-extraction (the v6
keystone) is KEPT; caches stay a fungible int beside the token list (cubes are not tokens). ⚠ the fixed values
(operator's illustrative leg=150 / arm=250 / head=1500) DIVERGE from §3a (160/20 FLAT) -- the RATIOS (head >> arm
> leg) are the intent, the MAGNITUDES are the operator's economy decision (the §3a calibration check is in STEP
2F). The proven spine (C1 server-only ledger, C2 gib-form, the presentation pass material/pop/landing, the
Static-cue reattach `c295de2c`) ALL carries over. **See STEP 2F for the whole model + the V7-1..4 phased plan.**
STEP 2E (the v6 value-bucket-with-UnitValue) is the decision trail -- superseded by STEP 2F (parts-primary).

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
  > **⚠ Phase-C constraint (recorded during Phase B) — RESOLVED in STEP 2C below.** The carried pool is a
  > *fungible int* — it can't, alone, tag which value came from a cache (cube form) vs a dismember gib (limb
  > form). **Resolution (skills-grounded): a thin, SERVER-ONLY provenance ledger beside the proven int rail**
  > — the value stays fungible; a bounded form→value ledger drives form-accurate scatter; the ledger needs no
  > replication because scatter is authority-only. See **STEP 2C**. Phase B's `ScatterPickupClass` seam is the
  > generic form-supply hook this builds on.

---

## STEP 2C — Phase C: the PROVENANCE resolution (skills-grounded) + the dismember migration

Phase C answers the ⚠ constraint Phase B recorded: the carried pool is one fungible `int32` with no provenance
— it can't, alone, scatter dismember-value as a **limb** while cache-value scatters as a **cube**.

### The decision — a THIN, SERVER-ONLY provenance ledger (resolution **c**, refined)
**The fungible value stays a rail; a bounded form→value ledger beside it drives form-accurate scatter. The
ledger is SERVER-ONLY (scatter is authority-only), so it needs no replication.**

**Cited guidance (the skills decided — not the lean):**
- **`afl-cpp-lyra-developer` (SKILL.md:20-22 — "extend Lyra, don't rewrite it").** The proven Phase-A int rail
  (collect/scatter/death/extract PIE-proven) is PRESERVED; provenance is an ADDED thin layer, not a REPLACEMENT
  of the rail with an inventory-shaped typed structure. `UAFLLootCarryComponent`'s own Phase-A doc-comment
  (`AFLLootCarryComponent.h:23-25`) already encodes the skill's split — *currency = a wallet rail; inventory =
  discrete owned things; `GetTotalItemCountByDefinition` counts instances, never sums value.* Phase C honors
  it: the **value** stays a rail.
- **`unreal-engine-expert` (references/networking.md:175, 177-193).** Scatter is **authority-only** (SpawnActor
  + the authority pool commit), so the provenance ledger is read only on the server → it needs **no
  replication** (only `CarriedValue` replicates, for the HUD). The cleanest possible: zero net cost, zero
  two-property sync risk. The skill's `FFastArraySerializer` (the `FInventoryList` pattern) is the documented
  **scale-up path** *if* the ledger ever needs client replication or grows large — unneeded for a bounded
  ~4-form server-only ledger now.
- **`expert-game-designer` (references/character.md:5-21 — the silhouette test + Visual Weight Hierarchy, HEAD
  30%).** A scattered head must **read as a head** — scattering it as a generic cube fails the silhouette read.
  So provenance must be **form-accurate** (per-limb: head→head-gib, arm→arm-gib), justifying a per-form ledger
  over a binary cache/dismember flag. The skill is **silent** on any "carrying-2-heads-matters" *mechanic* (its
  scope is visual design) — so there is **no** support for promoting a carried limb to a typed inventory entry.

**Rejected (with the cite):**
- **(b) contextual / proportional scatter** — non-deterministic provenance ("scatter ~30% as whatever was
  collected recently") violates the economy's **NO-RANDOMIZED-ACQUISITION** invariant (`AFLLootTypes.h:13-14`).
- **(d) revisit fungible → a typed inventory entry per limb** — contradicts `afl-cpp-lyra`'s *currency = a rail,
  not inventory* and the proven Phase-A decision; re-introduces exactly the instance-shaped representation Phase
  A correctly rejected, with no game-design *mechanic* to justify it.
- **(a) per-source SUB-POOLS (split the replicated int)** — would re-touch the PROVEN replicated rail
  (regression risk), and a binary cache-vs-dismember split is coarser than per-form (fails the per-limb read).
  Superseded by **(c)**: keep the rail intact, go per-form **server-side**.

### The provenance mechanism — concretely
- **Data:** `USTRUCT FAFLCarriedForm { TSubclassOf<AAFLLootCarryPickup> ScatterForm; TSoftObjectPtr<UStaticMesh>
  GibMesh; int32 Value; }`. A **server-only** `TArray<FAFLCarriedForm> CarriedForms` on
  `UAFLLootCarryComponent` (NOT a replicated UPROPERTY). Bounded (~4 entries: cube, head-gib, arm-gib, leg-gib).
- **The replicated `int32 CarriedValue` is UNCHANGED** — it stays the bank total + the HUD rail (Phase A
  intact). Invariant: `sum(CarriedForms.Value) == CarriedValue`, maintained on the authority.
- **Collect** (`Collect(int32 Value, const FAFLCarriedForm& Form)`): `CarriedValue += Value` (unchanged) **and**
  find-or-add the `Form` bucket, `+= Value`. The **old 1-arg `Collect(int32)` stays** as an overload that
  defaults to the cube form — the proven caches keep working untouched.
- **Scatter:** drain the ledger in **collection order** (deterministic), spawning each drained chunk **as its
  own form** (the gib-mesh pickup for dismember buckets, the cube pickup for cache buckets) — not always
  `ScatterPickupClass`. Partial drop (on-hit) drains forms in order until the drop amount is met; full scatter
  (death) drains every bucket as itself. Deterministic throughout (no roll).
- **Extract:** bank `CarriedValue` (unchanged) + clear the ledger.

### The dismember enemy-collect migration ⚠ (the 2 enum-flips — parallel to Phase B's cache flip)
Exactly the Phase-B move applied to the shipped dismember loot:
- `AAFLHeadLootBox.cpp:83` and `AAFLDismemberedLimb.cpp:129`: `Configure(EAFLLootValueModel::Watts, …,
  EnemyOnly, …)` → **`Configure(EAFLLootValueModel::CarryToExtractEnergy, …, EnemyOnly, …)`** + supply the
  limb's **gib form** (head-gib / arm-gib / leg-gib mesh).
- The grant's `CarryToExtractEnergy` case (`AFLLootGrantComponent.cpp:88-105`) passes the form through:
  `Carry->Collect(LootValue, Form)`. The grant component gains a `ScatterForm`/`GibMesh` field, set by
  `Configure` from the loot object (which knows its own form).
- **Result:** +160 (head) / +20 (limb) now enters the ENEMY retriever's **carried-at-risk pool**, tagged with
  the limb form so it scatters as that limb. ⚠ this re-touches SHIPPED, PIE-watched combat-loot (`9ac3e0ae`).

### The owner-reattach branch — UNCHANGED (re-confirmed; a SEPARATE axis)
The owner seam is **`EAFLLootEligibility::EnemyOnly` + `OnOwnerRetrieved` → `RestoreZone` + `Destroy`**
(`AFLLootGrantComponent.cpp:140-144`) — it **never reaches `GrantValue`**, so it is on a **different axis** from
the value model. The migration flips only the **enemy-collect VALUE MODEL**; the `EnemyOnly` eligibility and
the owner-reattach (body-restoration, no pool, no scatter) are **literally unchanged.**

### The limb-gib physical form (the `ScatterPickupClass` seam, per-form)
- The Phase-B `ScatterPickupClass` (the generic cube `AAFLLootCarryPickup`) becomes the **default** form (cache
  buckets). Dismember buckets carry the **limb-gib mesh** (`SM_AFL_Robot{Arm,Leg}_Gib` / head gib, from the
  limb-gib extraction work).
- **Recommended:** ONE `AAFLLootCarryPickup` whose **mesh is set per-spawn** from the bucket's `GibMesh`
  (reusing the per-spawn-mesh pattern the dismember gibs already use), rather than N near-identical pickup
  subclasses. The scattered limb-loot is the **lighter recoverable carry-pickup wearing the gib mesh** — *not*
  a re-spawned full `AAFLDismemberedLimb` (which carries the owner-reattach branch + would risk a double-grant):
  same silhouette, different (economy-recoverable) actor.
- Per **Decision B**: collect → the dismember loot object is **consumed** (the limb is picked up); the value
  rides the pool; it **reappears as a limb-gib pickup only if scattered** (damage/death). On extract it banks
  silently (the limb "cashed in").

### The behavior change — re-watch in C (stated since v3)
Enemy head/limb is now **CARRIED-AT-RISK, not instant-banked**: the killer must **survive + extract** to
realize +160/+20; a hit scatters a portion **as a limb-gib** (recoverable by anyone); death scatters the whole
pool (the limbs reappear on the ground). Owner-reattach unchanged. The re-watch is the migration's real proof.

### Re-confirm the shipped head-box mechanics survive
The migration swaps only the enemy-collect value model — these are **untouched** and re-watched: the
owner-vs-enemy branch (controller-match), the persist (`InitialLifeSpan=0`), the `RestoreZone` reattach, the
grant-once guard (`bSpent`), and the owner-death-vanish (`OnDeathStarted`; memory-flagged unverified — verify
in C).

---

## STEP 2D — the OVERLAP-PICKUP build plan (v5 — the B+C reframe)

The collection TRIGGER changes from grab-channel to overlap auto-pickup; the proven value-side carries over.
Built in WATCHED increments (✅ = the operator SEES it on screen, never a spawn-log):

- **E1 — the gib SCALE fix (BUG-1).** `AAFLLootCarryPickup::ApplyVisualMesh` resets `VisualMesh` scale to `1.0`
  when a gib applies (the cube keeps `0.3`) + a one-line instrument log confirming `SetVisualMesh` landed the
  gib. ⚠ touches the (uncommitted) C1 pickup. **Proves (WATCHED):** a scattered gib (`afl.LootCarry.TestLimbForm`
  → damage/die) reads as a **FULL-SIZE** head/arm/leg, not a 6 cm token. *If still a cube at full size →
  `SetVisualMesh` isn't landing → diagnose before proceeding.*
- **E2 — the enemy OVERLAP auto-collect + the collision (NEW + ⚠ shipped dismember).** The head/limb wear a
  `UAFLOverlapCollectComponent` (QueryOnly sphere) → `OnCollected` → `TryGrant` → +pool + despawn. The overlap's
  `IsViableCollector` consults the grant's `ResolveRetrievalMode` → **`EnemyCollect` only**. The gib body's
  `ECC_Pawn` → Overlap (no shove), Block world (tumble). **Proves (WATCHED):** an ENEMY walks over a head →
  **auto-collected, NO shove, NO "G"** (+pool, despawn); the OWNER walks over → **does NOT auto-reattach**.
- **E3 — drop the grab-channel + caches→overlap (⚠ uncommitted C3 + Phase-B cache).** Revert the C3 grab-ability
  routing (the grab → the proven owner-reattach via `TryGrant`); drop `IAFLLootRetrievalRouter` + retire
  `UAFLAG_CollectChannel`'s loot use (the channel BASE stays for HARVEST). The CARRY cache drops its channel +
  wears the overlap. **Proves (WATCHED):** the OWNER grabs (deliberate G) → **reattach** (RestoreZone, no pool);
  a CARRY/INSTANT cache walk-over → auto-collect; a map stress-object still hand-carries.
- **E4 — the FULL-loop re-watch (the gate that closes the intent).** Enemy walks over a head → auto-collect →
  +160 pool → despawn → shot/dies → **a FULL-SIZE head gib on the ground, CROSS-CLIENT (a client sees a head)** →
  recover → extract → +160 banks. Same arm/leg (+20). **This is the ✅ the C3 watch was missing.**

The channel-window re-grab spam is now **MOOT** (no grab-channel for loot). The C3 work is **not lost** — the
value-side (`Configure`/`MakeLimbForm`/despawn) + the ledger + the form carry over; only the grab-router/channel
revert.

---

## STEP 2E — the DISCRETE-PART-TOKEN model (v6 — the coherent dismember-loot spine)

v6 resolves two PIE-surfaced limits of the fungible-value model AS ONE coherent system: **(1)** the value-chunk
quantizer fragments one part into many (`K=ceil(Value/50)` -> a 160-head scatters as 4 head-gibs); **(2)** the
fungible pool drops owner-identity at collect, so a scattered-after-collect part can't be reattached. The fix:
make a DISMEMBER PART a DISCRETE TOKEN carrying its own identity — collected/lost/reattached/lost/collected
freely; value crystallizes ONLY at extraction (count). **Caches stay fungible** (cube currency, unchanged).

### Skill-grounding (cited)
- **`afl-cpp-lyra-developer` (SKILL.md:20-22 "extend Lyra, don't rewrite"; rule 4 / trap #3 net rules).** The
  token's OWNER ref is NET-SAFE: a stable player-id `int32` (`APlayerState::GetPlayerId()`), NEVER a raw pointer
  or a hard `APlayerState*` (survives respawn + relevancy). The reattach resolver is SERVER-AUTH (player-id ->
  PlayerState -> pawn -> `RestoreZone`) — the same server-resolved shape the proven owner-reattach already uses.
  The carried-parts ledger EXTENDS C1's server-only `CarriedForms` into a token list — not a rewrite.
- **`unreal-engine-expert` (networking — scatter is authority-only).** The parts ledger stays SERVER-ONLY (no
  replication): scatter `SpawnActor` is authority-only; the reattach owner-check is server-auth. The SCATTERED
  PICKUP carries the replicated identity (owner-id + zone + unit-value) on ITS OWN actor (for the check +
  display) — not the carrier's ledger. The HUD reads a replicated derived `int32` (part count + sum-value).
  `FFastArraySerializer` stays the scale-up path if the ledger ever needs client replication.
- **`expert-game-designer` — FLAGGED NOT-COVERING (per doctrine: flag-missing, don't-invent).** This skill is
  VISUAL/UI design (Apple Glass, HUD, environment, character) — it has NO loot-economy or partial-drop-feel
  reference (refs: ui-hud/environment/character/pipelines/ue5-design/afl-design). So the economy-feel is grounded
  in **`IRONICS_ECONOMY_SPEC.md`** instead: head=160 / limb=20 (§3a); NO RANDOMIZED ACQUISITION (§0); integer
  value conservation; value at-risk until extraction (§3a + the v5 model). The token model honors all of these.

### The token (data model)
`FAFLCarriedPart` (evolves `FAFLCarriedForm`): `{ int32 OwnerPlayerId; EAFLBodyZone OriginZone; int32 UnitValue;
TObjectPtr<UStaticMesh> GibMesh; TObjectPtr<UMaterialInterface> GibMaterial; }`. One token = one discrete severed
part. `OwnerPlayerId = GetPlayerId()` (net-safe, stable); `UnitValue` = 160 (head) / 20 (limb) from the zone row.
The carrier holds a SERVER-ONLY `TArray<FAFLCarriedPart> CarriedParts` (C1's ledger, now a token list — flat is
simplest for identity; bounded, a player carries a handful). The fungible cache-value `int32` track (v5) stays
UNCHANGED beside it. The HUD reads a replicated derived `int32 CarriedPartCount` + `int32 CarriedPartValue`.

### Discrete-whole-part scatter (fixes "4 heads from 1")
`SpawnFormPickups` QUANTIZER RULE: a PART token scatters as ONE WHOLE part-pickup (one pickup = one whole part,
carrying its token); a CUBE/cache bucket keeps `K=ceil(Value/50)` value-chunks (fungible currency, correctly
spread). So N heads + M limbs come out as N+M discrete pickups — never `ceil(160/50)=4`. Each scattered
part-pickup (`AAFLLootCarryPickup`) carries the token's `{OwnerPlayerId, OriginZone, UnitValue, GibMesh,
GibMaterial}`, replicated on the pickup actor.

### Dual routing — reattach becomes UNIFORM (off the carried identity)
A part-pickup, on overlap/grab, resolves the SAME owner-vs-enemy split a FRESH severed part uses — but driven by
the CARRIED `OwnerPlayerId` now (server-auth):
- toucher player-id == `pickup.OwnerPlayerId` -> **REATTACH**: resolve the owner's pawn -> `RestoreZone(
  pickup.OriginZone)` (the proven Static-cue path, `c295de2c`) -> the part returns to the owner's body, the token
  leaves the world. NO value (body-restoration — consistent with §3a owner-reattach = no-Watts).
- toucher != owner -> **COLLECT**: the token enters the toucher's `CarriedParts` (as today, now with identity).
So reattach is UNIFORM: ANY part — fresh-severed OR scattered-after-collect — is owner-reattachable +
enemy-collectible. An owner can chase their head across the map and reclaim it even after an enemy collected + dropped it.
- VALUE BOOKKEEPING (operator flag, confirmed conserved): a part is reattachable only while it is a WORLD PICKUP
  (fresh OR scattered). Once an enemy COLLECTS it (token in their pool, invisible — Decision B), the owner can't
  reach it until it SCATTERS back out; the enemy LOST the token on scatter (it left their pool when hit), so the
  owner's reattach takes nothing from the enemy — no double-count. If the enemy carries it to EXTRACTION instead,
  it's counted + gone (the owner's window closed). Value conserved at every step.

### Extraction — count at the checkpoint (the operator's keystone)
NO value crystallizes at collect. At the EXTRACTION checkpoint the carrier's `CarriedParts` are COUNTED:
`sum(UnitValue)` -> Watts (head=160, limb=20) via `EarnWattsAuthority`, tokens clear (cashed in); the fungible
cache int banks in the same step. **TEAMS:** the count aggregates per TEAM — each player extracts their own
tokens -> their Watts; the team total = sum of the team's extractions (the team is looked up at count-time from
each owner's PlayerState; the token carries the player-id, not a team-id). No carry-mechanic change for teams — a
scoring aggregation. So carried parts are pure PHYSICAL TOKENS until extraction: "got away with cleanly" is the
only thing that pays.

### Partial-drop — RESOLVED by discreteness (the STEP-1 question)
The token model dissolves the "33% of a 160-head = 53 (less than a whole part)" tension: a part is INDIVISIBLE,
so a hit ejects WHOLE part(s), never a fraction. The v5 portion-on-hit principle (decision 2) adapts to a COUNT:
on a confirmed hit, scatter `K` WHOLE part-tokens; on death, scatter ALL. The fungible cache track keeps its
percentage drop (divisible currency). **OPERATOR SUB-CHOICE for K (parts-per-hit feel):**
- **(K-a) one whole part per hit** — simplest, predictable; a hit knocks a single trophy loose.
- **(K-b) a fraction of the carried part-count** (~33%, rounded, min 1) — scales with load (the v5 portion
  principle, discretized); a heavily-laden carrier bleeds more per hit.
- Either drains FIFO (oldest-collected first, like the C1 drain); NO RANDOMIZED ACQUISITION holds (deterministic).
  **RECOMMENDATION: K-b** (preserves v5's "the more you carry, the more you risk" while staying discrete); K-a if
  the operator wants a flatter, more readable per-hit feel.

### What carries over (the proven spine — additive)
C1's server-only ledger, C2's limb-gib form, the presentation pass (material-carry + directional/angular pop +
collectible-on-settle), the Static-cue reattach (`c295de2c`) ALL carry over. v6 ENRICHES the token (owner-id +
zone + unit-value), changes the QUANTIZER (whole-part vs value-chunk), and routes the pickup off the carried
identity. The fungible cache track is untouched.

### Phased build plan (each WATCHED; ⚠ = re-touches SHIPPED code)
- **F1 ⚠ — discrete-whole-part scatter.** `FAFLCarriedForm` -> token (`+UnitValue`); `SpawnFormPickups`
  whole-part rule for gib-forms (cube-forms keep the chunk); the dismember `Configure` supplies the unit-value.
  **WATCHED:** collect ONE enemy head -> scatter -> exactly ONE head-gib (not 4); collect two DIFFERENT players'
  heads -> scatter -> TWO heads (count correct).
- **F2 ⚠ — owner-identity carry + the dual reattach routing.** `FAFLCarriedPart` `+OwnerPlayerId +OriginZone`;
  the pickup carries them (replicated); overlap/grab resolves owner->`RestoreZone` vs enemy->collect off the
  carried id; the dismember `Configure` supplies the owner-id + zone. **WATCHED (2-client):** an enemy collects
  player-X's head -> gets hit -> the head scatters -> **player-X walks over it and REATTACHES it**
  (reattach-after-collect); a non-owner walks over -> collects it.
- **F3 ⚠ — extraction count.** `ExtractAll` sums `CarriedParts` unit-values -> Watts (+ the cache int); team
  aggregation at count-time. **WATCHED:** carry N heads + M limbs through extraction -> **+N×160 + M×20 Watts**;
  tokens clear; in a team match the team total reflects both members' extractions.
- **F4 ⚠ — discrete partial-drop + tuning.** The chosen K-rule on `Event.Damage.Confirmed`; ALL on death.
  **WATCHED:** a hit ejects WHOLE part(s) (never fractional); death ejects all; 2-client recovery.

Each F-phase ends operator-WATCHED (✅ = seen on screen). Risk/reward intact: collect freely, but parts are
at-risk until extraction (drop whole ones per hit, lose all on death, an owner can reclaim theirs).

---

## STEP 2F — the PART-TOKEN model (v7 — parts PRIMARY, indivisible)

v7 supersedes STEP 2E's value-derived quantizer (the source of F1's residue) by making a part PRIMARY and
INDIVISIBLE: a token IS one whole part with a FIXED value -- no `round(Value/UnitValue)`, no quantizer, no
value-math, so the "53 of a head" residue is DESIGNED OUT, not patched. It ABSORBS F2 (identity) and F4 (drop
order) into ONE model.

### Why this supersedes F1 (the residue's root)
F1 derived the scattered part COUNT from value (`round(Value/UnitValue)`) and kept the drain value-based -- so a
33%-of-160 hit scattered a "53-value head" (a partial-value token), and 4 sustained hits shed 4 partial heads
(the PIE watch). The token model removes the derive entirely: a part is one indivisible token; a hit drops ONE
WHOLE token (or none); there is no fractional value to spawn. No quantizer, no residue -- ever, even in combat.

### The token (data model)
`FAFLCarriedPart { int32 OwnerPlayerId; EAFLBodyZone OriginZone; int32 FixedValue; TObjectPtr<UStaticMesh> GibMesh;
TObjectPtr<UMaterialInterface> GibMaterial; }`. One token = one whole part. `OwnerPlayerId = GetPlayerId()`
(net-safe int32). `OriginZone` is SPECIFIC (Head / LeftArm / RightArm / LeftLeg / RightLeg -- the enum is already
L/R-distinct, confirmed) so a part reattaches to the CORRECT slot (a left leg to the left-leg slot). `FixedValue`
is baked per zone-type (the DA zone row's `LootWatts`) -- value is an ATTRIBUTE of the part, never derived.
The carrier holds a SERVER-ONLY `TArray<FAFLCarriedPart> CarriedParts` (extends C1's server-only ledger -- now a
token list, not value-buckets). The fungible CACHE int (`CarriedValue`) stays BESIDE it for cube/cache loot. The
HUD reads a replicated derived `int32` (part count + part-value-sum + cache int). SUPERSEDED: STEP 2E's
`FAFLCarriedForm` value-bucket + `UnitValue` derive + the `SpawnFormPickups` quantizer.

### Fixed values (the value peg -- ⚠ OPERATOR DECISION, flagged)
Each part-type's value is baked in the DA zone row (`LootWatts`). Operator's illustrative set: leg=150, arm=250,
head=1500. **⚠ ECONOMY-SPEC FLAG -- these DIVERGE from `IRONICS_ECONOMY_SPEC.md` §3a (head=160, limb=20 FLAT,
limb=head/8):**
- **Invariants HELD:** integer, head-most-valuable (1500 > 250 > 150), no randomized acquisition (fixed). OK.
- **Calibration BLOWN at face value:** §3a pegs head=160 to ~40% of a ~4,000-Watt/match budget at ~10
  head-equivalents/match; head=1500 (~9×) makes combat-loot ~15,000/match -- it would DOMINATE the economy.
- **Two design changes:** arm (250) > leg (150) breaks §3a's FLAT-limb (all 20); the head/8 limb relationship is
  dropped. The RATIOS (head >> arm > leg) are the v7 intent; the MAGNITUDES must re-peg to the budget.
- **OPERATOR DECIDES one of:** (a) keep the ratios, re-scale to the ~4,000/match budget (e.g. head~160, arm~27,
  leg~16 -- arm > leg preserved, head at the proven peg); (b) deliberately grow combat-loot's economy share with
  higher magnitudes; (c) the 1500/250/150 are illustrative ratios only. Whatever is chosen UPDATES §3a (the spec
  is the economy's home).

### Scatter -- drop WHOLE tokens, smallest-first (absorbs F4)
- **On a confirmed hit:** drop ONE whole token, SMALLEST-`FixedValue`-first (leg -> other leg -> arm -> other arm
  -> head LAST). The head clings hardest -- the prize takes sustained damage / a kill to pry loose. **⚠ OPERATOR
  CONFIRM: one token per hit** -- the operator's "first shot drops a leg, second shot the other leg" reads as
  one-per-hit; a small count-per-hit is the alternative. Deterministic (no roll).
- **On death:** drop ALL remaining tokens.
- Each dropped token spawns ONE part-pickup (`AAFLLootCarryPickup`) wearing the token's mesh+material (the
  presentation pass's pop/material/landing -- unchanged) + carrying the replicated `{OwnerPlayerId, OriginZone}`.
- The cache int (`CarriedValue`) keeps its v5 percentage-scatter beside this (fungible currency, unchanged).

### Collect + uniform reattach (absorbs F2; CATEGORY identity)
A part-pickup, on overlap/grab, resolves off its carried `{OwnerPlayerId, OriginZone}` (server-auth):
- toucher player-id == `OwnerPlayerId` -> **REATTACH to that SPECIFIC zone**: resolve id -> GameState PlayerArray
  -> PlayerState -> pawn, `RestoreZone(OriginZone)` (the proven `c295de2c` path) -> the part returns to the
  correct slot. Token leaves the world. NO value (body-restoration).
- toucher != owner -> **COLLECT**: append the token to the toucher's `CarriedParts`.
Reattach is UNIFORM -- ANY part, fresh OR scattered-after-collect, is owner-reattachable + enemy-lootable. **
CATEGORY identity, NOT per-instance:** the token carries owner + zone + value (a category, e.g. "aria's head,
1500") -- there are NO globally-unique per-instance IDs (no mechanic needs per-instance tracking; the net model
stays minimal).

### Value at extraction (v6 keystone, KEPT)
No value at collect. At the extraction checkpoint, sum the token list's `FixedValue` -> Watts
(`EarnWattsAuthority`), tokens clear; the cache int banks in the same step; team-aggregated (each player extracts
their own tokens -> their Watts; the team total sums the team's extractions). Parts are pure tokens until then.

### Skill-grounding (cited)
- **`afl-cpp-lyra-developer`:** `OwnerPlayerId` = net-safe `int32 GetPlayerId()` (NEVER a raw `APlayerState*`);
  the reattach resolver is SERVER-AUTH (id -> GameState PlayerArray -> PlayerState -> pawn -> `RestoreZone`); the
  token list EXTENDS C1's server-only ledger (server-only, zero-replication). The scattered pickup carries the
  replicated `{owner, zone}` for the owner-check; the HUD reads a replicated derived int. `FFastArraySerializer`
  is the scale path IF the token list ever needs client replication at BR scale (not now -- server-only).
- **`IRONICS_ECONOMY_SPEC.md`:** the fixed values respect integer + no-randomized-acquisition + head-most-valuable
  + at-risk-until-extraction; the MAGNITUDE re-peg is the flagged operator decision above.
- **`expert-game-designer` -- FLAGGED NOT-COVERING** (visual/UI skill, no loot-economy reference) -- the economy
  is grounded in the SPEC, not a fabricated citation.

### Carries over / superseded
- CARRIES OVER: carry-at-risk (A); C1's server-only ledger (the token list extends it); C2's gib mesh + the
  presentation pass's material/pop/landing (the token carries mesh+material; the scatter presentation is
  unchanged); the `c295de2c` Static-cue reattach (the resolver the token's owner-id drives).
- SUPERSEDED: F1's value-derived quantizer + `UnitValue` (gone -- tokens are indivisible); the int-rail-as-the-
  unit FOR PARTS (parts are a token list now; the int rail is cache-only). STEP 2E is refined to parts-primary.

### Phased build plan (each WATCHED; ⚠ = re-touches SHIPPED code)
- **V7-1 ✅ SHIPPED `7c6effc6` — the token + indivisible scatter.** `FAFLCarriedPart` + the `CarriedParts` list
  (replacing the value-bucket for parts); collect -> append a token; scatter -> drop whole tokens (death = all; a
  hit = one token). The dismember `Configure` supplies the token (owner-id + zone + fixed value + mesh + material).
  **WATCHED:** one head collected -> die -> exactly ONE head; combat multi-hit, still one whole head, no fragmentation.
- **V7-2 ✅ SHIPPED `7c6effc6` — owner+zone identity + uniform reattach-after-collect.** The pickup carries
  `{OwnerPlayerId, OriginZone}` (replicated); collect resolves owner -> `RestoreZone(OriginZone)` (via
  `IAFLPartReattachTarget`) vs enemy -> collect. **WATCHED (2-client):** an enemy collected C_1's head -> got hit ->
  the head dropped as a real gib -> **C_1 walked over it and REATTACHED it** (the CLIENT-side reattach).
- **V7-3 ✅ SHIPPED `7bd200e7` — smallest-first drop order.** `ScatterOnePart` sorts `CarriedParts` ascending by
  `FixedValue` before dropping `[0]`. **WATCHED:** sustained hits dropped leg=16, then arm=27, then head=160 (head clings last).
- **V7-4 ✅ SHIPPED `7bd200e7` — extraction count.** `HandleExtractionComplete` sums `CarriedParts.FixedValue`
  alongside the cache int and banks BOTH, then clears both (the `Total` early-out banks a parts-only carry — empty
  rail). **WATCHED:** `+187 Watts (0 cache + 187 parts), pools cleared`.
  **TEAM AGGREGATION IS A FUTURE PIECE (NOT built):** the bank is **PER-PLAYER** (the wallet is on the extractor's
  PlayerState, via the existing `EarnWattsAuthority`). The project has **no team-score system** yet — the only
  `TeamId` is cosmetic (ARIA-faction skin), not a scoreboard. A team-mode-at-BR-scale design must add a team-score
  system FIRST; extraction banking is per-player today. (Supersedes the "team total reflects the team's extractions"
  intent above — that's the deferred layer, not the shipped behavior.)

The residue is DESIGNED OUT at V7-1 (indivisibility) -- "one head = one head" holds in combat from the FIRST
phase, not deferred to a drop-rule phase. Each phase ends operator-WATCHED (✅ = seen on screen).

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
- **Phase C ⚠ — the provenance ledger + the dismember enemy-collect migration + the LIMB-GIB form.** [C++]
  Four watched increments (STEP 2C is the design):
  - **C1 — the provenance ledger (NEW, additive).** `FAFLCarriedForm` + the server-only `CarriedForms` ledger
    + the 2-arg `Collect(value, form)` overload + form-accurate `ScatterValue` on `UAFLLootCarryComponent`;
    the 1-arg `Collect` (cube default) preserves the proven caches. **Proves:** collect **two distinct forms**
    (cube + a test gib) → scatter → **two distinct forms spawn** (the differentiation check — two inputs, two
    readouts) + `afl.LootCarry.Test.Run` still green (no rail regression).
  - **C2 — the limb-gib carry-pickup form (NEW content/wiring).** The dismember scatter form (one
    `AAFLLootCarryPickup` taking the gib mesh per-spawn) + wire the head/arm/leg gib meshes. **Proves:** a
    scattered dismember-value spawns a recognizable **limb gib** (the silhouette read), not a cube.
  - **C3 ⚠⚠ — the dismember enemy-collect migration (SHIPPED).** Flip `AAFLHeadLootBox` +
    `AAFLDismemberedLimb` `Configure` (`Watts → CarryToExtractEnergy`) + supply the gib form + the grant
    pass-through. **Proves (re-watch the shipped +160/+20):** collect an enemy head → **+160 in the CARRIED
    pool, not the instant wallet**; owner-reattach re-confirmed unchanged in the same watch.
  - **C4 ⚠ — the behavior-change + head-box-survival re-watch (SHIPPED behavior).** The full loop: collect
    enemy head → carried (not banked) → take damage → **head-gib scatters** → recover → extract → **+160
    banks**. AND owner reattach still `RestoreZone`s; the persist + owner-death-vanish + grant-once survive.
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
