# HUB-MOVEMENT-QUALITY — the deep look (operator mandate 2026-08-30)

**Ruling:** "Players will not willingly spend time in our hub if it is not full fledge top tier
movements and interactions." The hub's movement bar is therefore the ARENA bar: walking Outpost
Earth must feel indistinguishable from walking a match map. This doc scopes the gap.

## 1 · Measured root causes (not guesses)

| # | Symptom | Root cause | State |
|---|---|---|---|
| 1 | "Pulls / sluggish / pushed against while walking" | TWO-LAYER. (a) **AFL-3012 net throttle on the player's own pawn** (15 Hz / floor 5) — sparse corrections yanking prediction. (b) After (a) was fixed the feel persisted **only in Play-As-Client PIE**: the harness runs a full client-prediction loop plus TWO worlds on one machine. **Standalone walk = "AAA smooth" (operator-watched 08-29) — map and pawn EXONERATED.** The residual is the networked-client path. | (a) **FIXED** — player-controlled pawns keep engine rates; crowd/NPC throttle retained. (b) **OPEN → W-NET below.** Measurement pass armed: `p.NetShowCorrections 1` + AFL_SPRINTDIAG role-split log in a client-mode run. |
| 2 | "Pull and drag when we stop running" | Subsumed into #1(b) pending the client-mode measurement — the stop-drag was reported in the same Play-As-Client harness that standalone proved clean. | **FOLDED into W-NET** — re-test standalone verdict covers the stop too; if a stop-specific drag survives on a networked client only, it's the sprint-toggle correction window. |
| 4 | "No double jump / space does nothing" | Casualty of the broken-session states (deleted floor + dead cue layer sessions); every static+runtime layer probed clean (grant present, CanJump true, scripted Jump() rose 410u on the 900 JumpZ). | **PROVEN 08-30 operator-watched: jump AND double jump work** (JumpMaxCount=2 was already authored). |
| 5 | W-NET verdict | Scripted client-mode walk = flat 700 plateau; operator no-jump sprint = ramp to 980 HELD; corrections = 0 across all runs. | **CLOSED — hub networked movement certified clean.** Residual polish only: WallJump per-frame refire noise (shares IA_Jump, needs a Pressed gate), landing/stop feel tuning. |
| 3 | Couldn't enter tents/containers | Pack shells sealed openings with crude hulls (one box per entry tent). | **FIXED** — per-poly collision on all 10 shell meshes; operator-confirmed enterable. |

## 2 · Workstreams to the AAA bar

- **W-NET · Networked-client movement quality** (the live workstream; shipping hub = dedicated
  server, so client feel is the product). Suspects, in order: (1) the sprint MaxWalkSpeed swap is
  tag-event-driven on both sides but NOT part of the saved-move reconciliation — every toggle has
  a replication-skew window where client predicts 980 against a 700 server cap → correction pull;
  the AAA fix is a saved-move compressed flag (or a GetMaxSpeed override reading shared state).
  (2) Dual-world single-machine PIE frame cost — a harness artifact, not a product bug; real
  client verification needs a separate-process client. Measurement: client-mode PIE with
  `p.NetShowCorrections 1` + AFL_SPRINTDIAG (role-split TOLD-vs-DOING): corrections during plain
  walk = base desync; corrections clustered at sprint = suspect (1); zero corrections = suspect (2).
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

## 2.5 · FINAL VERDICT (2026-08-30, operator-certified: "client matches standalone now")

The lane is CLOSED. Real defects fixed en route: AFL-3012 player-pawn net throttle (exempted);
the parkour family net-desync (WallRun/Slide direct-drive CMC on both sides — REMOVED from the
hub, global netcode ticket stands); zero air braking (BrakingDecelerationFalling 0→1200,
FallingLateralFriction 0→0.35 — the downhill "slide until something stops us"); the editor's
bThrottleCPUWhenNotForeground (froze frames at 125ms/8fps whenever multi-window PIE focus
flickered — disabled). The residual "wacky feel" that survived every clean trace was the
HARNESS: single-process Play-As-Client runs two engine worlds in one process where window
focus decides frame delivery. **LAW: feel-test networked client movement ONLY with
RunUnderOneProcess=False** (separate processes — shipping-shaped). Final A/B on a fresh
machine: client sprint pinned 980 @ inputFrac=1.00, 101–106fps, indistinguishable from
standalone — watched and certified by the operator.

## 3 · Acceptance (the gate for "hub movement AAA")

- Operator walks hub then an arena back-to-back and calls them indistinguishable.
- No visible correction snaps at walk/sprint/stop on a net-PIE client.
- Full plaza + tent grid + container corridor walk with zero catches or velocity dips.
- All doors/prompts (AFL-3405) reachable and responsive on the first try.
