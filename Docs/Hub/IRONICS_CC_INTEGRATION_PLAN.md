# IRONICS_CC_INTEGRATION_PLAN

**Status:** NEW. Folds the Character Creator into the Lobby Hub programme as **Track C**, with a reset
of how creator work is scoped so it builds what the operator asked for and nothing else.
**Date:** 2026-08-26
**Why a reset:** the operator has never seen the creator live in the UX flow as designed; prior
attempts drifted from the recorded requests. The creator is on the critical path — the Loadout
(Barracks) and Store (PX) upgrades render *through* it. It cannot stay a side programme.
**Basis:** `IRONICS_CHARACTER_CREATOR_SSOT.md` §5–§8 · `IRONICS_CC_ROADMAP.html` · `IRONICS_PRICING_SSOT.md`
§4–§7 · `Lobby_Upgrade_Doc.docx` (Robo Labs, PX, Barracks) · live tracker store slices (extend-not-rebuild).
**Companions:** `IRONICS_LOBBY_HUB_SSOT.md` · `IRONICS_LOBBY_HUB_ROADMAP.html` (Track C section) ·
`IRONICS_LOBBY_HUB_CLAUDE_CODE_BRIEF.md` (lanes, worktrees).

---

## 1 · The intent lock

These are the operator's design requests as recorded on disk, numbered so every ticket can cite
them. **A ticket acceptance criterion that does not trace to an intent ID is out of scope.** An
agent that wants to add something not on this list writes one paragraph on the tracker and stops.
Nothing here is reopened; only the operator adds to it.

| ID | Request | Source |
|---|---|---|
| **I-1** | Flow is `ENTRY → CHASSIS → BUILD → SAVE → EQUIP → MATCH`; free movement between Chassis/Build/Save; nothing commits to the active look until SAVE | CC SSOT §5.1 |
| **I-2** | ENTRY is a full-screen menu, sibling of the Digital Market, pushed via the `afl.Store.Open` pattern; empty state = one action, "Start a build" | CC SSOT §5.1 |
| **I-3** | CHASSIS is a first-class choice (Original vs X), not a hidden variant; X line ships first | CC SSOT §5.1, §7 |
| **I-4** | BUILD exposes the live channels (Neon, Edge, Visor, Emblem; Chassis albedo only if proven) plus facemask, emblem, finish | CC SSOT §3.4, §5.1 |
| **I-5** | **The preview is the product** — the spawned pawn, never a stand-in, same lighting/pose as play | CC SSOT §5.3 |
| **I-6** | Combat-range zoom-out state is mandatory, not polish | CC SSOT §5.3 |
| **I-7** | Every choice reversible without loss: undo on placement, revert-to-saved on the build | CC SSOT §5.3 |
| **I-8** | **Not RGB sliders.** One hue arc per channel, saturation/value clamped into the neon band, 3–4 chroma stops; Neon and Edge linked by default with an unlink toggle | CC SSOT §6.1 |
| **I-9** | Free players pick from owned discrete colour SKUs; subscribers get the continuum; treatment/finish scalars are product, never a player slider | CC SSOT §6.2, Pricing §7 |
| **I-10** | Gamut clamp is enforced server-side on write; team override, if any, is visible in-creator | CC SSOT §6.3 |
| **I-11** | One creator shell, channel schema data-driven by the chassis's master; **extends the loadout pattern — never a new screen family** | CC SSOT §7, CC-5 |
| **I-12** | SAVE writes a named `FAFLCreatorBuild` to a slot; slot counter always visible; cap enforced server-side; over-cap = **saved-locked** shape, not save-refused | CC SSOT §5.1, memory: save gate |
| **I-13** | EQUIP sets `ActiveBuildIndex` and resolves into `FAFLCosmeticSelection` via `ServerSetCosmeticSelection` — the creator is another caller of the existing seam | CC SSOT §5.1, §4.1 |
| **I-14** | Creator-centric economy: **everyone builds**; purchase gates saving and carrying, never building | Pricing pivot ruling |
| **I-15** | Stickers: 9 fixed zones, placement by drag within zone clamped to the UV rect, behind a feature flag until the axis is proven | CC SSOT §9, CC-7 |
| **I-16** | Accessories/jewellery axis (chains, pendants, bracelets, watches) as parts on the proven `AddCharacterPart` mechanism; hand-cannon pairs as `.XT` bundle rows | memory: accessory axis scoped |
| **I-17** | Robo Labs is the creator's home in the hub; PX Store is where you try on / hold with mirrors and buy; Barracks is where owned assets are displayed like the store and equipped | Lobby_Upgrade_Doc |
| **I-18** | Store and Loadout are **product pages**, not filtered axis grids; visual layer from Claude Design via the three-lane pipeline | memory: store/loadout redesign |
| **I-19** | Build names need a profanity filter, uniqueness, and a report path before anyone else sees them | CC SSOT §8 |
| **I-20** | ~~Finish the jewellery/accessories system to working before moving on~~ **CLOSED by operator 2026-08-26 — off the build list; C0 cites the existing proof, nothing further is raised** | Lobby_Upgrade_Doc · operator |
| **I-21** | Creator UI is six regions per `IRONICS_CC_UI_HANDOFF` §2: **A** chassis picker (first — it determines the channel rail's contents), **B** preview viewport (`UImage` bound to `PreviewRT`; the largest element by area), **C** channel rail, **D** slot counter, **E** build-name field, **F** commit bar (`UCommonButtonBase` ×2: Save, Revert) | `IRONICS_CC_UI_HANDOFF` §2 |
| **I-22** | Grid, gutters and region proportions are **UNSPECIFIED** in the handoff and settled by a Claude Design mock at 1280×720; any spacing derived before that mock exists is provisional and is replaced by the mock, never hardened | `IRONICS_CC_UI_HANDOFF` §2 |
| **I-23** | The creator widget's parent is `UAFLW_LoadoutBase`; region B binds the existing `SetupPreviewCapture` / `PreviewRT` path. **F1 ruling (2026-08-26):** the capture renders the real `B_AFL_Robot_*` part actors via the gameplay part map on an `AAFLLoadoutDisplayPawn` (`AFLW_LoadoutBase.cpp:695, :852, :864-873`); colour resolves through `SetPreviewSelection → GetEffectiveSelection → BuildColorOverride`, the gameplay path. **In the hub, B previews the local gameplay pawn (C1). In the front end, the display pawn is the correct host** — kept, with a parity check owed (AFL-3214): facemask, finish, emblem, weapon, pose/anim, lighting identical to a gameplay spawn; anything tag- or cue-driven that the ASC-less pawn misses is a defect | CC-READ-5 audit + F1 verdict 2026-08-26 |
| **I-24** | **Brand lock:** ground `#222A3A`, surface-card `#0E122B`, accent Electric Neon Blue `#1E5AFF`, Watts magenta `#FF00D5`; Orbitron / NotoSans / DroidSansMono; cyber/neon direction; **no cyan accent, no Apple-Glass / frosted / white panels, no `#64B4FF`, no SF Pro**. This supersedes `expert-game-designer/references/afl-design.md`, which is stale | `AFLTokenCompiler.cpp` · tracker 2026-06-07 store correction · `IRONICS_CC_DESIGN_BRIEF.md` §0 |
| **I-25** | **RATIFIED (operator 2026-08-27): the Claude Design mock + spec.** Canvas: `IRONICS Creator Kit` (claude.ai/code/artifact/6703f673-2ef1-4e3d-b31a-39a65f883e48, 8 artboards) + `Docs/Hub/Design/IRONICS_CC_DESIGN_SPEC.md`. Binding numbers: 1280×720 settlement canvas, margins/gutters 16, header 56 @ y16, content y88 h536 — C rail x16 w304 · B viewport x336 w744 (largest, 744×536) · D strip x1096 w168 — footer y640 h64; controls h44; 1080p = uniform 1.5×, title-safe 5% (96,54,1728×972); type ramp + token set (locked §0 + the spec's PROPOSED extensions, ratified with it); kit states per the Kit page; bind map per spec §7 (gaps → tickets: F-Equip cites I-13, loadout Swap/Discard cite I-27). PX = walk-in stocked store (spatial, spawner-pattern engineering; nothing leaves unbought — leaving auto-returns; verbs TRY ON / BUY / PUT BACK) | operator ratification 2026-08-27 · `IRONICS_CC_DESIGN_BRIEF.md` §7 |
| **I-26** | **Complete AAA revamp ruled.** The creator/loadout visual layer AS BUILT is REJECTED ("scene cluttered, UI/UX B-grade, not what was requested — and not what is requested now"). No further authoring on the current WBP layout; the surface is rebuilt only from the ratified Claude Design mock + spec (AFL-3203 → I-25+). The C++ contract, the three seams, and the preview spine survive; the visual layer does not. "Whatever it takes": a proper 3D interactive character builder with a proper UX interaction system, 2026 AAA best practices | operator chat ruling 2026-08-27 |
| **I-27** | Loadout (Barracks) shows the player's character **free-moving** in a clean uncluttered scene; **equip / discard / swap are one-click verbs**; clear REAL item displays (I-5 applied to items); uniform brand awareness across creator/loadout/store (I-24 enforced everywhere) | operator chat ruling 2026-08-27 |

**Operator:** the Claude Design mock and spec did not exist on disk; `IRONICS_CC_DESIGN_BRIEF.md` is the
input that produces them (AFL-3203). The ratified output is appended here as I-25+ before C2 starts.
This table, not a chat thread, is what the agents build against.

---

## 2 · Why the attempts failed — diagnosed, not assumed

**C0 (CC-READ-5) establishes the true state before anything is authored.** It walks every CC SSOT
and CC roadmap row plus the memory-recorded "proven" items and answers, per row: *on disk?* (path) ·
*proven?* (PIE evidence hash/log or none) · *matches the intent ID it serves?* The output is a
**divergence table**: what was built vs. what I-1…I-20 asked for.

Failure classes to test for — each is a hypothesis with a check, none is asserted:

| # | Hypothesis | Check |
|---|---|---|
| F1 | Preview was a stand-in (RT capture of a separate mesh, different lighting/pose), not the pawn | **Answered 2026-08-26:** real part actors, same part map, same colour resolve, on a separate display pawn. Ruled: hub → local pawn (C1); front end → display pawn kept, parity check owed (AFL-3214). Not a rebuild. |
| F2 | Colour control shipped as sliders / free RGB / unclamped client values | Widget tree + what `ServerSetCosmeticSelection` receives; does the server clamp fire? |
| F3 | A new screen family was built instead of extending `AFLW_LoadoutBase` / the market widget | Class parents of every CC widget on disk |
| F4 | UI built above channels that were not proven end-to-end (ENTRY-to-render on a controllable pawn) | Per channel: is there a 2-client PIE proof hash? |
| F5 | Scope drift: work landed that traces to no intent ID (new axes, new economy behaviour, "improvements") | Divergence table column "intent ID"; blank = drift |
| F6 | Save/equip bypassed the seam (local save, direct selection write) | Call sites: does SAVE reach `IAFLCosmeticPersistence`? does EQUIP go through `ServerSetCosmeticSelection`? |
| F7 | Built on the Original line whose MIs are not neutralised, so finishes fight baked colour | Which chassis the shell defaults to; MI neutralisation state |

Anything that fails a check is **reverted or quarantined**, not patched: a drifted screen is not a
foundation. What passes is kept and cited as proven. The operator sees the divergence table before
C1 and rules on each quarantined item (keep / delete).

---

## 3 · One kit, three thin shells

The creator, the loadout, and the store product page show the same robot and touch the same
seams. They differ in *what the player can change* and *what commits at the end*. So they share one
widget kit and one preview spine, and each is a thin shell — which is exactly I-11 (one shell,
extends the loadout pattern) and I-18 (product pages) applied together, not a new design.

```
Preview spine (built once, Track C1, consumed by H3)
├── UAFLCosmeticPreviewComponent   client-local apply on the LOCAL PAWN via the same functions
│                                   the #43 path uses; restore from authoritative selection  (I-5, I-7)
├── AAFLHubPreviewAnchor            camera blend to a fixed view; drag-rotate                   (I-5)
├── Combat-range state              second anchor at gameplay distance; one toggle             (I-6)
├── AAFLHubMirror                   gated capture, show-only local pawn                         (I-17)
└── Front-end fallback              PreviewRT capture of the LOCAL PAWN (AFLW_LoadoutBase.cpp:481
                                    path) for the Landing/front-end context only — same pawn,
                                    same part actors, never a separate mesh                     (I-5)

Widget kit (Track C2) — CommonUI, brand tokens, one class each
├── AFLW_HueArc                     clamped hue arc + chroma stops + link toggle                (I-8, I-9)
├── AFLW_PartPicker                 catalog-filtered UCommonListView of tiles (owned/price/
│                                   entitled badge) — the market tile, reused                   (I-4, I-9)
├── AFLW_BuildSlotStrip             saved builds, slot counter, saved-locked state             (I-12, I-14)
├── AFLW_ChassisPicker              Original | X, first-class                                  (I-3)
├── AFLW_ChannelPanel               renders the schema for the resolved master                  (I-11)
└── AFLW_ActionBar                  Save / Equip / Buy / Revert, context-driven                (I-1, I-7, I-13)

Shells (thin, mode = data)
├── AFLW_Creator        (Robo Labs)   Chassis → Build → Save → Equip                            (I-1..I-14)
├── AFLW_Loadout        (Barracks)    pick saved build / owned part → Equip                     (I-17, I-18)
└── AFLW_ProductPage    (PX)          one SKU → try-on/hold → Buy → Equip                      (I-17, I-18)
```

- **Extend, never rebuild.** Every kit widget's parent is the proven market/loadout class where one
  exists (tile, list, wallet bind, buy loop — tracker: "extend-not-rebuild is doubly-validated").
  C0 names the parent for each.
- **The pawn is the preview everywhere.** In the hub it is literally the player standing at the
  anchor with the mirror; in the front end it is a capture *of the pawn*. No third mesh exists.
- **One commit path per verb.** Save → `IAFLCosmeticPersistence` (build blob) · Equip →
  `ServerSetCosmeticSelection` · Buy → `ClientRequestPurchase`. The shells own no economy logic.

---

## 4 · Track C — phases and gates

Track C runs in parallel with H1–H2 (it does not need the hub map) and **must be green at C3
before H3 (retail) opens**, because H3's pedestal → product page → try-on is C1 + C2 + the product
shell. The Labs door (H2.4) reads `Disabled` until C3 is green.

| Phase | Delivers | Gate (watched, no cheats) |
|---|---|---|
| **C0** | CC-READ-5 true-state pass · divergence table · quarantine/keep rulings · intent table ratified · **stale skill brand fixed (AFL-3202)** · **Claude Design mock + spec produced from the brief and ratified (AFL-3203)**. Seeded by the 2026-08-26 six-region audit: A and B missing, F half-bound, C/D/E present, parent `UAFLW_LoadoutBase` (F3 passes) | Every CC row has a disk/proof verdict; operator has ruled keep/delete per quarantined item |
| **C1** | Preview spine: preview component, anchors (portrait + combat-range), mirror, front-end capture-of-pawn | 2 clients: A previews a mask + a colour, B sees nothing; A exits, A is restored to the authoritative selection; combat-range toggle works |
| **C2** | Widget kit: hue arc (clamped, linked/unlinked), part picker, slot strip, chassis picker, channel panel, action bar | Each widget drives the preview spine on the pawn; server clamp rejects an out-of-band colour injected by cheat |
| **C3** | **Creator shell, end to end.** ENTRY → CHASSIS → BUILD → SAVE (named, slot counter, saved-locked over cap) → EQUIP → MATCH | **Operator walks the flow they designed**, no cheat, X line; two builds saved, switch, respawn, relaunch client — builds survive backend read-back; over-cap saves locked not refused; match spawns the equipped build |
| **C4** | Loadout shell on the kit; Barracks pedestals consume it (H3.6) | Equip an owned weapon and a saved build from Barracks; carried in the hub; correct in match |
| **C5** | Product page on the kit; try-on/hold + Buy; PX pedestals consume it (H3.1–H3.3) | The H3 canary (`Flag.Japan`) bought from the page, on both clients, respawn-durable |
| **C6** | Stickers placement UI (I-15, behind flag, gated on CC-7 UV work). Accessories are **closed** — C0 cites the existing proof; no C6 work | Stickers: one zone placed, clamped, atlas-composited, replicated |

Accessories/jewellery are closed by operator ruling (I-20); C6 is stickers only.

---

## 5 · Rules that keep agents on the requests

1. **Intent-ID gate.** Every AC cites an I-n. The session's SCOPE step (brief §4) prints the cited
   IDs; an AC with none is deleted before work starts.
2. **Data before surface.** No kit widget or shell ships on a channel/axis that lacks a 2-client
   PIE proof of ENTRY-to-render on a controllable pawn. C0's divergence table says which channels
   qualify today.
3. **No stand-ins, ever.** A preview that captures anything other than the local pawn's own part
   actors is a defect, not a placeholder.
4. **Extend-not-rebuild is a review check.** Each new widget's parent class is named in the ticket
   and verified in the diff.
5. **Three verbs, three seams.** Any economy/selection/persistence call from a shell that is not
   `ClientRequestPurchase` / `ServerSetCosmeticSelection` / the persistence seam is a defect.
6. **Visual layer comes from Claude Design through handoff docs** (I-18); Claude Code builds from
   the spec, never invents brand values or layouts.
7. **The operator watches C3.** The creator is not "done" by any agent's verdict; it is done when
   the operator has driven the designed flow on the real pawn and said so on the tracker.
8. **Drift is stopped, not merged.** Work that appears without an intent ID is quarantined at the
   next gate.

---

## 6 · Tasks (AFL format, `AFL-32xx`)

Lanes per `IRONICS_LOBBY_HUB_CLAUDE_CODE_BRIEF.md`: CC-W worktree · CC-E editor connection · CD Claude Design · OP operator.

```
[AFL-3200] CC-READ-5 — true-state pass and divergence table
Type: Research · Discipline: Engineering · Priority: P0 · Estimate: M · Sprint: HUB-C0 · Lane: CC-W (subagent fan-out) + CC-E (widget inspection)
Depends On: AFL-3001 · Blocks: AFL-3201..3260
## Acceptance Criteria
- [ ] Every row of IRONICS_CC_ROADMAP.html and every "proven" CC item in memory gets: on-disk (path) · proof (hash/log or NONE) · intent ID served
- [ ] Every CC/loadout/store widget on disk: class, parent, which seam it calls for save/equip/buy, what its preview captures (F1–F7 checks answered with evidence)
- [ ] Divergence table: built-vs-intent; each drifted item marked QUARANTINE with the operator's keep/delete column blank
- [ ] Intent table §1 ratified by the operator; I-21+ appended from any design docs/mockups supplied
- [ ] Docs/Hub/CC-READ-5.md committed alone
```

```
[AFL-3201] Quarantine execute
Type: Tech Debt · Discipline: Engineering · Priority: P0 · Estimate: S · Sprint: HUB-C0 · Lane: CC-W / CC-E per file · Depends On: AFL-3200
- [ ] Per the operator's keep/delete rulings: deleted items removed (content commit) or reverted (code commit); kept items cited as the foundation in AFL-3210+
- [ ] No drifted screen is left as a parent for new work
```

```
[AFL-3202] Fix the stale AFL design overrides in the expert-game-designer skill   (I-24)
Type: Tech Debt · Discipline: Design · Priority: P0 · Estimate: XS · Sprint: HUB-C0 · Lane: CC-W · Depends On: None
- [ ] `Tools/skills/expert-game-designer/references/afl-design.md`: replace the Apple-Glass tokens (#64B4FF, glass panels, SF Pro) with the locked IRONICS brand from IRONICS_CC_DESIGN_BRIEF.md §0; design language line reads "cyber / neon"
- [ ] `afl-ui-hud-design` and `lyra-skin-builder-marketplace` skills grep'd for the same stale values; corrected where found
- [ ] Doc-only commit; cited on the tracker as the drift root cause it removes
```

```
[AFL-3203] Claude Design — creator / loadout / product-page mock + spec from the brief   (I-21, I-22, I-24 → I-25+)
Type: Feature · Discipline: Design · Priority: P0 · Estimate: L · Sprint: HUB-C0 · Lane: CD (produce) · CC-W (bind-map verify) · OP (ratify) · Depends On: AFL-3202, AFL-3200
- [ ] Claude Design runs the §8 prompt in IRONICS_CC_DESIGN_BRIEF.md; outputs land in Docs/Hub/Design/
- [ ] CC-W verifies every bind-map name against the C++ contract on disk (path:line); gaps become tickets citing an intent ID
- [ ] §6 must-not and §7 acceptance walked line by line — returned to Claude Design on any fail
- [ ] Operator ratifies; grid/gutters/proportions + kit states appended to the intent lock as I-25+
- [ ] Blocks C2 (AFL-3220..3222): no kit widget is authored before this is ratified
```

```
[AFL-3210] UAFLCosmeticPreviewComponent — client-local apply on the local pawn   (I-5, I-7)
Type: Feature · Discipline: Engineering · Priority: P0 · Estimate: L · Sprint: HUB-C1 · Lane: CC-W · Depends On: AFL-3201, AFL-3002
## Acceptance Criteria
- [ ] Identical to IRONICS_LOBBY_HUB_TASKS AFL-3034 (this is the same component; owned here, consumed by H3) — plus: BeginPreview for every I-4 channel (Neon, Edge, Visor, Emblem, finish, facemask, emblem id) and every I-16 accessory part
- [ ] Undo stack per session (I-7): each BeginPreview pushes; Undo pops and re-applies; EndPreview clears and restores from the authoritative selection by re-running the normal apply
- [ ] Never writes FAFLCosmeticSelection; never RPCs
```

```
[AFL-3211] Preview anchors — portrait + combat-range   (I-5, I-6)
Type: Feature · Discipline: Engineering · Priority: P0 · Estimate: M · Sprint: HUB-C1 · Lane: CC-W + CC-E
- [ ] AAFLHubPreviewAnchor (as AFL-3031) with two placed instances per station: Portrait and CombatRange; one toggle switches; drag-rotate works at both
- [ ] Front-end context: AFLW_LoadoutBase PreviewRT path pointed at the local pawn's own part actors (not a separate mesh); combat-range = capture FOV/distance preset
```

```
[AFL-3212] AAFLHubMirror   (I-17)
Type: Feature · Discipline: Engineering · Priority: P1 · Estimate: M · Sprint: HUB-C1 · Lane: CC-W + CC-E
- [ ] As IRONICS_LOBBY_HUB_TASKS AFL-3035; placed at the Labs station as well as PX
```

```
[AFL-3214] Display-pawn parity — front-end preview host   (I-5, I-23)
Type: Research → Bug · Discipline: Engineering · Priority: P0 · Estimate: M · Sprint: HUB-C1 · Lane: CC-W (read) → CC-E (proof) · Depends On: AFL-3201
- [ ] Side-by-side, same selection: `AAFLLoadoutDisplayPawn` capture vs the gameplay pawn at spawn — facemask (`RefreshFacemaskForPawn` reaches the display pawn?), finish/MID push, emblem decal tint, carried weapon + hold pose, idle anim, lighting rig, camera FOV/distance at Portrait and Combat-range
- [ ] Every divergence listed with path:line; tag- or cue-driven visuals the ASC-less pawn cannot receive are called out explicitly
- [ ] Divergences fixed by making the display pawn run the same apply functions (never a cached look-alike); anything that cannot be made identical is surfaced to the operator as a product decision, not hidden
- [ ] Proof: operator compares the two renders on the same build and calls them identical

```
[AFL-3213] C1 gate proof
Type: Pipeline · Discipline: QA · Priority: P0 · Estimate: S · Sprint: HUB-C1
- [ ] Track C1 gate per §4, 2 clients, dev accounts; log ledger; tracker rows
```

```
[AFL-3220] AFLW_HueArc — clamped hue arc, chroma stops, link toggle   (I-8, I-9, I-10, I-25+)
Type: Feature · Discipline: Engineering · Priority: P0 · Estimate: L · Sprint: HUB-C2 · Lane: CC-W (C++ control) + CC-E (WBP from the ratified spec) · Depends On: AFL-3203
## Acceptance Criteria
- [ ] C++ UCommonUserWidget with a hue parameter [0,1), fixed S/V band from DA (data, not literals), 3–4 chroma stops as discrete snaps; outputs FAFLChannelValue with bContinuum=true
- [ ] Discrete mode (free player): the arc shows only owned colour SKUs as stops; output SourceSkuId with bContinuum=false (I-9)
- [ ] Linked mode: one arc drives Neon+Edge; unlink toggle reveals the second arc; default linked (I-8)
- [ ] Server clamp proof: a cheat injects an out-of-band ResolvedColor → ServerSetCosmeticSelection rejects; logged (I-10)
- [ ] Gamepad + mouse + touch input per CommonUI; brand tokens; no RGB slider anywhere in the tree
```

```
[AFL-3221] AFLW_PartPicker — catalog-filtered tile list (extends the market tile)   (I-4, I-9)
Type: Feature · Discipline: Engineering · Priority: P0 · Estimate: M · Sprint: HUB-C2 · Lane: CC-W + CC-E
- [ ] Parent = the proven market tile/list classes (named from CC-READ-5); filter by axis/type; badges: owned / price / entitled / saved-locked
- [ ] Selecting a tile calls BeginPreview only; nothing commits
```

```
[AFL-3222] AFLW_BuildSlotStrip · AFLW_ChassisPicker · AFLW_ChannelPanel · AFLW_ActionBar   (I-1, I-3, I-11, I-12, I-14)
Type: Feature · Discipline: Engineering · Priority: P0 · Estimate: L · Sprint: HUB-C2 · Lane: CC-W + CC-E + CD
- [ ] SlotStrip: saved builds from the persistence record; counter "n / cap" always visible; over-cap builds render saved-locked (I-12), never hidden
- [ ] ChassisPicker: Original | X; X default; selecting resolves the master and the ChannelPanel re-renders its schema from data (I-3, I-11)
- [ ] ActionBar: Save / Equip / Buy / Revert shown per context; Revert = EndPreview restore (I-7)
- [ ] Each widget's parent named and verified (extend-not-rebuild)
```

```
[AFL-3223] C2 gate proof
- [ ] Each kit widget drives the preview spine on the pawn (watched); clamp rejection logged; tracker rows
```

```
[AFL-3230] AFLW_Creator shell — ENTRY → CHASSIS → BUILD → SAVE → EQUIP   (I-1, I-2, I-13, I-14, I-19)
Type: Feature · Discipline: Engineering · Priority: P0 · Estimate: XL · Sprint: HUB-C3 · Lane: CC-W + CC-E + CD
## Acceptance Criteria
- [ ] Pushed via the afl.Store.Open pattern on UI.Layer.Menu (I-2); empty state = "Start a build" (I-2)
- [ ] Composes the kit; no economy/selection/persistence logic in the shell (§3)
- [ ] SAVE: name (filtered via UAFLTextFilterSubsystem, uniqueness check, report path stub logged — I-19) → FAFLCreatorBuild → IAFLCosmeticPersistence; cap enforced server-side; over-cap → saved-locked (I-12)
- [ ] EQUIP: ActiveBuildIndex → resolve → ServerSetCosmeticSelection (I-13)
- [ ] Everyone can build regardless of ownership; unowned parts preview and are marked; Save/Equip gate on entitlement (I-14)
- [ ] Labs door row switched from Disabled to this widget (H2.4)
```

```
[AFL-3231] C3 gate — operator drives the designed flow
Type: Pipeline · Discipline: QA · Priority: P0 · Estimate: M · Sprint: HUB-C3 · Lane: OP + CC-W ledger
- [ ] Operator: ENTRY → CHASSIS(X) → BUILD (arc, parts) → SAVE ×2 (named) → switch → EQUIP → respawn → relaunch client → both builds back from the backend → third save over a free cap = saved-locked → Deploy → match spawns the equipped build. Zero cheats.
- [ ] Operator's verdict on the tracker in their words; anything they flag becomes a ticket citing an intent ID
```

```
[AFL-3240] AFLW_Loadout shell on the kit + Barracks consumption   (I-17, I-18)
Type: Feature · Discipline: Engineering · Priority: P0 · Estimate: L · Sprint: HUB-C4 · Lane: CC-W + CC-E + CD
- [ ] Product-page layout (I-18) from the Claude Design handoff; picks a saved build or owned part; Equip only
- [ ] H3.6 Barracks pedestal interact opens this pre-focused; old loadout entry removed only after the gate (H3.8)
```

```
[AFL-3250] AFLW_ProductPage on the kit + try-on/hold + Buy + PX consumption   (I-17, I-18)
Type: Feature · Discipline: Engineering · Priority: P0 · Estimate: L · Sprint: HUB-C5 · Lane: CC-W + CC-E + CD
- [ ] Merges IRONICS_LOBBY_HUB_TASKS AFL-3032/3033: pre-focus API, Buy → ClientRequestPurchase, Equip → seam
- [ ] Try-on = BeginPreview; hold = weapon-mesh socket preview; mirror at the station
- [ ] H3 canary proof (Flag.Japan at a pedestal, both clients, respawn-durable)
```

```
[AFL-3260] Accessories / jewellery axis — CLOSED (operator 2026-08-26)   (I-16, I-20)
Type: Research · Discipline: Engineering · Priority: P2 · Estimate: XS · Sprint: HUB-C0 · Lane: CC-W
- [ ] C0 cites the accessories proof already on disk (hash + evidence line) so the jewellery counter rack (H3.6) and the preview component axis list can reference it
- [ ] No accessory build work is scheduled or raised in this programme
```

```
[AFL-3261] Stickers placement UI — behind feature flag   (I-15)   [GATED: CC-7.1 UV derivation]
Type: Feature · Discipline: Engineering · Priority: P1 · Estimate: L · Sprint: HUB-C6 · Lane: CC-W + CC-E
- [ ] 9-zone fixed array; drag within zone clamped to the UV rect; undo (I-7); atlas compositor V-flip (zr = 2 - Zone/3) honoured
- [ ] Flag off by default in the configurator until the axis proof lands
```

---

## 7 · How Track C threads into the hub roadmap

| Hub row | Now reads |
|---|---|
| H2.4 Labs door | `Disabled` until **C3** green, then `AFLW_Creator` |
| H3.2 preview anchor · H3.4 preview component · H3.5 mirror | **Delivered by C1** (owned here, consumed there) |
| H3.3 product page wiring | **Delivered by C5** |
| H3.6 Barracks racks | consume **C4** loadout shell |
| H3 entry gate | + Track C at C3 green |
| Jewellery counter rack | populated from the closed accessories axis (proof cited in C0, AFL-3260) |

Sequencing: C0 runs with H0. C1–C2 run during H1–H2 in their own worktrees (`cc-c1-preview`,
`cc-c2-kit`), CC-E tails batched with the H1/H2 editor sessions. C3 is the first thing the operator
watches after H2's loop. H3 opens on C3. C4/C5 run inside H3. C6 runs with H4/H5.

---

## 8 · Decisions needed from the operator

| # | Decision | Default |
|---|---|---|
| 1 | Attach any creator design requests / mockups / Claude Design outputs not already on disk → appended as I-21+ | Table §1 as ratified |
| 2 | Keep/delete per quarantined item from the C0 divergence table | Delete anything with no intent ID and no proof |
| 3 | Confirm: one widget kit, three thin shells (§3) is the engineering of I-11 + I-18 together, not a redesign | Proceed |
