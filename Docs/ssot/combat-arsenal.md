# SSOT — COMBAT + ARSENAL (Tier 2)

**What this is:** what the combat and weapon systems **are** and **why**. It changes when the system is
redesigned.
**What this is not:** a status board. **This document contains no status claims** — nothing here says what is
built, proven, done or owed, and no commit hash appears as evidence of progress. Those belong to Tier 3
(`LIVE_TRACKER`).

> **Why the separation is hardest to hold in this domain.** More of this system's design lives in C++ header
> comments than in any other, and header comments are written in the voice of the work that produced them
> ("proven", "harvested", "shipped"). That voice is **provenance**, not design. This document keeps the
> reasoning and drops the provenance — a design rule does not become more true because a particular commit
> demonstrated it.

**Doctrine is cited, never restated.** Laws live in [`Docs/DOCTRINE.md`](../DOCTRINE.md) and are referenced by
id (**A3**–**A10**, **N1**–**N9**, **T1**–**T6**, **C1**, **C2**, **NM2**, **NM3**, **X2**).

---

## 1. SCOPE

This SSOT governs: the arsenal tag contract as a system · the weapon data chain and the authoring contract for
a new weapon · the two ability families · the beam contract · lag compensation · combat telemetry ·
deployables · damage flow · cosmetic axes that touch combat · and which weapons are map-restricted.

It does not govern: currency, prices or entitlement grants (→ `ssot/economy-store.md`), what a ruleset is
(→ `ssot/match-modes.md`), how matches are formed (→ `ssot/matchmaking.md`), where weapons are placed in a map
(→ `ssot/map-build-system.md`), or the state of any implementation (→ Tier 3).

---

## 2. THE ARSENAL TAG CONTRACT — THE SYSTEM BEHIND THE LAW

**T1**–**T6** state the contract as law. This section explains the system those laws protect, and why each
failure mode is *silent*. That silence is the whole problem: none of these defects raise an error, log a
warning, or fail a build. **Every one of them presents as a weapon that simply works slightly wrong, in play,
under pressure.**

### 2.1 Match-phase gating is what makes a phase freeze real

Every weapon ability **and every movement ability** blocks on the match-phase tags (**T1**). The 30-second
pre-match warmup and its once-per-match placement are defined in `ssot/match-modes.md` §5 — not restated here.

**Why this is a system property rather than a per-ability nicety:**

The freeze has no central enforcer. There is no gate object that abilities pass through and no subsystem that
suppresses input during warmup. The freeze is *nothing but* the sum of every ability's `ActivationBlockedTags`.
It follows that **an ability which omits the gate is not partially frozen — it is not frozen at all**, and
neither is anything else about the phase changed to reveal that.

**The consequence, stated concretely:** an ability missing the warmup tag gets an **uncontested window**. During
a phase in which every other participant is frozen by design, that one ability fires into a field of players
who cannot answer. In a staked match this is not a bug report, it is a disputed result — the loser's complaint
is correct, and the evidence that they are correct is a tag absent from a data asset.

**The design conclusion:** the gate set is not per-ability configuration to be reasoned about case by case. It
is a **contract every ability signs on creation**, and the only safe posture is that a new ability carries the
full blocked-tag set by inheritance from the family base rather than by each author remembering to add it.
An author who *must remember* will eventually not.

### 2.2 One concept, one tag — and why two tags is worse than none

Heat has **one** tag (**T2**). The failure mode of splitting one concept across two tags is worth stating
precisely, because it is counter-intuitive:

Suppose a concept — overheating — is expressed as two tag names. Some abilities block on tag A; others block
on tag B. The heat system grants exactly one of them, say A.

- The abilities gating on **A** behave correctly. Nothing looks wrong.
- The abilities gating on **B** are **silently never gated**. Their `ActivationBlockedTags` entry is real,
  well-formed, and inert — a gate installed across a doorway that nothing ever closes.
- **There is no error.** Blocking on a tag nothing grants is legal GAS. No log line, no assertion, no failed
  cook. The ability simply never overheats, and the only symptom is a weapon that out-sustains its design.
- Worse, the *presence* of the entry actively conceals the defect. Anyone auditing the ability sees an
  overheat gate and concludes overheating is handled.

**Two tags for one concept is therefore more dangerous than no tag at all**, because a missing gate is visible
to inspection and an inert gate is not. This generalises past heat: **a concept that can be blocked on gets
exactly one tag name, and that name is declared once alongside its peers** (**T4**, **NM3**).

**T6** is the operational form of the same insight: *a tag-gated lockout is only real if something grants the
tag.* Verifying the ability blocks proves nothing — the granting effect is the half that can be absent.

### 2.3 Cooldowns are per-weapon because GAS gates on the granted tag

Every weapon owns its own cooldown effect granting its own `Cooldown.Weapon.<Name>` (**T3**). Cooldowns are
GameplayEffects, never timers or floats (**A6**), and live on the ability's CDO rather than in the AbilitySet.

**The mechanism that forces per-weapon-ness:** GAS does not evaluate "is *this ability* on cooldown." It
evaluates "does the owner **have the cooldown tag** this ability's cooldown effect grants." The tag is on the
*owner*, not on the ability.

So if two abilities share one cooldown effect, they share its tag — and therefore **one shot from either
weapon puts every ability pointing at that effect on cooldown**. The player fires weapon A and weapon B
refuses. Nothing is broken in any single asset; the coupling is entirely in the shared reference.

**Why this is easy to introduce and hard to see:** a shared cooldown effect is the natural first move when
adding a weapon — the effect is right there, it has the right duration, and copying it feels redundant. The
defect only appears when a player carries two of the coupled weapons and swaps between them under pressure,
which is exactly the situation least likely to be reproduced deliberately.

**The authoring rule that prevents recurrence:**

> **A new weapon's cooldown effect is created with the weapon, granting a tag named for the weapon.
> Pointing a new weapon at an existing weapon's cooldown effect is never correct — not as a placeholder,
> not "for now."** Duration may be inherited; **the tag may never be shared.**

Stated as a property rather than a procedure: the map from weapon to cooldown tag is **injective**. Any two
weapons sharing a cooldown tag is a defect by definition, without needing to know what either weapon does.

### 2.4 The contract is a census-checkable property

> **The arsenal's entire tag surface must be verifiable from disk, without running the game.**

Every element of §2.1–§2.3 is a statement about *files*: which tags appear in which ability's blocked set,
which tags are declared, which effects grant which tags, and whether the weapon→cooldown-tag map is injective.
None of it requires a play session to check.

**Why this property is required rather than convenient:**

- **Drift here is silent** (§2.1–§2.3). Every defect in this contract has the same signature: no error, no log,
  correct-looking data. The class of bug that announces itself can be found by playing. This class cannot.
- **A play session samples; a census covers.** Verifying tag gating in play means reproducing the exact
  conditions — the right phase, the right weapon, sustained fire, a second coupled weapon in the loadout — for
  every ability in the arsenal, against a combinatorial space that grows with every weapon added. A disk
  census reads every ability once.
- **It only surfaces in play, and play is when it is most expensive.** These defects manifest during matches,
  which for staked play means they manifest as disputes (§6.4). A check that can run before a build is worth
  far more than a check that runs after a complaint.
- **It makes the contract enforceable by something other than discipline.** A property checkable from disk can
  be checked automatically and repeatedly; a property checkable only in play is, in practice, checked when
  someone remembers.

**The census is over four questions**, each answerable by reading files:

1. Does every weapon and movement ability carry the full match-phase blocked set? (**T1**)
2. Does every tag referenced by any ability appear in a declared tag file? (**T4**)
3. Is every blocking tag granted by something? (**T6**)
4. Is the weapon→cooldown-tag map injective? (**T3**)

**The census must span both halves of the arsenal.** The tag surface is **not** wholly in C++: an ability base
declares part of the blocked set in code, while per-weapon assignments — which cooldown effect a weapon points
at, which effect grants which tag — live in **data assets under `Equipment/`** (§3.1). A census that greps
source only covers the half that was never the risk. **The per-weapon half is where the defects in §2.2 and
§2.3 are introduced**, because that is the half authored per weapon, by hand, most often. That the data half is
binary makes a census *harder*, not optional — it is the argument for tooling that can read it, not an
argument for confirming the contract in play.

---

## 3. THE WEAPON DATA CHAIN

### 3.1 Where the arsenal lives — "arsenal" and "equipment" are one thing

**The arsenal is addressed as EQUIPMENT on disk.** The weapon chain's data assets live under an
**`Equipment/`** content folder in the content GameFeature, with ability sets alongside the combat plugin's
`Sets/`. The two words name the same system from two directions: *arsenal* is the design view (what a player
can fight with), *equipment* is the engine view (what a pawn can have attached and granted).

Two consequences worth stating, because both are easy to get wrong:

- **A search for the arsenal that looks for "weapon" finds a fraction of it.** The chain's assets are named by
  their engine role, not by the word *weapon* — anyone auditing, censusing (§2.4) or porting the arsenal must
  address it as equipment or they will silently cover only the C++ half.
- **The folder is the address; the address is stable.** Shipped ids are never renamed (**NM5**), so the
  location convention is as load-bearing as the naming convention (**NM2**) — a reorganisation that moves the
  arsenal breaks every reference that resolves by path.

### 3.2 The chain, and why it has this many links

A weapon is **Blueprint children plus data assets**, never assembled from raw C++ (**A2**). Abilities reach the
pawn through AbilitySets granted by the equipment definition (**A3**).

```
ID_<Name>     item definition        — what the thing IS in an inventory
   └─ equippable fragment ─────────► WID_<Name>   equipment definition
                                        ├─ ActorsToSpawn  → B_AFL_<Name>  (the display actor)
                                        ├─ AttachSocket   → the hand socket + attach transform
                                        ├─ InstanceType   → B_WeaponInstance_AFL_<Name>
                                        └─ AbilitySetsToGrant → the fire ability
B_WeaponInstance_AFL_<Name>  — the equipped runtime object; carries the anim layer + the beam-colour feed
B_AFL_<Name>                 — the display actor: mesh, materials, the `Muzzle` socket, beam component
```

**Each link exists because it varies independently of the others.** The item definition changes when inventory
presentation changes; the equipment definition when attachment or granted abilities change; the instance when
animation or cosmetic feed changes; the display actor when art changes. Collapsing any two couples changes that
have no reason to travel together — the reason a "simpler" two-asset weapon is not simpler in practice.

**The size knob is the equipment definition's attach-transform scale**, not the display actor's root scale. The
equipment system writes the relative transform onto the spawned actor and then attaches, so any scale authored
on the actor's root is **overwritten at runtime** — it survives in the editor preview and dies in play. This is
a trap of exactly the §2 shape: visible, plausible, and wrong only where it matters.

### 3.3 The authoring contract for a new weapon

A new weapon satisfies **all** of the following. The list is a contract, not a checklist of nice-to-haves —
each item corresponds to a failure that is silent or expensive.

| # | Requirement | The failure it prevents |
|---|---|---|
| 1 | Own item definition with the standard fragment set | Missing fragments remove the weapon from pickup, quick bar or reticle without erroring |
| 2 | Own equipment definition — **never** another weapon's | A shared equipment definition makes the new weapon a skin of the old one, silently |
| 3 | Fire ability extends the family base and inherits the **full** blocked-tag set | §2.1 — the uncontested window |
| 4 | **Own** cooldown effect, own cooldown tag | §2.3 — one shot gating every coupled weapon |
| 5 | A **bot-fire GameplayEvent trigger** alongside the player input path (**A9**) | Bots cannot fire the weapon; the gap only appears in bot-populated matches |
| 6 | A character fire montage, played fire-and-forget | The gun fires and the character never pulls the trigger — the ability does not inherit one for free |
| 7 | A `Muzzle` socket on the mesh, grip authored at the mesh origin | Off-origin grips force per-weapon transform tuning; the muzzle resolver falls back rather than failing |
| 8 | All teardown in `EndAbility`, for **both** end and cancel (**X2**) | Looping cues, timers and audio survive a cancelled ability |
| 9 | A mobile material variant | The platform rule is PC + console + mobile, and the gap is invisible on PC |

**Fire-and-forget montage, specifically:** a single-shot ability ends when its shot resolves. A montage played
with a wait-for-completion task is therefore blended out by the ability's own end — the kick is cut off at
precisely the moment it should be visible. The montage is fired and abandoned deliberately.

**Left-hand grip is an authored pose, not runtime IK.** A static two-handed hold is a baked pose; a runtime
solve buys nothing and introduces elbow-flip and over-stretch failure modes. A new weapon inherits the hold. If
a correction is ever wanted it is a small blend to a **reachable-near** target, never a persistent solve to a
far socket — an unreachable target does not produce a slightly-wrong hand, it produces a fighting one.

### 3.4 Two chain models — and the one that applies to a new weapon

| Model | Shape | When |
|---|---|---|
| **Own-chain** | The item equips **its own** equipment definition → own instance, own ability set, own mesh | **Any weapon that is mechanically distinct.** A new weapon uses this. |
| **Shared-base cosmetic** | The item equips a **shared** equipment definition; identity comes from a cosmetic axis | A visual variant of an existing weapon — a skin, not a weapon |

**The distinction is mechanical distinctness, not visual distinctness.** A new mesh alone does not make a
weapon; it makes a skin. **C2** is the binding constraint here: a weapon needs a distinct risk/reward profile,
and if two weapons feel the same one of them should not exist. **A distinct mesh with a shared fire ability is
the exact shape of a weapon that fails C2** — it looks like arsenal growth and delivers none.

---

## 4. THE TWO ABILITY FAMILIES

Every fire ability is **hitscan** or **projectile**. They differ in one thing — *whether the shot resolves
instantly* — and that one difference determines their entire net model.

### 4.1 What both families owe (the shared fire contract)

Both descend from a common base and inherit:

- **Net shape:** local-predicted execution, per-actor instancing, no ability replication. The client predicts
  for feel; the server decides (**N1**).
- **The blocked-tag set** — §2.1, plus the shared disable tag (§8.2) and, where the weapon has heat, the single
  heat tag.
- **The muzzle resolver.** One resolver on the base walks candidate socket names in order and falls back to the
  hand socket — **never to world origin**. A weapon whose mesh lacks a muzzle socket therefore emits from the
  hand: visibly wrong, but not a beam from the middle of the map. **Muzzle position is cosmetic only.**
  Trace and damage originate from the camera, always.
- **Cooldown on the CDO** (§2.3) and **teardown in `EndAbility`** (**X2**).

### 4.2 Hitscan — what it owes

The shot resolves the instant it is fired, so the *hit itself* is the thing that must survive the network.

- **The client traces from the camera**, packs every hit into target data, and ships it in **one** handle.
- **The server validates** — schema first, then lag-compensated confirmation per hit (§6) — and applies the
  damage effect. **The server never re-derives aim from its own view of the client** (**N2**).
- **Multi-hit is a packing concern, not a new path.** Because the server's apply step loops the handle, a
  piercing shot is "pack N hits" and a pellet spread is "pack N pellets." Neither needs a new net struct, a new
  apply path, or a new ability family — which is why the two shapes are flags on one base rather than two
  bases.
- **The net struct lives in the always-loaded net-types module, never in a GameFeature** (**N8**).

**Two orthogonal hooks generate the whole family:** the **trace shape** (single line · piercing multi-hit ·
pellet cone) and the **fire mode** (fire-on-activate · hold-to-charge-fire-on-release · sustained auto-fire).
Every hitscan weapon is a point in that grid plus tuning. **This is the reason the family is one class**: a
new hitscan weapon is expected to be data, and needing a new class is a signal that a genuinely new mechanic
is being introduced.

**Heat, where a weapon has it, is a fire-cadence mechanic**: the fire interval ramps between a cold and a hot
rate, heat accrues per shot and decays with the *gap* since the last shot — so tapping stays cool and only a
sustained hold overheats. Two properties matter:

- **Heat per shot must exceed decay across the cold interval**, or a held burst from cold never heats at all
  and the mechanic is decorative.
- **The lockout is server-validated.** Client-side heat drives *feel*; the server ramps its own copy and
  applies the lockout effect. A client that ignores its local heat gains nothing (**N1**).

### 4.3 Projectile — what it owes

The shot takes time, so the *travelling object* is the thing that must survive the network.

- **The projectile spawns on authority only** and reaches clients by replication — exactly one authoritative
  projectile. The ability stays local-predicted so the firer gets an instant montage and cue.
- **The projectile owns travel, impact and damage.** The ability is deliberately thin and carries no damage
  effect: an object that already exists in the world, with a position and a collision response, is the correct
  owner of what happens when it arrives.
- **The server aims from replicated control rotation**, never the client viewpoint (**N2**) — the same
  doctrine as hitscan, arrived at from the other direction.
- **Variants are flags, not classes:** straight · homing (server soft-locks a target and hands the projectile a
  homing target) · fixed-arc lob (launch pitched up, gravity enabled, so range is set by aiming higher).
  **Homing and arc are mutually exclusive by design** — a homing lob has no coherent feel.
- **Homing must stay dodgeable.** Homing strength is tuned so that juking or breaking line of sight defeats it.
  Past a certain acceleration a homing projectile is not hard, it is **not a fight** — the target's inputs stop
  mattering, and a weapon whose counterplay is nil is a weapon players resent rather than respect (**C1**).

**One assembly requirement generalises past this family:**

> **Any physics parameter that shapes a replicated actor's trajectory must be set on that actor's own defaults
> — not only written by the spawning ability.**

The ability's write lands on the **server's** movement component. Clients extrapolate between replication
updates using **their** copy. If the two disagree — a client extrapolating a straight line while the server
flies a parabola — the projectile visibly stutters and snaps on every correction. The failure is not a
mis-tuned value; it is **two machines simulating different physics**, and it appears only in a networked
session.

---

## 5. THE BEAM CONTRACT

> **Gameplay owns the trace and the damage. Cosmetics own the beam. They meet at a GameplayCue, and the only
> things that cross are a world-space point and a colour.**

This is **A5** in its combat-specific form. Three rules follow, all non-negotiable:

**1 — Never hand-author a beam Niagara system.** Beams come from the library, which exposes exactly two
integration parameters: the **beam end point** (a world-space vector) and the **colour** (a linear colour).
A hand-authored beam re-implements what the library already parameterises, and does so per weapon — so every
subsequent change is N changes. It also almost always re-introduces a per-tick trace inside the visual, which
is rule 3.

**2 — Never spawn the visual inside the ability.** Cues are GAS's replicated, predicted, net-decoupled cosmetic
channel: trigger one on the predicting client and it plays instantly *and* reaches everyone else with no
additional netcode. Spawning directly in the ability means owning that plumbing by hand and coupling gameplay
to art. Going through the cue is **free correctness**, and it is what makes **N9** cheap to obey — the beam
particles are never replicated because only the endpoint and colour ever cross.

**3 — Never drive a beam from anything that runs its own trace on tick.** A tick-trace visual is client-only
and unreplicated, and it will disagree with the authoritative trace. Two traces means two answers to "where did
the shot land," and the one the player *sees* is the one that is wrong.

**The weapon describes its own look.** The cue must not hardcode which system or colour to use; it reads them
from the weapon through a small visual-provider interface (beam system, muzzle system, tint, muzzle socket
name, cosmetic range). One generic cue then serves every beam weapon and **a new beam look is pure data.**

**Cosmetic range is separate from damage range**, and the separation is deliberate: the visual may be allowed
to draw further than the weapon can hurt, and conflating them makes a tuning change to one silently change the
other.

**Colour variants are parameter swaps, never new systems.** A weapon *type* — mesh, sockets, animation class,
one visual system — is a real build. Its colours are values on that one system. The rule is stated as a
prohibition because the failure is over-production: authoring N systems for N colours produces N things to
maintain and fix, in exchange for nothing a parameter could not deliver.

---

## 6. LAG COMPENSATION AS A SYSTEM

Hit confirmation answers one question: **was the target where the shooter saw it, at the time the shooter
fired?** Everything below exists to answer that without trusting the shooter.

### 6.1 The rewind window and the RTT cap

The server rewinds to `now − ClampedRTT`, where `ClampedRTT = min(serverRTT/2 + interpolation, 200 ms)`
(**N4**).

- **The RTT is server-measured, never client-claimed.** A client-supplied latency figure is a request to rewind
  further, and a shooter who can choose how far the world rewinds can shoot at where targets *used to be*.
- **The cap is the design decision.** Rewind is a transfer of fairness from the target to the shooter: the
  further back the world goes, the more often a target is hit after they believed they had taken cover.
  A hard ceiling bounds that transfer, and bounds it **identically for everyone** — a player on a bad
  connection gets accuracy up to the cap and no further. Uncapped rewind makes latency a *weapon*.
- **The history window exceeds the cap** (≈1.2 s of history against a 200 ms cap) so that a legitimate request
  near the ceiling still lands inside recorded history rather than clamping to the oldest sample.

### 6.2 Ring-buffer ownership — per-pawn, sampled after physics

Each pawn owns its own fixed-size ring of bone snapshots; a world-level subsystem owns only the **registry** of
those components and reads them on demand.

- **Per-pawn ownership** means lifetime is trivially correct — a pawn that is destroyed takes its history with
  it — and sampling cost scales with pawns rather than with a central structure everything contends on.
- **Sampling happens after movement, cloth and IK have finalised bone transforms.** A snapshot taken earlier
  records poses that were *never rendered*, so the server would confirm hits against a body that no client ever
  saw.
- **Snapshots store world transforms, not bone-local ones.** The rewind path is on the hot path with a
  microsecond-scale budget; re-resolving a parent chain at confirm time would dominate it.
- **Only the server samples.** The component is added uniformly to every pawn variant for authoring simplicity
  and does nothing on clients — uniform content is worth more than a conditional that content authors must
  remember.
- **The rewind is non-mutating** (**N6**). It returns a token carrying the rewound poses, and confirmation
  queries the token. Writing bone transforms onto a live mesh would fight the next animation tick and corrupt
  the rest pose; a non-mutating rewind is also trivially **re-entrant**, so a splash and a follow-up shot in the
  same tick cannot corrupt each other. Restore is idempotent and scope-guarded (**N7**).
- **The shooter's own pawn is excluded** (**N5**). Their trace already ran in their own local frame; rewinding
  them too would double-compensate.

### 6.3 The server never reads the client viewpoint

**N2** is the single most load-bearing rule in this section. On a dedicated server, asking for the player's
view point does not fail — **it silently returns a fallback derived from control rotation.** The server then
proceeds confidently with an aim origin that is *close enough to look right and wrong enough to mis-resolve
hits*, most visibly at the edges of cover and at range.

**Camera position is delivered as target data or it does not exist.** There is no third option, and the
absence of an error is exactly why this is stated as a prohibition rather than a preference.

### 6.4 Why this is load-bearing for staked play

> **A hit dispute is a money dispute.**

In unstaked play a contested hit is an argument. Under stakes (`ssot/matchmaking.md` §3, `ssot/league-play.md`
§5) it is a claim on a balance, and the player making it is entitled to an answer that does not reduce to
"the server decided."

Three properties follow, and they are requirements rather than qualities:

- **One confirm path.** Live fire and any diagnostic tooling call **the same** confirmation code. A diagnostic
  that exercises a reimplementation of the confirm logic proves nothing about the path that actually resolves
  hits — and it will drift from it, because nothing forces the two to agree. Sharing the path *is* the point.
- **The verdict is logged in a stable, parseable form** — the rewind delta, the number of pawns considered, and
  the accept/reject verdict — so a disputed hit can be reconstructed rather than re-argued (§7).
- **Resolution is server-side and therefore spoof-proof.** Where the client's trace carries less information
  than the outcome needs — a capsule hit with no specific bone, for a system that resolves damage by body zone
  — **the server resolves the missing detail itself** from the rewound pose, rather than asking the client for
  it. The general rule: **when authoritative logic needs a detail the client could supply, the server derives
  it instead.** A client-supplied detail is a client-chosen outcome.

---

## 7. TELEMETRY — THE EVIDENCE CHAIN

Combat telemetry is the **evidence chain for dispute replay** required by `ssot/matchmaking.md` §9. Its
function is not analytics; analytics is a beneficiary.

### 7.1 Event families

| Family | What it records | Why it is its own family |
|---|---|---|
| **Rejection** | The server refused a client-built payload, with a short stable reason token (schema · geometry · lag-comp · angular) | The reason must be an enumerable token, not prose, so rejections can be counted by cause |
| **Angular anomaly** | Claimed aim angular velocity exceeded the per-pawn budget | **Informational, and deliberately separate from rejection**: the *rate* is the signal to graph. Folding it into rejections would conflate "we refused this" with "this looked odd" |
| **Accuracy distribution** | Per-pawn headshot/total-hit ratio samples | A distribution, not an event — its meaning is in aggregate |
| **Round / outcome** | Round start and resolution, elimination with location | The match-level spine a replay is indexed by |
| **Spatial** | Extraction contest and outcome, periodic living-pawn position — all carrying world **Z** | Location without height is not a position in a multi-tier map; a heatmap built from X/Y alone silently merges floors |

### 7.2 The stable line format

Every event is one line:

```
AFL_TELEMETRY: <event> key1=val1 key2=val2 ...
```

**The format is the contract.** Three properties make it worth pinning:

- **One line per event, never multi-line.** An event that spans lines cannot survive interleaving from other
  threads or systems, and interleaving is guaranteed on a live server.
- **`key=value`, not positional fields.** A positional format breaks every existing parser the moment a field
  is added. Key-value tolerates additive fields, which is how this data actually evolves — every family above
  gained fields after its first version.
- **A single prefix.** One grep recovers all combat telemetry from a log containing everything else.

### 7.3 One sink, one cutover

Every combat telemetry path funnels through the same small set of emit methods. **This is the entire reason the
class exists**: swapping the log sink for a live analytics stream becomes a change in one file, with no call
site touched. The alternative — emitting from call sites directly — means the cutover is a repo-wide edit, and
repo-wide edits are where call sites get missed and event families quietly stop reporting.

**Server-only guarding is the caller's responsibility**, deliberately: the sink is safe to call from anywhere,
and events that are meaningful only on authority are guarded where that fact is known.

---

## 8. DEPLOYABLES

### 8.1 A deployable is a lean AActor, never a Pawn

**A10** is doctrine. The system reason:

**A Pawn is not a neutral container — it is a participant.** Round logic counts pawns, team logic assigns them,
alive-checks enumerate them, spawn selection and enemy queries consider them, and scoring attributes to them.
A deployed wall implemented as a Pawn therefore becomes a **player-shaped object in every one of those
systems**: last-standing counts it, so the round does not end; the spawn selector avoids it; enemy queries
target it; team population reads high.

**None of that surfaces as a deployable bug.** It surfaces as a round that will not resolve — a defect that
looks like round logic and is not.

A deployable therefore carries the minimum to be a destructible obstacle: collision, a mesh, and a self-granted
ability-system + health stack so that ordinary weapon damage has a valid target. **It is destructible via the
same damage path as everything else** (§9) rather than via a bespoke hit handler — a second damage path would
need its own armour, falloff and zone semantics, and would drift from the first.

### 8.2 Arm · pulse · disable — the windows are the design

A deployable's behaviour is defined by its **windows**, and the windows are what make it fair:

| Window | Purpose |
|---|---|
| **Arm** | The delay between landing and taking effect. The device **telegraphs** during it — a ramping emissive and a charge audio |
| **Destructible-during-arm** | **The counterplay.** The device can be shot down for the whole arm window; killing it before it fires means it does nothing at all |
| **Effect** | A single pulse (area, team-filtered) or a persistent obstacle (a wall, until destroyed or expired) |
| **Consumed** | One effect, then gone — no lingering ambiguity about whether it is still live |

**Why destructibility during arm is the load-bearing element:** an area denial effect with no counterplay is
not a tactic, it is a tax. The arm window converts it into a **contest** — the thrower is asking the defender
to spend attention and ammunition, and the defender who spends them wins. This is why the telegraph is a
requirement rather than polish: **counterplay that cannot be perceived is not counterplay.** The visual ramp is
what tells the defender there is a clock and roughly how much of it is left.

**Team filtering is not optional** for effect deployables. An effect that catches friendlies converts a
tactical tool into a liability and, in team play, into a griefing vector.

### 8.3 The disable pattern

A disable effect grants a `State.*` tag that sits in **`ActivationBlockedTags` on the weapon family base**
(**T5**). Because the whole weapon roster descends from that base, **one tag disarms every weapon at once —
and touches nothing else.**

The design consequence is the point: **movement abilities are separate abilities, so a disabled player is
disarmed but can still flee.** A disable that also froze movement would be a stun, and a stun that lasts long
enough to be worth throwing is long enough to feel like a removal from play. Preserving mobility keeps the
effect a **repositioning pressure** rather than a death sentence.

This is the same structural shape as a self-inflicted overheat lockout, turned outward at enemies — which is
why it needs no new mechanism, only a new tag and a new granter.

---

## 9. DAMAGE FLOW

> **No ability modifies health directly** (**A4**). All damage flows through a meta-attribute and one
> execution calculation.

**The meta-attribute is transit-only:** a damage value is written into it, the execution consumes it, and it is
zeroed. It is never replicated — it is a parameter in flight, not state. Attributes that *are* state (health,
shield, armour, heat, zone health) replicate; the transit attribute does not, because there is no moment at
which a client benefits from knowing it.

**Why one execution rather than per-ability damage application:**

- **Every modifier composes in one place.** Armour, shields, per-zone absorption, distance falloff, material
  multipliers and consequence modifiers interact. Distributed across abilities they would interact
  *differently* in each — and the differences would be accidents, not design.
- **Ordering is a design decision and must be made once.** Zone health absorbs first and only the overflow
  continues to shield and health; a hit with no resolvable zone falls back to health directly. That ordering
  determines the entire feel of limb damage, and it must not be re-decidable per weapon.
- **The source-side damage seed is a captured attribute, not a loosely-named magnitude.** A magnitude passed by
  tag name is only read if the execution looks for that exact name — a mismatch is silent and yields zero
  damage. A captured attribute is a typed contract.

**Death fires from the combat attribute set** — the one damage actually drains — not from a parallel health
set. **The rule generalises: the system that owns the attribute owns the signal derived from it.** A death
signal sourced from a set that damage does not touch is a signal that never fires, and it fails identically for
every combatant, which makes it look like a death-system bug rather than a wiring one.

---

## 10. MAP WEAPONS ARE RESTRICTED

> **Rockets and specials (EMP, Shield) are map pickups, not loadout weapons.**

### 10.1 The design reason

**A weapon that must be contested for is a map-control mechanic. The same weapon in a loadout is a stat.**

The distinction is not about power level — it is about **what the weapon does to the match**:

- **As a pickup**, the weapon exists at a *place* and at a *time*. Players must go somewhere to get it,
  which means someone else can be there, which means the pickup **generates a fight that would not otherwise
  happen**. It is finite: used, it is gone, and getting another means contesting the place again. Denying it to
  an opponent is as valuable as taking it. The weapon's power is the *stake* of an engagement, and the skill it
  rewards is map awareness, timing and positioning.
- **As a loadout weapon**, all of that disappears. It is simply present, from spawn, always. It creates no
  location, no timing, no contest. Its power stops being a stake and becomes a **baseline** — and a baseline
  that strong compresses everything else, because every other weapon is now measured against a thing everyone
  always has. This is the mechanism by which one addition flattens a whole arsenal's risk/reward spread
  (**C2**).

**The restriction is therefore a map-design tool, not a nerf.** Moving a weapon to the map keeps it at full
strength — its strength is exactly what makes the contest for it meaningful — while converting it from
something players *have* into somewhere players *go*. **C3** (one signature mechanic per map) sits adjacent:
where a power weapon spawns is a deliberate part of a map's shape.

### 10.2 What qualifies a weapon for map-only status

A weapon belongs on the map, not in the loadout, when it meets these — the first two are close to necessary:

1. **It ends engagements rather than winning them.** Its counterplay is *not being there* — high burst, area
   denial, or an effect that removes the target's ability to respond. A weapon you beat by out-aiming belongs
   in a loadout; a weapon you beat by not being in front of it does not.
2. **Its counterplay is positional rather than mechanical.** If the correct answer is "control where it
   spawns" or "be somewhere else," the weapon's balance already lives in the map — so the map should own it.
3. **Always having it would flatten the arsenal.** Ask directly: if everyone spawned with this, would the
   other weapons still have a reason to be picked? A "no" is disqualifying (**C2**).
4. **Its value is durable enough to be worth a detour.** A pickup nobody crosses the map for creates no
   contest and is just a slower loadout weapon.
5. **It is finite in use.** Consumption is what makes the pickup recur as a contest rather than resolve once.

**The named three by category:** a **rocket** (burst that ends an engagement outright), an **EMP** (removes the
target's ability to respond), and a **shield/barrier** (denies space rather than dealing damage). All three
are answered positionally, and all three would compress the arsenal if universally held.

Beyond these three, membership is an **open question** (§12) — deliberately, because the test above is a
judgement about a *specific* arsenal, and answering it for a weapon that has no siblings yet answers nothing.

---

## 11. COSMETIC AXES THAT TOUCH COMBAT

Beams, weapon skins and pulse/muzzle looks are **independent ownable categories** on the axis model defined in
`ssot/economy-store.md` §6 — not restated here. Three combat-side consequences:

**1 — An axis applies across weapons, not to one.** An owned beam applies to any weapon, overriding that
weapon's default look. The axis is the item; the weapon is where it lands. Modelling a look as a property *of a
weapon* multiplies entries by the whole roster and produces a catalogue that grows quadratically while
expressing nothing new.

**2 — Baked identity wins over an applied axis.** A weapon may declare its look a **locked signature**, and a
signature look is not overridden by an owned axis. The reasoning is the same as elsewhere in the axis model:
an identity item's identity *is* the product. A rare weapon whose defining look can be replaced by a common
owned beam has had its distinctiveness sold out from under its owner — and distinctiveness is what they bought.

**3 — The cosmetic axis never crosses into gameplay.** A beam changes colour and system; it never changes
range, damage, cadence or trace. This is **A5** and §5 restated at the ownership layer, and it is the boundary
that keeps a cosmetic purchase from becoming a competitive one — the same firewall `ssot/league-play.md` §5
draws around rating, drawn here around the shot itself.

---

## 12. OPEN DESIGN QUESTIONS

1. **Weapon-pack composition and the free initial set.** How weapons are grouped into purchasable packs, and
   which set a player starts with. Interacts with `ssot/economy-store.md` (pricing ladders) and with **C2** —
   a starting set whose members feel alike wastes the one impression that is guaranteed.
2. **Which weapons qualify as map-only beyond the named three.** §10.2 gives the test; applying it requires an
   arsenal to apply it *to*. Deciding early risks locking a classification made without siblings to compare
   against.
3. **Overheat: universal mechanic or per-family.** The arsenal's heat gating is not uniform — on the order of
   nine of fifteen weapon abilities gate on no heat tag at all. **This document does not assert which of the
   two readings is correct**: it is either a deliberate design in which heat is the identity of one family
   (and its absence elsewhere is correct), or an unfinished rollout. The two are indistinguishable from the
   data and lead to opposite work. Note that under §2.2 the *safe* failure is the one present here — no gate,
   rather than an inert one.
4. **Cooldown durations: inherited or authored per weapon.** Durations currently inherit rather than being
   authored per weapon. Inheritance is correct if cooldown is a *family* property and per-weapon feel comes
   from cadence and damage; it is wrong if cooldown is part of a weapon's identity. **T3** is unaffected either
   way — it constrains the **tag**, never the duration, and a shared duration with distinct tags is fully
   conformant.

---

## 13. RULINGS OF RECORD

| Ruling | Date | Content |
|---|---|---|
| **R15 — Map weapons are restricted** | **2026-08-05** | **Rockets and specials (EMP, Shield) are map pickups, not loadout weapons** (§10). A weapon that must be contested for is a map-control mechanic; the same weapon in a loadout is a stat. As a pickup it creates a place, a timing and a fight; as a loadout item it creates none of those and becomes a baseline that compresses the rest of the arsenal (**C2**). Qualification test in §10.2; membership beyond the named three is an open question. |

---

## 14. RELATED

- [`Docs/DOCTRINE.md`](../DOCTRINE.md) — laws cited here: **A2** stay Lyra-canonical · **A3** abilities via
  AbilitySets · **A4** no ability touches health directly · **A5** cosmetics through cues · **A6** cooldowns are
  GEs · **A9** bot ability parity · **A10** a deployable is never a Pawn · **N1**–**N9** net safety ·
  **T1**–**T6** the arsenal tag contract · **C1** game feel before content · **C2** no generic reskins ·
  **C3** one signature mechanic per map · **NM2**/**NM3** prefixes and tag namespaces · **X2** teardown in
  `EndAbility`.
- `ssot/match-modes.md` — the warmup and ended phases the tag contract gates on (§5 there).
- `ssot/matchmaking.md` — the dispute-replay requirement telemetry serves (§9 there), and staking.
- `ssot/economy-store.md` — the cosmetic axis model (§6 there) the combat-facing axes belong to.
- `ssot/league-play.md` — the stake firewall (§5 there), of which §11's cosmetic boundary is the combat-side
  analogue.
- `ssot/map-build-system.md` — where map-restricted weapons are placed and what that placement does to a map.
