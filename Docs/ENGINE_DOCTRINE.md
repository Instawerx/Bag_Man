# ENGINE DOCTRINE — IRONICS (BAG MAN)

> IRONICS is a source-build game. D:\UE5.6-source builds everything: the editor,
> PIE, dedicated servers, shipping cooks, and every artifact that reaches players.
> The C: launcher engine is retired from project use.

**Ruled 2026-09-03.** This supersedes the two-engine partition (2026-08-27). One
engine, one lane, one writer of engine binaries.

---

## 1. The one engine — and how to invoke it

**`D:\UE5.6-source`** is THE engine for every target: `LyraEditor`, PIE, `LyraServer`
dedicated-server builds, and shipping cooks. There is no second engine in the project
loop.

**Invoke the engine by EXPLICIT PATH — always. This rule survives the consolidation.**
Never rely on `EngineAssociation` resolution as the *primary* selector: type the full
`D:\UE5.6-source\...\Build.bat` path in every build command, script, and CI step. The
explicit path is what guarantees the right compiler and headers; the association is only
a backstop for accidental double-clicks (see §2).

```powershell
:: EDITOR / PIE / content authoring  (editor closed; operator-owned build)
& "D:\UE5.6-source\Engine\Build\BatchFiles\Build.bat" LyraEditor Win64 Development `
    -Project="C:\Dev\Bag_Man\Bag_Man.uproject" -WaitMutex

:: DEDICATED SERVER / shipping
& "D:\UE5.6-source\Engine\Build\BatchFiles\Build.bat" LyraServer Win64 Development `
    -Project="C:\Dev\Bag_Man\Bag_Man.uproject" -WaitMutex
```

**Authoritative build verdict = the `Result:` line + fresh binary mtimes on disk.**
Never trust the process exit code (UBT has returned exit 0 on a `Result: Failed`).

## 2. EngineAssociation is now a safety, not a hazard

`Bag_Man.uproject` `EngineAssociation` = `{5066982E-439C-2993-A6CB-F48A14DE2492}`, which
the Windows registry (`HKCU\Software\Epic Games\Unreal Engine\Builds`) maps to
`D:/UE5.6-source`. Under the retired partition this GUID resolving to D: was a *trap*
(D: was the wrong, server-only engine). Now D: is the only engine, so the association
pointing at it is correct: a stray double-click or bare `Build.bat` resolves to the
source engine instead of a launcher. Keep the explicit-path habit anyway (§1) — this is
belt-and-suspenders, not a license to drop the path.

## 3. Provenance verification on R2 uploads (retained mandate)

Every artifact uploaded to R2 / distribution must be provenance-verified before it is
treated as shippable — the artifact must be proven to have come from the D: source engine,
not a stray launcher build. The launcher-reject proof from the ship-hardening block stands
as the known-bad fixture. (`verify_ship_provenance.ps1` is the intended gate; if it is not
yet on disk, authoring it is a named task and no R2 artifact is "verified" until it runs
green against a D:-built artifact.)

## 4. Engine version

The D: source tree is at **UE 5.6.0**. Syncing the source to the **5.6.1** tag is a
**named, separate future task** — NOT part of the consolidation. Until that task runs,
5.6.0 is the project's engine version; do not assume 5.6.1 behavior.

## 5. What is NO LONGER a rule (deleted — do not reintroduce)

The retired two-engine partition carried rules that are now dead. They are listed here so
a future session does not resurrect them from stale memory:

- ❌ **The D:→C: rebuild switch.** There is no "a D: LyraEditor build clobbered C:, rebuild
  C:" recovery. There is no C: engine in the loop to clobber or restore.
- ❌ **Ship-lane / dev-lane labeling.** There is one lane. `LyraEditor` and `LyraServer`
  are just targets of the same engine, built the same way, from the same changelist.
- ❌ **The clobber hazard.** With one engine there is one writer of engine binaries; the
  same-output-path overwrite trap between two engines cannot occur.
- ❌ **"Never launch the editor on D:."** The editor is *supposed* to run from D: now.

## 6. Retirement record — the two-engine partition (history, superseded in place)

*Kept as history, not as instruction.* From 2026-08-27 to 2026-09-03 the project ran a
two-engine partition:

- **C: `C:\Program Files\Epic Games\UE_5.6`** (launcher / Installed / **binary**) was the
  editor engine: editor, PIE, AIK, Fab, content authoring/commits, `LyraEditor` builds.
- **D: `D:\UE5.6-source`** (source) was the GameLift engine: `LyraServer` + cooks ONLY;
  "never launch the editor on D:"; a D: `LyraEditor` build overwrote the C: editor binaries
  (same per-project output path), recovered by a C: launcher rebuild.

**Why it was retired:** the binary/Installed C: engine cannot compile engine plugins that
ship without a precompiled binary — the `VoiceChat` interface module (a header-only
`ModuleType.External`, disabled by default) is absent from the Installed engine's module
manifest, so enabling EOS voice failed UBT with `'VoiceChat' is not a C++ module`. The
partition also created a whole bug class: two engines at potentially different changelists
producing client/server handshake mismatches, plus recurring "which engine did this build
come from" drift and the clobber-and-rebuild tax. Consolidating to the single source engine
makes voice (and any future engine-plugin work) compile natively and makes the
handshake-mismatch class structurally extinct.

---

*Doctrine SSOT. If a skill, memory, or `CLAUDE.md` line contradicts this file on the
engine, this file wins and the other should be corrected to match.*
