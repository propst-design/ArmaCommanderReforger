// What Fortify on a base looks at before anything goes in (#105, playtest 30 Oct: an MG nest behind a
// corrugated fence, its gun facing the fence, and a bunker against a building's garage doors; "it could
// dynamically look at a base and look what structures are already there and what the weak points are").
// One look per town, at the first order, in 12 sectors round the base:
//   the town's edge    how far the buildings reach that way;
//   the threat         which way the enemy comes from (ACR_BaseDefences.Threats);
//   open ground        how far you can see out from the edge that way;
//   walls and fences   how much of the sector they close, and the gaps and gates in them;
//   roads in           where a road leaves the town (a gate, if it goes through a wall line);
//   what stands there  the map's sandbags, bunkers and towers, our positions and checkpoints.
// Weak(sector) sums that up. ACR_BaseLines runs its lines across the worst sectors first and what
// it places counts as cover (AddCover, AddLight), so the next goes where the town is weakest now.
// The hard rules' traces are here too: a gun or firing position needs a clear line of fire in the
// way it faces (a fan at its muzzle height).
class ACR_BaseGate : Managed
{
 vector m_vPos;      // where the road leaves the town
 vector m_vOut;      // unit XZ, away from the town along the road
 int m_iSector;
 bool m_bWalled;     // through a wall or fence line
}

// A place a gun fits, and how it may look from there.
class ACR_BaseSpot : Managed
{
 vector m_vPos;
 float m_fFacing;    // compass the gun looks along
 int m_iSector;      // the sector it looks into
 float m_fReach;     // clear m along the facing, up to OPEN_RANGE
 bool m_bNest;       // a nest's gun, low down, has a line of fire here
 bool m_bBunker;     // a bunker's ports have
 bool m_bTower;      // a tower's platform has (it sees over what stops the others)
 bool m_bGate;       // looks down a road in
 ref array<vector> m_aCuts = {}; // line pieces that make way for it
}

class ACR_BaseSurvey : Managed
{
 static const int SECTORS = 12;
 static const float SECTOR_DEG = 30;
 static const int BINS = 72;              // 5 degrees each, for walls
 static const float OPEN_RANGE = 150;     // m looked out from the edge, and a gun's reach counted
 static const float THREAT_FLOOR = 0.1;   // a sector away from every threat still counts a little
 static const float MIN_SPOT_THREAT = 0.2; // guns don't look for spots in sectors below this
 static const float LIGHT_TO_GUN = 0.3;   // how much sandbags in a sector take off a gun's reason to go there

 // Muzzle heights, m over the ground. Sandbags stand 1.35 m high, a man fires over them.
 static const float UP_NEST = 1.1;        // ACR_Fortify.MUZZLE_UP, measured on the nest's gun
 static const float UP_BUNKER = 1.3;
 static const float UP_TOWER = 4.5;
 static const float UP_BAGS = 1.45;
 // The fan: its middle, the rays MID_DEG and WIDE_DEG either side, m clear, and a last look NEAR_DEG
 // either side for what stands right beside the line of fire.
 static const float MID_DEG = 10;
 static const float WIDE_DEG = 20;
 static const float NEAR_DEG = 35;
 static const float NEAR_CLEAR = 10;

 static const float BUILDING_CLEAR = 10;  // m from a gun's middle to any building (its 6 m footprint + 4 m)
 static const float DOOR_CLEAR = 10;      // m from a nest's or bunker's middle to a door or gate of a building
 static const float NEST_BUILDING = 8;    // m from a nest's or bunker's bags' middle to any building (bags 5.5 m + 2.5)
 static const float NEST_ROAD = 5;        // m from a nest's place to a road's edge (it stands on the line, which keeps off)
 static const float TOWER_BUILDING = 6;   // a tower's are narrower: bags 3.8 m
 static const float TOWER_ROAD = 4.5;
 static const float TOWER_DOOR = 7;
 static const float GUN_ROAD = 7;         // m from a gun's middle to a road's edge
 static const float WALL_DOOR = 3;        // m from a sandbag piece to a door (the command post's own aside)
 static const float BLOCK_CLOSE = 6;      // m: a command post's bags that face another building this close go
 static const float FOOT_HALF = 6;        // m: nothing solid in this box round a gun's middle (fences, walls, rocks); nests measure 11-14 x 6-10 m, bunkers and towers 12-14 x 12-14

 ACR_Base m_Base;
 vector m_vCenter;
 float m_fRadius;
 int m_iMs;
 ref array<vector> m_aDirs = {};
 ref array<float> m_aWeights = {};
 ref array<ref ACR_BaseDefenceSeg> m_aSegs = {};
 ref array<IEntity> m_aBuildings = {};
 ref array<vector> m_aDoors = {};
 ref array<ref ACR_BaseGate> m_aGates = {};
 ref array<float> m_aEdge = {};           // per sector: m from the centre to the last building
 ref array<float> m_aThreat = {};
 ref array<float> m_aOpen = {};           // 0..1
 ref array<float> m_aWall = {};           // 0..1: how much of the sector is closed by walls and fences
 ref array<int> m_aGaps = {};             // gaps and gates in those
 ref array<int> m_aRoads = {};
 ref array<float> m_aCover = {};           // what's there and the guns
 ref array<float> m_aLight = {};           // sandbags, redoubts and checkpoints: to a gun they count for little
 ref array<vector> m_aWorkSpots = {};     // the map's works and ours
 ref array<vector> m_aRocks = {};         // big rocks: a line can end on one
 ref array<vector> m_aWallEnds = {};      // two per wall piece (tests draw them)
 int m_iTown;                             // buildings within the search radius
 int m_iWallPieces;
 int m_iMapWorks;                         // the map's sandbags, bunkers and towers seen
 int m_iOurWorks;                         // our positions and checkpoints near
 int m_iLineMs;                           // what its lines cost to plan
 int m_iWalks;                            // lines walked to get them
 int m_iPass;                             // 1: the approaches are being walked a second time
 protected ref map<int, ref array<ref ACR_BaseDefenceSeg>> m_mRoads = new map<int, ref array<ref ACR_BaseDefenceSeg>>();
 protected ref map<int, ref array<IEntity>> m_mHouses = new map<int, ref array<IEntity>>();
 ref map<string, int> m_mRefused = new map<string, int>(); // command post pieces left out, and why
 protected ref array<int> m_aBins = {};

 static int s_iTraces;                    // line-of-fire traces (tests, the log)
 protected static ref array<IEntity> s_aWalls = {};
 protected static ref array<IEntity> s_aWorks = {};
 protected static ref array<IEntity> s_aRocks = {};

 // ---------------------------------------------------------------- sectors

 static float Normal(float bearing)
 {
  while (bearing < 0) bearing += 360;
  while (bearing >= 360) bearing -= 360;
  return bearing;
 }

 static int Sector(float bearing)
 {
  int s = (int)(Normal(bearing + SECTOR_DEG * 0.5) / SECTOR_DEG);
  if (s >= SECTORS) s = 0;
  return s;
 }

 static float SectorBearing(int s)
 {
  return s * SECTOR_DEG;
 }

 static vector Dir(float bearing)
 {
  float rad = bearing * Math.DEG2RAD;
  return Vector(Math.Sin(rad), 0, Math.Cos(rad));
 }

 protected static int Wrapped(int s)
 {
  return (s + SECTORS) % SECTORS;
 }

 // The roads and buildings that way, for a walk from there (a town has hundreds, and a walk asks often).
 // Shared: kept per sector for the survey's life.
 void ThisWay(int sector, out array<ref ACR_BaseDefenceSeg> roads, out array<IEntity> houses)
 {
  if (!m_mRoads.Contains(sector))
  {
   array<ref ACR_BaseDefenceSeg> newRoads = new array<ref ACR_BaseDefenceSeg>();
   array<IEntity> newHouses = new array<IEntity>();
   vector way = Dir(SectorBearing(sector));
   foreach (ACR_BaseDefenceSeg seg : m_aSegs)
   {
    vector mid = (seg.m_vA + seg.m_vB) * 0.5 - m_vCenter;
    mid[1] = 0;
    float apart = mid.Length();
    if (apart > 60 && ACR_BaseDefences.DotXZ(way, mid * (1.0 / apart)) < -0.2) continue;
    newRoads.Insert(seg);
   }
   foreach (IEntity house : m_aBuildings)
   {
    if (!house) continue;
    vector toHouse = house.GetOrigin() - m_vCenter;
    toHouse[1] = 0;
    float away = toHouse.Length();
    if (away > 60 && ACR_BaseDefences.DotXZ(way, toHouse * (1.0 / away)) < -0.2) continue;
    newHouses.Insert(house);
   }
   m_mRoads.Set(sector, newRoads);
   m_mHouses.Set(sector, newHouses);
  }
  roads = m_mRoads.Get(sector);
  houses = m_mHouses.Get(sector);
 }

 // How far the town reaches that way, between its sectors' figures.
 float EdgeSmooth(float bearing)
 {
  float at = Normal(bearing) / SECTOR_DEG;
  int s = (int)at;
  float share = at - s;
  return m_aEdge[s % SECTORS] * (1 - share) + m_aEdge[(s + 1) % SECTORS] * share;
 }

 // How weak the town is that way, now: the threat, times the open ground (a wall takes half of that
 // away), the roads in and the gaps; less what already covers it.
 float Weak(int s)
 {
  float ground = (0.35 + 0.65 * m_aOpen[s]) * (1 - 0.5 * m_aWall[s]);
  float ways = 0.2 * Math.Min(m_aRoads[s], 2) + 0.15 * Math.Min(m_aGaps[s], 2);
  return m_aThreat[s] * (ground + ways) - m_aCover[s] - LIGHT_TO_GUN * m_aLight[s];
 }

 // The same to a sandbag position.
 float WeakLight(int s)
 {
  return Weak(s) - (1 - LIGHT_TO_GUN) * m_aLight[s];
 }

 // A site looking into sector s covers it, and its neighbours less.
 void AddCover(int s, float value)
 {
  m_aCover[s] = m_aCover[s] + value;
  int left = Wrapped(s - 1);
  int right = Wrapped(s + 1);
  m_aCover[left] = m_aCover[left] + value * 0.4;
  m_aCover[right] = m_aCover[right] + value * 0.4;
 }

 void AddLight(int s, float value)
 {
  m_aLight[s] = m_aLight[s] + value;
  int left = Wrapped(s - 1);
  int right = Wrapped(s + 1);
  m_aLight[left] = m_aLight[left] + value * 0.4;
  m_aLight[right] = m_aLight[right] + value * 0.4;
 }

 // The worst sectors now that a threat comes from, up to 'most'.
 void WeakSectors(notnull array<int> sectors, int most)
 {
  for (int made = 0; made < most; made++)
  {
   int worst = -1;
   for (int s = 0; s < SECTORS; s++)
   {
    if (sectors.Contains(s) || m_aThreat[s] < MIN_SPOT_THREAT) continue;
    if (worst < 0 || WeakLight(s) > WeakLight(worst)) worst = s;
   }
   if (worst < 0) return;
   sectors.Insert(worst);
  }
 }

 // Something a line can end on near a point: its word, "" if nothing.
 string AnchorNear(vector p, float within)
 {
  if (ACR_BaseDefences.BuildingGap(m_aBuildings, p, within) < within) return "building";
  int count = m_aWallEnds.Count();
  for (int i = 0; i + 1 < count; i += 2)
  {
   vector a = m_aWallEnds[i];
   if (vector.DistanceXZ(a, p) > within + 15) continue;
   ACR_BaseDefenceSeg seg = new ACR_BaseDefenceSeg();
   seg.m_vA = a;
   seg.m_vB = m_aWallEnds[i + 1];
   vector q;
   if (ACR_BaseDefences.SegDistance(seg, p, q) < within) return "wall";
  }
  foreach (vector work : m_aWorkSpots) if (vector.DistanceXZ(work, p) < within) return "work";
  foreach (vector rock : m_aRocks) if (vector.DistanceXZ(rock, p) < within + 2) return "rock";
  if (ACR_BaseDefences.RoadEdgeGap(m_aSegs, p) < 1.5) return "road";
  return "";
 }

 // The nearest thing a line going 'heading' from 'tip' could end on, within 'within' m and not behind it: a
 // building's near side, a wall, a big rock, a work, the end of a line we've chosen.
 bool NearestAnchor(vector tip, vector heading, ACR_BaseWork work, float within, out vector at, out string word)
 {
  float best = within;
  bool found = false;
  foreach (IEntity b : m_aBuildings)
  {
   if (!b || vector.DistanceXZ(b.GetOrigin(), tip) > within + 40) continue;
   vector mins, maxs;
   b.GetBounds(mins, maxs);
   vector local = b.CoordToLocal(tip);
   vector near = Vector(Math.Clamp(local[0], mins[0], maxs[0]), 0, Math.Clamp(local[2], mins[2], maxs[2]));
   TryAnchor(tip, heading, b.CoordToParent(near), "building", best, at, word, found);
  }
  int count = m_aWallEnds.Count();
  for (int i = 0; i + 1 < count; i += 2)
  {
   vector a = m_aWallEnds[i];
   if (vector.DistanceXZ(a, tip) > within + 15) continue;
   ACR_BaseDefenceSeg seg = new ACR_BaseDefenceSeg();
   seg.m_vA = a;
   seg.m_vB = m_aWallEnds[i + 1];
   vector q;
   ACR_BaseDefences.SegDistance(seg, tip, q);
   TryAnchor(tip, heading, q, "wall", best, at, word, found);
  }
  foreach (vector spot : m_aWorkSpots) TryAnchor(tip, heading, spot, "work", best, at, word, found);
  foreach (vector rock : m_aRocks) TryAnchor(tip, heading, rock, "rock", best, at, word, found);
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   if (line.m_aPieces.IsEmpty()) continue;
   TryAnchor(tip, heading, line.m_aPieces[0].m_vFrom, "line", best, at, word, found);
   TryAnchor(tip, heading, line.m_aPieces[line.m_aPieces.Count() - 1].m_vTo, "line", best, at, word, found);
  }
  return found;
 }

 protected void TryAnchor(vector tip, vector heading, vector point, string name, inout float best, inout vector at, inout string word, inout bool found)
 {
  float apart = vector.DistanceXZ(tip, point);
  if (apart >= best || apart < 0.5) return;
  if (ACR_BaseDefences.DotXZ(ACR_BaseDefences.FlatDir(tip, point), heading) < 0.2) return;
  best = apart;
  at = point;
  word = name;
  found = true;
 }

 // The threatened arc round the town seen from its middle: how much of it the lines cover, how much walls
 // and buildings do, and both. Shares, 0..1.
 void Coverage(ACR_BaseWork work, out float ours, out float existing, out float either)
 {
  array<int> lineBins = {};
  array<int> wallBins = {};
  for (int bin = 0; bin < BINS; bin++)
  {
   lineBins.Insert(0);
   wallBins.Insert(m_aBins[bin]);
  }
  foreach (ACR_BaseLine line : work.m_aLines)
   foreach (ACR_LinePiece piece : line.m_aPieces)
    MarkInto(lineBins, ACR_BaseDefences.Bearing(piece.m_vFrom - m_vCenter), ACR_BaseDefences.Bearing(piece.m_vTo - m_vCenter));
  foreach (IEntity b : m_aBuildings)
  {
   if (!b || vector.DistanceXZ(b.GetOrigin(), m_vCenter) > m_fRadius) continue;
   vector mins, maxs;
   b.GetBounds(mins, maxs);
   vector c0 = b.CoordToParent(Vector(mins[0], 0, mins[2]));
   vector c1 = b.CoordToParent(Vector(maxs[0], 0, maxs[2]));
   vector c2 = b.CoordToParent(Vector(mins[0], 0, maxs[2]));
   vector c3 = b.CoordToParent(Vector(maxs[0], 0, mins[2]));
   MarkInto(wallBins, ACR_BaseDefences.Bearing(c0 - m_vCenter), ACR_BaseDefences.Bearing(c1 - m_vCenter));
   MarkInto(wallBins, ACR_BaseDefences.Bearing(c2 - m_vCenter), ACR_BaseDefences.Bearing(c3 - m_vCenter));
  }
  int arc = 0;
  int forOurs = 0;
  int forWalls = 0;
  int forEither = 0;
  for (int k = 0; k < BINS; k++)
  {
   if (ACR_BaseDefences.ThreatDot(m_aDirs, m_aWeights, Dir((k + 0.5) * 5)) < MIN_SPOT_THREAT) continue;
   arc++;
   if (lineBins[k] > 0) forOurs++;
   if (wallBins[k] > 0) forWalls++;
   if (lineBins[k] > 0 || wallBins[k] > 0) forEither++;
  }
  arc = Math.Max(arc, 1);
  ours = forOurs * 1.0 / arc;
  existing = forWalls * 1.0 / arc;
  either = forEither * 1.0 / arc;
 }

 protected void MarkInto(notnull array<int> bins, float from, float to)
 {
  from = Normal(from);
  to = Normal(to);
  float span = Normal(to - from);
  if (span > 180)
  {
   float swap = from;
   from = to;
   to = swap;
   span = 360 - span;
  }
  for (float at = from; at <= from + span + 0.01; at += 5) bins[(int)(Normal(at) / 5) % BINS] = 1;
  bins[(int)(Normal(to) / 5) % BINS] = 1;
 }

 // One walk's outcome (the log, for why few lines come up).
 void LogWalk(string town, int sector, float past, string words)
 {
  PrintFormat("[ACR] BASEWORKS walk %1 sector %2 +%3 m: %4", town, (int)SectorBearing(sector), (int)past, words);
 }

 // Wall ends with nothing joined to them, on the threatened side: where gaps start.
 void FreeWallEnds(notnull array<vector> ends)
 {
  map<int, int> cells = new map<int, int>();
  foreach (vector end : m_aWallEnds)
  {
   int key = Cell(end, 0, 0);
   cells.Set(key, cells.Get(key) + 1);
  }
  foreach (vector at : m_aWallEnds)
  {
   if (ends.Count() >= 40) return;
   if (m_aThreat[Sector(ACR_BaseDefences.Bearing(at - m_vCenter))] < MIN_SPOT_THREAT) continue;
   int near = 0;
   for (int dx = -1; dx <= 1; dx++)
    for (int dz = -1; dz <= 1; dz++) near += cells.Get(Cell(at, dx, dz));
   if (near <= 2) ends.Insert(at);
  }
 }

 protected int Cell(vector p, int dx, int dz)
 {
  return ((int)(p[0] / 3) + dx) * 20011 + (int)(p[2] / 3) + dz;
 }

 // ---------------------------------------------------------------- the look

 static ACR_BaseSurvey Take(ACR_Base base, string owner)
 {
  ACR_BaseSurvey survey = new ACR_BaseSurvey();
  int started = System.GetTickCount();
  survey.Look(base, owner);
  survey.m_iMs = System.GetTickCount() - started;
  return survey;
 }

 protected void Look(ACR_Base base, string owner)
 {
  m_Base = base;
  m_vCenter = base.GetOrigin();
  m_fRadius = ACR_BaseDefences.SearchRadius(base);
  ACR_BaseDefences.Threats(base, owner, m_aDirs, m_aWeights);
  // Past the search radius: a line can run out there, and must see what stands there.
  ACR_BaseDefences.Roads(m_vCenter, m_fRadius + 70, m_aSegs);
  ACR_BaseDefences.Buildings(m_vCenter, m_fRadius + 80, m_aBuildings);
  for (int s = 0; s < SECTORS; s++)
  {
   m_aEdge.Insert(0);
   m_aThreat.Insert(Math.Max(ACR_BaseDefences.ThreatDot(m_aDirs, m_aWeights, Dir(SectorBearing(s))), THREAT_FLOOR));
   m_aOpen.Insert(0);
   m_aWall.Insert(0);
   m_aGaps.Insert(0);
   m_aRoads.Insert(0);
   m_aCover.Insert(0);
   m_aLight.Insert(0);
  }
  for (int bin = 0; bin < BINS; bin++) m_aBins.Insert(0);
  LookAtBuildings();
  LookOut();
  LookAtWalls();
  LookAtRoads();
  LookAtWorks();
 }

 // The town's edge per sector, and the buildings' doors.
 protected void LookAtBuildings()
 {
  foreach (IEntity b : m_aBuildings)
  {
   if (!b || vector.DistanceXZ(b.GetOrigin(), m_vCenter) > m_fRadius) continue;
   m_iTown++;
   vector mins, maxs;
   b.GetBounds(mins, maxs);
   array<vector> corners = {Vector(mins[0], 0, mins[2]), Vector(maxs[0], 0, mins[2]), Vector(maxs[0], 0, maxs[2]), Vector(mins[0], 0, maxs[2])};
   foreach (vector local : corners)
   {
    vector corner = b.CoordToParent(local);
    float reach = vector.DistanceXZ(corner, m_vCenter);
    int s = Sector(ACR_BaseDefences.Bearing(corner - m_vCenter));
    if (reach > m_aEdge[s]) m_aEdge[s] = reach;
   }
   IEntity child = b.GetChildren();
   while (child)
   {
    string part = FullName(child);
    if (part.Contains("/Doors/") && !part.Contains("_INT")) m_aDoors.Insert(child.GetOrigin());
    child = child.GetSibling();
   }
  }
  // A sector with no buildings: as its neighbours, or a fair way out from the middle.
  array<float> edges = {};
  foreach (float edge : m_aEdge) edges.Insert(edge);
  float least = Math.Clamp(m_Base.m_fRadius * 0.5, 20, 60);
  for (int t = 0; t < SECTORS; t++)
  {
   if (edges[t] > 0) continue;
   float neighbours = Math.Max(edges[Wrapped(t - 1)], edges[Wrapped(t + 1)]) * 0.7;
   m_aEdge[t] = Math.Max(neighbours, least);
  }
 }

 // How far you see out from the edge, three looks per sector.
 protected void LookOut()
 {
  array<float> turns = {-10, 0, 10};
  for (int s = 0; s < SECTORS; s++)
  {
   float bearing = SectorBearing(s);
   vector from = m_vCenter + Dir(bearing) * (m_aEdge[s] + 6);
   float sum = 0;
   foreach (float turn : turns) sum += ACR_BaseDefences.Sight(from, Dir(bearing + turn), OPEN_RANGE);
   m_aOpen[s] = sum / (turns.Count() * OPEN_RANGE);
  }
 }

 // The prefab's path, else the mesh's.
 static string FullName(IEntity e)
 {
  EntityPrefabData data = e.GetPrefabData();
  if (data && !data.GetPrefabName().IsEmpty()) return data.GetPrefabName();
  VObject mesh = e.GetVObject();
  if (mesh) return mesh.GetResourceName();
  return "";
 }

 protected static bool CollectSolid(IEntity e)
 {
  if (!e || !e.GetPhysics() || Tree.Cast(e) || ChimeraCharacter.Cast(e)) return true;
  string name = FullName(e);
  if (name.Contains("/Walls/") || name.Contains("Fence") || name.Contains("/Gates/")) s_aWalls.Insert(e);
  else if (name.Contains("Rock") || name.Contains("Stone") || name.Contains("Granite") || name.Contains("Boulder")) s_aRocks.Insert(e);
  else if (name.Contains("/Sandbags/") || name.Contains("/Fortifications/") || name.Contains("/Bunkers/") || name.Contains("GuardTower") || name.Contains("Watchtower") || name.Contains("GuardPost")) s_aWorks.Insert(e);
  return true;
 }

 // Walls and fences in bins round the town (those well inside it are yards), the gaps between them;
 // the map's works by sector.
 protected void LookAtWalls()
 {
  s_aWalls.Clear();
  s_aWorks.Clear();
  s_aRocks.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(m_vCenter, m_fRadius + 30, CollectSolid, null, EQueryEntitiesFlags.STATIC);
  foreach (IEntity wall : s_aWalls)
  {
   vector mins, maxs;
   wall.GetBounds(mins, maxs);
   vector middle = wall.CoordToParent((mins + maxs) * 0.5);
   float bearing = ACR_BaseDefences.Bearing(middle - m_vCenter);
   int s = Sector(bearing);
   if (vector.DistanceXZ(middle, m_vCenter) < m_aEdge[s] * 0.5) continue;
   vector size = maxs - mins;
   if (size[1] < 1) continue;
   // A gate piece is a way through, whatever it's fixed to.
   if (FullName(wall).Contains("Gate"))
   {
    m_aGaps[s] = m_aGaps[s] + 1;
    continue;
   }
   vector a = Vector(mins[0], 0, 0.5 * (mins[2] + maxs[2]));
   vector b = Vector(maxs[0], 0, 0.5 * (mins[2] + maxs[2]));
   if (size[2] > size[0])
   {
    a = Vector(0.5 * (mins[0] + maxs[0]), 0, mins[2]);
    b = Vector(0.5 * (mins[0] + maxs[0]), 0, maxs[2]);
   }
   a = wall.CoordToParent(a);
   b = wall.CoordToParent(b);
   m_iWallPieces++;
   m_aWallEnds.Insert(a);
   m_aWallEnds.Insert(b);
   MarkBins(ACR_BaseDefences.Bearing(a - m_vCenter), ACR_BaseDefences.Bearing(b - m_vCenter));
  }
  // Each sector's share of closed bins, and short open runs between closed ones.
  float inSector = BINS * 1.0 / SECTORS;
  for (int bin = 0; bin < BINS; bin++)
  {
   int t = Sector((bin + 0.5) * 5);
   if (m_aBins[bin] > 0) m_aWall[t] = m_aWall[t] + 1 / inSector;
   if (m_aBins[bin] > 0 || m_aBins[(bin + BINS - 1) % BINS] == 0) continue;
   int run = 0;
   while (run < 4 && m_aBins[(bin + run) % BINS] == 0) run++;
   if (run < 4 && m_aBins[(bin + run) % BINS] > 0) m_aGaps[t] = m_aGaps[t] + 1;
  }
  // The map's works: a few sandbags count for less than a bunker. Well inside the town they
  // cover nothing further out.
  array<float> mapCover = {};
  for (int c = 0; c < SECTORS; c++) mapCover.Insert(0);
  foreach (IEntity work : s_aWorks)
  {
   m_aWorkSpots.Insert(work.GetOrigin());
   float bearingTo = ACR_BaseDefences.Bearing(work.GetOrigin() - m_vCenter);
   int u = Sector(bearingTo);
   if (vector.DistanceXZ(work.GetOrigin(), m_vCenter) < m_aEdge[u] * 0.4) continue;
   m_iMapWorks++;
   // (Nobody mans them: some cover, not a reason to leave the sector open.)
   float worth = 0.04;
   if (!FullName(work).Contains("/Sandbags/")) worth = 0.15;
   mapCover[u] = Math.Min(mapCover[u] + worth, 0.3);
  }
  foreach (int v, float cover : mapCover) if (cover > 0) AddCover(v, cover);
  foreach (IEntity rock : s_aRocks)
  {
   vector rockMins, rockMaxs;
   rock.GetBounds(rockMins, rockMaxs);
   if (rockMaxs[0] - rockMins[0] < 3 && rockMaxs[2] - rockMins[2] < 3) continue;
   m_aRocks.Insert(rock.CoordToParent((rockMins + rockMaxs) * 0.5));
  }
  s_aWalls.Clear();
  s_aWorks.Clear();
  s_aRocks.Clear();
 }

 protected void MarkBins(float from, float to)
 {
  from = Normal(from);
  to = Normal(to);
  float span = Normal(to - from);
  if (span > 180)
  {
   float swap = from;
   from = to;
   to = swap;
   span = 360 - span;
  }
  for (float at = from; at <= from + span + 0.01; at += 5)
   m_aBins[(int)(Normal(at) / 5) % BINS] = 1;
  m_aBins[(int)(Normal(to) / 5) % BINS] = 1;
 }

 // Where roads leave the town: a segment that crosses a little past the edge of its sector.
 protected void LookAtRoads()
 {
  foreach (ACR_BaseDefenceSeg seg : m_aSegs)
  {
   float bearing = ACR_BaseDefences.Bearing((seg.m_vA + seg.m_vB) * 0.5 - m_vCenter);
   float line = m_aEdge[Sector(bearing)] + 12;
   float da = vector.DistanceXZ(seg.m_vA, m_vCenter);
   float db = vector.DistanceXZ(seg.m_vB, m_vCenter);
   if ((da < line) == (db < line)) continue;
   vector inner = seg.m_vA;
   vector outer = seg.m_vB;
   if (da > db)
   {
    inner = seg.m_vB;
    outer = seg.m_vA;
   }
   float fraction = (line - Math.Min(da, db)) / Math.Max(Math.AbsFloat(da - db), 0.01);
   vector pos = inner + (outer - inner) * fraction;
   pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
   bool seen;
   foreach (ACR_BaseGate other : m_aGates) if (vector.DistanceXZ(other.m_vPos, pos) < 25) seen = true;
   if (seen) continue;
   ACR_BaseGate gate = new ACR_BaseGate();
   gate.m_vPos = pos;
   gate.m_vOut = ACR_BaseDefences.FlatDir(inner, outer);
   gate.m_iSector = Sector(ACR_BaseDefences.Bearing(pos - m_vCenter));
   int bin = (int)(Normal(ACR_BaseDefences.Bearing(pos - m_vCenter)) / 5) % BINS;
   gate.m_bWalled = m_aBins[(bin + 2) % BINS] > 0 || m_aBins[(bin + BINS - 2) % BINS] > 0;
   m_aGates.Insert(gate);
   m_aRoads[gate.m_iSector] = m_aRoads[gate.m_iSector] + 1;
   if (gate.m_bWalled) m_aGaps[gate.m_iSector] = m_aGaps[gate.m_iSector] + 1;
  }
 }

 // Our positions and checkpoints near: they count where they look.
 protected void LookAtWorks()
 {
  foreach (ACR_Fortification position : ACR_Fortify.s_Positions)
  {
   if (!position || vector.DistanceXZ(position.m_vPos, m_vCenter) > m_fRadius + 100) continue;
   m_iOurWorks++;
   m_aWorkSpots.Insert(position.m_vPos);
   AddCover(Sector(position.m_fYaw), 0.25 + 0.15 * position.m_iTier);
  }
  foreach (ACR_Roadblock roadblock : ACR_Roadblocks.s_aRoadblocks)
  {
   if (!roadblock || vector.DistanceXZ(roadblock.m_vPos, m_vCenter) > m_fRadius + 200) continue;
   m_iOurWorks++;
   m_aWorkSpots.Insert(roadblock.m_vPos);
   AddCover(Sector(ACR_BaseDefences.Bearing(roadblock.m_vPos - m_vCenter)), 0.4);
  }
 }

 float DoorGap(vector p)
 {
  float gap = float.MAX;
  foreach (vector door : m_aDoors)
  {
   float d = vector.DistanceXZ(door, p);
   if (d < gap) gap = d;
  }
  return gap;
 }

 // ---------------------------------------------------------------- line of fire

 protected static bool FireFilter(notnull IEntity e)
 {
  if (ChimeraCharacter.Cast(e) || Vehicle.Cast(e)) return false;
  if (Tree.Cast(e))
  {
   // Bushes don't stop a burst further out (ACR_Fortify.LineOfFireAt asks for the first 15 m clear of them too).
   vector mins, maxs;
   e.GetBounds(mins, maxs);
   if (maxs[1] - mins[1] < 2.5) return false;
  }
  if (e.GetPrefabData() && e.GetPrefabData().GetPrefabName().Contains("Barbed")) return false;
  return true;
 }

 // The terrain, or a road's embankment.
 static bool IsGround(IEntity e)
 {
  return !e || GenericTerrainEntity.Cast(e) || e.ClassName() == "RoadEntity";
 }

 // Clear m from a muzzle along a bearing, up to range. Rising ground only stops a gun that can't
 // aim up it (ACR_Fortify.AimsUpClear): a hit on the terrain is tried again aimed at a man's height
 // above the ground at the far end.
 static float Reach(vector muzzle, float bearing, float range)
 {
  BaseWorld world = GetGame().GetWorld();
  vector dir = Dir(bearing);
  TraceParam trace = new TraceParam();
  trace.Start = muzzle + dir * 1.2;
  trace.End = muzzle + dir * range;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  s_iTraces++;
  float hit = world.TraceMove(trace, FireFilter);
  if (hit >= 1) return range;
  float clear = 1.2 + hit * (range - 1.2);
  if (!IsGround(trace.TraceEnt)) return clear;
  vector aim = muzzle + dir * range;
  aim[1] = world.GetSurfaceY(aim[0], aim[2]) + 1.0;
  float rise = aim[1] - muzzle[1];
  if (rise <= 0 || Math.Atan2(rise, range) * Math.RAD2DEG > ACR_Fortify.MAX_AIM_UP) return clear;
  vector up = aim - muzzle;
  up.Normalize();
  TraceParam aimed = new TraceParam();
  aimed.Start = muzzle + up * 1.2;
  aimed.End = aim;
  aimed.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  s_iTraces++;
  if (world.TraceMove(aimed, FireFilter) >= 1) return range;
  return clear;
 }

 // The fan at a place: 'middle' m clear straight along the bearing, 'mid' and 'wide' m on the rays
 // either side (0: not asked). Traces stop at the first that fails.
 static bool FanClear(vector place, float up, float bearing, float middle, float mid, float wide)
 {
  vector muzzle = place;
  muzzle[1] = GetGame().GetWorld().GetSurfaceY(place[0], place[2]) + up;
  if (Reach(muzzle, bearing, middle) < middle - 0.5) return false;
  if (mid > 0)
  {
   if (Reach(muzzle, bearing - MID_DEG, mid) < mid - 0.5 || Reach(muzzle, bearing + MID_DEG, mid) < mid - 0.5) return false;
  }
  if (wide > 0)
  {
   if (Reach(muzzle, bearing - WIDE_DEG, wide) < wide - 0.5 || Reach(muzzle, bearing + WIDE_DEG, wide) < wide - 0.5) return false;
   if (Reach(muzzle, bearing - NEAR_DEG, NEAR_CLEAR) < NEAR_CLEAR - 0.5 || Reach(muzzle, bearing + NEAR_DEG, NEAR_CLEAR) < NEAR_CLEAR - 0.5) return false;
  }
  return true;
 }

 // A nest's and a bunker's line of fire must also pass the check a Fortify position's guns do.
 static bool NestFan(vector place, float bearing, float reach = 45)
 {
  string why;
  // The gun stands 1.1 m forward of the nest's middle (measured): the same trace the built gun gets.
  vector gun = place + Dir(bearing) * 1.1;
  float middle = Math.Max(reach * 2 / 3, 22);
  return FanClear(gun, UP_NEST, bearing, reach, middle, 22) && ACR_Fortify.LineOfFireAt(gun, bearing, null, why);
 }

 // A bunker's firing slit is 4 m forward of its origin (measured).
 static bool BunkerFan(vector place, float bearing)
 {
  string why;
  vector slit = place + Dir(bearing) * 4;
  return FanClear(slit, UP_BUNKER, bearing, 40, 25, 22) && ACR_Fortify.LineOfFireAt(slit, bearing, null, why);
 }

 // A tower's platform stands 3 m forward of its origin (measured).
 static bool TowerFan(vector place, float bearing)
 {
  return FanClear(place + Dir(bearing) * 3, UP_TOWER, bearing, 40, 25, 22);
 }

 // ---------------------------------------------------------------- the command post

 protected void Refuse(string why)
 {
  m_mRefused.Set(why, m_mRefused.Get(why) + 1);
 }

 // A command post's long sandbags face the side they stand on. One that faces another building a
 // few metres on guards nothing: it goes, with the end pieces beside it. Bags inside an open fence
 // stay (ACR_BaseDefences.Fences): fences aren't buildings.
 void TrimPost(ACR_BaseDefencePos pos)
 {
  array<vector> dropped = {};
  for (int i = pos.m_aPiecePos.Count() - 1; i >= 0; i--)
  {
   vector size = ACR_BaseDefences.HalfSize(pos.m_aPiecePrefab[i]);
   if (!pos.m_aPieceWall[i] || size[0] < 1.5) continue;
   vector ahead = pos.m_aPiecePos[i] + Dir(pos.m_aPieceYaw[i]) * BLOCK_CLOSE;
   if (ACR_BaseDefences.BuildingGap(m_aBuildings, ahead, 2, pos.m_Host) > 1) continue;
   dropped.Insert(pos.m_aPiecePos[i]);
   pos.RemovePiece(i);
   Refuse("post bags facing a building");
  }
  if (dropped.IsEmpty()) return;
  for (int k = pos.m_aPiecePos.Count() - 1; k >= 0; k--)
  {
   if (!pos.m_aPieceWall[k]) continue;
   foreach (vector gone : dropped)
   {
    if (vector.DistanceXZ(gone, pos.m_aPiecePos[k]) > 3) continue;
    pos.RemovePiece(k);
    break;
   }
  }
 }

 // ---------------------------------------------------------------- the log

 string WorstWords(int count)
 {
  string words = "";
  array<int> used = {};
  for (int made = 0; made < count; made++)
  {
   int worst = -1;
   for (int s = 0; s < SECTORS; s++)
   {
    if (used.Contains(s)) continue;
    if (worst < 0 || Weak(s) > Weak(worst)) worst = s;
   }
   used.Insert(worst);
   words += string.Format("%1 %2 (threat %3, open %4 m, walled %5%%, %6 roads, %7 gaps); ", (int)SectorBearing(worst), Weak(worst).ToString(-1, 2), m_aThreat[worst].ToString(-1, 1), (int)(m_aOpen[worst] * OPEN_RANGE), (int)(m_aWall[worst] * 100), m_aRoads[worst], m_aGaps[worst]);
  }
  return words;
 }

 string Words()
 {
  return string.Format("%1: %2 buildings, %3 doors, %4 wall pieces, %5 of the map's works, %6 of ours, %7 roads in | weakest: %8", m_Base.m_sName, m_iTown, m_aDoors.Count(), m_iWallPieces, m_iMapWorks, m_iOurWorks, m_aGates.Count(), WorstWords(3));
 }

 string FitWords()
 {
  return WorstWords(2);
 }

 string RefusedWords()
 {
  string words = "";
  foreach (string why, int count : m_mRefused) words += string.Format("%1 x%2, ", why, count);
  return words;
 }
}
