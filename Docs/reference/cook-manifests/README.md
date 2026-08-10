# Cook manifests

One CSV per full cook: `Path,Length` for every file in the tree, relative to the platform root
(`Saved/Cooked/<Platform>`). They exist so that **"what changed in the build?" is answerable without
keeping 9.5 GB of cooked output on disk**, and without re-cooking to find out.

| File | Tree | Files | Packages | Size |
|---|---|---|---|---|
| `cook_20260809_windows.csv` | Windows client, 2026-08-09 17:46 | 17,204 | 7,333 | 9,715 MB |
| `cook_20260810_windows.csv` | Windows client, 2026-08-10 12:37 | 17,195 | 7,329 | 9,713 MB |
| `cook_20260810b_windows.csv` | Windows client, 2026-08-10 late — **incremental** | 17,207 | 7,335 | 9,713 MB |
| `cook_20260810c_windows.csv` | Windows client, 2026-08-10 later — **clean** | 17,199 | 7,331 | 9,713 MB |

The **b** cook confirmed a claim the previous one had only made by reasoning: the Career hub's soft class
pointer returns `W_ReplayBrowserScreen` and `W_ReplayListEntry` to the build after HOST's deprecation had
removed their last referencer. 0 removed, 6 added, 0 errors. `W_ExperienceSelectionScreen` is still absent,
so the deprecation held across a second cook rather than only the one that introduced it.

## ⚠ THE COOK IS NOT REPRODUCIBLE, AND THE VARIANCE REACHES AFL CONTENT

**Two CLEAN cooks of the same content produced different package sets.** `08-10` and `08-10c` were both run
into an emptied `Saved/Cooked/Windows` with the same command, and they disagree in both directions. Every
run reported `Success - 0 error(s)`.

| package | 08-09 | 08-10 (clean) | 08-10b (incr) | 08-10c (clean) |
|---|:--:|:--:|:--:|:--:|
| `L_ShantyTown/ExtraSpawn` | ✅ | ✅ | ✅ | **✗** |
| `L_ShantyTown/Gameplay` | ✅ | ✅ | ✅ | **✗** |
| `L_ShantyTown/Layout` | ✅ | ✅ | ✅ | **✗** |
| `L_ShantyTown/Lighting` | ✅ | ✅ | ✅ | **✗** |
| `L_ShantyTown/District_{Arena,Duel,Team}` | ✅ | ✅ | ✅ | ✅ |
| `ShooterMaps L_Convolution/{Gameplay,Layout}` | ✅ | **✗** | ✅ | ✅ |

`L_ShantyTown.umap` itself cooked in every run — **only DataLayer packages move.** An earlier note here
blamed a clean-vs-incremental difference; that was wrong, and this table is what refuted it. The
incremental run is the one that agreed with history.

**Why it matters:** ShantyTown is the next BR map and its districts are streamed content. A build missing
`Layout`/`Gameplay`/`Lighting` is a broken streaming setup that PIE will never reproduce, because in PIE
every asset is on disk — the same shape as every cook-refs bug in `DefaultGame.ini`'s
`DirectoriesToAlwaysCook` block. The dangerous direction is the one observed: **fewer packages, still
"0 errors".**

**Confound not yet excluded:** `08-10c` ran immediately after a cook killed by an out-of-memory abort. The
partial tree was deleted before retrying, but the DerivedDataCache persisted.

**So: diff every cook against a manifest before trusting it, and record whether a previous cook aborted.**
Tracked as its own ticket with a three-step isolation plan.

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
