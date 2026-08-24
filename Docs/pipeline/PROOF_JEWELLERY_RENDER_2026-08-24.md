# FALSIFICATION — jewellery render proof, stated BEFORE the run

Written before `afl.Test.JewelProof` is armed. If any of these is observed, the axis is NOT proven
and the run is reported as a failure, not re-interpreted.

## What would falsify the claim "the accessory axis renders"

| # | Observation that kills it | What layer it names |
|---|---|---|
| F1 | `AFL_TEST[JEWEL]` absent from the log entirely | The harness never fired. VOID — proves nothing either way |
| F2 | `ABORT -- authority=N` | Armed on a client world; the authority arming fix regressed |
| F3 | Arm 1 FAIL, chain absent at `accessory_neck` | Attach path dead — consumer, part class, or socket |
| F4 | Arm 1b FAIL, chain at world origin | Socket name resolved but attach fell through to root |
| F5 | Arm 1c FAIL, no post-process instance | ABP not installed; sway is a claim not a fact |
| F6 | Arm 2 FAIL **with `[settled in N]`** | Pendant genuinely absent — the chain-owns-pendant design is broken |
| F7 | Arm 2 FAIL **with `[TIMED OUT]`** | Different bug: slow arrival, not absence. Do not conflate with F6 |
| F8 | Arm 3 FAIL | Un-equip does not destroy the pendant with its chain |
| F9 | Arm 3b FAIL | Un-equipping the chain destroyed the pendant SELECTION — data loss, worst case |
| F10 | Arm 4 FAIL | Re-equip does not restore as left |
| F11 | Arm 5 FAIL | Wrist side selection broken |
| F12 | Arm 5b FAIL | Watch swaying or bracelet rigid — the rigid/skeletal split is not real |
| F13 | Arm 6 FAIL, `wristParts=3` | The both-full refusal does not hold; `bWristEitherSide` is decorative |
| F14 | **Arm 7 `diverged>0`** | Replication gap — a client disagrees with the authority. Names the world |
| F15 | Arm 7 `clients=0` | Ran standalone; the replication question was never asked. NOT PROVEN |

## What would NOT falsify it

- Sway amplitude looking wrong. Not measurable here; operator's eye, and explicitly out of scope.
- Socket *placement* looking wrong (chain inside the ribcage). Arm 1b only proves "not at origin".
- Purps looking flat. It is marked `bPlaceholderArt` and is expected to look provisional.

## The suspect list, carried in

Named before the run so a single FAIL points at a layer instead of starting a search:

1. **A socket that resolves but is positioned wrong.** Arm 1b cannot see this — it only rejects the
   origin. A chain at a plausible-but-wrong offset passes every arm.
2. **A mesh that attaches at the origin.** Arm 1b/2b catch this directly.
3. **A bone chain whose lowest bone is not where a pendant hangs.** Measured: both chains carry
   `accessory_pendant` on `chain_04`, which IS the lowest bone. Structurally satisfied — but whether
   `chain_04` is where the art's lowest link actually is remains an eye question.
4. **Post-solve ordering measured with NO solver installed.** `SetupIKRig` still has no caller, so the
   earlier `0.0000` delta proves ordering holds for a pawn *without* FBIK. Untested with one. If
   jewellery lags or leads the body, this is the first suspect, not AnimDynamics.
5. **The pendant socket sits on Purps's synthetic chain, not a real drape.** Now marked
   `bPlaceholderArt=true`. Founders Link is the piece the pendant claim rests on.

## Correction to the stated basis for the Purps mark

Measured, and it does not support two of the three stated reasons:

```
                              bone segments        bend between segments   mesh min/max extent
SK_BagMan_Chain_FoundersLink  6.250/6.250/6.255    0.65deg, 2.89deg        0.154
SK_BagMan_Chain_FoundersPurps 6.250/6.263/6.283    3.66deg, 9.48deg        0.183
```

Both chains are evenly-divided ~6.25cm vertical bone chains — that is what the conform recipe
produces, so "synthetic linear bone chain" describes **Founders Link equally**. And Purps is
marginally *less* flat than Link, not more.

So the mark rests on the operator's visual judgement of the piece, which is authoritative and is
exactly the kind of call the eye owns. It does **not** rest on a measurable defect, and no measurable
criterion currently separates Purps from the accepted Founders Link. If "synthetic linear bone chain"
is genuinely disqualifying, Founders Link is disqualified by the same measurement.

---

# RESULT — run 2026-08-24 08:05, L_Expanse, PIE_Client x2, one process

`AFL_TEST[JEWEL] END arms=13 passed=13 PASS`, `worlds=3` (1 dedicated + 2 clients).

| Arm | Result | Evidence |
|---|---|---|
| 1 chain at neck | PASS | renderPawn=B_Hero_BagMan_Pro_C_0, settled in 1 tick |
| 1b not at origin | PASS | loc=(-4695.75, 250.00, -342.68) |
| 1c sway installed | PASS | post-process AnimDynamics live |
| 2 pendant on chain | PASS | both clients p=1, settled in 2 ticks |
| 2b pendant not at origin | PASS | loc=(-4696.28, 230.02, -346.63) |
| 3 un-equip removes both | PASS | chainGone=1 pendantGone=1 |
| 3b selection retained | PASS | selection pendant=AFL.Accessory.Pendant.TTG |
| 4 re-equip restores | PASS | chain=1 pendant=1 |
| 5 both wrists occupied | PASS | wristL=1 wristR=1 |
| 5b watch rigid / bracelet sways | PASS | watchRigid=1 braceSways=1 |
| 6 third wrist refused | PASS | wristParts=2 |
| 7 all client worlds agree | PASS | **clients=2 diverged=0** |
| 7b server spawns no cosmetics | PASS | Lyra suppresses on NM_DedicatedServer |

None of F1–F15 was observed.

The pendant re-drive under test fired on both clients independently:
`RedriveAccessoryChains on B_Hero_BagMan_Pro_C_0: attachedActors=3 chains=1` (pid=256) and
`on B_Hero_BagMan_Pro_C_4: attachedActors=3 chains=1` (pid=257).

## What this run does NOT establish

It is not `✅` under this project's doctrine. Nothing was watched by a human on a controllable pawn.
Specifically still open and unanswerable from a log:

1. **Socket placement.** Arm 1b only rejects the world origin. A chain at a plausible-but-wrong offset
   passes every arm in this suite.
2. **Wrist orientation in motion.** The correction is asserted numerically; whether a watch face reads
   up on a moving arm is a look.
3. **Sway amplitude.** Confirmed installed and running; whether it reads as jewellery is a judgement.
4. **Pendant hang.** Whether it hangs from the lowest link or intersects the chain mesh.
5. **Post-solve ordering with a solver installed.** `SetupIKRig` still has no caller. The ordering is
   proven for a pawn WITHOUT FBIK and remains untested with one.
