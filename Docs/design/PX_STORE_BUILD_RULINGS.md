# PX STORE — build rulings (operator, 2026-08-31)

Operator ratified the PX Store UI/UX mock (artifact `7b4a3d7b`) and ruled ALL six decision
cards per recommendation. This doc is the build SSOT for the store arc; it supplements
`Docs/ssot/economy-store.md` (economy law) and `Docs/Hub/Design/IRONICS_CC_DESIGN_SPEC.md`
(I-25 spatial ruling). Newest-wins precedence applies.

## The model (operator verbatim intent)

> "The store must be walkable and interactable main UX/UI; the cards and pages should have
> AAA flow that is not obtrusive and complimentary in fashion to our assets."

- **The WALKABLE store is the main experience** (I-25 confirmed): the PX Store interior on
  `L_AFL_OutpostEarth`, stocked by catalog-driven racks. No screen at the door.
- **Screens are complementary overlays**: the product page opens AT THE SHELF over the live
  world (~60% width), never a full-screen takeover. Confirm rides `UI.Layer.Modal`.

## Rulings

| # | Ruling |
|---|---|
| D-1 | `AFL.League.*` terms stay VOLTS-priced in-game; $5/mo is the WEBSITE price of the Volts that buy them. One flow; LEAGUE tab stays alive. |
| D-3 | The SIX ruled tabs stand (ROBOTS/WEAPONS/CREDITS/FACEMASKS/JEWELLERY/LEAGUE). Colours/skins/beams/edges are creator-side, reached via the axis-scoped get-more link — never store tabs. The ~200 priced rows for those families are NON-PRODUCTS; SKU catalog doc is stale. |
| D-7 | Owned single-grant rows GREY OUT in the store + the server refuses re-purchase as backstop. Counted SKUs (slots, credits) stay re-buyable by design. |
| D-10 | JEWELLERY = accessories only. Emblems pair with sticker credits as the MARKS surface (two tabs, one surface — the closed emblem/sticker ruling applied to the store). |
| D-11 | CHARACTER slots keep the proven pack ladder (3,000/4,990/10,000 V). Sticker/emblem/weapon slot axes author later at the x1 price point, flat. |
| RACK | The robots rack sells SLOT PACKS (sellable), not Characters (all free). Six-tab taxonomy adopted on every surface; door subtitle updates; `AFL_Lint_StoreTaxonomy` holds it. |

## Held back (blocked on backend, NOT design)

Bundle/pair cards (no game→ledger path), weapon-credit pool statement (exclusion property
unauthored), jewellery cards until accessories render, FacemaskCredit spec (no doc), Volts
bonus tiers. Each ships when its blocker clears; the surfaces above never fake them.

## Build plan (S1 = walkable spine)

1. `AAFLDisplayPedestal` (AFLHub): one catalog row staged in-world — item visual + floating
   product plate (door-sign widget pattern) + at-shelf verbs (E engage -> product page).
2. `AAFLDisplayRack` (AFLHub): axis-filtered, catalog-driven pedestal row — new catalog row
   = new stock, no map edit (I-25 law).
3. `UAFLW_ProductPage` (AFLCombat): the ruled overlay form (mock ProductPage artboard) —
   ~60% width over the live world, BUY -> confirm modal -> grant -> EQUIP.
4. Purchase confirm modal + refusal toasts (mock Flows artboard) — a rejected buy is never
   silent again.
5. Chassis updates: D-7 grey-out, D-10 tab move, server owned-refuse.
6. Interior dressing + rack placement in the PX Store building (operator anchors).
PIE verification deferred to the next natural PIE session (operator ruling).
