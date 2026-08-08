<#
.SYNOPSIS
  Grant dev currency to one or more PlayFab accounts via the deployed /earn Lambda.

.DESCRIPTION
  A staked match escrows real currency per player: escrow-entry subtracts the stake from each
  participant's PlayFab balance, and an unfunded player 4xxs, at which point settlement correctly
  refuses the whole match. So both dev identities must hold currency BEFORE a staked PIE run.

  The in-editor canary (afl.Online.EarnCanary) can do this, but it needs a live game world, so it only
  runs during PIE -- and this project's standing rule is ZERO tooling calls into a running PIE. It also
  hardcodes currencyCode "WA" and amount 5. This script talks to the same endpoint directly instead, so
  it needs no editor at all and can grant either currency.

  Contract (lambda/currency-earn/index.ts):
    X-Signature : hex(HMAC-SHA256(rawBody, earn secret))  -- constant-time compared server-side
    ts          : unix seconds, must be within +/-300s of server-now
    nonce       : fresh per request; the dedupe table makes a replay a no-grant 200
    currency    : WA (Watts) or VO (Volts)

  THE SIGNING KEY IS NEVER WRITTEN TO DISK, LOGGED, OR ECHOED -- it is pulled from Secrets Manager into
  memory, used to sign, and discarded. Signatures are not printed either.

.EXAMPLE
  .\Tools\Fund-DevAccount.ps1 -PlayFabId DF7C3188377BB66D,40419031BCC83EA5 -Currency VO -Amount 100
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string[]] $PlayFabId,

    [ValidateSet('WA', 'VO')]
    [string] $Currency = 'VO',

    [ValidateRange(1, 1000000)]
    [int] $Amount = 100,

    [string] $Reason   = 'devfund',
    [string] $Stack    = 'BagManTentpoleStack',
    [string] $SecretId = 'bagman/earn/hmac'
)

$ErrorActionPreference = 'Stop'

if (-not (Get-Command aws -ErrorAction SilentlyContinue)) {
    throw 'AWS CLI not on PATH; required to resolve the endpoint and the signing key.'
}

Write-Host ''
Write-Host "Funding $($PlayFabId.Count) account(s): $Amount $Currency each" -ForegroundColor Cyan

# Endpoint from the LIVE stack -- cdk-outputs.json drifts (it is only rewritten by --outputs-file).
$raw = aws cloudformation describe-stacks --stack-name $Stack --query 'Stacks[0].Outputs' --output json
if ($LASTEXITCODE -ne 0) { throw 'describe-stacks failed; check AWS credentials.' }
$url = (($raw | ConvertFrom-Json) | Where-Object { $_.OutputKey -eq 'EarnInvokeUrl' }).OutputValue
if (-not $url) { throw "Stack output 'EarnInvokeUrl' not found." }
Write-Host "  endpoint: $url"

$secret = aws secretsmanager get-secret-value --secret-id $SecretId --query SecretString --output text
if ($LASTEXITCODE -ne 0) { throw "Could not read secret '$SecretId'; check IAM permissions." }
if ($secret -is [array]) { throw 'Secret is multi-line; refusing to guess how to join it.' }

$hmac = New-Object System.Security.Cryptography.HMACSHA256
$hmac.Key = [Text.Encoding]::UTF8.GetBytes($secret)
Remove-Variable secret

$fail = 0
foreach ($id in $PlayFabId) {
    $ts      = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $matchId = [guid]::NewGuid().ToString()
    $nonce   = [guid]::NewGuid().ToString()

    # Built by concatenation, NOT ConvertTo-Json: the signature covers the EXACT bytes sent, so the body
    # string must be the one transmitted. A serializer is free to reorder keys or emit 100.0 for an
    # integer amount, either of which changes the bytes and breaks the signature.
    $body = '{"playFabId":"' + $id + '","currencyCode":"' + $Currency + '","amount":' + $Amount +
            ',"reason":"' + $Reason + '","matchId":"' + $matchId + '","nonce":"' + $nonce + '","ts":' + $ts + '}'

    $sig = -join ($hmac.ComputeHash([Text.Encoding]::UTF8.GetBytes($body)) | ForEach-Object { $_.ToString('x2') })

    try {
        $resp = Invoke-RestMethod -Uri $url -Method Post -Body $body -ContentType 'application/json' `
                                  -Headers @{ 'X-Signature' = $sig }
        Write-Host ("  {0}  OK  newBalance={1}" -f $id, $resp.newBalance) -ForegroundColor Green
    }
    catch {
        $fail++
        $status = $null
        if ($_.Exception.Response) { $status = [int]$_.Exception.Response.StatusCode }
        Write-Host ("  {0}  FAILED  http={1}  {2}" -f $id, $status, $_.Exception.Message) -ForegroundColor Red
    }
}

$hmac.Dispose()
Write-Host ''
if ($fail -gt 0) { throw "$fail of $($PlayFabId.Count) grant(s) failed." }
Write-Host 'All grants succeeded.' -ForegroundColor Green
