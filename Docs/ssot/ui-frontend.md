# SSOT — UI / FRONT END (Tier 2)

**What this is:** what the front end, the in-match HUD and the UI style system **are** and **why**. It changes
when the system is redesigned.
**What this is not:** a status board. **This document contains no status claims** — nothing here says what is
built, proven, done or owed, no wash-queue entries appear, and no commit hash appears as evidence of progress.
Those belong to Tier 3 (`LIVE_TRACKER`).

**Doctrine is cited, never restated.** Laws live in [`Docs/DOCTRINE.md`](../DOCTRINE.md) and are referenced by
id (**A5**, **B4**, **C1**, **N11**, **NM5**, **NM6**, **X20**).

---

## 1. SCOPE

This SSOT governs: what the front end is **for** · the stake lobby and its two axes · stake entry ·
population transparency · re-queue · guardrails · the map showcase as a separate surface · the loadout
surface · the house style system and what makes a surface conformant · the CommonUI architecture and input
routing · and the interfaces the front end exchanges with every other system.

It does not govern: how matches are actually formed (→ `ssot/matchmaking.md`), what a ruleset is
(→ `ssot/match-modes.md`), prices, currencies or entitlement (→ `ssot/economy-store.md`), what a character
save contains (→ `ssot/character-system.md`), or the state of any surface (→ Tier 3).

---

## 2. THE FRONT END IS A STAKE LOBBY, NOT A MAP BROWSER

> **The player chooses two things: MATCH SIZE and STAKE AMOUNT. Venue is a server outcome.**

This is the doc's organising decision, and every layout choice below follows from it.

### 2.1 Why these two axes and not others

**A map browser answers a question the player is not asking.** The player sitting in the front end wants a
match — of a certain shape, for a certain amount. Which venue it happens in is not a decision they need to
make in order to want to play, and `ssot/matchmaking.md` **D1** settles that it is not a decision they get.
The reasoning for that is recorded there and is **not restated**; what matters here is the UI consequence:

**Every axis the front end offers is an axis it must then honour.** A surface that presents a venue picker
has promised the player that venue, and either the matchmaker must deliver it — fragmenting the queue by map
— or the UI has lied. **The front end's axes and the matchmaker's queue dimensions are the same list**, and
they diverge only in the direction of a broken promise.

**Size and stake are the two axes because they are the two things a player's answer actually changes:** what
kind of match it is (size), and what it is worth (stake). Everything else is either a consequence of those or
a server concern.

### 2.2 Venue disclosure

**The venue is disclosed as an outcome, not offered as an input:** *"venue assigned at match start."*

Three properties of that disclosure:

- **It is stated up front, not discovered.** A player who is not told is not being spared a decision — they
  are being set up to be surprised, and a surprise about what they are getting is a trust cost the front end
  cannot afford at the moment it is asking for a stake.
- **It is not framed as a limitation.** Assignment is what keeps the population in one pool (§5). Presenting
  it as "you don't get to choose" reads as a missing feature; presenting it as an assignment reads as a
  format — the same information, and the second is true.
- **The venues remain browsable, elsewhere** (§8). The player who wants to see the maps has a place to do
  that; it just is not the place where they queue.

---

## 3. SPLIT LAYOUT, NOT A STEPPED FLOW

> **Both axes are live and editable at once. There is no wizard.**

### 3.1 Why split beats stepped, specifically for this product

A stepped flow (choose size → next → choose stake → next → confirm) is defensible for a **one-time**
decision. This is not one.

**Staked players re-queue constantly, and they change one variable — usually the stake.** That is the actual
loop: play a match, come back, adjust, go again. Against that loop:

- **A stepped flow makes the player re-walk the entire path to change one number.** The cost is paid on every
  single re-queue, forever, and it is paid most by the most engaged players — the ones who queue most.
- **A split layout makes the change a single interaction** and the re-queue one tap (§6).
- **A stepped flow also hides the relationship between the axes.** Stake and size interact — a band's
  population depends on both (§5) — and a flow that shows them on separate screens makes the player hold that
  relationship in their head instead of reading it off the screen.

**The general rule:** *step a flow only when the steps are genuinely sequential.* Size and stake are
independent; presenting them sequentially adds an ordering that does not exist in the decision.

### 3.2 TIER is the FIRST choice; ruleset is the second

> **LEAGUE PLAY / WATTS PLAY / VOLTS PLAY sits ABOVE the ruleset tabs (R76, R85).** It is the first thing a
> player picks, because it is the question with the largest consequence: *what am I playing for.* Everything
> below it is scoped by that answer — the stake presets are different ladders in the two staked tiers (R80,
> R88), and **the league control appears in LEAGUE PLAY and nowhere else**, because staked play is PRO MOD only
> (R86).
>
> **This is what closes §3.2.1's four-dimension problem.** The lobby never shows four controls at once: pick
> LEAGUE PLAY and you get league + ruleset + size, with no stake; pick a staked tier and you get ruleset + size
> + stake, with no league. **Three controls either way**, and nothing is set silently elsewhere.
>
> **The precedent is exact and the players already know it.** Every card room and every brokerage puts the
> play-money / real-money choice at the top and never mixes the two lists. Putting it lower would let a player
> configure an entire entry and discover the denomination last, which is the one thing that must never surprise
> them about money.

**MATCH PLAY and BATTLE ROYALE are two tabs below the tier choice — not a doubled flat list.**

`ssot/matchmaking.md` **R7** settles that the rulesets split the queue and get two tabs. The UI reasoning:

- **The rulesets are different products, not options within one.** They differ in win condition, respawn
  behaviour, match length **and position count** (`ssot/match-modes.md` §2). Flattening them into one list of size-and-stake
  combinations would present two products as variants of each other.
- **A flat list doubles in length and halves in legibility.** Every size × every stake × two rulesets is a
  grid the player has to filter mentally. Tabs remove the ruleset dimension from the grid entirely.
- **A tab is also the honest place for population reporting** (§5): a ruleset's population is a property of
  the ruleset, and a tab is where a player can see one is quiet before committing attention to it.

### 3.2.1 ~~THE LOBBY DOES NOT RENDER EVERY QUEUE DIMENSION~~ — RESOLVED BY R85/R86

**§2.1 rules that *"the front end's axes and the matchmaker's queue dimensions are the same list."* That is
currently FALSE, and R76 makes the gap wider before it makes it narrower.**

| Queue dimension | Rendered in the lobby? |
|---|---|
| Ruleset (MATCH PLAY · BATTLE ROYALE) | Yes — tabs (§3.2) |
| Bracket / size | Yes — the size axis (§3) |
| Stake amount | Yes — presets + numeric (§4) |
| **Tier / stake currency** (R76, R85) | **Yes — the first control** (§3.2) |
| **League (HAYWIRE · PRO MOD)** | **Yes, as of R86 — but only inside LEAGUE PLAY**, the only tier offering both (§3.2) |

**The league gap is pre-existing, not created here.** `ssot/matchmaking.md` §4.2 counts leagues as a **×2
multiplier** on the queue set and §2.2 lists league among the things *"the player does control"* — but §3.3's
shape below has no league control, and `design/IRONICS_LOBBY_UX_HANDOFF.md` contains **zero** occurrences of
HAYWIRE or PRO MOD. The lobby has been specified against three of four dimensions.

**Why this had to be closed before S1 was authored, not after:** a screen that cannot express a dimension the
matchmaker splits on will either silently pick one for the player — which §2.1 calls *"a broken promise"* — or
force the queue set to collapse a dimension it has already committed to. **Both are worse than a fourth
control.**

> **⚠ RESOLVED, AND BY REMOVING THE PROBLEM RATHER THAN RENDERING IT.** This section recorded a real defect:
> the lobby had never shown the league dimension that `ssot/matchmaking.md` §4.2 counted as a ×2 multiplier, so
> §2.1's *"the front end's axes and the matchmaker's queue dimensions are the same list"* was false. **R86 made
> league tier-scoped**, so it now appears exactly where it exists and nowhere it does not — §2.1 holds again,
> and no tier ever shows more than three controls. Kept rather than deleted because the defect stood for a long
> time, and its shape — *a queue dimension no screen rendered* — is the failure to watch for whenever a
> dimension is added.

### 3.3 The resulting shape

```
┌─ PLAY ─────────────────────────────────────────────────┐
│  [ MATCH PLAY ]  [ BATTLE ROYALE ]    ← ruleset tabs    │
│                                                         │
│  SIZE                    STAKE                          │
│  ┌──────────────────┐    ┌───────────────────────────┐  │
│  │ ○ Solo    142 ▮▮ │    │ [100] [250] [500] [1000]  │  │
│  │ ● Duo      88 ▮  │    │  ▸ or enter: [  450  ]     │  │
│  │ ○ Squad   210 ▮▮▮│    │  matching 400–500 V        │  │
│  └──────────────────┘    └───────────────────────────┘  │
│                                                         │
│  venue assigned at match start                          │
│  ┌───────────────────────────────────────────────────┐  │
│  │  QUEUE  ·  Duo · 450 V  ·  est. wait ~40s         │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

Both axes are visible, both are live, the band is stated, the population is on the same screen, and the
action is one control. **Nothing here is behind a step.**

---

## 4. STAKE ENTRY — PRESETS PRIMARY, NUMERIC SECONDARY, NO SLIDER

### 4.1 The three input options, and why the ruling lands where it does

> **Presets are the primary control. An editable numeric field is the secondary. There is no slider.**

| Method | Verdict |
|---|---|
| **Presets** — a small row of tiers | **PRIMARY.** One tap, no ambiguity, and it covers what most players want most of the time. It also *teaches the ladder* — a new player learns the meaningful stake tiers by seeing them |
| **Numeric field** — type a value | **SECONDARY.** Exact, fast for a player who knows their number, and the correct affordance for a value outside the presets. Present but not primary |
| **Slider** | **REJECTED (operator ruling)** |

**Why the slider is rejected, concretely:**

- **It is slow.** Dragging to a value takes longer than tapping a preset *and* longer than typing — it is the
  worst option on the axis it appears to optimise.
- **It is imprecise by construction.** A slider maps a continuous drag onto a value range, so hitting an exact
  figure requires fine motor control that scales with the range. Widen the stake range and precision gets
  worse; players end up at 447 when they meant 450.
- **It is poor on mobile** (**B4** makes mobile a shipping target, not an afterthought). A slider on a touch
  screen puts the player's own thumb over the value they are trying to read, and small drag targets are the
  least reliable touch interaction there is.
- **It implies a continuum where the design has tiers.** The stake bands (§4.2) are discrete. A slider tells
  the player every value is meaningfully different when the matcher is going to band them anyway.

### 4.2 The matching band must be visible

> **The band the stake will match into is displayed — "matching 400–500 V". A player must never believe an
> exact figure will be returned.**

`ssot/matchmaking.md` **D3** establishes that stake is a ticket parameter with banding. **This is the UI half
of that decision, and it is not optional.**

**The failure it prevents:** a player enters 450, is matched against 500, and concludes the system ignored
their input or cheated them. Nothing went wrong — banding is the design — but the interface allowed them to
form a belief the system was never going to honour. **An interface that accepts an exact number without
qualifying it has made a promise on the matcher's behalf.**

**Two properties follow:**

- **The band updates live as the value changes**, so the player sees which band they are in *before*
  committing, and can see the boundary they are near.
- **The band is stated in the same place as the value**, not in a help text or a tooltip. A qualification the
  player has to go looking for is a qualification that was not made.

---

## 5. POPULATION TRANSPARENCY IS A REQUIREMENT

> **Live counts and estimated wait, per size and per stake band, on the queue surface.**

`ssot/matchmaking.md` **D6** establishes population transparency as a design commitment. **This section states
what that obliges the UI to do**, because transparency that is not rendered is not transparency.

### 5.1 It is the UI-side fix for fragmentation

Every axis the front end offers divides the population (§2.1). Size and stake are two axes that genuinely
must exist — so fragmentation is not avoidable, only **manageable**. The management mechanism is
**information**:

- A player who can see that Duo has 88 players waiting and Squad has 210 **self-selects toward the populated
  option** when they are indifferent. The UI does not force anything; it just stops hiding the thing that
  would have made the choice easy.
- **This is a herding effect deliberately harnessed.** Left uninformed, players distribute across bands by
  preference alone and every band is thinner. Informed, they concentrate — which shortens waits, which makes
  the concentration self-reinforcing.
- It costs nothing and needs no matchmaking change. **It is the cheapest fragmentation mitigation available**,
  and it is purely a rendering decision.

### 5.2 It is also the honest option

> **An empty band must LOOK empty rather than silently never matching.**

A queue that accepts a player into a band with no population and then simply never returns a match has
behaved indistinguishably from a broken queue. The player waits, concludes matchmaking is broken, and is
**correct in every respect except the cause**.

Showing the band as empty converts a defect-shaped experience into an informed choice: *this band is quiet,
pick another or wait.* The player who then waits is waiting deliberately, which is a completely different
experience from the same wait imposed without explanation.

**Consequences for the display:**

- **Counts and waits are shown per band, not only in aggregate.** A healthy total conceals a dead band, and
  the dead band is exactly the one a player needs warning about.
- **Estimated wait is presented as an estimate**, with a shape that degrades honestly — a band with no data
  says so rather than reporting an optimistic number.
- **A cold band is never presented identically to a busy one.** How a cold band is presented *before* it folds
  is an open question (§14.5); that it must be visually distinct is not.

---

## 6. ONE-TAP RE-QUEUE

> **Remember the last selection. Offer "BATTLE AGAIN — <size>, <stake>" as a single action after a match.**

**This is the highest-leverage satisfaction lever in the whole flow.** The claim is worth defending, because
it is a small feature:

- **It sits at the highest-frequency moment in the product.** Every match ends. Whatever the player meets at
  that moment, they meet more often than any other screen — more often than the store, the loadout, or the
  creator. A one-tap improvement there compounds across every match ever played; the same effort spent on a
  once-per-session surface does not.
- **It catches the player at peak intent and spends none of it.** Immediately after a match is when the player
  most wants another one. Every interaction between that impulse and the next match is a chance for it to
  decay — and a stepped path back through size and stake (§3.1) spends the intent on navigation.
- **It removes the re-decision, which is the real cost.** Re-selecting the same size and stake is not merely
  slow; it re-opens a decision the player already made and was happy with. Most re-queues are *the same
  again*, and the interface should treat that as the default rather than as one option among many.
- **It makes the split layout pay off.** §3's design exists so that changing one variable is cheap. Re-queue
  is the case where changing *nothing* should be cheapest of all.

**Design properties:**

- **The remembered selection is stated in the action, not implied by it.** "BATTLE AGAIN — Duo, 450 V" is a
  single tap *and* full disclosure. A bare "PLAY AGAIN" re-queues a stake the player may not have re-read,
  which is unacceptable when the stake is real (§7).
- **Changing the selection is always one step away** — the action never traps the player into repeating.
- **The guardrails still apply** (§7). One-tap re-queue must not become a way to bypass a cap or a session
  limit; it is a shortcut through navigation, never through a check.

---

## 7. GUARDRAILS ARE DESIGN, NOT AN AFTERTHOUGHT

**Showing potential winnings beside a stake drives engagement — which is precisely why the limits belong in
the design from the start.**

The honest statement of the situation: an interface that displays what a player stands to win next to the
control that sets their risk is an interface **designed to increase stakes**. That is a legitimate product
decision, and it is also exactly the condition under which a player can talk themselves into a stake they
should not take. **The feature and its guardrail are the same design decision, and separating them into
"build now / add later" is how the later half does not happen.**

Two guardrails belong in the front end from the first version:

| Guardrail | What it does | Why it is UI, not only backend |
|---|---|---|
| **Stake cap relative to balance** | Bounds a single stake as a proportion of what the player holds | The cap must be **visible at the moment of entry**, not enforced as a rejection afterwards. A rejection teaches the player the number they wanted; a visible cap frames the range they have |
| **Session loss limit** | Bounds cumulative loss within a session | The player must be able to **see where they are against it**. A limit that only announces itself when it triggers arrives as a punishment rather than a boundary |

**Both must be legible before they bind.** A guardrail discovered by hitting it is experienced as the product
taking something away. The same guardrail shown in advance is experienced as the product having a shape — and
the difference is entirely presentation.

**Wider counsel framing.** `ssot/economy-store.md` §13 records the pending-counsel-review posture around
stakes and prizes. **That framing is cited, not re-argued.** What this section adds is only the UI obligation:
whatever the review concludes, the *interface* is where a limit becomes real to a player, so the surface must
be built able to express one.

---

## 8. THE MAP SHOWCASE IS A SEPARATE SURFACE

> **Venues are browsable — with the art, the layout, the identity — on a surface with NO queue attached.**

**Why the separation is what lets the lobby stay a stake lobby:**

- **A venue browser attached to a queue becomes a venue picker**, no matter how it is labelled. If a player can
  see maps *and* queue on the same surface, the natural reading is that the maps are choices — and either the
  matchmaker honours that (fragmenting the pool, §2.1) or the surface disappoints.
- **Detached, the same content is pure upside.** The maps are worth showing: they are among the most
  expensive things the project makes, they carry the game's identity, and a player who has seen them
  recognises them in play. **The problem was never showing maps; it was showing maps next to a queue button.**
- **It keeps each surface answering one question.** The lobby answers *what match do I want*. The showcase
  answers *what is this game*. A surface answering both answers neither cleanly — and the lobby is the one
  that degrades, because the showcase content is far more visually interesting than two axis controls and
  will dominate the screen.

**Design properties:** the showcase carries no queue affordance and no venue selection that persists into
matchmaking; it may deep-link *to* the lobby, but only to the lobby as it is — never pre-filtered by venue,
because a pre-filter is a venue choice wearing a different name.

---

## 9. THE LOADOUT AND STORE SURFACE

### 9.1 One chassis, two modes — the market model

> **The store and the loadout are TWO MODES OF ONE SHARED CHASSIS, not two screens.
> STORE = purchasable + BUY. LOADOUT = owned + EQUIP.**

The two surfaces do the same *thing* — present a filtered grid of catalog entries with tabs, over a live
preview — and differ in exactly two places: **which entries they list**, and **what a tile does when tapped**.

**Why one chassis is the right factoring:**

- **The duplicated part is the expensive part.** List, tabs, filtering, focus handling, layout, tile visuals
  and the live-preview integration are the bulk of the work and are identical in both. Authoring them twice
  produces two implementations that drift — and the drift shows up as the store and the locker feeling like
  different products, which is the opposite of what an owned-item flow wants.
- **The differing part is small and cleanly separable.** *What is listed* is a filter over the same catalog;
  *what a tile does* is one action. Both are parameters, not architecture.
- **It keeps the §9.4 content rule intact rather than weakening it.** Sharing a chassis is not the same as
  merging the surfaces: the **modes stay separate**, so the loadout still lists only owned items and the store
  still owns buying. **The separation is in the mode, not in the code** — and putting it in the mode is what
  makes it cheap enough to actually hold.

**Two structural properties of the override:**

- **Extend by pointing at a different tile, not by editing the shared graph.** The loadout mode supplies its
  own tile class through a per-item hook rather than replacing the chassis's default entry widget. The store
  path is left byte-identical, and the loadout tile is owned end to end — readable, bindable, and not
  dependent on opaque hooks into someone else's graph.
- **Modes are decoupled from any in-match equivalent.** A front-end loadout mode and an in-match loadout share
  *vocabulary* — the tile and the axis enum — and nothing else. Shared vocabulary is the correct amount of
  coupling between a menu surface and a gameplay surface; a shared implementation would tie a front-end layout
  change to in-match behaviour.

**The surface renders over the live scene.** The market sits over the hub's 3D armory with its own backdrop
dissolved when a display pawn is behind it, so the preview *is* the scene rather than a separate viewport.
This is §9.2's preview-is-the-product principle achieved structurally: there is only one object, so it cannot
diverge from what spawns.

**Category tabs filter the same list** rather than fetching different ones — a tab is a namespace filter over
entries already held. One source, N views: the tab set can change without changing what feeds it.

### 9.2 What it must expose

The loadout is where owned cosmetics become **selectable**, replacing any developer-only selection path.
It exposes one row per **independent cosmetic axis** (`ssot/economy-store.md` §6): identity, weapon, weapon
skin, beam, colour, mask/special — plus the sticker/emblem selection (§9.3).

**Three structural properties:**

- **One parameterised picker serves every axis.** Axis in → owned grid → equip out. Authoring a bespoke
  screen per axis multiplies the work by the axis count and guarantees the axes drift apart visually.
- **A live shared preview, not per-tile 3D.** One preview object re-applies the selection. Per-tile 3D
  multiplies cost by grid size for a benefit a thumbnail already delivers.
- **The preview is the product** — same object, same materials, same resolution path as the pawn that spawns
  (`ssot/character-system.md` §8.4). A preview that diverges from what spawns is a bait-and-switch discovered
  in the first match.

### 9.3 The loadout resolves references — it never reads a baked composite

> **A loadout must resolve the character's stored REFERENCES and re-check entitlement at selection time. It
> must never read a baked composite as its source of truth.**

`ssot/character-system.md` §8.2 establishes the reference rule for saved characters. **The loadout is the
surface where that rule is either honoured or broken**, because the loadout is where a sticker is swapped.

- **The swap surface is the loadout, not the creator.** Changing a sticker must not require re-entering
  character creation — it is an equip-time decision like every other owned cosmetic.
- **The loadout reads `(sticker id, zone, transform)` and resolves each id through the catalog and
  entitlement**, exactly as it resolves every other axis. A newly-purchased sticker is therefore immediately
  offered on every saved character.
- **The composite is a derived artefact the loadout triggers, never an input it reads.** If the loadout read
  the bake, the bake would be the source of truth — and a player who buys a sticker could not apply it to an
  existing save, which is the precise failure the reference rule exists to prevent.
- **Entitlement is re-checked here, server-side** (**N11**). Ownership can change between save and equip, so
  a stored reference is a claim to be validated, never a grant already made.

### 9.4 Owned-only, with one route out

**The loadout shows what the player owns.** Unowned items are not greyed inline; a single "get more" affordance
per axis deep-links to the store.

**Why:** greying out desirable options inside an authoring surface converts a creative act into a sales pitch
at the moment the player is most invested. A browse-everything surface is a legitimate and *separate* thing —
it belongs with the store, not in the locker, for the same reason the map showcase is separate from the lobby
(§8): **a surface that both lets you use what you have and advertises what you don't will be dominated by the
advertising.**

---

## 10. THE STYLE SYSTEM — THE HOUSE STANDARD

**Every UI surface inherits from the house style — in-match HUD and front end alike. No surface defines its
own palette, type ramp or chrome.** The style authority is `IRONICS_UI_STYLE_SSOT.md`; its rules are the house
standard and are cited here rather than duplicated.

### 10.1 The palette is a ROLE palette

The house chrome is **four colours with one job each**: a **lead** (fill, active, selected), an **accent**
(edge-glow, focus, hover — never core, fill or text), a **depth** (backing and contrast), and a **text**
colour.

**The role assignment is the rule, not the hex values.** A palette where every colour can do every job is not
a palette; it is a set of colours, and surfaces authored against it drift immediately because nothing
constrains the choice. **The blend rule** — lead as the core, accent as the rim — is what keeps the identity
consistent across surfaces authored at different times by different hands.

The accent is **size-gated**: on above a threshold size, dropped in favour of the core below it, because a
rim on a 16-pixel mark is noise rather than identity.

### 10.2 Chrome is not identity — the two colour systems never mix

> **CHROME is the app's own furniture and is the same for every player. IDENTITY is what a player picked or a
> brand owns, and is resolved by tag through the registry.**

This is the single most drift-prone rule in the UI, so it is stated as a prohibition:

- **A widget tinting a per-item colour resolves it through the identity registry**, never from the chrome
  tokens.
- **Chrome tokens never enter the identity registry.** A currency colour is furniture; it is not something a
  player can own or select.
- **Colours that look alike are not interchangeable.** Several distinct blues exist with distinct owners — a
  currency blue, a free base identity blue, a house default identity blue. **Conflating them is the classic
  failure here**, and it is invisible on any single screen: it only shows up as the same colour meaning two
  different things in two places.

**One value, one home.** A token duplicated into a second file to serve a second consumer is the drift
mechanism itself — two places holding what should be one value, diverging silently.

### 10.3 The readability law governs combat overlays

- **HUD and marker hues stay OFF the beam hues.** The HUD has to read *against live fire*; a marker in a beam
  colour disappears exactly when it matters. This is "no green on green" applied to UI.
- **Team identification is outline/rim plus nameplate — not HUD body colour, not post-process.** The HUD shows
  team through the score header and nameplates; **it does not recolour the world.** Recolouring the world for
  team read fights the map's own lighting and the weapon FX simultaneously.
- **The accent is brand chrome, not a combat marker hue.** On solid combat-overlay surfaces it stays a thin
  edge, never a fill.
- **Menu chrome may carry team identity; solid combat overlays may not** — the header does not sit over the
  beam, and the reticle does.

### 10.4 Copy and vocabulary are law

- **Currency names are exact and consistent.** One spelling, everywhere.
- **Never show real-world currency in-match, and never imply value converts to money.** This is the same
  no-cash-out boundary `ssot/economy-store.md` draws, expressed as a copy rule — and copy is where it is most
  easily violated by accident.
- **Internal names never appear in a player-facing string.** The code prefix and the legacy project name are
  internal only (**NM5** keeps them stable in code precisely so they never need to surface).
- **Numbers shown to players are integers**, in a tabular face, so they do not jitter as they change.

### 10.5 Component tokens

Reusable primitives — glass panel, meter rail, channel bar, banner, score header, world marker, button, value
chip, rarity badge — exist so a new surface is **assembled**, not authored. Each has one definition and one
backing widget.

**The accent-state variant applies to every token identically:** focus, hover and selected add the accent as
an **edge state layer**, never as the resting fill. This is what makes state read the same on a HUD meter and
a store tile without either surface knowing about the other.

**Glow only where it means something** — a threshold, a confirm, an objective. Glow applied for decoration
spends the one signal the style has for "this matters", and once spent it cannot be recovered on the surface
that actually needs it. **C1** is the parent principle: feel first, decoration never at its expense.

---

## 11. WASH STANDARDS — WHAT MAKES A SURFACE CONFORMANT

**The standards are Tier 2 and live here. The list of surfaces still to be washed is Tier 3 and does not.**

### 11.1 Every element resolves to one of three buckets

| Bucket | Meaning |
|---|---|
| **RETHEME** | The element's shape is correct; its colour is not. Recolour to the house palette |
| **REPLACE** | The element's *shape* carries foreign identity — a logo, a mark, a silhouette. Recolouring it is not enough; the asset is replaced |
| **SKIP** | The element is not player-facing (debug text) or belongs to a surface players never reach (hidden stock content). Explicitly out of scope, and recorded as such rather than left ambiguous |

**Why the RETHEME/REPLACE split matters:** recolouring a foreign logo produces a foreign logo in house
colours. It passes a palette audit and fails the actual goal, which makes it worse than leaving it alone —
it is a defect that has been marked as fixed.

### 11.2 Fix at the safe level — never at the master

> **Recolour at the intermediate instances. Never edit a shared master for a chrome sweep.**

Shared UI masters are **cross-inheritance hazards**: the same master can feed both menu chrome *and* the
gameplay HUD. Recolouring a master to fix a menu therefore re-tints team read and combat overlays, breaking
§10.3 — and the breakage appears in a surface nobody was editing.

**The instance level is the safe leverage**: it cascades to the intended surfaces and reaches nothing else.

**Sequence the work accordingly:** map the inheritance cascade **first**, then sweep in batches at the instance
level. A sweep that begins before the cascade is mapped is a sweep that will discover its own blast radius by
causing it.

### 11.3 Trace the colour to its real source

**A widget tint does not cover a baked-in material colour.** A control that *looks* wrong is usually wrong in
its material instance, not in its brush tint. **Tinting blind produces a widget with a correct tint over an
incorrect material** — and the visible result barely changes, so the fix reads as ineffective rather than
misdirected.

**X20** is the governing trap in its UI form: verify the asset that is actually wired, not the one whose name
matches what you are looking for.

### 11.4 Check inbound references before replacing anything shared

An element living in a shared or foundation location may serve more surfaces than the one being worked on.
**Replacing it cascades**, and the cascade is discovered by someone else, later, on a surface they did not
change. A reference check before replacement is the whole mitigation.

**The corollary:** where a shared element must change for one consumer only, the change belongs at the
consumer, not at the shared element.

### 11.5 Verify where the surface actually appears

**Some surfaces display outside the normal UI lifecycle** — a loading screen shows before the ordinary UI is
live, driven by config and early-load code. **A widget preview does not exercise that path.** Such surfaces
are verified during a real transition, in the real context, or they are not verified.

**The general standard:** *a UI surface is checked in the situation it appears in.* A surface checked in the
editor's preview has been checked against the editor, which is not a player.

### 11.6 Never touch the display mechanism to fix a look

Where a surface is shown by engine or plugin code, the **widget** is rethemed and the mechanism is left alone.
Replacing a display mechanism to change a colour is a structural change made for a cosmetic reason — the
largest possible blast radius for the smallest possible gain.

---

## 12. ARCHITECTURE

### 12.0 The front end is CommonUI/UMG — no web-tech UI layer (R75)

**The in-game front end is built on CommonUI and UMG. It does not embed an HTML/CSS/JS UI runtime.** A
Coherent Gameface conversion was scoped and declined; the reasoning is recorded in R75 because *"use web tech
for the UI"* is a proposal that recurs, and the answer here is specific to this game rather than general.

**Two reasons, and the second is the one that generalises least and matters most:**

1. **Our design language is built from the properties a CSS-in-engine runtime tells you not to use.** The
   house neon edging is `box-shadow` and `text-shadow`; the glass panels are `backdrop-filter`; the metric,
   queue and ladder layouts are CSS grid. Gameface's own guidance is to replace all three with pre-baked
   textures, an engine screen-space blur, and flex/absolute — **which is a description of how UMG already
   works**, where glows are 9-slice images or materials, blur is the native `Background Blur` widget, and grid
   is `UniformGridPanel`. The "reuse the web mockup" saving does not survive contact with §10's style system.
2. **⚠ A SCRIPTED UI LAYER IS AN ATTACK SURFACE ON A WAGERING CLIENT.** An HTML UI runs untrusted JavaScript
   in-process with the ability to call into native code, which is why every such architecture ships a
   validation gateway, a payload fuzzer and an illegal-transition state machine. **CommonUI has no scripting
   layer, so that boundary does not exist and none of that hardening is needed.** In a game that settles
   stakes, removing a boundary beats defending one.

**This governs the GAME client only.** Browser-side surfaces — portal, store, dashboards, broadcast overlays —
are web properties and are unaffected. **The two stacks share exactly one thing: the design tokens in §10**,
emitted from a single source to CSS custom properties and to a UE data asset. Nothing else is shared, and
nothing else needs to be.

### 12.1 Activatable widgets on named layers

Full-screen surfaces are **activatable widgets pushed onto a named layer stack** — menu surfaces onto the
menu layer, in-match HUD on the game layer.

**What the layer model buys, and why it is not optional:**

- **Input focus and back-navigation come from the stack**, not from per-widget bookkeeping. A pushed surface
  takes focus; popping it restores what was underneath. Hand-rolling that per surface is how back buttons end
  up behaving differently on different screens.
- **A menu surface can hide the in-match HUD by layer**, rather than by knowing which HUD widgets exist. The
  menu never needs a reference to the HUD.
- **Surfaces compose without knowing about each other.** A loadout pushed over a match, over the front end, or
  over a showcase behaves identically, because it only knows its layer.

**The C++ / WBP split:** the **C++ base owns bindings and data**; the **WBP child owns layout**. Data flow is
authored where it can be reviewed and typed; visual arrangement is authored where it can be iterated. Mixing
them puts layout changes at risk of breaking data flow and makes every visual tweak a code change.

**Cosmetics never reach across the boundary either way** (**A5**): a widget does not apply gameplay effects,
and gameplay does not build widgets.

**The push lifecycle runs AFTER construction — configure a pushed surface at runtime, not before.** A layer
push initialises the widget *after* its construct step, so state set before the push does not survive into the
constructed surface. A mode, filter or parameter that varies per entry point (§9.1) is therefore applied by
the **pusher, after pushing** — never by setting a property and expecting construction to observe it.

**The failure this prevents is silent and looks like the wrong feature:** the pre-set value is simply
overwritten by initialisation, so the surface opens in its default mode with no error. The reported symptom is
"the loadout button opens the store", which reads as a wiring mistake rather than an ordering one.

### 12.2 Input routing across PC, console and mobile

**B4** makes PC + console + mobile a shipping requirement, not a stretch goal. That obliges the UI to be built
for three input models from the start:

| Model | Requirement |
|---|---|
| **Pointer** | Hover states are meaningful; precision is available; the numeric field is a first-class input |
| **Gamepad** | **Every surface is fully navigable by directional focus.** Focus order is deliberate and visible — a control that can only be reached by pointing is unreachable on console |
| **Touch** | Targets sized for a thumb; **no hover-dependent information** (there is no hover); no small drag targets (§4.1) |

**Two rules that follow, and are the ones most often broken:**

- **No information may exist only in a hover state.** Hover does not exist on touch and is awkward on gamepad.
  A tooltip is an enhancement; if it carries the only copy of something the player needs, two of three
  platforms cannot see it.
- **Focus is a design object, not an emergent property.** Focus order, the default focused control on each
  surface, and what happens on back are decided per surface. Left emergent, they follow widget-tree order,
  which is an authoring artefact and not a navigation design.
- **Each surface declares its own desired focus target and input mode.** The default landing focus is stated
  by the surface, because only the surface knows which control a player arrived to use.
- **Resolve the focus target by NAME, not by a compile-time widget binding.** A surface whose layout child is
  authored separately (§12.1) cannot hard-bind to a control that the layout may rename, move or replace — a
  binding turns a layout edit into a compile break, and an optional binding turns it into an unfocusable
  screen on gamepad. Name resolution degrades to "no focus target" rather than to a broken surface, and it
  keeps layout authoring free.

**A layout that must differ by platform differs in arrangement, never in capability.** A surface that offers
fewer options on mobile has made mobile a lesser product; the split layout (§3) must survive the transition,
and how it does is an open question (§14.3).

### 12.3 Return routing converges — and the convergence must be verified, not assumed

**Every exit path returns to the hub**: post-match, disconnect, error, and the in-match pause menu all land in
the same place. The hub is where loadouts and the lobby live, and where matches launch from.

**The banked lesson, stated as a standard:** *convergence is a property to be verified per path, not inferred
from one.* A routing model can be correct for most paths and have a **second, independent knob** on one of
them — a widget-level map reference that bypasses the shared setting entirely. The symptom is a single path
landing somewhere else, and the cause is invisible from the shared setting because the shared setting is not
what that path reads.

**Therefore: every return path is enumerated and checked individually**, and any surface that opens a level
directly is treated as its own routing knob until proven otherwise. **X20** again: verify the reference the
path actually reads.

---

## 13. INTERFACES

### 13.1 What the front end CONSUMES

| From | What |
|---|---|
| **Matchmaking** | The **queue set** — which rulesets, sizes and stake bands are open; and the **band boundaries** for any entered value (§4.2) |
| **Matchmaking** | **Population and estimated wait**, per size and per stake band (§5) — including an explicit *no data* state |
| **League** | **Standings** in the display shape league defines (`ssot/league-play.md` §12.2) — the player's own position readable without paging |
| **Economy** | The **catalog** (what exists, its axis, its price shape) and the **entitlement answer** (what this player owns) |
| **Economy** | The **wallet** balance — read-only; the front end renders it and never computes it (**N11**) |
| **Character** | The saved character records as **references** (§9.3) |

**The front end computes none of these.** It is a rendering surface for state other systems own, and every
number it shows is a number it was given.

### 13.2 What the front end OWES — the ticket shape

**The front end's single output is a queue ticket.** It carries:

- the **stake currency** — WATTS or VOLTS (§3.2, R76),
- the **ruleset** (§3.2),
- the **match size**,
- the **stake amount** the player entered,
- the **player/party identity** the queue is for,
- and nothing else. **No venue.** A ticket carrying a venue would re-introduce the map-browser model through
  the interface even if the UI never showed a picker (§2.1).

Three properties of the ticket:

- **It is a request, not a reservation.** Band assignment, opponent selection and venue are all decided
  downstream; the ticket does not presume any of them.
- **It is validated server-side.** Every value on it — especially the stake — is re-checked against the wallet
  and the guardrails (§7) on receipt. **A client-supplied stake is a request to risk currency, and the client
  never decides currency** (**N11**).
- **It is the same shape for every entry path** — first queue, changed selection, or one-tap re-queue (§6).
  A shortcut that produced a differently-shaped ticket would be a second code path with a second set of
  checks, and the shortcut is exactly where a missing check would not be noticed.

---

## 14. OPEN DESIGN QUESTIONS

1. ~~**Preset tier values.**~~ — **CLOSED by R69 and R88.** VOLTS PLAY: `100 · 500 · 2,500 · 10,000 V`.
   WATTS PLAY: `250 · 1,000 · 5,000 · 25,000 W`. LEAGUE PLAY has no stake control at all (R85). The presets do
   teach the ladder, as this item argued — which is why each tier shows **its own** four rungs and never a
   converted figure from the other.
2. ~~**Whether the numeric field accepts arbitrary precision or snaps.**~~ — **CLOSED by R59: free entry,
   and the SERVER snaps.** The client never rounds, because rounding is deciding which pool the player enters
   — the same class of claim as asserting a balance, which **N11** forbids. The band readout already discloses
   the outcome (§4.2), so honesty costs nothing here.
3. ~~**Mobile layout under the split model.**~~ — **CLOSED: the AXES stay, the LIST scrolls.** The binding
   constraint is §12.2's — capability must not be reduced — and collapsing the axis row behind a disclosure
   would silently convert §3's split layout into the stepped flow §3 forbids. So when space runs out, **the
   queue list gives up height, never the axes.** R85 makes this materially easier than when the question was
   written: no tier shows more than three controls, so the row that has to survive is shorter than the
   four-control worst case ever feared.
4. ~~**Where pick/ban lives if PRO MOD tournaments adopt it.**~~ — **CLOSED: NOT IN THE LOBBY, and that is
   R18, not a preference.** Pick/ban is a *venue* interaction and the lobby is a stake lobby — a venue control
   there re-introduces the map browser through a side door, whichever way it is labelled. If tournaments adopt
   it, it lives in the **pre-match room of the tournament surface**, between seating and match start, where
   both parties are already committed and known to each other. **The lobby's promise is that venue is a server
   outcome**; a bracket format that needs otherwise needs its own screen, not an exception in this one.
5. ~~**How a cold band is presented before it folds.**~~ — **CLOSED by R61, plus one presentation rule.**
   R61 sets the fold threshold and makes it automatic with an operator override. Before it folds, the band
   **shows its real count and reads as cold** — §5.2's requirement that an empty band look empty applies to a
   thin one too. **What it must NOT do is hide, grey out, or silently redirect**: a queue that accepts a player
   and never returns is indistinguishable from a broken one, and that is the failure §5 exists to prevent.
   **The fold is announced, never performed silently** — which is exactly the outcome this item named.

---

## 15. RULINGS OF RECORD

| Ruling | Date | Content |
|---|---|---|
| **R84 — TIER IS THE FIRST CHOICE IN THE LOBBY** ⚠ *widened by R85/R86* | **2026-08-06**, **amended 2026-08-06** | **LEAGUE PLAY / WATTS PLAY / VOLTS PLAY** sits **above** the ruleset tabs (§3.2) and rides the ticket (§13.2), because it is the question with the largest consequence and it scopes everything below it — including the stake presets, which are **different ladders** in the two staked tiers (R80, R88). Precedent is exact: every card room puts the play-money / real-money choice at the top and never mixes the lists. Placing it lower would let a player configure an entire entry and meet the denomination last, which is the one thing that must never surprise them about money. **AMENDED:** originally worded as a two-way CURRENCY choice; **R85** makes it a three-way TIER choice by adding unstaked LEAGUE PLAY. **The §3.2.1 defect this row recorded is now RESOLVED** — **R86** scopes league to LEAGUE PLAY, so the lobby renders every dimension that exists in the tier the player has chosen, and never shows more than three controls. |
| **R75 — THE GAME FRONT END IS COMMONUI/UMG; NO WEB-TECH UI RUNTIME** | **2026-08-06** | No Gameface, no RmlUi, no CEF, no embedded HTML UI in the game client (§12.0). **Declined on two grounds.** (a) **The reuse argument does not hold:** §10's style system is delivered through `box-shadow`, `text-shadow`, `backdrop-filter` and CSS grid — the exact properties an in-engine CSS runtime tells you to avoid — and its prescribed replacements (baked glow textures, engine blur, flex/absolute) *are* the UMG idiom, so the design is more native to UMG than to the web runtime. (b) **A scripted UI layer is an attack surface on a client that settles wagers** — untrusted JS in-process calling native code, which is why such architectures ship validation gateways and fuzzers. **CommonUI has no scripting layer, so the boundary does not exist.** Also avoids a commercial dependency and the console focus/input bring-up. **Scope: the GAME client only.** Browser-side properties are unaffected; the two stacks share **only** the §10 design tokens, emitted from one source to CSS and to a UE data asset. |
| **R18 — The front end is a stake lobby, not a map browser** | **2026-08-05** | The player chooses **match size** and **stake amount**. **Venue is a server outcome, disclosed as "venue assigned at match start"** (§2). The front end's axes and the matchmaker's queue dimensions are the same list; any axis the UI offers it must honour, per `ssot/matchmaking.md` **D1**. |
| **R19 — Split layout, not a stepped flow** | **2026-08-05** | Both axes are live and editable at once; no wizard (§3). Staked players re-queue constantly and change one variable, usually stake — a stepped flow makes them re-walk the whole path every time. **Ruleset (MATCH PLAY / BATTLE ROYALE) is the top-level choice above both axes — two tabs, not a doubled flat list** (`ssot/matchmaking.md` **R7**). |
| **R20 — Stake entry: presets primary, numeric secondary, NO slider** | **2026-08-05** | Presets are the primary control; an editable numeric field is secondary. **The slider is rejected** — slow, imprecise by construction, poor on touch (**B4**), and it implies a continuum where the design has bands (§4.1). **The matching band must be visible** ("matching 400–500 V"); a player must never believe an exact figure will be returned (§4.2). |
| **R21 — Population transparency is a UI requirement** | **2026-08-05** | Live counts and estimated wait **per size and per stake band**, on the queue surface (§5). It is the UI-side mitigation for fragmentation and it is the honest option: **an empty band must look empty rather than silently never matching**, because a queue that accepts a player and never returns is indistinguishable from a broken one. |
| **R22 — One-tap re-queue** | **2026-08-05** | Remember the last selection; offer **"BATTLE AGAIN — <size>, <stake>"** as a single action after a match (§6). It sits at the highest-frequency moment in the product and catches peak intent without spending it on navigation. The remembered selection is **stated in the action**, and guardrails still apply — it is a shortcut through navigation, never through a check. |
| **R23 — Guardrails are design, not an afterthought** | **2026-08-05** | Showing potential winnings beside a stake drives engagement, **which is exactly why a stake cap relative to balance and a session loss limit belong in the design from the start** (§7). Both must be **legible before they bind** — a guardrail discovered by hitting it reads as the product taking something away. The wider stakes posture is `ssot/economy-store.md` §13's pending-counsel-review framing, cited and not re-argued. |

---

## 16. RELATED

- [`Docs/DOCTRINE.md`](../DOCTRINE.md) — laws cited here: **A5** cosmetics through cues, gameplay never touches
  art · **B4** multi-platform is a shipping requirement · **C1** game feel before content · **N11** the client
  never decides owned items or currencies · **NM5** a shipped id is never renamed · **NM6** the shipped
  artifact is canonical over the doc · **X20** verify the wired asset, not the canonically-named one.
- `ssot/matchmaking.md` — **D1** (the player never picks the map), **D3** (stake as a banded ticket
  parameter), **D6** (population transparency), **R7** (the ruleset queue split), and §10.1 (the result shape).
- `ssot/match-modes.md` — the two rulesets the top-level tabs present.
- `ssot/economy-store.md` — the cosmetic axis model (§6 there) the loadout renders, the wallet the front end
  displays, and §13's stakes-and-prizes counsel framing.
- `ssot/character-system.md` — §8.2's reference rule, which §9.3 here is the enforcing surface for; and §8.4's
  preview-is-the-product principle.
- `ssot/league-play.md` — §12.2 there defines the standings shape this surface renders.
- `ssot/combat-arsenal.md` — the beam hues §10.3's readability law requires the HUD to stay clear of.
