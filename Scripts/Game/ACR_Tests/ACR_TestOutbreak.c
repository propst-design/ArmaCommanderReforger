// srv:zombies (Everon whole map, about 2 min): zombie nests (ACR_Outbreak, #107), many of them.
// Without Project Outbreak loaded: no Z faction, no nests, no setup row (nothing changes).
// With it (swap Outbreak into tools/testserver-args.txt for the run): nests in open bases and the
// country, away from both starting bases; Z is never offered as a side; a US fire team 150 m from
// a nest makes it pour out; the zombies are Z, garrison groups, never squads; they close on the
// team; the team's side sees them as zombies; no base is held or being taken by Z; with the team
// gone the nest goes back in, its survivors in reserve. Then: specials (Giants, Nightingales,
// Mutants) are known and one at most comes out by day; a man the zombies kill joins them and marks
// a zombie area for his side; its AI sends a squad to clear it by day; a horde walks to another
// town and settles or joins the nest there; at night nests wake from further out and the AI sends
// nobody into a zombie area; a raid on a held base comes out inside it and contests it.
class ACR_TestOutbreak
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 protected static ACR_DefenderPost s_Nest;
 protected static float s_fStartGap;
 protected static int s_iOut;
 protected static ref ACR_DefenderPost s_Horde; // ref: a horde that joins a nest is removed from the posts
 protected static ACR_DefenderPost s_Raid;
 protected static ACR_Base s_RaidBase;
 protected static int s_iRaidInside;
 protected static bool s_bRaidContested;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 5) SetHour(12);
  if (second == 10)
  {
   if (!ACR_Outbreak.Loaded()) { WithoutOutbreak(); return; }
   Placement();
  }
  if (!s_Nest) return;
  if (second == 12) Approach(manager);
  if (second == 24) PouredOut(manager);
  if (second == 25) IntoTown(manager);
  if (second == 44) Closing(manager);
  if (second == 45) KilledJoin();
  if (second == 46) Away(manager);
  if (second == 66) BackIn();
  if (second == 68) Specials();
  if (second == 69) SendHorde();
  if (second == 82) HordeArrived();
  if (second == 83) Spawn(manager, "Sweep Team", s_Nest.m_vPos, 700);
  if (second == 86) SweepByDay(manager);
  if (second == 88) SetHour(23);
  if (second == 101) NoSweepAtNight(manager);
  if (second == 102) SendRaid();
  if (second > 102 && second < 122) RaidWatch();
  if (second == 122) RaidInside();
 }

 protected static void SetHour(int hour)
 {
  ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
  if (world && world.GetTimeAndWeatherManager()) world.GetTimeAndWeatherManager().SetHoursMinutesSeconds(hour, 0, 0, true);
 }

 protected static void KilledJoin()
 {
  int before = s_Nest.m_iReserve + s_Nest.m_Nest.m_iRisen;
  ACR_Outbreak.OnLoss("US", s_Nest.m_vPos, ACR_Outbreak.FACTION);
  int after = s_Nest.m_iReserve + s_Nest.m_Nest.m_iRisen;
  ACR_DangerRoad area = Area("US");
  Check("zombies: a man they kill joins them and marks a zombie area for his side", after == before + 1 && area != null && Area("USSR") == null, string.Format("%1 -> %2, area %3", before, after, area != null));
 }

 protected static ACR_DangerRoad Area(string side)
 {
  foreach (ACR_DangerRoad road : ACR_Insurgency.s_aRoads)
   if (road.m_bZombies && road.m_sSide == side && vector.DistanceXZ(road.m_vPos, s_Nest.m_vPos) < ACR_Outbreak.ZONE_RADIUS) return road;
  return null;
 }

 protected static void Specials()
 {
  string specials = ACR_Outbreak.Specials();
  bool loaded;
  ACR_Manager manager = ACR_Manager.GetInstance();
  foreach (ResourceName kind : manager.m_Bases.GetFaction(ACR_Outbreak.FACTION).m_aGarrison)
   if (kind.Contains("Giant") || kind.Contains("Mutant") || kind.Contains("Nurse")) loaded = true;
  if (loaded) Check("zombies: Giants, Nightingales and Mutants are known as specials", !specials.IsEmpty() && !specials.Contains("Shambler"), specials);
  else ACR_TestRegress.Skip("zombies: Giants, Nightingales and Mutants are known as specials", "no special creatures loaded");
  array<ResourceName> picks = {};
  ACR_Outbreak.Picks(60, picks);
  int count;
  foreach (ResourceName pick : picks) if (ACR_Outbreak.IsSpecial(pick)) count++;
  Check("zombies: one special at most comes out by day", picks.Count() == 60 && count <= 1, string.Format("%1 of %2", count, picks.Count()));
 }

 protected static void SendHorde()
 {
  // A nest with a town in horde reach (a country nest at the map's edge may have none).
  ACR_DefenderPost from;
  vector goal;
  string goalName;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
   if (!from && post.m_Nest && !post.m_Nest.m_bRoaming && !post.m_Group && !post.m_bGone && post.m_iReserve >= 8 && post != s_Nest && ACR_Outbreak.PickGoal(post.m_vPos, goal, goalName)) from = post;
  bool sent = from && ACR_Outbreak.SendHorde(from, 4, goal, goalName);
  foreach (ACR_DefenderPost horde : ACR_Defenders.s_Posts) if (horde.m_Nest && horde.m_Nest.m_bRoaming) s_Horde = horde;
  Check("zombies: a nest sends a horde to another town", sent && s_Horde != null, string.Format("to %1", goalName));
  ACR_Outbreak.s_fWalkScaleForTest = 400;
 }

 protected static void HordeArrived()
 {
  ACR_Outbreak.s_fWalkScaleForTest = 1;
  bool done = s_Horde && (s_Horde.m_bGone || !s_Horde.m_Nest.m_bRoaming);
  string detail = "no horde";
  if (s_Horde) detail = string.Format("%1, gone %2, roaming %3, reserve %4", s_Horde.m_sName, s_Horde.m_bGone, s_Horde.m_Nest.m_bRoaming, s_Horde.m_iReserve);
  Check("zombies: the horde walks there and settles or joins the nest", done, detail);
 }

 protected static void Spawn(ACR_Manager manager, string name, vector at, float distance)
 {
  vector dir = Vector(6400, 0, 6400) - at;
  dir[1] = 0;
  dir.Normalize();
  vector spot = ACR_Manager.Ground(at + dir * distance);
  manager.m_sSpawningFaction = "US";
  manager.SpawnUnit(name, FIRE_TEAM, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }

 protected static void SweepByDay(ACR_Manager manager)
 {
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  if (ai) ai.ManageDangerRoadsForTest();
  ACR_DangerRoad area = Area("US");
  bool sent = area && area.m_Sweeper && area.m_Sweeper.m_iAITask == ACR_EAITask.SWEEP;
  Check("zombies: by day its AI sends a squad to clear the zombie area", sent, string.Format("area %1, ai %2", area != null, ai != null));
  if (!area || !area.m_Sweeper) return;
  area.m_Sweeper.m_iAITask = ACR_EAITask.FREE;
  area.m_Sweeper = null;
  area.m_iSweeps = 0;
 }

 protected static void NoSweepAtNight(ACR_Manager manager)
 {
  Check("zombies: at night nests wake from further out", ACR_Outbreak.Night() && ACR_Outbreak.Range() == ACR_Outbreak.NIGHT_RANGE, string.Format("night %1, range %2", ACR_Outbreak.Night(), ACR_Outbreak.Range()));
  ACR_AICommander ai = manager.FobAICommanderFor("US");
  if (ai) ai.ManageDangerRoadsForTest();
  ACR_DangerRoad area = Area("US");
  Check("zombies: at night its AI sends nobody into a zombie area", area && !area.m_Sweeper, string.Format("area %1", area != null));
 }

 // A raid on the US starting base (nightfall never raids an HQ; this world has no other held base).
 protected static void SendRaid()
 {
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sOwner == "US" && !base.m_bPocket) s_RaidBase = base;
  ACR_DefenderPost from;
  float best = 1000000;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!s_RaidBase || !post.m_Nest || post.m_Nest.m_bRoaming || post.m_Group || post.m_bGone || post.m_iReserve < 8) continue;
   float d = vector.DistanceXZ(post.m_vPos, s_RaidBase.GetOrigin());
   if (d < best) { best = d; from = post; }
  }
  if (from) ACR_Outbreak.SendHorde(from, 4, s_RaidBase.GetOrigin(), s_RaidBase.m_sName, s_RaidBase);
  foreach (ACR_DefenderPost raid : ACR_Defenders.s_Posts) if (raid.m_Nest && raid.m_Nest.m_Raid) s_Raid = raid;
  ACR_Outbreak.s_fWalkScaleForTest = 500;
 }

 // The base's defenders may kill the raid within seconds: watch it every second.
 protected static void RaidWatch()
 {
  int inside;
  if (s_Raid && s_Raid.m_Group)
  {
   array<AIAgent> agents = {};
   s_Raid.m_Group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity zombie = agent.GetControlledEntity();
    if (zombie && ACR_Respawn.IsAlive(zombie) && vector.DistanceXZ(zombie.GetOrigin(), s_RaidBase.GetOrigin()) <= s_RaidBase.m_fRadius) inside++;
   }
  }
  s_iRaidInside = Math.Max(s_iRaidInside, inside);
  if (s_RaidBase && s_RaidBase.m_bContested) s_bRaidContested = true;
 }

 protected static void RaidInside()
 {
  ACR_Outbreak.s_fWalkScaleForTest = 1;
  bool held = s_RaidBase && s_RaidBase.m_sOwner == "US";
  Check("zombies: a raid comes out inside the held base and contests it, never takes it", s_iRaidInside > 0 && s_bRaidContested && held, string.Format("raid %1, %2 inside at most, contested %3, still held %4", s_Raid != null, s_iRaidInside, s_bRaidContested, held));
  Finish();
 }

 protected static void WithoutOutbreak()
 {
  int nests;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts) if (post.m_Nest) nests++;
  Check("zombies: without Outbreak, no Z faction and no nests", nests == 0, string.Format("%1 nests", nests));
  Check("zombies: without Outbreak, no Zombies setup row", !ACR_Setup.Rows().Contains("zombies\t"), "");
  ACR_TestRegress.Complete("zombies-off", 2);
 }

 protected static void Placement()
 {
  array<vector> starts = {};
  foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_sOwner.IsEmpty() && !base.m_bPocket && !base.m_bRetired) starts.Insert(base.GetOrigin());
  int atBases, country;
  bool clear = true;
  string detail = "";
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Nest) continue;
   if (post.m_Nest.m_sBase.IsEmpty()) country++;
   else atBases++;
   if (!s_Nest || (s_Nest.m_Nest.m_sBase.IsEmpty() && !post.m_Nest.m_sBase.IsEmpty())) s_Nest = post;
   foreach (vector start : starts) if (vector.DistanceXZ(post.m_vPos, start) < ACR_Outbreak.HQ_CLEAR) clear = false;
   detail += string.Format(" %1(%2)", post.m_sName, post.m_iReserve);
  }
  ACR_Manager manager = ACR_Manager.GetInstance();
  int kinds = manager.m_Bases.GetFaction(ACR_Outbreak.FACTION).m_aGarrison.Count();
  Check("zombies: nests in open bases and the country", atBases >= 1 && country >= 1 && kinds > 0, string.Format("%1 at bases, %2 in the country, %3 kinds:%4", atBases, country, kinds, detail));
  Check("zombies: away from both starting bases", clear, "");
  array<string> keys = {};
  ACR_FactionSelection.Candidates(keys);
  string sides = "";
  foreach (string key : keys) sides += " " + key;
  Check("zombies: Z is never offered as a side", !keys.Contains(ACR_Outbreak.FACTION), sides);
  Check("zombies: the setup screen has a Zombies row", ACR_Setup.Rows().Contains("zombies\t"), "");
  // A dedicated server draws no blood or fire; the garbage it spawns (replicated) shows it dressed them.
  Check("zombies: nest sites are dressed (garbage piles from the server)", ACR_NestDressing.s_iPiles * 2 >= atBases + country, string.Format("%1 piles for %2 nests", ACR_NestDressing.s_iPiles, atBases + country));
  NestLook(atBases + country);
  TruceHit(manager);
  if (!s_Nest) Finish();
 }

 // The nests' landmark (ACR_NestDressing): burnt-out cars laid here too (collision), remains, every
 // site burning at a tier; a cleared site goes out and burns again when the dead come back. The smoke
 // and fire themselves are client-only (untested here).
 protected static void NestLook(int nests)
 {
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  int live;
  string detail = "";
  ACR_DefenderPost other;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Nest || post.m_Nest.m_bRoaming) continue;
   int tier = mode.ACR_NestSiteTier(post.m_vPos);
   if (tier > 0) live++;
   detail += string.Format(" %1:t%2", post.m_iReserve, tier);
   if (post != s_Nest && !other) other = post;
  }
  Check("zombies: every nest site burns, by size", live == nests, string.Format("%1 of %2 live:%3", live, nests, detail));
  Check("zombies: burnt-out cars and remains round the nests", ACR_NestDressing.s_iWrecks >= nests && ACR_NestDressing.s_iRemains >= nests, string.Format("%1 wrecks, %2 remains for %3 nests", ACR_NestDressing.s_iWrecks, ACR_NestDressing.s_iRemains, nests));
  if (!other) { Check("zombies: a cleared nest goes out, and burns again when the dead come back", false, "no second nest"); return; }
  int before = mode.ACR_NestSiteTier(other.m_vPos);
  int wrecksBefore = ACR_NestDressing.s_iWrecks;
  ACR_NestDressing.Cleared(other.m_vPos);
  int cleared = mode.ACR_NestSiteTier(other.m_vPos);
  ACR_NestDressing.Add(other.m_vPos, other.m_Nest.m_iSize);
  int again = mode.ACR_NestSiteTier(other.m_vPos);
  Check("zombies: a cleared nest goes out, and burns again when the dead come back (no second dressing)", before > 0 && cleared == -before && again == before && ACR_NestDressing.s_iWrecks == wrecksBefore, string.Format("tier %1 -> %2 -> %3, wrecks %4 -> %5", before, cleared, again, wrecksBefore, ACR_NestDressing.s_iWrecks));
 }

 // Fire breaks the truce (Jacob, 12 Nov): a US soldier's hit on a Soviet one ends it at once.
 protected static void TruceHit(ACR_Manager manager)
 {
  IEntity us, ussr;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction()) continue;
   IEntity anchor = squad.GetAnchor();
   if (!anchor || !ACR_Respawn.IsAlive(anchor)) continue;
   string key = squad.GetFaction().GetFactionKey();
   if (!us && key == "US") us = anchor;
   if (!ussr && key == "USSR") ussr = anchor;
  }
  ACR_Truce.Reset();
  bool possible = ACR_Truce.Possible();
  ACR_Truce.Offer("USSR");
  ACR_Truce.Offer("US");
  bool held = ACR_Truce.Holds();
  ACR_Truce.Hit(ussr, us);
  bool broken = !ACR_Truce.Holds() && ACR_Truce.s_sFiredBy == "US";
  Check("zombies: a hit on the truce partner breaks the truce at once", possible && us && ussr && held && broken, string.Format("possible %1, soldiers %2/%3, held %4, broken %5 by '%6'", possible, us != null, ussr != null, held, broken, ACR_Truce.s_sFiredBy));
  ACR_Truce.Reset();
 }

 protected static void Approach(ACR_Manager manager)
 {
  vector dir = Vector(6400, 0, 6400) - s_Nest.m_vPos;
  dir[1] = 0;
  dir.Normalize();
  vector spot = ACR_Manager.Ground(s_Nest.m_vPos + dir * 150);
  manager.m_sSpawningFaction = "US";
  manager.SpawnUnit("Zombie Bait", FIRE_TEAM, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
  PrintFormat("[ACR] ZOMBIES test: bait at %1, 150 m from %2", spot, s_Nest.m_sName);
 }

 protected static void PouredOut(ACR_Manager manager)
 {
  SCR_AIGroup group = s_Nest.m_Group;
  int zombies, ofZ, squads;
  if (group)
  {
   array<AIAgent> agents = {};
   group.GetAgents(agents);
   foreach (AIAgent agent : agents) if (ACR_Outbreak.IsZombie(agent.GetControlledEntity())) ofZ++;
   zombies = agents.Count();
  }
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetFaction() && ACR_Outbreak.IsZombies(squad.GetFaction().GetFactionKey())) squads++;
  Check("zombies: a soldier 150 m away makes the nest pour out", group != null && zombies > 0, string.Format("%1 zombies", zombies));
  Check("zombies: they are Z, a garrison group, never a squad", zombies > 0 && ofZ == zombies && manager.m_Bases.IsGarrison(group) && squads == 0, string.Format("%1 of %2 Z, %3 Z squads", ofZ, zombies, squads));
  s_iOut = zombies;
  s_fStartGap = Gap(manager);
 }

 // Quiet at 150 m they don't notice the team (sight 200 m in a cone with a clear view, hearing
 // 60 m): it walks into the town, 40 m from the site.
 protected static void IntoTown(ACR_Manager manager)
 {
  ACR_Squad bait = Squad(manager, "Zombie Bait");
  if (!bait) return;
  vector dir = Vector(6400, 0, 6400) - s_Nest.m_vPos;
  dir[1] = 0;
  dir.Normalize();
  array<AIAgent> agents = {};
  bait.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man) man.SetOrigin(ACR_Manager.Ground(s_Nest.m_vPos + dir * 40 + Vector(i * 2, 0, 0)));
  }
  s_fStartGap = Gap(manager);
 }

 protected static void Closing(ACR_Manager manager)
 {
  float gap = Gap(manager);
  ACR_Squad bait = Squad(manager, "Zombie Bait");
  int alive;
  if (bait)
  {
   array<AIAgent> agents = {};
   bait.GetAgents(agents);
   foreach (AIAgent agent : agents) if (ACR_Respawn.IsAlive(agent.GetControlledEntity())) alive++;
  }
  int dead;
  if (s_Nest.m_Group) dead = s_iOut - manager.m_Bases.GuardsAlive(s_Nest.m_Group);
  Check("zombies: they close on the team (or have hit it, or died trying)", gap < s_fStartGap - 15 || gap < 15 || alive < 4 || dead >= 2, string.Format("nearest zombie %1 m, was %2 m; %3 of the team alive, %4 zombies dead", (int)gap, (int)s_fStartGap, alive, dead));
  // A contact seen then killed is forgotten: the reports made count.
  Check("zombies: the team's side sees them as zombies", ACR_Outbreak.s_iSightings > 0, string.Format("%1 zombie contacts reported", ACR_Outbreak.s_iSightings));
  bool held;
  foreach (ACR_Base base : ACR_Base.s_All) if (ACR_Outbreak.IsZombies(base.m_sOwner) || ACR_Outbreak.IsZombies(base.m_sCapturer)) held = true;
  Check("zombies: no base held or being taken by Z", !held, "");
 }

 // Home to a US starting base, far from every nest.
 protected static void Away(ACR_Manager manager)
 {
  ACR_Squad squad = Squad(manager, "Zombie Bait");
  if (!squad) return;
  vector home = vector.Zero;
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sOwner == "US" && !base.m_bPocket) home = base.GetOrigin();
  if (home == vector.Zero) return;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man) man.SetOrigin(ACR_Manager.Ground(home + Vector(i * 2, 0, 0)));
  }
 }

 protected static void BackIn()
 {
  Check("zombies: with nobody near, the nest goes back in", !s_Nest.m_Group && (s_Nest.m_iReserve > 0 || s_Nest.m_bGone), string.Format("out %1, reserve %2, cleared %3", s_Nest.m_Group != null, s_Nest.m_iReserve, s_Nest.m_bGone));
 }

 // The nearest living zombie of the nest to the bait's living men (m).
 protected static float Gap(ACR_Manager manager)
 {
  float best = 100000;
  ACR_Squad bait = Squad(manager, "Zombie Bait");
  if (!bait || !s_Nest.m_Group) return best;
  array<AIAgent> men = {};
  bait.GetAgents(men);
  array<AIAgent> zombies = {};
  s_Nest.m_Group.GetAgents(zombies);
  foreach (AIAgent z : zombies)
  {
   IEntity zombie = z.GetControlledEntity();
   if (!zombie || !ACR_Respawn.IsAlive(zombie)) continue;
   foreach (AIAgent m : men)
   {
    IEntity man = m.GetControlledEntity();
    if (man && ACR_Respawn.IsAlive(man)) best = Math.Min(best, vector.DistanceXZ(man.GetOrigin(), zombie.GetOrigin()));
   }
  }
  return best;
 }

 protected static void Check(string name, bool ok, string detail)
 {
  ACR_TestRegress.Check(name, ok, detail);
 }

 protected static void Finish()
 {
  ACR_TestRegress.Complete("zombies", 24);
 }

 protected static ACR_Squad Squad(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == name) return squad;
  return null;
 }
}
