// Where the AI commander digs a position in (the user, after the roadblock siting: "should we add
// this kind of score system to the mines and fortifications too? make the placement smarter and
// more effective"; Jacob: yes, as its own PR). DigIn used to try fixed spots 80-160 m out on the
// line to the enemy (and DEFENCE_STANDOFF to either side), those where its upgrades fit first.
// Now a fan of candidates towards the enemy (ACR_SiteScore.Fan, 80-200 m out, 50 degrees either
// side) and spots overlooking our roadblocks there are scored (Score):
// - field of fire: how far a gunner sees towards the enemy, and to either side of that;
// - slight high ground over the ground round it (a hollow costs score);
// - no dead ground close in front, where attackers could gather unseen;
// - cover behind: a reverse slope or trees to fall back into (counted twice: the edge of open
//   ground with cover behind it is the best spot);
// - off paved ground: every point of its footprint on asphalt or concrete costs PAVED_COST, and a
//   spot on an airfield (by runway lights, runway decals or an apron's concrete panels: Arland's
//   runway edge is grass and its apron is panels the ground trace misses) counts as all paved
//   (pn10b, 26 Oct: both Combat Engineers positions went on Arland's airfield, its longest field
//   of fire);
// - not walled in: a structure taller than a man (WALL_HEIGHT) within WALL_REACH in its arc
//   (WALL_ARC either side of the facing) costs WALL_COST, and its sight ahead stops there (pn11,
//   27 Oct: the kit's Airfield position faced a hangar wall 20 m off while the rays, through the
//   hangar's open doors, said "sees 120 m");
// - not against a fence or a building (#93, playtest 29 Oct: sandbags dug in by a farm building and
//   a wooden fence, facing a field across the fence): a fence or wall (a Structures/Walls prefab)
//   higher than FENCE_HEIGHT within FENCE_REACH in a wider arc (FENCE_ARC), or within FENCE_HUG in
//   any direction, costs FENCE_COST; a building within BUILDING_CLEAR in any direction (the wall
//   check only sees the arc) costs BUILDING_COST;
// - its upgrades fit (tiers II and III go in later), as before;
// - it overlooks a roadblock or minefield of ours (covers it).
// Better siting never means fewer positions: the score only orders the candidates, every one
// clear of our fields is still tried, the old line spots included.
class ACR_PositionSite : Managed
{
 vector m_vPos;
 float m_fScore;
 string m_sSource;     // "fan", "roadblock" (overlooking ours) or "line" (the old spots)
 float m_fSight;       // m seen towards the enemy from a gunner's eye
 float m_fSides;       // m seen 25 degrees either side, together
 float m_fRise;        // m over the ground round it (negative: a hollow)
 int m_iDeadFront;     // of the spots 20-65 m in front, those the gunner can't see
 bool m_bCoverBehind;  // a reverse slope or trees behind
 int m_iPaved;         // of PAVED_SAMPLES points of its footprint on asphalt or concrete
 bool m_bAirfield;     // on an airfield's runway, taxiway or apron (OnAirfield): all paved
 float m_fWall = -1;   // m to a structure in its arc (WallAhead), -1 none
 string m_sWall;       // what it is
 float m_fFence = -1;  // m to a fence or wall in front or close by (FenceOrBuilding), -1 none
 string m_sFence;
 float m_fBuilding = -1; // m to a building on any side, -1 none
 string m_sBuilding;
 bool m_bFits;       // its upgrades would fit
 string m_sOverlooks;  // "roadblock", "minefield" or "" (what of ours it sees within OVERLOOK m)
}

class ACR_PositionSiting
{
 static const float EYE = 1.2;              // a gunner's eye in a dug-in position
 static const float SIGHT_MAX = 300;
 static const float SIGHT_STEP = 15;
 static const int FIT_CHECKED = 8;          // the best this many are checked for their upgrades' fit (costly)
 static const float OVERLOOK = 300;
 static const float SAME_SPOT = 15;
 static const int PAVED_SAMPLES = 5;          // its centre, and 10 m ahead, behind and to either side
 static const float PAVED_COST = 2;           // per paved sample: a spot fully on a runway scores 10 less
 static const float AIRFIELD_NEAR = 15;       // m: runway lights, runway decals or concrete panels this close
 static const float WALL_REACH = 30;          // m: a structure this close in the arc walls the spot in
 static const float WALL_ARC = 50;            // degrees either side of the facing
 static const float WALL_HEIGHT = 2.5;        // m over the spot's ground: taller hides what is behind it
 static const float WALL_COST = 8;
 static const float FENCE_REACH = 25;         // m: a fence this close in FENCE_ARC hems its field of fire in
 static const float FENCE_ARC = 70;           // degrees either side of the facing
 static const float FENCE_HUG = 6;            // m: a fence this close on any side: dug in against it
 static const float FENCE_HEIGHT = 0.9;       // m over the spot's ground: a gunner's fire clears lower ones
 static const float FENCE_COST = 5;
 static const float BUILDING_CLEAR = 15;      // m: a building this close on any side
 static const float BUILDING_SIZE = 4;        // m: wider and deeper than this (not a post, a lamp, a sign)
 static const float BUILDING_MAX = 80;        // m: but not a forest block or a whole terrain piece
 static const float BUILDING_COST = 5;
 static const float ROADBLOCK_TOP = 2.5;      // m: a roadblock is seen when a point this high over it is (its own logs or sandbags hide its ground)

 // Candidates round our front base at 'basePos' facing 'enemyPos' for 'squad' (its upgrades' fit
 // depends on how it would face), best first; 'nearFields' counts those within DEFENCE_STANDOFF of
 // our fields (left out).
 static void Candidates(ACR_Squad squad, string faction, vector basePos, vector enemyPos, notnull array<ref ACR_PositionSite> sites, out int nearFields)
 {
  sites.Clear();
  nearFields = 0;
  vector toEnemy = enemyPos - basePos;
  toEnemy[1] = 0;
  toEnemy.Normalize();
  vector side = Vector(toEnemy[2], 0, -toEnemy[0]);
  array<vector> points = {};
  array<string> sources = {};
  array<vector> fan = {};
  ACR_SiteScore.Fan(basePos, enemyPos, 80, 200, 40, 50, 16.67, fan);
  foreach (vector f : fan) Add(points, sources, f, "fan");
  // Behind and to either side of our roadblocks there, looking down their road.
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (!rb || rb.m_sFaction != faction || vector.DistanceXZ(rb.m_vPos, basePos) > 450) continue;
   array<float> offsets = {-60, 60};
   foreach (float offset : offsets)
   {
    Add(points, sources, ACR_Manager.Ground(rb.m_vPos - toEnemy * 50 + side * offset), "roadblock");
    Add(points, sources, ACR_Manager.Ground(rb.m_vPos + side * offset), "roadblock");
   }
  }
  // The old line spots.
  array<float> laterals = {0, -ACR_AICommander.DEFENCE_STANDOFF, ACR_AICommander.DEFENCE_STANDOFF};
  foreach (float lateral : laterals)
  {
   for (float reach = 80; reach <= 160; reach += 40) Add(points, sources, ACR_Manager.Ground(basePos + toEnemy * reach + side * lateral), "line");
  }

  foreach (int k, vector point : points)
  {
   if (ACR_Minefields.FieldNear(faction, point, ACR_AICommander.DEFENCE_STANDOFF))
   {
    nearFields++;
    continue;
   }
   sites.Insert(Describe(faction, point, sources[k], toEnemy));
  }
  // Then whether its upgrades fit, for the best FIT_CHECKED only.
  Sort(sites);
  for (int c = 0; c < sites.Count() && c < FIT_CHECKED; c++)
  {
   if (squad) sites[c].m_bFits = ACR_Fortify.UpgradesFit(sites[c].m_vPos, ACR_Fortify.FacingYaw(squad, sites[c].m_vPos, faction, enemyPos));
   sites[c].m_fScore = Score(sites[c]);
  }
  Sort(sites);
  // Dynamism (ACR_Variety): the first is picked among the near-best, the rest follow best first.
  // Keyed by the side, the base and how many positions stand, so two at one front can differ.
  array<float> scores = {};
  foreach (ACR_PositionSite scored : sites) scores.Insert(scored.m_fScore);
  int first = ACR_Variety.Pick(scores, string.Format("position %1 %2 %3 %4", faction, (int)basePos[0], (int)basePos[2], ACR_Fortify.s_Positions.Count()));
  if (first > 0)
  {
   ACR_PositionSite picked = sites[first];
   sites.RemoveOrdered(first);
   sites.InsertAt(picked, 0);
  }
 }

 // Best first; equal scores keep their order.
 protected static void Sort(notnull array<ref ACR_PositionSite> sites)
 {
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

 // A candidate scored on the ground round it (its upgrades' fit is added later, for the best).
 static ACR_PositionSite Describe(string faction, vector pos, string source, vector toEnemy)
 {
  ACR_PositionSite site = new ACR_PositionSite();
  site.m_vPos = pos;
  site.m_sSource = source;
  BaseWorld world = GetGame().GetWorld();
  vector eye = pos;
  eye[1] = world.GetSurfaceY(pos[0], pos[2]) + EYE;
  float bearing = Math.Atan2(toEnemy[0], toEnemy[2]) * Math.RAD2DEG;
  site.m_fSight = ACR_SiteScore.SightRay(pos, EYE, bearing, SIGHT_MAX, SIGHT_STEP);
  site.m_fSides = ACR_SiteScore.SightRay(pos, EYE, bearing - 25, SIGHT_MAX / 2, 30) + ACR_SiteScore.SightRay(pos, EYE, bearing + 25, SIGHT_MAX / 2, 30);
  site.m_fRise = ACR_SiteScore.Elevation(pos, 40);
  for (float near = 20; near <= 65; near += 15)
  {
   if (!ACR_SiteScore.Visible(eye, pos + toEnemy * near)) site.m_iDeadFront++;
  }
  vector behind = pos - toEnemy * 30;
  float drop = world.GetSurfaceY(pos[0], pos[2]) - world.GetSurfaceY(behind[0], behind[2]);
  site.m_bCoverBehind = drop > 1.5 || ACR_FobBuild.CountTrees(behind, 12) >= 3;
  site.m_iPaved = Paved(pos, toEnemy);
  site.m_bAirfield = OnAirfield(pos);
  if (site.m_bAirfield) site.m_iPaved = PAVED_SAMPLES;
  site.m_fWall = WallAhead(pos, bearing, site.m_sWall);
  if (site.m_fWall >= 0) site.m_fSight = Math.Min(site.m_fSight, site.m_fWall);
  FenceOrBuilding(pos, bearing, site);
  site.m_sOverlooks = Overlooks(faction, eye);
  site.m_fScore = Score(site);
  return site;
 }

 // What of ours the eye sees within OVERLOOK m: "roadblock", "minefield" or "".
 protected static string Overlooks(string faction, vector eye)
 {
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (rb && rb.m_sFaction == faction && vector.DistanceXZ(rb.m_vPos, eye) <= OVERLOOK && ACR_SiteScore.VisibleAt(eye, rb.m_vPos, ROADBLOCK_TOP)) return "roadblock";
  }
  foreach (ACR_Minefield field : ACR_Minefields.s_Fields)
  {
   if (field && field.m_sFaction == faction && vector.DistanceXZ(field.m_vPos, eye) <= OVERLOOK && ACR_SiteScore.Visible(eye, field.m_vPos)) return "minefield";
  }
  return "";
 }

 // How many of PAVED_SAMPLES points of a position at 'pos' lie on asphalt or concrete.
 static int Paved(vector pos, vector toEnemy)
 {
  vector side = Vector(toEnemy[2], 0, -toEnemy[0]);
  array<vector> samples = {pos, pos + toEnemy * 10, pos - toEnemy * 10, pos + side * 10, pos - side * 10};
  int paved;
  foreach (vector sample : samples)
  {
   if (ACR_Fortify.HardSurface(ACR_Manager.Ground(sample))) paved++;
  }
  return paved;
 }

 // The nearest structure taller than WALL_HEIGHT and wider than a post (not a tree) within WALL_REACH
 // in the arc round 'bearing': its footprint's nearest point (its world bounds), so an open door or
 // window in it doesn't count as a view. -1 for none; 'what' names it.
 protected static vector s_vWallFrom;
 protected static float s_fWallBearing;
 protected static float s_fWallNearest;
 protected static string s_sWallWhat;
 static float WallAhead(vector pos, float bearing, out string what)
 {
  s_vWallFrom = pos;
  s_vWallFrom[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
  s_fWallBearing = bearing;
  s_fWallNearest = -1;
  s_sWallWhat = "";
  GetGame().GetWorld().QueryEntitiesBySphere(pos, WALL_REACH + 25, CheckWall, null, EQueryEntitiesFlags.STATIC);
  what = s_sWallWhat;
  return s_fWallNearest;
 }

 protected static bool CheckWall(IEntity e)
 {
  if (!e || e.GetParent() || Tree.Cast(e)) return true;
  vector mins, maxs;
  e.GetWorldBounds(mins, maxs);
  if (maxs[1] - Math.Max(mins[1], s_vWallFrom[1]) < WALL_HEIGHT) return true;
  if (maxs[0] - mins[0] < 3 && maxs[2] - mins[2] < 3) return true;
  vector near = Vector(Math.Clamp(s_vWallFrom[0], mins[0], maxs[0]), 0, Math.Clamp(s_vWallFrom[2], mins[2], maxs[2]));
  float d = vector.DistanceXZ(near, s_vWallFrom);
  // Inside its bounds (the terrain, a forest block): not a wall in front.
  if (d < 1 || d > WALL_REACH) return true;
  float to = Math.Atan2(near[0] - s_vWallFrom[0], near[2] - s_vWallFrom[2]) * Math.RAD2DEG;
  float diff = Math.AbsFloat(to - s_fWallBearing);
  while (diff > 180) diff = Math.AbsFloat(diff - 360);
  if (diff > WALL_ARC) return true;
  if (s_fWallNearest >= 0 && d >= s_fWallNearest) return true;
  s_fWallNearest = d;
  s_sWallWhat = e.ClassName();
  if (e.GetPrefabData()) s_sWallWhat = FilePath.StripPath(e.GetPrefabData().GetPrefabName());
  return true;
 }

 // The nearest fence or wall hemming a spot at 'pos' facing 'bearing' in, and the nearest building on
 // any side (their footprints' nearest points), into 'site' (-1: none).
 protected static ref ACR_PositionSite s_FenceSite;
 static void FenceOrBuilding(vector pos, float bearing, ACR_PositionSite site)
 {
  site.m_fFence = -1;
  site.m_fBuilding = -1;
  s_FenceSite = site;
  s_vWallFrom = pos;
  s_vWallFrom[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
  s_fWallBearing = bearing;
  GetGame().GetWorld().QueryEntitiesBySphere(pos, FENCE_REACH + 25, CheckFenceOrBuilding, null, EQueryEntitiesFlags.STATIC);
  s_FenceSite = null;
 }

 protected static bool CheckFenceOrBuilding(IEntity e)
 {
  if (!e || e.GetParent() || Tree.Cast(e)) return true;
  vector mins, maxs;
  e.GetWorldBounds(mins, maxs);
  float high = maxs[1] - Math.Max(mins[1], s_vWallFrom[1]);
  if (high < FENCE_HEIGHT) return true;
  vector near = Vector(Math.Clamp(s_vWallFrom[0], mins[0], maxs[0]), 0, Math.Clamp(s_vWallFrom[2], mins[2], maxs[2]));
  float d = vector.DistanceXZ(near, s_vWallFrom);
  if (d < 1) return true; // inside its bounds: the terrain, a forest block
  string name = e.ClassName();
  if (e.GetPrefabData()) name = e.GetPrefabData().GetPrefabName();
  string lower = name;
  lower.ToLower();
  if (lower.Contains("structures/walls/"))
  {
   float to = Math.Atan2(near[0] - s_vWallFrom[0], near[2] - s_vWallFrom[2]) * Math.RAD2DEG;
   float diff = Math.AbsFloat(to - s_fWallBearing);
   while (diff > 180) diff = Math.AbsFloat(diff - 360);
   bool hems = (d <= FENCE_REACH && diff <= FENCE_ARC) || d <= FENCE_HUG;
   if (hems && (s_FenceSite.m_fFence < 0 || d < s_FenceSite.m_fFence))
   {
    s_FenceSite.m_fFence = d;
    s_FenceSite.m_sFence = FilePath.StripPath(name);
   }
   return true;
  }
  float wide = maxs[0] - mins[0];
  float deep = maxs[2] - mins[2];
  if (high < WALL_HEIGHT || wide < BUILDING_SIZE || deep < BUILDING_SIZE || wide > BUILDING_MAX || deep > BUILDING_MAX) return true;
  if (d > BUILDING_CLEAR) return true;
  if (s_FenceSite.m_fBuilding < 0 || d < s_FenceSite.m_fBuilding)
  {
   s_FenceSite.m_fBuilding = d;
   s_FenceSite.m_sBuilding = FilePath.StripPath(name);
  }
  return true;
 }

 // By an airfield's runway lights, runway decals or concrete panels (an apron, a taxiway).
 protected static bool s_bAirfield;
 static bool OnAirfield(vector pos)
 {
  s_bAirfield = false;
  GetGame().GetWorld().QueryEntitiesBySphere(pos, AIRFIELD_NEAR, FindAirfield, null, EQueryEntitiesFlags.STATIC);
  return s_bAirfield;
 }

 protected static bool FindAirfield(IEntity e)
 {
  if (!e || !e.GetPrefabData()) return true;
  string name = e.GetPrefabData().GetPrefabName();
  name.ToLower();
  if (name.Contains("lightrunway") || name.Contains("decalmodel_runway") || name.Contains("concretepanel")) s_bAirfield = true;
  return !s_bAirfield;
 }

 static float Score(ACR_PositionSite site)
 {
  float score = Math.Min(site.m_fSight / 50, 6) + Math.Min(site.m_fSides / 100, 3);
  score += Math.Clamp(site.m_fRise / 3, -2, 2);
  score -= site.m_iDeadFront * 0.75;
  score -= site.m_iPaved * PAVED_COST;
  if (site.m_fWall >= 0) score -= WALL_COST;
  if (site.m_fFence >= 0) score -= FENCE_COST;
  if (site.m_fBuilding >= 0) score -= BUILDING_COST;
  if (site.m_bCoverBehind) score += 2;
  if (site.m_bFits) score += 2;
  if (!site.m_sOverlooks.IsEmpty()) score += 2;
  return score;
 }

 // For the log: "score 9.2 (sees 240 m, 180 m to the sides, 2 m up, no dead ground in front, cover
 // behind, its upgrades fit, overlooks our roadblock)".
 static string Breakdown(ACR_PositionSite site)
 {
  string why = string.Format("sees %1 m, %2 m to the sides", (int)site.m_fSight, (int)site.m_fSides);
  if (site.m_fRise >= 0.5) why += string.Format(", %1 m up", (int)Math.Round(site.m_fRise));
  else if (site.m_fRise <= -0.5) why += string.Format(", in a hollow %1 m down", (int)Math.Round(-site.m_fRise));
  if (site.m_iDeadFront == 0) why += ", no dead ground in front";
  else why += string.Format(", dead ground at %1 of 4 spots in front", site.m_iDeadFront);
  if (site.m_bCoverBehind) why += ", cover behind";
  if (site.m_fWall >= 0) why += string.Format(", walled in by %1 %2 m in front", site.m_sWall, (int)site.m_fWall);
  if (site.m_fFence >= 0) why += string.Format(", hemmed in by %1 %2 m off", site.m_sFence, (int)site.m_fFence);
  if (site.m_fBuilding >= 0) why += string.Format(", %1 m from %2", (int)site.m_fBuilding, site.m_sBuilding);
  if (site.m_bAirfield) why += ", on an airfield";
  else if (site.m_iPaved > 0) why += string.Format(", on paved ground (%1 of %2 points)", site.m_iPaved, PAVED_SAMPLES);
  if (site.m_bFits) why += ", its upgrades fit";
  if (!site.m_sOverlooks.IsEmpty()) why += ", overlooks our " + site.m_sOverlooks;
  if (site.m_sSource == "roadblock") why += ", by our roadblock";
  else if (site.m_sSource == "line") why += ", an old line spot";
  return string.Format("score %1 (%2)", site.m_fScore, why);
 }
}
