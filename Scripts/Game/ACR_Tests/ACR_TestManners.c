// -acrMannersTest 1 (server-capable, Arland, about 6 min; a mechanism check, the user's playtest judges):
// vehicle manners (BBB_Traffic, BBB_PullOver, ACR_Squad.PullOverFirst / Evade). On a straight 400-500 m
// stretch of one road, away from the bases:
//  1. head-on, twice: an M1025 and an M923 truck of one side drive at each other from the two ends: one
//     gives way (BBB_Traffic), they never touch, nobody is damaged, both get through;
//  2. get out: a crewed M1025 on the road is told to get out: it pulls onto the verge first, its
//     handbrake is on, the crew is out;
//  3. evade: an unarmed truck drives at an enemy BTR-70 parked at the far end: it gets away (EVADE)
//     and doesn't drive on into the BTR (the BTR's gunner may stop it on the way).
// Logs "[ACR] MANNERS ...". Every part is an option, off by default: the test turns them on.
class ACR_TestManners
{
 static const ResourceName M1025 = "{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et";
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static const ResourceName BTR = "{C012BB3488BEA0C2}Prefabs/Vehicles/Wheeled/BTR70/BTR70.et";
 static ref ACR_TestManners s_Test;

 protected vector m_vFrom, m_vTo, m_vMid, m_vDir;
 protected int m_iStep;
 protected int m_iStepAt;
 protected int m_iRun;
 protected ref array<string> m_aNames = {};
 protected ref array<ACR_Squad> m_aSquads = {};
 protected ref array<IEntity> m_aVehicles = {};
 protected ref array<float> m_aHealth = {};
 protected float m_fClosest;
 protected int m_iGiveWayBase;
 protected int m_iEvadeBase;
 protected float m_fEvadeDistance;
 protected vector m_vGetOutAt;
 protected int m_iPass;
 protected int m_iFail;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (!s_Test) s_Test = new ACR_TestManners();
  s_Test.Step(tick);
 }

 protected void Check(string what, bool ok)
 {
  ACR_TestRegress.Check("manners: " + what, ok);
  if (ok) m_iPass++;
  else m_iFail++;
 }

 protected void Step(int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   // The options this test checks, on (BBB's are read once a mission: read first, then set).
   BBB_Traffic.Read();
   BBB_Traffic.s_bGiveWay = true;
   BBB_Traffic.s_bHandbrake = true;
   BBB_Traffic.s_bEvade = true;
   ACR_Squad.s_bTestPullOver = true;
   ACR_Squad.s_bTestEvade = true;
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (tick == 4)
  {
   if (!PickRoad())
   {
    Print("[ACR] MANNERS FAIL: no straight road");
    Print("[ACR] TEST COMPLETE");
    m_iStep = 99;
    return;
   }
   m_iStep = 1;
   if (System.IsCLIParam("acrMannersGetOutTest")) m_iStep = 2; // the get-out part alone
   if (System.IsCLIParam("acrMannersEvadeTest")) m_iStep = 3; // the evade part alone
   m_iStepAt = tick;
  }
  if (m_iStep == 1) HeadOn(manager, tick);
  else if (m_iStep == 2) GetOut(manager, tick);
  else if (m_iStep == 3) Evade(manager, tick);
  else if (m_iStep == 4)
  {
   PrintFormat("[ACR] MANNERS done: %1 passed, %2 failed", m_iPass, m_iFail);
   Print("[ACR] TEST COMPLETE");
   m_iStep = 99;
  }
 }

 // ---------------------------------------------------------------- the road

 protected bool PickRoad()
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return false;
  vector mins, maxs;
  GetGame().GetWorld().GetBoundBox(mins, maxs);
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(Vector(mins[0] + 500, -1000, mins[2] + 500), Vector(maxs[0] - 500, 3000, maxs[2] - 500), roads);
  foreach (BaseRoad road : roads)
  {
   if (road.GetWidth() < 5) continue;
   array<vector> points = {};
   road.GetPoints(points);
   int n = points.Count();
   for (int i = 0; i + 1 < n; i++)
   {
    float length = 0;
    int j = i;
    while (j + 1 < n && length < 450)
    {
     length += vector.DistanceXZ(points[j], points[j + 1]);
     j++;
    }
    if (length < 400) break;
    if (vector.DistanceXZ(points[i], points[j]) < length * 0.95) continue;
    vector a = ACR_Manager.Ground(points[i]);
    vector b = ACR_Manager.Ground(points[j]);
    if (NearBase(a) || NearBase(b) || BBB_Terrain.IsWaterAt(a) || BBB_Terrain.IsWaterAt(b)) continue;
    m_vFrom = a;
    m_vTo = b;
    // The middle by length along the road (the straight line's middle is off a curving road).
    float half = length * 0.5;
    float walked = 0;
    vector mid = a;
    for (int k = i; k < j; k++)
    {
     float step = vector.DistanceXZ(points[k], points[k + 1]);
     if (walked + step >= half)
     {
      mid = points[k] + (points[k + 1] - points[k]) * ((half - walked) / Math.Max(step, 0.01));
      break;
     }
     walked += step;
    }
    m_vMid = ACR_Manager.Ground(mid);
    m_vDir = b - a;
    m_vDir[1] = 0;
    m_vDir.Normalize();
    PrintFormat("[ACR] MANNERS road %1 -> %2 (%3 m, width %4 m)", a, b, Math.Round(length), road.GetWidth());
    return true;
   }
  }
  return false;
 }

 protected bool NearBase(vector at)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && vector.DistanceXZ(base.GetOrigin(), at) < 400) return true;
  return false;
 }

 // ---------------------------------------------------------------- helpers

 protected void Spawn(ACR_Manager manager, string name, ResourceName prefab, string faction, vector at, vector facing)
 {
  string before = manager.m_sSpawningFaction;
  manager.m_sSpawningFaction = faction;
  manager.SpawnUnit(name, "", prefab, true, "", at, at - facing * 10);
  manager.m_sSpawningFaction = before;
  m_aNames.Insert(name);
 }

 // Squads and vehicles found for every name (false until all are crewed).
 protected bool Found(ACR_Manager manager)
 {
  m_aSquads.Clear();
  m_aVehicles.Clear();
  foreach (string name : m_aNames)
  {
   ACR_Squad found;
   foreach (ACR_Squad squad : manager.m_Squads) if (squad && squad.GetName() == name) found = squad;
   if (!found) return false;
   array<IEntity> vehicles = {};
   found.GetVehicles(vehicles);
   if (vehicles.IsEmpty() || !BBB.DriverOf(vehicles[0])) return false;
   m_aSquads.Insert(found);
   m_aVehicles.Insert(vehicles[0]);
  }
  return true;
 }

 protected void Clear(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : m_aSquads)
  {
   if (!squad) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents) if (agent.GetControlledEntity()) SCR_EntityHelper.DeleteEntityAndChildren(agent.GetControlledEntity());
   if (squad.m_Group) SCR_EntityHelper.DeleteEntityAndChildren(squad.m_Group);
  }
  foreach (IEntity vehicle : m_aVehicles) if (vehicle) SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
  m_aSquads.Clear();
  m_aVehicles.Clear();
  m_aNames.Clear();
  m_aHealth.Clear();
 }

 protected void Next(int tick)
 {
  m_iStep++;
  m_iStepAt = tick;
  m_iRun = 0;
 }

 // ---------------------------------------------------------------- 1. head-on

 protected void HeadOn(ACR_Manager manager, int tick)
 {
  int t = tick - m_iStepAt;
  if (t == 1)
  {
   // Swap the ends on the second run.
   vector a = m_vFrom;
   vector b = m_vTo;
   if (m_iRun == 1) { a = m_vTo; b = m_vFrom; }
   vector ab = b - a;
   ab[1] = 0;
   ab.Normalize();
   Spawn(manager, string.Format("Manners car %1", m_iRun + 1), M1025, "US", a, ab);
   Spawn(manager, string.Format("Manners truck %1", m_iRun + 1), TRUCK, "US", b, -ab);
   m_fClosest = float.MAX;
   m_iGiveWayBase = BBB_Traffic.s_iGiveWays;
  }
  if (t == 10)
  {
   if (!Found(manager))
   {
    Check(string.Format("head-on %1: both vehicles crewed", m_iRun + 1), false);
    Clear(manager);
    m_iRun++;
    m_iStepAt = tick;
    if (m_iRun >= 2) Next(tick);
    return;
   }
   foreach (IEntity vehicle : m_aVehicles) m_aHealth.Insert(BBB.Health(vehicle));
   m_aSquads[0].RouteTo(m_aVehicles[1].GetOrigin(), ACR_Squad.WP_MOVE, -1, "Moving", ACR_EOrderKind.MOVE, true);
   m_aSquads[1].RouteTo(m_aVehicles[0].GetOrigin(), ACR_Squad.WP_MOVE, -1, "Moving", ACR_EOrderKind.MOVE, true);
  }
  if (t > 10 && m_aVehicles.Count() == 2 && m_aVehicles[0] && m_aVehicles[1])
  {
   float d = vector.DistanceXZ(m_aVehicles[0].GetOrigin(), m_aVehicles[1].GetOrigin());
   if (d < m_fClosest) m_fClosest = d;
  }
  if (t == 95)
  {
   bool fine = m_aVehicles.Count() == 2 && m_aVehicles[0] && m_aVehicles[1];
   float lost = 0;
   bool through = fine;
   if (fine)
   {
    for (int k = 0; k < 2; k++) lost += m_aHealth[k] - BBB.Health(m_aVehicles[k]);
    // Each is past the middle, on the other side from where it started.
    vector startCar = m_vFrom;
    if (m_iRun == 1) startCar = m_vTo;
    through = vector.DistanceXZ(m_aVehicles[0].GetOrigin(), startCar) > 250 && vector.DistanceXZ(m_aVehicles[1].GetOrigin(), startCar) < 200;
   }
   int gave = BBB_Traffic.s_iGiveWays - m_iGiveWayBase;
   PrintFormat("[ACR] MANNERS head-on %1: give way %2, closest %3 m, health lost %4, both through %5", m_iRun + 1, gave, Math.Round(m_fClosest * 10) / 10, Math.Round(lost * 100) / 100, through);
   Check(string.Format("head-on %1: one gave way", m_iRun + 1), gave >= 1);
   Check(string.Format("head-on %1: they never touched (closest over 3 m) and lost no health", m_iRun + 1), m_fClosest > 3 && lost < 0.01);
   Check(string.Format("head-on %1: both got past each other", m_iRun + 1), through);
   Clear(manager);
   m_iRun++;
   m_iStepAt = tick;
   if (m_iRun >= 2) Next(tick);
  }
 }

 // ---------------------------------------------------------------- 2. get out

 protected void GetOut(ACR_Manager manager, int tick)
 {
  int t = tick - m_iStepAt;
  if (t == 1)
  {
   vector onRoad = m_vMid;
   vector road;
   vector roadDir;
   float roadWidth;
   if (BBB_PullOver.Snap(m_vMid, 40, road, roadDir, roadWidth)) onRoad = road;
   Spawn(manager, string.Format("Manners get out %1", m_iRun + 1), M1025, "US", onRoad, m_vDir);
  }
  if (t == 10)
  {
   if (!Found(manager))
   {
    Check("get out: crewed", false);
    Clear(manager);
    Next(tick);
    return;
   }
   // A bought vehicle spawns on a free spot off the road: set it on the road, facing along it.
   vector roadAt, roadDir;
   float roadWidth;
   if (BBB_PullOver.Snap(m_vMid, 40, roadAt, roadDir, roadWidth))
   {
    vector mat[4];
    Math3D.AnglesToMatrix(Vector(roadDir.VectorToAngles()[0], 0, 0), mat);
    mat[3] = roadAt + Vector(0, 0.5, 0);
    m_aVehicles[0].SetWorldTransform(mat);
    Physics physics = m_aVehicles[0].GetPhysics();
    if (physics) { physics.SetVelocity(vector.Zero); physics.SetAngularVelocity(vector.Zero); }
   }
  }
  if (t == 14 && m_aSquads.Count() == 1)
  {
   // Where it stands against the road (diagnostics for the pull-over's road snap).
   vector at = m_aVehicles[0].GetOrigin();
   vector sp, sd, mp, md, rp;
   float sw, mw;
   bool bbbSnap = BBB_PullOver.Snap(at, 30, sp, sd, sw);
   bool acaSnap = ACR_Minefields.Snap(at, 30, mp, md, mw);
   bool point = BBB_Terrain.RoadPointNear(at, 30, rp);
   ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
   BaseRoad road;
   float roadDistance = -1;
   int nodes = -1;
   if (aiWorld && aiWorld.GetRoadNetworkManager())
   {
    aiWorld.GetRoadNetworkManager().GetClosestRoad(at, road, roadDistance);
    array<vector> pts = {};
    if (road) { road.GetPoints(pts); nodes = pts.Count(); }
   }
   PrintFormat("[ACR] MANNERS get out at %1: BBB snap %2 (%3 m), ACA snap %4 (%5 m), road point %6 (%7 m), closest road %8 at %9 m", at, bbbSnap, Math.Round(vector.DistanceXZ(sp, at) * 10) / 10, acaSnap, Math.Round(vector.DistanceXZ(mp, at) * 10) / 10, point, Math.Round(vector.DistanceXZ(rp, at) * 10) / 10, road != null, roadDistance);
   PrintFormat("[ACR] MANNERS get out: the closest road has %1 points", nodes);
   m_vGetOutAt = at;
   bool ordered = m_aSquads[0].GetOut();
   PrintFormat("[ACR] MANNERS get out ordered %1, order now %2", ordered, m_aSquads[0].m_Order);
  }
  if (m_iRun == 1)
  {
   GetOutSuperseded(manager, tick, t);
   return;
  }
  if (t == 40)
  {
   IEntity vehicle = m_aVehicles[0];
   vector point, dir;
   float width;
   float off = 0, side = 0;
   if (vehicle && BBB_PullOver.Snap(vehicle.GetOrigin(), 30, point, dir, width))
   {
    side = vector.DistanceXZ(vehicle.GetOrigin(), point);
    off = side - width * 0.5;
   }
   bool handbrake;
   CarControllerComponent car;
   if (vehicle) car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
   if (car) handbrake = car.GetPersistentHandBrake();
   int aboard;
   array<AIAgent> agents = {};
   m_aSquads[0].GetAgents(agents);
   foreach (AIAgent agent : agents) if (agent.GetControlledEntity() && ChimeraCharacter.Cast(agent.GetControlledEntity()).IsInVehicle()) aboard++;
   PrintFormat("[ACR] MANNERS get out: %6 m from the road's middle (%1 m past its edge, the road %7 m wide), handbrake %2, %3 still aboard, pulled over %4 times in all, moved %5 m", Math.Round(off * 10) / 10, handbrake, aboard, BBB_PullOver.s_iParked, Math.Round(vector.DistanceXZ(vehicle.GetOrigin(), m_vGetOutAt)), Math.Round(side * 10) / 10, Math.Round(width * 10) / 10);
   // The user's ask: "move the vehicle off the road or to the side of the road instead of getting out in the middle".
   Check("get out: the vehicle stopped to the side of the road or off it (its centre 1.5 m or more from the middle)", side >= 1.5);
   Check("get out: handbrake on", handbrake);
   Check("get out: the crew is out", aboard == 0);
   Clear(manager);
   // Run 2: the same get-out, then a move order while it is still pulling over.
   m_iRun = 1;
   m_iStepAt = tick;
  }
 }

 // A new order during the pull-over takes the driver at once: no hand-driving on to the verge,
 // no handbrake, and the vehicle drives off to the new order with the crew aboard.
 protected void GetOutSuperseded(ACR_Manager manager, int tick, int t)
 {
  if (m_aSquads.Count() != 1 || !m_aVehicles[0]) return;
  IEntity vehicle = m_aVehicles[0];
  if (t == 16)
  {
   Check("get out, new order mid pull-over: it was pulling over when the order came", m_aSquads[0].m_Order == "Pulling over");
   vector onward = ACR_Manager.Ground(m_vGetOutAt + m_vDir * 220);
   m_aSquads[0].RouteTo(onward, ACR_Squad.WP_MOVE, -1, "Moving", ACR_EOrderKind.MOVE, true);
  }
  if (t == 20)
  {
   ChimeraCharacter driver = ChimeraCharacter.Cast(BBB.DriverOf(vehicle));
   AIControlComponent ai;
   if (driver) ai = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
   CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
   bool aiOn = ai != null && ai.IsAIActivated();
   bool handbrake = car != null && car.GetPersistentHandBrake();
   PrintFormat("[ACR] MANNERS get out superseded: 4 s after the move order the driver's AI is on %1, handbrake %2, order %3", aiOn, handbrake, m_aSquads[0].m_Order);
   Check("get out, new order mid pull-over: the driver has his AI back within 4 s", aiOn);
   Check("get out, new order mid pull-over: no handbrake", !handbrake);
  }
  if (t == 45)
  {
   vector moved = vehicle.GetOrigin() - m_vGetOutAt;
   float along = vector.Dot(moved, m_vDir);
   int aboard;
   array<AIAgent> agents = {};
   m_aSquads[0].GetAgents(agents);
   foreach (AIAgent agent : agents) if (agent.GetControlledEntity() && ChimeraCharacter.Cast(agent.GetControlledEntity()).IsInVehicle()) aboard++;
   PrintFormat("[ACR] MANNERS get out superseded: %1 m on along the road, %2 aboard", Math.Round(along), aboard);
   Check("get out, new order mid pull-over: it drove on to the new order (30 m or more along the road) with the crew aboard", along >= 30 && aboard > 0);
   Clear(manager);
   Next(tick);
   if (System.IsCLIParam("acrMannersGetOutTest")) m_iStep = 4;
  }
 }

 // ---------------------------------------------------------------- 3. evade

 protected void Evade(ACR_Manager manager, int tick)
 {
  int t = tick - m_iStepAt;
  if (t == 1)
  {
   Spawn(manager, "Manners BTR", BTR, "USSR", m_vTo, -m_vDir);
   Spawn(manager, "Manners convoy truck", TRUCK, "US", m_vFrom, m_vDir);
   m_iEvadeBase = BBB_Traffic.s_iEvades;
   m_fEvadeDistance = -1;
  }
  if (t == 12)
  {
   if (!Found(manager))
   {
    Check("evade: crewed", false);
    Clear(manager);
    Next(tick);
    return;
   }
   m_aSquads[1].RouteTo(m_vTo, ACR_Squad.WP_MOVE, -1, "Moving", ACR_EOrderKind.MOVE, true);
  }
  // Every 2 s: where the truck is, how fast, what it is doing (why it ends nearer the BTR).
  if (t > 12 && t % 2 == 0 && m_aVehicles.Count() == 2 && m_aVehicles[1] && m_aSquads.Count() == 2 && m_aSquads[1])
  {
   IEntity truck = m_aVehicles[1];
   float kmh;
   if (truck.GetPhysics()) kmh = truck.GetPhysics().GetVelocity().Length() * 3.6;
   vector heading = truck.GetTransformAxis(2);
   vector toBtr = m_vTo - truck.GetOrigin();
   toBtr[1] = 0;
   toBtr.Normalize();
   PrintFormat("[ACR] MANNERS evade t=%1: %2 m from the BTR, %3 km/h, facing it %4, order %5, BTR alive %6", t, Math.Round(vector.DistanceXZ(m_vTo, truck.GetOrigin())), Math.Round(kmh), Math.Round(vector.Dot(heading, toBtr) * 100) / 100, m_aSquads[1].m_Order, m_aVehicles[0] != null);
  }
  if (t > 12 && m_aVehicles.Count() == 2 && m_aVehicles[0] && m_aVehicles[1] && m_fEvadeDistance < 0 && BBB_Traffic.s_iEvades > m_iEvadeBase)
  {
   m_fEvadeDistance = vector.DistanceXZ(m_vTo, m_aVehicles[1].GetOrigin());
   PrintFormat("[ACR] MANNERS evade: turned away %1 m from the BTR (order %2)", Math.Round(m_fEvadeDistance), m_aSquads[1].m_Order);
  }
  if (t == 80)
  {
   float now = -1;
   if (m_aVehicles.Count() == 2 && m_aVehicles[1]) now = vector.DistanceXZ(m_vTo, m_aVehicles[1].GetOrigin()); // from where the BTR was parked: it may drive at the truck
   int evades = BBB_Traffic.s_iEvades - m_iEvadeBase;
   PrintFormat("[ACR] MANNERS evade: %1 evasions, %2 m from the BTR when it turned, %3 m now", evades, Math.Round(m_fEvadeDistance), Math.Round(now));
   Check("evade: the unarmed truck asked to get away", evades >= 1);
   // It got away, or the BTR stopped it on the way: either way it didn't drive on into the BTR.
   Check("evade: it turned away and didn't drive on into the BTR", m_fEvadeDistance > 0 && now >= m_fEvadeDistance - 5);
   Clear(manager);
   Next(tick);
   if (System.IsCLIParam("acrMannersEvadeTest")) m_iStep = 4;
  }
 }
}
