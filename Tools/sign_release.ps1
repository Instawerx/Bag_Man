<#
  Authenticode-sign IRONICS release artifacts with Azure Trusted Signing (Artifact Signing).

  DORMANT UNTIL ACTIVATED: needs (1) an Azure credential the machine can use non-interactively
  (`az login` cached token, or a service principal via AZURE_TENANT_ID/CLIENT_ID/CLIENT_SECRET env vars),
  (2) a Public-Trust certificate profile under the "IRONICS" signing account (created after Identity
  Validation clears), and (3) the Trusted Signing Client Tools installed (SignTool plugin + the
  Azure.CodeSigning.Dlib + .NET 8). Until then this script errors out cleanly and signs nothing.

  Usage:
    sign_release.ps1 -Profile <certProfileName> -Files a.exe,b.dll [-Account IRONICS] [-Endpoint https://eus.codesigning.azure.net]

  The endpoint is REGION-pinned: the "IRONICS" account is East US -> eus.codesigning.azure.net.
#>
param(
  [Parameter(Mandatory=$true)][string]$Profile,
  [Parameter(Mandatory=$true)][string[]]$Files,
  [string]$Account   = "IRONICS",
  [string]$Endpoint  = "https://eus.codesigning.azure.net",
  [string]$Timestamp = "http://timestamp.acs.microsoft.com",
  [string]$Dlib,        # path to Azure.CodeSigning.Dlib.dll; auto-discovered if omitted
  [string]$SignTool     # path to signtool.exe; auto-discovered if omitted
)
$ErrorActionPreference = "Stop"

# --- locate signtool.exe (Windows SDK) and the Trusted Signing dlib ---
if (-not $SignTool) {
  $SignTool = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
              Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $SignTool -or -not (Test-Path $SignTool)) { throw "signtool.exe not found — install the Windows SDK / Trusted Signing Client Tools." }

if (-not $Dlib) {
  $Dlib = Get-ChildItem "$env:USERPROFILE\.azuresigningtools\*\Azure.CodeSigning.Dlib.dll","C:\Program Files\*\Azure.CodeSigning.Dlib.dll" -ErrorAction SilentlyContinue |
          Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $Dlib -or -not (Test-Path $Dlib)) { throw "Azure.CodeSigning.Dlib.dll not found — install the Trusted Signing Client Tools." }

# --- write the run's metadata.json (endpoint + account + profile the dlib reads) ---
$meta = [ordered]@{
  Endpoint              = $Endpoint
  CodeSigningAccountName = $Account
  CertificateProfileName = $Profile
  CorrelationId          = "ironics-" + (Get-Date -Format "yyyyMMddHHmmss")
}
$metaPath = Join-Path $env:TEMP "ironics-trustedsigning-metadata.json"
[System.IO.File]::WriteAllText($metaPath, ($meta | ConvertTo-Json), (New-Object System.Text.UTF8Encoding($false)))
Write-Host "signtool=$SignTool"
Write-Host "dlib=$Dlib"
Write-Host "metadata=$metaPath (account=$Account profile=$Profile endpoint=$Endpoint)"

# --- sign each file, then verify ---
foreach ($f in $Files) {
  if (-not (Test-Path $f)) { throw "file to sign not found: $f" }
  Write-Host "--- signing $f ---" -ForegroundColor Cyan
  & $SignTool sign /v /fd SHA256 /tr $Timestamp /td SHA256 /dlib $Dlib /dmdf $metaPath $f
  if ($LASTEXITCODE -ne 0) { throw "signtool sign failed for $f (exit $LASTEXITCODE)" }
  & $SignTool verify /pa /v $f
  if ($LASTEXITCODE -ne 0) { throw "signtool verify failed for $f (exit $LASTEXITCODE)" }
}
Write-Host "=== SIGNED + VERIFIED $($Files.Count) file(s) as profile '$Profile' ===" -ForegroundColor Green
