# DISTRIBUTED RETAIL — design/plan (operator pivot, 2026-08-31; PLAN ONLY, build gated on approval)

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

## Open items for the operator at approval

- Which buildings are the Visor House and Jewellery House (name meshes/doors like SM_Door_001).
- Cart checkout point: at a till, from the chip anywhere, or both.
- Range try-fire: grabbing a weapon at the range — live-fire before buying (needs the range
  CanFire zone + temp ammo rules) or hold-only?
