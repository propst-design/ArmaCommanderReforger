// srv:coin (Everon whole map, about 40 s): counter-insurgency, one army against FIA (ACR_Coin, #244).
// The absent army holds no base, has no spawn point, no AI commander and no squads; every player
// joins the army, the absent side isn't offered; its HQ is the stronghold (a large FIA pocket with a
// depot); the FIA allegiance and leaning rows are hidden and FIA is hostile, unleaning; FIA counts are
// scaled up; the built-in preset picks COIN; town support is on and every town has a tier.
// srv:support (Everon whole map, about 50 s, time x30 for support): town support (ACR_TownSupport).
// Troops in a calm town win it over a tier, with a feed line; an empty town drifts towards the
// insurgency; the army's strike landing in a town and a building FIA destroys (real damage, FIA
// instigator) move it the right way at once; a Hostile held town pays half its value; informants in a
// Loyal town report FIA there; the army doesn't know a town it never held or saw, FIA does.
class ACR_TestCoin
{
 protected static ACR_Base s_Garrisoned;
 protected static ACR_Base s_Empty;
 protected static ACR_Base s_Struck;
 protected static ACR_Base s_Demolished;
 protected static ACR_Base s_Loyal;
 protected static ACR_Base s_Unseen;
 protected static IEntity s_Building;
 protected static IEntity s_Insurgent;
 protected static float s_fGarrisoned;
 protected static float s_fEmpty;
 protected static float s_fDemolished;
 protected static ref array<IEntity> s_aFound;
 protected static ACR_Base s_AidTown;
 static const ResourceName AID_TRUCK = "{2D74C39A650A3030}Prefabs/Vehicles/Wheeled/M923A1/M923A1_engineer.et";

 // ---------------------------------------------------------------- srv:coin

 static void RunCoin(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (second == 10) Sides(manager);
  if (second == 30) NoAbsentSquads(manager);
 }

 protected static void Sides(ACR_Manager manager)
 {
  string army = ACR_Coin.Army();
  string absent = ACR_OneSide.Absent();
  Check("coin: on, the army is the Blue side, the other side absent", ACR_Coin.On() && ACR_OneSide.On() && army == ACR_FactionSelection.West() && absent == ACR_FactionSelection.East(), string.Format("army '%1', absent '%2'", army, absent));
  int held, absentHeld, points;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == army) held++;
   if (base.m_sOwner == absent) absentHeld++;
  }
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints()) if (point && point.GetFactionKey() == absent) points++;
  ACR_FactionSettings settings = ACR_FactionSettings.Find(absent);
  Check("coin: the army holds bases; the absent side none, no spawn point, no AI commander", held > 0 && absentHeld == 0 && points == 0 && (!settings || !settings.m_bAICommander), string.Format("%1 held, %2 absent-held, %3 absent spawn points", held, absentHeld, points));
  SCR_Faction absentFaction = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(absent));
  Check("coin: every player joins the army, the absent side isn't offered", ACR_PlayerSide.Key() == army && absentFaction && !absentFaction.IsPlayable(), ACR_PlayerSide.Key());
  ACR_Base stronghold = ACR_Coin.Stronghold();
  bool depot;
  foreach (ACR_Depot d : ACR_Depots.s_aDepots) if (d.m_Base && d.m_Base == stronghold) depot = true;
  string strongText = "none";
  float far;
  if (stronghold) strongText = string.Format("%1 owner %2 pocket %3 garrison %4", stronghold.m_sName, stronghold.m_sOwner, stronghold.m_bPocket, stronghold.m_iGarrison);
  if (stronghold && ACR_OneSide.Home()) far = vector.DistanceXZ(stronghold.GetOrigin(), ACR_OneSide.Home().GetOrigin());
  Check("coin: the absent army's HQ is the stronghold: an FIA pocket of 16+ with a depot", stronghold && stronghold == ACR_OneSide.AbsentHQ() && stronghold.m_sOwner == ACR_Pockets.FACTION && stronghold.m_bPocket && stronghold.m_iGarrison >= ACR_Coin.STRONGHOLD_MIN && depot, string.Format("%1, depot %2, %3 m from the army's HQ", strongText, depot, (int)far));
  bool rows = ACR_Setup.HiddenByMatchType("fiaallegiance") && ACR_Setup.HiddenByMatchType("fialeaning") && ACR_Setup.HiddenByMatchType("zombiemode") && !ACR_Setup.HiddenByMatchType("coinarmy") && !ACR_Setup.Rows().Contains("fiaallegiance\t") && ACR_Setup.Rows().Contains("coinarmy\t");
  Check("coin: the allegiance and leaning rows are hidden, the army row shown; FIA hostile, no leaning", rows && ACR_Insurgency.Ally().IsEmpty() && !ACR_Insurgency.Leaning(), string.Format("rows %1, ally '%2', leaning %3", rows, ACR_Insurgency.Ally(), ACR_Insurgency.Leaning()));
  int houses;
  int roads = ACR_Guerrillas.CountOption(false, houses);
  int pockets;
  foreach (ACR_Base pocket : ACR_Base.s_All) if (pocket.m_bPocket && pocket.m_sOwner == ACR_Pockets.FACTION) pockets++;
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  Check("coin: FIA counts scaled up (many x1.5): pockets, cells, hideouts", ACR_Pockets.CountOption(false) == 18 && roads == 12 && houses == 9 && pockets > 7 && hideouts.Count() >= 3, string.Format("pockets wanted %1 placed %2, roads %3, houses %4, cells %5, hideouts %6", ACR_Pockets.CountOption(false), pockets, roads, houses, ACR_Guerrillas.WantedCells(), hideouts.Count()));
  bool preset;
  array<ref ACR_SetupPreset> builtIn = ACR_SetupPresets.BuiltIn();
  foreach (ACR_SetupPreset p : builtIn) if (p.m_sName == ACR_SetupPresets.COIN_NAME && p.m_Values.Get("acrMatchType") == "coin") preset = true;
  Check("coin: the built-in Counter-insurgency preset picks COIN", preset, "");
  int towns, tiered;
  foreach (ACR_Base town : ACR_Base.s_All)
  {
   if (!ACR_TownSupport.IsTown(town)) continue;
   towns++;
   if (town.m_iSupportTier >= 0 && town.m_fSupport >= 0) tiered++;
  }
  string strongTier = "-";
  if (stronghold) strongTier = ACR_TownSupport.TierName(stronghold.m_iSupportTier);
  Check("coin: town support is on, every town has a tier, the stronghold Hostile", ACR_TownSupport.On() && towns > 5 && tiered == towns && stronghold && stronghold.m_iSupportTier == ACR_TownSupport.HOSTILE, string.Format("%1 of %2 towns, stronghold %3", tiered, towns, strongTier));
 }

 protected static void NoAbsentSquads(ACR_Manager manager)
 {
  string absent = ACR_OneSide.Absent();
  int squads, army;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction()) continue;
   if (squad.GetFaction().GetFactionKey() == absent) squads++;
   if (squad.GetFaction().GetFactionKey() == ACR_Coin.Army()) army++;
  }
  Check("coin: the absent side has no squads, the army has some", squads == 0 && army > 0, string.Format("%1 absent, %2 army squads", squads, army));
  ACR_TestRegress.Complete("coin", 9);
 }

 // ---------------------------------------------------------------- srv:support

 static void RunSupport(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 10) Start(manager);
  if (!s_Garrisoned) return;
  if (second == 13) Income(manager);
  if (second == 14) Struck(manager);
  if (second == 15) Demolish(manager);
  if (second == 17) Demolished(manager);
  if (second == 19) Aid(manager);
  if (second == 23) Informants(manager);
  if (second == 24) Knowledge(manager);
  if (second == 46) Drifted(manager);
 }

 protected static void Start(ACR_Manager manager)
 {
  if (!ACR_TownSupport.On())
  {
   ACR_TestRegress.Abort("support", "town support is off (COIN not on)");
   return;
  }
  ACR_TownSupport.s_fRateScaleForTest = 30;
  string army = ACR_Coin.Army();
  array<ACR_Base> quiet = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!ACR_TownSupport.IsTown(base) || !base.m_sOwner.IsEmpty() || base.m_bPocket) continue;
   if (NearAnyone(manager, base.GetOrigin(), 700) || NearFIAPost(base.GetOrigin(), 700)) continue;
   quiet.Insert(base);
  }
  // Towns apart from each other, so one's men aren't in another.
  foreach (ACR_Base town : quiet)
  {
   if (!Apart(town)) continue;
   if (!s_Garrisoned) s_Garrisoned = town;
   else if (!s_Empty) s_Empty = town;
   else if (!s_Struck && ACR_FIAGrievance.InTown(town.GetOrigin())) s_Struck = town;
   else if (!s_Demolished && BuildingNear(town.GetOrigin(), 120)) s_Demolished = town;
   else if (!s_Loyal) s_Loyal = town;
   else if (!s_Unseen) s_Unseen = town;
  }
  if (!s_Garrisoned || !s_Empty || !s_Struck || !s_Demolished || !s_Loyal || !s_Unseen)
  {
   ACR_TestRegress.Abort("support", string.Format("not enough quiet towns apart (%1 candidates)", quiet.Count()));
   s_Garrisoned = null;
   return;
  }
  Set(s_Garrisoned, 52);
  Set(s_Empty, 40);
  Set(s_Struck, 30);
  Set(s_Demolished, 60);
  Set(s_Loyal, 10);
  Set(s_Unseen, 60);
  s_fGarrisoned = 52;
  s_fEmpty = 40;
  manager.m_Bases.SpawnGuards(army, s_Garrisoned.GetOrigin(), 4, "COIN test garrison", 30, 6);
  manager.m_Bases.SpawnGuards(ACR_Pockets.FACTION, s_Loyal.GetOrigin(), 2, "COIN test FIA in a Loyal town", 30, 6);
  // The FIA man who brings a building down: away from every test town.
  vector away = ACR_Manager.Ground(s_Unseen.GetOrigin() + Vector(0, 0, 400));
  SCR_AIGroup fia = manager.m_Bases.SpawnGuards(ACR_Pockets.FACTION, away, 1, "COIN test FIA sapper", 5, 2);
  s_Insurgent = FirstMan(fia);
  s_Building = BuildingNear(s_Demolished.GetOrigin(), 120);
  // The aid truck: the army's supply truck in a town it holds (its HQ).
  foreach (ACR_Base held : ACR_Base.s_All) if (held.m_sOwner == army && ACR_TownSupport.IsTown(held)) { s_AidTown = held; break; }
  if (s_AidTown)
  {
   vector spot = manager.m_Requisition.VehicleSpot(s_AidTown, 10, 40, 4);
   if (spot == vector.Zero) spot = ACR_Manager.Ground(s_AidTown.GetOrigin() + Vector(20, 0, 0));
   manager.SpawnUnit("COIN Aid Truck", "", AID_TRUCK, true, "", spot, ACR_Manager.Ground(spot + Vector(5, 0, 5)));
  }
  PrintFormat("[ACR] SUPPORT test: garrisoned %1, empty %2, struck %3, demolished %4, loyal %5, unseen %6", s_Garrisoned.m_sName, s_Empty.m_sName, s_Struck.m_sName, s_Demolished.m_sName, s_Loyal.m_sName, s_Unseen.m_sName);
 }

 protected static void Set(ACR_Base town, float support)
 {
  town.m_fSupport = support;
  town.m_iSupportTier = ACR_TownSupport.TierOf(support, -1);
  town.m_iSupportKnown = -1;
  town.m_fSupportCalmUntil = 0;
 }

 // A held town turned Hostile pays the army half its value.
 protected static void Income(ACR_Manager manager)
 {
  string army = ACR_Coin.Army();
  ACR_Base held;
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sOwner == army && ACR_TownSupport.IsTown(base) && base.m_iValue >= 2) { held = base; break; }
  if (!held) { Check("support: a Hostile held town pays half its income", false, "no held town worth 2+"); return; }
  float keep = held.m_fSupport;
  int keepTier = held.m_iSupportTier;
  held.m_fSupport = 30;
  held.m_iSupportTier = ACR_TownSupport.QUIET;
  int quiet = manager.m_Bases.GetIncome(army);
  held.m_fSupport = 90;
  held.m_iSupportTier = ACR_TownSupport.HOSTILE;
  int hostile = manager.m_Bases.GetIncome(army);
  string detail = manager.m_Bases.Describe(held, army);
  held.m_fSupport = keep;
  held.m_iSupportTier = keepTier;
  float lost = held.m_iValue * (1 - ACR_Coin.HostileIncome());
  Check("support: a Hostile held town pays half its income", Math.AbsFloat((quiet - hostile) - lost) <= 1 && detail.Contains("support: Hostile"), string.Format("%1 worth %2: quiet %3, hostile %4 RP; '%5'", held.m_sName, held.m_iValue, quiet, hostile, detail));
 }

 // The army's strike landing in a town turns it towards the insurgency at once.
 protected static void Struck(ACR_Manager manager)
 {
  float before = s_Struck.m_fSupport;
  ACR_FIAGrievance.StrikeLanded(ACR_Coin.Army(), s_Struck.GetOrigin(), ACR_FIAGrievance.STRIKE_HEAVY, "shelling");
  float moved = s_Struck.m_fSupport - before;
  Check("support: the army's barrage in a town turns it towards the insurgency", Math.AbsFloat(moved - ACR_FIAGrievance.STRIKE_HEAVY * ACR_TownSupport.HARM_SCALE) < 0.01 && s_Struck.m_sSupportWhy.Contains("our shelling"), string.Format("%1: +%2 (%3)", s_Struck.m_sName, moved, s_Struck.m_sSupportWhy));
 }

 // FIA brings a real building down in a town (FIA instigator): the town turns towards the army.
 protected static void Demolish(ACR_Manager manager)
 {
  s_fDemolished = s_Demolished.m_fSupport;
  if (!s_Building || !s_Insurgent) return;
  SCR_DestructibleBuildingComponent damage = SCR_DestructibleBuildingComponent.Cast(s_Building.FindComponent(SCR_DestructibleBuildingComponent));
  if (!damage) return;
  vector hit[3];
  hit[0] = s_Building.GetOrigin();
  hit[1] = "0 1 0";
  hit[2] = "0 -1 0";
  SCR_DamageContext context = new SCR_DamageContext(EDamageType.EXPLOSIVE, 1000000, hit, s_Building, damage.GetDefaultHitZone(), Instigator.CreateInstigator(s_Insurgent), null, -1, -1);
  damage.HandleDamage(context);
  PrintFormat("[ACR] SUPPORT test: building state %1", damage.GetState());
 }

 protected static void Demolished(ACR_Manager manager)
 {
  float moved = s_Demolished.m_fSupport - s_fDemolished;
  // A tick may fall between (FIA_WORKS / EMPTY at x30): the harm is what counts, -2 at least.
  Check("support: a building FIA destroys turns the town towards the army", s_Insurgent && moved <= -ACR_FIAGrievance.BUILDING * ACR_TownSupport.HARM_SCALE + 0.01 && s_Demolished.m_sSupportWhy.Contains("FIA demolitions"), string.Format("%1: %2 (%3), sapper %4", s_Demolished.m_sName, moved, s_Demolished.m_sSupportWhy, s_Insurgent != null));
 }

 // The truck hands out aid once (the town moves a step towards the army), then is refused with the reason.
 protected static void Aid(ACR_Manager manager)
 {
  ACR_Squad crew;
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == "COIN Aid Truck") crew = squad;
  IEntity truck = ACR_TownSupport.AidTruck(crew);
  if (!crew || !truck || !s_AidTown)
  {
   Check("support: an aid truck moves a town once and is refused the second time with the reason", false, string.Format("crew %1, truck %2, town %3", crew != null, truck != null, s_AidTown != null));
   return;
  }
  manager.m_Logistics.LoadPaid(truck, 500 / ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION);
  float paid = manager.m_Logistics.PaidSupplies(truck);
  float before = s_AidTown.m_fSupport;
  bool first;
  string firstSaid = ACR_TownSupport.Aid(crew, first);
  float moved = s_AidTown.m_fSupport - before;
  float paidAfter = manager.m_Logistics.PaidSupplies(truck);
  bool second;
  string secondSaid = ACR_TownSupport.Aid(crew, second);
  bool line = HasLine(manager.IntelFor(ACR_Coin.Army()), "Aid delivered to " + s_AidTown.m_sName);
  Check("support: an aid truck moves a town once and is refused the second time with the reason", first && Math.AbsFloat(moved + ACR_TownSupport.AID_STEP) < 0.01 && paid - paidAfter >= ACR_TownSupport.AID_SUPPLIES - 0.5 && !second && secondSaid.Contains("had aid") && line, string.Format("%1: %2 -> %3; paid %4 -> %5; '%6' / '%7'; line %8", s_AidTown.m_sName, before, s_AidTown.m_fSupport, paid, paidAfter, firstSaid, secondSaid, line));
  // Somewhere that isn't a town: refused, nothing spent.
  string thirdSaid = ACR_TownSupport.AidRefusal(null, truck, 2);
  Check("support: aid outside a town is refused with the reason", thirdSaid.Contains("drive the supply truck into one"), thirdSaid);
 }

 protected static void Informants(ACR_Manager manager)
 {
  ACR_Intel intel = manager.IntelFor(ACR_Coin.Army());
  bool line = HasLine(intel, "Informants in " + s_Loyal.m_sName);
  Check("support: informants in a Loyal town report FIA there", line && s_Loyal.m_iSupportTier == ACR_TownSupport.LOYAL, string.Format("%1 tier %2, line %3", s_Loyal.m_sName, ACR_TownSupport.TierName(s_Loyal.m_iSupportTier), line));
 }

 protected static void Knowledge(ACR_Manager manager)
 {
  string army = ACR_Coin.Army();
  int armyKnows = ACR_TownSupport.KnownTier(s_Unseen, army);
  int fiaKnows = ACR_TownSupport.KnownTier(s_Unseen, ACR_Pockets.FACTION);
  string detail = manager.m_Bases.Describe(s_Unseen, army);
  Check("support: the army doesn't know a town it never held or saw; FIA does", armyKnows < 0 && fiaKnows == s_Unseen.m_iSupportTier && detail.Contains("support: unknown"), string.Format("%1: army %2, FIA %3, '%4'", s_Unseen.m_sName, armyKnows, fiaKnows, detail));
 }

 protected static void Drifted(ACR_Manager manager)
 {
  ACR_Intel intel = manager.IntelFor(ACR_Coin.Army());
  bool line = HasLine(intel, s_Garrisoned.m_sName + " comes round to Quiet");
  Check("support: troops in a calm town win it over a tier, with a feed line", s_Garrisoned.m_fSupport < s_fGarrisoned - 6 && s_Garrisoned.m_iSupportTier == ACR_TownSupport.QUIET && line, string.Format("%1: %2 -> %3 (%4), line %5", s_Garrisoned.m_sName, s_fGarrisoned, s_Garrisoned.m_fSupport, ACR_TownSupport.TierName(s_Garrisoned.m_iSupportTier), line));
  Check("support: an empty town drifts towards the insurgency", s_Empty.m_fSupport > s_fEmpty + 2, string.Format("%1: %2 -> %3", s_Empty.m_sName, s_fEmpty, s_Empty.m_fSupport));
  ACR_TestRegress.Complete("support", 9);
 }

 // ---------------------------------------------------------------- helpers

 protected static bool Apart(ACR_Base town)
 {
  array<ACR_Base> picked = {s_Garrisoned, s_Empty, s_Struck, s_Demolished, s_Loyal, s_Unseen};
  foreach (ACR_Base other : picked) if (other && vector.DistanceXZ(other.GetOrigin(), town.GetOrigin()) < 900) return false;
  return true;
 }

 protected static bool NearAnyone(ACR_Manager manager, vector pos, float range)
 {
  return manager.m_Bases.MenWithin(pos, range, ACR_Coin.Army()) > 0 || manager.m_Bases.MenWithin(pos, range, ACR_Pockets.FACTION) > 0;
 }

 protected static bool NearFIAPost(vector pos, float range)
 {
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
   if (!post.m_bGone && post.m_Cell && vector.DistanceXZ(post.m_vPos, pos) < range) return true;
  return false;
 }

 protected static IEntity FirstMan(SCR_AIGroup group)
 {
  if (!group) return null;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents) if (agent && agent.GetControlledEntity()) return agent.GetControlledEntity();
  return null;
 }

 protected static bool HasLine(ACR_Intel intel, string text)
 {
  if (!intel) return false;
  foreach (string line : intel.m_Feed) if (line.Contains(text)) return true;
  return false;
 }

 protected static IEntity BuildingNear(vector pos, float radius)
 {
  if (!s_aFound) s_aFound = {};
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(pos, radius, CollectBuilding, null, EQueryEntitiesFlags.STATIC);
  IEntity best;
  float nearest = radius;
  foreach (IEntity e : s_aFound)
  {
   float d = vector.DistanceXZ(e.GetOrigin(), pos);
   if (d >= nearest) continue;
   nearest = d;
   best = e;
  }
  return best;
 }

 protected static bool CollectBuilding(IEntity e)
 {
  if (!SCR_DestructibleBuildingEntity.Cast(e) || e.GetParent()) return true;
  SCR_DestructibleBuildingComponent damage = SCR_DestructibleBuildingComponent.Cast(e.FindComponent(SCR_DestructibleBuildingComponent));
  if (damage && damage.GetState() != EDamageState.DESTROYED) s_aFound.Insert(e);
  return true;
 }

 protected static void Check(string name, bool ok, string detail)
 {
  ACR_TestRegress.Check(name, ok, detail);
 }
}
