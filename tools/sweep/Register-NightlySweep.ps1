# Registers (or removes) the nightly test sweep as a Windows scheduled task for the current user.
#   powershell -File tools\sweep\Register-NightlySweep.ps1            # every night at 01:07, stops starting tests at 08:30
#   powershell -File tools\sweep\Register-NightlySweep.ps1 -At 02:13 -Until 09:00
#   powershell -File tools\sweep\Register-NightlySweep.ps1 -Remove
# Jacob enables it; agents don't. Runs hidden, only when the PC is on (no wake), never the game client.
# It sweeps a detached worktree of each repo's main in _wt\sweep, so run this from the main ACA checkout.
param([string]$At = '01:07', [string]$Until = '08:30', [switch]$Remove)
$ErrorActionPreference = 'Stop'
$name = 'Reforger mods nightly test sweep'
if ($Remove) { Unregister-ScheduledTask -TaskName $name -Confirm:$false; Write-Host "Removed '$name'."; return }
$python = (Get-Command python -ErrorAction Stop).Source
$script = Join-Path $PSScriptRoot 'sweep.py'
$root = Split-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) -Parent
$log = Join-Path $root '_sweep\sweep-console.log'
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
$cmd = "/c `"`"$python`" `"$script`" ingest && `"$python`" `"$script`" run --until $Until >> `"$log`" 2>&1`""
$action = New-ScheduledTaskAction -Execute 'cmd.exe' -Argument $cmd -WorkingDirectory (Split-Path $PSScriptRoot -Parent)
$trigger = New-ScheduledTaskTrigger -Daily -At $At
$settings = New-ScheduledTaskSettingsSet -StartWhenAvailable:$false -DontStopOnIdleEnd -ExecutionTimeLimit (New-TimeSpan -Hours 10) -MultipleInstances IgnoreNew
Register-ScheduledTask -TaskName $name -Action $action -Trigger $trigger -Settings $settings -Description 'ACA tools/sweep: headless tests of every mod on main, report in _sweep\reports\latest.md' -Force | Out-Null
Write-Host "Registered '$name': daily at $At, no new tests after $Until. Report: $root\_sweep\reports\latest.md"
