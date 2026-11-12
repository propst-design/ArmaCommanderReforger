// -acrTwoICDeclutterTest 1 (server-capable, ~1 min): the 2IC UI's calmer look (#14 declutter, sketch
// boards 1-4; ACR_TwoICCompact), its logic without widgets. The channel: which lines need the player
// (a request it can't do yet, a FOB site with no truck) and that they stay pinned until answered;
// repeats merging with their time span; the newest 3 (5 on hover); the tool bar's count. The card's
// three lines and its pill. The FOB sites' one-line labels and cards, the cursor's one line, a card
// kept clear of the other sites. A sent truck's site label and a pinned suggestion's words.
// "Buy a truck" on the pinned no-truck line: refused while the side can't afford it (nothing bought,
// no RP spent, the reason on the line), bought through the requisition path otherwise (RP charged as
// the menu charges) at the deploying base nearest the FOB site, never by the 2IC itself; another
// waits until that truck has deployed; once it exists the line offers Send truck again.
class ACR_TestTwoICDeclutter
{
 static const int CHECKS = 20;
 static const int ME = 7;
 static const int OTHER = 8;
 static int s_iStep;
 static int s_iStepAt;
 static int s_iBuyId;
 static int s_iRPStart = -1;
 static string s_sBought = "";
 static bool s_bGuardHeld;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Options.Set("acr2IC", "1");
   ACR_TwoIC.ResetForTest();
  }
  if (tick > 100 && s_iStep < 2)
  {
   ACR_TestRegress.Abort("twoicdeclutter", string.Format("timed out in step %1", s_iStep));
   s_iStep = 2;
   return;
  }
  if (tick == 8)
  {
   CheckNeedsYou();
   CheckChannel();
   CheckPinnedUntilAnswered();
   CheckLastThree();
   CheckAllView();
   CheckCard();
   CheckSites(h);
   CheckPlacing();
   CheckTruck();
   CheckTruckStates();
   if (StartBuy(h)) s_iStepAt = tick;
   else Finish();
   return;
  }
  if (tick > 8 && s_iStep == 1) WaitReady(tick);
 }

 protected static void Finish()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && s_iRPStart >= 0) manager.m_Bases.GetFaction("US").m_iRP = s_iRPStart;
  ACR_Options.Unset("acr2IC");
  ACR_TwoIC.ResetForTest();
  ACR_TestRegress.Complete("twoicdeclutter", CHECKS);
  s_iStep = 2;
 }

 // ---------------------------------------------------------------- Buy a truck
 static void CheckTruckStates()
 {
  string why = "6/8 upgrades";
  string readyAnswer = why + ACR_TwoICCompact.TRUCK_READY + "a construction truck can take it";
  int none = ACR_TwoICCompact.TruckState(why);
  int buy = ACR_TwoICCompact.TruckState(why + ACR_TwoICCompact.TRUCK_FAIL + ACR_TwoICSuggest.NO_TRUCK);
  int retry = ACR_TwoICCompact.TruckState(why + ACR_TwoICCompact.TRUCK_FAIL + "the site isn't clear any more (too steep)");
  int ready = ACR_TwoICCompact.TruckState(readyAnswer);
  bool labels = ACR_TwoICCompact.BuyLabel(21, "Farm") == "Buy a truck at Farm (21 RP)" && ACR_TwoICCompact.BuyLabel(21, "") == "Buy a truck (21 RP)" && ACR_TwoICCompact.BuyLabel(-1, "Farm") == "Buy a truck";
  int open = ACR_TwoICSuggest.ACT_ACCEPT | ACR_TwoICRequests.ACT_CANCEL;
  bool readyLine = ACR_TwoICCompact.NeedText(readyAnswer) == "a construction truck is ready for the FOB site" && ACR_TwoICCompact.Reasons(readyAnswer) == why && ACR_TwoICCompact.NeedsYou(ACR_ETwoICLine.SUGGESTION, readyAnswer, open, ME, true);
  bool ok = none == ACR_TwoICCompact.TRUCK_NONE && buy == ACR_TwoICCompact.TRUCK_BUY && retry == ACR_TwoICCompact.TRUCK_RETRY && ready == ACR_TwoICCompact.TRUCK_SENDABLE;
  ACR_TestRegress.Check("twoicdeclutter: a no-truck line offers \"Buy a truck at Farm (21 RP)\"; another reason, or a truck ready, offers Send truck", ok && labels && readyLine,
   string.Format("states %1/%2/%3/%4, labels %5, ready line %6 '%7'", none, buy, retry, ready, labels, readyLine, ACR_TwoICCompact.NeedText(readyAnswer)));
 }

 // A suggestion whose Send truck finds no truck; Buy a truck refused one RP short, nothing bought
 // by the 2IC over a few ticks with RP to spare, then bought as the menu buys. False: skipped.
 protected static bool StartBuy(ACR_TestHarness h)
 {
  string w1 = "twoicdeclutter: Buy a truck is refused while the side can't afford it: nothing bought, no RP spent, the reason on the line";
  string w2 = "twoicdeclutter: Buy a truck goes through the requisition path (bought, RP charged as the menu does), never by the 2IC itself";
  string w3 = "twoicdeclutter: once the truck exists, the pinned line offers Send truck again";
  string w4 = "twoicdeclutter: Buy a truck deploys at the side's base nearest the FOB site that can (the HQ only without a site)";
  string w5 = "twoicdeclutter: after Buy a truck another waits (\"a truck is on its way to deploy\") until that truck has deployed, then can be bought";
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_AICommander ai;
  ACR_Squad bravo;
  if (manager)
  {
   ai = manager.FobAICommanderFor("US");
   if (h.m_Range) bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  }
  ACR_Base front;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && !base.m_bRetired && base.m_sOwner == "US") { front = base; break; }
  string skip = "";
  if (!manager || !ai || !bravo || !bravo.GetAnchor() || !front) skip = "no US AI commander, Bravo or US base";
  else if (ACR_TwoICSuggest.HasTruck(manager, "US")) skip = "the range has a construction truck already";
  if (!skip.IsEmpty())
  {
   ACR_TestRegress.Skip(w1, skip);
   ACR_TestRegress.Skip(w2, skip);
   ACR_TestRegress.Skip(w3, skip);
   ACR_TestRegress.Skip(w4, skip);
   ACR_TestRegress.Skip(w5, skip);
   return false;
  }
  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  s_iRPStart = data.m_iRP;
  ACR_FobSite fake = new ACR_FobSite();
  fake.m_vPos = bravo.GetAnchor().GetOrigin() + Vector(0, 0, 700);
  ACR_TwoICSuggestion s = ACR_TwoICSuggest.Suggest("US", front, fake);
  string sent = "";
  if (s) sent = ACR_TwoICSuggest.ActOn(manager, s, ACR_TwoICSuggest.ACT_ACCEPT);
  ACR_TwoICLine line;
  if (s) line = ACR_TwoIC.Find("US", s.m_iId);
  int entry, cost;
  ACR_Base at;
  string offer = ACR_TwoICSuggest.TruckOffer(manager, "US", fake.m_vPos, entry, cost, at);
  if (!line || ACR_TwoICCompact.TruckState(line.m_sAnswer) != ACR_TwoICCompact.TRUCK_BUY || entry < 0 || !at)
  {
   string why = string.Format("Send truck '%1'; line '%2'; offer entry %3 at %4: '%5'", sent, AnswerOf(line), entry, at != null, offer);
   ACR_TestRegress.Check(w1, false, why);
   ACR_TestRegress.Skip(w2, "no no-truck line or no truck to requisition");
   ACR_TestRegress.Skip(w3, "no no-truck line or no truck to requisition");
   ACR_TestRegress.Skip(w4, "no no-truck line or no truck to requisition");
   ACR_TestRegress.Skip(w5, "no no-truck line or no truck to requisition");
   return false;
  }
  s_iBuyId = s.m_iId;
  CheckTruckBase(w4, fake.m_vPos, at);
  ACR_RequisitionEntry truck = data.m_aRequisitions[entry];
  int boughtBefore = truck.m_iBought;
  // One RP short: refused, nothing bought, nothing spent; the line's offer says why.
  data.m_iRP = cost - 1;
  string refused = ACR_TwoICSuggest.BuyTruck(manager, null, "US");
  int offerCost;
  string offerBase, offerWhy;
  ACR_TwoICSuggest.OfferFor(manager, "US", offerCost, offerBase, offerWhy);
  bool refusedOk = refused.StartsWith("Can't buy a truck: not enough RP") && data.m_iRP == cost - 1 && truck.m_iBought == boughtBefore && offerCost == cost && offerWhy.Contains("not enough RP");
  ACR_TestRegress.Check(w1, refusedOk, string.Format("'%1'; RP %2 (was %3), bought %4 -> %5; offer %6 RP '%7' at %8", refused, data.m_iRP, cost - 1, boughtBefore, truck.m_iBought, offerCost, offerWhy, at.m_sName));
  // RP to spare: the 2IC buys nothing by itself; the click buys it as the menu does.
  int spare = cost + 40;
  data.m_iRP = spare;
  for (int i = 0; i < 5; i++) ACR_TwoICSuggest.Tick(manager, "US", ai, true);
  bool idle = truck.m_iBought == boughtBefore && data.m_iRP == spare;
  int idleBought = truck.m_iBought - boughtBefore;
  int idleRP = data.m_iRP;
  string open;
  ACR_TwoICSuggest.OfferFor(manager, "US", offerCost, offerBase, open);
  string label = ACR_TwoICCompact.BuyLabel(offerCost, offerBase);
  string got = ACR_TwoICSuggest.BuyTruck(manager, null, "US");
  bool charged = data.m_iRP == spare - cost && truck.m_iBought == boughtBefore + 1 && (got.Contains("deploying") || got.Contains("waiting"));
  ACR_TestRegress.Check(w2, idle && open.IsEmpty() && charged && got.Contains(at.m_sName), string.Format("2IC alone: bought %1, RP %2 of %3; '%4' (offer '%5') -> '%6'; RP %7 (expected %8), bought %9", idleBought, idleRP, spare, label, open, got, data.m_iRP, spare - cost, truck.m_iBought));
  // The guard: held while the bought truck's squad isn't there.
  s_sBought = ACR_TwoICSuggest.TruckBought("US");
  string after;
  ACR_TwoICSuggest.OfferFor(manager, "US", offerCost, offerBase, after);
  bool deployed = ACR_TwoICSuggest.SquadNamed(manager, "US", s_sBought);
  s_bGuardHeld = !s_sBought.IsEmpty() && (deployed || after == "a truck is on its way to deploy");
  PrintFormat("[ACR] twoicdeclutter: bought '%1', deployed at once %2, offer then '%3'", s_sBought, deployed, after);
  s_iStep = 1;
  return true;
 }

 // The truck's base: the deploying base nearest the site (each such base wins for its own spot,
 // over the HQ); the HQ only without a site.
 protected static void CheckTruckBase(string name, vector site, ACR_Base picked)
 {
  array<ACR_Base> deploying = {};
  ACR_Base hq;
  ACR_Base nearest;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != "US" || base.m_bContested) continue;
   if (!ACR_LogisticsRules.CanDeploy(base) || ACR_LogisticsRules.RelayBlocked(base)) continue;
   deploying.Insert(base);
   if (!hq && ACR_LogisticsRules.IsHQ(base)) hq = base;
   float d = vector.DistanceXZ(base.GetOrigin(), site);
   if (d < best)
   {
    best = d;
    nearest = base;
   }
  }
  bool own = true;
  string names = "";
  foreach (ACR_Base each : deploying)
  {
   ACR_Base got = ACR_TwoICSuggest.TruckBase("US", each.GetOrigin());
   if (got != each) own = false;
   names += each.m_sName;
   if (each == hq) names += " (HQ)";
   names += " ";
  }
  bool noSite = !hq || ACR_TwoICSuggest.TruckBase("US", vector.Zero) == hq;
  string pickedName = "-";
  if (picked) pickedName = picked.m_sName;
  string nearestName = "-";
  if (nearest) nearestName = nearest.m_sName;
  ACR_TestRegress.Check(name, picked && picked == nearest && own && noSite, string.Format("site's offer %1, nearest %2; deploying: %3; each its own %4; HQ without a site %5", pickedName, nearestName, names, own, noSite));
 }

 // Each tick until the bought truck can take the build: the pinned line says so, with Send truck.
 protected static void WaitReady(int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  ACR_TwoICSuggest.Tick(manager, "US", ai, true);
  ACR_TwoICLine line = ACR_TwoIC.Find("US", s_iBuyId);
  int state = ACR_TwoICCompact.TRUCK_NONE;
  if (line) state = ACR_TwoICCompact.TruckState(line.m_sAnswer);
  bool ready = state == ACR_TwoICCompact.TRUCK_SENDABLE;
  if (!ready && tick - s_iStepAt < 40) return;
  bool pinned = line && ACR_TwoICCompact.NeedsYou(ACR_ETwoICLine.SUGGESTION, line.m_sAnswer, line.m_iActions, ME, true);
  bool send = line && (line.m_iActions & ACR_TwoICSuggest.ACT_ACCEPT) != 0;
  ACR_TestRegress.Check("twoicdeclutter: once the truck exists, the pinned line offers Send truck again", ready && pinned && send && ACR_TwoICSuggest.HasTruck(manager, "US"),
   string.Format("after %1 s: state %2, pinned %3, Send truck %4, truck %5; '%6'", tick - s_iStepAt, state, pinned, send, ACR_TwoICSuggest.HasTruck(manager, "US"), AnswerOf(line)));
  // The guard lifted: its squad is there, and another truck can be bought.
  int entry, cost;
  ACR_Base at;
  string offer = ACR_TwoICSuggest.TruckOffer(manager, "US", vector.Zero, entry, cost, at);
  bool deployed = ACR_TwoICSuggest.SquadNamed(manager, "US", s_sBought);
  bool lifted = deployed && offer != "a truck is on its way to deploy" && ACR_TwoICSuggest.TruckBought("US").IsEmpty();
  ACR_TestRegress.Check("twoicdeclutter: after Buy a truck another waits (\"a truck is on its way to deploy\") until that truck has deployed, then can be bought", s_bGuardHeld && lifted,
   string.Format("'%1': held while not deployed %2; deployed %3, offer then '%4'", s_sBought, s_bGuardHeld, deployed, offer));
  ACR_TwoICSuggestion s = ACR_TwoICSuggest.Find(s_iBuyId);
  if (s) ACR_TwoICSuggest.ActOn(manager, s, ACR_TwoICRequests.ACT_CANCEL);
  Finish();
 }

 protected static string AnswerOf(ACR_TwoICLine line)
 {
  if (!line) return "-";
  return line.m_sAnswer;
 }

 // ---------------------------------------------------------------- the channel
 static void CheckNeedsYou()
 {
  int cantYet = ACR_TwoICRequests.ACT_KEEP | ACR_TwoICRequests.ACT_CANCEL;
  string failed = "6/8 upgrades" + ACR_TwoICCompact.TRUCK_FAIL + "no construction truck for the FOB site (requisition one)";
  int suggestion = ACR_TwoICSuggest.ACT_ACCEPT | ACR_TwoICRequests.ACT_CANCEL;
  bool asker = ACR_TwoICCompact.NeedsYou(ACR_ETwoICLine.REQUEST, "2IC: can't yet", cantYet + 16 * ME, ME, false);
  bool notOthers = !ACR_TwoICCompact.NeedsYou(ACR_ETwoICLine.REQUEST, "2IC: can't yet", cantYet + 16 * ME, OTHER, false);
  bool commander = ACR_TwoICCompact.NeedsYou(ACR_ETwoICLine.REQUEST, "2IC: can't yet", cantYet + 16 * ME, OTHER, true);
  bool coming = !ACR_TwoICCompact.NeedsYou(ACR_ETwoICLine.REQUEST, "2IC: Alpha is coming", ACR_TwoICRequests.ACT_CANCEL + 16 * ME, ME, true);
  bool truck = ACR_TwoICCompact.NeedsYou(ACR_ETwoICLine.SUGGESTION, failed, suggestion, ME, true);
  bool truckNotOthers = !ACR_TwoICCompact.NeedsYou(ACR_ETwoICLine.SUGGESTION, failed, suggestion, ME, false);
  bool open = !ACR_TwoICCompact.NeedsYou(ACR_ETwoICLine.SUGGESTION, "6/8 upgrades", suggestion, ME, true);
  bool report = !ACR_TwoICCompact.NeedsYou(ACR_ETwoICLine.REPORT, "", 0, ME, true);
  ACR_TestRegress.Check("twoicdeclutter: a request it can't do yet (its asker or the commander) and a FOB site with no truck (the commander) need the player; an open suggestion, a coming request and a report don't",
   asker && notOthers && commander && coming && truck && truckNotOthers && open && report,
   string.Format("asker %1, others not %2, commander %3, coming not %4, truck %5 (others not %6), open not %7, report not %8", asker, notOthers, commander, coming, truck, truckNotOthers, open, report));
 }

 // A channel as the sketch's: the 2IC took on squads one by one, took a base, suggested a site it
 // then had no truck for; my request it can't do yet, another one coming; an attack.
 static void Sample(notnull array<int> ids, notnull array<int> kinds, notnull array<string> texts, notnull array<string> answers, notnull array<string> statuses, notnull array<int> ages, notnull array<int> flags)
 {
  string failed = "6/8 upgrades" + ACR_TwoICCompact.TRUCK_FAIL + "no construction truck for the FOB site (requisition one with its 500 supplies, then Send truck again)";
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REPORT, "Took on Bravo", "", "", 51, 0);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REPORT, "Took on 3 squads nobody leads", "", "", 45, 0);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REPORT, "Took on Charlie", "", "", 38, 0);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REPORT, "Took Le Moule Sawmill", "", "", 30, 0);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.SUGGESTION, "2IC suggests: a FOB behind Le Moule Sawmill", failed, "Suggested", 20, ACR_TwoICSuggest.ACT_ACCEPT | ACR_TwoICRequests.ACT_CANCEL);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REQUEST, "Me: hold Lamentin", "2IC: can't yet: no squad free", "Can't yet", 15, (ACR_TwoICRequests.ACT_KEEP | ACR_TwoICRequests.ACT_CANCEL) + 16 * ME);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REQUEST, "Me: support me at Le Moule", "2IC: Alpha is on its way", "Coming, 2 min", 10, ACR_TwoICRequests.ACT_CANCEL + 16 * ME);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REPORT, "Attacking Provins with Alpha, Delta", "", "", 4, 0);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REPORT, "Took Lacourt", "", "", 2, 0);
 }

 static void Add(notnull array<int> ids, notnull array<int> kinds, notnull array<string> texts, notnull array<string> answers, notnull array<string> statuses, notnull array<int> ages, notnull array<int> flags, int kind, string text, string answer, string status, int age, int flag)
 {
  ids.Insert(ids.Count() + 1);
  kinds.Insert(kind);
  texts.Insert(text);
  answers.Insert(answer);
  statuses.Insert(status);
  ages.Insert(age);
  flags.Insert(flag);
 }

 static void CheckChannel()
 {
  array<int> ids = {};
  array<int> kinds = {};
  array<string> texts = {};
  array<string> answers = {};
  array<string> statuses = {};
  array<int> ages = {};
  array<int> flags = {};
  Sample(ids, kinds, texts, answers, statuses, ages, flags);
  array<ref ACR_TwoICRow> rows = {};
  ACR_TwoICCompact.Rows(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICFilter.ALL, ME, true, rows);
  // Merged: the three "Took on" into one, 1 + 3 + 1 squads, its span by the clock (3:26 now).
  ACR_TwoICRow merged;
  if (!rows.IsEmpty()) merged = rows[0];
  string range = ACR_TwoICCompact.Range(merged, 0, 206);
  bool mergedOk = merged && merged.m_iMerged == 3 && merged.m_sText == "Took on 5 squads" && merged.m_iId == 3 && rows.Count() == 7;
  string routine = "";
  if (merged) routine = ACR_TwoICCompact.RoutineText(merged.m_sText);
  bool read = routine == "2IC took on 5 squads" && range == " (2:35–2:48)" && ACR_TwoICCompact.RoutineText("Bravo is yours now") == "2IC: Bravo is yours now";
  array<int> yIds = {1, 2};
  array<int> yKinds = {ACR_ETwoICLine.REPORT, ACR_ETwoICLine.REPORT};
  array<string> yTexts = {"Bravo is yours now", "Charlie is yours now"};
  array<string> yEmpty = {"", ""};
  array<int> yAges = {9, 3};
  array<int> yFlags = {0, 0};
  array<ref ACR_TwoICRow> yours = {};
  ACR_TwoICCompact.Rows(yIds, yKinds, yTexts, yEmpty, yEmpty, yAges, yFlags, ACR_ETwoICFilter.ALL, ME, true, yours);
  bool yoursOk = yours.Count() == 1 && yours[0].m_sText == "2 squads are yours now";
  string detail = string.Format("%1 rows; merged '%2' x%3 id %4 -> '%5'%6; yours '%7'", rows.Count(), TextOf(merged), MergedOf(merged), IdOf(merged), routine, range, TextOf(yours[0]));
  ACR_TestRegress.Check("twoicdeclutter: reports that follow each other merge (\"2IC took on 5 squads (2:35–2:48)\"), routine and dimmed", mergedOk && read && yoursOk, detail);
  // Pinned on top, newest first; then the newest 3, a row with my buttons ahead of the reports.
  array<ACR_TwoICRow> picked = {};
  ACR_TwoICCompact.Pick(rows, ACR_TwoICCompact.SHOWN, ME, true, picked);
  string order = "";
  foreach (ACR_TwoICRow row : picked) order += string.Format("%1%2 ", row.m_iId, Pin(row));
  bool pinnedTop = picked.Count() == 5 && picked[0].m_bPinned && picked[0].m_iId == 6 && picked[1].m_bPinned && picked[1].m_iId == 5;
  bool rest = pinnedTop && picked[2].m_iId == 9 && picked[3].m_iId == 8 && picked[4].m_iId == 7 && !picked[4].m_bPinned;
  ACR_TestRegress.Check("twoicdeclutter: what needs the player is pinned on top (newest first), then the newest 3, newest first", pinnedTop && rest, "shown: " + order);
  // The tool bar's count: the two that need me; none for another player who isn't the commander.
  int mine = ACR_TwoICCompact.NeedsYouCount(kinds, answers, flags, ME, true);
  int others = ACR_TwoICCompact.NeedsYouCount(kinds, answers, flags, OTHER, false);
  ACR_TestRegress.Check("twoicdeclutter: the 2IC button's count is the lines that need the player", mine == 2 && others == 0, string.Format("mine %1, another player's %2", mine, others));
 }

 // Kept queued, and the truck sent: neither is pinned any more, the count is 0.
 static void CheckPinnedUntilAnswered()
 {
  array<int> ids = {};
  array<int> kinds = {};
  array<string> texts = {};
  array<string> answers = {};
  array<string> statuses = {};
  array<int> ages = {};
  array<int> flags = {};
  Sample(ids, kinds, texts, answers, statuses, ages, flags);
  array<ref ACR_TwoICRow> before = {};
  ACR_TwoICCompact.Rows(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICFilter.ALL, ME, true, before);
  int pinnedBefore = Pinned(before);
  // The request kept queued: Cancel only; the suggestion's truck sent: no buttons.
  flags[5] = ACR_TwoICRequests.ACT_CANCEL + 16 * ME;
  statuses[5] = "Queued";
  flags[4] = 0;
  statuses[4] = "Truck sent";
  answers[4] = "Construction Truck 1 is on its way to build it";
  array<ref ACR_TwoICRow> after = {};
  ACR_TwoICCompact.Rows(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICFilter.ALL, ME, true, after);
  int count = ACR_TwoICCompact.NeedsYouCount(kinds, answers, flags, ME, true);
  ACR_TestRegress.Check("twoicdeclutter: a pinned line stays pinned until answered (kept queued, a truck sent)", pinnedBefore == 2 && Pinned(after) == 0 && count == 0, string.Format("pinned %1 -> %2, count %3", pinnedBefore, Pinned(after), count));
 }

 // Six reports nothing merges: the newest 3, newest first; 5 on hover.
 static void CheckLastThree()
 {
  array<int> ids = {};
  array<int> kinds = {};
  array<string> texts = {};
  array<string> answers = {};
  array<string> statuses = {};
  array<int> ages = {};
  array<int> flags = {};
  array<string> names = {"Took Lamentin", "Took Lacourt", "Took Provins", "Took Villeneuve", "Took Richemont", "Took Harbor"};
  foreach (int i, string name : names) Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REPORT, name, "", "", 60 - i * 10, 0);
  array<ref ACR_TwoICRow> rows = {};
  ACR_TwoICCompact.Rows(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICFilter.ALL, ME, false, rows);
  array<ACR_TwoICRow> three = {};
  ACR_TwoICCompact.Pick(rows, ACR_TwoICCompact.SHOWN, ME, false, three);
  array<ACR_TwoICRow> five = {};
  ACR_TwoICCompact.Pick(rows, ACR_TwoICCompact.SHOWN_MORE, ME, false, five);
  bool threeOk = three.Count() == 3 && three[0].m_iId == 6 && three[1].m_iId == 5 && three[2].m_iId == 4;
  bool fiveOk = five.Count() == 5 && five[4].m_iId == 2;
  // Requests only: the reports drop out.
  array<ref ACR_TwoICRow> requests = {};
  ACR_TwoICCompact.Rows(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICFilter.REQUESTS, ME, false, requests);
  ACR_TestRegress.Check("twoicdeclutter: the channel shows its newest 3 lines, newest first (5 on hover); a filter keeps its own", threeOk && fiveOk && requests.IsEmpty(), string.Format("3: %1, 5: %2 (last id %3), requests %4", threeOk, five.Count(), LastId(five), requests.Count()));
 }

 // The All view (pn12f v2: five blocks with buttons covered a third of the map): pinned, each open
 // request one line, the newest 3 others, at most ROW_CAP rows with "+N more"; buttons only for the
 // request under the cursor.
 static void CheckAllView()
 {
  array<int> ids = {};
  array<int> kinds = {};
  array<string> texts = {};
  array<string> answers = {};
  array<string> statuses = {};
  array<int> ages = {};
  array<int> flags = {};
  string failed = "8/8 upgrades" + ACR_TwoICCompact.TRUCK_FAIL + ACR_TwoICSuggest.NO_TRUCK;
  int cancel = ACR_TwoICRequests.ACT_CANCEL + 16 * ME;
  int release = ACR_TwoICRequests.ACT_RELEASE + 16 * ME;
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REPORT, "Took Lamentin", "", "", 90, 0);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REQUEST, "Me: take Outpost", "2IC: going for Outpost", "Coming", 80, cancel);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REQUEST, "Me: support me at Farm", "2IC: Alpha 1 supports you", "Supporting", 70, release);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REQUEST, "Me: hold Farm", "2IC: 1st Squad holds Farm", "Holding", 60, release);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.SUGGESTION, "2IC suggests: a FOB behind Farm", failed, "Suggested", 50, ACR_TwoICSuggest.ACT_ACCEPT | ACR_TwoICRequests.ACT_CANCEL);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REPORT, "Took Provins", "", "", 40, 0);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REQUEST, "Me: hold Lacourt", "2IC: Bravo is coming", "Coming", 30, cancel);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REPORT, "Took Villeneuve", "", "", 20, 0);
  Add(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICLine.REPORT, "Took Richemont", "", "", 10, 0);
  array<ref ACR_TwoICRow> rows = {};
  ACR_TwoICCompact.Rows(ids, kinds, texts, answers, statuses, ages, flags, ACR_ETwoICFilter.ALL, ME, true, rows);
  array<ACR_TwoICRow> picked = {};
  int more = ACR_TwoICCompact.PickAll(rows, ACR_TwoICCompact.SHOWN, ME, true, picked);
  string order = "";
  foreach (ACR_TwoICRow row : picked) order += string.Format("%1%2 ", row.m_iId, Pin(row));
  // 1 pinned + 3 others leave 2 of the 4 open requests (the newest): +2 more.
  bool capped = picked.Count() == ACR_TwoICCompact.ROW_CAP && more == 2 && picked[0].m_iId == 5 && picked[1].m_iId == 7 && picked[2].m_iId == 4 && picked[3].m_iId == 9 && picked[5].m_iId == 6;
  // The v2 shot's channel: a pinned line and three open requests, nothing cut.
  array<ref ACR_TwoICRow> few = {};
  for (int i = 1; i <= 4; i++) few.Insert(rows[i]);
  array<ACR_TwoICRow> fewPicked = {};
  int fewMore = ACR_TwoICCompact.PickAll(few, ACR_TwoICCompact.SHOWN, ME, true, fewPicked);
  ACR_TestRegress.Check("twoicdeclutter: All shows the pinned lines, the open requests, the newest 3 others, at most 6 rows, \"+N more\" for the rest", capped && fewPicked.Count() == 4 && fewMore == 0, string.Format("shown: %1(+%2 more); the v2 channel: %3 rows, +%4", order, more, fewPicked.Count(), fewMore));
  // One line each on All, buttons only for the one under the cursor; full blocks on Requests.
  ACR_TwoICRow coming;
  if (rows.Count() > 1) coming = rows[1];
  bool oneLine = coming && ACR_TwoICCompact.OpenRequest(coming) && ACR_TwoICCompact.OneLine(coming, ACR_ETwoICFilter.ALL, 0) && !ACR_TwoICCompact.OneLine(coming, ACR_ETwoICFilter.ALL, coming.m_iId) && !ACR_TwoICCompact.OneLine(coming, ACR_ETwoICFilter.REQUESTS, 0);
  bool others = !ACR_TwoICCompact.OpenRequest(rows[0]) && !ACR_TwoICCompact.OpenRequest(picked[0]);
  ACR_TestRegress.Check("twoicdeclutter: on All an open request is one line with its chip; its buttons show for the line under the cursor (Requests keeps full blocks)", oneLine && others, string.Format("one line %1, reports and pinned lines aren't %2", oneLine, others));
 }

 // ---------------------------------------------------------------- the card
 static void CheckCard()
 {
  array<int> numbers = {1, 11, 11, 0, 0, 15, 100, 15};
  string stats = ACR_TwoICCompact.Stats(numbers);
  array<int> pillNumbers = {1, 13, 14, 1, 1, 0, 100, 0};
  string pill = ACR_TwoICCompact.Pill(pillNumbers);
  string off = ACR_TwoICCompact.Stats(pillNumbers);
  string doing = ACR_TwoICCompact.Doing("taking Le Moule (1st Squad, 2nd Squad), 2 holding, 1 in reserve, +1 more");
  string single = ACR_TwoICCompact.Doing("nothing yet");
  // Arland's usual view (pn12f: zoom 1, widest 0.527) isn't zoomed out; its widest is, and a
  // quarter in from it; Everon's widest too; nothing without a zoom.
  bool zoom = !ACR_MapDeclutter.IsFar(1, 0.527344) && ACR_MapDeclutter.IsFar(0.527344, 0.527344) && ACR_MapDeclutter.IsFar(0.65, 0.527344) && !ACR_MapDeclutter.IsFar(0.7, 0.527344);
  zoom = zoom && ACR_MapDeclutter.IsFar(0.15, 0.15) && !ACR_MapDeclutter.IsFar(0.5, 0.15) && !ACR_MapDeclutter.IsFar(0, 0.5) && !ACR_MapDeclutter.IsFar(0.3, 0);
  bool ok = stats == "11/11 squads  ·  no FOB  ·  reserve 15/100" && pill == "2IC  ·  13/14 squads  ·  1 FOB" && off == "13/14 squads  ·  1 FOB  ·  reserve off";
  ok = ok && doing == "Taking Le Moule (1st Squad, 2nd Squad)  ·  +3 more" && single == "Nothing yet" && zoom;
  ACR_TestRegress.Check("twoicdeclutter: the card's three lines (squads · FOBs · reserve; what first · +N more) and its pill zoomed out", ok, string.Format("'%1' | '%2' | '%3' | '%4' | '%5' | zoom %6", stats, pill, off, doing, single, zoom));
 }

 // ---------------------------------------------------------------- Establish FOB
 static void CheckSites(ACR_TestHarness h)
 {
  ACR_FobSite best = new ACR_FobSite();
  best.m_iSlots = 6;
  best.m_iDefences = 7;
  best.m_iHidden = 1;
  best.m_iEyes = 1;
  best.m_fRoad = 37.4;
  best.m_fBack = 443.6;
  ACR_FobSite third = new ACR_FobSite();
  third.m_iSlots = 5;
  third.m_iDefences = 7;
  third.m_iHidden = 1;
  third.m_iEyes = 3;
  third.m_fRoad = -1;
  third.m_fBack = 392;
  third.m_bTown = true;
  array<int> facts = {};
  ACR_TwoICCompact.Facts(best, facts);
  ACR_TwoICCompact.Facts(best, facts);
  ACR_TwoICCompact.Facts(third, facts);
  string first = ACR_TwoICCompact.SiteLine(0, facts);
  string other = ACR_TwoICCompact.SiteLine(2, facts);
  string none = ACR_TwoICCompact.SiteLine(3, facts);
  string expectFirst = string.Format("Best  ·  6/%1 upgrades  ·  hidden  ·  443 m back", ACR_FobSlots.SLOT_COUNT);
  string expectOther = string.Format("5/%1 upgrades  ·  seen by 2  ·  392 m back", ACR_FobSlots.SLOT_COUNT);
  ACR_TestRegress.Check("twoicdeclutter: each numbered site has ONE short line (\"Best · 6/8 upgrades · hidden · 443 m back\")", facts.Count() == 3 * ACR_TwoICCompact.FACTS && first == expectFirst && other == expectOther && none.IsEmpty(), string.Format("'%1' | '%2' | '%3'", first, other, none));
  array<string> card = {};
  ACR_TwoICCompact.SiteCard(0, facts, card);
  array<string> card3 = {};
  ACR_TwoICCompact.SiteCard(2, facts, card3);
  bool cardOk = card.Count() == 5 && card[0] == "Site 1: why the 2IC likes it" && card[1] == string.Format("6 of %1 upgrade spots clear", ACR_FobSlots.SLOT_COUNT) && card[3] == "Hidden from 1 of 1 enemy lookout" && card[4] == "Road 37 m off  ·  443 m behind the front";
  bool card3Ok = card3.Count() == 6 && card3[3] == "Hidden from 1 of 3 enemy lookouts" && card3[4] == "No road close  ·  392 m behind the front" && card3[5] == "But in a town";
  string cardText = "";
  foreach (string line : card) cardText += line + " / ";
  foreach (string line3 : card3) cardText += line3 + " / ";
  ACR_TestRegress.Check("twoicdeclutter: a site's card: why the 2IC likes it, a line per reason", cardOk && card3Ok, cardText);
  // The cursor's one line, from the server's card: the single reason it can't, or what fits.
  bool okFits, okBlocked;
  string fits = ACR_TwoICCompact.FobHereLine("FOB HERE\nHQ fits\nUpgrade spots 6/8; Field Hospital: too steep\nDefence spots 7/7\nCan be ordered here", okFits);
  string blocked = ACR_TwoICCompact.FobHereLine("FOB HERE\nHQ fits\nUpgrade spots 2/8; Field Hospital: too steep\nDefence spots 7/7\nCan't be ordered here: Field Hospital spot too steep (the truck looks up to 50 m round for a clear spot)", okBlocked);
  string real = "";
  bool okReal;
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Squad bravo;
  if (manager && h.m_Range) bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
  if (bravo && bravo.GetAnchor()) real = ACR_TwoICCompact.FobHereLine(ACR_TwoICSuggest.FobHereText(ACR_Manager.Ground(bravo.GetAnchor().GetOrigin() + Vector(300, 0, 0))), okReal);
  bool realOk = real.IsEmpty() || (real.StartsWith("Can build here: ") && okReal) || (real.StartsWith("Can't build here: ") && !okReal && !real.Contains("\n"));
  ACR_TestRegress.Check("twoicdeclutter: the cursor's card is one line: what fits, or the single reason it can't", okFits && fits == "Can build here: 6/8 upgrades  ·  7/7 defences" && !okBlocked && blocked == "Can't build here: Field Hospital spot too steep" && realOk,
   string.Format("'%1' | '%2' | on the range: '%3'", fits, blocked, real));
 }

 // The open card keeps clear of the other sites' lines: under its own line, else above it, else
 // left of the disc; on screen.
 static void CheckPlacing()
 {
  float w = 330;
  float hgt = 200;
  float x = 800;
  float y = 400;
  array<float> xs = {x + 14, x + 14, x - 20 - w, x - 20 - w};
  array<float> ys = {y + 22, y - 22 - hgt, y + 22, y - 22 - hgt};
  // Site 2's line just under site 1's: the card goes above.
  array<float> boxes = {x - 16, y - 16, 300, 32, x + 40, y + 120, 300, 32};
  int under = ACR_TwoICCompact.PickSpot(xs, ys, w, hgt, boxes, 1920, 1080);
  // Nothing in the way: under its line, as the sketch.
  array<float> mine = {x - 16, y - 16, 300, 32};
  int free = ACR_TwoICCompact.PickSpot(xs, ys, w, hgt, mine, 1920, 1080);
  // Near the right edge, a site below: the left side.
  array<float> edgeXs = {1700, 1700, 1700 - 34 - w, 1700 - 34 - w};
  array<float> edgeBoxes = {1600, 560, 300, 32, 1600, 160, 300, 32};
  int edge = ACR_TwoICCompact.PickSpot(edgeXs, ys, w, hgt, edgeBoxes, 1920, 1080);
  ACR_TestRegress.Check("twoicdeclutter: a site's card opens beside it, never over another site's line or off screen", under == 1 && free == 0 && (edge == 2 || edge == 3), string.Format("blocked below: %1, free: %2, at the right edge: %3", under, free, edge));
 }

 // ---------------------------------------------------------------- Send truck
 static void CheckTruck()
 {
  string answer = "6/8 upgrades  ·  7/7 defences" + ACR_TwoICCompact.TRUCK_FAIL + "no construction truck for the FOB site (requisition one with its 500 supplies, then Send truck again)";
  string need = ACR_TwoICCompact.NeedText(answer);
  string reasons = ACR_TwoICCompact.Reasons(answer);
  string other = ACR_TwoICCompact.NeedText("x" + ACR_TwoICCompact.TRUCK_FAIL + "the site isn't clear any more (too steep)");
  string title = ACR_TwoICCompact.SuggestTitle("2IC suggests: a FOB behind Le Moule Sawmill");
  string way = ACR_TwoICSuggest.SentLabel(4);
  string there = ACR_TwoICSuggest.SentLabel(0);
  bool ok = need == "no construction truck for the FOB site" && reasons == "6/8 upgrades  ·  7/7 defences" && other == "can't send a truck: the site isn't clear any more" && title == "Behind Le Moule Sawmill";
  ok = ok && way == "FOB site  ·  truck on the way  ·  about 4 min" && there == "FOB site  ·  truck there, building";
  ACR_TestRegress.Check("twoicdeclutter: a pinned suggestion says what it needs; a sent truck's site reads \"FOB site · truck on the way · about 4 min\"", ok, string.Format("'%1' | '%2' | '%3' | '%4' | '%5' | '%6'", need, reasons, other, title, way, there));
 }

 // ---------------------------------------------------------------- helpers
 protected static int Pinned(array<ref ACR_TwoICRow> rows)
 {
  int count;
  foreach (ACR_TwoICRow row : rows)
   if (row.m_bPinned) count++;
  return count;
 }

 protected static string Pin(ACR_TwoICRow row)
 {
  if (row.m_bPinned) return "*";
  return "";
 }

 protected static string TextOf(ACR_TwoICRow row)
 {
  if (!row) return "-";
  return row.m_sText;
 }

 protected static int MergedOf(ACR_TwoICRow row)
 {
  if (!row) return 0;
  return row.m_iMerged;
 }

 protected static int IdOf(ACR_TwoICRow row)
 {
  if (!row) return 0;
  return row.m_iId;
 }

 protected static int LastId(array<ACR_TwoICRow> rows)
 {
  if (rows.IsEmpty()) return 0;
  return rows[rows.Count() - 1].m_iId;
 }
}
