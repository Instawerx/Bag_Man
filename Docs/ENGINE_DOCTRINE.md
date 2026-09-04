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

## 3. Provenance verification on R2 uploads (MANDATORY gate)

Every artifact uploaded to R2 / distribution MUST pass `Tools/verify_ship_provenance.ps1`
first (authored 2026-09-03). It reads the monolithic game exe's PE version resource, which
`FEngineVersion` stamps as `<BranchName>-CL-<Changelist>`:

- **D: source build** → `UE5-CL-0` (BranchName `UE5`, `IsPromotedBuild=0`) → **PASS**, exit 0,
  prints `SHIP PROVENANCE VERIFIED (D:\UE5.6-source)`.
- **C: launcher build** → promoted `++UE5+Release-5.6-CL-44394996` (`IsPromotedBuild=1`) →
  **REFUSED**, exit 1.

Run it against any staged build root: `Tools\verify_ship_provenance.ps1 -Path <stagedRoot>`.
Proven 2026-09-03 (validator law — rejects known-bad before its pass counts):
- **Known-bad (REJECTS):** `D:\BagMan\StagedBuilds_W2A1_Shipping` — the launcher-built W2A1
  beta (`++UE5+Release-5.6-CL-44394996`), exit 1. **This artifact is provenance-contaminated;
  do NOT ship it — rebuild on D: first (operator ruling).**
- **Known-good (PASS):** `D:\BagMan\StagedBuilds\{Windows,WindowsClient,WindowsServer}`
  (`UE5-CL-0`), exit 0.

## 4. Engine version — UE 5.6.1 (fact; Build.version is the arbiter)

The project engine is **UE 5.6.1**. `D:\UE5.6-source\Engine\Build\Build.version`:
`MajorVersion 5`, `MinorVersion 6`, `PatchVersion 1`, `CompatibleChangelist 43139311`,
`IsPromotedBuild 0`, `BranchName "UE5"`, `Changelist 0`. (The C: launcher reported the same
5.6.1 compat CL but promoted — `IsPromotedBuild 1`, `BranchName "++UE5+Release-5.6"`,
`Changelist 44394996` — which is the provenance discriminator, §3.)

**Ruled 2026-09-03:** 5.6.1 is confirmed fact; the earlier "engine is 5.6.0 / sync to 5.6.1
is a future task" note was stale memory and is **void**. There is no pending 5.6.1 sync task.

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

**Why it was retired (durable reasons):** the partition created a whole bug class — two
engines at potentially different changelists producing client/server handshake mismatches —
plus recurring "which engine did this build come from" drift (a launcher-built beta actually
shipped, see §3) and the clobber-and-rebuild tax. Consolidating to the single source engine
makes any engine-plugin work build from one lane and makes the handshake-mismatch class
structurally extinct.

> **Correction (2026-09-03) — the voice-blocker premise was a misdiagnosis.** The
> consolidation was partly motivated by the belief that enabling EOS voice required the
> source engine because it failed UBT with `'VoiceChat' is not a C++ module`. That error was
> later root-caused NOT to any binary-vs-source engine limitation but to a **`.uproject`
> config mistake**: enabling the header-only `VoiceChat` **External** plugin *standalone*
> forces UBT to instantiate it as a target C++ module (`UEBuildTarget.FindOrCreateCppModuleByName`
> → External is not a `UEBuildModuleCPP` → throw). It reproduces **identically on both
> engines** (verified on the D: source engine 2026-09-03). The fix is to enable only
> `EOSVoiceChat` (which references `VoiceChat` as an *include-path* module, the way stock UE
> does). **Voice did not actually require the consolidation** — but the consolidation still
> stands on the durable reasons above.

---

*Doctrine SSOT. If a skill, memory, or `CLAUDE.md` line contradicts this file on the
engine, this file wins and the other should be corrected to match.*
