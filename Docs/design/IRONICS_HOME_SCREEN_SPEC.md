# IRONICS — HOME SCREEN SPEC (the LEAGUE PLAY / STAKED PLAY split)

**Implements:** `ssot/ui-frontend.md` **R98** — the home screen splits LEAGUE PLAY from STAKED PLAY.
**Style authority:** `IRONICS_UI_STYLE_SSOT.md`. Its rules are the house standard and are **cited here,
never duplicated** — a token copied into this file is the drift mechanism (`ssot/ui-frontend.md` §10.2,
*"one value, one home"*).
**Target:** UE5 CommonUI/UMG (**R75** — no web-tech UI runtime). Console + PC.
**Visual reference:** `IRONICS_Home_Screen_Mockup.html` (live states + motion; open in a browser).

---

## 1. WHAT THIS SCREEN IS

**The first decision in the game, made BEFORE any lobby exists.** Two doors, two products, two purposes.

| | LEAGUE PLAY | STAKED PLAY |
|---|---|---|
| Buy-in | **none** | Watts **or** Volts |
| Combat | Haywire **+** Pro Mod | **Pro Mod only** (R86) |
| Bots | yes — a match always starts (R87) | no |
| Rated | no | yes |
| Played for | **loot and Watts** | the stake |

**A LEAGUE PLAY player never reaches a stake control, because there is no stake to pick.** The denomination
choice (Watts / Volts) lives *behind* the staked door and is never asked on the league side.

**The majority of players are on the free side.** The split exists so the community path is not routed
through a wagering surface — see R98's reasoning in the register.

---

## 2. THE DOORS ARE NOT COLOUR-CODED — **RULED (R100, 2026-08-08)**

> **✅ RULED, NOT DERIVED: the two doors are NOT colour-coded. They differ by density, motion rate and
> content — never by palette.** Operator ruling **R100** (`ssot/ui-frontend.md` §15) confirmed the derived
> position. Electric leads fill on both doors; Arc-Violet stays a rim/focus accent on both.

**Why it was decided this way.** `ssot/ui-frontend.md` §10.2 states that **CHROME is the app's own furniture
and is identical for every player**, while **IDENTITY is what a player picked or a brand owns and is resolved
by tag through the registry**. Giving STAKED PLAY its own hue would make chrome carry meaning that belongs to
identity — the failure §10.2 calls *"invisible on any single screen: it only shows up as the same colour
meaning two different things in two places."*

**What would change if this is overruled.** Colour-separating the doors is a legitimate product choice, but
it needs a ruling rather than a mockup, because it widens what chrome is permitted to mean. Nothing else in
this spec depends on it.

---

## 2.1 THE C++ CHASSIS — `UAFLW_HomeScreen` (built 2026-08-08)

`AFLCombat/Public/UI/AFLW_HomeScreen.h` + `Private/UI/AFLW_HomeScreen.cpp`, an abstract
`UCommonActivatableWidget` the shipping WBP reparents to — the same chassis pattern as
`UAFLW_FrontEndMarket`. **The reason it is C++ and not a widget graph** is R98's sharpest clause:

> A LEAGUE PLAY PLAYER NEVER PICKS A STAKE AMOUNT — because there isn't one to pick.

A graph can be rewired by anyone; a static predicate can be held by a test. `IsStakeLegalForDoor` is that
predicate, and `AFLCombatTests/.../AFLHomeScreenSpec.cpp` holds it with three tests under **`AFL.Home.*`** —
no world, no widget tree, no PIE, so the rule is checked in CI before an editor is opened. The suite asserts
BOTH directions: league refuses every non-zero stake (swept, not spot-checked, so an off-by-one bound cannot
pass), *and* staked still accepts one — a too-strict predicate would satisfy the first assertion while making
staked play unenterable.

| Member | What it is for |
|---|---|
| `LeagueDoor` / `StakedDoor` | `BindWidget` — **required**. A home screen missing a door is not this screen, so it fails at compile rather than rendering a one-door surface that looks deliberate. |
| `bStakedPlayAvailable` | Defaults **false** — the honest state: the staked lobby is unbuilt and every staked queue is unpublished. Drives §5's Disabled treatment. |
| `StakedUnavailableReason` | Defaults to *"Not open yet"* so a WBP that forgets to set it still says something true. |
| `OnDoorChosen` + `BP_OnDoorChosen` | The class **resolves** the choice; the WBP **navigates**. §9.4 ends this spec at the split, so inventing destinations in C++ would bake in a guess the spec has not made. |
| `IsDoorAvailable` | League is **always** open. Gating the free half would strand the majority behind a door built for the minority. |
| `SetWalletReadout` | Chrome on both sides — a *balance* is not a *stake*, and league is exactly where Watts accumulate. Live source is `UAFLWalletComponent`; **the bind is owed**, and is left a setter rather than guessed. |

Two behaviours worth stating because they look like defects until you know why. `ChooseDoor` re-checks
availability **even though the disabled door is visually inert** — `SetIsInteractionEnabled` is presentation,
and a gamepad or accessibility path can still deliver the click, so the product rule is enforced where it is
authoritative. And the disabled staked door stays **visible and focusable**: a player must be able to see that
staked play exists and read why it is shut, or the split silently becomes a one-door screen.

**Nothing in the class sets colour.** Per R100 the palette belongs to the WBP and the style system; the class
comment records that the no-colour-coding rule is a *ruling*, not a preference, so a later edit knows.

**NOT in the class, and not scriptable:** all of §6's motion (the counter-phased breathe, the lift, the
glow) and the `1fr 1fr` slot fill that makes the doors equal weight. Those are UMG designer work, alongside
the art pass the type ramp still owes.

---

## 3. LAYOUT

**The split IS the composition.** Two equal doors side by side — equal weight is the statement that neither
path is the lesser one.

```
┌──────────────────────────────────────────────────────────────┐
│  ⚡IRONICS lockup                        [Watts] [Volts] [+]  │   top bar
│                                                              │
│                   CHOOSE HOW YOU PLAY                        │   kicker + headline
│                 Two ways into the arena                      │
│                                                              │
│   ┌────────────────────────┐  ┌────────────────────────┐    │
│   │  FREE TO PLAY          │  │  BUY-IN                │    │
│   │  LEAGUE                │  │  STAKED                │    │   the two doors
│   │  PLAY                  │  │  PLAY                  │    │   (equal, 1fr 1fr)
│   │  · Haywire + Pro Mod   │  │  [WATTS]  [VOLTS]      │    │
│   │  · Bots fill           │  │                        │    │
│   │  Earn Watts + Loot  →  │  │  Pro Mod · Rated    →  │    │
│   └────────────────────────┘  └────────────────────────┘    │
│                                                              │
│      Loadout   Store   Venues   Career   Settings            │   footer nav
└──────────────────────────────────────────────────────────────┘
```

**Grid:** `1fr 1fr`, gap `clamp(16px, 2vw, 30px)`. Doors collapse to a single column below 860px
equivalent — but note this is a console/PC surface, so the collapse is a safety net, not a target layout.

**Geometry:** panel radius **20px** (§3 band 16–24) · button **12px** · input **8px**.

---

## 4. TOKENS

**Cited from `IRONICS_UI_STYLE_SSOT.md` §2.1 / §3 — not restated.** The linear RGB values there are
authoritative; hex is for mockups only.

| Token | Role on this screen |
|---|---|
| `UI.House.Electric` | **LEAD** — door glass tint, `Glass.Border` override when active, CTA disc, fact ticks, currency marks |
| `UI.House.Violet` | **ACCENT** — door rim on hover/focus, nav hover border, lockup rim. **Never** a fill, core or text colour here |
| `UI.House.Black` | **DEPTH** — the ground everything reads against |
| `UI.House.White` | **TEXT** + glass-edge specular |
| `Glass.Bg.Primary` | door fill |
| `Glass.Bg.Secondary` | wallet chips, footer nav |
| `Glass.Bg.Tertiary` | dividers, badges, denomination cards |
| `Glass.Border` | every panel edge; **→ Electric when active** |
| `Glass.Blur` | 28px (band 20–40) — UMG `BackgroundBlur` |

**THE BLEND RULE, size-gated (§2.1 LAW).** Electric core + arc-violet rim, rim **ON at ≥64px**,
**core-dominant at ≤32px**.

- **Rim ON:** the wordmark lockup, the door panels.
- **Core-dominant, no rim:** wallet currency marks (15px), CTA arrow glyphs, footer nav glyphs.

**Semantic colours (§2.5) are deliberately unused on this screen** — they remain flagged for approval and
this surface has no state that needs one.

---

## 5. STATES

| Element | State | Treatment |
|---|---|---|
| Door | Default | `Glass.Bg.Primary`, `Glass.Border`; idle breathe ±5px / 7.5s, the two doors **counter-phased** |
| Door | Hover / Focus | Lift −12px, scale 1.012, border → Electric, violet rim fades in 300ms, outer Electric glow 46px @ 28%, **breathe pauses** |
| Door | Pressed | Settle to −6px, scale 0.996, 120ms |
| Door | Selected | Chosen scales 1.045 with a 90px Electric bloom; sibling → 12% opacity, scale 0.94, 3px blur |
| Door | Disabled | 42% opacity, 65% grayscale, motion stopped, no hover response, one-line reason beneath the title |
| Wallet chip | Default | `Glass.Bg.Secondary`; readout only, not a focus stop |
| Wallet "+" | Hover | Border → violet, text → full white. **Is** a focus stop |
| Nav button | Hover | Text → full white, border → violet, 22px violet glow @ 28% |

**Hover and focus are visually identical** — a controller player and a mouse player must see the same
affordance, or the surface teaches two different languages.

---

## 6. MOTION

**Motion is meaning (§3).** Nothing here moves decoratively: the entry sequence establishes hierarchy, the
breathe says *live*, and the select transition reveals depth by pushing the road not taken behind the chosen
one.

| Element | Trigger | Motion | Duration | Easing |
|---|---|---|---|---|
| Top bar | Screen enter | Fade + 14px down | 700ms @ 150ms | `(.16,1,.3,1)` |
| Headline | Screen enter | Fade + 14px down | 700ms @ 300ms | `(.16,1,.3,1)` |
| Door 1 / Door 2 | Screen enter | Rise 56px, scale .97 → 1 | 900ms @ 450 / 600ms | `(.16,1,.3,1)` |
| Doors | Idle | Breathe ±5px, counter-phased | 7.5s loop | `(.65,0,.35,1)` |
| Door | Select | Chosen blooms, sibling recedes + blurs | 620ms | `(.16,1,.3,1)` |
| CTA arrow | Hover | Translate +5px | 300ms | `(.16,1,.3,1)` |

**Reduced motion:** honour the platform setting — entry resolves to a plain fade, the breathe stops, the
select transition becomes an instant state change. The screen must remain fully operable with all motion off.

---

## 7. FOCUS AND INPUT (CommonUI)

- **Initial focus: LEAGUE PLAY.** The free door is the majority path and the default must say so.
  **Never default focus to the buy-in.**
- **Focus order:** League → Staked → Loadout → Store → Venues → Career → Settings → wallet "+".
- **D-pad ← / →** moves between doors; **↓** drops to the footer nav; **↑** returns to the doors.
- **A / Enter** commits. **B / Esc is inert** — this is the root surface, with nothing behind it.
- Focus never rests on a non-actionable element. The wallet chips are readouts; only the "+" is a stop.

---

## 8. CONTENT RULES AND EDGE CASES

- **Naming is LAW (§1).** Every player-facing string reads **IRONICS**. **BAG MAN and AFL never appear on
  this screen or any other.**
- **Wallet:** tabular numerals, thousands separators, abbreviate ≥1,000,000 as `1.2M`. A zero balance still
  renders — never hide the chip.
- **Volts = 0 does NOT disable the staked door.** Watts entry stays open; that is the entire point of the
  earned ladder (`ssot/economy-store.md` §3.3 — Watts entry is *"effectively free to anyone who plays"*).
  Disabling the door on a zero Volts balance would break the earn→stake loop.
- **Wallet unavailable** (profile fetch pending): chips show a shimmer skeleton, **never `0`**. A wrong
  balance is worse than no balance on a surface that leads to a wager.
- **Staked unavailable** (region gate, age gate, service down): staked door takes the disabled state with a
  one-line reason beneath the title. **League stays live** — a staked outage must never take the free game
  down with it.
- **Long localised titles:** door headings wrap to two lines by design; the type ramp steps down one stop
  before it truncates.
- **Ultrawide (21:9 / 32:9):** doors hold a max-width and the gap grows. They must never stretch past ~1.6
  aspect or the split stops reading as two equal choices.
- **TV safe area:** all content inside 5% title-safe. The footer nav is the first thing to lose padding.

---

## 9. OWED BEFORE BUILD

1. **The type ramp is unapproved.** `IRONICS_UI_STYLE_SSOT.md` §4 flags it as *derived, no type spec
   existed*. The mockup uses a system stack as a stand-in — **do not inherit a shipping display face from
   it.**
2. ~~**§2 colour-coding decision** needs a yes or an overrule.~~ **CLOSED by R100 (2026-08-08)** — the
   derived position was confirmed: no colour separation. **This was the last DESIGN blocker.** Items 1 and 3
   are ART dependencies (type ramp, bolt lockup), and neither gates the door structure, the routing, the
   focus order, or the never-show-a-stake-to-a-league-player rule. **The home screen is buildable now**,
   against a swappable stand-in face.
3. **Bolt lockup:** the final 2D UI texture and the 3D hero emblem integrate as produced
   (`IRONICS_UI_STYLE_SSOT.md` §1, `Docs/IRONICS_BOLT_Assets.html`). The inline mark in the mockup is a
   stand-in at the correct rim treatment.
4. **Where each door leads is out of scope here.** This spec ends at the split; the two lobbies behind it
   are separate surfaces.

---

## 10. RELATED

`ssot/ui-frontend.md` (R98, §2, §3.2, §10) · `IRONICS_UI_STYLE_SSOT.md` (the style authority) ·
`ssot/economy-store.md` §3 (Watts/Volts, the earn→stake loop) · `ssot/matchmaking.md` §4.2 (queue
dimensions — **unchanged by R98**)
