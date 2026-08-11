# PIE PASS — IRONICS FRONT END

**Why this exists.** Eight front-end surfaces are `COMMITTED` in Tier 3 and **none is `PROVEN`** —
nothing has been watched in PIE. This is the checklist that converts them, so the pass is a gate
rather than a vibe.

**Map:** `/Game/BagMan/Armory/L_IRONICS_Armory` — `GameDefaultMap` in `Config/DefaultEngine.ini`, and
what the game actually boots. Do not use `L_LyraFrontEnd`; find the front end by reading
`GameDefaultMap`, never by searching for a map named "FrontEnd".

**How the home screen is reached (verified 2026-08-11):**
`L_IRONICS_Armory` World Settings → `DefaultGameplayExperience = B_LyraFrontEnd_Experience` →
adds `/Game/UI/B_LyraFrontendStateComponent` → its `MainScreenClass` is **`W_IRONICS_Home_C`**.
So the stock-named Lyra experience is what serves OUR home screen; the IRONICS surface is reached by
a repointed `MainScreenClass`, not by a differently-named experience.

⚠ **A black editor viewport on this map is EXPECTED and is not a failure.** The home screen is a
widget pushed by the experience at runtime — it exists only under PIE. Judge this surface from PIE,
never from the editor viewport.

⚠ **`B_IRONICS_Armory_Experience` is ORPHANED** — referenced by nothing but `/Game/DefaultGame_Label`.
No map points at it. It is not what serves the front end and must not be assumed to. Its status is
an open question, not a blocker for this pass.

**Rules of the pass**
- `PROVEN` means **watched on screen**, not read from a log ([[proven-is-watched]]). A green log line
  is not a pass.
- **Do not run MCP/bridge calls while PIE is live.** Do all tooling before Play or after Stop.
- One surface at a time. If a check fails, record what was seen and stop — do not carry a suspected
  failure into the next surface, because the second symptom will not be isolatable.
- A check that cannot be performed is recorded as **NOT TESTED**, never as a pass.

---

## 1. Home screen — R98

| # | Check | Pass condition |
|---|---|---|
| 1.1 | Boot to front end | The IRONICS home screen renders, not Lyra's default front end |
| 1.2 | Two doors, exactly | LEAGUE PLAY and STAKED PLAY are present. There is no third door and no neutral/None state |
| 1.3 | Doors navigate | Each door opens its own lobby. LEAGUE never asks for a stake amount anywhere behind it |
| 1.4 | No palette split (R100) | The two doors differ by density/motion/content only. Neither door is colour-coded; Violet stays rim/glow only and never fills core or text |

## 2. Footer nav

| # | Check | Pass condition |
|---|---|---|
| 2.1 | Five destinations live | All five footer items route to a real surface |
| 2.2 | No dead clicks | Every item visibly responds. Watch for the CREDITS-style dead click specifically |
| 2.3 | Return routing converges (§12.3) | Backing out of each destination returns to the same place, and the home screen is reachable from all five |

## 3. S1 LobbyRoot — both doors

| # | Check | Pass condition |
|---|---|---|
| 3.1 | Split layout, not stepped (R19) | Size axis and queue list are **both live and editable at once**. No wizard, no forced order |
| 3.2 | Presets primary (R20) | Preset tiles are the primary control; the numeric field is secondary. **There is no slider** |
| 3.3 | Ladders are per tier (R69/R88) | WATTS shows `250 / 1,000 / 5,000 / 25,000 W`; VOLTS shows `100 / 500 / 2,500 / 10,000 V`. Neither shows a converted figure from the other |
| 3.4 | Matching band visible (§4.2) | Entering a stake shows the band it will match into. The exact figure is never presented as guaranteed |
| 3.5 | Population is real (R21/§5) | Counts and wait show per size and per band |
| 3.6 | **An empty band looks empty (§5.2)** | A band with nobody in it reads as cold and still shows its real count. It must **not** hide, grey out, or silently redirect |
| 3.7 | Wallet | Real balances render, and the not-known-yet state displays as itself rather than as zero |

⚠ 3.4 is **server-resolved**. In a single-client PIE it may not exercise a real band. If so, record
**NOT TESTED** — do not pass it on a client-side placeholder.

## 4. S2 QueueDetail

| # | Check | Pass condition |
|---|---|---|
| 4.1 | Payout ladder renders | Figures appear for the selected size and stake |
| 4.2 | Solved, not typed | Changing stake or size changes the ladder consistently. A static ladder is a fail |

## 5. S4 TicketReview

| # | Check | Pass condition |
|---|---|---|
| 5.1 | Staked confirms before commit | The staked path shows the ticket before entering a queue |
| 5.2 | League does not | The league path never presents a stake ticket |
| 5.3 | Ticket carries no venue (R18) | Nothing on the ticket names or implies a chosen venue |

## 6. S8 VenueShowcase

| # | Check | Pass condition |
|---|---|---|
| 6.1 | Three venues listed | NANOWATT, ARCANEON, INFINEON — ARCANEON appears **once**, not once per playlist |
| 6.2 | ARCANEON art renders | `T_Venue_Arcaneon` shows on the tile and full-bleed in the detail panel |
| 6.3 | **Empty slots hide the image** | Selecting NANOWATT or INFINEON shows **no image** — never the previously selected venue's art under a new venue's name. This is the specific regression the soft pointer exists to prevent |
| 6.4 | Text stays readable over art | Class label, name and blurb remain legible over the lower third of the ARCANEON image |
| 6.5 | **§8 INVARIANT — no queue** | There is no control anywhere on this surface that enters a queue, and no route out carries a venue. **This is the invariant the whole surface exists to hold; if it fails, nothing else here matters** |

## 7. Career hub

| # | Check | Pass condition |
|---|---|---|
| 7.1 | Hub opens with tabs | REPLAYS is a **tab**, not a push |
| 7.2 | RANK renders ladders | League standings display |

## 8. Throughout — regression watch

| # | Check | Pass condition |
|---|---|---|
| 8.1 | `W_Nameplate` log spam | Known `OPEN`: repeated `Accessed None … CallFunc_GetDynamicMaterial_ReturnValue`. Note whether it still fires; it is loud enough to bury real errors |
| 8.2 | Message log otherwise clean | No new Blueprint runtime errors introduced by the front-end surfaces |

---

## Recording the result

For each row: **PASS** (watched), **FAIL** (with what was seen), or **NOT TESTED** (with why).
Only surfaces whose rows all pass move to `PROVEN` in `Docs/LIVE_TRACKER.html`. A partially passing
surface stays `COMMITTED` — the tracker has no partial state, and inventing one would be a design
decision in a status file.
