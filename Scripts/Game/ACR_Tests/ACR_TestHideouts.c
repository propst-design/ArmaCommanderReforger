// srv:hideouts (Everon whole map, about 1.5 min): FIA hideouts (ACR_Guerrillas, #89). Checks:
// hideouts sit in a small isolated group of buildings away from the starting bases, every base
// and the cells; they feed short-handed cells in reach and raise a cell when one is gone; a US
// team beside one brings it out into the windows and it opens fire; the US has found it (feed
// line, map ring, its AI sends a squad to clear it), USSR hasn't; it doesn't pull back; wiped out,
// it is gone with its mark, the US hears of it and loses standing (leaning on).
// Growth (#96 part 1, ACR_FIAGrowth): the pool tops up short cells in reach and buys a new cell;
// a held base near a hideout slows or stops its recruits; two fast-forwarded hours keep the caps;
// spread turns only an empty neutral base (never one held lately) into a hidden pocket.
// Sabotage (#96 part 2, ACR_Raid): a raid on an unguarded Soviet FOB plants a charge and the HQ
// goes down; a guarded FOB is never picked and a raid sent at it plants nothing; only built FOBs
// are targets (never a starting HQ).
// Intel (#96, ACR_FIAIntel): a cell wiped out by the US gives the US (not USSR) an "FIA activity"
// ring that holds the hideout feeding it; a second lead tightens it and still holds it.
class ACR_TestHideouts
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 protected static ref ACR_DefenderPost s_Hideout; // kept: the post leaves s_Posts once destroyed
 protected static float s_fStanding;
 protected static ref array<string> s_aOwnedAtStart; // bases with an owner when it began (spread must avoid them)
 static const ResourceName USSR_TEAM = "{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et";
 protected static ACR_Base s_Fob;
 protected static ACR_Base s_Fob2;
 protected static ref ACR_Raid s_Raid;
 protected static ref ACR_Raid s_Raid2;

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
  if (second == 70) Slowed(manager);
  if (second == 71) FastForward(manager);
  if (second == 72) SpreadRules(manager);
  if (second == 73) IntelRings(manager);
  if (second == 74) RaidStart(manager);
  if (!s_Raid) return;
  if (second == 100) GuardedFob(manager);
  if (second == 125) RaidBlown(manager);
  if (second == 126) GuardedRaid(manager);
  if (second == 152) GuardedHolds(manager);
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
  s_aOwnedAtStart = {};
  foreach (ACR_Base held : ACR_Base.s_All) if (!held.m_sOwner.IsEmpty()) s_aOwnedAtStart.Insert(held.m_sName);
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
  foreach (ACR_DefenderPost home : hideouts) home.m_Cell.m_fPool = ACR_FIAGrowth.PoolCap();
  foreach (ACR_DefenderPost giver : hideouts) ACR_FIAGrowth.TopUp(giver);
  int right, inReach;
  foreach (int i, ACR_DefenderPost cell : cells)
  {
   bool reach;
   foreach (ACR_DefenderPost hideout : hideouts) if (vector.DistanceXZ(hideout.m_vPos, cell.m_Cell.m_vKillZone) <= ACR_Guerrillas.SUPPLY_REACH) reach = true;
   int expected = before[i];
   if (reach) { expected++; inReach++; }
   if (cell.m_iReserve == expected) right++;
  }
  ACR_TestRegress.Check("hideouts: their recruits top up each short-handed cell in reach", cells.Count() > 0 && right == cells.Count(), string.Format("%1 of %2 cells right, %3 in reach", right, cells.Count(), inReach));
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
  s_Hideout.m_Cell.m_fPool = ACR_FIAGrowth.CELL_COST;
  bool raised = ACR_FIAGrowth.RaiseCell(s_Hideout);
  float left = s_Hideout.m_Cell.m_fPool;
  int after;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts) if (post.m_Cell && !post.m_Cell.m_bHideout && !post.m_bGone) after++;
  ACR_TestRegress.Check("hideouts: a cell lost, a hideout buys a new one with its recruits", lost && raised && after == cells && left < 0.01, string.Format("%1 cells, %2 after losing one and raising, pool left %3", cells, after, left));
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
 }

 // A hideout still standing (null: none).
 protected static ACR_DefenderPost Standing()
 {
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  if (hideouts.IsEmpty()) return null;
  return hideouts[0];
 }

 // The base nearest a hideout held by the US for a moment: its recruits slow (inside 1.5 km) or stop
 // (inside 800 m) as the distance says.
 protected static void Slowed(ACR_Manager manager)
 {
  ACR_DefenderPost hideout = Standing();
  if (!hideout)
  {
   ACR_TestRegress.Skip("hideouts: a held base slows or stops recruiting", "no hideout left");
   return;
  }
  float before = ACR_FIAGrowth.FillRate(hideout);
  ACR_Base nearest;
  float d = 100000;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || !base.m_sOwner.IsEmpty() || base.m_bPocket) continue;
   float gap = vector.DistanceXZ(base.GetOrigin(), hideout.m_vPos);
   if (gap >= d) continue;
   d = gap;
   nearest = base;
  }
  if (!nearest)
  {
   ACR_TestRegress.Skip("hideouts: a held base slows or stops recruiting", "no open base");
   return;
  }
  nearest.m_sOwner = ACR_FactionSelection.West();
  float during = ACR_FIAGrowth.FillRate(hideout);
  nearest.m_sOwner = "";
  float expected = before;
  if (d < ACR_FIAGrowth.SLOW_REACH) expected = Math.Min(before, ACR_FIAGrowth.FILL_PER_MIN * ACR_FIAGrowth.SLOW_FACTOR);
  if (d < ACR_FIAGrowth.STOP_REACH) expected = 0;
  bool ok = Math.AbsFloat(during - expected) < 0.001 && (d >= ACR_FIAGrowth.SLOW_REACH || before > during || before == 0);
  ACR_TestRegress.Check("hideouts: a held base slows or stops recruiting", ok, string.Format("%1 %2 m off: %3/min before, %4 while held, %5 expected", nearest.m_sName, (int)d, before, during, expected));
 }

 // Two hours of growth at once: pools, cells and spreads within their caps; new pockets only in
 // bases nobody held at the start.
 protected static void FastForward(ACR_Manager manager)
 {
  for (int m = 0; m < 120; m++) ACR_FIAGrowth.Grow(1);
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  float most;
  foreach (ACR_DefenderPost hideout : hideouts) most = Math.Max(most, hideout.m_Cell.m_fPool);
  int cells = ACR_FIAGrowth.Cells();
  int spreads = ACR_FIAGrowth.Spreads();
  ACR_TestRegress.Check("hideouts: two fast-forwarded hours keep the caps (pool, cells, spreads)", most <= ACR_FIAGrowth.PoolCap() && cells <= ACR_FIAGrowth.CellCap() && spreads <= ACR_FIAGrowth.SpreadCap(), string.Format("%1 hideouts, pool %2 of %3, %4 of %5 cells, %6 of %7 spreads", hideouts.Count(), most, ACR_FIAGrowth.PoolCap(), cells, ACR_FIAGrowth.CellCap(), spreads, ACR_FIAGrowth.SpreadCap()));
 }

 // Spread: every pocket it made was an open base at the start, hidden from both sides; a base held
 // a moment ago is never picked.
 protected static void SpreadRules(ACR_Manager manager)
 {
  string us = ACR_FactionSelection.West();
  string ussr = ACR_FactionSelection.East();
  int made, wrong;
  string names;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.m_bPocket) continue;
   if (!s_aOwnedAtStart.Contains(base.m_sName))
   {
    made++;
    names += " " + base.m_sName;
    if (base.m_bBuilt || base.OwnerSeenBy(us) != "" || base.OwnerSeenBy(ussr) != "") wrong++;
   }
  }
  // A held-lately base: an open base made US for a tick, then open again, is skipped.
  ACR_DefenderPost hideout = Standing();
  string skipped = "no hideout left";
  bool heldSkipped = true;
  if (hideout)
  {
   string why;
   ACR_Base pick = ACR_FIAGrowth.SpreadSite(hideout, why);
   skipped = "no site in reach (" + why + ")";
   if (pick)
   {
    pick.m_sOwner = us;
    ACR_FIAGrowth.Grow(0);
    pick.m_sOwner = "";
    ACR_Base again = ACR_FIAGrowth.SpreadSite(hideout, why);
    heldSkipped = again != pick;
    skipped = string.Format("%1 held a moment ago: picked again %2 (%3)", pick.m_sName, !heldSkipped, why);
   }
  }
  ACR_TestRegress.Check("hideouts: spread only into empty neutral bases, hidden, never one held lately", wrong == 0 && heldSkipped, string.Format("%1 pockets made:%2, %3 wrong; %4", made, names, wrong, skipped));
 }

 // A cell wiped out where the US Cleanup Team stands: the US gets a ring holding the hideout that
 // feeds it, USSR none; a second lead tightens it.
 protected static void IntelRings(ACR_Manager manager)
 {
  ACR_DefenderPost hideout = Standing();
  ACR_Squad team;
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName().Contains("Cleanup Team") && squad.GetAnchor()) team = squad;
  if (!hideout || !team)
  {
   ACR_TestRegress.Skip("hideouts: a wiped cell gives its killers a ring holding its hideout", "no hideout or no US team");
   return;
  }
  string us = ACR_FactionSelection.West();
  string ussr = ACR_FactionSelection.East();
  ACR_FIAIntel.Wiped(team.GetAnchor().GetOrigin(), hideout, "a wiped-out cell");
  ACR_FIALead lead = ACR_FIAIntel.LeadOf(us, hideout);
  float first = -1;
  bool holds;
  if (lead)
  {
   first = lead.m_fRadius;
   holds = vector.DistanceXZ(lead.m_vCentre, hideout.m_vPos) < lead.m_fRadius;
  }
  ACR_FIAIntel.Lead(us, hideout, "a second wiped-out cell");
  bool tighter = lead && lead.m_fRadius < first && vector.DistanceXZ(lead.m_vCentre, hideout.m_vPos) < lead.m_fRadius;
  float second = -1;
  if (lead) second = lead.m_fRadius;
  array<vector> positions = {};
  array<float> radii = {};
  array<string> labels = {};
  ACR_Guerrillas.Zones(us, positions, radii, labels);
  array<vector> theirs = {};
  array<string> theirLabels = {};
  ACR_Guerrillas.Zones(ussr, theirs, radii, theirLabels);
  ACR_TestRegress.Check("hideouts: a wiped cell gives its killers a ring holding its hideout, a second lead tightens it", lead && holds && tighter && labels.Contains("FIA activity") && !theirLabels.Contains("FIA activity"), string.Format("lead %1, first %2 m holds %3, then %4 m holds %5, US rings %6, USSR activity %7", lead != null, first, holds, second, tighter, labels.Count(), theirLabels.Contains("FIA activity")));
 }

 // A Soviet FOB (its HQ built by script, as fobdesign does) 'dist' m from 'from', clear of bases.
 protected static ACR_Base MakeFob(ACR_Manager manager, vector from, float dist, ACR_Base not)
 {
  for (int i = 0; i < 24; i++)
  {
   float angle = i * 15 * Math.DEG2RAD;
   vector spot = ACR_Manager.Ground(from + Vector(Math.Cos(angle) * dist, 0, Math.Sin(angle) * dist));
   if (!ACR_Manager.OnMap(spot) || BBB_Terrain.IsWaterAt(spot) || ACR_TestStructures.InsideABase(spot, 150)) continue;
   if (not && vector.DistanceXZ(spot, not.GetOrigin()) < 400) continue;
   ACR_TestStructures.Place(ACR_FobBuild.HQ_USSR, spot);
   manager.m_Structures.ACR_ForceTick();
   foreach (ACR_Base base : ACR_Base.s_All) if (base && base.m_bBuilt && !base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), spot) < 5) return base;
   return null;
  }
  return null;
 }

 // An unguarded Soviet FOB 600 m from a hideout: a raid at it (there at once, appearing close in).
 protected static void RaidStart(ACR_Manager manager)
 {
  ACR_DefenderPost hideout = Standing();
  if (!hideout)
  {
   ACR_TestRegress.Abort("hideouts", "no hideout left for the raid");
   return;
  }
  s_Fob = MakeFob(manager, hideout.m_vPos, 600, null);
  ACR_Base fob;
  ACR_Roadblock roadblock;
  bool picked = s_Fob && ACR_Raid.PickTarget(ACR_Pockets.FACTION, hideout.m_vPos, fob, roadblock) && fob == s_Fob;
  bool onlyBuilt = !fob || fob.m_bBuilt;
  ACR_TestRegress.Check("hideouts: a raid picks the unguarded FOB (built FOBs only, never a starting HQ)", picked && onlyBuilt, string.Format("fob %1, picked %2", s_Fob != null, fob));
  if (!s_Fob) return;
  ACR_Raid.s_fStageForTest = 30;
  ACR_Raid.Start(ACR_Pockets.FACTION, s_Fob, null, hideout);
  s_Raid = ACR_Raid.Running();
  if (s_Raid) s_Raid.m_fArriveAt = 0;
 }

 // A second FOB with a Soviet team at its HQ: never picked.
 protected static void GuardedFob(ACR_Manager manager)
 {
  ACR_DefenderPost hideout = Standing();
  if (!hideout) return;
  s_Fob2 = MakeFob(manager, hideout.m_vPos, 900, s_Fob);
  if (!s_Fob2) return;
  IEntity hq = manager.m_Structures.HqEntityOf(s_Fob2);
  vector at = s_Fob2.GetOrigin();
  if (hq) at = hq.GetOrigin();
  manager.m_sSpawningFaction = ACR_FactionSelection.East();
  manager.SpawnUnit("FOB Guard", USSR_TEAM, "", false, "", at + Vector(6, 0, 0), at + Vector(6, 0, 0));
  manager.m_sSpawningFaction = "";
 }

 protected static void RaidBlown(ACR_Manager manager)
 {
  ACR_TestRegress.Check("hideouts: the raid on the unguarded FOB plants and the HQ goes down", s_Fob.m_bHqLost && manager.m_Structures.HqEntityOf(s_Fob) == null, string.Format("state %1, HQ lost %2", s_Raid.m_eState, s_Fob.m_bHqLost));
 }

 // The guarded FOB: not picked; a raid sent at it anyway plants nothing while the guard is there.
 protected static void GuardedRaid(ACR_Manager manager)
 {
  ACR_DefenderPost hideout = Standing();
  if (!s_Fob2 || !hideout)
  {
   ACR_TestRegress.Skip("hideouts: a guarded FOB is never picked", "no second FOB");
   ACR_TestRegress.Skip("hideouts: a raid at a guarded FOB plants nothing", "no second FOB");
   ACR_TestRegress.Complete("hideouts", 16);
   return;
  }
  ACR_Base fob;
  ACR_Roadblock roadblock;
  ACR_Raid.PickTarget(ACR_Pockets.FACTION, hideout.m_vPos, fob, roadblock);
  ACR_TestRegress.Check("hideouts: a guarded FOB is never picked", fob != s_Fob2 && fob != s_Fob, string.Format("picked %1", fob));
  ACR_Raid.Start(ACR_Pockets.FACTION, s_Fob2, null, hideout);
  s_Raid2 = ACR_Raid.s_aRaids[ACR_Raid.s_aRaids.Count() - 1];
  s_Raid2.m_fArriveAt = 0;
 }

 protected static void GuardedHolds(ACR_Manager manager)
 {
  ACR_Raid.s_fStageForTest = 0;
  if (!s_Raid2) return;
  ACR_TestRegress.Check("hideouts: a raid at a guarded FOB plants nothing", !s_Fob2.m_bHqLost && s_Raid2.m_eState != ACR_ERaidState.PLANTED && !s_Raid2.m_Charge, string.Format("state %1, HQ lost %2", s_Raid2.m_eState, s_Fob2.m_bHqLost));
  ACR_TestRegress.Complete("hideouts", 16);
 }

 protected static void Spawn(ACR_Manager manager, string name, vector at, float distance)
 {
  vector spot = ACR_Manager.Ground(at + Vector(distance, 0, 0));
  manager.m_sSpawningFaction = ACR_FactionSelection.West();
  manager.SpawnUnit(name, FIRE_TEAM, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }
}
