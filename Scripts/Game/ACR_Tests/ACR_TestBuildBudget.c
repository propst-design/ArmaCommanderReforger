// -acrBuildBudgetTest 1 (server-capable, ~30 s): the AI building budget (-acrAIBuildShare). Off
// by default (nothing set aside, unit purchases keep nothing back); at 10% a 200 RP gain puts
// 20 RP in it, which unit purchases keep back and the status line shows; it stops at 100 RP
// and at what the side holds; engineering spending comes out of it. #28: over 600 banked RP the
// share rises with the surplus and the budget is topped up from it; at or below 600 nothing changes.
// #28 step 2: a rich side keeps more for fire missions and fires at lower thresholds; step 3: it
// weighs its purchases towards dearer units.
class ACR_TestBuildBudget
{
 static const int CHECKS = 10;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || tick != 8) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  // The checks are on the plain 600 RP: no variety (ACR_Variety).
  ACR_Variety.s_bOff = true;
  ACR_AICommander ai = manager.AICommanderFor("US");
  if (!ai) ai = new ACR_AICommander(manager, "US");
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  if (!us)
  {
   ACR_TestRegress.Abort("budget", "no US faction data");
   return;
  }

  // Off by default.
  ACR_Options.Unset("acrAIBuildShare");
  us.m_iRP = 100;
  ai.AccrueBuildBudget();
  us.m_iRP = 300;
  ai.AccrueBuildBudget();
  ACR_TestRegress.Check("budget: off by default, nothing set aside", ACR_AICommander.BuildShare() == 0 && ai.BuildBudget() == 0 && ai.BuildKeep() == 0 && ai.BuildStatus().IsEmpty(), string.Format("share %1, budget %2", ACR_AICommander.BuildShare(), ai.BuildBudget()));

  // 10%: a 200 RP gain puts 20 RP in, kept back from units and shown.
  ACR_Options.Set("acrAIBuildShare", "10");
  us.m_iRP = 100;
  ai.AccrueBuildBudget();
  us.m_iRP = 300;
  ai.AccrueBuildBudget();
  ACR_TestRegress.Check("budget: at 10% a 200 RP gain sets 20 RP aside, kept back from unit purchases", ai.BuildBudget() == 20 && ai.BuildKeep() == 20 && ai.BuildStatus().Contains("build budget 20 RP"), string.Format("budget %1, keep %2, status '%3'", ai.BuildBudget(), ai.BuildKeep(), ai.BuildStatus()));

  // At most 100, and never more than the side holds.
  us.m_iRP = 5300;
  ai.AccrueBuildBudget();
  int capped = ai.BuildBudget();
  us.m_iRP = 40;
  ai.AccrueBuildBudget();
  ACR_TestRegress.Check("budget: at most 100 RP, and never more than the side holds", capped == 100 && ai.BuildBudget() == 40, string.Format("after +5000: %1, at 40 RP: %2", capped, ai.BuildBudget()));

  // Engineering spending comes out of it (never below zero).
  ai.SpendBuildBudget(25);
  int afterSpend = ai.BuildBudget();
  ai.SpendBuildBudget(500);
  ACR_TestRegress.Check("budget: engineering spending comes out of it", afterSpend == 15 && ai.BuildBudget() == 0, string.Format("after 25: %1, after 500 more: %2", afterSpend, ai.BuildBudget()));

  CheckSurplus(ai, us);
  CheckRichFire(ai, us);
  CheckQuality();

  // The share is clamped to 30%.
  ACR_Options.Set("acrAIBuildShare", "80");
  bool clamped = ACR_AICommander.BuildShare() == 30;
  ACR_Options.Unset("acrAIBuildShare");
  ACR_TestRegress.Check("budget: the share is at most 30%", clamped, "");

  // The setup screen's row: off by default, 15% when set.
  string offRow = ACR_Setup.Rows();
  ACR_Options.Set("acrAIBuildShare", "15");
  string onRow = ACR_Setup.Rows();
  ACR_Options.Unset("acrAIBuildShare");
  ACR_TestRegress.Check("budget: a setup row, off by default", offRow.Contains("aibuild\tAI build budget: off") && onRow.Contains("aibuild\tAI build budget: 15% of income") && ACR_Setup.Option("aibuild") == "acrAIBuildShare", "");
  ACR_TestRegress.Complete("budget", CHECKS);
 }

 // #28 at 15% (Balanced): the share at 300 and 600 banked RP is the plain 15% and a 200 RP gain up
 // to 600 sets the same 30 RP aside as before; above 600 it rises (22.5% at 800, 30% at the
 // 1000 RP cap, a Fortress's 50% at most), a gain sets more aside, and at the cap, with no gain
 // and an empty budget, the budget is topped up from the surplus to its 100 RP cap.
 protected static void CheckSurplus(ACR_AICommander ai, ACR_FactionData us)
 {
  ACR_Options.Set("acrAIBuildShare", "15");
  ai.SetDoctrineForTest(ACR_AIDoctrine.Find("balanced"));
  float at300 = ai.ShareAt(300);
  float at600 = ai.ShareAt(600);
  us.m_iRP = 400;
  ai.AccrueBuildBudget();
  ai.SpendBuildBudget(1000);
  us.m_iRP = 600;
  ai.AccrueBuildBudget();
  int below = ai.BuildBudget();
  bool same = at300 == 15 && at600 == 15 && below == 30 && ai.SurplusFloor(600) == 0 && !ai.BuildStatus().Contains("raised");
  ACR_TestRegress.Check("budget: at or below 600 banked RP the share and the budget are as before", same,
   string.Format("share at 300 %1%%, at 600 %2%%; 400 -> 600 RP puts %3 RP aside (30 before); floor at 600 %4", at300, at600, below, ai.SurplusFloor(600)));

  float at800 = ai.ShareAt(800);
  float atCap = ai.ShareAt(ACR_BaseSystem.POINTS_CAP);
  ai.SpendBuildBudget(1000);
  us.m_iRP = 600;
  ai.AccrueBuildBudget();
  us.m_iRP = 800;
  ai.AccrueBuildBudget();
  int above = ai.BuildBudget();
  ai.SpendBuildBudget(1000);
  us.m_iRP = ACR_BaseSystem.POINTS_CAP;
  ai.AccrueBuildBudget();
  ai.SpendBuildBudget(1000);
  ai.AccrueBuildBudget();
  int topped = ai.BuildBudget();
  string status = ai.BuildStatus();
  ai.SetDoctrineForTest(ACR_AIDoctrine.Find("fortress"));
  float fortressCap = ai.ShareAt(ACR_BaseSystem.POINTS_CAP);
  ai.SetDoctrineForTest(ACR_AIDoctrine.Find("balanced"));
  ai.SpendBuildBudget(1000);
  ACR_Options.Unset("acrAIBuildShare");
  bool rises = at800 == 22.5 && atCap == 30 && fortressCap == 50 && above >= 45 && topped == 100 && status.Contains("raised by 400 RP banked over 600");
  ACR_TestRegress.Check("budget: over 600 banked RP the share rises with the surplus, and at the RP cap the budget is topped up from it, within its caps", rises,
   string.Format("share at 800 %1%%, at 1000 %2%% (Fortress %3%%); 600 -> 800 RP puts %4 RP aside; at 1000 with no gain, empty budget -> %5 RP; status '%6'", at800, atCap, fortressCap, above, topped, status));
 }

 // #28 step 2: a rich side fires more. At or below 600 RP the factor is 1 and nothing changes; at
 // the cap it is 2: the support budget rises (one more mission, air strikes counted, the cap
 // doubled) and the thresholds for air and heavy preparation halve.
 protected static void CheckRichFire(ACR_AICommander ai, ACR_FactionData us)
 {
  ai.SetDoctrineForTest(ACR_AIDoctrine.Find("balanced"));
  us.m_iRP = 600;
  float poorFactor = ai.FireFactor();
  int poorBudget = ai.SupportBudgetForTest();
  us.m_iRP = ACR_BaseSystem.POINTS_CAP;
  float richFactor = ai.FireFactor();
  int richBudget = ai.SupportBudgetForTest();
  int air = ACR_AICommander.Lowered(8, richFactor);
  int prep = ACR_AICommander.Lowered(12, richFactor);
  int same = ACR_AICommander.Lowered(8, poorFactor);
  bool ok = poorFactor == 1 && richFactor == 2 && richBudget > poorBudget && air == 4 && prep == 6 && same == 8 && ACR_AICommander.Lowered(1, 2) == 1;
  ACR_TestRegress.Check("budget: a rich side keeps more for fire missions and fires at lower thresholds", ok,
   string.Format("factor %1 -> %2, support budget %3 -> %4, air threat 8 -> %5, heavy prep 12 -> %6", poorFactor, richFactor, poorBudget, richBudget, air, prep));
  us.m_iRP = 0;
 }

 // #28 step 3: a rich side weighs its purchases by price once more (none of it below the surplus).
 protected static void CheckQuality()
 {
  ACR_RequisitionEntry team = new ACR_RequisitionEntry();
  team.Set("team", "", "", 12, 1);
  ACR_RequisitionEntry apc = new ACR_RequisitionEntry();
  apc.Set("apc", "", "", 76, 1);
  apc.m_iBaseCost = 75;
  float poor = ACR_AICommander.QualityWeight(apc, 1);
  float teamRich = ACR_AICommander.QualityWeight(team, 2);
  float apcRich = ACR_AICommander.QualityWeight(apc, 2);
  bool ok = poor == 1 && teamRich < 0.5 && apcRich == 3;
  ACR_TestRegress.Check("budget: a rich side weighs purchases towards dearer units", ok, string.Format("at x1 %1; at x2 team %2, APC %3", poor, teamRich, apcRich));
 }
}
