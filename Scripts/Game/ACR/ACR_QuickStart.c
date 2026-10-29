// Quick start (#88, Jacob 28 Oct): the match opens at the fighting stage instead of a race for
// empty bases. Once the layout is in place (ACR_Skirmish.ApplyOnce: generated or hand-built, any
// size) the open bases are split between the sides along a seeded front line, then:
// - the split is balanced by what the bases earn (Split): even = half each; uneven (liberation)
//   = one side holds about two thirds and the other starts with double RP to take it back;
// - a strip of bases along the line may stay neutral (FIA pockets can only pick those);
// - garrisons: front bases (the third nearest the enemy) get a full one by kind of site, rear
//   bases a token one (virtual until an enemy comes near, as at every base);
// - the side's starting infantry move up to its front bases, plus extra holding squads scaled to
//   how much the side holds; vehicles stay at the HQ;
// - roadblocks at the front, sited by the AI's own siting (ACR_RoadblockSiting) once the world
//   runs (Fortify, from ACR_Mode.SpawnStartingUnits). They count towards the AI's doctrine limit.
//   Then tier I dug-in positions and minefields there, on the AI's own scored spots (DigIn,
//   LayFields).
// The same seed (acrSeed, the setup's Layout seed row) gives the same split; the setup map preview
// shows it. Option -acrQuickStart off (default) | even | uneven (setup row "Quick start").
// Not here yet: FOBs already up; a start situation per side.
class ACR_QuickStart
{
 static const float UNEVEN_SHARE = 0.65;  // what the bigger side holds in an uneven start
 static const float NEUTRAL_MAX = 0.2;    // share of the open bases left neutral, at most
 static const string EXTRA_NAME = "Holding Squad";
 static const float POCKET_CLEARANCE = 700; // m an FIA pocket keeps from a handed-out base

 static ref array<ACR_Base> s_Assigned = {}; // open bases a side was given
 static ref array<ACR_Base> s_Front = {};    // of those, the front ones
 static string s_sFront;                     // the front's description (log, tests)
 protected static bool s_bFortified;

 static string Mode()
 {
  string value = ACR_TestHooks.Get().QuickStart();
  if (value == "" && !ACR_Options.Get("acrQuickStart", value)) value = "";
  value.ToLower();
  if (value == "even" || value == "uneven") return value;
  if (value == "1" || value == "on") return "even";
  return "off";
 }

 static bool Assigned(ACR_Base base) { return s_Assigned.Contains(base); }

 static void Reset()
 {
  s_Assigned.Clear();
  s_Front.Clear();
  s_sFront = "";
  s_bFortified = false;
  s_mPositions.Clear();
  s_mFields.Clear();
  if (GetGame()) GetGame().GetCallqueue().Remove(FrontWorks);
 }

 // What a base is worth to hold: its RP if it pays, a little if not.
 static float Weight(bool income, int value)
 {
  if (income) return 0.5 + value;
  return 0.5;
 }

 // The split itself, with no world changes (the setup preview runs it too). sides: in, 0 / 1 for
 // each side's starting bases, -1 open, -2 left out (hidden or held by someone else); out, open
 // bases become 0, 1 or stay -1 (neutral). Returns the front's description.
 static string Split(notnull array<vector> positions, notnull array<float> weights, notnull array<int> sides, int seed, string mode, bool narrow)
 {
  vector west, east;
  int westCount, eastCount;
  foreach (int i, int s : sides)
  {
   if (s == 0) { west = west + positions[i]; westCount++; }
   if (s == 1) { east = east + positions[i]; eastCount++; }
  }
  if (westCount == 0 || eastCount == 0) return "";
  west = west * (1.0 / westCount);
  east = east * (1.0 / eastCount);
  float span = vector.DistanceXZ(west, east);
  if (span < 1) return "";
  vector axis = east - west;
  axis[1] = 0;
  axis.Normalize();
  vector across = Vector(-axis[2], 0, axis[0]);
  vector middle = (west + east) * 0.5;
  // Every draw happens whatever the mode, so a seed's front is the same in both modes.
  RandomGenerator rng = new RandomGenerator();
  rng.SetSeed(seed * 31 + 88);
  int shape = Math.Min(2, Math.Floor(3 * rng.RandFloat01()));
  float tilt = 0.5 * (rng.RandFloat01() - 0.5);
  float bulge = 0.12 + 0.12 * rng.RandFloat01();
  if (rng.RandFloat01() < 0.5) bulge = -bulge;
  float centre = 0.6 * (rng.RandFloat01() - 0.5);
  float width = 0.25 + 0.2 * rng.RandFloat01();
  float neutralShare = NEUTRAL_MAX * rng.RandFloat01();
  int big = 0;
  if (rng.RandFloat01() < 0.5) big = 1;
  if (narrow) neutralShare = 0;
  // Margin: how far a base lies on the east side of the line (distance ratio, so a curved
  // coast or an island still splits by who is nearer).
  array<int> open = {};
  array<float> margins = {};
  foreach (int j, int side : sides)
  {
   if (side != -1) continue;
   vector p = positions[j];
   float toWest = vector.DistanceXZ(p, west);
   float toEast = vector.DistanceXZ(p, east);
   float ratio = toWest / Math.Max(1, toWest + toEast) - 0.5;
   float lateral = vector.Dot(p - middle, across) / span;
   float line = 0;
   if (shape == 1) line = tilt * lateral;
   if (shape == 2) line = bulge * Math.Pow(2.718, -((lateral - centre) / width) * ((lateral - centre) / width));
   float margin = ratio - line;
   // Insertion by margin (ties by position, so the order of the input never matters).
   int at = 0;
   while (at < open.Count() && (margins[at] < margin || (margins[at] == margin && EastOf(p, positions[open[at]])))) at++;
   open.InsertAt(j, at);
   margins.InsertAt(margin, at);
  }
  if (open.IsEmpty()) return "";
  float total = 0;
  foreach (int k : open) total += weights[k];
  float westShare = 0.5;
  if (mode == "uneven")
  {
   westShare = 1 - UNEVEN_SHARE;
   if (big == 0) westShare = UNEVEN_SHARE;
  }
  // West takes bases from its own end until its share is reached; the rest go east.
  float held = 0;
  int cut = 0;
  while (cut < open.Count() && held + 0.5 * weights[open[cut]] <= westShare * total)
  {
   held += weights[open[cut]];
   cut++;
  }
  foreach (int n, int index : open)
  {
   if (n < cut) sides[index] = 0;
   else sides[index] = 1;
  }
  // The neutral strip: alternately the last of west's and the first of east's, each side
  // keeping at least one base of its own.
  int neutral = Math.Min(Math.Round(neutralShare * open.Count()), open.Count() - 2);
  int westEnd = cut - 1;
  int eastStart = cut;
  int left = 0;
  for (int m = 0; m < 2 * open.Count() && left < neutral; m++)
  {
   if (m % 2 == 0 && westEnd > 0) { sides[open[westEnd]] = -1; westEnd--; left++; }
   else if (m % 2 == 1 && eastStart < open.Count() - 1) { sides[open[eastStart]] = -1; eastStart++; left++; }
  }
  string shapeName = "straight";
  if (shape == 1) shapeName = "slanted";
  if (shape == 2 && bulge > 0) shapeName = "salient into the west";
  if (shape == 2 && bulge < 0) shapeName = "salient into the east";
  return string.Format("%1 front, %2 neutral", shapeName, left);
 }

 protected static bool EastOf(vector p, vector other)
 {
  return other[0] < p[0];
 }

 // Server, from ACR_Skirmish.ApplyOnce after the layout and before the FIA pockets.
 static void Apply()
 {
  Reset();
  string mode = Mode();
  if (mode == "off") return;
  string westKey = ACR_FactionSelection.West();
  string eastKey = ACR_FactionSelection.East();
  int size = ACR_Skirmish.SizeOption();
  if (ACR_Skirmish.s_Active) size = ACR_Skirmish.s_Active.m_eSize;
  array<ACR_Base> bases = {};
  array<vector> positions = {};
  array<float> weights = {};
  array<int> sides = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_bBuilt) continue;
   int side = -1;
   if (base.m_sOwner == westKey) side = 0;
   else if (base.m_sOwner == eastKey) side = 1;
   else if (!base.m_sOwner.IsEmpty() || base.m_bHidden) side = -2;
   bases.Insert(base);
   positions.Insert(base.GetOrigin());
   weights.Insert(Weight(base.m_bIncome, base.m_iValue));
   sides.Insert(side);
  }
  // Which side each starting unit is on, before the owners change.
  map<ACR_StartingUnit, string> unitSides = new map<ACR_StartingUnit, string>();
  foreach (ACR_StartingUnit unit : ACR_StartingUnit.s_All) unitSides.Set(unit, SideOf(unit));
  int seed = ACR_Generator.Seed();
  s_sFront = Split(positions, weights, sides, seed, mode, size == ACR_ESkirmishSize.FLASHPOINT);
  if (s_sFront.IsEmpty())
  {
   Print("[ACR] QUICKSTART none: no starting base for both sides");
   return;
  }
  foreach (int i, ACR_Base open : bases)
  {
   if (open.m_sOwner != "" || sides[i] < 0) continue;
   open.m_sOwner = westKey;
   if (sides[i] == 1) open.m_sOwner = eastKey;
   open.m_sCapturer = "";
   open.m_fScore = 100;
   s_Assigned.Insert(open);
  }
  // A bigger side (uneven start) holds its ground harder; the smaller one starts richer.
  string bigger = "";
  float westValue = Held(westKey);
  float eastValue = Held(eastKey);
  if (mode == "uneven" && westValue > eastValue) bigger = westKey;
  if (mode == "uneven" && eastValue > westValue) bigger = eastKey;
  array<string> keys = {westKey, eastKey};
  foreach (string key : keys)
  {
   array<ACR_Base> front = {};
   array<ACR_Base> rear = {};
   FrontOf(key, front, rear);
   int bonus = 0;
   if (key == bigger) bonus = 2;
   foreach (ACR_Base f : front) SetGarrison(f, 2 + 2 * f.m_iValue + bonus);
   foreach (ACR_Base r : rear) SetGarrison(r, 1 + r.m_iValue);
   s_Front.InsertAll(front);
   MoveUp(key, front, unitSides);
   ACR_Manager manager = ACR_Manager.GetInstance();
   if (mode == "uneven" && bigger != "" && key != bigger && manager)
   {
    ACR_FactionData data = manager.m_Bases.GetFaction(key);
    data.m_iRP = data.m_iRP * 2;
    PrintFormat("[ACR] QUICKSTART %1 starts with %2 RP (double, the smaller side)", key, data.m_iRP);
   }
   PrintFormat("[ACR] QUICKSTART %1: %2 bases (%3 front), worth %4", key, front.Count() + rear.Count(), front.Count(), Held(key));
  }
  PrintFormat("[ACR] QUICKSTART %1 seed %2: %3; %4 bases handed out", mode, seed, s_sFront, s_Assigned.Count());
 }

 // The side's bases' worth (HQs included).
 static float Held(string key)
 {
  float value = 0;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (!base.m_bRetired && base.m_sOwner == key) value += Weight(base.m_bIncome, base.m_iValue);
  return value;
 }

 // The side's handed-out bases, the third nearest the enemy (at least one) in front.
 static void FrontOf(string key, notnull array<ACR_Base> front, notnull array<ACR_Base> rear)
 {
  array<ACR_Base> own = {};
  array<float> distances = {};
  foreach (ACR_Base base : s_Assigned)
  {
   if (base.m_sOwner != key) continue;
   float d = EnemyDistance(base);
   int at = 0;
   while (at < distances.Count() && distances[at] <= d) at++;
   own.InsertAt(base, at);
   distances.InsertAt(d, at);
  }
  float count = own.Count();
  int fronts = Math.Ceil(count / 3);
  foreach (int i, ACR_Base b : own)
  {
   if (i < fronts) front.Insert(b);
   else rear.Insert(b);
  }
 }

 static float EnemyDistance(ACR_Base base)
 {
  float best = float.MAX;
  foreach (ACR_Base other : ACR_Base.s_All)
  {
   if (other.m_bRetired || other.m_sOwner.IsEmpty() || other.m_sOwner == base.m_sOwner || other.m_bPocket) continue;
   best = Math.Min(best, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
  }
  return best;
 }

 static ACR_Base NearestEnemy(ACR_Base base)
 {
  ACR_Base nearest;
  float best = float.MAX;
  foreach (ACR_Base other : ACR_Base.s_All)
  {
   if (other.m_bRetired || other.m_sOwner.IsEmpty() || other.m_sOwner == base.m_sOwner || other.m_bPocket) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), other.GetOrigin());
   if (d < best) { best = d; nearest = other; }
  }
  return nearest;
 }

 protected static void SetGarrison(ACR_Base base, int soldiers)
 {
  base.m_iGarrison = soldiers;
  base.m_iGarrisonLeft = soldiers;
 }

 // A starting unit's side as ACR_Mode.SpawnStartingUnits would read it.
 protected static string SideOf(ACR_StartingUnit unit)
 {
  if (unit.m_MovedTo && !unit.m_MovedTo.m_sOwner.IsEmpty()) return unit.m_MovedTo.m_sOwner;
  string owner = "";
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_sOwner.IsEmpty()) continue;
   float d = vector.DistanceXZ(unit.GetOrigin(), base.GetOrigin());
   if (d < best) { best = d; owner = base.m_sOwner; }
  }
  return owner;
 }

 // The side's foot squads start at its front bases, one per base in turn; vehicles stay at the
 // HQ. Then holding squads (the side's own rifle squad prefab), one per six bases held, 1-3.
 protected static void MoveUp(string key, array<ACR_Base> front, map<ACR_StartingUnit, string> unitSides)
 {
  if (front.IsEmpty()) return;
  ResourceName rifles;
  int next = 0;
  foreach (ACR_StartingUnit unit, string side : unitSides)
  {
   if (side != key || unit.m_sGroup.IsEmpty() || !unit.m_sVehicle.IsEmpty()) continue;
   if (rifles.IsEmpty()) rifles = unit.m_sGroup;
   Place(unit, front[next % front.Count()]);
   next++;
  }
  if (rifles.IsEmpty()) return;
  float held = 0;
  foreach (ACR_Base base : s_Assigned) if (base.m_sOwner == key) held += 1;
  int extra = Math.ClampInt(Math.Round(held / 6), 1, 3);
  for (int i = 0; i < extra; i++)
  {
   EntitySpawnParams parameters = new EntitySpawnParams();
   parameters.TransformMode = ETransformMode.WORLD;
   ACR_Base at = front[next % front.Count()];
   next++;
   parameters.Transform[3] = at.GetOrigin();
   ACR_StartingUnit added = ACR_StartingUnit.Cast(GetGame().SpawnEntity(ACR_StartingUnit, GetGame().GetWorld(), parameters));
   if (!added) continue;
   added.m_sName = string.Format("%1 %2", EXTRA_NAME, i + 1);
   added.m_sGroup = rifles;
   added.m_bCrewed = true;
   added.m_MovedTo = at;
  }
 }

 protected static void Place(ACR_StartingUnit unit, ACR_Base base)
 {
  unit.m_MovedTo = base;
  unit.SetOrigin(base.GetOrigin());
 }

 // Once the world runs (ACR_Mode.SpawnStartingUnits): roadblocks on the roads into the front
 // bases, two a side, three for an Engineers company or the bigger side of an uneven start.
 static void Fortify()
 {
  if (s_bFortified || s_Front.IsEmpty()) return;
  s_bFortified = true;
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return;
  array<string> keys = {ACR_FactionSelection.West(), ACR_FactionSelection.East()};
  float westValue = Held(keys[0]);
  float eastValue = Held(keys[1]);
  foreach (string key : keys)
  {
   int wanted = 2;
   if (ACR_DoctrineKits.CompanyOf(key) == "engineers") wanted++;
   if (Mode() == "uneven" && ((key == keys[0] && westValue > eastValue) || (key == keys[1] && eastValue > westValue))) wanted++;
   int built = 0;
   foreach (ACR_Base base : s_Front)
   {
    if (built >= wanted) break;
    if (base.m_sOwner != key) continue;
    ACR_Base enemy = NearestEnemy(base);
    if (!enemy) continue;
    int considered;
    string none;
    ACR_RoadSite site = ACR_RoadblockSiting.Best(base.GetOrigin(), enemy.GetOrigin(), key, false, considered, none);
    if (!site)
    {
     PrintFormat("[ACR] QUICKSTART no roadblock at %1: %2", base.m_sName, none);
     continue;
    }
    if (ACR_Roadblocks.SpawnRoadblock(key, site.m_vPos, site.m_fYaw)) built++;
   }
   PrintFormat("[ACR] QUICKSTART %1: %2 roadblocks at the front", key, built);
  }
  // Then dug-in positions and minefields, after the roadblocks (their siting scores spots that
  // overlook or cover what is already up), once the starting squads have registered: a position
  // and a field are planned as one of our infantry squads would dig or lay them, and right after
  // the units spawn there is none yet (srv:quickstart, 30 Oct: 0 and 0 at every base).
  GetGame().GetCallqueue().CallLater(FrontWorks, WORKS_RETRY_MS, false, 0);
 }

 static const int WORKS_RETRY_MS = 3000;
 static const int WORKS_TRIES = 10;  // 30 s at most for the squads to register

 protected static void FrontWorks(int attempt)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  array<string> keys = {ACR_FactionSelection.West(), ACR_FactionSelection.East()};
  bool ready = true;
  foreach (string key : keys)
  {
   if (!s_Front.IsEmpty() && !ACR_DoctrineKits.InfantryNear(manager, key, s_Front[0].GetOrigin())) ready = false;
  }
  if (!ready && attempt + 1 < WORKS_TRIES)
  {
   GetGame().GetCallqueue().CallLater(FrontWorks, WORKS_RETRY_MS, false, attempt + 1);
   return;
  }
  if (!ready) PrintFormat("[ACR] QUICKSTART a side has no infantry squad after %1 s: front works only where one is", WORKS_TRIES * WORKS_RETRY_MS / 1000);
  float westValue = Held(keys[0]);
  float eastValue = Held(keys[1]);
  foreach (string side : keys)
  {
   // A bigger side in an uneven start gets one more of each, an Engineers company one more
   // minefield (its kit already dug two positions in).
   int extra = 0;
   if (Mode() == "uneven" && ((side == keys[0] && westValue > eastValue) || (side == keys[1] && eastValue > westValue))) extra = 1;
   int positions = DigIn(side, POSITIONS + extra);
   int mines = 1 + extra;
   if (ACR_DoctrineKits.CompanyOf(side) == "engineers") mines++;
   int fields = LayFields(side, mines);
   PrintFormat("[ACR] QUICKSTART %1: %2 dug-in positions and %3 minefields at the front", side, positions, fields);
   s_mPositions.Set(side, positions);
   s_mFields.Set(side, fields);
  }
 }

 // Quick start's front works besides the roadblocks (user, pn13 playtest 29 Oct: "with the quick
 // start, fortified positions and minefield are missing"): tier I positions and minefields, built
 // at once and free, as the start's roadblocks are, on the AI's own scored spots
 // (ACR_PositionSiting, ACR_MineSiting) facing the nearest enemy base.
 static const int POSITIONS = 2;           // tier I positions a side
 static const float POSITION_APART = 300;  // m: a front base with a position of ours this close has one
 static ref map<string, int> s_mPositions = new map<string, int>(); // tests: built per side
 static ref map<string, int> s_mFields = new map<string, int>();

 // Up to 'wanted' tier I positions, one per front base of 'key' that has none near.
 protected static int DigIn(string key, int wanted)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !ACR_Fortify.Enabled()) return 0;
  int built = 0;
  foreach (ACR_Base base : s_Front)
  {
   if (built >= wanted) break;
   if (base.m_sOwner != key || ACR_Fortify.NearestOwn(key, base.GetOrigin(), POSITION_APART)) continue;
   ACR_Base enemy = NearestEnemy(base);
   // The plan sites it as this squad would dig it (facing, its upgrades' fit); nobody walks there.
   ACR_Squad squad = ACR_DoctrineKits.InfantryNear(manager, key, base.GetOrigin());
   if (!enemy || !squad)
   {
    PrintFormat("[ACR] QUICKSTART %1: no position at %2: no enemy base or no infantry squad of ours to site it from", key, base.m_sName);
    continue;
   }
   array<ref ACR_PositionSite> sites = {};
   int nearFields;
   ACR_PositionSiting.Candidates(squad, key, base.GetOrigin(), enemy.GetOrigin(), sites, nearFields);
   string why = string.Format("%1 spots, none fit", sites.Count());
   foreach (ACR_PositionSite site : sites)
   {
    string refusal;
    ACR_FortifyPlan plan = ACR_Fortify.Plan(squad, site.m_vPos, refusal, enemy.GetOrigin());
    if (!plan || plan.m_Target) continue;
    ACR_Fortification fort = ACR_Fortify.Build(plan, refusal);
    if (!fort)
    {
     why = refusal;
     continue;
    }
    built++;
    PrintFormat("[ACR] QUICKSTART %1: a position on the approach to %2 at %3 (%4)", key, base.m_sName, fort.m_vPos, ACR_PositionSiting.Breakdown(site));
    why = "";
    break;
   }
   if (!why.IsEmpty()) PrintFormat("[ACR] QUICKSTART %1: no position at %2: %3", key, base.m_sName, why);
  }
  return built;
 }

 // Up to 'wanted' minefields, one per front base of 'key', laid at once. Dynamism (ACR_Variety):
 // the first spot tried is picked among the near-best, seeded by the side and the base.
 protected static int LayFields(string key, int wanted)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !ACR_Minefields.Enabled()) return 0;
  ACR_AICommander ai = manager.EnsureAICommander(key);
  int laid = 0;
  foreach (ACR_Base base : s_Front)
  {
   if (laid >= wanted) break;
   if (base.m_sOwner != key) continue;
   ACR_Base enemy = NearestEnemy(base);
   ACR_Squad squad = ACR_DoctrineKits.InfantryNear(manager, key, base.GetOrigin());
   if (!enemy || !squad || !ai)
   {
    PrintFormat("[ACR] QUICKSTART %1: no minefield at %2: no enemy base or no infantry squad of ours to lay it", key, base.m_sName);
    continue;
   }
   array<ref ACR_MineSite> sites = {};
   int crowded;
   ACR_MineSiting.Candidates(ai, key, base.GetOrigin(), enemy.GetOrigin(), sites, crowded);
   array<float> scores = {};
   foreach (ACR_MineSite scored : sites) scores.Insert(scored.m_fScore);
   int first = ACR_Variety.Pick(scores, string.Format("quickstart mines %1 %2", key, base.m_sName));
   if (first > 0)
   {
    ACR_MineSite picked = sites[first];
    sites.RemoveOrdered(first);
    sites.InsertAt(picked, 0);
   }
   string why = string.Format("%1 spots, %2 more too near our own defences or bases", sites.Count(), crowded);
   foreach (ACR_MineSite site : sites)
   {
    string refusal;
    ACR_MinePlan plan = ACR_Minefields.Plan(squad, site.m_eKind, site.m_vPos, refusal, enemy.GetOrigin());
    if (!plan)
    {
     why = refusal;
     continue;
    }
    ACR_Minefields.Lay(plan, 1, null);
    ACR_Minefields.Finish(plan);
    if (!plan.m_Field || plan.m_Field.m_aMines.IsEmpty()) continue;
    laid++;
    PrintFormat("[ACR] QUICKSTART %1: %2 on the approach to %3 at %4 (%5 mines; %6)", key, ACR_Minefields.Name(site.m_eKind), base.m_sName, plan.m_vSite, plan.m_Field.m_aMines.Count(), ACR_MineSiting.Breakdown(site));
    why = "";
    break;
   }
   if (!why.IsEmpty()) PrintFormat("[ACR] QUICKSTART %1: no minefield at %2: %3", key, base.m_sName, why);
  }
  return laid;
 }
}
