// srv:pathprobe (Everon, about 6 min): can foot squads reach Gallows Base's centre?
// Everon night soak 26 Oct: two squads given grabs there had their move waypoint dropped by
// the engine every 10 s ("nearest walkable point, 0 m off") and gave up; the base stayed
// unclaimed for the whole first hour. Four fire teams start 180 m out (the ride drop-off to
// the north-west, east, south, west), are ordered to the centre and logged every 10 s (PROBE
// lines); the objects round the centre are listed once. First run (26 Oct): the drop-off,
// east and west approaches reach it; from the south the engine drops the move every 10 s (the
// centre is a wooded yard walled with concrete panels). Checks: each squad either gets inside
// the 60 m arrival distance or gives up as unreachable (so the AI drops the attack), and the
// drop-off approach gets in. #30: the south squad goes round (ACR_Squad.DetourRound) and gets in too.
class ACR_TestPathProbe
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 static const vector DROP = "5292.18 34.6188 10703.3"; // the ride drop-off of the soak's first grab
 static const float START_M = 180;
 static const float ARRIVED_M = 60;
 static const int END_S = 330; // #30: the south squad's way round (back out, flank, in) is about 600 m
 // 4 and 5 start beside the drop-off one: "Probe Rode" with a truck 1.3 km away counted as its
 // own (as after a ride), "Probe Plain" without (does a stale group vehicle break its moves?).
 static const int COUNT = 6;
 static const ResourceName TRUCK = "{92264FF932676C13}Prefabs/Vehicles/Wheeled/M923A1/M923A1_ammo.et";
 static const vector TRUCK_AT = "4960 28.4 11880"; // by the Airbase
 static ref array<string> s_aNames = {"Probe Drop", "Probe East", "Probe South", "Probe West", "Probe Rode", "Probe Plain"};
 static ref array<float> s_aBest = {100000, 100000, 100000, 100000, 100000, 100000};
 static ref array<bool> s_aStarted = {false, false, false, false, false, false};
 static IEntity s_Truck;
 static vector s_Centre;
 static ref map<string, int> s_mNear = new map<string, int>();

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  foreach (ACR_Squad peaceful : manager.m_Squads)
   if (peaceful.m_Group && peaceful.m_Group.GetGroupUtilityComponent()) peaceful.m_Group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
  if (second == 4)
  {
   ACR_Base gallows;
   foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sName == "Gallows Base") gallows = base;
   if (!gallows) { ACR_TestRegress.Abort("pathprobe", "no Gallows Base on this map (needs Everon)"); return; }
   s_Centre = gallows.GetOrigin();
   Survey();
  }
  if (second == 6)
  {
   manager.m_sSpawningFaction = "US";
   for (int i = 0; i < COUNT; i++)
   {
    vector at = Start(i);
    manager.SpawnUnit(s_aNames[i], FIRE_TEAM, "", false, "", at, at);
   }
   manager.m_sSpawningFaction = "";
   EntitySpawnParams parameters = new EntitySpawnParams();
   parameters.TransformMode = ETransformMode.WORLD;
   parameters.Transform[3] = ACR_Manager.Ground(TRUCK_AT);
   s_Truck = GetGame().SpawnEntityPrefab(Resource.Load(TRUCK), GetGame().GetWorld(), parameters);
  }
  if (second == 13)
  {
   ACR_Squad rode = Squad(manager, s_aNames[4]);
   SCR_AIGroupUtilityComponent utility;
   if (rode && rode.m_Group) utility = SCR_AIGroupUtilityComponent.Cast(rode.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
   SCR_AIVehicleUsageComponent usage;
   if (s_Truck) usage = SCR_AIVehicleUsageComponent.Cast(s_Truck.FindComponent(SCR_AIVehicleUsageComponent));
   if (utility && usage) utility.AddUsableVehicle(usage);
   if (rode) PrintFormat("[ACR] PROBE %1 group vehicles %2 (truck %3 m away)", s_aNames[4], rode.GroupVehicleCount(), (int)vector.DistanceXZ(TRUCK_AT, Start(4)));
  }
  if (second == 14)
  {
   for (int j = 0; j < COUNT; j++)
   {
    ACR_Squad squad = Squad(manager, s_aNames[j]);
    if (!squad) continue;
    s_aStarted[j] = true;
    PrintFormat("[ACR] PROBE %1 from %2 (%3 m out): %4", s_aNames[j], PositionOf(squad), (int)vector.DistanceXZ(PositionOf(squad), s_Centre), squad.MoveTo(s_Centre));
   }
  }
  if (second > 14)
  {
   for (int k = 0; k < COUNT; k++)
   {
    ACR_Squad probe = Squad(manager, s_aNames[k]);
    if (!probe) continue;
    float d = vector.DistanceXZ(PositionOf(probe), s_Centre);
    s_aBest[k] = Math.Min(s_aBest[k], d);
    if (second % 10 == 0) PrintFormat("[ACR] PROBE t=%1 %2: %3 m from the centre (best %4), order '%5', waypoint %6", second, s_aNames[k], (int)d, (int)s_aBest[k], probe.m_Order, probe.m_Group && probe.m_Group.GetCurrentWaypoint() != null);
   }
  }
  if (second == END_S)
  {
   int reached = 0;
   for (int n = 0; n < COUNT; n++)
   {
    ACR_Squad done = Squad(manager, s_aNames[n]);
    if (!s_aStarted[n] || !done) { PrintFormat("[ACR] PROBE %1 never started", s_aNames[n]); continue; }
    bool inside = s_aBest[n] < ARRIVED_M;
    if (inside) reached++;
    PrintFormat("[ACR] PROBE DIAG %1: best %2 m, order '%3', gave up unreachable %4, group vehicles %5", s_aNames[n], (int)s_aBest[n], done.m_Order, done.GaveUpUnreachable(), done.GroupVehicleCount());
    // Either it gets there, or it says it can't (the AI then drops that attack): never a
    // squad holding short of the base as though it had arrived.
    ACR_TestRegress.Check(string.Format("pathprobe: %1 arrives or reports it can't", s_aNames[n]), inside != done.GaveUpUnreachable(), string.Format("best %1 m, order '%2'", (int)s_aBest[n], done.m_Order));
   }
   // 26 Oct: reachable from the drop-off (north-west), east and west; the south side isn't.
   ACR_TestRegress.Check("pathprobe: the drop-off approach the AI's rides use reaches the centre", s_aBest[0] < ARRIVED_M, string.Format("best %1 m", (int)s_aBest[0]));
   ACR_TestRegress.Check("pathprobe: the south squad goes round and reaches the centre (#30)", s_aBest[2] < ARRIVED_M, string.Format("best %1 m", (int)s_aBest[2]));
   PrintFormat("[ACR] PROBE DIAG %1 of %2 probes reach Gallows Base's centre (rode %3 m, plain %4 m)", reached, COUNT, (int)s_aBest[4], (int)s_aBest[5]);
   ACR_TestRegress.Complete("pathprobe", 8);
  }
 }

 // 180 m out from the centre: the drop-off direction first, then east, south, west.
 static vector Start(int i)
 {
  vector dir = Vector(1, 0, 0);
  if (i == 0 || i >= 4) { dir = DROP - s_Centre; dir[1] = 0; dir = dir.Normalized(); }
  if (i >= 4) return ACR_Manager.Ground(s_Centre + dir * START_M + Vector(dir[2], 0, -dir[0]) * (i - 3) * 12);
  else if (i == 2) dir = Vector(0, 0, -1);
  else if (i == 3) dir = Vector(-1, 0, 0);
  return ACR_Manager.Ground(s_Centre + dir * START_M);
 }

 static vector PositionOf(ACR_Squad squad)
 {
  IEntity anchor = squad.GetAnchor();
  if (anchor) return anchor.GetOrigin();
  return vector.Zero;
 }

 static ACR_Squad Squad(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == name) return squad;
  return null;
 }

 // What stands within 45 m of the centre (prefab name counts), to see why it can't be reached.
 static void Survey()
 {
  s_mNear.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(s_Centre, 45, Collect);
  PrintFormat("[ACR] PROBE centre %1, ground %2", s_Centre, GetGame().GetWorld().GetSurfaceY(s_Centre[0], s_Centre[2]));
  foreach (string name, int count : s_mNear) PrintFormat("[ACR] PROBE near centre: %1 x%2", name, count);
 }

 static bool Collect(IEntity e)
 {
  if (!e.GetPrefabData() || e.GetParent()) return true;
  string name = FilePath.StripPath(e.GetPrefabData().GetPrefabName());
  if (name.IsEmpty()) return true;
  s_mNear.Set(name, s_mNear.Get(name) + 1);
  return true;
 }
}
