// -acrDriveFlowTest 1 (server-capable, ~7 min): #86 came back on 20 Nov (Jacob: "AI drivers pause at
// every waypoint in their route"). A crewed truck drives two multi-waypoint trips on the test range's
// east road and back, and every second its speed is checked against the trip's intermediate waypoints:
//  A. an ordered move over road hops (RouteTo with road hops on, as -acrNoRoadHops 0, the same hops DriveWatch's
//     re-route and a failed move fall back to);
//  B. a supply run's recorded route replayed (the road track thinned to bend points by
//     ACR_SupplyRuns.Thin, driven at FAST through ACR_Squad.WalkLegs, as ACR_SupplyRuns.Send does).
// A dip is a drop below DIP_MS within NEAR_M of an intermediate waypoint after it had got going (over
// 5 m/s); each waypoint counts once. A stop-start trip dips at nearly every one. Logs "[ACR] FLOW ...".
// VERDICT: per trip, it is a multi-waypoint trip, it arrives, and at most MAX_DIPS dips (6 checks).
class ACR_TestDriveFlow
{
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static const float DIP_MS = 2.0;
 static const float NEAR_M = 45;
 static const int MAX_DIPS = 1; // a junction or a slow corner, not one per waypoint
 static const int TRIP_LIMIT = 230; // s per trip
 static ACR_Squad s_Truck;
 static ref array<vector> s_aPoints; // the trip's intermediate waypoints
 static ref array<bool> s_aDipped;
 static vector s_vEnd;
 static string s_sTrip;
 static int s_iTrip;     // 0 none yet, 1 A, 2 B, 3 done
 static int s_iStarted;
 static int s_iDips;
 static int s_iStops;
 static int s_iTargets;
 static vector s_vTarget;
 static bool s_bRolling;
 static float s_fDriven;
 static vector s_vLast;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   s_aPoints = {};
   s_aDipped = {};
   ACR_Squad.s_bRoadHopsOn = true; // trip A: an ordered move over road hops
   ACR_FactionSettings soviets = ACR_FactionSettings.Find(ACR_FactionSelection.East());
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find(ACR_FactionSelection.West());
   if (us) us.m_bAICommander = false;
  }
  if (tick == 5)
   manager.SpawnUnit("Flow Truck", "", TRUCK, true, "", h.Ground("1236 0 2795"), h.Ground("1250 0 2795"));
  if (tick == 14)
  {
   s_Truck = h.NamedSquad(manager, "Flow Truck");
   if (!s_Truck || !ACR_Transport.CrewedVehicle(s_Truck)) { ACR_TestRegress.Abort("driveflow", "no crewed truck"); return; }
   StartTrip(h, tick, 1);
  }
  if (!s_Truck || s_iTrip < 1 || s_iTrip > 2) return;
  IEntity vehicle = ACR_Transport.CrewedVehicle(s_Truck);
  if (!vehicle || !vehicle.GetPhysics()) return;
  vector here = vehicle.GetOrigin();
  float speed = vehicle.GetPhysics().GetVelocity().Length();
  s_fDriven += vector.DistanceXZ(s_vLast, here);
  s_vLast = here;
  if (s_Truck.m_MoveTarget != s_vTarget)
  {
   s_vTarget = s_Truck.m_MoveTarget;
   s_iTargets++;
   PrintFormat("[ACR] FLOW %1 t=%2 new waypoint %3 at %4 m/s, %5 m ahead", s_sTrip, tick, s_vTarget, speed, (int)vector.DistanceXZ(here, s_vTarget));
  }
  if (speed > 5) s_bRolling = true;
  else if (speed < DIP_MS && s_bRolling)
  {
   s_bRolling = false;
   s_iStops++;
   int near = -1;
   float best = NEAR_M;
   foreach (int i, vector p : s_aPoints)
   {
    float d = vector.DistanceXZ(here, p);
    if (d < best) { best = d; near = i; }
   }
   if (near >= 0 && !s_aDipped[near])
   {
    s_aDipped[near] = true;
    s_iDips++;
   }
   PrintFormat("[ACR] FLOW %1 stop %2 at t=%3, %4 m driven, nearest waypoint %5 (%6 m), order '%7'", s_sTrip, s_iStops, tick, (int)s_fDriven, near, (int)best, s_Truck.m_Order);
  }
  float toEnd = vector.DistanceXZ(here, s_vEnd);
  if (tick % 10 == 0) PrintFormat("[ACR] FLOW %1 t=%2 speed %3 m/s, %4 m to go, waypoints %5, stops %6, dips %7", s_sTrip, tick, speed, (int)toEnd, s_iTargets, s_iStops, s_iDips);
  bool arrived = toEnd < 60;
  if (!arrived && tick - s_iStarted < TRIP_LIMIT) return;
  string summary = string.Format("%1: arrived %2 after %3 s, %4 m, %5 intermediate waypoints, %6 waypoints given, %7 stops, %8 dips at waypoints, %9 hop moves carried on", s_sTrip, arrived, tick - s_iStarted, (int)s_fDriven, s_aPoints.Count(), s_iTargets, s_iStops, s_iDips, ACR_Squad.s_iFlowsKept);
  Print("[ACR] FLOW RESULT " + summary);
  ACR_TestRegress.Check("driveflow " + s_sTrip + ": the truck gets there", arrived, summary);
  ACR_TestRegress.Check("driveflow " + s_sTrip + ": no stop at each waypoint (at most " + MAX_DIPS + " dips)", s_iDips <= MAX_DIPS, summary);
  if (s_iTrip == 1) StartTrip(h, tick, 2);
  else
  {
   s_iTrip = 3;
   ACR_TestRegress.Complete("driveflow", 6);
  }
 }

 protected static void StartTrip(ACR_TestHarness h, int tick, int trip)
 {
  IEntity vehicle = ACR_Transport.CrewedVehicle(s_Truck);
  if (!vehicle) { ACR_TestRegress.Abort("driveflow", "the truck lost its crew"); return; }
  s_iTrip = trip;
  s_iStarted = tick;
  s_iDips = 0;
  s_iStops = 0;
  s_iTargets = 0;
  s_fDriven = 0;
  s_bRolling = false;
  s_vLast = vehicle.GetOrigin();
  s_vTarget = vector.Zero;
  s_aPoints.Clear();
  s_aDipped.Clear();
  if (trip == 1)
  {
   s_sTrip = "ordered move";
   s_Truck.RouteTo(h.Ground("2931 0 2743"), ACR_Squad.WP_MOVE, 25, "Moving", 0);
   s_vEnd = s_Truck.RouteDestination();
  }
  else
  {
   s_sTrip = "supply route";
   array<vector> track = {};
   int nodes, millis;
   BBB_Roads.Route(vehicle.GetOrigin(), h.Ground("1236 0 2795"), track, nodes, millis);
   track.InsertAt(vehicle.GetOrigin(), 0);
   array<vector> legs = {};
   ACR_SupplyRuns.Thin(track, legs);
   if (!legs.IsEmpty()) legs.RemoveOrdered(0); // where it stands
   if (legs.IsEmpty()) legs.Insert(h.Ground("1236 0 2795"));
   s_vEnd = legs[legs.Count() - 1];
   s_Truck.SetMode(ACR_EMoveMode.FAST);
   s_Truck.WalkLegs(legs, ACR_Squad.WP_MOVE, -1, "Supply run to the test FOB", 0);
  }
  array<vector> planned = {};
  array<int> kinds = {};
  s_Truck.GetPlannedPoints(planned, kinds);
  // The waypoint it is on now and every leg after it, less the trip's end.
  if (vector.DistanceXZ(s_Truck.m_MoveTarget, s_vEnd) > 30) s_aPoints.Insert(s_Truck.m_MoveTarget);
  foreach (vector p : planned)
  {
   if (vector.DistanceXZ(p, s_vEnd) <= 30) continue;
   s_aPoints.Insert(p);
  }
  foreach (vector q : s_aPoints) s_aDipped.Insert(false);
  PrintFormat("[ACR] FLOW %1 starts at t=%2: %3 intermediate waypoints, end %4", s_sTrip, tick, s_aPoints.Count(), s_vEnd);
  ACR_TestRegress.Check("driveflow " + s_sTrip + ": a multi-waypoint trip (3 or more on the way)", s_aPoints.Count() >= 3, string.Format("%1 waypoints on the way", s_aPoints.Count()));
 }
}
