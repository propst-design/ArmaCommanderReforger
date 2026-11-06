// -acrTwoICAllTest 1 (server-capable, ~2 min): "I'm the 2IC" (-acr2IC 1, Jacob 6 Nov): while a player
// commands, the AI commander runs the whole side as if nobody did. No AI commander on either side;
// the test says a player commands US (ACR_TestHooks.PlayerCommandsForTest). The AI then spends the
// side's RP as a commander would (a -acr2ICKeep holds some back only when set), buys squads with a
// modest purse (6 Nov play: a 100 RP keep left it 0-42 RP and it bought nothing all match) and takes
// them on, and leaves a squad the player ordered alone. With nobody in command it only runs the
// squads, and buys nothing.
class ACR_TestTwoICAll
{
 static const int CHECKS = 6;
 static string s_sCommanded;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iSquadsAt;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Options.Unset("acr2IC");
   ACR_Options.Unset("acr2ICKeep");
   // The range's Farm is a plain base: let the 2IC field squads there.
   ACR_Options.Set("acrDeployment", "all");
   s_sCommanded = "";
   s_iStep = 0;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!alpha || !bravo || !alpha.GetAnchor() || !bravo.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("twoicall", "no Alpha or Bravo squad");
   return;
  }
  if (tick > 250) ACR_TestRegress.Abort("twoicall", string.Format("timed out in step %1", s_iStep));
  if (tick < 8) return;
  int waited = tick - s_iStepAt;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  if (!us || !ai)
  {
   ACR_TestRegress.Abort("twoicall", "no US faction data or AI");
   return;
  }

  if (s_iStep == 0)
  {
   ACR_Options.Set("acr2IC", "1");
   s_sCommanded = "US";
   bool runs = ACR_SquadAI.RunsAll(manager, "US") && !ACR_SquadAI.RunsAll(manager, "USSR");
   ACR_TestRegress.Check("twoicall: the 2IC runs the whole side only where a player commands", runs, string.Format("US %1, USSR %2", ACR_SquadAI.RunsAll(manager, "US"), ACR_SquadAI.RunsAll(manager, "USSR")));
   us.m_iRP = 400;
   int whole = ai.PurseForTest(true);
   ACR_Options.Set("acr2ICKeep", "100");
   int kept = ai.PurseForTest(true);
   us.m_iRP = 80;
   int keptPoor = ai.PurseForTest(true);
   ACR_Options.Unset("acr2ICKeep");
   ACR_TestRegress.Check("twoicall: it spends the side's RP like an AI commander; -acr2ICKeep holds some back only when set", whole == 400 && kept == 300 && keptPoor == 0, string.Format("400 RP: %1, with a 100 keep %2; 80 RP with the keep: %3", whole, kept, keptPoor));
   // A player's order claims Alpha: the 2IC must leave it alone from here on.
   ACR_SquadAI.TakeBack(alpha, ACR_ERequest.MOVE_TO);
   alpha.MoveTo(alpha.GetAnchor().GetOrigin() + Vector(-15, 0, 0));
   // A modest purse, as in play.
   us.m_iRP = 140;
   s_iSquadsAt = CountSide(manager, "US");
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   int now = CountSide(manager, "US");
   if (now <= s_iSquadsAt && waited < 100) return;
   ACR_TestRegress.Check("twoicall: the AI commander buys squads for the side with a modest purse", now > s_iSquadsAt, string.Format("US squads %1 -> %2 after %3 s, %4 RP left", s_iSquadsAt, now, waited, us.m_iRP));
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   // The bought squad is adopted on the next turn.
   int adopted = 0;
   foreach (ACR_Squad squad : manager.m_Squads)
    if (squad && squad != alpha && squad != bravo && squad.GetFaction() && squad.GetFaction().GetFactionKey() == "US" && squad.m_bAICommanded) adopted++;
   if (adopted == 0 && waited < 15) return;
   ACR_TestRegress.Check("twoicall: it takes on the squads it bought", adopted > 0, string.Format("%1 bought squad(s) the 2IC's after %2 s", adopted, waited));
   ACR_TestRegress.Check("twoicall: a squad the player ordered stays the player's", !alpha.m_bAICommanded && alpha.m_bClaimed && bravo.m_bAICommanded, string.Format("Alpha handed %1 claimed %2, Bravo handed %3", alpha.m_bAICommanded, alpha.m_bClaimed, bravo.m_bAICommanded));
   // Nobody in command any more: the 2IC runs the squads only, and buys nothing.
   s_sCommanded = "";
   us.m_iRP = 600;
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   // A purchase already under way may still arrive: count from 10 s on.
   if (waited == 10) s_iSquadsAt = CountSide(manager, "US");
   if (waited < 50) return;
   int after = CountSide(manager, "US");
   ACR_TestRegress.Check("twoicall: with nobody in command the 2IC buys nothing", after <= s_iSquadsAt && us.m_iRP >= 600, string.Format("US squads %1 -> %2, %3 RP", s_iSquadsAt, after, us.m_iRP));
   s_sCommanded = "";
   ACR_Options.Unset("acr2IC");
   ACR_Options.Unset("acrDeployment");
   ACR_TestRegress.Complete("twoicall", CHECKS);
   NextStep(tick);
  }
 }

 protected static int CountSide(ACR_Manager manager, string faction)
 {
  int count;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad.GetFaction() && squad.GetFaction().GetFactionKey() == faction && squad.CountAlive() > 0) count++;
  return count;
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
