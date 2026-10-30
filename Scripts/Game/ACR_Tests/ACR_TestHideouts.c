// srv:hideouts (Everon whole map, about 1.5 min): FIA hideouts (ACR_Guerrillas, #89). Checks:
// hideouts sit in a small isolated group of buildings away from the starting bases, every base
// and the cells; they feed short-handed cells in reach and raise a cell when one is gone; a US
// team beside one brings it out into the windows and it opens fire; the US has found it (feed
// line, map ring, its AI sends a squad to clear it), USSR hasn't; it doesn't pull back; wiped out,
// it is gone with its mark, the US hears of it and loses standing (leaning on).
class ACR_TestHideouts
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 protected static ref ACR_DefenderPost s_Hideout; // kept: the post leaves s_Posts once destroyed
 protected static float s_fStanding;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 10) Placed(manager);
  if (!s_Hideout) return;
  if (second == 11) Supply(manager);
  if (second == 12) Raise(manager);
  if (second == 13) Spawn(manager, "Hideout Bait", s_Hideout.m_vPos, 90);
  if (second == 45) Found(manager);
  if (second == 46) Sweep(manager);
  if (second == 47) Holds(manager);
  // A fresh team, so a side is still there when it falls (the bait may be dead by now).
  if (second == 48) Spawn(manager, "Cleanup Team", s_Hideout.m_vPos, 120);
  if (second == 56) WipeOut(manager);
  if (second == 68) Destroyed(manager);
 }

 protected static void Placed(ACR_Manager manager)
 {
  ACR_Options.Set("acrFIALeaning", "1");
  ACR_Insurgency.Apply();
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  if (hideouts.IsEmpty())
  {
   ACR_TestRegress.Abort("hideouts", "no hideout placed");
   return;
  }
  s_Hideout = hideouts[0];
  int good;
  string detail;
  foreach (ACR_DefenderPost post : hideouts)
  {
   array<IEntity> around = {};
   ACR_Garrison.BuildingsWithin(post.m_vPos, ACR_Guerrillas.HIDEOUT_ISOLATION, around);
   int group;
   foreach (IEntity building : around) if (ACR_Garrison.DistanceToBox(building, post.m_vPos) <= ACR_Guerrillas.HIDEOUT_CLUSTER) group++;
   float fromStart = 100000;
   foreach (vector start : ACR_Guerrillas.s_aStarts) fromStart = Math.Min(fromStart, vector.DistanceXZ(start, post.m_vPos));
   bool inBase;
   foreach (ACR_Base base : ACR_Base.s_All) if (!base.m_bRetired && !base.m_bHidden && vector.DistanceXZ(base.GetOrigin(), post.m_vPos) < base.m_fRadius + ACR_Guerrillas.BASE_CLEAR) inBase = true;
   float fromCell = 100000;
   foreach (ACR_DefenderPost other : ACR_Defenders.s_Posts) if (other != post && other.m_Cell && !other.m_bGone) fromCell = Math.Min(fromCell, vector.DistanceXZ(other.m_Cell.m_vKillZone, post.m_vPos));
   bool ok = group >= ACR_Guerrillas.HIDEOUT_MIN_BUILDINGS && around.Count() <= ACR_Guerrillas.HIDEOUT_MAX_AROUND + 2 && fromStart >= 900 && !inBase && fromCell >= ACR_Guerrillas.SPACING;
   if (ok) good++;
   detail += string.Format("%1: %2 buildings in the group, %3 round it, %4 m from a starting base, base %5, %6 m from a cell; ", post.m_sName, group, around.Count(), (int)fromStart, inBase, (int)fromCell);
  }
  ACR_TestRegress.Check("hideouts: in a small isolated group of buildings, away from bases and cells", good == hideouts.Count(), detail);
 }

 // Every cell short of men: those within reach of a hideout get one back, the rest none.
 protected static void Supply(ACR_Manager manager)
 {
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  array<ACR_DefenderPost> cells = {};
  array<int> before = {};
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || post.m_Cell.m_bHideout || post.m_bGone || post.m_Group) continue;
   post.m_iReserve = post.m_Cell.m_iStart - 1;
   cells.Insert(post);
   before.Insert(post.m_iReserve);
  }
  ACR_Guerrillas.Supply();
  int right, inReach;
  foreach (int i, ACR_DefenderPost cell : cells)
  {
   bool reach;
   foreach (ACR_DefenderPost hideout : hideouts) if (vector.DistanceXZ(hideout.m_vPos, cell.m_Cell.m_vKillZone) <= ACR_Guerrillas.SUPPLY_REACH) reach = true;
   int expected = before[i];
   if (reach) { expected++; inReach++; }
   if (cell.m_iReserve == expected) right++;
  }
  ACR_TestRegress.Check("hideouts: they send a man to each short-handed cell in reach", cells.Count() > 0 && right == cells.Count(), string.Format("%1 of %2 cells right, %3 in reach", right, cells.Count(), inReach));
 }

 protected static void Raise(ACR_Manager manager)
 {
  ACR_DefenderPost lost;
  int cells;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || post.m_Cell.m_bHideout || post.m_bGone) continue;
   cells++;
   if (!lost && !post.m_Group) lost = post;
  }
  if (lost) ACR_Defenders.Remove(lost);
  bool raised = ACR_Guerrillas.Raise();
  int after;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts) if (post.m_Cell && !post.m_Cell.m_bHideout && !post.m_bGone) after++;
  ACR_TestRegress.Check("hideouts: a cell lost, a hideout raises a new one", lost && raised && after == cells, string.Format("%1 cells, %2 after losing one and raising", cells, after));
 }

 protected static void Found(ACR_Manager manager)
 {
  string us = ACR_FactionSelection.West();
  string ussr = ACR_FactionSelection.East();
  ACR_GuerrillaCell cell = s_Hideout.m_Cell;
  ACR_TestRegress.Check("hideouts: a team beside it brings it out and it opens fire", s_Hideout.m_Group != null && cell.m_bSprung, string.Format("deployed %1, sprung %2 (%3)", s_Hideout.m_Group != null, cell.m_bSprung, cell.m_sSprung));
  ACR_Intel intel = manager.IntelFor(us);
  bool told;
  if (intel)
  {
   foreach (string line : intel.m_Feed) if (line.Contains("FIA hideout found")) told = true;
  }
  array<vector> positions = {};
  array<float> radii = {};
  array<string> labels = {};
  ACR_Guerrillas.Zones(us, positions, radii, labels);
  array<vector> theirs = {};
  ACR_Guerrillas.Zones(ussr, theirs, radii, labels);
  ACR_TestRegress.Check("hideouts: the side that fought it has found it (feed line, map ring), the other hasn't", cell.m_aKnownBy.Contains(us) && !cell.m_aKnownBy.Contains(ussr) && told && labels.Contains("FIA hideout") && theirs.IsEmpty(), string.Format("known by %1, told %2, %3 rings", cell.m_aKnownBy.Count(), told, positions.Count()));
 }

 protected static void Sweep(ACR_Manager manager)
 {
  ACR_DangerRoad mark;
  foreach (ACR_DangerRoad road : ACR_Insurgency.s_aRoads) if (road.m_Hideout == s_Hideout) mark = road;
  ACR_AICommander ai = manager.FobAICommanderFor(ACR_FactionSelection.West());
  if (ai) ai.ManageDangerRoadsForTest();
  bool sent = mark && mark.m_Sweeper && mark.m_Sweeper.m_iAITask == ACR_EAITask.SWEEP;
  bool supplyWaits = mark && ACR_Insurgency.OnTheWay(ACR_FactionSelection.West(), s_Hideout.m_vPos + Vector(-300, 0, 0), s_Hideout.m_vPos + Vector(300, 0, 0)) != null;
  ACR_TestRegress.Check("hideouts: its AI sends a squad to clear it (supply runs don't wait)", sent && !supplyWaits, string.Format("mark %1, sweeper %2, supply waits %3", mark != null, sent, supplyWaits));
 }

 // Long past an ambush's hit and run: it still holds.
 protected static void Holds(ACR_Manager manager)
 {
  s_Hideout.m_Cell.m_fSprungAt -= ACR_Guerrillas.HOUSE_HIT_S * 1000;
  ACR_Guerrillas.Tick();
  ACR_TestRegress.Check("hideouts: it holds its ground (no pull-back)", !s_Hideout.m_Cell.m_bWithdrawing && s_Hideout.m_Group != null, "");
 }

 protected static void WipeOut(ACR_Manager manager)
 {
  ACR_FIASide side = ACR_Insurgency.SideOf(ACR_FactionSelection.West());
  if (side) s_fStanding = side.m_fStanding;
  s_Hideout.m_iReserve = 0;
  if (!s_Hideout.m_Group) return;
  array<AIAgent> agents = {};
  s_Hideout.m_Group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man) SCR_EntityHelper.DeleteEntityAndChildren(man);
  }
 }

 protected static void Destroyed(ACR_Manager manager)
 {
  string us = ACR_FactionSelection.West();
  bool marked;
  foreach (ACR_DangerRoad road : ACR_Insurgency.s_aRoads) if (road.m_Hideout == s_Hideout) marked = true;
  ACR_Intel intel = manager.IntelFor(us);
  bool told;
  if (intel)
  {
   foreach (string line : intel.m_Feed) if (line.Contains("hideout") && line.Contains("destroyed")) told = true;
  }
  ACR_FIASide side = ACR_Insurgency.SideOf(us);
  float cost = -1;
  if (side) cost = s_fStanding - side.m_fStanding;
  ACR_TestRegress.Check("hideouts: wiped out, it is gone with its mark, the finder hears and loses standing", s_Hideout.m_bGone && !marked && told && Math.AbsFloat(cost - ACR_Insurgency.HIDEOUT_TAKEN) < 0.01, string.Format("gone %1, marked %2, told %3, standing cost %4", s_Hideout.m_bGone, marked, told, cost));
  ACR_TestRegress.Complete("hideouts", 8);
 }

 protected static void Spawn(ACR_Manager manager, string name, vector at, float distance)
 {
  vector spot = ACR_Manager.Ground(at + Vector(distance, 0, 0));
  manager.m_sSpawningFaction = ACR_FactionSelection.West();
  manager.SpawnUnit(name, FIRE_TEAM, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }
}
