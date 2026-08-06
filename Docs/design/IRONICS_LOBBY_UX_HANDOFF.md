# IRONICS — MATCHMAKING LOBBY: UX FLOW + DEVELOPER HANDOFF

**What this is:** the screen set, UX flow and implementation spec for the matchmaking lobby, built on the
PokerStars layout grammar and filled with our content model.

**What this is not:** a Tier 2 SSOT and not a status board. It carries screens and phasing; it does not carry
status claims. The rulings it implements live in `ssot/ui-frontend.md`, `ssot/matchmaking.md`,
`ssot/match-modes.md` and `ssot/economy-store.md` — **cited, never restated**. Tokens are
`design/IRONICS_UI_STYLE_SSOT.md`.

---

## 0. THE ONE CONFLICT, RESOLVED UP FRONT

**PokerStars' lobby is a BROWSER.** ~896 rows, one per named tournament, each with its own prize pool and
identity. The player scans and picks a specific event.

**R18 rules the opposite for us.** The front end is a **stake lobby, not a map browser**: the player chooses
**MATCH SIZE** and **STAKE**, and venue is a server outcome. `ui-frontend.md` §2.1 states the reason —
*every axis the front end offers is an axis it must then honour*, so a row naming a venue is a promise the
matchmaker must either keep (fragmenting the queue by map) or break.

### The resolution this document implements

> **PokerStars' LAYOUT, CHROME and INTERACTION GRAMMAR, 1:1. Our CONTENT MODEL, NAMING and RULINGS inside it.**

The skeleton transfers almost perfectly, because the *shape* of the problem is the same — a top-level format
choice, a filter row, a scannable list, a detail panel, a commit button:

| PokerStars element | IRONICS equivalent | Ruling |
|---|---|---|
| Format tabs (Cash · Zoom · Sit & Go · Spin & Go · Tourney) | **Ruleset tabs — SHOOTOUT · TURBO** | R19, matchmaking R7 |
| Filter row (Game · Buy-In · Table Size · Speed · Type) | **The two axes — SIZE · STAKE** | R18 |
| Tournament list (896 rows, one per event) | **Queue list — one row per SIZE × STAKE BAND** | R18 |
| Right detail card ("REGISTERING", buy-in, prize pool, Register) | **Queue detail panel** | §3.3 |
| `Register` / `Lobby` buttons | **`QUEUE` / `BATTLE AGAIN`** | R22 |
| Player count top-right (9,988) | **Population per band** | R21 |
| Spin & Go buy-in tile grid | **Stake preset tiles** | R20 |
| `Number of Games` stepper (− 1 +) | **Numeric stake entry (secondary)** | R20 |

**Five places we deliberately diverge**, each with the ruling that forces it:

1. **Rows are QUEUES, not events.** A row is `SHOOTOUT · Duo · 400–500 V`. It never names a map. **R18.**
2. **The list is short and bounded.** Queue count is bounded by design (`matchmaking.md` §4). PokerStars'
   infinite scroll is the wrong shape for a set that fits on one screen — no virtualisation needed, and the
   absence of scroll is itself information.
3. **We show a BAND, never an exact figure.** "matching 400–500 V". **R20 §4.2** — an interface that accepts
   an exact number without qualifying it has made a promise on the matcher's behalf.
4. **Volts and Watts, integers only. NEVER USD, anywhere, ever.** Copy law, `IRONICS_UI_STYLE_SSOT.md` §5.
   This is the single hardest 1:1 break from the reference — PokerStars is dollars end to end.
5. **Venue is disclosed as an outcome:** *"venue assigned at match start"*. **R18 §2.2** — stated up front, not
   framed as a limitation, and never a picker.

---

## 1. SCREEN SET

Eight screens. IDs are stable and are the naming contract for widgets, assets and tickets.

| ID | Screen | PokerStars analogue | Purpose |
|---|---|---|---|
| **S1** | `LobbyRoot` | Main lobby | Ruleset tabs + axes + queue list + detail. The hub. |
| **S2** | `QueueDetail` | "REGISTERING" panel / tournament modal | Everything about one queue before committing. |
| **S3** | `StakeEntry` | Spin & Go tile grid + stepper | Preset tiles, numeric field, live band readout. |
| **S4** | `TicketReview` | Register confirm | Guardrails surface. The last screen before currency moves. |
| **S5** | `InQueue` | Searching state | Population, estimated wait, cancel. |
| **S6** | `MatchFound` | Seat-assignment splash | Venue reveal + loadout last-look. |
| **S7** | `MatchResults` | Tournament results / cashier | Placement, payout, **one-tap re-queue**. |
| **S8** | `VenueShowcase` | *(no analogue — deliberate)* | Browse maps. **No queue attached.** R18 §8. |

**S8 has no PokerStars analogue on purpose.** It is where the venue-browsing instinct goes, so that S1 can
stay a stake lobby. `ui-frontend.md` §8: *a venue browser attached to a queue becomes a venue picker, no
matter how it is labelled.*

---

## 2. UX FLOW

```
                        ┌───────────────────────────────┐
                        │  ARMORY (hub, existing)        │
                        └───────────────┬───────────────┘
                                        │ PLAY
                                        ▼
        ┌───────────────────────────────────────────────────────┐
        │  S1  LobbyRoot                                         │
        │  ruleset tab ─► axes (SIZE · STAKE) ─► queue list      │
        └───────┬───────────────────────┬───────────────┬───────┘
                │ select row            │ edit stake    │ browse
                ▼                       ▼               ▼
        ┌───────────────┐      ┌────────────────┐   ┌──────────────┐
        │ S2 QueueDetail│◄────►│ S3 StakeEntry  │   │ S8 Showcase  │
        └───────┬───────┘      └────────────────┘   └──────┬───────┘
                │ QUEUE                                     │ deep-link
                ▼                                           │ (unfiltered)
        ┌───────────────┐                                   │
        │ S4 TicketReview│  guardrails: cap + session limit  │
        └───────┬───────┘                                   │
                │ CONFIRM ──► escrow (server)                │
                ▼                                           │
        ┌───────────────┐                                   │
        │ S5 InQueue    │──── cancel ──► S1 ◄───────────────┘
        └───────┬───────┘
                │ match found
                ▼
        ┌───────────────┐
        │ S6 MatchFound │  venue revealed here, first time
        └───────┬───────┘
                ▼
             [ MATCH ]
                ▼
        ┌───────────────────────────────────┐
        │ S7 MatchResults                    │
        │  ┌──────────────────────────────┐  │
        │  │ BATTLE AGAIN — Duo, 450 V    │──┼──► S5 directly (skips S1–S4)
        │  └──────────────────────────────┘  │    ⚠ guardrails STILL run
        │  [ Change ] ──► S1                 │
        └───────────────────────────────────┘
```

**The loop that matters is S7 → S5.** `ui-frontend.md` §6: re-queue sits at the highest-frequency moment in
the product and catches peak intent. **It skips S1–S4 as navigation but never skips the guardrail checks** —
R22 is explicit that it is a shortcut through navigation, never through a check.

---

## 3. S1 — LobbyRoot

### 3.1 Layout — the PokerStars skeleton, 1:1

```
┌────────────────────────────────────────────────────────────────────────────────┐
│ [BOLT] IRONICS          ⚡ 12,480 V   ⚡ 3,150 W        ◉ 1,284 online   [◄]   │ A  header
├────────────────────────────────────────────────────────────────────────────────┤
│   ┌──────────────┬──────────────┐                                              │ B  ruleset tabs
│   │  SHOOTOUT    │    TURBO     │                                              │
│   └══════════════┴──────────────┘                                              │
├────────────────────────────────────────────────────────────────────────────────┤
│  SIZE                          STAKE                                           │ C  axis row
│  ○ Solo   ● Duo   ○ Squad      [100][250][500][1000]   or [  450  ]            │
│                                 matching 400–500 V                             │
├───────────────────────────────────────────────┬────────────────────────────────┤
│  QUEUE                POP    EST. WAIT        │  ┌──────────────────────────┐  │
│ ─────────────────────────────────────────────  │  │  S2  QUEUE DETAIL        │  │
│  ▸ Duo · 400–500 V     88 ▮▮      ~40s        │  │                          │  │ D  list + detail
│    Solo · 400–500 V   142 ▮▮▮     ~15s        │  │  (see §4)                │  │
│    Squad · 400–500 V  210 ▮▮▮▮    ~25s        │  │                          │  │
│    Duo · 1000+ V        6 ▯       — quiet     │  └──────────────────────────┘  │
├───────────────────────────────────────────────┴────────────────────────────────┤
│  venue assigned at match start        ┌──────────────────────────────────────┐ │ E  commit bar
│                                        │  QUEUE · Duo · 450 V · ~40s          │ │
└────────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Grid

| Region | Desktop ≥1280 | Notes |
|---|---|---|
| A header | full width, `64px` | Wallet left-of-centre, population right. Mirrors PokerStars' balance + online count. |
| B tabs | full width, `48px` | Two tabs only. **Never a dropdown** — R19 wants them visible and comparable. |
| C axis row | full width, `96px` | **Both axes live simultaneously.** R19: no wizard. |
| D list / detail | `1fr / 420px`, gap `spacing-md` | PokerStars' list-left / card-right split, unchanged. |
| E commit bar | full width, `72px`, pinned | Always reachable. Re-queue speed depends on it. |

### 3.3 Queue row anatomy

```
▸  Duo · 400–500 V          88 ▮▮        ~40s
   └ size · band            └ pop bar     └ est wait
```

**A row is a QUEUE, not an event.** No map name, no venue art, no per-row identity. That is the R18 line and
it is the single most likely thing to drift back toward PokerStars under pressure — event rows are more
visually interesting, and that is exactly why they are tempting.

**Rows are sorted by population, descending.** §5.1: an informed player self-selects toward the populated
option, which shortens waits and self-reinforces. Sorting encodes that without forcing anything.

---

## 4. S2 — QueueDetail

**Reference: Option B** from the supplied comparison — tab row across the top rather than Option D's left
nav. **Reason:** Option D's left rail carries five sections (Overview / Structure / Tables / Chip Graph /
Satellites) because a poker tournament has that much internal structure. **A queue has three:** Overview,
Payouts, Rules. A five-slot nav holding three items reads as unfinished, and the tab row costs less vertical
space — which the payout ladder needs.

```
┌──────────────────────────────────────────────────────────────┐
│ ⚡ SHOOTOUT · DUO                                    [ ✕ ]    │
│ 400–500 V band                                                │
│                                        [ Back ]  [  QUEUE  ]  │
├──────────────────────────────────────────────────────────────┤
│  ┌────────────┐  ┌────────────┐  ┌────────────┐              │  metric row — 3 cards,
│  │ STAKE      │  │ PRIZE POOL │  │ PLAYERS    │              │  PokerStars 1:1
│  │ 450 V      │  │ ~15,390 V  │  │ 88 waiting │              │
│  │ band 400–500│ │ est. 18 pos│  │ 3 paid     │              │
│  └────────────┘  └────────────┘  └────────────┘              │
├──────────────────────────────────────────────────────────────┤
│  Overview  │  Payouts  │  Rules                               │
├──────────────────────────────────────────────────────────────┤
│  ⚑ venue assigned at match start                              │
│                                                               │
│  Last standing · no timer · no respawn                        │
│  Warmup 30s                                                   │
│                                                               │
│  PAYOUTS (18 positions, 3 paid)                               │
│    1st   ~68%  ~11.66×                                        │
│    2nd   ~24%   ~4.04×                                        │
│    3rd    ~8%   ~1.40×  ◄ min cash                            │
└──────────────────────────────────────────────────────────────┘
```

**Every payout figure is prefixed `~` and labelled `est.`** The ladder is a generating rule solved per exact
field size (`economy-store.md` §5.2), and the field is not final until the match starts. Showing a hard
number here would be the same class of promise R20 §4.2 forbids on the stake band.

**The min-cash row is marked.** `economy-store.md` §5.3 fixes it at 1.40× as an input, and it is the number a
player uses to decide whether cashing is worth it.

**⚠ THE EXAMPLE FIELD IS BR_36, AND THE FIELD SIZE IS NOT ARBITRARY.** `economy-store.md` §5.2 carries a design
constraint — **no queue may sit at a field size where the paid-places threshold straddles it** — and names the
thresholds as **N = 10, 14, 21, 27 and 34**. A 36-player field resolves to **36 solo / 18 duo / 9 squad**
finishing positions, none of which is a threshold, and all three appear in §5.2's own spot-check table, so
every figure above is independently checkable. **An earlier draft of this section used a 10-position field —
one of the five — and quoted `~5.36×` for the winner, a pre-R40 30%-depth figure.** The rule gives **11.66× at
18 positions** and **8.10× at 10**. Corrected here; the stale source figure at `economy-store.md` §5.2 was
corrected in the same pass.

**Squad is winner-takes-all, and nothing declares it.** Nine finishing positions is under ten, so `p = 1` and
the whole budget goes to first — **R37's structural point falling out of the arithmetic rather than out of a
mode flag.** S2 renders a single-row ladder in that case. **The Payouts tab is not empty and not disabled**;
that state belongs to TURBO alone (§16.7).

---

## 5. S3 — StakeEntry

**Reference: the Spin & Go tile grid, 1:1** — that layout is already exactly R20's ruling.

```
   PRESETS  (primary)
   ┌───────┐ ┌───────┐ ┌───────┐ ┌───────┐
   │ 100 V │ │ 250 V │ │ 500 V │ │1000 V │      selected = Electric fill + Violet rim
   └───────┘ └───────┘ └───────┘ └───────┘
   
   or enter  [    450    ] V        ◄ secondary, always visible, never hidden behind a toggle
   
   matching 400–500 V               ◄ LIVE, updates as the value changes
```

| Rule | Spec | Ruling |
|---|---|---|
| Presets are primary | Larger targets, first in focus order, first in reading order | R20 |
| Numeric is secondary but **not hidden** | Always visible; not behind a disclosure | R20 |
| **NO SLIDER** | Slow, imprecise by construction, poor on touch, and implies a continuum where the design has bands | **R20 — operator ruling** |
| Band is live | Recomputes on every value change, shown adjacent to the field | R20 §4.2 |
| Band boundary is visible | When within 10% of a boundary, show both bands greyed | prevents "why did I match 500?" |

**Preset values are an open question** (`ui-frontend.md` §14.1) — the four above are placeholders. They must
be round, memorable, and spread across real usage, which is not knowable yet. **Do not harden them.**

---

## 6. S4 — TicketReview — the guardrails screen

**This screen exists because of R23.** Showing potential winnings beside a stake drives engagement, *which is
exactly why the limits ship with it rather than after it*.

```
┌──────────────────────────────────────────────┐
│  CONFIRM ENTRY                                │
│                                               │
│  SHOOTOUT · Duo · 450 V                       │
│  matching 400–500 V                           │
│  venue assigned at match start                │
│                                               │
│  Stake        450 V                           │
│  Balance      12,480 V  →  12,030 V           │
│                                               │
│  ▓▓▓▓▓░░░░░  session   450 / 2,000 V          │  ◄ visible BEFORE it binds
│  cap this entry: 1,248 V max                  │  ◄ the range you have
│                                               │
│         [ Cancel ]     [ CONFIRM ]            │
└──────────────────────────────────────────────┘
```

**Both guardrails are legible before they bind** (`ui-frontend.md` §7): a cap shown at entry frames the range
you have; a cap discovered by rejection teaches you the number you wanted and takes it away. Same for the
session meter — a limit that only announces itself when it triggers arrives as a punishment rather than a
boundary.

**S4 is not skippable, including from re-queue.** R22.

---

## 7. S7 — MatchResults + one-tap re-queue

```
┌──────────────────────────────────────────────┐
│  2nd  of 10                                   │
│  +1,230 V                    balance 13,260 V │
│                                               │
│  ┌────────────────────────────────────────┐   │
│  │   ⚡  BATTLE AGAIN — Duo, 450 V         │   │  ◄ the single highest-value control
│  └────────────────────────────────────────┘   │     in the entire flow
│         [ Change ]        [ Armory ]          │
└──────────────────────────────────────────────┘
```

**The remembered selection is stated in the action, never implied by it.** A bare "PLAY AGAIN" re-queues a
stake the player may not have re-read — unacceptable when the stake is real (R22).

---

## 8. DESIGN TOKENS

Canonical source `IRONICS_UI_STYLE_SSOT.md` §2–§3. **Linear RGB is authoritative; hex is for mockups only.**

| Token | sRGB | Usage in the lobby |
|---|---|---|
| `UI.House.Electric` | `#1E5AFF` | **LEAD.** Selected tab, selected preset fill, QUEUE button, active row |
| `UI.House.Violet` | `#A855F7` | **ACCENT ONLY.** Focus rim, hover edge-glow, selected-tile rim. **Never fill, never text.** |
| `UI.House.Black` | `#05080F` | Panel depth, list backing |
| `UI.House.White` | `#FFFFFF` | Labels, values |
| `UI.House.Blue` | `#00ADFF` | Tertiary — inactive tab, unselected preset |
| `Glass.Bg.Primary` | `rgba(255,255,255,.12)` | Detail panel, metric cards |
| `Glass.Bg.Secondary` | `rgba(255,255,255,.08)` | Nested — payout rows |
| `Glass.Border` | `rgba(255,255,255,.20)` | Panel edge; → Electric when active |
| `Glass.Blur` | 20–40px | Behind panels |
| `Glass.Tint.Danger` | `rgba(255,80,80,.15)` | Session-limit meter at threshold |
| `Text.Primary/Secondary/Tertiary` | 1.0 / .7 / .45 | Values / labels / disabled |

**Geometry:** panel radius 16–24px · button 12px · input 8px.

**THE BLEND RULE (LAW).** Electric = core/fill/active. Violet = rim/edge/focus/hover, **never touching
readable core, fill or text**. The rim is **size-gated: ON at ≥64px, core-dominant at ≤32px.**

> **⚠ CHROME IS NOT IDENTITY** (`ui-frontend.md` §10.2). The lobby is entirely **chrome** — it uses house
> tokens only. Any per-player or per-brand colour (an identity, a finish) **must resolve through the identity
> registry**, never from these tokens. Three distinct blues exist with three distinct owners — the Volts
> currency blue `#1E5AFF`, the free base identity `#00ADFF`, and the house default identity `#5090FF`.
> **Conflating them is the classic failure here**, and it is invisible on any single screen.

**Typography:** Display = techno-sans, **ALL-CAPS** (tab labels, screen titles). Body = clean sans, sentence
case. **Numeric = tabular mono** — mandatory for stake, balance, population, wait and payout, so digits do not
jitter as they tick.

---

## 9. COMPONENTS

| Component | Variants | Key props | Notes |
|---|---|---|---|
| `AFLW_Lobby_RulesetTab` | active · inactive · disabled | `RulesetId`, `bHasPopulation` | Disabled when a ruleset has no code (TURBO). |
| `AFLW_Lobby_AxisSelector` | radio (size) · tile-grid (stake) | `AxisId`, `Options[]`, `Selected` | One parameterised component, two skins. |
| `AFLW_Lobby_QueueRow` | default · hover · selected · **cold** | `Size`, `BandLo`, `BandHi`, `Population`, `EstWaitSec`, `bCold` | **Cold is a first-class variant, not a disabled state** — §5.2. |
| `AFLW_Lobby_DetailPanel` | — | `QueueTicketPreview` | Hosts the tab row. |
| `AFLW_Lobby_MetricCard` | — | `Label`, `Value`, `SubValue` | The 3-card row. PokerStars 1:1. |
| `AFLW_Lobby_StakeEntry` | — | `Presets[]`, `Value`, `BandLo`, `BandHi` | Owns the live band readout. |
| `AFLW_Lobby_PayoutLadder` | — | `Positions`, `PaidPlaces`, `Rows[]` | Marks the min-cash row. |
| `AFLW_Lobby_GuardrailMeter` | under · near · **at limit** | `Current`, `Limit` | Danger tint only at limit. |
| `AFLW_Lobby_CommitBar` | ready · disabled · **queued** | `Summary`, `EstWait` | Pinned. |

**All are `UCommonActivatableWidget` children pushed to `UI.Layer.Menu`**, with the **C++ base owns bindings /
WBP child owns layout** split (`ui-frontend.md` §12.1).

---

## 10. STATES AND INTERACTIONS

| Element | State | Behaviour |
|---|---|---|
| Ruleset tab | hover | Violet rim fades in, 120ms |
| Ruleset tab | active | Electric underline 3px + fill @12% |
| Ruleset tab | no population | Label + count, **not greyed out** — R21 §5.2: an empty band must look empty, not disabled |
| Queue row | hover | Glass.Bg.Secondary + Violet left edge 2px |
| Queue row | selected | Electric fill @12%, Electric left edge 3px |
| Queue row | **cold** | Population pill `▯`, wait reads `— quiet`, row at 60% opacity. **Still selectable.** |
| Preset tile | selected | Electric fill + Violet rim (rim ≥64px only) |
| Numeric field | invalid | Danger tint border, band readout → `outside all bands` |
| QUEUE button | disabled | 40% opacity + reason string beneath. **Never a silent no-op.** |
| QUEUE button | pressed | 120ms scale 0.98 → escrow request |
| Session meter | at limit | Danger tint, CONFIRM disabled, reason stated |

---

## 11. RESPONSIVE

| Breakpoint | Changes |
|---|---|
| **Desktop ≥1280** | Default. List + detail side by side. |
| **Tablet 768–1279** | Detail panel becomes a right drawer over the list. Axis row unchanged. |
| **Mobile <768** | **Vertical stack: tabs → axes → list.** Detail becomes full-screen push. Commit bar stays pinned to the bottom. |

> **⚠ THE MOBILE CONSTRAINT, and it is a ruling not a preference.** `ui-frontend.md` §14.3: capability must
> **not** be reduced on mobile — the split model must survive. **What must not happen is the axis row
> collapsing behind a disclosure**, because that silently converts R19's split layout into the stepped flow
> R19 exists to forbid. If vertical space forces a choice, the QUEUE LIST scrolls; the axes stay visible.

**B4** makes PC + console + mobile a shipping requirement, so all three input models are first-class:
**pointer** (hover meaningful), **gamepad** (every control reachable by directional focus), **touch** (thumb-
sized targets, **no hover-only information**, no small drag targets — which is a second reason there is no
slider).

---

## 12. EDGE CASES

| Case | Behaviour |
|---|---|
| **No queue has population** | Show all rows as cold with a single line: *"Queues are quiet right now."* **Never hide rows** — R21 §5.2, a queue that accepts a player and never returns is indistinguishable from a broken one. |
| **Stake outside every band** | Numeric field shows `outside all bands`; QUEUE disabled with reason. Nearest band offered as a one-tap correction. |
| **Balance below minimum stake** | Presets above balance render at 40% with a lock glyph. **Route to the store, not an inline purchase prompt** — §9.4's reasoning applied here. |
| **Session limit reached** | CONFIRM disabled at S4, meter in danger tint, plain-language reason. Lobby stays browsable. |
| **Population data unavailable** | Show `—` and *"population unavailable"*. **Never render 0** — zero is a claim, absence is not. |
| **Est. wait unknown** | `~ —`. Never fabricate an optimistic figure (R21). |
| **Long localised strings** | Row labels truncate with ellipsis and a tooltip; **numeric fields never truncate.** |
| **Disconnect while queued** | S5 → reconnect banner; escrow is server-held so the ticket survives. Match id is the binding key (`matchmaking.md` §9.1). |
| **Match found while in S3** | Cannot occur — queueing requires S4 confirm. |

---

## 13. MOTION

**Luminous restraint: glow only where it means something** — a threshold, a confirm, an objective.

| Element | Trigger | Animation | Duration | Easing |
|---|---|---|---|---|
| Ruleset tab | select | Underline slides between tabs | 180ms | ease-out |
| Detail panel | row select | Fade + 8px rise | 160ms | ease-out |
| Preset tile | select | Fill wipe L→R + rim fade | 140ms | ease-out |
| Band readout | value change | Crossfade | 100ms | linear |
| Population pill | count change | Tick, no bounce | 200ms | ease-out |
| QUEUE button | press | Scale 0.98 → 1.0 | 120ms | ease-out |
| S5 spinner | queued | Continuous arc, **Electric core / Violet rim** | 1200ms loop | linear |
| S6 venue reveal | match found | Card scale 0.94 → 1.0 + blur-in | 320ms | ease-out |
| Session meter | crosses threshold | Single pulse, **once** | 240ms | ease-in-out |

**Nothing loops decoratively.** A pulsing element that is not communicating a state change spends the one
signal the style has for "this matters".

---

## 14. ACCESSIBILITY

**Focus order (S1):** wallet → ruleset tabs → size radio group → stake presets → numeric field → queue list →
detail panel → commit bar.

- **Every surface is fully navigable by directional focus.** A control reachable only by pointing is
  unreachable on console (`ui-frontend.md` §12.2).
- **Each screen declares its own default focus target, resolved BY NAME, never a compile-time bind** — §12.2:
  a hard binding turns a layout edit into a compile break; an optional one turns it into an unfocusable screen
  on gamepad.
- **No information exists only in a hover state.** Hover does not exist on touch.
- Queue rows are a `listbox`; each row announces *"Duo, 400 to 500 Volts, 88 players waiting, estimated wait
  40 seconds"*.
- Stake presets are a `radiogroup`; the band readout is `aria-live="polite"` so a change is announced without
  interrupting.
- The session meter is `role="meter"` with min/max/current.
- **Cold queues announce their state**, since the visual cue (opacity) is not available to a screen reader.
- Contrast: all text meets 4.5:1 against glass over the dark base. **Violet is never used for text** — its
  role is rim only, and it fails contrast at body sizes.

---

## 15. INTERFACES

**CONSUMES** (`ui-frontend.md` §13.1) — the front end computes none of these:

| From | What |
|---|---|
| Matchmaking | Queue set (open rulesets/sizes/bands) · band boundaries for an entered value |
| Matchmaking | Population + estimated wait **per band**, including an explicit *no data* state |
| Economy | Wallet balance (**read-only**), guardrail limits |
| Economy | Payout preview from the §5.2 generating rule |
| League | Standings for the profile chip |
| Character | Saved loadout **references** (§9.3) |

**OWES — the queue ticket** (§13.2), and nothing else:

```
ruleset · size · stake · party identity
```

> **NO VENUE ON THE TICKET.** A ticket carrying a venue re-introduces the map-browser model through the
> interface even if the UI never showed a picker. **It is a request, not a reservation**, and every value —
> especially the stake — is re-validated server-side on receipt (**N11**: the client never decides currency).

---

## 16. OPEN QUESTIONS THIS DOCUMENT DOES NOT CLOSE

Carried from `ui-frontend.md` §14 and `economy-store.md` §15 — **do not harden any of these in implementation**:

1. **Preset tier values** (§14.1) — the four in §5 are placeholders.
2. **Numeric precision: free entry or snap** (§14.2).
3. **Mobile layout under the split model** (§14.3) — §11 states the constraint, not the solution.
4. **Cold-band presentation before it folds** (§14.5) — §12 gives a first pass.
5. **The rake rate** (`economy-store.md` §15.7) — every payout figure shown in S2 depends on the 5% working
   assumption.
6. **The winner's-multiple sawtooth** (`economy-store.md` §15.9) — S2's payout preview will show a ~17–23%
   step at N = 14, 21, 27, 34. Whether to smooth or expose it is unruled.
7. **TURBO's payout shape** (`match-modes.md` §9.3) — TURBO has no placement ladder, so S2's Payouts tab has
   no defined content under that ruleset. **The tab is disabled, not empty**, until it does.
