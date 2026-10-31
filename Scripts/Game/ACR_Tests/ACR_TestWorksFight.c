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
  if (fought % 15 == 0) Report(manager, fought);
  if (fought >= LENGTH || (fought > 20 && (Attackers(manager) == 0 || Defenders(manager) == 0)))
  {
   Report(manager, fought);
   s_bDone = true;
   PrintFormat("[ACR] WORKSFIGHT RESULT %1: attackers lost %2 of %3, defenders lost %4 of %5, walls lost %6 of %7, %8 s, line men %9", s_Base.m_sName, s_iAttackers - Attackers(manager), s_iAttackers, s_iDefenders - Defenders(manager), s_iDefenders, s_iWalls - Walls(), s_iWalls, fought, !ACR_BaseWorks.s_bNoLineMenForTest);
   ACR_TestRegress.Check("worksfight: reached the end", true, "");
   ACR_TestRegress.Complete("worksfight", 1);
  }
 }

 protected static void Setup(ACR_Manager manager)
 {
  ACR_BaseWorks.s_bNoLineMenForTest = System.IsCLIParam("acrWorksFightOldTest");
  string side = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  int most = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_bHidden || base.m_bBuilt || base.m_sOwner == east) continue;
   float nearest = 99999;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other && !other.m_bRetired && other.m_sOwner == east) nearest = Math.Min(nearest, vector.DistanceXZ(other.GetOrigin(), base.GetOrigin()));
   if (nearest < 900) continue;
   array<IEntity> buildings = {};
   ACR_BaseDefences.Buildings(base.GetOrigin(), ACR_BaseDefences.SearchRadius(base), buildings);
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
  vector outward = Vector(0, 0, 1);
  if (!s_Work.m_aLines.IsEmpty()) outward = s_Work.m_aLines[0].m_vFacing;
  // Just out of the reserve's range, wherever its post stands.
  vector from = ACR_Manager.Ground(s_Work.m_Defenders.m_vPos + outward * 450);
  manager.m_sSpawningFaction = ACR_FactionSelection.East();
  manager.SpawnUnit(ONE, ENEMY, "", false, "", from, from);
  vector beside = ACR_Manager.Ground(from + Vector(15, 0, 15));
  manager.SpawnUnit(TWO, ENEMY, "", false, "", beside, beside);
  manager.m_sSpawningFaction = "";
  PrintFormat("[ACR] WORKSFIGHT attack from %1", from);
 }

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
