// -acrSupplyRunsTest 1 (server, about 4-5 min: two real truck drives, Arland test range): #242 phase 1 with -acrSupplyRuns
// and -acrLogisticsHubs on. No AI commanders. A US HQ placed 450 m out makes a FOB.
//  - pure: the floor delivery is 2x RP and 3x time; a sampled track thins to its bends.
//  - hubs: the US HQ is a hub; a plain held base is not (restock there only with hubs off).
//  - hub load: a crewed construction truck in the HQ's ring loads 500 paid supplies (20 RP) and is
//    sent; a new order calls its run off.
//  - run 1: one of two trucks on a road 350 m from the FOB, with paid loads, delivers 500 into the
//    FOB's store, and the drive is recorded as a route.
//  - run 2: the other truck, from the same start, replays the recorded route and delivers again
//    (the route counts 2 drives).
//  - AI: the US AI commander, handed the FOB with an empty store, sends a run (not the delivery).
//  - watchdog: that run's truck given up stuck fails the run (one recent failure for the FOB).
//  - floor: the delivery ordered beside a run costs 40 RP and takes 180 s.
class ACR_TestSupplyRuns
{
 static const int CHECKS = 10;
 static const ResourceName TRUCK = "{2D74C39A650A3030}Prefabs/Vehicles/Wheeled/M923A1/M923A1_engineer.et";
 static int s_iStep;
 static int s_iStepAt;
 static IEntity s_HQ;
 static ACR_Base s_Fob;
 static ACR_Base s_Hub;
 static ACR_Squad s_Crew;
 static vector s_vStart;
 static int s_iRP;
 static int s_iRoutes;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Options.Set("acrSupplyRuns", "1");
   ACR_Options.Set("acrLogisticsHubs", "1");
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("supplyruns", "no Alpha squad");
   return;
  }
  if (tick > 330 && s_iStep < 9) ACR_TestRegress.Abort("supplyruns", string.Format("timed out in step %1", s_iStep));
  if (tick < 5) return;
  int waited = tick - s_iStepAt;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");

  if (s_iStep == 0)
  {
   Pure();
   vector home = alpha.GetAnchor().GetOrigin();
   vector spot;
   for (int i = 0; i < 24; i++)
   {
    float angle = i * 15 * Math.DEG2RAD;
    vector candidate = ACR_Manager.Ground(home + Vector(Math.Cos(angle) * 450, 0, Math.Sin(angle) * 450));
    if (!NearBase(candidate) && !BBB_Terrain.IsWaterAt(candidate)) { spot = candidate; break; }
   }
   s_HQ = ACR_Manager.SpawnPrefab(ACR_TestStructures.HQ, spot);
   PrintFormat("[ACR] SUPPLYRUNS TEST headquarters at %1: %2", spot, s_HQ != null);
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base && base.m_bBuilt && !base.m_bRetired && base.m_sOwner == "US" && base.m_FobSupply) s_Fob = base;
    if (base && !base.m_bBuilt && base.m_sOwner == "US" && ACR_LogisticsRules.IsHQ(base)) s_Hub = base;
   }
   if (!s_Fob)
   {
    if (waited > 30) ACR_TestRegress.Abort("supplyruns", "the headquarters made no FOB");
    return;
   }
   if (!s_Hub)
   {
    ACR_TestRegress.Abort("supplyruns", "no US HQ base to load at");
    return;
   }
   Hubs();
   SetStock(s_Fob, 0);
   us.m_iRP = 1000;
   // A crewed construction truck in the HQ's ring (the hub check), and two on a road 350 m from
   // the FOB on Alpha's side, each with a paid load (runs 1 and 2: moving a crewed truck by script
   // doesn't take, so the drives start where the trucks spawn).
   vector at = ACR_Manager.Ground(s_Hub.GetOrigin() + Vector(8, 0, 8));
   vector road;
   if (BBB_Terrain.RoadPointNear(s_Hub.GetOrigin(), s_Hub.m_fRadius * 0.8, road)) at = road;
   manager.SpawnUnit("Hub Truck", "", TRUCK, true, "", at, ACR_Manager.Ground(at + Vector(5, 0, 5)));
   vector toward = (home2(alpha) - s_Fob.GetOrigin()).Normalized();
   vector want = ACR_Manager.Ground(s_Fob.GetOrigin() + toward * 350);
   if (!BBB_Terrain.RoadPointNear(want, 200, s_vStart)) s_vStart = want;
   manager.SpawnUnit("Run Truck A", "", TRUCK, true, "", s_vStart, ACR_Manager.Ground(s_vStart + Vector(5, 0, 5)));
   vector beside = s_vStart - toward * 25;
   if (!BBB_Terrain.RoadPointNear(beside, 40, beside)) beside = ACR_Manager.Ground(s_vStart - toward * 25);
   manager.SpawnUnit("Run Truck B", "", TRUCK, true, "", beside, ACR_Manager.Ground(beside + Vector(5, 0, 5)));
   PrintFormat("[ACR] SUPPLYRUNS TEST FOB %1 at %2, hub %3, drive start %4 (%5 m from the FOB)", s_Fob.m_sName, s_Fob.GetOrigin(), s_Hub.m_sName, s_vStart, Math.Round(vector.DistanceXZ(s_vStart, s_Fob.GetOrigin())));
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   if (waited < 4) return;
   ACR_Squad hubCrew, runA, runB;
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (squad.GetName() == "Hub Truck") hubCrew = squad;
    if (squad.GetName() == "Run Truck A") runA = squad;
    if (squad.GetName() == "Run Truck B") runB = squad;
   }
   if (!hubCrew || !runA || !runB || !ACR_Transport.CrewedVehicle(hubCrew) || !ACR_Transport.CrewedVehicle(runA) || !ACR_Transport.CrewedVehicle(runB) || !manager.m_SupplyRuns)
   {
    if (waited > 15) ACR_TestRegress.Abort("supplyruns", string.Format("trucks not crewed (hub %1, A %2, B %3) or no run manager (%4)", hubCrew != null, runA != null, runB != null, manager.m_SupplyRuns != null));
    return;
   }
   // Only the hub truck may go first.
   runA.SetAITask(ACR_EAITask.ROADBLOCK);
   runB.SetAITask(ACR_EAITask.ROADBLOCK);
   s_iRP = us.m_iRP;
   int code;
   string said = manager.m_SupplyRuns.Order("US", s_Fob, code);
   IEntity truck = ACR_Transport.CrewedVehicle(hubCrew);
   float paid = manager.m_Logistics.PaidSupplies(truck);
   ACR_SupplyRun hubRun = manager.m_SupplyRuns.RunTo(s_Fob);
   ACR_TestRegress.Check("supplyruns: a truck in the HQ loads 500 paid supplies for 20 RP and is sent", code == ACR_SupplyRuns.ORDER_STARTED && us.m_iRP == s_iRP - ACR_SupplyRuns.RUN_COST && paid >= 500 && hubRun && hubRun.m_Crew == hubCrew && hubRun.m_iStage == ACR_SupplyRuns.STAGE_DRIVE, string.Format("'%1'; RP %2 -> %3, paid aboard %4", said, s_iRP, us.m_iRP, paid));
   // Called off by a new order (it stays out of the rest), then the two road trucks get paid loads.
   hubCrew.RouteTo(ACR_Manager.Ground(truck.GetOrigin()), ACR_Squad.WP_MOVE, -1, "Moving", 0);
   float loads = ACR_SupplyRuns.RUN_SUPPLIES / ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION;
   manager.m_Logistics.LoadPaid(ACR_Transport.CrewedVehicle(runA), loads);
   manager.m_Logistics.LoadPaid(ACR_Transport.CrewedVehicle(runB), loads);
   runA.SetAITask(ACR_EAITask.FREE);
   runB.SetAITask(ACR_EAITask.FREE);
   s_Crew = hubCrew;
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   // The hub run called off; then run 1 from the road start.
   if (manager.m_SupplyRuns.RunTo(s_Fob))
   {
    if (waited > 10) ACR_TestRegress.Abort("supplyruns", "the hub truck's run wasn't called off by its new order");
    return;
   }
   s_Crew.SetAITask(ACR_EAITask.ROADBLOCK);
   int code;
   string said = manager.m_SupplyRuns.Order("US", s_Fob, code);
   PrintFormat("[ACR] SUPPLYRUNS TEST run 1: %1", said);
   NextStep(tick);
   return;
  }

  if (s_iStep == 4)
  {
   // Run 1 under way: delivered when it's over and the store holds 500.
   if (manager.m_SupplyRuns.RunTo(s_Fob) && waited < 130) return;
   int stock = ACR_FobSupply.Stock(s_Fob.m_FobSupply);
   ACR_TestRegress.Check("supplyruns: run 1 delivers 500 supplies into the FOB's store", stock >= 500 && !manager.m_SupplyRuns.RunTo(s_Fob), string.Format("stock %1 after %2 s, run still on %3", stock, waited, manager.m_SupplyRuns.RunTo(s_Fob) != null));
   s_iRoutes = manager.m_SupplyRuns.RouteCount();
   ACR_TestRegress.Check("supplyruns: the arrived run is recorded as a route", s_iRoutes >= 1, RouteText(manager));
   // Run 2: the other road truck, from the same start, replays it.
   SetStock(s_Fob, 0);
   int code;
   string said = manager.m_SupplyRuns.Order("US", s_Fob, code);
   ACR_SupplyRun run = manager.m_SupplyRuns.RunTo(s_Fob);
   bool replays = run && run.m_Route != null;
   ACR_TestRegress.Check("supplyruns: run 2 from the same start replays the recorded route", code == ACR_SupplyRuns.ORDER_STARTED && replays, string.Format("'%1'", said));
   NextStep(tick);
   return;
  }

  if (s_iStep == 5)
  {
   if (manager.m_SupplyRuns.RunTo(s_Fob) && waited < 150) return;
   int stock = ACR_FobSupply.Stock(s_Fob.m_FobSupply);
   int drives = 0;
   if (manager.m_SupplyRuns.RouteCount() > 0) drives = manager.m_SupplyRuns.RouteAt(0).m_iDrives;
   ACR_TestRegress.Check("supplyruns: run 2 delivers too and the route counts a second drive", stock >= 500 && drives >= 2 && manager.m_SupplyRuns.RouteCount() == s_iRoutes, string.Format("stock %1 after %2 s; %3", stock, waited, RouteText(manager)));
   // Still on (late): called off, so the AI check below sees only its own run.
   ACR_SupplyRun late = manager.m_SupplyRuns.RunTo(s_Fob);
   if (late && late.m_Crew && late.m_Truck) late.m_Crew.RouteTo(ACR_Manager.Ground(late.m_Truck.GetOrigin()), ACR_Squad.WP_MOVE, -1, "Moving", 0);
   NextStep(tick);
   return;
  }

  if (s_iStep == 6)
  {
   if (manager.m_SupplyRuns.RunTo(s_Fob))
   {
    if (waited > 6) ACR_TestRegress.Abort("supplyruns", "the late run 2 wasn't called off");
    return;
   }
   // The AI commander, handed the FOB with an empty store: a run, not the delivery.
   SetStock(s_Fob, 0);
   s_Fob.m_iSupplyDelivery = 0;
   ACR_FobAI.Set(s_Fob, true);
   s_iRP = us.m_iRP;
   ACR_AICommander ai = manager.FobAICommanderFor("US");
   ai.SetFobUpgradeEvalTimeForTest(0);
   ai.ManageHandedFobs();
   ACR_SupplyRun aiRun = manager.m_SupplyRuns.RunTo(s_Fob);
   ACR_TestRegress.Check("supplyruns: the AI commander sends a supply run rather than the delivery", aiRun != null && s_Fob.m_iSupplyDelivery == 0, string.Format("run %1 (stage %2), delivery %3, RP %4 -> %5", aiRun != null, RunStage(aiRun), s_Fob.m_iSupplyDelivery, s_iRP, us.m_iRP));
   ACR_FobAI.Set(s_Fob, false);
   // Watchdog: its truck gives up stuck.
   if (aiRun && aiRun.m_Crew) aiRun.m_Crew.m_fStuckAt = GetGame().GetWorld().GetWorldTime() + 1;
   NextStep(tick);
   return;
  }

  if (s_iStep == 7)
  {
   if (manager.m_SupplyRuns.RunTo(s_Fob) && waited < 6) return;
   ACR_TestRegress.Check("supplyruns: a truck that gives up stuck fails the run", !manager.m_SupplyRuns.RunTo(s_Fob) && manager.m_SupplyRuns.RecentFails(s_Fob) == 1, string.Format("run on %1, recent failures %2", manager.m_SupplyRuns.RunTo(s_Fob) != null, manager.m_SupplyRuns.RecentFails(s_Fob)));
   // Floor: the old delivery at 2x RP and 3x time.
   s_iRP = us.m_iRP;
   string said = ACR_FobSupply.Order(manager, "US", s_Fob);
   ACR_TestRegress.Check("supplyruns: the delivery costs 40 RP and takes 180 s with runs on", us.m_iRP == s_iRP - 40 && s_Fob.m_iSupplyDelivery == 500 && Math.AbsFloat(s_Fob.m_fSupplyDeliveryLeft - 180000) < 1, string.Format("'%1'; RP %2 -> %3, %4 ms", said, s_iRP, us.m_iRP, s_Fob.m_fSupplyDeliveryLeft));
   s_Fob.m_iSupplyDelivery = 0;
   ACR_Options.Unset("acrSupplyRuns");
   ACR_Options.Unset("acrLogisticsHubs");
   ACR_TestRegress.Complete("supplyruns", CHECKS);
   s_iStep = 9;
  }
 }

 protected static vector home2(ACR_Squad alpha)
 {
  return alpha.GetAnchor().GetOrigin();
 }

 protected static void Pure()
 {
  // Floor price (runs on at tick 1).
  bool floor = ACR_FobSupply.DeliveryCost() == 40 && Math.AbsFloat(ACR_FobSupply.DeliveryMs() - 180000) < 1;
  // An L: 0..400 m east every 20 m, then 0..300 m north: thins to the corner plus gap points.
  array<vector> track = {};
  for (int x = 0; x <= 400; x += 20) track.Insert(Vector(x, 0, 0));
  for (int z = 20; z <= 300; z += 20) track.Insert(Vector(400, 0, z));
  array<vector> thin = {};
  ACR_SupplyRuns.Thin(track, thin);
  bool corner;
  foreach (vector p : thin) if (vector.DistanceXZ(p, Vector(400, 0, 0)) < 1) corner = true;
  bool gaps = true;
  for (int i = 1; i < thin.Count(); i++) if (vector.DistanceXZ(thin[i - 1], thin[i]) > ACR_SupplyRuns.MAX_GAP_M + 1) gaps = false;
  ACR_TestRegress.Check("supplyruns pure: the delivery is the floor (2x RP, 3x time) and a track thins to its bends", floor && corner && gaps && thin.Count() < track.Count() && vector.DistanceXZ(thin[0], track[0]) < 0.01 && vector.DistanceXZ(thin[thin.Count() - 1], track[track.Count() - 1]) < 0.01, string.Format("delivery %1 RP / %2 ms; %3 of %4 points kept, corner %5, gaps ok %6", ACR_FobSupply.DeliveryCost(), ACR_FobSupply.DeliveryMs(), thin.Count(), track.Count(), corner, gaps));
 }

 protected static void Hubs()
 {
  // The HQ is a hub; the FOB, with no depot, isn't (its deploy point once made it look like an HQ).
  bool hqHub = ACR_SupplyRuns.IsHub(s_Hub, "US") && !ACR_SupplyRuns.IsHub(s_Fob, "US");
  ACR_Base plain;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && !base.m_bBuilt && base.m_sOwner == "US" && !ACR_LogisticsRules.IsHQ(base) && !ACR_SupplyRuns.IsPort(base) && !base.m_bContested && ACR_LogisticsRules.CanDeploy(base)) plain = base;
  if (!plain)
  {
   ACR_TestRegress.Check("supplyruns hubs: the HQ is a hub, a FOB without a depot isn't", hqHub, "no plain US deploy base to compare");
   return;
  }
  bool hubsOn = !ACR_SupplyRuns.RestocksAt(plain, "US");
  ACR_Options.Set("acrLogisticsHubs", "0");
  bool hubsOff = ACR_SupplyRuns.RestocksAt(plain, "US");
  ACR_Options.Set("acrLogisticsHubs", "1");
  ACR_TestRegress.Check("supplyruns hubs: the HQ is a hub; a plain base restocks only with hubs off", hqHub && hubsOn && hubsOff, string.Format("HQ %1 hub %2; %3: restocks with hubs on %4, off %5", s_Hub.m_sName, hqHub, plain.m_sName, !hubsOn, hubsOff));
 }

 protected static string RouteText(ACR_Manager manager)
 {
  string text = string.Format("%1 routes", manager.m_SupplyRuns.RouteCount());
  for (int i = 0; i < manager.m_SupplyRuns.RouteCount(); i++)
  {
   ACR_SupplyRoute route = manager.m_SupplyRuns.RouteAt(i);
   text += string.Format("; %1: %2 points, %3 m, %4 drives", route.m_sFaction, route.m_aPoints.Count(), Math.Round(route.Length()), route.m_iDrives);
  }
  return text;
 }

 protected static int RunStage(ACR_SupplyRun run)
 {
  if (!run) return 0;
  return run.m_iStage;
 }

 protected static void SetStock(ACR_Base fob, int value)
 {
  SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(fob.m_FobSupply);
  if (store) store.SetResourceValue(value);
 }

 protected static bool NearBase(vector p)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && vector.DistanceXZ(p, base.GetOrigin()) < base.m_fRadius + 150) return true;
  return false;
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
