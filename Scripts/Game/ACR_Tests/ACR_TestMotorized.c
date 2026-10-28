// -acrMotorizedTest 1 (server-capable, ~3 min): AI motorized assault. (1) A crewed LAV in the US
// AI's forced attack (debug ATTACK_NOW) is paired with an infantry squad (the test range's targets
// are all long road detours, so that ride is refused as for any truck). (2) The carrier ride
// itself: the LAV picks Bravo up, drives it to a point ~600 m away along the roads, lets it out
// and stays there (an ordinary taxi drives home).
class ACR_TestMotorized
{
 static IEntity s_Lav;
 static ref ACR_Squad s_Bravo;
 static vector s_Dest;
 static bool s_bStarted;

 static ACR_Squad Crew(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (s_Lav && ACR_Transport.CrewedVehicle(squad) == s_Lav) return squad;
  return null;
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
  }
  if (!alpha || !alpha.GetAnchor()) return;
  if (tick == 4)
  {
   s_Lav = ACR_Manager.SpawnPrefab(ACR_TestCrew.LAV, alpha.GetAnchor().GetOrigin() + Vector(30, 0, 20));
   SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(s_Lav.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<ECompartmentType> types = {ECompartmentType.PILOT, ECompartmentType.TURRET};
   if (seats) seats.SpawnDefaultOccupants(types);
  }
  ACR_Squad crew = Crew(manager);
  if (tick == 12)
  {
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = true;
  }
  if (tick == 16)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
    if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == "US") squad.m_iAITask = ACR_EAITask.FREE;
   int before = ACR_AICommander.s_iCarrierTries;
   PrintFormat("[ACR] MOTOR forced attack: %1", manager.m_Debug.Run(manager, ACR_EDebug.ATTACK_NOW, "USSR", ""));
   ACR_TestRegress.Check("motorized: the AI pairs its APC with an infantry squad in an attack", ACR_AICommander.s_iCarrierTries > before, "");
   // Part 2 without the AI commander.
   ACR_FactionSettings us2 = ACR_FactionSettings.Find("US");
   if (us2) us2.m_bAICommander = false;
  }
  if (tick == 22)
  {
   if (manager.m_Rides) { manager.m_Rides.CancelFor(crew); }
   foreach (ACR_Squad squad2 : manager.m_Squads) if (squad2.GetName().Contains("Bravo")) s_Bravo = squad2;
   if (!crew || !s_Bravo || !s_Bravo.GetAnchor()) { Print("[ACR] MOTOR no LAV crew or Bravo"); return; }
   manager.m_Rides.CancelFor(s_Bravo);
   s_Bravo.Halt("Holding");
   crew.Halt("Holding");
   // A destination the roads lead to: 500-900 m away, road no longer than 1.6x straight.
   vector from = s_Bravo.GetAnchor().GetOrigin();
   for (int k = 0; k < 72 && s_Dest == vector.Zero; k++)
   {
    int slot = k % 24;
    int ring = k / 24;
    float angle = Math.PI2 * slot / 24;
    float reach = 500 + ring * 200;
    vector probe = from + Vector(Math.Cos(angle) * reach, 0, Math.Sin(angle) * reach);
    vector road;
    if (!BBB_Terrain.RoadPointNear(probe, 120, road)) continue;
    float straight = vector.DistanceXZ(from, road);
    if (straight < 450) continue;
    array<vector> legs = {};
    int nodes, millis;
    BBB_Roads.Route(from, road, legs, nodes, millis);
    if (BBB_Roads.s_bLastDetour || BBB_Roads.s_fLastLength > straight * 1.6) continue;
    s_Dest = road;
    PrintFormat("[ACR] MOTOR destination %1: %2 m straight, %3 m by road", road, (int)straight, (int)BBB_Roads.s_fLastLength);
   }
   if (s_Dest == vector.Zero) { Print("[ACR] MOTOR no road destination found"); return; }
   string why;
   ACR_Ride ride = manager.m_Rides.RequestCarrier(s_Bravo, crew, s_Dest, ACR_Squad.WP_MOVE, -1, "Moving", 0, why);
   s_bStarted = ride != null && ride.m_bEscort;
   ACR_TestRegress.Check("motorized: carrier ride started", s_bStarted, why);
  }
  if (tick > 22 && tick % 15 == 0 && crew && s_Bravo && s_Bravo.GetAnchor())
   PrintFormat("[ACR] MOTOR t=%1 LAV %2 '%3' | Bravo '%4' mounted %5, %6 m apart, %7 m from the drop", tick, s_Lav.GetOrigin(), crew.m_Order, s_Bravo.m_Order, s_Bravo.IsMounted(), (int)vector.DistanceXZ(s_Lav.GetOrigin(), s_Bravo.GetAnchor().GetOrigin()), (int)vector.DistanceXZ(s_Lav.GetOrigin(), s_Dest));
  if (tick == 170)
  {
   bool dropped, stayed;
   string detail = "no ride";
   if (s_bStarted && crew && s_Bravo && s_Bravo.GetAnchor())
   {
    float apart = vector.DistanceXZ(s_Lav.GetOrigin(), s_Bravo.GetAnchor().GetOrigin());
    float atDrop = vector.DistanceXZ(s_Lav.GetOrigin(), s_Dest);
    dropped = !s_Bravo.IsMounted() && !manager.m_Rides.RideOf(s_Bravo);
    stayed = atDrop < 150 && crew.OrderState() != ACR_EOrderState.RETURN;
    detail = string.Format("Bravo '%1' mounted %2, %3 m apart; LAV '%4' %5 m from the drop", s_Bravo.m_Order, s_Bravo.IsMounted(), (int)apart, crew.m_Order, (int)atDrop);
   }
   ACR_TestRegress.Check("motorized: riders dropped and the LAV stays at the drop-off", dropped && stayed, detail);
   ACR_TestRegress.Complete("motorized", 3);
  }
 }
}
