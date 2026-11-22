// srv:coingrowth (Everon whole map, about 45 s): the insurgency tied to town support (#244 item 3).
// A hideout among Hostile towns recruits faster than one among Loyal towns, which gets nobody;
// grievance is folded into support (no second counter; the army reads what harm cost); the province
// line reads Pacified with every town Loyal and Uprising with every town Hostile, with a feed line and
// a higher spread cap; heavy strength raises the caps; with every hideout gone, one is raised after
// the quiet spell (shortened), small, near the least secure town.
class ACR_TestCoinGrowth
{
 protected static ref ACR_DefenderPost s_Hostile;
 protected static ref ACR_DefenderPost s_Loyal;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 10) Recruits(manager);
  if (!s_Hostile) return;
  if (second == 12) Folded(manager);
  if (second == 14) Province(manager);
  if (second == 15) Strength(manager);
  if (second == 16) WipeOut(manager);
  if (second == 33) Floor(manager);
 }

 protected static void Recruits(ACR_Manager manager)
 {
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  if (!ACR_TownSupport.On() || hideouts.Count() < 2)
  {
   ACR_TestRegress.Abort("coingrowth", string.Format("support %1, %2 hideouts", ACR_TownSupport.On(), hideouts.Count()));
   return;
  }
  // The two hideouts furthest apart; each town takes the tier of the one it is nearer.
  float far = -1;
  foreach (ACR_DefenderPost a : hideouts)
   foreach (ACR_DefenderPost b : hideouts)
   {
    float d = vector.DistanceXZ(a.m_vPos, b.m_vPos);
    if (d > far) { far = d; s_Hostile = a; s_Loyal = b; }
   }
  foreach (ACR_Base town : ACR_Base.s_All)
  {
   if (!ACR_TownSupport.IsTown(town)) continue;
   float support = 10;
   if (vector.DistanceXZ(town.GetOrigin(), s_Hostile.m_vPos) < vector.DistanceXZ(town.GetOrigin(), s_Loyal.m_vPos)) support = 90;
   Set(town, support);
  }
  float hostile = ACR_FIAGrowth.FillRate(s_Hostile);
  float loyal = ACR_FIAGrowth.FillRate(s_Loyal);
  Check("coingrowth: a hideout among Hostile towns recruits faster than one among Loyal towns", hostile > loyal && hostile >= ACR_FIAGrowth.FILL_PER_MIN * 0.5, string.Format("%1 %2/min vs %3 %4/min (%5 m apart)", s_Hostile.m_sName, hostile, s_Loyal.m_sName, loyal, (int)far));
  // Every town Loyal: nobody joins.
  foreach (ACR_Base all : ACR_Base.s_All) if (ACR_TownSupport.IsTown(all)) Set(all, 10);
  float none = ACR_FIAGrowth.FillRate(s_Hostile);
  Check("coingrowth: among Loyal towns a hideout recruits nobody", none == 0, string.Format("%1/min", none));
 }

 protected static void Folded(ACR_Manager manager)
 {
  ACR_Base town;
  foreach (ACR_Base base : ACR_Base.s_All) if (ACR_TownSupport.IsTown(base) && !base.m_bPocket && base.m_sOwner.IsEmpty()) { town = base; break; }
  float grievance = s_Hostile.m_Cell.m_fGrievance + s_Loyal.m_Cell.m_fGrievance;
  float before = town.m_fSupport;
  ACR_FIAGrievance.OnBuildingDown(town.GetOrigin(), ACR_Coin.Army());
  float after = s_Hostile.m_Cell.m_fGrievance + s_Loyal.m_Cell.m_fGrievance;
  bool line = HasLine(manager.IntelFor(ACR_Coin.Army()), "turns people against us");
  Check("coingrowth: grievance is folded into support: harm moves the town, not a second counter; the feed says what it cost", !ACR_FIAGrievance.On() && after == grievance && town.m_fSupport > before && line, string.Format("grievance on %1 (%2 -> %3), %4 support %5 -> %6, line %7", ACR_FIAGrievance.On(), grievance, after, town.m_sName, before, town.m_fSupport, line));
 }

 protected static void Province(ACR_Manager manager)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  foreach (ACR_DefenderPost hideout : hideouts) hideout.m_Cell.m_aKnownBy.Clear();
  foreach (ACR_Base loyal : ACR_Base.s_All) if (ACR_TownSupport.IsTown(loyal)) Set(loyal, 5);
  ACR_Province.Tick(now);
  int pacified = ACR_Province.Level();
  int capBefore = ACR_FIAGrowth.SpreadCap();
  foreach (ACR_Base hostile : ACR_Base.s_All) if (ACR_TownSupport.IsTown(hostile)) Set(hostile, 95);
  ACR_Province.Tick(now);
  string army = ACR_Coin.Army();
  bool line = HasLine(manager.IntelFor(army), "Province: Uprising");
  string tag = ACR_Province.Tag(army);
  Check("coingrowth: the province line follows the towns: Pacified, then Uprising with a feed line", pacified == ACR_Province.PACIFIED && ACR_Province.Level() == ACR_Province.UPRISING && tag.Contains("Province: Uprising") && line && ACR_Province.Tag(ACR_Pockets.FACTION).IsEmpty(), string.Format("%1 then %2 (share %3), tag '%4', line %5", ACR_Province.LevelName(pacified), ACR_Province.LevelName(ACR_Province.Level()), ACR_Province.Share(), tag, line));
  Check("coingrowth: an Uprising lets FIA spread further", ACR_FIAGrowth.SpreadCap() == capBefore + ACR_Province.UPRISING_SPREADS, string.Format("spread cap %1 -> %2", capBefore, ACR_FIAGrowth.SpreadCap()));
 }

 protected static void Strength(ACR_Manager manager)
 {
  int cells = ACR_FIAGrowth.CellCap();
  int pool = ACR_FIAGrowth.PoolCap();
  ACR_Options.Set("acrCoinStrength", "heavy");
  int heavyCells = ACR_FIAGrowth.CellCap();
  int heavyPool = ACR_FIAGrowth.PoolCap();
  ACR_Options.Set("acrCoinStrength", "few");
  string fewLevel = ACR_Coin.FIALevel("");
  ACR_Options.Unset("acrCoinStrength");
  Check("coingrowth: heavy strength raises the caps, few makes the FIA rows default to few", heavyCells > cells && heavyPool > pool && fewLevel == "few" && ACR_Coin.FIALevel("") == "many", string.Format("cells %1 -> %2, pool %3 -> %4, few -> '%5'", cells, heavyCells, pool, heavyPool, fewLevel));
 }

 // Every hideout gone; the quiet spell shortened to about a second.
 protected static void WipeOut(ACR_Manager manager)
 {
  // Towns back to the middle, one clearly the least secure.
  foreach (ACR_Base town : ACR_Base.s_All) if (ACR_TownSupport.IsTown(town)) Set(town, 40);
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  foreach (ACR_DefenderPost hideout : hideouts)
  {
   if (hideout.m_Group) SCR_EntityHelper.DeleteEntityAndChildren(hideout.m_Group);
   ACR_Defenders.Remove(hideout);
  }
  ACR_Province.s_fFloorScaleForTest = 1000;
  PrintFormat("[ACR] COINGROWTH test: %1 hideouts removed", hideouts.Count());
 }

 protected static void Floor(ACR_Manager manager)
 {
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  ACR_Base least = ACR_Province.s_FloorTown;
  string text = "none";
  float d = -1;
  if (!hideouts.IsEmpty())
  {
   text = string.Format("%1 (%2 men)", hideouts[0].m_sName, hideouts[0].m_iReserve);
   if (least) d = vector.DistanceXZ(hideouts[0].m_vPos, least.GetOrigin());
  }
  Check("coingrowth: with no hideout left, a small one is raised after the quiet spell", hideouts.Count() == 1 && ACR_Province.s_iRaised == 1 && hideouts[0].m_iReserve <= ACR_Province.FLOOR_MEN && d >= 0 && d <= 2100, string.Format("%1, raised %2, %3 m from %4", text, ACR_Province.s_iRaised, (int)d, ACR_Province.s_FloorTown));
  ACR_TestRegress.Complete("coingrowth", 7);
 }

 protected static void Set(ACR_Base town, float support)
 {
  town.m_fSupport = support;
  town.m_iSupportTier = ACR_TownSupport.TierOf(support, -1);
 }

 protected static bool HasLine(ACR_Intel intel, string text)
 {
  if (!intel) return false;
  foreach (string line : intel.m_Feed) if (line.Contains(text)) return true;
  return false;
 }

 protected static void Check(string name, bool ok, string detail)
 {
  ACR_TestRegress.Check(name, ok, detail);
 }
}
