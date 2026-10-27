// Where players deploy (server), chosen on the game's deploy map (SCR_MenuSpawnLogic, ACR_PlayerSide).
//  - Home points: the world's own player spawn points. They mark each side's HQ (IsHQ in
//    ACR_AICommander and ACR_LogisticsRules, the skirmish layout and the generator move them
//    there) and never move during a match. A home point is open while its side holds its HQ,
//    and always when the side has nowhere else left to deploy.
//  - Base points: one per base a side can deploy new units at (ACR_LogisticsRules.CanDeploy:
//    HQs, military sites and built FOBs by default), named after the base and closed while the
//    base is contested or its FOB is offline. A captured base's point changes sides with it.
//  - Side lock: a player who has deployed on a side stays on it for the match (reconnects
//    included), so nobody reads one side's map and then plays the other. Admins move a
//    player with the debug command `side`.
class ACR_SpawnPoints
{
 static const ResourceName PREFAB = "{5A2220DD0C16958B}Prefabs/MP/Spawning/SpawnPoint_US.et";
 static const int PRIORITY_HQ = 3;
 static const int PRIORITY_FOB = 2;
 static const int PRIORITY_BASE = 1;

 protected static ref map<ACR_Base, SCR_SpawnPoint> s_Points = new map<ACR_Base, SCR_SpawnPoint>();
 protected static ref map<SCR_SpawnPoint, ACR_Base> s_Bases = new map<SCR_SpawnPoint, ACR_Base>(); // reverse of s_Points
 protected static ref map<string, string> s_Locks = new map<string, string>(); // player identity -> side
 protected static bool s_bTimerSet;

 // One of ours (not a world-placed home point): readers that find HQs by spawn point skip these.
 static bool IsBasePoint(SCR_SpawnPoint point)
 {
  return point && s_Bases.Contains(point);
 }

 static void Reset()
 {
  foreach (ACR_Base base, SCR_SpawnPoint point : s_Points) if (point) SCR_EntityHelper.DeleteEntityAndChildren(point);
  s_Points.Clear();
  s_Bases.Clear();
  s_Locks.Clear();
  s_bTimerSet = false;
 }

 protected static bool Eligible(ACR_Base base, string side)
 {
  if (!base || base.m_bRetired || base.m_sOwner != side || base.m_bContested) return false;
  if (base.m_bBuilt && !base.FobOnline()) return false;
  return ACR_LogisticsRules.CanDeploy(base);
 }

 protected static bool IsPlayableSide(string key)
 {
  return key != "" && ACR_FactionSelection.IsSelected(key);
 }

 // The base a home point stands at (its side's HQ), or null.
 protected static ACR_Base HomeBase(SCR_SpawnPoint point)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired) continue;
   float distance = vector.DistanceXZ(point.GetOrigin(), base.GetOrigin());
   if (distance <= base.m_fRadius + 60 && distance < bestDistance) { bestDistance = distance; best = base; }
  }
  return best;
 }

 // Every second on the server, once the match has started.
 static void Tick(ACR_Manager manager)
 {
  if (!Replication.IsServer() || !ACR_Setup.Started()) return;
  ACR_FactionSelection.SyncClients(); // the deploy screen's side list
  if (ACR_Base.s_All.IsEmpty()) return;
  SetTimer(manager);
  // Base points: create, move between sides, open and close.
  array<ACR_Base> gone = {};
  foreach (ACR_Base base, SCR_SpawnPoint point : s_Points) if (!base || base.m_bRetired || !point) gone.Insert(base);
  foreach (ACR_Base dead : gone)
  {
   SCR_SpawnPoint point = s_Points.Get(dead);
   if (point) SCR_EntityHelper.DeleteEntityAndChildren(point);
   s_Points.Remove(dead);
  }
  // Rebuilt every tick: a point deleted by something else leaves no stale key behind.
  s_Bases.Clear();
  foreach (ACR_Base owner, SCR_SpawnPoint own : s_Points) s_Bases.Set(own, owner);
  map<string, int> open = new map<string, int>();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired) continue;
   bool homeHere = HasHomePoint(base);
   bool eligible = IsPlayableSide(base.m_sOwner) && Eligible(base, base.m_sOwner);
   SCR_SpawnPoint point;
   s_Points.Find(base, point);
   if (homeHere)
   {
    // The HQ's own home point does the job; no second point beside it.
    if (point) point.SetSpawnPointEnabled_S(false);
    continue;
   }
   if (!point && eligible) point = Create(manager, base);
   if (!point) continue;
   if (eligible && point.GetFactionKey() != base.m_sOwner) point.SetFactionKey(base.m_sOwner);
   point.SetSpawnPointEnabled_S(eligible);
   if (eligible) open.Set(base.m_sOwner, open.Get(base.m_sOwner) + 1);
  }
  // Home points: open while the side holds its HQ, or when it has nowhere else.
  foreach (SCR_SpawnPoint home : SCR_SpawnPoint.GetSpawnPoints())
  {
   if (!home || IsBasePoint(home)) continue;
   string side = home.GetFactionKey();
   if (!IsPlayableSide(side)) continue;
   ACR_Base hq = HomeBase(home);
   bool held = hq && Eligible(hq, side);
   bool enable = held || open.Get(side) == 0;
   if (hq && home.GetSpawnPointName() != hq.m_sName) home.SetSpawnPointName(hq.m_sName);
   home.SetPriority(PRIORITY_HQ);
   home.SetSpawnPointEnabled_S(enable);
  }
  LockDeployed();
 }

 protected static bool HasHomePoint(ACR_Base base)
 {
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
  {
   if (!point || IsBasePoint(point) || point.GetFactionKey() != base.m_sOwner) continue;
   if (vector.DistanceXZ(point.GetOrigin(), base.GetOrigin()) <= base.m_fRadius + 60) return true;
  }
  return false;
 }

 protected static SCR_SpawnPoint Create(ACR_Manager manager, ACR_Base base)
 {
  vector at = manager.ClearSpotNear(base, 4, 14, 1.5);
  SCR_SpawnPoint point = SCR_SpawnPoint.Cast(ACR_Manager.SpawnPrefab(PREFAB, at));
  if (!point) return null;
  point.SetFactionKey(base.m_sOwner);
  point.SetSpawnPointName(base.m_sName);
  int priority = PRIORITY_BASE;
  if (base.m_bBuilt) priority = PRIORITY_FOB;
  point.SetPriority(priority);
  s_Points.Set(base, point);
  s_Bases.Set(point, base);
  PrintFormat("[ACR] SPAWN POINT %1 for %2 at %3", base.m_sName, base.m_sOwner, at);
  return point;
 }

 // The deploy screen's respawn timer is the host's respawn delay (-acrRespawnSeconds).
 protected static void SetTimer(ACR_Manager manager)
 {
  if (s_bTimerSet) return;
  s_bTimerSet = true;
  SCR_RespawnTimerComponent timer = SCR_RespawnTimerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_RespawnTimerComponent));
  if (!timer) { Print("[ACR] No respawn timer on the game mode: players respawn without a delay"); return; }
  timer.SetRespawnTime(Math.Max(0, manager.m_iRespawnSeconds));
  PrintFormat("[ACR] Respawn delay %1 s", manager.m_iRespawnSeconds);
 }

 // The open spawn point a side's new command unit uses when nobody picks one (the player is
 // controlling a soldier): the HQ first, then FOBs, then the base nearest the HQ.
 static SCR_SpawnPoint Best(string side)
 {
  SCR_SpawnPoint home;
  SCR_SpawnPoint best;
  float bestScore = -float.MAX;
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
  {
   if (!point || point.GetFactionKey() != side || !point.IsSpawnPointEnabled()) continue;
   if (!IsBasePoint(point) && !home) home = point;
   float score = point.GetPriority() * 100000;
   if (home) score -= vector.DistanceXZ(point.GetOrigin(), home.GetOrigin());
   if (score > bestScore) { bestScore = score; best = point; }
  }
  return best;
 }

 // The base a spawn point belongs to (home points: the base at it), or null.
 static ACR_Base BaseOf(SCR_SpawnPoint point)
 {
  if (!point) return null;
  ACR_Base base;
  if (s_Bases.Find(point, base)) return base;
  return HomeBase(point);
 }

 // ---------------------------------------------------------------- side lock

 static string PlayerKey(int playerId)
 {
  string id = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
  // Without a backend (local tests, LAN) the identity is empty or all zeros.
  if (id.IsEmpty() || id.StartsWith("00000000-0000")) id = "name:" + GetGame().GetPlayerManager().GetPlayerName(playerId);
  return id;
 }

 // The side this player is locked to, or "".
 static string LockedSide(int playerId)
 {
  string side;
  s_Locks.Find(PlayerKey(playerId), side);
  return side;
 }

 // Admin move (debug command `side`): the player belongs to that side from their next deploy.
 static void Assign(int playerId, string side)
 {
  s_Locks.Set(PlayerKey(playerId), side);
  ApplyLock(playerId);
 }

 // A player who isn't alive in the field gets their locked side back (reconnect, admin move).
 static void ApplyLock(int playerId)
 {
  string side = LockedSide(playerId);
  if (side.IsEmpty() || ACR_PlayerSide.Key() != "") return;
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
  if (!pc || ACR_Respawn.IsAlive(pc.GetMainEntity())) return;
  Faction faction = GetGame().GetFactionManager().GetFactionByKey(side);
  SCR_PlayerFactionAffiliationComponent affiliation = SCR_PlayerFactionAffiliationComponent.Cast(pc.FindComponent(SCR_PlayerFactionAffiliationComponent));
  if (!faction || !affiliation || affiliation.GetAffiliatedFaction() == faction) return;
  affiliation.SetFaction_S(faction);
  PrintFormat("[ACR] %1 back on %2 (side lock)", GetGame().GetPlayerManager().GetPlayerName(playerId), side);
 }

 // Players alive in the field are locked to the side they deployed on.
 protected static void LockDeployed()
 {
  array<int> ids = {};
  GetGame().GetPlayerManager().GetPlayers(ids);
  foreach (int id : ids)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc || !ACR_Respawn.IsAlive(pc.GetMainEntity())) continue;
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   if (!faction || !IsPlayableSide(faction.GetFactionKey())) continue;
   string identity = PlayerKey(id);
   if (s_Locks.Contains(identity)) continue; // locked already (or moved by an admin)
   s_Locks.Set(identity, faction.GetFactionKey());
   PrintFormat("[ACR] %1 locked to %2 for this match", GetGame().GetPlayerManager().GetPlayerName(id), faction.GetFactionKey());
  }
 }
}

// Deploy screen (client): log what the spawn point list is built from, so a list that comes up
// empty in play shows why (26 Oct: Jacob found no spawn points on either side).
modded class SCR_SpawnPointRequestUIComponent
{
 override void ShowAvailableSpawnPoints(Faction faction)
 {
  super.ShowAvailableSpawnPoints(faction);
  if (!faction) return;
  string known;
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
  {
   if (!point) continue;
   known += string.Format(" %1/%2/%3", point.GetFactionKey(), point.GetSpawnPointName(), point.IsSpawnPointEnabled());
  }
  PrintFormat("[ACR] DEPLOY list for %1 (playable %2): %3 of theirs; all known:%4", faction.GetFactionKey(), SCR_Faction.Cast(faction) && SCR_Faction.Cast(faction).IsPlayable(), SCR_SpawnPoint.GetSpawnPointsForFaction(faction.GetFactionKey()).Count(), known);
 }
}
