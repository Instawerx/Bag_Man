# PROOF — signup grant + credit redemption, live

Run 2026-08-24 against the deployed `/counted-entitlement` and the live `bagman-counted-entitlements`
table. **Every counter is read back from DynamoDB, never from the handler's response** — a response is
the endpoint's claim about itself.

**The account was created fresh** (`Client/LoginWithCustomID`, `CreateAccount=true`,
`NewlyCreated=true`) so "a new account" is literal rather than "an account we reset".
Test account `1B4A8AE2938CB4F3`, left in place as evidence.

## Falsification, stated before the run

| # | Would falsify |
|---|---|
| F1 | a brand-new account already has a counter row |
| F2 | grant-once does not put the counter at exactly 3 in DynamoDB |
| F3 | a SECOND grant-once changes the counter, or reports `granted=true` |
| F4 | three redemptions do not walk 3 → 2 → 1 → 0 in DynamoDB |
| F5 | the fourth redemption succeeds, or moves the counter off 0 |
| F6 | a redeemed weapon is not actually in PlayFab inventory |

## Result — 6/6

```
1 a new account has NO counter row              PASS  dynamo=null
2 grant-once puts the counter at 3              PASS  dynamo=3   {"count":3,"granted":true}
3 a RETURNING account is NOT re-granted         PASS  dynamo=3   granted=false  seededAt=1787613108
4 three redemptions walk the counter to 0       PASS  Tempest:200->2 Vanguard:200->1 Breacher:200->0
5 the FOURTH is refused and the counter holds   PASS  409 NO CREDITS  dynamo=0
6 every redeemed weapon is really in inventory  PASS  3/3 delivered
```

**Arm 3 is the one that could only be proven live.** Idempotency is
`ConditionExpression: attribute_not_exists(SeededAt)` in DynamoDB — a local mirror cannot show it, and
a second login granting again is exactly the silent failure that would hand out infinite weapons.
`SeededAt=1787613108` is the marker planted by the first call and refused against by the second.

**Arm 6 is not decoration.** `GrantItemsToUser` returns HTTP 200 for an ItemId that does not exist,
reporting the failure inside a per-item row. A credit spent on an unresolvable target is the
join-defect shape at the redemption layer, so delivery is asserted from inventory rather than status.

This run also closes the two arms the previous pass correctly reported NOT PROVEN against a local
mirror — pack increments the counter (arm 2), redeem to zero and the next refused (arms 4 and 5).

## What this run does NOT establish

"A new account holds IRONICS on both axes, the sponsor hand cannons, and nothing else" is **game-side
state, not backend state**, and is not proven here:

- IRONICS on both axes is `ApplyDefaultIdentityIfUnset()` writing `FAFLCosmeticSelection` on the
  authority. It never touches PlayFab.
- The sponsor hand cannons are `GrantedFree` catalog rows — auto-owned in the wallet, never granted
  through PlayFab. The fresh account's PlayFab inventory was correctly EMPTY before redemption, which
  is consistent with that but does not demonstrate it.

Both are observable in a PIE session on a controllable pawn and remain owed.
