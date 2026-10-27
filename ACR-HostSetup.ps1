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

function Get-ACRAddonList([string]$pair, [string]$ace) {
  $ids = @('AC230926A0010001')
  if ($pair -ne 'vanilla') {
    $ids += @('595F2BF2F44836FB', '1337C0DE5DABBEEF', 'BADC0DEDABBEDA5E')
    if ($pair -eq 'ru_ua' -or $pair -eq 'us_ua') { $ids += '65F92D51845AC237' }
  }
  if ($ace -eq 'yes') { $ids += '60C4E0B49618CC62' }
  return ($ids -join ',')
}

# Every BBB host option. BBB reads them where the AI runs (this server). Default is BBB's own
# default; only changed values are passed. No Min = a switch ('1' on, '0' off).
# Left out: bbbBios/bbbIdentityEggs (every machine must match), bbbVanilla (ACA starts BBB),
# test-only flags, and aliases (bbbAntiArmor, bbbThreatDecay, bbbThreatExplosion).
$script:BBBOptions = @(
  @{ Flag = 'bbbExposedFire'; Group = 'Combat'; Label = 'Pinned in the open: keep spotting, fire faster'; Default = '1' }
  @{ Flag = 'bbbCombatResponse'; Group = 'Combat'; Label = 'Return fire toward incoming fire (no target yet)'; Default = '1' }
  @{ Flag = 'bbbCombatRadio'; Group = 'Combat'; Label = 'Squads share contact reports'; Default = '1' }
  @{ Flag = 'bbbCombatDetail'; Group = 'Combat'; Label = 'Keep AI near a firefight fully simulated'; Default = '1' }
  @{ Flag = 'bbbCombatEngagement'; Group = 'Combat'; Label = 'Take the shot before repositioning (< 100 m)'; Default = '1' }
  @{ Flag = 'bbbCombatTargetPriority'; Group = 'Combat'; Label = 'Prefer a visible close enemy to a remembered one'; Default = '1' }
  @{ Flag = 'bbbCombatManeuver'; Group = 'Combat'; Label = 'Shorter waits in cover under fire (experimental)'; Default = '0' }
  @{ Flag = 'bbbFireManeuver'; Group = 'Combat'; Label = 'Fire and maneuver: base of fire + assault (experimental)'; Default = '0' }
  @{ Flag = 'bbbBuildings'; Group = 'Combat'; Label = 'Use and clear buildings near contacts (experimental)'; Default = '0' }
  @{ Flag = 'bbbGrenades'; Group = 'Grenades'; Label = 'Throw frag grenades at enemies in cover'; Default = '0' }
  @{ Flag = 'bbbGrenadesMinDist'; Group = 'Grenades'; Label = 'Closest throw (m)'; Default = '12'; Min = 8; Max = 25 }
  @{ Flag = 'bbbGrenadesMaxDist'; Group = 'Grenades'; Label = 'Farthest throw (m)'; Default = '38'; Min = 20; Max = 45 }
  @{ Flag = 'bbbGrenadesFriendlyDist'; Group = 'Grenades'; Label = 'Keep friendlies this far from the blast (m)'; Default = '14'; Min = 8; Max = 25 }
  @{ Flag = 'bbbGrenadesCooldown'; Group = 'Grenades'; Label = 'Squad cooldown (s)'; Default = '25'; Min = 5; Max = 120 }
  @{ Flag = 'bbbThreat'; Group = 'Suppression'; Label = 'BBB suppression tuning (off = all of this group vanilla)'; Default = '1' }
  @{ Flag = 'bbbSuppressionDecay'; Group = 'Suppression'; Label = 'Suppression wears off at (1 = vanilla, lower = longer)'; Default = '0.55'; Min = 0.05; Max = 5 }
  @{ Flag = 'bbbExplosionThreat'; Group = 'Suppression'; Label = 'Explosion suppression (1 = vanilla)'; Default = '2.2'; Min = 0.1; Max = 5 }
  @{ Flag = 'bbbExplosionReflex'; Group = 'Suppression'; Label = 'Hit the dirt when a blast wave arrives'; Default = '1' }
  @{ Flag = 'bbbThreatGeometry'; Group = 'Suppression'; Label = 'High ground, flank and ambush fire pin harder'; Default = '1' }
  @{ Flag = 'bbbElevationThreat'; Group = 'Suppression'; Label = 'High-ground factor (0 = off)'; Default = '1'; Min = 0; Max = 3 }
  @{ Flag = 'bbbFlankThreat'; Group = 'Suppression'; Label = 'Rear/flank bonus (0 = off)'; Default = '0.5'; Min = 0; Max = 2 }
  @{ Flag = 'bbbAmbushThreat'; Group = 'Suppression'; Label = 'Ambush shock (0 = off)'; Default = '0.2'; Min = 0; Max = 1 }
  @{ Flag = 'bbbSkySuppression'; Group = 'Suppression'; Label = 'Aircraft gunfire is not a ground contact (fix)'; Default = '1' }
  @{ Flag = 'bbbAimFixes'; Group = 'Aim'; Label = 'Aim fixes (binoculars, suppressed aim)'; Default = '1' }
  @{ Flag = 'bbbAimTuning'; Group = 'Aim'; Label = 'Tuned aim preset (launchers, long range; untested)'; Default = '0' }
  @{ Flag = 'bbbAimScale'; Group = 'Aim'; Label = 'Aim error scale (1 = vanilla, higher = worse)'; Default = '1'; Min = 0; Max = 5 }
  @{ Flag = 'bbbSuppressedAim'; Group = 'Aim'; Label = 'Aim error when suppressed (1 = none)'; Default = '1.3'; Min = 1; Max = 5 }
  @{ Flag = 'bbbLauncherAim'; Group = 'Aim'; Label = 'Launcher aim error (0 = vanilla, never miss)'; Default = '0'; Min = 0; Max = 5 }
  @{ Flag = 'bbbLongRangeAim'; Group = 'Aim'; Label = 'Aim error growth past 215 m (0 = vanilla)'; Default = '0'; Min = 0; Max = 5 }
  @{ Flag = 'bbbBinocsUnderFire'; Group = 'Aim'; Label = 'Allow binoculars while under fire (vanilla)'; Default = '0' }
  @{ Flag = 'bbbArmour'; Group = 'Vehicles'; Label = 'Hold rifle fire at armour it cannot hurt'; Default = '1' }
  @{ Flag = 'bbbArmourHits'; Group = 'Vehicles'; Label = 'Hits before a round counts as useless'; Default = '12'; Min = 1; Max = 100 }
  @{ Flag = 'bbbArmourScatter'; Group = 'Vehicles'; Label = 'Non-AT infantry get low near armour'; Default = '0' }
  @{ Flag = 'bbbArmorReaction'; Group = 'Vehicles'; Label = 'Vehicles smoke and reverse from AT fire (experimental)'; Default = '0' }
  @{ Flag = 'bbbCrewTurnout'; Group = 'Vehicles'; Label = 'Crews turn out when quiet (experimental)'; Default = '0' }
  @{ Flag = 'bbbDriverAlign'; Group = 'Vehicles'; Label = 'Drivers turn for the gunner (experimental)'; Default = '0' }
  @{ Flag = 'bbbCommanderCrew'; Group = 'Vehicles'; Label = 'Commander seats count as crew'; Default = '1' }
  @{ Flag = 'bbbTacticalDismount'; Group = 'Vehicles'; Label = 'Passengers dismount under fire'; Default = '1' }
  @{ Flag = 'bbbGunnerReplace'; Group = 'Vehicles'; Label = 'Replace a dead or downed gunner'; Default = '1' }
  @{ Flag = 'bbbGunnerDrill'; Group = 'Vehicles'; Label = 'Gunners fire back at unseen shooters'; Default = '1' }
  @{ Flag = 'bbbGunnerScan'; Group = 'Vehicles'; Label = 'Gunners scan wider while moving'; Default = '1' }
  @{ Flag = 'bbbUnstick'; Group = 'Vehicles'; Label = 'Free stuck vehicles'; Default = '1' }
  @{ Flag = 'bbbDoors'; Group = 'Vehicles'; Label = 'Crews close doors and hatches'; Default = '1' }
  @{ Flag = 'bbbStaticWeapons'; Group = 'Static weapons'; Label = 'Man nearby static weapons in contact'; Default = '0' }
  @{ Flag = 'bbbStaticReach'; Group = 'Static weapons'; Label = 'Search radius (m)'; Default = '35'; Min = 10; Max = 80 }
  @{ Flag = 'bbbStaticArc'; Group = 'Static weapons'; Label = 'Enemy within this traverse arc (deg)'; Default = '60'; Min = 15; Max = 180 }
  @{ Flag = 'bbbStaticQuiet'; Group = 'Static weapons'; Label = 'Leave the gun after this long quiet (s)'; Default = '15'; Min = 5; Max = 120 }
  @{ Flag = 'bbbMedic'; Group = 'Medical'; Label = 'Safer healing (wait for cover and smoke)'; Default = '0' }
  @{ Flag = 'bbbMedicSmoke'; Group = 'Medical'; Label = 'Smoke before reaching an exposed casualty'; Default = '1' }
  @{ Flag = 'bbbDrag'; Group = 'Medical'; Label = 'Drag casualties into cover (with ACE)'; Default = '1' }
  @{ Flag = 'bbbSmoke'; Group = 'Smoke'; Label = 'Tactical smoke screens'; Default = '0' }
  @{ Flag = 'bbbSmokeDrag'; Group = 'Smoke'; Label = '... for casualty drags'; Default = '1' }
  @{ Flag = 'bbbSmokeManeuver'; Group = 'Smoke'; Label = '... for bounding advances'; Default = '1' }
  @{ Flag = 'bbbSmokeMorale'; Group = 'Smoke'; Label = '... for withdrawals'; Default = '1' }
  @{ Flag = 'bbbSmokeCooldown'; Group = 'Smoke'; Label = 'Squad cooldown (s)'; Default = '30'; Min = 5; Max = 180 }
  @{ Flag = 'bbbSmokeDist'; Group = 'Smoke'; Label = 'Throw distance (m)'; Default = '20'; Min = 8; Max = 35 }
  @{ Flag = 'bbbMorale'; Group = 'Morale'; Label = 'Badly hurt squads break contact (experimental)'; Default = '0' }
  @{ Flag = 'bbbMoraleSurrender'; Group = 'Morale'; Label = 'Cornered, out-of-ammo men surrender'; Default = '1' }
  @{ Flag = 'bbbMoraleCasualties'; Group = 'Morale'; Label = 'Break at this share of casualties'; Default = '0.5'; Min = 0.11; Max = 0.9 }
  @{ Flag = 'bbbMoraleSuppression'; Group = 'Morale'; Label = 'Break at this squad suppression'; Default = '0.6'; Min = 0.11; Max = 1 }
  @{ Flag = 'bbbMoraleDistance'; Group = 'Morale'; Label = 'Withdrawal bound (m)'; Default = '25'; Min = 10; Max = 50 }
  @{ Flag = 'bbbRearm'; Group = 'Soldiers'; Label = 'Take ammo and launchers from the fallen'; Default = '1' }
  @{ Flag = 'bbbRearmRadius'; Group = 'Soldiers'; Label = 'Rearm search radius (m)'; Default = '25'; Min = 5; Max = 100 }
  @{ Flag = 'bbbWeaponReady'; Group = 'Soldiers'; Label = 'Take the weapon in hand when walking empty-handed'; Default = '1' }
  @{ Flag = 'bbbRelaxedMarch'; Group = 'Soldiers'; Label = 'Lower weapons on quiet marches'; Default = '1' }
  @{ Flag = 'bbbHorizonScan'; Group = 'Soldiers'; Label = 'Scan horizons and flanks'; Default = '1' }
  @{ Flag = 'bbbThink'; Group = 'Diagnostics'; Label = 'Think overlay: look at an AI to see why it acts'; Default = '0' }
  @{ Flag = 'bbbPerf'; Group = 'Diagnostics'; Label = 'BBB timing report in the server log'; Default = '0' }
  @{ Flag = 'bbbTurretTrace'; Group = 'Diagnostics'; Label = 'Log turret boarding handoffs'; Default = '0' }
)

# The launcher starts every BBB switch on. Kept off: binoculars under fire (on = vanilla,
# undoing a fix) and log-only diagnostics. Default stays BBB's own, so args still reach BBB.
$script:BBBKeepOff = @('bbbBinocsUnderFire', 'bbbPerf', 'bbbTurretTrace')

function Get-BBBLaunchDefault($o) {
  if ($o.ContainsKey('Min') -or $script:BBBKeepOff -contains $o.Flag) { return $o.Default }
  return '1'
}

function Get-BBBChangedCount($bbb) {
  return @($script:BBBOptions | Where-Object { $bbb[$_.Flag] -ne (Get-BBBLaunchDefault $_) }).Count
}

function Get-BBBSummary($bbb) {
  $switches = @($script:BBBOptions | Where-Object { !$_.ContainsKey('Min') })
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
  if ($value -eq '1') { return 'on' }
  return 'off'
}

function Show-BBBOptions($bbb) {
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
  $s = [ordered]@{ World = 'Worlds/ACR_Arland.ent'; Factions = 'vanilla'; ACE = 'no'; AiO = 'yes'; Civilians = 'no' }
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
  if ($worlds -notcontains $s.World) { $s.World = $worlds[0] }
  if (@('vanilla', 'ru_us', 'ru_ua', 'us_ua') -notcontains $s.Factions) { $s.Factions = 'vanilla' }
  if (@('yes', 'no') -notcontains $s.ACE) { $s.ACE = 'no' }
  if (@('yes', 'no') -notcontains $s.AiO) { $s.AiO = 'yes' }
  if (@('yes', 'no') -notcontains $s.Civilians) { $s.Civilians = 'no' }
  # Optional mods get a row only when installed (ACR-Find.ps1).
  $hasAio = (Find-WorkshopMod '629405AEFC950C12') -ne ''
  $hasCivilians = Test-CivilPopulation
  foreach ($o in $script:BBBOptions) {
    $v = 0.0
    if (!$o.ContainsKey('Min')) { if (@('0', '1') -notcontains $bbb[$o.Flag]) { $bbb[$o.Flag] = Get-BBBLaunchDefault $o } }
    elseif (![double]::TryParse($bbb[$o.Flag], [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$v) -or $v -lt $o.Min -or $v -gt $o.Max) { $bbb[$o.Flag] = $o.Default }
  }
  while ($true) {
    $mapName = [IO.Path]::GetFileNameWithoutExtension($s.World) -replace '^ACR_', ''
    Write-Host ''
    $title = '=== HOST: MAP AND MODS ==='
    if ($Solo) { $title = '=== PLAY: MAP AND MODS ===' }
    Write-Host ($title + '  (type a number to change, Enter to launch)') -ForegroundColor Cyan
    Write-Host (' 1  Map                     ' + $mapName)
    Write-Host (' 2  Faction mods            ' + (Get-PairLabel $s.Factions))
    Write-Host (' 3  ACE All in One          ' + $s.ACE)
    Write-Host (' 4  BBB options             ' + (Get-BBBSummary $bbb))
    if ($hasAio) { Write-Host (' 5  AiO Command Menu        ' + $s.AiO) }
    if ($hasCivilians) { Write-Host (' 6  Civilians (Civil Pop.)  ' + $s.Civilians) }
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
      '5' { if ($hasAio) { if ($s.AiO -eq 'yes') { $s.AiO = 'no' } else { $s.AiO = 'yes' } } }
      '6' { if ($hasCivilians) { if ($s.Civilians -eq 'yes') { $s.Civilians = 'no' } else { $s.Civilians = 'yes' } } }
      default { Write-Host 'Type a number from the list, or press Enter to launch.' -ForegroundColor Yellow }
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
  return @{ Args = $extra; World = $s.World; Addons = (Get-ACRAddonList $s.Factions $s.ACE); Factions = $s.Factions; ACE = $s.ACE; AiO = $s.AiO; Civilians = $s.Civilians }
}

function Show-JoinModSetup {
  $file = Join-Path $PSScriptRoot 'join-settings.txt'
  $pair = 'vanilla'; $ace = 'no'; $aio = 'yes'; $civilians = 'no'
  if (Test-Path -LiteralPath $file) {
    foreach ($line in Get-Content -LiteralPath $file) {
      $kv = $line.Split('=', 2)
      if ($kv.Count -eq 2 -and $kv[0] -eq 'Factions') { $pair = $kv[1] }
      if ($kv.Count -eq 2 -and $kv[0] -eq 'ACE') { $ace = $kv[1] }
      if ($kv.Count -eq 2 -and $kv[0] -eq 'AiO' -and @('yes', 'no') -contains $kv[1]) { $aio = $kv[1] }
      if ($kv.Count -eq 2 -and $kv[0] -eq 'Civilians' -and @('yes', 'no') -contains $kv[1]) { $civilians = $kv[1] }
    }
  }
  $hasAio = (Find-WorkshopMod '629405AEFC950C12') -ne ''
  $hasCivilians = Test-CivilPopulation
  while ($true) {
    Write-Host ''
    Write-Host '=== JOIN MODS ===' -ForegroundColor Cyan
    Write-Host (' 1  Factions    ' + (Get-PairLabel $pair) + '   (must match the host)')
    Write-Host (' 2  ACE         ' + $ace + '   (must match the host)')
    if ($hasAio) { Write-Host (' 3  AiO         ' + $aio + '   (must match the host)') }
    if ($hasCivilians) { Write-Host (' 4  Civilians   ' + $civilians + '   (must match the host)') }
    $choice = (Read-Host 'Choice (Enter to continue)').Trim()
    if ($choice -eq '1') {
      $pairs = @('vanilla', 'ru_us', 'ru_ua', 'us_ua'); $i = [array]::IndexOf($pairs, $pair); $pair = $pairs[($i + 1) % $pairs.Count]
    } elseif ($choice -eq '2') { if ($ace -eq 'yes') { $ace = 'no' } else { $ace = 'yes' } }
    elseif ($choice -eq '3' -and $hasAio) { if ($aio -eq 'yes') { $aio = 'no' } else { $aio = 'yes' } }
    elseif ($choice -eq '4' -and $hasCivilians) { if ($civilians -eq 'yes') { $civilians = 'no' } else { $civilians = 'yes' } }
    else { break }
  }
  @('Factions=' + $pair, 'ACE=' + $ace, 'AiO=' + $aio, 'Civilians=' + $civilians) | Set-Content -LiteralPath $file
  return @{ Addons = (Get-ACRAddonList $pair $ace); Factions = $pair; ACE = $ace; AiO = $aio; Civilians = $civilians }
}
