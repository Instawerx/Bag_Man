<#
  Package + publish one IRONICS Shipping client release. Run AFTER the cook succeeds.
  Encodes the proven flow (0.1.0 .. 0.1.3): provenance gate -> pdb-free child-named bsdtar Zip64
  -> Shell.Application (Explorer reader) verify -> publish-release.ps1 (S3 + latest.json flip).

  Usage:
    package_and_publish.ps1 -Version 0.1.4-beta [-Stage <dir>] [-DryRun]
  -DryRun does everything EXCEPT the S3 publish (packages + verifies the zip locally).
#>
param(
  [Parameter(Mandatory=$true)][string]$Version,
  [string]$Stage      = "C:\Dev\Bag_Man\Saved\StagedBuilds\Windows",
  [string]$ReleaseDir = "D:\BagMan\releases\win64",
  [string]$SymbolsDir = "D:\BagMan\symbols",
  [string]$ProvenanceScript = "C:\Dev\Bag_Man\Tools\verify_ship_provenance.ps1",
  [string]$PublishScript    = "C:\Dev\Ironics-Platform\apps\api\scripts\publish-release.ps1",
  [switch]$DryRun
)
$ErrorActionPreference = "Stop"

Write-Host "=== PACKAGE $Version ===" -ForegroundColor Cyan
if (-not (Test-Path $Stage))       { throw "stage not found: $Stage (did the cook -stage succeed?)" }
$exe = Join-Path $Stage "Bag_Man\Binaries\Win64\IRONICS.exe"
$exeAlt = Get-ChildItem -Path $Stage -Recurse -Filter "IRONICS.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not (Test-Path $exe) -and $exeAlt) { $exe = $exeAlt.FullName }
if (-not (Test-Path $exe)) { throw "IRONICS.exe not found under $Stage -- wrong target or cook failed" }
Write-Host "  staged exe: $exe"

# 1. Ship-provenance gate (child PS; the script ends with `exit`).
Write-Host "--- provenance gate ---" -ForegroundColor Cyan
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $ProvenanceScript -Path $Stage
if ($LASTEXITCODE -ne 0) { throw "SHIP PROVENANCE REFUSED (exit $LASTEXITCODE) -- not a D: source build. Aborting." }

# 2. Move PDBs out of the stage (pdb-free zip); keep them as symbols.
$sym = Join-Path $SymbolsDir $Version
$pdbs = Get-ChildItem -Path $Stage -Recurse -Filter "*.pdb" -ErrorAction SilentlyContinue
if ($pdbs) {
  New-Item -ItemType Directory -Force -Path $sym | Out-Null
  foreach ($p in $pdbs) { Move-Item -Force $p.FullName (Join-Path $sym $p.Name) }
  Write-Host "  moved $($pdbs.Count) pdb(s) -> $sym"
} else { Write-Host "  no pdbs in stage" }

# 3. bsdtar Zip64, naming the top-level CHILDREN (never '.', which makes Explorer show 0 items).
$relDir = Join-Path $ReleaseDir $Version
New-Item -ItemType Directory -Force -Path $relDir | Out-Null
$zip = Join-Path $relDir "IRONICS-$Version-Win64.zip"
if (Test-Path $zip) { throw "release zip already exists: $zip (releases are immutable -- bump the version)" }
$top = Get-ChildItem -LiteralPath $Stage | ForEach-Object Name
Write-Host "--- zipping $($top.Count) top-level entries: $($top -join ', ') ---" -ForegroundColor Cyan
Push-Location $Stage
tar -a -c -f "$zip" --exclude "*.pdb" @top
$tarExit = $LASTEXITCODE
Pop-Location
if ($tarExit -ne 0) { throw "tar failed (exit $tarExit)" }
$size = (Get-Item $zip).Length
Write-Host "  zip: $zip ($([math]::Round($size/1GB,2)) GB)"

# 4. Verify with Explorer's reader (the audience's reader) -- MUST be non-zero (0.1.2 empty-zip law).
$sh = New-Object -ComObject Shell.Application
$count = $sh.NameSpace($zip).Items().Count
Write-Host "  Shell.Application top-level items = $count"
if ($count -lt 1) { throw "EMPTY-ZIP: Explorer reader sees 0 items -- packaging defect (the './' bug). Aborting." }

# 5. sha256 (also handed to publish so it isn't recomputed).
$hash = (Get-FileHash -Algorithm SHA256 -Path $zip).Hash.ToLower()
$shaPath = "$zip.sha256"
[System.IO.File]::WriteAllText($shaPath, "$hash  IRONICS-$Version-Win64.zip", (New-Object System.Text.UTF8Encoding($false)))
Write-Host "  sha256 = $hash"

if ($DryRun) {
  Write-Host "=== DRY RUN complete -- package built + verified, NOT published ===" -ForegroundColor Yellow
  Write-Host "  zip:   $zip"
  Write-Host "  items: $count   size: $size   sha256: $hash"
  return
}

# 6. Publish: S3 upload + latest.json flip (the website update). Re-runs the provenance gate on the stage.
Write-Host "--- publishing to ironics-releases + flipping latest.json ---" -ForegroundColor Cyan
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PublishScript `
    -Zip $zip -Version $Version -Channel "beta" `
    -Sha256Path $shaPath -ProvenanceScript $ProvenanceScript -StagedDir $Stage
if ($LASTEXITCODE -ne 0) { throw "publish-release.ps1 failed (exit $LASTEXITCODE)" }
Write-Host "=== PUBLISHED $Version -- latest.json now points at it; the portal download card serves it ===" -ForegroundColor Green
