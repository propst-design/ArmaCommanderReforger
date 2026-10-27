// Host option: all players on one side (-acrPlayers US / USSR; "any" or absent
// = free choice as before). The server puts every joining player on that side
// and refuses requests to switch to another, so the deploy screen goes straight
// to spawning. Server only; clients never see the option.
class ACR_PlayerSide
{
 protected static bool s_bRead;
 protected static string s_sKey;

 // Side that players deploy on without the deploy screen (tests), "" = the menu.
 static string AutoDeploySide()
 {
  string side = ACR_TestHooks.Get().AutoDeploySide();
  if (side.IsEmpty()) return "";
  string forced = Key();
  if (forced != "") return forced;
  side.ToLower();
  if (side == "east") return ACR_FactionSelection.East();
  return ACR_FactionSelection.West();
 }

 static void Force(string key)
 {
  s_bRead = true;
  s_sKey = key;
 }

 // The forced faction key, or "" for free choice.
 static string Key()
 {
  if (s_bRead) return s_sKey;
  s_bRead = true;
  string value;
  if (!ACR_Options.Get("acrPlayers", value)) return s_sKey;
  value.Trim();
  string lower = value;
  lower.ToLower();
  if (lower == "" || lower == "any" || lower == "free") return s_sKey;
  FactionManager factions = GetGame().GetFactionManager();
  if (!factions) { s_bRead = false; return ""; }
  // Match the key regardless of case.
  array<Faction> all = {};
  factions.GetFactionsList(all);
  foreach (Faction faction : all)
  {
   string key = faction.GetFactionKey();
   string keyLower = key;
   keyLower.ToLower();
   if (keyLower == lower) s_sKey = key;
  }
  if (s_sKey == "") PrintFormat("[ACR] Host option -acrPlayers %1: no such faction, players choose freely", value);
  else PrintFormat("[ACR] Host option: all players join %1", s_sKey);
  return s_sKey;
 }

 // The scenario may only have a spawn point for one side (Arland: US at the airfield).
 // With players forced onto a side that has none, the game's auto-spawn retried every
 // second forever and the game never finished loading (1 Oct, -acrPlayers USSR).
 // Place one at that side's own base farthest from the other side's spawn points.
 static const ResourceName SPAWN_POINT = "{5A2220DD0C16958B}Prefabs/MP/Spawning/SpawnPoint_US.et";
 static bool s_bSpawnChecked; // (regress test resets it)
 static void EnsureSpawnPoint()
 {
  if (s_bSpawnChecked) return;
  string key = Key();
  if (key == "") { s_bSpawnChecked = true; return; }
  if (ACR_Base.s_All.IsEmpty()) return; // bases not registered yet
  s_bSpawnChecked = true;
  if (SCR_SpawnPoint.GetSpawnPointCountForFaction(key) > 0) return;
  ACR_Base best;
  float bestDistance = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != key || !base.m_bAllowSpawn) continue;
   float nearestEnemy = 1000000;
   foreach (SCR_SpawnPoint other : SCR_SpawnPoint.GetSpawnPoints())
    if (other && other.GetFactionKey() != key) nearestEnemy = Math.Min(nearestEnemy, vector.Distance(other.GetOrigin(), base.GetOrigin()));
   if (nearestEnemy > bestDistance) { bestDistance = nearestEnemy; best = base; }
  }
  if (!best) { PrintFormat("[ACR] No base of %1 to put a player spawn point at", key); return; }
  SCR_SpawnPoint point = SCR_SpawnPoint.Cast(ACR_Manager.SpawnPrefab(SPAWN_POINT, ACR_Manager.Ground(best.GetOrigin() + Vector(6, 0, 6))));
  if (point) point.SetFactionKey(key);
  PrintFormat("[ACR] Player spawn point for %1 placed at %2: %3", key, best.m_sName, point != null);
 }

 // Server tick: anyone not on the chosen side is moved to it (only while not
 // alive in the field, so nobody is yanked mid-fight).
 static void Enforce()
 {
  string key = Key();
  if (key == "") return;
  EnsureSpawnPoint();
  Faction target = GetGame().GetFactionManager().GetFactionByKey(key);
  if (!target) return;
  array<int> ids = {};
  GetGame().GetPlayerManager().GetPlayers(ids);
  foreach (int id : ids)
  {
   PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(id);
   if (!pc) continue;
   SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(pc.FindComponent(SCR_PlayerFactionAffiliationComponent));
   if (!affiliation) continue;
   // The faction manager's record is what everything else reads (command, map, RP).
   // Set straight after connecting, it was wiped when the player finished registering,
   // while the player's own component kept USSR: he spawned, but as nobody's
   // commander (1 Oct, hosting as USSR). Re-sync the record when they differ.
   if (affiliation.GetAffiliatedFaction() == target)
   {
    if (SCR_FactionManager.SGetPlayerFaction(id) != target)
    {
     SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
     if (factionManager) factionManager.UpdatePlayerFaction_S(affiliation);
     PrintFormat("[ACR] %1: faction record re-synced to %2 (now %3)", GetGame().GetPlayerManager().GetPlayerName(id), key, SCR_FactionManager.SGetPlayerFaction(id) == target);
    }
    continue;
   }
   IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (body && ACR_Respawn.IsAlive(body)) continue;
   if (affiliation.SetFaction_S(target)) PrintFormat("[ACR] %1 assigned to %2", GetGame().GetPlayerManager().GetPlayerName(id), key);
  }
 }
}

// Selects the two playable sides before the skirmish layout is applied. The world
// is authored with US/USSR owners; these map to the selected factions at runtime.
class ACR_FactionSelection
{
 protected static bool s_bApplied;
 protected static bool s_bWorldMapped;
 protected static bool s_bUAChecked;
 protected static bool s_bUAAvailable;
 protected static string s_sWest = "US";
 protected static string s_sEast = "USSR";

 // This unique flag prefab is shipped by Ukraine Armed Forces RHS and does not
 // exist in vanilla or RHS Status Quo. Its presence detects the US -> UA override.
 protected static const ResourceName UA_PROBE = "{E50B55ED23B3EB27}Prefabs/Props/Fabric/Flags/Flag_UA_FA.et";

 static bool UkraineModLoaded()
 {
  if (!s_bUAChecked)
  {
   s_bUAChecked = true;
   Resource probe = Resource.Load(UA_PROBE);
   s_bUAAvailable = probe && probe.IsValid();
  }
  return s_bUAAvailable;
 }

 // Before the setup phase ends (ACR_Setup) the pair isn't locked: West/East show the current
 // choice without applying it.
 static string West() { Apply(); if (!s_bApplied) { string w, e; Choice(w, e); return w; } return s_sWest; }
 static string East() { Apply(); if (!s_bApplied) { string w, e; Choice(w, e); return e; } return s_sEast; }
 static bool IsSelected(string key) { return key == West() || key == East(); }
 static bool Applied() { return s_bApplied; }
 static void Reset()
 {
  s_bApplied = false;
  s_bWorldMapped = false;
  s_PlayableSent.Clear();
  s_sWest = "US";
  s_sEast = "USSR";
 }

 // Sides that can be played: every faction in the world's faction manager (the world lists the
 // factions it can use, vanilla or from mods) whose catalogs have soldiers and groups. Any
 // faction mod works once a world's faction manager lists it; RHS/UA have extra fitting
 // (StandardPath, paint, officers, UA kit) on top of this.
 static void Candidates(notnull array<string> keys)
 {
  keys.Clear();
  FactionManager manager = GetGame().GetFactionManager();
  if (!manager) return;
  array<Faction> all = {};
  manager.GetFactionsList(all);
  foreach (Faction baseFaction : all)
  {
   SCR_Faction faction = SCR_Faction.Cast(baseFaction);
   if (!faction) continue;
   string key = faction.GetFactionKey();
   if (key == "CIV" || key.IsEmpty()) continue;
   if (!HasEntries(faction, EEntityCatalogType.CHARACTER) || !HasEntries(faction, EEntityCatalogType.GROUP)) continue;
   keys.Insert(key);
  }
 }

 protected static bool HasEntries(SCR_Faction faction, EEntityCatalogType type)
 {
  SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(type, false);
  if (!catalog) return false;
  array<SCR_EntityCatalogEntry> entries = {};
  catalog.GetEntityList(entries);
  return !entries.IsEmpty();
 }

 // A faction key as the world spells it, matched without case; "" if it isn't a candidate.
 static string Key(string wanted)
 {
  wanted.Trim();
  wanted.ToLower();
  array<string> keys = {};
  Candidates(keys);
  foreach (string key : keys)
  {
   string lower = key;
   lower.ToLower();
   if (lower == wanted) return key;
  }
  return "";
 }

 // The chosen pair from -acrFactions / the header / the setup screen: a legacy pair name
 // (vanilla, ru_us, ru_ua, us_ua) or two faction keys "WEST,EAST". Falls back to US/USSR.
 static string Choice(out string west, out string east)
 {
  string pair = "vanilla";
  string chosen;
  if (ACR_Options.Get("acrFactions", chosen) && chosen != "") pair = chosen;
  pair.ToLower();
  west = "US";
  east = "USSR";
  if (pair == "ru_us") { west = "RHS_USAF"; east = "RHS_AFRF"; }
  else if (pair == "ru_ua") { west = "US"; east = "RHS_AFRF"; }
  else if (pair == "us_ua") { west = "RHS_USAF"; east = "US"; }
  else if (pair != "vanilla")
  {
   string spaced = pair;
   spaced.Replace(":", ",");
   array<string> parts = {};
   spaced.Split(",", parts, true);
   if (parts.Count() == 2)
   {
    west = Key(parts[0]);
    east = Key(parts[1]);
   }
  }
  if ((pair == "ru_ua" || pair == "us_ua") && !UkraineModLoaded()) { west = "US"; east = "USSR"; }
  if (west.IsEmpty() || east.IsEmpty() || west == east || !HasFaction(west) || !HasFaction(east)) { west = "US"; east = "USSR"; }
  return pair;
 }
 static string DisplayName(string key)
 {
  Apply();
  if (key == "RHS_AFRF") return "RHS Russia";
  if (key == "RHS_USAF") return "RHS US";
  if (key == "US" && UkraineModLoaded())
  {
   return "UA";
  }
  return key;
 }

 // Only the pair shows on the deploy screen's side list. The playable flag isn't replicated:
 // the server sets it here and each client gets it from SyncClients (26 Oct: clients listed
 // FIA, a player picked it and found no spawn point).
 static void SetPlayable(string west, string east)
 {
  FactionManager manager = GetGame().GetFactionManager();
  if (!manager) return;
  array<Faction> all = {};
  manager.GetFactionsList(all);
  foreach (Faction baseFaction : all)
  {
   SCR_Faction faction = SCR_Faction.Cast(baseFaction);
   if (faction) faction.SetIsPlayable(faction.GetFactionKey() == west || faction.GetFactionKey() == east);
  }
 }

 protected static ref map<int, string> s_PlayableSent = new map<int, string>();

 // Server, every second once the pair is applied: each player's game learns the pair once.
 static void SyncClients()
 {
  if (!s_bApplied) return;
  string signature = s_sWest + "," + s_sEast;
  array<int> ids = {};
  GetGame().GetPlayerManager().GetPlayers(ids);
  array<int> gone = {};
  foreach (int known, string ignored : s_PlayableSent) if (!ids.Contains(known)) gone.Insert(known);
  foreach (int left : gone) s_PlayableSent.Remove(left);
  foreach (int id : ids)
  {
   string sent;
   if (s_PlayableSent.Find(id, sent) && sent == signature) continue;
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc) continue;
   s_PlayableSent.Set(id, signature);
   pc.ACR_SendPlayableSides(s_sWest, s_sEast);
  }
 }

 static bool HasFaction(string key)
 {
  return GetGame().GetFactionManager().GetFactionByKey(key) != null;
 }

 static void Apply()
 {
  if (!Replication.IsServer()) return;
  FactionManager manager = GetGame().GetFactionManager();
  if (!manager) return;
  // Setup phase: the host may still change the pair; it is applied at Start.
  if (!s_bApplied && !ACR_Setup.Started()) return;
  if (!s_bApplied)
  {
   s_bApplied = true;
   string pair = Choice(s_sWest, s_sEast);
   PrintFormat("[ACR] Faction choice '%1' -> %2 / %3", pair, s_sWest, s_sEast);
   SetPlayable(s_sWest, s_sEast);
   PrintFormat("[ACR] Playable faction pair: %1 / %2", s_sWest, s_sEast);
  }
  if (s_bWorldMapped || ACR_Base.s_All.IsEmpty()) return;
  s_bWorldMapped = true;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == "US") base.m_sOwner = s_sWest;
   else if (base.m_sOwner == "USSR") base.m_sOwner = s_sEast;
  }
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
  {
   if (!point) continue;
   if (point.GetFactionKey() == "US") point.SetFactionKey(s_sWest);
   else if (point.GetFactionKey() == "USSR") point.SetFactionKey(s_sEast);
  }
 }
}

modded class SCR_PlayerFactionAffiliationComponent
{
 // For the test harness.
 bool ACR_WouldAllow(Faction faction)
 {
  return CanRequestFaction_S(faction);
 }

 override protected bool CanRequestFaction_S(Faction faction)
 {
  string key = ACR_PlayerSide.Key();
  if (key != "" && faction && faction.GetFactionKey() != key) return false;
  // Only the match's two sides (a client with a stale side list could still ask for FIA);
  // a side the host forces with -acrPlayers stays allowed.
  if (key == "" && faction && ACR_FactionSelection.Applied() && !ACR_FactionSelection.IsSelected(faction.GetFactionKey())) return false;
  // Deployed once: on that side for the rest of the match (ACR_SpawnPoints side lock).
  if (!GetPlayerController()) return super.CanRequestFaction_S(faction);
  int playerId = GetPlayerController().GetPlayerId();
  string locked = ACR_SpawnPoints.LockedSide(playerId);
  if (locked != "" && faction && faction.GetFactionKey() != locked)
  {
   ACR_Manager manager = ACR_Manager.GetInstance();
   if (manager) manager.Notify(playerId, string.Format("You are on %1 for this match.", ACR_FactionSelection.DisplayName(locked)));
   return false;
  }
  return super.CanRequestFaction_S(faction);
 }
}

// Players choose their side, loadout and spawn point on the game's deploy screen (Jacob 26 Oct,
// GitHub #39; the worlds use SCR_MenuSpawnLogic). The menu opens once the host presses Start;
// the host option -acrPlayers still puts everyone on one side, and a player locked to a side
// (ACR_SpawnPoints) gets it back on reconnecting. Tests deploy straight away instead
// (ACR_TestHooks.AutoDeploySide).
[BaseContainerProps(category: "Respawn")]
modded class SCR_MenuSpawnLogic
{
 override protected bool GetForcedFaction(out Faction faction)
 {
  string key = ACR_PlayerSide.Key();
  if (key == "") return false;
  faction = GetGame().GetFactionManager().GetFactionByKey(key);
  return faction != null;
 }

 override protected void DoSpawn_S(int playerId)
 {
  PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(playerId);
  if (!pc) return;
  // Setup phase: players deploy once the host presses Start (ACR_Setup).
  if (!ACR_Setup.Started())
  {
   GetGame().GetCallqueue().CallLater(DoSpawn_S, 1000, false, playerId);
   return;
  }
  // A hosting player registers before the first frame, where the skirmish layout moves the
  // spawn points to its HQs: set it up now (11 Oct).
  if (Replication.IsServer()) ACR_Skirmish.ApplyOnce();
  if (ACR_AutoDeploy(playerId)) return;
  ACR_SpawnPoints.ApplyLock(playerId);
  super.DoSpawn_S(playerId);
 }

 override void OnPlayerEntityLost_S(int playerId)
 {
  // The command unit died while its player controls a soldier: ACR_Respawn sends a new one,
  // the deploy screen stays shut.
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
  if (pc && pc.IsPossessing() && ACR_Respawn.IsAlive(pc.GetControlledEntity())) return;
  if (!ACR_PlayerSide.AutoDeploySide().IsEmpty())
  {
   DoSpawn_S(playerId);
   return;
  }
  ACR_SpawnPoints.ApplyLock(playerId);
  super.OnPlayerEntityLost_S(playerId);
 }

 override protected void OnPlayerSpawnFailed_S(int playerId)
 {
  super.OnPlayerSpawnFailed_S(playerId);
  // Auto deploy retries (respawn timer still running, no spawn point yet).
  if (!ACR_PlayerSide.AutoDeploySide().IsEmpty()) GetGame().GetCallqueue().CallLater(DoSpawn_S, 1000, false, playerId);
 }

 // For the regression test.
 Faction ACR_ForcedFaction()
 {
  Faction faction;
  GetForcedFaction(faction);
  return faction;
 }

 // Tests: spawn on a side's best spawn point without the menu. Returns false when the menu is used.
 protected bool ACR_AutoDeploy(int playerId)
 {
  string side = ACR_PlayerSide.AutoDeploySide();
  if (side.IsEmpty()) return false;
  Faction faction = GetGame().GetFactionManager().GetFactionByKey(side);
  if (!faction) return false;
  SCR_PlayerFactionAffiliationComponent factionComponent = GetPlayerFactionComponent_S(playerId);
  if (factionComponent.GetAffiliatedFaction() != faction) factionComponent.RequestFaction(faction);
  if (factionComponent.GetAffiliatedFaction() != faction) { OnPlayerSpawnFailed_S(playerId); return true; }
  SCR_PlayerLoadoutComponent loadoutComponent = GetPlayerLoadoutComponent_S(playerId);
  SCR_FactionPlayerLoadout current = SCR_FactionPlayerLoadout.Cast(loadoutComponent.GetLoadout());
  if (!current || current.GetFactionKey() != side)
   loadoutComponent.RequestLoadout(GetGame().GetLoadoutManager().GetRandomFactionLoadout(faction));
  SCR_BasePlayerLoadout loadout = loadoutComponent.GetLoadout();
  SCR_SpawnPoint point = ACR_SpawnPoints.Best(side);
  if (!point) point = SCR_SpawnPoint.GetRandomSpawnPointForFaction(side);
  if (!loadout || !point) { OnPlayerSpawnFailed_S(playerId); return true; }
  SCR_SpawnPointSpawnData data = new SCR_SpawnPointSpawnData(loadout.GetLoadoutResource(), point.GetRplId());
  if (!GetPlayerRespawnComponent_S(playerId).CanSpawn(data)) { OnPlayerSpawnFailed_S(playerId); return true; }
  GetPlayerRespawnComponent_S(playerId).RequestSpawn(data);
  return true;
 }
}

// The scenario's auto-spawn names a fixed side (US on Arland): with a forced players'
// side, spawn players on that side instead (the faction request for US was refused).
// (The container attribute must be repeated on a modded config class, or the world's
// respawn component loads without its spawn logic.)
[BaseContainerProps(category: "Respawn")]
modded class SCR_AutoSpawnLogic
{
 override protected bool GetForcedFaction(out Faction faction)
 {
  string key = ACR_PlayerSide.Key();
  if (key != "")
  {
   faction = GetGame().GetFactionManager().GetFactionByKey(key);
   if (faction) return true;
  }
  if (!super.GetForcedFaction(faction)) return false;
  // The scenario names vanilla US or USSR; with a mod pair selected those sides aren't
  // playable, so a player forced onto one spawned as vanilla US at its old HQ (8 Oct).
  SCR_Faction forced = SCR_Faction.Cast(faction);
  if (forced && !forced.IsPlayable())
  {
   string mapped;
   if (forced.GetFactionKey() == "US") mapped = ACR_FactionSelection.West();
   else if (forced.GetFactionKey() == "USSR") mapped = ACR_FactionSelection.East();
   Faction selected;
   if (!mapped.IsEmpty()) selected = GetGame().GetFactionManager().GetFactionByKey(mapped);
   if (selected) faction = selected;
  }
  return true;
 }

 // Players who left keep a queued auto-spawn retry; the game's own retry then threw a
 // null exception every second (no player controller any more).
 override protected void DoSpawn_S(int playerId)
 {
  if (!GetGame().GetPlayerManager().GetPlayerController(playerId)) return;
  // Setup phase: players spawn once the host presses Start (ACR_Setup).
  if (!ACR_Setup.Started())
  {
   GetGame().GetCallqueue().CallLater(DoSpawn_S, 1000, false, playerId);
   return;
  }
  // A hosting player (listen server, singleplayer) registers before the first frame,
  // where the skirmish layout moves the spawn points to its HQs: set it up now, so the
  // first command unit doesn't spawn at the scenario's authored HQ (11 Oct).
  if (Replication.IsServer()) ACR_Skirmish.ApplyOnce();
  super.DoSpawn_S(playerId);
 }

 // For the faction test: the scenario's authored side.
 void ACR_SetForcedFaction(string key) { m_sForcedFaction = key; }

 // For the regression test.
 Faction ACR_ForcedFaction()
 {
  Faction faction;
  GetForcedFaction(faction);
  return faction;
 }
}
