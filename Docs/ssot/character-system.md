# SSOT — CHARACTER SYSTEM (Tier 2)

**What this is:** what the character, identity and creator systems **are** and **why**. It changes when the
system is redesigned.
**What this is not:** a status board. **This document contains no status claims** — nothing here says what is
built, proven, done or owed, no phase-gate table appears, and no commit hash appears as evidence of progress.
Those belong to Tier 3 (`LIVE_TRACKER`).

**Doctrine is cited, never restated.** Laws live in [`Docs/DOCTRINE.md`](../DOCTRINE.md) and are referenced by
id (**A11**, **N8**, **N11**, **G4**–**G6**, **NM5**, **NM6**, **C2**, **X7**, **X8**, **X10**, **X11**).

---

## 1. SCOPE

This SSOT governs: where character content is addressed on disk · FBIK as the animation contract · the twelve
locked character rulings as system design · the two-line fork · the resolution spine · the sticker composite
system · the creator and its slot economy · identity production · the colour axis · naming · and the
interfaces character exchanges with the economy.

It does not govern: currency, prices or entitlement grants (→ `ssot/economy-store.md`), what a ruleset is
(→ `ssot/match-modes.md`), weapons (→ `ssot/combat-arsenal.md`), or the state of any implementation
(→ Tier 3).

---

## 2. WHERE CHARACTER CONTENT LIVES — DESIGN VOCABULARY vs DISK VOCABULARY

`ssot/combat-arsenal.md` §3.1 records that the arsenal is addressed as **equipment** on disk, so a search for
"weapon" finds a fraction of it. **The character system has the same divergence, and worse** — its content is
split across three unrelated address spaces under two different naming traditions.

### 2.1 The three homes

| What | Where | Naming tradition |
|---|---|---|
| **Skeleton, animation, locomotion, aim offsets, poses** | Under the **stock Lyra** hero path (`Characters/Heroes/Mannequin/…`) | Engine/Lyra names — `SKM_Manny`, `SKM_Quinn`, `SK_Mannequin` |
| **Identity, cosmetics, finishes, skin colours, brand marks, ProMod rigs** | Under the **project** character path (`BagMan/Characters/Cosmetics/…`, `BagMan/ProMod/…`) | AFL names — `DA_AFL_*`, `MI_*`, `CR_ProMod_FBIK` |
| **Experiences, pawn data, part maps, catalog** | Under **GameFeature plugin content** | `B_Experience_*`, `HeroData_*`, `DA_AFL_*` |

**The split is not accidental and should not be "tidied."** The animation half sits on the stock path because
the characters ride the **shared stock skeleton** (§3) — moving it would break the retarget and rig
relationships that the shared skeleton exists to provide. The identity half is project content because it is
authored per identity. And **G4** independently forbids collapsing them: `/Game` content must not reference
GameFeature content, so the split across `/Game` and plugin content is a structural boundary, not a filing
preference.

### 2.2 Search terms that do and do not find character content

| Term | Result |
|---|---|
| **`Cosmetic`** as a **filename** | **Nearly nothing.** Cosmetics are a *folder*, not a name prefix |
| **`Cosmetics/`** as a **folder** | Hundreds of assets — the whole identity half |
| **`Character`** as a filename | A small slice — mostly part maps and selectors, not the characters |
| **`Robot`** | **The identity actors themselves.** A per-identity character BP is named `B_AFL_Robot_<NAME>` |
| **`Manny` / `Quinn`** | The largest match set — the skeleton and animation half |
| **`Hero`** | Pawn and pawn-data assets only |
| **`FBIK`** | The rig, the post-process anim BP, and the IK rig — three assets carrying an entire doctrine |

**The three traps this creates**, each of which produces a confident wrong answer:

1. **Searching for "character" finds the plumbing, not the characters.** The characters are named *Robot* — a
   term from the X-line's design vocabulary that stuck as the asset convention (**NM5**: a shipped id is never
   renamed, so it will stay).
2. **Searching filenames for "cosmetic" returns almost nothing** while the cosmetic system is one of the
   largest asset groups in the project. Cosmetics are addressed by *folder*, and a filename search reports
   "there is nothing here" about the exact opposite.
3. **The animation half looks like untouched stock content.** It sits on the engine path with engine names,
   so a sweep scoped to project content misses the skeleton and every animation the characters depend on.

> **The rule this generalises to:** *before auditing, censusing or porting the character system, establish its
> address space.* A term that returns few results is evidence about the naming convention, never evidence
> about the system.

---

## 3. FBIK IS PERMANENT DOCTRINE

> **Every character, current and future, is FBIK, on the rigged Manny/Quinn skeleton bases.
> Not experimental, not optional, not per-line.**

This is **A11**. It is cited, not restated — what follows is the *system* the law implies.

### 3.1 What FBIK requires of a rig

A full-body solver differs from per-limb IK in one decisive way: **it solves the whole skeleton as one
constraint system.** Per-limb IK asks "where does this hand go"; FBIK asks "what pose satisfies every
effector at once," and propagates a foot correction up through the pelvis and spine into the arms.

That produces four hard requirements on any rig entering the system:

1. **The shared skeleton hierarchy, unmodified.** The solver is authored against a specific bone chain —
   pelvis, spine, upper/lower arm, hand, thigh, calf, foot. A body with **extra bones, missing bones, or a
   renamed chain** is not a variation the solver tolerates; it is a different constraint system.
2. **A clean bind with no phantom bones.** Import artefacts survive per-limb IK unnoticed because nothing
   solves through them. FBIK solves through everything, so a stray bone becomes a joint the solver is free to
   move, and the failure appears as a pose that is subtly wrong everywhere rather than obviously wrong in one
   place.
3. **Correct weighting, verified on the right bones.** **X11** applies with force here: bone names differ per
   family and per side, and weights must be verified on the **twist** bones — base bones read zero even on a
   correct bind. A body that reads "unweighted" by naive inspection may be correct; one that reads correct may
   be weighted only on bases and will deform badly under a full-body solve.
4. **Proportions within the shared skeleton's range.** The solver's effector targets are authored in the
   skeleton's proportions. A body whose limb lengths diverge meaningfully does not get a slightly different
   pose — it gets over-stretch and joint inversion, the same failure class that made a persistent
   far-target arm solve unusable in the arsenal's grip work (`ssot/combat-arsenal.md` §3.3).

### 3.2 What a new character must satisfy to be accepted

A new character is accepted into the system when it satisfies **all** of:

| # | Requirement | Failure if absent |
|---|---|---|
| 1 | Conforms to the shared skeleton — **no added bones**, no renamed chain | The solver has no valid chain; the pose is undefined rather than wrong |
| 2 | Clean bind, weights verified on twist bones (**X11**) | Deformation defects that only appear under a full-body solve |
| 3 | Proportions within the skeleton's range | Over-stretch and inversion at the effectors |
| 4 | Sockets and part-attachment points present and **bone-parented**, not loose | A loose attachment floats and fights the solve in both position and rotation |
| 5 | Rides the resolution spine (§6) rather than hardcoding its parts | The character cannot be swapped, coloured, or composed |
| 6 | Its distinctness is **visual identity**, not rig divergence (**C2**) | A rig fork buys nothing a texture could not, and costs the contract |

**Requirement 6 is the one most often argued against**, so state it plainly: *the answer to "this character
needs a different body" is authored art on the shared skeleton, not a different skeleton.* A silhouette can
change by geometry; the bone chain underneath it does not have to.

### 3.3 Why a non-FBIK character cannot be added later

This is the load-bearing consequence and the reason **A11** is doctrine rather than a preference.

**FBIK is not a feature applied per character — it is a property of the animation pipeline the characters
share.** One rig serves every character precisely *because* they share one skeleton. Introducing a character
that does not conform forces exactly one of three outcomes, and all three are bad:

- **Fork the rig.** Now there are two solvers to author, tune and keep in agreement. Every animation change is
  made twice, and the two drift — silently, because nothing compares them. This is the same multiplication
  argument that makes per-weapon beam systems wrong (`ssot/combat-arsenal.md` §5).
- **Branch the animation graph on character type.** Every downstream consumer — foot planting, weapon hold,
  hit reactions, movement states — acquires a conditional. The conditional is invisible in the common path and
  wrong in the rare one, which is the worst possible distribution for a defect.
- **Exempt the character from FBIK.** It then visibly does not plant its feet or settle its pose like every
  other character in the same match. **This is not a subtle difference at gameplay range** — foot sliding and
  a floating pose are among the most legible tells of a lower-production-value character, and the exempt one
  is standing next to conforming ones for comparison.

**The asymmetry that makes this permanent:** conforming characters can always be added to a conforming system
at zero marginal cost. A non-conforming character cannot be retrofitted — accepting one converts a single
shared contract into a per-character concern **for every character that already exists**, including the ones
authored before the exception was contemplated. The cost is not paid by the new character; it is paid by the
whole roster, forever.

### 3.4 The isolation rule — FBIK is additive

**The full-body solve is layered as an additional evaluation on top of the existing rig; the shared rig that
drives weapon and grab IK for every character is not modified.**

The reason is blast radius. The shared rig's controls are consumed by every character in the project. A change
to it to accommodate the full-body solve is a change to **all** of them, and the resulting regression does not
present as "the FBIK work broke something" — it presents as weapon hold breaking on unrelated characters, in
unrelated modes, discovered later by someone else.

**The general form:** *when adding a solve on top of a shared rig, add an evaluation; do not re-pin the shared
graph.* An additive node can be removed; an edited shared graph cannot be un-edited without knowing everything
that came to depend on the edit.

---

## 4. THE TWELVE LOCKED RULINGS AS SYSTEM DESIGN

These are operator rulings, restated with the reasoning that makes them load-bearing. **An agent finding
apparent contradicting evidence reports the conflict and stops — it does not resolve the conflict by changing
the ruling.**

| # | Ruling | The system reasoning |
|---|---|---|
| **L1** | **The shared mannequin rig is *the* rig.** Every body and head conforms to it. No MetaHuman, no variant rig, no alternative skeleton | This is **A11**'s precondition. One skeleton is what makes one FBIK rig, one animation set and one retarget possible (§3.3) |
| **L2** | **Pro Mod never dismembers.** Gore-free is the mode's design, achieved structurally | See §4.1 — the *mechanism* is an open question |
| **L3** | **Melee / hand-to-hand is cut** | ⚠ **Superseded in scope by R1:** melee is cut **project-wide**, not only for Pro Mod characters. L3's framing as a per-line decision is narrower than the standing ruling. This is a shooter |
| **L4** | **Two character lines ship alongside each other.** Neither replaces the other | The fork is a product decision, not a migration. §5 |
| **L5** | **Visors and facemasks are one line's axis only.** The other line uses mask *geometry* plus face stickers | The axes are **geometry-bound**: a visor needs visor geometry, a mask needs a head. Extending one axis across both lines would mean authoring geometry that fits neither |
| **L6** | **Preset heads, parametric everything else.** A preset roster, not face sliders | Sliders author a face nobody reads at combat range. Variation is spent where it *is* legible: colour, stickers, accessories, number. See §8.4 |
| **L7** | **Sticker placement is per-zone, capped at 9 total** | The cap is what makes the placement array fixed-size and therefore replicable. §7.2 |
| **L8** | **The character name sits alongside the account display name; the player chooses it** | The identity ledger is `account → characters owned → character names`. §8.3 |
| **L9** | **Character saves are a counted entitlement** | ⚠ **Superseded in quantity by R16:** the free allocation is **two**, not one. The *counted-entitlement shape* stands unchanged. §8.1 |
| **L10** | **Character mods do not carry between lines.** Shared palette data, forked SKUs | §5 — the ruling this document devotes a whole section to |
| **L11** | **The neon set is one set, not two.** The brief's colour axis and the character line's colour set are the same set | Two names for one set is the §2 problem in the design layer: it produces two authoring efforts and two sources of truth for one thing |
| **L12** | **Masks and stickers are both sold and granted, and how a player combines them is their preference** | The system **permits** combinations and does not adjudicate taste. A system that blocks "wrong" combinations is imposing a designer's aesthetic on a customisation product — the exact opposite of what the product sells |

### 4.1 L2 versus the mode gate — an unresolved reading

**Stated, not resolved.** L2 says gore-free is achieved *structurally* and that there is no dismemberment gate
to build. A **mode gate also exists** as a tag consulted in the damage path. These are two coherent readings of
the same design and they lead to opposite conclusions:

- **Reading A — structural, so the gate is redundant.** The gore-free line drops the dismemberment feature and
  uses a pawn without the gib component, so nothing dismember-related is loaded at all. Under this reading the
  gate is belt-and-braces: harmless, but removable, and L2 is exactly correct as written.
- **Reading B — the gate is load-bearing.** The dismemberment system has **two decoupled layers**: zone-based
  damage *routing*, and the gib/consequence layer. Removing the component removes the second. Under this
  reading only the tag removes the first, so the gate is the sole thing delivering conventional single-health
  damage — and removing it would silently restore zone routing, whose known failure mode is limb and head
  shots dealing zero damage against unseeded zone health.

**Why this must not be resolved by inspection alone:** the two readings differ on *whether the zone-routing
layer is active when the gib layer is absent*, and both are consistent with the design as documented. The
question determines something concrete — **whether that gate can ever be removed** — and answering it wrongly
in the removing direction produces a defect that presents as a damage-tuning problem, not as a mode problem.
Recorded as an open question (§13.1).

---

## 5. THE TWO-LINE FORK — PALETTE DATA SHARED, SKUs FORKED

> **The colour *values* are shared. The *entitlements* are forked.**

| Layer | Shared or forked | Why |
|---|---|---|
| **Palette data** — the colour values themselves | **SHARED.** One registry, one resolver | A colour is authored once |
| **Entitlement SKUs** — what a player buys | **FORKED.** Each line has its own SKU line | The looks are different authored art |
| **Geometry-bound axes** — face, chest mark | **FORKED by construction** | A visor needs visor geometry; a mask needs a head |
| **Weapons, skins, beams** | **SHARED** | A weapon is not a character mod |

### 5.1 Why this split, and not either uniform alternative

**The two halves are different kinds of thing, and the split falls exactly on that difference.** A colour value
is a *number*. A suit look is *authored art* — fabric response, panel breakup, how the accent reads across a
flex zone. The same number produces two genuinely different authored results on a metal robot and a fabric
suit. So: **share what is data, fork what is art.**

**If the palette data were forked instead** — a second registry per line:
- A colour would be authored **twice**, and the two copies would drift. Not dramatically; just enough that the
  same colour name reads differently across lines, which is worse than an obvious difference because nobody
  can tell whether it is intentional.
- **Adding a colour would become N registry edits instead of one**, and the cost of the palette would scale
  with the number of lines — which is precisely the scaling the shared-data model exists to prevent.
- Every downstream consumer would need to know which line it is resolving for, pushing a fork into code that
  currently has no concept of lines at all.

**If the SKUs were shared instead** — one entitlement granting the look on both lines:
- **It would grant art that was never authored.** A player owning a robot finish would be entitled to a suit
  colour that is a separate authored asset. Either that asset is produced for free, or the entitlement
  resolves to nothing and the player is holding a broken grant.
- **It would silently double the authoring obligation of every colour SKU.** Adding one colour would commit
  the project to authoring it on every line, forever, with no corresponding revenue — turning a content
  decision into a compounding liability.
- It would collapse the geometry-bound axes, which cannot be shared at all (a visor entitlement has no
  meaning on a head that takes masks).

**The framing this pre-empts:** forked SKUs read as a monetisation grab if you assume the two looks are the
same asset. They are not — and the evidence is that the *registry* stays shared. **A monetisation grab would
fork the data too**, because that is what forces the player to buy twice for one authored thing. Forking only
where the art genuinely differs is the honest line.

---

## 6. THE RESOLUTION SPINE — HOW A CHARACTER BECOMES A PAWN

A character is not a mesh; it is a **selection resolved at spawn**. The chain is: a selector component reads
the player's selection → a part map resolves each selection key to a part actor class → the part actor spawns
and attaches → a colour controller pushes the finish onto the resolved materials.

Four properties of this design are load-bearing:

**1 — Selection is keys, never hardcoded pins.** A part is addressed by a key that a map resolves. This is
what makes a character swappable, a roster extensible by data, and a colour composable at runtime rather than
baked per combination. **G5** is the failure to watch for: a map or data asset with no consumer is inert, and
inert configuration reads identically to configuration that is wired.

**2 — Persistent selection lives on the controller, not the pawn.** A pawn dies; the controller does not. The
persistent home re-pushes the selection onto each newly possessed pawn, which is what makes an identity
**survive respawn**. Storing it on the pawn would mean every death resets the player's appearance — a defect
that only appears after the first death, i.e. never during a quick check.

**3 — Server resolves, then replicates; clients never decide.** The authority sets the replicated value and
all clients converge (**N1**, **N11**). A client-side apply would produce a character who looks correct only
to themselves.

**4 — Composition order is part of the contract.** Where one step *swaps a material* and another *pushes
parameters onto it*, the swap must happen first — otherwise the parameter push lands on the material that is
about to be replaced, and the finish is silently stranded. **Every step in the spine is idempotent**, so
redundant re-runs from multiple triggers (possession, replication callback, a part arriving late) converge
instead of stacking.

### 6.1 Pawn variants: a sibling, not a child

A mode variant that must **remove** an inherited component cannot be a child Blueprint — **an inherited
component cannot be deleted in a child.** The variant is therefore a **duplicate**, which makes it a
**sibling** of the original rather than a descendant.

That has a direct consequence via **G6**: component-adding actions match by class **including subclasses**, so
entries targeting the original class **do not apply to the sibling**. Everything the variant needs must be
attached to the variant explicitly. **The failure mode is a variant that spawns correctly and is quietly
missing components** — no error, since nothing was expected to match. See also **X7** (the property casing
that makes this authoring silently return nothing) and **X10** (a duplicate must point at its own assets).

---

## 7. THE STICKER COMPOSITE SYSTEM

### 7.1 A bake, not live decals

**Placed stickers are composited into one texture per character; the character wears a single material.
One draw path regardless of sticker count.**

Three reasons, in order of weight:

- **Cost.** N live decal components × a full lobby is a shipping-grade per-frame cost that scales with
  *player creativity* — the worst possible scaling, because it is unbounded by anything the design controls.
- **Correctness.** Decals projected onto skeletal meshes **bleed across adjacent geometry** — a chest sticker
  appearing on an arm that swings past it. There is no tuning that fixes projection onto a deforming surface;
  it is the wrong tool.
- **Composition.** A bake puts face tattoos on the same rail as body stickers, and makes mask occlusion
  resolve naturally — mask geometry simply sits over a baked texture, rather than needing to interact with a
  projector.

**The cost moves rather than disappearing:** from per-frame draw to **memory (one target per character) and
bake time** at spawn and on change. Both are bounded — and what bounds them is the cap.

### 7.2 The cap is a structural property, not a limitation

Placement is capped per zone, nine total. **What the cap buys:**

- **The placement array is fixed-size and small** → it replicates directly, with no compact-reference
  indirection and no unbounded net traffic.
- **One sticker per small zone** → each zone is a single clamped UV rect, with **no overlap resolution and no
  z-ordering** to design, implement or explain.
- **The bake has a known worst case** at full lobby size, so the memory and time cost are bounded numbers
  rather than an open question.

> **Slot counts per zone are a tunable product lever. The fixed-size-array property is not.** Going unlimited
> does not "relax a limit" — it reopens the replication problem the cap solves, and re-introduces overlap and
> ordering as design work.

**The cap is also surfaced to the player, deliberately.** Showing a zone's usage makes each placement feel
considered and makes an extra-slot SKU legible rather than predatory. A hidden cap is experienced as the
product failing; a visible one is experienced as the product having a shape.

### 7.3 Net-serialisation residency — why this system is the likeliest to violate it

**N8** requires every net-serialised struct to live in the always-loaded net-types module, never in a
GameFeature module. The law is cited, not restated. **This section explains why the sticker system is the one
most likely to break it.**

Three factors compound here and nowhere else:

1. **This is the only genuinely new *data shape* in the character system.** Everything else the cosmetic
   system does is **selection** — pick an id, validate entitlement, replicate a fixed set of name keys. The
   creator introduces **player-authored data**: a name, and placements that are **transforms**. Neither is an
   id, so neither fits the existing replicated selection, and a new net-serialised struct must be authored.
   **A new struct is the only way to violate N8**, and this is the system that needs one.
2. **Every instinct points at the wrong module.** The struct is *about* stickers; the sticker system lives in
   a GameFeature; the zone definitions, the bake and the UI all live there too. Declaring the struct beside
   the code that uses it is the natural, tidy, and locally-correct choice — and it is the violation.
3. **The failure is invisible to the way this work is naturally checked.** A GameFeature module can unload and
   take its serialisation-cache registration with it, desyncing the cache and dropping connections.
   **Single-client testing never surfaces this** — and a character creator is overwhelmingly developed and
   inspected single-client, because placing a sticker and looking at it needs exactly one client.

> **Therefore: the struct is declared in the net-types module on day one. Not "authored locally and moved
> later."** By the time the defect appears, the struct has consumers, a replication surface and saved data —
> and the move becomes a migration rather than a decision.

### 7.4 The zone UV contract

Each zone is a defined UV region on the suit, with placement **clamped** so a sticker cannot cross a seam or
land on invalid surface.

**This is what makes placement feel considered rather than fiddly.** A clamped zone means the player cannot
produce a bad result — every position they can reach is a position that looks right. Without it, most of the
placement space is subtly wrong (crossing a seam, wrapping a limb, distorting across a UV discontinuity) and
the interface silently allows all of it, so the player's experience is of a tool that lets them fail.

**It is suit-authoring work that lands with the base body, not after.** The zones are a property of the suit's
UV layout; a suit authored without them cannot have them added without re-authoring the layout, and every
sticker placed under the old layout moves.

---

## 8. THE CREATOR AND ITS SLOT ECONOMY

### 8.1 Slots — two free, additional as an upsell

> **Two saves are free. Additional slots are an upsell, as a counted entitlement.**

**Slot axes: character · sticker · emblem · weapon.** Each axis is separately extensible, because each answers
a different limit a player hits: how many *identities* they keep, how much they can *decorate* one, and what
they can *bring* with it.

**Two free rather than one, and why the number matters:** one free save makes every experiment destructive —
the player must overwrite the character they already like to try anything. Two makes the creator **safe to
explore**, which is the behaviour that produces attachment, and attachment is what makes a third slot worth
buying. A single free slot optimises the wrong end: it maximises pressure to buy before the player has any
reason to care.

**Slots attach to the account, not to a character.** This is what makes the entitlement coherent — a slot is
capacity, and capacity that vanished when you deleted a character would be a refund problem, not a product.

**A counted entitlement, not a parallel inventory.** Slots are a **quantity on the one registry and the one
persistence seam** (`ssot/economy-store.md` §9) — the same shape as any other counted item. **N11** applies
without exception: the client never decides how many slots a player has.

**Any of the base neons is always available as a default.** A player who owns nothing still gets a character
that looks deliberate. An empty creator that produces a grey default teaches the player that the good version
is behind a purchase, before they have any reason to want one — and first impressions of a customisation
product are made by the *default*, since that is what everyone sees first.

### 8.2 Stickers remain swappable at loadout — the reference rule

> **A saved character stores a sticker selection as a REFERENCE to an owned item, never a baked copy of it.
> Stickers remain swappable at loadout from the player's owned catalog.**

**The failure a baked copy produces:** a player buys a new sticker and **cannot use it on a character they
already saved.** Their existing characters — the ones they care about, the ones they have played — are frozen
with whatever they owned at save time. The player's rational response is to conclude that buying cosmetics is
pointless unless they are also willing to rebuild a character, which is precisely the conclusion a cosmetics
economy cannot afford them to reach.

**What the reference rule implies, concretely:**

- **The save stores `(sticker id, zone, transform)` — an id plus placement, never the sticker's content.**
  The id resolves through the catalog and entitlement at load, exactly as every other cosmetic selection does
  (§6). The character record is a set of *choices*, not a set of *assets*.
- **Entitlement is validated at placement and again at resolve.** Ownership can change between save and load,
  so a saved reference is a claim to be re-checked, never a grant already made (**N11**).
- **The composite (§7.1) is a derived artefact, and derived artefacts are rebuilt, never stored as truth.**
  The bake is regenerated from the references at spawn and on change. If the bake were the save, the bake
  would be the baked copy this rule forbids — the same defect wearing a technical justification.
- **A newly-purchased sticker is immediately usable on every saved character**, because the saves never
  captured the old catalog in the first place.
- **The swap surface is the loadout**, not the creator. Changing a sticker must not require re-entering
  character creation — it is an equip-time decision, like every other owned cosmetic.

**The general form, which applies past stickers:** *save what the player chose, not what the choice resolved
to.* A record of resolved content is a snapshot of an entitlement state that will not stay true.

### 8.3 Name and the identity ledger

**The player chooses the character name, and it sits alongside the account display name rather than replacing
it** (L8). The authoritative chain is `account → characters owned → character names`.

**The account is the durable key. Character names are display-layer and re-nameable.** Therefore:

> **Never key entitlement, ownership or a trade record off a character name.**

**NM5** (a shipped id is never renamed) applies to the account and the character *record* — **not** to the
player-authored display string, which is designed to change. Keying anything durable off a mutable string
means a rename silently orphans it, and the orphaning is discovered by the player, later, as lost property.

**A player-authored name that other players see requires a profanity filter, a uniqueness rule and a report
path.** These are not polish. A free-text field visible to strangers is a moderation surface the moment it
ships, and retrofitting moderation onto names already in circulation means changing names players have
already attached to.

### 8.4 Creator design properties

Three properties carry a creator, in order:

1. **The preview is the product.** What the player sees in the creator is exactly what spawns — same pawn,
   same lighting, same materials. Any divergence reads as a bait-and-switch the first time they load into a
   match, and it costs more trust than the creator ever earns back.
2. **Placement is physical, not numeric.** Drag on the surface, snapped and clamped to the zone (§7.4).
   Numeric fields are advanced disclosure, never the primary interaction — a creator that opens with
   coordinate entry has told the player it is a tool, not a toy.
3. **Every choice is reversible without loss.** Undo on placement, revert-to-saved on the character. **A
   creator that punishes experimentation gets used once**, and the slot economy (§8.1) depends on the opposite
   behaviour.

**Preview at combat range, not only at portrait range.** The accent lighting is the strongest identity signal
at the distance players actually see each other; faces and stickers stop being legible long before it does.
A creator that only previews close sells looks that vanish in play. For the same reason, **any team-colour
override must be visible in the creator** — the player learns their accent is overridden there, not in their
first match.

**No paywall inside the creation flow.** Unowned items surface in the store; the creator shows what the player
owns, with a single route to get more. Greying out desirable options inside the authoring flow converts a
creative act into a sales pitch at the exact moment the player is most invested.

---

## 9. IDENTITY PRODUCTION — CONCEPT SHEET TO ACCEPTED ASSET

**The concept sheets are the spec.** An identity is not designed in the engine; it arrives with its look
already decided, and production conforms to it. This is what keeps a roster coherent — the alternative,
deciding each identity's look during its build, produces a set that reads as N separate decisions.

### 9.1 What one identity is

> **One identity = a distinguishing mark + colour-neutralised body materials + a default finish + complete
> registration.**

**Colour comes from the finish, never baked into the identity's materials.** The identity's materials are
authored **neutral**, and colour composes at runtime. This is the whole economy of the system: N identities ×
M colours costs `N + M` assets rather than `N × M`, and adding a colour does not touch a single identity.
**Baking colour into an identity is the one decision that destroys that arithmetic**, and it is tempting
precisely because it looks simpler for the first identity.

**The mark is the per-identity differentiator.** Where colour is shared and composable, the mark is what makes
one identity distinguishable from another wearing the same finish — which is the situation in every match.
**C2** applies here as directly as it does to weapons: if two identities read the same at gameplay range, one
of them is not an identity.

### 9.2 Complete registration — all of it or none of it

An identity is accepted only when **every** registration is present. **Nothing ships riding a fallback.**

The registrations span: the identity's tag, its part-map entries on every axis it claims, its mark-to-finish
mapping, its character actor, its neutralised materials, its mark texture, and its catalog rows.

**Why "all or nothing" rather than "mostly registered":** a partially-registered identity **resolves through a
fallback and looks fine.** It renders, it spawns, it is playable — and it is wearing another identity's
signature finish, or resolving on one axis and not the other. The defect is not visible as a defect; it is
visible as an identity that looks slightly generic, which is indistinguishable from an identity that was
designed to look that way.

**Two mechanical properties of registration that shape the whole process:**

- **Tags register at startup.** A freshly-added tag does not exist until the editor is relaunched, and
  authoring silently **drops** an unregistered tag. So every tag-keyed registration is necessarily a
  *second pass* — this is a sequencing constraint on the production line, not an inconvenience to work around.
- **Structured writes replace wholes, not parts.** In-place edits to elements of a structured array do not
  persist; the array is rebuilt and reassigned. Aliasing a source row instead of deep-copying it **renames the
  source** (**X10**'s general form: a duplicate that still points at its source is a live bug class).

### 9.3 Canary before batch

**One identity is completed and watched before any batch runs.** The first of anything establishes the
recipe; the rest are volume on an established line.

The reasoning is about *when a defect is discovered*, not about caution: a systematic error in the recipe is
identical in cost whether it is in one identity or thirty — **until it is in thirty**, at which point it is a
thirty-item correction with thirty chances to miss one. The canary converts a possible batch-wide rework into
a single-item fix.

---

## 10. THE COLOUR / PALETTE AXIS

Colour is **one axis among the independent ownable cosmetic axes** defined in `ssot/economy-store.md` §6 — not
restated here. Its character-specific properties:

- **One registry row per colour, resolved by tag.** A colour is a data row, not an asset per surface. Adding a
  colour is one row plus its presets; it touches no identity and no body (§9.1).
- **A colour is not one value.** A finish resolves several related values — base, accent, edge treatment.
  A colour that has a tag and a primary value but **no edge-ramp preset** is *partially* authored: it
  resolves, it renders, and it does not read like its siblings. **A partially-authored colour is the §9.2
  failure in the palette layer** — present enough to pass inspection, incomplete enough to look cheap next to
  the others.
- **One set, not two** (L11). Where a brief and a line each name a colour set, they name the same set. Two
  names for one thing produce two authoring efforts and two sources of truth.
- **The accent is a first-class choice, not a sub-option of body colour.** At gameplay range it is the
  strongest identity signal (§8.4), which makes it the most valuable single customisation choice in the
  system — and burying it inside another picker prices it as an afterthought.

---

## 11. NAMING

**Where a plan's placeholder name and the shipped asset name disagree, the shipped name is canonical and the
plan bends to it** (**NM6**). Pawn-data assets follow the project's shipped convention rather than any
document's proposed one, and a document naming an asset that does not exist under that name is describing
nothing.

Two consequences:

- **Read the artefact before specifying a name.** A spec that invents a name creates a second address for a
  thing that already has one, and every consumer then has to know which is real.
- **A shipped id is never renamed** (**NM5**), including one that looks wrong. Internal codenames and
  player-facing names deliberately differ; renaming to make them agree breaks resolution for the sake of
  tidiness.

Prefix and namespace conventions are **NM2** and **NM3**.

---

## 12. INTERFACES

### 12.1 What the character system OWES the economy

**The cosmetic SKU shape.** Every character-side cosmetic — identity, colour, mask, sticker, emblem,
accessory, slot — is expressed as an entry with: an **id** (stable, never renamed, **NM5**), an **axis**
(which independent category it belongs to, `ssot/economy-store.md` §6), a **content tier and rarity**, a
**collection** grouping, and its **commerce attributes**.

**The character system supplies the shape and the ids. It does not price, grant or persist.** Grants go
through the economy's grant interface and the single persistence seam (`ssot/economy-store.md` §9, §14.2) —
including character saves and slots, which are ordinary counted entitlements and **not** a parallel inventory.

**One additional property the character system owes:** a saved character record is **references, not content**
(§8.2), so the economy remains the sole authority on what a player owns and a saved character never asserts
ownership on its own.

### 12.2 What the character system CONSUMES

- **From the economy:** entitlement answers — may this player select this id — validated on the server at
  **selection**, at **placement**, and again at **resolve** (**N11**). Never a client assertion.
- **From the match layer:** the mode's pawn data and any team-colour override that supersedes the player's
  accent for readability (§8.4).
- **From the persistence seam:** the character record on load — part selections, finish, placements, name.

### 12.3 The lane and trap discipline that governs this work

**Not restated here.** Lane discipline is DOCTRINE §7; the banked traps are DOCTRINE §11. The ones that bite
this system most directly are cited in place above: **X7** (component-action property casing), **X8** (resolve
LFS before comparing blob content), **X10** (duplicates must point at their own assets; never duplicate a mesh
or skeleton carrying operator-placed sockets), **X11** (bone names differ; verify weights on twist bones),
**G4** (`/Game` must not reference GameFeature content), **G5** (a data asset with no consumer is inert), and
**G6** (component actions match subclasses, so a duplicate is a sibling — §6.1).

---

## 13. OPEN DESIGN QUESTIONS

1. **The L2 dismember-gate reading** (§4.1). Whether gore-free is achieved structurally — making the mode gate
   redundant and removable — or whether the gate is the only thing disabling zone-based damage routing and is
   therefore load-bearing. **It determines whether that gate can ever be removed**, and the failure mode of
   guessing wrongly is a damage defect that does not look like a mode defect.
2. **Slot pricing.** The free allocation is two and additional slots are an upsell (R16); what a slot costs,
   and whether the four slot axes price alike, is undecided. Interacts with the pricing ladders in
   `ssot/economy-store.md`.
3. **Whether emblems and stickers share a selection surface.** They are distinct axes (§12.1) but behave
   similarly — both are placed marks resolved from owned items. One surface is simpler to learn; two keep the
   axes legible as separate purchases. Interacts with the loadout surface, not only the creator.
4. **How a saved character behaves when a referenced cosmetic is revoked or expires.** The reference rule
   (§8.2) makes this reachable by construction: a save can point at something the player no longer owns.
   Options span silently substituting a default, showing the character as incomplete, or blocking the save
   from being equipped — with very different implications for a player who lent, traded or lost an item.
   **Whatever is chosen must not delete the reference**, or re-acquiring the item will not restore the
   character.
5. **Preset head coverage versus the two-line fork.** L6 fixes heads as a preset roster; §5 forks
   geometry-bound axes per line. How much head coverage each line carries — and whether a head authored for
   one line has any meaning on the other — is undecided, and it sizes a large share of the character-art
   workload.

---

## 14. RULINGS OF RECORD

| Ruling | Date | Content |
|---|---|---|
| **R16 — Creator slot economy** | **2026-08-05** | **Two character saves are free; additional slots are an upsell**, as a counted entitlement on the one registry and persistence seam (§8.1). **Supersedes L9's one-free allocation**; the counted-entitlement shape is unchanged. Slot axes are **character · sticker · emblem · weapon**, and slots attach to the account, not to a character. **Any base neon is always available as a default**, so a player who owns nothing still gets a deliberate-looking character. **Stickers remain swappable at loadout from the owned catalog**: a saved character stores a **reference** to an owned item, **never a baked copy** — a baked copy means a player who buys a sticker cannot use it on an existing save (§8.2). |
| **R17 — The two-line fork** | **2026-08-05** | **Palette DATA is shared; entitlement SKUs are forked** (§5). Share what is data, fork what is art: a colour value is a number authored once, while a suit look and a robot finish are separate authored assets. Forking the data would multiply authoring per line and let the same colour drift; sharing the SKUs would grant art that was never authored and make every colour a compounding obligation. Geometry-bound axes are forked by construction; weapons are shared, because a weapon is not a character mod. |

> **D16 (FBIK is permanent doctrine) mints no new ruling here** — it is already **A11** in DOCTRINE, on
> operator ruling **R5**. §3 explains the system; the law is cited, not restated.

---

## 15. RELATED

- [`Docs/DOCTRINE.md`](../DOCTRINE.md) — laws cited here: **A11** FBIK is permanent doctrine · **N1**/**N11**
  server authority and the client never decides ownership · **N8** net-serialised structs live in the
  always-loaded module · **G4**/**G5**/**G6** GameFeature partition, inert data assets, subclass matching ·
  **NM2**/**NM3** prefixes and namespaces · **NM5** a shipped id is never renamed · **NM6** the shipped
  artifact is canonical over the doc · **C2** no generic reskins · **X7**/**X8**/**X10**/**X11** banked traps.
  Lane discipline is DOCTRINE §7; the full trap set is DOCTRINE §11.
- `ssot/economy-store.md` — the cosmetic axis model (§6 there), the single persistence seam (§9 there), and
  the grant interface every character entitlement flows through (§14.2 there).
- `ssot/combat-arsenal.md` — §3.1 there is the design-vocabulary-versus-disk-vocabulary lesson §2 here
  extends; §3.3 there is the shared over-stretch failure that §3.1 here cites.
- `ssot/match-modes.md` — the rulesets whose pawn data and team-colour overrides the character system
  consumes.
- `ssot/league-play.md` — earned-only rewards, of which character-side prestige items are one kind.
