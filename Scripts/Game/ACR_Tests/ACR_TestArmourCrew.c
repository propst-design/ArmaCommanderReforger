// -acrArmourCrewTest 1 (server-capable, Arland, about 2.5 min): fighting vehicles keep their crews
// and hold still (Jacob's 19 Nov game).
//  1. A failed move under fire (the engine's STUCK/UNREACHABLE, after which vanilla adds a
//     player-level get-out for the whole group, SCR_AIProcessFailedMovementResult): the crew of a
//     manned gun (BTR-70) stays aboard; an unarmed truck's crew still gets out to fight on foot.
//     Live, a T-72A's whole crew walked off the working tank in contact (CREW LEFT, order 'Moving').
//  2. Holding a point (Defend, which a vehicle crew gets as a plain move): the vehicle drives
//     there and stops, it doesn't keep driving round the spot. Live, an MT-LB screen post "did
//     donuts". Run on vanilla's BTR-70, and on the MT-LB and T-72A when their mods are loaded
//     (the Cold War stack: srv:armourcrew with the args swap in the 0.6.2 notes); without them
//     those two are skipped.
// Logs "[ACR] ARMOUR ...". VERDICT: 2 checks for part 1 + 1 per vehicle held (3), skips allowed.
class ACR_TestArmourCrew
{
 static const ResourceName BTR = "{C012BB3488BEA0C2}Prefabs/Vehicles/Wheeled/BTR70/BTR70.et";
 static const ResourceName TRUCK = "{F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et";
 static const ResourceName MTLB = "{3B2B8A6A662AB8EC}IFV/MTLB/MTLB_Olive_WZSSR.et";
 static const ResourceName T72 = "{BC07885176C7FF8A}Prefabs/Vehicles/Tracked/T72A/T72A_Base.et";
 static const int EXPECTED = 5;
 // How far a held vehicle may wander once it got there before it counts as circling.
 static const float WANDER_M = 60;
 static ref ACR_TestArmourCrew s_Test;

 protected vector m_vHome;
 protected ref array<string> m_aNames = {};
 protected ref array<ResourceName> m_aPrefabs = {};
 protected ref array<ACR_Squad> m_aSquads = {};
 protected ref array<IEntity> m_aVehicles = {};
 protected ref array<int> m_aSeated = {};
 // Part 2, per held vehicle: the spot, first arrival, path driven since, closest, furthest after arrival.
 protected ref array<vector> m_aSpot = {};
 protected ref array<vector> m_aLast = {};
 protected ref array<float> m_aArrivedAt = {};
 protected ref array<float> m_aPathAfter = {};
 protected ref array<float> m_aClosest = {};
 protected ref array<float> m_aWander = {};
 protected int m_iStep;
 protected int m_iStepAt;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (!s_Test) s_Test = new ACR_TestArmourCrew();
  s_Test.Step(h, tick);
 }

 protected void Spawn(ACR_Manager manager, string name, ResourceName prefab, string faction, vector at)
 {
  string before = manager.m_sSpawningFaction;
  manager.m_sSpawningFaction = faction;
  manager.SpawnUnit(name, "", prefab, true, "", at, at + Vector(0, 0, -10));
  manager.m_sSpawningFaction = before;
  m_aNames.Insert(name);
  m_aPrefabs.Insert(prefab);
 }

 // Squads and vehicles found for every name (false until all are crewed).
 protected bool Found(ACR_Manager manager)
 {
  m_aSquads.Clear();
  m_aVehicles.Clear();
  foreach (string name : m_aNames)
  {
   ACR_Squad found;
   foreach (ACR_Squad squad : manager.m_Squads) if (squad && squad.GetName() == name) found = squad;
   if (!found) return false;
   array<IEntity> vehicles = {};
   found.GetVehicles(vehicles);
   if (vehicles.IsEmpty() || !BBB.DriverOf(vehicles[0])) return false;
   m_aSquads.Insert(found);
   m_aVehicles.Insert(vehicles[0]);
  }
  return true;
 }

 protected int Seated(ACR_Squad squad)
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

 protected SCR_AIGroupUtilityComponent Utility(ACR_Squad squad)
 {
  SCR_AIGroup group = squad.m_Group;
  if (!group) return null;
  return SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
 }

 // In contact: the squad's own driver stands in for a known enemy (KnownEnemies counts what the
 // group's perception lists within 300 m; nobody shoots anybody).
 protected IEntity FakeContact(ACR_Squad squad, IEntity vehicle)
 {
  SCR_AIGroupUtilityComponent utility = Utility(squad);
  IEntity body = BBB.DriverOf(vehicle);
  if (!utility || !utility.m_Perception || !body) return null;
  utility.m_Perception.m_aTargetEntities.Insert(body);
  return body;
 }

 protected void EndContact(ACR_Squad squad, IEntity body)
 {
  SCR_AIGroupUtilityComponent utility = Utility(squad);
  if (!utility || !utility.m_Perception || !body) return;
  int at = utility.m_Perception.m_aTargetEntities.Find(body);
  if (at >= 0) utility.m_Perception.m_aTargetEntities.Remove(at);
 }

 // What the engine does when a vehicle move fails: the event, then a player-level get-out for the
 // group (SCR_AIProcessFailedMovementResult, 1.9).
 protected void FailMove(ACR_Squad squad, IEntity vehicle)
 {
  SCR_AIGroupUtilityComponent utility = Utility(squad);
  if (!utility) return;
  utility.OnMoveFailed(EMoveError.STUCK, vehicle, true, vehicle.GetOrigin() + Vector(300, 0, 0));
  SCR_AIGetOutActivity getOut = new SCR_AIGetOutActivity(utility, null, vehicle, priority: SCR_AIActionBase.PRIORITY_BEHAVIOR_GET_OUT_VEHICLE_HIGH_PRIORITY, priorityLevel: SCR_AIActionBase.PRIORITY_LEVEL_PLAYER);
  utility.AddAction(getOut);
 }

 protected void Step(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (!alpha || !alpha.GetAnchor()) return;
  if (m_iStep == 0 && tick >= 3)
  {
   m_vHome = alpha.m_Home;
   Spawn(manager, "Armour Test BTR", BTR, "USSR", ACR_Manager.Ground(m_vHome + Vector(60, 0, 40)));
   Spawn(manager, "Armour Test Truck", TRUCK, "US", ACR_Manager.Ground(m_vHome + Vector(-60, 0, 40)));
   if (Resource.Load(MTLB).IsValid()) Spawn(manager, "Armour Test MT-LB", MTLB, "USSR", ACR_Manager.Ground(m_vHome + Vector(60, 0, -40)));
   else ACR_TestRegress.Skip("armour: an MT-LB holding a point stops there", "the Zagoria 89 MT-LB mod isn't loaded");
   if (Resource.Load(T72).IsValid()) Spawn(manager, "Armour Test T-72", T72, "USSR", ACR_Manager.Ground(m_vHome + Vector(-60, 0, -40)));
   else ACR_TestRegress.Skip("armour: a T-72A holding a point stops there", "the WCS T-72 mod isn't loaded");
   m_iStep = 1;
   m_iStepAt = tick;
   return;
  }
  if (m_iStep == 1)
  {
   if (!Found(manager))
   {
    if (tick - m_iStepAt > 25)
    {
     ACR_TestRegress.Abort("armourcrew", "the test vehicles were not crewed in 25 s");
     m_iStep = 99;
    }
    return;
   }
   // 1. Failed move under fire: the BTR (manned gun) and the truck (no gun).
   for (int ia = 0; ia < 2; ia++)
   {
    m_aSeated.Insert(Seated(m_aSquads[ia]));
    m_aSquads[ia].MoveTo(ACR_Manager.Ground(m_vHome + Vector(-400 + 800 * ia, 0, 400)));
   }
   m_iStep = 2;
   m_iStepAt = tick;
   return;
  }
  if (m_iStep == 2 && tick - m_iStepAt >= 2)
  {
   for (int ib = 0; ib < 2; ib++)
   {
    IEntity body = FakeContact(m_aSquads[ib], m_aVehicles[ib]);
    PrintFormat("[ACR] ARMOUR %1: in contact %2 (known enemies %3), seated %4, move fails", m_aSquads[ib].GetName(), body != null, m_aSquads[ib].KnownEnemies(300), Seated(m_aSquads[ib]));
    FailMove(m_aSquads[ib], m_aVehicles[ib]);
    EndContact(m_aSquads[ib], body);
   }
   m_iStep = 3;
   m_iStepAt = tick;
   return;
  }
  if (m_iStep == 3 && tick - m_iStepAt >= 8)
  {
   ACR_Squad btr = m_aSquads[0];
   ACR_Squad truck = m_aSquads[1];
   PrintFormat("[ACR] ARMOUR failed move under fire: BTR seated %1 of %2 (kept %3) | truck seated %4 of %5", Seated(btr), m_aSeated[0], btr.m_iMoveFailsUnderFireKept, Seated(truck), m_aSeated[1]);
   ACR_TestRegress.Check("armour: a manned gun's crew stays aboard after a failed move under fire", m_aSeated[0] >= 2 && Seated(btr) == m_aSeated[0] && btr.m_iMoveFailsUnderFireKept > 0, string.Format("seated %1 of %2", Seated(btr), m_aSeated[0]));
   ACR_TestRegress.Check("armour: an unarmed truck's crew still gets out after a failed move under fire", m_aSeated[1] >= 1 && Seated(truck) == 0, string.Format("seated %1 of %2", Seated(truck), m_aSeated[1]));
   // 2. Holding a point: every armed vehicle 150 m off to a spot in the open.
   for (int ic = 0; ic < m_aSquads.Count(); ic++)
   {
    m_aSpot.Insert(vector.Zero);
    m_aLast.Insert(m_aVehicles[ic].GetOrigin());
    m_aArrivedAt.Insert(-1);
    m_aPathAfter.Insert(0);
    m_aClosest.Insert(float.MAX);
    m_aWander.Insert(0);
    if (ic == 1) continue; // the truck sits this one out
    vector spot = ACR_Manager.Ground(m_aVehicles[ic].GetOrigin() + Vector(0, 0, 150));
    m_aSpot[ic] = spot;
    m_aSquads[ic].Defend(spot);
    PrintFormat("[ACR] ARMOUR hold %1: Defend %2 (%3 m off), order '%4', waypoint %5", m_aSquads[ic].GetName(), spot, (int)vector.DistanceXZ(spot, m_aVehicles[ic].GetOrigin()), m_aSquads[ic].m_Order, m_aSquads[ic].m_Waypoint);
   }
   m_iStep = 4;
   m_iStepAt = tick;
   return;
  }
  if (m_iStep == 4)
  {
   for (int id = 0; id < m_aSquads.Count(); id++)
   {
    if (id == 1 || !m_aVehicles[id]) continue;
    vector at = m_aVehicles[id].GetOrigin();
    float step = vector.DistanceXZ(at, m_aLast[id]);
    m_aLast[id] = at;
    float d = vector.DistanceXZ(at, m_aSpot[id]);
    if (d < m_aClosest[id]) m_aClosest[id] = d;
    if (m_aArrivedAt[id] < 0 && d < 30) m_aArrivedAt[id] = tick;
    if (m_aArrivedAt[id] >= 0)
    {
     m_aPathAfter[id] = m_aPathAfter[id] + step;
     if (d > m_aWander[id]) m_aWander[id] = d;
    }
    if ((tick - m_iStepAt) % 5 == 0)
    {
     float yaw = m_aVehicles[id].GetAngles()[1];
     PrintFormat("[ACR] ARMOUR hold %1 t=%2: %3 m from the spot, %4 km/h, yaw %5, seated %6, waypoint %7", m_aSquads[id].GetName(), tick - m_iStepAt, (int)d, (int)(step * 3.6), (int)yaw, Seated(m_aSquads[id]), m_aSquads[id].m_Group.GetCurrentWaypoint() != null);
    }
   }
   if (tick - m_iStepAt < 100) return;
   for (int ie = 0; ie < m_aSquads.Count(); ie++)
   {
    if (ie == 1) continue;
    string what = "a BTR-70";
    if (m_aPrefabs[ie] == MTLB) what = "an MT-LB";
    if (m_aPrefabs[ie] == T72) what = "a T-72A";
    float settled = 0;
    if (m_aArrivedAt[ie] >= 0) settled = tick - m_aArrivedAt[ie];
    string detail = string.Format("closest %1 m, %2 m driven in the %3 s since it got within 30 m, furthest %4 m after", (int)m_aClosest[ie], (int)m_aPathAfter[ie], (int)settled, (int)m_aWander[ie]);
    PrintFormat("[ACR] ARMOUR hold %1: %2", m_aSquads[ie].GetName(), detail);
    ACR_TestRegress.Check(string.Format("armour: %1 holding a point stops there", what), m_aArrivedAt[ie] >= 0 && m_aPathAfter[ie] < WANDER_M, detail);
   }
   ACR_TestRegress.Complete("armourcrew", EXPECTED);
   m_iStep = 99;
  }
 }
}
