// The calm command map (declutter part B, from the sketch agreed in #14): ACA's own map UI made
// quieter. The normal look; the host option -acrMapDeclutter 0 (setup row "Calm command map")
// brings back the classic map exactly as it was. Here: the option, the player's own choices (dark
// map, UI size) and the rules, as pure functions the srv test checks (ACR_TestMapDeclutter). What
// draws it is ACR_MapDeclutterView.c.
//  - INTEL merges repeats ("Bought Grenadier Team ×2, MG Team ×2 (-39 RP)"), shows the last 3 lines
//    and "+N older"; zoomed out it is one line ("INTEL  Le Moule under attack · 2 new").
//  - Bases: a dot in the owner's colour (blue ours, red enemy, yellow free), size pips after the name
//    instead of "[CP II]", a red ring while contested. Zoomed out only our bases, the front, the enemy
//    HQ and bases under attack keep their names; the rest are plain dots.
//  - A squad's marker stands where the squad is, or isn't on the map (JT's playtest: markers stacked
//    round a town's dot read as real positions, and zoomed in they were somewhere else). Zoomed out
//    until a base's ring is too small to tell who stands where in it, or with an icon that would
//    touch the dot, the unit condenses into the base: a count beside the dot ("×4 units · ×2 enemy"),
//    and a click on it or the base lists them. A corner mark says moving, fighting or holding (legend
//    bottom right). Zoomed out, order lines only for the selected squads.
enum ACR_ESquadMark
{
 HOLDING,
 MOVING,
 FIGHTING
}

enum ACR_EBaseSide
{
 FREE,
 OWN,
 ENEMY,
 OTHER // a third party (FIA pockets)
}

class ACR_MapDeclutter
{
 static const string OPTION = "acrMapDeclutter";
 static const string PREFS = "$profile:ACA_MapStyle.txt";
 static const int INTEL_SHOWN = 3;          // INTEL lines shown; the rest are "+N older"
 static const float MERGE_SECONDS = 120;    // feed lines this close together may merge
 static const float FAR_ZOOM = 1.25;        // zoomed out: at most this times the map's widest zoom
 static const float CLUSTER_MIN_RADIUS = 100; // m: a squad this close to a base (or in its ring) is "at" it
 static const float SMALL_RING = 44;        // reference px: a ring smaller than this on screen can't show who stands where in it
 static const float ICON_W = 30;            // reference px: a squad's icon, for what it would touch
 static const float ICON_H = 22;
 static const float DOT_REACH = 10;         // reference px: a base's dot and its white edge
 static const float PILE_REACH = 12;        // reference px: icons this close stand as one, with "×N"
 static const float ARRIVED = 40;           // m from its order's target: holding, not moving
 static const float FIGHT_RANGE = 200;      // m: a fresh contact this close: fighting
 static const int FIGHT_AGE = 30;           // s: contacts older than this don't count
 static const int HEADLINE_CHARS = 46;
 static const float ROUND_MARKER = 14;      // reference px: the radius of a soldier's or vehicle's round marker

 // ---------------------------------------------------------------- the option and the player's choices
 // Server: the host's choice (on unless set to 0).
 static bool OptionOn()
 {
  return ACR_LogisticsRules.Choice(OPTION, "1") != "0";
 }

 // Client: what the server last said (ACR_MapFeed sends it with the map picture). The setup
 // screen's layout preview stays classic: the host edits every base there, by name.
 protected static bool s_bOn = true;
 static bool On() { return s_bOn && !ACR_SetupClient.s_bActive; }
 static void SetOn(bool on) { s_bOn = on; }

 // The player's own: the dark map (off by default) and the UI size (compact by default), kept in
 // his profile between games.
 protected static bool s_bDark;
 protected static bool s_bCompact = true;
 protected static bool s_bPrefsRead;

 static bool Dark()
 {
  ReadPrefs();
  return s_bDark;
 }

 static bool Compact()
 {
  ReadPrefs();
  return s_bCompact;
 }

 static void SetDark(bool dark)
 {
  ReadPrefs();
  s_bDark = dark;
  SavePrefs();
 }

 static void SetCompact(bool compact)
 {
  ReadPrefs();
  s_bCompact = compact;
  SavePrefs();
 }

 protected static void ReadPrefs()
 {
  if (s_bPrefsRead) return;
  s_bPrefsRead = true;
  if (!FileIO.FileExists(PREFS)) return;
  FileHandle file = FileIO.OpenFile(PREFS, FileMode.READ);
  if (!file) return;
  string line;
  while (file.ReadLine(line) >= 0)
  {
   line = line.Trim();
   if (line == "dark=1") s_bDark = true;
   if (line == "dark=0") s_bDark = false;
   if (line == "size=normal") s_bCompact = false;
   if (line == "size=compact") s_bCompact = true;
  }
  file.Close();
 }

 protected static void SavePrefs()
 {
  FileHandle file = FileIO.OpenFile(PREFS, FileMode.WRITE);
  if (!file) return;
  if (s_bDark) file.WriteLine("dark=1");
  else file.WriteLine("dark=0");
  if (s_bCompact) file.WriteLine("size=compact");
  else file.WriteLine("size=normal");
  file.Close();
 }

 // Tests: the defaults again, without the profile file.
 static void ResetForTest()
 {
  s_bOn = true;
  s_bDark = false;
  s_bCompact = true;
  s_bPrefsRead = true;
 }

 // ---------------------------------------------------------------- colours
 // An sRGB colour (the sketch's hex) as widgets and the map canvas want it: linear.
 static Color Rgb(int r, int g, int b, float a = 1)
 {
  return new Color(Linear(r), Linear(g), Linear(b), a);
 }

 static float Linear(int channel)
 {
  float c = channel / 255.0;
  return Math.Pow(c, 2.2);
 }

 static Color PanelColor() { return Rgb(24, 27, 30, 0.96); }
 static Color AccentBlue() { return Rgb(90, 166, 240); }   // #5AA6F0: headers
 static Color TextColor() { return Rgb(226, 228, 230); }
 static Color GreyText() { return Rgb(150, 156, 162); }
 static Color ButtonColor() { return Rgb(36, 50, 66); }
 static Color ButtonHover() { return Rgb(52, 72, 96); }
 static Color ButtonBorder() { return Rgb(70, 86, 104); }
 // The dark map's tint: a near-black blue over the map (linear colour; tweak the strength here).
 static const float DARK_TINT_ALPHA = 0.82;
 static Color DarkTint() { return Rgb(8, 12, 24, DARK_TINT_ALPHA); }

 // ---------------------------------------------------------------- bases
 // Whose a base is, as this player sees it.
 static int SideOf(string owner, string mine)
 {
  if (owner.IsEmpty()) return ACR_EBaseSide.FREE;
  if (!mine.IsEmpty() && owner == mine) return ACR_EBaseSide.OWN;
  if (owner == ACR_Pockets.FACTION) return ACR_EBaseSide.OTHER;
  return ACR_EBaseSide.ENEMY;
 }

 // Blue ours, red enemy, yellow free (green: FIA, a third party).
 static Color SideColor(int side, float alpha = 1)
 {
  if (side == ACR_EBaseSide.OWN) return Rgb(47, 128, 237, alpha);
  if (side == ACR_EBaseSide.ENEMY) return Rgb(229, 62, 62, alpha);
  if (side == ACR_EBaseSide.OTHER) return Rgb(72, 168, 96, alpha);
  return Rgb(242, 201, 76, alpha);
 }

 // The base's size as pips (1-3, its income), from the label's " [N]" or, failing that, the
 // detail's "+N RP"; 0 for none.
 static int Pips(string label, string detail)
 {
  int open = label.LastIndexOf(" [");
  if (open >= 0 && label.EndsWith("]"))
  {
   string value = label.Substring(open + 2, label.Length() - open - 3);
   int n = value.ToInt();
   if (n > 0) return Math.Min(n, 3);
  }
  if (detail.Contains("+3 RP")) return 3;
  if (detail.Contains("+2 RP")) return 2;
  if (detail.Contains("+1 RP")) return 1;
  return 0;
 }

 // An HQ: the server adds an " | HQ" part to its detail while the calm map is on.
 static bool IsHQ(string detail)
 {
  // Its own " | HQ" part, not " | HQ DESTROYED: ..." (a FOB's lost HQ).
  return detail == "HQ" || detail.EndsWith(" | HQ") || detail.Contains(" | HQ | ");
 }

 static bool IsContested(string detail)
 {
  return detail.Contains("CONTESTED");
 }

 // The name alone: "Le Moule [2]" -> "Le Moule".
 static string ShortName(string label)
 {
  int open = label.LastIndexOf(" [");
  if (open > 0 && label.EndsWith("]")) return label.Substring(0, open);
  return label;
 }

 // The caption under a base's dot: its name, "· HQ" / "· enemy HQ" for HQs, "" for a plain dot.
 static string Caption(string label, string detail, int side, bool keepName)
 {
  if (!keepName) return "";
  string name = ShortName(label);
  if (IsHQ(detail))
  {
   if (side == ACR_EBaseSide.ENEMY) return name + " · enemy HQ";
   return name + " · HQ";
  }
  return name;
 }

 // Zoomed out: really far out, the map at most FAR_ZOOM times its widest zoom (minZoom, its
 // pixels per metre when the whole map fits). The one rule for the calm map's far view.
 static bool IsFar(float zoom, float minZoom)
 {
  return minZoom > 0 && zoom > 0 && zoom <= minZoom * FAR_ZOOM;
 }

 // The front: for each of our bases, the nearest base that isn't ours.
 static void FrontBases(array<vector> positions, array<int> sides, notnull array<bool> front)
 {
  front.Clear();
  foreach (int f, vector unused : positions) front.Insert(false);
  foreach (int i, vector own : positions)
  {
   if (i >= sides.Count() || sides[i] != ACR_EBaseSide.OWN) continue;
   int best = -1;
   float bestDistance;
   foreach (int j, vector other : positions)
   {
    if (j >= sides.Count() || sides[j] == ACR_EBaseSide.OWN) continue;
    float d = vector.DistanceXZ(own, other);
    if (best >= 0 && d >= bestDistance) continue;
    best = j;
    bestDistance = d;
   }
   if (best >= 0) front[best] = true;
  }
 }

 // Whether a base keeps its name: always when near; zoomed out only ours, the front, the enemy HQ,
 // a base under attack and the selected one.
 static bool KeepsName(bool far, int side, bool hq, bool contested, bool front, bool selected)
 {
  if (!far || selected || contested || front) return true;
  if (side == ACR_EBaseSide.OWN) return true;
  return hq && side == ACR_EBaseSide.ENEMY;
 }

 // The dark map's place names (ACR_PlayerMapTheme): the base within 'within' m of a place (the
 // nearest), or -1.
 static int NearestBase(vector pos, notnull array<vector> bases, float within)
 {
  int nearest = -1;
  float best = within;
  foreach (int b, vector at : bases)
  {
   float d = vector.DistanceXZ(at, pos);
   if (d >= best) continue;
   best = d;
   nearest = b;
  }
  return nearest;
 }

 // Whether base b's name is drawn: its rectangle in the layout's names (4 per base, x1 <= x0: none).
 static bool NameDrawn(notnull array<float> names, int b)
 {
  if (b < 0 || b * 4 + 2 >= names.Count()) return false;
  return names[b * 4 + 2] > names[b * 4];
 }

 // A place's own name shows on the dark map unless its base's name stands for it right now. Zoomed
 // out only some bases are named and the rest are bare dots, so their places keep their names (#203).
 static bool PlaceNameShows(int base, notnull array<float> names)
 {
  return !NameDrawn(names, base);
 }

 // ---------------------------------------------------------------- squads
 // A base's ring as far as "at it" goes (m).
 static float RingOf(int b, array<float> baseRadii)
 {
  if (b < 0 || b >= baseRadii.Count()) return CLUSTER_MIN_RADIUS;
  return Math.Max(CLUSTER_MIN_RADIUS, baseRadii[b]);
 }

 // The base a squad is at on the ground (in its ring, the nearest), or -1.
 static int BaseAt(vector pos, array<vector> basePos, array<float> baseRadii)
 {
  int at = -1;
  float atDistance;
  foreach (int b, vector base : basePos)
  {
   float d = vector.DistanceXZ(pos, base);
   if (d > RingOf(b, baseRadii) || (at >= 0 && d >= atDistance)) continue;
   at = b;
   atDistance = d;
  }
  return at;
 }

 // The base whose count a marker goes into at this zoom (the nearest), or -1: it stands where it is.
 // x, y: where it really is on screen; ox, oy: its icon's centre from there (a map symbol is drawn
 // above its point); w x h its icon; dots: x, y per base; rings: each base's ring on screen (px). It
 // goes in when its icon would touch the dot, or it is in a ring smaller than smallRing. Nothing else
 // moves it, so a marker on the map is always where its unit is.
 static int CondensesInto(float x, float y, float ox, float oy, float w, float h, array<float> dots, float dotReach, array<float> rings, float smallRing)
 {
  float cx = x + ox;
  float cy = y + oy;
  int into = -1;
  float bestDistance;
  for (int b = 0; b * 2 + 1 < dots.Count(); b++)
  {
   float dx = dots[b * 2] - x;
   float dy = dots[b * 2 + 1] - y;
   float d = Math.Sqrt(dx * dx + dy * dy);
   bool touches = Overlaps(cx - w * 0.5, cy - h * 0.5, cx + w * 0.5, cy + h * 0.5, dots[b * 2] - dotReach, dots[b * 2 + 1] - dotReach, dots[b * 2] + dotReach, dots[b * 2 + 1] + dotReach);
   bool inSmallRing = b < rings.Count() && rings[b] < smallRing && d <= rings[b];
   if ((!touches && !inSmallRing) || (into >= 0 && d >= bestDistance)) continue;
   into = b;
   bestDistance = d;
  }
  return into;
 }

 // Squads standing on top of each other (within reach px of the first) are one icon with "×N", at
 // the place of the one shown (the selected one if any, else the first). centres: x, y per group;
 // pileOf[g] the group shown for it (-1: g isn't standing); count[g] the squads in its pile; rep[g]
 // whether g's icon shows.
 static void Piles(array<float> centres, array<bool> standing, array<bool> selected, float reach, notnull array<int> pileOf, notnull array<int> count, notnull array<bool> rep)
 {
  pileOf.Clear();
  count.Clear();
  rep.Clear();
  int groups = centres.Count() / 2;
  for (int i = 0; i < groups; i++)
  {
   pileOf.Insert(-1);
   count.Insert(1);
   rep.Insert(true);
  }
  for (int g = 0; g < groups; g++)
  {
   if (pileOf[g] >= 0 || g >= standing.Count() || !standing[g]) continue;
   array<int> members = {g};
   for (int k = g + 1; k < groups; k++)
   {
    if (pileOf[k] >= 0 || k >= standing.Count() || !standing[k]) continue;
    float dx = centres[k * 2] - centres[g * 2];
    float dy = centres[k * 2 + 1] - centres[g * 2 + 1];
    if (dx * dx + dy * dy <= reach * reach) members.Insert(k);
   }
   int shown = -1;
   foreach (int m : members)
   {
    if (shown < 0 && m < selected.Count() && selected[m]) shown = m;
   }
   if (shown < 0) shown = g;
   foreach (int member : members)
   {
    pileOf[member] = shown;
    count[member] = members.Count();
    rep[member] = member == shown;
   }
  }
 }

 // A base's count: "×4 units", "×1 unit", "×2 enemy", "×4 units · ×2 enemy" ("" for none). The enemy
 // part in enemyColor when given.
 static string ChipText(int own, int enemy, Color enemyColor = null)
 {
  string text = "";
  if (own == 1) text = "×1 unit";
  else if (own > 1) text = string.Format("×%1 units", own);
  if (enemy <= 0) return text;
  string foes = string.Format("×%1 enemy", enemy);
  if (enemyColor) foes = ACR_TwoICStyle.Colored(foes, enemyColor);
  if (text.IsEmpty()) return foes;
  return text + " · " + foes;
 }

 // The list's title: "4 UNITS AT PUSTA", "1 UNIT + 2 ENEMY AT PUSTA", "2 ENEMY AT PUSTA".
 static string ListTitle(int own, int enemy, string place)
 {
  string text = "";
  if (own == 1) text = "1 UNIT";
  else if (own > 1) text = string.Format("%1 UNITS", own);
  if (enemy > 0)
  {
   if (!text.IsEmpty()) text += " + ";
   text += string.Format("%1 ENEMY", enemy);
  }
  // ToUpper changes the string itself and returns its length.
  place.ToUpper();
  return text + " AT " + place;
 }

 // A unit counted at a base from outside its ring (zoomed out, it stands too close to the dot to
 // draw): how far out, to the nearest 10 m ("420 m out"); "" in the ring.
 static string OutOfRing(float distance, float ring)
 {
  if (distance <= ring) return "";
  int rounded = (distance + 5) / 10;
  return string.Format("%1 m out", rounded * 10);
 }

 // The corner mark: fighting (a fresh contact close by), moving (an order not reached yet) or
 // holding. toTarget and nearestContact < 0: none.
 static int SquadMark(bool hasOrder, float toTarget, float nearestContact)
 {
  if (nearestContact >= 0 && nearestContact <= FIGHT_RANGE) return ACR_ESquadMark.FIGHTING;
  if (hasOrder && toTarget > ARRIVED) return ACR_ESquadMark.MOVING;
  return ACR_ESquadMark.HOLDING;
 }

 static string MarkName(int mark)
 {
  if (mark == ACR_ESquadMark.MOVING) return "moving";
  if (mark == ACR_ESquadMark.FIGHTING) return "fighting";
  return "holding";
 }

 // ---------------------------------------------------------------- INTEL
 // A purchase line ("Yung requisitioned MG Team at Lamentin (-20 RP)", or without the tag): who,
 // what, and the RP.
 static bool ParsePurchase(string line, out string tag, out string unit, out int rp)
 {
  tag = "";
  unit = "";
  rp = 0;
  int cost = line.LastIndexOf(" (-");
  if (cost < 0 || !line.EndsWith(" RP)")) return false;
  string number = line.Substring(cost + 3, line.Length() - cost - 7);
  rp = number.ToInt();
  string body = line.Substring(0, cost);
  int verb = body.IndexOf("requisitioned ");
  if (verb < 0) return false;
  if (verb > 0) tag = body.Substring(0, verb).Trim();
  string rest = body.Substring(verb + 14, body.Length() - verb - 14);
  int at = rest.LastIndexOf(" at ");
  if (at > 0) rest = rest.Substring(0, at);
  unit = rest.Trim();
  return !unit.IsEmpty();
 }

 // The feed (newest first) as shown: a run of one commander's purchases becomes one line ("Bought
 // Grenadier Team ×2, MG Team ×2 (-39 RP)"; "Popp bought ..." for another commander's), a line
 // repeated straight after itself becomes "line ×N"; only lines within MERGE_SECONDS of the run's
 // newest merge. texts and entryAges: the shown lines, newest first, and their newest line's age.
 // spots (as lines) and entrySpots (as texts): where a line happened; a shown line takes its run's
 // newest line's (vector.Zero: nowhere).
 static void MergeFeed(array<string> lines, array<int> ages, string myTag, notnull array<string> texts, notnull array<int> entryAges, array<vector> spots = null, array<vector> entrySpots = null)
 {
  texts.Clear();
  entryAges.Clear();
  if (entrySpots) entrySpots.Clear();
  vector runSpot;
  // The open run: a purchase run (tag, units and counts, RP) or a repeated line (count).
  bool buying;
  string runTag;
  string runLine;
  int runAge;
  int runCount;
  int runRp;
  array<string> units = {};
  array<int> unitCounts = {};
  foreach (int i, string line : lines)
  {
   int age = 0;
   if (i < ages.Count()) age = ages[i];
   string tag, unit;
   int rp;
   bool purchase = ParsePurchase(line, tag, unit, rp);
   bool close = runCount > 0 && age - runAge <= MERGE_SECONDS;
   if (close && purchase && buying && tag == runTag)
   {
    int known = units.Find(unit);
    if (known >= 0) unitCounts[known] = unitCounts[known] + 1;
    else
    {
     units.Insert(unit);
     unitCounts.Insert(1);
    }
    runRp += rp;
    runCount++;
    continue;
   }
   if (close && !purchase && !buying && line == runLine)
   {
    runCount++;
    continue;
   }
   if (runCount > 0)
   {
    Close(texts, entryAges, buying, runTag, myTag, runLine, runAge, runCount, runRp, units, unitCounts);
    if (entrySpots) entrySpots.Insert(runSpot);
   }
   runSpot = vector.Zero;
   if (spots && i < spots.Count()) runSpot = spots[i];
   buying = purchase;
   runTag = tag;
   runLine = line;
   runAge = age;
   runCount = 1;
   runRp = rp;
   units.Clear();
   unitCounts.Clear();
   if (purchase)
   {
    units.Insert(unit);
    unitCounts.Insert(1);
   }
  }
  if (runCount > 0)
  {
   Close(texts, entryAges, buying, runTag, myTag, runLine, runAge, runCount, runRp, units, unitCounts);
   if (entrySpots) entrySpots.Insert(runSpot);
  }
 }

 protected static void Close(array<string> texts, array<int> entryAges, bool buying, string tag, string myTag, string line, int age, int count, int rp, array<string> units, array<int> unitCounts)
 {
  string text = line;
  if (buying)
  {
   text = "Bought ";
   if (!tag.IsEmpty() && tag != myTag) text = tag + " bought ";
   foreach (int u, string unit : units)
   {
    if (u > 0) text += ", ";
    text += unit;
    if (unitCounts[u] > 1) text += " ×" + unitCounts[u].ToString();
   }
   text += string.Format(" (-%1 RP)", rp);
  }
  else if (count > 1) text = line + " ×" + count.ToString();
  texts.Insert(text);
  entryAges.Insert(age);
 }

 // "+N older": the lines past the INTEL_SHOWN shown.
 static int Older(int entries)
 {
  return Math.Max(0, entries - INTEL_SHOWN);
 }

 // Lines newer than the last look at the full INTEL (seenAgo s ago).
 static int NewCount(array<int> entryAges, int seenAgo)
 {
  int count;
  foreach (int age : entryAges) if (age < seenAgo) count++;
  return count;
 }

 // The one-line INTEL bar's text: our base under attack first, else the newest line, cut short.
 static string Headline(string attacked, array<string> texts)
 {
  string text = "";
  if (!attacked.IsEmpty()) text = attacked + " under attack";
  else if (texts && !texts.IsEmpty()) text = texts[0];
  else return "No contacts reported";
  if (text.Length() <= HEADLINE_CHARS) return text;
  // Cut at a space (never inside a multi-byte letter such as "×").
  string cut = text.Substring(0, HEADLINE_CHARS - 3);
  int space = cut.LastIndexOf(" ");
  if (space > 10) cut = cut.Substring(0, space);
  return cut + "...";
 }

 // ---------------------------------------------------------------- placement
 // Screen rectangles are 4 floats: x0, y0, x1, y1 (px). A list of them is a flat array.
 static bool Overlaps(float ax0, float ay0, float ax1, float ay1, float bx0, float by0, float bx1, float by1)
 {
  return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
 }

 // The first rectangle of the list the box hits, or -1.
 static int FirstHit(float x0, float y0, float x1, float y1, array<float> rects)
 {
  for (int r = 0; r + 3 < rects.Count(); r += 4)
   if (Overlaps(x0, y0, x1, y1, rects[r], rects[r + 1], rects[r + 2], rects[r + 3])) return r / 4;
  return -1;
 }

 // Base names keep their spot unless they would cover another base's name or dot, or what must stay
 // where it is (extra: rectangles, a squad's icon, a base's count): then the name goes above its
 // dot, to its right, to its left, or further down, whichever is clear first (the most important
 // bases, first in order, choose first). dots: x, y per base; captions: each base's caption rectangle
 // relative to its dot (x1 <= x0: no caption); shift: dx, dy per base.
 static void PlaceNames(array<float> dots, array<float> captions, array<int> order, float dotRadius, float gap, notnull array<float> shift, array<float> extra = null)
 {
  shift.Clear();
  int count = dots.Count() / 2;
  for (int i = 0; i < count; i++)
  {
   shift.Insert(0);
   shift.Insert(0);
  }
  // Every dot is an obstacle; placed names are added as they are placed.
  array<float> taken = {};
  for (int d = 0; d < count; d++)
  {
   taken.Insert(dots[d * 2] - dotRadius);
   taken.Insert(dots[d * 2 + 1] - dotRadius);
   taken.Insert(dots[d * 2] + dotRadius);
   taken.Insert(dots[d * 2 + 1] + dotRadius);
  }
  if (extra) foreach (float edge : extra) taken.Insert(edge);
  foreach (int b : order)
  {
   if (b < 0 || b >= count || b * 4 + 3 >= captions.Count()) continue;
   float x0 = captions[b * 4];
   float y0 = captions[b * 4 + 1];
   float x1 = captions[b * 4 + 2];
   float y1 = captions[b * 4 + 3];
   if (x1 <= x0 || y1 <= y0) continue;
   float h = y1 - y0;
   float mid = -(y0 + y1) * 0.5;
   array<float> moves = {0, 0, 0, -(y0 + y1), dotRadius + gap - x0, mid, -(dotRadius + gap) - x1, mid, 0, h + gap, 0, 2 * (h + gap)};
   float cx = dots[b * 2];
   float cy = dots[b * 2 + 1];
   int pick = 0;
   for (int m = 0; m + 1 < moves.Count(); m += 2)
   {
    // Its own dot doesn't count against it.
    float ox0 = cx + x0 + moves[m];
    float oy0 = cy + y0 + moves[m + 1];
    float ox1 = cx + x1 + moves[m];
    float oy1 = cy + y1 + moves[m + 1];
    bool clear = true;
    for (int r = 0; r + 3 < taken.Count(); r += 4)
    {
     if (r / 4 == b) continue;
     if (Overlaps(ox0, oy0, ox1, oy1, taken[r], taken[r + 1], taken[r + 2], taken[r + 3])) { clear = false; break; }
    }
    if (!clear) continue;
    pick = m;
    break;
   }
   shift[b * 2] = moves[pick];
   shift[b * 2 + 1] = moves[pick + 1];
   taken.Insert(cx + x0 + moves[pick]);
   taken.Insert(cy + y0 + moves[pick + 1]);
   taken.Insert(cx + x1 + moves[pick]);
   taken.Insert(cy + y1 + moves[pick + 1]);
  }
 }

 // Where a base's count goes (w x h, centred on the answer): touching its dot at x, y, to the right,
 // else to the left, above, at a corner or below (where its base's name is), whichever hits no dot,
 // none of others (the icons and counts: rectangles, they can't move) and not its base's name; failing
 // that it may cover the name, which then moves; failing that, an icon; to the right if every spot
 // is on a dot.
 static vector PlaceChip(float x, float y, float w, float h, float dotReach, float gap, array<float> dots, array<float> others, array<float> name)
 {
  float rx = dotReach + gap + w * 0.5;
  float ry = dotReach + gap + h * 0.5;
  array<float> spots = {x + rx, y, x - rx, y, x, y - ry, x + rx, y - ry, x - rx, y - ry, x + rx, y + ry, x - rx, y + ry, x, y + ry};
  for (int pass = 0; pass < 3; pass++)
  {
   for (int s = 0; s + 1 < spots.Count(); s += 2)
   {
    float x0 = spots[s] - w * 0.5;
    float y0 = spots[s + 1] - h * 0.5;
    float x1 = spots[s] + w * 0.5;
    float y1 = spots[s + 1] + h * 0.5;
    if (FirstHit(x0, y0, x1, y1, dots) >= 0) continue;
    if (pass < 2 && FirstHit(x0, y0, x1, y1, others) >= 0) continue;
    if (pass < 1 && FirstHit(x0, y0, x1, y1, name) >= 0) continue;
    return Vector(spots[s], spots[s + 1], 0);
   }
  }
  return Vector(spots[0], spots[1], 0);
 }

 // Where a box goes (w x h, its top left corner the answer): beside what it belongs to (from x0 to
 // x1, at y), to the right and lifted a little, else to its left; on screen, and clear of the
 // rectangles (the panels), going beside or above or below one when it has to. In unscaled px, as the
 // widgets are placed.
 static vector PlaceBox(float x0, float x1, float y, float w, float h, float side, float lift, float screenW, float screenH, float margin, array<float> keepClear)
 {
  float x = x1;
  float maxX = Math.Max(margin, screenW - w - margin);
  float maxY = Math.Max(margin, screenH - h - margin);
  float top = Math.Clamp(y - lift, margin, maxY);
  array<float> tries = {x1 + side, top, x0 - side - w, top};
  for (int r = 0; r + 3 < keepClear.Count(); r += 4)
  {
   tries.Insert(keepClear[r] - margin - w);
   tries.Insert(top);
   tries.Insert(keepClear[r + 2] + margin);
   tries.Insert(top);
   tries.Insert(Math.Clamp(x + side, margin, maxX));
   tries.Insert(keepClear[r + 3] + margin);
   tries.Insert(Math.Clamp(x + side, margin, maxX));
   tries.Insert(keepClear[r + 1] - margin - h);
  }
  for (int t = 0; t + 1 < tries.Count(); t += 2)
  {
   float px = tries[t];
   float py = tries[t + 1];
   if (px < margin || px > maxX || py < margin || py > maxY) continue;
   if (FirstHit(px, py, px + w, py + h, keepClear) < 0) return Vector(px, py, 0);
  }
  return Vector(Math.Clamp(x + side, margin, maxX), top, 0);
 }

 // A round marker (radius r px, centred at x, y) touching a base's name or dot.
 static bool OnAName(float x, float y, float r, array<float> obstacles)
 {
  return FirstHit(x - r, y - r, x + r, y + r, obstacles) >= 0;
 }

 // An order's end near a base (within reach px of its dot, or on its name) ends at that base: one
 // mark for all the orders going there, none on the name. names: each base's name rectangle
 // (x1 <= x0: none), or empty. The base, or -1.
 static int SnapToBase(float x, float y, array<float> dots, float reach, array<float> names)
 {
  if (names)
  {
   for (int n = 0; n + 3 < names.Count(); n += 4)
    if (names[n + 2] > names[n] && x >= names[n] && x <= names[n + 2] && y >= names[n + 1] && y <= names[n + 3]) return n / 4;
  }
  int best = -1;
  float bestDistance = reach;
  for (int b = 0; b + 1 < dots.Count(); b += 2)
  {
   float dx = dots[b] - x;
   float dy = dots[b + 1] - y;
   float d = Math.Sqrt(dx * dx + dy * dy);
   if (d > bestDistance) continue;
   bestDistance = d;
   best = b / 2;
  }
  return best;
 }

 // ---------------------------------------------------------------- canvas shapes
 // A filled shape the game can triangulate and that shows on a width x height canvas (0: unknown):
 // three points or more, some area, finite, not wholly off it. The calm map's dots and marks go
 // through this; once map-polygon-guard (#62) is in, ACR_OrderDisplay.AddPolygon does the same.
 static bool ShapeOk(array<float> vertices, float width, float height)
 {
  if (!vertices || vertices.Count() < 6 || vertices.Count() % 2 != 0) return false;
  float minX = vertices[0];
  float maxX = minX;
  float minY = vertices[1];
  float maxY = minY;
  float twiceArea = 0;
  int points = vertices.Count() / 2;
  for (int p = 0; p < points; p++)
  {
   float x = vertices[p * 2];
   float y = vertices[p * 2 + 1];
   if (!(Math.AbsFloat(x) < 1000000) || !(Math.AbsFloat(y) < 1000000)) return false;
   minX = Math.Min(minX, x);
   maxX = Math.Max(maxX, x);
   minY = Math.Min(minY, y);
   maxY = Math.Max(maxY, y);
   int q = (p + 1) % points;
   twiceArea += (x - vertices[0]) * (vertices[q * 2 + 1] - vertices[1]) - (vertices[q * 2] - vertices[0]) * (y - vertices[1]);
  }
  if (Math.AbsFloat(twiceArea) < 0.1) return false;
  if (width <= 0 || height <= 0) return true;
  return maxX >= -2 && maxY >= -2 && minX <= width + 2 && minY <= height + 2;
 }
}
