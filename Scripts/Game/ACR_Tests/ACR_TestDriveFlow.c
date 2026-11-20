// -acrDriveFlowTest 1 (server-capable, ~7 min): #86 came back on 20 Nov (Jacob: "AI drivers pause at
// every waypoint in their route"). On Reforger 1.9 every waypoint change ends the group's move and the
// driver plans again from a stop, so a crewed vehicle drives each trip on one waypoint (Jacob, 20 Nov).
// A crewed truck drives two trips on the test range's east road and back:
//  A. an ordered move (RouteTo, ~1.8 km);
//  B. a supply run's recorded route replayed (the road track thinned to bend points by
//     ACR_SupplyRuns.Thin, driven at FAST through ACR_Squad.WalkLegs, as ACR_SupplyRuns.Send does).
// Every second the truck's speed is read. A dip is a drop below DIP_MS after it had got going (over
// 5 m/s) within NEAR_M of a spot where its waypoint changed; stops elsewhere (the hill ~600 m out
// stops it on every build, 18 Nov's included) don't count. Logs "[ACR] FLOW ...".
// VERDICT: per trip, the input is a real trip, it arrives, one waypoint for the whole trip, and no dip
// where the waypoint changed (8 checks).
class ACR_TestDriveFlow
{
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static const float DIP_MS = 2.0;
 static const float NEAR_M = 45;
 static const int TRIP_LIMIT = 230; // s per trip
 static ACR_Squad s_Truck;
 static ref array<vector> s_aSwitches; // where the truck was when its waypoint changed mid-trip
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
   s_aSwitches = {};
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
   if (s_iTargets > 0) s_aSwitches.Insert(here);
   s_vTarget = s_Truck.m_MoveTarget;
   s_iTargets++;
   PrintFormat("[ACR] FLOW %1 t=%2 waypoint %3: %4 at %5 m/s, %6 m ahead", s_sTrip, tick, s_iTargets, s_vTarget, speed, (int)vector.DistanceXZ(here, s_vTarget));
  }
  if (speed > 5) s_bRolling = true;
  else if (speed < DIP_MS && s_bRolling)
  {
   s_bRolling = false;
   s_iStops++;
   float best = 99999;
   foreach (vector p : s_aSwitches) best = Math.Min(best, vector.DistanceXZ(here, p));
   bool dip = best <= NEAR_M;
   if (dip) s_iDips++;
   PrintFormat("[ACR] FLOW %1 stop %2 at t=%3, %4 m driven, %5 m from a waypoint change -> dip %6, order '%7'", s_sTrip, s_iStops, tick, (int)s_fDriven, (int)best, dip, s_Truck.m_Order);
  }
  float toEnd = vector.DistanceXZ(here, s_vEnd);
  if (tick % 10 == 0) PrintFormat("[ACR] FLOW %1 t=%2 speed %3 m/s, %4 m to go, waypoints %5, stops %6, dips %7", s_sTrip, tick, speed, (int)toEnd, s_iTargets, s_iStops, s_iDips);
  bool arrived = toEnd < 60;
  if (!arrived && tick - s_iStarted < TRIP_LIMIT) return;
  string summary = string.Format("%1: arrived %2 after %3 s, %4 m, %5 waypoints given, %6 stops, %7 dips where the waypoint changed", s_sTrip, arrived, tick - s_iStarted, (int)s_fDriven, s_iTargets, s_iStops, s_iDips);
  Print("[ACR] FLOW RESULT " + summary);
  ACR_TestRegress.Check("driveflow " + s_sTrip + ": the truck gets there", arrived, summary);
  ACR_TestRegress.Check("driveflow " + s_sTrip + ": one waypoint for the whole trip", s_iTargets == 1, summary);
  ACR_TestRegress.Check("driveflow " + s_sTrip + ": no stop where the waypoint changed", s_iDips == 0, summary);
  if (s_iTrip == 1) StartTrip(h, tick, 2);
  else
  {
   s_iTrip = 3;
   ACR_TestRegress.Complete("driveflow", 8);
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
  s_aSwitches.Clear();
  if (trip == 1)
  {
   s_sTrip = "ordered move";
   vector goal = h.Ground("2931 0 2743");
   float length = vector.DistanceXZ(vehicle.GetOrigin(), goal);
   ACR_TestRegress.Check("driveflow ordered move: a long trip (over 1.5 km)", length > 1500, string.Format("%1 m", (int)length));
   s_Truck.RouteTo(goal, ACR_Squad.WP_MOVE, 25, "Moving", 0);
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
   ACR_TestRegress.Check("driveflow supply route: a recorded route of 3 or more legs", legs.Count() >= 3, string.Format("%1 legs", legs.Count()));
   s_Truck.SetMode(ACR_EMoveMode.FAST);
   s_Truck.WalkLegs(legs, ACR_Squad.WP_MOVE, -1, "Supply run to the test FOB", 0);
   s_vEnd = s_Truck.RouteDestination();
  }
  PrintFormat("[ACR] FLOW %1 starts at t=%2, end %3", s_sTrip, tick, s_vEnd);
 }
}
