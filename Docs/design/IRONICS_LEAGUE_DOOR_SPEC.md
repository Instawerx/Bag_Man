# IRONICS — LEAGUE PLAY DOOR SPEC

**Implements:** `ssot/ui-frontend.md` **R98** — the free door behind the home-screen split.
**Style authority:** `IRONICS_UI_STYLE_SSOT.md` — cited, **never duplicated** (§10.2, *"one value, one home"*).
**Display type:** `UIDisplay.NeonTube` (style SSOT §6) — **derived, OPEN ITEM 5, flagged for approval.**
**Target:** UE5 CommonUI/UMG (**R75**). Console + PC.
**Visual reference:** `IRONICS_League_Door_Mockup.html` (live controls, states, motion).
**Sibling:** `IRONICS_HOME_SCREEN_SPEC.md` (the split this sits behind).

---

## 1. WHAT THIS SURFACE IS

The **free** door. Unrated, no buy-in, bots permitted, both leagues — played for **loot and Watts**.
This is the majority path and the community on-ramp.

**Four axes are rendered. A fifth is deliberately absent.**

| Axis | Values | Note |
|---|---|---|
| Ruleset | MATCH PLAY · BATTLE ROYALE | Two products, not options (`ui-frontend` §3.2) |
| League | HAYWIRE · PRO MOD | **Only LEAGUE PLAY offers both** — staked is Pro Mod only (R86) |
| Venue class | ARENA · MAP | R97. The *venue itself* stays a server outcome; the **class** is the choice |
| Size / **Field** | 1v1 … 8v8 (Match Play) · **`BR_9` `BR_20` `BR_36`** (Battle Royale, **R99**) | Carries the population readout (§3 below). Under BR the axis re-labels to **FIELD** — BR is one-vs-everyone and its position count IS the field size, so team brackets never appear there. **Paid-place counts are NOT shown on this door**: league has no pot, and quoting "3 paid" would describe a settlement that never happens here |
| ~~Stake~~ | **— absent —** | **There is no stake to pick.** Not hidden, not zeroed, not disabled |

> **The absent control is the point of R98.** A league player never encounters a buy-in. Any future
> temptation to render a greyed-out stake field here — "for consistency with the staked door" — reintroduces
> exactly the framing R98 removed.

---

## 2. DISPLAY TYPE — `UIDisplay.NeonTube`

**The heading uses the house neon-tube treatment. Nothing else on the surface does.**

Construction, bloom radii, hum timing and the UMG shipping note live in `IRONICS_UI_STYLE_SSOT.md` §6 and
are **not restated here**.

**The one rule this surface must not break:** the treatment is **size-gated to display, ≥64px** (style SSOT
§2.1). Every control, label, count and body string on this screen is **flat white on glass**.

**Why it is stated as a prohibition rather than a preference:** bloom at UI size is the specific thing that
makes neon interfaces unreadable. The operator brief was *"ideal but must read clean and crisp"* — those
are only compatible if the glow is confined to the one string that is pure identity and carries no
information a player has to parse quickly.

---

## 3. POPULATION — `ui-frontend` §5 IS A REQUIREMENT

Counts and estimated waits render **per size band**, on the band tile itself.

### 3.1 WHERE THE NUMBERS COME FROM

**`GET /population`, and nothing else. The server classifies; the door renders.**

The response carries a `state` per cell and per stake band. The door **never re-derives it** — §5's
thresholds are a policy decision, and a threshold two surfaces disagree about is a threshold that means
nothing. A door that computed its own `cold` would eventually disagree with the queue it is describing.

**The dev database is seeded, not empty** (`Bag_Man_Backend` → `npm run seed:population`). A mockup drawn
against an empty table cannot show a busy queue, a thin one and a dead one side by side, and the alternative
— hand-written numbers on the one surface whose entire job is honesty about population — is the exact
failure §5 exists to prevent. The seeder simulates an arrival process rather than listing figures, so a
cell's count and its wait come from **one run** and cannot contradict each other.

> ⚠ **Why that matters, concretely.** An earlier draft of this table carried `1,204 waiting · ~10s`. Those
> two numbers cannot both be true: at a 10-second median wait a 3v3 queue turns over its entire standing
> population every ten seconds, so 1,204 people standing would mean roughly 120 matches per second in one
> bracket. Hand-written pairs drift into impossibility precisely because nothing forces them to agree.

### 3.2 THE FIVE READINGS

Every example below is an **actual reading** from the seeded endpoint, not an illustration.

| Reading | Dot | Tile | Label | Means |
|---|---|---|---|---|
| **Live** | filled, Electric, **pulsing** (2.4s) | full opacity | `26 waiting · ~38s` | enough standing to fill a field |
| **Warm** | filled, tertiary blue, **no pulse** | full opacity | `5 waiting · ~20s` | fewer standing than one match needs |
| **No match yet** | as its state | as its state | `9 waiting · no match formed here yet` | people are queued; nothing has **ever** matched here |
| **Cold** | **hollow ring**, no glow, no pulse | **62% opacity** | *`Quiet · no estimate`* | the queue exists and is empty |
| **Not open** | hollow ring, 50% | **42% opacity, dashed border, not selectable** | *`Not open yet`* | **no map backs this queue** (R63) |

Plus **`Count unavailable`** (dashed ring) when the endpoint published the cell but could not count it.

**Three absences, three readings, and they must never collapse into each other:**

| | What it is a fact about |
|---|---|
| **Not open** | the **content**. No map ships for this cell, so it is not a queue yet. |
| **Cold** | the **queue**. It exists, anyone may enter, nobody has. |
| **Count unavailable** | **us**. The queue exists and we failed to read it. |

**An unopened bracket is still drawn.** It is disabled rather than hidden, because the whole designed ladder
is information: a player — and a reader of this spec — should be able to see what the game intends to offer
and which of it has actually shipped. Hiding it would be honest but mute.

### 3.3 THE PROPERTIES THAT ARE LOAD-BEARING

- **Per band, never aggregate only.** §5.2: *"A healthy total conceals a dead band, and the dead band is
  exactly the one a player needs warning about."*
- **A cold band is never presented identically to a busy one.** Distinct at a glance, and **still
  selectable** — the player who then waits is waiting *deliberately*, which §5.2 calls a completely
  different experience from the same wait imposed without explanation.
- **Estimates degrade honestly.** A band with no data says so. **Never render an optimistic number** on the
  one surface whose job is honesty about population.

This is *"the cheapest fragmentation mitigation available"* and **purely a rendering decision** — it needs
no matchmaking change (§5.1).

### 3.4 ⚠ FLAGGED FOR RULING — `classify()` READS OCCUPANCY, NOT HEALTH

Seeding the database surfaced a defect in §5's classification policy that argument alone had not.

`classify(waiting, slots)` calls a cell **live** when `waiting >= slots` and **warm** ("can't fill one
match") below that. But a matchmaker that pairs greedily removes `floor(waiting / slots)` full fields every
pass, so **the residue after any pass is always strictly less than `slots`, by construction.** Observing
`waiting >= slots` therefore means you looked *between* passes — it measures where in the matchmaking cycle
the read landed, not whether the queue is healthy.

The consequence is not theoretical. In the seeded set, all three of these read **`warm`**:

| Cell | Reading | Actually |
|---|---|---|
| `LeaguePlay_Haywire_MatchPlay_Arena_3v3` | `5 waiting · ~20s` | the busiest queue in the game |
| `LeaguePlay_Haywire_MatchPlay_Arena_8v8` | `12 waiting · ~39s` | healthy, slower |
| `LeaguePlay_ProMod_MatchPlay_Arena_8v8` | `9 waiting · no match formed here yet` | **dead — nothing has ever matched here** |

It bites League Play hardest, which is the majority side (R98) and the only unrated tier — an unrated queue
pairs any `slots` players, so it drains most completely and can essentially never show `live`.

**The honest signal is the observed wait, not the standing count.** A queue with two people in it and a
10-second median is thriving; one with nine and no match on record is dead. Both facts are already in the
response — `estimatedWaitSeconds` is measured, per cell, by the `/allocate` hook.

**Recommended:** classify on the wait where one exists, and fall back to the count only where it does not.
Left as a ruling rather than applied, because §5's thresholds are policy and this changes what every
population dot in the game means.

---

## 4. LAYOUT

Split layout, not a stepped flow (`ui-frontend` §3).

```
┌───────────────────────────────────────────────────────────────┐
│  ← Home                                  [Watts] [Volts]      │
│                                                               │
│                  ⟨ FREE TO PLAY · NO BUY-IN ⟩                 │
│                     L E A G U E   P L A Y                     │  ← UIDisplay.NeonTube
│                        ‖    ‖    ‖    ‖                       │     (mount hardware)
│        Play for loot and Watts. Bots fill any empty seat.     │
│                                                               │
│  ┌─────────────────────────────────┐  ┌────────────────────┐ │
│  │ RULESET     [Match Play][BR]    │  │ WHAT YOU PLAY FOR  │ │
│  │ LEAGUE      [Haywire][Pro Mod]  │  │  ⚡ Watts          │ │
│  │ VENUE CLASS [Arena][Map]        │  │  ▤ Loot            │ │
│  │ SIZE  ▦▦▦▦▦▦  + population      │  ├────────────────────┤ │
│  └─────────────────────────────────┘  │ summary → FIND     │ │
│                                       └────────────────────┘ │
└───────────────────────────────────────────────────────────────┘
```

Deck grid `minmax(0,1fr) 340px`, collapsing to one column below ~1080px equivalent. Panel radius 20px,
buttons 12px, inputs 8px (style SSOT §3 geometry).

---

## 5. STATES

| Element | State | Treatment |
|---|---|---|
| Segment / size tile | Selected | Electric fill @20%, border → Electric, inner + outer Electric glow |
| Segment / size tile | Hover / Focus | Border → **Arc-Violet**, 18px violet glow @22%, text → full white |
| Size tile | Cold | 62% opacity, hollow dot, italic label — distinct, **still selectable** |
| Find match | Hover | Lift 2px, Electric bloom 30→50px, violet 26px joins at the rim |
| Heading | Idle | Neon hum — style SSOT §6 |

**Hover and focus are visually identical** — a controller player and a mouse player must not be taught two
different languages.

---

## 6. FOCUS AND INPUT (CommonUI)

- **Focus order:** Back → Ruleset → League → Venue class → Size grid (row-major) → Find match → wallet.
- **B / Esc returns to the home screen.** Unlike the home screen, this surface **has** something behind it.
- **A / Enter** commits the focused control; the Size grid is a single focus group with D-pad traversal.
- Wallet chips are readouts, not focus stops.

---

## 7. EDGE CASES

- **All bands cold:** every size still renders with honest counts, and **Find match stays enabled**. Waiting
  deliberately is a different experience from waiting silently.
- **Population unavailable:** dots go neutral (dashed ring), counts read `Count unavailable`. **Never
  fabricate a number here**, and never render it as `0` — "we could not find out" and "nobody is there" are
  opposite claims.
- **Nothing in the ruleset is open:** distinct from all-cold. Every bracket renders **Not open yet**,
  disabled, and **Find match is disabled** — there is no queue to enter, so an enabled button would be an
  offer the game cannot honour. This is the live state of **Battle Royale on both doors today**.
- **Selection lands on a bracket that closes:** the selection moves to the first bracket that is still open,
  and to `none open yet` if there is none. Population arriving after first paint must never silently move
  what the player picked.
- **Battle Royale selected:** the axis re-labels to **FIELD** and renders `BR_9` · `BR_20` · `BR_36` (R99). MatchPlay team brackets never carry over — they are a different position model (2 positions regardless of team size).
- **Bots (R87):** *"bots fill remaining seats"* is stated up front, not discovered mid-match. Bots are a
  **LEAGUE PLAY feature** and must never appear on the staked side (`ai-bots` §6.3).
- **Long localised axis labels:** segment buttons wrap to two lines before truncating; the size grid reflows
  by `auto-fit`, never by shrinking the tile below a readable count.
- **TV safe area:** all content inside 5% title-safe; the right rail is the first to lose padding.

---

## 8. OWED

0. **⚠ RULING OWED — `classify()` reads occupancy, not health.** §3.4 above. It changes what every
   population dot in the game means, so it is not applied unilaterally.

1. **Type ramp unapproved** (style SSOT OPEN ITEM 1). The tube stroke widths are tuned to a condensed
   grotesque — a different shipping face requires re-tuning them.
2. **`UIDisplay.NeonTube` approval** (style SSOT OPEN ITEM 5), including the SDF-material-vs-texture call
   and whether the treatment extends beyond the two R98 door headings.
3. ~~BR bracket set under Battle Royale~~ — **CLOSED by R99** (`ssot/match-modes.md` §2.1.1): `BR_9` · `BR_20` · `BR_36`.
4. **Live population source.** No population/queue-health endpoint exists yet — the backend has 13 lambdas
   and none of them serves this. §5 is a requirement, so this is a build dependency, not a nicety.

---

## 9. RELATED

`ssot/ui-frontend.md` (R98, §3, §5) · `IRONICS_HOME_SCREEN_SPEC.md` · `IRONICS_UI_STYLE_SSOT.md` §6
(`UIDisplay.NeonTube`) · `ssot/economy-store.md` §3 (Watts are the player's property; the store is the
primary sink) · `ssot/ai-bots.md` §6.3 (bots are League-only)
