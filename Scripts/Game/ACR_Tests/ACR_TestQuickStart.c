// -acrQuickStartTest 1 (server, Everon whole map, ~40 s): quick start (#88, ACR_QuickStart), even.
// Checks: both sides hold bases beyond their HQ, balanced by what they earn; the split is the
// seed's (what the setup preview shows); an uneven split gives one side about two thirds; front
// garrisons bigger than rear ones; a small neutral strip; FIA pockets keep off the front; foot
// squads start at front bases plus holding squads; roadblocks up at the front for both sides.
class ACR_TestQuickStart
{
 static void Run(int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  if (tick == 10) Split();
  if (tick == 30) Forces(manager);
 }

 protected static void Split()
 {
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  int westGiven, eastGiven;
  foreach (ACR_Base given : ACR_QuickStart.s_Assigned)
  {
   if (given.m_sOwner == west) westGiven++;
   if (given.m_sOwner == east) eastGiven++;
  }
  ACR_TestRegress.Check("quickstart: both sides hold bases beyond their HQ", westGiven >= 2 && eastGiven >= 2, string.Format("%1 %2, %3 %4; %5", west, westGiven, east, eastGiven, ACR_QuickStart.s_sFront));
  float westValue = ACR_QuickStart.Held(west);
  float eastValue = ACR_QuickStart.Held(east);
  ACR_TestRegress.Check("quickstart: balanced by what the bases earn", Math.AbsFloat(westValue - eastValue) <= 7, string.Format("%1 vs %2", westValue, eastValue));

  // The same inputs again (starting bases as the sides, the rest open): the seed's split.
  array<ACR_Base> bases = {};
  array<vector> positions = {};
  array<float> weights = {};
  array<int> sides = {};
  int open = 0;
  int neutral = 0;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_bBuilt) continue;
   int side = -1;
   bool given = ACR_QuickStart.Assigned(base);
   if (!given && base.m_sOwner == west) side = 0;
   else if (!given && base.m_sOwner == east) side = 1;
   else if (base.m_bHidden) side = -2;
   if (side == -1) open++;
   if (side == -1 && !given) neutral++;
   bases.Insert(base);
   positions.Insert(base.GetOrigin());
   weights.Insert(ACR_QuickStart.Weight(base.m_bIncome, base.m_iValue));
   sides.Insert(side);
  }
  array<int> again = {};
  again.Copy(sides);
  ACR_QuickStart.Split(positions, weights, again, ACR_Generator.Seed(), "even", false);
  bool same = true;
  string differ = "";
  foreach (int i, ACR_Base b : bases)
  {
   if (sides[i] != -1) continue;
   string expected = "";
   if (again[i] == 0) expected = west;
   if (again[i] == 1) expected = east;
   // A pocket took a neutral base afterwards.
   string actual = b.m_sOwner;
   if (b.m_bPocket) actual = "";
   if (actual != expected) { same = false; differ += " " + b.m_sName; }
  }
  ACR_TestRegress.Check("quickstart: the seed's split (what the setup preview shows)", same, differ);

  array<int> uneven = {};
  uneven.Copy(sides);
  ACR_QuickStart.Split(positions, weights, uneven, ACR_Generator.Seed(), "uneven", false);
  float westShare = 0;
  float eastShare = 0;
  foreach (int j, int u : uneven)
  {
   if (sides[j] != -1) continue;
   if (u == 0) westShare += weights[j];
   if (u == 1) eastShare += weights[j];
  }
  float bigger = Math.Max(westShare, eastShare) / Math.Max(0.1, westShare + eastShare);
  ACR_TestRegress.Check("quickstart: uneven gives one side about two thirds", bigger >= 0.55 && bigger <= 0.8, string.Format("%1 of the open bases' worth", bigger));

  float neutralCap = 0.2 * open + 1;
  ACR_TestRegress.Check("quickstart: a small neutral strip", neutral <= neutralCap, string.Format("%1 of %2 open", neutral, open));

  bool garrisons = true;
  string detail = "";
  foreach (ACR_Base held : ACR_QuickStart.s_Assigned)
  {
   bool front = ACR_QuickStart.s_Front.Contains(held);
   if (front && held.m_iGarrison < 4) garrisons = false;
   if (!front && held.m_iGarrison > 4) garrisons = false;
   if (!garrisons && detail == "") detail = string.Format("%1 front %2 garrison %3", held.m_sName, front, held.m_iGarrison);
  }
  ACR_TestRegress.Check("quickstart: front garrisons 4+, rear ones 4 or fewer", garrisons && !ACR_QuickStart.s_Front.IsEmpty(), detail);

  bool clear = true;
  foreach (ACR_Base pocket : ACR_Base.s_All)
  {
   if (!pocket.m_bPocket) continue;
   foreach (ACR_Base side2 : ACR_QuickStart.s_Assigned)
    if (vector.DistanceXZ(pocket.GetOrigin(), side2.GetOrigin()) < ACR_QuickStart.POCKET_CLEARANCE) clear = false;
  }
  ACR_TestRegress.Check("quickstart: FIA pockets keep off the front", clear, "");

  int westUp, eastUp, extra;
  foreach (ACR_StartingUnit unit : ACR_StartingUnit.s_All)
  {
   if (unit.m_sName.StartsWith(ACR_QuickStart.EXTRA_NAME)) extra++;
   if (!unit.m_MovedTo || !ACR_QuickStart.s_Front.Contains(unit.m_MovedTo)) continue;
   if (unit.m_MovedTo.m_sOwner == west) westUp++;
   if (unit.m_MovedTo.m_sOwner == east) eastUp++;
  }
  ACR_TestRegress.Check("quickstart: foot squads start at front bases, holding squads added", westUp >= 2 && eastUp >= 2 && extra >= 2, string.Format("%1 %2, %3 %4, %5 holding squads", west, westUp, east, eastUp, extra));
 }

 protected static void Forces(ACR_Manager manager)
 {
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  int westBlocks, eastBlocks;
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (!rb) continue;
   if (rb.m_sFaction == west) westBlocks++;
   if (rb.m_sFaction == east) eastBlocks++;
  }
  ACR_TestRegress.Check("quickstart: roadblocks up at the front for both sides", westBlocks >= 1 && eastBlocks >= 1, string.Format("%1 %2, %3 %4", west, westBlocks, east, eastBlocks));
  int forward = 0;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.GetName().StartsWith(ACR_QuickStart.EXTRA_NAME)) continue;
   IEntity leader = squad.GetLeader();
   if (!leader) continue;
   foreach (ACR_Base front : ACR_QuickStart.s_Front)
    if (vector.DistanceXZ(leader.GetOrigin(), front.GetOrigin()) < 150) { forward++; break; }
  }
  ACR_TestRegress.Check("quickstart: holding squads spawned at the front", forward >= 2, string.Format("%1 at a front base", forward));
  ACR_TestRegress.Complete("quickstart", 10);
 }
}
