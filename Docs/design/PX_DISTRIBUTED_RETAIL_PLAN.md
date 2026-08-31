# DISTRIBUTED RETAIL — design/plan (operator pivot, 2026-08-31; S2 BUILD GREENLIT same day)

> **S2 greenlight (operator, verbatim):** "Continuously work through our new plan until ready for
> placement pass, cart checkout: till / chip-anywhere / both — **Both** but make it easily minimized
> or opened at player discretion. Non intrusive best practices, comfort and ease not pushy or
> desperate. Range rule: **test-fire yes but no players can die on our Lobby map. No Kills allowed.**"
>
> **Try-on mechanism (operator, same day): "temporary or map exception Grant process."** The try-on
> is NOT a client-local visual fake: the server grants a TRANSIENT map-scoped entitlement (wallet
> `TempMapGrants`, never persisted/PlayFab, dies with the PlayerState) and equips through the REAL
> selection seam. The weapon is genuinely in your hands (range test-fire live), the mask genuinely
> on (mirrors show replicated truth); discard/exit revokes + restores the server-side baseline
> snapshot. Scope-gated to hub pawns (hub travel component marker), rate-limited server-side.

Supersedes the centralized-store layout half of `PX_STORE_BUILD_RULINGS.md` (the six-tab economy
rulings D-1..D-11 stand unchanged; the SURFACE model pivots). Parent authority:
`Docs/Hub/LOBBY_UPGRADE_DOC.md` + `MAIN_MAP_LOBBY_SYSTEM_HELPER.md` s4/s5.

## The model (operator verbatim intent)

> Spawners on map placed throughout by product type — weapons by range, masks/visors in one
> building, jewelry in another. Products placed strategically. Walk up to item, grab it, product
> card pulls up small in screen, buy on the spot. Visor and jewelry buildings need mirrors. When
> you walk across item you automatically wear it or hold it. Simple Buy / Add to Cart / Discard.

## Retail zones (placement by product type)

| Zone | Products | Venue | Notes |
|---|---|---|---|
| SHOOTING RANGE | Weapons (incl. hand-cannon variants) | the range | Try-fire lives here already (the one CanFire zone); grabbing a weapon = holding it; range targets make the demo real |
| VISOR HOUSE | Facemasks/visors | dedicated building | Mirrors required; grab = wearing it |
| JEWELLERY HOUSE | Accessories (+ emblems as MARKS wall) | dedicated building | Mirrors required; D-9 render gap gates the jewellery half |
| ROBO LABS front | Robot slot packs + creator credits | outside/inside RoboLabs | Slots sell where robots are made |
| PX BUILDING | Credits + League + featured rotation | the existing PX interior | Becomes the "general store" corner, not the whole store |

## The GRAB loop (per item)

1. WALK-OVER / GRAB: crossing the item's pad auto-applies it CLIENT-LOCALLY (wear the mask, hold
   the weapon) — the s4 preview pattern; never replicated, server cost zero.
2. CARD: a SMALL corner card slides in (not a takeover; the world never dims): name · price ·
   BUY · ADD TO CART · DISCARD. ESC/Q = Discard. Proper back-handler this time.
3. BUY: on-the-spot confirm (compact, in-card two-tap: BUY -> "SURE? · 990 V" -> done) -> spoken
   grant -> the item STAYS ON because now you own it (auto-equip through the selection seam).
4. DISCARD: the item comes off, restored to your real loadout ("nothing leaves the store
   unbought" — force-restore on zone exit stays law).
5. ADD TO CART: batches ids client-side; a cart chip (count · total) follows the player; CHECKOUT
   at any till point (PX counter) or from the cart chip — one confirm, sequential grants,
   per-item refusals spoken. Cart is CLIENT-LOCAL and session-only (no persistence).

## Two tiers, one flow (operator: "a quick flow and a detailed flow option")

- **QUICK**: grab -> wearing it -> small card -> BUY (two taps) or ADD TO CART or DISCARD. Done
  standing there. This is the default and covers most purchases.
- **DETAILED**: the small card carries one **DETAILS** affordance -> the product page overlay
  (already built: world-visible, full read, related items) for the considered purchase. Same
  seams, no new machinery — the page becomes the opt-in deep dive instead of the front door.

## INTENTIONAL pickups (operator law: "not everywhere we step")

- Grab pads are TIGHT (the item's own footprint, ~1m), never zone-sized; crossing a shop floor
  never dresses you by accident.
- A short dwell (~0.3s on the pad) or an explicit E-grab arms the apply — brushing past does
  nothing. Which of the two feels right is an operator playtest call; both ship behind one knob.
- Placement rules: pads OFF the main walking lanes (alcoves, counters, wall bays); the decal
  ring + item name render only within ~4m and only with line of sight. Density budget per venue
  (readable shelves, not minefields).
- The hub at large stays CLEAN: retail pads exist only inside the ruled venues — the open lobby
  never sells at your feet.

## Cart discipline (non-intrusive on the lobby)

Chip is small, corner-anchored, collapsible to the icon; it renders only while the cart is
non-empty or inside a venue. No toasts outside venues; checkout confirms once, not per item.

## Cost & effects assessment (operator-requested)

**Server/runtime cost — near zero by construction (helper doc s4/s5 discipline):**
- Item displays, try-on applies, cards, cart = CLIENT-LOCAL; zero replication, zero GameLift
  overhead. The server sees only final purchase RPCs (unchanged from today).
- Mirrors: proximity-gated captures, show-only-filtered — the s5 pattern exists precisely so
  1,000 CCU survives them; capture cost is paid only by the player standing at one.
- Pads: N small overlap boxes (~40-80 across five venues) — trivial; same class of cost as the
  12 door volumes already proven.
- Perf watch-item: grab-apply churn (rapid wear/unwear swapping materials/meshes) — bounded by
  the dwell/E gate and a 1-apply-at-a-time rule.

**Dev cost (S2 estimate, in proven-pattern units):**
- Grab pad actor + intentionality gate: 1 build cycle (door-volume + preview-seam patterns).
- Small card + in-card confirm + DETAILS link: 1-2 cycles (C++-built tree, the page exists).
- Cart chip + till checkout: 1-2 cycles (client array + sequential validated purchases).
- Mirror actor: 1 cycle (s5 recipe is explicit).
- Placement passes: operator-anchor sessions per venue (the door-anchor flow, proven).
- Retire rack layout: trivial. TOTAL: roughly 5-7 build cycles + placement.

**UX effects:** shopping becomes exploration (venues worth walking to); the lobby stays a
lobby (no takeovers, no ambient commerce); try-on-first raises purchase confidence and mirrors
close the loop. Risk: discoverability — solved by door signs (wayfinding) + venue glyphs.

**Economy effects:** on-the-spot buying shortens the impulse path (good for SPARK-rung volume);
try-on-before-buy reduces refund/regret pressure; the cart aggregates multi-item baskets the
old per-tile flow never could. Risks: accidental purchases (killed by the two-tap confirm) and
price visibility (card always shows price before any tap).

## Mirrors (s5, verbatim)

Proximity-gated SceneCapture mirrors in the Visor and Jewellery houses: capture OFF by default,
box trigger wakes it, show-only-filtered to the local player + preview. One mirror actor class,
placed per building.

## Display mechanics (what shows the item)

- WEAPONS: the proven spawner pad (registry) — real mesh, real rotation. Range rows use the
  existing spawner tags; catalog->display-mesh map extends coverage over time.
- WEARABLES: mannequin-style stand (facemask on a head form / chain on a bust) OR floating item
  mesh from the item def; S2 scope picks per family after the anchor pass.
- Small floor decal ring marks a grabbable item; NO screen-space plates at distance (the plate
  wall bug: product signage renders AT-ITEM only, occluded by walls — door signs keep their
  through-wall wayfinding, products never).

## Bug fixes folded in (from the operator walk)

1. Product plates visible outside/through walls -> at-item-only signage (above).
2. ESC could not close the product page -> the small card and any retail screen registers as a
   CommonUI back handler (bIsBackHandler) — the HubGateCards lesson, applied everywhere.
3. No weapon shown on pedestals -> the item IS the display in this model.

## Build sequence (S2, after approval)

1. Grab pad actor (walk-over apply + card trigger + discard-on-exit) on the try-on preview seam.
2. Small card widget (C++-built, corner-anchored, back-handler correct) + in-card confirm.
3. Cart chip + checkout flow (client-local batch -> sequential validated purchases).
4. Mirror actor (s5 gating) + two placements.
5. Zone placement passes with operator anchors (range/visor house/jewellery house/labs/PX).
6. Retire the 19-rack layout + product-plate distance tiers.

## Placement anchors (operator, 2026-08-31)

- **VISOR HOUSE = the current PX building** (SM_Door_001, already open) — the 19-rack layout
  inside retires; facemask grab pads + mirrors replace it. The general-store corner (credits /
  League / till) shares the building or moves at the operator's final pass.
- **JEWELLERY = the SM_Scifi_Crate area** at (-3250, 1865, 82) — display cases + mirrors staged
  there; operator adjusts final placement.
- Weapons @ Shooting Range, slot packs @ RoboLabs front: pending their anchor passes.

## Ruled (operator, 2026-08-31)

- Cart checkout: **BOTH** — till pad and from-chip anywhere; chip minimized/opened at player
  discretion ([V]). Non-intrusive best practices; one confirm, never per item.
- Range try-fire: **live-fire yes**; **NO KILLS on the lobby map** — every hub player carries
  `Gameplay.DamageImmunity` (the Lyra health set zeroes all damage on it) via the net-profile
  component's dynamic-tag GE. Firing stays fully live for the range demo.

## S2 build (landed 2026-08-31 — pending PIE proof)

| Piece | Home | Shape |
|---|---|---|
| Map-exception grant | `AFLWalletComponent` (`TempMapGrants` + grant/revoke/query) + loadout commit gate temp clause | transient, server-only; `IsEntitled` stays REAL ownership |
| Try-on RPCs | `UAFLCosmeticLoadoutComponent::ServerRequestTryOn/ServerReleaseTryOn` | hub-marker scope gate, 0.3s churn bound, baseline snapshot, keep-on-purchase |
| Orchestrator | `UAFLRetailSubsystem` (AFLCombat, GameInstance subsystem) | pads/till feed it; owns dwell/press arm, card, cart, checkout, keys E/F/C/Q/V/X (non-consuming) |
| Small card | `UAFLW_RetailCard` | corner, C++-built, no input steal; BUY two-tap / CART / DISCARD / DETAILS→product page |
| Cart chip | `UAFLW_RetailCartChip` | top-right, [V] open/min, [X] one-confirm checkout, sequential grants + honest sweep |
| Grab pad | `AAFLDisplayPedestal` (evolved) | tight 90cm box, dwell/press knob, at-item-only LOS-occluded plate, optional DisplayProp mesh |
| Till | `AAFLRetailTill` | counter box + sign; walking up opens the chip expanded |
| Mirror | `AAFLHubMirror` (+`UAFLHubMirrorWidget`) | s5: capture off by default, wake box, show-only you+attached, RT into a world widget |
| No-kill law | `UAFLHubNetProfileComponent` | `Gameplay.DamageImmunity` dynamic-tag GE at ASC bind (authority) |
| Emblem gate fix | loadout commit | `EmblemId` was NEVER copied by the gate — every emblem equip silently no-opped; fixed facemask-shaped |

Known S2 gaps (ticketed): a None-baseline restore sticks on Weapon/Beam/Emblem/WeaponSkin axes
(facemask restores clean — its None is a meaningful un-equip); ESC not bound to discard (Q is —
raw ESC collides with the pause menu); gamepad pass minimal (FaceButton-Left = grab/details only).
