// Where the AI commander lays a minefield (the user, after the roadblock siting: "should we add this
// kind of score system to the mines and fortifications too? make the placement smarter and more
// effective"; Jacob: yes, as its own PR). MineSamples' fixed spots used to be tried in order and
// the first one Order accepted was taken. Now the candidates come from where a field does most,
// and are scored (Score):
// - on the road ahead of our roadblock, 60-150 m towards the enemy, and 60-150 m in front of our
//   position, where its own gunners see the field (covered);
// - mixed on a road: the field's 30 m square is turned along the road and spans its verges, so a
//   vehicle can't drive round it; on the enemy's road route to the base (ACR_RoadblockSiting's)
//   and at chokepoints (ACR_SiteScore.Choke) more;
// - AP off the road, more in dead ground an enemy can't see from further out, and in a tree line;
// - the old ring (MineSamples' 150-270 m out) last.
// Better siting never means fewer fields: the score only orders the candidates, every one clear of
// our own defences (ACR_AICommander.ClearOfOwnDefences) is still tried, the old ring included.
class ACR_MineSite : Managed
{
 vector m_vPos;
 ACR_EMineKind m_eKind;
 float m_fScore;
 string m_sSource;     // "roadblock" (ahead of ours on its road), "position", "route" or "ring"
 float m_fCover;       // m from the defence of ours whose gunners see it (0: none does)
 bool m_bOnRoute;      // on the enemy's road route to the base
 string m_sChoke;      // ACR_SiteScore.Choke on its road
 bool m_bDead;         // AP: out of an enemy's sight from DEAD_LOOK m further out
 bool m_bTrees;        // AP: in a tree line
}

class ACR_MineSiting
{
 static const float EYE = 1.6;              // a gunner's eye over the ground
 static const float COVER_RANGE = 200;      // m a defence of ours covers a field from
 static const float AHEAD_NEAR = 60;
 static const float AHEAD_FAR = 150;
 static const float AHEAD_STEP = 30;
 static const float DEAD_LOOK = 150;
 static const float ROUTE_NEAR = 20;
 static const float SAME_SPOT = 15;         // candidates closer than this are one

 // Candidates round our front base at 'basePos' facing 'enemyPos', best first; 'crowded' counts
 // those too near our own defences or bases (left out).
 static void Candidates(ACR_AICommander ai, string faction, vector basePos, vector enemyPos, notnull array<ref ACR_MineSite> sites, out int crowded)
 {
  sites.Clear();
  crowded = 0;
  vector toEnemy = enemyPos - basePos;
  toEnemy[1] = 0;
  toEnemy.Normalize();
  vector side = Vector(toEnemy[2], 0, -toEnemy[0]);
  float bearing = Math.Atan2(toEnemy[0], toEnemy[2]) * Math.RAD2DEG;
  array<vector> route = {};
  ACR_RoadblockSiting.RouteOf(basePos, enemyPos, faction, route);
  array<vector> points = {};
  array<string> sources = {};

  // On the road ahead of our roadblocks there, towards the enemy.
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (!rb || rb.m_sFaction != faction || vector.DistanceXZ(rb.m_vPos, basePos) > 450) continue;
   ACR_RoadInfo road = ACR_SiteScore.Road(rb.m_vPos, bearing);
   for (float ahead = AHEAD_NEAR; ahead <= AHEAD_FAR; ahead += AHEAD_STEP)
   {
    vector onRoad;
    if (road && road.Along(ahead, onRoad)) Add(points, sources, ACR_Manager.Ground(onRoad), "roadblock");
    else Add(points, sources, ACR_Manager.Ground(rb.m_vPos + toEnemy * ahead), "roadblock");
   }
  }
  // In front of our positions there.
  array<float> laterals = {0, -30, 30};
  foreach (ACR_Fortification fort : ACR_Fortify.s_Positions)
  {
   if (fort.m_sFaction != faction || vector.DistanceXZ(fort.m_vPos, basePos) > 400) continue;
   foreach (float lateral : laterals)
   {
    for (float out2 = AHEAD_NEAR; out2 <= AHEAD_FAR; out2 += AHEAD_STEP) Add(points, sources, ACR_Manager.Ground(fort.m_vPos + toEnemy * out2 + side * lateral), "position");
   }
  }
  // On the enemy's road route, 120-300 m out.
  for (int i = 1; i < route.Count(); i++)
  {
   vector a = route[i - 1];
   vector b = route[i];
   float len = vector.DistanceXZ(a, b);
   for (float s = 0; s < len; s += 25)
   {
    vector p = a + (b - a) * (s / Math.Max(len, 0.01));
    float fromBase = vector.DistanceXZ(p, basePos);
    if (fromBase >= 120 && fromBase <= 300) Add(points, sources, ACR_Manager.Ground(p), "route");
   }
  }
  // The old ring: 150-270 m out on the line to the enemy and 60 m to either side.
  array<float> ringLaterals = {0, -60, 60};
  foreach (float ringLateral : ringLaterals)
  {
   for (float reach = 150; reach <= 270; reach += 40) Add(points, sources, ACR_Manager.Ground(basePos + toEnemy * reach + side * ringLateral), "ring");
  }

  foreach (int k, vector point : points)
  {
   if (ai && !ai.ClearOfOwnDefences(point))
   {
    crowded++;
    continue;
   }
   sites.Insert(Describe(faction, point, sources[k], toEnemy, route));
  }
  // Best first; equal scores keep the order above (roadblock, position, route, ring).
  for (int x = 1; x < sites.Count(); x++)
  {
   int y = x;
   while (y > 0 && sites[y].m_fScore > sites[y - 1].m_fScore)
   {
    sites.SwapItems(y, y - 1);
    y--;
   }
  }
 }

 protected static void Add(notnull array<vector> points, notnull array<string> sources, vector p, string source)
 {
  foreach (vector other : points)
  {
   if (vector.DistanceXZ(other, p) < SAME_SPOT) return;
  }
  if (BBB_Terrain.IsWaterAt(p)) return;
  points.Insert(p);
  sources.Insert(source);
 }

 // A candidate's kind (mixed on a road, else AP) and what it has going for it, scored.
 static ACR_MineSite Describe(string faction, vector pos, string source, vector toEnemy, notnull array<vector> route)
 {
  ACR_MineSite site = new ACR_MineSite();
  site.m_vPos = pos;
  site.m_sSource = source;
  site.m_eKind = ACR_EMineKind.AP;
  vector roadPoint, roadDir;
  float roadWidth;
  if (ACR_Minefields.Snap(pos, ACR_Minefields.ROAD_SNAP, roadPoint, roadDir, roadWidth))
  {
   site.m_eKind = ACR_EMineKind.MIXED;
   site.m_sChoke = ACR_SiteScore.Choke(roadPoint, Math.Atan2(roadDir[0], roadDir[2]) * Math.RAD2DEG);
  }
  site.m_fCover = CoverFrom(faction, pos);
  if (route.Count() >= 2) site.m_bOnRoute = ACR_SiteScore.OnPath(pos, route) <= ROUTE_NEAR;
  if (site.m_eKind == ACR_EMineKind.AP)
  {
   BaseWorld world = GetGame().GetWorld();
   vector enemyEye = pos + toEnemy * DEAD_LOOK;
   enemyEye[1] = world.GetSurfaceY(enemyEye[0], enemyEye[2]) + EYE;
   site.m_bDead = !ACR_SiteScore.Visible(enemyEye, pos);
   site.m_bTrees = ACR_FobBuild.CountTrees(pos, 12) >= 4;
  }
  site.m_fScore = Score(site);
  return site;
 }

 // m to the nearest roadblock or position of ours within COVER_RANGE whose gunners see 'pos'.
 static float CoverFrom(string faction, vector pos)
 {
  BaseWorld world = GetGame().GetWorld();
  float best;
  array<vector> defences = {};
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (rb && rb.m_sFaction == faction) defences.Insert(rb.m_vPos);
  }
  foreach (ACR_Fortification fort : ACR_Fortify.s_Positions)
  {
   if (fort.m_sFaction == faction) defences.Insert(fort.m_vPos);
  }
  foreach (vector at : defences)
  {
   float d = vector.DistanceXZ(at, pos);
   if (d > COVER_RANGE || (best > 0 && d >= best)) continue;
   vector eye = at;
   eye[1] = world.GetSurfaceY(at[0], at[2]) + EYE;
   if (ACR_SiteScore.Visible(eye, pos)) best = d;
  }
  return best;
 }

 static float Score(ACR_MineSite site)
 {
  float score;
  if (site.m_sSource == "roadblock") score += 2;
  else if (site.m_sSource == "position") score += 1.5;
  else if (site.m_sSource == "route") score += 1;
  if (site.m_fCover > 0) score += 3;
  if (site.m_fCover >= AHEAD_NEAR && site.m_fCover <= AHEAD_FAR) score += 1;
  if (site.m_eKind == ACR_EMineKind.MIXED)
  {
   score += 1;
   if (site.m_bOnRoute) score += 2;
   if (site.m_sChoke.Contains("town")) score -= 1;
   else if (!site.m_sChoke.IsEmpty()) score += 1.5;
  }
  else
  {
   if (site.m_bDead) score += 1.5;
   if (site.m_bTrees) score += 1;
  }
  return score;
 }

 // For the log: "score 8.5 (ahead of our roadblock, covered from 90 m, on the road, on the enemy's
 // route, woods)".
 static string Breakdown(ACR_MineSite site)
 {
  string why;
  if (site.m_sSource == "roadblock") why = "ahead of our roadblock";
  else if (site.m_sSource == "position") why = "in front of our position";
  else if (site.m_sSource == "route") why = "on the enemy's road route";
  else why = "the ring out from the base";
  if (site.m_fCover > 0) why += string.Format(", covered from %1 m", (int)site.m_fCover);
  else why += ", not covered";
  if (site.m_eKind == ACR_EMineKind.MIXED)
  {
   why += ", on the road";
   if (site.m_bOnRoute && site.m_sSource != "route") why += ", on the enemy's route";
   if (!site.m_sChoke.IsEmpty()) why += ", " + site.m_sChoke;
  }
  else
  {
   if (site.m_bDead) why += ", dead ground";
   if (site.m_bTrees) why += ", tree line";
  }
  return string.Format("score %1 (%2)", site.m_fScore, why);
 }
 // ---------------------------------------------------------------- shapes (#200)
 // The shape the AI lays at a scored site, from the site and weighted at random among the near-best
 // (ACR_Variety, seeded per site):
 // - on a road (AT, mixed): a belt across it, verge to verge, so a vehicle can't drive round, likeliest on the
 //   enemy's road route and at a chokepoint; a block along the road, likelier where our gunners cover it; or
 //   the 30 m square;
 // - off the road (AP): a band across the enemy's approach, likelier in dead ground and tree lines; or the square.
 // Sizes vary a little too. Empty corners: the square.
 static const float BELT_LENGTH = 60;     // m across the road
 static const float BLOCK_LENGTH = 50;    // m along the road
 static const float BLOCK_WIDTH = 18;     // m across it
 static const float BAND_WIDTH = 55;      // m across the enemy's approach
 static const float BAND_DEPTH = 22;      // m deep
 static const float SHAPE_NEAR = 0.25;    // shapes this close to the best one's score are picked among

 static string ShapeFor(ACR_MineSite site, vector enemyPos, notnull array<vector> corners)
 {
  corners.Clear();
  string key = string.Format("mine shape %1", site.m_vPos);
  vector toEnemy = enemyPos - site.m_vPos;
  toEnemy[1] = 0;
  if (toEnemy.Length() < 1) toEnemy = Vector(0, 0, 1);
  toEnemy.Normalize();
  vector roadPoint, roadDir;
  float width;
  bool onRoad = site.m_eKind != ACR_EMineKind.AP && ACR_Minefields.Snap(site.m_vPos, ACR_Minefields.ROAD_SNAP, roadPoint, roadDir, width);
  array<float> scores = {};
  if (onRoad)
  {
   float belt = 1;
   if (site.m_bOnRoute) belt += 0.1;
   if (!site.m_sChoke.IsEmpty()) belt += 0.1;
   float block = 0.95;
   if (site.m_fCover > 0) block += 0.1;
   scores.Insert(belt);
   scores.Insert(block);
   scores.Insert(0.9);
   int pick = ACR_Variety.Pick(scores, key, SHAPE_NEAR);
   vector across = Vector(roadDir[2], 0, -roadDir[0]);
   if (pick == 0)
   {
    float length = ACR_Variety.Jitter(BELT_LENGTH, 0.25, key + " length");
    corners.Insert(roadPoint - across * (length * 0.5));
    corners.Insert(roadPoint + across * (length * 0.5));
    return "belt across the road";
   }
   if (pick == 1)
   {
    float along = ACR_Variety.Jitter(BLOCK_LENGTH, 0.2, key + " length") * 0.5;
    float half = BLOCK_WIDTH * 0.5;
    corners.Insert(roadPoint + roadDir * along + across * half);
    corners.Insert(roadPoint + roadDir * along - across * half);
    corners.Insert(roadPoint - roadDir * along - across * half);
    corners.Insert(roadPoint - roadDir * along + across * half);
    return "block along the road";
   }
   return "square";
  }
  float band = 1;
  if (site.m_bDead) band += 0.05;
  if (site.m_bTrees) band += 0.05;
  scores.Insert(band);
  scores.Insert(0.9);
  if (ACR_Variety.Pick(scores, key, SHAPE_NEAR) != 0) return "square";
  vector side = Vector(toEnemy[2], 0, -toEnemy[0]);
  float wide = ACR_Variety.Jitter(BAND_WIDTH, 0.2, key + " width") * 0.5;
  float deep = ACR_Variety.Jitter(BAND_DEPTH, 0.2, key + " depth") * 0.5;
  corners.Insert(site.m_vPos + side * wide + toEnemy * deep);
  corners.Insert(site.m_vPos - side * wide + toEnemy * deep);
  corners.Insert(site.m_vPos - side * wide - toEnemy * deep);
  corners.Insert(site.m_vPos + side * wide - toEnemy * deep);
  return "band across the approach";
 }
}
