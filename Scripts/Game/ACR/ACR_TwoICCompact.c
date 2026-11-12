// The 2IC UI's calmer look (#14 declutter, sketch boards 1-4): the logic behind it, apart from the
// widgets (ACR_TwoICView) so srv tests can check it (ACR_TestTwoICDeclutter).
// - The card: three lines ("11/11 squads · no FOB · reserve 15/100", "Taking Le Moule (1st Squad)
//   · +2 more"); a one-line pill ("2IC · 13/14 squads · 1 FOB") while zoomed out or placing a FOB.
// - The channel: a line that waits on the player (a request it can't do yet, a FOB site it has no
//   truck for) is pinned on top with "Needs you"; the 2IC's own reports are routine (dimmed), and
//   repeats merge ("2IC took on 5 squads (3:13–3:26)"); the newest 3 show (5 on hover).
// - The map tool bar's 2IC button: an orange count of the lines that need the player, while hidden.
// - Establish FOB: one short line per numbered site, its reasons in a card beside the site under the
//   cursor (never over another card), and one line for the cursor ("Can't build here: ...").
class ACR_TwoICRow : Managed
{
 int m_iId;          // the newest line's id (its buttons act on it)
 int m_iKind;        // ACR_ETwoICLine
 string m_sText;     // merged: the summed text ("Took on 5 squads")
 string m_sAnswer;
 string m_sStatus;
 int m_iAge;         // the newest line's age (s, at send time)
 int m_iFirstAge;    // the oldest merged line's age
 int m_iFlags;       // buttons + 16 x the player who asked
 int m_iMerged = 1;  // lines in it
 int m_iCount;       // "Took on": the squads summed
 bool m_bPinned;     // needs the player: on top, "Needs you"
 bool m_bRoutine;    // a report of the 2IC's own: dimmed
 int m_iOrder;       // its newest line's place in the channel (oldest 0)
}

class ACR_TwoICCompact
{
 static const int SHOWN = 3;              // channel lines shown by default
 static const int SHOWN_MORE = 5;         // on hover, or on Requests / Suggestions
 static const int PINNED_MAX = 3;
 static const int BLOCKS = 8;             // PINNED_MAX + SHOWN_MORE
 static const int ROW_CAP = 6;            // rows on All at most (pn12f v2: 5 open blocks covered a third of the map)
 static const string TRUCK_FAIL = " | Can't send a truck: "; // a suggestion's answer after a failed Send truck
 static const string TRUCK_READY = " | Truck ready: ";       // ...and once a truck can take it after all
 static const int TRUCK_NONE = 0;         // TruckState: not a failed Send truck
 static const int TRUCK_BUY = 1;          // no construction truck: "Buy a truck"
 static const int TRUCK_SENDABLE = 2;     // a truck is ready now: "Send truck" again
 static const int TRUCK_RETRY = 3;        // another reason (the site, the truck's order): "Send truck" again
 static const string NEEDS_YOU = "Needs you";
 static const int FACTS = 8;              // ints per FOB site (Facts)

 // ---------------------------------------------------------------- the channel
 // A line that waits on this player: a request the 2IC can't do yet that nobody kept queued (its
 // asker or the commander), or a suggested FOB site it found no truck for (the commander).
 static bool NeedsYou(int kind, string answer, int flags, int myId, bool commander)
 {
  int actions = flags % 16;
  if (kind == ACR_ETwoICLine.SUGGESTION) return commander && (actions & ACR_TwoICSuggest.ACT_ACCEPT) != 0 && TruckState(answer) != TRUCK_NONE;
  if (kind != ACR_ETwoICLine.REQUEST) return false;
  if (!commander && flags / 16 != myId) return false;
  return (actions & ACR_TwoICRequests.ACT_KEEP) != 0;
 }

 // The tool bar's badge: how many lines need this player.
 static int NeedsYouCount(array<int> kinds, array<string> answers, array<int> flags, int myId, bool commander)
 {
  int count;
  if (!kinds || !answers || !flags) return 0;
  foreach (int i, int kind : kinds)
  {
   if (i >= answers.Count() || i >= flags.Count()) break;
   if (NeedsYou(kind, answers[i], flags[i], myId, commander)) count++;
  }
  return count;
 }

 // A line with buttons for this player (a suggestion's are the commander's).
 static bool Actionable(int kind, int flags, int myId, bool commander)
 {
  if (flags % 16 == 0) return false;
  if (kind == ACR_ETwoICLine.SUGGESTION) return commander;
  return commander || flags / 16 == myId;
 }

 // Which reports merge when they follow each other: "" none.
 static string MergeKey(int kind, string text)
 {
  if (kind != ACR_ETwoICLine.REPORT) return "";
  if (text.StartsWith("Took on ")) return "took on";
  if (text.EndsWith(" is yours now")) return "yours";
  return "=" + text;
 }

 // "Took on Bravo": 1; "Took on 3 squads nobody leads": 3.
 static int TookOnCount(string text)
 {
  if (!text.StartsWith("Took on ")) return 0;
  string rest = text.Substring(8, text.Length() - 8);
  int n = rest.ToInt();
  if (n > 0 && rest.Contains(" squads")) return n;
  return 1;
 }

 // The channel's lines as rows: the filter's (an ACR_ETwoICFilter), oldest first, reports that
 // follow each other merged, each marked pinned (needs the player) or routine.
 static void Rows(array<int> ids, array<int> kinds, array<string> texts, array<string> answers, array<string> statuses, array<int> ages, array<int> flags, int filter, int myId, bool commander, notnull array<ref ACR_TwoICRow> rows)
 {
  rows.Clear();
  if (!ids || !kinds || !texts || !answers || !statuses || !ages || !flags) return;
  int count = ids.Count();
  if (kinds.Count() < count || texts.Count() < count || answers.Count() < count || statuses.Count() < count || ages.Count() < count || flags.Count() < count) return;
  string lastKey = "";
  for (int i = 0; i < count; i++)
  {
   int kind = kinds[i];
   if (filter == ACR_ETwoICFilter.REQUESTS && kind != ACR_ETwoICLine.REQUEST) continue;
   if (filter == ACR_ETwoICFilter.SUGGESTIONS && kind != ACR_ETwoICLine.SUGGESTION) continue;
   string key = MergeKey(kind, texts[i]);
   if (!key.IsEmpty() && key == lastKey && !rows.IsEmpty())
   {
    ACR_TwoICRow last = rows[rows.Count() - 1];
    last.m_iMerged++;
    last.m_iCount += TookOnCount(texts[i]);
    last.m_iId = ids[i];
    last.m_iAge = ages[i];
    last.m_iOrder = i;
    last.m_sText = MergedText(key, texts[i], last.m_iMerged, last.m_iCount);
    continue;
   }
   lastKey = key;
   ACR_TwoICRow row = new ACR_TwoICRow();
   row.m_iId = ids[i];
   row.m_iKind = kind;
   row.m_sText = texts[i];
   row.m_sAnswer = answers[i];
   row.m_sStatus = statuses[i];
   row.m_iAge = ages[i];
   row.m_iFirstAge = ages[i];
   row.m_iFlags = flags[i];
   row.m_iCount = TookOnCount(texts[i]);
   row.m_iOrder = i;
   row.m_bRoutine = kind == ACR_ETwoICLine.REPORT;
   row.m_bPinned = NeedsYou(kind, answers[i], flags[i], myId, commander);
   rows.Insert(row);
  }
 }

 // A merged report: "Took on 5 squads", "3 squads are yours now"; repeats of one line read as it.
 static string MergedText(string key, string newest, int lines, int squads)
 {
  if (key == "took on" && squads > 1) return string.Format("Took on %1 squads", squads);
  if (key == "yours" && lines > 1) return string.Format("%1 squads are yours now", lines);
  return newest;
 }

 // The rows to show, top to bottom: the pinned ones (newest first, at most PINNED_MAX), then the
 // newest 'shown' of the rest, newest first; a row with buttons for this player is kept ahead of
 // the routine ones, so an open suggestion doesn't scroll away.
 static void Pick(array<ref ACR_TwoICRow> rows, int shown, int myId, bool commander, notnull array<ACR_TwoICRow> picked)
 {
  picked.Clear();
  if (!rows) return;
  for (int i = rows.Count() - 1; i >= 0; i--)
  {
   if (rows[i].m_bPinned && picked.Count() < PINNED_MAX) picked.Insert(rows[i]);
  }
  int pinned = picked.Count();
  array<ACR_TwoICRow> rest = {};
  for (int a = rows.Count() - 1; a >= 0 && rest.Count() < shown; a--)
  {
   if (!rows[a].m_bPinned && Actionable(rows[a].m_iKind, rows[a].m_iFlags, myId, commander)) rest.Insert(rows[a]);
  }
  for (int b = rows.Count() - 1; b >= 0 && rest.Count() < shown; b--)
  {
   if (!rows[b].m_bPinned && !rest.Contains(rows[b])) rest.Insert(rows[b]);
  }
  // Newest first.
  for (int s = 1; s < rest.Count(); s++)
  {
   for (int t = s; t > 0 && rest[t].m_iOrder > rest[t - 1].m_iOrder; t--)
   {
    ACR_TwoICRow swap = rest[t];
    rest[t] = rest[t - 1];
    rest[t - 1] = swap;
   }
  }
  foreach (ACR_TwoICRow row : rest) picked.Insert(row);
 }

 // An open request (Coming, Supporting, Holding...): on All one line with its chip, its buttons only
 // for the line under the cursor.
 static bool OpenRequest(ACR_TwoICRow row)
 {
  return row && row.m_iKind == ACR_ETwoICLine.REQUEST && !row.m_bPinned && row.m_iFlags % 16 != 0;
 }

 // On All, a row shows as one line (no answer, no buttons): an open request, unless it is openId,
 // the one under the cursor.
 static bool OneLine(ACR_TwoICRow row, int filter, int openId)
 {
  return filter == ACR_ETwoICFilter.ALL && OpenRequest(row) && row.m_iId != openId;
 }

 // The All view, top to bottom: the pinned lines (newest first), each open request (newest first),
 // then the newest 'routine' of the rest (a row with my buttons first), at most ROW_CAP rows: open
 // requests are cut first. Returns how many pinned lines or open requests didn't fit ("+N more",
 // which opens Requests).
 static int PickAll(array<ref ACR_TwoICRow> rows, int routine, int myId, bool commander, notnull array<ACR_TwoICRow> picked)
 {
  picked.Clear();
  if (!rows) return 0;
  array<ACR_TwoICRow> pinned = {};
  array<ACR_TwoICRow> requests = {};
  array<ACR_TwoICRow> others = {};
  for (int i = rows.Count() - 1; i >= 0; i--)
  {
   ACR_TwoICRow row = rows[i];
   if (row.m_bPinned) pinned.Insert(row);
   else if (OpenRequest(row)) requests.Insert(row);
   else others.Insert(row);
  }
  array<ACR_TwoICRow> rest = {};
  foreach (ACR_TwoICRow mine : others)
  {
   if (rest.Count() < routine && Actionable(mine.m_iKind, mine.m_iFlags, myId, commander)) rest.Insert(mine);
  }
  foreach (ACR_TwoICRow other : others)
  {
   if (rest.Count() < routine && !rest.Contains(other)) rest.Insert(other);
  }
  for (int s = 1; s < rest.Count(); s++)
  {
   for (int t = s; t > 0 && rest[t].m_iOrder > rest[t - 1].m_iOrder; t--)
   {
    ACR_TwoICRow swap = rest[t];
    rest[t] = rest[t - 1];
    rest[t - 1] = swap;
   }
  }
  int pins = Math.Min(pinned.Count(), PINNED_MAX);
  int slots = Math.Max(0, ROW_CAP - pins - rest.Count());
  int reqs = Math.Min(requests.Count(), slots);
  for (int p = 0; p < pins; p++) picked.Insert(pinned[p]);
  for (int q = 0; q < reqs; q++) picked.Insert(requests[q]);
  foreach (ACR_TwoICRow shown : rest) picked.Insert(shown);
  return pinned.Count() - pins + requests.Count() - reqs;
 }

 // A routine report as the sketch reads it: "2IC took on 5 squads", "2IC: Bravo is yours now".
 static string RoutineText(string text)
 {
  array<string> verbs = {"Took", "Taking", "Attacking", "Counterattacking", "Couldn't", "Called"};
  foreach (string verb : verbs)
  {
   if (!text.StartsWith(verb + " ")) continue;
   string initial = text.Substring(0, 1);
   initial.ToLower();
   return "2IC " + initial + text.Substring(1, text.Length() - 1);
  }
  return "2IC: " + text;
 }

 // A merged row's span: " (3:13–3:26)"; "" for one line, or when both ends read the same.
 static string Range(ACR_TwoICRow row, int since, int clock)
 {
  if (!row || row.m_iMerged < 2) return "";
  string first = ACR_TwoICView.When(row.m_iFirstAge, since, clock);
  string last = ACR_TwoICView.When(row.m_iAge, since, clock);
  if (first == last) return "";
  return string.Format(" (%1–%2)", first, last);
 }

 // A suggestion's answer after a failed Send truck: TRUCK_BUY (no construction truck: the
 // player may buy one), TRUCK_SENDABLE (one is ready now), TRUCK_RETRY (another reason), else
 // TRUCK_NONE.
 static int TruckState(string answer)
 {
  if (answer.Contains(TRUCK_READY)) return TRUCK_SENDABLE;
  int at = answer.IndexOf(TRUCK_FAIL);
  if (at < 0) return TRUCK_NONE;
  string why = answer.Substring(at + TRUCK_FAIL.Length(), answer.Length() - at - TRUCK_FAIL.Length());
  if (why.StartsWith("no construction truck")) return TRUCK_BUY;
  return TRUCK_RETRY;
 }

 // The pinned no-truck line's button: "Buy a truck at Farm (21 RP)" (cost < 0: not known yet).
 static string BuyLabel(int cost, string base)
 {
  if (cost < 0) return "Buy a truck";
  if (base.IsEmpty()) return string.Format("Buy a truck (%1 RP)", cost);
  return string.Format("Buy a truck at %1 (%2 RP)", base, cost);
 }

 // A pinned suggestion's line: what it needs ("no construction truck for the FOB site"), from its
 // answer after a failed Send truck.
 static string NeedText(string answer)
 {
  if (answer.Contains(TRUCK_READY)) return "a construction truck is ready for the FOB site";
  int at = answer.IndexOf(TRUCK_FAIL);
  if (at < 0) return "";
  string why = answer.Substring(at + TRUCK_FAIL.Length(), answer.Length() - at - TRUCK_FAIL.Length());
  int paren = why.IndexOf(" (");
  if (paren > 0) why = why.Substring(0, paren);
  if (why.StartsWith("no construction truck")) return why;
  return "can't send a truck: " + why;
 }

 // The suggestion card's second line: "Behind Le Moule Sawmill" from "2IC suggests: a FOB behind
 // Le Moule Sawmill" ("" without a front).
 static string SuggestTitle(string text)
 {
  int at = text.IndexOf(" behind ");
  if (at < 0) return "";
  return "Behind " + text.Substring(at + 8, text.Length() - at - 8);
 }

 // A suggestion's reasons without a failed Send truck's note.
 static string Reasons(string answer)
 {
  int at = answer.IndexOf(TRUCK_FAIL);
  int ready = answer.IndexOf(TRUCK_READY);
  if (at < 0 || (ready >= 0 && ready < at)) at = ready;
  if (at < 0) return answer;
  return answer.Substring(0, at);
 }

 // ---------------------------------------------------------------- the card
 // numbers: ACR_TwoICCard.Pack (running, squads, of, FOBs, of, reserve, max, share %).
 static string Fobs(int fobs)
 {
  if (fobs == 1) return "1 FOB";
  if (fobs > 1) return string.Format("%1 FOBs", fobs);
  return "no FOB";
 }

 // "11/11 squads · no FOB · reserve 15/100".
 static string Stats(array<int> numbers)
 {
  if (!numbers || numbers.Count() < 8) return "";
  string squads = "stands by";
  if (numbers[0] != 0) squads = string.Format("%1/%2 squads", numbers[1], numbers[2]);
  string reserve = "reserve off";
  if (numbers[7] > 0) reserve = string.Format("reserve %1/%2", numbers[5], numbers[6]);
  return string.Format("%1  ·  %2  ·  %3", squads, Fobs(numbers[3]), reserve);
 }

 // "2IC · 13/14 squads · 1 FOB".
 static string Pill(array<int> numbers)
 {
  if (!numbers || numbers.Count() < 8) return "2IC";
  if (numbers[0] == 0) return "2IC  ·  stands by";
  return string.Format("2IC  ·  %1/%2 squads  ·  %3", numbers[1], numbers[2], Fobs(numbers[3]));
 }

 // The card's third line: what it does first, and how many more ("Taking Le Moule (1st Squad) ·
 // +2 more") from ACR_AICommander.TwoICDoing's "taking Le Moule (1st Squad), 2 holding, +1 more".
 static string Doing(string doing)
 {
  array<string> parts = {};
  Split(doing, parts);
  if (parts.IsEmpty()) return "";
  int more = 0;
  for (int i = 1; i < parts.Count(); i++)
  {
   string part = parts[i];
   if (part.StartsWith("+") && part.EndsWith(" more")) more += part.Substring(1, part.Length() - 6).ToInt();
   else more++;
  }
  string first = parts[0];
  string initial = first.Substring(0, 1);
  initial.ToUpper();
  first = initial + first.Substring(1, first.Length() - 1);
  if (more > 0) return string.Format("%1  ·  +%2 more", first, more);
  return first;
 }

 // Splits at ", " outside brackets ("attacking Harbor (Alpha, Bravo), 2 holding": two parts).
 static void Split(string text, notnull array<string> parts)
 {
  parts.Clear();
  int depth = 0;
  int start = 0;
  int length = text.Length();
  for (int i = 0; i < length; i++)
  {
   string c = text.Get(i);
   if (c == "(") depth++;
   else if (c == ")" && depth > 0) depth--;
   else if (c == "," && depth == 0 && i + 1 < length && text.Get(i + 1) == " ")
   {
    if (i > start) parts.Insert(text.Substring(start, i - start));
    start = i + 2;
    i++;
   }
  }
  if (start < length) parts.Insert(text.Substring(start, length - start));
 }

 // ---------------------------------------------------------------- FOB sites
 // A site's facts as FACTS ints: upgrade spots, defence spots, hidden, of eyes, road m (-1 none
 // close), m back, covering positions, flags (1 on the enemy's road in, 2 in a town).
 static void Facts(ACR_FobSite s, notnull array<int> facts)
 {
  facts.Insert(s.m_iSlots);
  facts.Insert(s.m_iDefences);
  facts.Insert(s.m_iHidden);
  facts.Insert(s.m_iEyes);
  int road = -1;
  if (s.m_fRoad >= 0 && s.m_fRoad <= ACR_FobSiting.ROAD_FAR) road = (int)s.m_fRoad;
  facts.Insert(road);
  facts.Insert((int)s.m_fBack);
  facts.Insert(s.m_iCover);
  int flags = 0;
  if (s.m_bOnRoute) flags |= 1;
  if (s.m_bTown) flags |= 2;
  facts.Insert(flags);
 }

 // "hidden" (no enemy lookout sees it) or "seen by 2".
 static string Seen(int hidden, int eyes)
 {
  if (eyes <= 0 || hidden >= eyes) return "hidden";
  return string.Format("seen by %1", eyes - hidden);
 }

 // "6/8 upgrades · 7/7 defences · hidden · 443 m back" (the suggestion's card), without the
 // defences on a numbered site's line.
 static string Short(int slots, int defences, int hidden, int eyes, int back, bool withDefences)
 {
  string text = string.Format("%1/%2 upgrades", slots, ACR_FobSlots.SLOT_COUNT);
  if (withDefences) text += string.Format("  ·  %1/%2 defences", defences, ACR_FobDefences.SPOT_COUNT);
  return text + string.Format("  ·  %1  ·  %2 m back", Seen(hidden, eyes), back);
 }

 // The numbered site n's one line (0 the best: "Best · ..."); "" without its facts.
 static string SiteLine(int n, array<int> facts)
 {
  int at = n * FACTS;
  if (!facts || at + FACTS > facts.Count()) return "";
  string text = Short(facts[at], facts[at + 1], facts[at + 2], facts[at + 3], facts[at + 5], false);
  if (n == 0) return "Best  ·  " + text;
  return text;
 }

 // Site n's card: "Site 2: why the 2IC likes it", then a line per reason.
 static void SiteCard(int n, array<int> facts, notnull array<string> lines)
 {
  lines.Clear();
  int at = n * FACTS;
  if (!facts || at + FACTS > facts.Count()) return;
  lines.Insert(string.Format("Site %1: why the 2IC likes it", n + 1));
  lines.Insert(string.Format("%1 of %2 upgrade spots clear", facts[at], ACR_FobSlots.SLOT_COUNT));
  lines.Insert(string.Format("%1 of %2 defence spots clear", facts[at + 1], ACR_FobDefences.SPOT_COUNT));
  int hidden = facts[at + 2];
  int eyes = facts[at + 3];
  string lookouts = "lookouts";
  if (eyes == 1) lookouts = "lookout";
  if (eyes <= 0) lines.Insert("Out of the enemy's sight");
  else lines.Insert(string.Format("Hidden from %1 of %2 enemy %3", hidden, eyes, lookouts));
  if (facts[at + 4] >= 0) lines.Insert(string.Format("Road %1 m off  ·  %2 m behind the front", facts[at + 4], facts[at + 5]));
  else lines.Insert(string.Format("No road close  ·  %1 m behind the front", facts[at + 5]));
  if (facts[at + 6] > 0) lines.Insert(string.Format("Covered by %1 of our positions", facts[at + 6]));
  if ((facts[at + 7] & 1) != 0) lines.Insert("But on the enemy's road in");
  if ((facts[at + 7] & 2) != 0) lines.Insert("But in a town");
 }

 // The cursor's one line, from the server's FOB here card (ACR_TwoICSuggest.FobHereText): the
 // single reason it can't, or what fits.
 static string FobHereLine(string card, out bool ok)
 {
  ok = false;
  array<string> lines = {};
  card.Split("\n", lines, true);
  string upgrades = "";
  string defences = "";
  foreach (string line : lines)
  {
   if (line.StartsWith("Can't be ordered here: "))
   {
    string why = line.Substring(23, line.Length() - 23);
    int paren = why.IndexOf(" (the truck");
    if (paren > 0) why = why.Substring(0, paren);
    return "Can't build here: " + why;
   }
   if (line.StartsWith("Upgrade spots ")) upgrades = Spots(line, 14);
   if (line.StartsWith("Defence spots ")) defences = Spots(line, 14);
  }
  if (lines.IsEmpty()) return "";
  ok = true;
  return string.Format("Can build here: %1 upgrades  ·  %2 defences", upgrades, defences);
 }

 // "6/8" from "Upgrade spots 6/8; Field Hospital: too steep".
 protected static string Spots(string line, int from)
 {
  string rest = line.Substring(from, line.Length() - from);
  int semi = rest.IndexOf(";");
  if (semi >= 0) rest = rest.Substring(0, semi);
  return rest;
 }

 // ---------------------------------------------------------------- placing a card
 // The first spot (x, y: its top left) where a w x h card stays on screen (8 px in) and overlaps
 // none of the boxes (x, y, w, h each); else the one that overlaps them least.
 static int PickSpot(array<float> xs, array<float> ys, float w, float h, array<float> boxes, float screenW, float screenH)
 {
  int best = 0;
  float bestOverlap = float.MAX;
  foreach (int i, float x : xs)
  {
   if (i >= ys.Count()) break;
   float y = ys[i];
   float overlap = 0;
   bool off = x < 8 || y < 8 || x + w > screenW - 8 || y + h > screenH - 8;
   if (off) overlap += w * h;
   if (boxes)
   {
    for (int b = 0; b + 3 < boxes.Count(); b += 4) overlap += Overlap(x, y, w, h, boxes[b], boxes[b + 1], boxes[b + 2], boxes[b + 3]);
   }
   if (overlap <= 0) return i;
   if (overlap < bestOverlap)
   {
    bestOverlap = overlap;
    best = i;
   }
  }
  return best;
 }

 static float Overlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh)
 {
  float w = Math.Min(ax + aw, bx + bw) - Math.Max(ax, bx);
  float h = Math.Min(ay + ah, by + bh) - Math.Max(ay, by);
  if (w <= 0 || h <= 0) return 0;
  return w * h;
 }
}
