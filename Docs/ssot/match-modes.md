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

### 2.1 BATTLE ROYALE — last participant standing

Simple, fast, decisive. No clock to manage, no comeback mechanic, no series to sit through. You are in until you
are out.

> **Named SHOOTOUT until R41 (§10).** The rename is a rename only — **every property in this section is
> unchanged.** Anything citing `match-modes.md` §2.1 still lands on the same ruleset.

| Property | Definition |
|---|---|
| **End condition** | One participant (or squad) remains. The match resolves the instant the alive-count reaches the survivor threshold. |
| **Timer** | **None.** The match runs until it resolves. |
| **Respawn policy** | **None.** Death is permanent for the match — participants are eliminated, not benched. |
| **Scoring basis** | **Placement.** Each elimination books the eliminated participant's finishing place, counting down from the participant count; the survivor takes first. |
| **Rank input** | Placement (1..N). A player's contribution to rank is *how long they survived*, not how much they killed. |
| **Suits** | Any footprint, including sparse and whole-map. Elimination is permanent, so the population falls monotonically and the fight concentrates on its own — a large footprint self-corrects. Pairs with the zone (§6). |

#### 2.1.1 FIELD SIZE — THE LAW (R99)

> **BATTLE ROYALE IS ONE VERSUS EVERYONE.** **Minimum 9 positions. Maximum 36.** The shipped ladder is
> **`BR_9` · `BR_20` · `BR_36`** — **three rungs, and each one sits at the TOP of a paid-count tier.**

| Rung | Positions | Paid | Structure |
|---|---|---|---|
| **`BR_9`** | 9 | **1** | **Winner-take-all.** All or nothing — no min-cash floor exists below N = 10 |
| **`BR_20`** | 20 | 3 | Podium. Min cash **1.40×** (R37) |
| **`BR_36`** | 36 | 6 | Full field. Winner's multiple **13.29×** (R40) |

**POSITIONS, NEVER PLAYER COUNT** — the same keying as R37. In solo BR a position is a player; in squad BR a
position is a **squad** (R92), so the minimum is 20 *squads*. `BR`'s position count **is** its field size,
which is why the registry carries `positions == slots` for these brackets and `positions: 2` for every team
bracket.

**FIELD SIZE IS NOT A FREE PARAMETER — IT SETS THE PODIUM.** The payout rule
(`ssot/economy-store.md` §5.2) is `p(N) = 1 if N < 10, else ceil(0.15 × N)`:

| N | Paid | What the player is actually offered |
|---|---|---|
| 8 | 1 | Winner-take-all — but one buy-in short of the tier's top |
| **9** | **1** | **Top of the winner-take-all tier.** Largest pot that still pays one place |
| 10–13 | 2 | A podium of two |
| 14 | 3 | **On a threshold** — the multiple has just dropped |
| **20** | **3** | **Top of its tier: the same three paid places as 14, with six more buy-ins in the pot** |

**WHY THE MINIMUM IS 9, AND WHY WINNER-TAKE-ALL IS NOT A DEFECT.** **R37 designs for exactly this** —
*"winner-takes-all for small fields, scaled payouts above"* — and §5.2 states the behaviour deliberately:
*"at N < 10 the single paid place takes the whole [pot]."* **Every MATCH PLAY bracket is winner-take-all
too**, at any team size, because a team mode has two positions. So a small all-or-nothing BR is not an edge
the design tolerates; **it is the case the payout rule was written to cover.**

**It also earns its place on product grounds** (operator, 2026-08-07): a substantial share of players prefer
the small winner-take-all format, and **it is the fastest-filling bracket there is** — which makes it a
fragmentation *mitigation* in a young population, since it fills when 20 and 36 cannot.

**9 rather than 8** is this section's own rule applied: both are winner-take-all, but 9 carries one more
buy-in into the same single prize. It is also the classic full-ring SNG size.

**WHY THE MAXIMUM IS 36.** `p(36) = 6`, it is R40's own worked example (*"at 36 positions, 6 paid… the
winner's multiple 13.29×"*), and it matches the measured ShantyTown BR envelope — 617 × 607 m full landscape
(`design/ShantyTown_BR_DESIGN.md`). **A larger field is a deliberate ruling, not an assumption**: past 36 the
binding constraint stops being the map and becomes *population*, because a staked field must fill with
humans (no bots — `ssot/ai-bots.md` §6.3).

> **⚠ NEVER SIZE A BRACKET AT N = 10, 14, 21, 27 OR 34.** Those are the exact values where `p(N)` increments
> (`economy-store.md` §5.2), and §5.3 records the winner's multiple dropping **17–23%** across each step. A
> bracket parked on a threshold has just bought an extra paid place with the winner's multiple and gets
> **the worst return available for its field size**. Sizes should sit at the *top* of a paid-count tier —
> 20 (3 paid), 26 (4), 33 (5), 36 (6) — never at its foot.

**WHY THREE RUNGS AND NOT SIX.** `ssot/matchmaking.md` §4.3 makes a bracket the expensive adder — each one
is **8 new queue cells** (4 tier×league pairs × 2 venue classes) — and §5.1's argument is that fewer, fuller
pools beat more, thinner ones. **9, 20 and 36 are three genuinely different products**: all-or-nothing, a
podium, and a full field. **24 and 32 are filler** that would split existing pools to buy a difference a
player cannot feel, and each sits below the top of its tier anyway. A fourth rung must earn itself on
population, not symmetry.

**THE HONEST-CARD OBLIGATION.** `BR_9` has **no min-cash floor** — the 1.40× guarantee in R37 exists only at
N ≥ 10. The front end must therefore label it **winner-take-all** on the card itself, next to the field size,
so the structure is chosen knowingly rather than discovered at settlement (`ssot/ui-frontend.md` §4.2's
principle applied to field shape rather than stake band).

**Why placement rather than kills:** under permanent death, surviving *is* the skill expression. A kill-weighted
score would reward a player who traded early over one who won, which inverts the format's own premise.

**Payout basis — placement (R36).** BATTLE ROYALE **earns on finishing position**. The ladder and its splits live in
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

### 2.2 MATCH PLAY — two teams, a series of rounds

Two sides, a bounded series, sides swapped at half. The competitive format: known length, a comeback path, and
a per-round reset that makes a single mistake cost a round rather than the match.

> **⚠ THIS SLOT PREVIOUSLY HELD TURBO, AND MATCH PLAY IS NOT TURBO RENAMED.** They are different rulesets with
> no property in common — TURBO was a single timed match with instant respawn scored on kill ratio; MATCH PLAY
> is a round series with no mid-round respawn scored on rounds won. **TURBO is parked, not renamed** (§9.7,
> R41). A reader who treats this section as continuous with what stood here will be wrong about every row.

| Property | Definition |
|---|---|
| **End condition** | A team reaches the **round-win threshold** — **7 by default, best of 13**. The series is bounded: it cannot run long. |
| **Round win** | **Wiping the enemy team, or completing a central-extract bank.** Two routes, deliberately — a team that cannot win the fight can still win the round on the objective. |
| **Round timer** | **Bounded per round.** On expiry the round resolves on **higher banked progress → core-holder → no-score replay**, in that order. The round timer is what removes the stall case; the match has no clock of its own. |
| **Respawn policy** | **Between rounds, never within one.** Death costs the round, not the match. This is the middle position between permanent death and instant respawn, and it is the whole reason the format has a comeback path. |
| **Sides** | **Swapped at half.** A map advantage held by one side is therefore held by both. |
| **Scoring basis** | **Two finishing positions.** A team won the series or it did not; there is no third place in a two-team format. |
| **Rank input** | **The series outcome only** (R66, `ssot/league-play.md`). Round differential is **recorded but not consumed**, so it can be switched on later as config. |
| **Suits** | **Arena footprints** — bounded, symmetric, with a contested centre the extract bank can occupy. A sparse or whole-map footprint spends each round walking. |

**Why a round series rather than a single match:** a single decisive match makes one bad engagement the whole
result, which is punishing in a format played for stakes. A series converts variance into a signal — the better
team wins more rounds — while the round-win threshold keeps the length bounded and known.

**Why two routes to a round win:** an elimination-only round is decided entirely by the fight, so a team behind
on gunplay has no line to pull on. The extract bank gives one, and it does it without a scoring pipeline of its
own — the round is still won or lost, only the way in differs.

**Why no mid-round respawn:** it is what makes a round a discrete unit. Respawning inside a round turns it into
a small deathmatch and removes the reason a round ends at all.

> **THE POSITION COUNT IS TWO, AND THAT IS A PAYOUT FACT.** MATCH PLAY is a team format, so §2.1's
> positions-not-players rule resolves it to **winner-takes-all automatically** — `ceil(0.15 × 2) = 1` — at every
> team size from 1v1 to 8v8, with no mode flag. **This is not a gap in the payout design; it is the payout
> design arriving at its own answer.** Contrast BATTLE ROYALE, where N positions produce a scaled ladder.

---

## 3. SHARED MACHINERY VERSUS WHAT ACTUALLY DIVERGES

This is the most important framing in the document, because it determines the implementation shape.

### 3.1 Common to both rulesets

| Concern | Why it is common |
|---|---|
| **Participant roster** | Both rulesets operate on the same set of participants gathered at match start; both must survive late joins and mid-match possession changes identically. |
| **Match id** | One server-authored identifier per match, replicated. It is the contract id staking and telemetry key against — independent of ruleset. |
| **Telemetry** | Both emit the same event families (eliminations with location, traversal samples, match resolution). A dispute-replay reads the same stream either way. |
| **Team assignment** | The size axis is a *lobby* property. BATTLE ROYALE takes squad size (solo/duo/squad over N positions); MATCH PLAY takes team format (1v1 through 8v8 over 2 positions). The ruleset fixes the position COUNT, the lobby fixes the size. |
| **Match-phase spine** | Warmup → Playing → PostGame (§5) is identical. Both freeze abilities during warmup, both conclude through the same PostGame machinery. |
| **Death signal** | Both react to the same authoritative death event; they differ only in what they *do* with it. |
| **Join coverage** | Both must apply match state to participants arriving after the phase edge that set it. |

### 3.2 What actually differs

**Two things. Only two.**

1. **The end condition** — alive-count threshold (BATTLE ROYALE) versus round-win threshold (MATCH PLAY).
2. **The respawn policy** — suppressed for the match (BATTLE ROYALE) versus suppressed within a round and
   released between rounds (MATCH PLAY).

Scoring differs as a *consequence* of those two: permanent death across a whole match produces an ordering of
N participants (placement); permanent death within a round, repeated, produces a two-sided series result.
**Both are placement — one over N positions, one over 2.** That is why a single payout rule serves both.

### 3.3 Therefore: A RULESET IS A MATCH-STRUCTURE COMPONENT

**A ruleset is authored as its own match-structure component — not as a fork of another, and not as a mode flag
inside one.**

**The naming contract**, so nothing has to be inferred from a label:

| Ruleset | Match structure |
|---|---|
| **BATTLE ROYALE** | `UAFLBattleRoyaleComponent` |
| **MATCH PLAY** | `UAFLRoundManagerComponent` |

**A ruleset with no component behind it is not a ruleset — it is a proposal**, and the front end must not offer
it (R41, `ssot/ui-frontend.md` §3). This is the rule that would have prevented a tab pointing at nothing.

The reasons are structural, not stylistic:

- **The pattern is already the architecture.** Match structure is expressed as a GameState component supplied by
  the experience's component list. Each such component owns exactly one ruleset's end condition and respawn
  policy while reusing the shared spine. A ruleset is selected by *which component the experience adds* — which
  means adding a ruleset adds a component and changes no existing one.
- **The respawn seam is polymorphic.** The always-loaded restart-policy interface (`ShouldBlockRestart()`) is
  consulted by the game mode before permitting a restart. BATTLE ROYALE answers *block, for the whole match*;
  MATCH PLAY answers *block within a round, release between rounds*. That is the entire respawn divergence,
  expressed through a seam that exists precisely so a ruleset can declare its policy **without editing the game
  mode** — and it is why a third ruleset with a third answer needs no new seam either.
- **The scoring substrate is shared.** Per-player elimination/death/assist counters are already replicated
  player state, fed by the existing elimination-message pipeline; any ruleset may read them. **Placement, by
  contrast, is a per-match ordering that only the ruleset's own component can produce** — over N participants
  for BATTLE ROYALE, over 2 sides for MATCH PLAY — which is exactly why it lives in that component and not in
  the shared substrate.
- **A fork would duplicate the shared 80%** — roster, match id, telemetry, join coverage, phase spine — and the
  duplicate would drift. A mode flag inside one component would make every future read of that component ask
  "which ruleset am I in?" at every branch, which is how end-condition bugs get written.

**The rule this establishes:** *a new ruleset is a new match-structure component. It may not be a flag inside an
existing one, and it may not fork the shared spine.*

---

## 4. THE COMBAT-RULES AXIS IS ORTHOGONAL — READ THIS TWICE

**A match is `(ruleset × league)`. The two axes are independent and neither constrains the other.**

> **⚠ LEAGUE IS NO LONGER AVAILABLE EVERYWHERE (R86).** The matrix below is still complete *within LEAGUE
> PLAY*, but **the two STAKED tiers offer PRO MOD only** — you wager on the precise, gore-free,
> enhanced-movement model, and HAYWIRE is where you play for progression. So a Haywire BATTLE ROYALE and a
> Haywire MATCH PLAY both exist, and neither can be staked. **This does not break the orthogonality argued
> below** — the axes remain independent *given a tier*; what changed is which tiers publish which league,
> which is a matchmaking question (`ssot/matchmaking.md` §4.2), not a ruleset one.
>
> **A THIRD AXIS EXISTS AND IS NOT THIS DOCUMENT'S.** Since **R76/R85**, a match also runs in a **tier** —
> LEAGUE PLAY, WATTS PLAY or VOLTS PLAY —
> WATTS PLAY or VOLTS PLAY (`ssot/matchmaking.md` §4.2, `ssot/economy-store.md` §3.1). It is deliberately not a
> row in the table below, because **it changes nothing about how a match ends, how respawn works, or what is
> scored** — which is the whole definition of a ruleset (§2). It is orthogonal to both axes here in exactly the
> way they are orthogonal to each other: every combination is legitimate, and the full queue formula is
> `currency × ruleset × bracket × league`. Recorded so a reader of this section does not conclude the match
> identity is complete at two axes.

| Axis | Values | What it governs |
|---|---|---|
| **Ruleset** (this document) | MATCH PLAY · BATTLE ROYALE | How the match ends, respawn policy, what is scored |
| **Combat rules / league** | **HAYWIRE** · **PRO MOD** | Damage model and the granted movement kit |

- **HAYWIRE** — the dismemberment combat model: the zone-HP damage routing and its gib/consequence layer are
  active.
- **PRO MOD** — gore-free by construction: the dismember feature is not loaded and a mode gate routes all damage
  to the conventional single-health chain. It also carries the enhanced movement kit.

**The independence is the point, and it is the thing most likely to be misread.** BATTLE ROYALE is not "the
Haywire mode" and MATCH PLAY is not "the Pro Mod mode." Every combination is legitimate: a Haywire BATTLE
ROYALE, a Pro Mod BATTLE ROYALE, a Haywire MATCH PLAY, a Pro Mod MATCH PLAY. A player picking a league is
choosing how combat feels; a player picking a ruleset is choosing how the match is won.

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
  see the countdown), not an inter-round pause. Running it per round would turn BATTLE ROYALE into a series,
  which it is not; and it would multiply a match-entry ritual by up to thirteen in MATCH PLAY, which spends the
  series waiting.
- **MATCH PLAY's round reset is NOT a warmup, and the distinction is load-bearing.** A round reset is a short
  countdown that re-spawns both teams between rounds (§2.2). It does not re-run the entry ritual and it is not
  the phase spine's to own. **Anything that freezes abilities on the round boundary is implementing a warmup and
  is wrong** — the freeze belongs to the match edges, not the round edges, or the arsenal tag contract below
  fires up to thirteen times a match.
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

## 6. THE ZONE — SCOPED TO BATTLE ROYALE

**The zone is a BATTLE ROYALE mechanism. It exists to force last-standing to resolve.**

Under permanent death and no clock, a BATTLE ROYALE on a large footprint has no intrinsic pressure to end: two
surviving participants can avoid each other indefinitely. A shrinking playable area removes that possibility by
making avoidance progressively impossible. **The zone is the end-condition's enforcement, not decoration.**

**MATCH PLAY does not need a zone, and the reason is precise: it already has a per-round timer** (§2.2). The
stall case the zone exists to remove cannot occur, because a round that nobody resolves is resolved by the clock
on banked progress. Termination is already guaranteed by a mechanism that is cheaper than a ring and produces no
extra deaths.

**What a MATCH PLAY ring would have to justify:** a pacing benefit against three real costs — (a) it competes
with the round reset, since every round re-spawns both teams and a shrinking area shrinks the legal spawn set;
(b) it adds a source of death that no side caused, in a format whose result is a *round win*, so a zone death
would either decide a round or need excluding, and both are design debt; (c) it adds replicated state to a
format that already replicates round state. **The default is no ring under MATCH PLAY.** A proposal to add one
must answer all three.

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

**None of these is a status claim — each is a decision the mode system owes.**

1. ~~**MATCH PLAY's size range.**~~ — **CLOSED, and the item answered itself: the RULESET fixes no range;
   PUBLICATION is a matchmaking decision.** MATCH PLAY supports 1v1 through 8v8 by construction — they are the
   same ruleset with the same two positions, so no size needs ruleset work to exist. **Which sizes are
   published is governed by two existing rules, not by this document:** a bracket is offered only where a map
   backs it, and a published bracket that cannot hold a population folds under **R61**. That is the whole
   answer. Fixing a range here would be this SSOT deciding a queue question, which is exactly the coupling
   §3.3 warns against — and it would have to be re-edited every time a map shipped.
2. ~~**Does round differential feed rank?**~~ — **CLOSED by R66** (`ssot/league-play.md`): **outcome only, but
   the differential is RECORDED** so it can be switched on later as config rather than a re-derivation. The fork
   this item named was real and the tie-breaker was incentive, not arithmetic — differential is a reason to keep
   shooting a beaten opponent, and this game is played for money.
3. ~~**The extract-bank route's weight.**~~ — **CLOSED by R72: equal.** One round either way, no score bonus
   and no speed bonus. This item's own warning stands as the reason — making them unequal *by accident* is a
   balance bug, and making them unequal on purpose collapses whichever route loses into a fallback.
4. ~~**Half-time side swap under odd formats.**~~ — **CLOSED by R73, which found the question DISSOLVES.**
   First-to-7 cannot resolve before round 7, and the swap is after round 6, so **the swap always happens before
   the series can end.** There is no odd-format case to rule on.
5. ~~**BATTLE ROYALE squad payout attribution.**~~ — **CLOSED by R92: EVEN SPLIT, across the ROSTER.** The
   squad's one payout divides equally among the players who *started* the match on that squad — not among the
   survivors, and not by contribution. **Contribution-weighting is the option that had to lose**, on integrity
   grounds rather than taste: it makes teammates compete with each other for credit inside a match they are
   supposed to be cooperating in, it is the natural home for a collusion argument nobody can audit, and it
   makes a settlement figure non-deterministic at the moment it must be paid. **Splitting across the roster
   rather than the survivors is the load-bearing half** — a survivors-only split pays the living more when a
   teammate dies, which prices a teammate's death as a benefit in a staked match. **§5.2's generating rule is
   untouched**: `N` is the number of SQUADS, positions are held by squads, and the split happens strictly
   after the position payout is determined.
6. ~~**Whether a third ruleset is wanted at all.**~~ — **CLOSED by R93: NO — not now — and the bar for ever
   adding one is set here rather than left to be argued case by case.** Two fully-populated rulesets beat three
   that split the queue (`ssot/matchmaking.md` **R7**), and **R85/R86 raise that cost since this item was
   written**: the ruleset axis now multiplies against the tier axis, so a third ruleset is no longer one
   division of the queue but several. **The bar, stated once:** a candidate must **(a)** produce **finishing
   positions**, so the payout apparatus transfers unchanged — this is precisely what parks TURBO (§9.7), which
   produces none and therefore cannot be paid, and *a ruleset that cannot be paid cannot be staked*; **(b)**
   bring population rather than move it, which means it answers a demand the two rulesets genuinely cannot; and
   **(c)** clear the bar in **LEAGUE PLAY first**, where an unrated tier can carry a thin population without a
   settlement failure. **§3.3 staying cheap is what makes waiting free** — the ease of adding one is an
   argument for deferring, never for proceeding.

### 9.7 PARKED — TURBO, and the design that goes with it

**TURBO is parked by R41, not deleted.** It was a real design — a single timed match, instant respawn, scored on
kill ratio — and it is preserved here so that reviving it is a decision rather than a rediscovery.

**Its shape, recorded once:** end on a hard clock (7 minutes standard, 10 high-stakes) with no series; instant
respawn; kill-ratio scoring against deaths with assists credited; dense district-scale footprints, because
instant respawn on a sparse footprint spends the match walking. **Why a clock and no series:** known duration
and no dead time were its two properties, and a best-of-N reintroduces both problems it existed to remove.
**Why ratio rather than raw kills:** raw kills reward whoever fed hardest into the respawn loop.

**The gaps it never closed, and they are the reason parking costs nothing:**

- **Team model** — solo, squad, or both. Kill-ratio is individually attributed, so a squad TURBO must define
  whether rank input is the individual's ratio, the squad's aggregate, or both.
- **The ratio formula** — assist weighting, what a zero-death score evaluates to (a pure ratio is undefined
  there), and whether the score is monotonic within a match.
- **⚠ ITS PAYOUT SHAPE — the structural one.** BATTLE ROYALE and MATCH PLAY both produce **finishing
  positions**, so one payout rule serves both. **TURBO produces none** — every participant is present at the
  end — so the entire positions-based apparatus does not transfer, and a TURBO payout would have to be defined
  against the *ratio itself*: tie resolution, participation floor, whether a losing player earns at all.
  **This is the deepest reason it is parked rather than half-built.** A ruleset that cannot be paid cannot be
  staked, and every mode here is played for stakes.
- **Respawn timing and its exploit surface** — "instant" must state zero-delay or not, and where the
  participant re-enters. In a staked format, spawn-camping and favourable-re-entry trading are exploits.
- **Time-expiry tie resolution.**
- **High-stakes as a duration or a distinct ruleset** — 7 versus 10 minutes was a parameter; if it were ever to
  differ in more than duration it becomes a third ruleset, and that boundary should be decided, not discovered.

**To revive it:** answer the payout question first. The rest are tractable; that one decides whether the ruleset
can exist in a staked game at all.

---

## 10. RULINGS OF RECORD

| Ruling | Date | Content |
|---|---|---|
| **R92 — A SQUAD'S BATTLE ROYALE PAYOUT SPLITS EVENLY ACROSS THE ROSTER** | **2026-08-06** | A squad holds one finishing position and receives one payout; it divides **equally among the players who started the match on that squad** — not among survivors, and **not by contribution** (§9.5). Contribution-weighting loses on integrity, not taste: it sets teammates competing for credit inside a match they must cooperate in, it is an unauditable home for collusion, and it makes a settlement figure non-deterministic at the moment it must be paid. **Roster-not-survivors is the load-bearing half** — a survivors-only split pays the living more when a teammate dies, pricing a teammate's death as a benefit in a staked match. **`ssot/economy-store.md` §5.2 is untouched:** `N` counts SQUADS, positions are held by squads, and the split occurs strictly after the position payout is determined. |
| **R99 — BATTLE ROYALE FIELD LAW: MIN 9, MAX 36, THREE RUNGS** | **2026-08-07** | **BATTLE ROYALE IS ONE VERSUS EVERYONE** (last participant standing, §2.1). **Minimum 20 positions, maximum 36; the shipped ladder is `BR_20` and `BR_36`** (§2.1.1). **Keyed on POSITIONS, never player count** (R37): solo = players, squad = SQUADS (R92), so the floor is 20 *squads* in squad BR. **THE LADDER IS `BR_9` · `BR_20` · `BR_36`**, each at the TOP of a paid-count tier: 9 → 1 paid (winner-take-all), 20 → 3, 36 → 6. **WINNER-TAKE-ALL IS NOT A DEFECT — R37 DESIGNS FOR IT** (*"winner-takes-all for small fields, scaled payouts above"*), §5.2 states the N<10 behaviour deliberately, and **every MATCH PLAY bracket is winner-take-all too** because a team mode has two positions. **`BR_9` also earns its place on product grounds (operator):** a substantial share of players prefer the small all-or-nothing format, and it is **the fastest-filling bracket there is** — a fragmentation *mitigation* in a young population, filling when 20 and 36 cannot. **9 rather than 8** is this ruling's own rule applied: both are winner-take-all, but 9 carries one more buy-in into the same single prize. **HONEST-CARD OBLIGATION: `BR_9` has NO min-cash floor** (R37's 1.40× exists only at N ≥ 10), so the front end must label it **winner-take-all** on the card itself. **THE MAXIMUM IS 36** because `p(36) = 6`, it is R40's worked example (13.29× winner multiple), and it matches the measured ShantyTown envelope; **past 36 the binding constraint stops being the map and becomes POPULATION**, since a staked field must fill with humans (no bots, `ai-bots.md` §6.3). **NEVER SIZE A BRACKET AT N = 10, 14, 21, 27 OR 34** — the exact values where `p(N)` increments, each carrying a **17–23% drop in the winner's multiple** (§5.3). Sit at the TOP of a tier (20 · 26 · 33 · 36), never its foot. **THREE RUNGS, NOT SIX:** `matchmaking.md` §4.3 makes a bracket the expensive adder — **8 queue cells each** — and §5.1 says fewer, fuller pools beat more, thinner ones; 24 and 32 would split existing pools to buy a difference a player cannot feel, and each sits below the top of its tier anyway. **CONSEQUENCE: `BR_18` → `BR_20`** in the queue registry (same 3 paid places, a strictly larger pot). |
| **R93 — NO THIRD RULESET; THE BAR FOR ONE IS SET** | **2026-08-06** | The ruleset axis stays **MATCH PLAY and BATTLE ROYALE** (R41). Two fully-populated rulesets beat three that split the queue (`ssot/matchmaking.md` **R7**), and **R85/R86 raise the cost**: the ruleset axis now multiplies against the tier axis, so a third is no longer one division of the queue but several. **A candidate must (a) produce FINISHING POSITIONS**, so the payout apparatus transfers unchanged — the exact test that parks TURBO (§9.7), which produces none, *and a ruleset that cannot be paid cannot be staked*; **(b) bring population rather than move it**; and **(c) clear the bar in LEAGUE PLAY first**, where an unrated tier can carry a thin population without a settlement failure. §3.3 staying cheap is what makes waiting free — the ease of adding one is an argument for deferring, never for proceeding. |
| **R72 — THE TWO ROUND-WIN ROUTES ARE EQUAL** | **2026-08-06** | Wiping the enemy team and completing the central-extract bank are each worth **exactly one round**, with **no score bonus and no speed bonus** (§2.2). §2.2's stated intent is *two genuinely viable routes*, and weighting either one collapses the other into a fallback — a bank worth more makes the fight something to avoid, which in a shooter is an identity change rather than a balance tweak. **Precedent is exact and long-running:** CS2 and Valorant both treat the objective and the elimination as the same single round win, and both have stayed balanced on that basis for over a decade. **Closes §9.3.** |
| **R73 — THE HALF-TIME SWAP QUESTION DISSOLVES** | **2026-08-06** | §9.4 asked what happens to the side swap in a series that ends before half. **It cannot happen.** MATCH PLAY is first-to-**7** of a best-of-13, so the earliest possible resolution is **round 7**, and the swap is after **round 6** — **the swap always occurs before the series can end**, at every published size from 1v1 to 8v8. No rule is needed and none is written. Recorded as a ruling rather than a deletion so the question is not re-opened by someone who notices the gap and assumes it was overlooked. **Closes §9.4.** |
| **R41 — THE TWO RULESETS ARE MATCH PLAY AND BATTLE ROYALE; TURBO IS PARKED** | **2026-08-06** | **The ruleset axis is renamed to the two match structures that exist.** (a) **SHOOTOUT → BATTLE ROYALE** — a rename only; §2.1's properties are unchanged and every citation of §2.1 still lands on the same ruleset. The old name was borrowed from poker and described nothing about the format; the new one is the name of its own match structure. (b) **TURBO is REMOVED from the ruleset axis and PARKED at §9.7** — its design is preserved in full, including the payout gap that is the real reason it cannot ship: it produces **no finishing positions**, so the positions-keyed payout rule (R36/R37) does not transfer, and **a ruleset that cannot be paid cannot be staked.** (c) **MATCH PLAY takes the second slot and is NOT TURBO renamed** — different end condition, different respawn policy, different scoring, no property in common. It is the two-team round series (§2.2), which had **no name and no front-end tab** despite being the most-specified format in the tree. **THE DEFECT THIS FIXES IS NOT THE DEAD TAB.** One tab named a mode after something it was not; the other named a mode that did not exist; and between them **the round series had no way to be reached at all.** **Consequence, and it is a gain:** MATCH PLAY has two finishing positions, so its payout resolves to winner-takes-all by the existing rule — which **closes** the disabled-payout-tab question carried at `design/IRONICS_LOBBY_UX_HANDOFF.md` §16.7 rather than deferring it. **New rule from §3.3: a ruleset with no match-structure component behind it is a proposal, and the front end must not offer it.** |
| **R1 — MELEE IS CUT** | **2026-08-05**, **amended 2026-08-06** | The game is **dual-mode, not tri-mode**. There are two rulesets and no melee/deathmatch ruleset. The instant-respawn arcade format previously specified as its own mode is **not** a third ruleset. **This is settled; a reader should not reopen it.** The superseded melee document remains on disk pending archival and is not a source of design authority. **AMENDED BY R41 — NAMING AND ONE CLAUSE.** The pair is now MATCH PLAY and BATTLE ROYALE, not SHOOTOUT and TURBO. R1's original wording placed melee's tempo in TURBO; **with TURBO parked, that home is gone.** The cut stands on its own merits — melee was cut for being a third ruleset, not for having somewhere else to go — but a reader should know **the instant-respawn tempo is now homeless**, and reviving it means reviving TURBO (§9.7), not reopening melee. |

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
