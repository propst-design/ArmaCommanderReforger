# Finds a Steam game or tool on this PC, wherever it is installed:
#   1. a location remembered from last time (game-paths.txt next to this file),
#   2. every Steam library (Steam's registry keys + libraryfolders.vdf),
#   3. Windows' list of installed programs,
#   4. common drive letters,
# and if all that fails, asks the player to point at the .exe once (remembered).
function Find-SteamApp([string]$folder, [string]$exe) {
  $memory = Join-Path $PSScriptRoot 'game-paths.txt'
  $remembered = @{}
  if (Test-Path -LiteralPath $memory) {
    foreach ($line in Get-Content -LiteralPath $memory) {
      $parts = $line.Split('|')
      if ($parts.Count -eq 2) { $remembered[$parts[0]] = $parts[1] }
    }
  }
  if ($remembered.ContainsKey($exe) -and (Test-Path -LiteralPath $remembered[$exe])) { return $remembered[$exe] }

  $roots = @()
  foreach ($key in 'HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam') {
    $item = Get-ItemProperty $key -ErrorAction SilentlyContinue
    if ($item -and $item.SteamPath) { $roots += $item.SteamPath }
    if ($item -and $item.InstallPath) { $roots += $item.InstallPath }
  }
  $libraries = @()
  foreach ($root in $roots) {
    $libraries += $root
    $libraryFile = Join-Path $root 'steamapps\libraryfolders.vdf'
    if (Test-Path -LiteralPath $libraryFile) {
      foreach ($m in [regex]::Matches((Get-Content -LiteralPath $libraryFile -Raw), '"path"\s+"([^"]+)"')) { $libraries += $m.Groups[1].Value.Replace('\\', '\') }
    }
  }
  foreach ($drive in (Get-PSDrive -PSProvider FileSystem -ErrorAction SilentlyContinue)) {
    $libraries += (Join-Path $drive.Root 'SteamLibrary')
    $libraries += (Join-Path $drive.Root 'Steam')
    $libraries += (Join-Path $drive.Root 'Program Files (x86)\Steam')
    $libraries += (Join-Path $drive.Root 'Program Files\Steam')
    $libraries += (Join-Path $drive.Root 'Games\Steam')
    $libraries += (Join-Path $drive.Root 'Games\SteamLibrary')
  }
  foreach ($library in ($libraries | Select-Object -Unique)) {
    if (!$library) { continue }
    $candidate = Join-Path $library "steamapps\common\$folder\$exe"
    if (Test-Path -LiteralPath $candidate) { return $candidate }
  }
  # Windows' installed-programs list (Steam registers every game there).
  foreach ($uninstall in 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*', 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*', 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*') {
    foreach ($entry in (Get-ItemProperty $uninstall -ErrorAction SilentlyContinue)) {
      if ($entry.DisplayName -ne $folder -or !$entry.InstallLocation) { continue }
      $candidate = Join-Path $entry.InstallLocation $exe
      if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
  }

  # Still nothing: ask once, and remember the answer.
  Write-Host ''
  Write-Host "Could not find $folder automatically." -ForegroundColor Yellow
  Write-Host "A window will open: please select $exe (usually in ...\steamapps\common\$folder\)."
  try {
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    $dialog.Title = "Where is $exe ($folder)?"
    $dialog.Filter = "$exe|$exe|Programs (*.exe)|*.exe"
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK -and (Test-Path -LiteralPath $dialog.FileName)) {
      $chosen = $dialog.FileName
      Add-Content -LiteralPath $memory -Value ($exe + '|' + $chosen)
      return $chosen
    }
  } catch {
    $typed = Read-Host "Paste the full path to $exe"
    if ($typed -and (Test-Path -LiteralPath $typed.Trim('"'))) {
      Add-Content -LiteralPath $memory -Value ($exe + '|' + $typed.Trim('"'))
      return $typed.Trim('"')
    }
  }
  return $null
}

# Where downloaded faction mods and ACE live: $env:ACR_MODS_DIR if set, the game's own Workshop
# folder (Documents\My Games\ArmaReforger\addons), else E:\ArmaReforgerMods\addons (the
# maintainer's). Returns $addonsDir with the first that exists appended, or unchanged.
function Add-ModAddons([string]$addonsDir) {
  $candidates = @()
  if ($env:ACR_MODS_DIR) { $candidates += $env:ACR_MODS_DIR }
  $docs = [Environment]::GetFolderPath('MyDocuments')
  if ($docs) { $candidates += (Join-Path $docs 'My Games\ArmaReforger\addons') }
  $candidates += 'E:\ArmaReforgerMods\addons'
  foreach ($dir in $candidates) {
    if (!(Test-Path -LiteralPath $dir)) { continue }
    if (($addonsDir.Split(',') | ForEach-Object { $_.TrimEnd('\') }) -contains $dir.TrimEnd('\')) { return $addonsDir }
    return $addonsDir + ',' + $dir
  }
  Write-Host 'Faction mods/ACE folder not found (set ACR_MODS_DIR to it); starting without it.' -ForegroundColor Yellow
  return $addonsDir
}

# All-in-One Command Menu (separate mod, 629405AEFC950C12): when its Workshop copy is
# downloaded on this PC, play/host/join load it too, no option needed. Only its own folder goes
# FIRST in -addonsDir, so its Workshop copy wins without exposing Workshop ACA/BBB copies
# ahead of the launcher's local addons. With duplicate mod IDs the first folder wins.
# Returns @(addonsDir, addonList), unchanged when it isn't installed.
function Add-AIOCommandMenu([string]$addonsDir, [string]$addonList) {
  $aioId = '629405AEFC950C12'
  $candidates = @()
  $docs = [Environment]::GetFolderPath('MyDocuments')
  if ($docs) { $candidates += (Join-Path $docs 'My Games\ArmaReforger\addons') }
  $candidates += 'E:\ArmaReforgerMods\addons'
  foreach ($dir in $candidates) {
    if (!(Test-Path -LiteralPath $dir)) { continue }
    $hit = Get-ChildItem -LiteralPath $dir -Directory -Filter "*_$aioId" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (!$hit) { continue }
    $aioDir = $hit.FullName
    $dirs = @($aioDir) + @($addonsDir.Split(',') | Where-Object { $_ -and $_.TrimEnd('\') -ne $aioDir.TrimEnd('\') })
    if (($addonList.Split(',')) -notcontains $aioId) { $addonList += ',' + $aioId }
    Write-Host 'All-in-One Command Menu found (Workshop copy): loading it too.' -ForegroundColor DarkCyan
    return @(($dirs -join ','), $addonList)
  }
  return @($addonsDir, $addonList)
}

# BBB is a required standalone addon. Development and ZIP copies keep it beside ACA.
function Assert-BBBDependency([string]$addonsDir) {
  foreach ($dir in $addonsDir.Split(',')) {
    if (!(Test-Path -LiteralPath $dir)) { continue }
    foreach ($addon in Get-ChildItem -LiteralPath $dir -Directory) {
      $project = Join-Path $addon.FullName 'addon.gproj'
      if ((Test-Path -LiteralPath $project) -and (Get-Content -LiteralPath $project -Raw) -match 'GUID\s+"BBB0261015000001"') { return }
    }
  }
  throw 'Better Battle Buddies is required. Install the complete ACActual-WithBBB.zip with both addon folders, or use the in-game Workshop installation.'
}
