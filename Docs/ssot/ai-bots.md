# SSOT — AI + BOTS (Tier 2)

**What this is:** what the bot AI systems **are** and **why**. It changes when the system is redesigned.
**What this is not:** a status board. **This document contains no status claims** — nothing here says what is
built, proven, done or owed, no tuned parameter values appear, and no commit hash appears as evidence of
progress. Those belong to Tier 3 (`LIVE_TRACKER`).

> **Why this system has its own SSOT (ruling R24).** AI touches matchmaking (bot-fill), combat
> (bots fire the same abilities under **A9**) and character (they drive the same movement components) — and is
> owned by none of them. **A system with its own design laws needs its own home**; distributing it across three
> SSOTs would put its foundational law in a document about something else, which is how a law stops being read.

**Doctrine is cited, never restated.** Laws live in [`Docs/DOCTRINE.md`](../DOCTRINE.md) and are referenced by
id (**A3**, **A6**, **A9**, **C1**, **G2**, **N1**, **X3**, **X13**).

---

## 1. SCOPE

This SSOT governs: bot-fire parity · the aim model · difficulty as an axis · combat movement and its EQS
substrate · bot-fill and where bots are legitimate · the navigation a map must provide · and the interfaces AI
exchanges with the rest of the game.

It does not govern: what the abilities themselves do (→ `ssot/combat-arsenal.md`), how humans are matched
(→ `ssot/matchmaking.md`), the movement components bots drive (→ `ssot/character-system.md`), what a ruleset is
(→ `ssot/match-modes.md`), map construction (→ `ssot/map-build-system.md`), or the state of any implementation
(→ Tier 3).

---

## 2. BOT-FIRE PARITY — THE FOUNDATIONAL LAW

> **Bots run the SAME abilities as players. Only the input trigger differs.**

This is **A9**. It is the law every other section here is built on top of, and it is stated first because
every attractive shortcut in AI development violates it.

**The mechanism.** A weapon ability is reached two ways: a player presses an input bound through the ability
set, and a bot's behaviour tree sends a **GameplayEvent** carrying the same input tag. The ability accepts
both. **One ability, two doors.** Abilities are still granted only through ability sets (**A3**), so a bot
acquires its arsenal by the same route a player does.

### 2.1 Why parity is a law and not a preference

**A separate bot firing path means bots are balanced against different numbers than players.** That failure
is not loud — it is a slow divergence:

- **Every arsenal change silently forks.** Retune a weapon's damage, cadence, cooldown or spread and you have
  changed it for players only. The bot path still holds the old numbers, and nothing reports the discrepancy
  because both paths are individually valid. The arsenal now has two balance states and one of them is
  invisible.
- **Bot difficulty stops meaning anything.** If bots use different numbers, "this bot is hard" no longer
  describes an opponent a player could learn from — it describes a different game running next to theirs.
  Practising against bots stops transferring to playing against people, which is most of what bots are for.
- **The tag contract stops covering them.** `ssot/combat-arsenal.md` §2 establishes that match-phase gating,
  heat and cooldowns are properties of the *ability*. A parallel bot fire path inherits none of that, so a
  bot would fire during warmup, never overheat and never respect a cooldown — three separate defects, all
  from one shortcut.

### 2.2 What parity forbids, explicitly

| Forbidden | Why |
|---|---|
| **Bot-only damage values** | The same weapon must hurt the same amount regardless of who pulled the trigger. A bot that hits harder is not a harder opponent — see §4.2 |
| **Bot-only cooldowns** | Cooldowns are GEs granting per-weapon tags (**A6**, `combat-arsenal` §2.3). A bot-only cooldown is a second cooldown authority that will drift |
| **Parallel fire code** | Any second path that traces, applies damage or spawns a projectile. The apply path is one path or the balance is two balances |
| **Bot-only weapons or abilities** | A weapon only bots can hold is untested by the people who would find its problems |

**The permitted difference is the trigger, and the trigger only.** If a change would make a bot's *shot*
differ from a player's shot fired under the same conditions, it is out of bounds regardless of how it is
implemented.

### 2.3 The corollary: a new weapon must be bot-fireable on the day it ships

Because the trigger is a property of the *ability*, a weapon authored without the event trigger is not
"missing bot support" — **it is a weapon bots cannot use at all**, and the gap surfaces only in a
bot-populated match. `ssot/combat-arsenal.md` §3.3 carries this as item 5 of the weapon authoring contract.

---

## 3. THE AIM MODEL

**The problem it exists to solve:** stock AI aim is a perfect, instantaneous ray. The controller recomputes
control rotation to point exactly at the focal point every frame — no delay, no error, no smoothing — and the
weapon trace reads that rotation directly. Under bot-fire parity (§2) that produces **a hitscan turret that
never misses and never looks like it is trying.** Parity means we cannot fix this by weakening the bot's
weapon; the weapon is the player's weapon. **So the fix must be in the aiming, and only in the aiming.**

### 3.1 The seam: humans are untouched by construction

The model is an **AI-controller override**. A human player has no AI controller, so **there is no code path
here a player can reach** — the separation is structural rather than a runtime flag that could be set wrongly.
This matters: a flag-gated aim modifier is one mis-set boolean away from applying to a person.

### 3.2 The structural idea — the wobble goes into the TARGET, not the output

**The bot chases a slightly-wrong, slowly-drifting point, while a spring-damper closes on that point.**
It does not aim correctly and then have error added to its output.

Two properties fall out for free, and they are the reason this shape was chosen:

- **Motion is continuous.** Nothing snaps. That matters beyond looks: the arsenal emits an **angular-velocity
  anomaly** telemetry event (`ssot/combat-arsenal.md` §7.1) for aim that moves impossibly fast. An
  error-on-output model would trip it constantly and fill the anti-cheat signal with bot noise, destroying its
  value for detecting actual anomalies.
- **Micro-correction is emergent, not authored.** The bot is always slightly behind a point that is itself
  moving. That *is* what tracking looks like when a person does it — nobody has to hand-author a correction
  behaviour, and none can be spotted as a repeating pattern.

### 3.3 The axes, and what each governs

Six axes shape aim. **Tuned values are Tier 3; the shape is here.**

| Axis | Governs | Direction with competence |
|---|---|---|
| **Reaction** | Seconds between a target becoming the focus and tracking beginning — **perception** | Falls: a better bot reacts sooner |
| **Track rate** | Hard cap on how fast aim may slew — the **motor limit** | Rises |
| **Stiffness** | How urgently the spring pulls toward the target | Rises |
| **Damping** | Whether aim overshoots and corrects, or approaches asymptotically | **Held in the overshoot band — see §3.4** |
| **Steady error** | Amplitude of residual wobble | Falls — **but never to zero** |
| **Error frequency** | How fast that wobble wanders | Roughly flat — **this is anatomy, not skill** |

**Two of these deliberately do not improve with competence**, and that is the most important thing in the
table. Damping and error frequency describe *what kind of thing is aiming*, not *how good it is*. A better
human does not stop overshooting and does not acquire a different tremor frequency — they overshoot less far
and wobble less widely. Modelling skill as "trends toward perfect on every axis" produces a top-tier bot that
is unmistakably a machine.

### 3.4 Overshoot is required, not incidental

**Damping is held below the critical point at every competence level, so aim passes the target and corrects
back.**

An asymptotic tracker — error only ever shrinking, never crossing zero — is **the most robotic-looking option
available, even at a slow rate.** Slowness does not make it look human; the *absence of correction* is the
tell. A bot that never overshoots is a failed bot, and any test of the model must treat "never overshoots" as
a **failure**, not a pass (**X13**: a check that cannot fail proves nothing).

**The same logic applies to the residual wobble, which must never reach zero.** A bot holding a perfect,
motionless bead on a stationary target is the single clearest signal available that it is not a person. The
floor on wobble is therefore a hard clamp, not a tuning default.

### 3.5 Personality variance — what stops a squad reading as one entity

**Each bot carries a stable personality: one normalised offset per axis, rolled once from a seed derived from
that bot's own identity, and never re-rolled.**

**Why per-bot variance is load-bearing rather than flavour:** without it, every bot at a given competence
level *is the same bot wearing a different name*. Facing four of them is facing one opponent rendered four
times — they acquire together, track at the same rate, and correct identically. **The squad reads as a single
system**, which is precisely the impression bots exist to avoid.

**Why the roll is stable for the bot's lifetime:** competence rises with match progress (§4), but **who the
bot is must not change under the player.** Re-rolling each round turns character into noise, and the roster
reads as one AI resampling itself rather than as a set of opponents. A player who learns that one opponent
plays close and aggressive should still be right about that next round.

**Aim and footwork share one roll.** The same seed drives the movement axes (§5), so a bot's aiming and its
positioning are one coherent person rather than two independently random behaviours stapled together.

**One further variance operates per-acquisition rather than per-bot:** a small jitter on the reaction delay
each time a target is acquired. Without it, every bot in a squad reacts **on the same frame** to the same
event, and the volley reads as scripted no matter how different the individual bots are. Human reaction varies
shot to shot, not just person to person.

### 3.6 Resolution order — and why the clamps are last

> **Interpolate the competence curve → apply the bot's personality offset → CLAMP to absolute limits.
> The clamp is last, on purpose.**

Clamping last means **neither a mis-tuned curve nor an unlucky roll can produce a bot outside the sanctioned
envelope.** If clamping happened before the personality offset, the offset could push a bot back outside it,
and the guarantee would be decorative.

The absolute limits are where *"never overwhelming"* is actually enforced — not in the curve. In particular
there is a floor on reaction time at roughly human-expert visual reaction: **below that, a bot is superhuman
by definition rather than merely good**, and no combination of competence and personality may reach it.

---

## 4. DIFFICULTY AS A DESIGN AXIS

### 4.1 What may legitimately scale

**Difficulty scales PERCEPTION and REACTION** — how quickly a bot notices, how fast it can bring its aim
around, how precisely it holds, how well it positions. Every one of these is a property of *the opponent's
skill*, and every one of them is something a player can beat with their own skill.

### 4.2 What must NOT scale — and why the distinction is not cosmetic

> **Difficulty NEVER scales damage or health. A bot that hits harder is not harder — it is unfair.**

The reasoning is a direct consequence of bot-fire parity (§2):

- **A harder opponent is one that outplays you. A stronger opponent is one that outguns you.** Only the first
  can be answered by playing better. Raising a bot's damage does not ask more skill of the player, it asks
  more health — the player's counterplay is unchanged and their outcome is worse. That is difficulty as a
  tax, and **C1** says feel comes first.
- **It breaks the arsenal's balance model.** Damage is a property of the weapon (`combat-arsenal` §9). A
  difficulty setting that changes damage makes the same weapon deal different amounts depending on who holds
  it, which is exactly the fork §2.1 forbids — arriving through a settings menu instead of through a code path.
- **It destroys the transfer.** A player who beats a high-difficulty bot should have learned something that
  works on a person. If that bot won by having more health, they learned nothing transferable.

**In a staked context the distinction stops being about feel.** Where a match result settles a wager
(`ssot/matchmaking.md` §9, `ssot/league-play.md` §5), a bot with inflated damage or health is **an
undisclosed change to the odds**. A player who loses to it has lost to a parameter they could not see and
could not counter. Perception-and-reaction scaling keeps the contest legible: the bot was faster, and being
faster is something the player can observe, contest and improve against.

### 4.3 Difficulty may rise with match progress — and must never rubber-band

**Competence is a function of match progress**, so a match can open gently and sharpen as it goes.

**A score-based brake may only ever REDUCE difficulty, never raise it.** Raising difficulty when bots are
losing is **rubber-banding regardless of how symmetric the input looks** — a symmetric-seeming rule that reads
score in both directions still means the player's success causes their opposition to strengthen, which
invalidates their success. And in a mode carrying stakes and rating, that is an **integrity problem rather
than a feel one**: the match is no longer settling on what the players did.

Stated as an invariant: **a player getting better must never make their opposition better.** The only
sanctioned direction for an outcome-sensitive adjustment is downward.

---

## 5. COMBAT MOVEMENT AND EQS

### 5.1 Positioning is a range band, not a distance preference

A bot repositions within an annulus around its target: a **preferred engagement range**, a **band half-width**
around it, a **reposition interval**, and a **lateral weight** governing how much it prefers moving *around*
the target versus toward it.

**Aggression is expressed as distance.** A short preferred range is a pusher; a long one is a kiter. This is
deliberately a *held range* rather than a *distance preference*, and the difference is the entire point:

> **A held range is a number. A retreat bias is a direction.**

A scoring term that simply prefers greater distance, with nothing opposing it, is an **unbounded retreat** —
on a map with no cover to stop it, bots walk backwards until the round stalls. A band has an inner edge and an
outer edge, so the behaviour is bounded by construction rather than by whatever the level geometry happens to
provide.

The band's clamps encode the two failure modes: **too close** and the bot is standing on its target, which is
not combat; **too far** and bots disengage to the far side of the map and the round never resolves.

### 5.2 Continuity — the bot must be re-tasked before it arrives

**The reposition interval is a continuity lever, not a pacing knob.**

If a bot is allowed to *reach* its destination, the move completes, the behaviour sequence ends, and the bot
**stands still waiting to be re-entered.** The visible symptom is a bot that stutters between motion and
stillness — and the cause is not a movement bug but a goal that was satisfiable.

**Re-tasking before arrival makes continuity emergent rather than authored.** The band is sized so that a bot
travelling at combat speed cannot cross it within one interval, so there is always a new goal before the old
one completes, and continuous motion falls out of the arithmetic rather than out of a special case.

**Both extremes are failures, which is why the interval is clamped on both sides.** Too slow reintroduces the
standing gaps. **Too fast is worse than standing still**: the goal moves before the bot can commit to a path,
direction reverses every tick, and the bot thrashes between two points. Any test of this must therefore assert
reversal rate **two-sided** — a one-sided "is it moving enough" check passes the thrashing case.

### 5.3 The structural lesson: ask what the query returns before touching movement code

> **Strafing failed as a movement problem and resolved as an EQS query problem. If a bot will not move
> somewhere, ask what the query returns before you touch movement code.**

The specific shape of it, because it generalises: strafing was already enabled on the movement request and
produced nothing visible. The reason was not in movement at all — **the query generated candidate positions
toward the target**, so velocity and facing were parallel and there was no lateral component to render.
**Strafe is authored in the query, not in movement code.**

A second instance of the same class: a reachability filter that **had never executed once**, because it was
scoped to the wrong context — evaluated against the item rather than the querier. It was present, well-formed
and inert. The behaviour it was meant to produce was simply absent, with no error.

**Why this is worth stating as a law rather than an anecdote:** both defects presented as *"the bot won't move
right"*, which points an investigator at movement. Movement was correct in both cases. **A behaviour tree can
only choose among the positions its query offers** — if the query returns nothing, or returns only positions
in one direction, or silently never filters, no amount of movement tuning can produce the missing behaviour.

**The diagnostic order that follows:** does the query return items at all → are they in the right places →
is the goal actually being issued → and only then, is the movement executing it. Instrumentation should
distinguish these states rather than reporting a single "the bot didn't move", because they have entirely
different causes.

**One caution on that instrumentation.** A lifetime average cannot see an intermittent extreme — it reports a
bot that was wedged for one round and fine for seven as *mildly sluggish*. Behavioural assertions are
evaluated **per round, failing on the worst one and naming it**. Delegate binds for these hooks need the
**X3** guard: a double-bind double-counts.

---

## 6. BOT-FILL — WHERE BOTS ARE LEGITIMATE

### 6.1 Fill is convergent and human-aware

**The target population is a property of the mode; bots fill the difference.** `target = team size × team
count`, and bots aim for `max(0, target − humans)`.

**Fill converges rather than deciding once.** A one-shot count taken at experience load is wrong on a listen
server, because the host loads before remote clients connect — "humans present" reads as 1 and the fill
overshoots. So the fill reconciles on every late human join and leave: a join over target removes a bot **from
the fuller team**, a leave under target adds one. Removing from the fuller team is what keeps the split
balanced, and the result converges to the same population regardless of connect order.

**Fill replaces only the count decision.** Spawning, possession and team routing reuse the same path every
participant takes, so a bot still flows through the ordinary team-assignment logic and is balanced by live
team population.

**Team assignment is keyed on live population, never on a player id.** Bots arrive with an uninitialised id,
so keying assignment by it piles every bot onto one team. Live count is the only key that is bot-safe by
construction.

### 6.2 Fill is not the matchmaker's job

`ssot/matchmaking.md` defines the assignment provider seam. **The provider exposes whether assignment is
authoritative; it never spawns or assigns bots.**

Where an authoritative matchmaker seats all humans before the match starts, the convergent fill path is inert
by design — there is nothing to converge, because the population is known up front. **Fill stays out of the
provider's split.** Two systems that both decide team membership will eventually disagree, and the disagreement
surfaces as an unbalanced match nobody can attribute.

### 6.3 The hard line — stake and rating

> **A match whose RESULT CARRIES STAKE OR RATING cannot be filled with bots.**

**Because a bot's presence changes what the wager settles on.** A staked match is an agreement about a
contest between the participants. Substituting a bot for one of them changes the contest into something the
other party did not agree to — and it does so in a way that is *invisible from the result*. The scoreboard
looks the same either way.

Three specific consequences, each independently sufficient:

- **The wager no longer settles what it claims to.** A player who staked on beating opponents did not stake on
  beating a fill bot, whatever the bot's difficulty. The stake was accepted under a description that is no
  longer true.
- **It is directly exploitable.** If a queue fills with bots when it is thin, then **making a queue thin
  becomes a strategy** — the reliable way to win a staked match is to enter one nobody else is in. That
  inverts the entire purpose of staking.
- **Rating becomes meaningless in exactly the place it matters most.** `ssot/league-play.md` §2.1 requires
  rating to measure strength so matchmaking can sort by it. Rating movement earned against fill bots is
  measuring the fill algorithm, and it contaminates the rating of every human that player subsequently meets.

**The rule is about the RESULT, not the mode.** The question is never "is this a bot mode" but **"does this
match's outcome move a balance or a rating?"** If yes, the population is human or the match does not run.
This connects directly to `ssot/matchmaking.md` §7's population-transparency requirement: **an empty band must
be shown as empty, not quietly filled.** Bot-filling a cold stake band would be the dishonest resolution of
exactly the problem transparency exists to handle honestly.

---

## 7. NAVIGATION — WHAT A MAP MUST PROVIDE

Bots require navigable space. The map-side requirements and the containment law live in
`ssot/map-build-system.md` and are **cited, not restated**. What belongs here is **what AI needs and what
happens when it is missing**:

| Requirement | Failure mode when absent |
|---|---|
| **Navigation coverage over all reachable play space** | Bots cannot path into uncovered regions. They do not error — they simply never go there, so part of the map is human-only and reads as bots ignoring an objective |
| **Coverage that matches where players actually go** | Query positions land on unnavigable ground, the reachability filter rejects them, and the query returns nothing. §5.3's diagnostic order applies |
| **Connectivity across vertical and multi-tier geometry** | Bots hold one tier and never contest the others. This looks like a behaviour bug and is a navigation gap |
| **Navigation that regenerates for runtime geometry** | A deployable or destructible that changes the space leaves bots pathing through what is now a wall |

**The failure signature is shared and worth recognising:** navigation gaps present as **behaviour** problems.
The bot appears passive, ignores an area, or refuses an objective — and every one of those descriptions points
an investigator at the behaviour tree, which is not where the problem is. **Verify navigation coverage before
diagnosing bot behaviour on a new or altered map.**

---

## 8. INTERFACES

### 8.1 What AI CONSUMES

| From | What |
|---|---|
| **Character** | The **pawn data** and the movement components. Bots drive the same components players do — sprint, dash, slide, roll (`ssot/character-system.md`) |
| **Combat** | **Ability sets**, granted by the same route as a player's (**A3**). Bots hold real weapons with real cooldowns |
| **Map** | **Navigation data** and the play space it covers (§7) |
| **Match** | **Match progress** (for competence, §4.3) and phase state — bots are gated by the same match-phase tags as players (`combat-arsenal` §2.1) |
| **Environment queries** | Candidate positions, filtered for reachability. §5.3 |

### 8.2 What AI OWES — a participant indistinguishable from a player

> **A bot must count, die, place and score through the same paths a human does.**

This is the whole obligation, and it is stronger than it sounds. Match logic must not need to know whether a
participant is a bot:

- **It counts** toward team population and alive counts.
- **It dies** through the same death path, firing the same signals, so last-standing and round resolution see
  it (`ssot/match-modes.md`).
- **It places** in a placement-based result exactly as a human does.
- **It scores**, and its eliminations attribute normally through combat telemetry (`combat-arsenal` §7).

**Why this matters more than it appears:** every one of those systems is a place where a special case for bots
becomes a bug for humans. `ssot/combat-arsenal.md` §8.1 records the mirror-image failure — a deployable
implemented as a Pawn gets counted as a participant and stalls the round. **Here the requirement runs the
other way: a bot that is not counted as a participant leaves a round that cannot resolve, because something
alive is not in the tally.**

**The single exception to the interface contract is the input trigger** (§2), and it is invisible to every
system above.

#### 8.2.1 R34 — a recorded, deliberate exception on CAPABILITY

> **R34 (2026-08-05): bots are a TEMPORARY POPULATION MEASURE until real player counts support matches.
> GAMEPLAY AND CHARACTER CAPABILITY ARE NEVER LIMITED TO KEEP BOTS VIABLE.**

**The general rule above is not weakened.** A bot still counts, dies, places and scores through the same
paths, and match logic still must not know whether a participant is a bot. **What R34 carves out is
different: it settles who yields when a NEW capability outruns bot support.**

The first instance is **water**. R32 makes water traversable and playable
([`ShantyTown_Water_Swim_DESIGN.md`](../design/ShantyTown_Water_Swim_DESIGN.md)), and navmesh does not cover
it — so bots cannot follow a player in. Under the general rule that would be a defect and a blocker. **Under
R34 it is accepted**, and the navigation work is *opportunistic*: added if it is cheap once swim works,
skipped otherwise.

**The cost is understood and accepted, not overlooked.** While bots cannot swim, water is a bot-free space,
and in a bot-populated match standing in it is advantageous. R34 accepts that because bots are transitional —
the exploit expires when the population does.

**Why this is recorded rather than simply done.** An exception that lives only in a decision nobody wrote down
is indistinguishable from a defect to whoever finds it next: they see bots failing to enter water, read §8.2,
and file it as a bug. **The carve-out has to be as findable as the rule it qualifies.**

**The boundary of the exception, so it is not over-applied.** R34 governs **capability gaps on new features**.
It does **not** license a bot that is miscounted, that dies through a different path, that is skipped by
placement, or whose eliminations attribute differently — those remain defects, because they corrupt the match
record rather than merely limiting where a bot can go.

**Server authority applies throughout** (**N1**): bots exist and act on the server. There is no client-side bot.

### 8.3 A note on the engine seam

The bot controller extends the engine's bot controller class. Where that base class is not exported for
subclassing across a module boundary, patching the engine class is the **correct fallback** under **G2**, not
a violation — and the patch must be recorded and re-applied after an engine bump, which is the only obligation
it creates.

---

## 9. OPEN DESIGN QUESTIONS

1. **Difficulty tier definitions.** How many player-facing tiers exist, what each is called, and where each
   sits on the competence curve. §4 fixes what may and may not scale; it does not fix the granularity or the
   labels.
2. **Whether personality variance is authored or seeded.** Currently derived from a bot's identity, which
   makes it stable and free. The alternative is an authored roster of named personalities — more
   characterful and more legible to players, but it is content to maintain and it caps the variety at the
   number authored. Interacts with whether bots are ever presented as recurring characters.
3. **Bot competence across two position depths.** Competence is a function of match progress. **BATTLE ROYALE
   and MATCH PLAY give it two different clocks** — a monotonically falling alive-count over N positions, versus
   a round score climbing to a threshold over 2 — and the curve is currently written against one of them. Which
   input each ruleset feeds, and whether one curve can serve both, is undefined.
   *(This question replaced "bot behaviour under TURBO's instant respawn", which R41 parked with the ruleset.
   Should TURBO ever revive, the original applies again: continuous respawn has no equivalent clock at all,
   so the competence curve would have no input rather than a second one — `ssot/match-modes.md` §9.7.)*
4. ~~**Whether bots appear only in unrated queues, or also warm cold stake bands.**~~ — **CLOSED by R74: they
   never enter a population count.** This item had already reasoned its way to the answer — *"a population count
   must never include a participant whose presence would be forbidden in a settling match"* — and R74 simply
   adopts it as law. The pointer it carried was also stale: population transparency is `ssot/matchmaking.md`
   **§7**, not §6.
5. **Squad-level coordination versus independent agents.** Bots currently reason individually. Coordination
   (focus fire, covering angles, staggered pushes) would read as far more competent — but it directly opposes
   §3.5: shared decisions are what make a squad read as one entity. If coordination is added, the variance
   that keeps them individual has to be preserved *through* it, and how is not obvious.

---

## 10. RULINGS OF RECORD

| Ruling | Date | Content |
|---|---|---|
| **R74 — A BOT IS NEVER COUNTED IN A POPULATION FIGURE** | **2026-08-06** | No population count, wait estimate or band-health figure shown to a player may include a bot — in any queue, rated or not, staked or not. This extends §6.3 from *filling* to *display*: §6.3 already forbids bots in any match whose result carries stake or rating, and R74 closes the remaining gap where a bot could still make a band **look** populated. **The reasoning is §9.4's own:** a count that includes a participant who cannot settle is a count a player cannot act on, and `ssot/matchmaking.md` §7 exists precisely so an empty band **looks** empty rather than silently never matching. A warmer-looking lobby is not worth the one property that section protects. **⚠ This also removes a mitigation the economy SSOT was leaning on** — `economy-store.md` §5.2 previously justified its paid-places threshold argument with *"bot-fill lands them full"*, citing §8.2.1, which says no such thing. That citation is corrected there; the surviving mitigation is the queue-sizing constraint, which never needed bots. **Closes §9.4.** |
| **R24 — AI and bots get their own SSOT** | **2026-08-05** | AI is **not** assigned to an existing SSOT. It touches matchmaking (bot-fill), combat-arsenal (bots fire the same abilities under **A9**) and character-system (they drive the same movement components), but is owned by none of them. **A system with its own design laws needs its own home** — distributing it would place its foundational law (bot-fire parity, §2) inside a document about something else. |

---

## 11. RELATED

- [`Docs/DOCTRINE.md`](../DOCTRINE.md) — laws cited here: **A3** abilities via ability sets · **A6** cooldowns
  are GEs · **A9** bot ability parity, the foundation of §2 · **C1** game feel before content · **G2** patching
  engine core is the correct fallback · **N1** server authority · **X3** `AddDynamic` is not idempotent ·
  **X13** a validator must fail on known-bad input.
- `ssot/combat-arsenal.md` — the abilities bots fire (§2 here is **A9** applied), the arsenal tag contract that
  gates them (§2 there), the angular-velocity telemetry §3.2 here must not pollute (§7.1 there), and the
  never-a-Pawn rule §8.2 here mirrors (§8.1 there).
- `ssot/matchmaking.md` — the assignment provider seam (§6.2 here), population transparency (§6 there) that
  §6.3's hard line protects, and staking.
- `ssot/league-play.md` — §2.1 there is why rating cannot absorb results against fill bots.
- `ssot/character-system.md` — the pawn data and movement components bots drive.
- `ssot/match-modes.md` — the phases bots are gated by, and the parked ruleset at §9.7.
- `ssot/map-build-system.md` — the navigable space §7 depends on.
