// FIA growth (#96 part 1): the hideouts' recruit economy. Part 2 of #89 had each hideout send a man
// to a short cell every 10 min and raise a cell every 30 min; this replaces those fixed ticks with a
// pool per hideout (ACR_GuerrillaCell.m_fPool), so FIA lives in the land nobody holds:
//  - The pool fills while no main side holds land near: FILL_PER_MIN recruits a minute, halved with
//    a held base or FOB within SLOW_REACH, nothing with one within STOP_REACH. Land of a side FIA
//    spares (its friend, or neutral with leaning) doesn't count: FIA moves freely among friends.
//  - Recruits buy, in this order: top-ups (the hideout itself, then each short-handed idle cell in
//    SUPPLY_REACH, a man each); a new cell (CELL_COST) while there are fewer than CellCap; the
//    saboteurs of #96 part 2 (an IED team, ACR_IED.Recruit, or a raid, ACR_Raid.Recruit); a
//    technical for a depot (part 3, ACR_Depots.Recruit); spread (SPREAD_COST): an
//    empty neutral base within SPREAD_REACH becomes a new FIA pocket, hidden until found.
//  - Caps from the FIA size option (-acrGuerrillas few|many): pool, cells, spreads.
//  - Losses shrink it: a wiped cell has to be paid for again, a taken pocket stays taken (spreads
//    count for the match, not how many stand), a cleared hideout's pool is gone with it.
// A wiped cell is not a kill counter: clearing hideouts and holding land is what stops FIA growing.
// When FIA is a main side (#97) ACR_Guerrillas.Apply places no hideouts, so none of this runs.
class ACR_FIAGrowth
{
 static const int EVERY_S = 60;
 static const float FILL_PER_MIN = 0.2;     // recruits a minute in empty land (one per 5 min)
 static const float STOP_REACH = 800;       // a held base this close: no recruits
 static const float SLOW_REACH = 1500;      // ... this close: half as many
 static const float SLOW_FACTOR = 0.5;
 static const int CELL_COST = 4;
 static const int SPREAD_COST = 6;
 static const float SPREAD_REACH = 2500;    // m from the hideout to a base it can spread into
 static const float SPREAD_QUIET_MS = 600000; // a base held or being taken in the last 10 min: never
 static const float SPREAD_ENEMY_CLEAR = 600; // nor one with a main side's man this close (attacked)
 protected static bool s_bMany;
 protected static int s_iSpreads;
 protected static int s_iSeconds;
 protected static ref map<string, float> s_mHeldAt; // base name: last world time it was held or being taken

 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_mHeldAt = new map<string, float>();
 }

 // Per match (ACR_Manager.Stop) and at ACR_Guerrillas.Apply.
 static void Reset()
 {
  s_bMany = false;
  s_iSpreads = 0;
  s_iSeconds = 0;
  if (s_mHeldAt) s_mHeldAt.Clear();
 }

 static void Apply(bool many)
 {
  Reset();
  s_bMany = many;
 }

 static bool Many() { return s_bMany; }
 static int Spreads() { return s_iSpreads; }

 static int PoolCap()
 {
  if (s_bMany) return 14;
  return 8;
 }

 // Cells (not hideouts, not saboteurs) FIA keeps at most: the start's and a few more.
 static int CellCap()
 {
  int extra = 1;
  if (s_bMany) extra = 3;
  return ACR_Guerrillas.WantedCells() + extra;
 }

 // Pockets FIA can spread into in a match (Jacob's open question 3: on with "few", +2).
 static int SpreadCap()
 {
  if (s_bMany) return 4;
  return 2;
 }

 // Every second (ACR_Guerrillas.Tick).
 static void Tick()
 {
  s_iSeconds++;
  if (s_iSeconds % EVERY_S != 0) return;
  Grow(EVERY_S / 60.0);
 }

 // 'minutes' of growth: the pools fill, then each hideout spends. Tests fast-forward with it.
 static void Grow(float minutes)
 {
  NoteHeld();
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  foreach (ACR_DefenderPost hideout : hideouts)
  {
   ACR_GuerrillaCell cell = hideout.m_Cell;
   cell.m_fPool = Math.Min(PoolCap(), cell.m_fPool + FillRate(hideout) * minutes);
   Spend(hideout);
  }
 }

 // Recruits a minute for this hideout (0: held land too close).
 static float FillRate(ACR_DefenderPost hideout)
 {
  bool slow;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!HeldAgainstFIA(base)) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), hideout.m_vPos);
   if (d < STOP_REACH) return 0;
   if (d < SLOW_REACH) slow = true;
  }
  if (slow) return FILL_PER_MIN * SLOW_FACTOR;
  return FILL_PER_MIN;
 }

 // A base (or FOB) a main side holds that FIA doesn't spare.
 protected static bool HeldAgainstFIA(ACR_Base base)
 {
  if (base.m_bRetired || base.m_bPocket || base.m_sOwner.IsEmpty() || base.m_sOwner == ACR_Pockets.FACTION) return false;
  return !ACR_Insurgency.Spared(base.m_sOwner);
 }

 // The pool spent in order; each step only when the ones before it have nothing to buy.
 static void Spend(ACR_DefenderPost hideout)
 {
  ACR_GuerrillaCell cell = hideout.m_Cell;
  if (TopUp(hideout) > 0) return;
  if (RaiseCell(hideout)) return;
  // Short of cells with too few recruits for one: save up (a cell comes before saboteurs).
  if (Cells() < CellCap() && cell.m_fPool < CELL_COST) return;
  // An IED team or a sabotage raid: raids first at night, IEDs first by day.
  bool night = ACR_Raid.IsNight();
  if (night && ACR_Raid.Recruit(hideout)) return;
  if (ACR_IED.Recruit(hideout)) return;
  if (!night && ACR_Raid.Recruit(hideout)) return;
  if (ACR_Depots.Recruit(hideout)) return;
  if (cell.m_fPool >= SPREAD_COST) Spread(hideout);
 }

 // Top-ups: the hideout itself while packed up, then each short idle cell in reach, a man each.
 static int TopUp(ACR_DefenderPost hideout)
 {
  ACR_GuerrillaCell home = hideout.m_Cell;
  int sent;
  if (!hideout.m_Group && hideout.m_iReserve < home.m_iStart && home.m_fPool >= 1)
  {
   hideout.m_iReserve++;
   home.m_fPool -= 1;
   sent++;
   PrintFormat("[ACR] GUERRILLAS %1 recruits a man for itself (%2 of %3, pool %4)", hideout.m_sName, hideout.m_iReserve, home.m_iStart, home.m_fPool);
  }
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (home.m_fPool < 1) break;
   ACR_GuerrillaCell cell = post.m_Cell;
   if (!cell || cell.m_bHideout || post.m_bGone || post.m_Group || post.m_iReserve >= cell.m_iStart) continue;
   if (vector.DistanceXZ(hideout.m_vPos, cell.m_vKillZone) > ACR_Guerrillas.SUPPLY_REACH) continue;
   post.m_iReserve++;
   home.m_fPool -= 1;
   sent++;
   PrintFormat("[ACR] GUERRILLAS %1 sends a man to %2 (%3 of %4, pool %5)", hideout.m_sName, post.m_sName, post.m_iReserve, cell.m_iStart, home.m_fPool);
  }
  return sent;
 }

 // Ambush and house cells standing.
 static int Cells()
 {
  int cells;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
   if (post.m_Cell && !post.m_Cell.m_bHideout && !post.m_bGone) cells++;
  return cells;
 }

 // A new cell on the roads the sides drive (or in a house) while under CellCap.
 static bool RaiseCell(ACR_DefenderPost hideout)
 {
  ACR_GuerrillaCell home = hideout.m_Cell;
  if (home.m_fPool < CELL_COST) return false;
  int cells = Cells();
  if (cells >= CellCap()) return false;
  bool house = Math.RandomFloat01() < 0.4;
  if (!ACR_Guerrillas.AddCell(house) && !ACR_Guerrillas.AddCell(!house)) return false;
  home.m_fPool -= CELL_COST;
  PrintFormat("[ACR] GUERRILLAS %1 raised a new cell (%2 of %3, pool %4)", hideout.m_sName, cells + 1, CellCap(), home.m_fPool);
  return true;
 }

 // ---------------------------------------------------------------- spread

 // Bases held or being taken now: noted, so spread keeps off them for SPREAD_QUIET_MS.
 protected static void NoteHeld()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   bool held = !base.m_sOwner.IsEmpty() && !base.m_bPocket;
   if (held || !base.m_sCapturer.IsEmpty() || base.m_bContested) s_mHeldAt.Set(base.m_sName, now);
  }
 }

 // An empty neutral base this hideout can turn into a pocket (null: none): within SPREAD_REACH,
 // not held, being taken or held in the last SPREAD_QUIET_MS, nobody of a main side near, as far
 // from the starting bases as the hideouts and pockets keep, apart from other pockets. Nearest first.
 static ACR_Base SpreadSite(ACR_DefenderPost hideout, out string why)
 {
  why = "no empty neutral base in reach";
  float now = GetGame().GetWorld().GetWorldTime();
  float clearance = ACR_Guerrillas.StartClearance();
  ACR_Base best;
  float bestDistance = SPREAD_REACH;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!SpreadableBase(base)) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), hideout.m_vPos);
   if (d > bestDistance) continue;
   float heldAt;
   if (s_mHeldAt.Find(base.m_sName, heldAt) && now - heldAt < SPREAD_QUIET_MS) { why = base.m_sName + " was held lately"; continue; }
   bool tooNear;
   foreach (vector start : ACR_Guerrillas.s_aStarts) if (vector.DistanceXZ(start, base.GetOrigin()) < clearance) tooNear = true;
   foreach (ACR_Base given : ACR_QuickStart.s_Assigned) if (vector.DistanceXZ(given.GetOrigin(), base.GetOrigin()) < ACR_QuickStart.POCKET_CLEARANCE) tooNear = true;
   foreach (ACR_Base pocket : ACR_Base.s_All) if (pocket.m_bPocket && vector.DistanceXZ(pocket.GetOrigin(), base.GetOrigin()) < ACR_Pockets.MIN_SPACING) tooNear = true;
   if (tooNear) { why = base.m_sName + " is too near a starting base or a pocket"; continue; }
   array<string> sides = {};
   ACR_Insurgency.SidesNear(base.GetOrigin(), SPREAD_ENEMY_CLEAR, sides);
   if (!sides.IsEmpty()) { why = base.m_sName + " has soldiers near"; continue; }
   best = base;
   bestDistance = d;
  }
  if (best) why = "";
  return best;
 }

 // Open, unbuilt, visible, nobody's and nobody taking it.
 static bool SpreadableBase(ACR_Base base)
 {
  if (!base || base.m_bRetired || base.m_bHidden || base.m_bBuilt || base.m_bPocket) return false;
  return base.m_sOwner.IsEmpty() && base.m_sCapturer.IsEmpty() && !base.m_bContested && base.m_fScore <= 0;
 }

 // A strong hideout (full strength, SPREAD_COST in the pool) turns a base into a pocket.
 static bool Spread(ACR_DefenderPost hideout)
 {
  ACR_GuerrillaCell home = hideout.m_Cell;
  if (s_iSpreads >= SpreadCap() || home.m_fPool < SPREAD_COST || hideout.m_iReserve < home.m_iStart) return false;
  string why;
  ACR_Base base = SpreadSite(hideout, why);
  if (!base) return false;
  int garrison = ACR_Pockets.GarrisonFor(base);
  ACR_Pockets.Add(base, garrison);
  home.m_fPool -= SPREAD_COST;
  s_iSpreads++;
  PrintFormat("[ACR] GUERRILLAS %1 spreads: %2 is an FIA pocket now (%3 men, %4 of %5 spreads, pool %6)", hideout.m_sName, base.m_sName, garrison, s_iSpreads, SpreadCap(), home.m_fPool);
  return true;
 }
}
