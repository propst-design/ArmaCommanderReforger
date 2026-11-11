// Logistics: ammo, repair, fuel and medical vehicles actually do their job.
// The game's AI never uses them, so this is scripted: a service vehicle that is
// stopped (or crawling) tops up friendly soldiers and vehicles within
// SERVICE_RANGE, a little every couple of seconds, using up its own stock. An
// empty truck restocks slowly while parked inside one of its side's spawn bases,
// so a truck that is intercepted on the way to the front is a real loss.
// Also home of the ammo / health / fuel measurements used for status markers.
//
// Game supplies: trucks that carry a vanilla supply container (arsenal, ammo,
// repair and construction trucks) share one pool with the game. Every tick the
// mod's stock is written into the container (stock x points per load), and any
// change made by the game since the last write (players taking gear from the
// arsenal, resupplying, building, loading or unloading supplies by hand) is read
// back into the stock first. So AI resupply and player use drain the same number.
enum ACR_EService
{
 AMMO = 1,
 REPAIR = 2,
 FUEL = 4,
 MEDICAL = 8,
 CONSTRUCTION = 16
}

class ACR_Logistics : Managed
{
 static const float SERVICE_RANGE = 25;
 static const float SERVICE_MAX_SPEED = 2;     // m/s: the truck must be (almost) stopped
 static const int TICK_MS = 2000;
 static const float SOLDIER_COOLDOWN_MS = 15000;
 // Stock, in service "loads": one soldier resupplied, one turret refilled, one repair or refuel step, one soldier treated.
 static const float CAPACITY_AMMO = 40;
 static const float CAPACITY_REPAIR = 30;
 static const float CAPACITY_FUEL = 30;
 static const float CAPACITY_MEDICAL = 30;
 static const float CAPACITY_CONSTRUCTION = 40;
 // Game supply points per load, per truck type (full truck: ammo/arsenal 1000,
 // repair 600, construction 1000), so one arsenal visit doesn't empty a truck.
 static const float SUPPLIES_PER_LOAD_AMMO = 25;
 static const float SUPPLIES_PER_LOAD_REPAIR = 20;
 static const float SUPPLIES_PER_LOAD_FUEL = 20;
 static const float SUPPLIES_PER_LOAD_MEDICAL = 20;
 static const float SUPPLIES_PER_LOAD_CONSTRUCTION = 25;
 static const float RESTOCK_PER_TICK = 0.04;   // share of capacity per tick at a spawn base (about 50 s to fill)

 protected ACR_Manager m_Manager;
 protected float m_fNextTick;
 protected ref map<IEntity, float> m_Stock = new map<IEntity, float>();
 protected ref map<IEntity, float> m_SoldierServedAt = new map<IEntity, float>();
 protected ref map<IEntity, float> m_GameSupplies = new map<IEntity, float>(); // container value after our last write
 protected static ref map<ResourceName, int> s_ServiceCache;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_ServiceCache = new map<ResourceName, int>();
 }

 // Service points: built structures (ACR_Structures: field hospital, vehicle maintenance, ammo
 // and fuel storage). Supplied from the FOB by default; soldiers are served after POINT_DWELL_MS in
 // range (then every POINT_SOLDIER_MS), vehicles one step every POINT_VEHICLE_MS.
 static const float POINT_DWELL_MS = 20000;
 static const float POINT_SOLDIER_MS = 30000;
 static const float POINT_VEHICLE_MS = 10000;
 protected ref map<IEntity, int> m_PointFlags = new map<IEntity, int>();
 protected ref map<IEntity, string> m_PointFaction = new map<IEntity, string>();
 protected ref map<IEntity, float> m_ArrivedAt = new map<IEntity, float>();   // soldier -> first seen at a point
 protected ref map<IEntity, float> m_LastSeenAt = new map<IEntity, float>();
 protected ref map<IEntity, ref map<IEntity, float>> m_VehicleServedAt = new map<IEntity, ref map<IEntity, float>>(); // point -> vehicle -> time

 void AddPoint(IEntity point, int flags, string faction)
 {
  m_PointFlags.Set(point, flags);
  m_PointFaction.Set(point, faction);
 }

 void RemovePoint(IEntity point)
 {
  m_PointFlags.Remove(point);
  m_PointFaction.Remove(point);
 }

 // Drops points whose structure was deleted.
 void PrunePoints()
 {
  for (int i = m_PointFlags.Count() - 1; i >= 0; i--)
   if (!m_PointFlags.GetKey(i)) m_PointFlags.RemoveElement(i);
  for (int j = m_PointFaction.Count() - 1; j >= 0; j--)
   if (!m_PointFaction.GetKey(j)) m_PointFaction.RemoveElement(j);
  for (int k = m_VehicleServedAt.Count() - 1; k >= 0; k--)
   if (!m_VehicleServedAt.GetKey(k)) m_VehicleServedAt.RemoveElement(k);
 }

 bool IsPoint(IEntity station)
 {
  return station && m_PointFlags.Contains(station);
 }

 void ACR_Logistics(ACR_Manager manager)
 {
  m_Manager = manager;
 }

 // ---------------------------------------------------------------- what is what

 // Service flags of a vehicle, from its prefab name (vanilla: M923A1_ammo,
 // _arsenal, _repair, _tanker; Ural4320_ammo, _repair, _tanker; ambulances).
 static int ServiceOf(IEntity vehicle)
 {
  if (!vehicle || !vehicle.GetPrefabData()) return 0;
  ResourceName prefab = vehicle.GetPrefabData().GetPrefabName();
  int cached;
  if (s_ServiceCache.Find(prefab, cached)) return cached;
  int flags = ServiceOfName(prefab);
  // Parts (fuel nozzles etc.) are not vehicles.
  if (!Vehicle.Cast(vehicle)) flags = 0;
  s_ServiceCache.Set(prefab, flags);
  return flags;
 }

 static int ServiceOfName(string prefab)
 {
  string name = prefab;
  name.ToLower();
  int flags;
  if (name.Contains("_ammo") || name.Contains("arsenal")) flags |= ACR_EService.AMMO;
  if (name.Contains("repair")) flags |= ACR_EService.REPAIR;
  if (name.Contains("tanker") || name.Contains("_fuel")) flags |= ACR_EService.FUEL;
  if (name.Contains("ambulance")) flags |= ACR_EService.MEDICAL;
  if (name.Contains("engineer")) flags |= ACR_EService.CONSTRUCTION;
  return flags;
 }

 static string ServiceName(int flags)
 {
  if ((flags & ACR_EService.AMMO) != 0) return "ammo";
  if ((flags & ACR_EService.REPAIR) != 0) return "repair";
  if ((flags & ACR_EService.FUEL) != 0) return "fuel";
  if ((flags & ACR_EService.MEDICAL) != 0) return "medical";
  if ((flags & ACR_EService.CONSTRUCTION) != 0) return "construction";
  return "";
 }

 static float Capacity(int flags)
 {
  if ((flags & ACR_EService.AMMO) != 0) return CAPACITY_AMMO;
  if ((flags & ACR_EService.REPAIR) != 0) return CAPACITY_REPAIR;
  if ((flags & ACR_EService.FUEL) != 0) return CAPACITY_FUEL;
  if ((flags & ACR_EService.MEDICAL) != 0) return CAPACITY_MEDICAL;
  if ((flags & ACR_EService.CONSTRUCTION) != 0) return CAPACITY_CONSTRUCTION;
  return 0;
 }

 static float SuppliesPerLoad(int flags)
 {
  if ((flags & ACR_EService.AMMO) != 0) return SUPPLIES_PER_LOAD_AMMO;
  if ((flags & ACR_EService.REPAIR) != 0) return SUPPLIES_PER_LOAD_REPAIR;
  if ((flags & ACR_EService.FUEL) != 0) return SUPPLIES_PER_LOAD_FUEL;
  if ((flags & ACR_EService.MEDICAL) != 0) return SUPPLIES_PER_LOAD_MEDICAL;
  if ((flags & ACR_EService.CONSTRUCTION) != 0) return SUPPLIES_PER_LOAD_CONSTRUCTION;
  return 0;
 }

 float StockOf(IEntity vehicle)
 {
  if (IsPoint(vehicle))
  {
   ACR_Base fob = ServiceFob(vehicle);
   if (!ACR_LogisticsRules.SuppliedServices()) return 1000;
   if (!fob) return 0;
   if (fob.m_bContested) return 0;
   int price = ACR_LogisticsRules.ServiceCost();
   if (price <= 0) return 1000;
   return ACR_FobSupply.Stock(fob.m_FobSupply) / (float)price;
  }
  int flags = ServiceOf(vehicle);
  if (flags == 0) return 0;
  float stock;
  if (!m_Stock.Find(vehicle, stock))
  {
   stock = Capacity(flags);
   m_Stock.Set(vehicle, stock);
  }
  return stock;
 }

 int StockPercent(IEntity vehicle)
 {
  float capacity = Capacity(ServiceOf(vehicle));
  if (capacity <= 0) return -1;
  return (int)(100 * StockOf(vehicle) / capacity);
 }

 bool Use(IEntity vehicle, float amount)
 {
  if (IsPoint(vehicle))
  {
   ACR_Base fob = ServiceFob(vehicle);
   if (!ACR_LogisticsRules.SuppliedServices()) return true;
   if (!fob) return false;
   return ACR_FobSupply.SpendBuild(fob.m_FobSupply, Math.Ceil(amount * ACR_LogisticsRules.ServiceCost()));
  }
  float stock = StockOf(vehicle);
  if (stock < amount) return false;
  m_Stock.Set(vehicle, stock - amount);
  int flags = ServiceOf(vehicle);
  if (SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled() && flags != 0) WriteGameSupplies(vehicle, flags);
  return true;
 }

 void AddStock(IEntity vehicle, float amount)
 {
  if (!vehicle || amount <= 0) return;
  int flags = ServiceOf(vehicle);
  if (flags == 0) return;
  float cap = Capacity(flags);
  float stock = StockOf(vehicle);
  m_Stock.Set(vehicle, Math.Clamp(stock + amount, 0, cap));
  if (SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled()) WriteGameSupplies(vehicle, flags);
 }

 ACR_Base ServiceFob(IEntity station)
 {
  if (!station || !IsPoint(station) || !ACR_LogisticsRules.SuppliedServices()) return null;
  return ACR_FobSupply.Nearby(m_PointFaction.Get(station), station.GetOrigin());
 }

 // ---------------------------------------------------------------- game supplies

 // The vanilla supply container of a vehicle: on the vehicle itself or on one
 // of its parts (the cargo box of M923A1/Ural service trucks).
 static SCR_ResourceContainer SupplyContainer(IEntity vehicle)
 {
  if (!vehicle) return null;
  ACR_FobProviderComponent fob = ACR_FobProviderComponent.Cast(vehicle.FindComponent(ACR_FobProviderComponent));
  if (fob) return ACR_FobSupply.CommandPostStore(fob.CommandPost());
  SCR_ResourceComponent resources = SCR_ResourceComponent.Cast(vehicle.FindComponent(SCR_ResourceComponent));
  if (resources)
  {
   SCR_ResourceContainer own = resources.GetContainer(EResourceType.SUPPLIES);
   if (own) return own;
  }
  IEntity child = vehicle.GetChildren();
  while (child)
  {
   SCR_ResourceComponent part = SCR_ResourceComponent.Cast(child.FindComponent(SCR_ResourceComponent));
   if (part)
   {
    SCR_ResourceContainer found = part.GetContainer(EResourceType.SUPPLIES);
    if (found) return found;
   }
   child = child.GetSibling();
  }
  return null;
 }

 // Game supplies in the truck right now (-1 = no container).
 static float GameSupplies(IEntity vehicle)
 {
  SCR_ResourceContainer container = SupplyContainer(vehicle);
  if (!container) return -1;
  return container.GetResourceValue();
 }

 // What the game did to the container since our last write goes into the stock.
 protected void ReadGameSupplies(IEntity station, int flags)
 {
  float written;
  if (!m_GameSupplies.Find(station, written)) return;
  SCR_ResourceContainer container = SupplyContainer(station);
  float perLoad = SuppliesPerLoad(flags);
  if (!container || perLoad <= 0) return;
  float delta = container.GetResourceValue() - written;
  if (Math.AbsFloat(delta) < 0.01) return;
  float stock = StockOf(station);
  m_Stock.Set(station, Math.Clamp(stock + delta / perLoad, 0, Capacity(flags)));
  m_GameSupplies.Set(station, container.GetResourceValue());
 }

 // The stock goes into the container (sized to the truck's full stock).
 protected void WriteGameSupplies(IEntity station, int flags)
 {
  SCR_ResourceContainer container = SupplyContainer(station);
  float perLoad = SuppliesPerLoad(flags);
  if (!container || perLoad <= 0) return;
  float max = Capacity(flags) * perLoad;
  if (Math.AbsFloat(container.GetMaxResourceValue() - max) > 0.5) container.SetMaxResourceValue(max);
  float target = StockOf(station) * perLoad;
  if (Math.AbsFloat(container.GetResourceValue() - target) >= 0.5) container.SetResourceValue(target);
  m_GameSupplies.Set(station, container.GetResourceValue());
 }

 // ---------------------------------------------------------------- measurements

 // Magazines (plus the loaded one, as a fraction) for primary and secondary weapons.
 static float SoldierMagazines(IEntity soldier)
 {
  if (!soldier) return 0;
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(soldier.FindComponent(BaseWeaponManagerComponent));
  InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(soldier.FindComponent(InventoryStorageManagerComponent));
  if (!weapons || !inventory) return 0;
  array<IEntity> list = {};
  weapons.GetWeaponsList(list);
  float total;
  foreach (IEntity weaponEntity : list)
  {
   BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
   if (!weapon) continue;
   string slot = weapon.GetWeaponSlotType();
   if (slot != "primary" && slot != "secondary") continue;
   array<BaseMuzzleComponent> muzzles = {};
   weapon.GetMuzzlesList(muzzles);
   foreach (BaseMuzzleComponent muzzle : muzzles)
   {
    total += inventory.GetMagazineCountByMuzzle(null, muzzle);
    int max = muzzle.GetMaxAmmoCount();
    if (max > 0) total += muzzle.GetAmmoCount() / (float)max;
   }
  }
  return total;
 }

 // Crewed guns of a vehicle: loaded ammo as a fraction of full (-1 = no guns).
 static float VehicleAmmo(IEntity vehicle)
 {
  array<BaseMagazineComponent> magazines = {};
  TurretMagazines(vehicle, magazines);
  if (magazines.IsEmpty()) return -1;
  float sum;
  foreach (BaseMagazineComponent magazine : magazines)
  {
   int max = magazine.GetMaxAmmoCount();
   if (max > 0) sum += magazine.GetAmmoCount() / (float)max;
  }
  return sum / magazines.Count();
 }

 static void TurretMagazines(IEntity vehicle, notnull array<BaseMagazineComponent> magazines)
 {
  magazines.Clear();
  if (!vehicle) return;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartmentsOfType(slots, ECompartmentType.TURRET);
  foreach (BaseCompartmentSlot slot : slots)
  {
   IEntity turret = slot.GetOwner();
   if (!turret) continue;
   TurretControllerComponent controller = TurretControllerComponent.Cast(turret.FindComponent(TurretControllerComponent));
   if (!controller || !controller.GetWeaponManager()) continue;
   array<BaseWeaponComponent> weapons = {};
   controller.GetWeaponManager().GetWeapons(weapons);
   foreach (BaseWeaponComponent weapon : weapons)
   {
    BaseMagazineComponent magazine = weapon.GetCurrentMagazine();
    if (magazine && !magazines.Contains(magazine)) magazines.Insert(magazine);
   }
  }
 }

 static float Health(IEntity entity)
 {
  if (!entity) return 0;
  DamageManagerComponent damage = DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
  if (!damage) return 1;
  if (damage.IsDestroyed()) return 0;
  return damage.GetHealthScaled();
 }

 // Fuel as a fraction of full (-1 = the vehicle has no fuel system).
 static float Fuel(IEntity vehicle)
 {
  if (!vehicle) return -1;
  SCR_FuelManagerComponent fuel = SCR_FuelManagerComponent.Cast(vehicle.FindComponent(SCR_FuelManagerComponent));
  if (!fuel) return -1;
  float amount, max, percentage;
  fuel.GetTotalValuesOfFuelNodes(amount, max, percentage);
  if (max <= 0) return -1;
  return amount / max;
 }

 // ---------------------------------------------------------------- service

 static string FactionOfVehicle(IEntity vehicle)
 {
  ChimeraCharacter driver = ACR_Squad.DriverOf(vehicle);
  if (driver && ACR_Respawn.IsAlive(driver))
  {
   FactionAffiliationComponent crew = FactionAffiliationComponent.Cast(driver.FindComponent(FactionAffiliationComponent));
   if (crew && crew.GetAffiliatedFaction()) return crew.GetAffiliatedFaction().GetFactionKey();
  }
  FactionAffiliationComponent own = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
  if (own && own.GetAffiliatedFaction()) return own.GetAffiliatedFaction().GetFactionKey();
  return "";
 }

 protected ref array<IEntity> m_Found = {};

 // All service vehicles in the world (found through the squads' crews and
 // the vehicles already known, plus a scan near every base now and then).
 protected ref array<IEntity> m_ServiceVehicles = {};
 protected float m_fNextScan;

 protected void RefreshServiceVehicles(float now)
 {
  for (int i = m_ServiceVehicles.Count() - 1; i >= 0; i--)
  {
   IEntity known = m_ServiceVehicles[i];
   if (!known || Health(known) <= 0) m_ServiceVehicles.RemoveOrdered(i);
  }
  if (now < m_fNextScan) return;
  m_fNextScan = now + 10000;
  m_Found.Clear();
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   IEntity crewed = ACR_Transport.CrewedVehicle(squad);
   if (crewed && !m_Found.Contains(crewed)) m_Found.Insert(crewed);
  }
  // Near a base: from the vehicle registry (a sphere query per base cost 13-26 ms
  // every 10 s, structural review 1 Oct).
  array<IEntity> registered = {};
  ACR_Vehicles.All(registered);
  foreach (IEntity candidate : registered)
  {
   if (m_Found.Contains(candidate) || ServiceOf(candidate) == 0) continue;
   vector at = candidate.GetOrigin();
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (vector.Distance(at, base.GetOrigin()) > base.m_fRadius + 150) continue;
    m_Found.Insert(candidate);
    break;
   }
  }
  foreach (IEntity vehicle : m_Found)
   if (ServiceOf(vehicle) != 0 && !m_ServiceVehicles.Contains(vehicle) && Health(vehicle) > 0) m_ServiceVehicles.Insert(vehicle);
 }

 void Tick()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextTick) return;
  m_fNextTick = now + TICK_MS;
  RefreshServiceVehicles(now);
  bool gameSupplies = SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled();
  if (gameSupplies)
   foreach (IEntity synced : m_ServiceVehicles) ReadGameSupplies(synced, ServiceOf(synced));
  foreach (IEntity station : m_ServiceVehicles)
  {
   int flags = ServiceOf(station);
   string faction = FactionOfVehicle(station);
   if (faction.IsEmpty()) continue;
   Restock(station, faction, flags);
   Physics physics = station.GetPhysics();
   if (physics && physics.GetVelocity().Length() > SERVICE_MAX_SPEED) continue;
   if (StockOf(station) < 1) continue;
   vector at = station.GetOrigin();
   foreach (ACR_Squad squad : m_Manager.m_Squads)
   {
    if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction) continue;
    IEntity anchor = squad.GetAnchor();
    if (!anchor || vector.DistanceXZ(anchor.GetOrigin(), at) > SERVICE_RANGE + 150) continue;
    ServeSquad(station, flags, squad, now);
   }
   // Players' own bodies too.
   array<int> players = {};
   GetGame().GetPlayerManager().GetPlayers(players);
   foreach (int id : players)
   {
    IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
    Faction playerFaction = SCR_FactionManager.SGetPlayerFaction(id);
    if (!body || !playerFaction || playerFaction.GetFactionKey() != faction || m_Manager.FindSquadBySoldier(body)) continue;
    if (vector.DistanceXZ(body.GetOrigin(), at) > SERVICE_RANGE) continue;
    ServeSoldier(station, flags, body, now);
    IEntity ridden = ACR_Transport.VehicleOf(body);
    if (ridden) ServeVehicle(station, flags, ridden, now);
   }
  }
  if (gameSupplies)
   foreach (IEntity written : m_ServiceVehicles) WriteGameSupplies(written, ServiceOf(written));
  TickPoints(now);
 }

 protected void TickPoints(float now)
 {
  for (int i = 0; i < m_PointFlags.Count(); i++)
  {
   IEntity point = m_PointFlags.GetKey(i);
   if (!point) continue;
   int flags = m_PointFlags.GetElement(i);
   string faction = m_PointFaction.Get(point);
   vector at = point.GetOrigin();
   foreach (ACR_Squad squad : m_Manager.m_Squads)
   {
    if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction) continue;
    IEntity anchor = squad.GetAnchor();
    if (!anchor || vector.DistanceXZ(anchor.GetOrigin(), at) > SERVICE_RANGE + 150) continue;
    ServeSquad(point, flags, squad, now);
   }
   array<int> players = {};
   GetGame().GetPlayerManager().GetPlayers(players);
   foreach (int id : players)
   {
    IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
    Faction playerFaction = SCR_FactionManager.SGetPlayerFaction(id);
    if (!body || !playerFaction || playerFaction.GetFactionKey() != faction || m_Manager.FindSquadBySoldier(body)) continue;
    if (vector.DistanceXZ(body.GetOrigin(), at) > SERVICE_RANGE) continue;
    ServeSoldier(point, flags, body, now);
    IEntity ridden = ACR_Transport.VehicleOf(body);
    if (ridden) ServeVehicle(point, flags, ridden, now);
   }
   // Parked vehicles of the side too (nobody aboard).
   if ((flags & (ACR_EService.REPAIR | ACR_EService.FUEL | ACR_EService.AMMO)) != 0)
   {
    array<IEntity> vehicles = {};
    ACR_Vehicles.All(vehicles);
    foreach (IEntity parked : vehicles)
    {
     if (vector.DistanceXZ(parked.GetOrigin(), at) > SERVICE_RANGE || Health(parked) <= 0) continue;
     if (ACR_Vehicles.FactionOf(parked) != faction) continue;
     ServeVehicle(point, flags, parked, now);
    }
   }
  }
 }

 // At a service point a soldier is only served after staying a while (and not too often).
 protected bool PointReady(IEntity soldier, float now)
 {
  float seen;
  float arrived;
  if (!m_LastSeenAt.Find(soldier, seen) || now - seen > TICK_MS * 3 || !m_ArrivedAt.Find(soldier, arrived))
  {
   m_ArrivedAt.Set(soldier, now);
   arrived = now;
  }
  m_LastSeenAt.Set(soldier, now);
  if (now - arrived < POINT_DWELL_MS) return false;
  float last;
  if (m_SoldierServedAt.Find(soldier, last) && now - last < POINT_SOLDIER_MS) return false;
  return true;
 }

 protected void ServeSquad(IEntity station, int flags, ACR_Squad squad, float now)
 {
  array<IEntity> vehicles = {};
  squad.GetVehicles(vehicles);
  // Possessed / player-crewed vehicles can temporarily fall out of the squad's
  // agent seat scan. The squad's owned vehicle is still a valid service target.
  if (squad.m_OwnVehicle && !vehicles.Contains(squad.m_OwnVehicle)) vehicles.Insert(squad.m_OwnVehicle);
  foreach (IEntity vehicle : vehicles)
   if (vehicle != station && vector.DistanceXZ(vehicle.GetOrigin(), station.GetOrigin()) <= SERVICE_RANGE + 5) ServeVehicle(station, flags, vehicle, now);
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (!soldier || !ACR_Respawn.IsAlive(soldier) || ACR_Transport.VehicleOf(soldier)) continue;
   if (vector.DistanceXZ(soldier.GetOrigin(), station.GetOrigin()) > SERVICE_RANGE) continue;
   int target = squad.MagazineTarget(soldier);
   ServeSoldier(station, flags, soldier, now, target);
  }
 }

 // An ammo run (ACR_AICommander.ManageAmmoRuns): the truck stopped where it met a squad hands out to
 // every man of it within 'range' and its vehicles, not only to those within SERVICE_RANGE of its
 // tailgate (a squad spreads out where it stops). Returns how many men and vehicles it served.
 int ServeSquadAt(IEntity station, ACR_Squad squad, float range)
 {
  if (!station || !squad) return 0;
  int flags = ServiceOf(station);
  if (flags == 0) return 0;
  if (SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled()) ReadGameSupplies(station, flags);
  if (StockOf(station) < 1) return 0;
  float now = GetGame().GetWorld().GetWorldTime();
  int served;
  array<IEntity> vehicles = {};
  squad.GetVehicles(vehicles);
  if (squad.m_OwnVehicle && !vehicles.Contains(squad.m_OwnVehicle)) vehicles.Insert(squad.m_OwnVehicle);
  foreach (IEntity vehicle : vehicles)
  {
   if (!vehicle || vehicle == station || vector.DistanceXZ(vehicle.GetOrigin(), station.GetOrigin()) > range) continue;
   float vehicleBefore = StockOf(station);
   ServeVehicle(station, flags, vehicle, now);
   if (StockOf(station) < vehicleBefore) served++;
  }
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (!soldier || !ACR_Respawn.IsAlive(soldier) || ACR_Transport.VehicleOf(soldier)) continue;
   if (vector.DistanceXZ(soldier.GetOrigin(), station.GetOrigin()) > range) continue;
   float before = StockOf(station);
   ServeSoldier(station, flags, soldier, now, squad.MagazineTarget(soldier));
   if (StockOf(station) < before) served++;
  }
  return served;
 }

 protected void ServeSoldier(IEntity station, int flags, IEntity soldier, float now, int magazineTarget = 6)
 {
  float last;
  if (IsPoint(station))
  {
   if (!PointReady(soldier, now)) return;
  }
  else if (m_SoldierServedAt.Find(soldier, last) && now - last < SOLDIER_COOLDOWN_MS) return;
  bool served;
  // A new disposable launcher for one who threw his spent one away.
  if ((flags & ACR_EService.AMMO) != 0 && m_Manager.m_Launchers && m_Manager.m_Launchers.IsOwed(soldier) && StockOf(station) >= ACR_Launchers.REARM_COST)
  {
   if (m_Manager.m_Launchers.Rearm(soldier))
   {
    Use(station, ACR_Launchers.REARM_COST);
    served = true;
   }
  }
  if ((flags & ACR_EService.AMMO) != 0 && SoldierMagazines(soldier) < magazineTarget * 0.75)
  {
   SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
   // Only spend stock when a magazine can actually be added (right weapon, room in the pack).
   if (inventory && inventory.IsResupplyMagazinesAvailable(Math.Max(2, magazineTarget - 1)) && Use(station, 1))
   {
    inventory.ResupplyMagazines(Math.Max(2, magazineTarget - 1));
    served = true;
   }
  }
  if ((flags & ACR_EService.MEDICAL) != 0 && Health(soldier) < 0.95)
  {
   SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(soldier.FindComponent(SCR_DamageManagerComponent));
   if (damage && Use(station, 1))
   {
    damage.FullHeal();
    served = true;
   }
  }
  if (served) m_SoldierServedAt.Set(soldier, now);
 }

 protected void ServeVehicle(IEntity station, int flags, IEntity vehicle, float now = 0)
 {
  if (IsPoint(station))
  {
   map<IEntity, float> served = m_VehicleServedAt.Get(station);
   if (!served)
   {
    served = new map<IEntity, float>();
    m_VehicleServedAt.Set(station, served);
   }
   float last;
   if (served.Find(vehicle, last) && now - last < POINT_VEHICLE_MS) return;
   served.Set(vehicle, now);
  }
  if ((flags & ACR_EService.AMMO) != 0)
  {
   array<BaseMagazineComponent> magazines = {};
   TurretMagazines(vehicle, magazines);
   foreach (BaseMagazineComponent magazine : magazines)
   {
    if (magazine.GetAmmoCount() >= magazine.GetMaxAmmoCount()) continue;
    if (!Use(station, 0.5)) break;
    magazine.SetAmmoCount(magazine.GetMaxAmmoCount());
   }
  }
  if ((flags & ACR_EService.REPAIR) != 0)
  {
   SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(vehicle.FindComponent(SCR_DamageManagerComponent));
   if (damage && !damage.IsDestroyed() && damage.GetHealthScaled() < 0.99 && Use(station, 1))
   {
    float repair = ACR_Structures.RepairFraction(station);
    damage.HealHitZones(damage.GetMaxHealth() * repair);
    damage.SetHealthScaled(Math.Min(1, damage.GetHealthScaled() + repair));
   }
  }
  if ((flags & ACR_EService.FUEL) != 0)
  {
   float fuel = Fuel(vehicle);
   SCR_FuelManagerComponent tanks = SCR_FuelManagerComponent.Cast(vehicle.FindComponent(SCR_FuelManagerComponent));
   if (tanks && fuel >= 0 && fuel < 0.98 && Use(station, 1))
    tanks.SetTotalFuelPercentage(Math.Min(1, fuel + 0.15));
  }
 }

 protected void Restock(IEntity station, string faction, int flags)
 {
  float capacity = Capacity(flags);
  float stock = StockOf(station);
  if (stock >= capacity) return;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bBuilt || base.m_bRetired || base.m_bContested || base.m_sOwner != faction || !ACR_LogisticsRules.CanDeploy(base)) continue;
   if (vector.DistanceXZ(base.GetOrigin(), station.GetOrigin()) > base.m_fRadius) continue;
   m_Stock.Set(station, Math.Min(capacity, stock + capacity * RESTOCK_PER_TICK));
   return;
  }
 }

 // Service vehicles of a faction offering this service, with stock left.
 void Stations(string faction, int service, notnull array<IEntity> result)
 {
  result.Clear();
  for (int i = 0; i < m_PointFlags.Count(); i++)
  {
   IEntity point = m_PointFlags.GetKey(i);
   if (point && (m_PointFlags.GetElement(i) & service) != 0 && m_PointFaction.Get(point) == faction && StockOf(point) > 0) result.Insert(point);
  }
  foreach (IEntity station : m_ServiceVehicles)
  {
   if ((ServiceOf(station) & service) == 0 || FactionOfVehicle(station) != faction) continue;
   if (StockOf(station) < 1) continue;
   result.Insert(station);
  }
 }

 int CountStations(string faction, int service)
 {
  int count;
  foreach (IEntity station : m_ServiceVehicles)
   if ((ServiceOf(station) & service) != 0 && FactionOfVehicle(station) == faction) count++;
  return count;
 }

 // Where idle service trucks wait at a base, and so where they count as parked (Jacob, 7 Nov:
 // Zub on Chernarus is a castle on a hilltop; US trucks were re-sent every 15 s for 34 min, when
 // 100-200 m east at the foot would have done). A vehicle's move ends at the road point nearest
 // its goal (ACR_Squad.RouteTo, BBB_Terrain.RoadPointNear: a road piece's own point); at Zub that
 // is 214 m east, 47 m down (srv:truckpark), so the trucks got there, but parked meant inside the
 // 45 m ring. The spot is one of those points within PARK_REACH of the centre: not steep, not on
 // water, and preferably not up on a hill over the ground round it, inside the ring, and on the
 // side away from the front ('away': a flat unit vector pointing away from it; zero for no
 // side). A flat town keeps the road point nearest its centre, as before. False: no road near.
 static const float PARK_REACH = 250;
 static const float PARK_SLOPE = 0.2;      // most rise per m, 6 m either way along both axes
 static const float PARK_HILL = 6;         // m over the mean ground 60 m round before it costs
 static const float PARK_HILL_COST = 20;   // score (m) per m higher than that
 static const float PARK_OUT_OF_RING = 40; // score (m) for a spot outside the base ring
 static const float PARK_MIN_WIDTH = 2.5;  // m: narrower pieces are paths
 static bool ParkSpot(vector centre, float radius, vector away, out vector spot)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return false;
  BaseWorld world = GetGame().GetWorld();
  array<BaseRoad> roads = {};
  aiWorld.GetRoadNetworkManager().GetRoadsInAABB(centre - Vector(PARK_REACH, 1000, PARK_REACH), centre + Vector(PARK_REACH, 1000, PARK_REACH), roads);
  float best = float.MAX;
  bool found;
  foreach (BaseRoad road : roads)
  {
   if (!road || road.GetWidth() < PARK_MIN_WIDTH) continue;
   array<vector> points = {};
   road.GetPoints(points);
   foreach (vector p : points)
   {
    float d = vector.DistanceXZ(p, centre);
    if (d > PARK_REACH) continue;
    float h = world.GetSurfaceY(p[0], p[2]);
    float rise = Math.Max(Math.AbsFloat(world.GetSurfaceY(p[0] + 6, p[2]) - h), Math.AbsFloat(world.GetSurfaceY(p[0] - 6, p[2]) - h));
    rise = Math.Max(rise, Math.Max(Math.AbsFloat(world.GetSurfaceY(p[0], p[2] + 6) - h), Math.AbsFloat(world.GetSurfaceY(p[0], p[2] - 6) - h)));
    if (rise / 6 > PARK_SLOPE || BBB_Terrain.IsDeepWaterAt(p)) continue;
    float score = d;
    if (d > radius) score += PARK_OUT_OF_RING;
    float hill = ACR_SiteScore.Elevation(p, 60) - PARK_HILL;
    if (hill > 0) score += hill * PARK_HILL_COST;
    float frontward = -((p[0] - centre[0]) * away[0] + (p[2] - centre[2]) * away[2]);
    if (frontward > 0) score += frontward;
    if (score >= best) continue;
    best = score;
    spot = ACR_Manager.Ground(p);
    found = true;
   }
  }
  return found;
 }
}
