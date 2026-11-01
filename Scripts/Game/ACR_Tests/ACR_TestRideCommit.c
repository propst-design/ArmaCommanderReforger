// -acrRideCommitTest 1 (server-capable, ~4 min): a posted squad keeps its trip and rides on it
// (Jacob 1 Nov, Chernarus: squads walked across the map for half an hour, pulled into new orders
// mid-way). The AI commander (balanced, no RP so no air lift) posts Alpha to a point 1.1-1.6 km off
// with no transport anywhere:
// - Alpha sets off on foot, committed to the trip: not free for other plans;
// - three unarmed Humvees turn up by it: Alpha catches a ride on the way, in a convoy when it is
//   too big for one;
// - posted somewhere else while aboard, the same ride takes it there (no getting out to walk);
// - it is dropped off there, and once it has arrived it is free again.
class ACR_TestRideCommit
{
 static const int CHECKS = 5;
 static const ResourceName HUMVEE = "{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et";
 static ref ACR_AICommander s_AI;
 static ref array<ACR_Squad> s_aJeeps = {};
 static vector s_Far;
 static vector s_Near;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iRP = -1;
 static string s_sDetail;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor()) return;
  if (tick > 380) Fail(manager, string.Format("timed out in step %1 (%2)", s_iStep, s_sDetail));
  if (tick == 4)
  {
   // Nothing that could give a ride yet (the opening purchase may land before tick 1, as in ridecarry).
   array<ACR_Squad> bought = {};
   foreach (ACR_Squad other : manager.m_Squads)
    if (other != alpha && ACR_Transport.CrewedVehicle(other)) bought.Insert(other);
   foreach (ACR_Squad other : bought)
   {
    IEntity vehicle = ACR_Transport.CrewedVehicle(other);
    if (vehicle) SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
    if (other.m_Group) SCR_EntityHelper.DeleteEntityAndChildren(other.m_Group);
   }
   return;
  }
  if (tick < 10 || s_iStep >= 9) return;
  if (!manager.m_Rides) manager.m_Rides = new ACR_Rides(manager);
  int waited = tick - s_iStepAt;
  vector at = alpha.GetAnchor().GetOrigin();
  ACR_Ride ride = manager.m_Rides.RideOf(alpha);

  // 0: posted far off with no transport: on foot, committed.
  if (s_iStep == 0)
  {
   s_Far = FarPoint(at);
   if (s_Far == vector.Zero)
   {
    Fail(manager, "no point 1.1-1.6 km from Alpha with a road that isn't a long way round");
    return;
   }
   ACR_FactionData data = manager.m_Bases.GetFaction("US");
   s_iRP = data.m_iRP;
   data.m_iRP = 0;
   s_AI = new ACR_AICommander(manager, "US");
   s_AI.SetDoctrineForTest(ACR_AIDoctrine.Find("balanced"));
   alpha.m_iAITask = ACR_EAITask.POSTED;
   alpha.SetMode(ACR_EMoveMode.FAST);
   s_AI.TravelForTest(alpha, s_Far, false);
   PrintFormat("[ACR] RIDECOMMIT Alpha (%1 men) posted to %2, %3 m off", alpha.CountAlive(), s_Far, (int)vector.DistanceXZ(at, s_Far));
   Next(tick);
   return;
  }
  if (s_iStep == 1)
  {
   if (waited < 5) return;
   array<ACR_Squad> free = {};
   s_AI.FreeSquadsForTest(free);
   bool committed = s_AI.Committed(alpha) && !free.Contains(alpha) && !ride;
   ACR_TestRegress.Check("ridecommit: posted far off with no transport, Alpha sets off on foot and is committed to the trip (not free for other plans)", committed,
    string.Format("committed %1, in the free pool %2, riding %3, order '%4'", s_AI.Committed(alpha), free.Contains(alpha), ride != null, alpha.m_Order));
   // Three Humvees turn up by it.
   for (int j = 0; j < 3; j++)
   {
    vector spot = ACR_Manager.Ground(at + Vector(30 + j * 12, 0, 20 + j * 8));
    manager.SpawnUnit("Ride Jeep " + (j + 1), "", HUMVEE, true, "", spot, spot + Vector(4, 0, 4));
   }
   Next(tick);
   return;
  }
  // 2: once they are free taxis, the next look for a ride on the way takes them.
  if (s_iStep == 2)
  {
   if (s_aJeeps.Count() < 3)
   {
    s_aJeeps.Clear();
    foreach (ACR_Squad spawned : manager.m_Squads)
     if (spawned.GetName().Contains("Ride Jeep")) s_aJeeps.Insert(spawned);
   }
   int freeTaxis = 0;
   foreach (ACR_Squad jeep : s_aJeeps)
    if (manager.m_Rides.IsFreeTransport(jeep, "US")) freeTaxis++;
   s_sDetail = string.Format("%1 jeeps, %2 free taxis", s_aJeeps.Count(), freeTaxis);
   if (freeTaxis < 3 && waited < 40) return;
   s_AI.CatchRidesForTest();
   ride = manager.m_Rides.RideOf(alpha);
   int seats = 0;
   if (!s_aJeeps.IsEmpty() && ACR_Transport.CrewedVehicle(s_aJeeps[0])) seats = ACR_Rides.FreeSeats(ACR_Transport.CrewedVehicle(s_aJeeps[0]));
   bool caught = ride != null && ride.m_Rider == alpha && vector.DistanceXZ(ride.m_Final, s_Far) < 1;
   ACR_TestRegress.Check("ridecommit: walking, Alpha catches a ride on the way to the same place", caught,
    string.Format("%1; ride %2, to %3", s_sDetail, ride != null, ACR_Manager.Grid(s_Far)));
   if (!caught)
   {
    ACR_TestRegress.Complete("ridecommit", CHECKS);
    Finish(manager);
    return;
   }
   if (alpha.CountAlive() > seats)
    ACR_TestRegress.Check("ridecommit: too big for one Humvee, Alpha gets a convoy", ride.m_aExtraTaxis.Count() >= 1,
     string.Format("%1 men, %2 seats a Humvee, %3", alpha.CountAlive(), seats, ACR_Rides.TaxiNames(ride)));
   else
    ACR_TestRegress.Skip("ridecommit: too big for one Humvee, Alpha gets a convoy", string.Format("Alpha has %1 men, one Humvee seats %2", alpha.CountAlive(), seats));
   Next(tick);
   return;
  }
  // 3: aboard and on the way: posted somewhere else, the ride takes it there.
  if (s_iStep == 3)
  {
   int aboardNow = ACR_TestGetOut.Aboard(alpha);
   s_sDetail = string.Format("%1 aboard", aboardNow);
   if (ride) s_sDetail += string.Format(", ride state %1", ride.m_State);
   if (!ride || ride.m_State != ACR_ERideState.TO_DROP)
   {
    if (!ride || waited > 150) Fail(manager, "the ride never got under way: " + s_sDetail);
    return;
   }
   if (waited < 8) return;
   s_Near = NearPoint(ride.m_Vehicle.GetOrigin(), s_Far);
   if (s_Near == vector.Zero)
   {
    Fail(manager, "no road point 550-950 m from the ride for the new posting");
    return;
   }
   s_AI.TravelForTest(alpha, s_Near, false);
   ACR_Ride after = manager.m_Rides.RideOf(alpha);
   bool redirected = after == ride && vector.DistanceXZ(after.m_Final, s_Near) < 1 && after.m_State == ACR_ERideState.TO_DROP && alpha.OrderState() == ACR_EOrderState.RIDE && s_AI.Committed(alpha);
   ACR_TestRegress.Check("ridecommit: posted elsewhere while aboard, the same ride takes Alpha there (no getting out to walk)", redirected,
    string.Format("%1; same ride %2, to %3, order '%4', committed %5", s_sDetail, after == ride, ACR_Manager.Grid(s_Near), alpha.m_Order, s_AI.Committed(alpha)));
   Next(tick);
   return;
  }
  // 4: dropped off there; once arrived, free again.
  if (s_iStep == 4)
  {
   int aboard = ACR_TestGetOut.Aboard(alpha);
   float left = vector.DistanceXZ(at, s_Near);
   s_sDetail = string.Format("%1 aboard, %2 m from the new posting, order '%3'", aboard, (int)left, alpha.m_Order);
   if ((ride || aboard > 0 || left > ACR_AICommander.TRIP_ARRIVED) && waited < 200) return;
   array<ACR_Squad> freeAfter = {};
   s_AI.FreeSquadsForTest(freeAfter);
   bool arrived = !ride && aboard == 0 && left <= ACR_AICommander.TRIP_ARRIVED && freeAfter.Contains(alpha);
   ACR_TestRegress.Check("ridecommit: Alpha is dropped off at the new posting, gets out, and is free again once there", arrived,
    string.Format("%1 after %2 s; in the free pool %3", s_sDetail, waited, freeAfter.Contains(alpha)));
   ACR_TestRegress.Complete("ridecommit", CHECKS);
   Finish(manager);
  }
 }

 protected static void Next(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 protected static void Fail(ACR_Manager manager, string why)
 {
  if (s_iStep >= 9) return;
  ACR_TestRegress.Abort("ridecommit", why);
  Finish(manager);
 }

 protected static void Finish(ACR_Manager manager)
 {
  s_iStep = 9;
  if (s_iRP >= 0) manager.m_Bases.GetFaction("US").m_iRP = s_iRP;
 }

 // A point 1.1-1.6 km from 'from' that a ride would take (the road there isn't a long way round).
 protected static vector FarPoint(vector from)
 {
  for (int k = 0; k < 48; k++)
  {
   int slot = k % 16;
   float angle = Math.PI2 * slot / 16;
   float reach = 1100 + (k / 16) * 250; // int-math ok (rings)
   vector point = ACR_Manager.Ground(from + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach));
   vector road;
   if (!ACR_Manager.OnMap(point) || !BBB_Terrain.RoadPointNear(point, 150, road)) continue;
   if (!RoadFine(from, road)) continue;
   return road;
  }
  return vector.Zero;
 }

 // A road point 550-950 m from 'from' (far enough to be a committed trip), away from 'avoid' if it can.
 protected static vector NearPoint(vector from, vector avoid)
 {
  for (int k = 0; k < 48; k++)
  {
   int slot = k % 24;
   float angle = Math.PI2 * slot / 24;
   float reach = 650 + (k / 24) * 200; // int-math ok (rings)
   vector road;
   if (!BBB_Terrain.RoadPointNear(from + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach), 120, road)) continue;
   float d = vector.DistanceXZ(from, road);
   if (d < ACR_AICommander.TRIP_COMMIT_MIN + 50 || d > 950 || vector.DistanceXZ(road, avoid) < 300) continue;
   if (!RoadFine(from, road)) continue;
   return road;
  }
  return vector.Zero;
 }

 protected static bool RoadFine(vector from, vector to)
 {
  array<vector> legs = {};
  int nodes, millis;
  BBB_Roads.Route(from, to, legs, nodes, millis);
  return !BBB_Roads.s_bLastDetour && BBB_Roads.s_fLastLength <= vector.DistanceXZ(from, to) * 1.6;
 }
}
