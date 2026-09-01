# IRONICS START SCREEN + SIGN-IN + ROUTE CHOICE — scope/plan (operator directive 2026-09-01)

> **NAMING (operator ruling 2026-09-01): the GAME is IRONICS.** BAG MAN / Bag_Man is the repo &
> internal codename only — it never appears on a player-facing surface. All screens use the real
> game logo (T_IRONICS_Color_Logo_BC) and game art ONLY — no invented lockups, taglines, or icons.

> Operator, verbatim: "Start Screen- Sign in... a gameplay video high action shot of gameplay in
> Shanty Town... sign and sign up, allow users to stay logged in and remember login and password
> for quick access... After login quick screen giving users choice to go to Lobby or go straight
> to Matchmaking Lobby... at least 3 Gameplay videos in Shanty Town, 1 in INFINEON, and 1 in
> L_Arena_04... start screen should be a 10s high action shot and Hero Shots of Ironics and
> Simularent, Ripsaw, Aria, and Scarlett weapons and our Hand Cannons. looping video intro...
> start screen needs to be in Shanty Town to be clear. Scope, plan, then execute."

**Doctrine gate:** both screens are NEW -> 1:1 mockups for operator approval BEFORE any engine
build (mockup-first, ruled 2026-08-27). Mock canvas published alongside this doc.

## Where this fits the ruled flow (SSOT reconciliation)

- `IRONICS_LOBBY_UX_FLOW_SSOT.md` **N1 Landing** (sign in/up, cold boot only; AFLW_Landing,
  ticket AFL-3024, never built) IS this start screen — we are building the ruled node, not
  inventing one.
- The SSOT's invariant §1.3 ("no menu after login; there is the base") is **AMENDED by dated
  operator ruling 2026-09-01**: one quick ROUTE CHOICE screen after login (Lobby | Matchmaking).
  The SSOT gets this amendment when the screens prove.

## Current reality (recon, verified file:line)

- Boot: `L_IRONICS_Armory` + stock Lyra frontend state component; `PressStartScreenClass =
  W_IRONICS_Startup` (SKIPPED on PC — `ShouldWaitForStartInput()` false), `MainScreenClass =
  W_IRONICS_Home`. The press-start flow step is the natural hook; it must be made to WAIT so the
  landing actually shows.
- Auth: `UAFLOnlineSubsystem` (AFLOnline plugin) auto-logs-in headlessly at GameInstance init.
  Dev = `LoginWithCustomID` (compiled out of shipping); shipping = EOS OIDC
  (`LoginWithOpenIdConnect`). **No email+password, no register, no persistence exist.** Generic
  `PostClientApi` transport is ready for `RegisterPlayFabUser` / `LoginWithEmailAddress`.
- STAY SIGNED IN (security ruling): the password is NEVER stored. Standard PlayFab pattern:
  after email sign-in, `LinkCustomID` binds a locally generated device GUID to the account
  (SaveGame slot, the economy-save precedent); subsequent boots replay `LoginWithCustomID` ->
  one-click entry. Sign-out unlinks + clears the slot. Delivers the operator outcome ("remember
  login and password for quick access") without a credential store.
- Matchmaking route target EXISTS: `UAFLW_Lobby_Root` (League/Staked) -> `StartMatchmaking`.
  Real placements still await T2 S12 (GameLift compute) — routing is buildable now.
- Media: WmfMedia active (mp4 H.264 loops in UMG on Windows today); ElectraPlayer present but
  DISABLED — must be enabled before console/mobile ship (flagged, not needed for PC proof).
  Sequencer + MovieRenderQueue enabled; ffmpeg 8.0.1 on PATH; `Content/Movies/` to be created.

## Phase V — the videos (no approval gate; content work)

**Method:** staged LIVE bot skirmish (BotFill is fully wired) + cinematic camera Level Sequences,
rendered via MovieRenderQueue (PNG sequence) -> ffmpeg -> H.264 mp4. Repeatable, AAA, no
hand-recording required; operator hand-played takes welcome as alternates.

- **V1 — the 10s start loop (Shanty Town, seamless):** storyboard on the mock canvas. Beats:
  wide rooftop firefight -> IRONICS hero close-up -> Ripsaw hero -> SIMULARENT + Aria hero ->
  Hand-Cannon akimbo action -> Scarlett hero -> loop-matched wide. Output
  `Content/Movies/MV_AFL_StartLoop.mp4` + FileMediaSource/MediaPlayer/MediaTexture (looping).
- **V2 — promotional set (files, not cooked):** 3x Shanty Town (district tour · Haywire BR
  action · ProMod parkour run), 1x "INFINEON" (**flag: INFINEON has NO own map — it is the
  stock Expanse arena, retheme owed; footage shows that reality**), 1x ARCANEON (L_Arena_04)
  showcase. 1080p60 targets, ~30-60s each.

## Phase S — the screens (GATED on mock approval)

- **S1 `UAFLW_Landing`** (the SSOT's N1; C++-built like every retail surface): full-bleed looping
  video ground; IRONICS lockup; sign-in card — EMAIL / PASSWORD, SIGN IN (accent CTA),
  CREATE ACCOUNT tab (note: "3 weapon credits on first sign-up" — the proven grant), STAY
  SIGNED IN toggle, SIGN IN WITH EPIC secondary (the shipping EOS path, honest to the backend),
  dev-skip (non-shipping). Auth adds to `UAFLOnlineSubsystem`: `RegisterWithEmail`,
  `LoginWithEmail`, `LinkDeviceForRememberMe`, remembered-boot replay. Flow: frontend press-start
  step forced to WAIT; a valid remembered token auto-signs-in and lands directly on S2.
- **S2 `UAFLW_RouteChoice`:** two doors — OUTPOST LOBBY (the base: shops, range, clubs) |
  MATCHMAKING (straight to the League queue). Routing: Lobby -> existing home/hub flow;
  Matchmaking -> push `UAFLW_Lobby_Root`. Optional "remember my choice" is **PROPOSED** on the
  mock — ratify or strike.
- **S3:** SSOT amendment + tracker rows on PIE proof.

## Build order

1. Mock approval (this canvas) ->
2. V1 sequence + capture rig in Shanty Town (parallel with S1 auth backend) ->
3. S1 landing + remember-me, PIE-proven (dev path) ->
4. S2 route choice, PIE-proven ->
5. V1 loop wired as the landing ground ->
6. V2 promo renders delivered ->
7. SSOT amendment + tracker.

## Open questions for the operator (answer any time; defaults in parentheses)

1. Loop resolution/framerate (1080p60; 4K optional later).
2. "Remember my choice" on the route screen — PROPOSED (default OFF, always ask).
3. INFINEON promo: shoot the stock-Expanse reality now (default) or wait for the retheme?
