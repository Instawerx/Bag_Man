# Cook manifests

One CSV per full cook: `Path,Length` for every file in the tree, relative to the platform root
(`Saved/Cooked/<Platform>`). They exist so that **"what changed in the build?" is answerable without
keeping 9.5 GB of cooked output on disk**, and without re-cooking to find out.

| File | Tree | Files | Packages | Size |
|---|---|---|---|---|
| `cook_20260809_windows.csv` | Windows client, 2026-08-09 17:46 | 17,204 | 7,333 | 9,715 MB |
| `cook_20260810_windows.csv` | Windows client, 2026-08-10 12:37 | 17,195 | 7,329 | 9,713 MB |
| `cook_20260810b_windows.csv` | Windows client, 2026-08-10 late | 17,207 | 7,335 | 9,713 MB |

The **b** cook confirmed a claim the previous one had only made by reasoning: the Career hub's soft class
pointer returns `W_ReplayBrowserScreen` and `W_ReplayListEntry` to the build after HOST's deprecation had
removed their last referencer. 0 removed, 6 added, 0 errors. `W_ExperienceSelectionScreen` is still absent,
so the deprecation held across a second cook rather than only the one that introduced it.

⚠ **Two of those six additions are not ours, and they have now moved in both directions.**
`ShooterMaps/.../L_Convolution_Blockout/{Gameplay,Layout}.uasset` **left** in the 08-09 → 08-10 diff and
**came back** in 08-10 → 08-10b, with nothing in either change touching ShooterMaps.

**Do not read that as nondeterminism — there is a confound, and it is the first thing to test.** The 08-10
cook ran into an *empty* `Saved/Cooked/Windows`; 08-10b ran *over* 08-10's tree. So the honest statement is
that a cook over an existing tree did not produce the same set as a clean one, which if true matters more
than randomness would: it means an incremental cook is not equivalent to the clean cook these manifests are
compared against. Ruled out already: the map generated identically both times — same World Partition cells,
same `GenerationHash` per cell — so the difference is in whether those two packages are *emitted*, not in
how the level was built.

**So when you take a manifest, note whether the tree was empty first.** Comparing a clean cook to an
incremental one is not a like-for-like diff until this is settled. Tracked as its own ticket; not chased
here because it is 2 packages of 7,335, neither of them AFL content, and both cooks reported 0 errors.

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
