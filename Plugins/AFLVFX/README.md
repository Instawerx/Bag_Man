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

- `AAFLCueNotify_LaserImpact` and `AAFLCueNotify_LaserBeamFlash` (burst cues) — small,
  same module, pattern in `references/integration-architecture.md` / `aik-briefing.md` prompt 4.
- The charge cue + camera-shake notifies — `references/full-weapon-feature-map.md`.
- Each weapon's `IAFLLaserVisualProvider` implementation + `DA_AFL_LaserVisual_*` data asset.

## Note vs the docs

`AFLLaserVisualProvider.h` here is the **canonical** interface — BlueprintNativeEvent (so
Blueprint weapons can implement it) and it adds `GetMuzzleMeshComponent`. It supersedes the
pure-virtual sketch in `integration-architecture.md`. Call it via the generated thunks:
`IAFLLaserVisualProvider::Execute_GetBeamSystem(Provider)`.

Verify against UE 5.6 API on first build — if any Niagara setter name drifted in your engine
build (`SetVariableVec3` / `SetVariableLinearColor`), the agent fixes the one call site.
