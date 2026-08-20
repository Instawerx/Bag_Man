# IRONICS — CHARACTER CREATOR · UI DESIGN HANDOFF

**Status:** spec, not an implementation. The widget is the UI lane's.
**Authored:** 2026-08-20 · **Lane split:** behaviour + interface = Claude Code lane (shipped, see §9); widget assets, layout and visual authoring = UI lane.

> **HOW TO READ THIS.** Every colour, face, size, radius and motion value below is a **token reference**, never a
> literal. Tokens resolve through `IRONICS_UI_STYLE_SSOT.md` and are emitted by `AFLTokenCompiler`
> (`Source/LyraEditor/AFL/AFLTokenCompiler.cpp`), which writes face/typeface/size into every `TS_IRONICS_*`
> style so the ramp cannot drift by hand-edit. Where this spec says **UNSPECIFIED**, the SSOT does not answer it
> and the item names what would settle it. Do not invent a visual language to fill a gap — an invented value
> becomes a shipped decision nobody made.

---

## 1 · OVERVIEW

### What the creator does

The player builds a **robot identity** by choosing colour on the channels their chosen chassis actually renders,
sees it on a live preview they can rotate, names it, and saves it to a slot. That saved build is what spawns.

### Where it sits in the flow

Front-end, out of match. `IRONICS_CHARACTER_CREATOR_SSOT.md` §5.2 places change-timing outside the match
boundary; the selection lock (`IsSelectionEditable`) is the gate. The creator is reached from the loadout
surface and shares its preview rig (`UAFLW_LoadoutBase` owns the display pawn, the `SceneCapture2D` and the
render target — the creator does not spawn a second one).

### What the player is actually deciding

Three things, in this order of consequence:

1. **Chassis** — which body. This decides *how many channels exist* (§7.6). It is not a cosmetic pick; it
   changes the tool.
2. **Colour per channel** — the product. Clamped to the neon gamut, so the decision is *hue*, and saturation
   and value are constrained (§7.3).
3. **Slot** — which of their saved builds this becomes. Slots are counted and capped (§7.7).

**The preview is the product** (`CREATOR_SSOT` §5.3). What the player sees is what spawns. This is architecturally
guaranteed, not merely intended: the preview resolves colour through the same `BuildColorOverride` →
`SetColorOverride` path the gameplay pawn uses on possession (§9). Any UI that renders colour by a second route
would reintroduce exactly the divergence that rule forbids.

---

## 2 · LAYOUT

### Shell

`UIPanel.Glass` base. Full-screen activatable widget over the front-end backdrop. Corner radius token
`--r-panel`; blur `Glass.Blur`; fill `Glass.Bg.Primary`; edge `Glass.Border`.

### Regions and ordering

| # | Region | Anchoring | Notes |
|---|---|---|---|
| A | **Chassis picker** | top rail, full width | Ordered first because it determines the channel rail's contents. |
| B | **Preview viewport** | centre, dominant | The `PreviewRT` render target. Largest element by area — it is the product. |
| C | **Channel rail** | side rail, vertical list | Length **varies by chassis** (§7.6). Must not imply a fixed set. |
| D | **Slot counter** | header or footer, persistent | Always visible; the cap is a live constraint, not a save-time surprise. |
| E | **Build name field** | adjacent to slot counter | Moderation state changes what strangers see (§7.8). |
| F | **Commit bar** | footer | Save / revert. `CREATOR_SSOT` §5.3: every choice reversible without loss. |

**Reading order for gamepad focus** is A → C → B(rotate) → E → F. Rationale in §8.

**UNSPECIFIED:** exact grid, gutters, and region proportions. `IRONICS_UI_STYLE_SSOT.md` specifies panel
geometry and the token set but no creator layout grid. **Settled by:** a mock at 1280×720 (the canvas the
compiler and existing WBPs author against, §4 of the SSOT), reviewed against the lobby's horizontal budget —
which overflowed 1280px once already with a wide display face.

---

## 3 · TOKENS

Every value the creator uses. **Source column is the authority; this table is a reference, not a redefinition.**

| Token | SSOT source | Usage in the creator |
|---|---|---|
| `UI.House.Electric` | §2.1 PRIMARY | Active/selected channel row, commit-button fill, active panel border |
| `UI.House.Violet` | §2.1 ACCENT | Focus and hover rim only. **Never** a channel swatch, never readable text |
| `UI.House.Black` | §2.1 DEPTH | Panel backing behind swatches so neon reads |
| `UI.House.White` | §2.1 TEXT | Labels, hex readout, channel names |
| `UIPanel.Glass` | §6 | Shell, channel rail, chassis picker |
| `UIButton.Glass` | §6 | Commit bar, unlink toggle, chassis tiles |
| `UIChip.Value` | §6 | Slot counter (`n / cap`), mono |
| `Glass.Bg.Primary/Secondary/Tertiary` | §3 | Shell / nested rail / dividers |
| `Glass.Border` | §3 | Panel edge; overridden to House Electric when active |
| `Glass.Tint.Danger` | §3 | At-cap slot state, error states — a **wash**, never a solid |
| `Glass.Shadow` | §3 | Z-separation of rail over viewport |
| `Text.Primary/Secondary/Tertiary` | §3 | Channel label / reason text / disabled label |
| **Display** — Orbitron | §4 | Screen title, chassis names. ALL-CAPS, identity-carrying text only |
| **Body** — Noto Sans | §4 | Channel labels, disabled reasons, helper copy. Sentence case |
| **Numeric** — Droid Sans Mono | §4 | **Hex/value readout, slot counter.** Mandatory: tabular numerals so digits do not jitter |
| `--r-panel` / `--r-button` / `--r-input` | compiler geometry | Shell / buttons / name field |
| `--blur` | compiler geometry | Glass blur radius |

**`UIDisplay.NeonTube` is NOT used in the creator.** It is size-gated display-only ≥64px (§6) and remains
unapproved (SSOT OPEN ITEM 5). The creator's headings use flat Display type on glass.

**Deliberately absent:** the neon registry hues (§2.2 — NeonBlue, NeonPink, etc.). Those are *player identity*
colours and appear inside swatches as **player data**, never as creator chrome. Chrome is house palette only.

---

## 4 · COMPONENTS

| Element | Base class | Variants |
|---|---|---|
| Creator screen | `UCommonActivatableWidget` (Lyra front-end pattern) | — |
| Preview viewport | `UImage` bound to `PreviewRT` (existing, `UAFLW_LoadoutBase::SetupPreviewCapture`) | — |
| Chassis tile | `UCommonButtonBase` | selected / unselected |
| **Channel row** | `UCommonUserWidget` | **Connected · PresentButInert · Absent** (§5, §7.1) |
| Hue arc | `UCommonUserWidget` + custom paint or `USlider` | enabled / disabled / unset |
| Swatch | `UImage` + `UTextBlock` readout | has-value / unset |
| Value readout | `UTextBlock`, mono | **always present** (§8 — this is the CVD requirement) |
| Link toggle | `UCommonButtonBase` (toggle) | linked / unlinked |
| Slot counter | `UIChip.Value` | under-cap / at-cap / upgrade-available |
| Name field | `UEditableTextBox` | editing / pending / approved / rejected (§7.8) |
| Commit bar | `UCommonButtonBase` ×2 | save enabled/disabled, revert enabled/disabled |

**Architecture:** C++ base owns bindings, WBP child owns layout — the proven AFL split (SSOT §6 pipeline).
The creator base binds to §9's interface; the WBP authors appearance.

---

## 5 · STATES AND INTERACTIONS

Every element, every state. Where a state is not reachable for an element it says so rather than being omitted.

### 5.1 Channel row — the load-bearing one

| State | Trigger | Treatment |
|---|---|---|
| **Default (Connected)** | `BodyState/EdgeState/… == Connected` | Full opacity. Label `Text.Primary`. Swatch shows colour + hex readout. Arc interactive |
| **Focused** | gamepad/mouse focus | `UI.House.Violet` outer rim (§2.1 state layer). Focus ring must be visible at TV distance (§8) |
| **Hovered** | mouse only | Violet rim at lower intensity than focus. Gamepad has no hover — do not rely on it |
| **Pressed** | drag on arc | Swatch and readout update **live**, per-frame. `bCaptureEveryFrame` is already true |
| **Disabled — PresentButInert** | state `== PresentButInert` | **Shown, dimmed to `Text.Tertiary`, arc non-interactive, REASON displayed.** See §7.1 |
| **Disabled — Absent** | state `== Absent` | Shown, dimmed, reason displayed, **visually distinct from PresentButInert** (§7.1) |
| **Loading** | schema not yet resolved (`ResolvedFromMaster == None`) | Skeleton row, no channel names guessed. Fails closed — claims no channels |
| **Empty / unset** | channel has no chosen colour | Swatch shows the unset treatment (§7.4); readout shows `—`, not a fabricated hex |
| **Error** | apply failed | `Glass.Tint.Danger` wash on the row; reason text. **UNSPECIFIED:** whether apply can fail client-side at all — the preview path has no server round-trip, so this may be unreachable. **Settled by:** confirming `CreatorApplyPreview` has no failure mode once the pawn exists |

### 5.2 Hue arc

| State | Treatment |
|---|---|
| Default | Arc renders the **reachable** gamut only (§7.3) |
| Focused | Violet rim on the arc track; handle enlarged for TV legibility |
| Pressed/dragging | Handle follows; swatch + hex update live |
| Disabled | Track at `Text.Tertiary`, handle hidden, no input |
| Unset | Handle absent, track visible — the channel has no hue yet (§7.4) |

### 5.3 Slot counter

Under-cap · at-cap · upgrade-available — see §7.7.

### 5.4 Commit bar

| State | Rule |
|---|---|
| Save enabled | working selection differs from saved **and** a slot is available |
| Save disabled — no change | Dimmed, reason "No changes" |
| Save disabled — at cap | Dimmed, reason names the cap and the upgrade path (§7.7) |
| Revert enabled | working differs from saved |

---

## 6 · MOTION

| Element | Trigger | Animation | Duration | Easing |
|---|---|---|---|---|
| Screen | activate | Panel breathe-in + blur ramp | UNSPECIFIED | UNSPECIFIED |
| Channel row | focus | Violet rim fade-in | UNSPECIFIED | UNSPECIFIED |
| Swatch | colour change | Cross-fade old→new | UNSPECIFIED | UNSPECIFIED |
| Preview pawn | rotate | Follows input 1:1, no smoothing on drag; inertia on release UNSPECIFIED | — | — |
| Slot counter | reaching cap | Single pulse, then rest | UNSPECIFIED | UNSPECIFIED |

**UNSPECIFIED across the board.** `IRONICS_UI_STYLE_SSOT.md` §3 states the *principle* — "motion is meaning;
panels breathe in/out; transitions reveal depth" and "luminous restraint" — but specifies **no durations or
easing curves anywhere**. The only concrete timing in the SSOT is the `UIDisplay.NeonTube` hum (two opacity
dips over ~5.2s on `steps(1)`), which the creator does not use.

**Settled by:** a motion pass adding a duration/easing token set to the SSOT (e.g. `Motion.Fast/Base/Slow`
+ named curves), emitted by `AFLTokenCompiler` like the geometry tokens already are. Until then the lane
should not hand-pick durations per widget — that is how a UI ends up with fourteen different fade times.

---

## 7 · EDGE CASES AND CONTENT

### 7.1 The three channel states — the defining edge case

`FAFLCreatorChannelSchema` reports **three** values per channel, not a boolean:

| State | Meaning | Render |
|---|---|---|
| `Connected` | Parameter exists **and** carries signal to an output | Normal, interactive |
| `PresentButInert` | Parameter exists, measured to have **zero downstream consumers on this master** | **DISABLED, VISIBLE, WITH A REASON** |
| `Absent` | Master has no such parameter | Disabled, visible, with a *different* reason |

**Why `PresentButInert` may never be hidden.** Ruled 2026-08-20 (CC-X24). Hiding it makes a missing feature
indistinguishable from a bug — to the player *and* to the next developer. The measured case: on `M_AFL_Character`
(the X-line flagship) `TeamColor` exists but is graph-disconnected, so **body colour is disabled there** while
edge and glow work. A player who sees no body control and no explanation reasonably concludes the creator is
broken.

**The reason string sources from `ResolvedFromMaster`** — the schema carries the master's name precisely so the
UI can say *why*. Suggested copy, sentence case, `Text.Secondary`:

- `PresentButInert` → "Body colour isn't available on this chassis."
- `Absent` → "This chassis has no visor to tint."

**The two disabled states must be visually distinguishable.** They mean different things and have different
futures: `PresentButInert` is restorable by a material change (CC-X25 is logged to do exactly that for the
X-line body channel); `Absent` is not. **UNSPECIFIED:** the distinguishing treatment. **Settled by:** a visual
pass — but note the constraint that it cannot rely on hue alone (§8).

### 7.2 Unaudited masters

`bMasterAudited == false` means the connectivity audit has never covered this master, so a `Connected` verdict
means *"present, and inertness unknown"* — **not** "measured to render."

This must not present identically to an audited-connected channel. The honest read is a caution, not a claim.
**UNSPECIFIED:** treatment. **Settled by:** a ruling on whether unaudited channels are offered at all. Both are
defensible — offer with a caveat marker, or withhold until audited — and it is a product call, not a visual one.

### 7.3 The clamp is visible, never a snap-back

The gamut floors saturation at **0.55** and value at **[0.45, 1.0]** (`AFLCreatorGamut`). Hue is the free axis.

**The arc must only expose reachable colour.** The player must never drag into a colour they cannot have and be
jerked back — a snap-back reads as the control fighting them, and it teaches that the tool is unreliable.

Implementation guidance: the arc's track renders colours produced by `AFLCreatorGamut::FromHue()`, so **the track
itself is the gamut**. There is nowhere out-of-gamut to drag *to*. Saturation and value are not player-facing
axes at all — do not ship sliders for them and then clamp the result.

**Why hue-only is honest rather than limiting** (`CREATOR_SSOT` §6.3): the clamp is a **gameplay** requirement —
near-black and near-white builds degrade match readability for every other player. The arc offers exactly the
freedom that survives the clamp.

### 7.4 Achromatic start

Dragging hue from grey, black or white is a real player path — a fresh channel or one set to a neutral. This is
handled correctly in the behaviour layer as of CC-5.2 (a bug that returned red for *every* hue from an achromatic
start was found and fixed by `afl.Creator.ArcProbe`).

**What the swatch shows before a hue is chosen:** the channel's current colour, whatever it is, plus a readout of
`—` rather than a fabricated hex. When the player first touches the arc, saturation lifts to the floor at their
chosen hue — this is a visible, expected transition, not a glitch. **Do not** pre-seed the handle at hue 0; that
implies red was chosen when nothing was.

### 7.5 Link model

`FAFLCreatorChannelLinks` defaults to **fully unlinked** (`LinkedMask == 0`). Spec the per-channel toggle.

**The default pairing is UNSPECIFIED and must stay that way until ruled.** The roadmap specifies "Neon and Edge
linked by default," which **does not map** onto the shipped channel set: measurement established `NeonColor` is
not a creator channel at all, and body is disabled on the X-line — leaving Edge and Glow as the two channels
there. **Settled by:** a product ruling on which channels move together. Until then the toggle exists, ships
off, and the UI must not present a pairing as a default.

### 7.6 Channel count varies by chassis

**Two** on the X-line (`M_AFL_Character`: edge, glow — body disabled). **Three** on Manny-based masters
(`M_Mannequin`: body, edge, glow). Visor is `Absent` on both measured masters.

**The rail must not imply a fixed set.** No fixed-height four-row layout with two rows greyed as if temporarily
unavailable. The rail is data-driven from the schema, and its length is a property of the chassis. Changing
chassis changes the rail — that is correct and should read as informative, not as an error.

### 7.7 Slot counter

Ladder (`IRONICS_PRICING_SSOT` §5.1): **2 free · 5 League · 10 hard cap.** `MaxUpgrade ? 10 : clamp(Baseline +
Purchased, Baseline, TierCeiling)`.

| State | Treatment |
|---|---|
| Under cap | `UIChip.Value`, mono, `n / cap` |
| At cap | `Glass.Tint.Danger` wash (a wash, never a solid) + the upgrade path named |
| Upgrade available | Route to the store surface. **No paywall inside the creation flow** (`character-system.md`): the creator shows what the player owns with a single route out — it does not grey desirable options mid-authoring |

**Currency spelling is LAW** (§5): **Watts**, **Volts**. Integer only. Never show USD.

### 7.8 Build naming

CC-5.4 gates public names by moderation state (`EAFLNameState`).

| Viewer | Pending | Approved | Rejected |
|---|---|---|---|
| **Owner** | Their own name, with a pending indicator | Their own name | Their own name + rejected indicator + edit route |
| **Stranger** | **"Unnamed Robot"** | The name | **"Unnamed Robot"** |

The owner **always** sees their own text — never replaced, never hidden from them. The gate is a *visibility*
rule for other players, not censorship of the author's own view. Fails closed: anything not `Approved` shows the
placeholder to strangers.

---

## 8 · ACCESSIBILITY

### 8.1 Colour-vision deficiency — non-negotiable, and specific to this product

Roughly **8% of male players** cannot reliably distinguish channels presented by hue alone. **This product is
colour**, so this is the one accessibility item that cannot be deferred.

**Requirements:**

1. **Every channel is identifiable by label, position and value readout — never by its colour.** The channel
   rail is a labelled list in stable order, not a row of coloured dots.
2. **A numeric readout is always present** beside every swatch, in **Droid Sans Mono** (§4 — tabular numerals).
   Format **UNSPECIFIED** between sRGB hex (`#1E5AFF`) and HSV (`H 226° S 0.55 V 0.90`). **Settled by:** a
   ruling — hex is universally recognised and copy-pasteable; HSV names the axis the arc actually manipulates
   and would make the clamp legible ("S is at the 0.55 floor"). Recommend HSV *plus* hex if space allows.
3. **No interaction may require distinguishing two similar hues.** Nothing is confirmed by "pick the one that
   matches"; nothing is disambiguated by swatch colour alone.
4. **Disabled-state differentiation must not rely on hue** (§7.1) — opacity, iconography, position or explicit
   text, since the two disabled reasons must be told apart by players who cannot compare tints.

### 8.2 Gamepad

- **Gamepad-only completability is required.** Every action reachable: chassis select, channel focus, hue
  adjust, link toggle, rotate, name entry, save.
- **Focus order:** A → C → B → E → F (§2). The channel rail is a vertical D-pad/stick list; up/down moves rows,
  left/right adjusts the focused channel's hue. Rotation binds to the right stick while the viewport has focus.
- **Back/cancel semantics:** back = revert-to-saved *if changes exist* (with confirmation), else close. It must
  never silently discard work — `CREATOR_SSOT` §5.3 requires every choice reversible without loss.
- **Focus takes on activation:** the chassis picker (region A), because it determines everything downstream.

### 8.3 TV distance and console

- **Focus visibility at TV distance:** the violet rim alone is insufficient at 10 feet. Focus needs a
  size/scale or border-weight change in addition to the colour state layer.
- **Console safe zones (TRC):** all interactive elements and text inside title-safe. **UNSPECIFIED:** the safe-zone
  inset value. **Settled by:** the platform TRC requirement per target; it is a certification input, not a design
  choice.
- **Text scale:** body type must survive a scale increase without clipping. The rail is a list, so it grows
  vertically and scrolls — do not fix its height.
- **Touch (mobile, B4 target):** minimum touch target UNSPECIFIED; the arc needs a larger hit region than its
  visual stroke. **Settled by:** the platform's minimum-target guidance.

---

## 9 · THE INTERFACE CONTRACT

**Shipped and callable now.** The widget calls these; it does not reimplement them.

### From `UAFLW_LoadoutBase` (CC-5.3)

| Function | Purpose |
|---|---|
| `CreatorSetChannel(EAFLCreatorChannel, FLinearColor)` | Set one channel on the working selection. **Clamps on entry** via the shared gamut — the preview cannot show an uncommittable colour |
| `CreatorApplyPreview()` | Push the working selection to the display pawn **through the shipping resolve path** |
| `CreatorRotatePreview(float DeltaYawDegrees)` | Spin the model. Rotates the **mesh**, not the actor (the capture is attached to the actor) |
| `CreatorGetPreviewYaw()` | Current yaw, degrees |
| `CreatorGetWorkingSelection()` | The uncommitted selection being edited |
| `CreatorGetSchema()` | `FAFLCreatorChannelSchema` for the bound chassis — drives the whole rail |
| `CreatorLinks` | `FAFLCreatorChannelLinks`, read/write |

### From `FAFLCreatorChannelSchema`

`BodyState` · `EdgeState` · `GlowState` · `VisorState` (`EAFLChannelAvailability`) · `bBodyAvailable` etc.
(usable == `Connected`) · `ResolvedFromMaster` (the reason source) · `bMasterAudited` (§7.2) ·
`AvailableCount()`.

### From `AFLCreatorGamut`

`HueOf(FLinearColor) → degrees` · `WithHue(FLinearColor, float) → clamped` · `FromHue(float) → clamped` ·
`ClampToNeon(FLinearColor)` · constants `MinSaturation 0.55`, `MinValue 0.45`, `MaxValue 1.0`.

**One clamp, shared.** These are the same functions the server commits with — single-sourced deliberately in
CC-5.2, because two implementations of one rule drift silently and the player is shown one colour and given
another.

### Not yet built — name the gap rather than assume it

- **Save/commit to a slot** is `UAFLCosmeticLoadoutComponent::ServerSaveBuild` (CC-3, shipped) but the creator
  has no wrapper yet; the widget should not call the server RPC directly.
- **Slot entitlement counting** is **not wired**: the SKUs are priced and purchasable, but buying a slot does
  not yet increment the counted entitlement. Tracked as CC-4.2 slot wiring. **The slot counter will read a
  static value until that lands** — spec it, bind it, expect it to be inert initially.
- **Name moderation state** (`EAFLNameState`) is shipped in CC-5.4; the creator has no wrapper.

---

## 10 · WHAT THIS SPEC DELIBERATELY DOES NOT DECIDE

Recorded so the gaps are visible rather than discovered during authoring:

| Gap | Settled by |
|---|---|
| Layout grid, gutters, region proportions | A 1280×720 mock reviewed against the lobby horizontal budget |
| All motion durations and easing | A motion token set added to the SSOT and emitted by `AFLTokenCompiler` |
| `PresentButInert` vs `Absent` visual distinction | Visual pass, constrained by §8.1 (not hue-alone) |
| Unaudited-master treatment | Product ruling: offer with caveat, or withhold |
| Link default pairing | Product ruling; roadmap's Neon+Edge does not map |
| Hex vs HSV readout | Ruling; recommend HSV + hex |
| Console safe-zone inset, touch target minimum | Platform TRC / platform guidance |
| Whether preview apply can fail | Confirm `CreatorApplyPreview` has no failure mode post-pawn |

---

## APPENDIX · MEASURED FACTS THIS SPEC RESTS ON

Every claim here was measured in-engine, not inferred. Cited so the lane can re-check rather than trust.

| Fact | Evidence |
|---|---|
| X-line offers **2** channels; Manny-based offers **3** | `DERIVED master=M_AFL_Character body=0 edge=1 glow=1 visor=0 count=2 audited=1` vs `M_Mannequin … count=3` |
| `TeamColor` exists on both masters, inert on one | Same run: `found=1` on both, opposite verdicts — inertness is keyed on the (master, parameter) pair |
| `NeonColor` cannot substitute for body | `AlbedoRecolor` measures `0.0`; `NeonColor`'s only path to `BaseColor` is gated by it |
| The gamut clamp is shared, not duplicated | `MinSaturation = 0.55f` appears exactly once in the codebase |
| The arc never leaves the gamut | `AFL_TEST[ARC] PASS — checked=24 outOfGamut=0 keptSV=1 linksDefaultOff=1` |
| Preview and spawn share one resolve path | `SetColorOverride(BuildColorOverride(*EffSel))` inside `RefreshSkinForPawn`, which both call |
