# AC: Actual (Arma Commander Actual) - multiplayer launcher for the Arland scenario.
#   Host: starts the Arma Reforger Server tool on this PC with the mod, waits for
#         the scenario to load, then joins it with the game.
#   Join: connects the game to a host's address.
#   Leave: leaves the ZeroTier network (see ACR-ZeroTier.ps1).
param([ValidateSet('Host', 'Join', 'Leave')][string]$Mode = 'Join', [string]$Address = '', [int]$Port = 2001, [switch]$Online)
$ErrorActionPreference = 'Stop'
# Everything this window shows also goes to launcher-log.txt (send it if something fails).
try { Start-Transcript -Path (Join-Path $PSScriptRoot 'launcher-log.txt') -Force | Out-Null } catch {}

. (Join-Path $PSScriptRoot 'ACR-Find.ps1')


. (Join-Path $PSScriptRoot 'ACR-Update.ps1')

. (Join-Path $PSScriptRoot 'ACR-ZeroTier.ps1')

. (Join-Path $PSScriptRoot 'ACR-HostSetup.ps1')

try {
  if ($Mode -eq 'Leave') {
    Disconnect-ZeroTier
    Read-Host 'Press Enter to close'
    exit 0
  }
  if (Update-FromGitHub) {
    # Run the freshly downloaded launcher instead of this old copy.
    $again = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $PSCommandPath, '-Mode', $Mode, '-Port', $Port)
    if ($Address) { $again += @('-Address', $Address) }
    if ($Online) { $again += '-Online' }
    & powershell.exe @again
    exit $LASTEXITCODE
  }
  $addonsPath = Split-Path $PSScriptRoot -Parent
  Assert-BBBDependency $addonsPath
  $mod = '-addonsDir "' + $addonsPath + '" -addons AC230926A0010001'
  $backend = ' -noBackend'
  if ($Online) { $backend = '' }
  $game = Find-SteamApp 'Arma Reforger' 'ArmaReforgerSteam.exe'
  if (!$game) { throw 'Arma Reforger was not found in your Steam libraries.' }

  if ($Mode -eq 'Host') {
    $server = Find-SteamApp 'Arma Reforger Server' 'ArmaReforgerServer.exe'
    if (!$server) { throw 'The "Arma Reforger Server" tool is not installed. In Steam: Library > (filter) Tools > Arma Reforger Server > Install. It is free. Then run this again.' }
    $serverProfile = Join-Path $PSScriptRoot '.local-server'
    $started = Get-Date
    $setup = Show-HostSetup
    $addonsDir = $addonsPath
    if ($setup.Factions -ne 'vanilla' -or $setup.ACE -eq 'yes') { $addonsDir += ',E:\ArmaReforgerMods\addons' } # faction mods and ACE both live there
    $withAio = Add-OptionalMods $addonsDir $setup.Addons $setup
    $mod = '-addonsDir "' + $withAio[0] + '" -addons ' + $withAio[1]
    $zt = Connect-ZeroTier
    $serverArgs = '-server "' + $setup.World + '" ' + $mod + ' -profile "' + $serverProfile + '" -bindPort ' + $Port + ' -maxFPS 60' + $backend + $setup.Args
    Write-Host 'Starting the server (a second window opens; leave it running)...' -ForegroundColor Cyan
    $proc = Start-Process -FilePath $server -ArgumentList $serverArgs -WorkingDirectory (Split-Path $server -Parent) -PassThru

    Write-Host ''
    # Public IP for friends coming over the internet (needs UDP port forwarding).
    $public = $null
    try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch {}
    foreach ($url in 'https://api.ipify.org', 'https://checkip.amazonaws.com') {
      try {
        $r = (Invoke-RestMethod -Uri $url -TimeoutSec 5 -UseBasicParsing).ToString().Trim()
        if ($r -match '^\d{1,3}(\.\d{1,3}){3}$') { $public = $r; break }
      } catch {}
    }
    Write-Host ('Tell your friends ONE of these addresses (port ' + $Port + ' UDP):') -ForegroundColor Yellow
    if ($zt) {
      Write-Host ('   ' + $zt + '   <- friends on ZeroTier (they also need your network ID, in zerotier.txt)') -ForegroundColor Yellow
    }
    if ($public) {
      Write-Host ('   ' + $public + '   <- friends over the internet without ZeroTier (needs UDP ' + $Port + ' forwarded on your router)') -ForegroundColor Yellow
    } else {
      Write-Host '   (Could not look up your public IP: search "what is my ip". Needs port forwarding.)'
    }
    Write-Host 'Other addresses of this PC (same house, other VPN apps):'
    Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
      Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' -and $_.IPAddress -ne $zt } |
      ForEach-Object { Write-Host ('   ' + $_.IPAddress + '   (' + $_.InterfaceAlias + ')') }
    Write-Host ''

    # Wait until the scenario has loaded on the server (it prints "[ACR] Scenario started").
    $logRoot = Join-Path $serverProfile 'logs'
    $ready = $false
    for ($i = 0; $i -lt 90 -and !$ready; $i++) {
      Start-Sleep -Seconds 2
      if ($proc.HasExited) { throw 'The server closed while loading. Its log is in ' + $logRoot }
      if (Test-Path -LiteralPath $logRoot) {
        $dir = Get-ChildItem -LiteralPath $logRoot -Directory | Where-Object { $_.CreationTime -ge $started.AddSeconds(-5) } | Sort-Object CreationTime -Descending | Select-Object -First 1
        if ($dir) {
          $log = Join-Path $dir.FullName 'console.log'
          if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern '\[ACR\] Scenario started' -Quiet)) { $ready = $true }
        }
      }
      Write-Host -NoNewline '.'
    }
    Write-Host ''
    if (!$ready) { Write-Host 'The server is taking long; joining anyway.' -ForegroundColor Yellow }
    Start-Sleep -Seconds 5
    $Address = '127.0.0.1'
  }
  else {
    $joinSetup = Show-JoinModSetup
    $addonsDir = $addonsPath
    if ($joinSetup.Factions -ne 'vanilla' -or $joinSetup.ACE -eq 'yes') { $addonsDir += ',E:\ArmaReforgerMods\addons' } # faction mods and ACE both live there
    $withAio = Add-OptionalMods $addonsDir $joinSetup.Addons $joinSetup
    $mod = '-addonsDir "' + $withAio[0] + '" -addons ' + $withAio[1]
    Write-Host ('Selected ' + $joinSetup.Factions + ' and ACE=' + $joinSetup.ACE + '. Match both selections to the host or the server will reject the connection.') -ForegroundColor Yellow
    $zt = Connect-ZeroTier
    # Ask until we get something that looks like an address (an IP like 26.12.34.56,
    # or a name with a dot), remembering the last one used.
    $lastFile = Join-Path $PSScriptRoot 'last-host.txt'
    $last = ''
    if (Test-Path -LiteralPath $lastFile) { $last = (Get-Content -LiteralPath $lastFile -Raw).Trim() }
    while ($true) {
      if (!$Address) {
        Write-Host ''
        Write-Host 'Type the HOST''S ADDRESS: the IP the host''s window shows, e.g. 10.147.17.5' -ForegroundColor Yellow
        Write-Host 'or 192.168.1.20 (same house). Not "y" -- the address.'
        $prompt = 'Host address'
        if ($last) { $prompt += ' [Enter = ' + $last + ']' }
        $Address = Read-Host $prompt
        if (!$Address.Trim() -and $last) { $Address = $last }
      }
      $Address = $Address.Trim()
      if ($Address -match '^(\d{1,3}(\.\d{1,3}){3}|localhost|[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)+)(:\d+)?$') { break }
      Write-Host ('"' + $Address + '" is not an address. It should look like 10.147.17.5') -ForegroundColor Red
      $Address = ''
    }
    Set-Content -LiteralPath $lastFile -Value $Address
  }

  if ($Address -notmatch ':\d+$') { $Address = $Address + ':' + $Port }
  # The game uses the player's normal profile (controls, graphics, name).
  $clientArgs = '-client ' + $Address + ' ' + $mod + ' -noSplash' + $backend
  Write-Host ('Joining ' + $Address + ' ...') -ForegroundColor Cyan
  Start-Process -FilePath $game -ArgumentList $clientArgs -WorkingDirectory (Split-Path $game -Parent)
  if ($Mode -eq 'Host') {
    Write-Host ''
    Write-Host 'Game started. Keep this window and the server window open while you play.' -ForegroundColor Green
    Write-Host 'To end the session: quit the game, then close the server window.'
    Read-Host 'Press Enter to close this window (the server keeps running)'
  }
} catch {
  Write-Host $_.Exception.Message -ForegroundColor Red
  Read-Host 'Press Enter to close'
  exit 1
}
