# HANDOFF — 2026-08-08

**Read this first in a new thread.** Everything below is committed and pushed to `personal/main`.
Last commit at handoff: `d20ca894`.

---

## 1. THE FOUR LIVE FRONTS (tasks #23–#26, plus #20)

| # | Front | State | Blocked on |
|---|---|---|---|
| **#20** | **Phase 3 spine — escrow/settle/rating round-trip** | Code is COMPLETE end to end. Never verified live. | **4 env vars** — see §2 |
| #23 | `W_IRONICS_Home` layout | Scaffold built, structurally correct, **no layout** | UMG designer pass (human) |
| #26 | BR publication | All 3 original gates CLOSED | Operator combat sign-off |
| #24 | D2/D3 district fences | Data layers exist, D1 works | Content authoring |
| #25 | ShantyTown river | Course solved (889m, 63pts, max depth 336cm) | Bed sampling must be REAL, never interpolated |

---

## 2. #20 IS BLOCKED ON ENVIRONMENT, NOT CODE — START HERE

The whole chain is already built and wired: `UAFLOnlineSubsystem` exposes `PostServerEscrow` /
`PostServerSettle` / `PostServerRating` (+ EOS OIDC login, shipped `015270ef`), and
`AFLGameCore/Private/Match/AFLMatchReporter.cpp` calls all three across the match lifecycle
(escrow at lines 342/463, settle 520, rating 538).

**It silently no-ops** because `IsMatchReportingConfigured()` (`AFLOnlineSubsystem.cpp:591`) requires four
values that are read from the **process environment at subsystem Initialize** (`:120–125`):

```
AFL_EARN_HMAC_KEY     AFL_ESCROW_URL     AFL_SETTLE_URL     AFL_RATING_URL
```

⚠ **They are read ONCE at startup, so the editor must be LAUNCHED with them already set** — setting them
in a running editor does nothing. They are deliberately absent from the game repo: the HMAC key is a
secret and lives in `Bag_Man_Backend`, never here.

**Verification sequence once they are set:**
1. Launch editor with the four vars in the environment.
2. Log line at startup reports `held` / the URLs, or `MISSING` per value — check this FIRST, it is the
   cheapest possible confirmation and it prints on every boot.
3. Console `afl.Match.Result.Test` → log `AFL_RESULTTEST: ... ALL GREEN`.
4. PIE a staked match to completion; confirm escrow → settle → rating each POST and return 2xx.
5. Cross-check the ledger in `Bag_Man_Backend` (mint-ledger + escrow rows already proven server-side,
   commits `c74dda6` / `b31a7a3`).

---

## 3. ARCHITECTURAL DIRECTION ACCEPTED 2026-08-08 (operator cheat sheet)

Three structural calls, **accepted as direction**:

1. **CommonUI tokens must emit CONCRETE STYLE ASSETS.** CommonUI resolves styling from compile-time
   `UCommonTextStyle` / `UCommonButtonStyle` / `UCommonBorderStyle` subclasses — it does **not** read a
   generic Data Asset at runtime. The `@ironics/tokens` compiler must run a factory pass that instantiates
   and saves real style assets. (This independently matches a prior audit finding.)
2. **`PreLogin` is the only safe identity gateway.** Parse travel options and reject the handshake there,
   before a pawn materialises.
3. **AWS API Gateway WebSockets over IoT Core** for the live channel — JSON route keys map natively to the
   existing Lambdas and reuse the current HMAC signatures without device certificates.

### ⚠ THE SAMPLE CODE IN THAT CHEAT SHEET HAS FOUR DEFECTS — DO NOT PASTE IT

| Defect | Why it breaks |
|---|---|
| It defines a **new `AAFLGameMode`** | One already exists at `AFLGameCore/Public/AFLGameMode.h`. The hooks belong in OURS; a second class collides. |
| `PreLogin` rejects on missing `PlayFabId` | **Locks out every PIE session and listen-server host**, none of which set it. Must be gated to dedicated/authenticated contexts. |
| `PostLogin` parses options from `PlayerState->GetSavedNetworkAddress()` | That is not the options string; it returns empty. Capture options in `PreLogin`/`Login` and carry them. |
| `UClass::TryFindTypeBitwise`, `time.get_ticks()` | Neither exists (UE / Python respectively). The JS client is also truncated mid-literal. |

---

## 4. EDITOR-BRIDGE FACTS — DO NOT RE-DERIVE THESE

Cost most of a session to establish. Full detail in `design/IRONICS_HOME_SCREEN_SPEC.md` §2.2.

- **UMG layout is NOT scriptable.** Slot properties (alignment/padding/fill), the `Visibility` enum, and
  widget animations are all unreachable — every form tried returns `0 changes`. A screen can be built and
  named by script but **must be laid out by a human in the designer.**
- `add_widget` **ignores its name argument**; widgets get `<Class>_<N>`, BP classes `<Asset>_C_<N>`.
  Counters **reset per asset**. Rename in a second pass.
- **`compile()` reports SUCCESS even when `BindWidget` bindings FAIL** — UMG logs those as *warnings*.
  The only reliable check is grepping the editor log for `required widget binding`.
- A successful rename does **not** prove parenting. Decisive test: `remove_widget(parent)`, then check
  whether the child still resolves.
- `configure_widget` takes **PascalCase** property names only.

## 5. OTHER TRAPS LIVE RIGHT NOW

- **`ExperienceOverride` silently hijacks every PIE.** In-memory clear only; reloads from
  `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` on each editor restart. Tell:
  `Identified experience ... (Source: DeveloperSettings)` in the log. It cost one wasted PIE this session.
- `W_Nameplate.uasset` is modified in the tree by a **separate background session** — do not touch.
- **NEVER STAGE:** `IK_ParkourPack` · `RTG_ParkourPack_to_Manny`.
- Editor **CLOSED** for any C++ build; **C: launcher engine, not D: source**.
- PIE: start/stop via the bridge is fine; **zero bridge calls while it runs**; read logs only after stopping.

---

## 6. WHAT SHIPPED THIS SESSION

- `8b2bcefd` **BR field size** — `?FieldSize=N` from the playlist. Was 35 bots in a 9-player match, because
  `Target = TeamSize × NumTeams` and one 36-team solo set is shared by all brackets. Proven live through
  the front end: `target 9`, 8 bots, `Source: OptionsString`.
- `7db89bb5` **`UAFLW_HomeScreen`** — R98's "a league player never picks a stake" held by
  `IsStakeLegalForDoor` + 3 automation tests under `AFL.Home.*`, all green headless.
- `d20ca894` **`W_IRONICS_Home`** scaffold — correct structure, no layout. Front-end wiring **reverted**.
- `5ebebac9` **R100** — the two doors are not colour-coded. Also corrected a bot-fill diagnosis that was
  wrong in the opposite direction to what the doc claimed.
