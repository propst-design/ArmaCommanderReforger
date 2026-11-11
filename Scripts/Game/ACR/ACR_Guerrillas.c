// FIA guerrillas (#89): beside the pockets (ACR_Pockets), small FIA cells hit the roads the main
// sides use and hold out in ordinary houses. No FIA commander: each cell is a defender post
// (ACR_Defenders) with a reserve that deploys when an enemy comes within its range and packs up
// once they are gone, so a cell costs nothing until someone comes near.
//  - Road ambush: its site is a stretch of road between bases, away from both starting bases
//    (BBB_Roads routes between bases, the most travelled stretches first). The men lie in wait
//    set back from the road (25-70 m off it) in the best cover that sees the kill zone (a treeline
//    over open grass, Jacob 6 Nov: men standing in the open by a barricade were no ambush), each
//    crouched on his own post facing it (ACR_Garrison posts, as ACR_Ambush's), holding their fire
//    until an enemy enters the kill zone, comes close or hits one of them; most bring a PKM on its
//    light tripod, dug in on the best-covered spot with a man on it (BBB keeps him there,
//    ACR_Defenders.CrewGunsOf; another man takes it if he falls, Reman); sometimes they block the road first with a log barricade
//    (ACR_Roadblocks), so a convoy has to slow in the kill zone. Hit and run: after a short fight
//    or heavy losses they pull back away from the road, leaving the gun, and pack up (the gun goes
//    with them unless someone else is on it), and set up on another road later.
//  - House: a few men in the windows of an ordinary building away from every base
//    (ACR_Garrison posts), quiet until enemies come close; after a fight they move on too.
//  - Hideout: FIA's home in a small isolated group of buildings (a farm or hamlet, not a village,
//    not by a base), 6-8 men in the windows. It never moves and fights to the last. While it
//    stands it recruits from the empty land round it and spends the recruits on its cells, new
//    cells and new pockets (ACR_FIAGrowth, #96). A side
//    that fights it has found it: a "FIA hideout" ring on its map, and its AI sends a squad to
//    clear it (ACR_Insurgency). Destroyed, the cells round it are no longer fed.
// Unseen until found like every enemy: a side only learns of a cell by contact (ACR_Intel).
// A cell that is wiped out is gone for the match. Options: -acrGuerrillas off|few|many (setup row
// "FIA guerrillas": default few with the setup screen, off otherwise, as the pockets), and
// -acrFIAHidden 1 ("Hidden FIA"): no "FIA resistance at" message, a pocket is only found by
// entering it (ACR_BaseSystem).
class ACR_GuerrillaCell : Managed
{
 bool m_bHouse;          // a house (a hideout is one too)
 bool m_bHideout;
 int m_iStart;           // men at the start (a hideout tops a cell back up to this)
 ref array<string> m_aKnownBy = {}; // hideout: sides that know where it is
 vector m_vKillZone;     // ambush: the road point it watches; house: the building; hideout: its buildings' middle
 vector m_vRoadDir;      // ambush: along the road there
 float m_fRoadWidth;
 ACR_Roadblock m_Block;  // the barricade it laid this time (none: a plain ambush)
 IEntity m_Gun;          // ambush: its tripod gun this time (none: rifles only)
 int m_iOut;             // men deployed (a loss springs it)
 bool m_bSprung;
 string m_sSprung;       // what sprang it
 float m_fSprungAt;
 int m_iAliveAtSpring;
 bool m_bWithdrawing;
 float m_fReadyAt;       // after a fight: back in action from then, elsewhere
 int m_iMoves;
 float m_fPool;          // hideout: recruits to spend (ACR_FIAGrowth)
}

class ACR_Guerrillas
{
 static const float AMBUSH_RANGE = 700;  // an enemy this close: the ambush gets set (vehicles come fast)
 static const float HOUSE_RANGE = 400;   // as a base garrison
 static const float KILL_RADIUS = 45;    // an enemy this close to the kill zone springs the ambush
 static const float CLOSE = 30;          // ...or this close to one of the men
 static const float HOUSE_CLOSE = 120;   // the house opens fire on enemies this close
 static const float HIT_S = 90;          // an ambush pulls back this long after it was sprung ...
 static const float HOUSE_HIT_S = 240;   // ... a house later
 static const float FALLBACK = 350;      // m back from the road when it pulls back
 static const float CLEAR = 250;         // pulled back: packs up with no enemy this close to its men
 static const float MOVE_ON_MIN_S = 600; // after a fight: 10-20 min until it sets up elsewhere
 static const float MOVE_ON_MAX_S = 1200;
 static const float BLOCK_CHANCE = 0.4;  // ambushes that block the road first
 static const float HIDE_CLEAR = 18;     // m from any road where they lie in wait (cc89guer1: 12 let men onto it)
 static const float HIDE_NEAR = 25;      // m off the road's edge the hiding place is looked for ...
 static const float HIDE_FAR = 70;       // ... to this far: back in the treeline rather than out in the grass
 static const float COVER_WEIGHT = 1.5;  // a hiding place's score for each tree, bush or rock round it (ACR_Ambush.Cover)
 static const float GUN_CHANCE = 0.7;    // ambushes that bring a tripod machine gun
 static const ResourceName GUN = "{723870DBB19D30B0}Prefabs/Weapons/Tripods/Tripod_6T5_PKM.et";
 static bool s_bGunForTest;              // srv:guerrillas: every ambush brings its gun
 static const int REMAN_EVERY_S = 8;     // an empty gun manned again this often ...
 static const float REMAN_REACH = 30;    // ... by the nearest man on foot this close
 static const float SPACING = 700;       // m between two cells
 static const float BASE_CLEAR = 150;    // m outside any base's circle
 static const int MAX_ROUTES = 20;       // road searches at the start (cc89rs2: 14 left too few sites to move on to)
 static const int MAX_HOUSE_LOOKS = 30;  // building searches at the start
 static const float AMBUSH_HQ_CLEAR = 900; // m from a starting base for an ambush (houses: as the pockets)

 static ref array<vector> s_aRoads;  // ambush sites to pick from (road points)
 static ref array<float> s_aWeights; // ... and how many of the routes pass there
 static ref array<vector> s_aHouses; // buildings to pick from
 static ref array<vector> s_aStarts; // the starting bases (held at Apply)
 protected static float s_fHouseClear;     // m from a starting base for a house

 static const float HIDEOUT_CLUSTER = 60;     // m: buildings this close to one another make a group
 static const int HIDEOUT_MIN_BUILDINGS = 3;
 static const float HIDEOUT_ISOLATION = 300;  // more than HIDEOUT_MAX_AROUND buildings this close: a village
 static const int HIDEOUT_MAX_AROUND = 10;
 static const float HIDEOUT_SPACING = 1500;   // m between two hideouts
 static const int MAX_HIDEOUT_LOOKS = 40;     // spots looked at for building groups at the start
 static const float HIDEOUT_AREA = 60;        // its men hold the windows this far round its middle
 static const float HIDEOUT_CLOSE = 150;      // it opens fire on enemies this close
 static const float HIDEOUT_FOUND = 400;      // a side with men this close when it opens fire has found it
 static const float SUPPLY_REACH = 3000;      // it feeds the cells this close (ACR_FIAGrowth.TopUp)
 protected static int s_iWantedCells;         // cells (not hideouts) placed at the start
 protected static bool s_bManyOption;         // -acrGuerrillas many (ACR_FIAGrowth's caps)

 static int WantedCells() { return s_iWantedCells; }
 // m from a starting base for houses, hideouts and (ACR_FIAGrowth) spread pockets.
 static float StartClearance() { return s_fHouseClear; }

 static int CountOption(bool half, out int houses, bool quarter = false)
 {
  string value = ACR_TestHooks.Get().Guerrillas();
  if (value == "" && !ACR_Options.Get("acrGuerrillas", value)) value = "";
  if (value == "")
  {
   if (!ACR_Setup.Wanted()) return 0;
   value = "few";
  }
  value.ToLower();
  houses = 0;
  s_bManyOption = value == "many";
  if (value == "off" || value == "0") return 0;
  int roads = 4;
  houses = 3;
  if (value == "many") { roads = 8; houses = 6; }
  if (quarter)
  {
   roads = Math.Ceil(roads * 0.25);
   houses = Math.Ceil(houses * 0.34);
  }
  else if (half)
  {
   roads = Math.Ceil(roads * 0.5);
   houses = Math.Ceil(houses * 0.5);
  }
  return roads;
 }

 static bool Hidden()
 {
  string value;
  if (!ACR_Options.Get("acrFIAHidden", value)) return false;
  return value == "1" || value == "on" || value == "true";
 }

 // Server, after ACR_Pockets.Apply (ACR_Skirmish.ApplyOnce).
 static void Apply()
 {
  s_aRoads.Clear();
  s_aWeights.Clear();
  s_aHouses.Clear();
  s_aStarts.Clear();
  s_mTraffic.Clear();
  s_mTrafficAt.Clear();
  s_iSeconds = 0;
  s_iWantedCells = 0;
  ACR_FIAGrowth.Reset();
  ACR_Depots.Reset();
  int size = ACR_Skirmish.SizeOption();
  if (ACR_Skirmish.s_Active) size = ACR_Skirmish.s_Active.m_eSize;
  if (size == ACR_ESkirmishSize.FLASHPOINT) return;
  int houses;
  int roads = CountOption(size == ACR_ESkirmishSize.HALF, houses, size == ACR_ESkirmishSize.QUARTER);
  if (roads + houses <= 0) return;
  string faction = ACR_Pockets.FACTION;
  if (ACR_FactionSelection.West() == faction || ACR_FactionSelection.East() == faction || !GetGame().GetFactionManager().GetFactionByKey(faction))
  {
   PrintFormat("[ACR] GUERRILLAS none: %1 is a main side or not in this world", faction);
   return;
  }
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases || manager.m_Bases.GetFaction(faction).m_aGarrison.IsEmpty())
  {
   PrintFormat("[ACR] GUERRILLAS none: no %1 garrison soldiers", faction);
   return;
  }
  int started = System.GetTickCount();
  float clearance = Survey();
  int placed;
  for (int i = 0; i < roads; i++) if (AddCell(false)) placed++;
  for (int j = 0; j < houses; j++) if (AddCell(true)) placed++;
  s_iWantedCells = placed;
  ACR_FIAGrowth.Apply(s_bManyOption);
  PrintFormat("[ACR] GUERRILLAS %1 cells of %2 wanted (%3 road sites, %4 houses to pick from, %5 m from starting bases, %6 ms)", placed, roads + houses, s_aRoads.Count(), s_aHouses.Count(), (int)clearance, System.GetTickCount() - started);
  int hideoutsStarted = System.GetTickCount();
  int wanted = roads / 2;
  array<vector> sites = {};
  FindHideouts(wanted, sites);
  foreach (vector site : sites) AddHideout(site);
  PrintFormat("[ACR] GUERRILLAS %1 hideouts of %2 wanted (%3 ms)", sites.Count(), wanted, System.GetTickCount() - hideoutsStarted);
  ACR_Depots.Apply();
 }

 // ---------------------------------------------------------------- hideouts

 // Up to 'wanted' hideout sites: the middle of a group of HIDEOUT_MIN_BUILDINGS+ buildings with
 // few others round it, looked for 0.8-2.5 km out from random bases, as far from the starting
 // bases as the houses, outside every base, apart from the cells and each other; the bigger
 // groups and those further off the roads first.
 static void FindHideouts(int wanted, notnull array<vector> sites)
 {
  if (wanted <= 0) return;
  array<ACR_Base> bases = {};
  foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_bRetired && !base.m_bHidden) bases.Insert(base);
  if (bases.IsEmpty()) return;
  array<vector> found = {};
  array<float> scores = {};
  for (int look = 0; look < MAX_HIDEOUT_LOOKS; look++)
  {
   float angle = Math.RandomFloat(0, Math.PI2);
   vector spot = bases.GetRandomElement().GetOrigin() + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * Math.RandomFloat(800, 2500);
   if (!Usable(spot, s_fHouseClear)) continue;
   vector middle;
   int members;
   if (!BuildingGroup(spot, middle, members)) continue;
   if (!Usable(middle, s_fHouseClear)) continue;
   float score = members + Math.Min(3, ACR_Minefields.RoadClearance(middle) / 50);
   int slot = 0;
   while (slot < scores.Count() && scores[slot] >= score) slot++;
   found.InsertAt(middle, slot);
   scores.InsertAt(score, slot);
  }
  foreach (vector candidate : found)
  {
   if (sites.Count() >= wanted) break;
   bool apart = !Crowded(candidate, null);
   foreach (vector taken : sites) if (vector.DistanceXZ(taken, candidate) < HIDEOUT_SPACING) apart = false;
   if (apart) sites.Insert(candidate);
  }
 }

 // The biggest group of buildings round 'spot', if it is one: HIDEOUT_MIN_BUILDINGS+ within
 // HIDEOUT_CLUSTER of one of them, no more than HIDEOUT_MAX_AROUND within HIDEOUT_ISOLATION.
 protected static bool BuildingGroup(vector spot, out vector middle, out int members)
 {
  array<IEntity> near = {};
  ACR_Garrison.BuildingsWithin(spot, HIDEOUT_ISOLATION, near);
  if (near.Count() < HIDEOUT_MIN_BUILDINGS) return false;
  IEntity best;
  int bestCount;
  foreach (IEntity building : near)
  {
   int count;
   foreach (IEntity other : near) if (vector.DistanceXZ(building.GetOrigin(), other.GetOrigin()) <= HIDEOUT_CLUSTER) count++;
   if (count <= bestCount) continue;
   bestCount = count;
   best = building;
  }
  if (bestCount < HIDEOUT_MIN_BUILDINGS) return false;
  array<IEntity> around = {};
  ACR_Garrison.BuildingsWithin(best.GetOrigin(), HIDEOUT_ISOLATION, around);
  if (around.Count() > HIDEOUT_MAX_AROUND) return false;
  vector sum;
  members = 0;
  foreach (IEntity member : around)
  {
   if (vector.DistanceXZ(member.GetOrigin(), best.GetOrigin()) > HIDEOUT_CLUSTER) continue;
   sum = sum + member.GetOrigin();
   members++;
  }
  middle = sum * (1.0 / members);
  return !BBB_Terrain.IsWaterAt(middle);
 }

 static ACR_DefenderPost AddHideout(vector site)
 {
  int men = Math.RandomIntInclusive(6, 8);
  ACR_DefenderPost post = ACR_Defenders.Add(ACR_Pockets.FACTION, site, string.Format("FIA hideout %1", ACR_Manager.Grid(site)), HIDEOUT_AREA);
  post.m_iReserve = men;
  ACR_GuerrillaCell cell = new ACR_GuerrillaCell();
  cell.m_bHouse = true;
  cell.m_bHideout = true;
  cell.m_iStart = men;
  cell.m_vKillZone = site;
  post.m_Cell = cell;
  post.m_fSpawnRange = HOUSE_RANGE;
  PrintFormat("[ACR] GUERRILLAS %1: %2 men", post.m_sName, men);
  return post;
 }

 // Hideouts standing (the cells' homes).
 static void Hideouts(notnull array<ACR_DefenderPost> posts)
 {
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
   if (post.m_Cell && post.m_Cell.m_bHideout && !post.m_bGone) posts.Insert(post);
 }

 // The command map: a ring on each hideout the side knows.
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || !post.m_Cell.m_bHideout || post.m_bGone || !post.m_Cell.m_aKnownBy.Contains(faction)) continue;
   positions.Insert(post.m_vPos);
   radii.Insert(HIDEOUT_AREA + 40);
   labels.Insert("FIA hideout");
  }
  ACR_FIAIntel.Zones(faction, positions, radii, labels);
 }

 // 'side' learns where the hideout is (ACR_Insurgency: found by a fight, or FIA's friend).
 static bool Know(ACR_DefenderPost post, string side)
 {
  if (side.IsEmpty() || post.m_Cell.m_aKnownBy.Contains(side)) return false;
  post.m_Cell.m_aKnownBy.Insert(side);
  return true;
 }

 // The places cells pick from: points on the roads out of each starting base to its nearest
 // bases (where the sides drive first), then between other bases, weighted by how many of the
 // routes searched pass there; and buildings by those roads. Ambushes keep AMBUSH_HQ_CLEAR from
 // the starting bases, houses as far as the pockets; all outside every base. Later in the match
 // the cells follow the traffic (Resite).
 protected static float Survey()
 {
  array<ACR_Base> held = {};
  array<ACR_Base> all = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_bHidden) continue;
   all.Insert(base);
   if (!base.m_sOwner.IsEmpty() && !base.m_bPocket)
   {
    held.Insert(base);
    // Quick start's handed-out bases aren't where a side starts from, and with most of the map
    // handed out their clearances left no room (playtest pn16: 0 hideouts of 4, 2 houses, 8 cells
    // of 14). Every base still keeps its own circle clear (Usable).
    if (!ACR_QuickStart.Assigned(base)) s_aStarts.Insert(base.GetOrigin());
   }
  }
  float gap = 0;
  foreach (ACR_Base a : held)
   foreach (ACR_Base b : held)
    if (a.m_sOwner != b.m_sOwner) gap = Math.Max(gap, vector.DistanceXZ(a.GetOrigin(), b.GetOrigin()));
  float clearance = Math.Min(ACR_Pockets.MIN_HQ_DISTANCE, Math.Max(1000, gap * 0.25));
  s_fHouseClear = clearance;
  // First out of each starting base to its three nearest bases 0.8-3 km away, then from random
  // bases to a random neighbour as far: the roads both sides will use.
  array<vector> froms = {};
  array<vector> tos = {};
  foreach (ACR_Base start : held)
  {
   array<ACR_Base> nearest = {};
   foreach (ACR_Base other : all)
   {
    float gapTo = vector.DistanceXZ(start.GetOrigin(), other.GetOrigin());
    if (other == start || gapTo < 800 || gapTo > 3000) continue;
    int at = 0;
    while (at < nearest.Count() && vector.DistanceXZ(start.GetOrigin(), nearest[at].GetOrigin()) < gapTo) at++;
    nearest.InsertAt(other, at);
   }
   for (int q = 0; q < nearest.Count() && q < 3; q++)
   {
    froms.Insert(start.GetOrigin());
    tos.Insert(nearest[q].GetOrigin());
   }
  }
  array<int> order = {};
  foreach (int n, ACR_Base unused : all) order.Insert(n);
  while (froms.Count() < MAX_ROUTES && !order.IsEmpty())
  {
   int pick = Math.RandomInt(0, order.Count());
   ACR_Base from = all[order[pick]];
   order.Remove(pick);
   array<ACR_Base> near = {};
   foreach (ACR_Base to : all)
   {
    float d = vector.DistanceXZ(from.GetOrigin(), to.GetOrigin());
    if (to != from && d >= 800 && d <= 3000) near.Insert(to);
   }
   if (near.IsEmpty()) continue;
   froms.Insert(from.GetOrigin());
   tos.Insert(near.GetRandomElement().GetOrigin());
  }
  foreach (int route, vector routeFrom : froms)
  {
   array<vector> legs = {};
   int nodes, millis;
   if (!BBB_Roads.Route(routeFrom, tos[route], legs, nodes, millis)) continue;
   foreach (vector leg : legs)
   {
    if (!Usable(leg, AMBUSH_HQ_CLEAR)) continue;
    bool merged;
    foreach (int k, vector known : s_aRoads)
    {
     if (vector.DistanceXZ(known, leg) > 60) continue;
     s_aWeights[k] = s_aWeights[k] + 1;
     merged = true;
     break;
    }
    if (merged) continue;
    s_aRoads.Insert(leg);
    s_aWeights.Insert(1);
   }
  }
  // Ordinary buildings by those roads (one per road site at most, a sample of the sites: each
  // look is a sphere query).
  int looks;
  foreach (vector site : s_aRoads)
  {
   if (looks >= MAX_HOUSE_LOOKS) break;
   // Roads near a starting base (ambushes may sit there) have no house far enough out.
   if (!Usable(site, clearance - 150) || Math.RandomFloat01() > 0.5) continue;
   looks++;
   array<IEntity> buildings = {};
   ACR_Garrison.BuildingsWithin(site, 150, buildings);
   foreach (IEntity building : buildings)
   {
    if (!Usable(building.GetOrigin(), clearance)) continue;
    s_aHouses.Insert(building.GetOrigin());
    break;
   }
  }
  return clearance;
 }

 // An ambush (or an IED, ACR_IED) could watch the road here: AMBUSH_HQ_CLEAR from the starting bases, outside every base.
 static bool UsableAmbush(vector at) { return Usable(at, AMBUSH_HQ_CLEAR); }

 protected static bool Usable(vector at, float clearance)
 {
  if (BBB_Terrain.IsWaterAt(at)) return false;
  foreach (vector start : s_aStarts) if (vector.DistanceXZ(at, start) < clearance) return false;
  foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_bRetired && !base.m_bHidden && vector.DistanceXZ(at, base.GetOrigin()) < base.m_fRadius + BASE_CLEAR) return false;
  return true;
 }

 // ---------------------------------------------------------------- following the traffic
 // FIA watches the roads: every TRAFFIC_EVERY_S the main sides' moving vehicles on a road are
 // counted in TRAFFIC_CELL squares. Every RESITE_EVERY_S one ambush cell that is waiting (not out,
 // not moving on after a fight) on a quiet road moves to the busiest stretch that is clearly
 // busier, usable (as Survey), SPACING from other cells, with a hiding place and no enemy near
 // right now; then the counts halve, so old traffic fades.
 static const float TRAFFIC_CELL = 100;
 static const int TRAFFIC_EVERY_S = 5;
 static const int RESITE_EVERY_S = 300;
 static const float TRAFFIC_REACH = 250;  // a stretch: the counts this close to a point
 static const float RESITE_MIN = 4;       // a stretch needs this many sightings to draw a cell
 static const float MOVING = 3;           // m/s: a vehicle this fast is travelling
 static ref map<int, float> s_mTraffic;
 static ref map<int, vector> s_mTrafficAt;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_aRoads = {};
  s_aWeights = {};
  s_aHouses = {};
  s_aStarts = {};
  s_mTraffic = new map<int, float>();
  s_mTrafficAt = new map<int, vector>();
 }
 protected static int s_iSeconds;

 static void NoteTraffic(vector pos, float weight = 1)
 {
  int key = (int)(pos[0] / TRAFFIC_CELL) * 10000 + (int)(pos[2] / TRAFFIC_CELL);
  float count;
  s_mTraffic.Find(key, count);
  s_mTraffic.Set(key, count + weight);
  if (!s_mTrafficAt.Contains(key)) s_mTrafficAt.Set(key, pos);
 }

 static float TrafficNear(vector pos)
 {
  float sum;
  foreach (int key, float count : s_mTraffic)
   if (vector.DistanceXZ(s_mTrafficAt.Get(key), pos) <= TRAFFIC_REACH) sum += count;
  return sum;
 }

 protected static void SampleTraffic(ACR_Manager manager)
 {
  array<IEntity> seen = {};
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == ACR_Pockets.FACTION) continue;
   // A side FIA spares draws no ambush; one it is hostile to draws them twice as hard.
   float weight = ACR_Insurgency.TrafficWeight(squad.GetFaction().GetFactionKey());
   if (weight <= 0) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   if (!vehicle || seen.Contains(vehicle)) continue;
   seen.Insert(vehicle);
   Physics physics = vehicle.GetPhysics();
   if (!physics || physics.GetVelocity().Length() < MOVING) continue;
   if (ACR_Minefields.RoadClearance(vehicle.GetOrigin()) > 8) continue;
   NoteTraffic(vehicle.GetOrigin(), weight);
  }
 }

 static bool Resite()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  bool moved;
  // The busiest stretches first.
  array<vector> places = {};
  array<float> counts = {};
  foreach (int key, float unused : s_mTraffic)
  {
   vector at = s_mTrafficAt.Get(key);
   float busy = TrafficNear(at);
   if (busy < RESITE_MIN) continue;
   int slot = 0;
   while (slot < counts.Count() && counts[slot] >= busy) slot++;
   places.InsertAt(at, slot);
   counts.InsertAt(busy, slot);
  }
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (moved || places.IsEmpty()) break;
   ACR_GuerrillaCell cell = post.m_Cell;
   if (!cell || cell.m_bHouse || post.m_Group || post.m_bGone || !Ready(post)) continue;
   float here = TrafficNear(cell.m_vKillZone);
   foreach (int i, vector place : places)
   {
    if (counts[i] < here * 2 + RESITE_MIN) break;
    if (!AmbushSiteOk(place, post)) continue;
    if (manager && ACR_Defenders.EnemyWithin(manager, post.m_sFaction, place, HOUSE_RANGE, true)) continue;
    string from = ACR_Manager.Grid(cell.m_vKillZone);
    SetRoad(post, place);
    post.m_sName = string.Format("FIA ambush %1", ACR_Manager.Grid(cell.m_vKillZone));
    PrintFormat("[ACR] GUERRILLAS ambush from %1 (%2 sightings) to the traffic at %3 (%4)", from, (int)here, ACR_Manager.Grid(cell.m_vKillZone), (int)counts[i]);
    moved = true;
    break;
   }
  }
  array<int> faded = {};
  foreach (int key, float count : s_mTraffic)
  {
   if (count * 0.5 < 0.5) faded.Insert(key);
   else s_mTraffic.Set(key, count * 0.5);
  }
  foreach (int gone : faded)
  {
   s_mTraffic.Remove(gone);
   s_mTrafficAt.Remove(gone);
  }
  return moved;
 }

 // An ambush could sit at 'at': usable, SPACING from every cell but 'except', a hiding place by the road.
 static bool AmbushSiteOk(vector at, ACR_DefenderPost except)
 {
  if (!Usable(at, AMBUSH_HQ_CLEAR) || Crowded(at, except)) return false;
  vector road, dir, hide;
  float width;
  return RoadSite(at, road, dir, width, hide, true);
 }

 // Another cell within SPACING of 'at'.
 protected static bool Crowded(vector at, ACR_DefenderPost except)
 {
  foreach (ACR_DefenderPost other : ACR_Defenders.s_Posts)
   if (other != except && other.m_Cell && !other.m_bGone && vector.DistanceXZ(other.m_Cell.m_vKillZone, at) < SPACING) return true;
  return false;
 }

 // A new cell at a free site, false with none left (at the start, and ACR_FIAGrowth.RaiseCell).
 static bool AddCell(bool house)
 {
  vector site;
  if (!PickSite(house, site)) return false;
  int men = Math.RandomIntInclusive(4, 6);
  string kind = "ambush";
  if (house) { men = Math.RandomIntInclusive(3, 5); kind = "house"; }
  ACR_DefenderPost post = ACR_Defenders.Add(ACR_Pockets.FACTION, site, string.Format("FIA %1 %2", kind, ACR_Manager.Grid(site)), 25);
  post.m_iReserve = men;
  ACR_GuerrillaCell cell = new ACR_GuerrillaCell();
  cell.m_bHouse = house;
  cell.m_iStart = men;
  cell.m_vKillZone = site;
  post.m_Cell = cell;
  post.m_fSpawnRange = HOUSE_RANGE;
  if (!house) SetRoad(post, site);
  PrintFormat("[ACR] GUERRILLAS %1: %2 men", post.m_sName, men);
  return true;
 }

 // A site from the survey, the most travelled roads likelier, SPACING from every other cell. A
 // road site with nowhere to hide beside it (roads all round, open ground) leaves the list.
 protected static bool PickSite(bool house, out vector site, float spacing = SPACING)
 {
  for (int attempt = 0; attempt < 12; attempt++)
  {
   int index = PickIndex(house, spacing);
   if (index < 0) return false;
   if (house)
   {
    site = s_aHouses[index];
    return true;
   }
   vector road, dir, hide;
   float width;
   if (RoadSite(s_aRoads[index], road, dir, width, hide, true))
   {
    site = s_aRoads[index];
    return true;
   }
   s_aRoads.Remove(index);
   s_aWeights.Remove(index);
  }
  return false;
 }

 protected static int PickIndex(bool house, float spacing)
 {
  array<vector> places = s_aRoads;
  if (house) places = s_aHouses;
  array<int> free = {};
  float total;
  foreach (int i, vector place : places)
  {
   bool crowded;
   foreach (ACR_DefenderPost other : ACR_Defenders.s_Posts)
    if (other.m_Cell && !other.m_bGone && vector.DistanceXZ(other.m_Cell.m_vKillZone, place) < spacing) crowded = true;
   if (crowded) continue;
   free.Insert(i);
   if (house) total += 1;
   else total += s_aWeights[i];
  }
  if (free.IsEmpty()) return -1;
  float roll = Math.RandomFloat(0, total);
  foreach (int index : free)
  {
   float weight = 1;
   if (!house) weight = s_aWeights[index];
   roll -= weight;
   if (roll <= 0) return index;
  }
  return free[free.Count() - 1];
 }

 // The road at 'at' and where to lie in wait beside it (HideSite); false with nowhere to hide.
 // any: only whether there is one (the first that will do, not the best: site checks).
 protected static bool RoadSite(vector at, out vector road, out vector dir, out float width, out vector hide, bool any = false)
 {
  width = 6;
  road = at;
  vector point;
  if (ACR_Minefields.Snap(at, 40, point, dir, width)) road = point;
  else dir = Vector(1, 0, 0);
  return HideSite(road, dir, width, hide, any);
 }

 // An ambush watches the road at 'site' (a site PickSite checked); it lies in wait off it.
 protected static void SetRoad(ACR_DefenderPost post, vector site)
 {
  ACR_GuerrillaCell cell = post.m_Cell;
  vector road, dir, hide;
  float width;
  RoadSite(site, road, dir, width, hide);
  cell.m_vKillZone = road;
  cell.m_vRoadDir = dir;
  cell.m_fRoadWidth = width;
  post.m_vPos = hide;
 }

 // Beside the road, HIDE_NEAR-HIDE_FAR m off its edge (and a little up or down it) and HIDE_CLEAR
 // from any road, with the kill zone in sight, where trees, bushes and rocks hide it and on higher
 // ground (ACR_Ambush's cover and sight rules). Cover counts most: a treeline 60 m back beats open
 // grass by the road (Jacob 6 Nov); with as much cover, nearer first. False with no such place.
 // any: the first that will do.
 static bool HideSite(vector road, vector dir, float width, out vector best, bool any = false)
 {
  vector side = Vector(dir[2], 0, -dir[0]);
  float bestScore = -1000;
  bool found;
  array<float> sides = {1, -1};
  array<float> offs = {25, 32, 40, 48, 56, 63, 70};
  array<float> alongs = {0, -10, 10, -20, 20};
  foreach (float s : sides)
  {
   foreach (float o : offs)
   {
    foreach (float a : alongs)
    {
     vector candidate = ACR_Manager.Ground(road + side * (s * (width * 0.5 + o)) + dir * a);
     if (BBB_Terrain.IsWaterAt(candidate) || ACR_Minefields.RoadClearance(candidate) < HIDE_CLEAR) continue;
     if (ACR_BaseSystem.Covered(candidate) || !ACR_Ambush.SeesKillZone(candidate, road)) continue;
     if (any)
     {
      best = candidate;
      return true;
     }
     float score = ACR_Ambush.Cover(candidate) * COVER_WEIGHT + ACR_Ambush.Height(candidate, road) - (o - HIDE_NEAR) * 0.03 - Math.AbsFloat(a) * 0.02;
     if (score <= bestScore) continue;
     bestScore = score;
     best = candidate;
     found = true;
    }
   }
  }
  return found;
 }

 // 'count' spots for the men round the hiding place, best first: in sight of the kill zone where
 // they can be, by the most cover and the highest ground, ACR_Ambush.SPACING apart, never on or by
 // a road, under a roof or inside what stands there (playtest pn20: men lay inside a bunker's mound),
 // with room to crouch. Short of such places the rest stay where they spawned.
 protected static void MenSpots(ACR_GuerrillaCell cell, vector hide, int count, notnull array<vector> spots)
 {
  spots.Clear();
  vector away = hide - cell.m_vKillZone;
  away[1] = 0;
  if (away.LengthSq() < 0.01) away = Vector(cell.m_vRoadDir[2], 0, -cell.m_vRoadDir[0]);
  away.Normalize();
  array<vector> places = {};
  array<float> scores = {};
  array<float> depths = {0, -7, 7};
  for (float a = -24; a <= 24; a += 6)
  {
   foreach (float depth : depths)
   {
    vector place = ACR_Manager.Ground(hide + cell.m_vRoadDir * a + away * depth);
    if (BBB_Terrain.IsWaterAt(place) || ACR_Minefields.RoadClearance(place) < HIDE_CLEAR - 4 || ACR_BaseSystem.Covered(place)) continue;
    if (!ACR_BaseSystem.RoomAt(place)) continue;
    float score = ACR_Ambush.Cover(place) * COVER_WEIGHT + ACR_Ambush.Height(place, cell.m_vKillZone) - Math.AbsFloat(a) * 0.02 - Math.AbsFloat(depth) * 0.03;
    if (!ACR_Ambush.SeesKillZone(place, cell.m_vKillZone)) score -= 20;
    places.Insert(place);
    scores.Insert(score);
   }
  }
  while (spots.Count() < count)
  {
   int best = -1;
   foreach (int i, vector open : places)
   {
    if (best >= 0 && scores[i] <= scores[best]) continue;
    bool clear = true;
    foreach (vector taken : spots) if (vector.DistanceXZ(taken, open) < ACR_Ambush.SPACING) clear = false;
    if (clear) best = i;
   }
   if (best < 0) break;
   spots.Insert(places[best]);
   places.Remove(best);
   scores.Remove(best);
  }
 }

 // ---------------------------------------------------------------- ACR_Defenders hooks

 // May it deploy now? (Not while moving on after a fight.)
 static bool Ready(ACR_DefenderPost post)
 {
  return GetGame().GetWorld().GetWorldTime() >= post.m_Cell.m_fReadyAt;
 }

 static float Range(ACR_DefenderPost post)
 {
  if (post.m_Cell.m_bHouse) return HOUSE_RANGE;
  return AMBUSH_RANGE;
 }

 // Just deployed (ACR_Defenders.SpawnGuards round the site): into position, holding fire.
 static void Deployed(ACR_Manager manager, ACR_DefenderPost post)
 {
  ACR_GuerrillaCell cell = post.m_Cell;
  SCR_AIGroup group = post.m_Group;
  cell.m_bSprung = false;
  cell.m_bWithdrawing = false;
  cell.m_iOut = manager.m_Bases.GuardsAlive(group);
  SetCombatMode(group, EAIGroupCombatMode.HOLD_FIRE);
  if (cell.m_bHouse)
  {
   // Into the building's windows (as a base garrison), the rest round it.
   float area = 25;
   if (cell.m_bHideout) area = HIDEOUT_AREA;
   int posts = ACR_Garrison.AssignArea(group, group.GetLeaderAgent(), cell.m_vKillZone, area, cell.m_iOut);
   if (posts > 0) Order(group, ACR_Squad.WP_GARRISON, cell.m_vKillZone, area, true);
   PrintFormat("[ACR] GUERRILLAS %1 deployed: %2 men in the house, %3 window posts", post.m_sName, cell.m_iOut, posts);
   return;
  }
  // Each man on his own spot round the hiding place (MenSpots), the best one for the gun if they
  // brought it; the others crouched on posts facing the kill zone under a garrison waypoint, as a
  // player's ambush (ACR_Ambush.Spots). A Defend left them standing about in the open (Jacob 6 Nov).
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  array<IEntity> men = {};
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (body && ACR_Respawn.IsAlive(body)) men.Insert(body);
  }
  array<vector> spots = {};
  MenSpots(cell, post.m_vPos, men.Count(), spots);
  string gun = "no gun";
  int first = 0;
  if (men.Count() >= 3 && !spots.IsEmpty() && (s_bGunForTest || Math.RandomFloat01() < GUN_CHANCE))
  {
   // The last man (never the leader) to the gun.
   IEntity gunner = men[men.Count() - 1];
   cell.m_Gun = SpawnGun(spots[0], cell.m_vKillZone);
   if (cell.m_Gun)
   {
    gunner.SetOrigin(ACR_Manager.Ground(spots[0] - Facing(spots[0], cell.m_vKillZone) * 1.2));
    GetGame().GetCallqueue().CallLater(SeatGunner, 1000, false, cell.m_Gun, gunner);
    GetGame().GetCallqueue().CallLater(SeatGunner, 3000, false, cell.m_Gun, gunner);
    men.Remove(men.Count() - 1);
    first = 1;
    gun = string.Format("a PKM, cover %1", ACR_Ambush.Cover(spots[0]));
   }
  }
  array<ref ACR_GarrisonSpot> held = {};
  vector middle = post.m_vPos;
  int onSpots;
  foreach (int i, IEntity man : men)
  {
   if (first + i >= spots.Count()) break;
   vector spot = spots[first + i];
   man.SetOrigin(spot);
   ACR_GarrisonSpot place = new ACR_GarrisonSpot();
   place.m_Pos = spot;
   place.m_Look = Facing(spot, cell.m_vKillZone);
   held.Insert(place);
   if (onSpots == 0) middle = spot;
   onSpots++;
  }
  float radius = 15;
  if (!held.IsEmpty() && ACR_Garrison.PostsAt(group, held, middle, radius) > 0) Order(group, ACR_Squad.WP_GARRISON, middle, radius, true);
  // None: a tight Defend (a wide one let the men walk onto the road, cc89guer1).
  else Order(group, ACR_Squad.WP_DEFEND, post.m_vPos, 10, false);
  string blocked = "plain";
  if (Math.RandomFloat01() < BLOCK_CHANCE) blocked = Block(post);
  PrintFormat("[ACR] GUERRILLAS %1 deployed: %2 men lying in wait %3 m off the road, %4 on posts, cover %5 at the hiding place (%6; %7)", post.m_sName, cell.m_iOut, (int)ACR_Minefields.RoadClearance(post.m_vPos), onSpots, ACR_Ambush.Cover(post.m_vPos), gun, blocked);
 }

 // A horizontal unit vector from 'from' towards 'to'.
 protected static vector Facing(vector from, vector to)
 {
  vector look = to - from;
  look[1] = 0;
  if (look.LengthSq() < 0.01) return Vector(0, 0, 1);
  look.Normalize();
  return look;
 }

 // The tripod gun on 'at', aimed at the kill zone. Its bushes stay: it is hiding.
 protected static IEntity SpawnGun(vector at, vector killZone)
 {
  Resource resource = Resource.Load(GUN);
  if (!resource.IsValid()) return null;
  vector look = Facing(at, killZone);
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(Math.Atan2(look[0], look[2]) * Math.RAD2DEG, 0, 0), params.Transform);
  params.Transform[3] = ACR_Manager.Ground(at);
  return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
 }

 // A moment after the gun spawned (its seat set up): its gunner onto it. Missed, Reman puts a
 // man of the cell on it.
 protected static void SeatGunner(IEntity gun, IEntity gunner)
 {
  ChimeraCharacter body = ChimeraCharacter.Cast(gunner);
  if (!gun || !body || !ACR_Respawn.IsAlive(body) || body.IsInVehicle()) return;
  BaseCompartmentSlot slot = ACR_Defenders.FreeTurret(gun);
  CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
  if (slot && access) access.GetInVehicle(gun, slot, true, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
 }

 // The gun a cell's men hold while they lie in wait or fight (none once pulling back): BBB keeps its
 // gunner on it (ACR_Defenders.CrewGunsOf, ACR_BBBHooks.GetHeldGuns, BBB_HeldGuns.Keep).
 static IEntity HeldGun(ACR_DefenderPost post)
 {
  if (!post || !post.m_Cell || post.m_Cell.m_bWithdrawing) return null;
  return post.m_Cell.m_Gun;
 }

 // Every REMAN_EVERY_S while they hold the gun (HeldGun): standing empty (its gunner shot, or not
 // seated at deploy), the nearest man of the cell on foot within REMAN_REACH takes it.
 protected static void Reman(ACR_DefenderPost post)
 {
  IEntity gun = HeldGun(post);
  if (!gun) return;
  BaseCompartmentSlot slot = ACR_Defenders.FreeTurret(gun);
  if (!slot) return;
  array<AIAgent> agents = {};
  post.m_Group.GetAgents(agents);
  ChimeraCharacter best;
  float nearest = REMAN_REACH;
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !ACR_Respawn.IsAlive(body) || body.IsInVehicle()) continue;
   CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
   if (!access || access.IsGettingIn() || access.IsGettingOut()) continue;
   float d = vector.DistanceXZ(body.GetOrigin(), gun.GetOrigin());
   if (d >= nearest) continue;
   nearest = d;
   best = body;
  }
  if (best && best.GetCompartmentAccessComponent().GetInVehicle(gun, slot, true, -1, ECloseDoorAfterActions.LEAVE_OPEN, false))
   PrintFormat("[ACR] GUERRILLAS %1: a man %2 m off takes the gun", post.m_sName, (int)nearest);
 }

 // The gun stays when they pull back; it goes when they pack up, unless someone else is on it.
 protected static void DropGun(ACR_GuerrillaCell cell)
 {
  IEntity gun = cell.m_Gun;
  cell.m_Gun = null;
  if (!gun) return;
  // Someone else on it (its own gunner was packed up a moment ago and may still sit there).
  BaseCompartmentSlot seat = ACR_Defenders.TurretSeat(gun);
  IEntity occupant;
  if (seat) occupant = seat.GetOccupant();
  if (occupant && ACR_Respawn.IsAlive(occupant) && ACR_Intel.FactionKeyOf(occupant) != ACR_Pockets.FACTION) return;
  SCR_EntityHelper.DeleteEntityAndChildren(gun);
 }

 // A log barricade across the road at the kill zone.
 protected static string Block(ACR_DefenderPost post)
 {
  ACR_GuerrillaCell cell = post.m_Cell;
  vector road;
  float yaw;
  string refusal;
  if (!ACR_Roadblocks.ValidateSite(cell.m_vKillZone, road, yaw, refusal, post.m_sFaction)) return "no barricade: " + refusal;
  cell.m_Block = ACR_Roadblocks.SpawnRoadblock(post.m_sFaction, road, yaw, post.m_sName, ACR_ERoadblockType.BARRICADE, false);
  if (!cell.m_Block) return "no barricade: it didn't spawn";
  return "the road blocked";
 }

 // Every second (ACR_Manager.Tick): spring, fight, pull back; watch the traffic.
 static void Tick()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases || s_aStarts.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  s_iSeconds++;
  if (s_iSeconds % TRAFFIC_EVERY_S == 0) SampleTraffic(manager);
  if (s_iSeconds % RESITE_EVERY_S == 0) Resite();
  ACR_FIAGrowth.Tick();
  ACR_IED.Tick();
  ACR_Raid.Tick();
  ACR_Depots.Tick();
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || !post.m_Group) continue;
   ACR_GuerrillaCell cell = post.m_Cell;
   int alive = manager.m_Bases.GuardsAlive(post.m_Group);
   if (s_iSeconds % REMAN_EVERY_S == 0) Reman(post);
   if (!cell.m_bSprung)
   {
    string why = SpringReason(manager, post, alive);
    if (why != "") Spring(manager, post, alive, why, now);
    continue;
   }
   // A hideout holds its ground to the last.
   if (cell.m_bWithdrawing || cell.m_bHideout) continue;
   float hit = HIT_S;
   if (cell.m_bHouse) hit = HOUSE_HIT_S;
   if (alive * 2 <= cell.m_iAliveAtSpring || now - cell.m_fSprungAt >= hit * 1000) Withdraw(post, alive);
  }
 }

 protected static string SpringReason(ACR_Manager manager, ACR_DefenderPost post, int alive)
 {
  ACR_GuerrillaCell cell = post.m_Cell;
  if (alive < cell.m_iOut) return "a man hit";
  if (cell.m_bHouse)
  {
   float close = HOUSE_CLOSE;
   if (cell.m_bHideout) close = HIDEOUT_CLOSE;
   if (ACR_Defenders.EnemyWithin(manager, post.m_sFaction, cell.m_vKillZone, close, true)) return "enemies close";
   return "";
  }
  if (ACR_Defenders.EnemyWithin(manager, post.m_sFaction, cell.m_vKillZone, KILL_RADIUS, true)) return "enemies in the kill zone";
  IEntity leader;
  if (post.m_Group.GetLeaderAgent()) leader = post.m_Group.GetLeaderAgent().GetControlledEntity();
  if (leader && ACR_Defenders.EnemyWithin(manager, post.m_sFaction, leader.GetOrigin(), CLOSE, true)) return "enemies too close";
  return "";
 }

 protected static void Spring(ACR_Manager manager, ACR_DefenderPost post, int alive, string why, float now)
 {
  ACR_GuerrillaCell cell = post.m_Cell;
  cell.m_bSprung = true;
  cell.m_sSprung = why;
  cell.m_fSprungAt = now;
  cell.m_iAliveAtSpring = alive;
  SetCombatMode(post.m_Group, EAIGroupCombatMode.FIRE_AT_WILL);
  PrintFormat("[ACR] GUERRILLAS %1 sprung (%2), %3 men", post.m_sName, why, alive);
  if (cell.m_bHideout) ACR_Insurgency.HideoutFound(post);
 }

 // Hit and run: away from the road (or from the house), then packed up once clear (StayOut).
 protected static void Withdraw(ACR_DefenderPost post, int alive)
 {
  ACR_GuerrillaCell cell = post.m_Cell;
  cell.m_bWithdrawing = true;
  vector away = post.m_vPos - cell.m_vKillZone;
  if (cell.m_bHouse) away = Vector(Math.RandomFloat(-1, 1), 0, Math.RandomFloat(-1, 1));
  away[1] = 0;
  if (away.LengthSq() < 0.01) away = Vector(1, 0, 0);
  away.Normalize();
  vector fallback = ACR_Manager.Ground(post.m_vPos + away * FALLBACK);
  // Off their posts (or out of the windows), and the gunner off the gun: they leave it.
  ACR_Garrison.Release(post.m_Group);
  LeaveGun(post);
  SetCombatMode(post.m_Group, EAIGroupCombatMode.RETURN_FIRE);
  Order(post.m_Group, ACR_Squad.WP_MOVE, fallback, 20, false);
  PrintFormat("[ACR] GUERRILLAS %1 pulls back to %2 (%3 of %4 left)", post.m_sName, ACR_Manager.Grid(fallback), alive, cell.m_iAliveAtSpring);
 }

 // Pulling back: a man of the cell on its gun gets off it (HeldGun no longer lists it, so BBB
 // doesn't put him back).
 protected static void LeaveGun(ACR_DefenderPost post)
 {
  IEntity gun = post.m_Cell.m_Gun;
  if (!gun || !post.m_Group) return;
  array<AIAgent> agents = {};
  post.m_Group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !body.IsInVehicle()) continue;
   CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
   if (!access || !access.GetCompartment() || access.GetCompartment().GetOwner() != gun) continue;
   access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
  }
 }

 // While deployed (ACR_Defenders.Tick): stays out while an enemy is near; pulled back, until
 // none is near its men. Any enemy, spared or not: men in a fight don't vanish.
 static bool StayOut(ACR_Manager manager, ACR_DefenderPost post)
 {
  ACR_GuerrillaCell cell = post.m_Cell;
  if (!cell.m_bWithdrawing) return ACR_Defenders.EnemyWithin(manager, post.m_sFaction, post.m_vPos, Range(post) + 50);
  array<AIAgent> agents = {};
  post.m_Group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man && ACR_Respawn.IsAlive(man) && ACR_Defenders.EnemyWithin(manager, post.m_sFaction, man.GetOrigin(), CLEAR)) return true;
  }
  return false;
 }

 // Packed up (ACR_Defenders.Tick): its barricade and gun go; after a fight it moves on to a new
 // site later; with nobody left the cell is gone.
 static void PackedUp(ACR_DefenderPost post)
 {
  ACR_GuerrillaCell cell = post.m_Cell;
  DropGun(cell);
  if (cell.m_Block)
  {
   ACR_Roadblocks.DismantleRoadblock(cell.m_Block);
   cell.m_Block = null;
  }
  bool fought = cell.m_bSprung;
  cell.m_bSprung = false;
  cell.m_bWithdrawing = false;
  if (post.m_iReserve <= 0)
  {
   PrintFormat("[ACR] GUERRILLAS %1 wiped out", post.m_sName);
   // Whoever wiped it out gets a lead on the hideout that fed it (ACR_FIAIntel, #96).
   if (!cell.m_bHideout) ACR_FIAIntel.Wiped(post.m_vPos, ACR_FIAIntel.SupplierOf(cell.m_vKillZone), "a wiped-out cell");
   ACR_Defenders.Remove(post);
   if (cell.m_bHideout) ACR_Insurgency.HideoutDestroyed(post);
   return;
  }
  if (!fought || cell.m_bHideout) return;
  vector site;
  vector old = cell.m_vKillZone;
  cell.m_vKillZone = vector.Zero; // its own old site doesn't crowd the pick
  // Short of free sites, closer to the others rather than back where it fought; the survey's
  // sites follow a few routes, so seven cells can crowd every one at half spacing too.
  bool picked = PickSite(cell.m_bHouse, site);
  if (!picked) picked = PickSite(cell.m_bHouse, site, SPACING * 0.5);
  if (!picked)
  {
   cell.m_vKillZone = old; // now it does: anywhere but where it fought
   picked = PickSite(cell.m_bHouse, site, SPACING * 0.2);
  }
  if (!picked) site = old;
  cell.m_vKillZone = site;
  post.m_vPos = site;
  if (!cell.m_bHouse) SetRoad(post, site);
  cell.m_iMoves++;
  cell.m_fReadyAt = GetGame().GetWorld().GetWorldTime() + Math.RandomFloat(MOVE_ON_MIN_S, MOVE_ON_MAX_S) * 1000;
  post.m_sName = string.Format("FIA %1 %2", Kind(cell), ACR_Manager.Grid(cell.m_vKillZone));
  PrintFormat("[ACR] GUERRILLAS moving on: %1 men, now %2 in %3 s", post.m_iReserve, post.m_sName, (int)((cell.m_fReadyAt - GetGame().GetWorld().GetWorldTime()) / 1000));
 }

 static string Kind(ACR_GuerrillaCell cell)
 {
  if (cell.m_bHideout) return "hideout";
  if (cell.m_bHouse) return "house";
  return "ambush";
 }

 // ---------------------------------------------------------------- helpers

 static void SetCombatMode(SCR_AIGroup group, EAIGroupCombatMode mode)
 {
  if (!group) return;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (utility && utility.GetCombatModeExternal() != mode) utility.SetCombatMode(mode);
 }

 protected static void DeleteWaypointLater(AIWaypoint waypoint)
 {
  if (waypoint) SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
 }

 // The group's waypoints replaced by one.
 static void Order(SCR_AIGroup group, ResourceName prefab, vector at, float radius, bool fastInit)
 {
  array<AIWaypoint> old = {};
  group.GetWaypoints(old);
  foreach (AIWaypoint waypoint : old)
  {
   group.RemoveWaypoint(waypoint);
   // A moment later, as ACR_Squad.DeleteWaypointSoon: a running Defend still reads it this frame
   // ("Defend Waypoint not provided to the node!").
   GetGame().GetCallqueue().CallLater(DeleteWaypointLater, ACR_Squad.WAYPOINT_DELETE_DELAY_MS, false, waypoint);
  }
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = at;
  AIWaypoint waypoint = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(Resource.Load(prefab), null, params));
  if (!waypoint) return;
  SCR_DefendWaypoint defend = SCR_DefendWaypoint.Cast(waypoint);
  if (defend && fastInit) defend.SetFastInit(true);
  waypoint.SetCompletionRadius(radius);
  group.AddWaypoint(waypoint);
 }
}
