# IRONICS — STAKED PLAY DOOR SPEC

**Implements:** `ssot/ui-frontend.md` **R98** — the buy-in door behind the home-screen split.
**Style authority:** `IRONICS_UI_STYLE_SSOT.md` — cited, **never duplicated** (§10.2, *"one value, one home"*).
**Display type:** `UIDisplay.NeonTube` (style SSOT §6) — derived, OPEN ITEM 5.
**Field law:** `ssot/match-modes.md` **R99** — BR is `BR_9 · BR_20 · BR_36`.
**Target:** UE5 CommonUI/UMG (**R75**). Console + PC.
**Visual reference:** `IRONICS_Staked_Door_Mockup.html`.
**Siblings:** `IRONICS_HOME_SCREEN_SPEC.md` · `IRONICS_LEAGUE_DOOR_SPEC.md`.

---

## 1. WHAT CHANGES FROM THE LEAGUE DOOR

| Axis | League door | Staked door |
|---|---|---|
| Denomination | — absent — | **WATTS · VOLTS** — sealed pools, no conversion (R81) |
| League | Haywire · Pro Mod | **— absent —** Pro Mod only (R86); a stated fact, not a control |
| Ruleset | Match Play · BR | same |
| Venue class | Arena · Map | same (R97) |
| Size / Field | brackets + population | same, with its own population |
| **Stake** | — absent — | **presets + numeric + live band** (§4) |
| Bots | fill empty seats (R87) | **never** (`ai-bots` §6.3) |

**Chrome is identical to the League door, deliberately.** §10.2 makes chrome the app's own furniture, the same
for every player, while identity is registry-resolved. The doors differ by **content and density**, never by
palette — colour-separating this surface would make chrome carry meaning that belongs to identity.

---

## 2. STAKE ENTRY — §4

**Presets primary, numeric secondary, NO slider.** §4.1 rejects the slider on four grounds, the load-bearing
one being that **it implies a continuum where the design has discrete tiers**.

| Ladder | Rungs | Ruling |
|---|---|---|
| Watts | 250 · 1,000 · 5,000 · 25,000 | R88 — sits **below** peg-equivalence (R80), deliberately cheaper |
| Volts | 100 · 500 · 2,500 · 10,000 | R69 / R88 |

**Switching denomination re-bases everything** — ladder, currency suffix, balance reference, band. **The
entered amount does NOT carry across:** 1,000 W and 1,000 V are not the same bet, and carrying the number
would imply they are.

---

## 3. THE MATCHING BAND — §4.2, NOT OPTIONAL

**The failure it prevents:** a player enters 450, is matched against 500, and concludes the system cheated
them. Nothing went wrong — banding *is* the design — but the interface let them form a belief it was never
going to honour. **An interface that accepts an exact number without qualifying it has made a promise on the
matcher's behalf.**

- **The band updates live** as the value changes, so the boundary is visible *before* commit.
- **The band is stated in the same place as the value** — never a tooltip. *"A qualification the player has
  to go looking for is a qualification that was not made."*
- **Snapping is the SERVER's** (R59). The UI displays; it never re-implements the rule. A tie between rungs
  goes **down** — a player is never quietly pushed up a tier.
- Band width is ±20% of the snapped rung, **widens with wait**, capped, **centre never moves** (R60).

---

## 4. FIELD SIZE — R99

Under **BATTLE ROYALE** the size axis becomes **FIELD** and renders `BR_9 · BR_20 · BR_36`. Match Play team
brackets never appear under BR — they are a different position model entirely (2 positions regardless of team
size).

**THE HONEST-CARD OBLIGATION.** Each field card states its payout shape next to the size:

| Card | Reads | Why |
|---|---|---|
| `BR_9` | **9 players · WINNER TAKE ALL** | `p(9) = 1`. **No min-cash floor exists below N = 10** — R37's 1.40× guarantee starts at 10 |
| `BR_20` | 20 players · 3 paid | |
| `BR_36` | 36 players · 6 paid | |

`WINNER TAKE ALL` is rendered in full white while the others stay secondary. This is §4.2's principle applied
to **field shape** rather than stake band: the structure is chosen knowingly, never discovered at settlement.

---

## 5. POPULATION — PER SIZE **AND** PER STAKE BAND

§5 requires counts *"per size and per stake band"*. Both render: size/field tiles carry their own counts, and
**each stake preset carries the population of that band**. A rung nobody is playing shows cold — hollow dot,
62% opacity, italic — and **stays selectable**.

---

## 6. BALANCE, ESCROW, AND THE GUARD

| Condition | Treatment |
|---|---|
| Sufficient balance | Find match enabled; "balance after" previews the deduction |
| **Insufficient balance** | Find match **disabled**, shortfall stated inline, route to store. **The client check is COURTESY ONLY — the server's 402 is the real gate** |
| Balance unknown | Skeleton, never `0`; Find match disabled until known. A wrong balance is worse than no balance on a wagering surface |
| Volts = 0 | Denomination stays selectable — switching to Watts is the intended path, not an error |

**Escrow is at MATCH START, not queue time.** Stated on the surface: a player who queues and cancels must
never have been charged.

---

## 7. STATES, FOCUS, EDGE CASES

| Element | State | Treatment |
|---|---|---|
| Denomination / preset / size | Selected | Electric fill @20–24%, border → Electric, Electric glow |
| Any control | Hover / Focus | Border → **Arc-Violet**, 18px violet glow @22% |
| Numeric input | Focus | Border → Electric, 20px glow. **Tabular numerals, always** |
| Band bar | Value change | Fill and marker ease 300ms — the band must be *seen* to move, not snap |
| Find match | Disabled | 40% opacity, no glow, reason inline above it |

**Focus order:** Back → Denomination → Ruleset → Venue class → Size/Field → Stake presets → numeric → Find
match → wallet. **B / Esc returns to the home screen.** Hover and focus are visually identical.

- **Amount below the lowest rung:** snaps up; the band says so before commit.
- **Amount above the highest rung:** clamps to the top rung.
- **Non-integer / non-positive:** rejected at the field. The server rejects too (E1) — this is convenience,
  not the guard.

---

## 8. OWED

1. **Population endpoint does not exist** — 13 backend lambdas, none serves queue health. Doubled here,
   because §5 wants per-band as well as per-size.
2. **`UIDisplay.NeonTube`** — style SSOT OPEN ITEM 5, still derived.
3. **Type ramp** — OPEN ITEM 1, still unapproved.
4. **BR squad variant.** R92 provides for squads holding positions; whether staked BR ships solo-only or
   both is unspecified. R99's floor applies to **squads** if squads are used.

---

## 9. RELATED

`ssot/ui-frontend.md` (R98, §4, §5) · `ssot/match-modes.md` §2.1.1 + R99 (field law) ·
`ssot/economy-store.md` §3 (sealed pools), §5.2 (payout depth) · `IRONICS_UI_STYLE_SSOT.md` §6
