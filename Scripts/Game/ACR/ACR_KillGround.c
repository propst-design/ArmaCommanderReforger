// Where the enemy will really walk, and what a place to fight from does to him (the user, 2 Nov: "it just needs to
// make sense and give the defenders the advantage"). Sectors and "how far does it see" say nothing about that: a
// spot can look a long way over ground nobody crosses, and a road can come in behind it. So it's done the way a
// company commander lays out a defence (engagement area development, ATP 3-21.8), in this order:
//   1. Walk him in. From every way he can come (more where the threats are, the roads as well) the cheapest walk to
//      the middle of the base, over the real ground: houses, stone, water and cliffs stop him, fences slow him.
//   2. Where he can be killed: the stretches of those ways that can be seen from where a man can stand. A place is
//      worth what it does to him there, more when it hits him from the side than head on, and more when something
//      already fires on that ground from another angle (crossfire), but little once enough does.
//   3. The ways least shot at count most (a soft minimum): he takes the softest way in, so that's the one to harden.
//      A place that leaves a way he comes by unseen close to it, in front or behind, loses for that: he gets to
//      grenade range untouched.
//   4. Walk him in again. What's built makes the ground it covers dear to him and he goes round it: then that's
//      where the next goes. Every plan is judged by the attacker's best answer to it, not by its own ideas.
// A seeded low-frequency luck on the ground gives him a different walk each match, so a town's defence differs
// too (ACR_Variety's rule: the choices stay among the good ones).
// -acrKillGround 0: the old way of valuing places (threat by sector, how far it sees).
class ACR_KillWay : Managed
{
 ref array<int> m_aCells = {};  // from where he comes to the middle
 float m_fWeight;               // how likely he comes this way
 float m_fFire;                 // metres of it under fire, now
 float m_fShare;                // how much it counts: the least shot at the most
 bool m_bRoad;
}

// Something that shoots, as the ground sees it.
class ACR_KillSite : Managed
{
 vector m_vEye;
 float m_fFacing;              // compass
 float m_fHalfArc;             // degrees either side
 float m_fRange;               // m
 float m_fPower;               // about 1: what a stretch of bags with its men does at its best
 float m_fFacingX, m_fFacingZ; // the same as a direction
 float m_fCos;                 // ... and its arc
 int m_iLooked = -1;           // the walk it was last aimed at
 ref array<int> m_aSees = {};  // cells on his ways it fires on
 ref array<float> m_aHit = {}; // ... how hard
 ref array<float> m_aDX = {};  // ... the line of fire there
 ref array<float> m_aDZ = {};
 ref array<int> m_aClose = {}; // cells on his ways close by that it doesn't see
 ref array<int> m_aRound = {}; // cells off his ways it fires on, once it's placed (a few, standing for those round them)
 ref array<float> m_aRoundX = {};
 ref array<float> m_aRoundZ = {};
 ref map<int, bool> m_mLOS = new map<int, bool>();
}

class ACR_KillGround : Managed
{
 static const float MOST_SIDE = 48;     // cells a side, at most
 static const float CELL_LEAST = 12;    // m
 static const float PAST_TOWN = 120;    // m past a town's search radius he's walked from
 static const int ENTRIES = 24;         // ways he can come from, round the ground
 static const float WAY_FLOOR = 0.03;   // ... the side away from every threat still counts a little
 static const float STAGE_SIDE = 0.75;  // a squad on a line turned 60 degrees off his main approach, against one on it
 static const float STAGE_WIDE = 0.5;   // ... 115 degrees: the mounted ones, swinging wide
 static const float STAGE_HIDE_LEAST = 150; // m past the base's ring a staging point moved out of its sight stays (ACR_AICommander.HiddenStaging)
 static const float ROAD_WAY = 1.8;     // a road in, against the fields beside it: vehicles have no other way
 static const float NOISE = 0.15;       // the match's luck on the ground
 static const float ROAD_WALK = 0.8;    // what a metre costs him on a road, against open ground, near the base
 static const float ROADS_NEAR_BASE = 150; // m from the middle he may use them (ACR_Squad.FIELD_APPROACH)
 static const float OFF_ROADS = 1.6;    // ... further out he keeps away from them
 static const float STONE_WALK = 7;     // a wall to get round (he finds the gate, or a hole)
 static const float BOARD_WALK = 5;
 static const float WIRE_WALK = 2.5;
 static const float STEEP = 0.75;       // rise per metre he won't climb
 static const float CLIMB = 3;          // what rise per metre costs him
 static const float BAGS_WALK = 2;      // bags in the way: he goes round if it's near as short
 static const float FIRE_COST = 1.6;    // a cell under full fire costs him this much more per metre
 static const float SOFT = 2.5;         // how hard the softest way draws the next place
 static const float SOFT_SPAN = 80;     // over this many metres under fire
 static const float SOFT_LEAST = 0.3;   // what a well covered way still counts for: a way that can't be covered doesn't take it all
 static const float SATURATE = 1.6;     // fire on a cell past which more does little
 static const float CROSS = 0.5;        // a second line of fire across the first, at best
 static const float FLANK_LEAST = 0.8;  // head on, against ...
 static const float FLANK_MOST = 1.2;   // ... from the side
 static const float NEAR_FULL = 60;     // m up to which a man hits him as well as he can
 static const float FAR_HIT = 0.25;     // ... and at the end of his range
 static const float NEAREST = 8;        // m: closer than this isn't a line of fire
 static const float FAR_LOOK = 70;      // m: past this his ways are looked at more thinly
 static const float CLOSE = 45;         // m: he gets this close, unseen: he's in grenade range
 static const float CLOSE_COST = 0.45;  // what that costs, against being fired on
 static const float INSIDE = 0.3;       // ground inside the town counts for this: kill him before he's in it
 static const float SUPPORT = 110;      // m: a place with no friend this close stands alone
 static const float LONE = 0.8;
 static const float MAN_HEIGHT = 1;     // m over the ground he's aimed at
 static const float AT_THE_MIDDLE = 40; // m from the middle: too late to be counted as stopping him
 // What things are.
 static const float BAG_ARC = 70;
 static const float BAG_RANGE = 150;
 static const float GUN_ARC = 50;
 static const float GUN_RANGE = 280;
 static const float GUN_POWER = 1.7;
 static const float POST_POWER = 2;

 static int s_iLastMs, s_iLastSolves, s_iLastTraces;

 static bool On() { return ACR_LogisticsRules.Choice("acrKillGround", "1") != "0"; }

 protected vector m_vCenter;
 protected float m_fCell;
 protected float m_fHalf;
 protected int m_iN;
 protected float m_fX, m_fZ;
 protected ref array<float> m_aY = {};
 protected ref array<float> m_aWalk = {};     // what a metre costs him, -1 no way
 protected ref array<bool> m_aRoad = {};
 protected ref array<bool> m_aInside = {};
 protected ref array<float> m_aFire = {};
 protected ref array<float> m_aFireX = {};
 protected ref array<float> m_aFireZ = {};
 protected ref array<float> m_aTraffic = {};
 protected ref array<float> m_aWayX = {};     // which way he goes there
 protected ref array<float> m_aWayZ = {};
 protected ref array<int> m_aOn = {};         // the cells on a way
 protected ref array<int> m_aEntries = {};
 protected ref array<float> m_aEntryWeights = {};
 protected ref array<int> m_aRoadEntries = {};
 protected ref array<float> m_aRoadWeights = {};
 protected ref array<int> m_aNext = {};
 protected ref array<int> m_aNextRoad = {};
 protected ref array<int> m_aNextPlain = {};
 protected int m_iHeart = -1;
 protected int m_iRoadHeart = -1;
 protected float m_fBaseRadius;               // > 0: a base's own ring, which his attack stages outside
 ref array<vector> m_aStagePoints = {};       // where he stages against it, as planned on (ACR_BaseWorks.StagingWatch)
 protected int m_iLook;
 ref array<ref ACR_KillWay> m_aWays = {};
 ref array<ref ACR_KillSite> m_aPlaced = {};
 protected int m_iSolves;
 protected int m_iStarted;
 protected int m_iTraces;
 protected ref array<int> m_aDX = {1, -1, 0, 0, 1, 1, -1, -1};
 protected ref array<int> m_aDZ = {0, 0, 1, -1, 1, -1, 1, -1};
 protected ref array<float> m_aStep = {1, 1, 1, 1, 1.414, 1.414, 1.414, 1.414};
 protected static ACR_KillGround s_Filling;

 // ---------------------------------------------------------------- the ground

 protected int At(float x, float z)
 {
  if (x < m_fX || z < m_fZ) return -1;
  int ix = (x - m_fX) / m_fCell;
  int iz = (z - m_fZ) / m_fCell;
  if (ix >= m_iN || iz >= m_iN) return -1;
  return iz * m_iN + ix;
 }

 vector Middle(int i)
 {
  int iz = i / m_iN;
  int ix = i - iz * m_iN;
  return Vector(m_fX + (ix + 0.5) * m_fCell, m_aY[i], m_fZ + (iz + 0.5) * m_fCell);
 }

 protected void HouseAt(IEntity b)
 {
  vector mins, maxs;
  b.GetBounds(mins, maxs);
  vector c1 = b.CoordToParent(Vector(mins[0], 0, mins[2]));
  vector c2 = b.CoordToParent(Vector(maxs[0], 0, mins[2]));
  vector c4 = b.CoordToParent(Vector(mins[0], 0, maxs[2]));
  vector c3 = c2 + c4 - c1;
  float ux = c2[0] - c1[0];
  float uz = c2[2] - c1[2];
  float vx = c4[0] - c1[0];
  float vz = c4[2] - c1[2];
  float uu = Math.Max(ux * ux + uz * uz, 0.01);
  float vv = Math.Max(vx * vx + vz * vz, 0.01);
  float loX = Math.Min(Math.Min(c1[0], c2[0]), Math.Min(c3[0], c4[0]));
  float hiX = Math.Max(Math.Max(c1[0], c2[0]), Math.Max(c3[0], c4[0]));
  float loZ = Math.Min(Math.Min(c1[2], c2[2]), Math.Min(c3[2], c4[2]));
  float hiZ = Math.Max(Math.Max(c1[2], c2[2]), Math.Max(c3[2], c4[2]));
  int fromX = Math.Max((loX - m_fX) / m_fCell, 0);
  int toX = Math.Min((hiX - m_fX) / m_fCell, m_iN - 1);
  int fromZ = Math.Max((loZ - m_fZ) / m_fCell, 0);
  int toZ = Math.Min((hiZ - m_fZ) / m_fCell, m_iN - 1);
  float apart = m_fCell * 0.3;
  for (int iz = fromZ; iz <= toZ; iz++)
  {
   for (int ix = fromX; ix <= toX; ix++)
   {
    float cx = m_fX + (ix + 0.5) * m_fCell;
    float cz = m_fZ + (iz + 0.5) * m_fCell;
    int hits;
    for (int sample = 0; sample < 5; sample++)
    {
     float sx = cx;
     float sz = cz;
     if (sample == 1 || sample == 2) sx -= apart;
     if (sample == 3 || sample == 4) sx += apart;
     if (sample == 1 || sample == 3) sz -= apart;
     if (sample == 2 || sample == 4) sz += apart;
     float dx = sx - c1[0];
     float dz = sz - c1[2];
     float along = (dx * ux + dz * uz) / uu;
     float across = (dx * vx + dz * vz) / vv;
     if (along >= 0 && along <= 1 && across >= 0 && across <= 1) hits++;
    }
    if (hits >= 3) m_aWalk[iz * m_iN + ix] = -1;
   }
  }
 }

 protected void Along(vector a, vector b, float walk, bool road)
 {
  float length = vector.DistanceXZ(a, b);
  int steps = length / (m_fCell * 0.5);
  steps++;
  for (int s = 0; s <= steps; s++)
  {
   float part = s * 1.0 / steps;
   int i = At(a[0] + (b[0] - a[0]) * part, a[2] + (b[2] - a[2]) * part);
   if (i < 0) continue;
   if (road) m_aRoad[i] = true;
   else if (m_aWalk[i] >= 0) m_aWalk[i] = Math.Max(m_aWalk[i], walk);
  }
 }

 protected static bool CollectWalls(IEntity e)
 {
  if (!e || Tree.Cast(e) || !e.GetPhysics() || ChimeraCharacter.Cast(e)) return true;
  string name = ACR_BaseSurvey.FullName(e);
  if (!name.Contains("/Walls/") && !name.Contains("Fence")) return true;
  int kind = ACR_BaseRing.WallKind(name);
  if (kind == 0) return true;
  vector mins, maxs;
  e.GetBounds(mins, maxs);
  vector size = maxs - mins;
  if (size[1] < 1) return true;
  vector a = Vector(mins[0], 0, 0.5 * (mins[2] + maxs[2]));
  vector b = Vector(maxs[0], 0, 0.5 * (mins[2] + maxs[2]));
  if (size[2] > size[0])
  {
   a = Vector(0.5 * (mins[0] + maxs[0]), 0, mins[2]);
   b = Vector(0.5 * (mins[0] + maxs[0]), 0, maxs[2]);
  }
  float walk = WIRE_WALK;
  if (kind == ACR_BaseRing.STONE) walk = STONE_WALK;
  else if (kind == ACR_BaseRing.BOARD) walk = BOARD_WALK;
  s_Filling.Along(e.CoordToParent(a), e.CoordToParent(b), walk, false);
  return true;
 }

 // The ground round 'center', out to 'half' m. 'inTown' (may be null): the survey whose town counts for less.
 protected void Build(vector center, float half, notnull array<ref ACR_BaseDefenceSeg> segs, notnull array<IEntity> houses, ACR_BaseSurvey inTown, string key)
 {
  m_iStarted = System.GetTickCount();
  m_vCenter = center;
  m_fHalf = half;
  m_fCell = Math.Max(CELL_LEAST, half * 2 / MOST_SIDE);
  m_iN = half * 2 / m_fCell;
  m_fX = center[0] - half;
  m_fZ = center[2] - half;
  int count = m_iN * m_iN;
  BaseWorld world = GetGame().GetWorld();
  for (int iz = 0; iz < m_iN; iz++)
  {
   float z = m_fZ + (iz + 0.5) * m_fCell;
   for (int ix = 0; ix < m_iN; ix++)
   {
    float x = m_fX + (ix + 0.5) * m_fCell;
    m_aY.Insert(world.GetSurfaceY(x, z));
    float walk = 1;
    if (BBB_Terrain.IsWater(x, z)) walk = -1;
    m_aWalk.Insert(walk);
    m_aRoad.Insert(false);
    m_aFire.Insert(0);
    m_aFireX.Insert(0);
    m_aFireZ.Insert(0);
    m_aTraffic.Insert(0);
    m_aWayX.Insert(0);
    m_aWayZ.Insert(0);
    m_aNext.Insert(-1);
    m_aNextRoad.Insert(-1);
    bool inside = false;
    if (inTown)
    {
     float toX = x - center[0];
     float toZ = z - center[2];
     float apart = Math.Sqrt(toX * toX + toZ * toZ);
     inside = apart < inTown.EdgeSmooth(ACR_BaseDefences.Bearing(Vector(toX, 0, toZ)));
    }
    m_aInside.Insert(inside);
   }
  }
  foreach (ACR_BaseDefenceSeg seg : segs) Along(seg.m_vA, seg.m_vB, 0, true);
  s_Filling = this;
  world.QueryEntitiesBySphere(center, half * 1.2, CollectWalls, null, EQueryEntitiesFlags.STATIC);
  s_Filling = null;
  foreach (IEntity house : houses) if (house) HouseAt(house);
  // The roads, the slopes, the luck.
  RandomGenerator rng = ACR_Variety.For(key);
  int coarse = 5;
  int gridN = m_iN / coarse + 2;
  array<float> luck = {};
  for (int g = 0; g < gridN * gridN; g++) luck.Insert(1 - NOISE + 2 * NOISE * rng.RandFloat01());
  bool fieldRoutes = ACR_LogisticsRules.Choice("acrFieldRoutes", "1") != "0";
  for (int gz = 0; gz < m_iN; gz++)
  {
   for (int gx = 0; gx < m_iN; gx++)
   {
    int i = gz * m_iN + gx;
    float c = m_aWalk[i];
    if (c < 0) continue;
    // Men on foot keep off the roads outside friendly ground, up to the last stretch before a base (ACR_Squad.FieldLegs,
    // #160). A road's for vehicles there: they have the ways on the roads.
    if (fieldRoutes)
    {
     vector here = Middle(i);
     bool close = vector.DistanceXZ(here, center) <= ROADS_NEAR_BASE;
     if (m_aRoad[i] && close) c = Math.Min(c, ROAD_WALK);
     else if (!close && NearRoad(gx, gz)) c *= OFF_ROADS;
    }
    else if (m_aRoad[i]) c = Math.Min(c, ROAD_WALK);
    if (gx > 0 && gz > 0 && gx < m_iN - 1 && gz < m_iN - 1)
    {
     float sx = (m_aY[i + 1] - m_aY[i - 1]) / (2 * m_fCell);
     float sz = (m_aY[i + m_iN] - m_aY[i - m_iN]) / (2 * m_fCell);
     float slope = Math.Sqrt(sx * sx + sz * sz);
     if (slope > STEEP && !m_aRoad[i])
     {
      m_aWalk[i] = -1;
      continue;
     }
     c += slope * CLIMB;
    }
    float fx = gx * 1.0 / coarse;
    float fz = gz * 1.0 / coarse;
    int ax = fx;
    int az = fz;
    float tx = fx - ax;
    float tz = fz - az;
    float top = luck[az * gridN + ax] * (1 - tx) + luck[az * gridN + ax + 1] * tx;
    float bottom = luck[(az + 1) * gridN + ax] * (1 - tx) + luck[(az + 1) * gridN + ax + 1] * tx;
    m_aWalk[i] = c * (top * (1 - tz) + bottom * tz);
   }
  }
  // Where he's going: the free cell nearest the middle.
  m_iHeart = NearestFree(center, false);
  m_iRoadHeart = NearestFree(center, true);
 }

 // A road on this cell or the next (within about a man's keeping-off distance on this grid).
 protected bool NearRoad(int ix, int iz)
 {
  for (int dz = -2; dz <= 2; dz++)
  {
   for (int dx = -2; dx <= 2; dx++)
   {
    int x = ix + dx;
    int z = iz + dz;
    if (x < 0 || z < 0 || x >= m_iN || z >= m_iN) continue;
    if (m_aRoad[z * m_iN + x]) return true;
   }
  }
  return false;
 }

 protected int NearestFree(vector p, bool road)
 {
  int best = -1;
  float bestApart = float.MAX;
  int count = m_iN * m_iN;
  for (int i = 0; i < count; i++)
  {
   if (m_aWalk[i] < 0 || (road && !m_aRoad[i])) continue;
   vector m = Middle(i);
   float apart = vector.DistanceXZ(m, p);
   if (apart >= bestApart) continue;
   bestApart = apart;
   best = i;
  }
  return best;
 }

 // Where he comes from. Against a base, the AI commander stages its attack ACR_AICommander.STAGING_DISTANCE past
 // the base's ring, on up to five lines turned 0, 60 and 115 degrees either way off the way from his side to it,
 // each moved to where the base's ground can't see it, and walks in from there (LaunchAttack, PickApproach,
 // HiddenStaging; the user, 2 Nov, Chotain Base: the attack came down the road and out of the woods his way,
 // not round the compass). So his ways start where those lines meet this ground, weighted as he uses them; the
 // whole way round still counts a little (WAY_FLOOR), and the roads come in where they do. A front with no base
 // (ForFront) keeps the threats round the edge.
 protected void Entries(array<vector> dirs, array<float> weights, bool even)
 {
  m_aEntries.Clear();
  m_aEntryWeights.Clear();
  m_aRoadEntries.Clear();
  m_aRoadWeights.Clear();
  float reach = m_fHalf - m_fCell;
  bool staged = !even && m_fBaseRadius > 0;
  for (int e = 0; e < ENTRIES; e++)
  {
   vector way = ACR_BaseSurvey.Dir(e * 360.0 / ENTRIES);
   int cell = EdgeCell(way, reach);
   if (cell < 0) continue;
   m_aEntries.Insert(cell);
   if (staged) m_aEntryWeights.Insert(WAY_FLOOR);
   else m_aEntryWeights.Insert(WeightFor(dirs, weights, way, even));
  }
  if (staged) Staging(dirs, weights, reach);
  // A road coming in: cells on a road at the edge, one per way in.
  int count = m_iN * m_iN;
  array<float> seen = {};
  for (int i = 0; i < count; i++)
  {
   if (!m_aRoad[i] || m_aWalk[i] < 0) continue;
   vector at = Middle(i);
   float apart = vector.DistanceXZ(at, m_vCenter);
   if (apart < reach - m_fCell) continue;
   float bearing = ACR_BaseDefences.Bearing(at - m_vCenter);
   bool known = false;
   foreach (float other : seen) if (Off(other, bearing) < 12) known = true;
   if (known) continue;
   seen.Insert(bearing);
   m_aRoadEntries.Insert(i);
   m_aRoadWeights.Insert(WeightFor(dirs, weights, ACR_BaseSurvey.Dir(bearing), even) * ROAD_WAY);
  }
 }

 // The first cell he can walk, in from the edge along 'way'.
 protected int EdgeCell(vector way, float reach)
 {
  float back = reach;
  while (back > m_fHalf * 0.5)
  {
   vector p = m_vCenter + way * back;
   int cell = At(p[0], p[2]);
   if (cell >= 0 && m_aWalk[cell] >= 0) return cell;
   back -= m_fCell;
  }
  return -1;
 }

 // His staging points against this base, as ACR_AICommander picks them, and an entry where the way in from each
 // meets the ground. A threat's weight is the way from his side; the line's share is how he uses it (foot squads
 // take the direct lines, mounted ones swing wide).
 protected void Staging(array<vector> dirs, array<float> weights, float reach)
 {
  float ring = m_fBaseRadius + ACR_AICommander.STAGING_DISTANCE;
  array<vector> eyes = {};
  ACR_AICommander.ObjectiveEyes(m_vCenter, m_fBaseRadius, eyes);
  foreach (int d, vector dir : dirs)
  {
   foreach (float turn : ACR_AICommander.s_aApproachTurns)
   {
    float share = 1;
    if (Math.AbsFloat(turn) > 90) share = STAGE_WIDE;
    else if (turn != 0) share = STAGE_SIDE;
    vector point;
    if (!BBB_Terrain.DryApproach(m_vCenter, ACR_AICommander.Turned(dir, turn), ring, point)) continue;
    point = Hidden(point, eyes);
    m_aStagePoints.Insert(point);
    int cell = EdgeCell(ACR_BaseDefences.FlatDir(m_vCenter, point), reach);
    if (cell < 0) continue;
    float weight = weights[d] * share;
    // One entry to a cell: the heavier use of it.
    int have = m_aEntries.Find(cell);
    if (have >= 0)
    {
     if (m_aEntryWeights[have] < weight) m_aEntryWeights[have] = weight;
     continue;
    }
    m_aEntries.Insert(cell);
    m_aEntryWeights.Insert(weight);
   }
  }
 }

 // A staging point the base's ground can't see, as ACR_AICommander.HiddenStaging moves it: a little further round
 // or further out, never closer than STAGE_HIDE_LEAST past the ring. The point itself when nowhere hides.
 protected vector Hidden(vector point, notnull array<vector> eyes)
 {
  if (!ACR_AICommander.GroundSeen(eyes, point)) return point;
  vector dir = point - m_vCenter;
  dir[1] = 0;
  float ring = dir.Length();
  if (ring < 1) return point;
  dir.Normalize();
  array<float> turns = {0, -10, 10, -20, 20};
  array<float> outs = {0, 60, -40, 120};
  foreach (float outward : outs)
  {
   float r = ring + outward;
   if (r < m_fBaseRadius + STAGE_HIDE_LEAST) continue;
   foreach (float turn : turns)
   {
    if (outward == 0 && turn == 0) continue;
    vector candidate = ACR_Manager.Ground(m_vCenter + ACR_AICommander.Turned(dir, turn) * r);
    if (BBB_Terrain.IsWaterAt(candidate)) continue;
    if (!ACR_AICommander.GroundSeen(eyes, candidate)) return candidate;
   }
  }
  return point;
 }

 protected static float WeightFor(array<vector> dirs, array<float> weights, vector way, bool even)
 {
  if (even) return 1;
  float threat = Math.Max(ACR_BaseDefences.ThreatDot(dirs, weights, way), 0);
  return threat * threat + WAY_FLOOR;
 }

 // Degrees between two bearings.
 static float Off(float a, float b)
 {
  float off = Math.AbsFloat(a - b);
  while (off > 360) off -= 360;
  if (off > 180) off = 360 - off;
  return off;
 }

 // ---------------------------------------------------------------- walking him in

 protected void HeapPush(notnull array<float> keys, notnull array<int> values, float key, int value)
 {
  int at = keys.Count();
  keys.Insert(key);
  values.Insert(value);
  while (at > 0)
  {
   int up = (at - 1) / 2;
   if (keys[up] <= key) break;
   keys[at] = keys[up];
   values[at] = values[up];
   at = up;
  }
  keys[at] = key;
  values[at] = value;
 }

 protected int HeapPop(notnull array<float> keys, notnull array<int> values, out float key)
 {
  int top = values[0];
  key = keys[0];
  int last = keys.Count() - 1;
  float moved = keys[last];
  int movedValue = values[last];
  keys.Remove(last);
  values.Remove(last);
  if (last == 0) return top;
  int at = 0;
  while (true)
  {
   int child = at * 2 + 1;
   if (child >= last) break;
   if (child + 1 < last && keys[child + 1] < keys[child]) child++;
   if (keys[child] >= moved) break;
   keys[at] = keys[child];
   values[at] = values[child];
   at = child;
  }
  keys[at] = moved;
  values[at] = movedValue;
  return top;
 }

 // The cheapest walk from everywhere to 'heart', into 'next'. On roads only, if asked.
 protected void Flow(int heart, bool roadsOnly, notnull array<int> next, bool plain = false)
 {
  float fireCost = FIRE_COST;
  if (plain) fireCost = 0;
  int n = m_iN;
  int count = n * n;
  array<float> dist = {};
  for (int i = 0; i < count; i++)
  {
   dist.Insert(-1);
   next[i] = -1;
  }
  if (heart < 0) return;
  array<float> keys = {};
  array<int> values = {};
  dist[heart] = 0;
  HeapPush(keys, values, 0, heart);
  while (!keys.IsEmpty())
  {
   float d;
   int cell = HeapPop(keys, values, d);
   if (d > dist[cell]) continue;
   int iz = cell / n;
   int ix = cell - iz * n;
   for (int k = 0; k < 8; k++)
   {
    int nx = ix + m_aDX[k];
    int nz = iz + m_aDZ[k];
    if (nx < 0 || nz < 0 || nx >= n || nz >= n) continue;
    int other = nz * n + nx;
    float walk = m_aWalk[other];
    if (walk < 0 || (roadsOnly && !m_aRoad[other])) continue;
    float through = d + walk * (1 + fireCost * m_aFire[other]) * m_aStep[k] * m_fCell;
    float was = dist[other];
    if (was >= 0 && was <= through) continue;
    dist[other] = through;
    next[other] = cell;
    HeapPush(keys, values, through, other);
   }
  }
 }

 protected void AddWay(int from, notnull array<int> next, float weight, bool road, int heart)
 {
  ACR_KillWay way = new ACR_KillWay();
  way.m_fWeight = weight;
  way.m_bRoad = road;
  int cell = from;
  int guard = m_iN * m_iN;
  while (cell >= 0 && guard > 0)
  {
   way.m_aCells.Insert(cell);
   if (cell == heart) break;
   cell = next[cell];
   guard--;
  }
  if (way.m_aCells.Count() < 2 || cell != heart) return;
  m_aWays.Insert(way);
 }

 // He walks in again, with all that fires on the ground as it is now.
 void Solve()
 {
  m_iSolves++;
  m_iLook++;
  // All that fires, on the cells of his old ways as well as round them. (Before, only the ground round them was put
  // back before the walk: his old ways showed no fire at all, and he walked them again as if nothing stood there.)
  Refire();
  Flow(m_iHeart, false, m_aNext);
  Flow(m_iRoadHeart, true, m_aNextRoad);
  m_aWays.Clear();
  foreach (int e, int entry : m_aEntries) AddWay(entry, m_aNext, m_aEntryWeights[e], false, m_iHeart);
  foreach (int r, int roadEntry : m_aRoadEntries) AddWay(roadEntry, m_aNextRoad, m_aRoadWeights[r], true, m_iRoadHeart);
  // The cells on them, and which way he goes there.
  foreach (int was : m_aOn) m_aTraffic[was] = 0;
  m_aOn.Clear();
  foreach (ACR_KillWay way : m_aWays)
  {
   int cells = way.m_aCells.Count();
   for (int k = 0; k + 1 < cells; k++)
   {
    int here = way.m_aCells[k];
    if (m_aTraffic[here] > 0) continue;
    m_aTraffic[here] = 1;
    m_aOn.Insert(here);
    int there = way.m_aCells[k + 1];
    int hz = here / m_iN;
    int tz = there / m_iN;
    float dx = (there - tz * m_iN) - (here - hz * m_iN);
    float dz = tz - hz;
    float len = Math.Max(Math.Sqrt(dx * dx + dz * dz), 0.1);
    m_aWayX[here] = dx / len;
    m_aWayZ[here] = dz / len;
   }
  }
  // What's placed fires on his new ways as well.
  foreach (ACR_KillSite placed : m_aPlaced) Aim(placed);
  Refire();
  Shares();
 }

 // The fire of all that's placed, from nothing: on the cells of his ways as each was last aimed, and round about.
 protected void Refire()
 {
  ForgetFire();
  foreach (ACR_KillSite firing : m_aPlaced)
  {
   foreach (int k, int c : firing.m_aSees) PutFire(c, firing.m_aDX[k], firing.m_aDZ[k], firing.m_aHit[k]);
   PutRound(firing);
  }
 }

 protected void ForgetFire()
 {
  int count = m_iN * m_iN;
  for (int i = 0; i < count; i++)
  {
   m_aFire[i] = 0;
   m_aFireX[i] = 0;
   m_aFireZ[i] = 0;
  }
 }

 protected void PutFire(int c, float dx, float dz, float hit)
 {
  m_aFire[c] = m_aFire[c] + hit;
  m_aFireX[c] = m_aFireX[c] + dx * hit;
  m_aFireZ[c] = m_aFireZ[c] + dz * hit;
 }

 // What it fires on off his ways. (On a cell of his way, Aim's figure is the true one: this only adds where that
 // isn't, so he doesn't step aside into ground it covers.)
 protected void PutRound(ACR_KillSite site)
 {
  float hit = site.m_fPower * 0.7;
  foreach (int k, int c : site.m_aRound)
  {
   if (m_aTraffic[c] > 0) continue;
   PutFire(c, site.m_aRoundX[k], site.m_aRoundZ[k], hit);
  }
 }

 // Metres under fire on each way, how much each way counts, what that puts on each cell.
 protected void Shares()
 {
  float least = float.MAX;
  foreach (ACR_KillWay way : m_aWays)
  {
   float fire = 0;
   foreach (int c : way.m_aCells)
   {
    float here = m_aFire[c];
    if (here > SATURATE) here = SATURATE;
    fire += here;
   }
   way.m_fFire = fire * m_fCell;
   if (way.m_fFire < least) least = way.m_fFire;
  }
  float total = 0;
  foreach (ACR_KillWay counted : m_aWays)
  {
   counted.m_fShare = counted.m_fWeight * (SOFT_LEAST + (1 - SOFT_LEAST) * Math.Pow(2.718, -SOFT * (counted.m_fFire - least) / SOFT_SPAN));
   total += counted.m_fShare;
  }
  if (total <= 0) return;
  foreach (int was : m_aOn) m_aTraffic[was] = 0;
  foreach (ACR_KillWay shared : m_aWays)
  {
   shared.m_fShare = shared.m_fShare / total;
   foreach (int on : shared.m_aCells)
   {
    if (m_aInside[on]) m_aTraffic[on] = m_aTraffic[on] + shared.m_fShare * INSIDE;
    else m_aTraffic[on] = m_aTraffic[on] + shared.m_fShare;
   }
  }
 }

 // ---------------------------------------------------------------- what a place does to him

 // A place, on the ground at 'at', its eye 'up' m over it.
 ACR_KillSite Site(vector at, float facing, float halfArc, float range, float power, float up)
 {
  ACR_KillSite site = new ACR_KillSite();
  site.m_vEye = at;
  site.m_vEye[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + up;
  site.m_fFacing = facing;
  site.m_fHalfArc = halfArc;
  site.m_fRange = range;
  site.m_fPower = power;
  vector way = ACR_BaseSurvey.Dir(facing);
  site.m_fFacingX = way[0];
  site.m_fFacingZ = way[2];
  site.m_fCos = Math.Cos(halfArc * Math.DEG2RAD);
  return site;
 }

 // What it fires on of the ways he walks now.
 protected void Aim(ACR_KillSite site)
 {
  site.m_iLooked = m_iLook;
  site.m_aSees.Clear();
  site.m_aHit.Clear();
  site.m_aDX.Clear();
  site.m_aDZ.Clear();
  site.m_aClose.Clear();
  float eyeX = site.m_vEye[0];
  float eyeZ = site.m_vEye[2];
  float range = site.m_fRange;
  foreach (int on, int c : m_aOn)
  {
   vector m = Middle(c);
   float dx = m[0] - eyeX;
   float dz = m[2] - eyeZ;
   float d2 = dx * dx + dz * dz;
   if (d2 > range * range) continue;
   float d = Math.Sqrt(d2);
   if (d < NEAREST) continue;
   // Far off, every second cell, counted double: his ways don't turn much out there.
   float every = 1;
   if (d > FAR_LOOK)
   {
    if (on % 2 != 0) continue;
    every = 2;
   }
   bool inArc = (dx * site.m_fFacingX + dz * site.m_fFacingZ) / d >= site.m_fCos;
   bool seen = inArc && Sees(site, c, m);
   if (!seen)
   {
    if (d < CLOSE) site.m_aClose.Insert(c);
    continue;
   }
   float reach = 1;
   if (d > NEAR_FULL) reach = 1 - (1 - FAR_HIT) * (d - NEAR_FULL) / Math.Max(range - NEAR_FULL, 1);
   float ux = dx / d;
   float uz = dz / d;
   float across = Math.AbsFloat(m_aWayX[c] * uz - m_aWayZ[c] * ux);
   float flank = FLANK_LEAST + (FLANK_MOST - FLANK_LEAST) * across;
   site.m_aSees.Insert(c);
   site.m_aHit.Insert(site.m_fPower * reach * flank * every);
   site.m_aDX.Insert(ux);
   site.m_aDZ.Insert(uz);
  }
 }

 protected bool Sees(ACR_KillSite site, int cell, vector middle)
 {
  bool known;
  if (site.m_mLOS.Find(cell, known)) return known;
  m_iTraces++;
  known = ACR_BaseSurvey.SeesFrom(site.m_vEye, middle, MAN_HEIGHT);
  site.m_mLOS.Set(cell, known);
  return known;
 }

 // The most it could add now, if it saw all it's aimed at: no sights asked.
 float Potential(ACR_KillSite site)
 {
  float eyeX = site.m_vEye[0];
  float eyeZ = site.m_vEye[2];
  float range2 = site.m_fRange * site.m_fRange;
  float sum = 0;
  foreach (int c : m_aOn)
  {
   float traffic = m_aTraffic[c];
   if (traffic <= 0) continue;
   int iz = c / m_iN;
   int ix = c - iz * m_iN;
   float dx = m_fX + (ix + 0.5) * m_fCell - eyeX;
   float dz = m_fZ + (iz + 0.5) * m_fCell - eyeZ;
   float d2 = dx * dx + dz * dz;
   if (d2 > range2 || d2 < NEAREST * NEAREST) continue;
   float along = dx * site.m_fFacingX + dz * site.m_fFacingZ;
   if (along <= 0 || along * along < site.m_fCos * site.m_fCos * d2) continue;
   float ratio = m_aFire[c] / SATURATE;
   sum += traffic * FLANK_MOST * (1 + CROSS) / (1 + ratio * ratio * ratio);
  }
  return sum * site.m_fPower * m_fCell;
 }

 // What it adds now: more for ground he walks that little fires on, and for crossing what fires there.
 float Value(ACR_KillSite site)
 {
  if (site.m_iLooked != m_iLook) Aim(site);
  float sum = 0;
  foreach (int k, int c : site.m_aSees)
  {
   float traffic = m_aTraffic[c];
   if (traffic <= 0) continue;
   float fire = m_aFire[c];
   float ratio = fire / SATURATE;
   float more = 1 / (1 + ratio * ratio * ratio);
   if (fire > 0.05)
   {
    float length = Math.Sqrt(m_aFireX[c] * m_aFireX[c] + m_aFireZ[c] * m_aFireZ[c]);
    if (length > 0.01)
    {
     float same = Math.AbsFloat(m_aFireX[c] * site.m_aDX[k] + m_aFireZ[c] * site.m_aDZ[k]) / length;
     more *= 1 + CROSS * (1 - same);
    }
   }
   sum += traffic * site.m_aHit[k] * more * m_fCell;
  }
  foreach (int near : site.m_aClose)
  {
   float covered = Math.Min(m_aFire[near], 1);
   sum -= m_aTraffic[near] * CLOSE_COST * (1 - covered) * m_fCell;
  }
  if (sum > 0 && !m_aPlaced.IsEmpty() && !HasFriend(site)) sum *= LONE;
  return sum;
 }

 protected bool HasFriend(ACR_KillSite site)
 {
  foreach (ACR_KillSite other : m_aPlaced)
  {
   if (other != site && vector.DistanceXZ(other.m_vEye, site.m_vEye) <= SUPPORT) return true;
  }
  return false;
 }

 // It's going in: its fire counts from now on, and the ways shift their weight.
 void Add(ACR_KillSite site)
 {
  if (site.m_iLooked != m_iLook) Aim(site);
  foreach (int k, int c : site.m_aSees)
  {
   float hit = site.m_aHit[k];
   m_aFire[c] = m_aFire[c] + hit;
   m_aFireX[c] = m_aFireX[c] + site.m_aDX[k] * hit;
   m_aFireZ[c] = m_aFireZ[c] + site.m_aDZ[k] * hit;
  }
  m_aPlaced.Insert(site);
  LookRound(site);
  PutRound(site);
  Shares();
 }

 // A place fires on all it sees, not only his ways now: a look at every so many cells in its arc, so that he doesn't
 // simply step aside into ground it covers well. Each look stands for the block round it.
 protected void LookRound(ACR_KillSite site)
 {
  vector eye = site.m_vEye;
  int span = site.m_fRange / m_fCell;
  int cx = (eye[0] - m_fX) / m_fCell;
  int cz = (eye[2] - m_fZ) / m_fCell;
  int step = 3;
  if (m_fCell > 12) step = 2;
  for (int dz = -span; dz <= span; dz += step)
  {
   for (int dx = -span; dx <= span; dx += step)
   {
    int ix = cx + dx;
    int iz = cz + dz;
    if (ix < 0 || iz < 0 || ix >= m_iN || iz >= m_iN) continue;
    int c = iz * m_iN + ix;
    if (m_aWalk[c] < 0) continue;
    vector m = Middle(c);
    float wide = m[0] - eye[0];
    float deep = m[2] - eye[2];
    float apart = Math.Sqrt(wide * wide + deep * deep);
    if (apart < NEAREST || apart > site.m_fRange) continue;
    // Far off, half as many.
    if (apart > FAR_LOOK && (dx + dz) % (step * 2) != 0) continue;
    if (Off(ACR_BaseDefences.Bearing(Vector(wide, 0, deep)), site.m_fFacing) > site.m_fHalfArc) continue;
    if (!Sees(site, c, m)) continue;
    for (int bz = 0; bz < step; bz++)
    {
     for (int bx = 0; bx < step; bx++)
     {
      if (ix + bx >= m_iN || iz + bz >= m_iN) continue;
      int other = (iz + bz) * m_iN + ix + bx;
      if (m_aWalk[other] < 0) continue;
      site.m_aRound.Insert(other);
      site.m_aRoundX.Insert(wide / apart);
      site.m_aRoundZ.Insert(deep / apart);
     }
    }
   }
  }
 }

 // Bags in the way.
 void AddBags(vector a, vector b)
 {
  float length = vector.DistanceXZ(a, b);
  int steps = length / (m_fCell * 0.5);
  steps++;
  for (int s = 0; s <= steps; s++)
  {
   float part = s * 1.0 / steps;
   int i = At(a[0] + (b[0] - a[0]) * part, a[2] + (b[2] - a[2]) * part);
   if (i >= 0 && m_aWalk[i] >= 0 && m_aWalk[i] < BAGS_WALK) m_aWalk[i] = BAGS_WALK;
  }
 }

 // How far 'facing' should turn, of 'turns', to do the most from there. The bearing to use.
 float BestFacing(vector at, float facing, notnull array<float> turns, float halfArc, float range, float power, float up)
 {
  float best = facing;
  ACR_KillSite first = Site(at, facing, halfArc, range, power, up);
  float bestValue = Value(first);
  foreach (float turn : turns)
  {
   ACR_KillSite turned = Site(at, facing + turn, halfArc, range, power, up);
   turned.m_mLOS = first.m_mLOS;
   float value = Value(turned);
   if (value <= bestValue * 1.1) continue;
   bestValue = value;
   best = facing + turn;
  }
  return best;
 }

 // ---------------------------------------------------------------- where he is now

 // The way he'd walk to the middle from 'from', the plain shortest, as a squad given the order does. Cells from where
 // he is or, from further off, from the edge on that side.
 void WayFrom(vector from, notnull array<int> cells)
 {
  cells.Clear();
  if (m_aNextPlain.IsEmpty())
  {
   int count = m_iN * m_iN;
   for (int i = 0; i < count; i++) m_aNextPlain.Insert(-1);
   Flow(m_iHeart, false, m_aNextPlain, true);
  }
  int cell = At(from[0], from[2]);
  if (cell < 0 || m_aWalk[cell] < 0 || m_aNextPlain[cell] < 0)
  {
   vector way = ACR_BaseDefences.FlatDir(m_vCenter, from);
   float back = Math.Min(m_fHalf - m_fCell, vector.DistanceXZ(m_vCenter, from));
   cell = -1;
   while (back > m_fCell)
   {
    vector p = m_vCenter + way * back;
    int here = At(p[0], p[2]);
    if (here >= 0 && m_aWalk[here] >= 0 && m_aNextPlain[here] >= 0)
    {
     cell = here;
     break;
    }
    back -= m_fCell;
   }
  }
  int guard = m_iN * m_iN;
  while (cell >= 0 && guard > 0)
  {
   cells.Insert(cell);
   if (cell == m_iHeart) break;
   cell = m_aNextPlain[cell];
   guard--;
  }
 }

 // What bags at 'at' facing 'facing' fire on of 'cells': metres, the nearer the more.
 float OnWay(vector at, float facing, notnull array<int> cells)
 {
  vector eye = at;
  eye[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + ACR_BaseSurvey.UP_BAGS;
  vector ahead = ACR_BaseSurvey.Dir(facing);
  float cosArc = Math.Cos(BAG_ARC * Math.DEG2RAD);
  float sum = 0;
  foreach (int c : cells)
  {
   vector m = Middle(c);
   // He's there already, as good as: what counts is the ground before it.
   if (vector.DistanceXZ(m, m_vCenter) < AT_THE_MIDDLE) continue;
   float dx = m[0] - eye[0];
   float dz = m[2] - eye[2];
   float d = Math.Sqrt(dx * dx + dz * dz);
   if (d < NEAREST || d > BAG_RANGE) continue;
   if ((dx * ahead[0] + dz * ahead[2]) / d < cosArc) continue;
   if (!ACR_BaseSurvey.SeesFrom(eye, m, MAN_HEIGHT)) continue;
   float reach = 1;
   if (d > NEAR_FULL) reach = 1 - (1 - FAR_HIT) * (d - NEAR_FULL) / Math.Max(BAG_RANGE - NEAR_FULL, 1);
   sum += reach * m_fCell;
  }
  return sum;
 }

 // ---------------------------------------------------------------- how it comes out

 // The way he'd choose now, of those he's likely to come by: metres under fire on it, and its share of what's near
 // the town. 'ways' how many he has.
 void Softest(out float fire, out int ways)
 {
  fire = float.MAX;
  float heaviest = 0;
  ways = m_aWays.Count();
  foreach (ACR_KillWay weight : m_aWays) heaviest = Math.Max(heaviest, weight.m_fWeight);
  foreach (ACR_KillWay way : m_aWays)
  {
   if (way.m_fWeight >= heaviest * 0.3 && way.m_fFire < fire) fire = way.m_fFire;
  }
  if (fire == float.MAX) fire = 0;
 }

 // The way he'd choose now, as Softest counts it. Null with none.
 ACR_KillWay SoftestWay()
 {
  float heaviest = 0;
  foreach (ACR_KillWay weight : m_aWays) heaviest = Math.Max(heaviest, weight.m_fWeight);
  ACR_KillWay best;
  foreach (ACR_KillWay way : m_aWays)
   if (way.m_fWeight >= heaviest * 0.3 && (!best || way.m_fFire < best.m_fFire)) best = way;
  return best;
 }

 // The weighted mean of metres under fire on his ways.
 float MeanFire()
 {
  float sum, weights;
  foreach (ACR_KillWay way : m_aWays)
  {
   sum += way.m_fFire * way.m_fWeight;
   weights += way.m_fWeight;
  }
  if (weights <= 0) return 0;
  return sum / weights;
 }

 void Done(string town)
 {
  float softest;
  int ways;
  Softest(softest, ways);
  s_iLastMs = System.GetTickCount() - m_iStarted;
  s_iLastSolves = m_iSolves;
  s_iLastTraces = m_iTraces;
  PrintFormat("[ACR] KILLGROUND %1: %2 ways in, the softest %3 m under fire, on average %4 m, %5 walks, %6 sights, %7 ms", town, ways, (int)softest, (int)MeanFire(), m_iSolves, m_iTraces, s_iLastMs);
 }

 // Tests draw it.
 void DumpWays(string town)
 {
  foreach (ACR_KillWay way : m_aWays)
  {
   foreach (int c : way.m_aCells)
   {
    vector m = Middle(c);
    PrintFormat("TOWN|%1|V|%2|%3|%4|%5", town, (int)m[0], (int)m[2], way.m_fShare.ToString(-1, 3), way.m_bRoad);
   }
   PrintFormat("TOWN|%1|V|end", town);
  }
 }

 // A town's whole plan, as he meets it: tests, one yardstick whichever way it was planned. Walks him in against it
 // until he settles. The same plan, the same answer.
 static ACR_KillGround Measure(ACR_BaseWork work, ACR_BaseSurvey survey)
 {
  ACR_KillGround ground = ForTown(survey, false, work.m_Base.m_sName);
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   int since = 3;
   foreach (ACR_LinePiece piece : line.m_aPieces)
   {
    ground.AddBags(piece.m_vFrom, piece.m_vTo);
    since++;
    if (!piece.m_bLong || since < 4) continue;
    since = 0;
    ground.Add(ground.Site(piece.m_vPos, ACR_BaseDefences.Bearing(piece.m_vFacing), BAG_ARC, BAG_RANGE, 0.7, ACR_BaseSurvey.UP_BAGS));
   }
  }
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (site.m_eKind == ACR_EBaseSite.NEST) ground.Add(ground.Site(site.m_vPos, site.m_fYaw, GUN_ARC, GUN_RANGE, GUN_POWER, ACR_BaseSurvey.UP_NEST));
   else if (site.m_eKind == ACR_EBaseSite.BUNKER) ground.Add(ground.Site(site.m_vPos, site.m_fYaw, GUN_ARC, GUN_RANGE, POST_POWER, ACR_BaseSurvey.UP_BUNKER));
  }
  ground.Solve();
  ground.Solve();
  return ground;
 }

 // ---------------------------------------------------------------- for whom

 // A town being fortified: its survey has the ground and the threats. 'even': round the whole town alike.
 static ACR_KillGround ForTown(ACR_BaseSurvey survey, bool even, string town)
 {
  ACR_KillGround ground = new ACR_KillGround();
  ground.Build(survey.m_vCenter, survey.m_fRadius + PAST_TOWN, survey.m_aSegs, survey.m_aBuildings, survey, "kill ground " + town);
  ground.m_fBaseRadius = survey.m_fRadius;
  if (survey.m_Base) ground.m_fBaseRadius = survey.m_Base.m_fRadius;
  ground.Entries(survey.m_aDirs, survey.m_aWeights, even);
  ground.Solve();
  return ground;
 }

 // A front: the ground round 'center', with him coming from 'enemy'.
 static ACR_KillGround ForFront(vector center, vector enemy, float half, string key)
 {
  array<ref ACR_BaseDefenceSeg> segs = {};
  array<IEntity> houses = {};
  ACR_BaseDefences.Roads(center, half, segs);
  ACR_BaseDefences.Buildings(center, half, houses);
  ACR_KillGround ground = new ACR_KillGround();
  ground.Build(center, half, segs, houses, null, "kill front " + key);
  array<vector> dirs = {ACR_BaseDefences.FlatDir(center, enemy)};
  array<float> weights = {1};
  ground.Entries(dirs, weights, false);
  ground.Solve();
  return ground;
 }
}
