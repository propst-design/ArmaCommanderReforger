// Where a town's bags go, worked out as closing the town (the user, 1 Nov: "some of the placement just doesn't
// make sense, really think outside the box"). Lines walked out from guessed seeds float: they close nothing,
// and an attacker walks round them. So the question is turned round: what's the cheapest line that shuts the
// town off from the enemy, using what already stands?
//   The town is rastered on a CELL m grid. Its shape is the houses and their yards swelled until they touch, the
//   holes filled and shrunk back (a lone barn stays out of it, the middle is always in it), and every cell knows
//   how far it is from that outline.
//   The line is then one shortest path right round the town, in a band just outside the outline: open ground
//   costs metres of bags, more where it sees little, stands on a road or on hard ground, or strays from the band,
//   and houses, stone walls, water and cliffs cost next to nothing. So it goes from house to house across the gaps
//   by itself, and every stretch of bags stands between two things. (In a planar grid the shortest path across a
//   band is the minimum cut of it: nothing cheaper closes the town.) A garden fence doesn't count: nobody fights
//   from behind boards or wire, so yards are part of the town's shape and the line stands outside them, in the open.
//   Bags then go where the enemy comes from, on the roads first, for as long as the budget lasts. With some left:
//   a line out in front across what he comes over, if it sees, and a line behind the ways through.
//   Seeded noise on the costs gives a different wall each match, all of them near the cheapest (ACR_Variety's
//   rule: never a poor one just to vary). The styles differ in where the band lies and what's built of it.
// -acrBaseRing 0: lines walked out from seeds, as before (ACR_BaseLines.Plan).
class ACR_RingRun : Managed
{
 ref array<vector> m_aWay = {};  // where it goes, in the ring's order
 vector m_vBefore;               // what it starts from, when that's something solid
 vector m_vOut;                  // away from the town at its middle
 vector m_vMiddle;
 float m_fLength;
 float m_fThreat;
 float m_fView;
 float m_fValue;
 int m_iRoads;
 int m_iTier;
 int m_iOrder;                   // along the ring
 int m_iParent;                  // its unbroken stretch of the ring: parts of one meet
 string m_sStart;                // what each end stands on: as ACR_BaseLine's ends, "" open
 string m_sEnd;
 bool m_bWanted;
 ref ACR_KillSite m_Site;        // what it does to him (ACR_KillGround)
 bool m_bOutFront;               // on the line further out
 bool m_bForWay;                 // laid out for one of his ways, not along the town
 float m_fBound;                 // the most it could score now
 bool m_bLooked;                 // the walker has tried it
}

class ACR_BaseRing : Managed
{
 static const float CELL = 6;
 static const float BEYOND = 120;      // m past the search radius the grid goes
 static const float CLOSE = 30;        // m houses and yards swell by to count as one town
 static const float YARD = 18;         // m from a house that a wall or a fence is its yard, not a paddock
 static const float ROAD_COST = 3;     // a stretch crosses a road, it doesn't follow one
 static const float INSIDE_COST = 3;   // ... and stays out of the town: a line at a wood's edge beats one through the yards
 static const float CORE = 55;         // m round the middle that's always in it
 static const float TOWN_PAST = 25;    // m past the search radius that can still be in it
 static const float OFF = 10;          // m past the outline the line wants to stand
 static const float BAND_IN = 24;      // m it may go in from there
 static const float BAND_OUT = 46;     // m it may go out
 static const float BAND_WIDTH = 22;   // m off its place where straying costs about a metre of bags per metre
 static const float NOISE = 0.12;      // what a match's luck does to the costs
 static const float STEEP = 0.75;      // rise per metre that counts as a cliff
 static const float LOOK_FAR = 78;     // m a cell is looked out from
 static const float LOOK_GOOD = 60;    // m: seeing this much costs nothing
 static const float BLIND_COST = 2;    // seeing too little to be worth laying bags
 static const float BOXED_COST = 2.5;  // ... and nothing behind either
 static const float WIRE_BACK = 4;     // m bags stand behind wire
 static const float PART = 36;         // m: a longer stretch is judged in parts this long
 static const float MIN_THREAT = 0.2;  // less threatened than this gets no bags
 static const float WAY_STEP = 18;     // m between the points a walk heads for
 static const float MIN_VIEW = 25;     // m a part must see on average to be worth laying, unless it holds a road
 static const float FORWARD_OFF = 38;  // the line out in front
 static const float FORWARD_IN = 14;
 static const float FORWARD_OUT = 34;
 static const float FORWARD_THREAT = 0.6;
 static const float FORWARD_VIEW = 40;
 static const int FORWARD_LEFT = 12;   // pieces that must be left for it
 static const float FORWARD_CLEAR = 15; // m from other bags
 static const float WALLED = 0.4;      // this much of the threatened ring along walls: nothing out in front
 static const float GATE_BACK = 18;    // m behind a way through, the line that faces it
 static const float GATE_HALF = 11;
 static const int GATE_LEFT = 6;
 static const int GATE_LINES = 3;
 static const float POINT_GAP = 25;    // m between strongpoints, or a little more
 static const float POINT_GAP_THIN = 8; // ... in a town they left thin
 static const float POINT_ENOUGH = 0.6; // of tier I's budget: thin below this
 static const float HOLE_VIEW = 12;    // m a part must see to fill a hole between two parts being built
 static const float WORTH_LEAST = 0.4; // what a part must do to him to be built
 static const float WORTH_SHARE = 0.1; // ... against the first that went in: past that it's bags on ground he hardly walks
 static const float WORTH_SHARE_FIRST = 0.05; // ... while tier I isn't full: a town gets its first line of defence, but not where nobody comes
 static const float NEXT_TO = 1.3;     // a part beside one going in: a line, not loose pieces
 static const float WALK_AGAIN = 0.34; // of the bags, between looks at where he'd walk now
 static const int MOST_WALKS = 2;
 static const int BUILD_ROUNDS = 3;    // goes at choosing and laying, while bags are left
 static const int BIG_TOWN = 120;      // pieces a town's budget is over to get one fewer
 static const int LEFT_TO_GO_ON = 8;   // pieces left that are worth another go
 static const float TIER_OVER = 1.12;  // a tier takes this much over its budget to keep a line together
 static const float TIER_ONE_SHARE = 0.93; // of tier I's budget that's planned on
 static const float OUT_FRONT = 0.85;  // against the line at the town's edge, for what it does: it ties into less
 static const float OUT_FRONT_DEPTH = 1.25; // ... in a town defended in depth
 static const int OUT_PARENT = 1000;   // its stretches are numbered apart from the edge's
 static const int WAY_PARENT = 2000;   // ... and those laid out for his ways
 static const int WAY_LINES_WAYS = 8;  // of his ways, the heaviest
 static const int WAY_LINES_MOST = 60; // places proposed
 static const float WAY_STEP_ALONG = 45; // m along a way between places
 static const float WAY_OUT = 80;      // m past the search radius a way is looked at from
 static const float WAY_IN = 40;       // m from the middle it's looked at up to
 static const float WAY_AHEAD = 40;    // m ahead of him the line across his way stands
 static const float WAY_SIDE = 45;     // m to his side a line firing across him stands
 static const float WAY_SIDE_NEAR = 22; // ... or this, if that's a house
 static const float STREET_OFF = 7;    // m from the middle of a road a street block's line starts
 static const float STREET_HALF = 11;  // half such a line
 static const float STREET_RIM_IN = -70; // m inside the outline those may stand: the streets are where he is
 static const float WAY_HALF = 15;     // half such a line
 static const float WAY_SHORTEST = 9;  // m: shorter, once its ends are off houses, walls, roads and paving, isn't a line
 static const float WAY_ANCHOR = 8;    // m from its end to something it can end on
 static const float ANCHORED = 1.15;   // ... each such end: nothing to walk round
 static const float LOOK_MARGIN = 0.85; // what can't come this near the best isn't looked at
 static const float BOUND_MARGIN = 1.1; // what it scored, as a bound on what it scores next: mostly it only falls
 static const float FRONT_EACH = 0.88; // a new line costs this much per line already going in, against growing one
 static const float GROW_LENGTH = 24;  // m a line grows by
 static const float GROW_TURN = 22;    // degrees it may turn doing it
 static const float WAY_RIM_OUT = 52;  // m past the town's outline a line laid out for him may stand: past this nobody supports it
 static const float WAY_RIM_IN = -30;  // ... and inside it

 static const int HOUSE = 1;
 static const int STONE = 2;
 static const int BOARD = 4;
 static const int WIRE = 8;
 static const int WATER = 16;
 static const int CLIFF = 32;
 static const int ROAD = 64;
 static const int HARD = 128;
 static const int LOOKED_HARD = 256;
 static const int TREE_ONE = 1024;     // the rest of a cell's int counts its trees
 static const int NEAR_ROAD = 512;
 static const int FREE_RING = HOUSE | STONE | WATER | CLIFF;

 static int s_iLastMs, s_iLastRuns, s_iLastCells; // tests, the log
 static int s_iPlans;                              // tests
 static bool s_bMeasureForTest;
 static int s_iLastAsked, s_iLastClosed;
 static const float CLOSED_WITHIN = 9;             // m from bags that a stretch of the way round is closed
 static ref array<vector> s_aLastPath;        // tests draw it
 static void ACR_InitStatics()
 {
  s_aLastPath = {};
 }

 static bool On() { return ACR_LogisticsRules.Choice("acrBaseRing", "1") != "0"; }
 // -acrBaseFinalWalk 0: no walk against the finished plan, no last part on his softest way (FinalWalk).
 static bool FinalWalkOn() { return ACR_LogisticsRules.Choice("acrBaseFinalWalk", "1") != "0"; }
 static const int FINAL_LEAST = 4;      // pieces of tier I and II left: fewer, and only a small gun can go on
 static const int FINAL_TRIES = 3;      // places tried for the line when one won't walk

 ACR_BaseSurvey m_Survey;
 protected ref RandomGenerator m_Luck;
 protected int m_iLaidFronts;     // lines already laid when choosing more
 protected float m_fFirstValue;   // what the first part that went in did to him
 int m_iN;
 float m_fX;
 float m_fZ;
 float m_fCX;
 float m_fCZ;
 ref array<int> m_aCell = {};
 ref array<float> m_aY = {};
 ref array<float> m_aRim = {};
 ref array<float> m_aThreat = {};
 ref array<float> m_aView = {};
 ref array<float> m_aBack = {};
 protected ref array<int> m_aDX = {1, -1, 0, 0, 1, 1, -1, -1};
 protected ref array<int> m_aDZ = {0, 0, 1, -1, 1, -1, 1, -1};
 protected ref array<float> m_aStep = {1, 1, 1, 1, 1.414, 1.414, 1.414, 1.414};
 protected static ACR_BaseRing s_Filling;

 // ---------------------------------------------------------------- the grid

 protected int At(float x, float z)
 {
  int ix = (x - m_fX) / CELL;
  int iz = (z - m_fZ) / CELL;
  if (x < m_fX || z < m_fZ || ix >= m_iN || iz >= m_iN) return -1;
  return iz * m_iN + ix;
 }

 vector Middle(int i)
 {
  int ix = i - (i / m_iN) * m_iN;
  int iz = i / m_iN;
  float x = m_fX + (ix + 0.5) * CELL;
  float z = m_fZ + (iz + 0.5) * CELL;
  return Vector(x, m_aY[i], z);
 }

 protected void Mark(float x, float z, int what)
 {
  int i = At(x, z);
  if (i >= 0) m_aCell[i] = m_aCell[i] | what;
 }

 protected void MarkAlong(vector a, vector b, int what)
 {
  float length = vector.DistanceXZ(a, b);
  int steps = length / 2;
  steps++;
  for (int s = 0; s <= steps; s++)
  {
   float part = s * 1.0 / steps;
   Mark(a[0] + (b[0] - a[0]) * part, a[2] + (b[2] - a[2]) * part, what);
  }
 }

 // A wall by what it does: stone stops men and bullets, boards and tin men and eyes, wire and rails only men.
 static int WallKind(string name)
 {
  if (name.Contains("Pole.et") || name.Contains("_Pole")) return 0;
  if (name.Contains("ConcreteWall") || name.Contains("BrickWall") || name.Contains("StoneWall") || name.Contains("Retaining") || name.Contains("Rampart")) return STONE;
  if (name.Contains("WoodenWall") || name.Contains("TinWall") || name.Contains("MetalWall") || name.Contains("Corrugated")) return BOARD;
  return WIRE;
 }

 protected static bool CollectGround(IEntity e)
 {
  if (!e) return true;
  ACR_BaseRing ring = s_Filling;
  if (Tree.Cast(e))
  {
   vector at = e.GetOrigin();
   int i = ring.At(at[0], at[2]);
   if (i >= 0) ring.m_aCell[i] = ring.m_aCell[i] + TREE_ONE;
   return true;
  }
  if (!e.GetPhysics() || ChimeraCharacter.Cast(e)) return true;
  string name = ACR_BaseSurvey.FullName(e);
  if (!name.Contains("/Walls/") && !name.Contains("Fence")) return true;
  int kind = WallKind(name);
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
  ring.MarkAlong(e.CoordToParent(a), e.CoordToParent(b), kind);
  return true;
 }

 protected void MarkHouse(IEntity b)
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
  int fromX = Math.Max((loX - m_fX) / CELL, 0);
  int toX = Math.Min((hiX - m_fX) / CELL, m_iN - 1);
  int fromZ = Math.Max((loZ - m_fZ) / CELL, 0);
  int toZ = Math.Min((hiZ - m_fZ) / CELL, m_iN - 1);
  for (int iz = fromZ; iz <= toZ; iz++)
  {
   for (int ix = fromX; ix <= toX; ix++)
   {
    float cx = m_fX + (ix + 0.5) * CELL;
    float cz = m_fZ + (iz + 0.5) * CELL;
    int hits;
    for (int sample = 0; sample < 5; sample++)
    {
     float sx = cx;
     float sz = cz;
     if (sample == 1 || sample == 2) sx -= 2;
     if (sample == 3 || sample == 4) sx += 2;
     if (sample == 1 || sample == 3) sz -= 2;
     if (sample == 2 || sample == 4) sz += 2;
     float dx = sx - c1[0];
     float dz = sz - c1[2];
     float along = (dx * ux + dz * uz) / uu;
     float across = (dx * vx + dz * vz) / vv;
     if (along >= 0 && along <= 1 && across >= 0 && across <= 1) hits++;
    }
    if (hits >= 2) m_aCell[iz * m_iN + ix] = m_aCell[iz * m_iN + ix] | HOUSE;
   }
  }
 }

 void Build(ACR_BaseSurvey survey)
 {
  m_Survey = survey;
  vector center = survey.m_vCenter;
  m_fCX = center[0];
  m_fCZ = center[2];
  float half = survey.m_fRadius + BEYOND;
  m_iN = half * 2 / CELL;
  m_fX = center[0] - half;
  m_fZ = center[2] - half;
  int count = m_iN * m_iN;
  BaseWorld world = GetGame().GetWorld();
  for (int iz = 0; iz < m_iN; iz++)
  {
   float z = m_fZ + (iz + 0.5) * CELL;
   for (int ix = 0; ix < m_iN; ix++)
   {
    float x = m_fX + (ix + 0.5) * CELL;
    float y = world.GetSurfaceY(x, z);
    m_aY.Insert(y);
    int cell = 0;
    if (BBB_Terrain.IsWater(x, z)) cell = WATER;
    m_aCell.Insert(cell);
    m_aThreat.Insert(-1);
    m_aView.Insert(-1);
    m_aBack.Insert(-1);
   }
  }
  // Too steep to walk or build on is as good as a wall.
  for (int gz = 1; gz < m_iN - 1; gz++)
  {
   for (int gx = 1; gx < m_iN - 1; gx++)
   {
    int here = gz * m_iN + gx;
    float sx = (m_aY[here + 1] - m_aY[here - 1]) / (2 * CELL);
    float sz = (m_aY[here + m_iN] - m_aY[here - m_iN]) / (2 * CELL);
    if (sx * sx + sz * sz > STEEP * STEEP) m_aCell[here] = m_aCell[here] | CLIFF;
   }
  }
  foreach (IEntity house : survey.m_aBuildings) if (house) MarkHouse(house);
  foreach (ACR_BaseDefenceSeg seg : survey.m_aSegs) MarkAlong(seg.m_vA, seg.m_vB, ROAD);
  for (int rz = 1; rz < m_iN - 1; rz++)
  {
   for (int rx = 1; rx < m_iN - 1; rx++)
   {
    int on = rz * m_iN + rx;
    if ((m_aCell[on] & ROAD) != 0) continue;
    if (((m_aCell[on - 1] | m_aCell[on + 1] | m_aCell[on - m_iN] | m_aCell[on + m_iN]) & ROAD) != 0) m_aCell[on] = m_aCell[on] | NEAR_ROAD;
   }
  }
  s_Filling = this;
  world.QueryEntitiesBySphere(center, half * 1.2, CollectGround, null, EQueryEntitiesFlags.STATIC);
  s_Filling = null;
  s_iLastCells = count;
 }

 // Stands where bags already are: a line can tie into it, for nothing.
 void MarkLine(ACR_BaseLine line)
 {
  foreach (ACR_LinePiece piece : line.m_aPieces) MarkAlong(piece.m_vFrom, piece.m_vTo, STONE);
 }

 // ---------------------------------------------------------------- the town's shape

 // Metres to the nearest set cell, near enough (two sweeps).
 protected void DistanceTo(notnull array<bool> seeds, notnull array<float> result)
 {
  int n = m_iN;
  int count = n * n;
  result.Clear();
  for (int k = 0; k < count; k++)
  {
   if (seeds[k]) result.Insert(0);
   else result.Insert(100000);
  }
  float straight = CELL;
  float slant = CELL * 1.414;
  for (int z = 0; z < n; z++)
  {
   for (int x = 0; x < n; x++)
   {
    int i = z * n + x;
    float d = result[i];
    if (d == 0) continue;
    if (x > 0) d = Math.Min(d, result[i - 1] + straight);
    if (z > 0)
    {
     d = Math.Min(d, result[i - n] + straight);
     if (x > 0) d = Math.Min(d, result[i - n - 1] + slant);
     if (x < n - 1) d = Math.Min(d, result[i - n + 1] + slant);
    }
    result[i] = d;
   }
  }
  for (int bz = n - 1; bz >= 0; bz--)
  {
   for (int bx = n - 1; bx >= 0; bx--)
   {
    int j = bz * n + bx;
    float e = result[j];
    if (e == 0) continue;
    if (bx < n - 1) e = Math.Min(e, result[j + 1] + straight);
    if (bz < n - 1)
    {
     e = Math.Min(e, result[j + n] + straight);
     if (bx < n - 1) e = Math.Min(e, result[j + n + 1] + slant);
     if (bx > 0) e = Math.Min(e, result[j + n - 1] + slant);
    }
    result[j] = e;
   }
  }
 }

 // Cells that can be reached from the seeds over cells where 'through' is true.
 protected void Flood(notnull array<int> seeds, notnull array<bool> through, notnull array<bool> reached)
 {
  int n = m_iN;
  int count = n * n;
  reached.Clear();
  for (int k = 0; k < count; k++) reached.Insert(false);
  array<int> stack = {};
  foreach (int seed : seeds)
  {
   if (reached[seed]) continue;
   reached[seed] = true;
   stack.Insert(seed);
  }
  while (!stack.IsEmpty())
  {
   int last = stack.Count() - 1;
   int i = stack[last];
   stack.Remove(last);
   int x = i - (i / n) * n;
   int z = i / n;
   if (x > 0 && !reached[i - 1] && through[i - 1]) { reached[i - 1] = true; stack.Insert(i - 1); }
   if (x < n - 1 && !reached[i + 1] && through[i + 1]) { reached[i + 1] = true; stack.Insert(i + 1); }
   if (z > 0 && !reached[i - n] && through[i - n]) { reached[i - n] = true; stack.Insert(i - n); }
   if (z < n - 1 && !reached[i + n] && through[i + n]) { reached[i + n] = true; stack.Insert(i + n); }
  }
 }

 // The signed distance from the town's outline, negative inside.
 void Outline(float core = CORE)
 {
  int n = m_iN;
  int count = n * n;
  float reach = m_Survey.m_fRadius + TOWN_PAST;
  array<bool> houses = {};
  bool anyHouse;
  for (int h = 0; h < count; h++)
  {
   bool isHouse = (m_aCell[h] & HOUSE) != 0;
   houses.Insert(isHouse);
   if (isHouse) anyHouse = true;
  }
  array<float> fromHouse = {};
  DistanceTo(houses, fromHouse);
  // And its yards.
  array<bool> built = {};
  for (int y = 0; y < count; y++)
  {
   int here = m_aCell[y];
   built.Insert((here & HOUSE) != 0 || ((here & (STONE | BOARD | WIRE)) != 0 && fromHouse[y] <= YARD));
  }
  array<float> near = {};
  DistanceTo(built, near);
  // Swelled, the holes filled: what the outside can't reach.
  array<bool> open = {};
  array<int> rim = {};
  for (int c = 0; c < count; c++)
  {
   bool isOpen = near[c] > CLOSE || !anyHouse;
   open.Insert(isOpen);
   int cx = c - (c / n) * n;
   int cz = c / n;
   if (cx == 0 || cz == 0 || cx == n - 1 || cz == n - 1)
   {
    open[c] = true;
    rim.Insert(c);
   }
  }
  array<bool> outside = {};
  Flood(rim, open, outside);
  // Shrunk back.
  array<float> fromOut = {};
  DistanceTo(outside, fromOut);
  array<bool> shape = {};
  for (int s = 0; s < count; s++)
  {
   // Up to the town's own reach: the next town along the road isn't this one's to close.
   vector m = Middle(s);
   float dx = m[0] - m_fCX;
   float dz = m[2] - m_fCZ;
   float apart = dx * dx + dz * dz;
   bool inShape = fromOut[s] > CLOSE && apart <= reach * reach;
   if (!inShape) inShape = apart <= core * core;
   shape.Insert(inShape);
  }
  // What hangs together with the middle.
  array<int> middle = {};
  int centerCell = At(m_fCX, m_fCZ);
  if (centerCell < 0) centerCell = count / 2;
  middle.Insert(centerCell);
  array<bool> kept = {};
  Flood(middle, shape, kept);
  array<bool> away = {};
  for (int a = 0; a < count; a++) away.Insert(!kept[a]);
  array<float> outward = {};
  array<float> inward = {};
  DistanceTo(kept, outward);
  DistanceTo(away, inward);
  m_aRim.Clear();
  for (int r = 0; r < count; r++)
  {
   if (kept[r]) m_aRim.Insert(-inward[r]);
   else m_aRim.Insert(outward[r]);
  }
 }

 // ---------------------------------------------------------------- what a cell is worth

 protected float ThreatAt(int i)
 {
  float known = m_aThreat[i];
  if (known >= 0) return known;
  vector m = Middle(i);
  float threat = m_Survey.m_aThreat[ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(m - m_Survey.m_vCenter))];
  threat = Math.Max(threat, 0);
  m_aThreat[i] = threat;
  return threat;
 }

 // Away from the town, at this cell.
 vector OutAt(int i)
 {
  int n = m_iN;
  int x = i - (i / n) * n;
  int z = i / n;
  float gx, gz;
  if (x >= 2 && x < n - 2 && z >= 2 && z < n - 2)
  {
   gx = m_aRim[i + 2] - m_aRim[i - 2];
   gz = m_aRim[i + 2 * n] - m_aRim[i - 2 * n];
  }
  if (Math.AbsFloat(gx) + Math.AbsFloat(gz) < 0.01)
  {
   vector m = Middle(i);
   gx = m[0] - m_fCX;
   gz = m[2] - m_fCZ;
  }
  vector way = Vector(gx, 0, gz);
  way.Normalize();
  return way;
 }

 // Metres seen away from the town, over the grid. Asked per block of four cells: it doesn't change much.
 protected float ViewAt(int i)
 {
  int n = m_iN;
  int x = i - (i / n) * n;
  int z = i / n;
  int block = (z / 2) * 2 * n + (x / 2) * 2;
  float known = m_aView[block];
  if (known >= 0) return known;
  vector eye = Middle(block);
  eye[1] = eye[1] + ACR_BaseSurvey.UP_BAGS;
  float bearing = ACR_BaseDefences.Bearing(OutAt(block));
  float best = ACR_BaseSurvey.Reach(eye, bearing, LOOK_FAR);
  if (best < LOOK_GOOD) best = Math.Max(best, ACR_BaseSurvey.Reach(eye, bearing - 25, LOOK_FAR));
  if (best < LOOK_GOOD) best = Math.Max(best, ACR_BaseSurvey.Reach(eye, bearing + 25, LOOK_FAR));
  m_aView[block] = best;
  return best;
 }

 // A short view behind as well as in front: a yard, not a place for a line. Asked only of blind ground, per block.
 protected bool BoxedAt(int i)
 {
  int n = m_iN;
  int x = i - (i / n) * n;
  int z = i / n;
  int block = (z / 2) * 2 * n + (x / 2) * 2;
  float known = m_aBack[block];
  if (known < 0)
  {
   vector eye = Middle(block);
   eye[1] = eye[1] + 1;
   known = ACR_BaseSurvey.Reach(eye, ACR_BaseDefences.Bearing(OutAt(block)) + 180, ACR_BaseLiner.BOX + 1.2);
   m_aBack[block] = known;
  }
  return known < ACR_BaseLiner.BOX;
 }

 protected bool IsHard(int i)
 {
  int cell = m_aCell[i];
  if ((cell & LOOKED_HARD) != 0) return (cell & HARD) != 0;
  // One look for four cells.
  int n = m_iN;
  int x = i - (i / n) * n;
  int z = i / n;
  int block = (z / 2) * 2 * n + (x / 2) * 2;
  bool hard = ACR_Fortify.HardSurface(Middle(block));
  int mark = LOOKED_HARD;
  if (hard) mark = mark | HARD;
  for (int bz = 0; bz < 2; bz++)
  {
   for (int bx = 0; bx < 2; bx++)
   {
    int other = block + bz * n + bx;
    if (other < m_aCell.Count()) m_aCell[other] = m_aCell[other] | mark;
   }
  }
  return hard;
 }

 // Metres of bags a metre of this cell is worth, -1 where the line can't go.
 protected float CostAt(int i, float off, float bandIn, float bandOut, bool threatMatters)
 {
  float d = m_aRim[i];
  if (d < off - bandIn || d > off + bandOut) return -1;
  int cell = m_aCell[i];
  if ((cell & FREE_RING) != 0) return 0.03;
  float c = 1;
  if ((cell & ROAD) != 0) c += ROAD_COST;
  else if (IsHard(i)) c += 1;
  if ((cell & NEAR_ROAD) != 0) c += 0.4;
  if (d < 0) c += INSIDE_COST;
  int trees = cell / TREE_ONE;
  c += 0.25 * Math.Min(trees, 6);
  int n = m_iN;
  int x = i - (i / n) * n;
  int z = i / n;
  if (x > 0 && x < n - 1 && z > 0 && z < n - 1)
  {
   float sx = (m_aY[i + 1] - m_aY[i - 1]) / (2 * CELL);
   float sz = (m_aY[i + n] - m_aY[i - n]) / (2 * CELL);
   c += Math.Min(Math.Sqrt(sx * sx + sz * sz) * 5, 3);
  }
  float view = ViewAt(i);
  c += 1.6 * (1 - Math.Min(view, LOOK_GOOD) / LOOK_GOOD);
  if (view < ACR_BaseLiner.SEES_FAR)
  {
   c += BLIND_COST;
   if (BoxedAt(i)) c += BOXED_COST;
  }
  float stray = (d - off) / BAND_WIDTH;
  c += 0.9 * stray * stray;
  if (threatMatters) c *= 1 + 0.7 * (1 - ThreatAt(i));
  return c;
 }

 // ---------------------------------------------------------------- the ring

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

 // One way round the town, the cells in order: from one side of a seam straight out from the middle, where the
 // threat is least, round to its other side. False if there's no way round.
 bool Solve(float off, float bandIn, float bandOut, float noise, bool threatMatters, string key, notnull array<int> path)
 {
  path.Clear();
  int n = m_iN;
  int count = n * n;
  RandomGenerator rng = ACR_Variety.For(key);
  // Low-frequency luck.
  int coarse = 8;
  int gridN = n / coarse + 2;
  array<float> grid = {};
  for (int g = 0; g < gridN * gridN; g++) grid.Insert(1 - noise + 2 * noise * rng.RandFloat01());
  int sectors = ACR_BaseSurvey.SECTORS;
  int least = 0;
  float leastSum = 1000000;
  for (int s = 0; s < sectors; s++)
  {
   float sum = m_Survey.m_aThreat[(s + sectors - 1) % sectors] + m_Survey.m_aThreat[s] + m_Survey.m_aThreat[(s + 1) % sectors];
   if (sum < leastSum)
   {
    leastSum = sum;
    least = s;
   }
  }
  vector seamWay = ACR_BaseSurvey.Dir(ACR_BaseSurvey.SectorBearing(least));
  array<float> cost = {};
  array<int> side = {};
  array<float> dist = {};
  array<int> parent = {};
  array<float> keys = {};
  array<int> values = {};
  for (int z = 0; z < n; z++)
  {
   for (int x = 0; x < n; x++)
   {
    int i = z * n + x;
    float c = CostAt(i, off, bandIn, bandOut, threatMatters);
    int sideHere = 0;
    if (c >= 0)
    {
     float fx = x * 1.0 / coarse;
     float fz = z * 1.0 / coarse;
     int gx = fx;
     int gz = fz;
     float tx = fx - gx;
     float tz = fz - gz;
     float top = grid[gz * gridN + gx] * (1 - tx) + grid[gz * gridN + gx + 1] * tx;
     float bottom = grid[(gz + 1) * gridN + gx] * (1 - tx) + grid[(gz + 1) * gridN + gx + 1] * tx;
     if ((m_aCell[i] & FREE_RING) == 0) c *= top * (1 - tz) + bottom * tz;
     vector m = Middle(i);
     float dx = m[0] - m_fCX;
     float dz = m[2] - m_fCZ;
     float along = dx * seamWay[0] + dz * seamWay[2];
     float across = dx * seamWay[2] - dz * seamWay[0];
     if (along > 0)
     {
      if (Math.AbsFloat(across) < CELL) c = -1;
      else if (Math.AbsFloat(across) < CELL * 2.5)
      {
       if (across > 0) sideHere = 1;
       else sideHere = -1;
      }
     }
    }
    cost.Insert(c);
    side.Insert(sideHere);
    parent.Insert(-1);
    if (c >= 0 && sideHere > 0)
    {
     dist.Insert(c);
     HeapPush(keys, values, c, i);
    }
    else dist.Insert(-1);
   }
  }
  int end = -1;
  while (!keys.IsEmpty())
  {
   float d;
   int cell = HeapPop(keys, values, d);
   if (d > dist[cell]) continue;
   if (side[cell] < 0)
   {
    end = cell;
    break;
   }
   int cx = cell - (cell / n) * n;
   int cz = cell / n;
   for (int k = 0; k < 8; k++)
   {
    int nx = cx + m_aDX[k];
    int nz = cz + m_aDZ[k];
    if (nx < 0 || nz < 0 || nx >= n || nz >= n) continue;
    int next = nz * n + nx;
    float there = cost[next];
    if (there < 0) continue;
    float through = d + (there + cost[cell]) * 0.5 * m_aStep[k] * CELL;
    float was = dist[next];
    if (was >= 0 && was <= through) continue;
    dist[next] = through;
    parent[next] = cell;
    HeapPush(keys, values, through, next);
   }
  }
  if (end < 0) return false;
  array<int> backwards = {};
  int walk = end;
  while (walk >= 0)
  {
   backwards.Insert(walk);
   walk = parent[walk];
  }
  for (int b = backwards.Count() - 1; b >= 0; b--) path.Insert(backwards[b]);
  return true;
 }

 protected string WordFor(int cell)
 {
  if ((cell & HOUSE) != 0) return "building";
  if ((cell & STONE) != 0) return "wall";
  if ((cell & WATER) != 0) return "water";
  if ((cell & CLIFF) != 0) return "slope";
  return "";
 }

 // The stretches of bags along the path, long ones in parts.
 void Runs(notnull array<int> path, notnull array<ref ACR_RingRun> runs)
 {
  int parent;
  int from = -1;
  int count = path.Count();
  for (int p = 0; p <= count; p++)
  {
   bool stops = p == count || (m_aCell[path[p]] & FREE_RING) != 0;
   if (!stops)
   {
    if (from < 0) from = p;
    continue;
   }
   if (from < 0) continue;
   // path[from .. p - 1] is one stretch.
   int cells = p - from;
   int parts = 1;
   if (cells * CELL > PART * 1.5) parts = Math.Max(1, Math.Round(cells * CELL / PART));
   for (int k = 0; k < parts; k++)
   {
    int a = from + cells * k / parts;
    int b = from + cells * (k + 1) / parts - 1;
    ACR_RingRun run = MakeRun(path, a, b, from, p - 1);
    run.m_iParent = parent;
    run.m_iOrder = runs.Count();
    runs.Insert(run);
   }
   parent++;
   from = -1;
  }
 }

 protected ACR_RingRun MakeRun(notnull array<int> path, int a, int b, int stretchFrom, int stretchTo)
 {
  ACR_RingRun run = new ACR_RingRun();
  int mid = path[(a + b) / 2];
  run.m_vMiddle = Middle(mid);
  run.m_vOut = OutAt(mid);
  run.m_fThreat = ThreatAt(mid);
  float viewSum = 0;
  float since = WAY_STEP;
  for (int k = a; k <= b; k++)
  {
   int cell = path[k];
   if ((m_aCell[cell] & ROAD) != 0) run.m_iRoads++;
   viewSum += ViewAt(cell);
   run.m_fLength += CELL;
   if (k > a && vector.DistanceXZ(Middle(path[k - 1]), Middle(cell)) > CELL * 1.2) run.m_fLength += CELL * 0.41;
   since += CELL;
   if (since < WAY_STEP && k != b) continue;
   since = 0;
   vector point = Middle(cell);
   if ((m_aCell[cell] & WIRE) != 0) point = point - OutAt(cell) * WIRE_BACK;
   else if ((m_aCell[cell] & BOARD) != 0) point = point + OutAt(cell) * WIRE_BACK;
   run.m_aWay.Insert(point);
  }
  run.m_fView = viewSum / (b - a + 1);
  // What each end stands on: something real where the stretch ends, else the next part of it.
  if (a == stretchFrom && a > 0)
  {
   run.m_sStart = WordFor(m_aCell[path[a - 1]]);
   run.m_vBefore = Middle(path[a - 1]);
  }
  else if (a != stretchFrom) run.m_sStart = "line";
  if (b == stretchTo && b + 1 < path.Count())
  {
   run.m_sEnd = WordFor(m_aCell[path[b + 1]]);
   run.m_aWay.Insert(Middle(path[b + 1]));
  }
  else if (b != stretchTo) run.m_sEnd = "line";
  float size = Math.Min(run.m_fLength / 25, 1);
  float road = 1;
  if (run.m_iRoads > 0) road = 2.2;
  run.m_fValue = run.m_fThreat * run.m_fThreat * road * Math.Sqrt(size) * (0.4 + 0.6 * Math.Min(run.m_fView / LOOK_GOOD, 1));
  return run;
 }

 // The ways into town are where roads cross the ring.
 void Gates(notnull array<int> path)
 {
  array<ref ACR_BaseGate> gates = {};
  int count = path.Count();
  for (int p = 0; p < count; p++)
  {
   if ((m_aCell[path[p]] & ROAD) == 0) continue;
   // The middle of those in a row.
   int last = p;
   while (last + 1 < count && (m_aCell[path[last + 1]] & ROAD) != 0) last++;
   vector at = Middle(path[(p + last) / 2]);
   p = last;
   vector onRoad;
   float dist;
   int seg = ACR_BaseDefences.NearestSeg(m_Survey.m_aSegs, at, onRoad, dist);
   if (seg < 0 || dist > CELL * 1.5) continue;
   bool seen;
   foreach (ACR_BaseGate other : gates) if (vector.DistanceXZ(other.m_vPos, onRoad) < 25) seen = true;
   if (seen) continue;
   ACR_BaseDefenceSeg road = m_Survey.m_aSegs[seg];
   vector along = ACR_BaseDefences.FlatDir(road.m_vA, road.m_vB);
   vector away = OutAt(path[last]);
   if (ACR_BaseDefences.DotXZ(along, away) < 0) along = along * -1;
   ACR_BaseGate gate = new ACR_BaseGate();
   gate.m_vPos = onRoad;
   gate.m_vPos[1] = GetGame().GetWorld().GetSurfaceY(onRoad[0], onRoad[2]);
   gate.m_vOut = along;
   gate.m_iSector = ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(onRoad - m_Survey.m_vCenter));
   gates.Insert(gate);
  }
  if (gates.IsEmpty()) return;
  m_Survey.m_aGates.Clear();
  foreach (ACR_BaseGate kept : gates) m_Survey.m_aGates.Insert(kept);
 }

 // Of the threatened part of the path, how much runs along walls.
 float WalledShare(notnull array<int> path)
 {
  int threatened, walled;
  foreach (int cell : path)
  {
   if (ThreatAt(cell) < 0.5) continue;
   threatened++;
   if ((m_aCell[cell] & STONE) != 0) walled++;
  }
  if (threatened == 0) return 0;
  return walled * 1.0 / threatened;
 }

 // ---------------------------------------------------------------- the plan

 protected static void Sort(notnull array<ref ACR_RingRun> runs)
 {
  // Few enough for this.
  for (int i = 1; i < runs.Count(); i++)
  {
   ACR_RingRun held = runs[i];
   int k = i - 1;
   while (k >= 0 && runs[k].m_fValue < held.m_fValue)
   {
    runs[k + 1] = runs[k];
    k--;
   }
   runs[k + 1] = held;
  }
 }

 protected static int PiecesFor(float metres)
 {
  return metres / ACR_BaseLines.LONG_STEP + 1;
 }

 protected static bool BagsNear(ACR_BaseWork work, vector at, float within)
 {
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   foreach (ACR_LinePiece piece : line.m_aPieces)
    if (vector.DistanceXZ(piece.m_vPos, at) < within) return true;
  }
  return false;
 }

 protected static bool WantedAt(notnull array<ref ACR_RingRun> all, int parent, int order)
 {
  foreach (ACR_RingRun run : all)
   if (run.m_bWanted && run.m_iParent == parent && run.m_iOrder == order) return true;
  return false;
 }

 // The parts of one stretch that got the same tier, walked as one line, best first. The pieces used. 'all': every
 // part there is, this round's and the earlier rounds' (a neighbour built in an earlier round is still a line to
 // tie into, not an open end).
 protected static int BuildWanted(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_RingRun> sorted, notnull array<ref ACR_RingRun> all, int most)
 {
  int used;
  array<ref ACR_RingRun> done = {};
  foreach (ACR_RingRun first : sorted)
  {
   if (!first.m_bWanted || done.Contains(first)) continue;
   ACR_RingRun whole = new ACR_RingRun();
   array<ACR_RingRun> parts = {};
   foreach (ACR_RingRun other : sorted)
   {
    if (other.m_bWanted && other.m_iParent == first.m_iParent && other.m_iTier == first.m_iTier) parts.Insert(other);
   }
   // Along the ring. (Those that don't touch their neighbours are their own line.)
   for (int a = 1; a < parts.Count(); a++)
   {
    ACR_RingRun held = parts[a];
    int b = a - 1;
    while (b >= 0 && parts[b].m_iOrder > held.m_iOrder)
    {
     parts[b + 1] = parts[b];
     b--;
    }
    parts[b + 1] = held;
   }
   int at = parts.Find(first);
   int lo = at;
   int hi = at;
   while (lo > 0 && parts[lo - 1].m_iOrder == parts[lo].m_iOrder - 1) lo--;
   while (hi + 1 < parts.Count() && parts[hi + 1].m_iOrder == parts[hi].m_iOrder + 1) hi++;
   whole.m_iTier = first.m_iTier;
   whole.m_sStart = parts[lo].m_sStart;
   whole.m_vBefore = parts[lo].m_vBefore;
   whole.m_sEnd = parts[hi].m_sEnd;
   // A part nobody's building next to it: that end is open, it ties in or curves back.
   if (whole.m_sStart == "line" && !WantedAt(all, first.m_iParent, parts[lo].m_iOrder - 1)) whole.m_sStart = "";
   if (whole.m_sEnd == "line" && !WantedAt(all, first.m_iParent, parts[hi].m_iOrder + 1)) whole.m_sEnd = "";
   whole.m_vOut = first.m_vOut;
   whole.m_vMiddle = first.m_vMiddle;
   array<ACR_RingRun> walked = {};
   for (int p = lo; p <= hi; p++)
   {
    ACR_RingRun part = parts[p];
    if (part.m_bLooked) continue;
    foreach (vector point : part.m_aWay) whole.m_aWay.Insert(point);
    whole.m_fLength = whole.m_fLength + part.m_fLength;
    done.Insert(part);
    walked.Insert(part);
   }
   if (used >= most || walked.IsEmpty()) continue;
   int gotLine = ACR_BaseLines.RingLine(work, survey, kit, whole, whole.m_iTier, most - used);
   // What's really there is what fires.
   float share = Math.Clamp(gotLine * 1.0 / Math.Max(PiecesFor(whole.m_fLength), 1), 0, 1);
   foreach (ACR_RingRun tried : walked)
   {
    tried.m_bLooked = true;
    if (tried.m_Site) tried.m_Site.m_fPower = tried.m_Site.m_fPower * share;
   }
   PrintFormat("[ACR] BASEWORKS %1: ring line tier %2, %3 m in %4 points, from '%5' to '%6': %7 pieces", work.m_Base.m_sName, whole.m_iTier, (int)whole.m_fLength, whole.m_aWay.Count(), whole.m_sStart, whole.m_sEnd, gotLine);
   used += gotLine;
  }
  return used;
 }

 protected static RandomGenerator LuckFor(string town)
 {
  return ACR_Variety.For("base ring lengths " + town);
 }

 // A town's lines. False: nothing came of it, the caller walks lines from seeds.
 static bool Plan(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit)
 {
  int started = System.GetTickCount();
  ACR_BaseStyle style = work.m_Style;
  string town = work.m_Base.m_sName;
  ACR_BaseRing ring = new ACR_BaseRing();
  ring.m_Luck = LuckFor(town);
  ring.Build(survey);
  float core = CORE;
  float off = OFF;
  float minThreat = MIN_THREAT;
  bool threatMatters = true;
  int budgetOne = ACR_BaseLines.Budget(1, survey.m_iTown);
  int budgetTwo = ACR_BaseLines.Budget(2, survey.m_iTown);
  int budgetThree = ACR_BaseLines.Budget(3, survey.m_iTown);
  int total = Math.Min(ACR_BaseLines.MOST_PIECES, budgetOne + budgetTwo + budgetThree);
  RandomGenerator rng = ACR_Variety.For("base ring " + town);
  off += rng.RandFloat01() * 8 - 4;
  if (style.m_iKind == ACR_BaseStyle.RING)
  {
   off = 3;
   minThreat = 0;
   threatMatters = false;
  }
  if (style.m_iKind == ACR_BaseStyle.DEPTH) off = 4;
  ring.Outline(core);
  survey.m_Ring = ring;
  if (ACR_KillGround.On()) survey.m_Kill = ACR_KillGround.ForTown(survey, !threatMatters, town);
  int used;
  // A funnel's arms go in first: the ring ties into them.
  if (style.m_iKind == ACR_BaseStyle.FUNNEL) used += ACR_BaseLines.RingFunnel(work, survey, kit, ring, total);
  // Depth: a thin line well out first, the real one close in.
  if (style.m_iKind == ACR_BaseStyle.DEPTH && !survey.m_Kill) used += ring.Forward(work, survey, kit, 1, budgetOne * style.m_fFirstShare, "base ring forward first " + town);
  array<int> path = {};
  if (!ring.Solve(off, BAND_IN, BAND_OUT, NOISE, threatMatters, "base ring path " + town, path))
  {
   PrintFormat("[ACR] BASEWORKS %1: no way round the town", town);
   s_iLastMs = System.GetTickCount() - started;
   return !work.m_aLines.IsEmpty();
  }
  ring.Gates(path);
  array<ref ACR_RingRun> runs = {};
  ring.Runs(path, runs);
  s_iLastRuns = runs.Count();
  s_aLastPath.Clear();
  foreach (int pathCell : path) s_aLastPath.Insert(ring.Middle(pathCell));
  array<ref ACR_RingRun> sorted = {};
  ACR_KillGround kill = survey.m_Kill;
  // A line further out is as good a place as the edge if it's where he can be killed: it's a choice from the start.
  if (kill)
  {
   ring.AddOutFront(runs, threatMatters, "base ring out " + town);
   ring.AddWayLines(kill, runs);
  }
  if (!kill)
  {
   foreach (ACR_RingRun all : runs)
    if (all.m_fThreat >= minThreat && (all.m_fView >= MIN_VIEW || all.m_iRoads > 0)) sorted.Insert(all);
   Sort(sorted);
  }
  // Strongpoints: only the best of it, apart, with gaps on purpose.
  int spent = used;
  // (Walking a part can lay a little more than it was asked for: tier I stays the job it says it is.)
  if (kill) budgetOne = (int)(budgetOne * TIER_ONE_SHARE);
  if (kill) spent = ring.ChooseRuns(kill, style, runs, sorted, used, budgetOne, budgetTwo, total, 0, "base ring parts " + town);
  array<ACR_RingRun> points = {};
  for (int pass = 0; pass < 2 && !kill; pass++)
  {
   float pointGap = POINT_GAP;
   if (pass > 0)
   {
    if (style.m_iKind != ACR_BaseStyle.POINTS || budgetOne * POINT_ENOUGH <= spent) break;
    pointGap = POINT_GAP_THIN;
   }
   foreach (ACR_RingRun run : sorted)
   {
    if (run.m_bWanted) continue;
    int need = PiecesFor(run.m_fLength);
    if (spent + need > total) continue;
    if (style.m_iKind == ACR_BaseStyle.POINTS)
    {
     bool near;
     foreach (ACR_RingRun point : points) if (vector.DistanceXZ(point.m_vMiddle, run.m_vMiddle) < pointGap + (point.m_fLength + run.m_fLength) * 0.5) near = true;
     if (near) continue;
     points.Insert(run);
    }
    spent += need;
    run.m_bWanted = true;
    run.m_iTier = 3;
    if (spent <= budgetOne) run.m_iTier = 1;
    else if (spent <= budgetOne + budgetTwo) run.m_iTier = 2;
   }
  }
  if (style.m_iKind != ACR_BaseStyle.POINTS)
  {
   SortAlong(runs);
   for (int hole = 1; hole + 1 < runs.Count(); hole++)
   {
    ACR_RingRun filling = runs[hole];
    ACR_RingRun last = runs[hole - 1];
    ACR_RingRun next = runs[hole + 1];
    if (filling.m_bWanted || !last.m_bWanted || !next.m_bWanted || last.m_iParent != filling.m_iParent || next.m_iParent != filling.m_iParent) continue;
    if (last.m_iOrder != filling.m_iOrder - 1 || next.m_iOrder != filling.m_iOrder + 1) continue;
    if (filling.m_fView < HOLE_VIEW || spent + PiecesFor(filling.m_fLength) > total) continue;
    spent += PiecesFor(filling.m_fLength);
    filling.m_bWanted = true;
    filling.m_iTier = last.m_iTier;
    sorted.Insert(filling);
   }
  }
  used += BuildWanted(work, survey, kit, sorted, runs, total - used);
  // What couldn't be laid doesn't fire, and its bags are still there to use: he walks again, and they go elsewhere.
  // (A big town's laying is the dearest part of its plan: one round fewer there.)
  int rounds = BUILD_ROUNDS;
  if (total > BIG_TOWN) rounds--;
  for (int round = 1; kill && round < rounds && total - used >= LEFT_TO_GO_ON; round++)
  {
   ring.m_iLaidFronts = work.m_aLines.Count();
   kill.Solve();
   array<ref ACR_RingRun> more = {};
   ring.ChooseRuns(kill, style, runs, more, used, budgetOne, budgetTwo, total, 0, string.Format("base ring parts %1 %2", town, round));
   if (more.IsEmpty()) break;
   used += BuildWanted(work, survey, kit, more, runs, total - used);
   foreach (ACR_RingRun extra : more) sorted.Insert(extra);
  }
  // Bags to spare: out in front if there's ground to watch and no wall to fight from behind.
  int left = total - used;
  if (!kill && style.m_iKind != ACR_BaseStyle.POINTS && style.m_iKind != ACR_BaseStyle.DEPTH && left >= FORWARD_LEFT && ring.WalledShare(path) <= WALLED)
  {
   int forward = ring.Forward(work, survey, kit, 2, left, "base ring forward " + town);
   used += forward;
   left -= forward;
  }
  if (!kill && left >= GATE_LEFT) used += ring.GateLines(work, survey, kit, sorted, left);
  if (s_bMeasureForTest) ring.MeasureClosed(work, path);
  s_iPlans++;
  s_iLastMs = System.GetTickCount() - started;
  PrintFormat("[ACR] BASEWORKS %1: ring %2 lines, %3 pieces of %4, %5 parts of the ring (%6 places looked at), %7 ms", town, work.m_aLines.Count(), used, total, s_iLastRuns, runs.Count(), s_iLastMs);
  return !work.m_aLines.IsEmpty();
 }

 // Which of 'runs' get bags: over and over, the part that does most to him where he walks now (ACR_KillGround), and
 // every so often he walks again, round what's going in. A line that's going in can grow from its ends until it ties
 // into something, and a new line costs against growing one: a front, not scattered bits. Into 'chosen', best first.
 // How many pieces are spent now. 'tier' 0: by the budgets.
 protected int ChooseRuns(ACR_KillGround kill, ACR_BaseStyle style, notnull array<ref ACR_RingRun> runs, notnull array<ref ACR_RingRun> chosen, int spent, int budgetOne, int budgetTwo, int total, int tier, string key)
 {
  foreach (ACR_RingRun run : runs) if (!run.m_Site) SiteFor(kill, run);
  bool points = style.m_iKind == ACR_BaseStyle.POINTS;
  float pointGap = POINT_GAP;
  array<ACR_RingRun> placed = {};
  array<int> fronts = {};
  int walks;
  int walkAt = spent + total * WALK_AGAIN;
  Bounds(kill, runs);
  for (int made = 0; made < 300; made++)
  {
   // The likeliest first: once what's left can't come near the best, it isn't looked at.
   SortByBound(runs);
   array<float> scores = {};
   array<ACR_RingRun> open = {};
   float bestNow = 0;
   float frontCost = 1;
   if (!points) frontCost = Math.Pow(FRONT_EACH, fronts.Count() + m_iLaidFronts);
   foreach (ACR_RingRun candidate : runs)
   {
    if (candidate.m_bWanted) continue;
    if (candidate.m_fBound < bestNow * LOOK_MARGIN) break;
    int need = PiecesFor(candidate.m_fLength);
    if (spent + need > total) continue;
    if (points)
    {
     bool near;
     foreach (ACR_RingRun point : placed) if (vector.DistanceXZ(point.m_vMiddle, candidate.m_vMiddle) < pointGap + (point.m_fLength + candidate.m_fLength) * 0.5) near = true;
     if (near) continue;
    }
    bool crowded;
    foreach (ACR_RingRun other : placed)
    {
     // The next along the same line, or another way of going on from the same end.
     if (other.m_iParent == candidate.m_iParent)
     {
      if (other.m_iOrder == candidate.m_iOrder) crowded = true;
      continue;
     }
     if (other.m_bOutFront == candidate.m_bOutFront && !other.m_bForWay && !candidate.m_bForWay) continue;
     if (vector.DistanceXZ(other.m_vMiddle, candidate.m_vMiddle) < FORWARD_CLEAR + (other.m_fLength + candidate.m_fLength) * 0.5) crowded = true;
    }
    if (crowded) continue;
    float value = kill.Value(candidate.m_Site);
    // Tier I always gets its bags, if anywhere's any use: past that, only where it counts.
    float enough = Math.Max(WORTH_LEAST, m_fFirstValue * WORTH_SHARE_FIRST);
    if (spent >= budgetOne) enough = Math.Max(WORTH_LEAST, m_fFirstValue * WORTH_SHARE);
    if (value < enough)
    {
     candidate.m_fBound = 0;
     continue;
    }
    value = Scaled(value, candidate, style, points);
    bool beside = WantedBeside(runs, candidate);
    if (beside && !points) value *= NEXT_TO;
    else if (!fronts.Contains(candidate.m_iParent)) value *= frontCost;
    float score = value / Math.Sqrt(Math.Max(need, 1));
    candidate.m_fBound = score * BOUND_MARGIN;
    if (score > bestNow) bestNow = score;
    scores.Insert(score);
    open.Insert(candidate);
   }
   if (open.IsEmpty())
   {
    // A town they left thin: strongpoints closer together.
    if (points && pointGap > POINT_GAP_THIN && budgetOne * POINT_ENOUGH > spent)
    {
     pointGap = POINT_GAP_THIN;
     continue;
    }
    break;
   }
   ACR_RingRun picked = open[ACR_Variety.Pick(scores, string.Format("%1 %2", key, made))];
   if (m_fFirstValue <= 0) m_fFirstValue = kill.Value(picked.m_Site);
   spent += PiecesFor(picked.m_fLength);
   picked.m_bWanted = true;
   picked.m_iTier = tier;
   if (tier == 0)
   {
    picked.m_iTier = 3;
    if (spent <= budgetOne) picked.m_iTier = 1;
    else if (spent <= budgetOne + budgetTwo) picked.m_iTier = 2;
    // The rest of a line it's part of, if that tier can nearly take it: one line, built in one go.
    int beside = TierBeside(runs, picked);
    if (beside > 0 && beside < picked.m_iTier)
    {
     // (Tier I stays the job it says it is.)
     if (beside == 2 && spent <= (budgetOne + budgetTwo) * TIER_OVER) picked.m_iTier = 2;
    }
   }
   chosen.Insert(picked);
   placed.Insert(picked);
   if (!fronts.Contains(picked.m_iParent)) fronts.Insert(picked.m_iParent);
   kill.Add(picked.m_Site);
   for (int w = 1; w < picked.m_aWay.Count(); w++) kill.AddBags(picked.m_aWay[w - 1], picked.m_aWay[w]);
   if (picked.m_bForWay && !points) Grow(kill, picked, runs);
   if (spent >= walkAt && walks < MOST_WALKS)
   {
    kill.Solve();
    walks++;
    walkAt = spent + total * WALK_AGAIN;
    Bounds(kill, runs);
   }
  }
  return spent;
 }

 protected static void SiteFor(ACR_KillGround kill, ACR_RingRun run)
 {
  float power = Math.Clamp(run.m_fLength / PART, 0.5, 1.3);
  run.m_Site = kill.Site(run.m_vMiddle, ACR_BaseDefences.Bearing(run.m_vOut), ACR_KillGround.BAG_ARC, ACR_KillGround.BAG_RANGE, power, ACR_BaseSurvey.UP_BAGS);
 }

 protected static float Scaled(float value, ACR_RingRun run, ACR_BaseStyle style, bool points)
 {
  if (run.m_bOutFront)
  {
   if (style.m_iKind == ACR_BaseStyle.DEPTH) value *= OUT_FRONT_DEPTH;
   else value *= OUT_FRONT;
  }
  if (run.m_bForWay)
  {
   if (!run.m_sStart.IsEmpty()) value *= ANCHORED;
   if (!run.m_sEnd.IsEmpty()) value *= ANCHORED;
  }
  return value;
 }

 // The most each could do, with nothing in its way: no sights asked.
 protected static void Bounds(ACR_KillGround kill, notnull array<ref ACR_RingRun> runs)
 {
  foreach (ACR_RingRun run : runs)
  {
   if (run.m_bWanted) continue;
   float most = kill.Potential(run.m_Site) * ANCHORED * ANCHORED * NEXT_TO;
   run.m_fBound = most / Math.Sqrt(Math.Max(PiecesFor(run.m_fLength), 1));
  }
 }

 protected static void SortAlong(notnull array<ref ACR_RingRun> runs)
 {
  for (int i = 1; i < runs.Count(); i++)
  {
   ACR_RingRun held = runs[i];
   int k = i - 1;
   while (k >= 0 && runs[k].m_iOrder > held.m_iOrder)
   {
    runs[k + 1] = runs[k];
    k--;
   }
   runs[k + 1] = held;
  }
 }

 protected static void SortByBound(notnull array<ref ACR_RingRun> runs)
 {
  for (int i = 1; i < runs.Count(); i++)
  {
   ACR_RingRun held = runs[i];
   float bound = held.m_fBound;
   int k = i - 1;
   while (k >= 0 && runs[k].m_fBound < bound)
   {
    runs[k + 1] = runs[k];
    k--;
   }
   runs[k + 1] = held;
  }
 }

 // Ways of going on from a line's ends, unless it already ends on something.
 protected void Grow(ACR_KillGround kill, ACR_RingRun from, notnull array<ref ACR_RingRun> runs)
 {
  int count = from.m_aWay.Count();
  if (count < 2) return;
  if (from.m_sEnd.IsEmpty()) GrowEnd(kill, from, from.m_aWay[count - 2], from.m_aWay[count - 1], 1, runs);
  if (from.m_sStart.IsEmpty()) GrowEnd(kill, from, from.m_aWay[1], from.m_aWay[0], -1, runs);
 }

 protected void GrowEnd(ACR_KillGround kill, ACR_RingRun from, vector back, vector tip, int dir, notnull array<ref ACR_RingRun> runs)
 {
  vector along = ACR_BaseDefences.FlatDir(back, tip);
  array<float> turns = {0, GROW_TURN, -GROW_TURN};
  float growBy = GROW_LENGTH * (0.75 + 0.5 * m_Luck.RandFloat01());
  foreach (float turn : turns)
  {
   float rad = turn * Math.DEG2RAD;
   vector way = along * Math.Cos(rad) + Vector(along[2], 0, -along[0]) * Math.Sin(rad);
   vector to = tip + way * growBy;
   int cell = At(to[0], to[2]);
   if (cell < 0 || (m_aCell[cell] & (WATER | CLIFF)) != 0) continue;
   if (m_aRim[cell] > WAY_RIM_OUT || m_aRim[cell] < WAY_RIM_IN) continue;
   int midCell = At((tip[0] + to[0]) * 0.5, (tip[2] + to[2]) * 0.5);
   if (midCell < 0 || (m_aCell[midCell] & (HOUSE | ROAD)) != 0) continue;
   vector facing = Vector(way[2], 0, -way[0]);
   if (ACR_BaseDefences.DotXZ(facing, from.m_vOut) < 0) facing = facing * -1;
   ACR_RingRun run = new ACR_RingRun();
   run.m_bForWay = true;
   run.m_vMiddle = (tip + to) * 0.5;
   run.m_vOut = facing;
   run.m_fLength = growBy;
   run.m_fThreat = from.m_fThreat;
   run.m_fView = from.m_fView;
   run.m_iParent = from.m_iParent;
   run.m_iOrder = from.m_iOrder + dir;
   string word = m_Survey.AnchorNear(to, WAY_ANCHOR);
   if (word == "road") word = "";
   if (dir > 0)
   {
    run.m_aWay.Insert(tip);
    run.m_aWay.Insert(to);
    run.m_sEnd = word;
    run.m_sStart = "line";
   }
   else
   {
    run.m_aWay.Insert(to);
    run.m_aWay.Insert(tip);
    run.m_sStart = word;
    run.m_sEnd = "line";
   }
   SiteFor(kill, run);
   // As Bounds: Scaled counts both ends (the one it grows from is 'line'), and it's beside the line it grows.
   run.m_fBound = kill.Potential(run.m_Site) * ANCHORED * ANCHORED * NEXT_TO / Math.Sqrt(Math.Max(PiecesFor(growBy), 1));
   runs.Insert(run);
  }
 }

 protected static int TierBeside(notnull array<ref ACR_RingRun> runs, ACR_RingRun run)
 {
  foreach (ACR_RingRun other : runs)
  {
   if (other != run && other.m_bWanted && other.m_iParent == run.m_iParent && Math.AbsInt(other.m_iOrder - run.m_iOrder) == 1) return other.m_iTier;
  }
  return 0;
 }

 protected static bool WantedBeside(notnull array<ref ACR_RingRun> runs, ACR_RingRun run)
 {
  foreach (ACR_RingRun other : runs)
  {
   if (other.m_bWanted && other.m_iParent == run.m_iParent && Math.AbsInt(other.m_iOrder - run.m_iOrder) == 1) return true;
  }
  return false;
 }

 // Short lines laid out for the ways he walks: one across his way ahead of him, one off to either side firing across
 // him. Each a choice like any part of the ring: built if it does more to him than the rest.
 protected void AddWayLines(ACR_KillGround kill, notnull array<ref ACR_RingRun> runs)
 {
  array<ACR_KillWay> ways = {};
  foreach (ACR_KillWay way : kill.m_aWays) ways.Insert(way);
  for (int a = 1; a < ways.Count(); a++)
  {
   ACR_KillWay held = ways[a];
   int b = a - 1;
   while (b >= 0 && ways[b].m_fShare < held.m_fShare)
   {
    ways[b + 1] = ways[b];
    b--;
   }
   ways[b + 1] = held;
  }
  array<int> seen = {};
  int added;
  for (int w = 0; w < ways.Count() && w < WAY_LINES_WAYS; w++) added = WayLinesFor(kill, ways[w], runs, seen, added);
 }

 // The places laid out for one of his ways, into 'runs' ('seen' keeps one to a place). Those added so far, and after.
 int WayLinesFor(ACR_KillGround kill, ACR_KillWay way, notnull array<ref ACR_RingRun> runs, notnull array<int> seen, int added)
 {
  vector center = m_Survey.m_vCenter;
  float outer = m_Survey.m_fRadius + WAY_OUT;
  array<int> cells = way.m_aCells;
  float since = WAY_STEP_ALONG;
  for (int k = 0; k + 1 < cells.Count() && added < WAY_LINES_MOST; k++)
  {
   vector at = kill.Middle(cells[k]);
   vector next = kill.Middle(cells[k + 1]);
   since += vector.DistanceXZ(at, next);
   float apart = vector.DistanceXZ(at, center);
   if (apart > outer || apart < WAY_IN || since < WAY_STEP_ALONG) continue;
   since = 0;
   vector his = ACR_BaseDefences.FlatDir(at, next);
   vector side = Vector(his[2], 0, -his[0]);
   // Averaged over a few cells: a walk on a grid turns at every step.
   int further = Math.Min(k + 4, cells.Count() - 1);
   his = ACR_BaseDefences.FlatDir(at, kill.Middle(cells[further]));
   side = Vector(his[2], 0, -his[0]);
   vector ahead = at + his * WAY_AHEAD;
   int aheadCell = At(ahead[0], ahead[2]);
   // Down a road: it stays open for vehicles, so a line either side of it, turned in towards him. A street block.
   if (aheadCell >= 0 && (m_aCell[aheadCell] & ROAD) != 0)
   {
    added += TryStreetBlock(ahead, at, side, runs, seen);
    added += TryStreetBlock(ahead, at, side * -1, runs, seen);
   }
   else added += TryWayLine(ahead, his * -1, runs, seen);
   added += TryFlank(at, his, side, runs, seen);
   added += TryFlank(at, his, side * -1, runs, seen);
  }
  return added;
 }

 // Everything placed, he's walked in once more against it, honestly (ACR_KillGround.Measure: every piece's bags and gun
 // as the tests judge them). While tier I's and tier II's budgets have pieces left, one more part goes on his softest
 // way in: a line laid out for it, or a small gun where no line is worth it. Before this the planner's last walk came
 // before the nests, guns and posts went in, and its own figure never saw its softest way (3 Nov, Chotain: it said
 // 53 m where the yardstick said 0). The honest ground, for the log and the men at runtime; null with no lines.
 static ACR_KillGround FinalWalk(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit)
 {
  if (!survey.m_Kill || !survey.m_Ring || work.m_aLines.IsEmpty()) return null;
  int started = System.GetTickCount();
  string town = work.m_Base.m_sName;
  ACR_KillGround kill = ACR_KillGround.Measure(work, survey);
  float softest;
  int ways;
  kill.Softest(softest, ways);
  int usedAll;
  int usedEarly;
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   usedAll += line.m_aPieces.Count();
   if (line.m_iTier <= 2) usedEarly += line.m_aPieces.Count();
  }
  int budgetOne = ACR_BaseLines.Budget(1, survey.m_iTown);
  int left = Math.Min(budgetOne + ACR_BaseLines.Budget(2, survey.m_iTown) - usedEarly, ACR_BaseLines.MOST_PIECES - usedAll);
  string did = "nothing more fits";
  int got;
  int places;
  int guns;
  ACR_KillWay way = kill.SoftestWay();
  if (way)
  {
   if (left >= FINAL_LEAST)
   {
    array<ref ACR_RingRun> runs = {};
    array<int> seen = {};
    survey.m_Ring.WayLinesFor(kill, way, runs, seen, 0);
    places = runs.Count();
    // A place whose line won't walk (a fence, a yard) is marked and the next is tried.
    for (int attempt = 0; attempt < FINAL_TRIES && got == 0; attempt++)
    {
     ACR_RingRun picked = ChooseOne(kill, work.m_Style, runs, usedEarly, budgetOne, usedEarly + left, string.Format("base ring last %1 %2", town, attempt));
     if (!picked) break;
     array<ref ACR_RingRun> chosen = {};
     chosen.Insert(picked);
     got = BuildWanted(work, survey, kit, chosen, runs, left);
    }
    if (got > 0) did = string.Format("a line of %1 pieces", got);
   }
   if (got == 0)
   {
    // The gun chooses its place by the honest ground.
    ACR_KillGround was = survey.m_Kill;
    survey.m_Kill = kill;
    guns = ACR_BaseLines.SmallGun(work, survey);
    survey.m_Kill = was;
    if (guns > 0) did = "a small gun";
   }
   // What stands now, measured from nothing as the tests measure it: Done's line is then the yardstick's own figure. (One
   // more Solve on the old ground gave another figure: his walk hasn't settled after Measure's two, and the tests do two.)
   if (got > 0 || guns > 0) kill = ACR_KillGround.Measure(work, survey);
  }
  float after;
  kill.Softest(after, ways);
  PrintFormat("[ACR] BASEWORKS %1: walked in against the finished plan: the softest way %2 m under fire, %3 pieces left, %4 places on it, %5 on it, now %6 m (%7 ms)", town, (int)softest, left, places, did, (int)after, System.GetTickCount() - started);
  kill.Done(town);
  return kill;
 }

 // Of 'runs', the one that does most to him now where he walks, weighted among the near-best (ACR_Variety), marked
 // wanted with its tier. Null when none is worth it or fits 'most'.
 protected static ACR_RingRun ChooseOne(ACR_KillGround kill, ACR_BaseStyle style, notnull array<ref ACR_RingRun> runs, int spent, int budgetOne, int most, string key)
 {
  bool points = style.m_iKind == ACR_BaseStyle.POINTS;
  array<float> scores = {};
  array<ACR_RingRun> open = {};
  foreach (ACR_RingRun candidate : runs)
  {
   if (candidate.m_bWanted) continue;
   int need = PiecesFor(candidate.m_fLength);
   if (spent + need > most) continue;
   if (!candidate.m_Site) SiteFor(kill, candidate);
   float value = kill.Value(candidate.m_Site);
   if (value < WORTH_LEAST) continue;
   value = Scaled(value, candidate, style, points);
   scores.Insert(value / Math.Sqrt(Math.Max(need, 1)));
   open.Insert(candidate);
  }
  if (open.IsEmpty()) return null;
  ACR_RingRun picked = open[ACR_Variety.Pick(scores, key)];
  picked.m_bWanted = true;
  picked.m_iTier = 2;
  if (spent + PiecesFor(picked.m_fLength) <= budgetOne) picked.m_iTier = 1;
  return picked;
 }

 protected int TryStreetBlock(vector ahead, vector at, vector side, notnull array<ref ACR_RingRun> runs, notnull array<int> seen)
 {
  float half = STREET_HALF * (0.75 + 0.5 * m_Luck.RandFloat01());
  vector mid = ahead + side * (STREET_OFF + half);
  return TryWayLine(mid, ACR_BaseDefences.FlatDir(mid, at), runs, seen, half, STREET_RIM_IN);
 }

 // Off to his side firing across him: well out if there's room, or from the gardens beside the street.
 protected int TryFlank(vector at, vector his, vector side, notnull array<ref ACR_RingRun> runs, notnull array<int> seen)
 {
  vector across = at + his * 15 + side * WAY_SIDE;
  int got = TryWayLine(across, ACR_BaseDefences.FlatDir(across, at), runs, seen);
  if (got > 0) return got;
  across = at + his * 15 + side * WAY_SIDE_NEAR;
  return TryWayLine(across, ACR_BaseDefences.FlatDir(across, at), runs, seen, WAY_HALF * 0.75, STREET_RIM_IN);
 }

 protected int TryWayLine(vector mid, vector facing, notnull array<ref ACR_RingRun> runs, notnull array<int> seen, float halfLine = 0, float rimIn = WAY_RIM_IN)
 {
  int cell = At(mid[0], mid[2]);
  if (cell < 0 || !Layable(cell)) return 0;
  if (m_aRim[cell] > WAY_RIM_OUT || m_aRim[cell] < rimIn) return 0;
  // One to a place (a block three cells a side) and a way of facing. (cell / 9 took nine cells along a row, 54 m
  // of it, for one place, and the next row for another.)
  int row = cell / m_iN;
  int key = ((row / 3) * (m_iN / 3 + 1) + (cell - row * m_iN) / 3) * 4 + (int)(ACR_BaseSurvey.Normal(ACR_BaseDefences.Bearing(facing)) / 90);
  if (seen.Contains(key)) return 0;
  seen.Insert(key);
  mid[1] = GetGame().GetWorld().GetSurfaceY(mid[0], mid[2]);
  vector across = Vector(facing[2], 0, -facing[0]);
  // No two towns' lines cut to one length.
  float half = halfLine;
  if (half <= 0) half = WAY_HALF * (0.75 + 0.5 * m_Luck.RandFloat01());
  // Only ground bags can stand on: each end comes in short of a house, a wall, a road or paving, so the walk can start.
  float toA = FreeReach(mid, across * -1, half);
  float toB = FreeReach(mid, across, half);
  if (toA + toB < WAY_SHORTEST) return 0;
  vector a = mid - across * toA;
  vector b = mid + across * toB;
  mid = (a + b) * 0.5;
  mid[1] = GetGame().GetWorld().GetSurfaceY(mid[0], mid[2]);
  ACR_RingRun run = new ACR_RingRun();
  run.m_bForWay = true;
  run.m_vMiddle = mid;
  run.m_vOut = facing;
  run.m_fLength = toA + toB;
  run.m_fThreat = ThreatAt(cell);
  run.m_fView = ViewAt(cell);
  run.m_aWay.Insert(a);
  run.m_aWay.Insert(mid);
  run.m_aWay.Insert(b);
  run.m_sStart = m_Survey.AnchorNear(a, WAY_ANCHOR);
  run.m_sEnd = m_Survey.AnchorNear(b, WAY_ANCHOR);
  if (run.m_sStart == "road") run.m_sStart = "";
  if (run.m_sEnd == "road") run.m_sEnd = "";
  // (Room between the numbers: it can grow either way.)
  run.m_iParent = WAY_PARENT + runs.Count();
  run.m_iOrder = (WAY_PARENT + runs.Count()) * 100;
  runs.Insert(run);
  return 1;
 }

 // Ground bags can stand on.
 protected bool Layable(int cell)
 {
  if ((m_aCell[cell] & (HOUSE | STONE | WATER | CLIFF | ROAD)) != 0) return false;
  return !IsHard(cell);
 }

 // How far from 'from' along 'dir', up to 'most', every step is ground bags can stand on.
 protected float FreeReach(vector from, vector dir, float most)
 {
  float step = CELL * 0.5;
  float free = 0;
  for (float d = step; d <= most; d += step)
  {
   vector at = from + dir * d;
   int cell = At(at[0], at[2]);
   if (cell < 0 || !Layable(cell)) return Math.Max(free - 1, 0);
   free = d;
  }
  return most;
 }

 // The parts of a line further out that can see, added to 'runs'.
 protected void AddOutFront(notnull array<ref ACR_RingRun> runs, bool threatMatters, string key)
 {
  array<int> path = {};
  if (!Solve(FORWARD_OFF, FORWARD_IN, FORWARD_OUT, NOISE, threatMatters, key, path)) return;
  array<ref ACR_RingRun> further = {};
  Runs(path, further);
  foreach (ACR_RingRun run : further)
  {
   if (run.m_fView < FORWARD_VIEW * 0.6) continue;
   run.m_bOutFront = true;
   run.m_iParent = run.m_iParent + OUT_PARENT;
   run.m_iOrder = run.m_iOrder + OUT_PARENT;
   runs.Insert(run);
  }
 }

 // Tests: of the threatened part of the way round, how much is closed: by what stands there, or bags within reach.
 protected void MeasureClosed(ACR_BaseWork work, notnull array<int> path)
 {
  s_iLastAsked = 0;
  s_iLastClosed = 0;
  foreach (int cell : path)
  {
   if (ThreatAt(cell) < 0.5) continue;
   s_iLastAsked++;
   if ((m_aCell[cell] & FREE_RING) != 0 || BagsNear(work, Middle(cell), CLOSED_WITHIN)) s_iLastClosed++;
  }
 }

 // A line out in front: the same solve further out, only what faces the threat and sees. The pieces used.
 protected int Forward(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, int tier, int most, string key)
 {
  array<int> path = {};
  if (most < FORWARD_LEFT || !Solve(FORWARD_OFF, FORWARD_IN, FORWARD_OUT, NOISE, true, key, path)) return 0;
  array<ref ACR_RingRun> runs = {};
  Runs(path, runs);
  array<ref ACR_RingRun> sorted = {};
  ACR_KillGround kill = survey.m_Kill;
  if (kill)
  {
   // Anywhere out in front that does something to him and isn't on top of other bags.
   array<ref ACR_RingRun> fit = {};
   foreach (ACR_RingRun out_part : runs)
   {
    if (out_part.m_fView >= FORWARD_VIEW * 0.6 && !BagsNear(work, out_part.m_vMiddle, FORWARD_CLEAR + out_part.m_fLength * 0.5)) fit.Insert(out_part);
   }
   ChooseRuns(kill, work.m_Style, fit, sorted, 0, 0, 0, most, tier, key);
   return BuildWanted(work, survey, kit, sorted, fit, most);
  }
  foreach (ACR_RingRun all : runs)
  {
   if (all.m_fThreat < FORWARD_THREAT || all.m_fView < FORWARD_VIEW || all.m_iRoads == 0) continue;
   if (BagsNear(work, all.m_vMiddle, FORWARD_CLEAR + all.m_fLength * 0.5)) continue;
   sorted.Insert(all);
  }
  Sort(sorted);
  int spent;
  foreach (ACR_RingRun run : sorted)
  {
   int need = PiecesFor(run.m_fLength);
   if (spent + need > most) continue;
   spent += need;
   run.m_bWanted = true;
   run.m_iTier = tier;
  }
  return BuildWanted(work, survey, kit, sorted, runs, most);
 }

 // Behind the ways through (a road across the ring, a short plug between two things) that are threatened: a short
 // line inside, looking at it. Where a wall does the closing it's what there is to fight from. The pieces used.
 protected int GateLines(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_RingRun> sorted, int most)
 {
  int used;
  int made;
  foreach (ACR_RingRun run : sorted)
  {
   if (made >= GATE_LINES || most - used < GATE_LEFT) break;
   if (!run.m_bWanted || run.m_fThreat < 0.5 || (run.m_fLength > 30 && run.m_iRoads == 0)) continue;
   vector mid = run.m_vMiddle - run.m_vOut * GATE_BACK;
   int cell = At(mid[0], mid[2]);
   if (cell < 0 || (m_aCell[cell] & (FREE_RING | BOARD | WIRE)) != 0) continue;
   if (BagsNear(work, mid, GATE_BACK * 0.7)) continue;
   vector across = Vector(run.m_vOut[2], 0, -run.m_vOut[0]);
   ACR_RingRun line = new ACR_RingRun();
   line.m_vOut = run.m_vOut;
   line.m_vMiddle = mid;
   line.m_fLength = GATE_HALF * 2;
   line.m_aWay.Insert(mid - across * GATE_HALF);
   line.m_aWay.Insert(mid);
   line.m_aWay.Insert(mid + across * GATE_HALF);
   ACR_KillSite site;
   if (survey.m_Kill)
   {
    site = survey.m_Kill.Site(mid, ACR_BaseDefences.Bearing(run.m_vOut), ACR_KillGround.BAG_ARC, ACR_KillGround.BAG_RANGE, 0.7, ACR_BaseSurvey.UP_BAGS);
    if (survey.m_Kill.Value(site) < WORTH_LEAST) continue;
   }
   int got = ACR_BaseLines.RingLine(work, survey, kit, line, 3, most - used);
   if (got <= 0) continue;
   if (site) survey.m_Kill.Add(site);
   used += got;
   made++;
  }
  return used;
 }
}
