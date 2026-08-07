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
| Size | 1v1 … 8v8 | Carries the population readout (§3 below) |
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

| Band state | Dot | Tile | Label |
|---|---|---|---|
| **Live** | filled, Electric, **pulsing** (2.4s) | full opacity | `1,204 waiting · ~10s` |
| **Warm** | filled, tertiary blue, **no pulse** | full opacity | `97 waiting · ~55s` |
| **Cold** | **hollow ring**, no glow, no pulse | **62% opacity** | *`Quiet · no estimate`* (italic) |

**Three properties, all load-bearing:**

- **Per band, never aggregate only.** §5.2: *"A healthy total conceals a dead band, and the dead band is
  exactly the one a player needs warning about."*
- **A cold band is never presented identically to a busy one.** Distinct at a glance, and **still
  selectable** — the player who then waits is waiting *deliberately*, which §5.2 calls a completely
  different experience from the same wait imposed without explanation.
- **Estimates degrade honestly.** A band with no data says so. **Never render an optimistic number** on the
  one surface whose job is honesty about population.

This is *"the cheapest fragmentation mitigation available"* and **purely a rendering decision** — it needs
no matchmaking change (§5.1).

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
- **Population unavailable:** dots go neutral, counts read `—`. **Never fabricate a number here.**
- **Battle Royale selected:** the size axis re-labels to BR brackets. MatchPlay sizes do not carry over.
- **Bots (R87):** *"bots fill remaining seats"* is stated up front, not discovered mid-match. Bots are a
  **LEAGUE PLAY feature** and must never appear on the staked side (`ai-bots` §6.3).
- **Long localised axis labels:** segment buttons wrap to two lines before truncating; the size grid reflows
  by `auto-fit`, never by shrinking the tile below a readable count.
- **TV safe area:** all content inside 5% title-safe; the right rail is the first to lose padding.

---

## 8. OWED

1. **Type ramp unapproved** (style SSOT OPEN ITEM 1). The tube stroke widths are tuned to a condensed
   grotesque — a different shipping face requires re-tuning them.
2. **`UIDisplay.NeonTube` approval** (style SSOT OPEN ITEM 5), including the SDF-material-vs-texture call
   and whether the treatment extends beyond the two R98 door headings.
3. **BR bracket set** for the size axis under Battle Royale is not specified here.
4. **Live population source.** No population/queue-health endpoint exists yet — the backend has 13 lambdas
   and none of them serves this. §5 is a requirement, so this is a build dependency, not a nicety.

---

## 9. RELATED

`ssot/ui-frontend.md` (R98, §3, §5) · `IRONICS_HOME_SCREEN_SPEC.md` · `IRONICS_UI_STYLE_SSOT.md` §6
(`UIDisplay.NeonTube`) · `ssot/economy-store.md` §3 (Watts are the player's property; the store is the
primary sink) · `ssot/ai-bots.md` §6.3 (bots are League-only)
