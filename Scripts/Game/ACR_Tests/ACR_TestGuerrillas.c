// srv:guerrillas (Everon whole map, about 2 min): FIA guerrilla cells (ACR_Guerrillas, #89).
// Checks: a few cells (road ambushes and houses) away from both starting bases and outside every
// base; a US fire team 400 m up the road deploys an ambush lying in wait off the road, holding its
// fire; the team in the kill zone springs it (fire at will); it pulls back after its fight; with
// nobody near it packs up, its barricade (if any) goes, and it moves on to another road later; a
// house cell deploys round its building holding its fire; traffic on a far road draws a waiting
// ambush there.
class ACR_TestGuerrillas
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 protected static ACR_DefenderPost s_Ambush;
 protected static ACR_DefenderPost s_House;
 protected static vector s_vOldSite;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 10) Placement(manager);
  if (!s_Ambush) return;
  if (second == 12) Approach(manager, "Guerrilla Bait", s_Ambush.m_Cell.m_vKillZone, 400);
  // The bait holds its fire: the ambush must spring on the kill zone, not on a firefight.
  if (second == 16) HoldFire(manager, "Guerrilla Bait");
  if (second == 32) Deployed(manager);
  if (second == 34) IntoKillZone(manager);
  if (second == 38) Sprung();
  if (second == 40) s_Ambush.m_Cell.m_fSprungAt -= ACR_Guerrillas.HIT_S * 1000;
  if (second == 42) PulledBack();
  if (second == 43) Away(manager, "Guerrilla Bait");
  if (second == 57) MovedOn();
  if (second == 58) FollowsTraffic();
  if (!s_House) return;
  if (second == 58) Approach(manager, "House Bait", s_House.m_Cell.m_vKillZone, 300);
  if (second == 62) HoldFire(manager, "House Bait");
  if (second == 77) HouseDeployed();
 }

 protected static void Placement(ACR_Manager manager)
 {
  array<ACR_Base> starts = {};
  foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_sOwner.IsEmpty() && !base.m_bPocket && !base.m_bRetired) starts.Insert(base);
  int roads, houses;
  bool clear = true;
  bool outside = true;
  string detail = "";
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell) continue;
   if (post.m_Cell.m_bHouse) { houses++; if (!s_House) s_House = post; }
   else { roads++; if (!s_Ambush) s_Ambush = post; }
   vector site = post.m_Cell.m_vKillZone;
   float keep = 1000;
   if (!post.m_Cell.m_bHouse) keep = ACR_Guerrillas.AMBUSH_HQ_CLEAR;
   foreach (ACR_Base start : starts) if (vector.DistanceXZ(site, start.GetOrigin()) < keep) clear = false;
   foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_bRetired && vector.DistanceXZ(site, base.GetOrigin()) < base.m_fRadius) outside = false;
   detail += string.Format(" %1(%2)", post.m_sName, post.m_iReserve);
  }
  Check("guerrillas: a few road ambushes and houses on the whole map", roads >= 2 && roads <= 4 && houses >= 1 && houses <= 3, string.Format("%1 roads, %2 houses:%3", roads, houses, detail));
  Check("guerrillas: away from both starting bases (ambushes 900 m, houses 1 km)", clear, "");
  Check("guerrillas: outside every base", outside, "");
  if (!s_Ambush) Finish();
  else
  {
   s_vOldSite = s_Ambush.m_Cell.m_vKillZone;
   Check("guerrillas: an ambush watches a road and hides off it", ACR_Minefields.RoadClearance(s_Ambush.m_Cell.m_vKillZone) < 3 && ACR_Minefields.RoadClearance(s_Ambush.m_vPos) >= 15, string.Format("kill zone %1 m from a road, hiding place %2 m", (int)ACR_Minefields.RoadClearance(s_Ambush.m_Cell.m_vKillZone), (int)ACR_Minefields.RoadClearance(s_Ambush.m_vPos)));
  }
 }

 // A US fire team 'distance' m from 'at', towards the map centre.
 protected static void Approach(ACR_Manager manager, string name, vector at, float distance)
 {
  vector dir = Vector(6400, 0, 6400) - at;
  dir[1] = 0;
  dir.Normalize();
  vector spot = at + dir * distance;
  vector road, roadDir;
  float width;
  if (ACR_Minefields.Snap(spot, 80, road, roadDir, width)) spot = road;
  spot = ACR_Manager.Ground(spot);
  manager.m_sSpawningFaction = "US";
  manager.SpawnUnit(name, FIRE_TEAM, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
  PrintFormat("[ACR] GUERRILLAS test: %1 at %2, %3 m from %4", name, spot, (int)vector.DistanceXZ(spot, at), at);
 }

 protected static void Deployed(ACR_Manager manager)
 {
  SCR_AIGroup group = s_Ambush.m_Group;
  int men, offRoad;
  if (group)
  {
   array<AIAgent> agents = {};
   group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity man = agent.GetControlledEntity();
    if (!man) continue;
    men++;
    if (ACR_Minefields.RoadClearance(man.GetOrigin()) >= 8) offRoad++;
   }
  }
  Check("guerrillas: the ambush deploys when enemies come up the road", group != null && men > 0, string.Format("%1 men", men));
  Check("guerrillas: they lie in wait off the road, holding their fire", men > 0 && offRoad == men && CombatMode(group) == EAIGroupCombatMode.HOLD_FIRE && !s_Ambush.m_Cell.m_bSprung, string.Format("%1 of %2 off the road, combat mode %3", offRoad, men, typename.EnumToString(EAIGroupCombatMode, CombatMode(group))));
  // An FIA barricade blocks the road: no lane cut through it for anyone (#85's lane is for a side's own roadblocks).
  ACR_Roadblock block = s_Ambush.m_Cell.m_Block;
  if (block) Check("guerrillas: its barricade has no lane through it", block.m_iLaneMoved == 0, string.Format("%1 pieces moved", block.m_iLaneMoved));
  else ACR_TestRegress.Skip("guerrillas: its barricade has no lane through it", "this ambush didn't block the road (a 40 % roll)");
 }

 protected static void IntoKillZone(ACR_Manager manager)
 {
  ACR_Squad bait = Squad(manager, "Guerrilla Bait");
  if (!bait) return;
  array<AIAgent> agents = {};
  bait.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man) man.SetOrigin(ACR_Manager.Ground(s_Ambush.m_Cell.m_vKillZone + Vector(i * 2, 0, 0)));
  }
 }

 protected static void Sprung()
 {
  ACR_GuerrillaCell cell = s_Ambush.m_Cell;
  Check("guerrillas: an enemy in the kill zone springs it: fire at will", cell.m_bSprung && CombatMode(s_Ambush.m_Group) == EAIGroupCombatMode.FIRE_AT_WILL, string.Format("sprung %1 (%2), combat mode %3", cell.m_bSprung, cell.m_sSprung, typename.EnumToString(EAIGroupCombatMode, CombatMode(s_Ambush.m_Group))));
 }

 protected static void PulledBack()
 {
  AIWaypoint waypoint;
  if (s_Ambush.m_Group) waypoint = s_Ambush.m_Group.GetCurrentWaypoint();
  bool away = waypoint && vector.DistanceXZ(waypoint.GetOrigin(), s_Ambush.m_Cell.m_vKillZone) > 200;
  Check("guerrillas: after its fight it pulls back from the road", s_Ambush.m_Cell.m_bWithdrawing && away, string.Format("withdrawing %1, waypoint %2", s_Ambush.m_Cell.m_bWithdrawing, waypoint != null));
 }

 // Home to a US starting base: every cell keeps 1 km or more from those, so nobody is near one.
 protected static void Away(ACR_Manager manager, string name)
 {
  ACR_Squad squad = Squad(manager, name);
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

 protected static void HoldFire(ACR_Manager manager, string name)
 {
  ACR_Squad squad = Squad(manager, name);
  if (squad && squad.m_Group && squad.m_Group.GetGroupUtilityComponent()) squad.m_Group.GetGroupUtilityComponent().SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
 }

 protected static void MovedOn()
 {
  ACR_GuerrillaCell cell = s_Ambush.m_Cell;
  bool later = cell.m_fReadyAt > GetGame().GetWorld().GetWorldTime() + 60000;
  bool moved = vector.DistanceXZ(cell.m_vKillZone, s_vOldSite) > 1;
  bool gone = s_Ambush.m_iReserve > 0 || s_Ambush.m_bGone;
  Check("guerrillas: with nobody near it packs up, its barricade goes", !s_Ambush.m_Group && !cell.m_Block && gone, string.Format("group %1, barricade %2, reserve %3, gone %4", s_Ambush.m_Group != null, cell.m_Block != null, s_Ambush.m_iReserve, s_Ambush.m_bGone));
  Check("guerrillas: survivors move on to another road later", s_Ambush.m_bGone || (cell.m_iMoves == 1 && moved && later), string.Format("moves %1, site %2 -> %3, ready in %4 s", cell.m_iMoves, s_vOldSite, cell.m_vKillZone, (int)((cell.m_fReadyAt - GetGame().GetWorld().GetWorldTime()) / 1000)));
  if (!s_House) Finish();
 }

 // Traffic on a stretch far from every cell: one waiting ambush moves there.
 protected static void FollowsTraffic()
 {
  array<vector> busy = {};
  foreach (vector road : ACR_Guerrillas.s_aRoads)
  {
   if (busy.Count() >= 5) break;
   // Stretches an ambush could take (cc89rs5guer: two picked by distance alone had no hiding place).
   bool far = ACR_Guerrillas.AmbushSiteOk(road, null);
   foreach (vector other : busy) if (vector.DistanceXZ(other, road) < 800) far = false;
   if (far) busy.Insert(road);
  }
  foreach (int n, vector stretch : busy)
   for (int k = 0; k < 6 + n; k++) ACR_Guerrillas.NoteTraffic(stretch);
  bool moved = ACR_Guerrillas.Resite();
  bool there;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
   if (post.m_Cell && !post.m_Cell.m_bHouse)
    foreach (vector point : busy) if (vector.DistanceXZ(post.m_Cell.m_vKillZone, point) < ACR_Guerrillas.TRAFFIC_REACH) there = true;
  Check("guerrillas: a waiting ambush moves to the roads the sides drive", moved && there, string.Format("%1 busy stretches, moved %2, a cell there %3", busy.Count(), moved, there));
 }

 protected static void HouseDeployed()
 {
  SCR_AIGroup group = s_House.m_Group;
  int men;
  if (group) men = group.GetAgentsCount();
  Check("guerrillas: a house cell deploys round its building, holding its fire", group != null && men > 0 && CombatMode(group) == EAIGroupCombatMode.HOLD_FIRE, string.Format("%1 men, combat mode %2", men, typename.EnumToString(EAIGroupCombatMode, CombatMode(group))));
  Finish();
 }

 protected static void Check(string name, bool ok, string detail)
 {
  ACR_TestRegress.Check(name, ok, detail);
 }

 protected static void Finish()
 {
  ACR_TestRegress.Complete("guerrillas", 13);
 }

 protected static EAIGroupCombatMode CombatMode(SCR_AIGroup group)
 {
  if (!group) return -1;
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (!utility) return -1;
  return utility.GetCombatModeExternal();
 }

 protected static ACR_Squad Squad(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == name) return squad;
  return null;
 }
}
