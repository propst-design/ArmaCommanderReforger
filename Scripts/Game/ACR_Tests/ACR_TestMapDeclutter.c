// -acrMapDeclutterTest 1 (server-capable, ~20 s): the calm command map's rules (ACR_MapDeclutter), as
// pure functions: INTEL merging (counts, RP totals, "+N older", "N new"), which bases keep their
// names at each zoom (and which places keep theirs on the dark map), which markers stand and which
// go into a base's count at each zoom (none is ever moved), the counts and their list's place, the
// owner colours, the size pips, the corner marks, and the host option's default and setup row.
class ACR_TestMapDeclutter
{
 static const int CHECKS = 25;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick != 5) return;
  CheckIntel();
  CheckBases();
  CheckDarkNames();
  CheckSquads();
  CheckPlacement();
  CheckOption();
  CheckSquadsList();
  CheckMarkerClicks();
  ACR_TestRegress.Complete("mapdeclutter", CHECKS);
 }

 static string Joined(array<string> texts)
 {
  string all = "";
  foreach (string text : texts) all += text + " | ";
  return all;
 }

 static void CheckIntel()
 {
  // The feed as the server sends it, newest first: four of Yung's purchases (the viewer's own), a
  // sighting twice, two of Popp's, a take of command five minutes back.
  array<string> lines = {
   "Yung requisitioned MG Team at Lamentin (-10 RP)",
   "Yung requisitioned Grenadier Team at Lamentin (-9 RP)",
   "Yung requisitioned MG Team at Lamentin (-10 RP)",
   "Yung requisitioned Grenadier Team at Lamentin (-10 RP)",
   "Alpha: enemy rifle squad near Le Moule",
   "Alpha: enemy rifle squad near Le Moule",
   "Popp requisitioned Transport Truck at Lamentin (-30 RP)",
   "Popp requisitioned Rifle Squad at Lamentin (-15 RP)",
   "Yung is in command"
  };
  array<int> ages = {5, 7, 8, 9, 30, 40, 50, 55, 300};
  array<string> texts = {};
  array<int> entryAges = {};
  ACR_MapDeclutter.MergeFeed(lines, ages, "Yung", texts, entryAges);
  array<string> expected = {"Bought MG Team ×2, Grenadier Team ×2 (-39 RP)", "Alpha: enemy rifle squad near Le Moule ×2", "Popp bought Transport Truck, Rifle Squad (-45 RP)", "Yung is in command"};
  bool same = texts.Count() == expected.Count();
  if (same) foreach (int i, string text : texts) if (text != expected[i]) same = false;
  bool agesOk = entryAges.Count() == 4 && entryAges[0] == 5 && entryAges[1] == 30 && entryAges[2] == 50 && entryAges[3] == 300;
  ACR_TestRegress.Check("mapdeclutter: INTEL merges a run of purchases (counts per unit, RP summed) and a repeated line", same && agesOk, Joined(texts));

  // Not merged: purchases minutes apart, or by different commanders; the tag-less form parses.
  array<string> apart = {"Yung requisitioned MG Team at Lamentin (-10 RP)", "Yung requisitioned MG Team at Lamentin (-10 RP)", "Popp requisitioned MG Team at Lamentin (-10 RP)"};
  array<int> apartAges = {5, 400, 410};
  array<string> apartTexts = {};
  array<int> apartEntryAges = {};
  ACR_MapDeclutter.MergeFeed(apart, apartAges, "Yung", apartTexts, apartEntryAges);
  string tag, unit;
  int rp;
  bool parsed = ACR_MapDeclutter.ParsePurchase("requisitioned Mortar Team at Le Moule (-24 RP)", tag, unit, rp) && tag.IsEmpty() && unit == "Mortar Team" && rp == 24;
  ACR_TestRegress.Check("mapdeclutter: purchases minutes apart or by another commander stay apart", apartTexts.Count() == 3 && apartTexts[0] == "Bought MG Team (-10 RP)" && apartTexts[2] == "Popp bought MG Team (-10 RP)" && parsed, Joined(apartTexts) + string.Format(" parsed %1 '%2' '%3' %4", parsed, tag, unit, rp));

  // The last 3 shown, "+N older"; "N new" since the last look; the bar's headline.
  bool older = ACR_MapDeclutter.Older(texts.Count()) == 1 && ACR_MapDeclutter.Older(3) == 0 && ACR_MapDeclutter.Older(0) == 0 && ACR_MapDeclutter.Older(9) == 6;
  bool fresh = ACR_MapDeclutter.NewCount(entryAges, 40) == 2 && ACR_MapDeclutter.NewCount(entryAges, 2) == 0;
  string attacked = ACR_MapDeclutter.Headline("Le Moule", texts);
  string newest = ACR_MapDeclutter.Headline("", texts);
  array<string> none = {};
  string empty = ACR_MapDeclutter.Headline("", none);
  bool headline = attacked == "Le Moule under attack" && newest.StartsWith("Bought MG Team") && newest.Length() <= ACR_MapDeclutter.HEADLINE_CHARS + 2 && newest.EndsWith("...") && empty == "No contacts reported";
  ACR_TestRegress.Check("mapdeclutter: INTEL shows 3 lines and +N older; the bar says what is under attack and how many are new", older && fresh && headline, string.Format("older %1, new %2/%3, '%4', '%5', '%6'", ACR_MapDeclutter.Older(texts.Count()), ACR_MapDeclutter.NewCount(entryAges, 40), ACR_MapDeclutter.NewCount(entryAges, 2), attacked, newest, empty));
 }

 static void CheckBases()
 {
  // Our two bases at 0 and 1000 m, a free base 500 m past the second (the front), one far out
  // (3000), an enemy base 800 m behind the first (the front too), the enemy HQ at 6000, an enemy
  // base at 4500 under attack.
  array<vector> pos = {Vector(0, 0, 0), Vector(1000, 0, 0), Vector(1500, 0, 0), Vector(3000, 0, 0), Vector(-800, 0, 0), Vector(6000, 0, 0), Vector(4500, 0, 0)};
  array<string> names = {"Lamentin", "Sawmill", "Le Moule", "Villeneuve", "Provins", "Harbor", "Richemont"};
  array<string> owners = {"US", "US", "", "", "USSR", "USSR", "USSR"};
  array<bool> hq = {false, false, false, false, false, true, false};
  array<bool> contested = {false, false, false, false, false, false, true};
  array<int> sides = {};
  foreach (string owner : owners) sides.Insert(ACR_MapDeclutter.SideOf(owner, "US"));
  array<bool> front = {};
  ACR_MapDeclutter.FrontBases(pos, sides, front);
  string farNamed = "";
  string nearNamed = "";
  foreach (int i, string name : names)
  {
   if (ACR_MapDeclutter.KeepsName(true, sides[i], hq[i], contested[i], front[i], false)) farNamed += name + " ";
   if (ACR_MapDeclutter.KeepsName(false, sides[i], hq[i], contested[i], front[i], false)) nearNamed += name + " ";
  }
  bool selectedKept = ACR_MapDeclutter.KeepsName(true, sides[3], false, false, false, true);
  // Far only near the map's widest zoom (the test range: 0.527 px/m; zoom 1 isn't far, pn12f).
  bool zoom = ACR_MapDeclutter.IsFar(0.527, 0.527) && ACR_MapDeclutter.IsFar(0.65, 0.527) && !ACR_MapDeclutter.IsFar(1, 0.527) && !ACR_MapDeclutter.IsFar(0.5, 0);
  ACR_TestRegress.Check("mapdeclutter: zoomed out only our bases, the front, the enemy HQ and bases under attack keep names; near, all", farNamed == "Lamentin Sawmill Le Moule Provins Harbor Richemont " && nearNamed == "Lamentin Sawmill Le Moule Villeneuve Provins Harbor Richemont " && selectedKept && zoom, string.Format("far: %1| near: %2| selected %3, zoom %4", farNamed, nearNamed, selectedKept, zoom));

  // Owner colours: blue ours, red enemy, yellow free (green FIA).
  Color own = ACR_MapDeclutter.SideColor(ACR_EBaseSide.OWN);
  Color enemy = ACR_MapDeclutter.SideColor(ACR_EBaseSide.ENEMY);
  Color free = ACR_MapDeclutter.SideColor(ACR_EBaseSide.FREE);
  bool sidesOk = ACR_MapDeclutter.SideOf("US", "US") == ACR_EBaseSide.OWN && ACR_MapDeclutter.SideOf("USSR", "US") == ACR_EBaseSide.ENEMY && ACR_MapDeclutter.SideOf("", "US") == ACR_EBaseSide.FREE && ACR_MapDeclutter.SideOf(ACR_Pockets.FACTION, "US") == ACR_EBaseSide.OTHER;
  bool colours = own.B() > own.R() && own.B() > own.G() && enemy.R() > enemy.G() && enemy.R() > enemy.B() && free.R() > free.B() && free.G() > free.B();
  ACR_TestRegress.Check("mapdeclutter: bases are coloured by owner: blue ours, red enemy, yellow free", sidesOk && colours, string.Format("sides %1, own %2, enemy %3, free %4", sidesOk, own, enemy, free));

  // Size pips instead of "[CP II]", from the label's [N] or the detail's +N RP; captions.
  bool pips = ACR_MapDeclutter.Pips("Le Moule [2]", "") == 2 && ACR_MapDeclutter.Pips("Lamentin [1]", "") == 1 && ACR_MapDeclutter.Pips("Harbor", "US | garrison 3 | +3 RP") == 3 && ACR_MapDeclutter.Pips("Farm", "Unclaimed | garrison 0 | no income") == 0 && ACR_MapDeclutter.Pips("Big [7]", "") == 3;
  string capHQ = ACR_MapDeclutter.Caption("Harbor [3]", "USSR | garrison 5 | +3 RP | HQ", ACR_EBaseSide.ENEMY, true);
  string capOwnHQ = ACR_MapDeclutter.Caption("Base [1]", "US | garrison 5 | +1 RP | HQ", ACR_EBaseSide.OWN, true);
  string capPlain = ACR_MapDeclutter.Caption("Le Moule [2]", "Unclaimed | +2 RP", ACR_EBaseSide.FREE, true);
  string capDot = ACR_MapDeclutter.Caption("Le Moule [2]", "Unclaimed | +2 RP", ACR_EBaseSide.FREE, false);
  bool hqDestroyed = !ACR_MapDeclutter.IsHQ("US | garrison 0 | HQ DESTROYED: FOB offline until rebuilt");
  ACR_TestRegress.Check("mapdeclutter: size pips come from the base size; the name drops [N]; HQs say so; a plain dot has no caption", pips && capHQ == "Harbor · enemy HQ" && capOwnHQ == "Base · HQ" && capPlain == "Le Moule" && capDot.IsEmpty() && hqDestroyed, string.Format("pips %1, '%2', '%3', '%4', '%5', destroyed-not-HQ %6", pips, capHQ, capOwnHQ, capPlain, capDot, hqDestroyed));

  // Filled shapes stay on the canvas and non-degenerate.
  array<float> onCanvas = {100, 100, 110, 100, 110, 110, 100, 110};
  array<float> offCanvas = {-500, -500, -490, -500, -490, -490};
  array<float> flat = {10, 10, 20, 20, 30, 30};
  ACR_TestRegress.Check("mapdeclutter: the calm map's filled shapes are on-canvas and non-degenerate", ACR_MapDeclutter.ShapeOk(onCanvas, 1920, 1080) && !ACR_MapDeclutter.ShapeOk(offCanvas, 1920, 1080) && !ACR_MapDeclutter.ShapeOk(flat, 1920, 1080), "");
 }

 static void CheckSquads()
 {
  // Lamentin at the origin (ring 100 m: 80 given, raised to the least), Sawmill 1000 m east (ring
  // 300 m). Squads, world m: on Lamentin's dot, 60 m out in its ring, 150 m out past its ring, 600 m
  // out in the open, 280 m from Sawmill in its ring.
  array<vector> bases = {Vector(0, 0, 0), Vector(1000, 0, 0)};
  array<float> radii = {80, 300};
  array<vector> squads = {Vector(5, 0, 5), Vector(60, 0, 0), Vector(150, 0, 0), Vector(600, 0, 600), Vector(1280, 0, 0)};
  bool ground = ACR_MapDeclutter.BaseAt(squads[0], bases, radii) == 0 && ACR_MapDeclutter.BaseAt(squads[1], bases, radii) == 0 && ACR_MapDeclutter.BaseAt(squads[2], bases, radii) == -1 && ACR_MapDeclutter.BaseAt(squads[3], bases, radii) == -1 && ACR_MapDeclutter.BaseAt(squads[4], bases, radii) == 1;
  ACR_TestRegress.Check("mapdeclutter: a squad is at a base on the ground when in its ring (100 m at least), nearest first", ground, "");

  // Across the zoom (px/m), each squad where it would stand: at each zoom it goes into a count only
  // for touching a dot or being in a ring too small to tell; zooming in, once out it stays out.
  array<float> zooms = {0.1, 0.2, 0.3, 0.42, 0.5, 0.8, 1.5, 3};
  string table = "";
  array<bool> everOut = {false, false, false, false, false};
  bool monotone = true;
  array<int> farInto = {};
  array<int> nearInto = {};
  foreach (int z, float zoom : zooms)
  {
   array<float> dots = {};
   array<float> rings = {};
   foreach (int b, vector base : bases)
   {
    dots.Insert(500 + base[0] * zoom);
    dots.Insert(500 + base[2] * zoom);
    rings.Insert(ACR_MapDeclutter.RingOf(b, radii) * zoom);
   }
   foreach (int g, vector squad : squads)
   {
    int into = ACR_MapDeclutter.CondensesInto(500 + squad[0] * zoom, 500 + squad[2] * zoom, 0, 0, 30, 22, dots, 10, rings, 44);
    table += string.Format("%1 ", into);
    if (z == 0) farInto.Insert(into);
    if (z == zooms.Count() - 1) nearInto.Insert(into);
    if (into >= 0 && everOut[g]) monotone = false;
    if (into < 0) everOut[g] = true;
   }
   table += "| ";
  }
  // Far (0.1 px/m): the first three on Lamentin (the third's icon touches its dot), the fourth stands,
  // the fifth in Sawmill's small ring. Near (3 px/m): only the one on the dot; the rest stand.
  bool far = farInto[0] == 0 && farInto[1] == 0 && farInto[2] == 0 && farInto[3] == -1 && farInto[4] == 1;
  bool near = nearInto[0] == 0 && nearInto[1] == -1 && nearInto[2] == -1 && nearInto[3] == -1 && nearInto[4] == -1;
  ACR_TestRegress.Check("mapdeclutter: zooming in, a squad goes from its base's count to its own place and stays there; in the open it always stands", far && near && monotone, table);

  // A big ring on screen doesn't take in what stands in it; one too small to tell does; the nearest
  // base wins.
  array<float> twoDots = {100, 100, 160, 100};
  array<float> big = {200, 200};
  array<float> small = {30, 30};
  bool rule = ACR_MapDeclutter.CondensesInto(100, 60, 0, 0, 30, 22, twoDots, 10, big, 44) == -1 && ACR_MapDeclutter.CondensesInto(115, 100, 0, 0, 30, 22, twoDots, 10, big, 44) == 0 && ACR_MapDeclutter.CondensesInto(145, 100, 0, 0, 30, 22, twoDots, 10, big, 44) == 1 && ACR_MapDeclutter.CondensesInto(100, 75, 0, 0, 30, 22, twoDots, 10, small, 44) == 0 && ACR_MapDeclutter.CondensesInto(100, 60, 0, 0, 30, 22, twoDots, 10, small, 44) == -1;
  // A symbol drawn 20 px above its point: what counts is the icon as drawn (the first shots: one
  // stood half under the dot).
  bool drawn = ACR_MapDeclutter.CondensesInto(100, 125, 0, -20, 30, 22, twoDots, 10, big, 44) == 0 && ACR_MapDeclutter.CondensesInto(100, 125, 0, 0, 30, 22, twoDots, 10, big, 44) == -1;
  ACR_TestRegress.Check("mapdeclutter: an icon touching a dot as drawn, or in a too-small ring, goes into the nearest base; else it stands", rule && drawn, string.Format("rule %1, as drawn %2", rule, drawn));

  // Piles: two on top of each other (the second selected), one a little off, one elsewhere, one not
  // standing (in a base's count) on top of the first.
  array<float> centres = {100, 100, 105, 103, 130, 100, 400, 400, 101, 101};
  array<bool> standing = {true, true, true, true, false};
  array<bool> selected = {false, true, false, false, false};
  array<int> pileOf = {};
  array<int> count = {};
  array<bool> rep = {};
  ACR_MapDeclutter.Piles(centres, standing, selected, 12, pileOf, count, rep);
  string got = "";
  foreach (int k, int pile : pileOf) got += string.Format("%1/%2/%3 ", pile, count[k], rep[k]);
  bool ok = pileOf[0] == 1 && pileOf[1] == 1 && pileOf[2] == 2 && pileOf[3] == 3 && pileOf[4] == -1;
  ok = ok && count[0] == 2 && count[1] == 2 && count[2] == 1 && count[3] == 1;
  ok = ok && !rep[0] && rep[1] && rep[2] && rep[3];
  ACR_TestRegress.Check("mapdeclutter: squads on top of each other are one icon with ×N (the selected one shown); a squad a little off stands", ok, got);

  // The count's words, by side; the list's title; how far out.
  bool words = ACR_MapDeclutter.ChipText(4, 0) == "×4 units" && ACR_MapDeclutter.ChipText(1, 0) == "×1 unit" && ACR_MapDeclutter.ChipText(0, 2) == "×2 enemy" && ACR_MapDeclutter.ChipText(3, 1) == "×3 units · ×1 enemy" && ACR_MapDeclutter.ChipText(0, 0).IsEmpty();
  bool titles = ACR_MapDeclutter.ListTitle(4, 0, "Pusta") == "4 UNITS AT PUSTA" && ACR_MapDeclutter.ListTitle(1, 2, "Pusta") == "1 UNIT + 2 ENEMY AT PUSTA" && ACR_MapDeclutter.ListTitle(0, 2, "Pusta") == "2 ENEMY AT PUSTA";
  bool away = ACR_MapDeclutter.OutOfRing(50, 100).IsEmpty() && ACR_MapDeclutter.OutOfRing(424, 100) == "420 m out" && ACR_MapDeclutter.OutOfRing(426, 100) == "430 m out";
  ACR_TestRegress.Check("mapdeclutter: a base's count says units and enemy; its list's title and a far unit's row say so too", words && titles && away, string.Format("words %1 (%2), titles %3 (%4), away %5 (%6)", words, ACR_MapDeclutter.ChipText(3, 1), titles, ACR_MapDeclutter.ListTitle(1, 2, "Pusta"), away, ACR_MapDeclutter.OutOfRing(424, 100)));

  bool marks = ACR_MapDeclutter.SquadMark(true, 300, -1) == ACR_ESquadMark.MOVING && ACR_MapDeclutter.SquadMark(true, 10, -1) == ACR_ESquadMark.HOLDING && ACR_MapDeclutter.SquadMark(false, -1, 150) == ACR_ESquadMark.FIGHTING && ACR_MapDeclutter.SquadMark(true, 300, 500) == ACR_ESquadMark.MOVING && ACR_MapDeclutter.SquadMark(false, -1, -1) == ACR_ESquadMark.HOLDING;
  ACR_TestRegress.Check("mapdeclutter: the corner mark says fighting, moving or holding", marks, "");
 }

 // Placement (pn12f shots: "Farm · HQ" under squad icons, order dots and "Crossroads"): names keep
 // their spot unless they'd cover another name or dot, or what must stay where it is (an icon, a
 // count); order ends near a base end at it.
 static void CheckPlacement()
 {
  // Three bases: Farm, Crossroads 30 px to its right and 10 up, Outpost far off. Captions centred under
  // their dots, 120 x 20 px.
  array<float> dots = {500, 500, 530, 490, 900, 500};
  array<float> captions = {-60, 14, 60, 34, -60, 14, 60, 34, -60, 14, 60, 34};
  array<int> order = {0, 1, 2};
  array<float> shift = {};
  ACR_MapDeclutter.PlaceNames(dots, captions, order, 10, 4, shift);
  array<float> names = {};
  for (int b = 0; b < 3; b++)
  {
   names.Insert(dots[b * 2] + shift[b * 2] + captions[b * 4]);
   names.Insert(dots[b * 2 + 1] + shift[b * 2 + 1] + captions[b * 4 + 1]);
   names.Insert(dots[b * 2] + shift[b * 2] + captions[b * 4 + 2]);
   names.Insert(dots[b * 2 + 1] + shift[b * 2 + 1] + captions[b * 4 + 3]);
  }
  bool apart = true;
  for (int i = 0; i < 3; i++)
  {
   for (int j = 0; j < 3; j++)
   {
    if (i == j) continue;
    if (j > i && ACR_MapDeclutter.Overlaps(names[i * 4], names[i * 4 + 1], names[i * 4 + 2], names[i * 4 + 3], names[j * 4], names[j * 4 + 1], names[j * 4 + 2], names[j * 4 + 3])) apart = false;
    // ... nor over another base's dot.
    if (ACR_MapDeclutter.Overlaps(names[i * 4], names[i * 4 + 1], names[i * 4 + 2], names[i * 4 + 3], dots[j * 2] - 10, dots[j * 2 + 1] - 10, dots[j * 2] + 10, dots[j * 2 + 1] + 10)) apart = false;
   }
  }
  bool kept = shift[0] == 0 && shift[1] == 0 && shift[4] == 0 && shift[5] == 0 && (shift[2] != 0 || shift[3] != 0);
  ACR_TestRegress.Check("mapdeclutter: base names near each other separate (the first keeps its spot)", apart && kept, string.Format("shifts %1,%2 %3,%4 %5,%6", shift[0], shift[1], shift[2], shift[3], shift[4], shift[5]));

  // An icon standing on Outpost's name's spot: the name gives way, the icon doesn't move.
  array<float> icon = {885, 516, 915, 538};
  array<float> gave = {};
  ACR_MapDeclutter.PlaceNames(dots, captions, order, 10, 4, gave, icon);
  float nameY0 = dots[5] + gave[5] + captions[9];
  float nameY1 = dots[5] + gave[5] + captions[11];
  float nameX0 = dots[4] + gave[4] + captions[8];
  float nameX1 = dots[4] + gave[4] + captions[10];
  bool gaveWay = (gave[4] != 0 || gave[5] != 0) && !ACR_MapDeclutter.Overlaps(nameX0, nameY0, nameX1, nameY1, icon[0], icon[1], icon[2], icon[3]);
  ACR_TestRegress.Check("mapdeclutter: a base's name gives way to an icon standing on it", gaveWay, string.Format("moved %1,%2", gave[4], gave[5]));

  array<float> obstacles = {};
  for (int d = 0; d < 3; d++)
  {
   obstacles.Insert(dots[d * 2] - 10);
   obstacles.Insert(dots[d * 2 + 1] - 10);
   obstacles.Insert(dots[d * 2] + 10);
   obstacles.Insert(dots[d * 2 + 1] + 10);
  }
  foreach (float edge : names) obstacles.Insert(edge);

  // A base's count (60 x 20): right of its dot in the open; left of it when another dot is there;
  // past an icon, at a dot's cost only when nothing else will do; over its own name before an icon
  // (the first shots: it hid a squad to leave the name alone); to the right when boxed in.
  array<float> noRects = {};
  vector open = ACR_MapDeclutter.PlaceChip(900, 500, 60, 20, 10, 3, noRects, obstacles, noRects);
  array<float> neighbour = {930, 490, 950, 510};
  vector left = ACR_MapDeclutter.PlaceChip(900, 500, 60, 20, 10, 3, neighbour, noRects, noRects);
  array<float> iconsAround = {820, 470, 980, 530};
  vector corner = ACR_MapDeclutter.PlaceChip(900, 500, 60, 20, 10, 3, noRects, iconsAround, noRects);
  vector overIcons = ACR_MapDeclutter.PlaceChip(900, 500, 60, 20, 10, 3, neighbour, iconsAround, noRects);
  array<float> iconsRight = {913, 440, 990, 535, 820, 440, 990, 487};
  array<float> ownName = {820, 505, 900, 535};
  vector overName = ACR_MapDeclutter.PlaceChip(900, 500, 60, 20, 10, 3, noRects, iconsRight, ownName);
  array<float> boxed = {700, 300, 1100, 700};
  vector stuck = ACR_MapDeclutter.PlaceChip(900, 500, 60, 20, 10, 3, boxed, noRects, noRects);
  bool chips = open[0] == 943 && open[1] == 500 && left[0] == 857 && left[1] == 500 && corner[0] == 943 && corner[1] == 500 && overIcons[0] == 857 && overName[0] == 857 && overName[1] == 500 && stuck[0] == 943;
  ACR_TestRegress.Check("mapdeclutter: a base's count goes beside its dot, clear of what is there: over its own name before an icon, over an icon before a dot", chips, string.Format("open %1, beside another %2, icons around %3, both %4, over its name %5, boxed in %6", open, left, corner, overIcons, overName, stuck));

  // The list (500 x 300) on a 1920 x 1080 screen, with the ACA panel top right and INTEL top left:
  // right of what it opens from, or left of it at the screen's edge, or beside the panel; on the screen.
  array<float> panels = {1600, 96, 1896, 700, 110, 96, 450, 190};
  vector plain = ACR_MapDeclutter.PlaceBox(600, 600, 500, 500, 300, 60, 30, 1920, 1080, 8, panels);
  vector byPanel = ACR_MapDeclutter.PlaceBox(1300, 1300, 500, 500, 300, 60, 30, 1920, 1080, 8, panels);
  vector low = ACR_MapDeclutter.PlaceBox(600, 600, 1000, 500, 300, 60, 30, 1920, 1080, 8, panels);
  array<float> noPanels = {};
  vector edge2 = ACR_MapDeclutter.PlaceBox(1800, 1800, 500, 500, 300, 60, 30, 1920, 1080, 8, noPanels);
  // Beside a count 100 px wide: past its far end to the right, before its near end to the left.
  vector pastChip = ACR_MapDeclutter.PlaceBox(600, 700, 500, 500, 300, 20, 30, 1920, 1080, 8, noPanels);
  vector beforeChip = ACR_MapDeclutter.PlaceBox(1400, 1500, 500, 500, 300, 20, 30, 1920, 1080, 8, noPanels);
  bool placed = plain[0] == 660 && plain[1] == 470 && byPanel[0] == 740 && low[1] == 772 && edge2[0] == 1240 && pastChip[0] == 720 && beforeChip[0] == 880;
  bool clearOf = ACR_MapDeclutter.FirstHit(byPanel[0], byPanel[1], byPanel[0] + 500, byPanel[1] + 300, panels) < 0;
  ACR_TestRegress.Check("mapdeclutter: a base's list opens beside its count, clear of the panels and on the screen", placed && clearOf, string.Format("%1, %2, %3, %4, %5, %6", plain, byPanel, low, edge2, pastChip, beforeChip));

  // Order ends near a base end at it: two orders to Farm merge into one mark.
  // ... and one landing on a base's name (Crossroads', moved above its dot) ends at that base too.
  array<float> none = {};
  bool snaps = ACR_MapDeclutter.SnapToBase(505, 498, dots, 26, names) == 0 && ACR_MapDeclutter.SnapToBase(492, 512, dots, 26, names) == 0 && ACR_MapDeclutter.SnapToBase(533, 507, dots, 26, names) == 1 && ACR_MapDeclutter.SnapToBase(700, 700, dots, 26, names) == -1;
  bool onName = ACR_MapDeclutter.SnapToBase(580, 460, dots, 26, names) == 1 && ACR_MapDeclutter.SnapToBase(580, 460, dots, 26, none) == -1;
  ACR_TestRegress.Check("mapdeclutter: order ends near a base or on its name merge at its dot", snaps && onName, string.Format("near %1, on a name %2", snaps, onName));

  // Soldiers' round markers (14 px) on Farm's name or touching Crossroads' dot count as on it
  // (hidden); one in the open doesn't.
  bool onIt = ACR_MapDeclutter.OnAName(500, 524, 14, obstacles) && ACR_MapDeclutter.OnAName(552, 490, 14, obstacles) && !ACR_MapDeclutter.OnAName(700, 700, 14, obstacles);
  ACR_TestRegress.Check("mapdeclutter: soldiers' markers on a base's name or dot count as on it", onIt, string.Format("on %1", onIt));
 }

 // A click on a marker drawn over a base's name (a town's "Civilians (n)" ring label, a zombie contact)
 // reaches the base; squads, pings, our command unit and bases keep their own clicks. The rectangles:
 // Farm's name and icon, then Crossroads'; the click on Farm's name, where the zone's label sits.
 static void CheckMarkerClicks()
 {
  ACR_UnitMarker zone = new ACR_UnitMarker();
  ACR_UnitMarker contact = new ACR_UnitMarker();
  contact.m_bContact = true;
  ACR_UnitMarker ping = new ACR_UnitMarker();
  ping.m_iPingId = 3;
  ACR_UnitMarker own = new ACR_UnitMarker();
  own.m_Original = true;
  ACR_UnitMarker baseMarker = new ACR_UnitMarker();
  baseMarker.m_BaseIndex = 0;
  ACR_UnitMarker squad = new ACR_UnitMarker();
  ACR_UnitMarker commandUnit = new ACR_UnitMarker();
  commandUnit.m_bCommandUnit = true;
  bool passes = zone.PassesClickToBase(false) && contact.PassesClickToBase(false) && commandUnit.PassesClickToBase(true) && !squad.PassesClickToBase(true) && !ping.PassesClickToBase(false) && !own.PassesClickToBase(true) && !baseMarker.PassesClickToBase(false);
  array<float> rects = {500, 500, 16, 16, 518, 502, 60, 14, 800, 300, 16, 16, 818, 302, 90, 14};
  bool hits = ACR_MapMarkerHit.RectAt(rects, 540, 508) == 1 && ACR_MapMarkerHit.RectAt(rects, 850, 309) == 3 && ACR_MapMarkerHit.RectAt(rects, 650, 650) == -1;
  array<float> broken = {1, 2, 3};
  bool safe = ACR_MapMarkerHit.RectAt(broken, 2, 3) == -1;
  ACR_TestRegress.Check("mapdeclutter: a click on a zone label or zombie contact over a base's name reaches the base", passes && hits && safe, string.Format("passes %1 (zone %4, contact %5, squad %6, base %7), hits %2, safe %3", passes, hits, safe, zone.PassesClickToBase(false), contact.PassesClickToBase(false), squad.PassesClickToBase(true), baseMarker.PassesClickToBase(false)));
 }

 // The dark map's place names (#203): a name near a base gives way only while that base's name is
 // drawn. West Zagoria's case: a base in nearly every town, and zoomed out only an HQ named.
 static void CheckDarkNames()
 {
  array<vector> bases = {Vector(1000, 0, 1000), Vector(3000, 0, 1000), Vector(5000, 0, 1000)};
  int atHQ = ACR_MapDeclutter.NearestBase(Vector(1100, 0, 1050), bases, 300);
  int atDot = ACR_MapDeclutter.NearestBase(Vector(3150, 0, 900), bases, 300);
  int alone = ACR_MapDeclutter.NearestBase(Vector(4000, 0, 1000), bases, 300);
  int past = ACR_MapDeclutter.NearestBase(Vector(5000, 0, 1310), bases, 300);
  bool picks = atHQ == 0 && atDot == 1 && alone == -1 && past == -1;
  // Zoomed out: base 0 named, bases 1 and 2 bare dots (x1 <= x0), as the far layout leaves them.
  array<float> farNames = {-20.0, -30.0, 40.0, -18.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  bool farShows = !ACR_MapDeclutter.PlaceNameShows(atHQ, farNames) && ACR_MapDeclutter.PlaceNameShows(atDot, farNames) && ACR_MapDeclutter.PlaceNameShows(alone, farNames) && ACR_MapDeclutter.PlaceNameShows(past, farNames);
  // Closer in every base is named: every name near a base gives way, as before.
  array<float> nearNames = {-20.0, -30.0, 40.0, -18.0, -25.0, -30.0, 50.0, -18.0, -22.0, -30.0, 44.0, -18.0};
  bool nearShows = !ACR_MapDeclutter.PlaceNameShows(atHQ, nearNames) && !ACR_MapDeclutter.PlaceNameShows(atDot, nearNames) && ACR_MapDeclutter.PlaceNameShows(alone, nearNames);
  // No layout yet (no names at all): nothing gives way.
  array<float> none = {};
  bool empty = ACR_MapDeclutter.PlaceNameShows(atHQ, none) && ACR_MapDeclutter.PlaceNameShows(atDot, none);
  ACR_TestRegress.Check("mapdeclutter: on the dark map a place near a base keeps its name while the base shows only as a dot (#203)", picks && farShows && nearShows && empty, string.Format("bases %1 %2 %3 %4, far %5, near %6, no layout %7", atHQ, atDot, alone, past, farShows, nearShows, empty));
 }

 static void CheckOption()
 {
  // On by default; -acrMapDeclutter 0 or the setup row turns it off.
  ACR_Options.Unset(ACR_MapDeclutter.OPTION);
  bool byDefault = ACR_MapDeclutter.OptionOn();
  string onRows = ACR_Setup.Rows();
  ACR_Options.Set(ACR_MapDeclutter.OPTION, "0");
  bool off = !ACR_MapDeclutter.OptionOn();
  string offRows = ACR_Setup.Rows();
  ACR_Options.Unset(ACR_MapDeclutter.OPTION);
  ACR_TestRegress.Check("mapdeclutter: on by default, -acrMapDeclutter 0 turns it off", byDefault && off, string.Format("default %1, with 0 off %2", byDefault, off));
  bool row = onRows.Contains("declutter\tCalm command map: on") && offRows.Contains("declutter\tCalm command map: off (the classic map)") && ACR_Setup.Option("declutter") == "acrMapDeclutter" && ACR_SetupDialog.Tab("declutter") == 1;
  ACR_TestRegress.Check("mapdeclutter: a setup row on the Command tab", row, string.Format("tab %1, option %2", ACR_SetupDialog.Tab("declutter"), ACR_Setup.Option("declutter")));
  // The player's own choices default to the sketch: the light map, compact.
  ACR_MapDeclutter.ResetForTest();
  ACR_TestRegress.Check("mapdeclutter: the dark map is off and the UI compact by default", !ACR_MapDeclutter.Dark() && ACR_MapDeclutter.Compact() && ACR_MapDeclutter.On(), "");
 }

 // The Squads list's search (name or kind, any case) and its rows (health or ammo only when down);
 // the map layers by a squad's symbol, all on to begin with.
 static void CheckSquadsList()
 {
  bool search = ACR_SquadsList.Matches("", "1st Squad", "infantry") && ACR_SquadsList.Matches("2nd", "2nd Squad", "infantry") && ACR_SquadsList.Matches("mort", "Heavy Weapons", "mortar") && ACR_SquadsList.Matches("squad", "1ST SQUAD", "") && !ACR_SquadsList.Matches("2nd", "1st Squad", "infantry");
  string full = ACR_SquadsList.Label("1st Squad", "infantry", "holding", 100, 100, false);
  string hurt = ACR_SquadsList.Label("A Very Long Squad Name Indeed", "MG", "moving", 60, 100, true);
  bool rows = full == "1st Squad · infantry · holding" && hurt.StartsWith("» A Very Long Squad N...") && hurt.EndsWith("HP 60%") && !hurt.Contains("ammo");
  bool layers = !ACR_MapLayers.AnyOff() && ACR_MapLayers.KindOf(0) == ACR_MapLayers.INFANTRY && ACR_MapLayers.KindOf(EMilitarySymbolIcon.ARMOR) == ACR_MapLayers.VEHICLES && ACR_MapLayers.KindOf(EMilitarySymbolIcon.SUPPLY | EMilitarySymbolIcon.MOTORIZED) == ACR_MapLayers.SUPPORT;
  ACR_TestRegress.Check("mapdeclutter: the Squads list searches by name or kind; rows show health or ammo only when down; layers by symbol, all on", search && rows && layers, string.Format("search %1, rows '%2' '%3', layers %4", search, full, hurt, layers));
 }
}
