// Native RU/UA purchases: real crew seats, faction, stock and timeout retention.
// -acrTyphoonPurchaseTest 1; load RHS and UAAF with -acrFactions ru_ua.
class ACR_TyphoonPurchaseRow
{
 string side;
 string name;
 string source;
 IEntity vehicle;
 bool purchase;
 ACR_RequisitionEntry entry;
 int stockBefore;
 int boughtBefore;
}

class ACR_TestTyphoonPurchases
{
 static int tick;
 static IEntity unowned;
 static ref array<ref ACR_TyphoonPurchaseRow> rows = {};

 static void DumpDefaults(IEntity entity)
 {
  if (!entity) return;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(entity.FindComponent(SCR_BaseCompartmentManagerComponent));
  array<BaseCompartmentSlot> slots = {};
  if (compartments)
  {
   compartments.GetCompartmentsOfType(slots, ECompartmentType.PILOT);
   array<BaseCompartmentSlot> turrets = {};
   compartments.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
   slots.Copy(turrets);
   array<BaseCompartmentSlot> pilots = {};
   compartments.GetCompartmentsOfType(pilots, ECompartmentType.PILOT);
   foreach (BaseCompartmentSlot pilot : pilots) slots.Insert(pilot);
   foreach (BaseCompartmentSlot slot : slots)
    PrintFormat("[ACR] VEHICLEPROBE DEFAULT entity=%1 checked=%2 unchecked=%3 occupied=%4", entity.GetPrefabData().GetPrefabName(), slot.GetDefaultOccupantPrefab(), slot.GetDefaultOccupantPrefab(false), slot.GetOccupant() != null);
  }
  IEntity child = entity.GetChildren();
  while (child)
  {
   DumpDefaults(child);
   child = child.GetSibling();
  }
 }

 static bool CorrectSeats(IEntity vehicle, bool armed)
 {
  if (!vehicle) return false;
  SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!seats) return false;
  array<BaseCompartmentSlot> pilots = {};
  array<BaseCompartmentSlot> turrets = {};
  array<BaseCompartmentSlot> cargo = {};
  seats.GetCompartmentsOfType(pilots, ECompartmentType.PILOT);
  seats.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
  seats.GetCompartmentsOfType(cargo, ECompartmentType.CARGO);
  array<IEntity> drivers = {};
  array<IEntity> gunners = {};
  foreach (BaseCompartmentSlot pilot : pilots)
   if (pilot.GetOccupant() && !drivers.Contains(pilot.GetOccupant())) drivers.Insert(pilot.GetOccupant());
  foreach (BaseCompartmentSlot turret : turrets)
   if (turret.GetOccupant() && !gunners.Contains(turret.GetOccupant())) gunners.Insert(turret.GetOccupant());
  bool emptyCargo = !cargo.IsEmpty();
  foreach (BaseCompartmentSlot passenger : cargo)
   if (passenger.GetOccupant() || !passenger.GetDefaultOccupantPrefab().IsEmpty()) emptyCargo = false;
  int expectedGunners;
  if (armed) expectedGunners = 1;
  PrintFormat("[ACR] TYPHOONSEATS %1 drivers=%2 gunners=%3 cargoEmpty=%4", vehicle.GetPrefabData().GetPrefabName(), drivers.Count(), gunners.Count(), emptyCargo);
  return drivers.Count() == 1 && gunners.Count() == expectedGunners && emptyCargo;
 }

 static void Run()
 {
  tick++;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 10)
  {
   // Its fixtures are RHS and UAAF vehicles: without those mods (e.g. srv:, which loads ACA and
   // BBB only) the run can't be valid, so it says why instead of failing its checks (#54).
   bool rhsBase = false;
   foreach (ACR_Base rhs : ACR_Base.s_All)
    if (rhs.m_sOwner == "RHS_AFRF") rhsBase = true;
   if (!rhsBase)
   {
    ACR_TestRegress.Abort("typhoons", "needs RHS and UAAF loaded (-acrFactions ru_ua); no base is held by RHS_AFRF in this run");
    GetGame().GetCallqueue().Remove(Run);
    return;
   }
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   unowned = ACR_Manager.SpawnPrefab("{9BF596A3464B6DB8}Prefabs/Vehicles/Wheeled/K4386/K4386_Camo.et", ACR_Manager.Ground("1420 0 2860"));
   SCR_BaseCompartmentManagerComponent emptySeats;
   if (unowned) emptySeats = SCR_BaseCompartmentManagerComponent.Cast(unowned.FindComponent(SCR_BaseCompartmentManagerComponent));
   array<ECompartmentType> unownedTypes = {ECompartmentType.PILOT, ECompartmentType.TURRET};
   if (emptySeats) emptySeats.SpawnDefaultOccupants(unownedTypes);
   array<string> sides = {"US", "RHS_AFRF"};
   foreach (string side : sides)
   {
    ACR_Base home;
    foreach (ACR_Base base : ACR_Base.s_All)
     if (!home && base.m_sOwner == side && ACR_LogisticsRules.CanDeploy(base)) home = base;
    ACR_FactionData data = manager.m_Bases.GetFaction(side);
    if (!data || !home) { PrintFormat("[ACR] VEHICLEPROBE MISSING SIDE %1", side); continue; }
    data.m_iRP = 10000;
    foreach (int index, ACR_RequisitionEntry entry : data.m_aRequisitions)
    {
     ResourceName resolved = ACR_FactionCatalog.ResolvePrefab(side, entry.m_sVehicle, EEntityCatalogType.VEHICLE);
     PrintFormat("[ACR] VEHICLEPROBE BUY side=%1 index=%2 name=%3 group=%4 source=%5 resolved=%6", side, index, entry.m_sName, entry.m_sGroup, entry.m_sVehicle, resolved);
     if (!entry.m_sVehicle.Contains("K4386") && !(side == "US" && entry.m_sVehicle.Contains("BTR70"))) continue;
     ACR_TyphoonPurchaseRow row = new ACR_TyphoonPurchaseRow();
     row.side = side;
     row.name = entry.m_sName + " 1";
     row.source = entry.m_sVehicle;
     row.purchase = true;
     row.entry = entry;
     row.stockBefore = entry.m_iRemaining;
     row.boughtBefore = entry.m_iBought;
     rows.Insert(row);
     string reply = manager.m_Requisition.PurchaseAt(side, index, home);
     PrintFormat("[ACR] VEHICLEPROBE PURCHASE side=%1 name=%2 reply=%3", side, row.name, reply);
    }
    ResourceName gunship = manager.m_Supports.HelicopterFor(side);
    ResourceName transport = manager.m_Supports.TransportHelicopterFor(side);
    PrintFormat("[ACR] VEHICLEPROBE HELI side=%1 gunship=%2 transport=%3", side, gunship, transport);
    array<ResourceName> helicopters = {gunship, transport};
    foreach (int heliIndex, ResourceName helicopter : helicopters)
    {
     ACR_TyphoonPurchaseRow heliRow = new ACR_TyphoonPurchaseRow();
     heliRow.side = side;
     heliRow.name = "Probe heli " + side + " " + heliIndex;
     heliRow.source = helicopter;
     vector pos = manager.ClearSpotNear(home, 50, 130, 12);
     manager.m_sSpawningFaction = side;
     bool spawned = manager.m_Requisition.SpawnUnit(heliRow.name, "", helicopter, true, "", pos, pos);
     manager.m_sSpawningFaction = "";
     rows.Insert(heliRow);
     PrintFormat("[ACR] VEHICLEPROBE HELISPAWN %1 spawned=%2", heliRow.name, spawned);
    }
   }
  }
  if (tick == 12 || tick == 20 || tick == 48)
  {
   array<IEntity> vehicles = {};
   ACR_Vehicles.All(vehicles);
   foreach (IEntity vehicle : vehicles)
   {
    if (!vehicle || !vehicle.GetPrefabData()) continue;
    string prefab = vehicle.GetPrefabData().GetPrefabName();
    string buyer = ACR_Vehicles.BoughtBy(vehicle);
    if (tick == 12 && prefab.Contains("K4386")) DumpDefaults(vehicle);
    string pending;
    manager.m_PendingCrewNames.Find(vehicle, pending);
    string squadName;
    foreach (ACR_Squad squad : manager.m_Squads)
     if (ACR_Transport.CrewedVehicle(squad) == vehicle) squadName = squad.GetName();
    PrintFormat("[ACR] VEHICLEPROBE ENTITY tick=%1 owner=%2 prefab=%3 pending=%4 squad=%5 seats=%6", tick, buyer, prefab, pending, squadName, ACR_GunnerDrive.DescribeTurrets(vehicle));
    foreach (ACR_TyphoonPurchaseRow matchRow : rows)
     if (matchRow.side == buyer && (matchRow.name == pending || matchRow.name == squadName)) matchRow.vehicle = vehicle;
   }
  }
  if (tick != 50) return;
  foreach (ACR_TyphoonPurchaseRow resultRow : rows)
  {
   ACR_Squad registered;
   foreach (ACR_Squad candidate : manager.m_Squads)
    if (candidate.GetName() == resultRow.name && candidate.GetFaction() && candidate.GetFaction().GetFactionKey() == resultRow.side) registered = candidate;
   array<IEntity> occupants = {};
   if (resultRow.vehicle) ACR_PendingPurchase.Occupants(resultRow.vehicle, occupants);
   string actual;
   if (resultRow.vehicle && resultRow.vehicle.GetPrefabData()) actual = resultRow.vehicle.GetPrefabData().GetPrefabName();
   bool ready = registered && resultRow.vehicle && occupants.Count() > 0;
   PrintFormat("[ACR] VEHICLEPROBE RESULT side=%1 name=%2 source=%3 actual=%4 occupants=%5 registered=%6", resultRow.side, resultRow.name, resultRow.source, actual, occupants.Count(), registered != null);
   ACR_TestRegress.Check("typhoons: " + resultRow.side + " " + resultRow.name + " physically crewed and registered", ready, actual);
   if (resultRow.source.Contains("K4386"))
   {
    bool charged = resultRow.entry && resultRow.entry.m_iBought == resultRow.boughtBefore + 1 && resultRow.entry.m_iRemaining == resultRow.stockBefore - 1;
    ACR_TestRegress.Check("typhoons: " + resultRow.side + " " + resultRow.name + " purchase stays charged past timeout", charged);
    bool ownFaction = !occupants.IsEmpty();
    foreach (IEntity man : occupants)
    {
     FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(man.FindComponent(FactionAffiliationComponent));
     if (!affiliation || !affiliation.GetAffiliatedFaction() || affiliation.GetAffiliatedFaction().GetFactionKey() != resultRow.side) ownFaction = false;
    }
    ACR_TestRegress.Check("typhoons: " + resultRow.side + " " + resultRow.name + " seated crew belong to buyer", ownFaction);
    ACR_TestRegress.Check("typhoons: " + resultRow.side + " " + resultRow.name + " driver gunner and empty cargo", CorrectSeats(resultRow.vehicle, resultRow.source.Contains("_Armed")));
   }
  }
  ACR_TestRegress.Check("typhoons: all eight purchase and helicopter fixtures exist", rows.Count() == 8);
  array<IEntity> unownedCrew = {};
  if (unowned) ACR_PendingPurchase.Occupants(unowned, unownedCrew);
  ACR_TestRegress.Check("typhoons: unowned vehicle receives no fallback crew", unowned && unownedCrew.IsEmpty() && ACR_Vehicles.BoughtBy(unowned).IsEmpty());
  ACR_TestRegress.Complete("typhoons", 19);
  GetGame().GetCallqueue().Remove(Run);
 }
}

modded class ACR_TestHooks
{
 override void RangeStarted(ACR_TestRange range)
 {
  if (System.IsCLIParam("acrTyphoonPurchaseTest")) ACR_Options.Set("acrAI", "none");
  super.RangeStarted(range);
  if (System.IsCLIParam("acrTyphoonPurchaseTest")) GetGame().GetCallqueue().CallLater(ACR_TestTyphoonPurchases.Run, 1000, true);
 }

 override void RangeEnded(ACR_TestRange range)
 {
  if (GetGame()) GetGame().GetCallqueue().Remove(ACR_TestTyphoonPurchases.Run);
  super.RangeEnded(range);
 }
}
