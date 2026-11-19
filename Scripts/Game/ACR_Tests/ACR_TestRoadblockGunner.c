// -acrRoadblockGunnerTest 1 (server-capable, Arland, about 6 min): a machine-gun team holding its
// roadblock keeps one man on the gun (Jacob's 19 Nov game: the US Weapons Team's gunner got out
// "on his own" every 2.5-3.5 min all match, no enemy near, and someone climbed back in).
// Logs "[ACR] RBGUNNER ..." with what each man was doing when the gun emptied.
// VERDICT: 2 checks (fixture, nobody leaves the gun in 5 min).
class ACR_TestRoadblockGunner
{
 static const ResourceName MG_TEAM = "{958039B857396B7B}Prefabs/Groups/BLUFOR/Group_US_MachineGunTeam.et";
 static const int WATCH_S = 300;
 static ref ACR_TestRoadblockGunner s_Test;

 protected ref ACR_Roadblock m_Roadblock;
 protected SCR_AIGroup m_Group;
 protected int m_iStep;
 protected int m_iStepAt;
 protected IEntity m_Gunner;
 protected int m_iLeft;
 protected int m_iDepartures;
 protected ref array<string> m_aRecent = {};

 static void Run(ACR_TestHarness h, int tick)
 {
  if (!s_Test) s_Test = new ACR_TestRoadblockGunner();
  s_Test.Step(h, tick);
 }

 protected static string ActionName(AIActionBase action)
 {
  if (!action) return "none";
  string name = action.Type().ToString();
  SCR_AIActionBase scripted = SCR_AIActionBase.Cast(action);
  if (scripted) name = string.Format("%1 (prio %2, level %3)", name, scripted.GetPriority(), scripted.EvaluatePriorityLevel());
  if (action.GetRelatedGroupActivity()) name += " for " + action.GetRelatedGroupActivity().Type().ToString();
  return name;
 }

 protected string Snapshot(ACR_Squad squad)
 {
  string text = "";
  SCR_AIGroupUtilityComponent groupUtility = SCR_AIGroupUtilityComponent.Cast(m_Group.FindComponent(SCR_AIGroupUtilityComponent));
  if (groupUtility) text = "group " + ActionName(groupUtility.GetCurrentAction());
  AIWaypoint waypoint = m_Group.GetCurrentWaypoint();
  text += string.Format(", waypoint %1", waypoint);
  array<AIAgent> agents = {};
  m_Group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   SCR_ChimeraAIAgent chimera = SCR_ChimeraAIAgent.Cast(agent);
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!chimera || !body) continue;
   string seat = "on foot";
   if (body.IsInVehicle()) seat = "in " + typename.EnumToString(ECompartmentType, ACR_Transport.SeatOf(body));
   text += string.Format(" | %1 %2: %3", squad.NumberOf(body), seat, ActionName(chimera.m_UtilityComponent.GetCurrentAction()));
  }
  return text;
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
  if (m_iStep == 0 && tick >= 4)
  {
   vector post = ACR_Manager.Ground(alpha.m_Home + Vector(-40, 0, 35));
   m_Roadblock = ACR_Roadblocks.SpawnRoadblock("US", post, 0, "Gunner test");
   m_Group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(MG_TEAM, post + Vector(5, 0, 0)));
   m_iStep = 1;
   m_iStepAt = tick;
   return;
  }
  ACR_Squad squad = manager.FindSquadByGroup(m_Group);
  if (m_iStep == 1 && tick - m_iStepAt >= 4)
  {
   bool ok = squad && m_Roadblock && m_Roadblock.m_Turret;
   ACR_TestRegress.Check("rbgunner: real turret and machine-gun team fixture", ok);
   if (!ok)
   {
    ACR_TestRegress.Complete("rbgunner", 2);
    m_iStep = 99;
    return;
   }
   // As ACR_SquadWorks completes a roadblock (the order a builder holds it with).
   squad.m_ActiveRoadblock = m_Roadblock;
   squad.m_RoadblockTurret = m_Roadblock.m_Turret;
   squad.m_vRoadblockSite = m_Roadblock.m_vPos;
   squad.IssueWaypoint(ACR_Squad.WP_DEFEND, m_Roadblock.m_vPos, 15);
   squad.SetOrder("Holding roadblock", ACR_EOrderState.DEFEND);
   squad.m_OrderKind = ACR_EOrderKind.DEFEND;
   m_iStep = 2;
   m_iStepAt = tick;
   return;
  }
  if (m_iStep == 2)
  {
   if (!squad) return;
   IEntity seated = null;
   SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(m_Roadblock.m_Turret.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<BaseCompartmentSlot> slots = {};
   if (seats) seats.GetCompartmentsOfType(slots, ECompartmentType.TURRET);
   if (!slots.IsEmpty()) seated = slots[0].GetOccupant();
   string now = string.Format("t=%1 gun %2 | %3", tick - m_iStepAt, squad.NumberOf(seated), Snapshot(squad));
   m_aRecent.Insert(now);
   if (m_aRecent.Count() > 4) m_aRecent.RemoveOrdered(0);
   if (!m_Gunner && seated)
   {
    m_Gunner = seated;
    m_iLeft = squad.m_iUnexpectedCrewDepartures;
    PrintFormat("[ACR] RBGUNNER manned at t=%1: %2", tick - m_iStepAt, now);
   }
   else if (m_Gunner && seated != m_Gunner)
   {
    m_iDepartures++;
    PrintFormat("[ACR] RBGUNNER gun left at t=%1 (departure %2); the seconds before:", tick - m_iStepAt, m_iDepartures);
    foreach (string line : m_aRecent) PrintFormat("[ACR] RBGUNNER   %1", line);
    m_Gunner = seated;
   }
   if ((tick - m_iStepAt) % 30 == 0) PrintFormat("[ACR] RBGUNNER %1", now);
   if (tick - m_iStepAt < WATCH_S + 30) return;
   ACR_TestRegress.Check("rbgunner: the gunner holds the roadblock's gun for 5 min", m_Gunner != null && m_iDepartures == 0, string.Format("%1 departures, crew-left count %2", m_iDepartures, squad.m_iUnexpectedCrewDepartures - m_iLeft));
   ACR_TestRegress.Complete("rbgunner", 2);
   m_iStep = 99;
  }
 }
}
