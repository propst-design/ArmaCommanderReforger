// -acrTwoICTest 1 (server-capable, ~2 min): the always-on second-in-command's claim rules (#14). No
// AI commander on either side (as when players command both). With -acr2IC off nothing is taken
// on; on, every unclaimed squad of the side is the 2IC's and gets AI tasks; a player's order claims
// a squad, out of the 2IC's plans, and the 2IC leaves it alone after; the map tags the 2IC's squads
// [AI], not the claimed one; handed back with the toggle, the 2IC has it again. And it builds only
// out of the build budget, never from the side's RP (#14).
class ACR_TestTwoIC
{
 static const int CHECKS = 7;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iAlphaSerial;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Options.Set("acr2IC", "0"); // off to start (on by default since 19 Nov)
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!alpha || !bravo || !alpha.GetAnchor() || !bravo.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("twoic", "no Alpha or Bravo squad");
   return;
  }
  if (tick > 190) ACR_TestRegress.Abort("twoic", string.Format("timed out in step %1", s_iStep));
  if (tick < 8) return;
  int waited = tick - s_iStepAt;

  if (s_iStep == 0)
  {
   ACR_TestRegress.Check("twoic: with the 2IC off, nothing is taken on", ACR_SquadAI.CountFor("US") == 0 && !alpha.m_bAICommanded && !bravo.m_bAICommanded, string.Format("handed %1", ACR_SquadAI.CountFor("US")));
   // The 2IC's engineering: out of the build budget only.
   ACR_AICommander builder = manager.FobAICommanderFor("US");
   ACR_FactionData us = manager.m_Bases.GetFaction("US");
   bool budgetOnly = false;
   string budgetDetail = "no US AI or faction data";
   if (builder && us)
   {
    us.m_iRP = 500;
    ACR_Options.Unset("acrAIBuildShare");
    bool offHanded = builder.CanBuildHandedForTest(20, 25);
    bool offFull = builder.CanBuild(20, 25);
    ACR_Options.Set("acrAIBuildShare", "15");
    builder.m_fBuildBudget = 25;
    bool onHanded = builder.CanBuildHandedForTest(20, 25);
    builder.m_fBuildBudget = 15;
    bool shortHanded = builder.CanBuildHandedForTest(20, 25);
    ACR_Options.Unset("acrAIBuildShare");
    builder.m_fBuildBudget = 0;
    budgetOnly = !offHanded && offFull && onHanded && !shortHanded;
    budgetDetail = string.Format("share off, 500 RP: 2IC %1 (the AI in command %2); share on: 25 in the budget %3, 15 %4", offHanded, offFull, onHanded, shortHanded);
   }
   ACR_TestRegress.Check("twoic: the 2IC builds only out of the build budget (share off: nothing, whatever the side's RP)", budgetOnly, budgetDetail);
   ACR_Options.Set("acr2IC", "1");
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   bool both = alpha.m_bAICommanded && bravo.m_bAICommanded;
   if (!both && waited < 10) return;
   ACR_TestRegress.Check("twoic: with the 2IC on and no AI commander, every unclaimed squad of the side is the 2IC's", both && !alpha.m_bClaimed && !bravo.m_bClaimed, string.Format("Alpha %1, Bravo %2 after %3 s (handed %4)", alpha.m_bAICommanded, bravo.m_bAICommanded, waited, ACR_SquadAI.CountFor("US")));
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   bool tasked = alpha.m_iAITask != ACR_EAITask.FREE || bravo.m_iAITask != ACR_EAITask.FREE;
   if (!tasked && waited < 50) return;
   ACR_TestRegress.Check("twoic: the 2IC gives its squads AI tasks", tasked, string.Format("Alpha %1, Bravo %2 after %3 s", typename.EnumToString(ACR_EAITask, alpha.m_iAITask), typename.EnumToString(ACR_EAITask, bravo.m_iAITask), waited));
   // A player's order claims Alpha.
   bool claimed = ACR_SquadAI.TakeBack(alpha, ACR_ERequest.MOVE_TO) && !alpha.m_bAICommanded && alpha.m_bClaimed;
   alpha.MoveTo(alpha.GetAnchor().GetOrigin() + Vector(-15, 0, 0));
   s_iAlphaSerial = alpha.m_iOrderSerial;
   ACR_AICommander ai = manager.FobAICommanderFor("US");
   bool forgotten = ai && !ai.PlansFor(alpha);
   ACR_TestRegress.Check("twoic: a player's order claims a squad, out of the 2IC's plans", claimed && forgotten, string.Format("claimed %1, forgotten %2", claimed, forgotten));
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   // Claimed, the 2IC leaves it alone (two strategy ticks); the map tags the 2IC's squads only.
   if (waited < 32) return;
   bool alone = !alpha.m_bAICommanded && alpha.m_bClaimed && alpha.m_iAITask == ACR_EAITask.FREE && alpha.m_iOrderSerial == s_iAlphaSerial;
   ACR_FeedFrame frame = ACR_MapFeed.Build(manager, GetGame().GetFactionManager().GetFactionByKey("US"));
   bool alphaTag, bravoTag;
   foreach (string groupName : frame.m_GroupNames)
   {
    if (groupName.StartsWith(alpha.GetName())) alphaTag = groupName.Contains("[AI]");
    if (groupName.StartsWith(bravo.GetName())) bravoTag = groupName.Contains("[AI]");
   }
   ACR_TestRegress.Check("twoic: claimed, the 2IC leaves it alone; the map tags the 2IC's squad [AI], not the claimed one", alone && bravoTag && !alphaTag, string.Format("Alpha handed %1, task %2, serial %3 -> %4; tags Alpha %5, Bravo %6", alpha.m_bAICommanded, typename.EnumToString(ACR_EAITask, alpha.m_iAITask), s_iAlphaSerial, alpha.m_iOrderSerial, alphaTag, bravoTag));
   string handed = ACR_SquadAI.Toggle(alpha);
   PrintFormat("[ACR] TWOIC toggle: %1", handed);
   NextStep(tick);
   return;
  }

  if (s_iStep == 4)
  {
   if (waited < 10) return;
   ACR_TestRegress.Check("twoic: handed back with the toggle, the 2IC has it again", alpha.m_bAICommanded && !alpha.m_bClaimed, string.Format("Alpha handed %1, claimed %2", alpha.m_bAICommanded, alpha.m_bClaimed));
   ACR_Options.Unset("acr2IC");
   ACR_TestRegress.Complete("twoic", CHECKS);
   s_iStep = 5;
  }
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
