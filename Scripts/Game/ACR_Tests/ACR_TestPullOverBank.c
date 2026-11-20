// -acrPullOverBankTest 1 (srv:pulloverbank, Everon, ~2.5 min; 20 Nov 2026, play-test: "Dismount pull over needs
// checks against rollovers. Seen two on the same stretch of Everon road."). The play-test's own stretch isn't in
// the local logs, so the test finds a comparable one: the Everon road stretch whose right verge (the side a
// pull-over takes) the old pull-over accepted (smooth from the road's middle, nothing solid on it) but slopes most
// across, for 32 m or more after a straight lead-in. An M923 truck and then a BTR-70 are set on the road 8 m before
// it, rolling at 29 km/h, and told to get out (ACR_Squad.GetOut, pull over on by default; 2IC off). Each must not
// overturn (hull roll sampled every 50 ms stays under 25 deg, not flipped), must stop safely (stopped, handbrake on,
// hull under 8 deg of roll) and its crew must be out. 6 checks.
// Uses no BBB symbol newer than BBB 9d12970, so the same test runs as a control against the old pull-over.
class ACR_TestPullOverBank
{
 static const int CHECKS = 6;
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static const ResourceName BTR = "{C012BB3488BEA0C2}Prefabs/Vehicles/Wheeled/BTR70/BTR70.et";
 static ref ACR_TestPullOverBank s_Test;

 protected vector m_vStart, m_vDir;
 protected float m_fBankSlope, m_fBankLength;
 protected int m_iRun;      // 0 truck, 1 BTR
 protected int m_iRunAt;
 protected string m_sName;
 protected ACR_Squad m_Squad;
 protected IEntity m_Vehicle;
 protected float m_fMaxRoll;
 protected bool m_bDone;
 protected bool m_bStarted;

 static void Run(int seconds)
 {
  int tick = seconds;
  if (!s_Test) s_Test = new ACR_TestPullOverBank();
  s_Test.Step(tick);
 }

 protected void Check(string what, bool ok)
 {
  ACR_TestRegress.Check("pulloverbank: " + what, ok);
 }

 protected string Kind()
 {
  if (m_iRun == 0) return "truck";
  return "APC";
 }

 protected void Step(int tick)
 {
  if (m_bDone) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (!m_bStarted)
  {
   if (tick < 3) return;
   m_bStarted = true;
   // The test's own squads stay its own: no 2IC or AI commander picking them up.
   ACR_Options.Set("acr2IC", "0");
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
   if (!FindBank())
   {
    ACR_TestRegress.Abort("pulloverbank", "no banked road stretch found");
    m_bDone = true;
    return;
   }
   m_iRunAt = tick;
   return;
  }
  if (tick > m_iRunAt) RunVehicle(manager, tick - m_iRunAt, tick);
 }

 // ---------------------------------------------------------------- the stretch

 // A verge the pull-over before 20 Nov took (smooth from the road's middle: under 0.22 m per half metre, it
 // refused 0.25; nothing solid within 3 m of the spot, no water) but a truck can't stand on: the spot 2.5 m past
 // the edge slopes 8 deg or more across (a truck's footprint, 1.25 m either side). Returns that roll, else -1.
 protected float RiskyVerge(BaseWorld world, vector p, vector right, float width)
 {
  vector spot = p + right * (width * 0.5 + 2.5);
  float last = world.GetSurfaceY(p[0], p[2]);
  for (float s = 0.5; s <= width * 0.5 + 2.51; s += 0.5)
  {
   vector q = p + right * s;
   float y = world.GetSurfaceY(q[0], q[2]);
   if (Math.AbsFloat(y - last) >= 0.22) return -1;
   last = y;
  }
  vector a = spot + right * 1.25;
  vector b = spot - right * 1.25;
  float roll = Math.AbsFloat(Math.Atan2(world.GetSurfaceY(a[0], a[2]) - world.GetSurfaceY(b[0], b[2]), 2.5)) * Math.RAD2DEG;
  if (roll < 8) return -1;
  spot[1] = world.GetSurfaceY(spot[0], spot[2]);
  if (BBB_Terrain.IsWaterAt(spot)) return -1;
  BBB_PullOverObstacles obstacles = new BBB_PullOverObstacles();
  world.QueryEntitiesBySphere(spot + Vector(0, 1, 0), 3, obstacles.Add, null, EQueryEntitiesFlags.STATIC);
  if (obstacles.m_bBlocked) return -1;
  return roll;
 }

 protected bool FindBank()
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return false;
  BaseWorld world = GetGame().GetWorld();
  vector mins, maxs;
  world.GetBoundBox(mins, maxs);
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(Vector(mins[0] + 300, -1000, mins[2] + 300), Vector(maxs[0] - 300, 3000, maxs[2] - 300), roads);
  float best = 0;
  int scanned;
  foreach (BaseRoad road : roads)
  {
   float width = road.GetWidth();
   if (width < 4.5) continue;
   array<vector> raw = {};
   road.GetPoints(raw);
   // Resample every 4 m.
   array<vector> pts = {};
   for (int i = 0; i + 1 < raw.Count(); i++)
   {
    float seg = vector.DistanceXZ(raw[i], raw[i + 1]);
    for (float d = 0; d < seg; d += 4) pts.Insert(raw[i] + (raw[i + 1] - raw[i]) * (d / Math.Max(seg, 0.01)));
   }
   for (int sense = 0; sense < 2; sense++)
   {
    if (sense == 1)
    {
     array<vector> back = {};
     for (int r = pts.Count() - 1; r >= 0; r--) back.Insert(pts[r]);
     pts = back;
    }
    int run = 0;
    float sumSlope = 0;
    for (int k = 1; k + 1 < pts.Count(); k++)
    {
     scanned++;
     vector dir = pts[k + 1] - pts[k - 1];
     dir[1] = 0;
     dir.Normalize();
     vector right = Vector(dir[2], 0, -dir[0]);
     float roll = RiskyVerge(world, pts[k], right, width);
     if (roll > 0)
     {
      run++;
      sumSlope += roll;
      continue;
     }
     // A run ended: 8 or more samples (32 m), with a straight lead-in of 4 samples before it.
     if (run >= 8)
     {
      int first = k - run;
      int lead = first - 4;
      float score = sumSlope / run;
      int last = first + 10;
      if (last > k - 1) last = k - 1;
      if (lead >= 0 && score > best && Straight(pts, lead, last))
      {
       vector start = ACR_Manager.Ground(pts[first - 2]);
       if (!NearBase(start) && !BBB_Terrain.IsWaterAt(start) && Math.AbsFloat(start[1] - world.GetSurfaceY(start[0], start[2])) < 1)
       {
        best = score;
        m_vStart = start;
        m_vDir = pts[first + 1] - pts[first - 1];
        m_vDir[1] = 0;
        m_vDir.Normalize();
        m_fBankSlope = score;
        m_fBankLength = run * 4;
       }
      }
     }
     run = 0;
     sumSlope = 0;
    }
   }
  }
  PrintFormat("[ACR] PULLOVERBANK scanned %1 road samples; stretch at %2 heading %3: %4 m long, the verge on the right %5 deg across on average (smooth, clear: the old pull-over took it)", scanned, m_vStart, Math.Round(m_vDir.VectorToAngles()[0]), m_fBankLength, Math.Round(m_fBankSlope));
  return best > 0;
 }

 // The road turns less than 15 degrees between samples a and b.
 protected bool Straight(array<vector> pts, int a, int b)
 {
  if (b <= a + 1) return false;
  vector d0 = pts[a + 1] - pts[a];
  d0[1] = 0;
  d0.Normalize();
  for (int i = a + 1; i < b; i++)
  {
   vector d = pts[i + 1] - pts[i];
   d[1] = 0;
   d.Normalize();
   if (vector.Dot(d0, d) < 0.966) return false;
  }
  return true;
 }

 protected bool NearBase(vector at)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && vector.DistanceXZ(base.GetOrigin(), at) < 300) return true;
  return false;
 }

 // ---------------------------------------------------------------- a vehicle on it

 protected void RunVehicle(ACR_Manager manager, int t, int tick)
 {
  if (t == 1)
  {
   m_sName = "Bank " + Kind();
   string faction = "US";
   ResourceName prefab = TRUCK;
   if (m_iRun == 1) { faction = "USSR"; prefab = BTR; }
   string before = manager.m_sSpawningFaction;
   manager.m_sSpawningFaction = faction;
   manager.SpawnUnit(m_sName, "", prefab, true, "", m_vStart, m_vStart - m_vDir * 10);
   manager.m_sSpawningFaction = before;
  }
  if (t == 10)
  {
   m_Squad = null;
   m_Vehicle = null;
   foreach (ACR_Squad squad : manager.m_Squads) if (squad && squad.GetName() == m_sName) m_Squad = squad;
   array<IEntity> vehicles = {};
   if (m_Squad) m_Squad.GetVehicles(vehicles);
   if (!vehicles.IsEmpty() && BBB.DriverOf(vehicles[0])) m_Vehicle = vehicles[0];
   if (!m_Vehicle)
   {
    Check(Kind() + ": crewed on the stretch", false);
    NextRun(manager, tick);
    return;
   }
   Place(Vector(0, 0, 0));
  }
  if (!m_Vehicle) return;
  if (t == 14)
  {
   // Rolling along the road at 29 km/h, then told to get out (as at the end of a drive).
   Place(m_vDir * 8);
   m_fMaxRoll = 0;
   GetGame().GetCallqueue().Remove(SampleRoll);
   GetGame().GetCallqueue().CallLater(SampleRoll, 50, true);
   bool ordered = m_Squad.GetOut();
   PrintFormat("[ACR] PULLOVERBANK %1: get out ordered %2 at 29 km/h, order %3", Kind(), ordered, m_Squad.m_Order);
  }
  if (t > 14 && t < 45 && (t % 3) == 0)
  {
   PrintFormat("[ACR] PULLOVERBANK %1 t=%2: %3 m on, %4 m right, %5 km/h, roll %6 deg (max %7), order %8", Kind(), t, Math.Round(vector.Dot(m_Vehicle.GetOrigin() - m_vStart, m_vDir) * 10) / 10, Math.Round(vector.Dot(m_Vehicle.GetOrigin() - m_vStart, Vector(m_vDir[2], 0, -m_vDir[0])) * 10) / 10, Math.Round(Speed() * 36) / 10, Math.Round(RollDeg()), Math.Round(m_fMaxRoll), m_Squad.m_Order);
  }
  if (t == 45)
  {
   GetGame().GetCallqueue().Remove(SampleRoll);
   bool flipped = SCR_AIVehicleUsability.IsVehicleFlipped(m_Vehicle);
   float roll = Math.AbsFloat(RollDeg());
   CarControllerComponent car = CarControllerComponent.Cast(m_Vehicle.FindComponent(CarControllerComponent));
   bool handbrake = car != null && car.GetPersistentHandBrake();
   int aboard;
   array<AIAgent> agents = {};
   m_Squad.GetAgents(agents);
   foreach (AIAgent agent : agents) if (agent.GetControlledEntity() && ChimeraCharacter.Cast(agent.GetControlledEntity()).IsInVehicle()) aboard++;
   float speed = Speed();
   PrintFormat("[ACR] PULLOVERBANK %1 end: flipped %2, max roll %3 deg, roll now %4 deg, %5 km/h, handbrake %6, %7 aboard, moved %8 m on", Kind(), flipped, Math.Round(m_fMaxRoll), Math.Round(roll), Math.Round(speed * 36) / 10, handbrake, aboard, Math.Round(vector.Dot(m_Vehicle.GetOrigin() - m_vStart, m_vDir)));
   Check(Kind() + ": didn't overturn (not flipped, roll stayed under 25 deg)", !flipped && m_fMaxRoll < 25);
   Check(Kind() + ": a safe stop (stopped, handbrake on, under 8 deg of roll)", speed < 0.5 && handbrake && roll < 8);
   Check(Kind() + ": the crew is out", aboard == 0);
   NextRun(manager, tick);
  }
 }

 protected void Place(vector velocity)
 {
  vector mat[4];
  Math3D.AnglesToMatrix(Vector(m_vDir.VectorToAngles()[0], 0, 0), mat);
  mat[3] = m_vStart + Vector(0, 0.4, 0);
  m_Vehicle.SetWorldTransform(mat);
  Physics physics = m_Vehicle.GetPhysics();
  if (physics) { physics.SetVelocity(velocity); physics.SetAngularVelocity(vector.Zero); }
 }

 protected void SampleRoll()
 {
  if (!m_Vehicle) return;
  float roll = Math.AbsFloat(RollDeg());
  if (roll > m_fMaxRoll) m_fMaxRoll = roll;
 }

 protected float RollDeg()
 {
  vector mat[4];
  m_Vehicle.GetWorldTransform(mat);
  return Math.Asin(Math.Clamp(mat[0][1], -1, 1)) * Math.RAD2DEG;
 }

 protected float Speed()
 {
  Physics physics = m_Vehicle.GetPhysics();
  if (!physics) return 0;
  return physics.GetVelocity().Length();
 }

 protected void NextRun(ACR_Manager manager, int tick)
 {
  GetGame().GetCallqueue().Remove(SampleRoll);
  if (m_Squad)
  {
   array<AIAgent> agents = {};
   m_Squad.GetAgents(agents);
   foreach (AIAgent agent : agents) if (agent.GetControlledEntity()) SCR_EntityHelper.DeleteEntityAndChildren(agent.GetControlledEntity());
   if (m_Squad.m_Group) SCR_EntityHelper.DeleteEntityAndChildren(m_Squad.m_Group);
  }
  if (m_Vehicle) SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
  m_Squad = null;
  m_Vehicle = null;
  m_iRun++;
  m_iRunAt = tick;
  if (m_iRun < 2) return;
  ACR_TestRegress.Complete("pulloverbank", CHECKS);
  m_bDone = true;
 }
}
