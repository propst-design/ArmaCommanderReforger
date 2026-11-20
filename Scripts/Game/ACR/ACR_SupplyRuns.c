// #242 physical logistics, phase 1 (Jacob's rulings, 10 Nov 2026): supply runs, logistics hubs and
// recorded routes. Everything here is off unless the host turns it on (-acrSupplyRuns 1,
// -acrLogisticsHubs 1, -acrSaveRoutes 1: setup screen, Experimental tab).
//
// Supply runs: only a FOB's building supplies become physical. Spawning stays on RP.
//  - A run is one of the side's AI-crewed construction trucks: load 500 paid supplies at a hub
//    (RUN_COST RP, paid when it loads; a truck already carrying 500 paid supplies goes straight),
//    drive to the FOB, unload into its store on arrival (inside the ring or at its truck park).
//  - Watchdog: ETA = route length at plain truck speed x1.5 + 2 min. A truck that gives up stuck,
//    or passes 2x ETA, fails the run with a feed line and the grid. Truck and load stay where they
//    stopped (a player can still drive it in and unload); a destroyed truck loses its load.
//  - Floor: the abstract delivery (ACR_FobSupply.Order) stays, at 2x RP and 3x time while runs are
//    on, so a stuck truck never starves a side (AI driving delivered about 13 of 76 rides on 6 Nov).
//  - Player-driven loads get +25% (PLAYER_BONUS): a run whose driver was a player, and a player's
//    own "Unload supplies" of a paid load into a FOB (ACR_FobSupply.Unloaded).
// Hubs (-acrLogisticsHubs 1): trucks load and restock only at the HQ, ports (a base with deep water
//    by its ring) and hub FOBs (a FOB with a built supply depot). Off: any held deploy base, as before.
// Recorded routes: every run that arrives is recorded (both sides, each keeps its own): its track
//    thinned to bend points. A later run between the same ends replays it as a chain of move legs
//    (ACR_Squad.WalkLegs: #86's FlowHops, SkipStraightHops, DriveWatch and OnGroupMoveFailed all
//    apply unchanged). A leg that sticks falls back to road routing where it stands (ACR_Squad.Reroute)
//    and the route is marked suspect; a second failure sets it aside for the match until a drive
//    restores it. Routes that cross an enemy base or a known FIA road are skipped while that stands.
//    Routes live in the match; -acrSaveRoutes 1 keeps them in the host's profile per map
//    ($profile:ACA_Routes_<world>.txt). Nothing leaves the host's machine.
class ACR_SupplyRoute : Managed
{
 string m_sFaction;
 ref array<vector> m_aPoints;
 int m_iDrives;     // runs that arrived along it (or recorded it)
 int m_iFails;      // stuck on it this match since its last good drive
 bool m_bFailed;    // set aside until someone drives it again

 void ACR_SupplyRoute()
 {
  m_aPoints = {};
 }

 vector Start() { return m_aPoints[0]; }
 vector End() { return m_aPoints[m_aPoints.Count() - 1]; }

 float Length()
 {
  float length;
  for (int i = 1; i < m_aPoints.Count(); i++) length += vector.DistanceXZ(m_aPoints[i - 1], m_aPoints[i]);
  return length;
 }

 string ToText()
 {
  string text = string.Format("route|%1|%2|", m_sFaction, m_iDrives);
  foreach (int i, vector p : m_aPoints)
  {
   if (i > 0) text += ";";
   text += string.Format("%1 %2", Math.Round(p[0]), Math.Round(p[2]));
  }
  return text;
 }

 static ACR_SupplyRoute FromText(string line)
 {
  array<string> parts = {};
  line.Split("|", parts, false);
  if (parts.Count() < 4 || parts[0] != "route") return null;
  ACR_SupplyRoute route = new ACR_SupplyRoute();
  route.m_sFaction = parts[1];
  route.m_iDrives = Math.Max(1, parts[2].ToInt());
  array<string> points = {};
  parts[3].Split(";", points, true);
  foreach (string point : points)
  {
   array<string> xz = {};
   point.Split(" ", xz, true);
   if (xz.Count() < 2) continue;
   route.m_aPoints.Insert(ACR_Manager.Ground(Vector(xz[0].ToFloat(), 0, xz[1].ToFloat())));
  }
  if (route.m_sFaction.IsEmpty() || route.m_aPoints.Count() < 2) return null;
  return route;
 }
}

class ACR_SupplyRun : Managed
{
 string m_sFaction;
 ACR_Base m_Fob;
 ACR_Base m_Hub;           // where it loads (stage LOAD)
 ACR_Squad m_Crew;
 IEntity m_Truck;
 int m_iStage;
 vector m_vGoal;           // where this stage drives to
 float m_fSentAt;          // this stage
 float m_fEtaMs;
 int m_iSerial;            // the crew's order serial after our order: another order calls the run off
 int m_iReroutes;          // the crew's re-routes / stuck count when the route leg started
 int m_iStuck;
 ACR_SupplyRoute m_Route;  // the recorded route it replays (null: road routing)
 ref array<vector> m_aTrack;
 float m_fSampledAt;
 bool m_bByPlayer;         // a player drove it at some point: +25% on delivery

 void ACR_SupplyRun()
 {
  m_aTrack = {};
 }
}

class ACR_SupplyRuns : Managed
{
 static const int STAGE_LOAD = 1;
 static const int STAGE_DRIVE = 2;
 static const int RUN_SUPPLIES = 500;
 static const int RUN_COST = 20;               // RP, paid when the truck loads at a hub (today's delivery price)
 static const float FLOOR_COST_FACTOR = 2;     // the abstract delivery while runs are on (ruling 1)
 static const float FLOOR_TIME_FACTOR = 3;
 static const float PLAYER_BONUS = 0.25;
 static const float TRUCK_SPEED = 8;           // m/s, plain truck speed for the ETA
 static const float ETA_SLACK = 1.5;
 static const float ETA_EXTRA_MS = 120000;
 static const float ETA_FAIL_FACTOR = 2;
 static const float PARK_ARRIVE = 40;          // m from the FOB's truck park counts as delivered
 static const float FAIL_MEMORY_MS = 900000;   // a failed run marks the FOB's road bad this long
 static const int FLOOR_AFTER_FAILS = 2;       // AI: after this many failed runs it buys the floor
 static const float STUCK_WAIT_MS = 600000;    // a truck that gave up stuck isn't sent again for this long
 static const int TICK_MS = 2000;
 // Routes.
 static const float ROUTE_START_M = 150;       // a route's start this near the truck...
 static const float ROUTE_END_M = 150;         // ...and its end this near the FOB's goal
 static const float SAMPLE_MS = 3000;
 static const float SAMPLE_MIN_M = 10;
 static const float BEND_M = 15;               // keep a point where the track strays this far from the line
 static const float MAX_GAP_M = 160;           // and never leave points further apart
 static const float MIN_ROUTE_M = 300;
 static const int MOST_ROUTES = 40;            // per side
 static const int FAILS_TO_SET_ASIDE = 2;
 static const float DANGER_BASE_M = 50;        // a point this far inside an enemy base's ring skips the route

 protected ACR_Manager m_Manager;
 protected float m_fNextTick;
 protected ref array<ref ACR_SupplyRun> m_Runs;
 protected ref array<ref ACR_SupplyRoute> m_Routes;
 protected ref map<ACR_Base, int> m_Fails;      // FOB -> failed runs within FAIL_MEMORY_MS
 protected ref map<ACR_Base, float> m_FailedAt;
 protected bool m_bLoaded;

 void ACR_SupplyRuns(ACR_Manager manager)
 {
  m_Manager = manager;
  m_Runs = {};
  m_Routes = {};
  m_Fails = new map<ACR_Base, int>();
  m_FailedAt = new map<ACR_Base, float>();
 }

 // ---------------------------------------------------------------- options
 static bool On() { return ACR_LogisticsRules.Choice("acrSupplyRuns", "0") == "1"; }
 static bool HubsOn() { return ACR_LogisticsRules.Choice("acrLogisticsHubs", "0") == "1"; }
 static bool SaveRoutesOn() { return ACR_LogisticsRules.Choice("acrSaveRoutes", "0") == "1"; }

 static int FloorCost() { return (int)Math.Round(ACR_FobSupply.DELIVERY_COST * FLOOR_COST_FACTOR); }
 static float FloorMs() { return ACR_FobSupply.DELIVERY_MS * FLOOR_TIME_FACTOR; }

 // ---------------------------------------------------------------- hubs

 // A base with deep water by its ring (checked once per base): a port.
 static bool IsPort(ACR_Base base)
 {
  if (!base) return false;
  if (base.m_iPortState == 0)
  {
   base.m_iPortState = 2;
   vector centre = base.GetOrigin();
   for (int ring = 0; ring < 2 && base.m_iPortState == 2; ring++)
   {
    float reach = base.m_fRadius + 60 + ring * 60;
    for (int i = 0; i < 12; i++)
    {
     float angle = i * Math.PI2 / 12;
     vector p = Vector(centre[0] + Math.Cos(angle) * reach, centre[1], centre[2] + Math.Sin(angle) * reach);
     if (!BBB_Terrain.IsDeepWaterAt(p)) continue;
     base.m_iPortState = 1;
     break;
    }
   }
  }
  return base.m_iPortState == 1;
 }

 // A logistics hub of 'faction': its HQ, a port it holds, or a FOB of its with a built supply depot.
 static bool IsHub(ACR_Base base, string faction)
 {
  if (!base || base.m_bRetired || base.m_bContested || base.m_bHqLost || base.m_sOwner != faction) return false;
  // A FOB is a hub only with a depot: LogisticsRules.IsHQ also takes a FOB's own deploy point for an HQ's.
  if (base.m_bBuilt)
  {
   ACR_Manager manager = ACR_Manager.GetInstance();
   return manager && manager.m_Structures && manager.m_Structures.HasDepot(base);
  }
  return ACR_LogisticsRules.IsHQ(base) || IsPort(base);
 }

 // Where trucks of 'faction' restock (ACR_Logistics.Restock) and runs load: hubs with the hubs
 // option, else any held, uncontested deploy base that isn't a FOB (as before #242).
 static bool RestocksAt(ACR_Base base, string faction)
 {
  if (!base) return false;
  if (HubsOn()) return IsHub(base, faction);
  return !base.m_bBuilt && !base.m_bRetired && !base.m_bContested && base.m_sOwner == faction && ACR_LogisticsRules.CanDeploy(base);
 }

 // The hub nearest 'at', never 'except' (a run doesn't load at the FOB it supplies).
 static ACR_Base NearestHub(string faction, vector at, ACR_Base except = null)
 {
  ACR_Base best;
  float bestDist = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base == except || !RestocksAt(base, faction)) continue;
   float d = vector.DistanceXZ(at, base.GetOrigin());
   if (d < bestDist) { bestDist = d; best = base; }
  }
  return best;
 }

 // ---------------------------------------------------------------- runs

 ACR_SupplyRun RunTo(ACR_Base fob)
 {
  foreach (ACR_SupplyRun run : m_Runs) if (run.m_Fob == fob) return run;
  return null;
 }

 ACR_SupplyRun RunOf(ACR_Squad crew)
 {
  foreach (ACR_SupplyRun run : m_Runs) if (run.m_Crew == crew) return run;
  return null;
 }

 int RunCount() { return m_Runs.Count(); }
 int RouteCount() { return m_Routes.Count(); }
 ACR_SupplyRoute RouteAt(int i) { return m_Routes[i]; }

 // Failed runs to this FOB lately (AI: buys the floor after FLOOR_AFTER_FAILS).
 int RecentFails(ACR_Base fob)
 {
  float at;
  if (!fob || !m_FailedAt.Find(fob, at)) return 0;
  if (GetGame().GetWorld().GetWorldTime() - at > FAIL_MEMORY_MS)
  {
   m_FailedAt.Remove(fob);
   m_Fails.Remove(fob);
   return 0;
  }
  return m_Fails.Get(fob);
 }

 // Why this squad can't drive a run ("" = it can): an AI crew of a construction truck, free.
 string WhyNotCrew(ACR_Squad crew, string faction)
 {
  if (!crew || !m_Manager.m_Squads.Contains(crew) || crew.CountAlive() <= 0) return "gone";
  if (!crew.GetFaction() || crew.GetFaction().GetFactionKey() != faction) return "not ours";
  if (crew.PlayerSteers() || crew.HasPlayer()) return "led by a player";
  IEntity truck = ACR_Transport.CrewedVehicle(crew);
  if (!truck || ACR_Logistics.Health(truck) <= 0 || (ACR_Logistics.ServiceOf(truck) & ACR_EService.CONSTRUCTION) == 0) return "no construction truck";
  if (RunOf(crew)) return "on a supply run";
  if (crew.m_iAITask != ACR_EAITask.FREE && crew.m_iAITask != ACR_EAITask.POSTED) return "busy";
  if (crew.OrderState() == ACR_EOrderState.BUILD_FOB || crew.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || crew.HasJob()) return "building";
  if (m_Manager.m_Rides && (m_Manager.m_Rides.RideOf(crew) || m_Manager.m_Rides.Unloading(crew))) return "giving a ride";
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - crew.m_fStuckAt < STUCK_WAIT_MS || now - crew.m_fGaveUpAt < STUCK_WAIT_MS) return "stuck lately";
  return "";
 }

 // The side's free truck crew for a run to 'fob': one already carrying a paid load first, then
 // the nearest to a hub-and-FOB trip.
 ACR_Squad FreeCrew(string faction, ACR_Base fob)
 {
  ACR_Squad best;
  float bestScore = float.MAX;
  foreach (ACR_Squad crew : m_Manager.m_Squads)
  {
   if (!WhyNotCrew(crew, faction).IsEmpty()) continue;
   IEntity truck = ACR_Transport.CrewedVehicle(crew);
   vector at = truck.GetOrigin();
   float score = vector.DistanceXZ(at, fob.GetOrigin());
   if (m_Manager.m_Logistics.PaidSupplies(truck) < RUN_SUPPLIES)
   {
    ACR_Base hub = NearestHub(faction, at, fob);
    if (!hub) continue;
    score = vector.DistanceXZ(at, hub.GetOrigin()) + vector.DistanceXZ(hub.GetOrigin(), fob.GetOrigin());
   }
   if (score < bestScore) { bestScore = score; best = crew; }
  }
  return best;
 }

 static const int ORDER_STARTED = 0;
 static const int ORDER_NO_TRUCK = 1;
 static const int ORDER_REFUSED = 2;

 // Start a run to 'fob' for 'faction' (server). 'code': ORDER_STARTED, ORDER_NO_TRUCK (no free
 // construction truck crew: the caller may buy the floor), ORDER_REFUSED (the reason is returned).
 string Order(string faction, ACR_Base fob, out int code)
 {
  code = ORDER_REFUSED;
  if (!Replication.IsServer() || !On()) return "Supply runs are off";
  if (!fob || fob.m_bRetired || !fob.m_bBuilt || fob.m_sOwner != faction || !fob.m_FobSupply) return "Select one of your built FOBs";
  if (fob.m_bHqLost) return "FOB HQ destroyed: rebuild it with an Establish FOB order";
  ACR_SupplyRun already = RunTo(fob);
  if (already) return string.Format("A supply run to %1 is already on its way (%2)", fob.m_sName, already.m_Crew.GetName());
  SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(fob.m_FobSupply);
  if (!store || store.GetMaxResourceValue() - ACR_FobSupply.Stock(fob.m_FobSupply) < RUN_SUPPLIES) return "FOB needs room for 500 supplies";
  ACR_Squad crew = FreeCrew(faction, fob);
  if (!crew)
  {
   code = ORDER_NO_TRUCK;
   return "No free construction truck for a supply run";
  }
  IEntity truck = ACR_Transport.CrewedVehicle(crew);
  ACR_SupplyRun run = new ACR_SupplyRun();
  run.m_sFaction = faction;
  run.m_Fob = fob;
  run.m_Crew = crew;
  run.m_Truck = truck;
  string how;
  if (m_Manager.m_Logistics.PaidSupplies(truck) >= RUN_SUPPLIES) how = "already loaded";
  else
  {
   ACR_Base here = HubUnder(faction, truck.GetOrigin(), fob);
   if (here)
   {
    if (!Load(run, here)) return string.Format("Not enough RP (%1 needed to load a supply run)", RUN_COST);
    how = string.Format("loaded at %1 (%2 RP)", here.m_sName, RUN_COST);
   }
   else
   {
    run.m_Hub = NearestHub(faction, truck.GetOrigin(), fob);
    if (!run.m_Hub) return "No logistics hub to load at";
    how = string.Format("loads at %1 first (%2 RP there)", run.m_Hub.m_sName, RUN_COST);
   }
  }
  m_Runs.Insert(run);
  crew.SetAITask(ACR_EAITask.SUPPLY_RUN);
  if (run.m_Hub) Drive(run, STAGE_LOAD);
  else Drive(run, STAGE_DRIVE);
  code = ORDER_STARTED;
  PrintFormat("[ACR] SUPPLYRUN %1: %2 to %3, %4 (stage %5, ETA %6 s, route %7)", faction, crew.GetName(), fob.m_sName, how, run.m_iStage, Math.Round(run.m_fEtaMs / 1000), run.m_Route != null);
  return string.Format("Supply run: %1 takes 500 supplies to %2 (%3)", crew.GetName(), fob.m_sName, how);
 }

 // The hub the truck stands in, or null.
 protected ACR_Base HubUnder(string faction, vector at, ACR_Base except)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base != except && RestocksAt(base, faction) && vector.DistanceXZ(at, base.GetOrigin()) <= base.m_fRadius) return base;
  return null;
 }

 // Pay RUN_COST and make 500 of the truck's supplies a paid load. False: not enough RP (nothing spent).
 protected bool Load(ACR_SupplyRun run, ACR_Base hub)
 {
  if (!m_Manager.m_Bases.Spend(run.m_sFaction, RUN_COST)) return false;
  float loads = RUN_SUPPLIES / ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION;
  m_Manager.m_Logistics.LoadPaid(run.m_Truck, loads);
  PrintFormat("[ACR] SUPPLYRUN %1: %2 loads 500 paid supplies at %3 (%4 RP)", run.m_sFaction, run.m_Crew.GetName(), hub.m_sName, RUN_COST);
  return true;
 }

 // Where a run unloads: the FOB's truck park (a road spot near it), else its centre.
 protected vector FobGoal(ACR_Base fob)
 {
  vector spot;
  if (ACR_Logistics.ParkSpot(fob.GetOrigin(), fob.m_fRadius, vector.Zero, spot)) return spot;
  return ACR_Manager.Ground(fob.GetOrigin());
 }

 protected void Drive(ACR_SupplyRun run, int stage)
 {
  run.m_iStage = stage;
  vector from = run.m_Truck.GetOrigin();
  if (stage == STAGE_LOAD) run.m_vGoal = ACR_Manager.Ground(run.m_Hub.GetOrigin());
  else run.m_vGoal = FobGoal(run.m_Fob);
  run.m_Route = null;
  run.m_aTrack.Clear();
  run.m_aTrack.Insert(from);
  float now = GetGame().GetWorld().GetWorldTime();
  run.m_fSentAt = now;
  run.m_fSampledAt = now;
  float length = vector.DistanceXZ(from, run.m_vGoal) * 1.3; // roads wind
  run.m_Crew.SetMode(ACR_EMoveMode.FAST);
  string label = "Supply run to " + run.m_Fob.m_sName;
  if (stage == STAGE_LOAD) label = "Loading supplies at " + run.m_Hub.m_sName;
  ACR_SupplyRoute route;
  if (stage == STAGE_DRIVE) route = PickRoute(run.m_sFaction, from, run.m_vGoal);
  if (route)
  {
   array<vector> legs = {};
   // From the route point nearest the truck on, then the goal itself.
   int first;
   float nearest = float.MAX;
   foreach (int i, vector p : route.m_aPoints)
   {
    float d = vector.DistanceXZ(p, from);
    if (d < nearest) { nearest = d; first = i; }
   }
   if (nearest < 30 && first + 1 < route.m_aPoints.Count()) first++;
   for (int k = first; k < route.m_aPoints.Count(); k++) legs.Insert(route.m_aPoints[k]);
   if (vector.DistanceXZ(legs[legs.Count() - 1], run.m_vGoal) > 15) legs.Insert(run.m_vGoal);
   run.m_Crew.WalkLegs(legs, ACR_Squad.WP_MOVE, -1, label, 0);
   run.m_Route = route;
   length = route.Length();
   PrintFormat("[ACR] SUPPLYRUN %1: %2 replays a recorded route (%3 legs, %4 m, %5 drives)", run.m_sFaction, run.m_Crew.GetName(), legs.Count(), Math.Round(length), route.m_iDrives);
  }
  else run.m_Crew.RouteTo(run.m_vGoal, ACR_Squad.WP_MOVE, -1, label, 0);
  run.m_fEtaMs = length / TRUCK_SPEED * ETA_SLACK * 1000 + ETA_EXTRA_MS;
  run.m_iSerial = run.m_Crew.m_iOrderSerial;
  run.m_iReroutes = run.m_Crew.Reroutes();
  run.m_iStuck = run.m_Crew.m_iStuckCount;
 }

 void Tick()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextTick) return;
  m_fNextTick = now + TICK_MS;
  if (!m_bLoaded)
  {
   m_bLoaded = true;
   if (SaveRoutesOn()) LoadRoutes();
  }
  for (int i = m_Runs.Count() - 1; i >= 0; i--)
  {
   ACR_SupplyRun run = m_Runs[i];
   string end = Step(run, now);
   if (end.IsEmpty()) continue;
   if (run.m_Crew && run.m_Crew.m_iAITask == ACR_EAITask.SUPPLY_RUN) run.m_Crew.SetAITask(ACR_EAITask.FREE);
   m_Runs.Remove(i);
  }
 }

 // One check of a run: "" while it goes on, else why it ended (it has said so).
 protected string Step(ACR_SupplyRun run, float now)
 {
  string fobName = "?";
  if (run.m_Fob) fobName = run.m_Fob.m_sName;
  if (!run.m_Truck || ACR_Logistics.Health(run.m_Truck) <= 0) return Fail(run, "the truck was destroyed, its load lost");
  if (!run.m_Crew || !m_Manager.m_Squads.Contains(run.m_Crew) || run.m_Crew.CountAlive() <= 0) return Fail(run, "the crew was lost; the truck stands at " + ACR_Manager.Grid(run.m_Truck.GetOrigin()));
  if (!run.m_Fob || run.m_Fob.m_bRetired || run.m_Fob.m_bHqLost || run.m_Fob.m_sOwner != run.m_sFaction) return Tell(run, string.Format("Supply run to %1 called off: the FOB is lost", fobName), "fob lost");
  if (ACR_Transport.CrewedVehicle(run.m_Crew) != run.m_Truck) return Fail(run, "the crew left the truck at " + ACR_Manager.Grid(run.m_Truck.GetOrigin()));
  if (run.m_Crew.PlayerSteers() || run.m_Crew.HasPlayer()) run.m_bByPlayer = true;
  else if (run.m_Crew.m_iOrderSerial != run.m_iSerial) return Tell(run, string.Format("Supply run to %1 called off: %2 has a new order", fobName, run.m_Crew.GetName()), "new order");
  vector at = run.m_Truck.GetOrigin();
  // The track it drives (recorded on arrival).
  if (run.m_iStage == STAGE_DRIVE && now - run.m_fSampledAt >= SAMPLE_MS)
  {
   run.m_fSampledAt = now;
   if (vector.DistanceXZ(at, run.m_aTrack[run.m_aTrack.Count() - 1]) >= SAMPLE_MIN_M) run.m_aTrack.Insert(at);
  }
  // A recorded route that stuck: the squad fell back to the roads (Reroute) where it stood.
  if (run.m_Route && (run.m_Crew.Reroutes() > run.m_iReroutes || run.m_Crew.m_iStuckCount > run.m_iStuck))
  {
   ACR_SupplyRoute stale = run.m_Route;
   run.m_Route = null;
   stale.m_iFails++;
   if (stale.m_iFails >= FAILS_TO_SET_ASIDE) stale.m_bFailed = true;
   string state = "suspect";
   if (stale.m_bFailed) state = "set aside for this match";
   PrintFormat("[ACR] SUPPLYRUN %1: recorded route stuck at %2, falling back to the roads; route %3", run.m_sFaction, ACR_Manager.Grid(at), state);
  }
  // Arrived?
  if (run.m_iStage == STAGE_LOAD)
  {
   if (run.m_Hub && vector.DistanceXZ(at, run.m_Hub.GetOrigin()) <= run.m_Hub.m_fRadius)
   {
    if (!RestocksAt(run.m_Hub, run.m_sFaction)) return Fail(run, run.m_Hub.m_sName + " can't load supplies any more");
    if (!Load(run, run.m_Hub)) return Tell(run, string.Format("Supply run to %1 called off at %2: not enough RP to load (%3 needed)", fobName, run.m_Hub.m_sName, RUN_COST), "no rp");
    Drive(run, STAGE_DRIVE);
    return "";
   }
  }
  else if (vector.DistanceXZ(at, run.m_Fob.GetOrigin()) <= run.m_Fob.m_fRadius || vector.DistanceXZ(at, run.m_vGoal) <= PARK_ARRIVE)
   return Deliver(run);
  // Watchdog.
  if (run.m_Crew.m_fStuckAt >= run.m_fSentAt) return Fail(run, "the truck is stuck at " + ACR_Manager.Grid(at));
  if (now - run.m_fSentAt > run.m_fEtaMs * ETA_FAIL_FACTOR) return Fail(run, string.Format("the truck is overdue (%1 min) at %2", Math.Round((now - run.m_fSentAt) / 60000), ACR_Manager.Grid(at)));
  return "";
 }

 protected string Deliver(ACR_SupplyRun run)
 {
  float bonus = 0;
  if (run.m_bByPlayer) bonus = PLAYER_BONUS;
  float added;
  float moved = m_Manager.m_Logistics.UnloadPaidInto(run.m_Truck, run.m_Fob, bonus, added);
  if (run.m_iStage == STAGE_DRIVE) RecordTrack(run.m_sFaction, run.m_aTrack, run.m_Truck.GetOrigin());
  m_Fails.Remove(run.m_Fob);
  m_FailedAt.Remove(run.m_Fob);
  string extra = "";
  if (added >= 1) extra = string.Format(" (+%1 for a player-driven load)", Math.Round(added));
  float left = m_Manager.m_Logistics.PaidSupplies(run.m_Truck);
  if (left >= 1) extra += string.Format("; %1 stay aboard (FOB full)", Math.Round(left));
  PrintFormat("[ACR] SUPPLYRUN %1: %2 delivered %3 supplies to %4 in %5 s%6", run.m_sFaction, run.m_Crew.GetName(), Math.Round(moved), run.m_Fob.m_sName, Math.Round((GetGame().GetWorld().GetWorldTime() - run.m_fSentAt) / 1000), extra);
  m_Manager.NotifyFaction(run.m_sFaction, string.Format("Supply run: %1 delivered %2 supplies to %3%4", run.m_Crew.GetName(), Math.Round(moved + added), run.m_Fob.m_sName, extra));
  return "delivered";
 }

 protected string Fail(ACR_SupplyRun run, string why)
 {
  if (run.m_Fob)
  {
   m_Fails.Set(run.m_Fob, RecentFails(run.m_Fob) + 1);
   m_FailedAt.Set(run.m_Fob, GetGame().GetWorld().GetWorldTime());
  }
  if (run.m_Crew) run.m_Crew.DropHoldRetry();
  string fobName = "?";
  if (run.m_Fob) fobName = run.m_Fob.m_sName;
  string line = string.Format("Supply run to %1 failed: %2", fobName, why);
  PrintFormat("[ACR] SUPPLYRUN %1: %2", run.m_sFaction, line);
  m_Manager.NotifyFaction(run.m_sFaction, line);
  return "failed";
 }

 protected string Tell(ACR_SupplyRun run, string line, string why)
 {
  PrintFormat("[ACR] SUPPLYRUN %1: %2", run.m_sFaction, line);
  m_Manager.NotifyFaction(run.m_sFaction, line);
  return why;
 }

 // ---------------------------------------------------------------- routes

 // The proven route of 'faction' from near 'from' to near 'to', most drives first; null: none.
 ACR_SupplyRoute PickRoute(string faction, vector from, vector to)
 {
  ACR_SupplyRoute best;
  foreach (ACR_SupplyRoute route : m_Routes)
  {
   if (route.m_sFaction != faction || route.m_bFailed) continue;
   if (vector.DistanceXZ(route.Start(), from) > ROUTE_START_M || vector.DistanceXZ(route.End(), to) > ROUTE_END_M) continue;
   if (best && route.m_iDrives <= best.m_iDrives) continue;
   if (Dangerous(route)) continue;
   best = route;
  }
  return best;
 }

 // An enemy-held base on the way, or a known FIA road (ACR_Insurgency.OnTheWay).
 protected bool Dangerous(ACR_SupplyRoute route)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_sOwner.IsEmpty() || base.m_sOwner == route.m_sFaction || ACR_Insurgency.Friendly(base.m_sOwner, route.m_sFaction)) continue;
   foreach (vector p : route.m_aPoints)
    if (vector.DistanceXZ(p, base.GetOrigin()) < base.m_fRadius + DANGER_BASE_M) return true;
  }
  for (int i = 1; i < route.m_aPoints.Count(); i++)
   if (ACR_Insurgency.OnTheWay(route.m_sFaction, route.m_aPoints[i - 1], route.m_aPoints[i])) return true;
  return false;
 }

 // A sampled track thinned to its bend points (pure, for srv:regress): a point is kept where the
 // track between the last kept point and the next strays more than BEND_M from the straight line,
 // or the next would be more than MAX_GAP_M away.
 static void Thin(notnull array<vector> track, notnull array<vector> result)
 {
  result.Clear();
  if (track.Count() < 2) return;
  result.Insert(track[0]);
  int anchor = 0;
  for (int j = 2; j < track.Count(); j++)
  {
   bool keep = vector.DistanceXZ(track[anchor], track[j]) > MAX_GAP_M;
   for (int k = anchor + 1; k < j && !keep; k++)
    if (ACR_AICommander.DistanceToSegment(track[k], track[anchor], track[j]) > BEND_M) keep = true;
   if (!keep) continue;
   anchor = j - 1;
   result.Insert(track[anchor]);
  }
  result.Insert(track[track.Count() - 1]);
 }

 // Every run that arrives is a route (ruling 11). The same ends again: one more drive, and a route
 // set aside or suspect is restored (and takes this drive's points if it had been set aside).
 void RecordTrack(string faction, notnull array<vector> track, vector arrived)
 {
  array<vector> full = {};
  full.Copy(track);
  if (full.IsEmpty() || vector.DistanceXZ(full[full.Count() - 1], arrived) >= SAMPLE_MIN_M) full.Insert(arrived);
  array<vector> points = {};
  Thin(full, points);
  if (points.Count() < 2) return;
  ACR_SupplyRoute fresh = new ACR_SupplyRoute();
  fresh.m_sFaction = faction;
  fresh.m_aPoints.Copy(points);
  if (fresh.Length() < MIN_ROUTE_M) return;
  foreach (ACR_SupplyRoute route : m_Routes)
  {
   if (route.m_sFaction != faction || vector.DistanceXZ(route.Start(), fresh.Start()) > ROUTE_START_M || vector.DistanceXZ(route.End(), fresh.End()) > ROUTE_END_M) continue;
   if (route.m_bFailed) route.m_aPoints.Copy(points);
   route.m_iDrives++;
   route.m_iFails = 0;
   route.m_bFailed = false;
   PrintFormat("[ACR] ROUTE %1: drive %2 on a recorded route (%3 points, %4 m)", faction, route.m_iDrives, route.m_aPoints.Count(), Math.Round(route.Length()));
   SaveRoutes();
   return;
  }
  fresh.m_iDrives = 1;
  m_Routes.Insert(fresh);
  PrintFormat("[ACR] ROUTE %1: new route recorded, %2 points over %3 m (%4 -> %5)", faction, points.Count(), Math.Round(fresh.Length()), ACR_Manager.Grid(fresh.Start()), ACR_Manager.Grid(fresh.End()));
  TrimRoutes(faction);
  SaveRoutes();
 }

 // Tests: add a route by hand.
 void AddRouteForTest(string faction, notnull array<vector> points)
 {
  ACR_SupplyRoute route = new ACR_SupplyRoute();
  route.m_sFaction = faction;
  route.m_aPoints.Copy(points);
  route.m_iDrives = 1;
  m_Routes.Insert(route);
 }

 protected void TrimRoutes(string faction)
 {
  int count;
  foreach (ACR_SupplyRoute route : m_Routes) if (route.m_sFaction == faction) count++;
  while (count > MOST_ROUTES)
  {
   int worst = -1;
   foreach (int i, ACR_SupplyRoute candidate : m_Routes)
    if (candidate.m_sFaction == faction && (worst < 0 || candidate.m_iDrives < m_Routes[worst].m_iDrives)) worst = i;
   if (worst < 0) return;
   m_Routes.Remove(worst);
   count--;
  }
 }

 protected static string RouteFile()
 {
  string world = FilePath.StripExtension(FilePath.StripPath(GetGame().GetWorldFile()));
  if (world.IsEmpty()) world = "world";
  return "$profile:ACA_Routes_" + world + ".txt";
 }

 protected void SaveRoutes()
 {
  if (!SaveRoutesOn()) return;
  FileHandle file = FileIO.OpenFile(RouteFile(), FileMode.WRITE);
  if (!file) return;
  foreach (ACR_SupplyRoute route : m_Routes) file.WriteLine(route.ToText());
  file.Close();
 }

 protected void LoadRoutes()
 {
  string path = RouteFile();
  if (!FileIO.FileExists(path)) return;
  FileHandle file = FileIO.OpenFile(path, FileMode.READ);
  if (!file) return;
  string line;
  while (file.ReadLine(line) >= 0)
  {
   ACR_SupplyRoute route = ACR_SupplyRoute.FromText(line);
   if (route) m_Routes.Insert(route);
  }
  file.Close();
  PrintFormat("[ACR] ROUTE loaded %1 saved routes from %2", m_Routes.Count(), path);
 }

 // Map detail for a FOB (ACR_FobSupply.Describe).
 string Describe(ACR_Base fob)
 {
  ACR_SupplyRun run = RunTo(fob);
  if (!run) return "";
  if (run.m_iStage == STAGE_LOAD) return string.Format("\nSupply run: %1 loading at %2", run.m_Crew.GetName(), run.m_Hub.m_sName);
  return string.Format("\nSupply run: %1 on its way", run.m_Crew.GetName());
 }
}
