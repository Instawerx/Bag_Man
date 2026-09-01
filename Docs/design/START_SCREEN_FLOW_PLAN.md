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
- Auth (CORRECTED by operator 2026-09-01 + website docs): **EPIC SIGN-IN IS ACTIVE** — the
  website portal anchors accounts as `epic#<sub>` (`lib/accounts.ts`) and the game anchors the
  SAME Epic identity via `LoginWithOpenIdConnect` connection `epic`, title `1A2077`
  (IRONICS_VOLTS_PURCHASE_ADMIN_SCOPE.md:158-163). One identity spine, both sides.
- **GAMELIFT-OVER-PLAYFAB PRIORITY (operator ruling):** no NEW PlayFab surface. The earlier
  email+password direction (RegisterPlayFabUser et al.) is STRUCK — sign-in and sign-up both run
  through Epic (Epic's own flow covers account creation; the OIDC login already carries
  CreateAccount:true, which fires the 3-credit recruit grant on first sign-in).
- STAY SIGNED IN = **EOS PersistentAuth** (the platform-standard refresh-token credential):
  Epic's SDK persists the auth locally and replays it on boot — one-click re-entry, no password
  ever stored, sign-out clears it. "Login just saves" (operator ruling, verbatim intent).
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
  showcase. 4K loop; promos 1080p60+, ~30-60s each.

## Phase S — the screens (GATED on mock approval)

- **S1 `UAFLW_Landing`** (the SSOT's N1; C++-built): full-bleed looping video ground; the real
  IRONICS logo; sign-in card — **SIGN IN WITH EPIC primary CTA** (the active identity path;
  first sign-in creates the account + fires the 3-credit recruit grant), STAY SIGNED IN toggle
  (EOS PersistentAuth), dev-skip (non-shipping). Auth surface change is SMALL: expose the
  existing EOS OIDC path behind a user-initiated button + PersistentAuth persistence — no new
  PlayFab APIs. Flow: frontend press-start step forced to WAIT; persisted Epic auth auto-signs-in
  and lands directly on S2.
- **S2 `UAFLW_RouteChoice`:** two doors — OUTPOST LOBBY (the base) | MATCHMAKING (the queue).
  Routing: Lobby -> existing home/hub flow; Matchmaking -> push `UAFLW_Lobby_Root`.
  "Remember my choice" STRUCK (operator: not necessary — login saves, the choice is asked).
- **S3:** SSOT amendment + tracker rows on PIE proof.

## Build order

1. Mock approval (this canvas) ->
2. V1 sequence + capture rig in Shanty Town (parallel with S1 auth backend) ->
3. S1 landing + remember-me, PIE-proven (dev path) ->
4. S2 route choice, PIE-proven ->
5. V1 loop wired as the landing ground ->
6. V2 promo renders delivered ->
7. SSOT amendment + tracker.

## Rulings received (operator, 2026-09-01)

1. Loop = **4K**. 2. Remember-my-choice: **struck** — login saves, route is always asked.
3. INFINEON: **good to shoot as is**. 4. Epic sign-in ACTIVE (website) = the primary auth path;
**GameLift/AWS services preferred over PlayFab** wherever a choice exists.
