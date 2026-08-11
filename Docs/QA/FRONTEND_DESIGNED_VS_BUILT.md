# FRONT END — DESIGNED vs BUILT

**Purpose.** One page that says, per SSOT requirement, what is built, what is built-but-broken, and what
does not exist. Written 2026-08-11 against `Docs/ssot/ui-frontend.md` and verified in source and in the
editor asset registry — **not** from the tracker, which lags.

**How to read the states.** `BUILT` = code exists and was watched working. `DEFECTIVE` = code exists and
does the wrong thing at runtime; the root cause is named. `ABSENT` = no implementation at all, verified by
source search. `UNVERIFIED` = built, never watched.

---

## 1. The scoreboard

| § | Requirement (ruling) | State | Note |
|---|---|---|---|
| 3.2 | Two doors, LEAGUE \| STAKED (**R98**) | **BUILT** | `W_IRONICS_Home`, `EAFLHomeDoor`, test-covered |
| 3 | Split layout, both axes live, no wizard (**R19**) | **BUILT** | `AFLW_Lobby_Root`, one chassis both doors |
| 3.2 | Doors differ by density/motion, never palette (**R100**) | **UNVERIFIED** | never checked against the built screen |
| 4 | Presets primary, numeric secondary, **no slider** (**R20**) | **DEFECTIVE** | presets spawn and wire to `SetStake`, but **their label is never set** — they render the WBP placeholder `{ButtonText}`, so no rung is legible. `UCommonButtonBase` has no text setter in UE 5.6; the preset needs an AFL class with a label setter, or the inner text block set by name |
| 4.2 | Matching band visible, updates live (**R20**) | **UNVERIFIED** | `SetStakeBand`/`BandReadout` exist and are server-resolved; never watched updating live |
| 5 | Population per size and per band (**R21**) | **BUILT** | `AFLMatchPopulationComponent`; rows read "Quiet"/"no estimate" |
| 5.2 | **An empty band must LOOK empty** | **BUILT** | `DetailPanel::ShowNoSelection`, driven unconditionally from `FinishRefresh` |
| 6 | One-tap re-queue, "BATTLE AGAIN" (**R22**) | **ABSENT** | zero matches in source. §6 calls this the highest-frequency moment in the product |
| 7 | Stake cap + session loss limit, legible BEFORE they bind (**R23**) | **ABSENT** | zero matches. S4 TicketReview is the surface that would carry it |
| 8 | Showcase separate, no queue, no venue on exit (**R18**) | **BUILT** | `AFLW_VenueShowcase`; ARCANEON key art wired |
| 9 | Loadout + Store, one chassis two modes | **UNVERIFIED** | built; not exercised this pass |
| 10 | Style system as compiled tokens | **BUILT** | `BS_/TS_/BTN_IRONICS_*`, close-out reported 0 owed |
| 12.0 | CommonUI/UMG, no web runtime (**R75**) | **BUILT** | — |
| 12.3 | **Return routing converges** | **DEFECTIVE** | see §2 below — this is the biggest structural gap |
| 13.2 | Ticket shape, server-authored | **BUILT** | `CommitQueue`, `RequiresTicketReview`; backend authors every attribute |

## 2. Return routing — the one that is systemically wrong

§12.3 requires return routing to converge and be **verified, not assumed**. It was never built as a system.
Each screen was left to solve it alone, and most did not:

| Screen | Back affordance |
|---|---|
| `AFLW_VenueShowcase` | ✅ `BackButton` → `DeactivateWidget` (the only one authored with it) |
| `AFLW_Lobby_Root` | ✅ added 2026-08-11 — was declared `BindWidgetOptional` and **never bound** |
| `AFLW_CareerHub` | ⚠ keyboard only — `bIsBackHandler` added 2026-08-11. **No visible control**, and neither Career WBP contains one |
| `AFLW_CareerRank` | ❌ none |
| `AFLW_FrontEndMarket` | ❌ none |
| `AFLW_LoadoutBase` | ❌ none |
| `AFLW_TicketReview` | ✅ `CancelButton`, required bind, wired |

**The pattern to stop repeating:** `BindWidgetOptional` + never bound is invisible. A missing `BindWidget`
fails at compile; a missing `BindWidgetOptional` is silently null, and a bound-but-unwired button looks
identical to a working one until someone clicks it.

## 3. Defects found this pass, and what each one actually was

Every one of these presented as "dead button" or "missing data". None of them was.

1. **Lobby Back did nothing** — declared, never bound. Fixed `59d9ac0d`.
2. **Doors could not take focus** — `bIsFocusable=False` on both door instances. Fixed `59d9ac0d`.
3. **Queue rows could not take focus** — rows are spawned at RUNTIME, so there is no instance in any WBP to
   fix; it had to be the class. Fixed `29d4a2ec`.
4. **Career could not be exited** — no back affordance existed at all. Keyboard exit added `29d4a2ec`.
5. **HAYWIRE/PRO MOD looked dead** — they worked. Only spawned rows/tiles/presets ever got `SetIsSelected`;
   the fixed axis buttons were bound and never told anything. Fixed `63f14be5`, **and again** after the
   first fix silently failed: `SetIsSelected` is guarded by `if (bSelectable && ...)` and is a **no-op with
   no warning** on a button that is not selectable.
6. **STAKED showed `Text Block` everywhere** — `BroadcastSelection` returned early with no selection, so the
   panel was never written and kept its authored strings. Fixed `63f14be5`.
7. **STAKED listed nothing** — NOT missing data. Twenty staked cells publish. `WBP_IRONICS_Lobby_Staked`
   carries `Door=Staked` in class defaults, so `SetDoor(Staked)` early-returned on `Door == InDoor`, the
   R86 `League = ProMod` correction never ran, League stayed Haywire, and the filter matched 0 of 20.
   Fixed `2134908c`. **A door invariant applied only on the transition is not an invariant.**
8. **Every matchmaking ticket refused** — `Creator` was built as
   `{ Id: <master_player_account>, Type: 'title_player_account' }`, two different namespaces. Fixed and
   deployed, backend `81afad5`.

## 4. What blocks "League is playable"

1. `/create-ticket` now resolves the creator correctly — **deployed, not yet watched returning 200.**
2. Bot fill (`UAFLBotFillComponent`) is **match-side**: it tops an existing session up to bracket size. It
   is not a matchmaking bypass and cannot start a match on its own.
3. So the remaining question is whether a ticket now places a session. Untested.

## 5. What blocks "Staked is ready for staking"

1. **Preset labels** (§1, R20) — presets are unreadable, so a stake cannot be chosen deliberately. **This is
   the top blocker.**
2. **R23 guardrails are ABSENT** — §7 requires the cap and the session limit to be legible *before* they
   bind. Taking real stakes without them ships the engagement driver without the protection that was
   designed alongside it.
3. Band readout live-update unverified.

## 6. What the last 40 commits add that source-reading missed

Reading the history rather than only the code changes the picture in three ways. **This section exists
because the first draft of this document was written from source and the tracker alone, and was wrong or
blind in each of the following.**

**a. There is a live cook / packaging workstream, and it is not front-end.** Eleven of the last forty
commits are one investigation: `be97de4b` (the cook is not reproducible and drops `L_ShantyTown`
DataLayers) → `9629bbd6` (**an ABORTED cook poisons the next one**) → `1ce62186` (abort ruled out as the
cause, DDC is an input not the cause) → `94a791ca` (force-cook the DataLayer roots, 19 of 19) →
`5f5ab3b4` (a check for the DataLayer gap, **because the fix only covers today's folders**). Plus
`076004dd`, `5f3ca71b`, `d6ae77e5`, `1e4c8151`, `86d51f1d`, `0dc03380`. Cook health gates shipping and is
entirely absent from the front-end matrix above — a "100% front end" that cannot cook reproducibly is not
100% of anything.

**b. Two nav decisions were rulings, not omissions.** `5b884d3a` deprecated **HOST** and deferred
**REPLAYS**, driven by a measurement (`86d51f1d` — the HOST strip is real in cook size, and it took
REPLAYS with it). REPLAYS then returned as a Career *tab* rather than a push (`0b8f7702`). So the footer
being five items is a decision with a cost behind it, not an unfinished six.

**c. Kill telemetry is deliberately out of scope.** `103ed783` scopes it as a secondary system —
**analysis only, nothing built** — with the two rulings recorded in `089fdd66`. It should never appear on
a gap list as missing work.

**Corrections to this document's own earlier rows:**

- **The type ramp is RULED** (`3d745e84`), so R100's "type ramp is unapproved" art dependency is closed.
  The §3.2 R100 row above should be read as blocked only on being *checked*, not on a pending approval.
- **The online count is real and self-releasing** (`36c5c77d`, presence heartbeat) and server-resolved
  (`78bafdf4`). The §5 BUILT row is stronger than "a component exists".

---

## 7. Order this suggests

1. Preset labels — staking is not choosable without them
2. R23 guardrails — required before real stakes move
3. A back affordance as a SYSTEM, not per screen — visible control, one pattern, applied to all seven
4. R22 re-queue
5. Then the full PIE pass in `FRONTEND_PIE_PASS.md`, and only then move rows to `PROVEN` in the tracker
