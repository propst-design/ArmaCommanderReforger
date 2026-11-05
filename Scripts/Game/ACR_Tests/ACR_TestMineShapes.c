// -acrMineShapesTest 1 (server-capable, ~30 s): minefield shapes (#200). The outline from a commander's
// points (corners in any order, the size and length limits), an AP area that spreads its mines over the
// whole area off the roads, an AT line across a road (its first mines on the road), a mixed area over a
// road, a shaped field laid inside its outline with signs along its edges and its outline on its own
// side's map only, the same area refused again, and the AI's shapes (a belt across a road on the enemy's
// route, a band across the approach off the road, and different shapes at different spots with variety on).
class ACR_TestMineShapes
{
 static const int CHECKS = 10;
 static bool s_bDone;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range || s_bDone) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("mineshapes", "no Alpha squad");
   return;
  }
  if (tick < 5) return;
  s_bDone = true;
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  us.m_iRP = 500;
  CheckOutlines();
  CheckPlans(alpha);
  CheckAI();
  ACR_TestRegress.Complete("mineshapes", CHECKS);
 }

 // The outline from the points alone.
 protected static void CheckOutlines()
 {
  // A 40 x 20 m rectangle clicked corner, opposite corner, corner, opposite corner (a bow-tie in click order).
  array<vector> clicked = {Vector(0, 0, 0), Vector(40, 0, 20), Vector(40, 0, 0), Vector(0, 0, 20)};
  array<vector> outline = {};
  string why = ACR_MineShape.Outline(clicked, outline);
  float area = ACR_MineShape.Area(outline);
  ACR_TestRegress.Check("mineshapes: corners in any order make one plain shape", why.IsEmpty() && Math.AbsFloat(area - 800) < 1, string.Format("'%1', %2 m²", why, area));
  bool inside = ACR_MineShape.Inside(outline, Vector(20, 0, 10)) && ACR_MineShape.Inside(outline, Vector(39, 0, 1)) && !ACR_MineShape.Inside(outline, Vector(41, 0, 10)) && !ACR_MineShape.Inside(outline, Vector(20, 0, -1));
  array<vector> line = {Vector(0, 0, 0), Vector(30, 0, 0)};
  array<vector> band = {};
  string lineWhy = ACR_MineShape.Outline(line, band);
  bool lineOk = lineWhy.IsEmpty() && band.Count() == 4 && Math.AbsFloat(ACR_MineShape.Area(band) - 30 * ACR_MineShape.LINE_HALF * 2) < 1;
  ACR_TestRegress.Check("mineshapes: inside or not, and a line is a band either side of it", inside && lineOk, string.Format("inside %1, line '%2' %3 corners", inside, lineWhy, band.Count()));
  array<vector> scratch = {};
  array<vector> tooLong = {Vector(0, 0, 0), Vector(130, 0, 0)};
  array<vector> tooBig = {Vector(0, 0, 0), Vector(80, 0, 0), Vector(80, 0, 50), Vector(0, 0, 50)};
  array<vector> tooNarrow = {Vector(0, 0, 0), Vector(60, 0, 0), Vector(60, 0, 1), Vector(0, 0, 1)};
  array<vector> tooShort = {Vector(0, 0, 0), Vector(5, 0, 0)};
  string a = ACR_MineShape.Outline(tooLong, scratch);
  string b = ACR_MineShape.Outline(tooBig, scratch);
  string c = ACR_MineShape.Outline(tooNarrow, scratch);
  string d = ACR_MineShape.Outline(tooShort, scratch);
  bool refused = a.Contains("too long") && b.Contains("too big") && c.Contains("too narrow") && d.Contains("too short");
  ACR_TestRegress.Check("mineshapes: refused when too long, too big, too narrow, or a line too short", refused, string.Format("'%1' '%2' '%3' '%4'", a, b, c, d));
 }

 // Plans (nothing charged) and one shaped field laid by hand.
 protected static void CheckPlans(ACR_Squad alpha)
 {
  string why;
  // An AP area off the roads near the squad: 50 x 30 m.
  ACR_MinePlan ap = null;
  array<vector> area = {};
  vector from = alpha.GetAnchor().GetOrigin();
  for (int r = 0; r < 4 && !ap; r++)
  {
   for (int k = 0; k < 12 && !ap; k++)
   {
    float angle = Math.PI2 * k / 12;
    vector middle = from + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * (70 + r * 25);
    area.Clear();
    area.Insert(middle + Vector(-25, 0, -15));
    area.Insert(middle + Vector(25, 0, -15));
    area.Insert(middle + Vector(25, 0, 15));
    area.Insert(middle + Vector(-25, 0, 15));
    ap = ACR_Minefields.Plan(alpha, ACR_EMineKind.AP, middle, why, vector.Zero, area);
   }
  }
  int inside, offRoad;
  float closest = float.MAX;
  float minX = float.MAX;
  float maxX = -float.MAX;
  float minZ = float.MAX;
  float maxZ = -float.MAX;
  if (ap)
  {
   foreach (int i, vector pos : ap.m_aPos)
   {
    if (ACR_MineShape.Inside(ap.m_aOutline, pos)) inside++;
    if (ACR_Minefields.RoadClearance(pos) >= 1) offRoad++;
    minX = Math.Min(minX, pos[0]);
    maxX = Math.Max(maxX, pos[0]);
    minZ = Math.Min(minZ, pos[2]);
    maxZ = Math.Max(maxZ, pos[2]);
    for (int j = i + 1; j < ap.m_aPos.Count(); j++) closest = Math.Min(closest, vector.DistanceXZ(pos, ap.m_aPos[j]));
   }
  }
  // Spread: the mines reach across at least 70% of the area both ways.
  bool spread = ap && maxX - minX >= 35 && maxZ - minZ >= 21;
  bool apOk = ap && ap.m_aPos.Count() >= 26 && inside == ap.m_aPos.Count() && offRoad == ap.m_aPos.Count() && closest >= ACR_Minefields.AP_SPACING - 0.01 && spread;
  string apDetail = why;
  if (ap) apDetail = string.Format("%1 mines, %2 inside, %3 off the road, closest %4 m, spread %5 x %6 m", ap.m_aPos.Count(), inside, offRoad, closest, maxX - minX, maxZ - minZ);
  ACR_TestRegress.Check("mineshapes: an AP area spreads its mines over the whole area, off the roads", apOk, apDetail);

  // An AT line across a road, 60 m verge to verge.
  vector road, roadDir;
  float width;
  bool found = ACR_TestMinefields.FindRoad(from, road, roadDir, width);
  vector across = Vector(roadDir[2], 0, -roadDir[0]);
  array<vector> line = {road - across * 30, road + across * 30};
  ACR_MinePlan at = null;
  if (found) at = ACR_Minefields.Plan(alpha, ACR_EMineKind.AT, road, why, vector.Zero, line);
  int atInside, atOnRoad;
  if (at)
  {
   foreach (vector atPos : at.m_aPos)
   {
    if (ACR_MineShape.Inside(at.m_aOutline, atPos)) atInside++;
    if (ACR_Minefields.RoadClearance(atPos) < 0) atOnRoad++;
   }
  }
  bool atOk = at && at.m_aPos.Count() >= 10 && atInside == at.m_aPos.Count() && atOnRoad >= 1 && atOnRoad < at.m_aPos.Count();
  string atDetail = string.Format("road %1, %2", found, why);
  if (at) atDetail = string.Format("%1 mines, %2 inside the band, %3 on the road", at.m_aPos.Count(), atInside, atOnRoad);
  ACR_TestRegress.Check("mineshapes: an AT line across a road puts mines on the road first, the rest along the line", atOk, atDetail);

  // A mixed area over the road: 40 m along it, 24 m across.
  array<vector> block = {road + roadDir * 20 + across * 12, road + roadDir * 20 - across * 12, road - roadDir * 20 - across * 12, road - roadDir * 20 + across * 12};
  ACR_MinePlan mixed = null;
  if (found) mixed = ACR_Minefields.Plan(alpha, ACR_EMineKind.MIXED, road, why, vector.Zero, block);
  int atOn, atAll, apOff, apAll;
  if (mixed)
  {
   foreach (int m, vector mixedPos : mixed.m_aPos)
   {
    bool onRoad = ACR_Minefields.RoadClearance(mixedPos) < 0;
    if (mixed.m_aAntiTank[m])
    {
     atAll++;
     if (onRoad) atOn++;
    }
    else
    {
     apAll++;
     if (ACR_Minefields.RoadClearance(mixedPos) >= 1) apOff++;
    }
   }
  }
  bool mixedOk = mixed && atAll >= 6 && atOn == atAll && apAll >= 15 && apOff == apAll;
  string mixedDetail = why;
  if (mixed) mixedDetail = string.Format("AT %1 (%2 on the road), AP %3 (%4 off it)", atAll, atOn, apAll, apOff);
  ACR_TestRegress.Check("mineshapes: a mixed area over a road has its AT mines on the road, its AP mines off it", mixedOk, mixedDetail);

  // The AP area laid by hand: the field keeps its outline, signs along its edges, the outline on US maps only.
  ACR_Minefield field = null;
  int signs;
  if (ap)
  {
   ACR_Minefields.Lay(ap, 1.0, null);
   field = ap.m_Field;
  }
  array<vector> usCorners = {};
  array<int> usCounts = {};
  array<vector> ruCorners = {};
  array<int> ruCounts = {};
  ACR_Minefields.Shapes("US", usCorners, usCounts);
  ACR_Minefields.Shapes("USSR", ruCorners, ruCounts);
  array<vector> zonePos = {};
  array<float> zoneRadii = {};
  array<string> zoneLabels = {};
  ACR_Minefields.Zones("US", zonePos, zoneRadii, zoneLabels);
  if (field) signs = field.m_aSigns.Count();
  bool laid = field && field.m_aCorners.Count() == 4 && field.m_aMines.Count() == ap.m_aPos.Count() && signs >= 5 && usCounts.Count() == 1 && usCounts[0] == 4 && ruCounts.IsEmpty() && zonePos.Count() == 1;
  string laidDetail = "no AP plan";
  if (field) laidDetail = string.Format("%1 corners, %2 mines, %3 signs, US outlines %4, USSR %5, zones %6", field.m_aCorners.Count(), field.m_aMines.Count(), signs, usCounts.Count(), ruCounts.Count(), zonePos.Count());
  ACR_TestRegress.Check("mineshapes: a shaped field keeps its outline, signs along its edges, its outline on its own side's map", laid, laidDetail);

  // The same area again: refused.
  string again;
  bool refused = ap && !ACR_Minefields.Plan(alpha, ACR_EMineKind.AP, ap.m_vSite, again, vector.Zero, area) && again.Contains("already");
  ACR_TestRegress.Check("mineshapes: the same area again is refused", refused, again);

  if (field)
  {
   foreach (IEntity mine : field.m_aMines)
   {
    if (mine) SCR_EntityHelper.DeleteEntityAndChildren(mine);
   }
   field.m_aMines.Clear();
   ACR_Minefields.RemoveField(field);
  }
 }

 // The AI's shapes (ACR_MineSiting.ShapeFor).
 protected static void CheckAI()
 {
  vector road, roadDir;
  float width;
  vector origin = vector.Zero;
  if (!ACR_Base.s_All.IsEmpty()) origin = ACR_Base.s_All[0].GetOrigin();
  bool found = ACR_TestMinefields.FindRoad(origin, road, roadDir, width);
  ACR_MineSite onRoad = new ACR_MineSite();
  onRoad.m_vPos = road;
  onRoad.m_eKind = ACR_EMineKind.MIXED;
  onRoad.m_bOnRoute = true;
  ACR_MineSite field = new ACR_MineSite();
  field.m_vPos = road + Vector(roadDir[2], 0, -roadDir[0]) * 80;
  field.m_eKind = ACR_EMineKind.AP;
  field.m_bDead = true;
  vector enemy = road + roadDir * 500;
  ACR_Variety.s_bOff = true;
  array<vector> belt = {};
  string beltShape = ACR_MineSiting.ShapeFor(onRoad, enemy, belt);
  array<vector> band = {};
  string bandShape = ACR_MineSiting.ShapeFor(field, enemy, band);
  ACR_Variety.s_bOff = false;
  float length = 0;
  if (belt.Count() == 2) length = vector.DistanceXZ(belt[0], belt[1]);
  float bandArea = ACR_MineShape.Area(band);
  bool best = found && belt.Count() == 2 && length >= 40 && length <= 80 && band.Count() == 4 && bandArea >= 600 && bandArea <= 2000;
  ACR_TestRegress.Check("mineshapes: the AI lays a belt across a road on the enemy's route, a band across the approach off it", best, string.Format("%1 (%2 m), %3 (%4 m²)", beltShape, length, bandShape, bandArea));
  // With variety: different spots along the road get different shapes.
  map<string, int> kinds = new map<string, int>();
  for (int s = 0; s < 16; s++)
  {
   ACR_MineSite spot = new ACR_MineSite();
   spot.m_vPos = road + roadDir * (s * 7);
   spot.m_eKind = ACR_EMineKind.MIXED;
   spot.m_bOnRoute = true;
   array<vector> corners = {};
   string shape = ACR_MineSiting.ShapeFor(spot, enemy, corners);
   int seen;
   kinds.Find(shape, seen);
   kinds.Set(shape, seen + 1);
  }
  string list = "";
  foreach (string kind, int n : kinds) list += string.Format("%1 %2; ", kind, n);
  ACR_TestRegress.Check("mineshapes: with variety the AI's shapes differ from spot to spot", kinds.Count() >= 2, list);
 }
}
