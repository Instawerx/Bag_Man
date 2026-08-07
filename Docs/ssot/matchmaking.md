# SSOT — MATCHMAKING (Tier 2)

**What this is:** what the matchmaking system **is** and **why**. It changes when the system is redesigned.
**What this is not:** a status board. **This document contains no status claims** — nothing here says what is
built, proven, done or owed, and no commit hash appears as evidence of progress. Those belong to Tier 3
(`LIVE_TRACKER`).

> **Why the separation matters here in particular.** Matchmaking is where design decisions become *configuration*,
> and configuration is the thing most likely to be described by what it currently contains rather than by what it
> is for. A queue list is not a design; the rule that bounds the queue list is.

**Doctrine is cited, never restated.** Laws live in [`Docs/DOCTRINE.md`](../DOCTRINE.md) and are referenced by id
(**N1**, **N11**, **N12**, **G5**, **G7**, **P8**).

---

## 1. SCOPE

This SSOT governs: what the player is actually choosing when they queue · the three-layer separation between
player-facing queues, server-side pools, and the play-spaces that load · how stake enters a ticket and how it is
banded · how region is handled · how population is disclosed · party formation and its relationship to team
assignment · the staking contract's binding key and evidence chain · and the interfaces matchmaking owes to the
league and economy systems.

It does not govern: what a ruleset is (→ `ssot/match-modes.md`), which play-spaces exist or how they are sized
(→ `ssot/map-build-system.md`), rating math or ladder shape (→ `ssot/league-play.md`), currency definitions,
rake, or payout curves (→ `ssot/economy-store.md`), or the state of any implementation (→ Tier 3).

---

## 2. THE PLAYER NEVER PICKS THE MAP

**The front end is a STAKE LOBBY, not a map browser.** A player chooses *what they are playing for* — bracket,
league, ruleset, stake — and never *where*. **Venue is a server outcome**, disclosed honestly in the UI as
*"venue assigned at match start."*

### 2.1 Why

**Population fragmentation.** Every player-facing choice splits the matchmaking pool, and the split is
multiplicative. A map browser turns one queue into one queue *per venue*, and each resulting pool is a fraction
of the population that has to fill a match on its own. **Queue time is the strongest single predictor of a
multiplayer title failing** — not balance, not content volume. A player who waits does not experience the map
they chose; they experience waiting, and then they leave. Map selection is a feature that consumes the thing
that makes every other feature reachable.

**Staked integrity.** Player-selected venue is a lever, and a lever in a staked match is an exploit surface:
- **Map-specific advantage** — a player who has ground one venue selects it every time, converting familiarity
  into an edge that the stake then pays out on.
- **Opponent steering** — a player who can pick a venue can pick the venue an opponent is weakest on, and with
  an open lobby browser can wait for that opponent specifically.

**Server selection removes the lever entirely.** There is no venue preference to exploit because there is no
venue preference to express. This is the same reasoning as server authority over match state (**N1**): if a
client can influence a condition the wager settles on, that influence is worth money.

### 2.2 What the player does control

**Tier** (LEAGUE PLAY · WATTS PLAY · VOLTS PLAY) · bracket (how many a side) · ruleset · **venue class
(ARENA / MAP)** · stake amount · party — and **league (how combat feels) only inside LEAGUE PLAY**, where both
are offered (R86). Everything they choose is a property of the *contest*. Nothing they choose is a property of
the *terrain*.

> **VENUE CLASS PASSES THAT TEST; THE MAP STILL DOES NOT (R97).** The class is *how the contest is shaped* —
> contained and round-based, or district-scale. The MAP remains a server outcome inside the chosen class, so
> **R18 is intact**: this is still a stake lobby, not a map browser. The player says "an Arena fight"; the
> server still says which Arena. Anyone reading this as permission to let players pick maps has read a
> statement about the *shape of the contest* as a statement about *terrain*.

---

## 3. THREE LAYERS, NOT ONE ASSET

The failure this prevents: **configuration count growing with content.** If a venue is a lobby entry, then every
new map, every new district and every new size variant adds a player-facing choice — the front end grows without
bound, the pool fragments further with each addition, and content becomes something the game gets *worse* at
absorbing.

| Layer | Audience | What it carries | Growth behaviour |
|---|---|---|---|
| **QUEUE** | **Player-facing** | **Ruleset** (the top-level split, §4.1) · bracket · league · party-size range · ranked flag | **Few. Bounded. Does NOT grow with content** (§4) |
| **MAP POOL** | **Server data** | Which play-spaces serve which party sizes; weighting, rotation, recency | Grows with content — one row per servable play-space |
| **PLAY-SPACE** | **Never player-facing** | The experience that actually loads: map × district × size | **Unlimited** |

### 3.1 The design property this buys

> **Adding a district adds a POOL ROW, not a lobby entry. The lobby is invariant under content growth.**

This is the property to protect. A new district, a new map, a new size variant on an existing map — each is a
row in server data that changes what the server *may* select, and changes nothing the player sees or chooses.
Content expands the server's options while the player's decision space stays fixed.

The corollary is a test: **if a proposed feature would add a player-facing entry per unit of content, it is
breaking this layer separation** — regardless of how reasonable it sounds in isolation.

### 3.2 Soft references throughout

Pools reference play-spaces **softly**. A pool that hard-references every experience it can select drags the
entire map set into memory the moment the pool is loaded — and the pool is loaded by the front end, which is
exactly where load cost is least affordable. The front end must be able to read *"this queue has 41 servable
play-spaces"* without loading any of them.

This also keeps the reference direction clean: pools point at play-spaces; play-spaces know nothing about pools
(**G5** — a data asset with no consumer is inert, and the consumer here is the server-side selector, not the
experience).

### 3.3 Selection is a server concern

Weighting, rotation and recency live in pool data because they are tuning, not design: how often a venue comes
up, how long before it can repeat, whether a new venue is boosted while it is fresh. Changing them is a config
edit and a live-ops lever — **never a code change and never a player-visible restructure.**

---

## 4. QUEUE COUNT IS BOUNDED BY DESIGN

### 4.1 Ruleset is the TOP-LEVEL choice — the queue splits (R7)

**MATCH PLAY and BATTLE ROYALE have separate queues, and ruleset is the FIRST choice a player makes — above
size and above stake. Two tabs, not one doubled flat list.**

The hierarchy matters as much as the split. A flat list of every `currency × ruleset × bracket × league` combination is
the same information presented as an undifferentiated wall; two tabs present it as *"which game are you here to
play, and then what size."* The player answers one question before being asked the next.

**Why the queue splits rather than merging:**

- **They are different products, not variants of one.** A player who wants a bounded team series and is dropped
  into a last-standing free-for-all does not experience a slightly-off match — they experience the wrong game. They quit, and **on a wagered match
  they eat a leaver penalty for it.** That is the worst failure available in this system, and a merged queue
  aims it squarely at the mode carrying money. No matching gain justifies it.
- **Observed practice separates by FORMAT.** Wager and competitive platforms split on format, not on variant of
  format; elimination and respawn formats are not merged into one entry anywhere it matters. The precedent is
  consistent enough to be treated as a finding rather than a preference.
- **The fragmentation objection does not survive contact.** Merging doubles the *entries* but does **not**
  double the *pool*: those players were never interchangeable. A merged queue's population is **nominal, not
  effective** — it counts people who would refuse the match it is about to offer them. Where a band genuinely
  goes cold, the transparency mechanism (§7) already folds it into a neighbour in configuration, which is the
  correct instrument for that problem.

### 4.2 The arithmetic

Queue count is the product of the **contest** dimensions only:

```
  queues  =  (tier × league) pairs  ×  rulesets  ×  VENUE CLASSES  ×  brackets
```

**There are exactly FOUR (tier × league) pairs**, because league is **tier-scoped** (R86) rather than a free
multiplier:

| Tier | Buy-in | Leagues offered | Rated | Bots |
|---|---|---|---|---|
| **LEAGUE PLAY** | **none** | **HAYWIRE + PRO MOD** | No | **Permitted** |
| **WATTS PLAY** | Watts | **PRO MOD only** | Yes | No |
| **VOLTS PLAY** | Volts | **PRO MOD only** | Yes | No |

- **Rulesets** — **2** (MATCH PLAY, BATTLE ROYALE), split per R7 (§4.1) and expressed as tabs.
- **Venue classes** — **2** (ARENA, MAP), admitted by **R97**. **These are two styles of play, not two
  scenery sets**, which is the only reason a terrain-shaped word appears on a list of contest properties at
  all. An ARENA is the round-based, contained, extraction-shaped contest; a MAP is the district-scale one. A
  player who queued for one and was handed the other did not get a bigger version of their match — they got a
  different game, and §4.3's admission test is exactly that pools which cannot merge earn a dimension.
- **Brackets** — the party-size bands the district model serves: `1v1, 2v2` · `3v3, 4v4` · `5v5, 8v8`, plus the
  BR counts. On the order of **8**.

That places the player-facing entry count **on the order of 64**, reached through a layered choice rather than
presented flat.

> **LEAGUE IS NO LONGER A FREE MULTIPLIER, AND THAT IS THE POINT (R86).** It was `× 2` across everything; it is
> now two pairs in one tier and one pair in each of the others. **Staked play is PRO MOD only** — you wager on
> the precise, gore-free, enhanced-movement model, and HAYWIRE is where you play for progression. The queue
> count is unchanged at 28 concrete cells, but **half of them are now bot-fillable**, because LEAGUE PLAY
> carries neither stake nor rating and `ssot/ai-bots.md` §6.3 therefore permits fill there. **That is the real
> gain: the on-ramp always starts.** **Not every cell is necessarily offered** — a ruleset suited to arena footprints need not be
published at every bracket (`ssot/match-modes.md` §2.2), and an unoffered cell is a pool that never had to be
filled. After R63 the concretely publishable set is **28**, not 64.

A **ranked flag** rides the queue rather than doubling it wherever ranked and unranked can share a pool; where
they cannot, it becomes another multiplier and should be recognised as such rather than added quietly.

> **CURRENCY IS THE CASE THAT SENTENCE DESCRIBES, AND R76 IS THE RECOGNITION IT DEMANDS.** Watts and Volts
> players **cannot** share a pool — two participants staking different currencies cannot settle one pot — so
> currency is not a flag that rides the queue, it is a multiplier. It is recorded here as one rather than
> arriving quietly, which is exactly what this paragraph asks of any fourth dimension.

### 4.3 The invariant

> **Content growth must never increase the queue count.**

Maps, districts, play-spaces and size variants are **pool rows** (§3). The only things that may add a queue are
a new bracket, a new **(tier × league) pair** (R76, R86), a new **venue class** (R97), or a new ruleset — all
of which are *design* decisions made deliberately, and all of which are rare.

> **⚠ VENUE CLASS IS THE ONE ADDER THAT LOOKS LIKE TERRAIN, AND THE DISTINCTION IS THE WHOLE RULING.** Adding
> a MAP to a class is still a pool row and still free. Adding a *class* is a design decision about how the
> game is played. If someone later argues that a third class is needed because a new map does not fit the two,
> **that is the invariant failing** — the answer is a pool row, or a bracket, not a class. **If the queue count is rising because content shipped,
the layer separation has failed.**

> **THE FOURTH ADDER WAS ADMITTED DELIBERATELY, AND THE TEST IT PASSED IS WORTH RECORDING.** A dimension earns
> a place on this list only if the pools it creates **genuinely cannot merge**. Brackets cannot merge because a
> 5v5 is not an 8v8; rulesets cannot merge because the match ends differently; leagues cannot merge because the
> damage model differs. **Currency cannot merge because a pot cannot settle in two denominations.** Region, by
> contrast, failed this test (§6) and stayed off the queue — the difference is not importance, it is whether a
> shared pool would produce a match that cannot resolve.

---

## 5. STAKE IS A TICKET PARAMETER, NEVER A QUEUE DIMENSION

**Stake is a free-entry amount carried on the ticket. It is not a queue.** A stake-tier queue set would multiply
the queue count by the tier count and fragment the pool along a second axis — the precise failure §3 and §4
exist to prevent.

> **⚠ THE AMOUNT AND THE CURRENCY ARE DIFFERENT THINGS, AND ONLY ONE OF THEM IS A QUEUE DIMENSION.**
>
> | | Is it a queue dimension? | Why |
> |---|---|---|
> | **Stake AMOUNT** (450, 2,500 …) | **No — a ticket parameter.** Unchanged by R76. | Amounts within a currency **can** settle together; the server bands them (§5.2) and widens the band over wait (R60). Making each tier a queue is the fragmentation this section forbids. |
> | **Stake CURRENCY** (Watts / Volts) | **YES — a queue dimension (R76).** | Amounts in *different* currencies **cannot** settle together at all. There is no band wide enough to merge them, because the pot itself could not resolve. |
>
> The heading above still holds in full: it forbids **tiering** the queue by amount, and R76 does not do that.
> A reader who takes the heading as also forbidding the currency split has read a statement about *amounts* as
> a statement about *denominations*.

### 5.1 Entry UX

- **Presets are primary** — the common amounts, one tap. Most players never leave this path.
- **An editable numeric field is secondary** — for a player who wants a specific figure.
- **NO SLIDER.** A slider is slow to land on a value, imprecise by construction, and poor on mobile where a
  fingertip covers the target. Stake is a number the player has in mind; the interface should accept it, not
  make them hunt for it.

### 5.2 Banding — why an exact amount cannot be matched

**The server bands the entered amount and matches within a tolerance band.** Without banding, two players
entering 437 and 450 are two pools of one, and the free-entry field silently becomes the worst possible queue
splitter — unbounded, invisible, and self-inflicted.

- **Bands widen over wait time**, exactly as skill tolerance widens. A player who has waited is matched against
  a broader stake range, because a slightly-off stake is strictly better than no match.
- **The band is disclosed in the UI** — *"matching 400–500 V"*. The player must never believe they will be
  returned their exact figure. Disclosing the band up front makes the settled amount expected rather than a
  surprise, and a surprise about money is the kind of surprise that costs trust permanently.

### 5.3 Server validation

The server validates the entered amount against the player's balance and the tier table before a ticket is
accepted. **A client never asserts what it can afford** (**N11**) — a balance is a server fact, and a stake
accepted on a client's word is a stake that can be fabricated.

> **THE CLIENT NEVER SNAPS EITHER (R59).** The player types any integer and it travels on the ticket
> **unrounded**; the **server** resolves it to a band. A client-side snap would have the client choosing which
> pool the player enters — the same class of claim as asserting a balance, and forbidden by the same law. It
> also buys nothing, because the server must re-resolve regardless. The band readout (§5.2) is what makes this
> invisible to the player: they see the outcome before they commit.

**The tier table is `100 · 500 · 2,500 · 10,000 V` (R68, `ssot/economy-store.md`).** Four rungs, ~5× geometric.
Higher tiers are deliberately unshipped until the top rung is populated — an unfillable tier would advertise its
own emptiness under §7, which is the one thing that section exists to prevent.

---

## 6. REGION IS A PLAYER PROFILE ATTRIBUTE, NEVER ON THE TICKET

**Players queue from anywhere into any bracket.** Region does not split pools and adds no queue assets.

### 6.1 The three read points

Region is read at exactly three places, none of which is matchmaking:

1. **League standings** — regional boards are *filtered views of the same result set*, not separate competitions
   fed by separate pools.
2. **Prize eligibility** — whether a player may receive a given prize in their territory.
3. **Backend reporting** — population and health by region.

### 6.2 Self-declared origin, with change friction

Origin is **self-declared**. That is the right default: it needs no identity documents, no IP policing, and no
false positives against travellers, VPN users, dual-nationals or anyone on a mobile carrier that geolocates
badly.

Self-declaration is gameable **once prizes attach** — a player could re-declare into whichever region has the
weakest board. The defence is **friction, not verification**:

> **Region is set once and changed rarely, and a change takes effect at the next season boundary — never
> immediately.**

This defeats leaderboard-shopping without any identity apparatus: the value of hopping regions comes from doing
it *late*, when the standings are known. A change that lands only at the next season reset removes the timing
that makes it worth doing, while remaining completely reasonable for the genuine case (a player who actually
moved).

### 6.3 Granularity — the schema decision

**Store COUNTRY. Derive a broader REGION BRACKET for ranking.**

- **Country is the fact, and it is what eligibility needs.** Prize eligibility is territorial and legal; it is
  precise to a country and cannot be answered from a continent.
- **A broader bracket is what ranking needs.** A country-level ladder in a small market is a leaderboard of
  three people — it is not a competition, and it devalues the standing for everyone in it. Ranking needs
  population depth, which means aggregating countries into brackets.
- **Store the precise value; derive the coarse one via a mapping.** Storing only the bracket destroys
  eligibility precision permanently and irreversibly. Storing only the country forces every future
  bracket-boundary change into a data migration. Storing country as the fact and treating the bracket as a
  **config-driven mapping** means bracket boundaries can be re-cut — as population shifts and a region grows
  enough to stand alone — **without touching a single player record.**

**Why this is decided here rather than later:** it determines the leaderboard schema, and a schema is expensive
to change **once players hold standings in it**. Standings are a durable player-visible possession; a migration
that re-buckets them either invalidates history or creates two incompatible eras of it.

---

## 7. POPULATION TRANSPARENCY

**Live population counts and estimated wait are shown per size and per stake band — and, following R7 (§4.1),
counts are per RULESET.** Because ruleset is the top-level choice, a player sees the population of the format
they are actually choosing; an aggregate across both rulesets would advertise players who would never accept
the match. **A count that includes people who would refuse the offer is worse than no count** — it converts an
honest signal into a misleading one.

This is three things at once:

1. **The UI-side fix for fragmentation.** Given visible counts, players **self-select into populated bands**.
   The pool consolidates because players can see where the players are — a behavioural solution to a
   distribution problem, achieved without removing choice.
2. **A live-ops lever.** A band that has gone cold **folds into its neighbour in configuration** — no code
   change, no client update. Combined with §3's pool layer, the response to a population shift is a config edit.
3. **Honesty.** An empty band **looks empty** rather than silently never matching. The failure mode being
   avoided is a player queueing into a band that cannot fill, waiting, and concluding the game is broken. Showing
   zero is a worse-looking UI and a far better experience: it converts a mysterious failure into an obvious,
   actionable choice.

The same disclosure principle governs the stake band (§5.2) and venue assignment (§2): **tell the player what
the system is actually doing.** In a staked context this is not just courtesy — an undisclosed mechanism that
affects money reads as a rigged one.

---

## 8. PARTY FORMATION AND TEAM ASSIGNMENT

### 8.1 Party → eligible queues

A party is formed before queueing and travels **with the ticket** as a unit. **Party size determines which
queues the party is eligible for**: a queue carries a party-size range, and a party may enter any queue whose
range admits it. A party larger than a bracket's team size cannot enter that bracket — there is no seat
arrangement that keeps the party together, and splitting it is forbidden (§8.3).

### 8.2 Team assignment is a provider decision, not a matchmaking decision

Team assignment sits behind a **provider seam**: matchmaking (or a local source) produces assignments, and the
in-match consumption layer applies them without knowing which source produced them.

- **Matchmaker-authoritative** where a real matchmaking service is the source: teams arrive with the match
  placement, balanced against skill.
- **Local fill** otherwise — offline, casual, and editor testing. Assignment is computed locally and balanced by
  live counts.

**Why a seam rather than one implementation:** the match must always have a valid team source. A design where
teams come only from the backend has no answer for offline or local testing, and the failure is total — no teams
means no match. The seam makes the backend swappable and the local path permanently available.

**Bot fill follows the same split:** permitted where a match should always start (offline, casual), and excluded
where the result must mean something — **a ranked or staked result cannot be produced against bots**, and a
ranked queue holds for real players with an honest visible wait rather than quietly filling.

### 8.3 Party integrity is absolute

> **Same-party members are NEVER placed on opposing sides.**

This is not a balance preference; it is an integrity rule. Two people in a party on opposite sides of a staked
match is the win-trading vector in its most convenient possible form — coordinated, pre-arranged, and invisible
in the result. Balance is handled instead by **capping how many seats a single party may occupy** (no more than
one team's worth), never by splitting the party across the fixture.

The user-facing consequence is also the correct one: **you always play with your party, never against them.**

### 8.4 Solo/FFA is a team CONFIGURATION, not a mode

**Solo is expressed as one team per participant.** It is not a separate mode, a separate ruleset, or a separate
code path — it is the team-assignment layer configured so that every participant is their own team.

This is what makes **BATTLE ROYALE resolve to last *player* standing rather than last *team* standing**: the ruleset
counts surviving teams, and when every participant is a team of one, the last surviving team is the last
surviving player. The ruleset does not know or care which configuration it is running under.

**The consequence to hold on to:** solo-versus-squad is a *lobby* property, and both rulesets support both.
Anything that treats solo as a distinct mode has duplicated a configuration into a code path.

---

## 9. THE STAKING CONTRACT

Staking applies a poker **structure** — entry → pool → payout — to non-cashable in-game currency. The currency
definitions, rake, tiers and payout curves are owned by `ssot/economy-store.md`. What matchmaking owns is the
**binding**: how a stake is attached to a specific contest and how that contest is later proven.

### 9.1 Match id is the binding key

**One server-authored identifier per match binds the stake, the participants, the result and the evidence.**
Every downstream system — escrow, payout, league result, dispute review — keys off the same id. It is authored
server-side once and replicated; a client never proposes it.

Without a single binding key, a settlement is an assertion about which match it settles. With one, escrow,
result and telemetry are three views of the same record.

### 9.2 Server authority over escrow and payout

Entry is **escrowed** — deducted and held — when a seat is taken, and settled from the pool on result. **Both
operations are server-authoritative** (**N1**, **N11**), atomic, and reversible on failure: a match that never
forms must return every entry in full, never strand one. This is the same transactional discipline required of
any ownership transfer (**N12**) — the failure mode is identical (a player charged for something they did not
receive) and so is the remedy.

**The client's role is display.** It shows the stake, the band, the pool and the outcome. It computes none of
them.

### 9.3 The evidence chain

A staked result must be **reconstructable after the fact**. The telemetry stream carries, keyed to the match id:
participants and their assignment, eliminations with ordering and location, match state at each phase
transition, and the resolution.

**Why this is a matchmaking concern and not only a telemetry one:** the evidence must be complete *from the
moment the ticket is accepted*, not from match start. A dispute about a match includes disputes about how the
match was formed — who was matched with whom, at what stake band, into which venue. **A settlement that cannot be
re-read is settled by assertion**, and in a staked economy the party who asserts loudest should not be the party
who wins.

### 9.4 One seat per account

A single account occupies **exactly one seat** in a staked match. Multiple seats controlled by one person is
direct control over the pool's outcome, and it is the collusion vector that requires no coordination with anyone
else.

---

## 10. INTERFACES — WHAT MATCHMAKING OWES

Stated as interfaces (shapes), not implementations.

### 10.1 To the league system — the RESULT SHAPE

Matchmaking (with the match) emits, per match:
- the **match id** (§9.1);
- the **participant set** with team assignment;
- the **ruleset** the match ran, since rank input differs by ruleset — placement over N positions under BATTLE
  ROYALE, the series outcome over 2 under MATCH PLAY (`ssot/match-modes.md` §2);
- the **result** in that ruleset's terms;
- the **league** the match ran under;
- **whether the match was ranked**, and **whether it was staked** — as *independent* facts.

> **The firewall this interface must preserve: stake size is not a field the rating consumes.** Rank moves on
> outcome and opponent skill only. A staked win and an unstaked win of the same result are the same rating event.
> Emitting the stake alongside the result is fine; a rating that *reads* it would make rank buyable.

### 10.2 To the economy — the SETTLEMENT SHAPE

- the **match id** (the same key);
- the **participants and their entries** as escrowed;
- the **outcome ordering** the payout curve consumes — **finishing position under both rulesets**, over N for
  BATTLE ROYALE and over 2 for MATCH PLAY, which is why one curve serves both (`ssot/match-modes.md` §3.2);
- the **terminal state**: settled, cancelled-refund, or held-pending-review.

**The third terminal state is required, not optional.** A match flagged for integrity review must be able to
hold settlement without either paying out or refunding, or every review races the payout it is reviewing.

### 10.3 From the league system

Matchmaking **consumes** a skill rating to form fair matches. It does not own the rating math, the ladder, or
the season structure. Anything that changes how rating is *calculated* is a league concern; how it is *used to
sort a queue* is this document's.

---

## 11. OPEN DESIGN QUESTIONS

**⚑ ALL FOUR QUESTIONS THIS SECTION CARRIED ARE NOW CLOSED (R59–R63, 2026-08-06).** The reasoning is preserved
below because each ruling was taken *against* the alternative recorded here, and a reader who cannot see what
was rejected cannot tell whether the ruling still holds when conditions change.

### 11.1 ~~Skill input — and whether it is per-ruleset~~ — **CLOSED by R64/R65** (`ssot/league-play.md`)

*The question was:* one rating or one per ruleset. Placement skill and kill-ratio skill are not obviously the
same competence; a single rating pools better, separate ratings are fairer but split the rating population.

**Resolved: TWO ladders, one per ruleset, on OpenSkill.** Note the question's own framing is stale — **R41
removed kill-ratio entirely**, so both rulesets now produce a finishing position and the comparison is between
orderings of different *depth*, not different *kind*. §4.1 of `league-play.md` carries the full reasoning.

### 11.2 ~~The band-widening curve~~ — **CLOSED by R60**

*The question was:* how fast, to what ceiling, and symmetric or not. This section already flagged the answer:
*"being matched **up** exposes them beyond their intent."*

**Resolved: stepped and asymmetric** — one band step every 15s, capped at **2 steps down / 1 step up**, hard stop
at 60s, then offer the fold. Asymmetry is the direct answer to the exposure this section identified: in a staked
game, being matched up spends the player's money, not just their time. **Stepped rather than continuous** because
§5.2 requires the band be *disclosed*, and a step is legible in a UI where a moving curve is not.

### 11.3 ~~Minimum viable population per band before it folds~~ — **CLOSED by R61**

*The question was:* the threshold, and automatic versus operator. *"Automatic folding reacts faster; manual
folding never surprises a player mid-session."*

**Resolved: automatic, with an operator override.** Threshold — fewer than **2× field size** in the band over a
rolling **5 minutes**. Automation is in the spirit of §7, which already rules folding a config-level lever
needing no code change; the override answers this section's own concern, and is what a launch window needs when
the data is thin and atypical.

### 11.4 ~~Party-versus-solo matching fairness~~ — **CLOSED by R62**

*The question was:* segregation, free mixing, or compensation. This section correctly noted the interaction with
§8.1's party-size ranges and with the rating.

**Resolved: no segregation; compensate through the rating.** Party seats stay capped at one team's worth (§8.3).
Segregation is what a dense ladder can afford and we cannot — it halves an already thin pool, and §7 would then
have to show two sets of bands as empty instead of one. **The interaction this section predicted is now
load-bearing:** the rating consumed *does* account for coordination, which makes R62 a dependency of R65 rather
than an independent choice.

---

## 12. RULINGS OF RECORD

| Ruling | Date | Content |
|---|---|---|
| **R97 — VENUE CLASS (ARENA / MAP) IS A QUEUE DIMENSION** | **2026-08-07** | **ARENA and MAP are two styles of play, and both are player-chosen at queue time (§2.2, §4.2).** A player who queued for one and got the other did not get a bigger version of their match — they got a different game, which is precisely §4.3's admission test: a dimension earns its place when the pools it creates **cannot merge**. Currency passed that test under R76; region failed it under §6; this passes it. **R18 IS INTACT** — the MAP remains a server outcome *within* the chosen class, so this is still a stake lobby and not a map browser: the player says “an Arena fight”, the server still says which Arena. **REJECTED: “whoever stakes chooses”.** In a staked match BOTH parties stake and a queue has no creator, so it would have meant *whoever queued first* — handing that player the style they are stronger at while the opponent must accept it. That is a bought edge in a wagering game, and the symmetric form (both choose, only matching choices meet) costs nothing more because it is exactly what `bracket` already is. **COST, RECORDED HONESTLY:** the concrete cell count doubles, each pool is thinner, and the lobby gains a FOURTH control — see `ssot/ui-frontend.md` §3.2. |
| **R85 — THREE TIERS: LEAGUE PLAY · WATTS PLAY · VOLTS PLAY** | **2026-08-06** | **AMENDS R77's universal buy-in.** A third, **unstaked** tier sits beside the two staked ones: LEAGUE PLAY has **no entry cost, no rating, and permits bots**; WATTS PLAY and VOLTS PLAY are staked, rated and bot-free. **Why a free tier earns its place after R77 rejected one:** R77 was decided when the only axis was currency, and a zero-cost *currency* tier was incoherent. As the home for HAYWIRE (R86) this is not a third denomination — it is a different *product*. **The population argument is decisive:** `ssot/ai-bots.md` §6.3 bars bots wherever stake **or** rating is present, so before R85 **no queue in the game could be filled at all**. LEAGUE PLAY is the only tier that can always start, which makes it the on-ramp every other tier depends on. |
| **R86 — STAKED PLAY IS PRO MOD ONLY; HAYWIRE IS LEAGUE PLAY'S** | **2026-08-06** | Both staked tiers offer **PRO MOD alone**; HAYWIRE runs in LEAGUE PLAY alongside PRO MOD. **You wager on the precise model, not the chaotic one** — PRO MOD is gore-free by construction and carries the enhanced movement kit, which is the competitive surface; HAYWIRE's dismemberment layer is spectacle and progression. **League stops being a free ×2 multiplier** and becomes tier-scoped: four (tier × league) pairs, not six. **LEAGUE PLAY carries BOTH models deliberately** — a PRO MOD player needs somewhere to warm up, learn a venue and grind progression without risking a balance, and excluding them would force competitive players into HAYWIRE to progress, which is backwards. HAYWIRE has one home; PRO MOD has three. |
| **R76 — STAKE CURRENCY IS A QUEUE DIMENSION: WATTS PLAY AND VOLTS PLAY** | **2026-08-06** | The two staked tiers are **separate pools**, and the queue formula becomes `currencies × rulesets × brackets × leagues` (§4.2). **The reason is settlement, not preference:** two participants staking different currencies cannot settle one pot, so no band is wide enough to merge them. §4.2 already anticipated this exact case — a flag *"rides the queue… where they cannot [share a pool], it becomes another multiplier and should be recognised as such rather than added quietly"* — and **this ruling is that recognition, made loudly.** §4.3's adder list is amended to admit currency as the fourth permitted adder, and §5 now separates the stake **AMOUNT** (a ticket parameter, unchanged) from the stake **CURRENCY** (a queue dimension). **Cost accepted:** nominal queue count doubles to ~64, concretely 28 after R63. |
| **R77 — THE STAKED TIERS ARE RATED** ⚠ *amended by R85* | **2026-08-06**, **amended 2026-08-06** | ~~Every match has a buy-in~~ — **SUPERSEDED BY R85**, which adds an unstaked LEAGUE PLAY tier. What stands: **both STAKED tiers are rated.** Both WATTS PLAY and VOLTS PLAY are staked and both are rated, so the two OpenSkill ladders (R64/R65) stay **per-ruleset, not per-currency** — currency does not multiply the ladder count. **`ranked` and `staked` REMAIN INDEPENDENT BOOLEANS** on the result (§10.1, `ssot/league-play.md` §12.1); this ruling deliberately does **not** collapse them into a mode enum, because the R14 stake firewall's premise — *a staked win and an unstaked win of the same result move the ladder identically* — depends on both being expressible. **⚠ CONSEQUENCE, STATED PLAINLY: no bots in either tier.** `ssot/ai-bots.md` §6.3 bars them on the **rating** clause alone, and its test is result-scoped — *"does this match's outcome move a balance or a rating?"* Making a tier cheaper does not make it fillable. |
| **R59 — THE CLIENT NEVER SNAPS A STAKE** | **2026-08-06** | Free numeric entry stays (R20); the entered integer travels on the ticket **unrounded** and the **server** resolves it to a band (§5.3). A client-side snap would have the client choosing which pool the player enters — **the same class of claim as asserting a balance, and forbidden by the same law (N11)** — and it buys nothing, because the server must re-resolve regardless. The band readout (§5.2) is what keeps this invisible: the player sees the outcome before committing. **Closes nothing that was open; hardens what §5.3 already implied.** |
| **R60 — BAND WIDENING IS STEPPED AND ASYMMETRIC** | **2026-08-06** | One band step every **15s**, capped at **2 steps DOWN / 1 step UP**, hard stop at **60s** then offer the fold (§11.2). **Asymmetric because §11.2 identified the reason itself:** being matched *down* costs a player nothing they did not offer, while being matched *up* **spends money they did not intend to stake**. In an unstaked game this is a preference; here it is exposure. **Stepped rather than continuous** because §5.2 requires the band be *disclosed*, and a step is legible in a UI where a continuously drifting band is not. **Closes §11.2.** |
| **R61 — COLD BANDS FOLD AUTOMATICALLY, WITH AN OPERATOR OVERRIDE** | **2026-08-06** | A band folds into its neighbour when it holds fewer than **2× field size over a rolling 5 minutes**; an operator may pin or force a fold at any time (§11.3). Automation is in the spirit of §7, which already rules folding a **config-level lever needing no code change and no client update** — so a band going cold at 3am should not wait for a human. **The override answers §11.3's own objection** (manual folding never surprises a player mid-session) and is what a launch window needs, when population is thin for reasons the threshold cannot know. **Closes §11.3.** |
| **R62 — PARTIES ARE NOT SEGREGATED; THE RATING COMPENSATES** | **2026-08-06** | Parties and solo players share a pool. Party seats stay capped at one team's worth (§8.3), and the rating consumed **accounts for coordination** (§11.4). Segregation is what a dense ladder can afford and we cannot — it halves an already thin pool, and §7 would then have to show two sets of bands as empty instead of one. **⚠ THE INTERACTION §11.4 PREDICTED IS NOW LOAD-BEARING:** the coordination adjustment makes this ruling a **dependency of R65's rating model**, not an independent choice — a rating that ignores party status makes this ruling a lie. **Closes §11.4.** |
| **R63 — PUBLISH ONLY BRACKETS THAT HAVE A MAP** | **2026-08-06** | MATCH PLAY publishes **1v1 · 3v3 · 5v5 · 8v8**. 2v2 and 4v4 stay **unoffered** until the R31 purpose-sized maps exist. §4 explicitly permits this — *an unoffered cell is a pool that never had to be filled* — and the alternative is worse than an absence: running a bracket on a footprint built for another size is the defect already recorded against INFINEON at 8v8. **This is a publishing rule, not a ruleset rule:** `match-modes.md` §9.1 keeps the size range open, and this ruling decides only what the front end offers today. |
| **R7 — THE TWO RULESETS SPLIT THE QUEUE** | **2026-08-05**, **renamed 2026-08-06** | Ruleset is the **top-level choice**, above size and above stake — presented as **two tabs**, not one doubled flat list. Settled on three grounds: (1) **UX** — they are different products, not variants; a player who wants one and is dropped into the other quits and **eats a leaver penalty on a wagered match**, the worst failure available in this system; (2) **observed practice** — wager and competitive platforms separate by **format**, never merging elimination and respawn formats; (3) **fragmentation does not apply** — merging doubles the entries but not the pool, because those players were never interchangeable, so a merged queue's population is **nominal, not effective**. A genuinely cold band folds into a neighbour via the transparency mechanism (§7), which is the correct instrument for that problem. Carried as **§4.1**, with the arithmetic in **§4.2** and per-ruleset counts in **§7**. **This is settled; a reader should not reopen it.** **RENAMED BY R41** (`ssot/match-modes.md`): the pair is MATCH PLAY and BATTLE ROYALE. **The split reasoning is unchanged and if anything stronger** — the two now differ in end condition, respawn policy AND position count, where the old pair differed in the first two only. |

---

## 13. RELATED

- [`Docs/DOCTRINE.md`](../DOCTRINE.md) — laws cited here: **N1** server authority · **N11** client never decides
  balances/ownership · **N12** transactional transfer with escrow and rollback · **G5** inert data assets ·
  **G7** backend residency · **P8** backend proven standalone before integration.
- `ssot/match-modes.md` — the rulesets a queue offers; solo/FFA as the configuration that makes BATTLE ROYALE resolve
  to last player standing.
- `ssot/map-build-system.md` — what a play-space is, the district model, and the footprint ladder that decides
  which brackets a play-space can serve.
- `ssot/league-play.md` — the rating this system consumes and the standings it feeds.
- `ssot/economy-store.md` — currencies, stake tiers, rake, payout curves, and the wallet the escrow acts on.
