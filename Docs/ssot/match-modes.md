# SSOT — MATCH MODES (Tier 2)

**What this is:** what the match-mode system **is** and **why**. It changes when the system is redesigned.
**What this is not:** a status board. **This document contains no status claims** — nothing here says what is
built, proven, done or owed, and no commit hash appears as evidence of progress. Those belong to Tier 3
(`LIVE_TRACKER`).

> **Why the separation matters here in particular.** A ruleset doc that records *what currently resolves* instead
> of *what the ruleset is* becomes wrong the moment a component changes, and a reader cannot tell the difference
> between "this is the design" and "this was true last week." If a sentence would change because of a commit, it
> does not belong here.

**Doctrine is cited, never restated.** Laws live in [`Docs/DOCTRINE.md`](../DOCTRINE.md) and are referenced by id
(**T1–T6**, **N1**, **N8**, **A3**, **A6**, **A9**, **X6**, **P4**, **L4**).

---

## 1. SCOPE

This SSOT governs: the two match rulesets and what distinguishes them · the combat-rules axis and its
independence from the ruleset axis · the match-phase spine and the warmup contract · the zone's role and which
ruleset it serves · determinism requirements for staked play · match-scoped policy tags and their symmetry law ·
the open design questions the mode system still owes an answer to.

It does not govern: which maps host which bracket (→ `ssot/map-build-system.md`), how players are matched or
staked (→ `ssot/matchmaking.md`), rank ladders and progression (→ `ssot/league-play.md`), weapon behaviour
(→ `ssot/combat-arsenal.md`), or the state of any implementation (→ Tier 3).

---

## 2. THE TWO RULESETS

A **ruleset** answers three questions: *how does the match end*, *what happens when you die*, and *what is
scored*. Everything else about a match is shared machinery. There are exactly two rulesets.

### 2.1 SHOOTOUT — last player standing

The speed format in the poker sense: simple, fast, decisive. No clock to manage, no comeback mechanic, no series
to sit through. You are in until you are out.

| Property | Definition |
|---|---|
| **End condition** | One participant (or squad) remains. The match resolves the instant the alive-count reaches the survivor threshold. |
| **Timer** | **None.** The match runs until it resolves. |
| **Respawn policy** | **None.** Death is permanent for the match — participants are eliminated, not benched. |
| **Scoring basis** | **Placement.** Each elimination books the eliminated participant's finishing place, counting down from the participant count; the survivor takes first. |
| **Rank input** | Placement (1..N). A player's contribution to rank is *how long they survived*, not how much they killed. |
| **Suits** | Any footprint, including sparse and whole-map. Elimination is permanent, so the population falls monotonically and the fight concentrates on its own — a large footprint self-corrects. Pairs with the zone (§6). |

**Why placement rather than kills:** under permanent death, surviving *is* the skill expression. A kill-weighted
score would reward a player who traded early over one who won, which inverts the format's own premise.

**Payout basis — placement (R36).** SHOOTOUT **earns on finishing position**. The ladder and its splits live in
`ssot/economy-store.md` §5.2 and are not restated here. What belongs in the ruleset definition is *why the
basis is available at all*: **placement is the only outcome this ruleset generates.** With no timer and no
respawn, there is nothing else a match produces that a payout could key on. The same signal already feeds the
rank input above and league rating, so paying on it adds a consumer rather than a system.

> **The positions-not-players rule — a ruleset property before it is an economy one.**
> Payout keys on **finishing positions**, never on player count. **A team mode has exactly TWO finishing
> positions regardless of how many players are in it** — a 5v5 is ten players and two positions. The economy's
> paid-places formula therefore resolves every team mode to winner-takes-all **automatically**, with no mode
> flag and no branch anyone can forget. Scaled payouts exist only where there are more than two positions to
> scale across, which means FFA formats only.
>
> Recorded here as well as in the economy SSOT because **the position count is a fact about the ruleset's
> shape**, not about its payout — anything else keying on match outcome inherits the same distinction.

### 2.2 TURBO — timed, respawning, kill-ratio

The high-tempo format. A fixed clock, no downtime on death, and a score that rewards sustained performance
rather than survival.

| Property | Definition |
|---|---|
| **End condition** | **The clock.** **7 minutes** standard, **10 minutes** high-stakes. The match ends when time expires — nothing else ends it. |
| **Respawn policy** | **Instant.** Death costs the death itself and the tempo of dying; it does not remove the player. |
| **Scoring basis** | **Kill ratio.** Performance is measured from eliminations against deaths (with assists credited), not from survival. |
| **Rank input** | The kill-ratio score at time expiry. |
| **Series** | **None.** A TURBO match is a single timed match — **no best-of-N**. The clock is the whole structure. |
| **Suits** | **Dense footprints** — district-scale, at or near the dense baseline of the footprint ladder (`ssot/map-build-system.md` §3). Instant respawn on a sparse or whole-map footprint spends the match walking; the format needs re-engagement to be immediate. |

**Why a hard clock and no series:** the two high-stakes properties of TURBO are *known duration* and *no dead
time*. A best-of-N reintroduces both problems it exists to remove — variable length, and a player sitting out a
round. The clock is the format.

**Why kill-ratio rather than raw kills:** raw kills reward whoever fed most aggressively into a respawn loop. A
ratio against deaths makes a reckless trade cost something, which is what keeps a respawning format competitive
rather than chaotic.

---

## 3. SHARED MACHINERY VERSUS WHAT ACTUALLY DIVERGES

This is the most important framing in the document, because it determines the implementation shape.

### 3.1 Common to both rulesets

| Concern | Why it is common |
|---|---|
| **Participant roster** | Both rulesets operate on the same set of participants gathered at match start; both must survive late joins and mid-match possession changes identically. |
| **Match id** | One server-authored identifier per match, replicated. It is the contract id staking and telemetry key against — independent of ruleset. |
| **Telemetry** | Both emit the same event families (eliminations with location, traversal samples, match resolution). A dispute-replay reads the same stream either way. |
| **Team assignment** | Solo/FFA versus squads is a *lobby* property, not a ruleset property. Either ruleset can run either way. |
| **Match-phase spine** | Warmup → Playing → PostGame (§5) is identical. Both freeze abilities during warmup, both conclude through the same PostGame machinery. |
| **Death signal** | Both react to the same authoritative death event; they differ only in what they *do* with it. |
| **Join coverage** | Both must apply match state to participants arriving after the phase edge that set it. |

### 3.2 What actually differs

**Two things. Only two.**

1. **The end condition** — alive-count threshold (SHOOTOUT) versus clock expiry (TURBO).
2. **The respawn policy** — suppressed for the match (SHOOTOUT) versus never suppressed (TURBO).

Scoring differs as a *consequence* of those two: permanent death produces an ordering (placement), a respawning
clock produces a rate (ratio).

### 3.3 Therefore: TURBO is a SIBLING, not a fork

**Intended shape: TURBO is authored as a sibling match-structure component alongside the existing one — not as a
fork of it, and not as a mode flag inside it.**

The reasons are structural, not stylistic:

- **The pattern is already the architecture.** Match structure is expressed as a GameState component supplied by
  the experience's component list. Each such component owns exactly one ruleset's end condition and respawn
  policy while reusing the shared spine. A ruleset is selected by *which component the experience adds* — which
  means adding TURBO adds a component and changes no existing one.
- **The respawn seam already exists and is polymorphic.** The always-loaded restart-policy interface
  (`ShouldBlockRestart()`) is consulted by the game mode before permitting a restart. SHOOTOUT answers *block*;
  TURBO answers *never block*. That is the entire respawn divergence, expressed through a seam that exists
  precisely so a ruleset can declare its policy **without editing the game mode**.
- **The scoring substrate already exists and is shared.** Per-player elimination/death/assist counters are
  already replicated player state, fed by the existing elimination-message pipeline. A kill-ratio ruleset reads
  them; it does not need a new scoring pipeline. Placement, by contrast, is a per-match ordering that only the
  permanent-death ruleset can produce — which is exactly why it lives in that ruleset's component and not in the
  shared substrate.
- **A fork would duplicate the shared 80%** — roster, match id, telemetry, join coverage, phase spine — and the
  duplicate would drift. A mode flag inside one component would make every future read of that component ask
  "which ruleset am I in?" at every branch, which is how end-condition bugs get written.

**The rule this establishes:** *a new ruleset is a new match-structure component. It may not be a flag inside an
existing one, and it may not fork the shared spine.*

---

## 4. THE COMBAT-RULES AXIS IS ORTHOGONAL — READ THIS TWICE

**A match is `(ruleset × league)`. The two axes are independent and neither constrains the other.**

| Axis | Values | What it governs |
|---|---|---|
| **Ruleset** (this document) | SHOOTOUT · TURBO | How the match ends, respawn policy, what is scored |
| **Combat rules / league** | **HAYWIRE** · **PRO MOD** | Damage model and the granted movement kit |

- **HAYWIRE** — the dismemberment combat model: the zone-HP damage routing and its gib/consequence layer are
  active.
- **PRO MOD** — gore-free by construction: the dismember feature is not loaded and a mode gate routes all damage
  to the conventional single-health chain. It also carries the enhanced movement kit.

**The independence is the point, and it is the thing most likely to be misread.** SHOOTOUT is not "the Haywire
mode" and TURBO is not "the Pro Mod mode." Every combination is legitimate: a Haywire SHOOTOUT, a Pro Mod
SHOOTOUT, a Haywire TURBO, a Pro Mod TURBO. A player picking a league is choosing how combat feels; a player
picking a ruleset is choosing how the match is won.

**Structurally this holds because the two axes are carried by different things.** The combat axis is carried by
which game features the experience enables and which pawn data it uses. The ruleset axis is carried by which
match-structure component the experience adds (§3.3). They compose in one experience definition without either
being aware of the other — which is why the matrix is complete rather than a set of blessed pairs.

> **Consequence for authoring:** an experience is named for its *combination*, and adding a ruleset must not
> require duplicating the league split, nor vice versa. If adding a ruleset ever forces a league fork, the
> orthogonality has been broken and that is a design defect, not a naming problem.

---

## 5. THE MATCH-PHASE SPINE

Both rulesets run the same three phases.

```
  Warmup  ──30s──▶  Playing  ──ruleset end condition──▶  PostGame
```

| Phase | What it is |
|---|---|
| **Warmup** | A fixed pre-match freeze. **30 seconds by default** (a tunable console value, compressed by the automated harness), with a **visible countdown** published to the HUD. |
| **Playing** | The match. The active ruleset owns when this ends. |
| **PostGame** | Terminal. Abilities are frozen again and per-player results are broadcast. Both rulesets conclude through this same machinery rather than implementing their own ending. |

### 5.1 The warmup contract

- **Warmup runs ONCE, before the match begins — never per round.** It is a match-entry ritual (load in, orient,
  see the countdown), not an inter-round pause. A per-round warmup would make TURBO's clock meaningless and would
  turn SHOOTOUT into a series, which neither ruleset is.
- **Warmup freezes all weapon and movement abilities.** The freeze is achieved by the arsenal tag contract —
  every weapon and movement ability blocks on the match-state tags (**DOCTRINE T1**). Warmup is an *ability*
  block, not a locomotion block: a participant can still walk and look around, which is what makes it usable as
  an orientation window.
- **An ability that omits the gate is an exploit surface, and that is precisely why the contract is law.** In a
  staked match, a single un-gated ability means one participant can act during a window in which every other
  participant is frozen — first blood before the match legally starts, on a wager. The contract is not
  housekeeping; it is the guarantee that the match starts fair. **Every new weapon and movement ability inherits
  the gate, and an ability that does not is a defect regardless of how it plays.**

---

## 6. THE ZONE — SCOPED TO SHOOTOUT

**The zone is a SHOOTOUT mechanism. It exists to force last-standing to resolve.**

Under permanent death and no clock, a SHOOTOUT on a large footprint has no intrinsic pressure to end: two
surviving participants can avoid each other indefinitely. A shrinking playable area removes that possibility by
making avoidance progressively impossible. **The zone is the end-condition's enforcement, not decoration.**

**TURBO does not need a zone,** because a clock already guarantees termination. A ring under TURBO could only
serve *pacing* — compressing play toward the end of the timer so the final minute is dense rather than diffuse.

**What a TURBO ring would have to justify:** that pacing benefit against three real costs — (a) it competes with
respawn, since a respawning player must always have somewhere legal to spawn, and a shrinking area shrinks that
set; (b) it adds a second source of death to a format whose score is a kill ratio, so zone deaths would either
pollute the ratio or need excluding, and both are design debt; (c) it adds replicated state to a format that
otherwise needs none. **The default is no ring under TURBO.** A proposal to add one must answer all three.

**Zone sizing.** The final zone is an arena fight and is sized by the footprint ladder against *expected
surviving participants*, not lobby size — see `ssot/map-build-system.md` §3.3. Sizing the final circle off the
drop count is the classic failure of a match that ends in an empty field.

**Zone integrity requirements** (they follow from staking, §7): the shrink schedule and every circle position are
**server-authoritative** (**DOCTRINE N1**), derived from the match seed, telegraphed before each contraction so a
participant can act on it, and logged so the sequence can be replayed. A zone a client can influence is a zone a
client can exploit.

---

## 7. DETERMINISM FOR STAKED PLAY

Both rulesets are played for stakes, and that imposes requirements a casual mode would not need.

| Requirement | Why |
|---|---|
| **Server-authoritative resolution** (**DOCTRINE N1**) | End condition, elimination, scoring and any zone state are decided by the server. A client observes outcomes; it never produces them. |
| **Seeded from the match id** | Anything with a random element (zone positions, loot placement) derives from the per-match identifier so the same match id reproduces the same match conditions. |
| **Logged and replayable** | A staked outcome must be reconstructable after the fact from the telemetry stream: who was eliminated, in what order, where, and what the match state was at each transition. A dispute that cannot be re-read is a dispute settled by assertion. |
| **No client-authored match state** | Including the derived kind — a scoreboard a client computes and reports is not evidence. |
| **Replicated state uses safe types** (**DOCTRINE N8**) | Any net-serialized struct introduced by a ruleset lives in the always-loaded net-types module, never a game-feature module. |
| **Two-client verification** (**DOCTRINE P4**) | A ruleset's replicated state cannot be validated on a listen-host alone. |

---

## 8. MATCH-SCOPED POLICY TAGS AND THE SYMMETRY LAW

A ruleset expresses a *policy* — most importantly its respawn policy — as a gameplay tag applied to participants
for the duration of the match. Tag-driven policy is the right mechanism: it is queryable, it replicates, it
survives pawn destruction because it lives on player state rather than the pawn, and it composes with the
existing ability-blocking machinery (**DOCTRINE A6**, **A3**).

It also has one failure mode, and it is severe enough to be stated as a rule:

> ### THE SYMMETRY LAW
> **Whatever sets a match-scoped policy tag MUST clear it at match end.**
>
> A policy tag that survives its match corrupts the next one. A no-respawn tag left set means the following
> match begins with participants who cannot respawn — they are counted in the roster but can never be alive,
> so the alive-count is wrong from the first frame and every count derived from it is wrong too. The failure is
> silent, it appears one match *later* than its cause, and it looks like a bug in the end condition rather than
> in the setup. (**DOCTRINE X6**.)

**Corollaries:**
- **Set and clear belong to the same owner.** The component that applies a match policy is the component that
  removes it. A policy applied by one system and cleared by another will eventually be applied and not cleared.
- **Clearing is unconditional.** It happens on every path out of the match, including abnormal ones.
- **Freezing is not a policy tag.** The PostGame ability freeze is a *phase* state owned by the spine; it is not
  a ruleset policy and is not the ruleset's to clear.
- **Late joiners must receive the current policy** (**DOCTRINE L4** on population coverage) — a participant who
  arrives after the tag sweep is uncovered unless the ruleset applies policy on join as well as at the edge.

---

## 9. OPEN DESIGN QUESTIONS

TURBO is specified above at the level a ruleset needs; these are genuine design gaps in it, recorded as design
scope. **None of these is a status claim — each is a decision the mode system owes.**

1. **Team model.** Is TURBO solo-only, squad-only, or both? Kill-ratio scoring is individually attributed, so a
   squad TURBO must define whether rank input is the individual's ratio, the squad's aggregate, or both. This
   also decides whether friendly fire and assist credit cross squad boundaries.
2. **The kill-ratio formula.** Eliminations, deaths and assists are separately tracked, so the formula must
   state how assists are weighted and what a zero-death score evaluates to (a pure ratio is undefined at zero
   deaths; a difference-based score is not). It must also state whether the score is monotonic within a match —
   i.e. whether a late death can reduce a player's standing below someone who stopped playing.
3. **⚠ TURBO'S PAYOUT SHAPE — OPEN, and now the asymmetry is sharper.** SHOOTOUT's payout basis is **ruled**
   (R36, §2.1) and its ladder is locked (`ssot/economy-store.md` §5.2). **TURBO has neither.** Its *scoring*
   is defined — kill-ratio — but its **payout is not**, and the gap is structural rather than merely undone:
   **TURBO has no placement ladder to key on.** Every participant is present at the end, so there are no
   finishing positions, which means the entire positions-based apparatus SHOOTOUT uses does not transfer.
   A TURBO payout must therefore be defined **against the ratio itself** — including how ties resolve, whether
   there is a participation floor, and whether a losing player earns at all. **This cannot be inherited from
   SHOOTOUT; it has to be designed.** It interacts with open question 2 (the ratio formula is undefined at zero
   deaths, and a payout keyed on an undefined score is undefined too) and with open question 1 (a squad TURBO
   must decide whether payout attributes individually or to the squad).
4. **Respawn timing and its exploit surface.** "Instant" must state whether it is truly zero-delay and where the
   participant re-enters. Instant respawn at the point of death is a different game from instant respawn at a
   spawn point, and in a staked format the difference is exploitable (spawn-camping, or trading into a
   favourable re-entry).
5. **Time-expiry tie resolution.** What settles two participants on an identical ratio when the clock expires.
6. **High-stakes as a duration or as a distinct ruleset.** 7 versus 10 minutes is specified in §2.2 as a
   duration parameter. If high-stakes is ever to differ in more than duration, it becomes a third ruleset rather
   than a parameter — and that boundary should be decided deliberately, not discovered.

---

## 10. RULINGS OF RECORD

| Ruling | Date | Content |
|---|---|---|
| **R1 — MELEE IS CUT** | **2026-08-05** | The game is **dual-mode, not tri-mode**. There are two rulesets — SHOOTOUT and TURBO — and no melee/deathmatch ruleset. The instant-respawn arcade format previously specified as its own mode is **not** a third ruleset; the tempo it was reaching for is TURBO's. **This is settled; a reader should not reopen it.** The superseded melee document remains on disk pending archival and is not a source of design authority. |

---

## 11. RELATED

- [`Docs/DOCTRINE.md`](../DOCTRINE.md) — laws cited here: **T1–T6** arsenal tag contract · **N1** server
  authority · **N8** net-serialized struct residency · **A3** ability granting · **A6** cooldowns as effects ·
  **A9** bot ability parity · **X6** clear what you set · **P4** two-client proof · **L4** population coverage.
- `ssot/map-build-system.md` — footprint ladder and final-circle sizing (§3.3), district model, which brackets a
  map hosts.
- `ssot/matchmaking.md` — how a player reaches a ruleset, lobby composition, staking structure.
- `ssot/league-play.md` — how placement and kill-ratio become rank.
- `ssot/character-system.md` — the pawn and movement kit each league grants.
- `ssot/combat-arsenal.md` — weapon behaviour and the ability set the tag contract governs.
