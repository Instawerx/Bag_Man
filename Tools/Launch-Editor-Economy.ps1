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

    # Launch the DEDICATED SERVER instead of the editor. The server reads the same six env vars through the
    # same IsRunningDedicatedServer() || GIsEditor gate, so the secret handling below is shared rather than
    # duplicated into a second script.
    [switch] $Server,

    # Server only. Map to open, and the file holding allocator-captured GameSessionData, which is passed as
    # ?MatchmakerData= -- the source UAFLMatchmakerDataProvider::ResolveGameSessionData reads today.
    [string] $Map = 'L_Arena_04',
    [string] $Experience = 'B_AFLExperience_2v2_ProMod',
    [string] $MatchmakerDataFile = 'D:\BagMan\captured-matchmakerdata.json',
    [int]    $Port = 7777,

    # Bots are REFUSED a team in an authoritative match (R74) but still spawn, and the spawn manager then
    # ensures on PlayerTeamId != INDEX_NONE. Suppress them at the source. UAFLBotFillComponent reads NumBots
    # from the same OptionsString, so this is a URL option, not the editor's bOverrideBotCount setting.
    [int]    $NumBots = 0,

    # Warmup must outlast CLIENT BOOT. Default 30s expired 41s before two cooked clients finished loading,
    # so ServerStartMatch escrowed an EMPTY match: "0 team(s) with players -- need 2". Passed via -dpcvars
    # because device-profile cvars apply early enough to be read at BeginPlay; -ExecCmds runs too late.
    [int]    $WarmupSeconds = 180,

    # S12: start under GameLift instead of reading the roster from ?MatchmakerData=. Populates the five
    # AFL_GAMELIFT_* vars UAFLGameLiftHostSubsystem reads, including a FRESHLY MINTED auth token.
    [switch] $GameLift,
    [string] $ComputeName = 'bagman-dev-EREMOSPECIALTY',
    [string] $FleetId     = 'fleet-40c6b342-c03b-44cd-a9d3-56fe65156a7a',

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
# way to pick up these values, so refuse to add a second instance silently. (Server runs are short-lived
# and intentionally repeatable, so this guard is editor-only.)
$running = if ($Server) { $null } else { Get-Process -Name 'UnrealEditor' -ErrorAction SilentlyContinue }
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
if ($DryRun -and -not $Server) {
    Write-Host ''
    Write-Host '  -DryRun: not launching. Re-run without -DryRun to boot the editor.' -ForegroundColor Yellow
    exit 0
}

if ($Server) {
    # Prefer the COOKED/STAGED server. Running the uncooked project server crashes at boot after ~8 GB with
    # an assertion in FGenerationInfo::Serialize via PackageFileSummary -- the loader hits package headers in
    # a layout it does not expect. Same family as the recorded "uncooked -game cannot resolve PrimaryAssetIds"
    # finding. A staged build carries its own cooked content, so it is NOT passed the .uproject.
    $cookedExe  = 'D:\BagMan\Archive\WindowsServer\LyraServer.exe'
    $projectExe = 'C:\Dev\Bag_Man\Binaries\Win64\LyraServer.exe'
    if (Test-Path $cookedExe) {
        $serverExe = $cookedExe
        $useCooked = $true
    }
    elseif (Test-Path $projectExe) {
        Write-Warning 'Only the UNCOOKED project server exists. It is expected to crash at boot; run a server cook:'
        Write-Warning '  RunUAT BuildCookRun ... -server -serverconfig=Development -noclient -cook -stage -pak -archive'
        $serverExe = $projectExe
        $useCooked = $false
    }
    else {
        throw "No LyraServer.exe. It can ONLY be built from the D: source engine -- the C: launcher answers 'Server targets are not currently supported from this engine distribution'."
    }
    if (-not (Test-Path $MatchmakerDataFile)) {
        throw "No captured payload at $MatchmakerDataFile. Run Tools\Invoke-Allocator.ps1 first -- the payload must be PRODUCED BY THE ALLOCATOR, not hand-written."
    }
    # Read as raw bytes -> exact string. This is the allocator's verbatim output and must not be reformatted.
    $mm = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($MatchmakerDataFile)).Trim()

    # UE URL options separate with '?', NOT '&'. The JSON below contains no '?', so it survives ParseOption
    # intact -- but it does contain quotes, so the whole URL is passed as ONE argument.
    # In GameLift mode the launch option is DELIBERATELY OMITTED. If both sources were present a passing test
    # would be ambiguous -- the provider prefers GameLift, but you could not prove from the outside that the
    # roster had not simply come from the command line. Leaving it out makes onStartGameSession the ONLY
    # possible source, so a reconciled roster is proof the hop worked.
    $url = if ($GameLift) { "$Map`?Experience=$Experience`?NumBots=$NumBots" }
           else           { "$Map`?Experience=$Experience`?NumBots=$NumBots`?MatchmakerData=$mm" }

    Write-Host ''
    Write-Host 'Server launch:' -ForegroundColor Cyan
    Write-Host "  exe        : $serverExe"
    Write-Host "  map        : $Map"
    Write-Host "  experience : $Experience"
    Write-Host "  matchmaker : $mm"
    Write-Host ''
    Write-Host '  Watch the log for, in order:' -ForegroundColor Cyan
    Write-Host '    [AFLOnline] Server signer (dedicated server): key=held ...'
    Write-Host '    AFL_MATCHREPORT: economics from MATCHMAKER -- tier=VoltsPlay ... stake=10 VO'
    Write-Host '    AFL_MATCHREPORT: ... escrow request(s) posted'
    Write-Host ''

    if ($GameLift) {
        # ⚠ THE AUTH TOKEN IS SHORT-LIVED -- measured at 180 minutes. It must NEVER be written into a runbook,
        # an .env file, or a stored script: a pasted token works right up until it silently does not, and the
        # failure surfaces as InitSDK refusing to connect. Mint a fresh one on every launch instead. Same
        # discipline as the earn HMAC key: fetched at launch, held in the process, never persisted, never
        # echoed -- reported only as held/MISSING.
        Write-Host ''
        Write-Host 'GameLift compute:' -ForegroundColor Cyan

        $comp = aws gamelift describe-compute --fleet-id $FleetId --compute-name $ComputeName --output json 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Compute '$ComputeName' not registered on $FleetId. Register it first -- see Docs/RUNBOOK_GAMELIFT_COMPUTE.md."
        }
        $c = ($comp | ConvertFrom-Json).Compute
        if ($c.ComputeStatus -ne 'Active') { throw "Compute is '$($c.ComputeStatus)', not Active." }

        $tok = aws gamelift get-compute-auth-token --fleet-id $FleetId --compute-name $ComputeName --output json 2>&1
        if ($LASTEXITCODE -ne 0) { throw "get-compute-auth-token failed: $tok" }
        $t = $tok | ConvertFrom-Json

        $env:AFL_GAMELIFT_WEBSOCKET_URL = $c.GameLiftServiceSdkEndpoint
        $env:AFL_GAMELIFT_FLEET_ID      = $c.FleetId
        $env:AFL_GAMELIFT_HOST_ID       = $c.ComputeName
        $env:AFL_GAMELIFT_PROCESS_ID    = "bagman-$PID"
        $env:AFL_GAMELIFT_AUTH_TOKEN    = $t.AuthToken

        $mins = [math]::Round(([DateTime]::Parse($t.ExpirationTimestamp).ToUniversalTime() - [DateTime]::UtcNow).TotalMinutes, 0)
        Write-Host ("  compute   : {0}  ({1} @ {2})" -f $c.ComputeName, $c.ComputeStatus, $c.IpAddress)
        Write-Host ("  location  : {0}" -f $c.Location)
        Write-Host ("  websocket : {0}" -f $c.GameLiftServiceSdkEndpoint)
        Write-Host ("  authToken : held, expires in {0} min" -f $mins) -ForegroundColor Green
        Write-Host '  the roster will now come from onStartGameSession, NOT ?MatchmakerData=' -ForegroundColor Yellow
    }
    else {
        # Explicitly clear, so a previous -GameLift run in the same shell cannot leak stale credentials into
        # what is supposed to be a local launch-option run.
        foreach ($v in 'AFL_GAMELIFT_WEBSOCKET_URL','AFL_GAMELIFT_FLEET_ID','AFL_GAMELIFT_HOST_ID','AFL_GAMELIFT_PROCESS_ID','AFL_GAMELIFT_AUTH_TOKEN') {
            Remove-Item "env:$v" -ErrorAction SilentlyContinue
        }
    }

    if ($DryRun) { Write-Host '  -DryRun: not launching.' -ForegroundColor Yellow; exit 0 }

    # ⚠ Start-Process -ArgumentList EATS the embedded double quotes, and the JSON arrives as
    # {matchId:...} instead of {"matchId":...} -- malformed, and silently so. Verified by reading the
    # server's own "LogInit: Command Line:" echo. Build ONE command line and escape the inner quotes as \"
    # per the Windows C runtime convention, then launch via ProcessStartInfo so nothing re-parses it.
    $escaped = $url -replace '"', '\"'
    # A cooked/staged server must NOT be handed the .uproject -- it loads its own pak content.
    $dp = '-dpcvars="afl.Match.WarmupDuration={0}"' -f $WarmupSeconds
    $cmdline = if ($useCooked) { '"{0}" -log -port={1} {2}' -f $escaped, $Port, $dp }
               else            { '"{0}" "{1}" -log -port={2} {3}' -f $ProjectPath, $escaped, $Port, $dp }
    if ($EditorArgs) { $cmdline += ' ' + ($EditorArgs -join ' ') }

    $psi = New-Object Diagnostics.ProcessStartInfo
    $psi.FileName        = $serverExe
    $psi.Arguments       = $cmdline
    $psi.UseShellExecute = $true
    $proc = [Diagnostics.Process]::Start($psi)

    Write-Host "  launched pid=$($proc.Id) on port $Port" -ForegroundColor Green
    Write-Host '  VERIFY the quotes survived: grep the log for "LogInit: Command Line:" and confirm it shows' -ForegroundColor Yellow
    Write-Host '  {"matchId":... with quotes intact, not {matchId:...' -ForegroundColor Yellow
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
