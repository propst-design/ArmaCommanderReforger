// -acrGetOutTest 1 (server-capable): a squad that boarded a vehicle crewed by
// another squad (a requisitioned Humvee's driver) must get out on Get out.
// Live 1 Oct MP: the passengers stayed "mounted" after Get out from the panel.
class ACR_TestGetOut
{
 static IEntity s_Taxi;

 static int Aboard(ACR_Squad squad)
 {
  int count;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && character.IsInVehicle()) count++;
  }
  return count;
 }

 static void Run(ACR_TestHarness harness, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !harness.m_Range) return;
  ACR_Squad scout = manager.FindSquadByGroup(harness.m_Range.m_Group);
  ACR_Squad alpha = scout;
  if (tick == 1)
  {
   ACR_Squad.s_bNoGetOutFallback = System.IsCLIParam("acrGetOutOldTest");
   PrintFormat("[ACR] GETOUT game waypoint only: %1", ACR_Squad.s_bNoGetOutFallback);
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (!alpha) return;
  if (tick == 5)
  {
   vector at = alpha.GetAnchor().GetOrigin();
   manager.SpawnUnit("Taxi", "", "{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et", true, "", ACR_Manager.Ground(at + Vector(12, 0, 0)), ACR_Manager.Ground(at + Vector(16, 0, 4)));
  }
  if (tick == 8)
  {
   s_Taxi = alpha.FindVehicleNear(alpha.GetAnchor().GetOrigin() + Vector(12, 0, 0), 8);
   PrintFormat("[ACR] GETOUT taxi %1, Alpha get in: %2", s_Taxi != null, s_Taxi && alpha.GetIn(s_Taxi.GetOrigin()));
  }
  if (tick > 8 && tick <= 40 && tick % 5 == 0)
   PrintFormat("[ACR] GETOUT t%1 boarding: aboard %2 | %3", tick, Aboard(alpha), alpha.Describe());
  // Live case: Jacob was in the driver's body (possessed), so the driver had no AI.
  if (tick == 41 && s_Taxi)
  {
   SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(s_Taxi.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<BaseCompartmentSlot> pilots = {};
   if (seats) seats.GetCompartmentsOfType(pilots, ECompartmentType.PILOT);
   IEntity driver;
   if (!pilots.IsEmpty()) driver = pilots[0].GetOccupant();
   AIControlComponent control;
   if (driver) control = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
   if (control) control.DeactivateAI();
   PrintFormat("[ACR] GETOUT driver AI off (like a possessed driver): %1", control != null);
   // ...and he had driven off, leaving the man who didn't fit far behind.
   vector away = ACR_Manager.Ground(s_Taxi.GetOrigin() + Vector(0, 0, 250));
   s_Taxi.SetOrigin(away + Vector(0, 1, 0));
   Physics physics = s_Taxi.GetPhysics();
   if (physics) physics.SetVelocity(vector.Zero);
   PrintFormat("[ACR] GETOUT taxi moved 250 m from the man left behind");
  }
  if (tick == 42)
  {
   PrintFormat("[ACR] GETOUT before Get out: aboard %1 | %2", Aboard(alpha), alpha.Describe());
   PrintFormat("[ACR] GETOUT Get out: %1", alpha.GetOut());
  }
  if (tick > 42 && tick <= 72 && tick % 5 == 0)
   PrintFormat("[ACR] GETOUT t%1 after: aboard %2 | %3", tick, Aboard(alpha), alpha.Describe());
  if (tick == 73)
  {
   int left = Aboard(alpha);
   // getoutold is the control run, the game's Get out waypoint alone (no EnsureGetOut): there the
   // passengers staying aboard is what shows the fallback is needed (#54, as crewold in #56).
   if (ACR_Squad.s_bNoGetOutFallback)
    ACR_TestRegress.Check("control: the game's Get out waypoint alone leaves passengers in another squad's vehicle aboard", left > 0, string.Format("still aboard %1", left));
   else
    ACR_TestRegress.Check("passengers in another squad's vehicle get out on Get out", left == 0, string.Format("still aboard %1", left));
   ACR_TestRegress.Complete("getout", 1);
  }
 }
}
