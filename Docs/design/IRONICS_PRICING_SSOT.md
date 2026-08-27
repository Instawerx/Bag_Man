# IRONICS_PRICING_SSOT

> **PRECEDENCE (ruled 2026-08-24):** the NEWEST economy doc is authoritative; where the newest is SILENT on a question, the prior doc governs. This doc last changed **2026-08-19** (`a61d9e43`). Today `IRONICS_PRICING_SSOT.md` (2026-08-19) is newest and `IRONICS_ECONOMY_SPEC.md` (2026-08-05) fills its gaps — e.g. Volt packs, on which the newer doc is silent. The rule exists so a later disagreement resolves without re-ruling.

**Status:** REPLACEMENT. Supersedes `IRONICS_PRICING_SCARCITY_SSOT.md` in full.
**Date:** 2026-08-17
**Basis:** CC-READ-1 through CC-READ-4 (read-only forensic passes, editor + terminal lanes).
**Rule:** every mechanism claim in this document cites a file and line, or is marked
UNVERIFIED. Where the prior doc claimed a mechanism the code does not implement, that is
called out explicitly rather than carried forward.

---

## 0 · Why this is a replacement, not a revision

`IRONICS_PRICING_SCARCITY_SSOT.md` (221 lines) contains seven internal contradictions, and
its §5 "now law" summary re-asserts the exact tier ladder that its own R1/R2 rulings
retired. A reader cannot determine which half is live from the document itself.

The seven, for the record:

1. `$10` is SPARK/Watts-buyable at `:24` and Premium/Volts-only at `:45`.
2. §5 (`:194-198`) re-affirms the retired ladder, FLICKER rung, and category→rung mapping
   as "now law" after R1/R2 superseded all three.
3. FLICKER is "confirmed law" (`:155-160`, `:196`) and "absorbed into Impulse" (`:48`).
4. Identity price is ARC $23 (`:147`) and Singularity $500 (`:36`).
5. Finishes/masks are SPARK $10 (`:148-149`) and Standard $2.99–4.99 (`:44`).
6. Singularity is Volts-only (`:76`, `:120`) and carries a 5,000,000 W price (`:111`, `:130`).
7. Roster count is "30 Character + 7 Team" (`:36`); disk truth is 28 identities, 28 Team
   rows, 5 CharacterPartMap Team keys.

**One additional correction.** R6 (`:64-68`) states the Premium Volts-only wall is
"enforced at the PURCHASE layer … `AFLWalletComponent.cpp:342-344`". Those lines contain no
tier concept. They read a per-row integer. The wall described in the prior doc is a data
convention that was mistaken for a code mechanism. See §3.

---

## 1 · What the code actually enforces

Established by CC-READ-4 §1 and §2. This section is mechanism, not policy.

### 1.1 Earnability is one integer

`UAFLWalletComponent::ServerPurchaseCosmetic` (`AFLWalletComponent.cpp:300-367`, dev-only
behind `#if UE_BUILD_SHIPPING` at `:305-307`) and its shipping twin
`ClientRequestPurchase` (`:376-409`) both derive payment options from the row alone:

```
bVoltsAvailable = (Entry->PriceVolts > 0)   // :333
bWattsAvailable = (Entry->PriceWatts > 0)   // :334
```

A Watts payment is rejected at `:346` (shipping: `:398-399`) if and only if
`PriceWatts == 0`. Field comment `AFLCosmeticCoreTypes.h:202` — "0 = NOT directly
Watts-buyable".

**Therefore: `PriceWatts` is the single earnability control in the entire economy.**
There is no tier gate, no band gate, no acquisition gate, no threshold. Dual-priced
items already work with no code change (`:328-334`).

### 1.2 Availability is one bool

`bTransactable` (`AFLCosmeticCoreTypes.h:236`) is read at `AFLWalletComponent.cpp:323`
and `:387`, hard-declining with "not yet available (backend-gated)".

All 27 `AFL.Bundle.*` rows currently carry `bTransactable = false` (CC-READ-3 §1).
**No player can purchase an identity bundle today.**

### 1.3 What is declared but never read

| Field | Declaration | Status in UE |
|---|---|---|
| `MintCap` | `AFLCosmeticCoreTypes.h:241` | **DEAD** — never read by C++ |
| `EAFLCosmeticTier Tier` | `:190`, enum `:89` | **DEAD** — never read |
| `EAFLContentTier ContentTier` | `:196`, enum `:120` | **DEAD** — data-only |
| `bIntactOnlyBundle` | `:246` | **DEAD** — never read |

`Rarity` / `RarityTag` **are** read — `AFLCosmeticCatalogSubsystem.cpp:236-281` — solely to
resolve a badge frame colour (`:264-268`) and label (`:277-280`). Presentation only.

### 1.4 The one real mint cap is in the backend

`BundleMintLedgerTable.MintCap` in `Bag_Man_Backend`, enforced atomically by
`purchase-bundle/index.ts:159-168` via `ConditionExpression: 'MintedCount < MintCap'`.

This is a **different field** from the UE `MintCap`. The UE field is inert; the backend
field gates real mints. Retiring scarcity is therefore a backend work item, not a game one.

---

## 2 · Scarcity is retired

Cosmetic scarcity is removed with no exceptions and no carve-outs.

**Retired:** mint counts as a pricing mechanism · the $1→$500 stretched curve ·
never-discount at Bolt and above · never-reissue · the Grail/$500 band · identities sold as
1-of-1 Singularity bundles.

**Retained:** rarity as a **visual axis only**, which is already all the code does. Badges
and frame colours keep working with nothing behind them. This is standard practice and
requires no change.

**Consequences:**

- Time-limited promotions become permissible. They were previously blocked by
  never-reissue; that rule is gone.
- `MintCap` and `Tier` stay in the struct as inert fields. Removing them is optional
  cleanup, not part of this pivot — nothing reads them, so nothing breaks either way.
- The backend `BundleMintLedgerTable` enforcement must be unwound separately, in
  `Bag_Man_Backend` on `origin/master`. **Do not fold this into a game-repo commit.**

**Founders is out of scope.** It is a beta-tester signup limit on the web, not an economy
object. It has no catalog row, no code branch, and no backend representation
(CC-READ-4 §5). It does not appear again in this document.

---

## 3 · The ladder

One ladder. The R2 cheap-first bands, carried forward as the sole model.

**Peg:** 1 Volt = $0.001 · 10 Watts = 1 Volt.
**Floor:** cheapest paid item = $0.99 = 990 V = 9,900 W.
**Earn rate:** ~4,000 W per match. Earn rates are locked; price is the only lever.

| Band | USD | Volts | Watts | Default earnability |
|---|---|---|---|---|
| Free | $0 | — | — | `GrantedFree` |
| Impulse | $0.99–1.99 | 990–1,990 | 9,900–19,900 | pay-either |
| Standard | $2.99–4.99 | 2,990–4,990 | 29,900–49,900 | pay-either |
| Premium | $7.99–14.99 | 7,990–14,990 | — | Volts only |

**Band sets the price point. Earnability is a per-row product decision.**

This distinction matters and did not exist in the prior doc. A band's "default
earnability" is guidance for populating `PriceWatts`, not a rule the code enforces — the
code only ever reads the row. An item may sit in a pay-either band and still ship
Volts-only by carrying `PriceWatts = 0`. That is a deliberate product choice, and every
such divergence is documented in §4.

---

## 4 · Robot packs

Free players buy robots to use in the character creator. League subscribers do not need to.

| SKU | Price | Robots | Band | `PriceVolts` | `PriceWatts` |
|---|---|---|---|---|---|
| Robot ×1 | $3.00 | 1 | Standard | 3,000 | **0** |
| Robot ×3 | $4.99 | 3 | Standard | 4,990 | **0** |
| Robot ×8 | $10.00 | 8 | Premium | 10,000 | **0** |

**All three ship `PriceWatts = 0` — robot packs are real-money only.** The ×1 and ×3 sit
in a pay-either band and diverge from its default by operator ruling (2026-08-17). Recorded
here as the documented exception §3 anticipates.

The ×3 is priced at $4.99 rather than $5.00 specifically to stay inside Standard. The band
boundary was not moved.

**Ladder math:** ×3 plus the free baseline of 2 reaches 5, matching League's baseline. ×8
plus baseline reaches 10, the hard cap. Neither can overrun the cap.

**The ×1 will not sell.** ×3 costs $1.99 more for two additional robots. It is an anchor,
not a revenue SKU. This is intentional and stated so it is not later mistaken for a
pricing error.

**The ×8 may carry bundled slots as a time-limited promotion** so a $10 purchase yields a
full 10-robot roster. Permissible now that never-reissue is retired (§2).

---

## 5 · Save slots

### 5.1 Ladder

| | Baseline | Add-ons | Ladder cap | Max upgrade |
|---|---|---|---|---|
| Free | 2 | $3 each, up to 3 | 5 | $10 → 10 |
| League | 5 | $3 each, up to 5 | 10 | $10 → 10 |

Hard cap is 10 in all cases.

### 5.2 Three distinct entitlement facts

Keeping these separate is what makes lapse handling tractable.

| Fact | Shape | Precedent |
|---|---|---|
| Baseline (2 or 5) | **Conditional** — derived from sub state | **NONE EXISTS** |
| Purchased slots | **Counted** — permanent, $3 each | **NONE on the cosmetic seam** |
| Max upgrade | **Boolean owned** — permanent, $10 | `OwnedCosmeticIds` |

Effective cap resolves as:

```
MaxUpgrade ? 10 : clamp(Baseline + Purchased, Baseline, TierCeiling)
TierCeiling = 5 (free) | 10 (League)
```

All four values are data. No number is hardcoded.

### 5.3 Two of the three shapes do not exist yet

CC-READ-1 §8.4b, confirmed by both lanes:

- The persistence seam (`AFLEconomyPersistenceSubsystem.h:85-91`) stores counted
  **currency** (`int32 Volts/Watts`) and a boolean **owned-set**
  (`TArray<FName> OwnedCosmeticIds`). There is **no per-cosmetic quantity**.
- Health packs are **not** the counted precedent. They ride Lyra inventory, match-scoped,
  and never touch `IAFLCosmeticPersistence` (`AFLHealthPickup.h:53-60,96-98`;
  `IRONICS_HEALTH_CONSUMABLE_SSOT.md:44,122`).
- **Conditional entitlement precedent: ZERO.** No revoke, expire, or subscription-derived
  grant exists in source or docs.

**Counted-on-the-cosmetic-seam and conditional are both new construction.** This is the
largest engineering item in the pricing pivot and it must not be estimated as conforming
to an existing pattern.

### 5.4 OPEN — the $3 collision

A purchased robot is a creator build. A creator build occupies a save slot. "Buy a robot
for $3" and "buy a slot for $3" are the same transaction at the same price.

Documented here as **one mechanism**: a $3 purchase increments the counted slot
entitlement and grants creator rights to that slot. The ×3 and ×8 packs increment it by 3
and 8.

If robots and slots are intended as genuinely separate products, this section needs an
operator ruling before implementation — as written, two parallel $3 ladders would either
double-charge or double-grant.

---

## 6 · Battle Pass subscription

> **NAMING — RULED 2026-08-27.** `LeaguePlay` is the **free, unstaked** match tier
> (`IsStaked() = Tier != LeaguePlay`, `IRONICS_LEAGUE_DOOR_SPEC.md:14`). The **paid tier is the
> BATTLE PASS**. The collision is resolved by naming the paid product, not by renaming the free
> one — renaming the free tier would have churned gameplay code, the front end and player language
> to fix a commerce-side name.
>
> **ONE PRODUCT — RULED 2026-08-27.** The subscription **and** the Battle Pass are the same
> product: *the subscription grants the current season's pass.* They were previously documented as
> two (a recurring subscription here, a ~8,000 V seasonal pass at `ECONOMY_SPEC` §4). That price is
> now superseded and points here.
>
> **PRECEDENCE, verified 2026-08-27 rather than assumed.** This doc governs pricing: last CONTENT
> commit `a61d9e43` (2026-08-19, +13/−4) against `ECONOMY_SPEC`'s `0c5c88c3` (2026-07-09, +5/−0).
> `ECONOMY_SPEC`'s header cited `1c072a49` (2026-08-05), which is a directory move with zero line
> changes — so the gap is 41 days, not 14.
>
> ⚠ **`ECONOMY_SPEC` §4.1 records an OPEN economy question this ruling creates:** the pass's
> "exactly self-sustaining" payout was tuned to ~8,000 V/season and does not survive a recurring
> price (~15–16k V/season on monthly, ~7.5k on annual). Operator's call; nothing here resolves it.

| Term | Price | Effective annual | Catalog SKU (verified 2026-08-27) |
|---|---|---|---|
| Monthly | $5.00 | $60.00 | `AFL.League.Monthly` — 5,000 V |
| Annual, paid up front | $30.00 | $30.00 | `AFL.League.Annual` — 30,000 V |
| Annual commit, paid quarterly | $10.00 / quarter | $40.00 | `AFL.League.Quarterly` — 10,000 V |

The quarterly path costs $10 more than paying up front. Intentional financing spread,
recorded so it is not later read as an error.

**Catalog prices MATCH this table** at the 1,000 V = $1 peg, verified against
`Docs/reference/catalog-export.json`.

⚠ **The SKU ids and display names still say "League"** — `AFL.League.Monthly`, *"League — Monthly"*
— which now contradicts the naming ruling above. These are **live commerce rows** on the shipping
title, so they are reported here rather than rewritten: changing a live SKU id is a commerce
migration with entitlement consequences, not a rename. **Operator's call**, and the two halves are
separable — display names can be restyled to "Battle Pass" without touching the ids that
entitlements key off.

### 6.1 What the subscription grants

- **The colour continuum.** Continuous hue across the live channels, versus discrete
  colour SKUs for free players. This is the core value proposition — see §7.
- **5 baseline save slots** instead of 2.

**Slots cannot carry the subscription.** A free player reaching 5 slots costs $4.99 once,
against $5/month. The sub must be carried by the continuum.

### 6.2 Lapse rule

**Applied colours stay applied. The ability to change them locks.**

A lapsed subscriber's builds do not mutate, revert, grey out, or disappear. They freeze
exactly as authored, and the hue controls become read-only until renewal. Same shape as a
design tool: your files survive, editing stops.

- Purchased slots and purchased robots **survive a lapse outright** — they are owned, not
  conditional.
- Baseline drops 5 → 2. A lapsed member holding 7 builds keeps all 7; builds beyond their
  effective cap become **read-only, never deleted**. Renewal or slot purchase restores
  write access.

Any rule where a lapse visibly changes a robot the player made will generate refunds and
dominate reviews. This is the single most important behaviour in this document.

---

## 7 · Colour: what is free, what is sold, what is subscribed

Grounded in CC-READ-3 §7 (live/inert channel audit).

**Live colour channels on the X-line chassis (`M_AFL_Character`):**

| Channel | Params | Status |
|---|---|---|
| Neon | `NeonColor`, `NeonIntensity` | LIVE - but NOT driven by the creator |
| Edge | `EdgeGlowColor`, `EdgeGlowMagnitude` | LIVE, creator-driven |
| Chassis albedo | `AlbedoRecolor` (scalar) | NOT A CHANNEL - treatment scalar, see SSOT 3.4 |
| Visor (`M_AFL_Visor_Clean`) | `EmissiveColor`, `BaseTint` + 3 scalars | FULLY LIVE, creator-driven |
| Emblem (`M_AFL_Branding_Decal`) | `NeonColor` | LIVE, proven variable |

`TeamColor`, `EmissiveColor2` and `EmissiveColor3` are **inert** on `M_AFL_Character` - the
inverse of `M_Mannequin`, where `TeamColor` is the colour axis. **CONFIRMED 2026-08-19 by a
graph-connectivity audit** (TASK 0; see SSOT 3.4): each has zero downstream consumers in the
T3D export. `EmissiveColor` (unnumbered) IS consumed and is live - the old "`EmissiveColor1-3`"
phrasing wrongly grouped it with the inert pair.

**THREE creator channels, not four and not five** - and on the X-line chassis, **TWO**.

**RULED 2026-08-20 (CC-X24), and this is no longer a caution but a fact:** the creator's body
colour writes `TeamColor`, which `M_AFL_Character` does not consume, so body colour does not tint
the chassis body. **The creator does not sell body colour on the flagship chassis.** It is shown
disabled with the reason (`FAFLCreatorChannelSchema` reports `PresentButInert`), and the X-line
offers edge and glow only - measured `count=2`.

The obvious escape was checked and does not work: `NeonColor` IS the parameter that would tint the
body, but its only path to `BaseColor` is weighted by `AlbedoRecolor`, measured at `0.0`, so it is
connected-but-silent. Restoring the channel needs the material retarget logged as CC-X25.
Body colour retains value on `M_Mannequin` slot 1 and via `BaseTint` on the visor masters, so it is
not worthless - it is simply not a flagship-chassis body channel. See CREATOR_SSOT 3.4.

**The model:**

- **Discrete colour SKUs remain sellable to free players.** 94 `SKIN_COLOR_EDGE` rows and
  45 `FINISH` rows keep their value — they are the free player's route to a specific look.
- **The continuum is the subscription.** Subscribers get a clamped hue arc across the live
  channels rather than a picker of owned discrete colours.
- **Treatment stays a product either way.** A finish is a multi-channel material recipe
  (`NeonIntensity`, `EmissiveFloor`, `AlbedoRecolor`, `BrandIntensity`), not a hue. No
  slider reaches it.

**Gamut is clamped, not free RGB.** Saturation and value stay inside a neon band. This
protects the brand and, more importantly, protects match readability: created configs
survive team mode by operator ruling (2026-08-17), so body colour no longer carries
friend-or-foe, and near-black or near-white builds would degrade legibility for every
other player.

---

## 8 · Invariants carried forward

Unchanged by this pivot.

- **No cash-out, ever.** Currency is one-way. No path converts Watts or Volts back to
  money or out of the system.
- **No lootboxes or gacha.** All purchases are direct, known-item.
- **Battle pass at exactly self-sustaining** — ~8,000 V, completing the premium track
  returns exactly the next season's pass, no surplus.
- **Server-authoritative balances.** Client displays, server decides.
- **`GrantedFree` auto-ownership** (`AFLWalletComponent.cpp:147-171`).
- Bank / Hyperledger / Bolts / real-money custody remain **gated pending legal sign-off**.

**New gate:** recurring subscription billing is real-money recurring commerce and joins
the existing legal gate. Note the site states 13+; auto-renewal disclosure and
cancellation requirements apply in California, the EU, and elsewhere. Not a blocker for
design; a blocker for taking money.

---

## 9 · Verification owed before the first catalog write

1. **PlayFab real-money check, title `1A2077`.** `bTransactable = false` on all 27 bundles
   makes exposure near-certainly zero, but `BundleMintLedgerTable.MintedCount` is polluted
   by canary and dev buys (`scripts/canary-bundle.ts:71-90`) and cannot answer it.
   PlayFab is the only source distinguishing a real VO top-up from a dev grant.
2. **BP-graph reads of the dead fields.** `MintCap`, `Tier`, `ContentTier`, and
   `bIntactOnlyBundle` are all `BlueprintReadOnly`. No C++ reads them; a store tile WBP
   might. Settle by opening the store WBPs.
3. **`ClientRequestPurchase` / `ServerPurchaseCosmetic` branch.** CC-READ-4 confirms the
   `#if UE_BUILD_SHIPPING` guard at `AFLWalletComponent.cpp:305-307`. One confirming read
   of the widget-side call sites (`AFLW_FrontEndMarket.cpp:476,483,904,906`) before money
   moves.

---

## 10 · Open items

| # | Item | Blocks |
|---|---|---|
| 1 | §5.4 — are robots and slots one product or two? | Slot implementation |
| 2 | Identity roster cut — keep 6, delete the rest, harvest colours, emblems to store | Catalog edit |
| 3 | Backend mint-cap unwind in `Bag_Man_Backend` | Scarcity retirement completion |
| 4 | `AFL.Weapon.Volt` carries `Cosmetic.Identity.BigSixx` — data error, BigSixx tagged twice, VOLT untagged | Catalog hygiene |

---

## Appendix · Rulings carried forward from the retired doc

| Origin | Carried as |
|---|---|
| R2 cheap-first bands | §3, sole ladder |
| R3 floor $0.99 | §3 |
| R4 earn locked, price is the lever | §3 |
| R5 bundle = buy-once→grant-N container | Mechanism retained; Grail pricing retired |
| R6 general-catalog weapon/skin/beam split | Retained; its enforcement claim corrected (§0) |
| R1 roster reclassify | **RETIRED** — identities are no longer sold |
| Original SPARK/SURGE/ARC/THUNDERBOLT ladder | **RETIRED** |
| FLICKER rung | **RETIRED** — absorbed into Impulse |
| §2 rarity mint ladder | **RETIRED** as economy; rarity survives as visual only |
