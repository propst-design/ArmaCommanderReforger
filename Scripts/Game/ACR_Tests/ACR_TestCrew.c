// -acrCrewTest 1 (server-capable, ~1 min): an armed vehicle crew keeps its seats through a
// Defend order and the order after it (the game's Defend waypoint ordered turret crews out:
// LAV crew bailing out, 2 Oct live), and Reinforce's vehicle service prices and refills it.
class ACR_TestCrew
{
 static const ResourceName LAV = "{0FBF8F010F81A4E5}Prefabs/Vehicles/Wheeled/LAV25/LAV25.et";
 static IEntity s_Lav;
 static ref ACR_Roadblock s_Roadblock;
 static SCR_AIGroup s_RoadblockGroup;
 static IEntity s_PostGunner;
 static int s_PostDepartures;
 static vector s_PostFrom;
 static ref ACR_TestRoadblockPlanner s_PostPlanner;

 static ACR_Squad Crew(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (ACR_Transport.CrewedVehicle(squad) == s_Lav && s_Lav) return squad;
  return null;
 }

 static vector s_From;
 static IEntity DriverOf(IEntity vehicle)
 {
  CarControllerComponent car = CarControllerComponent.Cast(vehicle.FindComponent(CarControllerComponent));
  if (!car || !car.GetPilotCompartmentSlot()) return null;
  return car.GetPilotCompartmentSlot().GetOccupant();
 }

 static int Seated(ACR_Squad squad)
 {
  int seated;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter c = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (c && c.IsInVehicle()) seated++;
  }
  return seated;
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   ACR_Squad.s_bGameDefend = System.IsCLIParam("acrCrewOldTest");
   PrintFormat("[ACR] CREW game Defend waypoint for crews: %1", ACR_Squad.s_bGameDefend);
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (!alpha || !alpha.GetAnchor()) return;
  vector home = alpha.m_Home;
  if (tick == 4)
  {
   s_Lav = ACR_Manager.SpawnPrefab(LAV, home + Vector(40, 0, 20));
   SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(s_Lav.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<ECompartmentType> types = {ECompartmentType.PILOT, ECompartmentType.TURRET};
   if (seats) seats.SpawnDefaultOccupants(types);
  }
  ACR_Squad crew = Crew(manager);
  if (tick == 4)
  {
   vector post = ACR_Manager.Ground(home + Vector(-40, 0, 35));
   s_Roadblock = ACR_Roadblocks.SpawnRoadblock("US", post, 0, "Retention test");
   s_RoadblockGroup = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(ACR_TestRegress.STALE_GROUP, post + Vector(5, 0, 0)));
  }
  ACR_Squad postSquad = manager.FindSquadByGroup(s_RoadblockGroup);
  if (tick == 8)
  {
   ACR_TestRegress.Check("roadblock: real turret and squad fixture", postSquad && s_Roadblock && s_Roadblock.m_Turret);
   if (postSquad && s_Roadblock)
   {
    postSquad.m_ActiveRoadblock = s_Roadblock;
    postSquad.m_RoadblockTurret = s_Roadblock.m_Turret;
    postSquad.m_vRoadblockSite = s_Roadblock.m_vPos;
    postSquad.IssueWaypoint(ACR_Squad.WP_DEFEND, s_Roadblock.m_vPos, 15);
    postSquad.SetOrder("Holding roadblock", ACR_EOrderState.DEFEND);
    postSquad.m_OrderKind = ACR_EOrderKind.DEFEND;
   }
  }
  if (tick == 35)
  {
   if (postSquad && s_Roadblock && s_Roadblock.m_Turret)
   {
    SCR_BaseCompartmentManagerComponent postSeats = SCR_BaseCompartmentManagerComponent.Cast(s_Roadblock.m_Turret.FindComponent(SCR_BaseCompartmentManagerComponent));
    array<BaseCompartmentSlot> postSlots = {};
    if (postSeats) postSeats.GetCompartmentsOfType(postSlots, ECompartmentType.TURRET);
    if (!postSlots.IsEmpty()) s_PostGunner = postSlots[0].GetOccupant();
    s_PostDepartures = postSquad.m_iUnexpectedCrewDepartures;
   }
   ACR_TestRegress.Check("roadblock: native behavior physically mounts the gun", s_PostGunner != null);
  }
  if (tick == 65)
  {
   bool retained = postSquad && s_PostGunner && s_Roadblock && ACR_Transport.VehicleOf(s_PostGunner) == s_Roadblock.m_Turret && postSquad.m_iUnexpectedCrewDepartures == s_PostDepartures;
   ACR_TestRegress.Check("roadblock: same gunner remains aboard for 30 seconds", retained);
   if (postSquad)
   {
    s_PostFrom = postSquad.GetAnchor().GetOrigin();
    s_PostPlanner = new ACR_TestRoadblockPlanner(manager, "US");
    postSquad.m_iAITask = ACR_EAITask.ROADBLOCK;
    postSquad.SetOrder("Construction guard fixture", ACR_EOrderState.BUILD_ROADBLOCK);
    s_PostPlanner.ReleaseBuilders();
    ACR_TestRegress.Check("roadblock: planner leaves ongoing construction assigned", postSquad.m_iAITask == ACR_EAITask.ROADBLOCK);
    postSquad.SetOrder("Holding roadblock", ACR_EOrderState.DEFEND);
    postSquad.m_Commander = 991;
    s_PostPlanner.ReleaseBuilders();
    ACR_TestRegress.Check("roadblock: planner leaves player-controlled squad assigned", postSquad.m_iAITask == ACR_EAITask.ROADBLOCK);
    postSquad.m_Commander = 0;
    // A finished builder holding its roadblock stays on it while the doctrine's holders allow
    // (AI defences, #25); with none allowed it rejoins normal idle planning.
    s_PostPlanner.ReleaseBuilders();
    ACR_TestRegress.Check("roadblock: finished builder keeps holding within the doctrine's holders", postSquad.m_iAITask == ACR_EAITask.ROADBLOCK && postSquad.m_ActiveRoadblock == s_Roadblock);
    ACR_AIDoctrine noHolders = new ACR_AIDoctrine();
    noHolders.m_iHolders = 0;
    s_PostPlanner.SetDoctrineForTest(noHolders);
    s_PostPlanner.ReleaseBuilders();
    ACR_TestRegress.Check("roadblock: finished builder rejoins normal idle planning", postSquad.m_iAITask == ACR_EAITask.POSTED && postSquad.m_ActiveRoadblock == s_Roadblock);
    ACR_Base defendTarget;
    float nearestTarget = float.MAX;
    foreach (ACR_Base friendly : ACR_Base.s_All)
    {
     if (friendly.m_sOwner != "US") continue;
     float targetDistance = vector.DistanceXZ(friendly.GetOrigin(), s_PostFrom);
     if (targetDistance < nearestTarget) { nearestTarget = targetDistance; defendTarget = friendly; }
    }
    if (defendTarget) s_PostPlanner.DefendWith(postSquad, defendTarget);
    ACR_TestRegress.Check("roadblock: ordinary defender planner recalls completed builder", defendTarget && postSquad.m_iAITask == ACR_EAITask.DEFEND && postSquad.m_AITarget == defendTarget && postSquad.m_ActiveRoadblock == null);
   }
  }
  if (tick == 72)
   ACR_TestRegress.Check("roadblock: a new order releases the gunner", s_PostGunner && s_Roadblock && ACR_Transport.VehicleOf(s_PostGunner) != s_Roadblock.m_Turret && postSquad && postSquad.m_ActiveRoadblock == null);

  if (tick == 10)
  {
   if (!crew) { ACR_TestRegress.Check("crew: LAV crew registered", false, ""); return; }
   PrintFormat("[ACR] CREW %1 seated %2, defend: %3", crew.GetName(), Seated(crew), crew.Defend(ACR_Manager.Ground(home + Vector(70, 0, 40))));
   // srv:crewold is the control run: crews get the game's Defend waypoint, as before the fix.
   if (ACR_Squad.s_bGameDefend) ACR_TestRegress.Check("crewold (control): a vehicle crew's Defend is the game's Defend waypoint, as before the fix", SCR_DefendWaypoint.Cast(crew.m_Waypoint) != null, string.Format("%1", crew.m_Waypoint));
   else ACR_TestRegress.Check("crew: a vehicle crew's Defend is a plain move for the game", SCR_DefendWaypoint.Cast(crew.m_Waypoint) == null, string.Format("%1", crew.m_Waypoint));
  }
  if (tick == 30 && crew)
  {
   PrintFormat("[ACR] CREW after Defend: seated %1 | %2", Seated(crew), crew.Describe());
   ACR_TestRegress.Check("crew: still aboard while defending", Seated(crew) >= 2, string.Format("%1 seated", Seated(crew)));
   crew.MoveTo(ACR_Manager.Ground(home + Vector(120, 0, 60)));
  }
  if (tick == 50 && crew)
  {
   PrintFormat("[ACR] CREW after the next order: seated %1 | %2", Seated(crew), crew.Describe());
   ACR_TestRegress.Check("crew: still aboard after the order replacing Defend", Seated(crew) >= 2, string.Format("%1 seated", Seated(crew)));
   // Reinforce's vehicle service: empty guns and some damage cost RP, then all is full again.
   array<BaseMagazineComponent> magazines = {};
   ACR_Logistics.TurretMagazines(s_Lav, magazines);
   foreach (BaseMagazineComponent magazine : magazines) magazine.SetAmmoCount(magazine.GetMaxAmmoCount() / 4);
   SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(s_Lav.FindComponent(SCR_DamageManagerComponent));
   if (damage) damage.SetHealthScaled(0.6);
   int cost = ACR_Requisition.ServiceCost(s_Lav);
   array<IEntity> list = {s_Lav};
   int served = manager.m_Requisition.ServiceVehicles(list);
   float ammo = ACR_Logistics.VehicleAmmo(s_Lav);
   PrintFormat("[ACR] CREW service: %1 magazines, cost %2 RP, served %3, ammo now %4, health %5", magazines.Count(), cost, served, ammo, ACR_Logistics.Health(s_Lav));
   ACR_TestRegress.Check("crew: rearm + repair priced by what's missing", cost > 5, string.Format("%1 RP", cost));
   ACR_TestRegress.Check("crew: vehicle rearmed and repaired", served == 1 && ammo > 0.99 && ACR_Logistics.Health(s_Lav) > 0.98, string.Format("ammo %1 health %2", ammo, ACR_Logistics.Health(s_Lav)));
   // Engine damage at full health (Everon night soak 26 Oct: a gunship serviced on its pad
   // every minute for 36 min, never repaired) is repaired too.
   int movedServed = 0;
   float moveBefore = -1;
   if (damage)
   {
    damage.SetMovementDamage(0.7);
    moveBefore = damage.GetMovementDamage();
    movedServed = manager.m_Requisition.ServiceVehicles(list);
   }
   float moveAfter = -1;
   if (damage) moveAfter = damage.GetMovementDamage();
   ACR_TestRegress.Check("crew: service repairs engine damage on a vehicle at full health", movedServed == 1 && moveAfter >= 0 && moveAfter < 0.01, string.Format("served %1, movement damage %2 -> %3, health %4", movedServed, moveBefore, moveAfter, ACR_Logistics.Health(s_Lav)));
   // As if a player had dropped control and left the driver's and the vehicle's AI parked.
   ChimeraCharacter driver = ChimeraCharacter.Cast(DriverOf(s_Lav));
   AIControlComponent driverAI;
   if (driver) driverAI = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
   if (driverAI && driverAI.IsAIActivated()) driverAI.DeactivateAI();
   AIControlComponent vehicleAI = AIControlComponent.Cast(s_Lav.FindComponent(ChimeraAIVehicleControlComponent));
   if (vehicleAI && vehicleAI.IsAIActivated()) vehicleAI.DeactivateAI();
  }
  if (tick == 52 && crew)
  {
   crew.WakeCrew("test");
   ChimeraCharacter woken = ChimeraCharacter.Cast(DriverOf(s_Lav));
   AIControlComponent woke;
   if (woken) woke = AIControlComponent.Cast(woken.FindComponent(AIControlComponent));
   AIControlComponent vehicleWoke = AIControlComponent.Cast(s_Lav.FindComponent(ChimeraAIVehicleControlComponent));
   ACR_TestRegress.Check("crew: parked driver and vehicle AI back on after a player leaves", woke && woke.IsAIActivated() && (!vehicleWoke || vehicleWoke.IsAIActivated()), "");
   s_From = s_Lav.GetOrigin();
   crew.MoveTo(ACR_Manager.Ground(home + Vector(40, 0, 140)));
  }
  if (tick == 75 && crew)
  {
   float moved = vector.DistanceXZ(s_Lav.GetOrigin(), s_From);
   ACR_TestRegress.Check("crew: the woken LAV takes the next order", moved > 20, string.Format("%1 m", moved));
   float postMoved;
   if (postSquad && postSquad.GetAnchor()) postMoved = vector.DistanceXZ(s_PostFrom, postSquad.GetAnchor().GetOrigin());
   if (postSquad)
   {
    vector target;
    if (postSquad.m_AITarget) target = postSquad.m_AITarget.GetOrigin();
    vector waypointAt;
    if (postSquad.m_Group && postSquad.m_Group.GetCurrentWaypoint()) waypointAt = postSquad.m_Group.GetCurrentWaypoint().GetOrigin();
    PrintFormat("[ACR] CREW recall diag: anchor %1 from %2, target %3 (%4 m from post), waypoint %5 (%6), mounted %7 | %8", postSquad.GetAnchor(), s_PostFrom, target, vector.DistanceXZ(target, s_PostFrom), waypointAt, postSquad.m_Group.GetCurrentWaypoint(), postSquad.IsMounted(), postSquad.Describe());
   }
   ACR_TestRegress.Check("roadblock: recalled squad physically leaves its post", postSquad && postSquad.m_iAITask == ACR_EAITask.DEFEND && postMoved > 3, string.Format("moved %1 m", postMoved));
   ACR_TestRegress.Complete("crew", 18);
  }
 }
}

// Use the production free-squad filter and defender planner; restrict the fixture
// to its one squad so this test cannot redirect the vehicle-crew control.
class ACR_TestRoadblockPlanner : ACR_AICommander
{
 ACR_Squad m_OnlySquad;
 void ACR_TestRoadblockPlanner(ACR_Manager manager, string factionKey)
 {
  m_Manager = manager;
  m_sFaction = factionKey;
 }
 void ReleaseBuilders() { ReleaseRoadblockBuilders(); }
 void DefendWith(ACR_Squad squad, ACR_Base target)
 {
  m_OnlySquad = squad;
  m_Threat.Set(target, squad.Strength() + 1);
  m_Defence.Set(target, 0);
  AssignDefenders();
 }
 override protected void MySquads(array<ACR_Squad> result, bool freeOnly)
 {
  super.MySquads(result, freeOnly);
  for (int i = result.Count() - 1; i >= 0; i--)
   if (result[i] != m_OnlySquad) result.RemoveOrdered(i);
 }
}
