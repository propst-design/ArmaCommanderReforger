# Isolated startup only. This does not certify gameplay or multiplayer acceptance.
[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][ValidatePattern('^(ACA|BBB|AIO|MAP-[A-Z]+|PACK-[A-Z]+)$')][string]$Mod,
 [Parameter(Mandatory=$true)][string]$AddonRoot,
 [Parameter(Mandatory=$true)][string]$ReportDirectory,
 [string]$DependencyRoot,
 [string]$ServerPath,
 # ACA map add-ons: loaded with the downloaded Workshop ACA/BBB and the map from InstalledRoot.
 [string]$AddonId,
 [string]$World,
 [string]$InstalledRoot,
 # Log line that proves the add-on started (maps: a generated base; content packs: plugged in).
 [string]$Signal,
 # Extra server options for the check, e.g. '-acrSetup 0' (plain options only).
 [ValidatePattern('^[-A-Za-z0-9 ]*$')][string]$ServerArgs
)
$ErrorActionPreference = 'Stop'
if (Get-Process ArmaReforgerServer -ErrorAction SilentlyContinue) { throw 'A server is running. Wait for its test/session to finish before preparing a release.' }
if (!$ServerPath) {
 $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
 $libraries = @($steam, 'E:\SteamLibrary', 'C:\Program Files (x86)\Steam')
 if ($steam -and (Test-Path -LiteralPath (Join-Path $steam 'steamapps/libraryfolders.vdf'))) {
  foreach ($match in [regex]::Matches((Get-Content -LiteralPath (Join-Path $steam 'steamapps/libraryfolders.vdf') -Raw), '"path"\s+"([^"]+)"')) { $libraries += $match.Groups[1].Value.Replace('\\', '\') }
 }
 foreach ($library in $libraries) {
  if (!$library) { continue }
  $candidate = Join-Path $library 'steamapps/common/Arma Reforger Server/ArmaReforgerServer.exe'
  if (Test-Path -LiteralPath $candidate) { $ServerPath = $candidate; break }
 }
}
if (!$ServerPath -or !(Test-Path -LiteralPath $ServerPath)) { throw 'Arma Reforger Server was not found. Install the server through Steam.' }
$ids = @{ ACA='AC230926A0010001'; BBB='BBB0261015000001'; AIO='629405AEFC950C12' }
$signals = @{ ACA='\[ACR\] Squad registered'; BBB='\[BBB\] started on its own'; AIO='\[AIO\] ready' }
# Not $world: PowerShell names ignore case, so that would overwrite the -World parameter
# (6 Nov: every add-on check started GM_Arland and timed out).
$serverWorld = '{77C41EA429B6AC35}worlds/GameMaster/GM_Arland.ent'
$dirs = Split-Path $AddonRoot -Parent
if ($Mod -match '^(MAP|PACK)-') {
 if (!$AddonId -or !$World -or !$InstalledRoot) { throw 'ACA add-on startup requires -AddonId, -World and -InstalledRoot.' }
 $ids[$Mod] = $AddonId
 $signals[$Mod] = '\[ACR\] GEN BASE'
 if ($Signal) { $signals[$Mod] = $Signal }
 $serverWorld = $World
 $dirs += ',' + $InstalledRoot
}
if ($Mod -eq 'ACA') {
 if (!$DependencyRoot) { throw 'ACA startup requires the staged BBB dependency.' }
 $dirs += ',' + (Split-Path $DependencyRoot -Parent)
 $serverWorld = 'Worlds/ACR_TestRange.ent'
}
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
$profile = Join-Path $ReportDirectory 'profile'
$arguments = '-server "' + $serverWorld + '" -addonsDir "' + $dirs + '" -addons ' + $ids[$Mod] + ' -profile "' + $profile + '" -maxFPS 60 -noBackend'
if ($ServerArgs) { $arguments += ' ' + $ServerArgs }
Write-Host "Checking $Mod in isolation (usually under one minute)..."
$process = Start-Process -FilePath $ServerPath -ArgumentList $arguments -WorkingDirectory (Split-Path $ServerPath -Parent) -WindowStyle Hidden -PassThru
$started = Get-Date
$passed = $false
$body = ''
# The exact projects the engine must name as loaded. ACA add-ons: also the published ACA/BBB
# copies passed first in -InstalledRoot (a downloaded copy with the same ID must not win).
$required = @($AddonRoot, $DependencyRoot)
if ($Mod -match '^(MAP|PACK)-') {
 foreach ($dir in $InstalledRoot.Split(',')) {
  foreach ($name in @('ArmaCommanderReforger', 'BetterBattleBuddies')) {
   if (Test-Path -LiteralPath (Join-Path $dir "$name\addon.gproj")) { $required += (Join-Path $dir $name) }
  }
 }
}
# Community terrains place editable objects without an RplComponent; the engine logs a script
# error for each as the world loads and carries on (same filter as ACR-Watcher.ps1).
$terrainNoise = '(Create entity @"ENTITY:\d+" \(''[^'']*'', GenericEntity\)[^\r\n]*\r?\n)[^\r\n]*SCRIPT\s+\(E\): Editable entity @"" is missing RplComponent![^\r\n]*'
function Read-Log($path) { return [regex]::Replace((Get-Content -LiteralPath $path -Raw), $terrainNoise, '$1terrain editable object without RplComponent (ignored)') }
try {
 while (((Get-Date) - $started).TotalSeconds -lt 90) {
  Start-Sleep -Seconds 2
  $log = Get-ChildItem -LiteralPath $profile -Filter console.log -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($log) {
   $body = Read-Log $log.FullName
   if ($body -match "SCRIPT\s+\(E\)|Can't compile|Unable to start replication") { throw "Startup failed. See $($log.FullName)" }
   if ($body -match $signals[$Mod]) {
    # Require the engine to name the exact staged project(s), not merely the requested CLI paths.
    foreach ($expected in $required) {
     if (!$expected) { continue }
     $normalized = (Join-Path $expected 'addon.gproj').Replace('\', '/')
     $loaded = "gproj: '" + $normalized + "'"
     if (!$body.Replace('\', '/').Contains($loaded)) { throw "Engine did not confirm loading $normalized" }
    }
    Start-Sleep -Seconds 3
    $body = Read-Log $log.FullName
    if ($body -match "SCRIPT\s+\(E\)|Can't compile|Unable to start replication") { throw 'Script errors appeared after startup.' }
    $passed = $true
    break
   }
  }
  if ($process.HasExited) { throw 'Server exited before successful startup.' }
 }
 if (!$passed) { throw 'Startup timed out. Candidate remains blocked.' }
} finally {
 if (!$process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue }
 # Stop-Process returns before the server has gone; the next check (ACA after BBB) refuses while it lingers.
 $process.WaitForExit(30000) | Out-Null
 @("mod=$Mod", "passed=$passed", "arguments=$arguments", $body) | Set-Content -LiteralPath (Join-Path $ReportDirectory 'result.txt') -Encoding UTF8
}
Write-Host "$Mod isolated startup passed. Gameplay/MP acceptance remains separate."
