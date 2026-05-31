# Build Runbook — drive it all by prompt (AIK / Claude Code in the Unreal terminal)

The skill is assembled; nothing is imported or built yet. This is the ordered, paste-ready
sequence to go from "skill installed" to "second beam re-skinned to AAA," maximizing what the
terminal agent does and flagging the few steps that need a human in the editor / watching PIE.

Legend: **[TERMINAL]** agent runs it · **[EDITOR]** human opens UE / GUI step · **[PIE]** human
watches it work (the ✅ gate). The agent never marks ✅ on an [EDITOR]/[PIE] step — you do.

Before anything: paste **§0** then **§0b** from `aik-briefing.md` (awareness + the file
reorg). The agent's last report shows that's already done — the skill is assembled. Good.

---

## Phase 1 — Place the files you downloaded  **[you, then TERMINAL verifies]**

Drop into the repo:
- `C:\Dev\Bag_Man\Tools\LaserVFX5_2v_STV.zip`  (the marketplace pack)
- `C:\Dev\Bag_Man\Plugins\AFLVFX\…`            (the code-module stubs)
- `C:\Dev\Bag_Man\Plugins\AFLVFXLibrary\…`     (optional content plugin)

### Prompt P1 — verify placement
```
Verify these exist and report sizes, do not change anything:
  C:\Dev\Bag_Man\Tools\LaserVFX5_2v_STV.zip
  C:\Dev\Bag_Man\Plugins\AFLVFX\AFLVFX.uplugin
  C:\Dev\Bag_Man\Plugins\AFLVFX\Source\AFLVFX\Private\AFLCueNotify_LaserBeam.cpp
  C:\Dev\Bag_Man\Plugins\AFLVFXLibrary\AFLVFXLibrary.uplugin
Then read Tools\skills\afl-laser-beam-system\references\import-and-rebrand.md and
references\build-runbook.md and confirm you'll follow this phase order.
```

## Phase 2 — Import the pack  **[TERMINAL] + one [EDITOR] pass**

The reference-preserving path: bring the pack in under `/Game` first (its internal paths are
`/Game/LaserFX_BP/…`, so a straight copy keeps every reference valid), upgrade 5.2→5.6 by
resaving, then do the AFL renames in-editor (which creates redirectors safely). Migrating into
the `AFLVFXLibrary` mount is an optional later editor "Migrate" — not required for v1.

### Prompt P2 — unzip, audit, stage  **[TERMINAL]**
```
1. Unzip C:\Dev\Bag_Man\Tools\LaserVFX5_2v_STV.zip to C:\Dev\Bag_Man\Tools\_laser_src\.
2. Run: python Tools\skills\afl-laser-beam-system\scripts\audit_laser_pack.py
   Tools\_laser_src --out Tools\_laser_audit
   (writes inventory.json + rename_manifest.csv — the import plan).
3. Copy ONLY the importable tree into the project, preserving internal paths so references
   resolve: copy _laser_src\Content\LaserFX_BP\{Niagara,Material,Resource} ->
   C:\Dev\Bag_Man\Content\LaserFX_BP\. Do NOT copy Demo\, Map\, Config\, or the BP_Laser /
   BP_Laser_Orb blueprints (reference-only tick-trace demos).
4. Confirm *.uasset landed as Git LFS pointers (git lfs status).
Report the audit counts and the copied tree. No engine launch yet.
```

### Prompt P3 — upgrade 5.2→5.6 by resave  **[TERMINAL] (commandlet)**
```
Run the resave commandlet to upgrade + resave the imported packages to 5.6:
  "<UE>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Dev\Bag_Man\Bag_Man.uproject"
  -run=ResavePackages -ProjectOnly -AutoCheckOutPackages=false -Unattended -NoP4
Report any package that fails to load/resave. Do not delete anything.
```

### CHECKPOINT — open the editor once  **[EDITOR]**
- Open the project in UE 5.6. Confirm `/Game/LaserFX_BP/Niagara/…` shows up.
- Open 4 systems (`NS_Laser_Basic`, `NS_Laser_Twist`, an `NS_OrbLaser_*`, an
  `NS_OrbLaser_Center_*`) — confirm each **compiles** (no broken modules from the 5.2→5.6 jump).
- Delete `Demo/`, `Map/`, and the two `BP_Laser*` blueprints here in-editor; **Fix Up
  Redirectors** on `/Game/LaserFX_BP/`.
- Apply the AFL renames from `Tools\_laser_audit\rename_manifest.csv` (right-click → Rename
  creates redirectors), then **Fix Up Redirectors** again.
Tell the agent when this is done; it does not proceed past P3 until you confirm.

## Phase 3 — Stand up the AFLVFX module  **[TERMINAL]**

### Prompt P4 — build the module
```
Enable the AFLVFX plugin in Bag_Man.uproject (and AFLVFXLibrary if present). Regenerate
project files, then build:
  Build.bat LyraEditor Win64 Development -Project="C:\Dev\Bag_Man\Bag_Man.uproject"
The stubs are AFLVFX.uplugin + Build.cs + AFLVFX module + IAFLLaserVisualProvider +
AAFLCueNotify_LaserBeam. Fix any compile error in place (most likely a Niagara setter name
that drifted in this engine build — adjust the single call site, don't restructure). Report
a clean build.
```

### Prompt P5 — author the remaining cues (extend the stubs)
```
In AFLVFX, following references\integration-architecture.md and aik-briefing.md prompt 4,
add the burst cues AAFLCueNotify_LaserImpact (sparks at Params.Location/Normal) and
AAFLCueNotify_LaserBeamFlash (one-shot muzzle->Location flash, auto-destroy ~0.08s, reads the
weapon via IAFLLaserVisualProvider). Build clean. Add the GameplayCue tags from aik-briefing
prompt 1 if not present.
```

## Phase 4 — Prove the cosmetic layer once  **[TERMINAL] setup, [PIE] gate**

### Prompt P6 — make ONE weapon drive the cue (smallest possible proof)
```
Pick the existing second/channeled beam weapon. Author DA_AFL_LaserVisual_Test pointing
BeamSystem at NS_AFL_Laser_Twist (post-rename), BeamColor cyan, MuzzleSocketName "Muzzle",
CosmeticRange = its max range, MuzzleMeshComponent = its weapon mesh. Make that weapon
implement IAFLLaserVisualProvider backed by it. In its ability, on activate
AddGameplayCue(GameplayCue.Weapon.Laser.Beam, SourceObject=weapon); on end RemoveGameplayCue.
Do not change its trace or damage. Build clean and report the diff.
```

### CHECKPOINT — watch it  **[PIE]**  ← the real ✅
Listen server + 2 clients. Fire the beam:
- endpoint terminates on the surface the crosshair is over and tracks as you sweep,
- start stays glued to the muzzle, color correct, remote clients see it,
- target still takes damage (proves layers decoupled), pawn still walks/looks/jumps after.
Only when watched green does the cosmetic standard count as proven.

## Phase 5 — Audit + replace, per `beam-replacement-plan.md`

### Prompt P7 — RP-1 audit  **[TERMINAL]**
Paste **RP-1** from `beam-replacement-plan.md` (scan for retired assets + in-ability spawns +
tick-trace beams; output the per-weapon table). This tells you if Pulse touches a retired
asset (decides keep-as-is vs cosmetic re-route).

### Prompt P8 — RP-2 fix the second beam  **[TERMINAL] + [PIE]**
Paste **RP-2** (re-skin the channeled beam via the library through the cue, gameplay
unchanged), then **RP-3** (retire the old `NS_AFL_PrismBeam` once unreferenced). [PIE]-verify
it now reads AAA *and* still passes what it passed before.

### Prompt P9 — Pulse, only if needed; then standardize + lint  **[TERMINAL] + [PIE]**
Paste **RP-4** (Pulse cosmetic re-route ONLY if it uses a retired asset — else report and
leave it), **RP-5** (standardize remaining lasers), **RP-6** (CI lint + cook audit).

---

## Where the human is unavoidable

The agent does all file/codegen/build/commandlet/audit work. You are needed for: the **one
editor pass** in Phase 2 (Niagara compile-check + rename + Fix Up Redirectors — GUI-reliable),
and every **[PIE]** gate (watching it work is the definition of ✅). Everything else is prompts.

## If the agent can't see a path
Add `C:\Dev\Bag_Man\Tools` / `Plugins` to its allowed dirs, or keep the working dir at the
repo root so the root `CLAUDE.md` auto-loads and its `@Tools/skills/...` import resolves.
