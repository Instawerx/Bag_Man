# RUN CARD — Jewellery render proof (CC-8)

**Lane:** Operator-watched PIE. Per CLAUDE.md, `✅` means *watched in PIE on a controllable pawn* —
the harness cannot award it, and does not try to.
**Agent activity during the run: ZERO.** No MCP calls, no console injection, no log reads while PIE
is up. Everything below is typed by the operator. Logs are read only after PIE stops.

---

## 0. Preconditions

| # | Check | Why it is here |
|---|---|---|
| 0.1 | Editor built from the commit carrying `RedriveAccessoryChains` | The pendant fix is the thing under test; an older DLL proves the old behaviour |
| 0.2 | Editor opened **after** that build | A hot-reload DLL makes a green build a lie (banked) |
| 0.3 | Level = `L_Expanse` | Runs ProMod, which pulls `LAS_AFL_ExtractionMatch` — the action set that grants `AFLAccessoryPartComponent`. Reachability confirmed on `B_Experience_ProMod` and `B_Experience_Haywire` |
| 0.4 | All ten jewellery pieces imported, part BPs carry meshes, rows carry `AccessoryPartClass` | Verified by spawn read-back, not by CDO read (CDOs do not carry BP SCS templates) |

---

## 1. Setup — the two-client dedicated configuration

In the editor toolbar, **Play** dropdown:

- **Net Mode:** `Play As Client`
- **Number of Players:** `2`
- **Run Under One Process:** **ON** ← *not optional*

`Run Under One Process` is what puts the server world and both client worlds in the same process.
The harness enumerates `GEngine->GetWorldContexts()`; separate processes cannot see each other, and
arm 7 — the replication arm, the entire reason for a two-client run — would report `NOT PROVEN`.

---

## 2. Arm, then play

In the **editor** console, **before** pressing Play:

```
afl.Test.JewelProof
```

Expect: `AFL_TEST[JEWEL] ARMED (authority) -- waiting for a SERVER-side pawn; giving up after 60s.`

Then press **Play**. On the server pawn appearing you should see:

```
AFL_TEST[JEWEL] firing on authority world <name> (netmode=...)
AFL_TEST[JEWEL] BEGIN on <pawn> (playerId=N)
```

If instead you see `GAVE UP after 60s -- no AUTHORITY pawn appeared`, stop: the run configuration is
wrong (almost certainly `Run Under One Process` is off), not the product.

**Let it run to `AFL_TEST[JEWEL] END`** — roughly 15–25 seconds. Do not stop PIE early; the FSM is
what drives the equips, and a stop mid-run reports the clock rather than the product.

---

## 3. What the harness asserts (you do not need to watch these)

| Arm | Claim |
|---|---|
| 1 / 1b / 1c | Chain present at `accessory_neck`, not at world origin, post-process sway instance installed |
| 2 / 2b | Pendant present at `accessory_pendant` — on the **chain's** mesh — and not at origin |
| 3 / 3b | Un-equipping the chain removes chain **and** pendant, while the selection still holds the pendant id |
| 4 | Re-equipping the chain restores both |
| 5 / 5b | Both wrists occupied; watch rigid, bracelet swaying |
| 6 | A third wrist item is refused — still exactly 2 |
| **7** | **Every world holding this player agrees on which sockets are occupied** |

Each arm now prints per-world occupancy (`[World:netmode n= p= wl= wr=]`) and either
`[settled in N ticks]` or `[TIMED OUT after N ticks -- absent, not slow]`. A FAIL that says TIMED OUT
and a FAIL that says settled are different bugs; the line tells you which.

**Arm 7 is the one this run exists for.** It is what would have named the pendant re-drive defect
directly instead of leaving it as an unexplained FAIL on arm 2.

---

## 4. What only your eye can answer

The harness proves plumbing. These are yours, and none of them can be logged:

1. **Socket placement.** The chain resolves at `accessory_neck` and is not at the origin — that does
   not mean it sits on the collarbone rather than inside the ribcage.
2. **Wrist orientation in motion.** The `+90 / +180` correction is asserted numerically at 3/3, but
   whether the watch face reads *up* on a moving arm is a look, not a number.
3. **Sway amplitude.** `AnimDynamics` is confirmed installed and running. Whether the chain swings
   like jewellery or like rope is a judgement.
4. **Pendant hang.** Whether it hangs from the lowest chain link or intersects the chain mesh.
5. **The second client's view.** Walk client 1 up to client 2 and look at the jewellery on the
   *observed* player — this is where the pendant defect would have been visible to a human.

---

## 5. Standing render-only suspects — carry them into the run

Named in advance so the single proof is diagnostic rather than binary:

- **Post-solve ordering when a solver is actually installed.** `SetupIKRig` still has no confirmed
  caller, so the earlier `0.0000` delta may only prove the solver was off. If jewellery visibly lags
  or leads the body under FBIK, this is the cause — not AnimDynamics.
- **LOD.** `LODThreshold = 1` per asset. Sway should stop at LOD 2+. Back away from a mirror/second
  player and confirm it degrades rather than pops.
- **The shared canary-named skeleton (CC-X42).** The canary gen shares a namespace with product
  output. If a piece looks like the wrong mesh, suspect this before suspecting the conform.
- **URO.** `bEnableUpdateRateOptimizations` interacting with AnimDynamics at distance is *unmeasured*
  — an open question, not a finding.

---

## 6. Pass criteria

- `AFL_TEST[JEWEL] END arms=N passed=N PASS`, **with `clients=2 diverged=0` on arm 7**, and
- operator sight-check on §4 items 1–5 with no visual defect.

Anything less is `PARTIAL` and is reported as such. A green harness with an unwatched pawn is not
`✅` under this project's doctrine, and will not be recorded as one.

---

## 7. Abort conditions

| Symptom | Meaning | Action |
|---|---|---|
| `ABORT -- loadout=MISSING` | Action set not granted on this experience | Wrong level/experience — check §0.3 |
| `ABORT -- authority=N` | Fired on a client world | Should be impossible now; report it, it means the authority arming regressed |
| `ABORT -- hard cap 3600 ticks` | FSM outlived ~58s | Report the phase number in the message; that phase never settled |
| `NOT PROVEN: no client world` | Standalone PIE | `Run Under One Process` off, or Net Mode not `Play As Client` |
