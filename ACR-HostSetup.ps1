# Map and optional mod loading for Play/Host/Join. Match settings are chosen in game.
# Map/mod choices are remembered in host-settings.txt, play-settings.txt or join-settings.txt.
function Get-PairFactionKeys([string]$pair) {
  switch ($pair) {
    'ru_us' { return @('RHS_USAF', 'RHS_AFRF') }
    'ru_ua' { return @('US', 'RHS_AFRF') }
    'us_ua' { return @('RHS_USAF', 'US') }
    default { return @('US', 'USSR') }
  }
}

function Get-PairLabel([string]$pair) {
  switch ($pair) {
    'ru_us' { return 'RHS Russia vs RHS US' }
    'ru_ua' { return 'RHS Russia vs UA (UA replaces vanilla US)' }
    'us_ua' { return 'RHS US vs UA (UA replaces vanilla US)' }
    default { return 'Vanilla US vs USSR' }
  }
}

# Expansion packs: separate add-ons beside ACA that plug into ACR_ContentSocket. Only those
# whose folder is present are offered. (Civil Population needs ACA's civilians branch: not here.)
$script:ACRPacks = @(
  @{ Key = 'coldwar'; Folder = 'ACAColdWar'; Id = 'AC230926CC000002'; Label = 'Cold War (era vehicles and kit)' },
  @{ Key = 'modern'; Folder = 'ACAModern'; Id = 'AC230926DD000003'; Label = 'Modern (RHS sides: pick RHS Russia vs RHS US)' }
)

function Get-AvailablePacks {
  $root = Split-Path $PSScriptRoot -Parent
  return @($script:ACRPacks | Where-Object { Test-Path -LiteralPath (Join-Path (Join-Path $root $_.Folder) 'addon.gproj') })
}

function Get-PackLabel([string]$pack) {
  foreach ($p in $script:ACRPacks) { if ($p.Key -eq $pack) { return $p.Label } }
  return 'none'
}

# ACA map add-ons beside ACA (ACAMap*): world path -> add-on GUID, for the map row.
function Get-MapAddons {
  $maps = [ordered]@{}
  $root = Split-Path $PSScriptRoot -Parent
  foreach ($dir in Get-ChildItem -LiteralPath $root -Directory -Filter 'ACAMap*' -ErrorAction SilentlyContinue) {
    $project = Join-Path $dir.FullName 'addon.gproj'
    if (!(Test-Path -LiteralPath $project)) { continue }
    $m = [regex]::Match((Get-Content -LiteralPath $project -Raw), 'GUID\s+"([0-9A-Fa-f]{16})"')
    if (!$m.Success) { continue }
    foreach ($world in Get-ChildItem -LiteralPath (Join-Path $dir.FullName 'Worlds') -Filter 'ACR_*.ent' -ErrorAction SilentlyContinue) {
      $maps['Worlds/' + $world.Name] = $m.Groups[1].Value
    }
  }
  return $maps
}

# A pack or a map add-on brings Workshop mods of its own, as faction mods and ACE do.
function Test-NeedsModAddons([string]$pair, [string]$ace, [string]$pack, [string]$mapId) {
  return ($pair -ne 'vanilla' -or $ace -eq 'yes' -or ($pack -and $pack -ne 'none') -or [bool]$mapId)
}

function Get-ACRAddonList([string]$pair, [string]$ace, [string]$pack = 'none', [string]$mapId = '') {
  $ids = @('AC230926A0010001')
  if ($pair -ne 'vanilla') {
    $ids += @('595F2BF2F44836FB', '1337C0DE5DABBEEF', 'BADC0DEDABBEDA5E')
    if ($pair -eq 'ru_ua' -or $pair -eq 'us_ua') { $ids += '65F92D51845AC237' }
  }
  if ($ace -eq 'yes') { $ids += '60C4E0B49618CC62' }
  foreach ($p in $script:ACRPacks) { if ($p.Key -eq $pack) { $ids += $p.Id } }
  if ($mapId) { $ids += $mapId }
  return ($ids -join ',')
}

# Every BBB host option, read from BBB's own settings registry (BBB_Settings.c in the BBB
# addon beside ACA: the dev checkout or the package's BetterBattleBuddies folder), so the
# launcher always offers what the BBB it starts has. BBB reads them where the AI runs (this
# server). Default is BBB's own default; only changed values are passed. A switch has no Min
# and no Values; a choice has Values.
# Left out: bbbVanilla (ACA starts BBB) and bbbBios/bbbIdentityEggs (every machine must match).
$script:BBBSkip = @('bbbVanilla', 'bbbBios', 'bbbIdentityEggs')

function Find-BBBSettingsFile {
  $root = Split-Path $PSScriptRoot -Parent
  foreach ($addon in Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue) {
    $project = Join-Path $addon.FullName 'addon.gproj'
    if (!(Test-Path -LiteralPath $project)) { continue }
    if ((Get-Content -LiteralPath $project -Raw) -notmatch 'GUID\s+"BBB0261015000001"') { continue }
    $file = Join-Path $addon.FullName 'Scripts\Game\BBB\BBB_Settings.c'
    if (Test-Path -LiteralPath $file) { return $file }
  }
  return $null
}

function Read-BBBOptions {
  $options = @()
  $file = Find-BBBSettingsFile
  if (!$file) { return $options }
  $str = '"((?:[^"\\]|\\.)*)"'
  $num = '(-?[\d.]+)'
  $group = ''
  $culture = [Globalization.CultureInfo]::InvariantCulture
  foreach ($line in Get-Content -LiteralPath $file) {
    $m = [regex]::Match($line, '^\s*g = ' + $str + ';')
    if ($m.Success) { $group = $m.Groups[1].Value; continue }
    $m = [regex]::Match($line, '^\s*(On|Off)\(g, ' + $str + ', ' + $str)
    if ($m.Success) {
      $default = '0'
      if ($m.Groups[1].Value -eq 'On') { $default = '1' }
      $options += @{ Flag = $m.Groups[2].Value; Group = $group; Label = $m.Groups[3].Value; Default = $default }
      continue
    }
    $m = [regex]::Match($line, '^\s*Num\(g, ' + $str + ', ' + $num + ', ' + $num + ', ' + $num + ', ' + $num + ', ' + $str)
    if ($m.Success) {
      $options += @{ Flag = $m.Groups[1].Value; Group = $group; Label = $m.Groups[6].Value; Default = $m.Groups[2].Value
                     Min = [double]::Parse($m.Groups[3].Value, $culture); Max = [double]::Parse($m.Groups[4].Value, $culture) }
      continue
    }
    $m = [regex]::Match($line, '^\s*Def\(g, ' + $str + ', BBB_ESettingKind\.CHOICE, ' + $str + ', ' + $str + '.*\.Choices\(' + $str + ', ' + $str + '\)')
    if ($m.Success) {
      $options += @{ Flag = $m.Groups[1].Value; Group = $group; Label = $m.Groups[3].Value; Default = $m.Groups[2].Value
                     Values = $m.Groups[4].Value.Split('|'); Labels = $m.Groups[5].Value.Split('|') }
    }
  }
  return @($options | Where-Object { $script:BBBSkip -notcontains $_.Flag })
}

$script:BBBOptions = @(Read-BBBOptions)

# The launcher starts every BBB switch on, the experimental and off-by-default ones too (Hot AI,
# vehicles giving way...). Kept at BBB's default: numbers, choices (binoculars under fire = vanilla,
# AI skill = the mission's own, Think panel) and the log-only Diagnostics group.
function Get-BBBLaunchDefault($o) {
  if ($o.ContainsKey('Min') -or $o.ContainsKey('Values') -or $o.Group -eq 'Diagnostics') { return $o.Default }
  return '1'
}

function Get-BBBChangedCount($bbb) {
  return @($script:BBBOptions | Where-Object { $bbb[$_.Flag] -ne (Get-BBBLaunchDefault $_) }).Count
}

function Get-BBBSummary($bbb) {
  if ($script:BBBOptions.Count -eq 0) { return "BBB's own defaults (BBB_Settings.c not found beside ACA)" }
  $switches = @($script:BBBOptions | Where-Object { !$_.ContainsKey('Min') -and !$_.ContainsKey('Values') })
  $onCount = @($switches | Where-Object { $bbb[$_.Flag] -eq '1' }).Count
  return ([string]$onCount + ' of ' + $switches.Count + ' on, ' + (Get-BBBChangedCount $bbb) + ' changed from defaults')
}

function Get-BBBArgs($bbb) {
  $out = ''
  foreach ($o in $script:BBBOptions) {
    if ($bbb[$o.Flag] -ne $o.Default) { $out += ' -' + $o.Flag + ' ' + $bbb[$o.Flag] }
  }
  return $out
}

function Format-BBBValue($o, [string]$value) {
  if ($o.ContainsKey('Min')) { return $value }
  if ($o.ContainsKey('Values')) {
    $i = [array]::IndexOf($o.Values, $value)
    if ($i -ge 0 -and $i -lt $o.Labels.Count) { return $o.Labels[$i] }
    return $value
  }
  if ($value -eq '1') { return 'on' }
  return 'off'
}

function Show-BBBOptions($bbb) {
  if ($script:BBBOptions.Count -eq 0) { Write-Host "BBB's settings file was not found beside ACA: BBB runs with its own defaults." -ForegroundColor Yellow; return }
  while ($true) {
    Write-Host ''
    Write-Host '=== ALL BBB OPTIONS ===  (number: toggle or set, d: all defaults, Enter: back)' -ForegroundColor Cyan
    $group = ''
    for ($i = 0; $i -lt $script:BBBOptions.Count; $i++) {
      $o = $script:BBBOptions[$i]
      if ($o.Group -ne $group) { $group = $o.Group; Write-Host ('-- ' + $group) -ForegroundColor DarkCyan }
      $value = Format-BBBValue $o $bbb[$o.Flag]
      $line = ('{0,3}  {1,-56} {2}' -f ($i + 1), $o.Label, $value)
      $launchDefault = Get-BBBLaunchDefault $o
      if ($bbb[$o.Flag] -ne $launchDefault) { Write-Host ($line + '   (default ' + (Format-BBBValue $o $launchDefault) + ')') -ForegroundColor Yellow }
      else { Write-Host $line }
    }
    $choice = (Read-Host 'Choice').Trim()
    if ($choice -eq '') { return }
    if ($choice -eq 'd') { foreach ($o in $script:BBBOptions) { $bbb[$o.Flag] = Get-BBBLaunchDefault $o }; continue }
    $n = 0
    if (![int]::TryParse($choice, [ref]$n) -or $n -lt 1 -or $n -gt $script:BBBOptions.Count) { Write-Host ('Type a number from 1 to ' + $script:BBBOptions.Count + ', d, or Enter.') -ForegroundColor Yellow; continue }
    $o = $script:BBBOptions[$n - 1]
    if ($o.ContainsKey('Values')) {
      $i = [array]::IndexOf($o.Values, $bbb[$o.Flag])
      $bbb[$o.Flag] = $o.Values[($i + 1) % $o.Values.Count]
      continue
    }
    if (!$o.ContainsKey('Min')) {
      if ($bbb[$o.Flag] -eq '1') { $bbb[$o.Flag] = '0' } else { $bbb[$o.Flag] = '1' }
      continue
    }
    $entry = (Read-Host ($o.Label + ' (' + $o.Min + ' to ' + $o.Max + ', Enter = default ' + $o.Default + ')')).Trim()
    if ($entry -eq '') { $bbb[$o.Flag] = $o.Default; continue }
    $v = 0.0
    if ([double]::TryParse($entry, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$v) -and $v -ge $o.Min -and $v -le $o.Max) {
      $bbb[$o.Flag] = $v.ToString([Globalization.CultureInfo]::InvariantCulture)
    } else { Write-Host ('Not a number from ' + $o.Min + ' to ' + $o.Max + '; unchanged.') -ForegroundColor Yellow }
  }
}

function Show-HostSetup([switch]$Solo) {
  $name = 'host-settings.txt'
  if ($Solo) { $name = 'play-settings.txt' }
  $file = Join-Path $PSScriptRoot $name
  # Only pre-load choices belong here. Ignore old saved match settings.
  $s = [ordered]@{ World = 'Worlds/ACR_Arland.ent'; Factions = 'vanilla'; ACE = 'no'; Pack = 'none' }
  $bbb = @{}
  foreach ($o in $script:BBBOptions) { $bbb[$o.Flag] = Get-BBBLaunchDefault $o }
  # Settings files from before the full BBB list used these names (yes/no).
  $oldNames = @{ FireManeuver = 'bbbFireManeuver'; CombatManeuver = 'bbbCombatManeuver'; CrewTurnout = 'bbbCrewTurnout'; DriverAlign = 'bbbDriverAlign' }
  if (Test-Path -LiteralPath $file) {
    foreach ($line in Get-Content -LiteralPath $file) {
      $kv = $line.Split('=', 2)
      if ($kv.Count -ne 2) { continue }
      $key = $kv[0].Trim(); $value = $kv[1].Trim()
      if ($s.Contains($key)) { $s[$key] = $value }
      elseif ($oldNames.ContainsKey($key)) { if ($value -eq 'yes') { $bbb[$oldNames[$key]] = '1' } }
      elseif ($bbb.ContainsKey($key) -and $value -ne '') { $bbb[$key] = $value }
    }
  }
  $worlds = @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'Worlds') -Filter 'ACR_*.ent' -ErrorAction SilentlyContinue |
              Where-Object { $_.Name -notlike '*TestRange*' } | ForEach-Object { 'Worlds/' + $_.Name })
  if ($worlds.Count -eq 0) { $worlds = @('Worlds/ACR_Arland.ent') }
  $mapAddons = Get-MapAddons
  foreach ($w in $mapAddons.Keys) { if ($worlds -notcontains $w) { $worlds += $w } }
  if ($worlds -notcontains $s.World) { $s.World = $worlds[0] }
  $packs = @('none') + @(Get-AvailablePacks | ForEach-Object { $_.Key })
  if ($packs -notcontains $s.Pack) { $s.Pack = 'none' }
  if (@('vanilla', 'ru_us', 'ru_ua', 'us_ua') -notcontains $s.Factions) { $s.Factions = 'vanilla' }
  if (@('yes', 'no') -notcontains $s.ACE) { $s.ACE = 'no' }
  foreach ($o in $script:BBBOptions) {
    $v = 0.0
    if ($o.ContainsKey('Values')) { if ($o.Values -notcontains $bbb[$o.Flag]) { $bbb[$o.Flag] = $o.Default } }
    elseif (!$o.ContainsKey('Min')) { if (@('0', '1') -notcontains $bbb[$o.Flag]) { $bbb[$o.Flag] = Get-BBBLaunchDefault $o } }
    elseif (![double]::TryParse($bbb[$o.Flag], [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$v) -or $v -lt $o.Min -or $v -gt $o.Max) { $bbb[$o.Flag] = $o.Default }
  }
  while ($true) {
    $mapName = [IO.Path]::GetFileNameWithoutExtension($s.World) -replace '^ACR_', ''
    Write-Host ''
    $title = '=== HOST: MAP AND MODS ==='
    if ($Solo) { $title = '=== PLAY: MAP AND MODS ===' }
    Write-Host ($title + '  (type a number to change, Enter to launch)') -ForegroundColor Cyan
    $mapNote = ''
    if ($mapAddons.Contains($s.World)) { $mapNote = '   (map add-on)' }
    Write-Host (' 1  Map                     ' + $mapName + $mapNote)
    Write-Host (' 2  Faction mods            ' + (Get-PairLabel $s.Factions))
    Write-Host (' 3  ACE All in One          ' + $s.ACE)
    Write-Host (' 4  BBB options             ' + (Get-BBBSummary $bbb))
    if ($packs.Count -gt 1) { Write-Host (' 5  Expansion pack          ' + (Get-PackLabel $s.Pack)) }
    Write-Host 'Choose the playable sides, battlefield and match settings in game, then press Start.' -ForegroundColor DarkGray
    Write-Host 'Faction mods must be loaded now; UA replaces vanilla US while loaded.' -ForegroundColor DarkGray
    if (!$Solo) { Write-Host 'Connect before friends: the first connection controls setup; access does not transfer after disconnect.' -ForegroundColor Yellow }
    $choice = (Read-Host 'Choice').Trim()
    if ($choice -eq '') { break }
    switch ($choice) {
      '1' { if ($worlds.Count -gt 1) { $i = [array]::IndexOf($worlds, $s.World); $s.World = $worlds[($i + 1) % $worlds.Count] } else { Write-Host 'Only one map is available.' -ForegroundColor Yellow } }
      '2' { $pairs = @('vanilla', 'ru_us', 'ru_ua', 'us_ua'); $i = [array]::IndexOf($pairs, $s.Factions); $s.Factions = $pairs[($i + 1) % $pairs.Count] }
      '3' { if ($s.ACE -eq 'yes') { $s.ACE = 'no' } else { $s.ACE = 'yes' } }
      '4' { Show-BBBOptions $bbb }
      '5' { if ($packs.Count -gt 1) { $i = [array]::IndexOf($packs, $s.Pack); $s.Pack = $packs[($i + 1) % $packs.Count] } else { Write-Host 'No expansion pack is installed beside ACA.' -ForegroundColor Yellow } }
      default { Write-Host 'Type a number from 1 to 5, or press Enter to launch.' -ForegroundColor Yellow }
    }
  }
  # Only options changed from the launcher default are saved, so a new default reaches old files.
  $lines = @($s.Keys | ForEach-Object { $_ + '=' + $s[$_] })
  foreach ($o in $script:BBBOptions) { if ($bbb[$o.Flag] -ne (Get-BBBLaunchDefault $o)) { $lines += ($o.Flag + '=' + $bbb[$o.Flag]) } }
  $lines | Set-Content -LiteralPath $file
  # World launches have no mission header: enable the in-game setup explicitly.
  $extra = ' -acrSetup 1'
  # This launcher starts a separate local server. Its owner connects first.
  # Use the existing opt-in authority rule, without changing dedicated-server defaults.
  if (!$Solo) { $extra += ' -acrFirstPlayerHost 1' }
  # Dev copies only (a git checkout, never a release): chat "/login 1" makes you admin.
  if (Test-Path -LiteralPath (Join-Path $PSScriptRoot '.git')) { $extra += ' -acrAdminPassword 1' }
  # BBB options changed from their defaults (the AI runs where these args go).
  $extra += Get-BBBArgs $bbb
  $mapId = ''
  if ($mapAddons.Contains($s.World)) { $mapId = $mapAddons[$s.World] }
  return @{ Args = $extra; World = $s.World; Addons = (Get-ACRAddonList $s.Factions $s.ACE $s.Pack $mapId); Factions = $s.Factions; ACE = $s.ACE; Pack = $s.Pack; NeedsMods = (Test-NeedsModAddons $s.Factions $s.ACE $s.Pack $mapId) }
}

function Show-JoinModSetup {
  $file = Join-Path $PSScriptRoot 'join-settings.txt'
  $pair = 'vanilla'; $ace = 'no'; $pack = 'none'; $map = 'none'
  if (Test-Path -LiteralPath $file) {
    foreach ($line in Get-Content -LiteralPath $file) {
      $kv = $line.Split('=', 2)
      if ($kv.Count -eq 2 -and $kv[0] -eq 'Factions') { $pair = $kv[1] }
      if ($kv.Count -eq 2 -and $kv[0] -eq 'ACE') { $ace = $kv[1] }
      if ($kv.Count -eq 2 -and $kv[0] -eq 'Pack') { $pack = $kv[1] }
      if ($kv.Count -eq 2 -and $kv[0] -eq 'Map') { $map = $kv[1] }
    }
  }
  $packs = @('none') + @(Get-AvailablePacks | ForEach-Object { $_.Key })
  if ($packs -notcontains $pack) { $pack = 'none' }
  # A host on a map add-on's world: the joiner loads that add-on too.
  $mapAddons = Get-MapAddons
  $maps = @('none') + @($mapAddons.Keys)
  if ($maps -notcontains $map) { $map = 'none' }
  while ($true) {
    Write-Host ''
    Write-Host '=== JOIN MODS ===' -ForegroundColor Cyan
    Write-Host (' 1  Factions    ' + (Get-PairLabel $pair) + '   (must match the host)')
    Write-Host (' 2  ACE         ' + $ace + '   (must match the host)')
    if ($packs.Count -gt 1) { Write-Host (' 3  Expansion   ' + (Get-PackLabel $pack) + '   (must match the host)') }
    if ($maps.Count -gt 1) {
      $mapLabel = 'none (a map built into ACA)'
      if ($map -ne 'none') { $mapLabel = [IO.Path]::GetFileNameWithoutExtension($map) -replace '^ACR_', '' }
      Write-Host (' 4  Map add-on  ' + $mapLabel + '   (the host''s map, if it is a map add-on)')
    }
    $choice = (Read-Host 'Choice (Enter to continue)').Trim()
    if ($choice -eq '1') {
      $pairs = @('vanilla', 'ru_us', 'ru_ua', 'us_ua'); $i = [array]::IndexOf($pairs, $pair); $pair = $pairs[($i + 1) % $pairs.Count]
    } elseif ($choice -eq '2') { if ($ace -eq 'yes') { $ace = 'no' } else { $ace = 'yes' } }
    elseif ($choice -eq '3' -and $packs.Count -gt 1) { $i = [array]::IndexOf($packs, $pack); $pack = $packs[($i + 1) % $packs.Count] }
    elseif ($choice -eq '4' -and $maps.Count -gt 1) { $i = [array]::IndexOf($maps, $map); $map = $maps[($i + 1) % $maps.Count] }
    else { break }
  }
  @('Factions=' + $pair, 'ACE=' + $ace, 'Pack=' + $pack, 'Map=' + $map) | Set-Content -LiteralPath $file
  $mapId = ''
  if ($map -ne 'none') { $mapId = $mapAddons[$map] }
  return @{ Addons = (Get-ACRAddonList $pair $ace $pack $mapId); Factions = $pair; ACE = $ace; Pack = $pack; NeedsMods = (Test-NeedsModAddons $pair $ace $pack $mapId) }
}
