// -acrModernPackTest 1 (server-capable, ~1 min; preset modernpack): #109, the Modern pack
// (ACAModern) plugged into ACR_ContentSocket on RHS: Status Quo's sides. Run it with the pack and
// its mods loaded and -acrFactions ru_us (RHS US vs RHS Russia). Checks the socket reports the pack
// and the setup screen picks its tablet skin, that each RHS side's buy list offers the pack's
// vehicles and they all load, that the vanilla US and USSR lists keep only vanilla catalog vehicles,
// and that every pack vehicle bought by its side comes with that side's own RHS crew in every crew
// seat (driver or pilot, every turret, a commander seat wired to a turret that the vehicle seats).
class ACR_TestModernPack
{
 static ref array<ref ACR_PendingPurchase> s_Bought;
 static ref array<string> s_Sides;
 // "<purchase index>:<seat index>" -> who sat there first ("<prefab leaf>:<faction>").
 static ref map<string, string> s_Seen;
 static int s_iExpected;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_Bought = {};
  s_Sides = {};
  s_Seen = new map<string, string>();
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   array<string> keys = {"RHS_USAF", "RHS_AFRF", "US", "USSR"};
   foreach (string key : keys)
   {
    ACR_FactionSettings settings = ACR_FactionSettings.Find(key);
    if (settings) settings.m_bAICommander = false;
   }
  }
  if (tick == 8) Lists(manager);
  if (tick >= 9 && tick <= 24) Sample();
  if (tick == 25) Report();
  if (tick == 27) ACR_TestRegress.Complete("modernpack", s_iExpected);
 }

 // The pack's vehicles per side, by name (ACR_ModernPlug.AddRequisitions).
 protected static void PackNames(string side, notnull array<string> names)
 {
  names.Clear();
  if (side == "RHS_USAF") names.Copy({"M1126 Stryker CROWS", "M1A1 Abrams Tank", "AH-64D Apache"});
  if (side == "RHS_AFRF") names.Copy({"T-72B Tank", "Mi-24V Hind"});
 }

 protected static ACR_Base HomeOf(string side)
 {
  foreach (ACR_Base candidate : ACR_Base.s_All)
   if (candidate.m_sOwner == side) return candidate;
  return null;
 }

 protected static string Leaf(string path)
 {
  if (path.IsEmpty()) return "-";
  return FilePath.StripExtension(FilePath.StripPath(path));
 }

 protected static void Lists(ACR_Manager manager)
 {
  ACR_ContentSocket socket = ACR_ContentSocket.Get();
  string era = socket.Name();
  PrintFormat("[ACR] MODERNTEST socket '%1', sides %2 vs %3", era, ACR_FactionSelection.West(), ACR_FactionSelection.East());
  ACR_TestRegress.Check("modern: the Modern pack is plugged in", socket.Plugged() && era.Contains("Modern"), era);
  ACR_TestRegress.Check("modern: the setup screen picks the modern tablet skin", ACR_SetupTheme.Chosen() == "tablet", ACR_SetupTheme.Chosen());
  s_iExpected += 2;
  array<string> sides = {"RHS_USAF", "RHS_AFRF"};
  int slot;
  foreach (string side : sides)
  {
   array<ref ACR_RequisitionEntry> entries = {};
   ACR_FactionCatalog.BuildRequisitions(side, ACR_FactionSettings.Find(side), entries);
   array<string> wanted = {};
   PackNames(side, wanted);
   int found;
   int invalid;
   string report;
   foreach (ACR_RequisitionEntry entry : entries)
   {
    if (entry.m_sVehicle.IsEmpty()) continue;
    Resource resource = Resource.Load(entry.m_sVehicle);
    if (!resource || !resource.IsValid()) { invalid++; report += " bad:" + entry.m_sName; }
    if (!wanted.Contains(entry.m_sName)) continue;
    found++;
    report += string.Format(" +%1 (%2 RP, armed %3, aircraft %4)", entry.m_sName, entry.m_iCost, entry.m_bArmed, entry.m_bAircraft);
    Buy(manager, side, entry, slot);
    slot++;
   }
   PrintFormat("[ACR] MODERNTEST %1: %2 entries%3", side, entries.Count(), report);
   ACR_TestRegress.Check("modern: " + side + " buy list prefabs all load", invalid == 0, report);
   ACR_TestRegress.Check("modern: " + side + " sells all the pack's vehicles", found == wanted.Count(), string.Format("%1 of %2:%3", found, wanted.Count(), report));
   s_iExpected += 2;
  }
  // The WCS mods merge every variant into the vanilla catalogs: those lists keep vanilla only.
  array<string> vanillaSides = {"US", "USSR"};
  foreach (string vanillaSide : vanillaSides)
  {
   array<ref ACR_RequisitionEntry> list = {};
   ACR_FactionCatalog.BuildRequisitions(vanillaSide, ACR_FactionSettings.Find(vanillaSide), list);
   int foreign;
   string names;
   foreach (ACR_RequisitionEntry item : list)
   {
    string path = item.m_sVehicle;
    if (path.Contains("Stryker") || path.Contains("M1A1") || path.Contains("AH64") || path.Contains("T72") || path.Contains("Mi24"))
    {
     foreign++;
     names += " " + item.m_sName;
    }
   }
   PrintFormat("[ACR] MODERNTEST %1 (vanilla): %2 entries, mod vehicles%3", vanillaSide, list.Count(), names);
   ACR_TestRegress.Check("modern: WCS vehicles stay out of the vanilla " + vanillaSide + " list", foreign == 0, string.Format("%1:%2", foreign, names));
   s_iExpected++;
  }
  // WCS_Armaments injects Javelin/Igla/Stinger characters into vanilla US, USSR and FIA
  // character catalogs: garrisons stay vanilla-only (no Javelins or MANPADS on FIA).
  array<string> garrisonSides = {"FIA", "US", "USSR"};
  foreach (string gSide : garrisonSides)
  {
   array<ResourceName> pool = manager.m_Bases.GetFaction(gSide).m_aGarrison;
   int leaked;
   string bad;
   foreach (ResourceName soldier : pool)
   {
    string sPath = soldier;
    if (sPath.Contains("Javelin") || sPath.Contains("IGLA") || sPath.Contains("Stinger"))
    {
     leaked++;
     bad += " " + Leaf(sPath);
    }
   }
   PrintFormat("[ACR] MODERNTEST %1 garrison pool: %2 entries, leaked%3", gSide, pool.Count(), bad);
   ACR_TestRegress.Check("modern: WCS Javelin/AA soldiers stay out of " + gSide + " garrisons", !pool.IsEmpty() && leaked == 0, string.Format("pool %1, leaked %2:%3", pool.Count(), leaked, bad));
   s_iExpected++;
  }
  // FIA garrisons draw the Modern pack's guerrilla kits (rolled per soldier) and every prefab loads.
  array<ResourceName> fiaPool = manager.m_Bases.GetFaction("FIA").m_aGarrison;
  int picks = 200;
  int kitted;
  int invalidMod;
  for (int pick = 0; pick < picks && !fiaPool.IsEmpty(); pick++)
  {
   ResourceName role = fiaPool[pick % fiaPool.Count()];
   array<ResourceName> one = {role};
   ResourceName picked = ACR_BaseSystem.PickGarrisonSoldier("FIA", one);
   string pStr = picked;
   if (!pStr.Contains("MOD_FIA_")) continue;
   kitted++;
   Resource res = Resource.Load(picked);
   if (!res || !res.IsValid()) invalidMod++;
  }
  ACR_TestRegress.Check("modern: FIA garrisons draw Modern guerrilla kits and all prefabs load", !fiaPool.IsEmpty() && kitted * 10 >= picks * 5 && invalidMod == 0, string.Format("%1 of %2 kitted, %3 invalid", kitted, picks, invalidMod));
  s_iExpected++;
 }

 protected static void Buy(ACR_Manager manager, string side, ACR_RequisitionEntry entry, int slot)
 {
  ACR_Base home = HomeOf(side);
  float ring = 40;
  if (!home && !ACR_Base.s_All.IsEmpty())
  {
   home = ACR_Base.s_All[0];
   ring = 260;
  }
  if (!home) { PrintFormat("[ACR] MODERNTEST no base for %1", side); return; }
  // Helicopters need more room than the ground vehicles.
  float clearance = 7;
  if (entry.m_bAircraft) clearance = 14;
  vector at = manager.ClearSpotNear(home, ring + 30 * slot, ring + 30 * slot + 50, clearance);
  ACR_PendingPurchase purchase = new ACR_PendingPurchase();
  purchase.m_sName = entry.m_sName;
  purchase.m_sFaction = side;
  manager.m_sSpawningFaction = side;
  bool ok = manager.m_Requisition.SpawnParts(purchase, "", entry.m_sVehicle, true, "", at, at);
  manager.m_sSpawningFaction = "";
  PrintFormat("[ACR] MODERNTEST bought %1 for %2: %3 %4 %5", entry.m_sName, side, ok, entry.m_sVehicle, purchase.m_sError);
  s_Bought.Insert(purchase);
  s_Sides.Insert(side);
 }

 protected static void Seats(IEntity vehicle, notnull array<BaseCompartmentSlot> seats)
 {
  seats.Clear();
  if (!vehicle) return;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (compartments) compartments.GetCompartments(seats);
 }

 protected static void Sample()
 {
  foreach (int i, ACR_PendingPurchase purchase : s_Bought)
  {
   array<BaseCompartmentSlot> seats = {};
   Seats(purchase.m_Vehicle, seats);
   foreach (int j, BaseCompartmentSlot seat : seats)
   {
    IEntity man = seat.GetOccupant();
    string key = string.Format("%1:%2", i, j);
    if (!man || !man.GetPrefabData() || s_Seen.Contains(key)) continue;
    s_Seen.Set(key, man.GetPrefabData().GetPrefabName() + "|" + ACR_Intel.FactionKeyOf(man));
   }
  }
 }

 protected static void Report()
 {
  foreach (int i, ACR_PendingPurchase purchase : s_Bought)
  {
   string side = s_Sides[i];
   IEntity vehicle = purchase.m_Vehicle;
   if (!vehicle)
   {
    ACR_TestRegress.Check("modern: " + side + " " + purchase.m_sName + " spawns crewed by its side", false, "no vehicle: " + purchase.m_sError);
    s_iExpected++;
    continue;
   }
   array<BaseCompartmentSlot> seats = {};
   Seats(vehicle, seats);
   int crewSeats;
   int crewFilled;
   int wrong;
   string roster;
   string missing;
   foreach (int j, BaseCompartmentSlot seat : seats)
   {
    // A commander seat counts when the vehicle seats someone there: the CROWS Stryker's is a
    // passenger seat (WCS disables its commander sight and default occupant; the CROWS gunner
    // fights the vehicle), and ACA leaves such seats empty (ACR_Vehicles, GetDefaultOccupantPrefab).
    bool commander = ACR_Transport.CommanderSeat(seat) && seat.GetDefaultOccupantData() && seat.GetDefaultOccupantData().IsValid();
    bool crewSeat = seat.GetType() == ECompartmentType.PILOT || seat.GetType() == ECompartmentType.TURRET || commander;
    if (!crewSeat) continue;
    crewSeats++;
    string who;
    if (!s_Seen.Find(string.Format("%1:%2", i, j), who))
    {
     missing += " '" + seat.GetCompartmentName() + "'";
     continue;
    }
    crewFilled++;
    array<string> parts = {};
    who.Split("|", parts, false);
    string prefab = parts[0];
    string faction;
    if (parts.Count() > 1) faction = parts[1];
    // The side's own RHS soldier: its faction, and an RHS prefab (not the WCS default occupant).
    if (faction != side || !prefab.Contains("RHS")) wrong++;
    roster += string.Format(" %1:%2", Leaf(prefab), faction);
   }
   string summary = string.Format("%1 (%2): %3/%4 crew seats, %5 not the side's RHS crew;%6 empty:%7", Leaf(vehicle.GetPrefabData().GetPrefabName()), side, crewFilled, crewSeats, wrong, roster, missing);
   PrintFormat("[ACR] MODERNTEST crew %1", summary);
   ACR_TestRegress.Check("modern: " + side + " " + purchase.m_sName + " spawns crewed by its side", crewSeats > 0 && crewFilled == crewSeats && wrong == 0, summary);
   s_iExpected++;
  }
 }
}
