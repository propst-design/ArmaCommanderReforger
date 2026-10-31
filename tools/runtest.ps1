# Antigravity's helper (runs on Windows, PowerShell 5+): the same as runtest.sh + verdict.sh.
#   powershell -File tools/runtest.ps1 <unique id> <preset> [max polls of 5 s]
#   powershell -File tools/runtest.ps1 -VerdictOnly <id>
# Drops a request for the test launcher (tools/ACR-Watcher.ps1), waits for the result,
# prints its header plus script errors, then the verdict (PASS only with status=completed
# AND "[ACR] VERDICT PASS"). Use a new id every time (old results stay). Exit 0 = PASS.
param(
    [Parameter(Mandatory = $true, Position = 0)][string]$Id,
    [Parameter(Position = 1)][string]$Preset,
    [Parameter(Position = 2)][int]$Polls = 60,
    [switch]$VerdictOnly
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$req = Join-Path $root '.acr-requests'
$result = Join-Path $req "$Id.result.txt"
$buildFile = Join-Path $req "$Id.build.txt"

if (-not $VerdictOnly) {
    if (-not $Preset) { throw 'Preset required (e.g. srv:compile)' }
    if (Test-Path -LiteralPath $result) { throw "Result for '$Id' already exists: use a new id" }
    if (Test-Path -LiteralPath (Join-Path $req "$Id.request")) { throw "Request '$Id' is already queued: use a new id" }
    # The commit, +dirty for uncommitted game files, and the sibling BBB checkout srv: runs load.
    $sha = (& git -C $root log -1 --format=%h).Trim()
    & git -C $root diff --quiet HEAD -- Scripts Prefabs Configs Worlds Missions
    if ($LASTEXITCODE -ne 0) { $sha += '+dirty' }
    $bbbRoot = Join-Path (Split-Path -Parent $root) 'BetterBattleBuddies'
    if (Test-Path -LiteralPath (Join-Path $bbbRoot '.git')) {
        $bbb = (& git -C $bbbRoot log -1 --format=%h).Trim()
        & git -C $bbbRoot diff --quiet HEAD -- Scripts
        if ($LASTEXITCODE -ne 0) { $bbb += '+dirty' }
        $sha += " bbb $bbb"
    }
    [IO.File]::WriteAllText($buildFile, $sha)
    [IO.File]::WriteAllText((Join-Path $req "$Id.request"), "$Preset`n")
    for ($i = 0; $i -lt $Polls; $i++) {
        Start-Sleep -Seconds 5
        if (Test-Path -LiteralPath $result) { break }
    }
    if (-not (Test-Path -LiteralPath $result)) {
        Write-Output "No result for $Id after $($Polls * 5) s (request still queued or running)"
        exit 2
    }
    Start-Sleep -Milliseconds 500
    $lines = Get-Content -LiteralPath $result -Encoding UTF8
    $lines | Select-Object -First 6
    $noise = "SCR_SpawnLogic|SCR_MapUIElementContainer|SCR_PlayerArsenalLoadout|ScenarioFramework|SpinningWidget|SCR_ScenarioUICommon|Can't compile"
    $lines | Where-Object { $_ -match '\(E\)' -and $_ -notmatch $noise } | Select-Object -First 20
    $compile = $lines | Where-Object { $_ -match "Can't compile" } | Select-Object -First 10
    if ($compile) { Write-Output '--- compile errors:'; $compile }
    if ($Preset -eq 'publish' -or $Preset -eq 'push') { exit 0 }
}

if (-not (Test-Path -LiteralPath $result)) { Write-Output "VERDICT MISSING: no result for $Id"; exit 2 }
$lines = Get-Content -LiteralPath $result -Encoding UTF8
$status = (($lines | Where-Object { $_ -match '^status=' } | Select-Object -First 1) -replace '^status=', '').Trim()
$presetLine = $lines | Where-Object { $_ -match '^preset=' } | Select-Object -First 1
$build = 'unknown build'
if (Test-Path -LiteralPath $buildFile) { $build = (Get-Content -LiteralPath $buildFile -Raw).Trim() }
$vline = $lines | Where-Object { $_ -match '\[ACR\] VERDICT ' } | Select-Object -First 1
$fails = @($lines | Where-Object { $_ -match 'REGRESS FAIL' }).Count
$passes = @($lines | Where-Object { $_ -match 'REGRESS PASS' }).Count
if ($status -ne 'completed') { $v = "FAIL (launcher: $status)" }
elseif (-not $vline) { $v = 'NO VERDICT (legacy test: TEST COMPLETE only)' }
else { $v = ($vline -replace '.*\[ACR\] VERDICT ', '').Trim() }
Write-Output "RUN $Id | $presetLine | build $build | REGRESS lines: $passes pass, $fails fail"
Write-Output "VERDICT $v"
$lines | Where-Object { $_ -match 'REGRESS FAIL|REGRESS SKIP' } | Select-Object -First 20
if ($v -like 'PASS *') { exit 0 } else { exit 1 }
