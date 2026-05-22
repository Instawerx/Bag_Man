# kill_ubt_zombies.ps1 — recovery script for the UBT Session 0 zombie pattern.
#
# When the orchestrator's pre_flight_check_ubt_mutex halts a task with
# `stage: pre_flight_ubt_mutex` and a "Suspect Session 0 dotnets" reason,
# run THIS script from an ELEVATED (Administrator) PowerShell window.
#
# Why this is needed:
#   Claude Code's in-session `Build.bat` invocations occasionally orphan
#   a `dotnet.exe` process into Session 0 (the SYSTEM session). That
#   zombie holds `Global\UnrealBuildTool_Mutex_<hash>` for the lifetime
#   of the dotnet process. User-session `Stop-Process` returns silently
#   but does not actually kill Session 0 processes; only an Administrator
#   process can.
#
# How to launch elevated:
#   * Win + X → "Terminal (Admin)" or "Windows PowerShell (Admin)"
#   * OR right-click PowerShell in Start menu → "Run as administrator"
#
# Then `cd` into the repo and run this script:
#   cd C:\Dev\Bag_Man
#   .\Tools\AFL_Yolo\kill_ubt_zombies.ps1
#
# Use `-WhatIf` to dry-run (lists candidates without killing):
#   .\Tools\AFL_Yolo\kill_ubt_zombies.ps1 -WhatIf

[CmdletBinding(SupportsShouldProcess = $true)]
param ()

# --- Admin guard -------------------------------------------------------------
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)
if (-not $isAdmin) {
    Write-Host ""
    Write-Host "  This script must run from an ELEVATED PowerShell." -ForegroundColor Yellow
    Write-Host "  Session 0 dotnet processes are unkillable from a user-perm shell." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "  Relaunch:  Win + X → Terminal (Admin)  →  cd $PWD  →  re-run this script"
    Write-Host ""
    exit 2
}

# --- Find candidates --------------------------------------------------------
# A "candidate" is any dotnet.exe in SessionId 0 (the SYSTEM session) with
# no visible command line (the WMI signature of an orphaned UBT child from
# a Claude Code in-session Build.bat). Restricting to Session 0 ensures we
# don't touch legitimate user-session dotnet processes (VS Code, Azure CLI,
# .NET-based developer tools, etc.).

$candidates = Get-CimInstance Win32_Process -Filter "Name='dotnet.exe'" |
    Where-Object { $_.SessionId -eq 0 }

if (-not $candidates) {
    Write-Host "No Session 0 dotnet zombies found. UBT mutex should be free." -ForegroundColor Green
    exit 0
}

Write-Host ""
Write-Host "Candidate zombie dotnet.exe processes in Session 0:" -ForegroundColor Cyan
$candidates | ForEach-Object {
    $proc = Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue
    $wsMb = if ($proc) { [math]::Round($proc.WS / 1MB, 1) } else { '?' }
    $cpu  = if ($proc) { [math]::Round($proc.CPU, 1) } else { '?' }
    Write-Host ("  PID {0,-6}  WS={1}MB  CPU={2}s  Created={3}" -f $_.ProcessId, $wsMb, $cpu, $_.CreationDate)
}
Write-Host ""

# --- Kill ----------------------------------------------------------------
$killed = 0
$failed = 0
foreach ($c in $candidates) {
    if ($PSCmdlet.ShouldProcess("dotnet.exe PID $($c.ProcessId) (Session 0)", "Stop-Process -Force")) {
        try {
            Stop-Process -Id $c.ProcessId -Force -ErrorAction Stop
            Start-Sleep -Milliseconds 500
            $stillAlive = Get-Process -Id $c.ProcessId -ErrorAction SilentlyContinue
            if ($stillAlive) {
                Write-Host ("  FAILED to kill PID {0} (still alive after Stop-Process -Force; reboot may be required)" -f $c.ProcessId) -ForegroundColor Red
                $failed++
            } else {
                Write-Host ("  Killed PID {0}" -f $c.ProcessId) -ForegroundColor Green
                $killed++
            }
        } catch {
            Write-Host ("  ERROR killing PID {0}: {1}" -f $c.ProcessId, $_.Exception.Message) -ForegroundColor Red
            $failed++
        }
    }
}

Write-Host ""
Write-Host ("Result: killed={0}, failed={1}" -f $killed, $failed)
if ($failed -gt 0) {
    Write-Host ""
    Write-Host "Some processes could not be killed even from admin. Reboot is the reliable next step." -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "UBT mutex should now be free. You can re-launch the orchestrator task." -ForegroundColor Green
exit 0
