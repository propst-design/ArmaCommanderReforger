// -acrGrabsTest 1 (server-capable, ~20 s): quick grabs of empty bases skip support teams (#34), as
// attacks do. Alpha is the only free US squad, 200 m from an empty base: named as a medic team it
// isn't sent to grab it; under its own name (the control) it is. Then another job takes Alpha
// (base works borrow attackers, WorksWorkerFurther): the grab drops it instead of keeping it.
class ACR_TestGrabs
{
 static const int CHECKS = 5;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range || tick != 8) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   ACR_TestRegress.Abort("grabs", "no Alpha squad");
   return;
  }
  ACR_Base empty;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base && !base.m_bRetired && base.m_sOwner.IsEmpty() && base.IsVisibleTo("US")) { empty = base; break; }
  }
  if (!empty)
  {
   ACR_TestRegress.Abort("grabs", "no empty base the US side can see");
   return;
  }
  // Every other US squad busy; Alpha free, 200 m from the empty base.
  array<ACR_Squad> others = {};
  array<int> tasks = {};
  foreach (ACR_Squad other : manager.m_Squads)
  {
   if (!other || other == alpha || !other.GetFaction() || other.GetFaction().GetFactionKey() != "US") continue;
   others.Insert(other);
   tasks.Insert(other.m_iAITask);
   other.m_iAITask = ACR_EAITask.ATTACK;
  }
  IEntity anchor = alpha.GetAnchor();
  vector home = anchor.GetOrigin();
  anchor.SetOrigin(ACR_Manager.Ground(empty.GetOrigin() + Vector(200, 0, 0)));
  string name = alpha.m_Name;

  // As a medic team.
  alpha.m_Name = "Medic Team 1";
  bool support = ACR_AICommander.IsSupportTeam(alpha);
  ACR_TestRegress.Check("grabs: a squad named as a medic team is a support team", support, alpha.m_Name);
  alpha.m_iAITask = ACR_EAITask.FREE;
  ACR_AICommander medicAI = new ACR_AICommander(manager, "US");
  medicAI.AssignAttacksForTest();
  bool medicStays = alpha.m_iAITask == ACR_EAITask.FREE;
  string medicDetail = string.Format("task %1, target %2", typename.EnumToString(ACR_EAITask, alpha.m_iAITask), alpha.m_AITarget != null);
  ACR_TestRegress.Check("grabs: a support team is never sent to grab an empty base", medicStays, string.Format("%1 200 m from %2: %3", alpha.m_Name, empty.m_sName, medicDetail));

  // The control: under its own name it goes.
  alpha.m_Name = name;
  alpha.m_iAITask = ACR_EAITask.FREE;
  ACR_AICommander plainAI = new ACR_AICommander(manager, "US");
  plainAI.AssignAttacksForTest();
  bool plainGoes = alpha.m_iAITask == ACR_EAITask.ATTACK && alpha.m_AITarget == empty;
  ACR_TestRegress.Check("grabs: in the same spot a fighting squad grabs it (the control)", plainGoes, string.Format("%1: task %2, target %3", alpha.m_Name, typename.EnumToString(ACR_EAITask, alpha.m_iAITask), alpha.m_AITarget == empty));

  // Another job takes the grabber: the grab lets it go, and the squad keeps its new job.
  bool grabbing = plainAI.IsAttackingForTest(empty);
  alpha.SetAITask(ACR_EAITask.FORTIFY);
  plainAI.UpdateAttacksForTest();
  bool dropped = !plainAI.IsAttackingForTest(empty) && alpha.m_iAITask == ACR_EAITask.FORTIFY;
  ACR_TestRegress.Check("grabs: the grab is running before the squad is taken", grabbing, empty.m_sName);
  ACR_TestRegress.Check("grabs: a grabber taken for another job leaves the grab and keeps that job", dropped, string.Format("still attacking %1: %2, task %3", empty.m_sName, plainAI.IsAttackingForTest(empty), typename.EnumToString(ACR_EAITask, alpha.m_iAITask)));

  alpha.Halt("Holding");
  alpha.m_iAITask = ACR_EAITask.FREE;
  alpha.m_AITarget = null;
  anchor.SetOrigin(home);
  foreach (int i, ACR_Squad squad : others)
  {
   if (squad) squad.m_iAITask = tasks[i];
  }
  ACR_TestRegress.Complete("grabs", CHECKS);
 }
}
