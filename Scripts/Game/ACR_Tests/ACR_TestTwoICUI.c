// -acrTwoICUITest 1 (server-capable, ~2 min): what the map's 2IC card and channel get from the server
// (#14 v1, ACR_TwoIC). No AI commander on either side. With -acr2IC off the channel keeps nothing;
// on, taking on the side's squads is a channel line, and the card counts the squads it runs, shows
// the reserve (the build budget, off without a share) and what it is doing; a player's order that
// claims a squad is a line and one squad fewer on the card; the channel keeps CHANNEL_LINES, a new
// status changes its signature; with the AI commander in command the 2IC stands by; the view's ages.
class ACR_TestTwoICUI
{
 static const int CHECKS = 14;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iRunsBefore;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Options.Set("acr2IC", "0"); // off to start (on by default since 19 Nov)
   ACR_Options.Unset("acrAIBuildShare");
   ACR_TwoIC.ResetForTest();
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!alpha || !bravo || !alpha.GetAnchor() || !bravo.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("twoicui", "no Alpha or Bravo squad");
   return;
  }
  if (tick > 200) ACR_TestRegress.Abort("twoicui", string.Format("timed out in step %1", s_iStep));
  if (tick < 8) return;
  int waited = tick - s_iStepAt;
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  if (!ai)
  {
   ACR_TestRegress.Abort("twoicui", "no US AI commander");
   return;
  }

  if (s_iStep == 0)
  {
   int id = ACR_TwoIC.Say("US", ACR_ETwoICLine.REPORT, "a line while off");
   ACR_TestRegress.Check("twoicui: with the 2IC off, its channel keeps nothing", id == 0 && ACR_TwoIC.Count("US") == 0, string.Format("id %1, lines %2", id, ACR_TwoIC.Count("US")));
   ACR_Options.Set("acr2IC", "1");
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   if (!(alpha.m_bAICommanded && bravo.m_bAICommanded) && waited < 10) return;
   ACR_TwoICLine took = Newest("US", "Took on");
   ACR_TestRegress.Check("twoicui: taking on the side's squads is a line in its channel", took != null && took.m_iKind == ACR_ETwoICLine.REPORT, Lines("US"));
   ACR_TwoICCard card = ACR_TwoIC.Build(manager, "US");
   bool counts = card.m_bRunning && card.m_iSquads == ACR_SquadAI.CountFor("US") && card.m_iSquads >= 2 && card.m_iSquadsAll >= card.m_iSquads;
   bool reserveOff = card.m_iShare == 0 && card.m_iReserveMax == ACR_AICommander.BUILD_BUDGET_MAX;
   ACR_TestRegress.Check("twoicui: the card counts the squads the 2IC runs of the side's; no share, the reserve is off", counts && reserveOff, Card(card));
   // A share on: the reserve is the build budget, at the share the AI sets aside.
   ACR_Options.Set("acrAIBuildShare", "15");
   ai.m_fBuildBudget = 48;
   ACR_TwoICCard shared = ACR_TwoIC.Build(manager, "US");
   ACR_TestRegress.Check("twoicui: with a share, the card's reserve is the build budget and its share of income", shared.m_iReserve == 48 && shared.m_iShare == (int)ai.EffectiveShare() && shared.m_iShare > 0, Card(shared));
   ACR_Options.Unset("acrAIBuildShare");
   ai.m_fBuildBudget = 0;
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   // The doing line, once the 2IC has given out tasks; an attack or grab is a channel line too.
   bool tasked = alpha.m_iAITask != ACR_EAITask.FREE || bravo.m_iAITask != ACR_EAITask.FREE;
   bool attacking = false;
   foreach (ACR_Squad any : manager.m_Squads)
    if (any && any.m_bAICommanded && any.m_iAITask == ACR_EAITask.ATTACK && any.GetFaction() && any.GetFaction().GetFactionKey() == "US") attacking = true;
   if (!attacking && waited < 70) return;
   ACR_TwoICCard card = ACR_TwoIC.Build(manager, "US");
   string doing = card.m_sDoing;
   ACR_TestRegress.Check("twoicui: the card says what the 2IC is doing", tasked && !doing.IsEmpty() && doing != "nothing yet", string.Format("'%1' (Alpha %2, Bravo %3)", doing, typename.EnumToString(ACR_EAITask, alpha.m_iAITask), typename.EnumToString(ACR_EAITask, bravo.m_iAITask)));
   if (!attacking) ACR_TestRegress.Skip("twoicui: a grab or an attack is a channel line and on the card", "no 2IC squad attacked within 70 s");
   else
   {
    ACR_TwoICLine taking = Newest("US", "Taking");
    if (!taking) taking = Newest("US", "Attacking");
    if (!taking) taking = Newest("US", "Counterattacking");
    ACR_TestRegress.Check("twoicui: a grab or an attack is a channel line and on the card", taking != null && (doing.Contains("taking") || doing.Contains("attacking") || doing.Contains("raiding")), string.Format("'%1'; %2", doing, Lines("US")));
   }
   // A player's order claims Alpha.
   s_iRunsBefore = card.m_iSquads;
   ACR_SquadAI.TakeBack(alpha, ACR_ERequest.MOVE_TO);
   alpha.MoveTo(alpha.GetAnchor().GetOrigin() + Vector(-15, 0, 0));
   ACR_TwoICLine yours = Newest("US", alpha.GetName() + " is yours now");
   ACR_TwoICCard after = ACR_TwoIC.Build(manager, "US");
   ACR_TestRegress.Check("twoicui: a squad a player claims is a line, and one squad fewer on the card", yours != null && after.m_iSquads == s_iRunsBefore - 1, string.Format("runs %1 -> %2; %3", s_iRunsBefore, after.m_iSquads, Lines("US")));
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   // The channel keeps the newest CHANNEL_LINES; a status change is a new signature.
   for (int i = 0; i < ACR_TwoIC.CHANNEL_LINES + 4; i++) ACR_TwoIC.Say("US", ACR_ETwoICLine.REQUEST, string.Format("request %1", i));
   int newest = ACR_TwoIC.Say("US", ACR_ETwoICLine.REQUEST, "the newest request");
   string before = ACR_TwoIC.ChannelSig("US");
   bool setOk = ACR_TwoIC.SetStatus("US", newest, "Coming");
   string after = ACR_TwoIC.ChannelSig("US");
   bool same = ACR_TwoIC.SetStatus("US", newest, "Coming") && ACR_TwoIC.ChannelSig("US") == after;
   bool gone = !ACR_TwoIC.SetStatus("US", 1, "Coming");
   array<int> ids = {};
   array<int> kinds = {};
   array<string> texts = {};
   array<string> answers = {};
   array<string> statuses = {};
   array<int> ages = {};
   array<int> flags = {};
   ACR_TwoIC.Channel("US", ids, kinds, texts, answers, statuses, ages, flags);
   bool packed = ids.Count() == ACR_TwoIC.CHANNEL_LINES && texts.Count() == ids.Count() && ages.Count() == ids.Count() && ids[ids.Count() - 1] == newest && statuses[statuses.Count() - 1] == "Coming" && ages[0] >= 0;
   bool ussr = ACR_TwoIC.Count("USSR") <= ACR_TwoIC.CHANNEL_LINES && !ACR_TwoIC.Find("USSR", newest);
   ACR_TestRegress.Check("twoicui: the channel keeps the newest lines per side; a new status is a new signature, the same one isn't", ACR_TwoIC.Count("US") == ACR_TwoIC.CHANNEL_LINES && setOk && before != after && same && gone && packed && ussr, string.Format("lines %1, set %2, sig %3 -> %4, same %5, gone %6, packed %7 (%8 ids), USSR apart %9", ACR_TwoIC.Count("US") , setOk, before, after, same, gone, packed, ids.Count(), ussr));
   // A side the AI commander leads: the 2IC stands by, in command and in its start-up hold alike
   // (pn10: it took on Red's squads before Red's AI commander took command).
   ACR_FactionSettings settings = ACR_FactionSettings.Find("US");
   bool wasActive = ai.m_bActive;
   ACR_TwoICCard standby;
   ACR_TwoICCard held;
   if (settings)
   {
    settings.m_bAICommander = true;
    ai.m_bActive = true;
    standby = ACR_TwoIC.Build(manager, "US");
    ai.m_bActive = false;
    held = ACR_TwoIC.Build(manager, "US");
    settings.m_bAICommander = false;
    ai.m_bActive = wasActive;
   }
   bool heldOff = held && !held.m_bRunning;
   ACR_TestRegress.Check("twoicui: a side the AI commander leads has no 2IC, in command or in its start-up hold", standby && !standby.m_bRunning && standby.m_sDoing.Contains("stands by") && heldOff && ACR_TwoIC.Build(manager, "US").m_bRunning, Card(standby) + string.Format("; in the hold: running %1", !heldOff));
   bool ages2 = ACR_TwoICView.Age(30) == "now" && ACR_TwoICView.Age(240) == "4 min" && ACR_TwoICView.Age(3900) == "1 h 5 min";
   ACR_TestRegress.Check("twoicui: the channel's ages read now, 4 min, 1 h 5 min", ages2, string.Format("%1 / %2 / %3", ACR_TwoICView.Age(30), ACR_TwoICView.Age(240), ACR_TwoICView.Age(3900)));
   CheckPanel(manager, ai, alpha, bravo);
   ACR_Options.Unset("acr2IC");
   ACR_TwoIC.ResetForTest();
   ACR_TestRegress.Complete("twoicui", CHECKS);
   s_iStep = 4;
  }
 }

 // The panel (step 3): a row per squad of the side, the claimed one the commander's, the 2IC's with
 // its task; the reserve share is the side's own; a small force holds with at most half; the rows
 // go to a player only while his panel is open, the first at once.
 protected static void CheckPanel(ACR_Manager manager, ACR_AICommander ai, ACR_Squad alpha, ACR_Squad bravo)
 {
  ACR_TwoICPanelData panel = ACR_TwoIC.BuildPanel(manager, "US");
  int all;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad.m_Group && squad.GetFaction() && squad.GetFaction().GetFactionKey() == "US") all++;
  int a = panel.m_SquadNames.Find(alpha.GetName());
  int b = panel.m_SquadNames.Find(bravo.GetName());
  bool rows = panel.m_SquadNames.Count() == all && panel.m_SquadIds.Count() == all && a >= 0 && b >= 0;
  string detail = string.Format("%1 rows of %2", panel.m_SquadNames.Count(), all);
  if (rows)
  {
   rows = (panel.m_SquadFlags[a] & ACR_TwoIC.ROW_2IC) == 0 && panel.m_RunBys[a] == "commander" && (panel.m_SquadFlags[b] & ACR_TwoIC.ROW_2IC) != 0 && panel.m_RunBys[b] == "2IC" && panel.m_Doings[b] == ai.TaskText(bravo) && !panel.m_Doings[b].IsEmpty() && !panel.m_Wheres[b].IsEmpty();
   detail += string.Format("; Alpha '%1' '%2' '%3' %4; Bravo '%5' '%6' '%7' %8", panel.m_Wheres[a], panel.m_Doings[a], panel.m_RunBys[a], panel.m_SquadFlags[a], panel.m_Wheres[b], panel.m_Doings[b], panel.m_RunBys[b], panel.m_SquadFlags[b]);
  }
  ACR_TestRegress.Check("twoicui: the panel has a row per squad of the side: where, doing and who runs it", rows, detail);
  // The reserve share: the panel's, for its own side only.
  ACR_AICommander enemy = manager.FobAICommanderFor("USSR");
  ACR_TwoIC.SetShareFor("US", 20);
  bool own = ai.SideShare() == 20 && ACR_TwoIC.BuildPanel(manager, "US").m_iShare == 20;
  bool apart = !enemy || enemy.SideShare() == ACR_AICommander.BuildShare();
  ACR_TwoIC.SetShareFor("US", 99);
  bool clamped = ai.SideShare() == ACR_TwoIC.SHARE_MAX;
  ACR_TestRegress.Check("twoicui: the panel's reserve share is its side's own (the enemy keeps the setting's), at most 30%", own && apart && clamped, string.Format("US %1, USSR %2 (setting %3), clamped %4", own, apart, ACR_AICommander.BuildShare(), ai.SideShare()));
  ACR_TwoIC.ResetForTest();
  // Small force: fewer than SMALL_FORCE squads, at most half of them hold.
  int count = ACR_SquadAI.CountFor("US");
  bool small = ai.SmallForceForTest();
  int cap = ai.HoldersCapForTest();
  int expected = ai.Doctrine().m_iHolders;
  if (count < ACR_TwoIC.SMALL_FORCE) expected = Math.Min(expected, count / 2);
  ACR_TestRegress.Check("twoicui: under 4 squads the 2IC is a small force: at most half of them hold", small == (count < ACR_TwoIC.SMALL_FORCE) && cap == expected && ACR_TwoIC.SmallText("US").Contains(count.ToString()), string.Format("%1 squads: small %2, holders cap %3 (expected %4); '%5'", count, small, cap, expected, ACR_TwoIC.SmallText("US")));
  // Open and fresh.
  ACR_TwoIC.SetPanelOpen(99, true);
  bool opened = ACR_TwoIC.PanelOpen(99) && ACR_TwoIC.PanelFresh(99) && !ACR_TwoIC.PanelFresh(99);
  ACR_TwoIC.SetPanelOpen(99, false);
  bool closed = !ACR_TwoIC.PanelOpen(99);
  ACR_TestRegress.Check("twoicui: the panel's rows go to a player only while it is open, the first at once", opened && closed, string.Format("opened %1, closed %2", opened, closed));
 }

 protected static ACR_TwoICLine Newest(string faction, string start)
 {
  ACR_TwoICLine found;
  array<int> ids = {};
  array<int> kinds = {};
  array<string> texts = {};
  array<string> answers = {};
  array<string> statuses = {};
  array<int> ages = {};
  array<int> flags = {};
  ACR_TwoIC.Channel(faction, ids, kinds, texts, answers, statuses, ages, flags);
  foreach (int i, int id : ids)
   if (texts[i].StartsWith(start)) found = ACR_TwoIC.Find(faction, id);
  return found;
 }

 protected static string Lines(string faction)
 {
  array<int> ids = {};
  array<int> kinds = {};
  array<string> texts = {};
  array<string> answers = {};
  array<string> statuses = {};
  array<int> ages = {};
  array<int> flags = {};
  ACR_TwoIC.Channel(faction, ids, kinds, texts, answers, statuses, ages, flags);
  string all = string.Format("%1 line(s):", ids.Count());
  foreach (string text : texts) all += " '" + text + "'";
  return all;
 }

 protected static string Card(ACR_TwoICCard card)
 {
  if (!card) return "no card";
  return "card " + card.Sig();
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
