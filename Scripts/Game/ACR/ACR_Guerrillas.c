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
// Unseen until found like every enemy: a side only learns of a cell by contact (ACR_Intel).
// A cell that is wiped out is gone for the match. Options: -acrGuerrillas off|few|many (setup row
// "FIA guerrillas": default few with the setup screen, off otherwise, as the pockets), and
// -acrFIAHidden 1 ("Hidden FIA"): no "FIA resistance at" message, a pocket is only found by
// entering it (ACR_BaseSystem).
class ACR_GuerrillaCell : Managed
{
 bool m_bHouse;
 vector m_vKillZone;     // ambush: the road point it watches; house: the building
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
  PrintFormat("[ACR] GUERRILLAS %1 cells of %2 wanted (%3 road sites, %4 houses to pick from, %5 m from starting bases, %6 ms)", placed, roads + houses, s_aRoads.Count(), s_aHouses.Count(), (int)clearance, System.GetTickCount() - started);
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
    s_aStarts.Insert(base.GetOrigin());
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

 static void NoteTraffic(vector pos)
 {
  int key = (int)(pos[0] / TRAFFIC_CELL) * 10000 + (int)(pos[2] / TRAFFIC_CELL);
  float count;
  s_mTraffic.Find(key, count);
  s_mTraffic.Set(key, count + 1);
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
   IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
   if (!vehicle || seen.Contains(vehicle)) continue;
   seen.Insert(vehicle);
   Physics physics = vehicle.GetPhysics();
   if (!physics || physics.GetVelocity().Length() < MOVING) continue;
   if (ACR_Minefields.RoadClearance(vehicle.GetOrigin()) > 8) continue;
   NoteTraffic(vehicle.GetOrigin());
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
    if (manager && ACR_Defenders.EnemyWithin(manager, post.m_sFaction, place, HOUSE_RANGE)) continue;
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
    if (!ACR_Ambush.SeesKillZone(candidate, road)) continue;
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
   int posts = ACR_Garrison.AssignArea(group, group.GetLeaderAgent(), cell.m_vKillZone, 25, cell.m_iOut);
   if (posts > 0) Order(group, ACR_Squad.WP_GARRISON, cell.m_vKillZone, 25, true);
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
   // Out from the road until clear of it (a bend can bring another stretch close).
   for (int step = 0; step < 4; step++)
   {
    vector spot = ACR_Manager.Ground(cell.m_vKillZone + cell.m_vRoadDir * along + across * (off + step * 6));
    if (BBB_Terrain.IsWaterAt(spot) || ACR_Minefields.RoadClearance(spot) < HIDE_CLEAR - 4) continue;
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
   if (cell.m_bWithdrawing) continue;
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
   if (ACR_Defenders.EnemyWithin(manager, post.m_sFaction, cell.m_vKillZone, HOUSE_CLOSE)) return "enemies close";
   return "";
  }
  if (ACR_Defenders.EnemyWithin(manager, post.m_sFaction, cell.m_vKillZone, KILL_RADIUS)) return "enemies in the kill zone";
  IEntity leader;
  if (post.m_Group.GetLeaderAgent()) leader = post.m_Group.GetLeaderAgent().GetControlledEntity();
  if (leader && ACR_Defenders.EnemyWithin(manager, post.m_sFaction, leader.GetOrigin(), CLOSE)) return "enemies too close";
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
 // none is near its men.
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
   return;
  }
  if (!fought) return;
  vector site;
  vector old = cell.m_vKillZone;
  cell.m_vKillZone = vector.Zero; // its own old site doesn't crowd the pick
  // Short of free sites, closer to the others rather than back where it fought.
  if (!PickSite(cell.m_bHouse, site) && !PickSite(cell.m_bHouse, site, SPACING * 0.5)) site = old;
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
