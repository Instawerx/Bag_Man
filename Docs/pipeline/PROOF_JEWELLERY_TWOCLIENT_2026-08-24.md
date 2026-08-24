# FALSIFICATION — two-client first render, stated BEFORE the run

Run configuration: dedicated server + 2 clients, one process, L_Expanse (ProMod).
Production path: ownership via `UAFLWalletComponent::DebugGrantOwnership`, every equip via
`ServerEquipWearable` (re-checks catalog row, re-checks ownership, resolves the slot, delegates to
`ServerSetAccessory`). **No `AddCharacterPart` call and no direct `AccessorySet` write anywhere in the
harness** — a proof that injected parts would be measuring its own injection.

The one deviation, stated rather than buried: ownership is **granted**, not purchased. A purchase
would spend real Volts and make the proof non-repeatable. The grant lands in the same wallet the equip
path re-reads, so the ownership check is still exercised — it is the acquisition that is short-cut,
not the gate.

## What kills the claim

| # | Observation | Layer it names |
|---|---|---|
| F1 | no `AFL_TEST[JEWEL]` lines | harness never fired — VOID, proves nothing |
| F2 | `ABORT -- authority=N` | armed on a client world |
| F3 | arm 1 FAIL — pendant renders with no chain | the dependency is not real; pendant is a free-floating part |
| F4 | arm 1b FAIL — selection lost | equipping a pendant with no chain destroys the selection |
| F5 | arm 2 FAIL | chain attach path dead |
| F6 | arm 2b FAIL — chain at origin | socket resolved but attach fell to root |
| F7 | arm 2c FAIL | ABP not installed; sway is a claim |
| F8 | arm 3 FAIL | chain-owns-pendant broken |
| F9 | **arm 3b FAIL — pendant not below the chain** | pendant parented to mesh ROOT, not the lowest bone. This is the named suspect "a bone chain whose lowest bone is not where a pendant hangs", and it is the arm that separates it from a working hang |
| F10 | arm 4 / 4b FAIL | un-equip does not cascade, or destroys the selection |
| F11 | arm 5 FAIL | re-equip does not restore as left |
| F12 | arm 6 FAIL | wrist side selection broken |
| F13 | arm 6b FAIL — wrists disagree in sign | correction applied to both sides or neither |
| F14 | arm 6c FAIL — one or both face down | correction has the wrong sign |
| F15 | arm 6d FAIL | rigid/skeletal split not real |
| F16 | arm 7 FAIL — `wristParts=3` | the both-full refusal does not hold |
| F17 | **arm 7b FAIL — no reason reached any client** | silent refusal: the player is told nothing |
| F18 | arm 8 `diverged>0` | replication gap between clients; names the world |
| F19 | **arm 9 FAIL** | the observer client does NOT render it on the other player |
| F20 | arm 10 FAIL | dedicated server spawned cosmetics — engine guarantee broken |
| F21 | arm 11 FAIL | sway missing in one or more clients |

## What does NOT falsify it

- **Differing pendant positions between clients.** AnimDynamics is client-local. Divergence is
  CORRECT and arm 11 never fails on it — it prints the positions as evidence of independence.
- **Founders Purps hanging wrong.** Flat mesh, synthetic linear bone chain, marked
  `bPlaceholderArt=true`. It will render; it should not ship. Not read as the mechanism failing.
- **Arm 6c on a moving pawn.** Wrist up-vector is pose-dependent on a live animated character. A
  marginal value is reported, not argued; the operator's eye settles it.
- **Sway amplitude, socket placement, pendant intersection.** Not measurable here.

## Named suspects, carried in

1. **A socket that resolves but is positioned wrong.** Arm 2b only rejects the world origin. A chain
   at a plausible-but-wrong offset passes every arm in this suite. **Eye only.**
2. **A mesh that attaches at the origin.** Arms 2b and 3b catch this directly.
3. **A bone chain whose lowest bone is not where a pendant hangs.** Now has a real arm — 3b requires
   the pendant to sit BELOW the chain's own origin. Structurally `accessory_pendant` is on `chain_04`,
   the lowest bone on both chains; whether `chain_04` is where the art's lowest link is remains eye.
4. **Post-solve ordering measured with NO solver installed.** `SetupIKRig` still has no caller. The
   ordering is proven for a pawn WITHOUT FBIK and is untested with one. If jewellery lags or leads the
   body, this is the first suspect, not AnimDynamics. **Unchanged by this run.**
5. **The pendant socket on Purps sits on a synthetic chain, not a real drape.** Purps is marked
   not-shippable. The proof runs on Founders Link.

---

# RESULT — 2026-08-24, L_Expanse, dedicated + 2 clients, one process

`arms=21 passed=17 PARTIAL/FAIL`. Three findings, each a different kind.

## PASSED (17)

Chain at neck, not at origin, sway installed. Pendant renders ON the chain. Un-equip removes both and
keeps the selection. Re-equip returns both. Same watch sits on both wrists. Watch rigid, bracelet
sways. Third wrist item refused with a reason the CLIENT can read
(`"both wrists are full -- remove one first"`). Both client worlds agree (`clients=2 diverged=0`).
The OBSERVER client renders all four on the other player. Dedicated server spawns no cosmetics.
Sway simulated in every client — `pendantZ=-362.0111` vs `-362.0110`, independent to the 4th decimal.

## FINDING 1 — pendant-alone is REFUSED, not accepted-and-hidden (ruling conflict)

```
[AFLWearable] REFUSED id=AFL.Accessory.Pendant.TTG -- a pendant needs a chain -- equip one first.
```
The ruling is "pendant with no chain -> nothing renders, **selection intact**". The product refuses the
equip outright, so the selection never holds it. Note the asymmetry: that exact state IS reachable by
un-equipping a chain (arm 4b passes, selection kept). So the same state is legal when arrived at one
way and refused when arrived at the other. **Not patched — the harness must not edit the product to
make its own arm pass. Operator's ruling.**

## FINDING 2 — the chain does not hang down; the pendant sits ABOVE the collar

```
3b  chainZ=-346.83  pendantZ=-341.67  drop=-5.16cm
    actorZ=-346.83 compZ=-346.83 root=-346.83
    chain_01=-346.94 chain_02=-347.35 chain_03=-347.70 chain_04=-347.91
    [socket accessory_pendant Z=-341.67]
```
The bone chain spans **18.75cm** of length (measured: ~6.25cm per segment) but descends only
**1.08cm** in world Z from root to `chain_04`. The chain is hanging roughly **3° off horizontal** — it
sticks out from the neck socket instead of hanging on the chest. The pendant socket then lands 5.16cm
**above** the attach point.

This is the named suspect *"a socket that resolves but is positioned wrong"*, and the trace separates
it from the other candidate: `accessory_pendant` IS on the lowest bone, so this is **not** a
wrong-bone problem — the whole chain is oriented wrong.

Two candidate causes, both untested:
1. `accessory_neck` inherits `spine_03`'s frame, whose "down" is not world-down, and nothing corrects
   a chain's orientation at attach (the correction is deliberately wrist-only).
2. AnimDynamics `SimulationSpace = Component` applies gravity along the **component's** -Z, not the
   world's — so the chain "hangs" along the socket's axis.

## FINDING 3 — the wrist correction does not survive the SHIPPING attach path

```
6b  L.up.z=-0.284  R.up.z=+0.284
    SURVIVING relRot L=R(0) R=R(0)
```
`BeginPlay` logs that it applied the correction on both sides
(`base R(R=90.00)`, and `+ right-wrist mirror R(R=180.00)` on the right). The relative rotation read
back at measurement time is **zero on both**. The correction is applied and then reset by something
after `BeginPlay`. `±0.284` is exactly the socket's raw +Z z-component from the original
measurements — i.e. the final transform carries no correction at all.

**Why this was not caught before:** `afl.Test.WristOrientation` passes 3/3 because it does
`SpawnActor` + `AttachToComponent` and calls `ApplyWristCorrection()` explicitly. That is a
convenience path. The shipping path is `UChildActorComponent` + `BeginPlay`, and it behaves
differently. A proof arm that does not exercise the shipping path is blind exactly where it matters.

## Still open, unchanged by this run

Post-solve ordering with a solver installed — `SetupIKRig` still has no caller, so ordering is proven
for a pawn WITHOUT FBIK and untested with one.
