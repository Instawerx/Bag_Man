# IRONICS_LOBBY_UX_FLOW_SSOT

**Status:** NEW. **This is the governing document of the Phase and Lobby System upgrade.** It is the
operator's flow diagram (`Lobby_Upgrade_Doc.docx`, page 1) transcribed edge for edge. Every other
document in the programme — SSOT, map spec, roadmap, tasks, creator plan — implements a node or an
edge of this flow and cites it. A phase gate is a set of edges walked, not a list of features shipped.
**Date:** 2026-08-27
**Rule:** the diagram is not reinterpreted. Where a node's *screen* is not drawn, the screen is
whatever already exists behind that system. Where an arrow's *mechanism* is not drawn, §3 names the
existing seam that carries it. One reading is marked **[CONFIRM]** in §1.2 because the arrows admit
two meanings; the operator settles it in one line.

---

## 1 · The flow as drawn

```
                    ┌──────────────────────────────────────────┐
                    │   LANDING PAGE — Sign in or Sign Up      │
                    │   Backdrop: 3D Map Shot                  │
                    └───────────┬──────────────────┬───────────┘
                      NEW PLAYER│                  │RETURN PLAYER
                                ▼                  ▼
                   ┌─────────────────────┐  ┌──────────────────────────┐
                   │ Load Wallet with    │  │ Wallet · Load Last Set   │
                   │ Free Starters       │  │ Character                │
                   └──────────┬──────────┘  └────────────┬─────────────┘
                              │  ⇅                        │  ⇅
 ═════════════════════════════╪══════════════════════════╪══════════════════════
 ║  SUPER LOBBY MAP — "OUTPOST EARTH MWR" (Main Zone)     ▼                    ║
 ║  Walkable · interactable · chat · players & friends shown ·                  ║
 ║  wear accessories · carry weapons · NO FIRE except authorised zones          ║
 ║                                                                             ║
 ║   ROBO LABS    PATRIOT CAFÉ    EM LOUNGE    LOADOUT BARRACKS                 ║
 ║                                                                             ║
 ║   TOURNAMENTS  SHOOTING        PX STORE                  DEPLOYMENTS         ║
 ║   MINI GAMES   RANGE                                                        ║
 ═══════╪═════════════════════════════════════════════════════╪════════════════
        │ enters tent/zone                                     │ walk in
        ▼                                                      ▼
 ┌──────────────────────────┐                     ┌──────────────────────────┐
 │ TOURNAMENTS / CHALLENGES │                     │ LEAGUE LOBBY │ STAKED   │
 │ PLAY  ──────►  ENDS      │                     │              │ LOBBY    │
 └────────────┬─────────────┘                     └─────────────┬────────────┘
              │                                                 ▼
              │                                     ┌──────────────────────┐
              │                                     │ ASSIGNED MATCH / ENDS│
              │                                     └──────────┬───────────┘
              └──────────────► back to SUPER LOBBY ◄───────────┘
```

### 1.1 Nodes

| # | Node | Type | Screen / state the player is in |
|---|---|---|---|
| N1 | **Landing Page** | Full-screen menu | Sign in / Sign up over a 3D map shot of the base. Cold-boot entry only. |
| N2 | **New Player → Load Wallet with Free Starters** | Transition | First wallet read grants the free starter set (`GrantedFree` auto-ownership). Player is told what they got. |
| N3 | **Return Player → Wallet · Load Last Set Character** | Transition | Wallet read + persisted selection read-back; the player's last build is on them before they see the base. |
| N4 | **Super Lobby Map — Outpost Earth MWR** | Persistent world (dedicated server) | Third-person, walkable, chat, nameplates for players and friends, accessories worn, weapons carried, fire blocked. |
| N4a | Robo Labs | In-lobby destination | Creator (Track C) — build / save / equip. |
| N4b | Patriot Café | In-lobby destination | Social club, first flavour. |
| N4c | EM Lounge | In-lobby destination | Social club, second flavour. |
| N4d | Loadout Barracks | In-lobby destination | Owned assets displayed like the store; equip. |
| N4e | Tournaments Mini Games | **Outbound** door (tent/zone) | Leaves the hub → N5. |
| N4f | Shooting Range | **Outbound** door (ruled 2026-08-26: separate map) | Leaves the hub → range map → returns. Fire authorised there. |
| N4g | PX Store | In-lobby destination | All assets displayed and buyable; try on / hold; mirrors; jewellery counter, weapons, masks, stickers, robots. |
| N4h | Deployments | **Outbound** door | Leaves the hub → N6. |
| N5 | **Tournaments / Challenges Play → Ends** | Experience | Plays; **ends**; returns to N4. |
| N6 | **League Lobby / Staked Lobby** | Existing matchmaking surface | The League/Staked door as it exists today. |
| N7 | **Assigned Match / Ends** | Existing match server | Plays; **ends**; returns to N4. |

### 1.2 Edges

| Edge | From → To | Meaning as drawn | Mechanism (§3) |
|---|---|---|---|
| E1 | N1 → N2 | New player branch | Identity says first login → wallet seed |
| E2 | N1 → N3 | Return player branch | Identity says known account → persistence read-back |
| E3 | N2 ⇅ N4 | New player enters the base (double-headed: the base is also where you spend the starters) | Hub join |
| E4 | N3 ⇅ N4 | Return player enters the base with last set character | Hub join |
| E5 | N4 → N4a…N4d, N4g | Walk into an in-lobby destination; its screen opens over the world | Destination volume → PushWidget |
| E6 | N4e → N5 | Enter tent/zone → transported to tournament / mini map / partition map | Destination volume → ExperienceTravel |
| E7 | N5 → N4 | **Ends → back to the Super Lobby** | Return-to-hub |
| E8 | N4f → range → N4 | Range door → range map → exit → back | ExperienceTravel + return |
| E9 | N4h → N6 | Walk into Deployments → League or Staked lobby | Destination volume → the existing door widget |
| E10 | N6 → N7 | Matchmaker assigns a match | Existing FlexMatch → match server travel |
| E11 | N7 → N4 | **Match ends → back to the Super Lobby** (two arrows drawn: one lands by the PX, one by Deployments — both mean "back in the base") | Return-to-hub |

**[CONFIRM — one reading]** The double-headed arrows E3/E4 land at different places on the drawing:
the new-player arrow meets the base at **Robo Labs**, the return-player arrow at **Patriot Café / EM
Lounge**. Read literally: **a new player's first stop is Robo Labs** (build your first robot with the
free starters), and **a returning player lands in the social base** wearing their last set character.
If that is the intent, E3 becomes "hub join → auto-walk/prompt to Robo Labs on first entry" and the
Landing's new-player branch says so. If the arrows were placed for layout only, both branches land
on the spawn plaza. Default if silent: **the literal reading** (first-time players are routed to Robo
Labs).

### 1.3 The two loops — the invariant

Every arrow that leaves the base comes back to the base. **There is no menu after a match or a
tournament; there is the base.** The Landing Page is entered once per boot, never on return. This is
the single most important property of the flow and every gate from H2 onward proves it.

**AMENDED (operator ruling 2026-09-01, screens passed live):** one screen now sits between the
Landing and the base — the **Route Choice** quick screen (`AFLW_RouteChoice`: "WHERE TO?" —
Outpost Lobby | Matchmaking). It is asked **every session** (login saves; the route choice does
not), shown once per boot immediately after sign-in, never on return from a match. Matchmaking
routes through the home screen's own League door (pending-route flag consumed on activation), so
the queue path stays the one proven door wiring; back/default = the base. The Landing itself is
now `AFLW_Landing` — Epic sign-in over the 10s Shanty Town action loop
(`/Game/Movies/MT_AFL_StartLoop`), stay-signed-in persisted via EOS PersistentAuth prefs. The
once-per-boot property is unchanged: Landing + Route Choice together are the boot-only pair.

---

## 2 · What each node shows (UI, per node)

| Node | UI surface | Exists today? | Programme item |
|---|---|---|---|
| N1 Landing | `AFLW_Landing` on `UI.Layer.Menu`: sign in / sign up (existing identity), 3D map-shot backdrop, one primary action **Enter Base** | Front-end map exists; landing widget does not | AFL-3024 |
| N2 / N3 | Transitional card over the backdrop: "Starter kit loaded" (lists the free set from the owned-set) / "Welcome back, <name>" with the last build rendered (display pawn, Track C parity) | Wallet seed and read-back exist; card does not | AFL-3024 |
| N4 | Hub HUD: zone prompt, nameplates, chat, friends marker; world-space door prompts | None | H1.6, H4.1–4.2 |
| N4a Robo Labs | `AFLW_Creator` — the six regions | Partially (A/B/F bound 2026-08-26, WBP pending) | Track C3 |
| N4b/c Lounges | Club door prompt → club UI (create / join / privacy) | None | H4.3–4.4 |
| N4d Barracks | `AFLW_Loadout` product page on racks | Old loadout widget exists | Track C4, M7 |
| N4e/f/h doors | World prompt + confirm; N4h opens the existing League/Staked door widget | Door widget exists | H2.1–2.2, M5 |
| N4g PX | Pedestal interact → `AFLW_ProductPage` (try-on / hold / buy) + mirrors | Old market widget exists | Track C5, M7 |
| N5 | Whatever the tournament / mini-game surface is; ends → E7 | Not built (S19 lane) | H6.1; door reads `Disabled` until then |
| N6 | The existing League/Staked door widget, unchanged | Yes | H2.2 |
| N7 | The existing match; match-end summary; E11 | Yes | H2.3 |

---

## 3 · Edge → existing seam (nothing new invented)

| Edge | Carried by | Cited / owed |
|---|---|---|
| E1/E2 | Existing identity (IRONICS accounts; EOS OAuth when C2 green) | HUB-READ-3 |
| E2/N3 | `IAFLCosmeticPersistence` read-back → `FAFLCosmeticSelection` on spawn | `#43` seam |
| E1/N2 | `GrantedFree` auto-ownership on first wallet read | `AFLWalletComponent.cpp:147-171` |
| E3/E4 | `UAFLHubJoinSubsystem::RequestHubJoin()` → hub shard (dev IP in H2, `/hub/join` in H5) | AFL-3022, AFL-3051 |
| E5 | `AAFLHubDestinationVolume` → `PushContentToLayer_ForPlayer` (the `afl.Store.Open` pattern) | AFL-3020 |
| E6/E8 | `AAFLHubDestinationVolume` → Lyra experience travel with `ReturnToHub=1` | AFL-3020, AFL-3025 |
| E7/E11 | Match/experience-end client path → `RequestHubJoin()` instead of front-end menu | AFL-3022 |
| E9 | Deployments door → the existing League/Staked door widget class | AFL-3021, HUB-READ-2 |
| E10 | Existing FlexMatch flow, untouched | Guardrail #1 |

---

## 4 · Gates are edges

| Gate | Edges that must be walked, watched, no cheat |
|---|---|
| **H1** | N4 exists: two players on Outpost Earth as their own robots, walk between N4a–N4h footprints, no fire |
| **H2** | **E1→E3 or E2→E4 → E8 (range and back) → E9 → E10 → E11** — the right-hand loop closed, plus E5 for Labs/Barracks/PX opening their current widgets |
| **C3** | N4a: the creator flow ENTRY→CHASSIS→BUILD→SAVE→EQUIP, then E9→E10→E11 shows the build in a match |
| **H3** | N4g and N4d: try on / buy / equip at a pedestal; owned assets equipped from Barracks; both survive E11 |
| **H4** | N4b/N4c: club formed, visibility masked; party walks E9→E10 as one ticket; both return via E11 |
| **H5** | E3/E4 through the real join service with friend-follow; 64-client soak; E10 unaffected |
| **H6** | **E6 → N5 → E7** — the left-hand loop closed; then the operator walks the whole diagram end to end |

---

## 5 · Decisions

| # | Decision | Default |
|---|---|---|
| 1 | §1.2 [CONFIRM]: new players are routed to Robo Labs on first entry; returning players land in the social base | Literal reading — yes |
| 2 | N2/N3 transitional cards: shown as cards over the backdrop before Enter Base, or as a toast on arrival in the base | Cards before Enter Base |
