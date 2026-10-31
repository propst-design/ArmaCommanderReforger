// FIA guerrillas (#89): beside the pockets (ACR_Pockets), small FIA cells hit the roads the main
// sides use and hold out in ordinary houses. No FIA commander: each cell is a defender post
// (ACR_Defenders) with a reserve that deploys when an enemy comes within its range and packs up
// once they are gone, so a cell costs nothing until someone comes near.
//  - Road ambush: its site is a stretch of road between bases, away from both starting bases
//    (BBB_Roads routes between bases, the most travelled stretches first). The men lie in wait
//    off the road, holding their fire, until an enemy enters the kill zone, comes close or hits
//    one of them; sometimes they block the road first with a log barricade (ACR_Roadblocks), so a
//    convoy has to slow in the kill zone. Hit and run: after a short fight or heavy losses they
//    pull back away from the road and pack up, and set up on another road later.
//  - House: a few men in the windows of an ordinary building away from every base
//    (ACR_Garrison posts), quiet until enemies come close; after a fight they move on too.
//  - Hideout: FIA's home in a small isolated group of buildings (a farm or hamlet, not a village,
//    not by a base), 6-8 men in the windows. It never moves and fights to the last. While it
//    stands it feeds the cells round it (a man back to a short-handed cell within 3 km every
//    10 min) and raises a new cell every 30 min while there are fewer than at the start. A side
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
 int m_iOut;             // men deployed (a loss springs it)
 bool m_bSprung;
 string m_sSprung;       // what sprang it
 float m_fSprungAt;
 int m_iAliveAtSpring;
 bool m_bWithdrawing;
 float m_fReadyAt;       // after a fight: back in action from then, elsewhere
 int m_iMoves;
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
 static const float SPACING = 700;       // m between two cells
 static const float BASE_CLEAR = 150;    // m outside any base's circle
 static const int MAX_ROUTES = 20;       // road searches at the start (cc89rs2: 14 left too few sites to move on to)
 static const int MAX_HOUSE_LOOKS = 30;  // building searches at the start
 static const float AMBUSH_HQ_CLEAR = 900; // m from a starting base for an ambush (houses: as the pockets)

 static ref array<vector> s_aRoads = {};  // ambush sites to pick from (road points)
 static ref array<float> s_aWeights = {}; // ... and how many of the routes pass there
 static ref array<vector> s_aHouses = {}; // buildings to pick from
 static ref array<vector> s_aStarts = {}; // the starting bases (held at Apply)
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
 static const float SUPPLY_REACH = 3000;      // it feeds the cells this close
 static const int SUPPLY_EVERY_S = 600;
 static const int RAISE_EVERY_S = 1800;
 protected static int s_iWantedCells;         // cells (not hideouts) placed at the start

 static int CountOption(bool half, out int houses)
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
  if (value == "off" || value == "0") return 0;
  int roads = 4;
  houses = 3;
  if (value == "many") { roads = 8; houses = 6; }
  if (half)
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
  int size = ACR_Skirmish.SizeOption();
  if (ACR_Skirmish.s_Active) size = ACR_Skirmish.s_Active.m_eSize;
  if (size == ACR_ESkirmishSize.FLASHPOINT) return;
  int houses;
  int roads = CountOption(size == ACR_ESkirmishSize.HALF, houses);
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
  PrintFormat("[ACR] GUERRILLAS %1 cells of %2 wanted (%3 road sites, %4 houses to pick from, %5 m from starting bases, %6 ms)", placed, roads + houses, s_aRoads.Count(), s_aHouses.Count(), (int)clearance, System.GetTickCount() - started);
  int hideoutsStarted = System.GetTickCount();
  int wanted = roads / 2;
  array<vector> sites = {};
  FindHideouts(wanted, sites);
  foreach (vector site : sites) AddHideout(site);
  PrintFormat("[ACR] GUERRILLAS %1 hideouts of %2 wanted (%3 ms)", sites.Count(), wanted, System.GetTickCount() - hideoutsStarted);
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

 // Every SUPPLY_EVERY_S: each hideout sends a man to each short-handed idle cell in reach.
 static int Supply()
 {
  array<ACR_DefenderPost> hideouts = {};
  Hideouts(hideouts);
  int sent;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   ACR_GuerrillaCell cell = post.m_Cell;
   if (!cell || cell.m_bHideout || post.m_bGone || post.m_Group || post.m_iReserve >= cell.m_iStart) continue;
   foreach (ACR_DefenderPost hideout : hideouts)
   {
    if (vector.DistanceXZ(hideout.m_vPos, cell.m_vKillZone) > SUPPLY_REACH) continue;
    post.m_iReserve++;
    sent++;
    PrintFormat("[ACR] GUERRILLAS %1 sends a man to %2 (%3 of %4)", hideout.m_sName, post.m_sName, post.m_iReserve, cell.m_iStart);
    break;
   }
  }
  return sent;
 }

 // Every RAISE_EVERY_S: with a hideout standing and fewer cells than at the start, a new one.
 static bool Raise()
 {
  array<ACR_DefenderPost> hideouts = {};
  Hideouts(hideouts);
  if (hideouts.IsEmpty()) return false;
  int cells;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts) if (post.m_Cell && !post.m_Cell.m_bHideout && !post.m_bGone) cells++;
  if (cells >= s_iWantedCells) return false;
  bool house = Math.RandomFloat01() < 0.4;
  if (!AddCell(house) && !AddCell(!house)) return false;
  PrintFormat("[ACR] GUERRILLAS a hideout raised a new cell (%1 of %2)", cells + 1, s_iWantedCells);
  return true;
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
 static ref map<int, float> s_mTraffic = new map<int, float>();
 static ref map<int, vector> s_mTrafficAt = new map<int, vector>();
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
  return RoadSite(at, road, dir, width, hide);
 }

 // Another cell within SPACING of 'at'.
 protected static bool Crowded(vector at, ACR_DefenderPost except)
 {
  foreach (ACR_DefenderPost other : ACR_Defenders.s_Posts)
   if (other != except && other.m_Cell && !other.m_bGone && vector.DistanceXZ(other.m_Cell.m_vKillZone, at) < SPACING) return true;
  return false;
 }

 // A new cell at a free site, false with none left.
 protected static bool AddCell(bool house)
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
   if (RoadSite(s_aRoads[index], road, dir, width, hide))
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
 protected static bool RoadSite(vector at, out vector road, out vector dir, out float width, out vector hide)
 {
  width = 6;
  road = at;
  vector point;
  if (ACR_Minefields.Snap(at, 40, point, dir, width)) road = point;
  else dir = Vector(1, 0, 0);
  return HideSite(road, dir, width, hide);
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

 // Beside the road, 20-35 m off its edge and HIDE_CLEAR from any road, with the kill zone in
 // sight, where trees, bushes and rocks hide it and on higher ground (ACR_Ambush's cover and
 // sight rules). False with no such place.
 static bool HideSite(vector road, vector dir, float width, out vector best)
 {
  vector side = Vector(dir[2], 0, -dir[0]);
  float bestScore = -1000;
  bool found;
  array<float> sides = {1, -1};
  array<float> offs = {20, 25, 30, 35};
  foreach (float s : sides)
  {
   foreach (float o : offs)
   {
    vector candidate = ACR_Manager.Ground(road + side * (s * (width * 0.5 + o)));
    if (BBB_Terrain.IsWaterAt(candidate) || ACR_Minefields.RoadClearance(candidate) < HIDE_CLEAR) continue;
    if (ACR_BaseSystem.Covered(candidate) || !ACR_Ambush.SeesKillZone(candidate, road)) continue;
    float score = ACR_Ambush.Cover(candidate) + ACR_Ambush.Height(candidate, road) - Math.AbsFloat(o - 20) * 0.05;
    if (score <= bestScore) continue;
    bestScore = score;
    best = candidate;
    found = true;
   }
  }
  return found;
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
  // Spread along the road on the site's side, each with the kill zone in sight where he can.
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  vector across = post.m_vPos - cell.m_vKillZone;
  across[1] = 0;
  float off = across.Length();
  across.Normalize();
  foreach (int i, AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (!man) continue;
   float along = (i - (agents.Count() - 1) * 0.5) * ACR_Ambush.SPACING;
   // Out from the road until clear of it (a bend can bring another stretch close) and of what stands
   // on the ground (playtest pn20: men lay inside a bunker's mound). None clear: he stays where he spawned.
   for (int step = 0; step < 4; step++)
   {
    vector spot = ACR_Manager.Ground(cell.m_vKillZone + cell.m_vRoadDir * along + across * (off + step * 6));
    if (BBB_Terrain.IsWaterAt(spot) || ACR_Minefields.RoadClearance(spot) < HIDE_CLEAR - 4 || ACR_BaseSystem.Covered(spot)) continue;
    man.SetOrigin(spot);
    break;
   }
  }
  // A tight Defend: a wide one let the men walk onto the road (cc89guer1).
  Order(group, ACR_Squad.WP_DEFEND, post.m_vPos, 10, false);
  string blocked = "plain";
  if (Math.RandomFloat01() < BLOCK_CHANCE) blocked = Block(post);
  PrintFormat("[ACR] GUERRILLAS %1 deployed: %2 men lying in wait %3 m off the road (%4)", post.m_sName, cell.m_iOut, (int)off, blocked);
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
  if (s_iSeconds % SUPPLY_EVERY_S == 0) Supply();
  if (s_iSeconds % RAISE_EVERY_S == 0) Raise();
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || !post.m_Group) continue;
   ACR_GuerrillaCell cell = post.m_Cell;
   int alive = manager.m_Bases.GuardsAlive(post.m_Group);
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
  if (cell.m_bHouse) ACR_Garrison.Release(post.m_Group);
  SetCombatMode(post.m_Group, EAIGroupCombatMode.RETURN_FIRE);
  Order(post.m_Group, ACR_Squad.WP_MOVE, fallback, 20, false);
  PrintFormat("[ACR] GUERRILLAS %1 pulls back to %2 (%3 of %4 left)", post.m_sName, ACR_Manager.Grid(fallback), alive, cell.m_iAliveAtSpring);
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

 // Packed up (ACR_Defenders.Tick): its barricade goes; after a fight it moves on to a new site
 // later; with nobody left the cell is gone.
 static void PackedUp(ACR_DefenderPost post)
 {
  ACR_GuerrillaCell cell = post.m_Cell;
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

 protected static void SetCombatMode(SCR_AIGroup group, EAIGroupCombatMode mode)
 {
  if (!group) return;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (utility && utility.GetCombatModeExternal() != mode) utility.SetCombatMode(mode);
 }

 // The group's waypoints replaced by one.
 protected static void Order(SCR_AIGroup group, ResourceName prefab, vector at, float radius, bool fastInit)
 {
  array<AIWaypoint> old = {};
  group.GetWaypoints(old);
  foreach (AIWaypoint waypoint : old)
  {
   group.RemoveWaypoint(waypoint);
   SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
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
