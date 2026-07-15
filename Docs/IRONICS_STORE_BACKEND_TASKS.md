# IRONICS Digital Market — Backend Tasks (deferred, ship-gating)

Status log for the server/backend work the store needs before **shipping** purchases work.
The UE-side store (browse · preview · rich card · per-tile BUY/EQUIP · OWNED flip) is **PIE-proven**
against a **dev grant** path; the items below are what makes real (PlayFab) purchases work in a cooked build.

_Last updated: 2026-07-14 (B2c buy-loop PIE-green)._

---

## 1. [BLOCKER] Seed the PlayFab `AFL_Main` catalog with the purchasable cosmetics
**Symptom today:** a tile BUY → `ClientRequestPurchase` → PlayFab returns
`http=400 "ItemNotFound" errorCode=1047` → `[Wallet] PurchaseItem(...) REJECTED`.

**Cause:** PlayFab `AFL_Main` holds only the **3 test items + 1 beam** (`AFL.Test.Token`, etc.).
None of the real cosmetics exist there. (See memory: *PlayFab catalog = 3 test items*.)

**Do:** create one PlayFab catalog item per purchasable `CosmeticId` — the **177 purchasable** entries:
WeaponSkin 43 · Beam 43 · Bundle 29 · Facemask 22 · Finish 21 · Body 10 · Edge 5 · Weapon 3 · Ability 1.
Each with the Volts price (VC `VO`) and, on SPARK/dual items, the Watts price (VC `WA`) — matching
`FAFLCatalogEntry.PriceVolts` / `PriceWatts` in `DA_AFL_CosmeticCatalog`.

**Interim (in code, done):** `UAFLW_FrontEndMarket::HandleStoreTileBuy` uses
`#if UE_BUILD_SHIPPING → ClientRequestPurchase` (PlayFab) `#else → ServerPurchaseCosmetic` (local-authority
grant, compiled out of shipping). So dev/PIE works now; **shipping requires this catalog.**

## 2. Price parity — one source of truth
Keep PlayFab item prices in sync with `DA_AFL_CosmeticCatalog` (`PriceVolts`/`PriceWatts`). Prefer a
**generator** that emits the PlayFab catalog from the DA so the two never drift (the store card + the
charge both read the same numbers).

## 3. Honor `bTransactable` (inert gate)
Entries flagged `bTransactable=false` must not be purchasable even once seeded (the shipped-inert gate).
Confirm the seed script skips / disables them.

## 4. Bundle fulfillment
A `AFL.Bundle.*` purchase must grant all `ContainedEntitlementIds` (the child cosmetics). Verify the
PlayFab bundle grants the children on purchase (not just the bundle id).

## 5. Purchase confirm / failure UX (front-end, optional polish)
Today a rejected buy is silent. Add a confirm + a failure toast (e.g. "insufficient funds" / "not
available") so the player gets feedback. Success already flips the tile → OWNED.

## 6. Anti-spoof verification (already UE-complete)
The anti-spoof ladder (A1.2–A1.4, OWED = live grant) is done UE-side — re-verify it holds once the real
catalog is seeded (server validates price/ownership before grant).

---

### Not backend (tracked elsewhere, UE-side polish still open)
- Button spec finish: 2px lit border · hover/pressed/disabled states · ⚡ bolt glyph · Watts button · EQUIP button.
- Card polish: neon-glass rarity bottom bar · piping frame · gradient · uniform spacing (to the reference mockup).
- Chrome: wallet pills · player card · bottom bar.
