// srv:coinai (Everon whole map, about 80 s): the AI army's COIN jobs (ACR_AICommanderSupport, #244 item 5).
// With the army's AI commander on (the US, as on every srv: run) and three extra rifle squads: it posts a
// squad to the worst town it knows; it sends a squad to search its FIA intel ring; a supply truck with
// a paid load hands out aid in a held Quiet town; it keeps its reserve; it holds fire on a Quiet town
// but not in open country; the postures default by side and the host option overrides them.
class ACR_TestCoinAI
{
 static const ResourceName RIFLES = "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
 static const ResourceName TRUCK = "{2D74C39A650A3030}Prefabs/Vehicles/Wheeled/M923A1/M923A1_engineer.et";
 protected static ACR_Base s_Worst;
 protected static ACR_Base s_Home;
 protected static ref ACR_FIALead s_Lead;
 protected static bool s_bLoaded;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (second == 6) Start(manager);
  if (!s_Home) return;
  if (second >= 12 && second <= 20 && !s_bLoaded) LoadTruck(manager);
  if (second == 95) Verify(manager);
 }

 protected static void Start(ACR_Manager manager)
 {
  string army = ACR_Coin.Army();
  if (!ACR_TownSupport.On())
  {
   ACR_TestRegress.Abort("coinai", "town support is off");
   return;
  }
  s_Home = ACR_OneSide.Home();
  if (!s_Home || !ACR_TownSupport.IsTown(s_Home)) { ACR_TestRegress.Abort("coinai", "no home town"); s_Home = null; return; }
  // Every town known Quiet; the home Quiet and due aid; the worst: the nearest open town, Hostile.
  float nearest = 100000;
  foreach (ACR_Base town : ACR_Base.s_All)
  {
   if (!ACR_TownSupport.IsTown(town)) continue;
   Set(town, 40);
   if (town == s_Home || town.m_bPocket || !town.m_sOwner.IsEmpty()) continue;
   float d = vector.DistanceXZ(town.GetOrigin(), s_Home.GetOrigin());
   if (d < nearest) { nearest = d; s_Worst = town; }
  }
  if (s_Worst) Set(s_Worst, 90);
  s_Home.m_fAidAt = 0;
  // A lead on the hideout nearest home.
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  ACR_DefenderPost near;
  float best = 100000;
  foreach (ACR_DefenderPost hideout : hideouts)
  {
   float h = vector.DistanceXZ(hideout.m_vPos, s_Home.GetOrigin());
   if (h < best) { best = h; near = hideout; }
  }
  if (near) s_Lead = ACR_FIAIntel.Lead(army, near, "test");
  // Enough squads for the jobs and the reserve, and a supply truck at home.
  for (int i = 0; i < 3; i++) manager.SpawnUnit(string.Format("COIN AI Rifles %1", i + 1), RIFLES, "", false, "", vector.Zero, ACR_Manager.Ground(s_Home.GetOrigin() + Vector(30 + i * 12, 0, 20)));
  vector spot = manager.m_Requisition.VehicleSpot(s_Home, 10, 40, 4);
  if (spot == vector.Zero) spot = ACR_Manager.Ground(s_Home.GetOrigin() + Vector(-25, 0, 0));
  manager.SpawnUnit("COIN AI Aid Truck", "", TRUCK, true, "", spot, ACR_Manager.Ground(spot + Vector(5, 0, 5)));
  string worst = "none";
  if (s_Worst) worst = s_Worst.m_sName;
  PrintFormat("[ACR] COINAI test: home %1, worst %2 (%3 m), lead %4", s_Home.m_sName, worst, (int)nearest, s_Lead != null);
 }

 protected static void LoadTruck(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (squad.GetName() != "COIN AI Aid Truck") continue;
   IEntity truck = ACR_TownSupport.AidTruck(squad);
   if (!truck) return;
   manager.m_Logistics.LoadPaid(truck, 500 / ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION);
   s_bLoaded = true;
   PrintFormat("[ACR] COINAI test: truck loaded (%1 paid)", manager.m_Logistics.PaidSupplies(truck));
  }
 }

 protected static void Verify(ACR_Manager manager)
 {
  string army = ACR_Coin.Army();
  ACR_AICommander ai = manager.AICommanderFor(army);
  if (!ai) { ACR_TestRegress.Abort("coinai", "no AI commander for the army"); return; }
  bool posted = s_Worst && ai.PostedInForTest(s_Worst);
  Check("coinai: the AI army posts a squad to the worst town it knows", posted && ai.TownPostCountForTest() >= 1, string.Format("worst %1 posted %2, posts %3", s_Worst, posted, ai.TownPostCountForTest()));
  ACR_Squad searcher = ai.SearcherForTest();
  bool searching = s_Lead && searcher && vector.DistanceXZ(ai.SearchAtForTest(), s_Lead.m_vCentre) < 1;
  Check("coinai: it sends a squad to search its FIA intel ring", searching, string.Format("lead %1, searcher %2", s_Lead != null, searcher != null));
  Check("coinai: a supply truck hands out aid in a held Quiet town", ACR_TownSupport.s_iAid >= 1 && s_Home.m_fAidAt > 0, string.Format("%1 aid, %2 aid at %3, support %4", ACR_TownSupport.s_iAid, s_Home.m_sName, s_Home.m_fAidAt, s_Home.m_fSupport));
  int free = ai.FreeSquadsCountForTest();
  Check("coinai: it keeps a reserve uncommitted", free >= ACR_Coin.ReserveKeep(army) || ai.ReserveSquadForTest() != null, string.Format("%1 free (keep %2), reserve squad %3", free, ACR_Coin.ReserveKeep(army), ai.ReserveSquadForTest() != null));
  // Fire: a Quiet town is spared, open country isn't.
  ACR_Base quiet;
  foreach (ACR_Base town : ACR_Base.s_All) if (ACR_TownSupport.IsTown(town) && town.m_iSupportTier == ACR_TownSupport.QUIET && town.m_sOwner != ACR_Pockets.FACTION && ACR_FIAGrievance.InTown(town.GetOrigin())) { quiet = town; break; }
  string held = "";
  if (quiet) held = ACR_TownSupport.FireRestraint(army, ACR_ESupportType.ARTILLERY_BATTERY, quiet.GetOrigin());
  vector open = OpenSpot(s_Home.GetOrigin());
  string fires = ACR_TownSupport.FireRestraint(army, ACR_ESupportType.ARTILLERY_BATTERY, open);
  string smoke = "";
  if (quiet) smoke = ACR_TownSupport.FireRestraint(army, ACR_ESupportType.MORTAR_SMOKE, quiet.GetOrigin());
  Check("coinai: it holds fire on a Quiet town, not in open country or with smoke", quiet && !held.IsEmpty() && fires.IsEmpty() && smoke.IsEmpty(), string.Format("'%1' / open '%2' / smoke '%3'", held, fires, smoke));
  bool defaults = ACR_Coin.Posture(ACR_FactionSelection.West()) == ACR_Coin.AIRMOBILE && ACR_Coin.Posture(ACR_FactionSelection.East()) == ACR_Coin.CONVOY && ACR_Coin.TownPosts(ACR_FactionSelection.West()) < ACR_Coin.TownPosts(ACR_FactionSelection.East());
  ACR_Options.Set("acrCoinPosture", "convoy");
  bool picked = ACR_Coin.Posture(ACR_FactionSelection.West()) == ACR_Coin.CONVOY;
  ACR_Options.Unset("acrCoinPosture");
  Check("coinai: postures default by side (Blue airmobile, Red garrison and convoy) and the host can pick", defaults && picked, string.Format("defaults %1, picked %2", defaults, picked));
  ACR_TestRegress.Complete("coinai", 6);
 }

 protected static vector OpenSpot(vector around)
 {
  for (int a = 0; a < 32; a++)
  {
   vector spot = around + Vector(Math.Cos(a * 0.7) * (600 + a * 40), 0, Math.Sin(a * 0.7) * (600 + a * 40));
   if (BBB_Terrain.IsWaterAt(spot) || ACR_FIAGrievance.Houses(spot, ACR_FIAGrievance.TOWN_RADIUS) > 0) continue;
   return spot;
  }
  return around + Vector(2000, 0, 0);
 }

 protected static void Set(ACR_Base town, float support)
 {
  town.m_fSupport = support;
  town.m_iSupportTier = ACR_TownSupport.TierOf(support, -1);
  town.m_iSupportKnown = town.m_iSupportTier;
 }

 protected static void Check(string name, bool ok, string detail)
 {
  ACR_TestRegress.Check(name, ok, detail);
 }
}
