// FIA grievance (Jacob, 21 Nov: a player's idea): a main side's harm to civilians and towns grows
// FIA's strength, not only turns FIA's standing (ACR_Insurgency leaning) against that side.
//  - Harm: a civilian killed (Civil Population socket, ACR_CivilianSocket.CivilianKilled), a building
//    destroyed by a side (the modded SCR_DestructibleBuildingComponent, ACR_Report.c), and a strike
//    support landing in a town (ACR_Supports: battery, rocket strike, bomb run, helicopter guns, HE
//    mortars; a town = TOWN_BUILDINGS within TOWN_RADIUS). Only harm a main side caused counts.
//  - Where: each hideout takes the harm at full weight within NEAR of it, or within NEAR_CELL of a
//    cell or pocket it feeds (ACR_FIAIntel.SupplierOf), falling off to nothing at REACH. Harm far
//    from FIA recruits nobody.
//  - Grievance per hideout (ACR_GuerrillaCell.m_fGrievance) is capped at MAX, so one barrage can't
//    flood the map, and halves every HALF_LIFE_MIN. It recruits PER_POINT a minute for each point
//    into the hideout's pool (ACR_FIAGrowth.FillRate), on top of the land's own fill and even with a
//    held base near (half within STOP_REACH): anger finds recruits under occupation too. The pool
//    cap and what it buys (cells, saboteurs, technicals, spread) keep their own caps.
//  - Both sides hear it: once a hideout's grievance from one side passes ANNOUNCE since the last
//    line (at most every ANNOUNCE_GAP_MS), the side that did it reads "FIA recruits after our
//    shelling near X" and the other side "... after enemy shelling near X". The hideout isn't named.
//  - Independent of leaning (-acrFIALeaning): option -acrFIAGrievance 0 turns it off (default on
//    wherever FIA hideouts are placed; no setup row).
class ACR_FIAGrievance
{
 static const float MAX = 6;               // grievance points a hideout holds at most
 static const float HALF_LIFE_MIN = 15;    // it halves this often
 static const float PER_POINT = 0.03;      // recruits a minute for each point (MAX: 0.18, near the land's 0.2)
 static const float STOP_FACTOR = 0.5;     // within ACR_FIAGrowth.STOP_REACH of a held base
 static const float NEAR = 600;            // m from the hideout: full weight
 static const float NEAR_CELL = 400;       // ... or this close to a cell or pocket it feeds
 static const float REACH = 3000;          // m: no weight beyond (ACR_Guerrillas.SUPPLY_REACH)
 static const float CIVILIAN = 1;          // points for a civilian killed
 static const float BUILDING = 0.5;        // a building destroyed
 static const float STRIKE_HEAVY = 1.5;    // a battery, bomb run or rocket strike on a town
 static const float STRIKE_LIGHT = 1;      // HE mortars or a helicopter's guns on a town
 static const float TOWN_RADIUS = 150;
 static const int TOWN_BUILDINGS = 4;    // a hamlet counts (a hideout's own group has 3+)
 static const float ANNOUNCE = 1;          // points from one side since the last line
 static const float ANNOUNCE_GAP_MS = 600000;
 protected static bool s_bOn;
 protected static ref map<string, float> s_mUnannounced; // "hideout|side": points since the last line
 protected static ref map<string, float> s_mAnnouncedAt; // "hideout|side": world time of the last line
 static int s_iEvents;                     // tests: harm events that fed a hideout
 static int s_iLines;                      // tests: feed lines sent

 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_mUnannounced = new map<string, float>();
  s_mAnnouncedAt = new map<string, float>();
 }

 // Per match (ACR_Manager.Stop) and at ACR_Guerrillas.Apply.
 static void Reset()
 {
  s_bOn = false;
  s_iEvents = 0;
  s_iLines = 0;
  if (s_mUnannounced) s_mUnannounced.Clear();
  if (s_mAnnouncedAt) s_mAnnouncedAt.Clear();
  if (GetGame()) GetGame().GetCallqueue().Remove(StrikeLanded);
 }

 // Server, once the hideouts are placed (ACR_Guerrillas.Apply).
 static void Apply()
 {
  Reset();
  s_bOn = OptionOn();
  PrintFormat("[ACR] GRIEVANCE %1 (-acrFIAGrievance)", s_bOn);
 }

 static bool On() { return s_bOn; }

 protected static bool OptionOn()
 {
  string value;
  if (!ACR_Options.Get("acrFIAGrievance", value)) return true;
  value.ToLower();
  return !(value == "0" || value == "off" || value == "false");
 }

 // ---------------------------------------------------------------- harm

 // A civilian killed at 'pos' by someone of 'by' (ACR_CivilianSocket.CivilianKilled).
 static void OnCivilianKilled(vector pos, string by)
 {
  // COIN: the towns near it lean away from whoever did it (ACR_TownSupport), FIA too.
  ACR_TownSupport.Harm(pos, by, CIVILIAN, "killing of civilians");
  Harm(pos, by, CIVILIAN, "killing of civilians");
 }

 // A building destroyed at 'pos' by someone of 'by' (the modded SCR_DestructibleBuildingComponent).
 static void OnBuildingDown(vector pos, string by)
 {
  ACR_TownSupport.Harm(pos, by, BUILDING, "demolitions");
  Harm(pos, by, BUILDING, "demolitions");
 }

 // A strike support called by 'by' on 'target' (ACR_Supports): counted when it lands, if in a town.
 static void OnStrikeCalled(string by, vector target, ACR_ESupportType type, int delaySeconds)
 {
  if ((!s_bOn && !ACR_TownSupport.On()) || !Replication.IsServer()) return;
  float points = STRIKE_LIGHT;
  string what = "shelling";
  if (type == ACR_ESupportType.BOMB_RUN) what = "bombing";
  if (type == ACR_ESupportType.HELI_GUN_RUN || type == ACR_ESupportType.HELI_LOITER || type == ACR_ESupportType.HELI_DOOR_GUNS || type == ACR_ESupportType.ATTACK_HELI) what = "air strikes";
  if (type == ACR_ESupportType.ARTILLERY_BATTERY || type == ACR_ESupportType.BOMB_RUN || type == ACR_ESupportType.ROCKET_STRIKE) points = STRIKE_HEAVY;
  int ms = Math.Max(0, delaySeconds) * 1000;
  GetGame().GetCallqueue().CallLater(StrikeLanded, ms, false, by, target, points, what);
 }

 static void StrikeLanded(string by, vector target, float points, string what)
 {
  if (!InTown(target)) return;
  // Leaning by conduct counts it too: a town shelled is harm as a building destroyed is.
  ACR_Insurgency.OnTownStruck(target, by);
  ACR_TownSupport.Harm(target, by, points, what);
  Harm(target, by, points, what);
 }

 // Houses (the map's destructible buildings, not built structures) round a point: a town or hamlet.
 static bool InTown(vector pos)
 {
  return Houses(pos, TOWN_RADIUS) >= TOWN_BUILDINGS;
 }

 protected static int s_iHouses;
 static int Houses(vector pos, float radius)
 {
  s_iHouses = 0;
  GetGame().GetWorld().QueryEntitiesBySphere(pos, radius, CountHouse, null, EQueryEntitiesFlags.STATIC);
  return s_iHouses;
 }

 protected static bool CountHouse(IEntity e)
 {
  if (!e || e.GetParent() || !SCR_DestructibleBuildingEntity.Cast(e)) return true;
  vector mins, maxs;
  e.GetBounds(mins, maxs);
  vector size = maxs - mins;
  if (size[0] >= 3.5 && size[2] >= 3.5 && size[1] >= 2.5) s_iHouses++;
  return true;
 }

 // 'points' of harm at 'pos' by 'by' ('what' names it in the feed line), shared out to the hideouts
 // by where it happened.
 static void Harm(vector pos, string by, float points, string what)
 {
  if (!s_bOn || !Replication.IsServer() || by.IsEmpty() || by == ACR_Pockets.FACTION) return;
  if (by != ACR_FactionSelection.West() && by != ACR_FactionSelection.East()) return;
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  bool fed;
  foreach (ACR_DefenderPost hideout : hideouts)
  {
   float weight = WeightAt(hideout, pos);
   if (weight <= 0) continue;
   ACR_GuerrillaCell cell = hideout.m_Cell;
   float before = cell.m_fGrievance;
   cell.m_fGrievance = Math.Min(MAX, cell.m_fGrievance + points * weight);
   float gained = cell.m_fGrievance - before;
   if (gained <= 0) continue;
   fed = true;
   PrintFormat("[ACR] GRIEVANCE %1 +%2 (%3 by %4 at %5, weight %6): %7 of %8", hideout.m_sName, gained, what, by, ACR_Manager.Grid(pos), weight, cell.m_fGrievance, MAX);
   Note(hideout, by, gained, what, pos);
  }
  if (fed) s_iEvents++;
 }

 // How much of harm at 'pos' a hideout takes: 1 near it or near a cell or pocket it feeds, falling
 // off to 0 at REACH.
 static float WeightAt(ACR_DefenderPost hideout, vector pos)
 {
  float d = vector.DistanceXZ(hideout.m_vPos, pos);
  if (d <= NEAR) return 1;
  if (d >= REACH) return 0;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || post.m_Cell.m_bHideout || post.m_bGone) continue;
   if (vector.DistanceXZ(post.m_Cell.m_vKillZone, pos) > NEAR_CELL && vector.DistanceXZ(post.m_vPos, pos) > NEAR_CELL) continue;
   if (ACR_FIAIntel.SupplierOf(post.m_Cell.m_vKillZone) == hideout) return 1;
  }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.m_bPocket || base.m_bRetired || vector.DistanceXZ(base.GetOrigin(), pos) > NEAR_CELL) continue;
   if (ACR_FIAIntel.SupplierOf(base.GetOrigin()) == hideout) return 1;
  }
  return 1 - (d - NEAR) / (REACH - NEAR);
 }

 // ---------------------------------------------------------------- recruits

 // Recruits a minute the hideout's grievance brings (ACR_FIAGrowth.FillRate); 'stopped': a held
 // base within STOP_REACH.
 static float Fill(ACR_DefenderPost hideout, bool stopped)
 {
  if (!s_bOn || !hideout || !hideout.m_Cell) return 0;
  float rate = hideout.m_Cell.m_fGrievance * PER_POINT;
  if (stopped) rate *= STOP_FACTOR;
  return rate;
 }

 // 'minutes' passed (ACR_FIAGrowth.Grow, after the fill): every hideout's grievance fades.
 static void Fade(float minutes)
 {
  float keep = Math.Pow(0.5, minutes / HALF_LIFE_MIN);
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  foreach (ACR_DefenderPost hideout : hideouts)
  {
   ACR_GuerrillaCell cell = hideout.m_Cell;
   cell.m_fGrievance = cell.m_fGrievance * keep;
   if (cell.m_fGrievance < 0.01) cell.m_fGrievance = 0;
  }
 }

 // ---------------------------------------------------------------- the sides hear

 protected static void Note(ACR_DefenderPost hideout, string by, float gained, string what, vector pos)
 {
  string key = hideout.m_sName + "|" + by;
  float pending = s_mUnannounced.Get(key) + gained;
  s_mUnannounced.Set(key, pending);
  if (pending < ANNOUNCE) return;
  float now = GetGame().GetWorld().GetWorldTime();
  float last;
  if (s_mAnnouncedAt.Find(key, last) && now - last < ANNOUNCE_GAP_MS) return;
  s_mAnnouncedAt.Set(key, now);
  s_mUnannounced.Set(key, 0);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  string place = ACR_CivilianSocket.PlaceName(pos);
  array<string> sides = {ACR_FactionSelection.West(), ACR_FactionSelection.East()};
  foreach (string side : sides)
  {
   ACR_Intel intel = manager.IntelFor(side);
   if (!intel) continue;
   string whose = "enemy";
   if (side == by) whose = "our";
   intel.AddFeed(now, string.Format("FIA recruits after %1 %2 %3", whose, what, place));
  }
  s_iLines++;
  PrintFormat("[ACR] GRIEVANCE %1 announced: %2 %3 %4", hideout.m_sName, by, what, place);
 }
}
