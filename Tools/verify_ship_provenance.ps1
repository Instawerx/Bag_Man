<#
.SYNOPSIS
  Ship-provenance gate: verifies a staged/shipping UE artifact was built by the D: SOURCE
  engine (D:\UE5.6-source), NOT the retired C: launcher/Installed engine.

.DESCRIPTION
  MANDATORY before any R2 upload (Docs/ENGINE_DOCTRINE.md). Engine provenance is baked into
  the monolithic game exe's PE version resource (ProductVersion / FileVersion) by
  FEngineVersion, in the form "<BranchName>-CL-<Changelist>". Observed on real artifacts
  2026-09-03:
    D: source engine   ->  "UE5-CL-0"                        (BranchName "UE5", IsPromotedBuild=0)
    C: launcher engine ->  "++UE5+Release-5.6-CL-44394996"   (promoted farm branch, IsPromotedBuild=1)
  A "++"-prefixed promoted branch is the visible proxy for IsPromotedBuild=1 -- i.e. a
  launcher/Installed-engine build. The source engine stamps the plain local branch.

  Shipping builds are MONOLITHIC, so there is no staged Build.version / .modules / .target to
  read -- the exe version resource is the reliable, always-present marker.

.PARAMETER Path
  A staged build ROOT, a Binaries\Win64 directory, or a single .exe.

.PARAMETER SourceBranch
  The D: source engine's Build.version BranchName (default "UE5"). PASS requires the exe's
  branch to equal this AND to be non-promoted.

.OUTPUTS
  exit 0  -> "SHIP PROVENANCE VERIFIED (D:\UE5.6-source)"
  exit 1  -> loud refusal (launcher/promoted stamp, unknown stamp, or no UE-stamped exe)
#>
param(
    [Parameter(Mandatory = $true)][string]$Path,
    [string]$SourceBranch = "UE5"
)

$ErrorActionPreference = "Stop"
$skip = "EpicWebHelper|CrashReportClient|EOSBootstrapper|UnrealCEFSubProcess"

function Find-GameExes([string]$p) {
    if (Test-Path $p -PathType Leaf) { return @(Get-Item $p) }
    if (-not (Test-Path $p))          { throw "path not found: $p" }
    # Prefer the monolithic game/client/server exe under a Binaries\Win64 folder.
    $exes = Get-ChildItem -Path $p -Recurse -Filter "*.exe" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match "Binaries\\Win64" -and $_.Name -notmatch $skip }
    if (-not $exes) {
        # Fall back to any top-level exe (the staged bootstrapper stub carries the same stamp).
        $exes = Get-ChildItem -Path $p -Filter "*.exe" -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -notmatch $skip }
    }
    return $exes
}

$exes = Find-GameExes $Path
if (-not $exes -or $exes.Count -eq 0) {
    Write-Host "SHIP PROVENANCE: no game exe found under $Path" -ForegroundColor Red
    exit 1
}

$fail = $false
$checked = 0
foreach ($e in $exes) {
    $vi = (Get-Item $e.FullName).VersionInfo
    $pv = $vi.ProductVersion
    if ([string]::IsNullOrWhiteSpace($pv)) { $pv = $vi.FileVersion }
    if ([string]::IsNullOrWhiteSpace($pv)) { continue }   # non-UE helper exe: no stamp
    $checked++

    if ($pv -match '^(?<branch>.+)-CL-(?<cl>\d+)$') { $branch = $Matches.branch; $cl = $Matches.cl }
    else { $branch = $pv; $cl = "?" }

    $isPromoted = $branch -match '^\+\+'
    if (($branch -eq $SourceBranch) -and (-not $isPromoted)) {
        Write-Host ("  PASS  {0,-42} [{1}]  branch={2} CL={3}" -f $e.Name, $pv, $branch, $cl) -ForegroundColor Green
    }
    else {
        $why = if ($isPromoted) { "PROMOTED/LAUNCHER build" } else { "unexpected branch (want '$SourceBranch')" }
        Write-Host ("  FAIL  {0,-42} [{1}]  branch={2} CL={3}  <- {4}" -f $e.Name, $pv, $branch, $cl, $why) -ForegroundColor Red
        $fail = $true
    }
}

if ($checked -eq 0) {
    Write-Host "SHIP PROVENANCE: no UE-stamped exe under $Path (only unstamped helpers)" -ForegroundColor Red
    exit 1
}

Write-Host ""
if ($fail) {
    Write-Host "SHIP PROVENANCE REFUSED: artifact was NOT built by the D: source engine (D:\UE5.6-source)." -ForegroundColor Red
    Write-Host "A promoted stamp (++UE5+Release-*) is a C: launcher/Installed build. Rebuild on D: before any R2 upload." -ForegroundColor Red
    exit 1
}
else {
    Write-Host "SHIP PROVENANCE VERIFIED (D:\UE5.6-source)" -ForegroundColor Green
    exit 0
}
