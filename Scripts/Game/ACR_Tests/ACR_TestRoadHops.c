// -acrRoadHopsTest 1 (server-capable, ~4 min): #86, a convoy crawled because the AI driver
// braked and re-planned at every road hop (one every 160 m). A crewed truck drives ~1.9 km
// east along the test range's roads; every second logs its speed, and a stop is a drop below
// 1.5 m/s after it had got going (over 5 m/s). With -acrRoadHopsOffTest 1 (preset roadhopsoff) it
// drives every hop as before, for the A/B; that run only checks the arrival.
class ACR_TestRoadHops
{
 static ACR_Squad s_Truck;
 static vector s_vEnd;
 static vector s_vLast;
 static vector s_vTarget;
 static float s_fDriven;
 static float s_fTopSpeed;
 static int s_iStops;
 static int s_iTargets;
 static int s_iArrived;
 static bool s_bRolling;
 static bool s_bDone;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   if (System.IsCLIParam("acrRoadHopsOffTest")) ACR_Squad.s_bRoadFlowOff = true;
   ACR_FactionSettings soviets = ACR_FactionSettings.Find(ACR_FactionSelection.East());
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find(ACR_FactionSelection.West());
   if (us) us.m_bAICommander = false;
  }
  if (tick == 5)
   manager.SpawnUnit("Truck 1", "", "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et", true, "", h.Ground("1236 0 2795"), h.Ground("1250 0 2795"));
  if (tick == 14)
  {
   s_Truck = h.NamedSquad(manager, "Truck 1");
   IEntity truck;
   if (s_Truck) truck = ACR_Transport.CrewedVehicle(s_Truck);
   if (!truck) { ACR_TestRegress.Abort("roadhops", "no crewed truck"); return; }
   s_vLast = truck.GetOrigin();
   s_Truck.RouteTo(h.Ground("2931 0 2743"), ACR_Squad.WP_MOVE, 25, "Moving", 0);
   s_vEnd = s_Truck.RouteDestination();
   // Each hop and how sharply the road turns there, to line the stops up against.
   array<vector> hops = {};
   array<int> kinds = {};
   s_Truck.GetPlannedPoints(hops, kinds);
   hops.InsertAt(s_Truck.m_MoveTarget, 0);
   hops.InsertAt(s_vLast, 0);
   for (int i = 1; i < hops.Count(); i++)
   {
    float turn = 0;
    if (i + 1 < hops.Count())
    {
     vector a = hops[i] - hops[i - 1];
     vector b = hops[i + 1] - hops[i];
     turn = Math.AbsFloat(Math.Atan2(a[0] * b[2] - a[2] * b[0], a[0] * b[0] + a[2] * b[2]) * Math.RAD2DEG);
    }
    PrintFormat("[ACR] ROADHOPS hop %1 %2, %3 m from the last, turns %4 deg", i, hops[i], (int)vector.DistanceXZ(hops[i - 1], hops[i]), (int)turn);
   }
   ACR_TestRegress.Check("roadhops: the trip is planned along the roads", s_Truck.HasActiveRoute(), string.Format("end %1, road flow %2", s_vEnd, ACR_Squad.RoadFlow()));
  }
  if (!s_Truck || tick <= 14 || s_bDone) return;
  IEntity vehicle = ACR_Transport.CrewedVehicle(s_Truck);
  if (!vehicle || !vehicle.GetPhysics()) return;
  float speed = vehicle.GetPhysics().GetVelocity().Length();
  s_fTopSpeed = Math.Max(s_fTopSpeed, speed);
  s_fDriven += vector.DistanceXZ(s_vLast, vehicle.GetOrigin());
  s_vLast = vehicle.GetOrigin();
  if (s_Truck.m_MoveTarget != s_vTarget)
  {
   s_vTarget = s_Truck.m_MoveTarget;
   s_iTargets++;
   PrintFormat("[ACR] ROADHOPS t=%1 new waypoint %2 at %3 m/s, %4 m ahead", tick, s_vTarget, speed, (int)vector.DistanceXZ(vehicle.GetOrigin(), s_vTarget));
  }
  if (speed > 5) s_bRolling = true;
  else if (speed < 1.5 && s_bRolling)
  {
   s_bRolling = false;
   s_iStops++;
   PrintFormat("[ACR] ROADHOPS stop %1 at t=%2, %3 m driven, %4 m to the hop", s_iStops, tick, (int)s_fDriven, (int)vector.DistanceXZ(vehicle.GetOrigin(), s_Truck.m_MoveTarget));
  }
  float toEnd = vector.DistanceXZ(vehicle.GetOrigin(), s_vEnd);
  if (tick % 5 == 0) PrintFormat("[ACR] ROADHOPS t=%1 speed %2 m/s, driven %3 m, %4 m to go, targets %5, stops %6, order '%7'", tick, speed, (int)s_fDriven, (int)toEnd, s_iTargets, s_iStops, s_Truck.m_Order);
  if (toEnd < 60) s_iArrived = tick;
  if (s_iArrived == 0 && tick < 400) return;
  s_bDone = true;

  float km = Math.Max(0.1, s_fDriven / 1000);
  float stops = s_iStops;
  string summary = string.Format("arrived t=%1, %2 m driven, %3 waypoints, %4 stops (%5 per km), avg %6 m/s, top %7 m/s, road flow %8", s_iArrived, (int)s_fDriven, s_iTargets, s_iStops, stops / km, s_fDriven / Math.Max(1, tick - 14), s_fTopSpeed, ACR_Squad.RoadFlow());
  Print("[ACR] ROADHOPS RESULT " + summary);
  ACR_TestRegress.Check("roadhops: the truck gets there", s_iArrived > 0, summary);
  if (ACR_Squad.RoadFlow())
  {
   ACR_TestRegress.Check("roadhops: fewer waypoints than 160 m hops", s_fDriven / 160 > s_iTargets, summary);
   ACR_TestRegress.Check("roadhops: under two stops per km (old driving 2-2.7)", km * 2 > stops, summary);
  }
  else
  {
   ACR_TestRegress.Skip("roadhops: fewer waypoints than 160 m hops", "old driving");
   ACR_TestRegress.Skip("roadhops: under two stops per km (old driving 2-2.7)", "old driving");
  }
  ACR_TestRegress.Complete("roadhops", 4);
 }
}
