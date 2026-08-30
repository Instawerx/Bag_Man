# HUB-MOVEMENT-QUALITY — the deep look (operator mandate 2026-08-30)

**Ruling:** "Players will not willingly spend time in our hub if it is not full fledge top tier
movements and interactions." The hub's movement bar is therefore the ARENA bar: walking Outpost
Earth must feel indistinguishable from walking a match map. This doc scopes the gap.

## 1 · Measured root causes (not guesses)

| # | Symptom | Root cause | State |
|---|---|---|---|
| 1 | "Pulls / sluggish / pushed against while walking" | **AFL-3012 net throttle on the player's own pawn**: `HubNetUpdateFrequency=15`, floor 5 Hz, applied to every hub pawn including the local player. A player-controlled character corrected at 5–15 Hz produces sparse server corrections yanking the client's prediction — the exact reported feel. The helper-doc target was written for *crowd bandwidth*, not the owner's correction loop. | **FIXED** — player-controlled pawns keep engine rates (possession-robust deferred decision); the throttle still lands on future crowd/NPC actors. Quantisation + zone-cull stay for everyone. Documented AFL-3012 deviation. |
| 2 | "Pull and drag when we stop running" | Animation tuning, not components (duplicate-component census: clean; one of each movement/IK comp). Candidates: distance-matched stop vs the Pro sprint speed (980), FBIK ground tug at deceleration. | **OPEN** — discriminator: does the same drag show in a ProMod ARENA match? Same pawn + ABP there. Yes → movement-overhaul polish lane owns it. No → hub-specific (this map's surfaces/net) and it stays here. |
| 3 | Couldn't enter tents/containers | Pack shells sealed openings with crude hulls (one box per entry tent). | **FIXED** — per-poly collision on all 10 shell meshes; operator-confirmed enterable. |

## 2 · Workstreams to the AAA bar

- **W1 · Net posture, tiered not blanket** (AFL-3012 evolution). When hub crowds arrive, throttle
  by *distance/relevancy tier* (near players full-rate, far players stepped down), never blanket.
  The zone-cull machinery already in the component is the right chassis. Player-controlled pawns
  are never throttled, period.
- **W2 · Stop/locomotion polish.** Run the #2 discriminator first. If hub-owned: audit ground
  friction/braking vs arena values, FBIK enable states in the hub, and the stop-anim distance
  curves against the Pro sprint speed. Acceptance = operator watched side-by-side with an arena.
- **W3 · Collision smoothness sweep.** The per-poly shells fixed entry; a full walk sweep should
  hunt micro-catches: prop bases, tent rope stakes, container lips, terrain seams. Method: a
  scripted perimeter walk logging velocity dips >30% without input change, then fix the named
  meshes (bevel colliders / walkable-slope tweaks), operator walk to close.
- **W4 · Interactions** (AFL-3405+): destination doors with prompts at the ratified doorways;
  every interactive surface reachable without collision fights.
- **W5 · Test methodology.** Net-PIE client feel ≠ standalone feel ≠ shipping feel. Every
  movement-feel judgment gets stated WITH its harness (standalone / listen / client). The
  regression pair for any tuning change: one standalone walk + one net-PIE client walk.

## 3 · Acceptance (the gate for "hub movement AAA")

- Operator walks hub then an arena back-to-back and calls them indistinguishable.
- No visible correction snaps at walk/sprint/stop on a net-PIE client.
- Full plaza + tent grid + container corridor walk with zero catches or velocity dips.
- All doors/prompts (AFL-3405) reachable and responsive on the first try.
