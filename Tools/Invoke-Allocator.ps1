<#
.SYNOPSIS
  Drive the DEPLOYED match-allocator and capture the GameSessionData it hands to GameLift.

.DESCRIPTION
  Produces the MatchmakerData payload for a dedicated-server run of #20, in the one way that is honest:
  by making the real allocator produce it.

  WHAT IS REAL HERE, AND WHAT IS NOT.
    REAL  - the allocator Lambda runs, resolveEconomics() executes, and it verifies that EVERY member
            agreed on tier/league/stake/currency, rejecting mismatches with a 400. R86 (staked play is
            PRO MOD only) is enforced there too. The economics block that ends up in GameSessionData is
            the allocator's OUTPUT, not something written by hand.
    REAL  - the payload is read back from the GameLift PLACEMENT the allocator created, so it is exactly
            the bytes GameLift received.
    NOT   - no live players queued. The ticket attributes below stand in for what PlayFab matchmaking
            would attach. That is the one simulated input, and the allocator still validates it.

  This matters because an earlier attempt fabricated the economics with a dev cvar, which proved only that
  the fake agreed with itself. See the revert in AFLMatchReporter.cpp.

  The placement will NOT fulfil - the Anywhere fleet has zero registered compute (that is S12). It does not
  need to: the placement exists in PENDING and its GameSessionData is readable, which is all we want.

  THE SIGNING KEY IS NEVER WRITTEN TO DISK OR ECHOED. Note this endpoint uses the TENTPOLE secret
  (bagman/tentpole/hmac), a different trust domain from the earn/money key used by escrow and settle.

.EXAMPLE
  .\Tools\Invoke-Allocator.ps1 -PlayFabId DF7C3188377BB66D,40419031BCC83EA5 -Stake 10
#>
[CmdletBinding()]
param(
    # Two PlayFabIds. These become GameLift PlayerIds AND, via ?PlayFabId= on connect, the server-side
    # ReconcileIds -- so matchmaker identity and reconcile key agree by construction.
    [Parameter(Mandatory = $true)]
    [string[]] $PlayFabId,

    [ValidateSet('WattsPlay', 'VoltsPlay', 'LeaguePlay')]
    [string] $Tier = 'VoltsPlay',

    [ValidateSet('ProMod', 'Haywire')]
    [string] $League = 'ProMod',

    [int] $Stake = 10,

    [ValidateSet('VO', 'WA')]
    [string] $Currency = 'VO',

    [string] $MatchId,
    [string] $OutFile  = 'D:\BagMan\captured-matchmakerdata.json',
    [string] $Stack    = 'BagManTentpoleStack',
    [string] $SecretId = 'bagman/tentpole/hmac'
)

$ErrorActionPreference = 'Stop'

if ($PlayFabId.Count -lt 2) {
    throw "Need at least 2 PlayFabIds: MATCH PLAY resolves over exactly 2 finishing positions, so a staked match needs two teams with a human in each."
}

Write-Host ''
Write-Host 'Allocator capture' -ForegroundColor Cyan
Write-Host '-----------------'

if (-not (Get-Command aws -ErrorAction SilentlyContinue)) { throw 'AWS CLI not on PATH.' }

$raw = aws cloudformation describe-stacks --stack-name $Stack --query 'Stacks[0].Outputs' --output json
if ($LASTEXITCODE -ne 0) { throw 'describe-stacks failed; check AWS credentials.' }
$url = (($raw | ConvertFrom-Json) | Where-Object { $_.OutputKey -eq 'InvokeUrl' }).OutputValue
if (-not $url) { throw "Stack output 'InvokeUrl' (the /allocate endpoint) not found." }
Write-Host "  endpoint : $url"

if (-not $MatchId) { $MatchId = "bagman-drill-" + ([guid]::NewGuid().ToString('N').Substring(0, 12)) }
Write-Host "  matchId  : $MatchId"

# One member per PlayFabId, alternating teams so the roster yields two teams.
$members = @()
for ($i = 0; $i -lt $PlayFabId.Count; $i++) {
    $members += [ordered]@{
        Entity     = [ordered]@{ Id = $PlayFabId[$i]; Type = 'title_player_account' }
        TeamId     = "$($i % 2)"
        # The stand-in for PlayFab matchmaking ticket attributes. EVERY member carries the SAME values --
        # a disagreement is what resolveEconomics() is built to catch, and it will 400 the request.
        Attributes = [ordered]@{ DataObject = [ordered]@{ tier = $Tier; league = $League; stake = $Stake; currency = $Currency } }
    }
}
$bodyObj = [ordered]@{ MatchId = $MatchId; Members = $members; RegionPreferences = @(); ServerDetails = $null }
$body    = $bodyObj | ConvertTo-Json -Depth 10 -Compress

$secret = aws secretsmanager get-secret-value --secret-id $SecretId --query SecretString --output text
if ($LASTEXITCODE -ne 0) { throw "Could not read '$SecretId'. NOTE this endpoint uses the TENTPOLE secret, not the earn key." }
if ([string]::IsNullOrWhiteSpace($secret)) { throw "'$SecretId' is EMPTY. It is created by CDK but populated out-of-band post-deploy." }
if ($secret -is [array]) { throw 'Secret is multi-line; refusing to guess how to join it.' }

$hmac = New-Object System.Security.Cryptography.HMACSHA256
$hmac.Key = [Text.Encoding]::UTF8.GetBytes($secret)
Remove-Variable secret
$sig = -join ($hmac.ComputeHash([Text.Encoding]::UTF8.GetBytes($body)) | ForEach-Object { $_.ToString('x2') })
$hmac.Dispose()

Write-Host "  posting  : $($PlayFabId.Count) members, $Tier/$League, stake $Stake $Currency"
try {
    $resp = Invoke-RestMethod -Uri $url -Method Post -Body $body -ContentType 'application/json' -Headers @{ 'X-Signature' = $sig }
    Write-Host "  allocator: $($resp | ConvertTo-Json -Depth 6 -Compress)" -ForegroundColor Green
}
catch {
    $status = if ($_.Exception.Response) { [int]$_.Exception.Response.StatusCode } else { '?' }
    # A 400 here is usually resolveEconomics rejecting the roster -- that is the verification working.
    Write-Host "  allocator returned http=$status" -ForegroundColor Yellow
    Write-Host "  $($_.ErrorDetails.Message)" -ForegroundColor Yellow
}

# The payload as GAMELIFT received it. This is the capture -- not a reconstruction.
$placementId = ("bagman-" + ($MatchId -replace '[^a-zA-Z0-9-]', '-'))
if ($placementId.Length -gt 48) { $placementId = $placementId.Substring(0, 48) }
Write-Host ''
Write-Host "  reading placement '$placementId' ..."
$pl = aws gamelift describe-game-session-placement --placement-id $placementId --output json 2>&1
if ($LASTEXITCODE -ne 0) { throw "No placement found. The allocator did not reach StartGameSessionPlacement; read its response above." }

$p = ($pl | ConvertFrom-Json).GameSessionPlacement
Write-Host "  status   : $($p.Status)   (PENDING is expected -- the fleet has no registered compute until S12)"

if (-not $p.GameSessionData) { throw 'Placement carries no GameSessionData.' }
$dir = Split-Path $OutFile -Parent
if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
[IO.File]::WriteAllText($OutFile, $p.GameSessionData, (New-Object Text.UTF8Encoding($false)))

Write-Host ''
Write-Host 'CAPTURED GameSessionData (verbatim, as GameLift received it):' -ForegroundColor Green
Write-Host $p.GameSessionData
Write-Host ''
Write-Host "  written to: $OutFile"
Write-Host "  use as    : LyraServer.exe <Map>?MatchmakerData=<contents>" -ForegroundColor Cyan
