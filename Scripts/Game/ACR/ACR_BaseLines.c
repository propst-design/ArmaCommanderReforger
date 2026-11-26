// A base's works as defensive lines (#105, the owner on photos of 30 Oct: "the sandbags can chain until an
// area is covered, it does not make sense to have one singular defense and then nothing"). The line is the
// unit, not the position: sandbag pieces chained end to end across an approach the survey found weak
// (ACR_BaseSurvey), from cover to cover where it can.
//   A line runs from a seed both ways (ACR_BaseLiner.Walk), along the town's edge at some offset, or
//   straight across a gap in a wall. Each piece must fit and, but for a piece or two, have a view on its
//   firing side; one that doesn't turns, more and more sharply, and then the line goes round with a
//   round piece.
//   An open end goes on to the nearest anchor within 30 m: a building, a wall or fence, a big rock, another
//   work or line. With none it curves back, a round piece and an end piece. A slope too steep or the road
//   also ends it.
//   A road: the line stops at its edge and goes on across it. A lane stays open and the checkpoint
//   stands in it, its sides taking the place of the bags that would touch them.
//   Under 30 m isn't a line unless it closes a gap between two things. Lines stay apart, but a candidate
//   that runs into a chosen line is cut short and goes on to its end, so an approach gets one line.
//   Nests are built into a line, at least 12 m of bags either side. Bunkers stand at a line's end, flank
//   to its last piece, towers right behind it, the command post behind its middle, wire in front.
//   A 3.4 m gap for men on foot every 28-38 m, none within 19 m of a nest's middle.
// A line is built in stretches of 24-30 m, one site each: the squad works along it and pays per stretch,
// and a lost stretch can be built again. The worst approach first, then the next while the tier's budget
// lasts. Which approach, the offset, where it bends and where the guns go are weighted among the near-best
// and seeded (ACR_Variety).
// How a town's works are laid out (the owner's defence styles, 31 Oct 2026). Each town gets one from its ground
// and what threatens it, weighted among the near-best and seeded, per side: the same town can be a wall to one
// side and strongpoints to the other. They differ in where lines are looked for and how they're chosen, so
// the rest of the plan (nests, gaps, wire, posts) works on any of them. -acrBaseStyles 0: all WALL.
class ACR_BaseStyle : Managed
{
 static const int WALL = 0;   // a long line across each approach, from cover to cover
 static const int POINTS = 1; // strongpoints: short lines curving back at both ends, well apart, gaps on purpose
 static const int DEPTH = 2;  // a thin line well out, the real one close in behind it
 static const int FUNNEL = 3; // lines across the ways in, the guns on those
 static const int RING = 4;   // a small place: close in, all the way round
 static const int KINDS = 5;
 static int s_iForceForTest = -1;

 int m_iKind = WALL;
 ref array<float> m_aOffsets = {4, 9, 15, 22, 30, 40}; // m past the town's edge lines are looked for at
 ref array<float> m_aLater = {};     // ... at tier II, where it differs: that tier starts afresh
 int m_iOneWay = 24;                 // long pieces from a seed each way
 float m_fClash = 22;                // m between lines
 float m_fMinThreat = 0.2;           // approaches less threatened than this get no line
 float m_fLengthCap = 120;           // m of a line that count for it
 float m_fLaneWorth = 0.35;          // what a road through it counts for
 float m_fFirstShare = 1;            // tier I's share of its budget: the rest goes to tier II
 bool m_bApart;                      // lines don't tie into each other
 bool m_bGunsOnLanes;

 static string Word(int kind)
 {
  switch (kind)
  {
   case WALL: return "wall";
   case POINTS: return "strongpoints";
   case DEPTH: return "depth";
   case FUNNEL: return "funnel";
   case RING: return "ring";
  }
  return "?";
 }

 static bool On()
 {
  return ACR_LogisticsRules.Choice("acrBaseStyles", "1") != "0";
 }

 array<float> OffsetsFor(int tier)
 {
  if (tier >= 2 && !m_aLater.IsEmpty()) return m_aLater;
  return m_aOffsets;
 }

 static ACR_BaseStyle For(ACR_BaseWork work, ACR_BaseSurvey survey)
 {
  ACR_BaseStyle style = new ACR_BaseStyle();
  int threatened;
  float open, walls;
  int ways;
  for (int s = 0; s < ACR_BaseSurvey.SECTORS; s++)
  {
   if (survey.m_aThreat[s] < 0.5) continue;
   threatened++;
   open += survey.m_aOpen[s];
   walls += survey.m_aWall[s];
   ways += survey.m_aRoads[s];
  }
  float arc = threatened * 1.0 / ACR_BaseSurvey.SECTORS;
  open /= Math.Max(threatened, 1);
  walls /= Math.Max(threatened, 1);
  int town = survey.m_iTown;
  array<float> scores = {};
  for (int k = 0; k < KINDS; k++) scores.Insert(-1);
  scores[WALL] = 0.55 + 0.2 * walls + 0.1 * (1 - arc);
  scores[POINTS] = 0.6 + 0.35 * arc;
  if (town >= 40) scores[POINTS] = scores[POINTS] + 0.1;
  if (town >= 15) scores[DEPTH] = 0.55 + 0.55 * open;
  if (ways >= 1) scores[FUNNEL] = 0.6 + 0.08 * Math.Min(ways, 3);
  if (town <= 25) scores[RING] = 0.9 - 0.012 * town;
  // The commander's doctrine leans it: a Fortress digs in depth, a Raider keeps to islands it can leave.
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_AICommander commander;
  if (manager) commander = manager.AICommanderFor(work.m_sFaction);
  ACR_AIDoctrine doctrine;
  if (commander && commander.m_bActive) doctrine = commander.Doctrine();
  if (doctrine)
  {
   if (doctrine.m_sKey == "fortress")
   {
    if (scores[DEPTH] >= 0) scores[DEPTH] = scores[DEPTH] + 0.2;
    scores[WALL] = scores[WALL] + 0.1;
   }
   if (doctrine.m_sKey == "raider") scores[POINTS] = scores[POINTS] + 0.2;
   if (doctrine.m_sKey == "blitz" && scores[FUNNEL] >= 0) scores[FUNNEL] = scores[FUNNEL] + 0.15;
  }
  int kind = ACR_Variety.Pick(scores, string.Format("base style %1 %2", work.m_Base.m_sName, work.m_sFaction), 0.25);
  if (!On()) kind = WALL;
  if (s_iForceForTest >= 0) kind = s_iForceForTest;
  style.Set(kind);
  PrintFormat("[ACR] BASEWORKS %1: style %2 (arc %3, open %4, walls %5, ways %6, %7 buildings)", work.m_Base.m_sName, Word(kind), arc, open, walls, ways, town);
  return style;
 }

 protected void Set(int kind)
 {
  m_iKind = kind;
  switch (kind)
  {
   case POINTS:
    m_aOffsets = {9, 15, 22, 30};
    m_iOneWay = 7;
    m_fClash = 40;
    m_fLengthCap = 50;
    m_bApart = true;
    break;
   case DEPTH:
    m_aOffsets = {40, 50, 32};
    m_aLater = {4, 9, 15};
    m_iOneWay = 12;
    m_fFirstShare = 0.55;
    break;
   case FUNNEL:
    m_aOffsets = {9, 15, 22};
    m_iOneWay = 14;
    m_fLaneWorth = 0.8;
    m_bGunsOnLanes = true;
    break;
   case RING:
    m_aOffsets = {3, 6, 10};
    m_iOneWay = 12;
    m_fMinThreat = 0;
    m_fClash = 12;
    break;
  }
 }
}

class ACR_BaseLines
{
 // Long high piece: 3.43 m over its ends (x -1.69 .. 1.74); these leave a few cm of overlap at a joint.
 static const float LONG_STEP = 3.38;
 static const float END_STEP = 2.1;       // end high piece: 2.13 m
 static const float SHORT_STEP = 1.68;    // short high piece: 2.0 m by its bounds; a little more overlap at the end
 static const int BREAK_WALK = 1;
 static const int BREAK_ROAD = 2;
 static const float MIN_LINE = 30;        // m: shorter isn't a line, unless both ends stand on something
 static const float MIN_GAP = 6.5;        // m: ... and then not under this
 static const float EXTEND_REACH = 30;    // m an open end looks for something to end on
 static const int WAY_IN_WALKS = 3;       // ways into town walked for a line across them
 static const float SHORT_RUN = 5.5;      // m of bags at a line's end that are worth leaving
 static const float RING_STANDS = 7;      // m from a building or a wall that a stretch of the ring's end stands on it
 static const float RING_SHORT = 10;      // m: shorter with one end free isn't worth having
 static const float RING_PLUG = 24;       // m: shorter with both ends on something is a plug in a gap
 static const float RISE_WORTH = 0.08;    // of a line's score, per metre it stands up over the ground round it
 static const float DEAD_COST = 0.15;     // ... per spot of dead ground close in front
 static const float GATE_HELD = 30;       // m from a way in that a line holds it (the audit's GATE_NEAR)
 static const float GATE_OUT = 50;        // m on along the road where a lane still holds it: a line further out
 static const float CLUTTER_SHARE = 0.1;  // stretches with something left behind the bags (the user, 2 Nov: one behind every one looked like junk)
 static const float LIGHT_SHARE = 0.25;   // ... offered a lamp at their foot (Light takes 60% of those)
 static const float GATE_CHECKPOINT = 0.5; // a checkpoint where it's the only thing that would hold a way in
 static const int GATE_CHECKPOINT_ASKS = 2; // ways in looked at for one each time: asking the road is dear
 static const int PLUG_ARM = 3;           // long pieces each way across a way in, and up to 2 more
 static const float SEED_LOOK = 60;       // m a seed is looked out from before it's walked
 static const float SEED_BLIND = 8;       // m: seeing less it isn't walked at all
 static const float SEED_CHANCE = 0.25;   // how much luck counts against that (1 = the whole 60 m)
 static const int SEED_WALKS = 4;         // seeds walked on an approach at most
 static const float SEED_APART = 12;      // m: a seed nearer a walked line's than this is the same line
 static const int HUG_RUN = 3;            // pieces in a row along a wall that come out: the wall's the line there
 static const float HUG_NEAR = 4;         // m from a wall that's along it
 static const float HUG_DOT = 0.85;       // and how nearly parallel
 static const int GATE_PIECES = 12;       // what's left in a town's budget that a way in nobody holds can still have
 static const float FUNNEL_OPEN_MIN = 15; // degrees a funnel's arm opens out from straight across the road
 static const float FUNNEL_OPEN_MORE = 25;
 static const int FUNNEL_ARM = 6;         // long pieces in an arm, and up to 4 more
 static const int BLIND_RUN = 4;          // long pieces in a row seeing nothing that come out of a line
 static const float WAY_IN_ASIDE = 6;     // m from a road's middle a line across it starts from
 static const int MOST_EXTEND = 10;       // long pieces it may spend getting there
 // Cap on one base's sandbag pieces: replicated props, each a streaming node and a navmesh rebuild. For
 // scale, one vanilla sandbag position is 26-40 pieces, and a medium checkpoint about 28.
 static const int MOST_PIECES = 140;
 static const int MOST_WIRE = 10;         // wire segments in front of one line
 static const float WALK_APART = 28;      // m of bags between gaps for men on foot, and up to 10 more
 static const float NEST_CLEAR = 19;      // m from a nest's middle to a walk gap: its 12 m of bags each side stay unbroken
 static const int NEST_WINGS = 4;         // long pieces of bags either side of a nest: 13.5 m
 static const int NEST_LEVELS = 5;        // how far a town with too few places goes down (NestWings, NestReach)
 static const int NEST_COMPACT = 4;       // the last: the small gun position, one long piece wide
 static int s_iFirstNestLevelForTest;
 // -acrBaseSmallGuns 0: nests only.
 static bool SmallGunsOn() { return ACR_LogisticsRules.Choice("acrBaseSmallGuns", "1") != "0"; }
 static const float NEST_BEHIND = 1;      // m its middle stands behind the line: its wings' bags are on the line then
 static const float NEST_DEPTH = 2.8;     // m its bags reach either way from there (measured, 6 m in all)
 static const float NEST_HALF = 5.2;      // m its bags reach to its sides (measured -5.2 .. 4.4; USSR -5.5 .. 4.5)
 static const float COMPACT_HALF = 1.9;   // the small gun position: its side bags reach 1.7 m (Sandbag_MG_US_01)
 static const float COMPACT_DEPTH = 1.1;
 static const float BEND_DOT = 0.94;      // two pieces' facings closer than this aren't a bend (about 20 degrees)
 static const float BEND_WORTH = 0.25;    // a gun at a bend sees along both runs
 static const float GUN_OPEN_GATE = 1.5;  // a gun on a way in that has none
 static const int MOST_GUN_LOOKS = 30;       // places looked at for a gun, about
 static const float STRETCH_MOST = 34;       // m a stretch gets to, whatever's left of the line
 static ref array<float> WIRE_OUTS; // m in front of the bags wire stands, the furthest that works
 static void ACR_InitStatics()
 {
  WIRE_OUTS = {32, 24, 16, 7};
 }
 static const float GUN_WORTH_LEAST = 2;      // what a gun must do to him to be placed
 static const float LONE_GUN_WORTH = 10;      // ... a gun on its own, with no line about it
 static const float GATE_TURN = 20;       // degrees a gun on a straight run turns for one
 static const float GUN_TURN_MOST = 25;   // degrees off its bags' front a gun built from parts can stand
 static const float DRESS_CHANCE = 0.65;  // a line has stretches of low or netted bags
 static const int DRESS_LEAST = 6;        // pieces in a line for that
 static const int DIRT_EVERY = 5;         // pieces of a stretch to a mound of dirt
 static const float DIRT_OUT = 1.6;       // m in front of the bags' middle
 static const float OBSTACLE_CHANCE = 0.6; // a line with a road through it has obstacles out in front
 static const int OBSTACLE_MOST = 6;      // hedgehogs each side of the road
 static const float OBSTACLE_STEP = 3.2;  // m from one to the next
 static const float OBSTACLE_OUT = 14;    // m out in front of the line
 static const float OBSTACLE_ASIDE = 8;   // m from the road's middle to the first
 static const float OBSTACLE_ROAD = 4;    // m it keeps from any road's edge
 static const ResourceName DIRT_LONG_1 = "{FD625252F3654B76}PrefabsEditable/Auto/Structures/Military/Fortifications/E_DirtCover_01_long_v1.et";
 static const ResourceName DIRT_LONG_2 = "{913666B24FA1C458}PrefabsEditable/Auto/Structures/Military/Fortifications/E_DirtCover_01_long_v2.et";
 static const ResourceName DIRT_LONG_3 = "{74AA2A4B43445333}PrefabsEditable/Auto/Structures/Military/Fortifications/E_DirtCover_01_long_v3.et";
 static const ResourceName DIRT_SHORT = "{D7FB89CF21DDA2A3}PrefabsEditable/Auto/Structures/Military/Fortifications/E_DirtCover_01_short.et";
 static const ResourceName HEDGEHOG = "{E6AE368A342034CB}PrefabsEditable/Auto/Props/Military/Fortification/E_CzechHedgehog_01_rusty.et";
 static const ResourceName LOGS_BARRICADE = "{3BAF6AA8C20D924E}Prefabs/Compositions/Misc/SubCompositions/Fortifications/Barricade_Logs_01.et";
 static const float GATE_GUN_ASIDE = 7;   // m from the road a gun on its own stands, or a little more
 static const float GATE_GUN_HALF = 3;    // m it reaches round its middle
 static const float GATE_GUN_REACH = 30;  // m its fan must be clear
 static const int GATE_GUN_LOOKS = 32;    // ways of standing tried for one way in: each is several traces
 static const float CORNER_GUN_OUT = 5;   // m outside a compound wall's corner a gun in a ring of its own stands, or 3 or 6 more
 static const float CORNER_MEET = 0.8;    // m apart two wall pieces' ends can be and meet
 static const float CORNER_TALL = 3;      // m a wall piece must stand to be a compound's wall: a garden wall or a fence gets no gun
 static const float CORNER_COS_LEAST = 0.64; // two pieces meeting at under 50 degrees run on: no corner
 static const float CORNER_COS_MOST = -0.64; // ... over 130: a kink
 static const float CORNER_CONVEX = 0.3;  // how squarely the corner's outside faces away from the town's middle
 static const float CORNER_THREAT = 0.35; // and towards a threat
 static const float WALK_GAP_NEAR = 10.5; // m: nests stand no nearer a walk gap
 static const float COMPACT_ROAD = 4;     // m from its middle to a road's edge
 static const float GUN_ON_GATE = 150;    // m: a gun this close to a way into town, facing it, covers it
 static const float GUN_GATE_DOT = 0.7;   // ... within about 45 degrees of its facing
 static const float GUN_GATE_WORTH = 0.7;
 static const float GUN_LOOK = 150;       // m a gun's view counts up to
 static const float COMPACT_HOUSE = 5;    // ... to a building, its middle and its gun (1.1 m forward)

 // ---------------------------------------------------------------- budgets

 static int Budget(int tier, int town)
 {
  switch (tier)
  {
   case 1: return Math.ClampInt(50 + town / 2, 50, 90);
   case 2: return Math.ClampInt(15 + town / 4, 15, 30);
   case 3: return 25;
  }
  return 0;
 }

 // ---------------------------------------------------------------- the plan

 static void Plan(ACR_BaseWork work, ACR_BaseSurvey survey)
 {
  ACR_BaseDefenceKit kit = ACR_BaseDefences.Kit(work.m_sFaction);
  array<ref ACR_BaseLine> pool = {};
  array<int> tried = {};
  int started = System.GetTickCount();
  work.m_Style = ACR_BaseStyle.For(work, survey);
  ACR_BaseStyle style = work.m_Style;
  if (ACR_BaseRing.On() && ACR_BaseLiner.PlacementOn() && ACR_BaseRing.Plan(work, survey, kit))
  {
   survey.m_iLineMs = System.GetTickCount() - started;
   return;
  }
  // A gap in a wall is close in: that's tier II's line where tier I's is far out.
  bool startsFar = !style.m_aLater.IsEmpty();
  if (!startsFar)
  {
   GapLines(work, survey, kit, pool);
   WayInLines(work, survey, kit, pool);
  }
  int total = 0;
  int first = Budget(1, survey.m_iTown) * style.m_fFirstShare;
  total += Fill(work, survey, kit, pool, tried, 1, first);
  if (startsFar)
  {
   pool.Clear();
   tried.Clear();
   GapLines(work, survey, kit, pool);
   WayInLines(work, survey, kit, pool);
  }
  total += Fill(work, survey, kit, pool, tried, 2, Budget(2, survey.m_iTown) + Budget(1, survey.m_iTown) - first);
  // Nothing came of it (a mine's concrete, a walled yard): better a line that watches a fence than none.
  if (ACR_BaseLiner.PlacementOn() && work.m_aLines.IsEmpty() && !survey.m_bLoose)
  {
   survey.m_bLoose = true;
   survey.m_iPass = 0;
   pool.Clear();
   tried.Clear();
   GapLines(work, survey, kit, pool);
   total += Fill(work, survey, kit, pool, tried, 1, first);
   total += Fill(work, survey, kit, pool, tried, 2, Budget(2, survey.m_iTown) + Budget(1, survey.m_iTown) - first);
  }
  if (ACR_BaseLiner.PlacementOn() && !survey.m_bLoose) total += OpenGateLines(work, survey, kit, total);
  array<ref ACR_BaseLine> behind = {};
  array<int> noMore = {};
  for (int s = 0; s < ACR_BaseSurvey.SECTORS; s++) noMore.Insert(s);
  Fallback(work, survey, kit, behind);
  total += Fill(work, survey, kit, behind, noMore, 3, Math.Min(Budget(3, survey.m_iTown), MOST_PIECES - total));
  survey.m_iLineMs = System.GetTickCount() - started;
  PrintFormat("[ACR] BASEWORKS %1: %2 lines, %3 sandbag pieces (at most %4), %5 walks, %6 ms", work.m_Base.m_sName, work.m_aLines.Count(), total, MOST_PIECES, survey.m_iWalks, survey.m_iLineMs);
  if (work.m_aLines.IsEmpty()) PrintFormat("[ACR] BASEWORKS %1: no line fits: %2", work.m_Base.m_sName, survey.FitWords());
 }

 // Lines worth having on one approach: a few offsets and shifts past the town's edge, a seed nudged along
 // if it doesn't fit, until two have come up.
 protected static void WalkSector(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_BaseLine> pool, int s, int tier)
 {
  string town = work.m_Base.m_sName;
  RandomGenerator rng = ACR_Variety.For(string.Format("base line %1 %2", town, s));
  // The second look at an approach starts from further out, and differently.
  array<float> offsets = {};
  array<float> wanted = work.m_Style.OffsetsFor(tier);
  foreach (float each : wanted) offsets.Insert(each);
  if (survey.m_iPass > 0)
  {
   rng = ACR_Variety.For(string.Format("base line again %1 %2", town, s));
   offsets.Clear();
   for (int o = wanted.Count() - 1; o >= 0; o--) offsets.Insert(wanted[o] - 2);
  }
  float bearing = ACR_BaseSurvey.SectorBearing(s);
  array<float> nudges = {0, -7, 7};
  int found = 0;
  if (ACR_BaseLiner.PlacementOn() && !survey.m_bLoose)
  {
   LookedWalks(work, survey, kit, pool, s, offsets, nudges, bearing, rng);
   return;
  }
  foreach (float offset : offsets)
  {
   if (found >= 2) return;
   float shift = rng.RandFloat01() * 16 - 8;
   float past = offset + rng.RandFloat01() * 3;
   foreach (float nudge : nudges)
   {
    float seedAt = bearing + shift + nudge;
    float reach = survey.EdgeSmooth(seedAt) + past;
    if (reach < 15) continue;
    vector seed = survey.m_vCenter + ACR_BaseSurvey.Dir(seedAt) * reach;
    ACR_BaseLine line = WalkFrom(work, survey, kit, seed, seedAt, past, rng, s, false, vector.Zero);
    if (!line) continue;
    pool.Insert(line);
    found++;
    break;
   }
  }
 }

 // Every offset and nudge looked out from first, then walked best first until two have come up: the ground
 // decides where the line goes, not which offset happened to be tried first. Weighted, so it isn't the same
 // spot every time a town's fortified.
 protected static void LookedWalks(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_BaseLine> pool, int s, notnull array<float> offsets, notnull array<float> nudges, float bearing, RandomGenerator rng)
 {
  array<vector> seeds = {};
  array<float> bearings = {};
  array<float> pasts = {};
  array<float> looks = {};
  foreach (float offset : offsets)
  {
   float shift = rng.RandFloat01() * 16 - 8;
   float past = offset + rng.RandFloat01() * 3;
   foreach (float nudge : nudges)
   {
    float seedAt = bearing + shift + nudge;
    float reach = survey.EdgeSmooth(seedAt) + past;
    if (reach < 15) continue;
    vector seed = survey.m_vCenter + ACR_BaseSurvey.Dir(seedAt) * reach;
    vector eye = seed;
    eye[1] = ACR_BaseLiner.Ground(seed) + ACR_BaseSurvey.UP_BAGS;
    seeds.Insert(seed);
    bearings.Insert(seedAt);
    pasts.Insert(past);
    float seedSees = ACR_BaseSurvey.Reach(eye, seedAt, SEED_LOOK);
    if (seedSees < SEED_BLIND)
    {
     seeds.Remove(seeds.Count() - 1);
     bearings.Remove(bearings.Count() - 1);
     pasts.Remove(pasts.Count() - 1);
     continue;
    }
    looks.Insert(seedSees / SEED_LOOK + rng.RandFloat01() * SEED_CHANCE);
   }
  }
  int found = 0;
  int walked = 0;
  while (found < 2 && walked < SEED_WALKS && !looks.IsEmpty())
  {
   int best = 0;
   foreach (int i, float look : looks) if (look > looks[best]) best = i;
   vector at = seeds[best];
   float seedBearing = bearings[best];
   float seedPast = pasts[best];
   seeds.Remove(best);
   bearings.Remove(best);
   pasts.Remove(best);
   looks.Remove(best);
   walked++;
   ACR_BaseLine line = WalkFrom(work, survey, kit, at, seedBearing, seedPast, rng, s, false, vector.Zero);
   if (!line) continue;
   pool.Insert(line);
   found++;
   // The next one from somewhere else: not the same stretch again from a few metres along.
   for (int k = seeds.Count() - 1; k >= 0; k--)
   {
    if (vector.DistanceXZ(seeds[k], at) >= SEED_APART) continue;
    seeds.Remove(k);
    bearings.Remove(k);
    pasts.Remove(k);
    looks.Remove(k);
   }
  }
 }

 // A line from a seed, both ways. null if it doesn't come to a line.
 protected static ACR_BaseLine WalkFrom(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, vector seed, float seedAt, float past, RandomGenerator rng, int sector, bool gap, vector target)
 {
  seed[1] = ACR_BaseLiner.Ground(seed);
  ACR_BaseLiner liner = new ACR_BaseLiner(survey, work, seedAt);
  liner.m_fOffset = past;
  liner.m_fWobble = rng.RandFloat01() * 2.5;
  liner.m_fPhase = rng.RandFloat01() * 6.28;
  // Most lines have fire bays, at their own spacing, mostly stepping out.
  if (ACR_BaseLiner.BaysOn() && rng.RandFloat01() < 0.7)
  {
   liner.m_iBayEvery = 5 + rng.RandInt(0, 4);
   if (rng.RandFloat01() < 0.3) liner.m_fBaySide = -1;
  }
  vector facing = ACR_BaseSurvey.Dir(seedAt);
  vector heading = Vector(facing[2], 0, -facing[0]);
  ACR_BaseLine line = new ACR_BaseLine();
  line.m_iSector = sector;
  line.m_vFacing = facing;
  string words;
  survey.m_iWalks++;
  if (gap)
  {
   liner.m_bTarget = true;
   liner.m_vTarget = target;
   liner.m_sTargetWord = "wall";
   heading = ACR_BaseDefences.FlatDir(seed, target);
   int side = 1;
   if (ACR_BaseDefences.DotXZ(ACR_BaseLiner.FacingOf(heading, 1), facing) < 0) side = -1;
   int seedFit = liner.Fit(seed, heading, LONG_STEP, side, false, true);
   if (seedFit != ACR_BaseLiner.FIT_OK)
   {
    survey.LogWalk(work.m_Base.m_sName, sector, past, "gap's first piece: " + ACR_BaseLiner.FitWord(seedFit));
    return null;
   }
   line.m_bGap = true;
   line.m_sEndA = "wall";
   line.m_sEndB = liner.Walk(seed, heading, side, work.m_Style.m_iOneWay, line.m_aRight, line.m_aLanes, line.m_aLaneOut, kit);
   words = string.Format("gap, %1 pieces, ended %2", line.m_aRight.Count(), ACR_BaseLiner.FitWord(liner.m_iStop));
   if (side < 0)
   {
    // The chain reads from its right end: the walk went the other way.
    foreach (ACR_LinePiece gapWalked : line.m_aRight) line.m_aLeft.Insert(gapWalked);
    line.m_aRight.Clear();
    string word = line.m_sEndA;
    line.m_sEndA = line.m_sEndB;
    line.m_sEndB = word;
   }
  }
  else
  {
   int fitRight = liner.Fit(seed, heading, LONG_STEP, 1, false);
   int fitLeft = liner.Fit(seed, heading * -1, LONG_STEP, -1, false);
   if (fitRight != ACR_BaseLiner.FIT_OK && fitLeft != ACR_BaseLiner.FIT_OK)
   {
    survey.LogWalk(work.m_Base.m_sName, sector, past, string.Format("seed: %1", ACR_BaseLiner.FitWord(fitRight)));
    return null;
   }
   line.m_sEndB = liner.Walk(seed, heading, 1, work.m_Style.m_iOneWay, line.m_aRight, line.m_aLanes, line.m_aLaneOut, kit);
   int stopRight = liner.m_iStop;
   line.m_sEndA = liner.Walk(seed, heading * -1, -1, work.m_Style.m_iOneWay, line.m_aLeft, line.m_aLanes, line.m_aLaneOut, kit);
   words = string.Format("%1 pieces ended %2, %3 pieces ended %4", line.m_aRight.Count(), ACR_BaseLiner.FitWord(stopRight), line.m_aLeft.Count(), ACR_BaseLiner.FitWord(liner.m_iStop));
  }
  Measure(line, survey);
  survey.LogWalk(work.m_Base.m_sName, sector, past, string.Format("%1: %2 m", words, (int)line.m_fLength));
  // Short of a line now: an open end can still get longer in Realize, so a little short is kept.
  if (line.Count() < 2 || (line.m_fLength < MIN_LINE - 10 && !line.Closes())) return null;
  if (line.m_fLength < MIN_GAP) return null;
  return line;
 }

 // A stretch of the ring (ACR_BaseRing) as a line on the work: walked through its points, the bags' firing side
 // away from the town. The pieces it came to, 0 if it came to nothing.
 static int RingLine(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, ACR_RingRun run, int tier, int most)
 {
  if (run.m_aWay.Count() < 2 || most < 2) return 0;
  float bearing = ACR_BaseDefences.Bearing(run.m_vOut);
  ACR_BaseLiner liner = new ACR_BaseLiner(survey, work, ACR_BaseDefences.Bearing(run.m_vMiddle - survey.m_vCenter));
  liner.m_bRing = true;
  liner.AllRoads();
  liner.m_bTarget = true;
  liner.m_aWay = run.m_aWay;
  liner.m_iWay = 1;
  liner.m_vTarget = run.m_aWay[1];
  liner.m_sTargetWord = run.m_sEnd;
  RandomGenerator rng = ACR_Variety.For(string.Format("base ring line %1 %2", work.m_Base.m_sName, work.m_aLines.Count()));
  if (ACR_BaseLiner.BaysOn() && rng.RandFloat01() < 0.7)
  {
   liner.m_iBayEvery = 5 + rng.RandInt(0, 4);
   if (rng.RandFloat01() < 0.3) liner.m_fBaySide = -1;
  }
  vector start = run.m_aWay[0];
  start[1] = ACR_BaseLiner.Ground(start);
  vector heading = ACR_BaseDefences.FlatDir(start, liner.m_vTarget);
  if (run.m_sStart == "line") liner.m_vTiesIn = start;
  else if (run.m_sEnd == "line") liner.m_vTiesIn = run.m_aWay[run.m_aWay.Count() - 1];
  int side = 1;
  if (ACR_BaseDefences.DotXZ(ACR_BaseLiner.FacingOf(heading, 1), run.m_vOut) < 0) side = -1;
  // Up against what it starts from.
  bool placed;
  if (run.m_vBefore != vector.Zero)
  {
   vector back = ACR_BaseDefences.FlatDir(start, run.m_vBefore);
   array<float> backs = {5, 3, 1.5};
   foreach (float step : backs)
   {
    vector tried = start + back * step;
    vector triedWay = ACR_BaseDefences.FlatDir(tried, liner.m_vTarget);
    if (liner.Fit(tried, triedWay, LONG_STEP, side, false, true) != ACR_BaseLiner.FIT_OK) continue;
    start = tried;
    heading = triedWay;
    placed = true;
    break;
   }
  }
  if (!placed && liner.Fit(start, heading, LONG_STEP, side, false, true) != ACR_BaseLiner.FIT_OK)
  {
   vector beyond, beyondWay;
   if (!liner.PastIt(start, side, beyond, beyondWay, ACR_BaseLiner.RING_PAST_ROAD))
   {
    // Off to the side: behind wire, or clear of the road.
    vector aside = start - run.m_vOut * ACR_BaseRing.WIRE_BACK;
    if (liner.Fit(aside, heading, LONG_STEP, side, false, true) != ACR_BaseLiner.FIT_OK && !liner.PastIt(aside, side, beyond, beyondWay))
    {
     PrintFormat("[ACR] BASEWORKS %1: ring line can't start at %2 (%3)", work.m_Base.m_sName, start, ACR_BaseLiner.FitWord(liner.Fit(start, heading, LONG_STEP, side, false, true)));
     return 0;
    }
    if (liner.Fit(aside, heading, LONG_STEP, side, false, true) == ACR_BaseLiner.FIT_OK)
    {
     beyond = aside;
     beyondWay = heading;
    }
   }
   start = beyond;
   heading = beyondWay;
  }
  ACR_BaseLine line = new ACR_BaseLine();
  line.m_iSector = ACR_BaseSurvey.Sector(bearing);
  line.m_vFacing = run.m_vOut;
  line.m_bGap = true;
  line.m_bRing = true;
  line.m_sEndA = run.m_sStart;
  survey.m_iWalks++;
  int walk = Math.Min(run.m_fLength / LONG_STEP + 8, most);
  line.m_sEndB = liner.Walk(start, heading, side, walk, line.m_aRight, line.m_aLanes, line.m_aLaneOut, kit);
  // What it was heading for may not be where it stopped: an end says what it really stands on, or it's open.
  if (!line.m_aRight.IsEmpty())
  {
   if (StandsOn(line.m_sEndA) && survey.AnchorNear(line.m_aRight[0].m_vFrom, RING_STANDS).IsEmpty()) line.m_sEndA = "";
   if (StandsOn(line.m_sEndB) && survey.AnchorNear(line.m_aRight[line.m_aRight.Count() - 1].m_vTo, RING_STANDS).IsEmpty()) line.m_sEndB = "";
  }
  if (side < 0)
  {
   // The chain reads from its right end: the walk went the other way.
   foreach (ACR_LinePiece walked : line.m_aRight) line.m_aLeft.Insert(walked);
   line.m_aRight.Clear();
   string word = line.m_sEndA;
   line.m_sEndA = line.m_sEndB;
   line.m_sEndB = word;
  }
  Measure(line, survey);
  PrintFormat("[ACR] BASEWORKS %1: ring line walked %2 pieces, stopped %3, ends '%4' '%5'", work.m_Base.m_sName, line.Count(), ACR_BaseLiner.FitWord(liner.m_iStop), line.m_sEndA, line.m_sEndB);
  if (line.Count() < 1) return 0;
  work.m_aLines.Insert(line);
  int count = Realize(work, line, survey, kit, most);
  count -= DropOnRoad(line, survey);
  if (count <= 0)
  {
   work.m_aLines.RemoveItem(line);
   return 0;
  }
  line.m_iTier = tier;
  foreach (ACR_LinePiece piece : line.m_aPieces) survey.AddLight(ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(piece.m_vFacing)), 0.03);
  return count;
 }

 // An end or a curve laid close beside a road can stand on it: it comes out, a way through. How many came out.
 protected static int DropOnRoad(ACR_BaseLine line, ACR_BaseSurvey survey)
 {
  int dropped;
  array<ref ACR_LinePiece> pieces = line.m_aPieces;
  for (int i = pieces.Count() - 1; i >= 0; i--)
  {
   if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, pieces[i].m_vPos) >= 0) continue;
   if (i > 0) pieces[i - 1].m_iBreak = BREAK_ROAD;
   pieces.RemoveOrdered(i);
   dropped++;
  }
  return dropped;
 }

 // A blind stretch or a short end cut off a ring line can leave an end short of what it stood on.
 protected static string EndAfterCuts(string word, vector tip, ACR_BaseSurvey survey)
 {
  if (StandsOn(word))
  {
   string real = survey.AnchorNear(tip, RING_STANDS + 1);
   if (real.IsEmpty()) return "cut";
   return real;
  }
  if (word == "road" && ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, tip) > 5.5) return "cut";
  return word;
 }

 protected static bool StandsOn(string word)
 {
  return word == "building" || word == "wall";
 }

 // A funnel's arms round the most threatened way in, before the ring: it ties into them. The pieces used.
 static int RingFunnel(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, ACR_BaseRing ring, int most)
 {
  ACR_BaseGate way;
  float worst = -1;
  foreach (ACR_BaseGate gate : survey.m_aGates)
  {
   float threat = survey.m_aThreat[gate.m_iSector];
   if (threat <= worst) continue;
   worst = threat;
   way = gate;
  }
  if (!way) return 0;
  array<ref ACR_BaseLine> pool = {};
  RandomGenerator rng = ACR_Variety.For("base ring funnel " + work.m_Base.m_sName);
  if (!FunnelLine(work, survey, kit, way, rng, pool)) return 0;
  ACR_BaseLine line = pool[0];
  work.m_aLines.Insert(line);
  int count = Realize(work, line, survey, kit, most);
  if (count == 0)
  {
   work.m_aLines.RemoveItem(line);
   return 0;
  }
  line.m_iTier = 1;
  ring.MarkLine(line);
  return count;
 }

 // Gaps in walls and fences on the threatened side: free wall ends facing each other across open
 // ground. Closing one is the best kind of line.
 protected static void GapLines(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_BaseLine> pool)
 {
  array<vector> ends = {};
  survey.FreeWallEnds(ends);
  RandomGenerator rng = ACR_Variety.For("base line gaps " + work.m_Base.m_sName);
  int tried;
  foreach (int i, vector a : ends)
  {
   for (int k = i + 1; k < ends.Count() && tried < 6; k++)
   {
    vector b = ends[k];
    float apart = vector.DistanceXZ(a, b);
    if (apart < 2 * LONG_STEP || apart > 40) continue;
    vector middle = (a + b) * 0.5;
    float bearing = ACR_BaseDefences.Bearing(middle - survey.m_vCenter);
    int sector = ACR_BaseSurvey.Sector(bearing);
    if (survey.m_aThreat[sector] < work.m_Style.m_fMinThreat) continue;
    // Roughly across the way out, not along it.
    vector across = ACR_BaseDefences.FlatDir(a, b);
    if (Math.AbsFloat(ACR_BaseDefences.DotXZ(across, ACR_BaseSurvey.Dir(bearing))) > 0.6) continue;
    tried++;
    vector start = a + across * 0.6;
    ACR_BaseLine line = WalkFrom(work, survey, kit, start, bearing, 0, rng, sector, true, b);
    if (line) pool.Insert(line);
   }
  }
 }

 // A candidate across each way into town that's threatened: the road goes through it as a lane. Worst first,
 // a few at most: each is a walk.
 protected static void WayInLines(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_BaseLine> pool)
 {
  if (!ACR_BaseLiner.PlacementOn()) return;
  array<ACR_BaseGate> gates = {};
  foreach (ACR_BaseGate gate : survey.m_aGates)
   if (survey.m_aThreat[gate.m_iSector] >= work.m_Style.m_fMinThreat) gates.Insert(gate);
  RandomGenerator rng = ACR_Variety.For("base line ways in " + work.m_Base.m_sName);
  for (int n = 0; n < WAY_IN_WALKS && !gates.IsEmpty(); n++)
  {
   int worst = 0;
   foreach (int g, ACR_BaseGate each : gates)
    if (survey.WeakLight(each.m_iSector) > survey.WeakLight(gates[worst].m_iSector)) worst = g;
   ACR_BaseGate way = gates[worst];
   gates.Remove(worst);
   float bearing = ACR_BaseDefences.Bearing(way.m_vPos - survey.m_vCenter);
   if (work.m_Style.m_iKind == ACR_BaseStyle.FUNNEL && FunnelLine(work, survey, kit, way, rng, pool)) continue;
   // Beside the road, a step clear of it: the walk finds its far side by itself.
   vector across = Vector(way.m_vOut[2], 0, -way.m_vOut[0]);
   array<float> sides = {1, -1};
   foreach (float side : sides)
   {
    vector seed = way.m_vPos + across * (side * WAY_IN_ASIDE);
    if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, seed) < 1) continue;
    float past = vector.DistanceXZ(seed, survey.m_vCenter) - survey.EdgeSmooth(bearing);
    ACR_BaseLine line = WalkFrom(work, survey, kit, seed, bearing, past, rng, way.m_iSector, false, vector.Zero);
    if (!line || line.m_aLanes.IsEmpty()) continue;
    pool.Insert(line);
    break;
   }
  }
 }

 // A threatened way in with no line near it, once the rest is chosen: a line across it if it can have one, the
 // road its lane, short of the town's most pieces. The pieces used.
 protected static int OpenGateLines(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, int total)
 {
  int used = 0;
  RandomGenerator rng = ACR_Variety.For("base line open ways " + work.m_Base.m_sName);
  foreach (ACR_BaseGate way : survey.m_aGates)
  {
   if (survey.m_aThreat[way.m_iSector] < work.m_Style.m_fMinThreat) continue;
   if (total + used + 4 >= MOST_PIECES) break;
   if (GateHeld(work, way)) continue;
   float bearing = ACR_BaseDefences.Bearing(way.m_vPos - survey.m_vCenter);
   vector across = Vector(way.m_vOut[2], 0, -way.m_vOut[0]);
   array<float> sides = {1, -1};
   array<float> alongs = {0, 15};
   bool done = false;
   string words = "";
   foreach (float along : alongs)
   {
    if (done) break;
    foreach (float side : sides)
    {
     vector seed = way.m_vPos + way.m_vOut * along + across * (side * WAY_IN_ASIDE);
     if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, seed) < 1)
     {
      words += " road";
      continue;
     }
     float past = vector.DistanceXZ(seed, survey.m_vCenter) - survey.EdgeSmooth(bearing);
     ACR_BaseLine line = WalkFrom(work, survey, kit, seed, bearing, past, rng, way.m_iSector, false, vector.Zero);
     if (!line)
     {
      words += " walk";
      continue;
     }
     if (line.m_aLanes.IsEmpty())
     {
      words += " nolane";
      continue;
     }
     work.m_aLines.Insert(line);
     int count = Realize(work, line, survey, kit, Math.Min(GATE_PIECES, MOST_PIECES - total - used));
     if (count == 0)
     {
      words += " realize";
      work.m_aLines.RemoveItem(line);
      continue;
     }
     line.m_iTier = 2;
     used += count;
     done = true;
     PrintFormat("[ACR] BASEWORKS %1: a way in nobody held gets %2 pieces of its own", work.m_Base.m_sName, count);
     break;
    }
   }
   // Hemmed in, nowhere a line starts by itself: from the road's edge out, each way, to what it can end on.
   foreach (float plugAt : alongs)
   {
    if (done) break;
    ACR_BaseLine plug = PlugLine(work, survey, kit, way, plugAt, rng);
    if (!plug)
    {
     words += " noplug";
     continue;
    }
    work.m_aLines.Insert(plug);
    int plugged = Realize(work, plug, survey, kit, Math.Min(GATE_PIECES, MOST_PIECES - total - used));
    if (plugged == 0)
    {
     words += " plugrealize";
     work.m_aLines.RemoveItem(plug);
     continue;
    }
    plug.m_iTier = 2;
    used += plugged;
    done = true;
    PrintFormat("[ACR] BASEWORKS %1: a way in nobody held gets a plug of %2 pieces", work.m_Base.m_sName, plugged);
   }
   if (!done) PrintFormat("[ACR] BASEWORKS %1: no line for the way in at %2:%3", work.m_Base.m_sName, way.m_vPos, words);
  }
  return used;
 }

 // A short line across a way in 'along' m out from where the road leaves town: an arm from each edge of the road
 // straight across it. Null if they come to too little.
 protected static ACR_BaseLine PlugLine(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, ACR_BaseGate way, float along, RandomGenerator rng)
 {
  vector at = way.m_vPos + way.m_vOut * along;
  vector across = Vector(way.m_vOut[2], 0, -way.m_vOut[0]);
  ACR_BaseLiner liner = new ACR_BaseLiner(survey, work, ACR_BaseDefences.Bearing(way.m_vPos - survey.m_vCenter));
  liner.m_bTarget = true;
  liner.m_sTargetWord = "";
  ACR_BaseLine line = new ACR_BaseLine();
  line.m_iSector = way.m_iSector;
  line.m_vFacing = way.m_vOut;
  line.m_iSeam = BREAK_ROAD;
  survey.m_iWalks++;
  int arm = PLUG_ARM + rng.RandInt(0, 3);
  vector rightSeed, leftSeed;
  if (!RoadSide(survey, at, across, rightSeed) || !RoadSide(survey, at, across * -1, leftSeed)) return null;
  liner.m_vTarget = rightSeed + across * ((arm + 4) * LONG_STEP);
  line.m_sEndB = liner.Walk(rightSeed, across, 1, arm, line.m_aRight, line.m_aLanes, line.m_aLaneOut, kit);
  liner.m_vTarget = leftSeed - across * ((arm + 4) * LONG_STEP);
  line.m_sEndA = liner.Walk(leftSeed, across * -1, -1, arm, line.m_aLeft, line.m_aLanes, line.m_aLaneOut, kit);
  if (line.m_aRight.Count() + line.m_aLeft.Count() < 3) return null;
  // An arm that didn't go: the line ends there on the road, whatever stopped it.
  if (line.m_aRight.IsEmpty()) line.m_sEndB = "road";
  if (line.m_aLeft.IsEmpty()) line.m_sEndA = "road";
  line.m_bPlug = true;
  line.m_aLanes.Clear();
  line.m_aLaneOut.Clear();
  line.m_aLanes.Insert(at);
  line.m_aLaneOut.Insert(way.m_vOut);
  Measure(line, survey);
  return line;
 }

 // The first place clear of the road going 'way' from a point on it.
 protected static bool RoadSide(ACR_BaseSurvey survey, vector at, vector way, out vector side)
 {
  for (float off = 3; off <= 12; off += 1.5)
  {
   side = at + way * off;
   if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, side) < 1) continue;
   side[1] = ACR_BaseLiner.Ground(side);
   return true;
  }
  return false;
 }

 static bool GateHeld(ACR_BaseWork work, ACR_BaseGate gate)
 {
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   foreach (vector lane : line.m_aLanes)
    if (vector.DistanceXZ(lane, gate.m_vPos) < GATE_HELD || vector.DistanceXZ(lane, gate.m_vPos + gate.m_vOut * GATE_OUT) < GATE_HELD) return true;
   foreach (ACR_LinePiece piece : line.m_aPieces) if (vector.DistanceXZ(piece.m_vPos, gate.m_vPos) < GATE_HELD) return true;
  }
  return work.SiteWithin(gate.m_vPos, GATE_HELD + GATE_OUT, ACR_EBaseSite.ROADBLOCK);
 }

 // A funnel across a way in: an arm from each side of the road, opening out towards the enemy and facing in over
 // it, so whoever comes along the road comes into their fire. False if both arms won't go.
 protected static bool FunnelLine(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, ACR_BaseGate way, RandomGenerator rng, notnull array<ref ACR_BaseLine> pool)
 {
  vector out_way = way.m_vOut;
  vector across = Vector(out_way[2], 0, -out_way[0]);
  float open = (FUNNEL_OPEN_MIN + rng.RandFloat01() * FUNNEL_OPEN_MORE) * Math.DEG2RAD;
  int arm = FUNNEL_ARM + rng.RandInt(0, 4);
  float bearing = ACR_BaseDefences.Bearing(way.m_vPos - survey.m_vCenter);
  ACR_BaseLiner liner = new ACR_BaseLiner(survey, work, bearing);
  liner.m_bTarget = true;
  liner.m_bOpenOnly = true;
  liner.m_sTargetWord = "";
  ACR_BaseLine line = new ACR_BaseLine();
  line.m_iSector = way.m_iSector;
  line.m_vFacing = out_way;
  line.m_iSeam = BREAK_ROAD;
  survey.m_iWalks++;
  vector rightWay = across * Math.Cos(open) + out_way * Math.Sin(open);
  vector leftWay = across * -Math.Cos(open) + out_way * Math.Sin(open);
  vector rightSeed = way.m_vPos + across * WAY_IN_ASIDE;
  vector leftSeed = way.m_vPos - across * WAY_IN_ASIDE;
  rightSeed[1] = ACR_BaseLiner.Ground(rightSeed);
  leftSeed[1] = ACR_BaseLiner.Ground(leftSeed);
  if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, rightSeed) < 1 || ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, leftSeed) < 1) return false;
  liner.m_vTarget = rightSeed + rightWay * ((arm + 4) * LONG_STEP);
  line.m_sEndB = liner.Walk(rightSeed, rightWay, 1, arm, line.m_aRight, line.m_aLanes, line.m_aLaneOut, kit);
  liner.m_vTarget = leftSeed + leftWay * ((arm + 4) * LONG_STEP);
  line.m_sEndA = liner.Walk(leftSeed, leftWay, -1, arm, line.m_aLeft, line.m_aLanes, line.m_aLaneOut, kit);
  if (line.m_aRight.Count() < 3 || line.m_aLeft.Count() < 3) return false;
  line.m_aLanes.Clear();
  line.m_aLaneOut.Clear();
  line.m_aLanes.Insert(way.m_vPos);
  line.m_aLaneOut.Insert(out_way);
  Measure(line, survey);
  pool.Insert(line);
  return true;
 }

 protected static void Measure(ACR_BaseLine line, ACR_BaseSurvey survey)
 {
  float length = 0;
  foreach (ACR_LinePiece piece : line.m_aLeft) length += vector.DistanceXZ(piece.m_vFrom, piece.m_vTo);
  foreach (ACR_LinePiece other : line.m_aRight) length += vector.DistanceXZ(other.m_vFrom, other.m_vTo);
  line.m_fLength = length;
  if (line.m_aLeft.IsEmpty() && line.m_aRight.IsEmpty()) return;
  ACR_LinePiece mid;
  if (!line.m_aRight.IsEmpty()) mid = line.m_aRight[0];
  else mid = line.m_aLeft[0];
  line.m_vMiddle = mid.m_vPos;
  line.m_vFacing = mid.m_vFacing;
  // How much of it sees well out: every third piece, at a gun's height (a nest wants this).
  int seen = 0;
  int looked = 0;
  LookFrom(line.m_aLeft, seen, looked);
  LookFrom(line.m_aRight, seen, looked);
  line.m_fView = seen * 1.0 / Math.Max(looked, 1);
  int longs, seeing;
  float sum;
  CountSeeing(line.m_aLeft, longs, seeing, sum);
  CountSeeing(line.m_aRight, longs, seeing, sum);
  if (longs > 0)
  {
   line.m_fSeeing = seeing * 1.0 / longs;
   line.m_fLookMean = sum / longs;
  }
  if (!ACR_BaseLiner.PlacementOn()) return;
  // The ground as the AI's own positions are judged on it (ACR_PositionSiting): a little up, and nowhere close in
  // front where attackers can gather unseen.
  line.m_fRise = ACR_SiteScore.Elevation(line.m_vMiddle, 40);
  line.m_iDeadFront = DeadFront(line.m_vMiddle, line.m_vFacing);
 }

 // Of the spots 20-65 m out, those a man behind the bags can't see the ground at.
 static int DeadFront(vector at, vector facing)
 {
  vector eye = at;
  eye[1] = ACR_BaseLiner.Ground(at) + ACR_BaseSurvey.UP_BAGS;
  int dead = 0;
  for (float near = 20; near <= 65; near += 15)
  {
   if (!ACR_SiteScore.Visible(eye, at + facing * near)) dead++;
  }
  return dead;
 }

 protected static void CountSeeing(array<ref ACR_LinePiece> run, inout int longs, inout int seeing, inout float sum)
 {
  foreach (ACR_LinePiece piece : run)
  {
   if (!piece.m_bLong || piece.m_fLook < 0) continue;
   longs++;
   sum += piece.m_fLook;
   if (piece.m_fLook >= ACR_BaseLiner.SEES_FAR) seeing++;
  }
 }

 // An open end that sees little comes off, so the line ends where it still watches something.
 protected static void TrimBlindEnd(array<ref ACR_LinePiece> run)
 {
  while (run.Count() > 2)
  {
   ACR_LinePiece last = run[run.Count() - 1];
   if (!last.m_bLong || last.m_fLook < 0 || last.m_fLook >= ACR_BaseLiner.SEES_FAR) return;
   run.Remove(run.Count() - 1);
   run[run.Count() - 1].m_iBreak = 0;
  }
 }

 protected static void LookFrom(array<ref ACR_LinePiece> run, inout int seen, inout int looked)
 {
  for (int i = 1; i < run.Count(); i += 3)
  {
   ACR_LinePiece piece = run[i];
   vector muzzle = piece.m_vPos + piece.m_vFacing * 0.8;
   muzzle[1] = ACR_BaseLiner.Ground(muzzle) + ACR_BaseSurvey.UP_NEST;
   looked++;
   if (ACR_BaseSurvey.Reach(muzzle, ACR_BaseDefences.Bearing(piece.m_vFacing), 45) >= 44) seen++;
  }
 }

 // Up to 'budget' sandbag pieces of this tier: the best line for the worst approach, then the next, walking
 // an approach only when it's next in line. While weak approaches remain, it goes on spending. What's
 // left of the pool goes on to the next tier. The pieces used.
 protected static int Fill(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_BaseLine> pool, notnull array<int> tried, int tier, int budget)
 {
  int used = 0;
  int round = 0;
  while (budget - used >= 2)
  {
   TrimToChosen(work, pool, survey);
   // The worst approach not yet looked at, if it's worse than anything on offer.
   int next = -1;
   for (int s = 0; s < ACR_BaseSurvey.SECTORS; s++)
   {
    if (tried.Contains(s) || survey.m_aThreat[s] < work.m_Style.m_fMinThreat) continue;
    if (next < 0 || survey.WeakLight(s) > survey.WeakLight(next)) next = s;
   }
   float bestWeak = -100;
   foreach (ACR_BaseLine known : pool) bestWeak = Math.Max(bestWeak, survey.WeakLight(known.m_iSector));
   if (next >= 0 && (pool.IsEmpty() || survey.WeakLight(next) > bestWeak))
   {
    tried.Insert(next);
    WalkSector(work, survey, kit, pool, next, tier);
    continue;
   }
   if (pool.IsEmpty())
   {
    // Every approach walked and pieces left over: a second look, once.
    if (survey.m_iPass > 0 || tier > 2) break;
    survey.m_iPass = 1;
    tried.Clear();
    continue;
   }
   array<ACR_BaseLine> open = {};
   array<float> scores = {};
   foreach (ACR_BaseLine line : pool)
   {
    open.Insert(line);
    scores.Insert(Score(line, survey, work.m_Style));
   }
   int picked = ACR_Variety.Pick(scores, string.Format("base line pick %1 %2 %3", work.m_Base.m_sName, tier, round), 0.15);
   ACR_BaseLine chosen = open[picked];
   round++;
   // A strong ref until it's on the work.
   int at = pool.Find(chosen);
   work.m_aLines.Insert(chosen);
   pool.Remove(at);
   int count = Realize(work, chosen, survey, kit, budget - used);
   if (count == 0)
   {
    work.m_aLines.RemoveItem(chosen);
    continue;
   }
   chosen.m_iTier = tier;
   used += count;
   foreach (ACR_LinePiece piece : chosen.m_aPieces) survey.AddLight(ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(piece.m_vFacing)), 0.03);
  }
  return used;
 }

 protected static float Score(ACR_BaseLine line, ACR_BaseSurvey survey, ACR_BaseStyle style)
 {
  float weak = 0;
  int n = 0;
  foreach (ACR_LinePiece a : line.m_aLeft)
  {
   weak += survey.WeakLight(ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(a.m_vFacing)));
   n++;
  }
  foreach (ACR_LinePiece b : line.m_aRight)
  {
   weak += survey.WeakLight(ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(b.m_vFacing)));
   n++;
  }
  float score = weak / Math.Max(n, 1) + Math.Min(line.m_fLength, style.m_fLengthCap) / 60 + 0.3 * line.Anchors() + 0.6 * line.m_fView + style.m_fLaneWorth * line.m_aLanes.Count();
  // What its bags see: a long line of them watching fences isn't worth a short one watching a field.
  if (ACR_BaseLiner.PlacementOn())
  {
   score += 0.7 * line.m_fSeeing + 0.9 * line.m_fLookMean / ACR_BaseLiner.LOOK_FAR;
   score += RISE_WORTH * Math.Clamp(line.m_fRise, -3, 3) - DEAD_COST * line.m_iDeadFront;
  }
  if (line.m_bGap) score += 0.4;
  return score;
 }

 // Candidates against the lines chosen: one whose seed stands near one doubles it and goes; one that only
 // runs into one is cut short at the point it comes near, and its end there is open again (Realize takes
 // it on to that line). Too short then: it goes.
 protected static void TrimToChosen(ACR_BaseWork work, notnull array<ref ACR_BaseLine> pool, ACR_BaseSurvey survey)
 {
  for (int i = pool.Count() - 1; i >= 0; i--)
  {
   ACR_BaseLine line = pool[i];
   bool cut = false;
   int right = FirstNear(work, line.m_aRight, line);
   int left = FirstNear(work, line.m_aLeft, line);
   if (right == 0 || left == 0)
   {
    pool.Remove(i);
    continue;
   }
   if (right > 0)
   {
    while (line.m_aRight.Count() > right) line.m_aRight.Remove(line.m_aRight.Count() - 1);
    line.m_aRight[line.m_aRight.Count() - 1].m_iBreak = 0;
    line.m_sEndB = "";
    cut = true;
   }
   if (left > 0)
   {
    while (line.m_aLeft.Count() > left) line.m_aLeft.Remove(line.m_aLeft.Count() - 1);
    line.m_aLeft[line.m_aLeft.Count() - 1].m_iBreak = 0;
    line.m_sEndA = "";
    cut = true;
   }
   if (!cut) continue;
   Measure(line, survey);
   if (line.Count() < 2 || line.m_fLength < MIN_LINE - 10) pool.Remove(i);
  }
 }

 // The first piece of a run within CLASH of a chosen line, -1 if none.
 protected static int FirstNear(ACR_BaseWork work, array<ref ACR_LinePiece> run, ACR_BaseLine line)
 {
  foreach (ACR_BaseLine other : work.m_aLines)
  {
   if (other == line || other.m_aPieces.IsEmpty()) continue;
   if (vector.DistanceXZ(other.m_vMiddle, line.m_vMiddle) > (other.m_fLength + line.m_fLength) * 0.5 + work.m_Style.m_fClash + 2) continue;
   foreach (int i, ACR_LinePiece piece : run)
   {
    foreach (ACR_LinePiece chosen : other.m_aPieces)
     if (vector.DistanceXZ(piece.m_vPos, chosen.m_vPos) < work.m_Style.m_fClash) return i;
   }
  }
  return -1;
 }

 // The chosen line's pieces in order, cut to 'most', its open ends taken on to an anchor or curved back.
 // The pieces it has, 0 if it comes to nothing that reads as a line.
 protected static int Realize(ACR_BaseWork work, ACR_BaseLine line, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, int most)
 {
  float lookWay = ACR_BaseDefences.Bearing(line.m_vFacing);
  if (line.m_bRing) lookWay = ACR_BaseDefences.Bearing(line.m_vMiddle - survey.m_vCenter);
  ACR_BaseLiner liner = new ACR_BaseLiner(survey, work, lookWay);
  liner.m_bRing = line.m_bRing;
  if (line.m_bRing) liner.AllRoads();
  if (ACR_BaseLiner.PlacementOn() && work.m_Style.m_iKind == ACR_BaseStyle.POINTS)
  {
   RandomGenerator sides = ACR_Variety.For(string.Format("base redoubt %1 %2", work.m_Base.m_sName, work.m_aLines.Count()));
   liner.m_iReturns = 1 + sides.RandInt(0, 3);
  }
  // Curves take two pieces each.
  int room = most - 4;
  while (line.Count() > room && line.Count() > 0)
  {
   // Keep the middle: off the longer way, whose end is then open.
   if (line.m_aLeft.Count() > line.m_aRight.Count())
   {
    line.m_sEndA = "";
    line.m_aLeft.Remove(line.m_aLeft.Count() - 1);
    if (!line.m_aLeft.IsEmpty()) line.m_aLeft[line.m_aLeft.Count() - 1].m_iBreak = 0;
   }
   else
   {
    line.m_sEndB = "";
    line.m_aRight.Remove(line.m_aRight.Count() - 1);
    if (!line.m_aRight.IsEmpty()) line.m_aRight[line.m_aRight.Count() - 1].m_iBreak = 0;
   }
  }
  if (line.m_aLeft.IsEmpty() && line.m_aRight.IsEmpty()) return 0;
  if (ACR_BaseLiner.PlacementOn())
  {
   if (line.m_sEndB.IsEmpty()) TrimBlindEnd(line.m_aRight);
   if (line.m_sEndA.IsEmpty()) TrimBlindEnd(line.m_aLeft);
  }
  SeedStub(line.m_aRight, line.m_aLeft, line);
  SeedStub(line.m_aLeft, line.m_aRight, line);
  // Which end has lost its anchor to a cut.
  if (LoneTail(line.m_aRight)) line.m_sEndB = "";
  if (LoneTail(line.m_aLeft)) line.m_sEndA = "";
  // On to something to end on, if it's near; what the budget allows.
  int left = Math.Min(MOST_EXTEND, most - line.Count() - 4);
  if (line.m_sEndB.IsEmpty()) line.m_sEndB = liner.Extend(line.m_aRight, 1, left, kit, line.m_aLanes, line.m_aLaneOut);
  left = Math.Min(MOST_EXTEND, most - line.Count() - 2);
  if (line.m_sEndA.IsEmpty()) line.m_sEndA = liner.Extend(line.m_aLeft, -1, left, kit, line.m_aLanes, line.m_aLaneOut);
  // (The way there may have crossed a road and left a stub.)
  if (LoneTail(line.m_aRight)) line.m_sEndB = "";
  if (LoneTail(line.m_aLeft)) line.m_sEndA = "";
  // The seed and the way out from it each side, for a curve from there once a side's pieces have all come off. A side
  // with no pieces (a gap's, a ring's, a walk that didn't go) takes them from the other: with neither set, the curve
  // was tried at the world's origin.
  vector seedRight, seedLeft, headingRight, headingLeft;
  if (!line.m_aLeft.IsEmpty())
  {
   seedRight = line.m_aLeft[0].m_vFrom;
   headingRight = ACR_BaseDefences.FlatDir(line.m_aLeft[0].m_vTo, line.m_aLeft[0].m_vFrom);
  }
  else if (!line.m_aRight.IsEmpty())
  {
   seedRight = line.m_aRight[0].m_vFrom;
   headingRight = ACR_BaseDefences.FlatDir(line.m_aRight[0].m_vFrom, line.m_aRight[0].m_vTo);
  }
  if (!line.m_aRight.IsEmpty())
  {
   seedLeft = line.m_aRight[0].m_vFrom;
   headingLeft = ACR_BaseDefences.FlatDir(line.m_aRight[0].m_vTo, line.m_aRight[0].m_vFrom);
  }
  else if (!line.m_aLeft.IsEmpty())
  {
   seedLeft = line.m_aLeft[0].m_vFrom;
   headingLeft = ACR_BaseDefences.FlatDir(line.m_aLeft[0].m_vFrom, line.m_aLeft[0].m_vTo);
  }
  if (line.m_sEndB.IsEmpty() && liner.CurveBack(line.m_aRight, 1, kit, seedRight, headingRight)) line.m_sEndB = "curve";
  if (line.m_sEndA.IsEmpty() && liner.CurveBack(line.m_aLeft, -1, kit, seedLeft, headingLeft)) line.m_sEndA = "curve";
  // An end with nothing to stand on and no room to curve isn't a line we build. (A stretch of the ring still closes
  // what it closes.)
  if (line.m_bRing && line.m_sEndA.IsEmpty()) line.m_sEndA = "cut";
  if (line.m_bRing && line.m_sEndB.IsEmpty()) line.m_sEndB = "cut";
  if (line.m_sEndA.IsEmpty() || line.m_sEndB.IsEmpty()) return 0;
  // Left reversed, then right: the seed in the middle.
  line.m_aPieces.Clear();
  for (int i = line.m_aLeft.Count() - 1; i >= 0; i--)
  {
   ACR_LinePiece piece = line.m_aLeft[i];
   vector from = piece.m_vFrom;
   piece.m_vFrom = piece.m_vTo;
   piece.m_vTo = from;
   // What follows it now is what came before it on the walk.
   if (i > 0) piece.m_iBreak = line.m_aLeft[i - 1].m_iBreak;
   else piece.m_iBreak = line.m_iSeam;
   line.m_aPieces.Insert(piece);
  }
  foreach (ACR_LinePiece right : line.m_aRight) line.m_aPieces.Insert(right);
  if (ACR_BaseLiner.PlacementOn() && !survey.m_bLoose)
  {
   CutBlindStretches(line);
   if (!line.m_bGap) CutHuggingRuns(line, survey);
  }
  DropLoneRuns(line);
  if (ACR_BaseLiner.PlacementOn()) DropShortRuns(line);
  if (ACR_BaseLiner.PlacementOn()) liner.GateShoulders(line, kit, work);
  float length = 0;
  foreach (ACR_LinePiece counted : line.m_aPieces) length += vector.DistanceXZ(counted.m_vFrom, counted.m_vTo);
  line.m_fLength = length;
  // Short of a line, unless it closes a gap between two things.
  bool shortRing = line.m_bRing && line.Anchors() >= 1 && length >= RING_SHORT;
  if ((length < MIN_LINE && !line.Closes() && !shortRing) || length < MIN_GAP)
  {
   line.m_aPieces.Clear();
   return 0;
  }
  ACR_LinePiece mid = line.m_aPieces[line.m_aPieces.Count() / 2];
  line.m_vMiddle = mid.m_vPos;
  line.m_vFacing = mid.m_vFacing;
  line.m_iSector = ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(mid.m_vFacing));
  if (line.m_bRing)
  {
   // What's near it goes by where it stands: in a bend of the outline that isn't the way it faces.
   line.m_iSector = ACR_BaseSurvey.Sector(ACR_BaseDefences.Bearing(mid.m_vPos - survey.m_vCenter));
   line.m_sEndA = EndAfterCuts(line.m_sEndA, line.m_aPieces[0].m_vFrom, survey);
   line.m_sEndB = EndAfterCuts(line.m_sEndB, line.m_aPieces[line.m_aPieces.Count() - 1].m_vTo, survey);
   line.m_bPlug = length < RING_PLUG && line.Closes();
  }
  // The lanes it kept.
  for (int l = line.m_aLanes.Count() - 1; l >= 0; l--)
  {
   bool near;
   foreach (ACR_LinePiece kept : line.m_aPieces) if (vector.DistanceXZ(kept.m_vPos, line.m_aLanes[l]) < 14) near = true;
   if (near) continue;
   line.m_aLanes.Remove(l);
   line.m_aLaneOut.Remove(l);
  }
  return line.m_aPieces.Count();
 }

 // What's left round the seed when a break falls right after it: too short to stand by itself, it comes off
 // and the line starts from the far side of the break.
 protected static void SeedStub(array<ref ACR_LinePiece> run, array<ref ACR_LinePiece> other, ACR_BaseLine line)
 {
  foreach (ACR_LinePiece near : other) if (near.m_iBreak != 0) return;
  float length = 0;
  foreach (ACR_LinePiece behind : other) length += vector.DistanceXZ(behind.m_vFrom, behind.m_vTo);
  int cut = -1;
  foreach (int i, ACR_LinePiece piece : run)
  {
   length += vector.DistanceXZ(piece.m_vFrom, piece.m_vTo);
   if (piece.m_iBreak == 0) continue;
   cut = i;
   break;
  }
  if (cut < 0 || length >= 2 * LONG_STEP - 0.2 || cut + 1 >= run.Count()) return;
  string word = "";
  if (run[cut].m_iBreak == BREAK_ROAD) word = "road";
  if (run == line.m_aRight) line.m_sEndA = word;
  else line.m_sEndB = word;
  other.Clear();
  for (int k = 0; k <= cut; k++) run.RemoveOrdered(0);
 }

 // Four or more pieces in a row that see nothing, with a line either side of them: they come out and the line
 // has a gap there, which a man can walk through and nobody was firing from.
 protected static void CutBlindStretches(ACR_BaseLine line)
 {
  array<ref ACR_LinePiece> pieces = line.m_aPieces;
  int i = 2;
  while (i < pieces.Count() - 2)
  {
   int run = 0;
   while (i + run < pieces.Count() - 2 && pieces[i + run].m_bLong && pieces[i + run].m_iBreak == 0 && pieces[i + run].m_fLook >= 0 && pieces[i + run].m_fLook < ACR_BaseLiner.SEES_FAR) run++;
   if (run < BLIND_RUN)
   {
    i += Math.Max(run, 1);
    continue;
   }
   pieces[i - 1].m_iBreak = BREAK_WALK;
   for (int k = 0; k < run; k++) pieces.RemoveOrdered(i);
  }
 }

 // A stretch running along a wall a step behind it adds nothing the wall doesn't give: it comes out, a gap in
 // the bags where the wall does the work.
 protected static void CutHuggingRuns(ACR_BaseLine line, ACR_BaseSurvey survey)
 {
  array<ref ACR_LinePiece> pieces = line.m_aPieces;
  int i = 2;
  while (i < pieces.Count() - 2)
  {
   int run = 0;
   while (i + run < pieces.Count() - 2 && pieces[i + run].m_bLong && pieces[i + run].m_iBreak == 0 && !pieces[i + run].m_bCurve && AlongWall(survey, pieces[i + run])) run++;
   if (run < HUG_RUN)
   {
    i += Math.Max(run, 1);
    continue;
   }
   pieces[i - 1].m_iBreak = BREAK_WALK;
   for (int k = 0; k < run; k++) pieces.RemoveOrdered(i);
  }
 }

 protected static bool AlongWall(ACR_BaseSurvey survey, ACR_LinePiece piece)
 {
  vector along = ACR_BaseDefences.FlatDir(piece.m_vFrom, piece.m_vTo);
  int count = survey.m_aWallEnds.Count();
  for (int i = 0; i + 1 < count; i += 2)
  {
   vector a = survey.m_aWallEnds[i];
   vector b = survey.m_aWallEnds[i + 1];
   if (vector.DistanceXZ(a, piece.m_vPos) > HUG_NEAR + 15 && vector.DistanceXZ(b, piece.m_vPos) > HUG_NEAR + 15) continue;
   if (Math.AbsFloat(ACR_BaseDefences.DotXZ(ACR_BaseDefences.FlatDir(a, b), along)) < HUG_DOT) continue;
   ACR_BaseDefenceSeg seg = new ACR_BaseDefenceSeg();
   seg.m_vA = a;
   seg.m_vB = b;
   vector q;
   if (ACR_BaseDefences.SegDistance(seg, piece.m_vPos, q) < HUG_NEAR) return true;
  }
  return false;
 }

 // A single piece on its own between roads, or between a road and the line's end, comes off. (Only roads
 // break a line yet: walk gaps come after the nests.)
 protected static void DropLoneRuns(ACR_BaseLine line)
 {
  array<ref ACR_LinePiece> pieces = line.m_aPieces;
  for (int i = pieces.Count() - 2; i > 0; i--)
  {
   if (pieces[i].m_iBreak != 0 && pieces[i - 1].m_iBreak != 0) pieces.RemoveOrdered(i);
  }
  while (pieces.Count() >= 2 && pieces[0].m_iBreak != 0)
  {
   pieces.RemoveOrdered(0);
   line.m_sEndA = "road";
  }
  while (pieces.Count() >= 2 && pieces[pieces.Count() - 2].m_iBreak != 0)
  {
   pieces[pieces.Count() - 2].m_iBreak = 0;
   pieces.Remove(pieces.Count() - 1);
   line.m_sEndB = "road";
  }
 }

 // What a cut leaves at a line's end, too little to stand by itself, comes off and the line ends at the cut.
 protected static void DropShortRuns(ACR_BaseLine line)
 {
  array<ref ACR_LinePiece> pieces = line.m_aPieces;
  while (pieces.Count() >= 2)
  {
   float length = 0;
   int count = 0;
   foreach (ACR_LinePiece first : pieces)
   {
    length += vector.DistanceXZ(first.m_vFrom, first.m_vTo);
    count++;
    if (first.m_iBreak != 0) break;
   }
   if (count == pieces.Count() || length >= SHORT_RUN) break;
   string word = "gap";
   if (pieces[count - 1].m_iBreak == BREAK_ROAD) word = "road";
   for (int off = 0; off < count; off++) pieces.RemoveOrdered(0);
   line.m_sEndA = word;
  }
  while (pieces.Count() >= 2)
  {
   float tail = 0;
   int back = pieces.Count() - 1;
   while (back >= 0)
   {
    if (back < pieces.Count() - 1 && pieces[back].m_iBreak != 0) break;
    tail += vector.DistanceXZ(pieces[back].m_vFrom, pieces[back].m_vTo);
    back--;
   }
   if (back < 0 || tail >= SHORT_RUN) break;
   string tailWord = "gap";
   if (pieces[back].m_iBreak == BREAK_ROAD) tailWord = "road";
   pieces[back].m_iBreak = 0;
   while (pieces.Count() > back + 1) pieces.Remove(pieces.Count() - 1);
   line.m_sEndB = tailWord;
  }
 }

 // Fewer than two long pieces beyond a gap or a road aren't worth having: they come off. True if any did.
 static bool LoneTail(array<ref ACR_LinePiece> run)
 {
  bool cut = false;
  while (true)
  {
   int n = run.Count();
   int longs = 0;
   int k = n - 1;
   while (k >= 0)
   {
    if (run[k].m_bLong) longs++;
    if (k > 0 && run[k - 1].m_iBreak != 0) break;
    k--;
   }
   if (k <= 0 || longs >= 2) return cut;
   while (run.Count() > k) run.Remove(run.Count() - 1);
   run[k - 1].m_iBreak = 0;
   cut = true;
  }
  return cut;
 }

 // A line behind the first, if the town has the depth: well inside its edge on the worst approach.
 protected static void Fallback(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, notnull array<ref ACR_BaseLine> behind)
 {
  if (work.m_aLines.IsEmpty()) return;
  ACR_BaseLine front = work.m_aLines[0];
  float bearing = ACR_BaseDefences.Bearing(front.m_vFacing);
  RandomGenerator rng = ACR_Variety.For("base line back " + work.m_Base.m_sName);
  array<float> backs = {30, 42, 54};
  foreach (float back : backs)
  {
   vector seed = front.m_vMiddle - front.m_vFacing * back;
   float at = ACR_BaseDefences.Bearing(seed - survey.m_vCenter);
   // It must still be the same side of the town.
   if (vector.DistanceXZ(seed, survey.m_vCenter) < 25 || ACR_BaseDefences.DotXZ(ACR_BaseSurvey.Dir(at), front.m_vFacing) < 0.5) continue;
   float radius = vector.DistanceXZ(seed, survey.m_vCenter);
   float past = radius - survey.EdgeSmooth(at);
   ACR_BaseLine line = WalkFrom(work, survey, kit, seed, at, past, rng, ACR_BaseSurvey.Sector(bearing), false, vector.Zero);
   if (line) behind.Insert(line);
  }
 }

 // ---------------------------------------------------------------- gaps and stretches

 // Sites: stretches of 24-30 m along each line, a new one at a gap or a road. Each dressed behind. Comes
 // after the nests: gaps keep clear of them.
 // 'firstLine': from this line on (a line laid after the rest, ACR_BaseRing.FinalWalk).
 static void Stretches(ACR_BaseWork work, ACR_BaseSurvey survey, int firstLine = 0)
 {
  ACR_BaseDefenceKit kit = ACR_BaseDefences.Kit(work.m_sFaction);
  foreach (int l, ACR_BaseLine line : work.m_aLines)
  {
   if (l < firstLine) continue;
   WalkGaps(work, line, l);
   RandomGenerator rng = ACR_Variety.For(string.Format("base stretch %1 %2", work.m_Base.m_sName, l));
   float target = 24 + rng.RandFloat01() * 6;
   array<ACR_LinePiece> pieces = {};
   float length = 0;
   int order = 0;
   float left = 0;
   foreach (ACR_LinePiece counted : line.m_aPieces) left += vector.DistanceXZ(counted.m_vFrom, counted.m_vTo);
   foreach (int i, ACR_LinePiece piece : line.m_aPieces)
   {
    pieces.Insert(piece);
    float span = vector.DistanceXZ(piece.m_vFrom, piece.m_vTo);
    length += span;
    left -= span;
    bool cut = piece.m_iBreak != 0 || i == line.m_aPieces.Count() - 1;
    // A short tail goes on the stretch before it.
    if (length >= target && left >= 8) cut = true;
    if (length >= STRETCH_MOST) cut = true;
    if (!cut) continue;
    AddStretch(work, line, l, pieces, survey, kit, order);
    order++;
    pieces.Clear();
    length = 0;
   }
   line.m_iSites = order;
   PrintFormat("[ACR] BASEWORKS %1 line %2, tier %3: %4 pieces, %5 m in %6 stretches, facing %7, ends %8 and %9", work.m_Base.m_sName, l, line.m_iTier, line.m_aPieces.Count(), (int)line.m_fLength, line.m_iSites, (int)ACR_BaseDefences.Bearing(line.m_vFacing), line.m_sEndA, line.m_sEndB);
  }
 }

 // A way through on foot: a long piece left out every so often, well away from nests and from the ends.
 protected static void WalkGaps(ACR_BaseWork work, ACR_BaseLine line, int index)
 {
  RandomGenerator rng = ACR_Variety.For(string.Format("base walk %1 %2", work.m_Base.m_sName, index));
  float until = WALK_APART + rng.RandFloat01() * 10;
  float since = 0;
  array<ref ACR_LinePiece> pieces = line.m_aPieces;
  for (int i = 0; i < pieces.Count(); i++)
  {
   ACR_LinePiece piece = pieces[i];
   since += vector.DistanceXZ(piece.m_vFrom, piece.m_vTo);
   if (piece.m_iBreak != 0)
   {
    since = 0;
    continue;
   }
   // Two long pieces at least either side of it, before a road or the end.
   if (since < until || i < 3 || i + 4 >= pieces.Count() || !piece.m_bLong || pieces[i - 1].m_iBreak != 0 || pieces[i - 2].m_iBreak != 0 || pieces[i + 1].m_iBreak != 0 || pieces[i + 2].m_iBreak != 0) continue;
   if (work.SiteWithin(piece.m_vPos, NEST_CLEAR, ACR_EBaseSite.NEST)) continue;
   pieces[i - 1].m_iBreak = BREAK_WALK;
   pieces.RemoveOrdered(i);
   i--;
   since = 0;
   until = WALK_APART + rng.RandFloat01() * 10;
  }
 }

 protected static void AddStretch(ACR_BaseWork work, ACR_BaseLine line, int index, array<ACR_LinePiece> pieces, ACR_BaseSurvey survey, ACR_BaseDefenceKit kit, int order)
 {
  ACR_LinePiece mid = pieces[pieces.Count() / 2];
  ACR_BaseDefencePos pos = new ACR_BaseDefencePos();
  pos.m_eKind = ACR_EBaseDefenceKind.LINE;
  pos.m_vPos = mid.m_vPos;
  pos.m_vFacing = mid.m_vFacing;
  foreach (ACR_LinePiece piece : pieces)
  {
   pos.m_aPiecePos.Insert(piece.m_vPos);
   pos.m_aPieceYaw.Insert(piece.m_fYaw);
   pos.m_aPiecePrefab.Insert(piece.m_sPrefab);
   pos.m_aPieceWall.Insert(true);
  }
  DirtOn(pos, pieces, survey);
  // Now and then something left behind the bags, so people seem to live here: one stretch in ten, not every one.
  ACR_BaseDefences.Seed(mid.m_vPos);
  vector first = pieces[0].m_vPos;
  vector along = ACR_BaseDefences.FlatDir(first, pieces[pieces.Count() - 1].m_vPos);
  float span = vector.DistanceXZ(first, pieces[pieces.Count() - 1].m_vPos);
  int things = 0;
  if (ACR_BaseDefences.Rand() < CLUTTER_SHARE) things = 1;
  if (things > 0) ACR_BaseDefences.Dress(pos, kit, first, mid.m_vFacing, along, span, 1.5, 1.2, things, survey.m_aSegs, survey.m_aBuildings, true);
  if (ACR_BaseDefences.Rand() < LIGHT_SHARE) ACR_BaseDefences.Light(pos, kit, mid.m_vPos - mid.m_vFacing * 0.9, survey.m_aBuildings);
  for (int d = pos.m_aPiecePos.Count() - 1; d >= 0; d--)
   if (!pos.m_aPieceWall[d] && ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, pos.m_aPiecePos[d]) < 0.3) pos.RemovePiece(d);
  ACR_BaseSite site = new ACR_BaseSite();
  site.m_eKind = ACR_EBaseSite.PLANNED;
  site.m_Plan = pos;
  site.m_vPos = mid.m_vPos;
  site.m_fYaw = ACR_BaseDefences.Bearing(mid.m_vFacing);
  site.m_iSector = ACR_BaseSurvey.Sector(site.m_fYaw);
  site.m_iTier = line.m_iTier;
  site.m_iLine = index;
  site.m_iOrder = order;
  site.m_fScore = survey.WeakLight(site.m_iSector);
  work.m_aSites.Insert(site);
 }

 // ---------------------------------------------------------------- what stands on the lines

 // A nest's width on the line: the long pieces that come out for it. Its wings' bags reach 5.2 m and 4.4 m
 // from its middle (USSR 5.5 and 4.5), so 3 pieces, 10.1 m, leave the line's ends on them. FIA's wider
 // nest takes 4.
 static int NestPieces(string faction)
 {
  if (faction == "FIA") return 4;
  return 3;
 }

 // Sideways from the cut's middle so that the nest's bags, which aren't centred on it, are: they reach
 // -5.2 .. 4.5 m (US), -5.5 .. 4.6 m (USSR), -4.7 .. 7.0 m (FIA).
 static float NestShift(string faction)
 {
  if (faction == "FIA") return -1.15;
  if (faction == "USSR" || faction == "RHS_AFRF") return 0.45;
  return 0.35;
 }

 // Bags either side of a nest and its field of fire, by level: the full run first; a town that has too few
 // places takes shorter wings, then a shorter field of fire (pn23 playtest: Saint-Philippe, 101 buildings,
 // no nest at all: 16 runs too short, 9 places with no fan). 6.8 m of bags and the nest's own wings still
 // read as a wall. Last, where the big nest has no room at all: the game's small gun position (3.2 m
 // wide, as Fortify's), in the place of one long piece.
 // The last level, the small gun position: one piece each side. A compound's walls leave short, bent lines, and
 // with two Chotain Base got 1 nest of 3 (2 Nov: 23 of its 27 windows 'short or not straight').
 static int NestWings(int level)
 {
  switch (level)
  {
   case 0: return NEST_WINGS;
   case 1: return 3;
  }
  if (level >= NEST_COMPACT) return 1;
  return 2;
 }

 static float NestReach(int level)
 {
  if (level >= 3) return 30;
  return 45;
 }

 // Up to 'most' nests, built into the lines: a straight run with the fan clear and unbroken bags either
 // side, so it reads as part of a wall. Worst sector first, a weighted pick; fewer if the lines have no
 // such place even at the last level.
 static void Nests(ACR_BaseWork work, ACR_BaseSurvey survey, int most)
 {
  int made;
  int levels = NEST_LEVELS;
  if (!SmallGunsOn()) levels = NEST_COMPACT;
  for (int level = s_iFirstNestLevelForTest; level < levels && made < most; level++) made = NestsAt(work, survey, most, level, made);
 }

 // One small gun position more, where it does most to him on the ways he walks now (ACR_BaseRing.FinalWalk, with the
 // survey's kill ground the honest one for the look). How many went in.
 static int SmallGun(ACR_BaseWork work, ACR_BaseSurvey survey)
 {
  if (!SmallGunsOn()) return 0;
  return NestsAt(work, survey, 1, NEST_COMPACT, 0);
 }

 // Whether this gun stands in an emplacement built from parts rather than the game's ready-made nest: weighted
 // and seeded, where its shape fits. A wide cut gives a bastion out ahead of the line, one piece a notch.
 // False: it had to be from parts ('must') and can't be.
 protected static bool PartsFor(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseSite site, ACR_BaseLine line, int first, int width, int index, bool must, float yaw)
 {
  if (!ACR_Emplacement.On()) return !must;
  ACR_Emplacement e = EmplacementAt(work, survey, line.m_aPieces[first], line.m_aPieces[first + width - 1], site.m_fReach);
  if (!e) return !must;
  // The gun keeps the turn it was given for a way in, if the front of its bags allows that.
  float turned = Math.AbsFloat(ACR_BaseSurvey.Normal(yaw - e.m_fYaw + 180) - 180);
  if (turned <= GUN_TURN_MOST && ACR_BaseSurvey.NestFan(e.m_vPos, yaw, site.m_fReach)) e.m_fGunYaw = yaw;
  // Out ahead of the line, or turned, it may now look the same way as a gun beside it.
  if (ACR_BaseWorks.GunsNear(work, e.m_vPos, e.m_fGunYaw)) return !must;
  if (!must)
  {
   array<float> scores = {1.0, 1.0};
   if (work.m_Style.m_iKind == ACR_BaseStyle.POINTS) scores[1] = 1.1;
   if (ACR_Variety.Pick(scores, string.Format("base gun parts %1 %2", work.m_Base.m_sName, index), 0.2) != 1) return true;
  }
  site.m_Emplacement = e;
  site.m_vPos = e.m_vPos;
  site.m_fYaw = ACR_FobDefences.BearingYaw(e.m_fGunYaw);
  site.m_sPrefab = "";
  site.m_bCompact = false;
  PrintFormat("[ACR] BASEWORKS %1: gun %2 built from parts at %3 (%4 pieces)", work.m_Base.m_sName, ACR_Emplacement.ShapeWord(e.m_iShape), e.m_vPos, e.m_aPieces.Count());
  return true;
 }

 // An emplacement in the gap from one piece to another, if the ground has room for it out ahead and a field of
 // fire from there.
 protected static ACR_Emplacement EmplacementAt(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_LinePiece a, ACR_LinePiece b, float reach)
 {
  // Left and right as the enemy's the other way: the walk's order may be either.
  vector facing = (a.m_vFacing + b.m_vFacing) * 0.5;
  vector from = a.m_vFrom;
  vector to = b.m_vTo;
  if (ACR_BaseDefences.DotXZ(ACR_BaseDefences.RightOf(facing), to - from) < 0)
  {
   from = b.m_vTo;
   to = a.m_vFrom;
  }
  ACR_Emplacement e = ACR_Emplacement.Between(work.m_sFaction, from, to, facing);
  if (!e) return null;
  float half = ACR_Emplacement.TIP_X + 0.3;
  if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, e.m_vPos) < COMPACT_ROAD) return null;
  if (ACR_BaseDefences.BuildingGap(survey.m_aBuildings, e.m_vPos, COMPACT_HOUSE) < COMPACT_HOUSE) return null;
  if (survey.DoorGap(e.m_vPos) < ACR_BaseSurvey.DOOR_CLEAR) return null;
  if (e.m_iShape != ACR_Emplacement.NOTCH && !ACR_BaseDefences.WorldClear(e.m_vPos, e.m_fYaw, Vector(half, 0, 1.2), null, true)) return null;
  if (!ACR_BaseSurvey.NestFan(e.m_vPos, e.m_fYaw, reach)) return null;
  return e;
 }

 // What a place is worth to a gun beyond its sector: a way into the town in its fan, and how far it sees.
 protected static float GunWorth(ACR_BaseWork work, ACR_BaseSurvey survey, vector place, float yaw)
 {
  if (!ACR_BaseLiner.PlacementOn()) return 0;
  float worth = 0;
  vector ahead = ACR_BaseSurvey.Dir(yaw);
  foreach (ACR_BaseGate gate : survey.m_aGates)
  {
   if (survey.m_aThreat[gate.m_iSector] < ACR_BaseSurvey.MIN_SPOT_THREAT) continue;
   if (!GunCoversGate(place, yaw, gate, true)) continue;
   worth += GUN_GATE_WORTH;
   if (work.m_Style.m_bGunsOnLanes) worth += GUN_GATE_WORTH;
   break;
  }
  vector eye = place + ahead * 0.5;
  eye[1] = ACR_BaseLiner.Ground(eye) + ACR_BaseSurvey.UP_NEST;
  worth += 0.3 * ACR_BaseSurvey.Reach(eye, yaw, GUN_LOOK) / GUN_LOOK;
  return worth;
 }

 // A gun covers a way into town if a stretch of the road coming in lies in its fan, in range. 'seeing': and
 // nothing stands in the way of it.
 static bool GunCoversGate(vector place, float yaw, ACR_BaseGate gate, bool seeing)
 {
  vector ahead = ACR_BaseSurvey.Dir(yaw);
  array<float> outs = {0, 40, 80, 120};
  foreach (float out_m : outs)
  {
   vector point = gate.m_vPos + gate.m_vOut * out_m;
   vector to = point - place;
   to[1] = 0;
   float apart = to.Length();
   if (apart < 5 || apart > GUN_ON_GATE) continue;
   to = to * (1.0 / apart);
   if (ACR_BaseDefences.DotXZ(to, ahead) < GUN_GATE_DOT) continue;
   if (!seeing) return true;
   vector muzzle = place + ahead * 0.5;
   muzzle[1] = ACR_BaseLiner.Ground(muzzle) + ACR_BaseSurvey.UP_NEST;
   if (ACR_BaseSurvey.Reach(muzzle, ACR_BaseDefences.Bearing(to), apart) >= apart - 10) return true;
  }
  return false;
 }

 protected static bool WalkGapNear(ACR_BaseLine line, vector at)
 {
  foreach (ACR_LinePiece piece : line.m_aPieces)
   if (piece.m_iBreak == BREAK_WALK && vector.DistanceXZ(piece.m_vTo, at) < WALK_GAP_NEAR) return true;
  return false;
 }

 // 'yaw', or up to GATE_TURN off it where that brings a threatened way in into the fan and the fan's clear there.
 protected static float YawToGate(ACR_BaseSurvey survey, vector place, float yaw, float reach)
 {
  array<float> turns = {GATE_TURN, -GATE_TURN};
  foreach (ACR_BaseGate gate : survey.m_aGates)
  {
   if (survey.m_aThreat[gate.m_iSector] < ACR_BaseSurvey.MIN_SPOT_THREAT) continue;
   if (GunCoversGate(place, yaw, gate, false)) return yaw;
   foreach (float turn : turns)
   {
    if (!GunCoversGate(place, yaw + turn, gate, false)) continue;
    if (ACR_BaseSurvey.NestFan(place, yaw + turn, reach)) return yaw + turn;
   }
  }
  return yaw;
 }

 // The most threatened way in no gun of the town covers yet. Null: every one is covered.
 protected static ACR_BaseGate NextOpenGate(ACR_BaseWork work, ACR_BaseSurvey survey, bool seeing = false, array<ACR_BaseGate> skip = null)
 {
  if (!ACR_BaseLiner.PlacementOn()) return null;
  ACR_BaseGate worst;
  foreach (ACR_BaseGate gate : survey.m_aGates)
  {
   if (survey.m_aThreat[gate.m_iSector] < ACR_BaseSurvey.MIN_SPOT_THREAT) continue;
   if (skip && skip.Contains(gate)) continue;
   if (worst && survey.m_aThreat[gate.m_iSector] <= survey.m_aThreat[worst.m_iSector]) continue;
   bool covered = false;
   foreach (ACR_BaseSite site : work.m_aSites)
   {
    if (site.m_eKind != ACR_EBaseSite.NEST) continue;
    if (GunCoversGate(site.m_vPos, site.m_fYaw, gate, seeing)) covered = true;
   }
   if (!covered) worst = gate;
  }
  return worst;
 }

 protected static bool LaneNear(ACR_BaseLine line, vector at)
 {
  foreach (vector lane : line.m_aLanes) if (vector.DistanceXZ(lane, at) < 30) return true;
  return false;
 }

 // Those of a level, after the 'made' already placed: how many now.
 protected static int NestsAt(ACR_BaseWork work, ACR_BaseSurvey survey, int most, int level, int made)
 {
  bool compact = level >= NEST_COMPACT;
  ResourceName small = "";
  if (compact)
  {
   small = ACR_Fortify.CompactGun(work.m_sFaction);
   if (small.IsEmpty() && !ACR_Emplacement.On()) return made;
  }
  int wings = NestWings(level);
  float reach = NestReach(level);
  array<float> aside = {15, -15};
  if (level >= 2)
  {
   aside.Insert(25);
   aside.Insert(-25);
  }
  array<float> yaws = {};
  array<float> worths = {};
  array<vector> places = {};
  array<ACR_BaseLine> onLines = {};
  array<int> firsts = {};
  array<int> widths = {};
  array<bool> bends = {};
  array<ref ACR_KillSite> kills = {};
  ACR_KillGround kill = survey.m_Kill;
  float gunPower = ACR_KillGround.GUN_POWER;
  if (compact) gunPower = ACR_KillGround.GUN_POWER * 0.7;
  int width = NestPieces(work.m_sFaction);
  vector half = Vector(NEST_HALF, 0, NEST_DEPTH);
  float roadKeep = ACR_BaseSurvey.NEST_ROAD;
  float houseKeep = ACR_BaseSurvey.NEST_BUILDING;
  string smallFor = "";
  if (compact)
  {
   width = 1;
   half = Vector(COMPACT_HALF, 0, COMPACT_DEPTH);
   roadKeep = COMPACT_ROAD;
   houseKeep = COMPACT_HOUSE;
   smallFor = work.m_sFaction;
  }
  int noRun, noRoad, noHouse, noDoor, noRoom, noFire;
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   if (line.m_iTier > 2) continue;
   int count = line.m_aPieces.Count();
   for (int i = wings; i + width + wings <= count; i++)
   {
    ACR_LinePiece a = line.m_aPieces[i];
    ACR_LinePiece b = line.m_aPieces[i + width - 1];
    bool run = true;
    for (int k = i - wings; k < i + width + wings; k++)
    {
     ACR_LinePiece piece = line.m_aPieces[k];
     if (!piece.m_bLong || (k < i + width + wings - 1 && piece.m_iBreak != 0)) run = false;
    }
    if (!run || ACR_BaseDefences.DotXZ(a.m_vFacing, b.m_vFacing) < 0.85)
    {
     noRun++;
     continue;
    }
    vector facing = (a.m_vFacing + b.m_vFacing) * 0.5;
    vector middle = (a.m_vFrom + b.m_vTo) * 0.5;
    vector place = middle - facing * NEST_BEHIND + ACR_BaseDefences.RightOf(facing) * NestShift(work.m_sFaction);
    if (compact) place = middle;
    place[1] = ACR_BaseLiner.Ground(place);
    if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, place) < roadKeep)
    {
     noRoad++;
     continue;
    }
    if (ACR_BaseDefences.BuildingGap(survey.m_aBuildings, place, houseKeep) < houseKeep)
    {
     noHouse++;
     continue;
    }
    if (survey.DoorGap(place) < ACR_BaseSurvey.DOOR_CLEAR)
    {
     noDoor++;
     continue;
    }
    float yaw = ACR_BaseDefences.Bearing(facing);
    if (!ACR_BaseDefences.WorldClear(middle, yaw, half, null, true))
    {
     noRoom++;
     continue;
    }
    // The gun may look a little aside of the line's own facing.
    if (!ACR_BaseSurvey.NestFan(place, yaw, reach, smallFor))
    {
     bool found = false;
     foreach (float turn : aside)
     {
      if (!ACR_BaseSurvey.NestFan(place, yaw + turn, reach, smallFor)) continue;
      yaw += turn;
      found = true;
      break;
     }
     if (!found)
     {
      noFire++;
      continue;
     }
    }
    if (compact && ACR_BaseDefences.BuildingGap(survey.m_aBuildings, ACR_BaseSurvey.SmallGunMuzzle(place, yaw, smallFor), houseKeep) < houseKeep)
    {
     noHouse++;
     continue;
    }
    if (WalkGapNear(line, place))
    {
     noRun++;
     continue;
    }
    // Turned a little to a way in it would otherwise just miss.
    if (ACR_BaseLiner.PlacementOn()) yaw = YawToGate(survey, place, yaw, reach);
    // Not with another line's bags close in front of the gun.
    if (BagsAhead(work, place, yaw, line))
    {
     noFire++;
     continue;
    }
    bool onlyParts = compact && small.IsEmpty();
    if (onlyParts && !EmplacementAt(work, survey, a, b, reach))
    {
     noFire++;
     continue;
    }
    places.Insert(place);
    yaws.Insert(yaw);
    worths.Insert(GunWorth(work, survey, place, yaw));
    onLines.Insert(line);
    firsts.Insert(i);
    widths.Insert(width);
    bends.Insert(onlyParts);
    kills.Insert(null);
   }
   // A bend: only an emplacement built from parts will stand there.
   if (ACR_Emplacement.On())
   {
    for (int j = wings; j + 2 + wings <= count; j++)
    {
     ACR_LinePiece first = line.m_aPieces[j];
     ACR_LinePiece second = line.m_aPieces[j + 1];
     if (!first.m_bLong || !second.m_bLong || first.m_iBreak != 0 || second.m_iBreak != 0) continue;
     if (ACR_BaseDefences.DotXZ(first.m_vFacing, second.m_vFacing) > BEND_DOT) continue;
     bool whole = true;
     for (int w = j - wings; w < j + 2 + wings; w++)
     {
      if (!line.m_aPieces[w].m_bLong) whole = false;
      if (w < j + 1 + wings && line.m_aPieces[w].m_iBreak != 0) whole = false;
     }
     if (!whole) continue;
     ACR_Emplacement corner = EmplacementAt(work, survey, first, second, reach);
     if (!corner || WalkGapNear(line, corner.m_vPos) || BagsAhead(work, corner.m_vPos, corner.m_fYaw, line)) continue;
     places.Insert(corner.m_vPos);
     yaws.Insert(corner.m_fYaw);
     worths.Insert(GunWorth(work, survey, corner.m_vPos, corner.m_fYaw) + BEND_WORTH);
     onLines.Insert(line);
     firsts.Insert(j);
     widths.Insert(2);
     bends.Insert(true);
     kills.Insert(null);
    }
   }
  }
  for (; made < most; made++)
  {
   array<float> scores = {};
   array<int> open = {};
   ACR_BaseGate uncovered = NextOpenGate(work, survey);
   // (Places a few metres apart along a line do much the same: with many, every so often is enough.)
   int stride = places.Count() / MOST_GUN_LOOKS + 1;
   foreach (int c, vector at : places)
   {
    if (ACR_BaseWorks.GunsNear(work, at, yaws[c])) continue;
    float nestScore;
    if (kill)
    {
     if (c % stride != 0) continue;
     if (!kills[c]) kills[c] = kill.Site(at, yaws[c], ACR_KillGround.GUN_ARC, ACR_KillGround.GUN_RANGE, gunPower, ACR_BaseSurvey.UP_NEST);
     nestScore = kill.Value(kills[c]);
     if (nestScore < GUN_WORTH_LEAST) continue;
     if (work.m_Style.m_bGunsOnLanes && LaneNear(onLines[c], at)) nestScore *= 1.15;
    }
    else
    {
     nestScore = survey.Weak(ACR_BaseSurvey.Sector(yaws[c])) + worths[c];
     if (work.m_Style.m_bGunsOnLanes && LaneNear(onLines[c], at)) nestScore += 0.5;
     // A way in with no gun on it counts for more than an approach with few.
     if (uncovered && GunCoversGate(at, yaws[c], uncovered, false)) nestScore += GUN_OPEN_GATE;
    }
    open.Insert(c);
    scores.Insert(nestScore);
   }
   if (open.IsEmpty()) break;
   int picked = open[ACR_Variety.Pick(scores, string.Format("base nest %1 %2", work.m_Base.m_sName, made))];
   // Turned to where it does most, if its fan is clear there: a small gun's from its own muzzle (smallFor), as its
   // place was checked above, not a big nest's.
   if (kill)
   {
    array<float> gunTurns = {15, -15, 25, -25};
    float best = kill.BestFacing(places[picked], yaws[picked], gunTurns, ACR_KillGround.GUN_ARC, ACR_KillGround.GUN_RANGE, gunPower, ACR_BaseSurvey.UP_NEST);
    if (best != yaws[picked] && !ACR_BaseWorks.GunsNear(work, places[picked], best) && ACR_BaseSurvey.NestFan(places[picked], best, reach, smallFor) && !BagsAhead(work, places[picked], best, onLines[picked]))
    {
     yaws[picked] = best;
     kills[picked] = kill.Site(places[picked], best, ACR_KillGround.GUN_ARC, ACR_KillGround.GUN_RANGE, gunPower, ACR_BaseSurvey.UP_NEST);
    }
   }
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.NEST;
   site.m_iTier = 2;
   site.m_vPos = places[picked];
   site.m_fYaw = ACR_FobDefences.BearingYaw(yaws[picked]);
   site.m_iSector = ACR_BaseSurvey.Sector(yaws[picked]);
   site.m_fScore = survey.Weak(site.m_iSector);
   site.m_iSpot = ACR_FobDefences.POST_COUNT;
   site.m_sPrefab = ACR_FobDefences.PrefabFor(site.m_iSpot, work.m_sFaction, 1);
   if (compact)
   {
    site.m_sPrefab = small;
    site.m_bCompact = true;
   }
   site.m_iLine = work.m_aLines.Find(onLines[picked]);
   site.m_fReach = reach;
   for (int cut = firsts[picked]; cut < firsts[picked] + widths[picked]; cut++) site.m_aCuts.Insert(onLines[picked].m_aPieces[cut].m_vPos);
   if (!PartsFor(work, survey, site, onLines[picked], firsts[picked], widths[picked], made, bends[picked], yaws[picked]))
   {
    places.Remove(picked);
    yaws.Remove(picked);
    worths.Remove(picked);
    onLines.Remove(picked);
    firsts.Remove(picked);
    widths.Remove(picked);
    bends.Remove(picked);
    kills.Remove(picked);
    made--;
    continue;
   }
   work.m_aSites.Insert(site);
   survey.AddCover(site.m_iSector, ACR_BaseWorks.COVER_NEST);
   if (kill && kills[picked]) kill.Add(kills[picked]);
  }
  if (made < most) PrintFormat("[ACR] BASEWORKS %1: %2 of %3 MG nests: %4 places on its lines, none or too close for more (short or not straight %5, road %6, building %7, door %8, no room %9", work.m_Base.m_sName, made, most, places.Count(), noRun, noRoad, noHouse, noDoor, noRoom);
  if (made < most) PrintFormat("[ACR] BASEWORKS %1: nest places with no line of fire: %2 (level %3)", work.m_Base.m_sName, noFire, level);
  return made;
 }

 // A compound's wall is a walled base's first line: a gun in a ring of its own just outside each threatened corner
 // of it, looking out along the corner's bisector, so both faces and the ground before them are under fire (the
 // user, 2 Nov, Chotain Base: "use the actual base as defence and build on that"). Up to 'most', drawn among the
 // ones that do most to him.
 static void CornerGuns(ACR_BaseWork work, ACR_BaseSurvey survey, int most)
 {
  if (most <= 0 || !ACR_BaseLiner.PlacementOn() || !ACR_Emplacement.On() || survey.m_aWallEnds.Count() < 4) return;
  vector center = survey.m_vCenter;
  array<vector> corners = {};
  array<vector> outwards = {};
  int pieces = survey.m_aWallEnds.Count() / 2;
  int metAll, squareAll, convexAll;
  for (int i = 0; i < pieces; i++)
  {
   if (survey.m_aWallTall[i] < CORNER_TALL) continue;
   for (int j = i + 1; j < pieces; j++)
   {
    if (survey.m_aWallTall[j] < CORNER_TALL) continue;
    vector shared, d1, d2;
    if (!SharedEnd(survey, i, j, shared, d1, d2)) continue;
    metAll++;
    float dot = ACR_BaseDefences.DotXZ(d1, d2);
    if (dot > CORNER_COS_LEAST || dot < CORNER_COS_MOST) continue;
    squareAll++;
    vector outward = (d1 + d2) * -1;
    outward[1] = 0;
    if (outward.LengthSq() < 0.01) continue;
    outward.Normalize();
    if (ACR_BaseDefences.DotXZ(outward, ACR_BaseDefences.FlatDir(center, shared)) < CORNER_CONVEX) continue;
    convexAll++;
    if (ACR_BaseDefences.ThreatDot(survey.m_aDirs, survey.m_aWeights, outward) < CORNER_THREAT) continue;
    bool known = false;
    foreach (vector have : corners) if (vector.DistanceXZ(have, shared) < 3) known = true;
    if (known) continue;
    corners.Insert(shared);
    outwards.Insert(outward);
   }
  }
  if (corners.IsEmpty())
  {
   PrintFormat("[ACR] BASEWORKS %1: %2 wall pieces, no corner towards him (met %3, square %4, convex %5)", work.m_Base.m_sName, pieces, metAll, squareAll, convexAll);
   return;
  }
  // Where each gun would stand, and what it does to him there.
  array<vector> places = {};
  array<float> yaws = {};
  array<float> scores = {};
  array<ref ACR_KillSite> kills = {};
  int noRoad, noHouse, noHard, noPiece, noWorld, noFan, noWorth;
  foreach (int c, vector corner : corners)
  {
   // Just outside the corner, or a little further if the wall's own footing is in the way.
   array<float> outs = {CORNER_GUN_OUT, CORNER_GUN_OUT + 3, CORNER_GUN_OUT + 6};
   foreach (float outBy : outs)
   {
    vector at = corner + outwards[c] * outBy;
    at[1] = ACR_BaseLiner.Ground(at);
    if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, at) < COMPACT_ROAD) { noRoad++; continue; }
    if (ACR_BaseDefences.BuildingGap(survey.m_aBuildings, at, COMPACT_HOUSE) < COMPACT_HOUSE) { noHouse++; continue; }
    if (survey.DoorGap(at) < ACR_BaseSurvey.DOOR_CLEAR || ACR_Fortify.HardSurface(at)) { noHard++; continue; }
    if (work.PieceWithin(at, GATE_GUN_HALF + 1)) { noPiece++; continue; }
    if (!ACR_BaseDefences.WorldClear(at, 0, Vector(GATE_GUN_HALF, 0, GATE_GUN_HALF), null, true)) { noWorld++; continue; }
    float yaw = ACR_BaseDefences.Bearing(outwards[c]);
    if (survey.m_Kill)
    {
     array<float> turns = {15, -15, 25, -25};
     float best = survey.m_Kill.BestFacing(at, yaw, turns, ACR_KillGround.GUN_ARC, ACR_KillGround.GUN_RANGE, ACR_KillGround.GUN_POWER * 0.7, ACR_BaseSurvey.UP_NEST);
     if (best != yaw && ACR_BaseSurvey.NestFan(at, best, GATE_GUN_REACH)) yaw = best;
    }
    if (ACR_BaseWorks.GunsNear(work, at, yaw) || !ACR_BaseSurvey.NestFan(at, yaw, GATE_GUN_REACH) || BagsAhead(work, at, yaw, null)) { noFan++; continue; }
    ACR_KillSite killSite;
    float score = 1;
    if (survey.m_Kill)
    {
     killSite = survey.m_Kill.Site(at, yaw, ACR_KillGround.GUN_ARC, ACR_KillGround.GUN_RANGE, ACR_KillGround.GUN_POWER * 0.7, ACR_BaseSurvey.UP_NEST);
     score = survey.m_Kill.Value(killSite);
     if (score < LONE_GUN_WORTH) { noWorth++; continue; }
    }
    places.Insert(at);
    yaws.Insert(yaw);
    scores.Insert(score);
    kills.Insert(killSite);
    break;
   }
  }
  PrintFormat("[ACR] BASEWORKS %1: %2 wall corners towards him, %3 stand: road %4, house/hard %5, piece/world %6, fan %7, worth %8", work.m_Base.m_sName, corners.Count(), places.Count(), noRoad, noHouse + noHard, noPiece + noWorld, noFan, noWorth);
  int made = 0;
  while (made < most && !places.IsEmpty())
  {
   int picked = ACR_Variety.Pick(scores, string.Format("base corner guns %1 %2", work.m_Base.m_sName, made));
   vector at = places[picked];
   float yaw = yaws[picked];
   ACR_KillSite killSite = kills[picked];
   // (Remove swaps the last in: the same on every list keeps them in step.)
   places.Remove(picked);
   yaws.Remove(picked);
   scores.Remove(picked);
   kills.Remove(picked);
   if (ACR_BaseWorks.GunsNear(work, at, yaw)) continue;
   vector look = ACR_BaseSurvey.Dir(yaw);
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.NEST;
   site.m_iTier = 2;
   site.m_vPos = at;
   site.m_fYaw = ACR_FobDefences.BearingYaw(yaw);
   site.m_iSector = ACR_BaseSurvey.Sector(yaw);
   site.m_fScore = survey.Weak(site.m_iSector);
   site.m_iSpot = ACR_FobDefences.POST_COUNT;
   site.m_sPrefab = "";
   site.m_iLine = -1;
   site.m_fReach = GATE_GUN_REACH;
   site.m_Emplacement = ACR_Emplacement.Ring(work.m_sFaction, at, look);
   work.m_aSites.Insert(site);
   survey.AddCover(site.m_iSector, ACR_BaseWorks.COVER_NEST);
   if (killSite) survey.m_Kill.Add(killSite);
   PrintFormat("[ACR] BASEWORKS %1: a gun at the wall's corner at %2, facing %3", work.m_Base.m_sName, at, (int)yaw);
   made++;
  }
 }

 // Two wall pieces meeting at an end: that end, and each piece's way on from it.
 protected static bool SharedEnd(ACR_BaseSurvey survey, int i, int j, out vector shared, out vector d1, out vector d2)
 {
  for (int a = 0; a < 2; a++)
  {
   for (int b = 0; b < 2; b++)
   {
    vector ea = survey.m_aWallEnds[i * 2 + a];
    vector eb = survey.m_aWallEnds[j * 2 + b];
    if (vector.DistanceXZ(ea, eb) > CORNER_MEET) continue;
    shared = (ea + eb) * 0.5;
    d1 = ACR_BaseDefences.FlatDir(ea, survey.m_aWallEnds[i * 2 + 1 - a]);
    d2 = ACR_BaseDefences.FlatDir(eb, survey.m_aWallEnds[j * 2 + 1 - b]);
    return true;
   }
  }
  return false;
 }

 // A way in no gun covers, once the lines' own guns are placed: a gun in a ring of its own beside the road, looking
 // out along it. Up to 'most' of them, the most threatened first.
 static void GateGuns(ACR_BaseWork work, ACR_BaseSurvey survey, int most)
 {
  if (!ACR_BaseLiner.PlacementOn() || !ACR_Emplacement.On()) return;
  RandomGenerator rng = ACR_Variety.For("base gate guns " + work.m_Base.m_sName);
  array<float> sides = {1, -1};
  if (rng.RandFloat01() < 0.5) sides = {-1, 1};
  array<float> asides = {GATE_GUN_ASIDE, GATE_GUN_ASIDE + 6, GATE_GUN_ASIDE + 12};
  array<float> alongs = {4, 14, -6, 24};
  array<float> inwards = {0.25, 0, 0.5, -0.25};
  array<ACR_BaseGate> skip = {};
  int made = 0;
  while (made < most)
  {
   ACR_BaseGate gate = NextOpenGate(work, survey, true, skip);
   if (!gate) return;
   vector across = Vector(gate.m_vOut[2], 0, -gate.m_vOut[0]);
   bool placed = false;
   int looks = 0;
   foreach (float along : alongs)
   {
    foreach (float side : sides)
    {
     foreach (float aside : asides)
     {
      if (placed || looks >= GATE_GUN_LOOKS) break;
      vector at = gate.m_vPos + gate.m_vOut * along + across * (side * aside);
      at[1] = ACR_BaseLiner.Ground(at);
      if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, at) < COMPACT_ROAD) continue;
      if (ACR_BaseDefences.BuildingGap(survey.m_aBuildings, at, COMPACT_HOUSE) < COMPACT_HOUSE) continue;
      if (survey.DoorGap(at) < ACR_BaseSurvey.DOOR_CLEAR || ACR_Fortify.HardSurface(at)) continue;
      if (work.PieceWithin(at, GATE_GUN_HALF + 1)) continue;
      if (!ACR_BaseDefences.WorldClear(at, 0, Vector(GATE_GUN_HALF, 0, GATE_GUN_HALF), null, true)) continue;
      // Turned towards the road it's there to watch, or as near that as its fan is clear.
      foreach (float inward : inwards)
      {
       vector look = gate.m_vOut - across * (side * inward);
       look[1] = 0;
       look.Normalize();
       float yaw = ACR_BaseDefences.Bearing(look);
       looks++;
       if (!GunCoversGate(at, yaw, gate, true) || ACR_BaseWorks.GunsNear(work, at, yaw)) continue;
       if (!ACR_BaseSurvey.NestFan(at, yaw, GATE_GUN_REACH) || BagsAhead(work, at, yaw, null)) continue;
       ACR_KillSite killSite;
       if (survey.m_Kill)
       {
        killSite = survey.m_Kill.Site(at, yaw, ACR_KillGround.GUN_ARC, ACR_KillGround.GUN_RANGE, ACR_KillGround.GUN_POWER * 0.7, ACR_BaseSurvey.UP_NEST);
        if (survey.m_Kill.Value(killSite) < LONE_GUN_WORTH) continue;
       }
       ACR_BaseSite site = new ACR_BaseSite();
       site.m_eKind = ACR_EBaseSite.NEST;
       site.m_iTier = 2;
       site.m_vPos = at;
       site.m_fYaw = ACR_FobDefences.BearingYaw(yaw);
       site.m_iSector = ACR_BaseSurvey.Sector(yaw);
       site.m_fScore = survey.Weak(site.m_iSector);
       site.m_iSpot = ACR_FobDefences.POST_COUNT;
       site.m_sPrefab = "";
       site.m_iLine = -1;
       site.m_fReach = GATE_GUN_REACH;
       site.m_Emplacement = ACR_Emplacement.Ring(work.m_sFaction, at, look);
       work.m_aSites.Insert(site);
       survey.AddCover(site.m_iSector, ACR_BaseWorks.COVER_NEST);
       if (killSite) survey.m_Kill.Add(killSite);
       PrintFormat("[ACR] BASEWORKS %1: a gun on its own at the way in at %2", work.m_Base.m_sName, gate.m_vPos);
       placed = true;
       break;
      }
     }
    }
   }
   if (placed)
   {
    made++;
    continue;
   }
   PrintFormat("[ACR] BASEWORKS %1: no room for a gun at the way in at %2", work.m_Base.m_sName, gate.m_vPos);
   skip.Insert(gate);
  }
 }

 // Up to 'most' bunkers and towers tied to the lines: a bunker past a line's end, flank to its last piece,
 // its curve coming out for it; a tower right behind the line, front to the bags (a bunker's ports
 // wouldn't see over them).
 static void Posts(ACR_BaseWork work, ACR_BaseSurvey survey, int most)
 {
  array<ref ACR_BaseSpot> spots = {};
  bool towers = work.m_sFaction != "FIA";
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   if (line.m_aPieces.IsEmpty()) continue;
   EndSpots(work, survey, line, 0, 1, spots);
   EndSpots(work, survey, line, line.m_aPieces.Count() - 1, -1, spots);
   if (towers) BehindSpots(work, survey, line, spots);
  }
  int towersMade, bunkersMade;
  int made;
  for (made = 0; made < most; made++)
  {
   array<ACR_BaseSpot> open = {};
   array<float> scores = {};
   foreach (ACR_BaseSpot spot : spots)
   {
    // A tower looks the same way as the nest in front of it, on purpose.
    if (work.GunWithin(spot.m_vPos, 30)) continue;
    open.Insert(spot);
    if (survey.m_Kill)
    {
     float up = ACR_BaseSurvey.UP_BUNKER;
     if (!spot.m_bBunker) up = ACR_BaseSurvey.UP_TOWER;
     if (!spot.m_Kill) spot.m_Kill = survey.m_Kill.Site(spot.m_vPos, spot.m_fFacing, ACR_KillGround.GUN_ARC, ACR_KillGround.GUN_RANGE, ACR_KillGround.POST_POWER, up);
     scores.Insert(survey.m_Kill.Value(spot.m_Kill));
    }
    else scores.Insert(survey.Weak(spot.m_iSector) + 0.2 * spot.m_fReach / ACR_BaseSurvey.OPEN_RANGE);
   }
   if (open.IsEmpty()) break;
   ACR_BaseSpot chosen = open[ACR_Variety.Pick(scores, string.Format("base post %1 %2", work.m_Base.m_sName, made))];
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.BUNKER;
   site.m_iTier = 3;
   site.m_vPos = chosen.m_vPos;
   site.m_fYaw = ACR_FobDefences.BearingYaw(chosen.m_fFacing);
   site.m_iSector = chosen.m_iSector;
   site.m_fReach = chosen.m_fReach;
   site.m_fScore = survey.Weak(chosen.m_iSector);
   site.m_iSpot = 0;
   if (towers && ACR_BaseWorks.TowerThere(work, chosen, towersMade, bunkersMade, made)) site.m_iSpot = 1;
   if (site.m_iSpot == 1) towersMade++;
   else
   {
    bunkersMade++;
    site.m_aCuts = chosen.m_aCuts;
   }
   site.m_sPrefab = ACR_FobDefences.PrefabFor(site.m_iSpot, work.m_sFaction, 2);
   work.m_aSites.Insert(site);
   survey.AddCover(site.m_iSector, ACR_BaseWorks.COVER_BUNKER);
   if (survey.m_Kill && chosen.m_Kill) survey.m_Kill.Add(chosen.m_Kill);
  }
  if (made < most) PrintFormat("[ACR] BASEWORKS %1: %2 of %3 bunkers and towers (%4 places by its lines)", work.m_Base.m_sName, made, most, spots.Count());
 }

 // At a line's end: 'index' its last piece, 'inward' the way back along the line.
 protected static void EndSpots(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseLine line, int index, int inward, notnull array<ref ACR_BaseSpot> spots)
 {
  // Out from the last long piece; what curves back from it can make way.
  int at = index;
  array<vector> curve = {};
  array<ref ACR_LinePiece> pieces = line.m_aPieces;
  while (!pieces[at].m_bLong && at + inward >= 0 && at + inward < pieces.Count())
  {
   curve.Insert(pieces[at].m_vPos);
   at += inward;
  }
  ACR_LinePiece last = pieces[at];
  float outer = -1.0 * inward;
  vector out_way = ACR_BaseDefences.FlatDir(last.m_vFrom, last.m_vTo) * outer;
  vector tip = last.m_vTo;
  if (inward > 0) tip = last.m_vFrom;
  vector facing = last.m_vFacing;
  array<float> forwards = {6.2, 8, 10};
  array<float> backs = {2, 0, 4};
  array<float> turns = {0, 20, 40};
  int found = 0;
  foreach (float back : backs)
  {
   foreach (float forward : forwards)
   {
    if (found >= 3) return;
    vector place = tip + out_way * forward - facing * back;
    foreach (float turn : turns)
    {
     // Turned out, across the front of the line's end.
     vector look = facing * Math.Cos(turn * Math.DEG2RAD) + out_way * Math.Sin(turn * Math.DEG2RAD);
     ACR_BaseSpot spot = MakeSpot(work, survey, place, ACR_BaseDefences.Bearing(look), false, curve);
     if (!spot) continue;
     spots.Insert(spot);
     found++;
     break;
    }
   }
  }
 }

 protected static void BehindSpots(ACR_BaseWork work, ACR_BaseSurvey survey, ACR_BaseLine line, notnull array<ref ACR_BaseSpot> spots)
 {
  int count = line.m_aPieces.Count();
  array<float> backs = {7, 9, 11};
  array<vector> none = {};
  int found = 0;
  for (int i = 1; i < count && found < 6; i += 2)
  {
   ACR_LinePiece piece = line.m_aPieces[i];
   foreach (float back : backs)
   {
    ACR_BaseSpot spot = MakeSpot(work, survey, piece.m_vPos - piece.m_vFacing * back, ACR_BaseDefences.Bearing(piece.m_vFacing), true, none);
    if (!spot) continue;
    spots.Insert(spot);
    found++;
    break;
   }
  }
 }

 // A bunker's or tower's place. Their bags are measured from the composition's origin: a bunker x +-5.5, z
 // -3.5 .. 5.5 (its firing slit 4 m forward); a tower x +-2.5, z -0.5 .. 6.3 (its platform 3 m forward).
 protected static ACR_BaseSpot MakeSpot(ACR_BaseWork work, ACR_BaseSurvey survey, vector place, float facing, bool towerOnly, array<vector> mayGo)
 {
  place[1] = ACR_BaseLiner.Ground(place);
  vector look = ACR_BaseSurvey.Dir(facing);
  // What both need clear; the tower is the narrower.
  if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, place) < ACR_BaseSurvey.TOWER_ROAD) return null;
  vector towerMid = place + look * 3;
  if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, towerMid) < ACR_BaseSurvey.TOWER_ROAD) return null;
  if (ACR_BaseDefences.BuildingGap(survey.m_aBuildings, towerMid, ACR_BaseSurvey.TOWER_BUILDING) < ACR_BaseSurvey.TOWER_BUILDING) return null;
  if (survey.DoorGap(towerMid) < ACR_BaseSurvey.TOWER_DOOR) return null;
  if (work.PieceWithin(towerMid, 3.6, mayGo)) return null;
  bool tower = work.m_sFaction != "FIA" && HasRoom(towerMid, facing, Vector(3, 0, 3.8)) && ACR_BaseSurvey.TowerFan(place, facing);
  bool bunker = false;
  if (!towerOnly)
  {
   vector bunkerMid = place + look;
   bunker = ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, bunkerMid) >= ACR_BaseSurvey.GUN_ROAD && ACR_BaseDefences.BuildingGap(survey.m_aBuildings, bunkerMid, ACR_BaseSurvey.NEST_BUILDING) >= ACR_BaseSurvey.NEST_BUILDING && !work.PieceWithin(bunkerMid, 5.4, mayGo) && HasRoom(bunkerMid, facing, Vector(5.5, 0, 4.5)) && ACR_BaseSurvey.BunkerFan(place, facing) && !BagsAhead(work, place, facing, null);
  }
  if (!tower && !bunker) return null;
  ACR_BaseSpot spot = new ACR_BaseSpot();
  spot.m_vPos = place;
  spot.m_fFacing = facing;
  spot.m_iSector = ACR_BaseSurvey.Sector(facing);
  spot.m_bBunker = bunker;
  spot.m_bTower = tower;
  foreach (vector cut : mayGo) spot.m_aCuts.Insert(cut);
  vector muzzle = place;
  muzzle[1] = place[1] + ACR_BaseSurvey.UP_BUNKER;
  spot.m_fReach = Math.Max(ACR_BaseSurvey.Reach(muzzle, facing, ACR_BaseSurvey.OPEN_RANGE), 40);
  return spot;
 }

 protected static bool HasRoom(vector mid, float facing, vector half)
 {
  string why;
  if (!ACR_FobBuild.FootprintClear(mid, Math.Max(half[0], half[2]), null, why, true, ACR_FobDefences.MAX_RISE, 0.5)) return false;
  return ACR_BaseDefences.WorldClear(mid, facing, half, null, true);
 }

 // Our bags in a gun's fan, close enough to stop it (they're as high as its muzzle). Not its own line's
 // pieces beside it, which stand across, not ahead.
 protected static bool BagsAhead(ACR_BaseWork work, vector at, float facing, ACR_BaseLine own)
 {
  vector look = ACR_BaseSurvey.Dir(facing);
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   if (line == own) continue;
   foreach (ACR_LinePiece piece : line.m_aPieces)
   {
    if (vector.DistanceXZ(piece.m_vPos, at) > 30) continue;
    if (ACR_BaseDefences.DotXZ(ACR_BaseDefences.FlatDir(at, piece.m_vPos), look) > 0.9) return true;
   }
  }
  return false;
 }

 // ---------------------------------------------------------------- what the bags are like, and what's out in front

 // -acrBaseDress 0: every line plain high bags.
 static bool DressOn() { return ACR_LogisticsRules.Choice("acrBaseDress", "1") != "0"; }
 // -acrBaseObstacles 0: only wire out in front of the lines.
 static bool ObstaclesOn() { return ACR_LogisticsRules.Choice("acrBaseObstacles", "1") != "0"; }

 // Stretches that aren't plain high bags: a line that goes up first is hasty, low in places; one that comes later
 // has had time for nets. FIA makes do: low ones whenever. By the piece, seeded per town and line, so no two
 // lines are alike and no piece more stands than would anyway.
 static void DressLines(ACR_BaseWork work, int firstLine = 0)
 {
  if (!DressOn()) return;
  bool fia = work.m_sFaction == "FIA";
  bool east = ACR_Emplacement.East(work.m_sFaction);
  ResourceName low = ACR_BaseDefences.LONG_LOW_PLASTIC;
  ResourceName camo = ACR_BaseDefences.LONG_CAMO_US;
  if (east)
  {
   low = ACR_BaseDefences.LONG_LOW_BURLAP;
   camo = ACR_BaseDefences.LONG_CAMO_EAST;
  }
  foreach (int l, ACR_BaseLine line : work.m_aLines)
  {
   if (l < firstLine) continue;
   RandomGenerator rng = ACR_Variety.For(string.Format("base dress %1 %2", work.m_Base.m_sName, l));
   ResourceName swap = low;
   if (line.m_iTier > 1 && !fia) swap = camo;
   float chance = DRESS_CHANCE;
   if (fia) chance = 0.85;
   if (rng.RandFloat01() >= chance) continue;
   int runs = 1 + rng.RandInt(0, 2);
   if (fia) runs++;
   int count = line.m_aPieces.Count();
   if (count < DRESS_LEAST) continue;
   for (int r = 0; r < runs; r++)
   {
    int from = rng.RandInt(1, count - 1);
    int length = 2 + rng.RandInt(0, 3);
    for (int k = from; k < from + length && k < count; k++)
    {
     ACR_LinePiece piece = line.m_aPieces[k];
     if (!piece.m_bLong || piece.m_bCurve) break;
     piece.m_sPrefab = swap;
    }
   }
  }
 }

 protected static int Mod(int a, int b) { return a - b * (a / b); }

 // Dirt thrown up against the front of a stretch, here and there. Its slots in a stretch's plan.
 protected static void DirtOn(ACR_BaseDefencePos pos, array<ACR_LinePiece> pieces, ACR_BaseSurvey survey)
 {
  if (!DressOn()) return;
  array<ResourceName> mounds = {DIRT_LONG_1, DIRT_LONG_2, DIRT_LONG_3, DIRT_SHORT};
  foreach (int i, ACR_LinePiece piece : pieces)
  {
   if (Mod(i, DIRT_EVERY) != DIRT_EVERY / 2 || !piece.m_bLong) continue;
   vector at = piece.m_vPos + piece.m_vFacing * DIRT_OUT;
   at[1] = ACR_BaseLiner.Ground(at);
   if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, at) < 3 || ACR_Fortify.HardSurface(at)) continue;
   if (ACR_BaseDefences.BuildingGap(survey.m_aBuildings, at, 4) < 3) continue;
   if (!ACR_BaseDefences.WorldClear(at, piece.m_fYaw, Vector(2.5, 0, 1), null)) continue;
   // A mound for the place, and the same one every time for it.
   int which = Mod(Math.AbsInt((int)(at[0] * 3 + at[2] * 7)), mounds.Count());
   pos.m_aPiecePos.Insert(at);
   pos.m_aPieceYaw.Insert(piece.m_fYaw);
   pos.m_aPiecePrefab.Insert(mounds[which]);
   pos.m_aPieceWall.Insert(false);
  }
 }

 // Out in front of a line with a road through it, either side of the road: what stops a vehicle going round the
 // gate. Czech hedgehogs for regulars, log barricades for FIA. As a line's wire is, a site of its own.
 static void Obstacles(ACR_BaseWork work, ACR_BaseSurvey survey, int firstLine = 0)
 {
  if (!ObstaclesOn()) return;
  bool fia = work.m_sFaction == "FIA";
  foreach (int l, ACR_BaseLine line : work.m_aLines)
  {
   if (l < firstLine || line.m_iTier > 2 || line.m_aLanes.IsEmpty()) continue;
   RandomGenerator rng = ACR_Variety.For(string.Format("base obstacles %1 %2", work.m_Base.m_sName, l));
   if (rng.RandFloat01() >= OBSTACLE_CHANCE) continue;
   ACR_BaseDefencePos pos = new ACR_BaseDefencePos();
   pos.m_eKind = ACR_EBaseDefenceKind.WIRE;
   pos.m_vPos = line.m_vMiddle;
   pos.m_vFacing = line.m_vFacing;
   foreach (int n, vector lane : line.m_aLanes)
   {
    vector out_way = line.m_aLaneOut[n];
    vector across = Vector(out_way[2], 0, -out_way[0]);
    array<float> sides = {1, -1};
    foreach (float side : sides)
    {
     int most = OBSTACLE_MOST - rng.RandInt(0, 3);
     float step = OBSTACLE_STEP;
     if (fia)
     {
      most = 2;
      step = 7;
     }
     float out_m = OBSTACLE_OUT + rng.RandFloat01() * 5;
     for (int k = 0; k < most; k++)
     {
      vector at = lane + out_way * (out_m + Mod(k, 2) * 2.5) + across * (side * (OBSTACLE_ASIDE + k * step));
      at[1] = ACR_BaseLiner.Ground(at);
      if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, at) < OBSTACLE_ROAD) continue;
      if (ACR_BaseDefences.BuildingGap(survey.m_aBuildings, at, 4) < 3 || BBB_Terrain.IsWaterAt(at)) continue;
      float half = 1.1;
      if (fia) half = 3.4;
      float yaw = ACR_BaseDefences.Bearing(across) + rng.RandFloat01() * 40 - 20;
      if (!ACR_BaseDefences.WorldClear(at, yaw, Vector(half, 0, 1.1), null)) continue;
      pos.m_aPiecePos.Insert(at);
      pos.m_aPieceYaw.Insert(yaw);
      if (fia) pos.m_aPiecePrefab.Insert(LOGS_BARRICADE);
      else pos.m_aPiecePrefab.Insert(HEDGEHOG);
      pos.m_aPieceWall.Insert(false);
     }
    }
   }
   if (pos.m_aPiecePos.Count() < 2) continue;
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.PLANNED;
   site.m_Plan = pos;
   site.m_vPos = line.m_vMiddle;
   site.m_fYaw = ACR_BaseDefences.Bearing(line.m_vFacing);
   site.m_iSector = line.m_iSector;
   site.m_iTier = 2;
   site.m_iLine = l;
   site.m_fScore = survey.WeakLight(site.m_iSector);
   work.m_aSites.Insert(site);
   PrintFormat("[ACR] BASEWORKS %1 line %2: %3 obstacles out in front of its road", work.m_Base.m_sName, l, pos.m_aPiecePos.Count());
  }
 }

 // ---------------------------------------------------------------- wire and the command post

 // Wire in front of the lines (tier II): one site per line's middle part, a line of segments 7 m out,
 // where they fit, off the road and off a lane.
 static void Wire(ACR_BaseWork work, ACR_BaseSurvey survey, int firstLine = 0)
 {
  float length = ACR_Fortify.WireLength(survey.m_vCenter);
  foreach (int l, ACR_BaseLine line : work.m_aLines)
  {
   if (l < firstLine || line.m_iTier > 2 || line.m_fLength < 25) continue;
   ACR_BaseDefencePos pos = new ACR_BaseDefencePos();
   pos.m_eKind = ACR_EBaseDefenceKind.WIRE;
   pos.m_vPos = line.m_vMiddle;
   pos.m_vFacing = line.m_vFacing;
   int count = line.m_aPieces.Count();
   int step = Math.Max((int)Math.Ceil(length / LONG_STEP), 1);
   int mid = count / 2;
   int reach = Math.Min(MOST_WIRE / 2 * step, count / 2 - 1);
   for (int i = mid - reach; i <= mid + reach; i += step)
   {
    if (i < 0 || i >= count) continue;
    ACR_LinePiece piece = line.m_aPieces[i];
    if (!piece.m_bLong) continue;
    vector along = ACR_BaseDefences.FlatDir(piece.m_vFrom, piece.m_vTo);
    float yaw = ACR_BaseDefences.Bearing(along);
    if (ACR_Fortify.WireAlongX()) yaw -= 90;
    // As far out as the bags can still fire on it: wire holds him out of grenade range and under their fire. With
    // ground in the way, nearer.
    vector at;
    bool found = false;
    foreach (float out_m : WIRE_OUTS)
    {
     at = piece.m_vPos + piece.m_vFacing * out_m;
     at[1] = ACR_BaseLiner.Ground(at);
     if (ACR_BaseDefences.RoadEdgeGap(survey.m_aSegs, at) < 1.5 || ACR_BaseDefences.BuildingGap(survey.m_aBuildings, at, 4) < 3 || BBB_Terrain.IsWaterAt(at)) continue;
     bool lane = false;
     foreach (vector gate : line.m_aLanes) if (vector.DistanceXZ(gate, at) < 14) lane = true;
     if (lane || !ACR_BaseDefences.WorldClear(at, ACR_BaseDefences.Bearing(piece.m_vFacing), Vector(length * 0.5, 0, 0.5))) continue;
     vector eye = piece.m_vPos;
     eye[1] = ACR_BaseLiner.Ground(eye) + ACR_BaseSurvey.UP_BAGS;
     if (!ACR_BaseSurvey.SeesFrom(eye, at, 0.5)) continue;
     found = true;
     break;
    }
    if (!found) continue;
    pos.m_aPiecePos.Insert(at);
    pos.m_aPieceYaw.Insert(yaw);
    pos.m_aPiecePrefab.Insert(ACR_Fortify.WIRE);
    pos.m_aPieceWall.Insert(false);
   }
   if (pos.m_aPiecePos.Count() < 2) continue;
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.PLANNED;
   site.m_Plan = pos;
   site.m_vPos = line.m_vMiddle;
   site.m_fYaw = ACR_BaseDefences.Bearing(line.m_vFacing);
   site.m_iSector = line.m_iSector;
   site.m_iTier = 2;
   site.m_iLine = work.m_aLines.Find(line);
   site.m_fScore = survey.WeakLight(site.m_iSector);
   work.m_aSites.Insert(site);
  }
 }

 // The command post: in a building behind the worst line's middle if one has a layout, else as near
 // the town's middle as before.
 static void CommandPost(ACR_BaseWork work, ACR_BaseSurvey survey)
 {
  array<ref ACR_BaseDefencePos> posts = {};
  ACR_BaseDefences.Posts(work.m_Base, ACR_BaseDefences.Kit(work.m_sFaction), survey.m_aDirs, survey.m_aWeights, survey.m_aBuildings, posts);
  if (posts.IsEmpty()) return;
  array<float> scores = {};
  foreach (ACR_BaseDefencePos post : posts)
  {
   survey.TrimPost(post);
   float score = post.m_fScore;
   if (!work.m_aLines.IsEmpty())
   {
    ACR_BaseLine front = work.m_aLines[0];
    float apart = vector.DistanceXZ(post.m_vPos, front.m_vMiddle);
    if (ACR_BaseDefences.DotXZ(ACR_BaseDefences.FlatDir(post.m_vPos, front.m_vMiddle), front.m_vFacing) > 0.5) score += 20 - Math.Min(apart, 100) * 0.1;
   }
   scores.Insert(score);
  }
  int picked = ACR_Variety.Pick(scores, "base post " + work.m_Base.m_sName);
  ACR_BaseDefencePos chosen = posts[picked];
  ACR_BaseSite site = new ACR_BaseSite();
  site.m_eKind = ACR_EBaseSite.PLANNED;
  site.m_Plan = chosen;
  site.m_vPos = chosen.m_vPos;
  site.m_fYaw = ACR_BaseDefences.Bearing(chosen.m_vFacing);
  site.m_iSector = ACR_BaseSurvey.Sector(site.m_fYaw);
  site.m_iTier = 3;
  site.m_fScore = 1;
  work.m_aSites.Insert(site);
 }

 // ---------------------------------------------------------------- checkpoints

 // A checkpoint in a line's lane, on the road from each of the two nearest enemy bases: road, checkpoint
 // and line read as one gate. Bags its sides would touch make way when it's built (ACR_BaseWorks.GateWay,
 // from its real pieces). Worked last in its tier (NextPlan).
 static void Checkpoints(ACR_BaseWork work, ACR_BaseSurvey survey, int most)
 {
  vector center = work.m_Base.GetOrigin();
  array<ACR_Base> enemies = {};
  array<float> dists = {};
  foreach (ACR_Base other : ACR_Base.s_All)
  {
   if (!other || other.m_bRetired || other.m_sOwner.IsEmpty() || other.m_sOwner == work.m_sFaction) continue;
   enemies.Insert(other);
   dists.Insert(vector.DistanceXZ(center, other.GetOrigin()));
  }
  for (int round = 0; round < most && !enemies.IsEmpty(); round++)
  {
   int nearest = 0;
   foreach (int i, float d : dists) if (d < dists[nearest]) nearest = i;
   ACR_Base enemy = enemies[nearest];
   enemies.Remove(nearest);
   dists.Remove(nearest);
   vector toEnemy = ACR_BaseDefences.FlatDir(center, enemy.GetOrigin());
   array<vector> spots = {};
   array<float> yaws = {};
   array<float> scores = {};
   array<int> sectors = {};
   array<bool> onLine = {};
   foreach (ACR_BaseLine line : work.m_aLines)
   {
    foreach (int l, vector lane : line.m_aLanes)
    {
     if (ACR_BaseDefences.DotXZ(line.m_aLaneOut[l], toEnemy) < -0.1) continue;
     vector pos;
     float yaw;
     string refusal;
     if (!ACR_Roadblocks.ValidateSite(lane, pos, yaw, refusal, work.m_sFaction, enemy.GetOrigin(), work.m_Base)) continue;
     if (work.SiteWithin(pos, 60, ACR_EBaseSite.ROADBLOCK)) continue;
     spots.Insert(pos);
     yaws.Insert(yaw);
     sectors.Insert(line.m_iSector);
     scores.Insert(survey.Weak(line.m_iSector) + ACR_BaseDefences.DotXZ(line.m_aLaneOut[l], toEnemy));
     onLine.Insert(true);
    }
   }
   // A way in no line crosses: where it can have one, nothing else will hold it.
   string gateWhy;
   if (ACR_BaseLiner.PlacementOn())
   {
    int asked = 0;
    foreach (ACR_BaseGate open : survey.m_aGates)
    {
     if (asked >= GATE_CHECKPOINT_ASKS) break;
     if (survey.m_aThreat[open.m_iSector] < work.m_Style.m_fMinThreat) { gateWhy += " quiet"; continue; }
     if (GateHeld(work, open)) { gateWhy += " held"; continue; }
     if (ACR_BaseDefences.DotXZ(open.m_vOut, toEnemy) < -0.1) { gateWhy += " away"; continue; }
     asked++;
     vector gatePos;
     float gateYaw;
     string gateRefusal;
     if (!ACR_Roadblocks.ValidateSite(open.m_vPos, gatePos, gateYaw, gateRefusal, work.m_sFaction, enemy.GetOrigin(), work.m_Base)) { gateWhy += " site(" + gateRefusal + ")"; continue; }
     if (work.SiteWithin(gatePos, 60, ACR_EBaseSite.ROADBLOCK)) { gateWhy += " taken"; continue; }
     spots.Insert(gatePos);
     yaws.Insert(gateYaw);
     sectors.Insert(open.m_iSector);
     scores.Insert(survey.Weak(open.m_iSector) + ACR_BaseDefences.DotXZ(open.m_vOut, toEnemy) + GATE_CHECKPOINT);
     onLine.Insert(false);
    }
   }
   if (spots.IsEmpty())
   {
    PrintFormat("[ACR] BASEWORKS %1: no checkpoint for the road from %2: no line crosses a road that way; %3 gates:%4", work.m_Base.m_sName, enemy.m_sName, survey.m_aGates.Count(), gateWhy);
    continue;
   }
   int picked = ACR_Variety.Pick(scores, string.Format("base works road %1 %2", work.m_Base.m_sName, round));
   ACR_BaseSite site = new ACR_BaseSite();
   site.m_eKind = ACR_EBaseSite.ROADBLOCK;
   site.m_iTier = 2;
   site.m_bOwnGate = !onLine[picked];
   site.m_vPos = spots[picked];
   site.m_fYaw = yaws[picked];
   site.m_iSector = sectors[picked];
   site.m_fScore = survey.Weak(site.m_iSector);
   work.m_aSites.Insert(site);
   survey.AddLight(site.m_iSector, ACR_BaseWorks.COVER_CHECKPOINT);
  }
 }
}
