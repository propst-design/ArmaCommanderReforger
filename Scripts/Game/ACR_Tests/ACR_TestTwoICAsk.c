// -acrTwoICAskTest 1 (server-capable, ~4 min): "Ask the 2IC" (#14 v1 step 4, ACR_TwoICRequests). No
// AI commander, the 2IC on. Hold this: a free squad of the 2IC's is coming at once, then holds the
// spot until released, AssignDefenders leaving it there; released, it is free again and the ring
// goes. Take this base: our own base is refused; an enemy base becomes the 2IC's attack target (or
// it says why not yet), cancelled the target is dropped. Support me 4 km out: can't yet (too far),
// kept queued it says so, cancelled it ends. A player gets at most 3 live requests. With the AI
// commander in command the requests end.
class ACR_TestTwoICAsk
{
 static const int CHECKS = 11;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iHold;
 static vector s_vHold;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_TwoIC.ResetForTest();
   ACR_Options.Unset("acr2IC");
  }
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!bravo || !bravo.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("twoicask", "no Bravo squad");
   return;
  }
  if (tick > 260) ACR_TestRegress.Abort("twoicask", string.Format("timed out in step %1", s_iStep));
  if (tick < 8) return;
  int waited = tick - s_iStepAt;
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  if (!ai)
  {
   ACR_TestRegress.Abort("twoicask", "no US AI commander");
   return;
  }

  if (s_iStep == 0)
  {
   // The 2IC on and Hold this (120 m from Bravo) in the same second: the request is answered
   // before the 2IC's first plans give every squad a task (requests first).
   ACR_Options.Set("acr2IC", "1");
   s_vHold = bravo.GetAnchor().GetOrigin() + Vector(120, 0, 0);
   string asked = ACR_TwoICRequests.AskFor(manager, "US", 7, "Tester", ACR_ETwoICAsk.HOLD, s_vHold);
   PrintFormat("[ACR] TWOICASK hold: %1", asked);
   ACR_TwoICRequest r = Newest();
   if (r) s_iHold = r.m_iId;
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   ACR_TwoICRequest hold = ACR_TwoICRequests.Find(s_iHold);
   if ((!hold || hold.m_iState != ACR_ETwoICState.COMING) && waited < 4) return;
   ACR_TwoICLine line = ACR_TwoIC.Find("US", s_iHold);
   bool coming = hold && hold.m_iState == ACR_ETwoICState.COMING && hold.m_Squad && ai.OnRequest(hold.m_Squad) && line && line.m_sStatus.StartsWith("Coming") && !line.m_sAnswer.IsEmpty() && (line.m_iActions & ACR_TwoICRequests.ACT_CANCEL) != 0;
   array<vector> rings = {};
   array<string> labels = {};
   array<int> kinds = {};
   ACR_TwoICRequests.Rings("US", rings, labels, kinds);
   ACR_TestRegress.Check("twoicask: Hold this: a free squad of the 2IC's is coming at once, the line says who, with a ring on the map", coming && rings.Count() == 1 && labels[0].StartsWith("Hold"), Describe(hold) + string.Format("; %1 ring(s) %2", rings.Count(), Labels(labels)));
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   ACR_TwoICRequest held = ACR_TwoICRequests.Find(s_iHold);
   if (held && held.m_iState == ACR_ETwoICState.COMING && waited < 160) return;
   bool holding = held && held.m_iState == ACR_ETwoICState.HOLDING && held.m_Squad && held.m_Squad.m_iAITask == ACR_EAITask.DEFEND && ai.OnRequest(held.m_Squad);
   ACR_TestRegress.Check("twoicask: there, it holds the spot until released (the 2IC's own plans leave it there)", holding, Describe(held) + string.Format(" after %1 s", waited));
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   // Stays on it across two strategy ticks.
   if (waited < 32) return;
   ACR_TwoICRequest still = ACR_TwoICRequests.Find(s_iHold);
   ACR_Squad holder;
   if (still) holder = still.m_Squad;
   bool stays = still && still.m_iState == ACR_ETwoICState.HOLDING && holder && holder.GetAnchor() && vector.DistanceXZ(holder.GetAnchor().GetOrigin(), s_vHold) < 80;
   ACR_TestRegress.Check("twoicask: a holding squad stays on the spot across the 2IC's strategy ticks", stays, Describe(still));
   // The forecast's guard (pn44): a live Hold at a base covers the side it holds, not the far side.
   ACR_Base near;
   float nearDist = float.MAX;
   foreach (ACR_Base b : ACR_Base.s_All)
   {
    if (!b || b.m_bRetired) continue;
    float bd = vector.DistanceXZ(b.GetOrigin(), s_vHold);
    if (bd < nearDist)
    {
     nearDist = bd;
     near = b;
    }
   }
   if (near)
   {
    vector c = near.GetOrigin();
    float side = Math.Atan2(s_vHold[0] - c[0], s_vHold[2] - c[2]) * Math.RAD2DEG;
    bool inReach = nearDist <= near.m_fRadius + ACR_TwoICRequests.TAKE_RADIUS;
    bool middle = nearDist < ACR_TwoICRequests.HOLD_MIDDLE;
    bool thisSide = ACR_TwoICRequests.HoldingAt("US", near, side, 60);
    bool farSide = ACR_TwoICRequests.HoldingAt("US", near, side + 180, 60);
    ACR_TestRegress.Check("twoicask: a live Hold at a base covers the side it holds (no forecast advice there again), not the far side", thisSide == inReach && farSide == (inReach && middle), string.Format("%1 %2 m away (reach %3), this side %4, far side %5", near.m_sName, (int)nearDist, inReach, thisSide, farSide));
   }
   else ACR_TestRegress.Skip("twoicask: a live Hold at a base covers the side it holds (no forecast advice there again), not the far side", "no base on the range");
   // Released.
   string released = ACR_TwoICRequests.ActOn(manager, still, ACR_TwoICRequests.ACT_RELEASE);
   ACR_TwoICLine line = ACR_TwoIC.Find("US", s_iHold);
   array<vector> rings = {};
   array<string> labels = {};
   array<int> kinds = {};
   ACR_TwoICRequests.Rings("US", rings, labels, kinds);
   bool free = holder && !ai.OnRequest(holder) && holder.m_iAITask == ACR_EAITask.FREE;
   ACR_TestRegress.Check("twoicask: released, its squad is free again, the line says so and the ring goes", free && !ACR_TwoICRequests.Find(s_iHold) && line && line.m_sStatus == "Done" && line.m_sAnswer.Contains("free again") && rings.IsEmpty(), string.Format("'%1'; squad free %2; line '%3' '%4'; %5 ring(s)", released, free, StatusOf(line), AnswerOf(line), rings.Count()));
   NextStep(tick);
   return;
  }

  if (s_iStep == 4)
  {
   // Take this base: ours is refused; an enemy one is the next attack's target, or why not yet.
   ACR_Base own, enemy;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (!base || base.m_bRetired || base.m_bBuilt) continue;
    if (!own && base.m_sOwner == "US") own = base;
    if (!enemy && base.m_sOwner == "USSR") enemy = base;
   }
   if (!own || !enemy)
   {
    ACR_TestRegress.Skip("twoicask: Take this base: our own refused, an enemy one the attack target, cancelled dropped", "no US or USSR base on the range");
    NextStep(tick);
    return;
   }
   string refused = ACR_TwoICRequests.AskFor(manager, "US", 7, "Tester", ACR_ETwoICAsk.TAKE, own.GetOrigin());
   ACR_TwoICRequests.AskFor(manager, "US", 7, "Tester", ACR_ETwoICAsk.TAKE, enemy.GetOrigin() + Vector(40, 0, 0));
   ACR_TwoICRequest take = Newest();
   ACR_TwoICRequests.Tick("US", ai, true);
   bool answered = take && take.m_Base == enemy && (take.m_iState == ACR_ETwoICState.COMING || take.m_iState == ACR_ETwoICState.CANT) && !take.m_sAnswer.IsEmpty();
   bool targeted = ai.RequestedTarget() == enemy || ai.IsAttackingForTest(enemy) || take.m_iState == ACR_ETwoICState.CANT;
   string detail = string.Format("own: '%1'; enemy: %2, requested %3", refused, Describe(take), ai.RequestedTarget() == enemy);
   ACR_TwoICRequests.ActOn(manager, take, ACR_TwoICRequests.ACT_CANCEL);
   bool dropped = ai.RequestedTarget() != enemy && !ACR_TwoICRequests.Find(take.m_iId);
   ACR_TestRegress.Check("twoicask: Take this base: our own refused, an enemy one the attack target (or why not yet), cancelled dropped", refused.Contains("ours already") && answered && targeted && dropped, detail + string.Format("; dropped %1", dropped));
   NextStep(tick);
   return;
  }

  if (s_iStep == 5)
  {
   // Support me 4 km out: too far; kept queued; cancelled.
   vector far = bravo.GetAnchor().GetOrigin() + Vector(0, 0, 4000);
   ACR_TwoICRequests.AskFor(manager, "US", 7, "Tester", ACR_ETwoICAsk.SUPPORT, far);
   ACR_TwoICRequest support = Newest();
   ACR_TwoICRequests.Tick("US", ai, true);
   bool cant = support && support.m_iState == ACR_ETwoICState.CANT && support.m_sAnswer.Contains("m away");
   ACR_TwoICLine line;
   if (support) line = ACR_TwoIC.Find("US", support.m_iId);
   bool keepOffered = line && (line.m_iActions & ACR_TwoICRequests.ACT_KEEP) != 0 && line.m_sStatus == "Can't yet";
   string kept = ACR_TwoICRequests.ActOn(manager, support, ACR_TwoICRequests.ACT_KEEP);
   bool queued = support && support.m_bQueued && line && line.m_sStatus == "Queued" && (line.m_iActions & ACR_TwoICRequests.ACT_KEEP) == 0;
   ACR_TwoICRequests.ActOn(manager, support, ACR_TwoICRequests.ACT_CANCEL);
   bool cancelled = support && support.m_iState == ACR_ETwoICState.CANCELLED && line && line.m_sStatus == "Cancelled";
   ACR_TestRegress.Check("twoicask: Support me 4 km out: can't yet (too far), kept queued it says Queued, cancelled it ends", cant && keepOffered && queued && cancelled, Describe(support) + string.Format("; keep offered %1, '%2', queued %3, cancelled %4", keepOffered, kept, queued, cancelled));
   // At most 3 live requests a player.
   for (int i = 0; i < 3; i++) ACR_TwoICRequests.AskFor(manager, "US", 8, "Other", ACR_ETwoICAsk.SUPPORT, far);
   string fourth = ACR_TwoICRequests.AskFor(manager, "US", 8, "Other", ACR_ETwoICAsk.SUPPORT, far);
   ACR_TestRegress.Check("twoicask: a player gets at most 3 live requests", fourth.Contains("3 requests") && ACR_TwoICRequests.LiveCount("US") == 3, string.Format("'%1', %2 live", fourth, ACR_TwoICRequests.LiveCount("US")));
   // The line text: the asking player, the place (before the flood below may drop the ended line).
   ACR_TwoICLine asked = ACR_TwoIC.Find("US", s_iHold);
   ACR_TestRegress.Check("twoicask: a request's line says who asked and where", asked && asked.m_sText.StartsWith("Tester: hold ") && asked.m_iPlayer == 7 && asked.m_iKind == ACR_ETwoICLine.REQUEST, string.Format("'%1' player %2", TextOf(asked), PlayerOf(asked)));
   // pn44 (7 Nov): reports pushed live requests out of the channel, and their Cancel with them.
   // 30 reports later each live request still has its line and Cancel, and Cancel still ends it.
   array<int> otherIds = {};
   ACR_TwoICRequests.LiveIdsForTest("US", 8, otherIds);
   int lastReport;
   for (int f = 0; f < 30; f++) lastReport = ACR_TwoIC.Say("US", ACR_ETwoICLine.REPORT, string.Format("Flood report %1", f));
   int keptCount;
   foreach (int keptId : otherIds)
   {
    ACR_TwoICLine keptLine = ACR_TwoIC.Find("US", keptId);
    if (keptLine && (keptLine.m_iActions & ACR_TwoICRequests.ACT_CANCEL) != 0) keptCount++;
   }
   int lines = ACR_TwoIC.Count("US");
   bool lastKept = ACR_TwoIC.Find("US", lastReport) != null;
   ACR_TwoICRequest first = null;
   if (!otherIds.IsEmpty()) first = ACR_TwoICRequests.Find(otherIds[0]);
   string cancel = "-";
   if (first) cancel = ACR_TwoICRequests.ActOn(manager, first, ACR_TwoICRequests.ACT_CANCEL);
   ACR_TwoICLine firstLine = null;
   if (!otherIds.IsEmpty()) firstLine = ACR_TwoIC.Find("US", otherIds[0]);
   bool ended = firstLine && firstLine.m_sStatus == "Cancelled" && ACR_TwoICRequests.LiveCount("US") == 2;
   ACR_TestRegress.Check("twoicask: live requests keep their line and Cancel through 30 reports (the channel drops old reports instead), and Cancel still ends one", otherIds.Count() == 3 && keptCount == 3 && lines <= ACR_TwoIC.CHANNEL_LINES + 3 && lastKept && ended, string.Format("%1 requests found, %2 kept with Cancel, %3 lines, cancel '%4', ended %5, newest report kept %6", otherIds.Count(), keptCount, lines, cancel, ended, lastKept));
   // The AI commander takes the side: the requests end.
   ACR_TwoICRequests.Tick("US", ai, false);
   ACR_TestRegress.Check("twoicask: with the AI commander in command the requests end", ACR_TwoICRequests.LiveCount("US") == 0, string.Format("%1 live", ACR_TwoICRequests.LiveCount("US")));
   ACR_Options.Unset("acr2IC");
   ACR_TwoIC.ResetForTest();
   ACR_TestRegress.Complete("twoicask", CHECKS);
   s_iStep = 6;
  }
 }

 protected static ACR_TwoICRequest Newest()
 {
  return ACR_TwoICRequests.Last();
 }

 protected static string Describe(ACR_TwoICRequest r)
 {
  if (!r) return "no request";
  return string.Format("request %1 %2 '%3' squad %4", r.m_iId, typename.EnumToString(ACR_ETwoICState, r.m_iState), r.m_sAnswer, r.m_sSquad);
 }

 protected static string Labels(array<string> labels)
 {
  string all = "";
  foreach (string label : labels) all += " '" + label + "'";
  return all;
 }

 protected static string StatusOf(ACR_TwoICLine line)
 {
  if (!line) return "-";
  return line.m_sStatus;
 }

 protected static string AnswerOf(ACR_TwoICLine line)
 {
  if (!line) return "-";
  return line.m_sAnswer;
 }

 protected static string TextOf(ACR_TwoICLine line)
 {
  if (!line) return "-";
  return line.m_sText;
 }

 protected static int PlayerOf(ACR_TwoICLine line)
 {
  if (!line) return -1;
  return line.m_iPlayer;
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
