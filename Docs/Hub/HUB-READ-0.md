# HUB-READ-0 — Editor-connection capability probe

**Session:** CC-E 2026-08-27 (base `ac6dc9c3`) · probe run on scratch assets under `/Game/Dev/CCE_Probe*`,
deleted afterwards; the only dirty package left behind was the open map (probe actor spawn/destroy,
net-zero), discarded on editor close — `git status` on the `.umap` clean.
**Method:** every operation attempted live over the direct editor connection (AIK bridge,
`execute_script`/`execute_python`); PASS/FAIL from the actual call, not assumption. Items marked
MEASURED-PRIOR were not re-attempted because the prior measurement was a crash/freeze; citations given.

## PASS list (the connection can do all of this)

| Operation | Evidence |
|---|---|
| Place / move / delete actors | `EditorActorSubsystem` spawn → move (readback loc=100,100,10000) → destroy, all PASS |
| Set actor properties | `AActor.hidden=True` readback True |
| Set component / CDO properties | This session: `WBP_AFL_Loadout` CDO `DisplayPartMap` set→compile→save, byte-verified on disk |
| Create Blueprint child (Blueprintable parent) | `BP_CCE_Probe` (parent `AActor`) created |
| Create DataAsset | `DA_CCE_Probe` (`LyraPawnData`) created |
| Edit DataAsset properties | `default_camera_mode` set on the scratch DA |
| Create GE Blueprint | `GE_CCE_Probe` (parent `GameplayEffect`) created |
| Create MI | `MI_CCE_Probe2` + `MaterialEditingLibrary.set_material_instance_parent` (readback BasicShapeMaterial) |
| Save assets | `save_directory` PASS; WBP saves this session byte-verified |
| Delete assets | scratch folders removed |
| WBP authoring (tree + slots + compile) | This session: 9 widgets added/configured in `WBP_AFL_Creator`, compile errors=0 via `read_log('compile')` |
| Launch / stop PIE | `LevelEditorSubsystem.editor_request_begin_play()/editor_request_end_play()` — 6 PIE runs this session (armed pre-PIE; no bridge calls during PIE per standing law) |
| Read logs | `read_log('compile')` + post-PIE file reads of `Saved/Logs/Bag_Man.log` |
| Screenshots in PIE | `Shot SHOWUI` console (FScreenshotRequest, game viewport w/ UI) — ScreenShot00008/00013–16 |

## FAIL / AIK-or-operator list (derived, with the measured failure each covers)

| Operation | Status | Mitigation |
|---|---|---|
| Open/switch map via bridge | **MEASURED-PRIOR crash** (`load_level` crashes the editor — memory `reference_bridge_load_level_crashes_editor`); not re-attempted | Relaunch the editor with the map on the command line (`UnrealEditor.exe <uproject> /Game/...`) |
| Material **graph** ops | MEASURED-PRIOR crash family (memory `reference_material_bridge_ops_crash_hardening`) | MI-level parameter/parent writes are fine (above); graph edits → operator/AIK |
| Niagara module internals | Not drivable over the connection (standing) | AIK |
| Tripo / genAI | Not drivable over the connection (standing) | AIK |
| `create_asset` with a **non-Blueprintable** C++ parent | MEASURED-PRIOR modal freezes the game thread + HTTP listener (memory) | Grep UHT gen.cpp for `IsBlueprintBase` before any create; or duplicate a sibling asset |

**Multi-context PIE caveat (measured this session):** with the project's multi-client PIE settings,
`UnrealEditorSubsystem.get_game_world()` returns a context with no local player; console commands
executed on it silently run in the wrong world. Resolve the UI client world by widget census and pick
the lowest `UEDPIE_N` (details: memory `reference_aik_http_direct_client_and_pie_harness`).

**Conclusion:** nothing on the AFL-3002 probe list requires AIK except the four rows above; every
map/placement/data/experience task in `IRONICS_HUB_MAP_BUILD_SPEC.md` M1–M8 is drivable over the
direct connection, with map OPENS done by editor relaunch.
