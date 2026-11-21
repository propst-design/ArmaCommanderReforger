// Shared pieces for scoring where the AI commander builds: roadblocks now (ACR_RoadblockSiting),
// minefields and Fortify positions later. A fan of candidate points towards the enemy, sight lines
// (along a road or a bearing, from a given height), the road a point is on (how far to a junction
// or a dead end each way, a side road joining, how straight it runs, its width), and terrain terms
// (height over the surroundings, trees, chokepoints). Each user keeps its own weights and its own
// fallback ladder; scoring adds and subtracts, it never refuses a site the building rules allow.
class ACR_RoadInfo : Managed
{
 BaseRoad m_Road;
 vector m_vOnRoad;                      // the nearest point of the road
 float m_fYaw;                          // the way ahead (compass)
 ref array<vector> m_aAhead = {};       // the road walked ahead, from m_vOnRoad
 ref array<vector> m_aBehind = {};      // ... and behind
 ref array<BaseRoad> m_aChain = {};     // the pieces walked
 float m_fAhead;                        // m walked ahead before m_sAhead stopped it (WALK: open)
 string m_sAhead;                       // "junction", "dead end" or ""
 float m_fBehind;
 string m_sBehind;
 float m_fJoin;                         // m to the nearest side road joining (WALK: none)
 float m_fStraight;                     // m ahead before the road turns STRAIGHT_TURN
 float m_fWidth;

 // m to the nearest junction, dead end or side road, either way, and what it is.
 float NearestBreak(out string kind)
 {
  float best = ACR_SiteScore.WALK;
  kind = "";
  if (!m_sAhead.IsEmpty() && m_fAhead < best)
  {
   best = m_fAhead;
   kind = m_sAhead;
  }
  if (!m_sBehind.IsEmpty() && m_fBehind < best)
  {
   best = m_fBehind;
   kind = m_sBehind;
  }
  if (m_fJoin < best)
  {
   best = m_fJoin;
   kind = "side road joining";
  }
  return best;
 }

 // A point 'metres' along the road ahead (positive) or behind (negative); false past the walk.
 bool Along(float metres, out vector point)
 {
  array<vector> path = m_aAhead;
  if (metres < 0) path = m_aBehind;
  float want = Math.AbsFloat(metres);
  float walked;
  for (int i = 1; i < path.Count(); i++)
  {
   float len = vector.DistanceXZ(path[i - 1], path[i]);
   if (walked + len >= want)
   {
    point = path[i - 1] + (path[i] - path[i - 1]) * ((want - walked) / Math.Max(len, 0.01));
    return true;
   }
   walked += len;
  }
  return false;
 }
}

class ACR_SiteScore
{
 static const float CONNECT = 4;          // road pieces this close at their ends continue each other
 static const float STRAIGHT_TURN = 20;   // degrees
 static const float WALK = 400;           // m walked along a road each way
 static const float TALL = 1000;          // road queries' box height (pieces' bounds don't sit at the ground)
 static int s_iRoadsLooked;               // tests and the log

 static RoadNetworkManager Roads()
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld) return null;
  return aiWorld.GetRoadNetworkManager();
 }

 // ---------------------------------------------------------------- candidates

 // Points in a fan round 'towards' from 'origin': every 'step' m from nearM to farM, every
 // angleStep degrees out to halfAngle either side, on the ground.
 static void Fan(vector origin, vector towards, float nearM, float farM, float step, float halfAngle, float angleStep, notnull array<vector> samples)
 {
  vector dir = towards - origin;
  dir[1] = 0;
  if (dir.Length() < 1) dir = "0 0 1";
  dir.Normalize();
  float baseYaw = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
  BaseWorld world = GetGame().GetWorld();
  for (float turn = -halfAngle; turn <= halfAngle + 0.01; turn += angleStep)
  {
   float rad = (baseYaw + turn) * Math.DEG2RAD;
   vector way = Vector(Math.Sin(rad), 0, Math.Cos(rad));
   for (float reach = nearM; reach <= farM + 0.01; reach += step)
   {
    vector p = origin + way * reach;
    p[1] = world.GetSurfaceY(p[0], p[2]);
    samples.Insert(p);
   }
  }
 }

 // ---------------------------------------------------------------- sight

 // m seen along 'path' from 'from' at 'eye' m up: points every 'step' m, a man's height over
 // the ground, until the first one the eye can't see (people and vehicles don't block).
 static float SightAlongPath(vector from, float eye, notnull array<vector> path, float most, float step)
 {
  BaseWorld world = GetGame().GetWorld();
  vector eyePos = from;
  eyePos[1] = world.GetSurfaceY(from[0], from[2]) + eye;
  float along;
  float next = step;
  float seen;
  for (int i = 1; i < path.Count(); i++)
  {
   vector a = path[i - 1];
   vector b = path[i];
   float len = vector.DistanceXZ(a, b);
   while (next <= along + len && next <= most)
   {
    vector p = a + (b - a) * ((next - along) / Math.Max(len, 0.01));
    if (!Visible(eyePos, p)) return seen;
    seen = next;
    next += step;
   }
   along += len;
  }
  return seen;
 }

 // m seen along a compass bearing from 'from' at 'eye' m up (to a man's height over the ground).
 static float SightRay(vector from, float eye, float bearing, float most, float step)
 {
  BaseWorld world = GetGame().GetWorld();
  vector eyePos = from;
  eyePos[1] = world.GetSurfaceY(from[0], from[2]) + eye;
  float rad = bearing * Math.DEG2RAD;
  vector dir = Vector(Math.Sin(rad), 0, Math.Cos(rad));
  float seen;
  for (float d = step; d <= most + 0.01; d += step)
  {
   if (!Visible(eyePos, from + dir * d)) return seen;
   seen = d;
  }
  return seen;
 }

 // An eye at 'eyePos' sees a man's head (1 m over the ground) at 'ground'.
 static bool Visible(vector eyePos, vector ground)
 {
  return VisibleAt(eyePos, ground, 1.0);
 }

 // ... a point 'height' m over the ground at 'ground' (over a roadblock's own logs or sandbags, say,
 // which would otherwise hide the point inside them).
 static bool VisibleAt(vector eyePos, vector ground, float height)
 {
  BaseWorld world = GetGame().GetWorld();
  vector target = ground;
  target[1] = world.GetSurfaceY(ground[0], ground[2]) + height;
  TraceParam trace = new TraceParam();
  trace.Start = eyePos;
  trace.End = target;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  return world.TraceMove(trace, SightFilter) >= 1;
 }

 protected static bool SightFilter(notnull IEntity e)
 {
  return !ChimeraCharacter.Cast(e) && !Vehicle.Cast(e);
 }

 // ---------------------------------------------------------------- roads

 // The nearest point of a road piece's line to pos (XZ), its segment and distance.
 static float Nearest(BaseRoad road, vector pos, out int segment, out vector point)
 {
  array<vector> points = {};
  road.GetPoints(points);
  float best = float.MAX;
  segment = -1;
  for (int i = 0; i + 1 < points.Count(); i++)
  {
   vector a = points[i];
   vector seg = points[i + 1] - a;
   seg[1] = 0;
   float lenSq = seg.LengthSq();
   if (lenSq < 0.01) continue;
   vector v = pos - a;
   float t = Math.Clamp((v[0] * seg[0] + v[2] * seg[2]) / lenSq, 0, 1);
   vector p = a + seg * t;
   float d = vector.DistanceXZ(pos, p);
   if (d >= best) continue;
   best = d;
   segment = i;
   point = p;
  }
  return best;
 }

 // The pieces (other than 'road') touching the point 'end'.
 static void Touching(BaseRoad road, vector end, notnull array<BaseRoad> touching)
 {
  touching.Clear();
  RoadNetworkManager roads = Roads();
  if (!roads) return;
  array<BaseRoad> near = {};
  roads.GetRoadsInAABB(end - Vector(CONNECT + 2, TALL, CONNECT + 2), end + Vector(CONNECT + 2, TALL, CONNECT + 2), near);
  foreach (BaseRoad other : near)
  {
   if (!other || other == road) continue;
   int segment;
   vector point;
   if (Nearest(other, end, segment, point) <= CONNECT) touching.Insert(other);
  }
 }

 // An end where exactly one other piece starts or ends: the road just goes on.
 static bool Continues(BaseRoad road, vector end, out BaseRoad next)
 {
  next = null;
  array<BaseRoad> touching = {};
  Touching(road, end, touching);
  if (touching.Count() != 1) return false;
  array<vector> points = {};
  touching[0].GetPoints(points);
  if (points.IsEmpty()) return false;
  if (vector.DistanceXZ(points[0], end) > CONNECT && vector.DistanceXZ(points[points.Count() - 1], end) > CONNECT) return false;
  next = touching[0];
  return true;
 }

 // Walk the road from 'start' (on 'road', segment 'segment') heading 'dir' (+1 with the points,
 // -1 against), through plain continuations, up to 'most' m. 'path' gets the points passed,
 // 'chain' the pieces. Returns the m walked; 'stop' says why it ended ("" when it went the
 // whole way): "junction" (a fork or a T) or "dead end".
 static float Walk(BaseRoad road, int segment, vector start, int dir, float most, notnull array<vector> path, out string stop, notnull array<BaseRoad> chain)
 {
  stop = "";
  path.Insert(start);
  float walked;
  vector at = start;
  int hops;
  while (road && hops < 12)
  {
   hops++;
   if (!chain.Contains(road)) chain.Insert(road);
   array<vector> points = {};
   road.GetPoints(points);
   int i = segment + 1;
   if (dir < 0) i = segment;
   while (i >= 0 && i < points.Count())
   {
    vector next = points[i];
    float step = vector.DistanceXZ(at, next);
    if (walked + step >= most)
    {
     path.Insert(at + (next - at) * ((most - walked) / Math.Max(step, 0.01)));
     return most;
    }
    walked += step;
    at = next;
    path.Insert(at);
    i += dir;
   }
   BaseRoad following;
   if (!Continues(road, at, following))
   {
    array<BaseRoad> touching = {};
    Touching(road, at, touching);
    stop = "junction";
    if (touching.IsEmpty()) stop = "dead end";
    return walked;
   }
   array<vector> nextPoints = {};
   following.GetPoints(nextPoints);
   road = following;
   if (vector.DistanceXZ(nextPoints[0], at) <= CONNECT)
   {
    dir = 1;
    segment = 0;
   }
   else
   {
    dir = -1;
    segment = nextPoints.Count() - 2;
   }
  }
  return walked;
 }

 // The road at 'pos', walked WALK m each way; 'facing' (compass) picks which way is ahead.
 // null when there is no road piece near.
 static ACR_RoadInfo Road(vector pos, float facing)
 {
  RoadNetworkManager roads = Roads();
  if (!roads) return null;
  // Real road pieces only (navlinks are the network's invisible junction links).
  BaseRoad road;
  float distance;
  roads.GetClosestRoad(pos, road, distance, true);
  if (!road) return null;
  s_iRoadsLooked++;
  ACR_RoadInfo info = new ACR_RoadInfo();
  info.m_Road = road;
  int segment;
  Nearest(road, pos, segment, info.m_vOnRoad);
  array<vector> points = {};
  road.GetPoints(points);
  if (segment < 0 || segment + 1 >= points.Count()) return null;
  float rad = facing * Math.DEG2RAD;
  vector want = Vector(Math.Sin(rad), 0, Math.Cos(rad));
  vector along = points[segment + 1] - points[segment];
  along[1] = 0;
  int ahead = 1;
  float sign = 1;
  if (vector.Dot(along, want) < 0)
  {
   ahead = -1;
   sign = -1;
  }
  vector heading = along * sign;
  info.m_fYaw = Math.Atan2(heading[0], heading[2]) * Math.RAD2DEG;
  info.m_fAhead = Walk(road, segment, info.m_vOnRoad, ahead, WALK, info.m_aAhead, info.m_sAhead, info.m_aChain);
  info.m_fBehind = Walk(road, segment, info.m_vOnRoad, -ahead, WALK, info.m_aBehind, info.m_sBehind, info.m_aChain);
  info.m_fJoin = JoinNear(info.m_vOnRoad, info.m_aChain, info.m_aAhead, info.m_aBehind, 60);
  info.m_fStraight = StraightAlong(info.m_aAhead);
  info.m_fWidth = road.GetWidth();
  return info;
 }

 // m from pos to the nearest end of another road (not one of 'chain', the pieces walked) that
 // touches the road walked, within 'reach' (WALK when none): a side road joining part way along
 // a piece (a T on the through road).
 static float JoinNear(vector pos, notnull array<BaseRoad> chain, notnull array<vector> ahead, notnull array<vector> behind, float reach)
 {
  RoadNetworkManager roads = Roads();
  if (!roads) return WALK;
  array<BaseRoad> near = {};
  roads.GetRoadsInAABB(pos - Vector(reach, TALL, reach), pos + Vector(reach, TALL, reach), near);
  float best = WALK;
  foreach (BaseRoad other : near)
  {
   if (!other || chain.Contains(other)) continue;
   array<vector> points = {};
   other.GetPoints(points);
   if (points.Count() < 2) continue;
   array<vector> ends = {points[0], points[points.Count() - 1]};
   foreach (vector end : ends)
   {
    float d = vector.DistanceXZ(end, pos);
    if (d > reach || d >= best) continue;
    if (OnPath(end, ahead) <= CONNECT || OnPath(end, behind) <= CONNECT) best = d;
   }
  }
  return best;
 }

 static float OnPath(vector p, notnull array<vector> path)
 {
  float best = float.MAX;
  for (int i = 1; i < path.Count(); i++) best = Math.Min(best, DistanceToSegment(p, path[i - 1], path[i]));
  return best;
 }

 static float DistanceToSegment(vector p, vector a, vector b)
 {
  vector seg = b - a;
  seg[1] = 0;
  float lenSq = seg.LengthSq();
  if (lenSq < 0.01) return vector.DistanceXZ(p, a);
  vector v = p - a;
  float t = Math.Clamp((v[0] * seg[0] + v[2] * seg[2]) / lenSq, 0, 1);
  return vector.DistanceXZ(p, a + seg * t);
 }

 // m along 'path' (from its start) until its heading turns more than STRAIGHT_TURN from the first.
 static float StraightAlong(notnull array<vector> path)
 {
  if (path.Count() < 2) return 0;
  vector first = path[1] - path[0];
  first[1] = 0;
  float along;
  for (int i = 1; i < path.Count(); i++)
  {
   vector seg = path[i] - path[i - 1];
   seg[1] = 0;
   float len = seg.Length();
   if (len < 0.5) continue;
   if (first.Length() < 0.5) first = seg;
   if (vector.Dot(seg.Normalized(), first.Normalized()) < Math.Cos(STRAIGHT_TURN * Math.DEG2RAD)) return along;
   along += len;
  }
  return along;
 }

 // ---------------------------------------------------------------- terrain

 // m a point stands above (positive) or below the mean ground 'radius' m round it.
 static float Elevation(vector pos, float radius)
 {
  BaseWorld world = GetGame().GetWorld();
  float here = world.GetSurfaceY(pos[0], pos[2]);
  float sum;
  for (int i = 0; i < 8; i++)
  {
   float a = i * 45 * Math.DEG2RAD;
   sum += world.GetSurfaceY(pos[0] + Math.Cos(a) * radius, pos[2] + Math.Sin(a) * radius);
  }
  return here - sum / 8;
 }

 // Chokepoint terms across a road at 'pos' heading 'yaw': "bridge" (water either side),
 // "cutting" / "embankment" (ground well above / below on both sides), "woods" (trees on both
 // sides), "town" (buildings round it); comma separated.
 static string Choke(vector pos, float yaw)
 {
  BaseWorld world = GetGame().GetWorld();
  float rad = yaw * Math.DEG2RAD;
  vector side = Vector(Math.Cos(rad), 0, -Math.Sin(rad));
  float here = world.GetSurfaceY(pos[0], pos[2]);
  string kinds;
  if (BBB_Terrain.IsWaterAt(pos + side * 12) || BBB_Terrain.IsWaterAt(pos - side * 12)) kinds = Add(kinds, "bridge");
  vector left = pos + side * 15;
  vector right = pos - side * 15;
  float l = world.GetSurfaceY(left[0], left[2]) - here;
  float r = world.GetSurfaceY(right[0], right[2]) - here;
  if (l > 2.5 && r > 2.5) kinds = Add(kinds, "cutting");
  else if (l < -2.5 && r < -2.5) kinds = Add(kinds, "embankment");
  if (ACR_FobBuild.CountTrees(pos + side * 18, 10) >= 4 && ACR_FobBuild.CountTrees(pos - side * 18, 10) >= 4) kinds = Add(kinds, "woods");
  if (Buildings(pos, 40) >= 4) kinds = Add(kinds, "town");
  return kinds;
 }

 static string Add(string kinds, string kind)
 {
  if (kinds.IsEmpty()) return kind;
  return kinds + "," + kind;
 }

 protected static int s_iBuildings;
 static int Buildings(vector pos, float radius)
 {
  s_iBuildings = 0;
  GetGame().GetWorld().QueryEntitiesBySphere(pos, radius, CountBuilding, null, EQueryEntitiesFlags.STATIC);
  return s_iBuildings;
 }

 // The map's houses: vanilla keeps them under Prefabs/Structures/Houses as SCR_DestructibleBuildingEntity
 // (as ACR_Generator counts them, a57e495); no vanilla prefab path has "/Buildings/", so that test alone
 // found no town anywhere (FOB, roadblock and mine siting never saw one). Mod paths still count.
 protected static bool CountBuilding(IEntity e)
 {
  if (!e || e.GetParent()) return true;
  if (SCR_DestructibleBuildingEntity.Cast(e))
  {
   s_iBuildings++;
   return true;
  }
  if (!e.GetPrefabData()) return true;
  string name = e.GetPrefabData().GetPrefabName();
  if ((name.Contains("/Buildings/") || name.Contains("/Houses/")) && !name.Contains("/Walls/") && !name.Contains("/Fences/")) s_iBuildings++;
  return true;
 }
}
