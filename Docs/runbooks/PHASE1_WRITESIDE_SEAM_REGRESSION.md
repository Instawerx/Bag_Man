# Phase 1 — Write-Side Seam Consolidation: Regression Recipe

**Change (refactor-behind-interface ONLY):** the two authoritative PlayFab writes — earn (`/earn`
Lambda) and purchase (`Client/PurchaseItem`) — were moved out of `UAFLWalletComponent`'s inline
path and now route through `IAFLCosmeticPersistence` (`EarnThroughBackend` / `PurchaseThroughBackend`),
implemented in the existing `UAFLEconomyPersistenceSubsystem`. This brings the WRITE side to parity
with the already-seamed LOAD side. **Same transport, body, endpoint, completion, anti-spoof.** Zero
behaviour change is the claim; this recipe is how we falsify it before committing.

## Files touched (4)
- `AFLCosmeticServices.h` — +2 completion delegates (`FAFLOnEarnComplete`, `FAFLOnPurchaseComplete`),
  +2 pure-virtuals on `IAFLCosmeticPersistence`.
- `AFLEconomyPersistenceSubsystem.h` — +2 override decls.
- `AFLEconomyPersistenceSubsystem.cpp` — +2 impl bodies (transport moved here VERBATIM; +Guid/DateTime includes).
- `AFLWalletComponent.cpp` — both call sites now call the seam; the completions (AFL_A13S3 earn log /
  purchase REJECTED-log + `ApplyPurchaseResult`) stay wallet-side and fire identically.

**Byte-preserved (NOT in the diff):** `ApplyPurchaseResult` (Option-A local mirror), `CommitMutation`,
`PersistState`, `SaveBalance`, `SaveOwnedSet`, `ServerPurchaseCosmetic`, all 6 existing seam methods,
and the `VerifyA12` dev spoof-probe (a separate inline `PostClientApi` at wallet `:520` — out of scope,
intentionally left inline; it probes PlayFab directly to PROVE the spend wall).

## The two known micro-deltas (both benign, both on unreachable defensive branches)
On the REAL path (dedicated server with AFLOnline present, client logged in) behaviour is byte-identical:
same JSON body on the wire, same endpoint, same `AFL_A13S3 earn ok` / purchase completion. The only
differences are on branches that don't execute in the shipping config:
1. **Earn, `Online == null`:** old code was silent (inside `if (Online)`); new impl calls
   `OnComplete(false, "")` → wallet logs `AFL_A13S3 earn FAIL` (empty resp). Unreachable — earn is
   `IsRunningDedicatedServer`-gated and AFLOnline is always present there. Outcome (no grant) unchanged.
2. **Purchase, `Online == null`:** old code `Fail("AFLOnline unavailable")`; new impl calls
   `OnComplete(false)` → wallet logs the REJECTED line + `OnComplete(false)`. Same outcome (denied).
   Also the wallet now `Fail("persistence seam unavailable")` if the SEAM is null — but the seam is a
   GameInstanceSubsystem always present in AFLCombat. Unreachable.

Neither changes a grant, a spend, a balance, or the wire contract.

## PIE regression (launcher editor)

### THE production-seam proof — `afl.Online.VerifyPurchaseSeam` (NEW; the one that exercises the relocated code)
The gap the old trio missed: **none of them drives the relocated `PurchaseThroughBackend` via the production
entry.** `afl.Online.VerifyA12` uses an INLINE PlayFab probe (`A12_TryBuy`), NOT `ClientRequestPurchase`, so
it never touches the refactored path. `afl.Online.VerifyPurchaseSeam` closes that: it drives the REAL entry
`ClientRequestPurchase → PurchaseThroughBackend → PlayFab → ApplyPurchaseResult` end-to-end, buying the
transient-injected `AFL.Test.Token` (10 VO — the only affordable PlayFab-backed item; no shipping cosmetic is
in the PlayFab AFL_Main catalog, which seeds only the test items + one owned beam).

- **Fixture:** run on the seeded `AFL_DEV_TEST_01` account (VO 1234 after `npm run setup:economy`, idempotent).
  For a **pristine 0→1 grant**, revoke `AFL.Test.Token` from `DF7C…` first (PlayFab Admin/RevokeInventoryItem);
  otherwise it is a stackable RE-BUY (the verify sums `RemainingUses` so a re-buy still reads as granted, but a
  revoke makes `grantedOnPlayFab=1` unambiguous). The verify injects the catalog entry itself (dev-only, transient).
- **PASS (`AFL_TEST[SEAM] PASS`):** `seamAccepted=1` (the production entry drove `PurchaseThroughBackend` and
  PlayFab accepted) · `serverVO −10` (server deducted) · `grantedOnPlayFab=1` (token units up) · `mirrorDeducted=1`
  (`ApplyPurchaseResult` fired) · `spendSpoofRejected=1` (an over-priced Premium buy through the SAME entry REJECTED).
- **Log evidence to cite:** `LogAFLOnline: [AFLOnline] PurchaseItem -> http=200 ok=1` (token, via the seam) then
  `http=400 … InsufficientFunds` (Premium spoof) — the relocated transport firing on the production path.

### Regression guards — the other verifies still pass exactly as pre-refactor
| Console cmd | Proves | Pass = |
|---|---|---|
| `afl.Online.VerifyA11` | Load path still resolves the seam subsystem + reads from PlayFab (account-not-machine) | `bFromPlayFab=1`, balance/owned match the account |
| `afl.Wallet.VerifyA13a` | Earn-forge wall: a client CANNOT forge an earn (server/HMAC only) | forged earn REJECTED; no client-side balance bump |
| `afl.Online.VerifyA12` | Spend-spoof wall via the INLINE probe (unchanged path — regression guard only) | over-spend / price mismatch REJECTED server-side |

> NOTE: on the shared `DF7C…` account, `VerifyA11`'s hardcoded `want VO=1234` and `VerifyA12`'s grant sub-assert
> can read FAIL purely from **fixture drift** (VO decremented + token already owned by prior runs) — re-run
> `npm run setup:economy` to reset VO→1234 before judging them. Those FAILs are not the refactor.

## Live-dedicated earn (next D:\ source-cooked dedicated-server session — THE transport proof)
The earn `/earn` transport only runs on a dedicated server (`IsRunningDedicatedServer` gate). Cook the
dedicated server from `D:\UE5.6-source` (NOT the launcher — ABI split), launch, run the extraction earn
funnel with `DevCustomId AFL_DEV_TEST_02`, TWO earn batches as before. The proof line must be
**byte-identical** to the pre-refactor output:

```
AFL_A13S3 earn ok pid=B5FCB1DE6A5A4203 +N newBal=M
```

Same pid (resolved via `GetResolvedPlayFabId`, A1.4 anti-spoof preserved — the seam carries the resolved
`PlayFabId` FString, NOT the login key), same `+N` delta, `newBal` advancing across the two batches.
If that line appears unchanged, the /earn transport survived the move intact.

## Gate to commit
Two halves prove the two relocated transports through their PRODUCTION paths:
1. **PURCHASE (this session, PIE):** `afl.Online.VerifyPurchaseSeam` → `AFL_TEST[SEAM] PASS` — the production
   `ClientRequestPurchase → PurchaseThroughBackend` fired to PlayFab (`http=200`), deducted+granted, mirror
   reflected, spend-spoof rejected. Plus the three regression guards (A11/A13a/A12) green on a reset fixture.
2. **EARN (next D:\ source-cooked dedicated session):** the `AFL_A13S3 earn ok pid=… +N` line reproduced —
   the `/earn` transport now living in `EarnThroughBackend` survives the move.

Only when BOTH are watched green, commit the code files:
- Phase-1 refactor (4): `AFLCosmeticServices.h`, `AFLEconomyPersistenceSubsystem.{h,cpp}`, `AFLWalletComponent.cpp`.
- Production-seam proof (5): `AFLCosmeticCatalogSubsystem.{h,cpp}` (dev transient-inject), `AFLWalletComponent.{h,cpp}`
  (`DebugVerifyPurchaseSeam` + `bSeamAccepted`), `AFLCombatCheats.cpp` (`afl.Online.VerifyPurchaseSeam`).

This recipe doc is a separate uncommitted artifact — not part of the code commit. Push game repo → `personal`. No co-author trailer.
