<#
.SYNOPSIS
  Release the escrow on an ABANDONED match by settling it as 'cancelled-refund'.

.DESCRIPTION
  A match that never reached a terminal state leaves its stakes sitting in bagman-match-escrow with
  Status 'escrowed'. The currency has already left the players' wallets, so the position is not neutral:
  doing nothing is a silent confiscation.

  This posts the terminal state the backend already defines for exactly this case. From
  lambda\settle-match\index.ts computePayouts():

      'cancelled-refund' -> exact entries back, NO rake, because "a cancelled match must be
      economically invisible; taking a rake on a match that did not happen is the house charging
      for its own failure."

  ⚠ WHY NOT JUST POST A WINNER. Settling 'settled' with an invented result would pay someone the pot
  minus rake for a match they did not play and write a false row into the settlement ledger, which is
  the one table the whole economy is audited from. A refund is the only honest close for a match with
  no outcome.

  ⚠ THIS IS ONE-SHOT AND IRREVERSIBLE. settle-match CLAIMS the matchId with a conditional write before
  any currency moves, so a match can be settled exactly once, ever. A replay returns the recorded
  outcome and moves nothing. That is the anti-double-pay guarantee, and it also means a WRONG terminal
  state cannot be corrected by calling again. Check the matchId before running.

  THE SIGNING KEY IS NEVER WRITTEN TO DISK, LOGGED, OR ECHOED. Neither is the signature.
  Note this endpoint uses the EARN/money secret, the same trust domain as escrow -- NOT the tentpole key
  that /allocate uses.

.EXAMPLE
  .\Tools\Refund-AbandonedMatch.ps1 -MatchId C4AC8AF9-406F-A55E-3887-9C934380525A `
      -PlayFabId DF7C3188377BB66D,40419031BCC83EA5 -EntryAmount 10
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'High')]
param(
    [Parameter(Mandatory = $true)]
    [string] $MatchId,

    # Every player who was escrowed for this match. Each is given a finishingPosition, dense from 1,
    # because validateRequest requires dense positions -- but under 'cancelled-refund' the positions are
    # never scored against the payout curve, each entry simply gets its own entryAmount back.
    [Parameter(Mandatory = $true)]
    [string[]] $PlayFabId,

    # Must match what escrow actually took. The refund pays back exactly this, so a wrong value here
    # either short-changes a player or mints currency.
    [Parameter(Mandatory = $true)]
    [int] $EntryAmount,

    [ValidateSet('VO', 'WA')]
    [string] $Currency = 'VO',

    [string] $Stack    = 'BagManTentpoleStack',
    [string] $SecretId = 'bagman/earn/hmac'
)

$ErrorActionPreference = 'Stop'

if (-not (Get-Command aws -ErrorAction SilentlyContinue)) {
    throw 'AWS CLI not on PATH; required to resolve the endpoint and the signing key.'
}

Write-Host ''
Write-Host 'Abandoned-match refund' -ForegroundColor Cyan
Write-Host '----------------------'
Write-Host "  matchId  : $MatchId"
Write-Host "  refunding: $($PlayFabId.Count) player(s), $EntryAmount $Currency each (no rake)"

# Endpoint from the LIVE stack -- cdk-outputs.json drifts (it is only rewritten by --outputs-file).
$raw = aws cloudformation describe-stacks --stack-name $Stack --query 'Stacks[0].Outputs' --output json
if ($LASTEXITCODE -ne 0) { throw 'describe-stacks failed; check AWS credentials.' }
$url = (($raw | ConvertFrom-Json) | Where-Object { $_.OutputKey -eq 'SettleMatchInvokeUrl' }).OutputValue
if (-not $url) { throw "Stack output 'SettleMatchInvokeUrl' not found." }
Write-Host "  endpoint : $url"

# Built by concatenation, NOT ConvertTo-Json: the signature covers the EXACT bytes sent, so the body
# string must be the one transmitted. A serializer is free to reorder keys or emit 10.0 for an integer
# entryAmount, either of which changes the bytes and breaks the signature.
$position = 0
$entries = foreach ($id in $PlayFabId) {
    $position++
    '{"playFabId":"' + $id + '","finishingPosition":' + $position + ',"entryAmount":' + $EntryAmount + '}'
}
$body = '{"matchId":"' + $MatchId + '","currencyCode":"' + $Currency +
        '","terminalState":"cancelled-refund","entries":[' + ($entries -join ',') + ']}'

Write-Host ''
Write-Host '  body to be signed:' -ForegroundColor DarkGray
Write-Host "  $body" -ForegroundColor DarkGray

if (-not $PSCmdlet.ShouldProcess($MatchId, "settle as cancelled-refund (IRREVERSIBLE -- a match settles once)")) {
    Write-Host '  aborted; nothing signed, nothing posted.' -ForegroundColor Yellow
    return
}

$secret = aws secretsmanager get-secret-value --secret-id $SecretId --query SecretString --output text
if ($LASTEXITCODE -ne 0) { throw "Could not read secret '$SecretId'; check IAM permissions." }
if ($secret -is [array]) { throw 'Secret is multi-line; refusing to guess how to join it.' }

$hmac = New-Object System.Security.Cryptography.HMACSHA256
$hmac.Key = [Text.Encoding]::UTF8.GetBytes($secret)
Remove-Variable secret

$sig = -join ($hmac.ComputeHash([Text.Encoding]::UTF8.GetBytes($body)) | ForEach-Object { $_.ToString('x2') })

try {
    $resp = Invoke-RestMethod -Uri $url -Method Post -Body $body -ContentType 'application/json' `
                              -Headers @{ 'X-Signature' = $sig }
    Write-Host ''
    Write-Host '  RESPONSE' -ForegroundColor Green
    $resp | ConvertTo-Json -Depth 6 | Write-Host

    # 'replayed' means the match was ALREADY claimed, so this call moved nothing. Surfaced loudly because
    # it is the difference between "the refund happened just now" and "something settled this earlier".
    if ($resp.replayed) {
        Write-Host ''
        Write-Host '  ⚠ REPLAYED -- this match was already settled; NO currency moved on this call.' -ForegroundColor Yellow
        Write-Host ("    recorded terminalState: {0}" -f $resp.terminalState) -ForegroundColor Yellow
    }
}
catch {
    $detail = ''
    if ($_.ErrorDetails) { $detail = $_.ErrorDetails.Message }
    elseif ($_.Exception.Response) {
        $sr = New-Object IO.StreamReader($_.Exception.Response.GetResponseStream())
        $detail = $sr.ReadToEnd()
    }
    Write-Host ''
    Write-Host "  FAILED: $($_.Exception.Message)" -ForegroundColor Red
    if ($detail) { Write-Host "  detail: $detail" -ForegroundColor Red }
    throw
}
