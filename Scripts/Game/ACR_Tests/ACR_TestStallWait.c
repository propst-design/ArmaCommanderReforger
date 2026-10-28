// -acrStallWaitTest 1 (server-capable, ~90 s): #53. A gun truck on a move order whose gunner
// stands outside it (AI off, BBB's gunner replacement held off so nobody re-seats him) and whose
// driver doesn't drive. The stall clock waits for him: no back-up in 40 s, five times the 8 s
// limit. Once he is back aboard, the truck that still doesn't move is taken as stuck as before.
class ACR_TestStallWait
{
 static ACR_Squad s_Squad;
 static IEntity s_Vehicle;
 static bool s_bBoarding;

 // The driver's AI off: on a move order, the truck doesn't move.
 static void Freeze(IEntity vehicle)
 {
  ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
  if (!driver) return;
  AIControlComponent ai = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
  if (ai && ai.IsAIActivated()) ai.DeactivateAI();
 }

 static string State()
 {
  if (!s_Squad || !s_Vehicle) return "no truck";
  int alive, aboard;
  array<AIAgent> agents = {};
  s_Squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   alive++;
   if (ACR_Transport.VehicleOf(body) == s_Vehicle) aboard++;
  }
  return string.Format("alive=%1 aboard=%2 order='%3' tries=%4 unsticking=%5 boarding seen=%6", alive, aboard, s_Squad.m_Order, s_Squad.UnstickTries(), s_Squad.IsUnsticking(), s_bBoarding);
 }

 static bool AllAboard()
 {
  array<AIAgent> agents = {};
  s_Squad.GetAgents(agents);
  int alive, aboard;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   alive++;
   if (ACR_Transport.VehicleOf(body) == s_Vehicle) aboard++;
  }
  return alive > 0 && aboard == alive;
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
   ACR_Squad.s_fStallSeconds = 8;
  }
  if (tick == 5) manager.SpawnUnit("Wait Truck", "", "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et", true, "", h.Ground("1390 0 2795"), h.Ground("1395 0 2790"));
  if (tick == 14)
  {
   s_Squad = h.NamedSquad(manager, "Wait Truck");
   if (s_Squad) s_Vehicle = ACR_Transport.CrewedVehicle(s_Squad);
   if (!s_Vehicle) { ACR_TestRegress.Abort("stallwait", "the truck or its crew is missing"); return; }
   BBB_VehicleHelpers.s_bGunnerReplace = false;
   array<AIAgent> agents = {};
   s_Squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    ChimeraCharacter gunner = ChimeraCharacter.Cast(agent.GetControlledEntity());
    if (!gunner || ACR_Transport.SeatOf(gunner) != ECompartmentType.TURRET) continue;
    gunner.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
    AIControlComponent gunnerAI = AIControlComponent.Cast(gunner.FindComponent(AIControlComponent));
    if (gunnerAI && gunnerAI.IsAIActivated()) gunnerAI.DeactivateAI();
   }
   s_Squad.MoveTo(h.Ground("1700 0 2800"));
   Freeze(s_Vehicle);
  }
  if (!s_Squad || !s_Vehicle || tick <= 14) return;
  if (!s_Squad.IsUnsticking()) Freeze(s_Vehicle);
  if (tick < 55 && s_Squad.CrewBoarding(s_Vehicle)) s_bBoarding = true;
  if (tick % 5 == 0) PrintFormat("[ACR] STALLWAIT t=%1: %2", tick, State());
  if (tick == 55)
  {
   bool waited = s_bBoarding && s_Squad.UnstickTries() == 0 && !s_Squad.IsUnsticking() && s_Squad.m_Order != "Stuck, holding";
   ACR_TestRegress.Check("stall: a truck waiting for its own gunner to get in isn't taken as stuck (#53)", waited, State());
   // Let BBB seat him again: from now on the truck is simply stuck.
   BBB_VehicleHelpers.s_bGunnerReplace = true;
  }
  if (tick == 85)
  {
   bool backedUp = s_Squad.UnstickTries() > 0 || s_Squad.IsUnsticking() || s_Squad.m_Order == "Stuck, holding";
   ACR_TestRegress.Check("stall: with the crew aboard the same frozen truck is taken as stuck again", AllAboard() && backedUp, State());
   ACR_TestRegress.Complete("stallwait", 2);
  }
  if (tick == 87) Print("[ACR] TEST COMPLETE");
 }
}
