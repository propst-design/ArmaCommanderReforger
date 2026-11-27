// Housekeeping for long sessions (server). Every body and every destroyed
// vehicle is noted when it dies; after a while, and only when no player is
// close enough to notice, the body or wreck is removed. Nearby loose equipment
// is left alone because its ownership can't be proven. Also caps AI soldiers.
class ACR_Cleanup : Managed
{
 float m_fBodySeconds = 240;                  // bodies stay at least this long
 float m_fWreckSeconds = 420;                 // destroyed vehicles
 static const float BODY_PRIVACY = 120;       // m: never removed with a player this close
 static const float WRECK_PRIVACY = 200;
 static const int MAX_BODIES = 60;            // over this, the oldest go early (still out of sight)
 static const int MAX_WRECKS = 12;
 int m_iCheckMs = 10000;

 protected ACR_Manager m_Manager;
 protected ref array<IEntity> m_Bodies = {};
 protected ref array<float> m_BodyTimes = {};
 protected ref array<IEntity> m_Wrecks = {};
 protected ref array<float> m_WreckTimes = {};
 protected ref array<IEntity> m_WatchedVehicles = {};
 protected ref array<IEntity> m_Litter = {};       // e.g. spent launchers thrown away
 protected ref array<float> m_LitterTimes = {};
 protected float m_fNextCheck;
 int m_iRemovedBodies;
 int m_iRemovedWrecks;

 void ACR_Cleanup(ACR_Manager manager)
 {
  m_Manager = manager;
 }

 // Called from the game mode when anything controllable is destroyed.
 void OnDestroyed(IEntity entity)
 {
  if (!entity) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (ChimeraCharacter.Cast(entity))
  {
   if (m_Bodies.Contains(entity)) return;
   m_Bodies.Insert(entity);
   m_BodyTimes.Insert(now);
   return;
  }
  if (Vehicle.Cast(entity)) AddWreck(entity, now);
 }

 // Things soldiers throw away (spent launchers): removed after a few minutes out of sight.
 void AddLitter(IEntity item)
 {
  if (!item || m_Litter.Contains(item)) return;
  m_Litter.Insert(item);
  m_LitterTimes.Insert(GetGame().GetWorld().GetWorldTime());
 }

 // Vehicles we spawn (purchases, supports) are watched for destruction too,
 // since not every vehicle reports through the game mode.
 void Watch(IEntity vehicle)
 {
  if (vehicle && !m_WatchedVehicles.Contains(vehicle)) m_WatchedVehicles.Insert(vehicle);
 }

 protected void AddWreck(IEntity vehicle, float now)
 {
  if (m_Wrecks.Contains(vehicle)) return;
  m_Wrecks.Insert(vehicle);
  m_WreckTimes.Insert(now);
 }

 void Tick()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextCheck) return;
  m_fNextCheck = now + m_iCheckMs;
  for (int w = m_WatchedVehicles.Count() - 1; w >= 0; w--)
  {
   IEntity watched = m_WatchedVehicles[w];
   if (!watched) { m_WatchedVehicles.Remove(w); continue; }
   if (ACR_Logistics.Health(watched) <= 0)
   {
    AddWreck(watched, now);
    m_WatchedVehicles.Remove(w);
   }
  }
  array<vector> players = {};
  PlayerPositions(players);
  Sweep(m_Bodies, m_BodyTimes, m_fBodySeconds, BODY_PRIVACY, MAX_BODIES, players, now, true);
  Sweep(m_Wrecks, m_WreckTimes, m_fWreckSeconds, WRECK_PRIVACY, MAX_WRECKS, players, now, false);
  Sweep(m_Litter, m_LitterTimes, 180, 60, 60, players, now, false, true);
 }

 protected void PlayerPositions(notnull array<vector> result)
 {
  array<int> ids = {};
  GetGame().GetPlayerManager().GetPlayers(ids);
  foreach (int id : ids)
  {
   IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (body) result.Insert(body.GetOrigin());
  }
 }

 protected bool Watched(vector at, float privacy, array<vector> players)
 {
  foreach (vector p : players)
   if (vector.Distance(p, at) < privacy) return true;
  return false;
 }

 protected static bool HasLivingOccupant(IEntity vehicle)
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return false;
  array<IEntity> occupants = {};
  compartments.GetOccupants(occupants);
  foreach (IEntity occupant : occupants)
   if (occupant && ACR_Respawn.IsAlive(occupant)) return true;
  return false;
 }

 // Oldest first (they were added in order of death).
 protected void Sweep(array<IEntity> list, array<float> times, float minSeconds, float privacy, int cap, array<vector> players, float now, bool bodies, bool litter = false)
 {
  int excess = list.Count() - cap;
  for (int i = 0; i < list.Count(); i++)
  {
   IEntity entity = list[i];
   if (!entity)
   {
    list.RemoveOrdered(i);
    times.RemoveOrdered(i);
    i--;
    excess--;
    continue;
   }
   bool old = now - times[i] >= minSeconds * 1000;
   bool early = excess > 0 && now - times[i] >= 30000; // piling up: go sooner (never in the first 30 s)
   if (!old && !early) continue;
   // A body someone climbed back into (revived) or a vehicle still crewed is left alone.
   if (bodies && ACR_Respawn.IsAlive(entity)) { list.RemoveOrdered(i); times.RemoveOrdered(i); i--; continue; }
   // Litter someone picked up again is theirs now.
   if (litter && entity.GetParent()) { list.RemoveOrdered(i); times.RemoveOrdered(i); i--; continue; }
   if (!bodies && !litter && HasLivingOccupant(entity)) continue;
   if (Watched(entity.GetOrigin(), privacy, players)) continue;
   SCR_EntityHelper.DeleteEntityAndChildren(entity);
   if (bodies)
   {
    m_iRemovedBodies++;
   }
   else if (!litter) m_iRemovedWrecks++;
   list.RemoveOrdered(i);
   times.RemoveOrdered(i);
   i--;
   excess--;
  }
 }

 // ---------------------------------------------------------------- AI cap

 // Living soldiers of a faction in squads.
 int SoldiersOf(string faction)
 {
  int count;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == faction) count += squad.CountAlive() + squad.CountDormant();
  return count;
 }

 string Status()
 {
  return string.Format("tracking %1 bodies, %2 wrecks; removed %3 bodies, %4 wrecks", m_Bodies.Count(), m_Wrecks.Count(), m_iRemovedBodies, m_iRemovedWrecks);
 }
}
