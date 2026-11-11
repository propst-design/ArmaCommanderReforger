// -acrDriveRoutesTest 1 (server, ~8 min): BBB's scripted driver against the game's own (Phase 1 kill
// test, 10 Nov 2026). The same ROUTES road trips on whatever world the preset loads, picked from the
// map's road network the same way every run: road points 1.0-1.6 km apart whose road route is at most
// 1.6x the straight line, starts at least 400 m apart. One M923 truck per route, driven by the game
// (preset driveroutes*) or by BBB (driveroutes*bbb: -bbbDriver 1). Per truck: arrived, time, metres,
// stops, crashes (from over 5 m/s to under 1 m/s within one second), ACA stuck counts, BBB back-outs.
// Compare A and B of the same world: the kill test is arrival at least as good, crashes at most 1 per 10 km.
class ACR_TestDriveRoutes
{
 static const int ROUTES = 4;
 static const float MIN_TRIP = 1000;
 static const float MAX_TRIP = 1600;
 static const int LIMIT_S = 420;
 static ref array<vector> s_aFrom;
 static ref array<vector> s_aTo;
 static ref array<ref ACR_DriveRouteRun> s_aRuns;
 static int s_iStart;
 static bool s_bDone;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   s_aFrom = {};
   s_aTo = {};
   s_aRuns = {};
   ACR_FactionSettings east = ACR_FactionSettings.Find(ACR_FactionSelection.East());
   if (east) east.m_bAICommander = false;
   ACR_FactionSettings west = ACR_FactionSettings.Find(ACR_FactionSelection.West());
   if (west) west.m_bAICommander = false;
   // The B run: the watcher only passes -acr...Test flags, so the test switches BBB's driver on.
   if (System.IsCLIParam("acrDriveRoutesBBBTest")) BBB_Drive.s_bOn = true;
   PrintFormat("[ACR] DRIVEROUTES scripted driver %1", BBB_Drive.s_bOn);
  }
  // Driving, not fighting: anyone not on the trucks' side is removed (the test range has garrisons).
  if (tick % 5 == 2) ClearOthers(h);
  if (tick == 3)
  {
   PickRoutes();
   PrintFormat("[ACR] DRIVEROUTES %1 routes", s_aFrom.Count());
   if (s_aFrom.IsEmpty()) { ACR_TestRegress.Abort("driveroutes", "no routes on this map"); return; }
   for (int i = 0; i < s_aFrom.Count(); i++)
   {
    vector dir = s_aTo[i] - s_aFrom[i];
    dir[1] = 0;
    dir.Normalize();
    manager.SpawnUnit(string.Format("Route Truck %1", i + 1), "", "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et", true, "", s_aFrom[i], s_aFrom[i] + dir * 8);
   }
  }
  if (tick == 14)
  {
   for (int j = 0; j < s_aFrom.Count(); j++)
   {
    ACR_DriveRouteRun run = new ACR_DriveRouteRun();
    run.m_iRoute = j + 1;
    run.m_Squad = h.NamedSquad(manager, string.Format("Route Truck %1", j + 1));
    if (run.m_Squad) run.m_Vehicle = ACR_Transport.CrewedVehicle(run.m_Squad);
    run.m_vGoal = s_aTo[j];
    if (run.m_Vehicle)
    {
     run.m_vLast = run.m_Vehicle.GetOrigin();
     run.m_Squad.MoveTo(s_aTo[j]);
    }
    s_aRuns.Insert(run);
    PrintFormat("[ACR] DRIVEROUTES route %1: %2 -> %3 (%4 m straight), truck %5", j + 1, s_aFrom[j], s_aTo[j], (int)vector.DistanceXZ(s_aFrom[j], s_aTo[j]), run.m_Vehicle != null);
   }
   s_iStart = tick;
  }
  if (tick <= 14 || s_bDone || !s_aRuns) return;
  int open = 0;
  foreach (ACR_DriveRouteRun r : s_aRuns)
  {
   r.Sample(tick - s_iStart);
   if (r.m_iArrived < 0 && r.m_Vehicle) open++;
  }
  if (tick % 15 == 0)
  {
   foreach (ACR_DriveRouteRun line : s_aRuns) PrintFormat("[ACR] DRIVEROUTES t=%1 %2", tick - s_iStart, line.Line());
   PrintFormat("[ACR] DRIVEROUTES t=%1 BBB started %2 arrived %3 handed back %4 (last: %5) back-outs %6 guard slow-downs %7, %8 us/frame", tick - s_iStart, BBB_Drive.s_iStarted, BBB_Drive.s_iArrived, BBB_Drive.s_iHandedBack, BBB_Drive.s_sLastEnd, BBB_Drive.s_iRecoveries, BBB_Drive.s_iGuardStops, FrameUs());
  }
  if (open > 0 && tick - s_iStart < LIMIT_S) return;
  s_bDone = true;
  int arrived = 0;
  int crashes = 0;
  float metres = 0;
  float secs = 0;
  foreach (ACR_DriveRouteRun f : s_aRuns)
  {
   PrintFormat("[ACR] DRIVEROUTES RESULT %1", f.Line());
   if (f.m_iArrived >= 0) { arrived++; secs += f.m_iArrived; }
   crashes += f.m_iCrashes;
   metres += f.m_fDriven;
  }
  string mode = "game driver";
  if (BBB_Drive.s_bOn) mode = "BBB driver";
  string summary = string.Format("%1: %2/%3 arrived, %4 km driven, %5 crashes, avg trip %6 s; BBB started %7 arrived %8 handed back %9", mode, arrived, s_aRuns.Count(), metres / 1000, crashes, secs / Math.Max(1, arrived), BBB_Drive.s_iStarted, BBB_Drive.s_iArrived, BBB_Drive.s_iHandedBack);
  Print("[ACR] DRIVEROUTES SUMMARY " + summary);
  PrintFormat("[ACR] DRIVEROUTES SUMMARY BBB back-outs %1, guard slow-downs %2, %3 us/frame", BBB_Drive.s_iRecoveries, BBB_Drive.s_iGuardStops, FrameUs());
  ACR_TestRegress.Check("driveroutes: every route got a crewed truck with an order", s_aRuns.Count() == s_aFrom.Count() && AllCrewed(), summary);
  if (BBB_Drive.s_bOn) ACR_TestRegress.Check("driveroutes: BBB took the wheel on every route", BBB_Drive.s_iStarted >= s_aRuns.Count(), summary);
  else ACR_TestRegress.Check("driveroutes: BBB stayed out with -bbbDriver off", BBB_Drive.s_iStarted == 0, summary);
  ACR_TestRegress.Complete("driveroutes", 2);
 }

 static int s_iCleared;
 static void ClearOthers(ACR_TestHarness h)
 {
  AIWorld world = GetGame().GetAIWorld();
  if (!world) return;
  string ours = ACR_FactionSelection.West();
  array<AIAgent> agents = {};
  world.GetAIAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(body.FindComponent(FactionAffiliationComponent));
   if (!aff || !aff.GetAffiliatedFaction() || aff.GetAffiliatedFaction().GetFactionKey() == ours) continue;
   h.Kill(body);
   s_iCleared++;
  }
  if (s_iCleared > 0 && h.m_Ticks % 30 == 2) PrintFormat("[ACR] DRIVEROUTES cleared %1 others so far", s_iCleared);
 }

 static float FrameUs()
 {
  if (BBB_Drive.s_iFrames <= 0) return 0;
  return BBB_Drive.s_iFrameMs * 1000.0 / BBB_Drive.s_iFrames;
 }

 static bool AllCrewed()
 {
  foreach (ACR_DriveRouteRun r : s_aRuns) if (!r.m_Vehicle) return false;
  return true;
 }

 // The same routes every run: road points sorted by position, paired greedily.
 static void PickRoutes()
 {
  ChimeraAIWorld ai = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!ai || !ai.GetRoadNetworkManager()) return;
  vector mins, maxs;
  GetGame().GetWorld().GetBoundBox(mins, maxs);
  array<BaseRoad> roads = {};
  ai.GetRoadNetworkManager().GetRoadsInAABB(mins, maxs, roads);
  array<vector> cand = {};
  array<vector> pts = {};
  foreach (BaseRoad road : roads)
  {
   pts.Clear();
   road.GetPoints(pts);
   if (pts.Count() < 2 || road.GetWidth() < 4) continue;
   cand.Insert(pts[pts.Count() / 2]);
  }
  // Sort by x then z: the same order every run whatever the engine returns.
  for (int i = 1; i < cand.Count(); i++)
  {
   vector c = cand[i];
   int j = i - 1;
   while (j >= 0)
   {
    vector o = cand[j];
    if (o[0] < c[0] || (o[0] == c[0] && o[2] <= c[2])) break;
    cand[j + 1] = o;
    j--;
   }
   cand[j + 1] = c;
  }
  int stride = Math.Max(1, cand.Count() / 40);
  for (int a = 0; a < cand.Count() && s_aFrom.Count() < ROUTES; a += stride)
  {
   vector from = cand[a];
   if (Near(from, s_aFrom, 400) || Near(from, s_aTo, 400) || GetGame().GetWorld().GetSurfaceY(from[0], from[2]) < 1) continue;
   for (int b = cand.Count() - 1; b >= 0; b -= stride)
   {
    vector to = cand[b];
    float d = vector.DistanceXZ(from, to);
    if (d < MIN_TRIP || d > MAX_TRIP || Near(to, s_aTo, 400) || Near(to, s_aFrom, 400) || GetGame().GetWorld().GetSurfaceY(to[0], to[2]) < 1) continue;
    array<vector> legs = {};
    int nodes, millis;
    if (!BBB_Roads.Route(from, to, legs, nodes, millis) || legs.IsEmpty()) continue;
    if (BBB_Roads.s_fLastLength > d * 1.6) continue;
    s_aFrom.Insert(ACR_Manager.Ground(from));
    s_aTo.Insert(ACR_Manager.Ground(to));
    break;
   }
  }
 }

 static bool Near(vector p, array<vector> list, float range)
 {
  foreach (vector q : list) if (vector.DistanceXZ(p, q) < range) return true;
  return false;
 }
}

class ACR_DriveRouteRun : Managed
{
 int m_iRoute;
 ACR_Squad m_Squad;
 IEntity m_Vehicle;
 vector m_vGoal;
 vector m_vLast;
 float m_fDriven;
 float m_fLastSpeed;
 float m_fTop;
 int m_iArrived = -1;
 int m_iStops;
 int m_iCrashes;
 bool m_bRolling;

 void Sample(int t)
 {
  if (!m_Vehicle || m_iArrived >= 0) return;
  Physics phys = m_Vehicle.GetPhysics();
  float speed = 0;
  if (phys) speed = phys.GetVelocity().Length();
  m_fTop = Math.Max(m_fTop, speed);
  m_fDriven += vector.DistanceXZ(m_vLast, m_Vehicle.GetOrigin());
  m_vLast = m_Vehicle.GetOrigin();
  if (m_fLastSpeed > 5 && speed < 1)
  {
   m_iCrashes++;
   PrintFormat("[ACR] DRIVEROUTES route %1: CRASH at t=%2 (%3 -> %4 m/s) at %5, %6 m to go", m_iRoute, t, m_fLastSpeed, speed, ACR_Manager.Grid(m_Vehicle.GetOrigin()), (int)vector.DistanceXZ(m_Vehicle.GetOrigin(), m_vGoal));
  }
  m_fLastSpeed = speed;
  if (speed > 5) m_bRolling = true;
  else if (speed < 1.5 && m_bRolling) { m_bRolling = false; m_iStops++; }
  if (vector.DistanceXZ(m_Vehicle.GetOrigin(), m_vGoal) < 45) m_iArrived = t;
 }

 string Line()
 {
  int stuck = 0;
  string order = "?";
  if (m_Squad) { stuck = m_Squad.m_iStuckCount; order = m_Squad.m_Order; }
  string where = "?";
  float toGo = -1;
  if (m_Vehicle) { where = ACR_Manager.Grid(m_Vehicle.GetOrigin()); toGo = vector.DistanceXZ(m_Vehicle.GetOrigin(), m_vGoal); }
  return string.Format("route %1: arrived %2, %3 m driven, %4 m to go, top %5 m/s, stops %6, crashes %7, ACA stuck %8, order '%9'", m_iRoute, m_iArrived, (int)m_fDriven, (int)toGo, m_fTop, m_iStops, m_iCrashes, stuck, order + "' at " + where + " BBB " + BBB_Drive.Driving(m_Vehicle).ToString());
 }
}
