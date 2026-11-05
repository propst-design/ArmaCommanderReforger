// srv:worksfight (about 6 min, Everon): does a fortified town cost an attacker anything? Two USSR rifle
// squads, nothing else, walk into a town with all its works up and its reserve of 12 out. It reports,
// it doesn't judge: the same men in the old places are srv:worksfightold, and AI fights differ by
// the run, so compare several runs of each. "[ACR] WORKSFIGHT" lines: who is left, where the men stand.
class ACR_TestWorksFight
{
 static const ResourceName ENEMY = "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et";
 static const string ONE = "Fight Attack 1";
 static const string TWO = "Fight Attack 2";
 static const int LENGTH = 300;
 protected static ACR_Base s_Base;
 protected static ACR_BaseWork s_Work;
 protected static int s_iAttackers;
 protected static int s_iDefenders;
 protected static int s_iWalls;
 protected static int s_iStart;
 protected static bool s_bDone;
 protected static int s_iDefendersLeft;
 protected static ref ACR_BagSampler s_Sampler;
 static void ACR_InitStatics()
 {
  s_Sampler = new ACR_BagSampler();
  s_aLineSamples = {};
  s_aLineSees = {};
  s_aLineBlindBags = {};
  s_aLineBlindOther = {};
  s_aLineFront = {};
  s_aLineDist = {};
 }
 protected static bool s_bShots;
 protected static bool s_bListening;
 // Per line of bags: why a man on it can't see the enemy (the bags, or the ground and trees), and where the enemy is.
 protected static ref array<int> s_aLineSamples;
 protected static ref array<int> s_aLineSees;
 protected static ref array<int> s_aLineBlindBags;
 protected static ref array<int> s_aLineBlindOther;
 protected static ref array<int> s_aLineFront;
 protected static ref array<float> s_aLineDist;
 static const float ON_LINE = 6;      // m from a piece of a line: the man is on that line

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 8) Setup(manager);
  if (!s_Base || second < 15 || s_bDone) return;
  if (second == 15) Attack(manager);
  if (second == 25) Orders(manager);
  if (s_iStart == 0)
  {
   if (s_Work.m_Defenders && s_Work.m_Defenders.m_Group) s_iStart = second;
   if (second == 240) ACR_TestRegress.Abort("worksfight", "the reserve never came out");
   return;
  }
  int fought = second - s_iStart;
  Sample(manager);
  if (fought % 15 == 0) Report(manager, fought);
  if (fought % 30 == 0 && s_Work.m_Defenders && s_Work.m_Defenders.m_Group)
  {
   array<IEntity> posts = {};
   ACR_Garrison.PostEntities(s_Work.m_Defenders.m_Group, posts);
   array<vector> places = {};
   foreach (IEntity post : posts) if (post) places.Insert(post.GetOrigin());
   ACR_BagSampler.Dump(string.Format("t=%1", fought), s_Work.m_Defenders.m_Group, places);
  }
  if (fought >= LENGTH || (fought > 20 && (Attackers(manager) == 0 || Defenders(manager) == 0)))
  {
   Report(manager, fought);
   s_bDone = true;
   s_Sampler.StopListening();
   PrintFormat("[ACR] WORKSFIGHT MEN %1", s_Sampler.Words());
   PrintFormat("[ACR] WORKSFIGHT held: into cover %1 times, sent back %2 times (%5 failed), holds %3, places lost %4", ACR_BagHold.s_iEntered, ACR_BagHold.s_iSentBack, ACR_BagHold.s_iHolds, ACR_BagHold.s_iLost, ACR_BagHold.s_iMoveFails);
   LineWords();
   PrintFormat("[ACR] WORKSFIGHT RESULT %1: attackers lost %2 of %3, defenders lost %4 of %5, walls lost %6 of %7, %8 s, line men %9", s_Base.m_sName, s_iAttackers - Attackers(manager), s_iAttackers, s_iDefenders - Defenders(manager), s_iDefenders, s_iWalls - Walls(), s_iWalls, fought, !ACR_BaseWorks.s_bNoLineMenForTest);
   ACR_TestRegress.Check("worksfight: reached the end", true, "");
   ACR_TestRegress.Complete("worksfight", 1);
  }
 }

 protected static void Setup(ACR_Manager manager)
 {
  ACR_BaseWorks.s_bNoLineMenForTest = System.IsCLIParam("acrWorksFightOldTest");
  if (System.IsCLIParam("acrWorksFightValueOffTest") || System.IsCLIParam("acrWorksFightVillageOffTest")) ACR_Options.Set("acrKillGround", "0");
  // Not always the biggest town: a village, a plan with little to hide behind.
  bool village = System.IsCLIParam("acrWorksFightVillageTest") || System.IsCLIParam("acrWorksFightVillageOffTest");
  if (System.IsCLIParam("acrWorksFightOffTest")) ACR_Options.Set("acrBagHold", "0");
  if (System.IsCLIParam("acrWorksFightCompactTest")) ACR_BaseLines.s_iFirstNestLevelForTest = ACR_BaseLines.NEST_COMPACT;
  string side = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  // -acrWorksFightBaseTest: the user's match of 2 Nov. Chotain Base, a walled military site, held against an attack
  // from Firing Range's side; the attackers stage as the AI commander does (ACR_AICommander.STAGING_DISTANCE out, on
  // its approach lines) instead of 450 m straight out.
  if (Fortress())
  {
   foreach (ACR_Base site : ACR_Base.s_All)
   {
    if (!site || site.m_bRetired) continue;
    if (site.m_sName == "Chotain Base") s_Base = site;
    else if (site.m_sName == "Firing Range" || site.m_sName == "Gravette Base") site.m_sOwner = east;
    else if (site.m_sOwner == east) site.m_sOwner = side;
   }
   if (!s_Base)
   {
    ACR_TestRegress.Abort("worksfight", "no Chotain Base here");
    return;
   }
  }
  int most = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (s_Base) break;
   if (!base || base.m_bRetired || base.m_bHidden || base.m_bBuilt || base.m_sOwner == east) continue;
   float nearest = 99999;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other && !other.m_bRetired && other.m_sOwner == east) nearest = Math.Min(nearest, vector.DistanceXZ(other.GetOrigin(), base.GetOrigin()));
   if (nearest < 900) continue;
   array<IEntity> buildings = {};
   ACR_BaseDefences.Buildings(base.GetOrigin(), ACR_BaseDefences.SearchRadius(base), buildings);
   if (village && (buildings.Count() < 12 || buildings.Count() > 30)) continue;
   if (buildings.Count() <= most) continue;
   most = buildings.Count();
   s_Base = base;
  }
  if (!s_Base)
  {
   ACR_TestRegress.Abort("worksfight", "no town");
   return;
  }
  s_Base.m_sOwner = side;
  int built = ACR_BaseWorks.BuildAllForTest(s_Base, side);
  s_Work = ACR_BaseWorks.Find(s_Base);
  ACR_BaseWorks.TiersDoneForTest(s_Work);
  PrintFormat("[ACR] WORKSFIGHT %1: %2 sites, %3 lines, reserve %4", s_Base.m_sName, built, s_Work.m_aLines.Count(), s_Work.m_Defenders.m_iReserve);
  s_iDefenders = s_Work.m_Defenders.m_iReserve;
  s_iWalls = Walls();
 }

 // From out in front of the worst approach, as an attack would come.
 protected static void Attack(ACR_Manager manager)
 {
  // Where the enemy really is: a plan's first line needn't face him (it can fire across him).
  vector outward = Vector(0, 0, 1);
  array<vector> dirs = {};
  array<float> weights = {};
  ACR_BaseDefences.Threats(s_Base, s_Base.m_sOwner, dirs, weights);
  if (!dirs.IsEmpty()) outward = dirs[0];
  else if (!s_Work.m_aLines.IsEmpty()) outward = s_Work.m_aLines[0].m_vFacing;
  // Just out of the reserve's range, wherever its post stands.
  vector from = ACR_Manager.Ground(s_Work.m_Defenders.m_vPos + outward * 450);
  vector beside = ACR_Manager.Ground(from + Vector(15, 0, 15));
  if (Fortress())
  {
   // As the AI commander stages: one squad on his main line, one on the 60-degree line nearer his second threat.
   vector center = s_Base.GetOrigin();
   float ring = s_Base.m_fRadius + ACR_AICommander.STAGING_DISTANCE;
   if (!BBB_Terrain.DryApproach(center, outward, ring, from)) from = ACR_Manager.Ground(center + outward * ring);
   vector flank = ACR_AICommander.Turned(outward, -60);
   vector other = ACR_AICommander.Turned(outward, 60);
   if (dirs.Count() > 1 && ACR_BaseDefences.DotXZ(other, dirs[1]) > ACR_BaseDefences.DotXZ(flank, dirs[1])) flank = other;
   if (!BBB_Terrain.DryApproach(center, flank, ring, beside)) beside = ACR_Manager.Ground(center + flank * ring);
  }
  manager.m_sSpawningFaction = ACR_FactionSelection.East();
  manager.SpawnUnit(ONE, ENEMY, "", false, "", from, from);
  manager.SpawnUnit(TWO, ENEMY, "", false, "", beside, beside);
  manager.m_sSpawningFaction = "";
  PrintFormat("[ACR] WORKSFIGHT attack from %1 and %2", from, beside);
 }

 static bool Fortress() { return System.IsCLIParam("acrWorksFightBaseTest"); }

 protected static void Orders(ACR_Manager manager)
 {
  vector center = ACR_Manager.Ground(s_Base.GetOrigin());
  s_iAttackers = 0;
  array<string> names = {ONE, TWO};
  foreach (string name : names)
  {
   ACR_Squad squad = Find(manager, name);
   if (!squad) continue;
   squad.MoveTo(center);
   s_iAttackers += squad.CountAlive();
  }
 }

 protected static ACR_Squad Find(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == name) return squad;
  return null;
 }

 protected static int Attackers(ACR_Manager manager)
 {
  int men = 0;
  array<string> names = {ONE, TWO};
  foreach (string name : names)
  {
   ACR_Squad squad = Find(manager, name);
   if (squad) men += squad.CountAlive();
  }
  return men;
 }

 protected static int Defenders(ACR_Manager manager)
 {
  if (!s_Work.m_Defenders) return 0;
  int men = 0;
  if (s_Work.m_Defenders.m_Group) men += manager.m_Bases.GuardsAlive(s_Work.m_Defenders.m_Group);
  if (s_Work.m_Defenders.m_GunCrew) men += manager.m_Bases.GuardsAlive(s_Work.m_Defenders.m_GunCrew);
  return men;
 }

 protected static int Walls()
 {
  int alive = 0;
  foreach (ACR_BaseSite site : s_Work.m_aSites)
   foreach (IEntity wall : site.m_aWalls) if (wall) alive++;
  return alive;
 }

 // What the men out do by the second, from their first shot on: where they stand, their stance, their fire.
 protected static void Sample(ACR_Manager manager)
 {
  SCR_AIGroup group;
  if (s_Work.m_Defenders) group = s_Work.m_Defenders.m_Group;
  if (!group) return;
  array<IEntity> enemies = {};
  array<string> names = {ONE, TWO};
  foreach (string name : names)
  {
   ACR_Squad squad = Find(manager, name);
   if (squad) ACR_BagSampler.Alive(squad.m_Group, enemies);
  }
  array<IEntity> posts = {};
  ACR_Garrison.PostEntities(group, posts);
  array<vector> places = {};
  foreach (IEntity post : posts) if (post) places.Insert(post.GetOrigin());
  array<IEntity> men = {};
  ACR_BagSampler.Alive(group, men);
  if (!s_bListening)
  {
   s_bListening = true;
   s_Sampler.ListenForShots();
  }
  foreach (IEntity man : men)
  {
   if (!s_Sampler.m_aMen.Contains(man)) s_Sampler.m_aMen.Insert(man);
   if (!s_bShots)
   {
    if (s_Sampler.Fired(man) > 0) s_bShots = true;
    continue;
   }
   IEntity enemy = ACR_BagSampler.Nearest(enemies, man.GetOrigin());
   vector facing = vector.Zero;
   foreach (ACR_BaseLine line : s_Work.m_aLines)
   {
    facing = line.m_vFacing;
    break;
   }
   s_Sampler.Sample(man, enemy, places, facing);
   LineSample(man, enemy);
  }
  s_Sampler.Deaths(GetGame().GetWorld().GetWorldTime() / 1000);
 }

 // A man on a line (within ON_LINE of one of its pieces): does he see the nearest enemy from his eye, and if not,
 // what's in the way; how far off the enemy is and whether he's in front of that line.
 protected static void LineSample(IEntity man, IEntity enemy)
 {
  if (!man || !enemy) return;
  int count = s_Work.m_aLines.Count();
  while (s_aLineSamples.Count() < count)
  {
   s_aLineSamples.Insert(0);
   s_aLineSees.Insert(0);
   s_aLineBlindBags.Insert(0);
   s_aLineBlindOther.Insert(0);
   s_aLineFront.Insert(0);
   s_aLineDist.Insert(0);
  }
  vector at = man.GetOrigin();
  int on = -1;
  float nearest = ON_LINE;
  foreach (int l, ACR_BaseLine line : s_Work.m_aLines)
  {
   foreach (ACR_LinePiece piece : line.m_aPieces)
   {
    float d = vector.DistanceXZ(piece.m_vPos, at);
    if (d >= nearest) continue;
    nearest = d;
    on = l;
   }
  }
  if (on < 0) return;
  ChimeraCharacter character = ChimeraCharacter.Cast(man);
  if (!character) return;
  vector aim = ACR_BagSampler.Target(enemy);
  s_aLineSamples[on] = s_aLineSamples[on] + 1;
  s_aLineDist[on] = s_aLineDist[on] + vector.DistanceXZ(aim, at);
  if (ACR_BaseDefences.DotXZ(s_Work.m_aLines[on].m_vFacing, ACR_BaseDefences.FlatDir(at, aim)) > 0.5) s_aLineFront[on] = s_aLineFront[on] + 1;
  TraceParam look = new TraceParam();
  look.Start = character.EyePosition();
  look.End = aim;
  look.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float view = GetGame().GetWorld().TraceMove(look, ACR_Garrison.SolidFilter);
  if (view >= 1) s_aLineSees[on] = s_aLineSees[on] + 1;
  else if (ACR_BagSampler.IsBag(look.TraceEnt)) s_aLineBlindBags[on] = s_aLineBlindBags[on] + 1;
  else s_aLineBlindOther[on] = s_aLineBlindOther[on] + 1;
 }

 protected static void LineWords()
 {
  foreach (int l, int samples : s_aLineSamples)
  {
   if (samples == 0) continue;
   ACR_BaseLine line = s_Work.m_aLines[l];
   PrintFormat("[ACR] WORKSFIGHT LINE %1 facing %2 (%3 pieces): %4 samples, sees %5%%, blind: bags %6%%, ground or trees %7%%, enemy %8 m off, in front %9%%", l, (int)ACR_BaseDefences.Bearing(line.m_vFacing), line.m_aPieces.Count(), samples, 100 * s_aLineSees[l] / samples, 100 * s_aLineBlindBags[l] / samples, 100 * s_aLineBlindOther[l] / samples, (int)(s_aLineDist[l] / samples), 100 * s_aLineFront[l] / samples);
  }
 }

 // How far the men stand from the nearest bags, on average, and how many are on a line.
 protected static void Report(ACR_Manager manager, int fought)
 {
  int onBags = 0;
  float total = 0;
  int counted = 0;
  // The base fallen, its reserve is gone with it.
  SCR_AIGroup group;
  if (s_Work.m_Defenders) group = s_Work.m_Defenders.m_Group;
  if (group)
  {
   array<AIAgent> agents = {};
   group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body || !ACR_Respawn.IsAlive(body)) continue;
    float best = 9999;
    foreach (ACR_BaseLine line : s_Work.m_aLines)
     foreach (ACR_LinePiece piece : line.m_aPieces) best = Math.Min(best, vector.DistanceXZ(piece.m_vPos, body.GetOrigin()));
    total += best;
    counted++;
    if (best < 3) onBags++;
   }
  }
  float average = 0;
  if (counted > 0) average = total / counted;
  float nearest = 9999;
  array<string> names = {ONE, TWO};
  foreach (string name : names)
  {
   ACR_Squad squad = Find(manager, name);
   if (squad && squad.GetAnchor()) nearest = Math.Min(nearest, vector.DistanceXZ(squad.GetAnchor().GetOrigin(), s_Base.GetOrigin()));
  }
  PrintFormat("[ACR] WORKSFIGHT t=%1: attackers %2 (nearest %3 m from the middle), defenders %4, on the bags %5, average %6 m from them, walls %7 of %8, owner %9", fought, Attackers(manager), (int)nearest, Defenders(manager), onBags, average.ToString(-1, 1), Walls(), s_iWalls, s_Base.m_sOwner);
  int manned, guns;
  if (s_Work.m_Defenders)
  {
   foreach (IEntity gun : s_Work.m_Defenders.m_aCrewGuns)
   {
    if (!gun || ACR_Logistics.Health(gun) <= 0) continue;
    guns++;
    if (!ACR_Defenders.FreeTurret(gun)) manned++;
   }
  }
  PrintFormat("[ACR] WORKSFIGHT t=%1: works guns manned %2 of %3", fought, manned, guns);
 }
}

// srv:worksaisoak (about 21 min, Everon): does the AI fortify? A match with Quick start, many FIA cells and
// -acrFortifyAI, AI on both sides: towns with works and sites up, by the minute. It reports; the
// floor is one site built past what Quick start gave.
class ACR_TestWorksSoak
{
 static const int LENGTH = 1200;
 protected static int s_iAtStart = -1;

 static void Run(int second)
 {
  if (second == 1) ACR_Options.Set("acrFortifyAI", "1");
  if (second < 30 || second % 60 != 0) return;
  int towns, sites;
  foreach (ACR_BaseWork work : ACR_BaseWorks.s_aWorks)
  {
   int built = ACR_BaseWorks.BuiltSites(work);
   if (built == 0) continue;
   towns++;
   sites += built;
  }
  if (s_iAtStart < 0) s_iAtStart = sites;
  PrintFormat("[ACR] WORKSSOAK t=%1: %2 towns with works, %3 sites", second, towns, sites);
  if (second < LENGTH) return;
  ACR_TestRegress.Check("worksaisoak: the AI built something past Quick start", sites > s_iAtStart, string.Format("%1 sites at the start, %2 now, %3 towns", s_iAtStart, sites, towns));
  ACR_TestRegress.Complete("worksaisoak", 1);
 }
}
