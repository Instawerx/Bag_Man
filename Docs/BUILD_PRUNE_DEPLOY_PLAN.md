# IRONICS — Prune, Package & Deploy Plan (v1, 2026-09-01)

Operator directive: read, scope, and plan the pruning and cuts for Build and Deploy. Deliver the
leanest possible packaged game + launcher while keeping the complete live flow working
(Epic EOS sign-in, PlayFab, GameLift). Preserve everything on Git AAA-style before cutting.

**STATUS: PLAN — awaiting operator approval. Nothing has been cut.**

Grounding: 5-agent evidence sweep (disk, cook config, usage closure, build lanes, git) +
cook-manifest aggregation of `cook_20260810g_windows.csv` (9,713 MB / 17,207 files) + staged-build
manifest probes on `D:\BagMan\StagedBuilds_W2A1_Shipping` (the 2026-08-15 Shipping client, 3.4 GB).

---

## 1 · The one fact that shapes everything

**Package weight and repo weight are two different axes.** `bCookAll=False` means the cook takes
only: MapsToCook + AlwaysCook primary-asset scans + DirectoriesToAlwaysCook + their reference
closure. The cook manifest proves it:

- The 19 marketplace/anim packs everyone assumed were bloating the build (**~33.4 GB on disk** —
  Downtown_West 12 G, ModularOldTown 4.4 G, LensFlareVFX 2.8 G, StoreProps 2.0 G, Watermills 1.8 G,
  HighTechPack1, Fantastic_Village, ModularSciFi_H, Polar, BigNiagaraBundle, Kobo_Dungeon,
  GothicFantasy, Realistic_Starter_VFX, Wild_West, DynamicFalling, ParkourAnimations,
  FreeSample/FreeLadder anim sets, Fab/Motifect) contribute **0 bytes to the package**. They are
  repo/LFS/clone weight only.
- Docs/ (53 MB), Tools/ (2.7 GB), SSOTs, ref images, `Content/AFL/_Bridge` (8.1 GB DCC scratch):
  **confirmed absent from the staged shipping manifests**. The "Docs must never ship" guarantee
  already holds. (One stray: `Content/Tools/MT_DebugTool` stages — 6 MB dir, cut below.)

So the plan has two independent tracks: **A. Package prune** (what players download) and
**B. Repo prune** (what we store/clone), plus **C. Ship & verify** and **D. Launcher/distribution**.

## 2 · Current sizes (measured)

| Thing | Size | Evidence |
|---|---|---|
| Working tree total | 239 GB | du (C:\Dev\Bag_Man) |
| — Content | 64.1 GB | du |
| — Plugins | 13.7 GB (AFLHub 7.4 = MegaBase import) | du |
| — .git (≈all LFS) | 80 GB local store; 66.3 GB referenced at HEAD | lfs inventory |
| — dev Binaries + Saved + Tools + DDC | ~22 GB (never ship, mostly gitignored) | du |
| Last full client cook (2026-08-10) | **9,713 MB loose / 7,335 packages** | cook manifest |
| Last SHIPPING client (2026-08-15, LyraGameEOS) | **3.4 GB staged** | D:\BagMan\StagedBuilds_W2A1_Shipping |
| Dev client archive / LyraServer.exe | 6.6 GB / 356 MB | D:\BagMan\Archive |
| Fresh-clone checkout cost today | ~66 GB (LFS at HEAD) | lfs pointers |

**Cooked-package attribution (top of the 9,713 MB):** DeepWaterStation **1,958 MB (20 %)**,
BagMan 1,758, Sci_FI_Valley_Village **1,305 (13 %)**, ShantyTown 831, Maps 761, Engine 550,
Characters 364, SpaceshipInterior 358, ShooterMaps 238, CyberPunkAssets 191, Weapons 166,
shaders ~400, LyraExampleContent 101.

**Two package parasites, cook-proven:**
1. **Sci_FI_Valley_Village 1.3 GB** ships *only* because parked `L_ValleyVillage.umap` sits in
   `/Game/Maps`, which the AssetManager Map rule force-cooks recursively. Zero live-flow value.
2. **DeepWaterStation 1.96 GB + CyberPunkAssets 191 MB** are dragged by a handful of ARCANEON
   assets (L_Arena_04 + `BagMan/ArtPass/B_ARCANEON_*` + `BagMan/Barriers/B_BagMan_Cyber*`) whose
   reference closure pulls enormous texture sets.

**Staleness note:** the 08-10 baseline predates the hub map scan, SkyClub/GNC venue scan entries,
recent AFLCombat content, and the 112 MB start-loop movie. A cook under TODAY'S config is
estimated **12–15 GB loose → ~4.5–5.5 GB shipping pak** before pruning.

## 3 · Track A — Package prune (the player download)

All config edits in `Config/DefaultGame.ini` unless noted. Each step ends with an incremental
cook + manifest diff vs `cook_20260810g` + `cook_soft_refs.py --strict-never-cook` +
`cook_datalayers.py` (the standing lint gates).

| # | Cut | Est. package Δ (loose) |
|---|---|---|
| A1 | Map scan rule: replace the recursive `/Game/Maps` directory entry with explicit roster SpecificAssets (L_ShantyTown, L_Arena_01, L_Arena_04, L_Duel_01) and MOVE `L_ValleyVillage` + `L_Arena_05` out of `/Game/Maps` to `/Game/DeveloperUtils/ParkedMaps` (the HOST lesson: the move is the stronger half). Keep `/AFLHub/Maps`, venue dirs, DataLayer entries untouched. | **−1.4 GB** |
| A2 | Drop stock `L_LyraFrontEnd` from MapsToCook and `L_DefaultEditorOverview` (an editor map!) from SpecificAssets — boot map is L_IRONICS_Armory. | −30–60 MB |
| A3 | ARCANEON art extraction: SizeMap the 6 referencing assets, migrate them onto a trimmed `Content/BagMan/ArcaneonArt/` subset (or duplicated-asset slice), then DeepWaterStation/CyberPunkAssets fall out of the closure. Riskiest item — own verify gate (ARCANEON + BR barriers visually intact in cooked build). | **−1.2–1.8 GB** |
| A4 | Break the LyraExampleContent chain (ShooterCore `MI_PhysicsTest` → T_Paint textures): repoint or delete the PhysicsCube item. | −101 MB |
| A5 | ShooterMaps non-roster maps (L_Convolution_Blockout, L_Expanse_Blockout, L_FiringRange_WP, L_ShooterFrontendBackground) → move out of scanned root inside the plugin. | −80–120 MB |
| A6 | `bSkipEditorContent=True`, `ForDistribution=True` (Oodle level 7), add `Content/Tools` + `Content/AFL/_Bridge` + `Content/_BridgeTest` to DirectoriesToNeverCook (belt-and-braces). | −3–8 % pak |
| A7 | Cultures: 14 → operator ruling (recommend en + es-419 + pt-BR + fr + de or just en for now). | small |
| A8 | Disable never-shipped stock GF plugins in uproject: TopDownArena, ShooterExplorer, ShooterTests (ShooterCore + ShooterMaps stay — live deps). Effective on the D: source-engine lane, which is the shipping lane. | −5 MB pak, repo hygiene |
| A9 | Config smells: delete vestigial `/Game/Unused` GameFeatureData scan + `/Game/UI/Temp` playlist dir after confirming empty/unused. | 0 |

KEEP (explicitly): SkyClub (314 MB) + GentlemensNightClub (1.36 GB) — wired hub-door venues via
`DA_AFL_HubDestinations`; Duel01 chain — playlist + experiences wired; CloudsLighting + UDS —
operator-ruled intended; Movies/MV_AFL_StartLoop.mp4 — the live start screen; the EOS ClientSecret
in cooked config is Epic's standard embedded-client model (optional hardening: `bEncryptPakIniFiles=True`).

**Projected shipping client after Track A: ≈ 3.5–4.5 GB** (vs ~4.5–5.5 GB unpruned current-config,
and it now INCLUDES hub + venues + all Sep content the 3.4 GB Aug-15 build predates).
Projected server: first-ever Shipping server build, est. **2.5–3.5 GB staged**.

## 4 · Track B — Repo prune (storage/clone weight)

**Preservation first — this is the AAA fork-and-cut step (P0, blocking):**
1. Land the 7 pending working-tree files (operator's armory placement + the bot-persistence
   session's C++ — not mine to commit unilaterally).
2. `git push personal --tags` — **all 64 milestone tags are currently local-only (verified: zero
   on the remote)**. This is the single most urgent preservation gap.
3. Tag `pre-prune-2026-09` + branch `archive/pre-prune-full`; push both.
4. Optional but recommended: GitHub fork `Instawerx/Bag_Man-Archive` (same account = shared LFS
   storage, zero re-upload) as the frozen everything-repo. Note: CLAUDE.md names C12-Ai-Gaming as
   the org repo but no such remote exists — operator to reconcile.
5. **NO history rewrite.** Deleting via `git rm` keeps history + tags valid; old LFS objects stay
   on the server (they cost GitHub storage, not clone time — fresh clones only pull HEAD's LFS).
   A filter-repo rewrite of a 250 GB LFS monorepo risks every tag/branch for a benefit
   (server-side storage) that isn't the pain point. Explicitly rejected.

**Then the cut list (git rm + disk delete on main):**

| Cut | Disk | Notes |
|---|---|---|
| 19 cook-proven-unreachable packs | **~33.4 GB** | Zero package impact (proven); pure repo/clone win |
| Content/AFL/_Bridge → archive branch/zip only | 8.1 GB | DCC scratch; keep on archive fork |
| Sci_FI_Valley_Village + parked ValleyVillage/Arena_05 maps | 2.6 GB | After A1 lands |
| DeepWaterStation + CyberPunkAssets minus extracted ARCANEON slice | ~10 GB | After A3 lands |
| AFLHub MegaBase demo Levels (1.5 G) + Showreel_BuiltData (1.5 G) + HDRI (260 M) | ~3.3 GB | Inside a USED plugin — reference-check the hub map first (only 312 MegaBase refs) |
| Anim packs: relocate `FreeAnimationLibrary/AFL` (climb montage — LOAD-BEARING) + confirm `AM_AFL_Grab_RT` orphaned, then delete Parkour/FreeSample/FreeLadder/DynamicFalling/Fab packs | ~1.25 GB | Most already untracked/gitignored — **unrecoverable once disk-deleted**; confirm Fab re-download entitlement first |
| Lyra stock: LyraExampleContent (after A4), Mannequin_UE4 legacy | ~0.4 GB | |
| Local-only hygiene (not git): dev Binaries 13.6 G, Saved 4.5 G, Tools/Generated 2.6 G, DDC 1.1 G, `git lfs prune` −14 G, D:\BagMan Intermediate ~54–97 G + stale Cooked/StagedBuilds ~30 G, Military Megapack zip 8.3 G (after import verify) | **~75–130 GB** | Machine cleanup |

**Projected repo after Track B:** working tree 239 GB → **~145–155 GB** (mostly .git/lfs 66 GB +
kept content); **fresh-clone checkout 66 GB → ~21–25 GB**. GitHub-side storage unchanged (by design
— history preserved).

## 5 · Track C — Ship builds & the keep-the-live-game gate

Shipping lane (unchanged doctrine): D:\UE5.6-source RunUAT BuildCookRun.
- Client: `-client -clientconfig=Shipping` LyraGameEOS/CustomConfig=EOS (needs the gitignored
  `Config/Custom/EOS/DefaultEngine.ini` present).
- Server: **first Shipping server** `-server -serverconfig=Shipping -noclient` (all prior server
  cooks were Development).
- Junctions Saved/Cooked+StagedBuilds+Intermediate → D:\BagMan already in place; D: has 501 GB free.

**Acceptance gate (the guarantee — cooked builds only, PIE proves nothing here):**
1. Cooked client boots → start screen (4K loop plays) → Epic EOS sign-in → Route Choice.
2. Hub lap: store lap (card/till/credit), both venue doors, STORE travel.
3. Each roster map launches via its playlist: ShantyTown (BR + district streaming), NANOWATT,
   ARCANEON, INFINEON, Duel01.
4. GameLift Anywhere match: cooked Shipping server up, client PLAY → allocator → join with
   PlayerSessionId; PlayFab wallet/persistence round-trip in match.
5. Manifest diff vs baseline + both lint gates green; a fresh manifest CSV becomes the new baseline.
Every Track A/B step re-runs the cheap half of this (boot + one map); the full gate runs at the end
and before any distribution.

## 6 · Track D — Launcher / distribution (needs a ruling)

Verified: **no player-distribution launcher has ever existed** — "launcher" in this repo means the
C: engine install and `Tools/Launch-Editor-Economy.ps1` (dev/server bootstrap). Clients were
hand-copied. So the launcher is green-field. Options, cheapest first:
- **D1 — Signed archive + website download** (days): zip the staged Shipping client, host behind the
  existing website; simplest live path. No patching.
- **D2 — Itch.io/butler channel** (days): free CDN + delta patching + a real installer feel.
- **D3 — Epic Games Store** (weeks-months): fits the EOS identity spine perfectly; store review + fees.
- **D4 — Custom patcher/chunked HTTP** (weeks): real ChunkId assignment work (today all chunks = -1).
Recommendation: D1 or D2 now, D3 as the destination. GameLift server side is a separate work item —
the S12 plan's managed-EC2 fleet + first `upload-build` (today's fleet is Anywhere on the dev LAN).

## 7 · Open operator rulings

1. Approve the plan + phase order (P0 preserve → A-track → B-track → C-gate → D-launch).
2. Platform scope: Win64-only for this launcher? (All evidence is Win64; consoles/mobile have no SDK/cook trail.)
3. Cultures to stage (A7).
4. Venues + Duel01 confirmed KEEP? (Plan assumes keep.)
5. MegaBase demo Levels/Showreel/HDRI deletion inside AFLHub (needs hub reference check first).
6. Distribution channel (D1–D4) + archive fork name.
7. Fab re-download entitlements confirmed for the untracked anim packs before their unrecoverable disk delete.

## 8 · Verified guarantees & corrections

- Docs/SSOTs/ref images/scratch: already excluded from staging (manifest-proven); NeverCook additions make it structural.
- No ClientSecret in staged loose inis (08-15 build); embedded-client secret in pak config is Epic-standard.
- `Tools/Launch-Editor-Economy.ps1` is stale vs the two-engine ruling (points editor at D:) — fix in P0.
- CLAUDE.md's `/AFLVFXLibrary/Laser/` doctrine path is dead — the live laser library is `Content/LaserFX_BP` (43 MB, 158 refs); AFLVFXLibrary plugin is an empty shell. Doc fix + shell removal in B-track.

---

# v2 ADDENDUM (2026-09-01) — approved, with two operator additions

**Operator approvals recorded:** full sequence approved; aggressive-cut mandate ("if we can remove
it without it affecting the game, we do it — everything is source-recoverable"); Matchmaking and
Staked features are the MOST protected surfaces — no cut may touch playlists, experiences,
AFLOnline, EOS/PlayFab/GameLift config, or the allocator/join flow. Distribution: website-zip only
for now.

## 9 · Execution log (what has been done)

- **P0 COMPLETE**: 7 pending files landed (`3d7e05e3` bot-persistence gate, `fc0ab373` operator
  content); all 64+1 tags pushed (118 tag refs verified on remote); tag `pre-prune-2026-09` +
  branch `archive/pre-prune-full` pushed; `Launch-Editor-Economy.ps1` editor lane re-pointed to C:.
  History-rewrite rejected stands. Archive FORK remains optional (branch+tags already pin
  everything server-side).
- **Track A COMPLETE (config)**: stock `L_LyraFrontEnd` + `L_DefaultEditorOverview` out of the
  cook; NeverCook guards added for `_Bridge`, `AFL/References`, `Tools`, `_BridgeTest`;
  `bSkipEditorContent=True`; `ForDistribution=True` (Oodle 7); cultures 14 → **en**;
  TopDownArena/ShooterExplorer/ShooterTests disabled in uproject.
- **Track B COMPLETE (repo)**: 15 tracked packs + Sci_FI_Valley_Village + parked
  L_ValleyVillage/L_Arena_05 + AFL/_Bridge + MegaBase Showreel/HDRI git-rm'd; untracked anim packs
  disk-deleted. Hub's MegaBase/Levels refs verified and KEPT. FreeAnimationLibrary tracked climb
  files KEPT. **Content 64.1 GB → 22.9 GB.** Commit `bf16e71e`.
- **Track C COMPLETE (2026-09-04)** — cooked, provenance-clean, packaged:
  - **Engine consolidated to ONE source engine** (`Docs/ENGINE_DOCTRINE.md`): `D:/UE5.6-source` builds
    editor, PIE, server, cooks, all artifacts; the C: launcher is retired. Both consolidation builds
    green (LyraEditor + LyraServer, same CL — handshake-mismatch class extinct). Engine ruled UE 5.6.1.
  - **Shipping CLIENT cook** (`LyraGameEOS Win64 Shipping`): BUILD SUCCESSFUL, 0 cook errors / 99 warnings,
    staged **6.15 GB** → `D:\BagMan\StagedBuilds\Windows`.
  - **First Shipping SERVER cook** (`LyraServer Win64 Shipping`): BUILD SUCCESSFUL, staged **3.13 GB**
    → `D:\BagMan\StagedBuilds\WindowsServer`.
  - **Both provenance-clean**: `Tools/verify_ship_provenance.ps1` → `UE5-CL-0` PASS (client + server).
    Gate authored + validator-law proven (REJECTS the launcher-built `StagedBuilds_W2A1_Shipping` beta).
  - **COMMS-3 voice ON** (`AFLONLINE_WITH_VOICE=1`, EOSVoiceChat provider) cooked into the client; the
    `afl.Voice.*` console harness is the pre-armed 2-client audio-proof mechanism.
  - **§11 release artifact packaged** (steps 0–1): `releases/win64/0.1.0-beta/IRONICS-0.1.0-beta-Win64.zip`
    (4.05 GB, pdb excluded, store-mode) + `.sha256` + `manifest.json`. Commits `f0edc55c..70970633` on `personal`.
  - **Remaining for Track C sign-off**: the cooked live-flow **watched** lap (§5 acceptance) + the
    manifest-diff lint vs `cook_20260810g`.

## 10 · Encryption (operator addition 1) — RULED + IMPLEMENTED

UE-native crypto, the industry-standard cheap set (Config/DefaultCrypto.ini, key already present):

| Flag | Now | Why |
|---|---|---|
| `bEncryptPakIniFiles` | **True** | Config (incl. the embedded EOS client credentials) no longer greppable from the pak |
| `bEncryptPakIndex` | **True** | Blocks casual pak browsing/unpacking tools |
| `bEnablePakSigning` | **True** | RSA tamper detection on every pak read |
| `bEncryptUAssetFiles` / `bEncryptAllAssetFiles` | False | Full-asset AES costs runtime IO/decompression for marginal protection; not the AAA default. Revisit only if asset-ripping becomes a real problem |

Note: pak encryption keys ship inside the executable by necessity (standard for every UE title) —
this raises the bar from "zip open" to "dedicated tooling", which is exactly what it's for.

## 11 · Download manager, website-zip channel (operator addition 2) — DESIGN

Institutional-grade delivery on the existing AWS estate (same account as the API Gateway/Lambda
stack), minimal moving parts:

0. **Ship-provenance gate (MANDATORY — blocks upload).** Before any release is packaged or uploaded,
   the staged Shipping dir MUST pass `Tools\verify_ship_provenance.ps1 -Path <stagedRoot>` (exit 0 =
   D:-source build; a promoted `++UE5+Release-*` launcher stamp is refused, exit 1). See
   `Docs/ENGINE_DOCTRINE.md` §3. Proven known-bad: the launcher-built `StagedBuilds_W2A1_Shipping`
   beta REJECTS — **do not ship it; rebuild on D: first.**
1. **Artifact discipline**: every release is immutable —
   `releases/win64/<semver>/IRONICS-<semver>-Win64.zip` + `IRONICS-<semver>-Win64.sha256` +
   `manifest.json` (version, size, hash, minimum-launcher fields for a future patcher). Built from
   the staged Shipping dir, zipped deterministically, hashed at build time. Never overwrite a
   version; `latest.json` is the only mutable pointer.
2. **Storage/CDN**: private S3 releases bucket (versioning + object-lock optional), CloudFront in
   front with **Origin Access Control** — the bucket is never public. Signed URLs (CloudFront
   key-pair, short TTL ~15 min) minted by a `GET /download/latest` Lambda on the existing API
   Gateway. Gate the mint behind the website's Epic sign-in session if desired (ties downloads to
   accounts; optional for open beta).
3. **Integrity UX**: the website download page shows version, size, SHA-256, and release notes;
   the manifest hash is the support answer to "my download is corrupt". CloudFront handles ranged
   requests natively → browser-resumable downloads for free.
4. **Observability/abuse**: CloudFront standard logs → the existing metrics path; WAF rate limit
   on the mint endpoint; per-release download counters (Athena over logs — no new infra).
5. **Upgrade path**: this layout is exactly what a future delta patcher (Track D4) consumes —
   nothing is throwaway.

## 12 · Admin systems + website final pass (operator addition 3) — SCOPE (website repo)

To be executed in the website/backend repo, planned here for sequencing after Track C proves:
- **Release admin**: publish/promote/rollback a release (writes `latest.json`), release-notes
  editor, download metrics panel.
- **Player/account admin**: the existing Volts purchase admin scope hardened to institutional
  grade — audit log on every admin mutation, role separation (viewer/operator/owner), session
  timeout, IP allowlist option.
- **Live-ops surfaces**: entitlement lookup/grant/revoke (PlayFab), match/session inspection
  (GameLift), wallet adjustments with dual-entry audit.
- **Quality gate**: Lighthouse/a11y pass on the public site, Epic sign-in flow re-test, legal
  pages current, download page copy + art (real IRONICS assets only, per the naming ruling).

## 13 · Amended remaining sequence

1. **✅ Track C cook DONE** (client + server, provenance-clean, 2026-09-04). Remaining: manifest-diff lint
   vs `cook_20260810g` + aggregate new-package attribution.
2. **OBSOLETE** — "rebuild editor binaries on C:" is superseded by the source-engine consolidation
   (`Docs/ENGINE_DOCTRINE.md`); the editor builds on `D:/UE5.6-source`. Remaining: operator one-engine PIE lap.
3. ARCANEON texture-budget pass (A3 revised: cap oversized DeepWaterStation/CyberPunk textures
   referenced by the closure — safer than asset moves; measured against the new manifest).
4. **✅ First Shipping server build DONE** (`LyraServer` Shipping, provenance-clean). Remaining: the full
   cooked live-flow **watched** gate (§5) — EOS sign-in → hub/store lap → every roster map → GameLift match.
5. **◐ Release §11 steps 0–1 DONE** (provenance gate + zip/sha256/manifest at `releases/win64/0.1.0-beta/`).
   Remaining: stand up the S3/CloudFront/Lambda mint + website download page (AWS/website — needs a real
   release semver; `0.1.0-beta` is a placeholder).
6. Admin/website final pass (§12).
