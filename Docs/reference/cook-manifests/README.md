# Cook manifests

One CSV per full cook: `Path,Length` for every file in the tree, relative to the platform root
(`Saved/Cooked/<Platform>`). They exist so that **"what changed in the build?" is answerable without
keeping 9.5 GB of cooked output on disk**, and without re-cooking to find out.

⭐ **`cook_20260810g` is the current reference baseline** — the most recent clean cook from a healthy
preceding state. Diff against that one, never against `b` (incremental) or `c` (post-abort); the section
below is why that distinction decides whether a diff means anything.

| File | Tree | Files | Packages | Size |
|---|---|---|---|---|
| `cook_20260809_windows.csv` | Windows client, 2026-08-09 17:46 | 17,204 | 7,333 | 9,715 MB |
| `cook_20260810_windows.csv` | Windows client, 2026-08-10 12:37 | 17,195 | 7,329 | 9,713 MB |
| `cook_20260810b_windows.csv` | Windows client, 2026-08-10 late — **incremental** | 17,207 | 7,335 | 9,713 MB |
| `cook_20260810c_windows.csv` | Windows client, 2026-08-10 later — **clean, after an OOM abort** | 17,199 | 7,331 | 9,713 MB |
| `cook_20260810d_windows.csv` | Windows client, 2026-08-10 last — **clean, healthy state** | 17,203 | 7,333 | 9,713 MB |
| `cook_20260810e_windows.csv` | Windows client — **clean, after a DELIBERATE abort** | 17,207 | 7,335 | 9,713 MB |
| `cook_20260810f_windows.csv` | Windows client — **clean, project DDC cleared** | 17,199 | 7,331 | 9,713 MB |
| `cook_20260810g_windows.csv` | Windows client — **clean, DataLayers force-cooked** ⭐ | 17,207 | 7,335 | 9,713 MB |

The **b** cook confirmed a claim the previous one had only made by reasoning: the Career hub's soft class
pointer returns `W_ReplayBrowserScreen` and `W_ReplayListEntry` to the build after HOST's deprecation had
removed their last referencer. 0 removed, 6 added, 0 errors. `W_ExperienceSelectionScreen` is still absent,
so the deprecation held across a second cook rather than only the one that introduced it.

## ⚠ DATALAYER PACKAGES COOK UNSTABLY — DIFF EVERY COOK

**Six cooks. The `.umap` files never move; their WorldPartition DataLayer packages do**, in both
directions, across three different maps, always reporting `Success - 0 error(s)`.

| moving set | 08-10 | 08-10b *incr* | 08-10c *post-OOM* | 08-10d | 08-10e *post-abort* | 08-10f *no DDC* |
|---|:--:|:--:|:--:|:--:|:--:|:--:|
| `L_ShantyTown/{ExtraSpawn,Gameplay,Layout,Lighting}` | ✅ | ✅ | **✗** | ✅ | ✅ | ✅ |
| `L_Convolution_Blockout/{Gameplay,Layout}` | ✗ | ✅ | ✅ | ✗ | ✅ | ✅ |
| `L_Expanse_Blockout/{ExtraSpawn,Gameplay,LayoutModelA,Lighting}` | ✅ | ✅ | ✅ | ✅ | ✅ | **✗** |

**Two hypotheses were tested and both are answered NO:**

- **An aborted cook is NOT the trigger.** Reproduced deliberately — a cook killed at 879 packages (the
  OOM died at 831), partial tree deleted, DDC intact, plenty of memory. Nothing dropped.
- **The DDC is an INPUT, not the cause.** Clearing it changed the outcome — it broke a four-run
  monotonic climb in the discovered total (7653 → 7655 → 7657 → 7659 → **7655**) and dropped four
  `L_Expanse_Blockout` layers — but produced a different answer, not a stable or correct one.

**Severity:** in `08-10f`, the cleanest state available, **our shipping maps are intact** — `L_ShantyTown`
7/7 layers, `L_Expanse` 4/4. The layers that move most belong to the ShooterMaps `*_Blockout` samples.
The one time AFL content dropped (`08-10c`) has not recurred in four subsequent cooks. Treat this as a
build-integrity hazard, not a live defect.

**✅ MITIGATED 2026-08-10 — the variance can no longer reach a build.** `DefaultGame.ini` now force-cooks
both DataLayer roots:

```
+DirectoriesToAlwaysCook=(Path="/Game/Maps/DataLayers")
+DirectoriesToAlwaysCook=(Path="/ShooterMaps/Maps/DataLayers")
```

Measured, clean cook with a cleared DDC on both sides so the entries were the only variable
(`08-10f` → `08-10g`): **+4 packages, +8 files, +0.01 MB**, 0 removed, 0 errors — and **19 of 19**
DataLayers on disk now cook, up from 15. Ten kilobytes, the cheapest entry in that file by three orders
of magnitude; these are tiny descriptors whose *absence* is what costs.

That makes the packages **declared rather than discovered**, so the cooker cannot omit them whatever
varies — structural, not probabilistic. It does not explain the instability, which is still open; it
makes it stop mattering here.

**Still true regardless:**
1. **Diff every cook against a manifest.** None of this appeared in totals, warnings, or exit codes.
2. **Ship from clean cooks only** — an incremental tree emits packages a clean one does not.
3. **A new WorldPartition map needs its DataLayer folder covered**, or it re-enters the lottery —
   and `Tools/AFL_Lint/cook_datalayers.py --cooked-dir <tree>` is the check that says so out loud
   rather than relying on someone remembering. It compares every `DataLayers/<Map>/*.uasset` on
   disk to the cook and exits 1 on any gap.

*Three earlier readings of this are superseded by the table above: "nondeterminism", then "a
clean-vs-incremental confound", then "reproducible from a healthy state". Each was the most a smaller
sample could support, and each was written too confidently for the evidence under it.*

⚠ **THE 08-09 TREE NO LONGER EXISTS.** Its manifest is the only surviving record of it. That is the
point of this folder — the tree was 9.5 GB and was deleted after being measured, but the thing worth
keeping was never the bytes.

## Why a manifest and not just the numbers

The totals already live in `Config/DefaultGame.ini` beside the `DirectoriesToNeverCook` block. Totals
answer "did the build grow"; they do not answer "**which** packages left". On the 08-09 → 08-10 diff
those were opposite questions: the totals moved by −4 packages and −2 MB, which looks like nothing,
while underneath 26 packages left and 22 arrived. The whole reachable set behind the old front-end
root had been removed and the new front end had replaced it almost exactly.

That is also how the REPLAYS finding surfaced. `W_ReplayBrowserScreen` was expected to survive the
HOST deprecation and did not — its only referencers were the two dead roots that moved — and nothing
in the totals would ever have shown it.

## Snapshot a tree

```powershell
$w = "C:\Dev\Bag_Man\Saved\Cooked\Windows"
Get-ChildItem -Recurse -File $w | ForEach-Object {
  [PSCustomObject]@{ Path = $_.FullName.Substring($w.Length); Length = $_.Length }
} | Export-Csv "Docs\reference\cook-manifests\cook_<yyyyMMdd>_windows.csv" -NoTypeInformation
```

## Diff two manifests

```powershell
$a = Import-Csv Docs\reference\cook-manifests\cook_20260809_windows.csv
$b = Import-Csv Docs\reference\cook-manifests\cook_20260810_windows.csv
$pkg = { $_.Path -match '\.(uasset|umap)$' }
$ap = [System.Collections.Generic.HashSet[string]]::new(); $a | Where-Object $pkg | ForEach-Object { [void]$ap.Add($_.Path) }
$bp = [System.Collections.Generic.HashSet[string]]::new(); $b | Where-Object $pkg | ForEach-Object { [void]$bp.Add($_.Path) }
"removed:"; $ap | Where-Object { -not $bp.Contains($_) } | Sort-Object
"added:";   $bp | Where-Object { -not $ap.Contains($_) } | Sort-Object
```

Compare **packages** (`.uasset` / `.umap`), not raw file count — a package is usually two or three
files (`.uasset` + `.uexp`, sometimes `.ubulk`), so file deltas move in multiples and read as noise.

## Related

- `Config/DefaultGame.ini` — `DirectoriesToAlwaysCook` (assets a string path would otherwise drop) and
  `DirectoriesToNeverCook` (the HOST deprecation), each with its own measured before/after.
- `Tools/AFL_Lint/cook_soft_refs.py --cooked-dir <tree>` — checks a cook tree for the specific defect
  these manifests keep catching by hand: an asset named only by a C++ string literal, which the cooker
  cannot follow. A manifest tells you what moved; the lint tells you whether anything *needed* it.
  Run it against the tree, not the manifest — it globs real files.
