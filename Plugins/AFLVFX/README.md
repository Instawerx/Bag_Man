# AFLVFX stubs — placement & purpose

These are **committable starting-point files** so the agent EXTENDS real code instead of
authoring from zero. Download and drop the `Plugins\` contents into your repo:

```
C:\Dev\Bag_Man\Plugins\AFLVFX\            <- the code module (cue + interface)
C:\Dev\Bag_Man\Plugins\AFLVFXLibrary\     <- content-only plugin (optional; see runbook)
```

After dropping `AFLVFX\`, regenerate project files and build (the runbook prompt does this):
```
<UE>\Engine\Build\BatchFiles\Build.bat LyraEditor Win64 Development -Project="C:\Dev\Bag_Man\Bag_Man.uproject"
```

## What's here

| File | State |
|---|---|
| `AFLVFX/AFLVFX.uplugin` | ready — runtime module, deps GameplayAbilities + Niagara |
| `AFLVFX/Source/AFLVFX/AFLVFX.Build.cs` | ready |
| `.../Public/AFLVFX.h`, `.../Private/AFLVFX.cpp` | ready — module impl |
| `.../Public/AFLLaserVisualProvider.h` | ready — the weapon→cue interface (BlueprintNativeEvent) |
| `.../Public/AFLCueNotify_LaserBeam.h` + `.../Private/...cpp` | ready — looping beam cue; compiles + runs |
| `AFLVFXLibrary/AFLVFXLibrary.uplugin` | ready — only needed if you migrate the pack into a plugin mount |

## What the agent still authors (extend from these)

- ~~`AAFLCueNotify_LaserImpact` and `AAFLCueNotify_LaserBeamFlash` (burst cues)~~ —
  **DONE** (authored alongside the beam cue; same module, const-correct SourceObject).
- The charge cue + camera-shake notifies — `references/full-weapon-feature-map.md`.
- Each weapon's `IAFLLaserVisualProvider` implementation + `DA_AFL_LaserVisual_*` data asset.
- **The `GCN_AFL_Laser_*` notify ASSETS** (see correction below) — the C++ classes do
  not fire on their own.

## CRITICAL CORRECTION — a C++ cue class alone does NOT fire

The cue notify classes here (`AAFLCueNotify_LaserBeam` / `_LaserImpact` / `_LaserBeamFlash`)
are **parent classes only.** Earlier wording implied the looping beam cue "compiles + runs"
as if spawning the C++ class is enough — that is **wrong for this project.** Lyra's
`LyraGameplayCueManager` is **asset-scan + path-based, not C++-class-scanned**, and only
scans `/Game/GameplayCueNotifies` + `/Game/GameplayCues` (per `DefaultGame.ini`). A bare
C++ notify in this plugin's `/AFLVFX/` mount is **invisible** to it.

**To fire:** create a tagged **`GCN_AFL_Laser_*.uasset` parented to the C++ class**, named
so its tag derives to `GameplayCue.Weapon.Laser.*`, saved under **`/Game/GameplayCues/`**.
Fastest: duplicate a working `GCN_AFL_Pulse_*` and re-parent. Full detail +
why-not-a-config-edit in `references/integration-architecture.md` (§"how the cue notify is
DISCOVERED"). This is suspect #1 if a cue won't fire on a clean build.

## Note vs the docs

`AFLLaserVisualProvider.h` here is the **canonical** interface — BlueprintNativeEvent (so
Blueprint weapons can implement it) and it adds `GetMuzzleMeshComponent`. It supersedes the
pure-virtual sketch in `integration-architecture.md`. Call it via the generated thunks:
`IAFLLaserVisualProvider::Execute_GetBeamSystem(Provider)`.

Verify against UE 5.6 API on first build — if any Niagara setter name drifted in your engine
build (`SetVariableVec3` / `SetVariableLinearColor`), the agent fixes the one call site.
