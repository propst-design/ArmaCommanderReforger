// -acrTwoICFobTest 1 (server-capable, ~1 min): the 2IC and FOBs while a player commands (pn44, 7 Nov:
// the whole-side 2IC never suggested a FOB, so "Send truck" and "Buy a truck" never came, and a
// player's armed Establish FOB never got its numbered sites). No AI commander; the test says a player
// commands US (ACR_TestTwoICAll.s_sCommanded), and with -acr2IC 2 ("I'm the 2IC") the 2IC runs the whole side.
// 1. Establish FOB armed: its sites come, through the manager's own tick.
// 2. "Build a FOB here" with a loaded construction truck by the 2IC's squad: it goes to build it, the
//    line says who with Cancel, the map shows the site as a sent truck's dashed ring.
// 3. Cancel: the build stops and the 2IC has no FOB project left.
// 4. No truck: the 2IC buys one at the base nearest the site, or says why it can't.
class ACR_TestTwoICFob
{
 static const int CHECKS = 4;
 static const int PLAYER = 9;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iRequest;
 static vector s_vSite;
 static int s_iRPBefore;
 static IEntity s_Truck;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_TwoIC.ResetForTest();
   ACR_TwoICSuggest.ResetForTest();
   ACR_Options.Unset("acr2IC");
   ACR_Options.Unset("acr2ICKeep");
   ACR_TestTwoICAll.s_sCommanded = "";
   s_iStep = 0;
  }
  ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (!bravo || !bravo.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("twoicfob", "no Bravo squad");
   return;
  }
  if (tick > 200) ACR_TestRegress.Abort("twoicfob", string.Format("timed out in step %1", s_iStep));
  if (tick < 8) return;
  int waited = tick - s_iStepAt;
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  if (!ai)
  {
   ACR_TestRegress.Abort("twoicfob", "no US AI commander");
   return;
  }

  if (s_iStep == 0)
  {
   // "I'm the 2IC" (-acr2IC 2 since main c6ea9c07; 1 is the plain 2IC), a player in command of
   // US: it runs the whole side.
   ACR_Options.Set("acr2IC", "2");
   ACR_TestTwoICAll.s_sCommanded = "US";
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   // Once Bravo is the 2IC's: Establish FOB armed with it, the sites behind the front come.
   if (!bravo.m_bAICommanded && waited < 20) return;
   string front = ACR_TwoICSuggest.AskSites(manager, PLAYER, "US", bravo.GetAnchor().GetOrigin());
   PrintFormat("[ACR] TWOICFOB sites asked behind '%1' (whole side %2)", front, ACR_SquadAI.RunsAll(manager, "US"));
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   int sites = ACR_TwoICSuggest.SitesSentForTest(PLAYER);
   if (sites < 0 && waited < 40) return;
   ACR_TestRegress.Check("twoicfob: with the 2IC running the whole side, an armed Establish FOB still gets the 2IC's sites", ACR_SquadAI.RunsAll(manager, "US") && sites >= 0, string.Format("sites %1 after %2 s, job still open %3", sites, waited, ACR_TwoICSuggest.JobFor(PLAYER)));
   // A clear site behind our nearest base, and a loaded construction truck by Bravo.
   s_vSite = vector.Zero;
   ACR_Base own;
   float ownDist = float.MAX;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (!base || base.m_bRetired || base.m_bBuilt || base.m_sOwner != "US") continue;
    float d = vector.DistanceXZ(base.GetOrigin(), bravo.GetAnchor().GetOrigin());
    if (d < ownDist)
    {
     ownDist = d;
     own = base;
    }
   }
   array<ref ACR_FobSite> found = {};
   if (own) ai.TwoICFobSites(own, -1, found);
   array<ACR_FobSite> best = {};
   ACR_TwoICSuggest.Top(found, 1, best);
   if (!best.IsEmpty()) s_vSite = best[0].m_vPos;
   s_Truck = ACR_Manager.SpawnPrefab(ACR_TestTwoICSuggest.CONSTRUCTION, ACR_Manager.Ground(bravo.GetAnchor().GetOrigin() + Vector(10, 0, 0)));
   if (s_Truck)
   {
    ACR_Vehicles.SetOwner(s_Truck, "US");
    manager.m_Logistics.AddStock(s_Truck, ACR_FobBuild.LOADS_RESERVED);
   }
   if (s_vSite == vector.Zero)
   {
    ACR_TestRegress.Skip("twoicfob: Build a FOB here: the 2IC's truck goes, the line says who with Cancel, a dashed ring on the site", "no clear FOB site on the range");
    ACR_TestRegress.Skip("twoicfob: Cancel stops the build and the 2IC has no FOB project left", "no clear FOB site on the range");
    ACR_TestRegress.Skip("twoicfob: with no truck of its own the 2IC buys one at the base nearest the site, or says why not", "no clear FOB site on the range");
    if (s_Truck) SCR_EntityHelper.DeleteEntityAndChildren(s_Truck);
    ACR_TestTwoICAll.s_sCommanded = "";
    ACR_Options.Unset("acr2IC");
    ACR_TwoIC.ResetForTest();
    ACR_TestRegress.Complete("twoicfob", CHECKS);
    s_iStep = 6;
    return;
   }
   // Bravo, by the truck, off whatever the whole-side 2IC gave it (an attack's squad is never taken
   // off it to build: pn44's test run had it staging for Outpost). Requests are answered ahead of the
   // 2IC's own plans, so it is still free when the ask is.
   ai.Forget(bravo);
   bravo.SetAITask(ACR_EAITask.FREE);
   string asked = ACR_TwoICRequests.AskFor(manager, "US", PLAYER, "Tester", ACR_ETwoICAsk.FOB, s_vSite);
   ACR_TwoICRequest r = ACR_TwoICRequests.Last();
   s_iRequest = 0;
   if (r) s_iRequest = r.m_iId;
   PrintFormat("[ACR] TWOICFOB asked a FOB at %1: %2", s_vSite, asked);
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   ACR_TwoICRequest r = ACR_TwoICRequests.Find(s_iRequest);
   if (r && r.m_iState != ACR_ETwoICState.COMING && waited < 10) return;
   ACR_TwoICLine line = ACR_TwoIC.Find("US", s_iRequest);
   ACR_Squad builder;
   if (r) builder = r.m_Squad;
   bool going = r && r.m_iState == ACR_ETwoICState.COMING && builder && builder.OrderState() == ACR_EOrderState.BUILD_FOB && ai.HasActiveFobProject();
   bool lineOk = line && line.m_sStatus.StartsWith("Coming") && line.m_sAnswer.Contains(r.m_sSquad) && (line.m_iActions & ACR_TwoICRequests.ACT_CANCEL) != 0;
   array<vector> rings = {};
   array<string> labels = {};
   array<int> kinds = {};
   ACR_TwoICRequests.Rings("US", rings, labels, kinds);
   bool ring = false;
   foreach (int i, int kind : kinds)
   {
    if (kind == ACR_TwoICSuggest.RING_SENT && labels[i].StartsWith("FOB site") && r && vector.DistanceXZ(rings[i], r.m_vPos) < 1) ring = true;
   }
   ACR_TestRegress.Check("twoicfob: Build a FOB here: the 2IC's truck goes, the line says who with Cancel, a dashed ring on the site", going && lineOk && ring, Describe(r) + string.Format("; line '%1' '%2'; ring %3", StatusOf(line), AnswerOf(line), ring));
   // Cancel: the build stops.
   string cancelled = "-";
   if (r) cancelled = ACR_TwoICRequests.ActOn(manager, r, ACR_TwoICRequests.ACT_CANCEL);
   bool stopped = !ACR_TwoICRequests.Find(s_iRequest) && (!builder || builder.OrderState() != ACR_EOrderState.BUILD_FOB) && !ai.HasActiveFobProject();
   ACR_TestRegress.Check("twoicfob: Cancel stops the build and the 2IC has no FOB project left", stopped, string.Format("'%1'; builder order %2; project %3", cancelled, BuilderOrder(builder), ai.HasActiveFobProject()));
   NextStep(tick);
   return;
  }

  if (s_iStep == 4)
  {
   // No truck: the 2IC buys one (or says why it can't).
   if (s_Truck) SCR_EntityHelper.DeleteEntityAndChildren(s_Truck);
   s_Truck = null;
   ACR_FactionData us = manager.m_Bases.GetFaction("US");
   if (us) us.m_iRP = 400;
   s_iRPBefore = manager.m_Bases.GetRP("US");
   ai.ResetFobTruckBoughtForTest();
   ACR_TwoICRequests.AskFor(manager, "US", PLAYER, "Tester", ACR_ETwoICAsk.FOB, s_vSite);
   ACR_TwoICRequest r = ACR_TwoICRequests.Last();
   s_iRequest = 0;
   if (r) s_iRequest = r.m_iId;
   NextStep(tick);
   return;
  }

  if (s_iStep == 5)
  {
   ACR_TwoICRequest r = ACR_TwoICRequests.Find(s_iRequest);
   if (r && r.m_sAnswer.IsEmpty() && waited < 6) return;
   string answer = "-";
   if (r) answer = r.m_sAnswer;
   int spent = s_iRPBefore - manager.m_Bases.GetRP("US");
   bool bought = r && answer.StartsWith("buying a construction truck at") && spent > 0;
   bool why = r && r.m_iState == ACR_ETwoICState.CANT && answer.Contains("can't buy one: ");
   bool truckFound = r && r.m_iState == ACR_ETwoICState.COMING;
   ACR_TestRegress.Check("twoicfob: with no truck of its own the 2IC buys one at the base nearest the site, or says why not", bought || why || truckFound, string.Format("%1; spent %2 RP", Describe(r), spent));
   if (r) ACR_TwoICRequests.ActOn(manager, r, ACR_TwoICRequests.ACT_CANCEL);
   ACR_TestTwoICAll.s_sCommanded = "";
   ACR_Options.Unset("acr2IC");
   ACR_TwoIC.ResetForTest();
   ACR_TestRegress.Complete("twoicfob", CHECKS);
   s_iStep = 6;
  }
 }

 protected static string Describe(ACR_TwoICRequest r)
 {
  if (!r) return "no request";
  return string.Format("request %1 %2 '%3' squad %4", r.m_iId, typename.EnumToString(ACR_ETwoICState, r.m_iState), r.m_sAnswer, r.m_sSquad);
 }

 protected static string BuilderOrder(ACR_Squad builder)
 {
  if (!builder) return "-";
  return typename.EnumToString(ACR_EOrderState, builder.OrderState());
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

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
