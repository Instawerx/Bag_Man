# Cook manifests

One CSV per full cook: `Path,Length` for every file in the tree, relative to the platform root
(`Saved/Cooked/<Platform>`). They exist so that **"what changed in the build?" is answerable without
keeping 9.5 GB of cooked output on disk**, and without re-cooking to find out.

⭐ **`cook_20260810d` is the current reference baseline** — the most recent clean cook from a healthy
preceding state. Diff against that one, never against `b` (incremental) or `c` (post-abort); the section
below is why that distinction decides whether a diff means anything.

| File | Tree | Files | Packages | Size |
|---|---|---|---|---|
| `cook_20260809_windows.csv` | Windows client, 2026-08-09 17:46 | 17,204 | 7,333 | 9,715 MB |
| `cook_20260810_windows.csv` | Windows client, 2026-08-10 12:37 | 17,195 | 7,329 | 9,713 MB |
| `cook_20260810b_windows.csv` | Windows client, 2026-08-10 late — **incremental** | 17,207 | 7,335 | 9,713 MB |
| `cook_20260810c_windows.csv` | Windows client, 2026-08-10 later — **clean, after an OOM abort** | 17,199 | 7,331 | 9,713 MB |
| `cook_20260810d_windows.csv` | Windows client, 2026-08-10 last — **clean, healthy state** ⭐ | 17,203 | 7,333 | 9,713 MB |

The **b** cook confirmed a claim the previous one had only made by reasoning: the Career hub's soft class
pointer returns `W_ReplayBrowserScreen` and `W_ReplayListEntry` to the build after HOST's deprecation had
removed their last referencer. 0 removed, 6 added, 0 errors. `W_ExperienceSelectionScreen` is still absent,
so the deprecation held across a second cook rather than only the one that introduced it.

## ⚠ A COOK IS ONLY REPRODUCIBLE FROM A HEALTHY PRECEDING STATE

**The cook itself is deterministic. Its INPUT STATE is what varies, and a disturbed one silently changes
the output.** Four cooks settled this; every one reported `Success - 0 error(s)`.

| package | 08-10 clean | 08-10b **incremental** | 08-10c clean, **after an abort** | 08-10d clean |
|---|:--:|:--:|:--:|:--:|
| `L_ShantyTown/{ExtraSpawn,Gameplay,Layout,Lighting}` | ✅ | ✅ | **✗ dropped** | ✅ |
| `L_ShantyTown/District_{Arena,Duel,Team}` | ✅ | ✅ | ✅ | ✅ |
| `ShooterMaps L_Convolution/{Gameplay,Layout}` | ✗ | **✅ extra** | ✅ | ✗ |

**08-10 → 08-10d — the only two runs that are like-for-like (clean, no preceding abort) — differ by
0 removed and exactly +4 added: the four packages actually authored in between.** Identical otherwise.
So the two anomalies are both state, not randomness:

- **An aborted cook poisons the next clean one.** `08-10c` followed a cook killed by OOM. Deleting the
  partial tree was not enough — something survives it (DDC or WorldPartition state) and cost four
  **AFL** DataLayer packages. This is the dangerous direction: *fewer* packages, no error, and a defect
  PIE can never reproduce because in PIE every asset is on disk. Same shape as every cook-refs bug the
  `DirectoriesToAlwaysCook` block exists to stop.
- **An incremental cook emits packages a clean cook does not.** The two Convolution packages appear only
  in trees descended from `08-10b`. Less dangerous — extra, not missing — but it means an incremental
  tree is not a valid shipping artifact or manifest baseline.

**Rules that follow, and they are cheap:**
1. **After any aborted or killed cook, do not trust the next one.** Delete the cook tree *and* clear the
   DerivedDataCache before re-cooking.
2. **Ship and baseline from clean cooks only.**
3. **Diff every cook against a manifest anyway.** All of this was invisible in the totals and in the exit
   code; only the package-level diff showed it.

Two earlier characterisations here were wrong and are corrected by the table above: this was first called
nondeterminism, then a clean-vs-incremental confound. It is neither on its own — it is *disturbed input
state*, of two different kinds.

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
