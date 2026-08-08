<#
.SYNOPSIS
  Launch the Bag_Man UE5 editor with the AFL economy / match-reporting environment populated.

.DESCRIPTION
  UAFLOnlineSubsystem::Initialize reads its endpoint + signing config from the PROCESS ENVIRONMENT
  exactly ONCE, at subsystem startup (AFLOnlineSubsystem.cpp:118-134). Setting these variables in an
  already-running editor does nothing -- the editor must be LAUNCHED with them present. That is what
  this script is for.

  IsMatchReportingConfigured() (AFLOnlineSubsystem.cpp:591) gates the whole escrow -> settle -> rating
  chain on four of them:

      AFL_EARN_HMAC_KEY   AFL_ESCROW_URL   AFL_SETTLE_URL   AFL_RATING_URL

  Without all four the chain silently no-ops and AFLMatchReporter logs
  "AFL_MATCHREPORT: economy not wired". The other two (AFL_EARN_URL, AFL_RESOLVE_URL) drive the
  A1.3b / A1.4 canaries and are set here too because they cost nothing extra.

  URLs are read from the LIVE CloudFormation stack outputs rather than cdk-outputs.json, because that
  file is only rewritten on a `cdk deploy --outputs-file` run and was found stale on 2026-08-08 (it
  was missing every endpoint added after /award-achievement, including all three C endpoints).

  THE HMAC KEY IS NEVER WRITTEN TO DISK, NEVER LOGGED, AND NEVER ECHOED. It is pulled from Secrets
  Manager into this process's memory and inherited by the editor child process. The script reports
  only "held" or "MISSING", mirroring the engine's own log line.

.PARAMETER DryRun
  Resolve and report everything, but do not launch the editor. Use this to confirm the environment
  is fully green before committing to an editor boot.

.PARAMETER Stack
  CloudFormation stack name. Defaults to BagManTentpoleStack.

.PARAMETER SecretId
  Secrets Manager id for the earn HMAC key. Defaults to bagman/earn/hmac.

.PARAMETER EditorArgs
  Any remaining arguments are forwarded verbatim to UnrealEditor.exe.

.EXAMPLE
  .\Tools\Launch-Editor-Economy.ps1 -DryRun
  .\Tools\Launch-Editor-Economy.ps1
#>
[CmdletBinding()]
param(
    [switch] $DryRun,
    [string] $Stack    = 'BagManTentpoleStack',
    [string] $SecretId = 'bagman/earn/hmac',
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $EditorArgs
)

$ErrorActionPreference = 'Stop'

$ProjectPath = 'C:\Dev\Bag_Man\Bag_Man.uproject'
$EditorPath  = 'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe'

# Engine env var  <-  CloudFormation output key
$UrlMap = [ordered]@{
    'AFL_ESCROW_URL'  = 'EscrowEntryInvokeUrl'
    'AFL_SETTLE_URL'  = 'SettleMatchInvokeUrl'
    'AFL_RATING_URL'  = 'UpdateRatingInvokeUrl'
    'AFL_EARN_URL'    = 'EarnInvokeUrl'
    'AFL_RESOLVE_URL' = 'ResolveInvokeUrl'
}
# The four that IsMatchReportingConfigured() actually gates on.
$Required = @('AFL_EARN_HMAC_KEY', 'AFL_ESCROW_URL', 'AFL_SETTLE_URL', 'AFL_RATING_URL')

function Write-Step($msg) { Write-Host "  $msg" }

Write-Host ''
Write-Host 'AFL economy launch' -ForegroundColor Cyan
Write-Host '------------------'

# --- 0. Preflight -----------------------------------------------------------------------------
if (-not (Test-Path $ProjectPath)) { throw "Project not found: $ProjectPath" }
if (-not (Test-Path $EditorPath))  { throw "Editor not found: $EditorPath" }
if (-not (Get-Command aws -ErrorAction SilentlyContinue)) {
    throw 'AWS CLI not on PATH. It is required to resolve endpoints and the signing key.'
}

# An editor that is already up captured the OLD environment at ITS launch. Relaunching is the only
# way to pick up these values, so refuse to add a second instance silently.
$running = Get-Process -Name 'UnrealEditor' -ErrorAction SilentlyContinue
if ($running -and -not $DryRun) {
    Write-Host ''
    Write-Warning ("UnrealEditor is ALREADY RUNNING (pid $($running.Id -join ', ')).")
    Write-Warning 'That instance read its environment at ITS launch and cannot pick these up.'
    Write-Warning 'Close it first, then re-run. Aborting so you do not get two editors.'
    exit 1
}

# --- 1. Endpoints from the live stack ---------------------------------------------------------
Write-Step "Resolving endpoints from CloudFormation stack '$Stack' ..."
$raw = aws cloudformation describe-stacks --stack-name $Stack --query 'Stacks[0].Outputs' --output json
if ($LASTEXITCODE -ne 0) { throw "describe-stacks failed. Are AWS credentials configured for account 302659227808?" }

$outputs = @{}
foreach ($o in ($raw | ConvertFrom-Json)) { $outputs[$o.OutputKey] = $o.OutputValue }

foreach ($var in $UrlMap.Keys) {
    $key = $UrlMap[$var]
    if ($outputs.ContainsKey($key)) {
        Set-Item -Path "env:$var" -Value $outputs[$key]
    } else {
        Write-Warning "Stack output '$key' not found -> $var will be MISSING. Has the stack been deployed?"
        Set-Item -Path "env:$var" -Value ''
    }
}

# --- 2. Signing key from Secrets Manager ------------------------------------------------------
# Captured straight into the environment. Not written anywhere, not printed.
Write-Step "Fetching signing key from Secrets Manager ('$SecretId') ..."
$secret = aws secretsmanager get-secret-value --secret-id $SecretId --query SecretString --output text
if ($LASTEXITCODE -ne 0) { throw "Could not read secret '$SecretId'. Check IAM permissions for the current caller." }

# The escrow/settle/rating Lambdas use res.SecretString RAW as the HMAC key -- no JSON unwrapping
# (lambda/escrow-entry/index.ts:76). So the env var must be byte-identical to the stored string.
# A multi-line capture would arrive here as an array and get silently mangled; refuse instead.
if ($secret -is [array]) { throw 'Secret is multi-line; refusing to guess how to join it. HMAC would not match.' }
$env:AFL_EARN_HMAC_KEY = $secret
Remove-Variable secret

# --- 3. Report (mirrors the engine's own boot line) --------------------------------------------
Write-Host ''
Write-Host 'Environment:' -ForegroundColor Cyan
$missing = @()
foreach ($var in @('AFL_EARN_HMAC_KEY') + @($UrlMap.Keys)) {
    $val = [Environment]::GetEnvironmentVariable($var, 'Process')
    $req = $Required -contains $var

    if ([string]::IsNullOrEmpty($val)) {
        $shown = 'MISSING'
        $color = if ($req) { 'Red' } else { 'Yellow' }
        if ($req) { $missing += $var }
    } elseif ($var -eq 'AFL_EARN_HMAC_KEY') {
        $shown = 'held'          # never print the key
        $color = 'Green'
    } else {
        $shown = $val
        $color = 'Green'
    }

    $tag = if ($req) { '[gates C]' } else { '[canary] ' }
    Write-Host ("  {0} {1,-18} {2}" -f $tag, $var, $shown) -ForegroundColor $color
}

if ($missing.Count -gt 0) {
    Write-Host ''
    throw ("Refusing to launch: {0} still MISSING. Match reporting would silently no-op." -f ($missing -join ', '))
}

Write-Host ''
Write-Host '  All four gating values present -- IsMatchReportingConfigured() will return true.' -ForegroundColor Green

# --- 4. Launch ---------------------------------------------------------------------------------
if ($DryRun) {
    Write-Host ''
    Write-Host '  -DryRun: not launching. Re-run without -DryRun to boot the editor.' -ForegroundColor Yellow
    exit 0
}

Write-Host ''
Write-Step 'Launching editor ...'
Write-Host ''
Write-Host '  Confirm at the top of the log (this is the cheapest possible check):' -ForegroundColor Cyan
Write-Host '    [AFLOnline] Server signer (editor): key=held escrowUrl=... settleUrl=... ratingUrl=...'
Write-Host ''
Write-Host '  Then, in the editor console:  afl.Match.Result.Test   ->  expect AFL_RESULTTEST: ... ALL GREEN'
Write-Host ''

# NB: `@(x) + $null` yields an array CONTAINING a null, which Start-Process rejects outright.
# EditorArgs is null whenever no extra arguments are passed, so it must be appended conditionally.
$argList = @("`"$ProjectPath`"")
if ($EditorArgs) { $argList += $EditorArgs }
Start-Process -FilePath $EditorPath -ArgumentList $argList
