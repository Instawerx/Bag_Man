# SSOT — LEAGUE PLAY (Tier 2)

**What this is:** what the progression, rating and standings systems **are** and **why**. It changes when the
system is redesigned.
**What this is not:** a status board. **This document contains no status claims** — nothing here says what is
built, proven, done or owed, and no commit hash appears as evidence of progress. Those belong to Tier 3
(`LIVE_TRACKER`).

> **Why the separation matters here in particular.** Progression systems are described naturally as *"players
> are currently at…"*, which is state, not design. A league document that records standings has recorded
> something that changes every match.

**Doctrine is cited, never restated.** Laws live in [`Docs/DOCTRINE.md`](../DOCTRINE.md) and are referenced by id
(**N1**, **N11**, **NM5**, **P8**).

---

## 1. SCOPE

This SSOT governs: the separation of career volume, skill rating and achievements into distinct axes · the
career progression ladder · how rank input differs by ruleset · the stake firewall · threshold rewards and how
they are granted · regional standings · season structure · advancement and decay · anti-abuse as a structural
concern · and the interfaces league owes and consumes.

It does not govern: how players are matched (→ `ssot/matchmaking.md`), currency, prices, or the grant mechanism
itself (→ `ssot/economy-store.md`), what a ruleset is (→ `ssot/match-modes.md`), or the state of any
implementation (→ Tier 3).

---

## 2. THREE AXES, DELIBERATELY SEPARATE

This is the load-bearing structure of the whole system. **Three things players experience as "progress" are
measured differently, mean different things, and are consumed by different systems.**

| Axis | Measures | Shape | Consumed by |
|---|---|---|---|
| **CAREER VOLUME** (§3) | Attendance and accumulation — how much you have played | A never-ending ladder of named thresholds | Rewards, prestige display |
| **SKILL RATING** (§4) | Strength — how likely you are to beat a given opponent | A rating that moves both directions | **Matchmaking** |
| **ACHIEVEMENTS** (§10) | Specific accomplishments | A checklist | Rewards, gates |

### 2.1 The critical distinction: volume is not rating

> **CUMULATIVE-VOLUME PROGRESSION AND SKILL RATING ARE DIFFERENT AXES AND MUST NEVER BE CONFLATED.**
> **Volume rewards attendance. Rating measures strength. Matchmaking consumes rating — never volume.**

**Why this is a hard rule rather than a preference:**

- **Conflating them means a player grinds their way into matches they cannot win.** If time played feeds the
  number that sorts opponents, a dedicated but average player is steadily promoted into lobbies of stronger
  opponents purely for showing up. Their reward for loyalty is a worsening experience, which is the exact
  opposite of what a loyalty system is for.
- **It is also a staking-integrity problem.** Where matches carry stakes, the sorting number decides who a
  player risks currency against. A number inflated by hours rather than strength systematically places
  persistent players against opponents who beat them — and they pay for the privilege. **A progression system
  that quietly worsens a player's expected outcome is not a progression system; it is a tax on attendance.**
- **The two must move independently in both directions.** A player can be high-volume and mid-rating (plays a
  lot, plays evenly), or low-volume and high-rating (new but strong). Both are ordinary, and any model that
  cannot express both has collapsed the axes.

**The practical test:** if a change would cause *playing more* to change *who you are matched against*, absent
any change in results, it has violated this rule.

---

## 3. CAREER PROGRESSION — VOLUME-DRIVEN, IN THE POKER SHAPE

**Progression is driven by time played** — sustained participation, not peak skill alone. The reference shape
is the poker-circuit career: a player accrues a permanent, publicly legible record of *how far they have
come*, and it never resets.

### 3.1 The threshold ladder

Named thresholds at cumulative eliminations: **100 · 500 · 1,000 · 5,000 · 10,000 — and continuing** by the
same alternating pattern (a ×5 step, then a ×2 step, indefinitely).

**The design property to preserve:**

> **A ladder that never ends, where each tier is visibly further than the last.**

- **It never ends.** There is no final tier to "complete", and therefore no point at which a dedicated player
  stops having something ahead of them. A capped ladder converts the game's most committed players into
  finished players.
- **Each tier is visibly further.** The gaps widen geometrically, so a tier is always a genuine milestone
  rather than a routine tick. Early tiers arrive quickly — the first is reachable in a normal run of play,
  which is what makes the ladder legible before it becomes aspirational. Later tiers are meant to be rare, and
  their rarity **is** the signal.
- **The distance is honest.** Because each step is a multiple of the last, a player can see that the next tier
  is several times the effort of the one they just cleared. This is a feature: it sets an accurate expectation
  instead of the flat-looking ladder that quietly slows down and reads as a nerf.

### 3.2 Why volume rather than skill for this axis

Skill ladders are, by construction, zero-sum: for a player to climb, others must fall, and roughly half the
population is below average at any moment. That is correct for *matchmaking*, and demoralising as the sole
measure of *progress*.

A career-volume ladder is **non-zero-sum** — a player's advancement is never taken by another player's success,
and it cannot be lost to a bad run. It rewards the behaviour a live game most needs (returning) with the thing
players most want (visible, permanent advancement), without lying about strength. It is precisely *because* it
does not claim to measure skill that it can be generous.

### 3.3 What counts

Cumulative eliminations are the named metric. **The counter is server-authoritative** (**N1**) and derived from
the same match results everything else reads (§11) — never a client-reported total (**N11**).

---

## 4. RANK INPUT DIFFERS BY RULESET

The two rulesets produce **structurally different results** (`ssot/match-modes.md` §2):

| Ruleset | Produces | Nature |
|---|---|---|
| **BATTLE ROYALE** | **Placement** (1..N) | An ordering over **N** participants |
| **MATCH PLAY** | **Series outcome** (1..2) | An ordering over **2** sides |

**The league system must consume both without either distorting the other.**

> **⚠ R41 CHANGED THE SHAPE OF THIS PROBLEM, AND IT GOT EASIER.** The former pair produced placement and a
> **kill ratio** — one ordinal and bounded by lobby size, the other continuous and unbounded — so combining
> them naively was a **category error, not a scaling problem**. That is no longer the case: **both rulesets now
> produce a finishing position**, differing only in how many positions there are. It is now genuinely a scaling
> problem, and a tractable one.
>
> **What survives the change:** a 2-position result carries far less information per match than an N-position
> one. Beating one opponent is one bit; placing 3rd of 36 is not. So the two still cannot be pooled without
> weighting, and §4.1's fork stands — but it is a fork about *information content*, not about incommensurable
> units.

### 4.1 The two options, and their trade-offs

**Option A — two ladders, one per ruleset.**
*For:* each rating measures exactly one thing and is directly interpretable; no normalisation to argue about;
a player's BATTLE ROYALE standing cannot be inflated by MATCH PLAY performance or vice versa; it matches R7
that the rulesets are different products with separate queues.
*Against:* it splits the rating population the same way splitting queues splits the match population — each
rating converges more slowly, and a player who plays both has two partial identities instead of one. It also
doubles the standings surface and forces the front end to answer "which rank am I?" with "which one do you
mean?"

**Option B — one ladder, with per-ruleset normalisation.**
*For:* one number, one identity, one leaderboard; the whole population feeds a single rating, so it converges
faster and is more meaningful at the margins.
*Against:* it requires mapping an N-position result and a 2-position result onto a common scale, and **that
mapping is a design liability**. If the normalisation is even slightly off, one ruleset becomes the efficient way to rate up, and
players follow the incentive rather than the format they prefer — which corrupts both the rating and the
population split R7 was protecting. The mapping also needs re-derivation whenever either ruleset's balance
changes.

### 4.2 This is not decidable from design alone

Choosing between them depended on **how many players play both rulesets** and **how quickly a rating converges
at the real population size** — neither knowable in advance. **⚠ CLOSED by R64: two ladders.** The deadlock did
not break on data, it broke on **R41** — while the pair produced placement versus a kill *ratio* the options
were incommensurable, and once both rulesets emitted a finishing position the choice became a tractable one
about *depth*. Two was taken because it **cannot** produce the rating-efficient ruleset §4.1 names, where one
ladder always can. **R76 does not disturb this: the ladders are per-RULESET, not per-currency** — WATTS PLAY
and VOLTS PLAY share a ruleset's ladder (R82).

What *is* fixed regardless of the choice:
- **The result interface carries the ruleset** (`ssot/matchmaking.md` §10.1), so either model is servable
  without changing what matches emit.
- **Whatever is chosen must not create a rating-efficient ruleset.** If one format rates up faster per unit of
  time, the design has failed, whichever option produced it.

---

## 5. THE STAKE FIREWALL — A LEAGUE-SIDE INVARIANT

> **STAKE SIZE HAS NO INPUT TO RATING. A player betting large is not thereby better.**

`ssot/matchmaking.md` §10.1 states this as an interface property — the result shape emits stake and rank as
independent facts. **It is restated here as a league invariant because this is where the pressure to violate it
will originate.**

The pressure is real and will sound reasonable: high-stakes matches are more competitive, so shouldn't they
count for more? Weighting rating by stake would make the ladder reflect "real" competition. Both are plausible
and both are wrong:

- **It makes rank buyable.** If a larger stake moves rating further, then currency purchases rating progress —
  directly through play, without any explicit "buy a rank" product. That is the pay-to-win boundary crossed by
  arithmetic rather than by a store listing.
- **It corrupts matchmaking.** Rating exists to sort players by strength. A rating containing a spending
  signal sorts by strength-plus-wealth, and the matches it produces are worse for everyone in them.
- **It punishes cautious players for being cautious.** A skilled player who stakes modestly would rank below a
  weaker player who stakes heavily — the ladder would be measuring appetite for risk and calling it skill.

**The rule in full:** a staked win and an unstaked win of the same result against the same opposition produce
**identical** rating movement. Stake moves the pot, never the ladder.

**Where staked performance may legitimately be surfaced:** a **separate** high-stakes standings board that
tracks staked results explicitly and touches skill rating not at all. Separation is what makes it safe — the
moment it feeds rating, it is the firewall violation wearing a different label.

**Reinforced from the economy side by R38** (`ssot/economy-store.md` §5.4): a stake is a **closed loop** that
settles between participants, kept structurally separate from **earning**, which is what a match pays. **No
conflict — the two rules isolate the wager from different things.** This section forbids stake from reaching
*rating*; R38 forbids it from being conflated with *earning*. A design that merged either boundary would
breach both, which is worth knowing: the firewall has two walls, and they were derived independently.

---

## 6. THRESHOLD REWARDS

Crossing a career threshold (§3) grants rewards: **badges · sticker packs · emblems · weapons.**

### 6.1 Rewards are grants through the economy, never a parallel inventory

> **Every threshold reward is delivered through the economy's reward-grant interface**
> (`ssot/economy-store.md` §14.2), which writes through **the single persistence seam** (`economy-store.md` §9).
> **League never writes the ledger.**

This is not a layering preference — it is the same requirement the economy states for itself, applied here
because progression is exactly the system most tempted to bypass it:

- A reward is an entitlement. An entitlement granted outside the one write path is **invisible to the audit
  trail**, does not survive whatever durability the seam provides, and cannot be reconciled when a player asks
  why they do or do not have something.
- Progression grants arrive in **bulk and on retry** — a season rollover computes many grants at once, and any
  failure is retried. This is precisely why the interface carries an **idempotency key**: a retried grant that
  awards twice is an inflation bug that surfaces only when someone audits totals, long after the cause.
- A second grant path means a second implementation of clamping, ordering and atomicity, and the second one is
  the one that is subtly wrong.

**Division of authority, stated plainly:** **league decides *who has earned what*; economy performs the
grant.** Neither reaches across. League never mutates a balance or an owned set; economy never evaluates
eligibility.

### 6.2 Earned rewards are earned-only

Threshold and rank rewards are **not purchasable**. Their entire value is as a signal that something was done,
and a signal that can be bought is not a signal. This follows the economy's earn-and-gate stance and is the
same reasoning as the stake firewall (§5): the moment currency can produce the marker, the marker stops
meaning what it displays.

Reward ids follow the standard address discipline and, once shipped, are never renamed (**NM5**).

---

## 7. REGIONAL BRACKETS ARE LEADERBOARDS, NOT QUEUES

> **Players compete regionally while queueing globally.**

Region is a **player-profile attribute** owned by matchmaking (`ssot/matchmaking.md` §6). League **reads** it
for two purposes only: **regional standings** and **prize eligibility** (`ssot/economy-store.md` §13).

### 7.1 Why this split is the correct one

**Regional *standings* cost nothing; regional *queues* cost everything.**

- A regional board is a **filtered view of one global result set**. It creates no new pool, adds no queue, and
  cannot fragment the population — the same matches feed the global board and every regional one
  simultaneously.
- A regional queue would **partition the players themselves**, and every partition multiplies with the ones
  already there (currency × ruleset × bracket × league — R76). Queue time is the failure mode matchmaking is designed to avoid,
  and region is the axis most tempting to split on because it *sounds* like it improves the experience.
- **The player-facing benefit of regional competition is almost entirely the standings.** Players want to
  place among peers they can compare themselves to. That is a presentation property, and it is fully served by
  filtering — with the bonus that a player appears on their regional board *and* the global one from the same
  results.

**The asymmetry to hold on to:** you can always add a regional view to a global pool; you cannot recover a
global pool once you have split it into regional ones and the population has thinned.

### 7.2 What league must not do with region

- **Never treat region as a matchmaking input.** That decision belongs to matchmaking and is settled there.
- **Never let a regional board become a separate competition with its own rules**, or it becomes a queue by
  implication — different rules require different matches.
- **Region changes take effect at a season boundary** (§8), never immediately; the reasoning is matchmaking's
  (§6.2 there) and league is the system that would otherwise be gamed by it.

---

## 8. SEASON STRUCTURE

Seasons are bounded periods with a defined start and end. **Boundaries exist for three structural reasons**,
none of which is "seasons are conventional":

1. **They bound rating drift.** Any rating accumulates error over time — from population change, balance
   changes, and players whose real strength has moved. A boundary is the sanctioned moment to recalibrate
   (§9.2) without it reading as an arbitrary correction.
2. **They give the prize series a natural cadence.** The monthly/periodic collection mechanic
   (`ssot/economy-store.md` §13) needs a defined window with a start, an end and a settlement. A season is that
   window, and aligning them means one calendar rather than two competing ones.
3. **They are the point at which a region change takes effect** (§7.2, `ssot/matchmaking.md` §6.2). Deferring
   region changes to a boundary is what removes the incentive to leaderboard-shop late in a period — and that
   defence only works if boundaries exist and are predictable.

**One calendar.** Season, prize series and any pass cadence share boundaries. Two overlapping calendars produce
a player experience of permanent partial resets and an operational burden of reconciling them.

**Career volume does not reset** (§3). Only rating recalibrates and only seasonal standings clear. **A season
boundary must never take away something presented as permanent** — the expectation-whiplash failure is worse
than whatever the reset was meant to achieve.

---

## 9. ADVANCEMENT AND DECAY

### 9.1 Career volume: advancement only

Career thresholds are **cumulative and permanent**. They are never lost, never decay, and never reset. This
follows from what the axis measures (§3.2) — you cannot un-play a match, so a record of matches played cannot
honestly go backwards.

### 9.2 Skill rating: moves both ways, by construction

Rating must fall as well as rise, or it is not a measure of strength. This is not a penalty; it is the
definition. A rating that only increases converges to everyone being top-rated and stops sorting anyone.

**Seasonal recalibration is a soft compression toward the middle, not a wipe.** A full reset discards real
information and forces every player through re-derivation from nothing; a soft compression keeps most of what
is known while restoring mobility and correcting drift.

### 9.3 Inactivity decay: the consequence either way

**If standing decays with inactivity:** the ladder more accurately reflects *current* strength, and the top is
harder to squat. The cost is that it converts absence into loss, which reliably produces anxiety-driven play
and churn among exactly the players who were most invested — a player who returns to find their standing eroded
often does not return again.

**If standing does not decay:** returning players are met with what they left, which is the friendlier and
more retentive behaviour. The cost is that inactive players occupy high positions and the top of the ladder
becomes partly historical rather than current.

**⚠ SETTLED by R67, and it went further than the lean above.** Decay begins after **21 days** of inactivity and
is **floored at the bottom of the player's tier** — applied broadly rather than confined to top positions,
because the floor makes broad application safe. The lean's instinct was right about the risk and wrong about
the remedy: what makes decay punitive is not *who* it touches but *how far it drops them*, and a floor answers
that directly. Overwatch removed decay outright after exactly that backlash.

---

## 10. ACHIEVEMENTS ARE NOT PROGRESSION TIERS

Both grant rewards; they are different instruments and should not be merged.

| | **Progression ladder** (§3) | **Achievements** (§10) |
|---|---|---|
| Shape | **A ladder** — one axis, ordered, never-ending | **A checklist** — many independent items |
| Answers | *"How far have I come?"* | *"What have I done?"* |
| Direction | Always forward, one dimension | Breadth across many dimensions |
| Failure if merged | Becomes a to-do list with no sense of distance | Becomes a single grind with a hidden order |

**What each is for:**

- **The ladder gives direction.** There is always exactly one next thing, and its distance is legible. It is
  the answer to *"what am I working toward"* for a player who does not want to choose.
- **Achievements give breadth and long tail.** They reward playing *differently* rather than *more* — the
  variety principle. They are also the natural home for goals that are not volume-shaped at all (a streak, a
  single outstanding match, a specific accomplishment), and for **gates**, where completing something is a
  precondition for access to something else.
- **A healthy achievement set spans a completion curve deliberately**, from near-universal onboarding markers
  to genuinely rare prestige items, so that every segment of the population has a live goal. A set clustered at
  one difficulty serves one segment and is invisible to the rest.

**Rewards for both flow through the same grant interface** (§6.1). The distinction is in what they measure and
how they motivate, never in how they are delivered.

---

## 11. ANTI-ABUSE IS A FIRST-CLASS DESIGN CONCERN

Progression that grants value creates an incentive to obtain it fraudulently. **Structural mitigations are
design; detection heuristics are not, and are deliberately not specified here** — a documented detector is a
documented evasion.

### 11.1 The four vectors

| Vector | What it is | Why it targets this system |
|---|---|---|
| **Volume farming** | Manufacturing the counted event without genuine play | **The primary vector**, because progression is volume-driven (§3) |
| **Win-trading** | Arranging outcomes between colluding players | Rating and seasonal standings key on outcomes |
| **Boosting** | A strong player playing an account to lift it | Rating is transferable-by-proxy if identity is not enforced |
| **Smurfing** | A strong player on a new account | Rating starts uncalibrated, so the account farms weaker opposition |

### 11.2 Volume-driven progression makes farming the primary vector — the structural answers

Because the ladder counts a cumulative event, **the cheapest attack is to manufacture that event**. The
mitigations are structural properties of the design, not detectors bolted on afterwards:

- **The counted event must be server-authoritative and match-scoped** (**N1**). It is derived from the same
  authoritative result the rest of the system reads (§12), never self-reported, never assembled client-side
  (**N11**).
- **Progression accrues from HUMAN OPPONENTS ONLY** (R87). An elimination of a bot never feeds the career
  ladder, in any tier, ever.

> **⚠ REWRITTEN BY R87, AND THE OLD WORDING WAS IMPRECISE IN A WAY THAT MATTERED.** This clause used to read
> *"progression accrues only in real matches — offline, bot-filled, **or otherwise unrated** — do not feed the
> career ladder."* Under R85 that would have been a **catastrophe by accident**: LEAGUE PLAY is deliberately
> unrated, so the literal clause would have stopped progression in the one tier built to feed it.
>
> **The fix is to name the actual risk.** What the clause was ever guarding against is **farming against
> bots** — an opponent who cannot resist. It was never guarding against *the absence of a rating*, which is a
> matchmaking property with no bearing on whether a kill was earned. Keyed on the opponent instead of the
> queue, the rule is **more accurate AND more permissive**: a human elimination in unrated LEAGUE PLAY counts,
> a bot elimination anywhere does not, and the exploit surface closes tighter than the old wording managed.
> This removes the entire category of farming that does
  not require a second human.
- **Ratio-shaped rewards resist farming better than count-shaped ones.** A rate cannot be inflated by volume
  alone, because the denominator grows too. Where a reward can be expressed as a rate without distorting its
  meaning, it should be.
- **Rewards are non-transferable** (§6.2). Farming an account is far less attractive when the proceeds cannot
  be moved off it. This single property removes most of the *economic* motive for boosting and multi-accounting.
- **Volume is not rating** (§2.1). Because farmed volume cannot buy an easier lobby or a better rating, a
  successful farm yields cosmetics and prestige markers and nothing that affects competition — which caps the
  damage at reputational rather than structural.

### 11.3 Where enforcement lives

Detection, adjudication and penalties are an **integrity concern shared with the economy** (the currency being
debased is economy state) and matchmaking (the lobby being manipulated is a match). League's contribution is
**not to be exploitable by construction**: authoritative counting, no progression outside real matches, and
non-transferable rewards. Any enforcement action that reverses granted rewards is an **economy** operation and
goes through the seam like any other (§6.1).

---

## 12. INTERFACES

### 12.1 What league CONSUMES

**From matchmaking** (`ssot/matchmaking.md` §10.1) — the per-match result: **match id**, participants with team
assignment, **the ruleset the match ran** (because rank input differs by it, §4), the result in that ruleset's
terms, the league it ran under, and whether it was ranked and whether it was staked **as independent facts**.
League reads the ruleset and the result. **It does not read stake for rating purposes** (§5).

**From matchmaking** (§6 there) — the player's **region attribute**, for standings and eligibility only (§7).

**From economy** — confirmation that a reward grant succeeded, so a threshold is not re-fired. League does not
inspect balances or owned sets beyond what a gate check requires.

### 12.2 What league OWES the front end — the standings shape

A standings entry carries: **the subject** (a player, or a team where team standings exist) · **the axis** it
belongs to (career tier, skill rating, or a specific board) · **the scope** (global or a region bracket) · **the
time arc** (the current season, or all-time career) · **the position and the value** · and **enough identity to
display** without a second lookup.

Three properties the shape must have:

- **A player must be able to see their own position without paging to it.** A board that can only be read from
  the top is useless to the 99% of players who are not near it.
- **Regional and global positions come from the same query shape** (§7.1), differing only by a filter — if they
  need different shapes, they have become different competitions.
- **The career tier is displayable without the raw counter.** The tier is the legible face; the exact number is
  detail, and a UI that must show the number to be meaningful has failed to make the ladder legible.

### 12.3 What league OWES economy

The **reward grant** (§6.1): recipient, entitlement or currency, and an **idempotency key**. Nothing else
crosses.

**Durable standings depend on durable persistence.** Rating, tiers, standings and season state are **new state
shapes** on the same seam (`ssot/economy-store.md` §9), and **P8** applies to the backend that holds them: it is
proven standalone before anything integrates against it. A ladder that cannot survive a restart is not a
ladder.

---

## 13. OPEN DESIGN QUESTIONS

1. ~~**The rating algorithm, and whether it is per-ruleset.**~~ — **CLOSED by R64 and R65.**
2. ~~**§4's one-ladder-versus-two.**~~ — **CLOSED by R64: two ladders.** §4.2 called this *"not decidable from
   design alone"* on the grounds that it depends on cross-ruleset play rates and convergence at real population.
   **That objection was answered by R41, not by data:** the former pair produced placement versus a kill *ratio*
   — incommensurable units — so the one-ladder option required a normalisation nobody could validate. Both
   rulesets now produce a finishing position, so the choice is between two clean ladders and one ladder needing
   a defensible depth-weighting. **Two was taken because it cannot produce the failure §4.1 names** — a
   rating-efficient ruleset — while one always can.
3. ~~**Decay policy.**~~ — **CLOSED by R67.**
4. ~~**Season length.**~~ — **CLOSED by R67.** Note this section's own constraint held: it is *the same number
   as the prize cadence* (§8), so R67's ~10 weeks sets both.
5. ~~**Crossing two thresholds in one match.**~~ — **CLOSED: BOTH grants fire, presented SEQUENTIALLY.** The
   grant path is already idempotent (§6.1), so correctness was never at risk — this was pacing, and the item
   named the real failure itself: *"a player who crosses two tiers and sees one notification will reasonably
   believe they were shortchanged."* **Never merge two earned things into one notification.** They present one
   after the other with a short stagger, so each reads as its own event. Awarding only the higher is the one
   option that is actually wrong: it silently deletes something the player earned.
6. ~~**Whether party play is rated differently from solo.**~~ — **CLOSED by R65, jointly with
   `matchmaking.md` R62**, exactly as this item required (*"should be decided with it rather than
   separately"*). The rating **does** account for coordination; R62's no-segregation ruling depends on it.

---

## 14. RULINGS OF RECORD

| Ruling | Date | Content |
|---|---|---|
| **R87 — LEAGUE PLAY IS UNRATED AND BOT-FILLABLE; PROGRESSION KEYS ON HUMAN OPPONENTS** | **2026-08-06** | LEAGUE PLAY (R85) carries **no stake and no rating**, so `ssot/ai-bots.md` §6.3 permits bot-fill there — it is the only tier that can always start, and before R85 **no queue in the game could be filled at all**. **§11.2 is rewritten as a consequence, and the old wording was a latent trap:** it excluded *"offline, bot-filled, or otherwise unrated"* contexts from the career ladder, which read literally would have **stopped progression in the very tier designed to feed it**. The rule now keys on the **opponent, not the queue** — a human elimination counts wherever it happens, a bot elimination counts nowhere. **This is more accurate and more permissive at once:** the risk was always farming against something that cannot resist, never the absence of a rating, which is a matchmaking property that says nothing about whether a kill was earned. |
| **R82 — PROGRESSION RIDES UNDERNEATH BOTH STAKED TIERS** | **2026-08-06** | Career volume, loot and achievements accrue in **WATTS PLAY and VOLTS PLAY alike**. They are **not one side of the split** — the currency tiers differ in what a match *costs and pays*, never in what it *counts toward*. **The two OpenSkill ladders stay per-RULESET, not per-currency** (R64/R65): currency does not multiply the ladder count, so a player has two ratings, not four. **§11.2 needed no rewrite because R77 made both tiers rated** — its exclusion of *"offline, bot-filled, or otherwise unrated"* contexts does not catch WATTS PLAY, since a cheaper buy-in is not an unrated context. **The point of the whole split:** a Watts player earns the same career, the same achievements and the same rank as a Volts player. Only the denomination of the wager differs. |
| **R64 — TWO LADDERS, ONE PER RULESET** | **2026-08-06** | A player holds a **separate rating for MATCH PLAY and for BATTLE ROYALE** (§4.1 Option A). **R7 already split the queues on the grounds the two are different products; the rating follows the same logic.** The decisive property is asymmetric: two ladders **cannot** produce the failure §4.1 names — a rating-efficient ruleset that players chase instead of the format they prefer — whereas one ladder with normalisation always can, and the mapping would need re-derivation on every balance change. **Accepted cost:** a player who plays both holds two identities and each ladder converges on half the data. **Closes §13.1 and §13.2.** |
| **R65 — RATING IS OPENSKILL (WENG-LIN), ONE ENGINE IN TWO CONFIGURATIONS** | **2026-08-06** | **Plackett-Luce** for BATTLE ROYALE — literally the model for *"rank N participants by finishing position"*, which is exactly what that ruleset emits — and the **two-team** configuration for MATCH PLAY. One implementation, no second system, and **no patent exposure**: TrueSkill proved the Bayesian approach at AAA scale but is Microsoft-patented; OpenSkill is the same mathematics under an open licence. Elo cannot express N-way placement at all and Glicko-2 is pairwise, so both would need a bolt-on for BR — which is where rating bugs live. **⚠ THE RATING MUST ACCOUNT FOR PARTY COORDINATION** (`matchmaking.md` **R62**): R62's no-segregation ruling *depends* on this, so a rating that ignores party status makes R62 a lie. **Closes §13.1 and §13.6.** |
| **R66 — ROUND DIFFERENTIAL IS RECORDED, NOT CONSUMED** | **2026-08-06** | MATCH PLAY rank input is the **series outcome only**; a 7–0 and a 7–6 move rating identically. The differential is **stored on the result** so this can be switched on later as a config change rather than a re-derivation. **The reason to hold is incentive, not arithmetic:** differential rewards running up the score, and at the margin of a *staked* match that is a reason to keep shooting an already-beaten opponent. **Precedent exists for the other side** — CS2 Premier does consume round differential — so this is a judgement call under our own conditions, not a settled principle. Revisit if convergence at real population proves too slow. **Closes `match-modes.md` §9.2.** |
| **R67 — SEASONS ARE ~10 WEEKS; DECAY AFTER 21 DAYS, FLOORED** | **2026-08-06** | Season length **~10 weeks**, which §13.4 requires also be the prize cadence (§8). Inactivity decay begins after **21 days** and is **floored at the bottom of the player's tier**. **The floor is the load-bearing part:** decay without one dumps a returning player below their real skill, which is the classic failure — Overwatch removed decay outright after exactly that backlash. Decay exists to stop abandoned high ratings distorting matchmaking for players still there; it does not exist to punish absence, and the floor is what keeps those two apart. **Closes §13.3 and §13.4.** |
| **R10 — Progression is volume-driven, and volume is NOT rating** | **2026-08-05** | Career progression is time-played driven in the poker-career shape, with named thresholds at cumulative eliminations **100 · 500 · 1,000 · 5,000 · 10,000 and continuing** — a ladder that never ends, each tier visibly further than the last (§3). **Cumulative volume and skill rating are separate axes and must never be conflated: volume rewards attendance, rating measures strength, and matchmaking consumes rating only** (§2.1). Conflation would grind players into matches they cannot win — a UX failure and, under stakes, an integrity failure. |
| **R11 — Threshold rewards are economy grants** | **2026-08-05** | Badges, sticker packs, emblems and weapons awarded at thresholds are granted **through the economy's reward-grant interface and the single persistence seam** — never a parallel inventory path (§6.1). League decides who has earned what; economy performs the grant. |
| **R12 — Regional brackets are leaderboards, not queues** | **2026-08-05** | Players **compete regionally while queueing globally** (§7). Region is matchmaking's profile attribute, read here for standings and prize eligibility only. Regional standings are a filtered view of one global result set and cost nothing; regional queues would partition the population, and a split pool cannot be recovered once the population has thinned. |
| **R13 — Rank input differs by ruleset** | **2026-08-05**, **amended 2026-08-06** | **AMENDED BY R41** (`ssot/match-modes.md`): BATTLE ROYALE produces **placement over N positions**; MATCH PLAY produces **a series outcome over 2** (§4). **The original premise — placement versus kill ratio — no longer holds, and its consequence was the harder one:** two incommensurable quantities became two orderings of different depth, so the normalisation question is now a scaling problem rather than a category error. League consumes both without either distorting the other. ~~Whether this is two ladders or one with normalisation is not decidable from design alone~~ — **SUPERSEDED BY R64: two ladders, one per ruleset**, on OpenSkill per R65. The constraint this row set still governs and was the deciding argument: **whatever is chosen must not make one ruleset the rating-efficient one.** |
| **R14 — The stake firewall** | **2026-08-05** | **Stake size has no input to rating** (§5). A staked win and an unstaked win of the same result against the same opposition move the ladder identically. Restated as a league-side invariant because this is where the pressure to violate it originates: weighting rating by stake would make rank purchasable by arithmetic rather than by a store listing. Staked performance may be surfaced on a **separate** board that does not feed rating. |

---

## 15. RELATED

- [`Docs/DOCTRINE.md`](../DOCTRINE.md) — laws cited here: **N1** server authority · **N11** the client never
  decides state · **NM5** a shipped id is never renamed · **P8** a backend is proven standalone before
  integration.
- `ssot/matchmaking.md` — the result shape league consumes (§10.1 there), the region attribute (§6 there), and
  the rating it consumes back for sorting.
- `ssot/economy-store.md` — the reward-grant interface (§14.2 there), the single persistence seam (§9 there),
  and the prize series whose cadence shares this system's season boundaries (§13 there).
- `ssot/match-modes.md` — the two rulesets whose results are the rank inputs.
- `ssot/map-build-system.md` — the brackets a season's competition is played across.
