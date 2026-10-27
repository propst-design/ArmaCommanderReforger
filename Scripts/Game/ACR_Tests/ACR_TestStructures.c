// -acrStructuresTest 1 (server-capable, ~2 min): player-built structures. Finished compositions
// are spawned on the test range (as if built by US players): a field hospital heals a wounded
// soldier, vehicle maintenance + fuel storage repair and refuel a parked Humvee, a helipad takes
// a bought helicopter, a headquarters away from the bases becomes a forward base where units can
// be bought; removing the structures undoes it.
class ACR_TestStructures
{
 static const ResourceName QUARTERS_SMALL = "{DB7F01C68EACBEC0}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_LivingArea_S_US_01.et";
 static const ResourceName QUARTERS_LARGE = "{74B740B56C82C0CF}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_LivingArea_L_US_01.et";
 static const ResourceName REPAIR_LIGHT = "{A0FEA3E718F5B605}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_VehicleMaintenance_S_Conflict_US_01.et";
 static IEntity s_QuartersSmall, s_QuartersLarge, s_RepairLight;
 static ACR_Base s_GuardBase;
 static const ResourceName HOSPITAL = "{D0281DAF0B131743}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_FieldHospital_M_Conflict_US_01.et";
 static const ResourceName REPAIR = "{7AD06D9CF2672AC5}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatMedium/E_VehicleMaintenance_M_Conflict_US_01.et";
 static const ResourceName FUEL = "{6D2A35DA74224780}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_FuelStorage_S_US_01.et";
 static const ResourceName AMMO = "{1BED8F81D2FA2137}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_AmmoStorage_S_US_01.et";
 static const ResourceName HELIPAD = "{D3DDB76548C16BDB}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatLarge/E_Helipad_L_Conflict_US_01.et";
 static const ResourceName HQ = "{16AF2FE47A6C297B}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_Headquarters_S_Conflict_US_01.et";
 static const ResourceName HUMVEE = "{4A71F755A4513227}Prefabs/Vehicles/Wheeled/M998/M1025.et";
 static const ResourceName CONSTRUCTION = "{2D74C39A650A3030}Prefabs/Vehicles/Wheeled/M923A1/M923A1_engineer.et";

 static IEntity s_Hospital, s_Repair, s_Fuel, s_Ammo, s_Helipad, s_HQ, s_Humvee, s_Construction;
 static IEntity s_Wounded;
 static ACR_Base s_Base;
 static vector s_Yard;
 static bool s_bHealed, s_bRepaired, s_bRefuelled;
 static ACR_Base s_Fob;
 static float s_EmptyTruckStock;
 static IEntity s_RejectedBuild;
 static IEntity s_Depot, s_ExtraDepot, s_FobFuel, s_FobVehicle;
 static SCR_AIGroup s_RPServiceGroup;

 static void CheckHousingAndRepair(ACR_Manager manager)
 {
  ACR_Base fob = s_Fob;
  ACR_FobProviderComponent provider = ACR_FobProviderComponent.Cast(fob.m_FobSupply.FindComponent(ACR_FobProviderComponent));
  CheckCatalog(provider, QUARTERS_SMALL);
  CheckCatalog(provider, QUARTERS_LARGE);
  ACR_TestRegress.Check("housing: outside quarters give no defenders", s_QuartersSmall && s_QuartersLarge && fob.m_iGarrison == 0, "");
  SCR_CampaignBuildingCompositionComponent small = SCR_CampaignBuildingCompositionComponent.Cast(s_QuartersSmall.FindComponent(SCR_CampaignBuildingCompositionComponent));
  SCR_CampaignBuildingCompositionComponent large = SCR_CampaignBuildingCompositionComponent.Cast(s_QuartersLarge.FindComponent(SCR_CampaignBuildingCompositionComponent));
  ACR_TestRegress.Check("housing: native costs are 275 and 475 supplies", small.ACR_CompositionCost() == 275 && large.ACR_CompositionCost() == 475, "");
  s_QuartersSmall.SetOrigin(fob.GetOrigin() + Vector(0, 0, 15));
  small.ACR_TestRelayCompletion(false);
  manager.m_Structures.UpdateHousing(fob);
  ACR_TestRegress.Check("housing: unfinished quarters give no defenders", fob.m_iGarrison == 0 && fob.m_iGarrisonLeft == 0, "");
  small.ACR_TestRelayCompletion(true);
  manager.m_Structures.UpdateHousing(fob);
  ACR_TestRegress.Check("housing: small quarters add eleven defenders", fob.m_iGarrison == 11 && fob.m_iGarrisonLeft == 11, "");
  s_QuartersLarge.SetOrigin(fob.GetOrigin() + Vector(0, 0, -15));
  manager.m_Structures.UpdateHousing(fob);
  ACR_TestRegress.Check("housing: large quarters add nineteen defenders", fob.m_iGarrison == 30 && fob.m_iGarrisonLeft == 30, "");
  manager.m_Structures.UpdateHousing(fob);
  ACR_TestRegress.Check("housing: repeated updates never duplicate defenders", fob.m_iGarrison == 30 && fob.m_iGarrisonLeft == 30, "");
  fob.m_iGarrisonLeft = 23;
  manager.m_Structures.UpdateHousing(fob);
  ACR_TestRegress.Check("housing: surviving quarters do not replenish casualties", fob.m_iGarrisonLeft == 23, "");
  manager.m_Bases.DebugCapture(manager, fob, "USSR");
  ACR_TestRegress.Check("housing: capture removes enemy housing bonus", fob.m_iGarrison == 0 && fob.m_iGarrisonLeft == 0, "");
  manager.m_Bases.DebugCapture(manager, fob, "US");
  ACR_TestRegress.Check("housing: recapture uses normal half-garrison rule", fob.m_iGarrison == 30 && fob.m_iGarrisonLeft == 15, "");
  // Captures hand the FOB's buildings over; put the relay fixture's enemy antenna back.
  ACR_TestFobRelay.RestoreEnemySide(manager);
  SCR_EntityHelper.DeleteEntityAndChildren(s_QuartersLarge);
  manager.m_Structures.UpdateHousing(fob);
  ACR_TestRegress.Check("housing: losing large quarters clamps survivors to eleven", fob.m_iGarrison == 11 && fob.m_iGarrisonLeft == 11, "");
  SCR_EntityHelper.DeleteEntityAndChildren(s_QuartersSmall);
  manager.m_Structures.UpdateHousing(fob);
  ACR_TestRegress.Check("housing: losing all quarters removes the garrison", fob.m_iGarrison == 0 && fob.m_iGarrisonLeft == 0, "");

  // Exercise the real repair and spending path on one vehicle, with identical damage.
  IEntity vehicle = Place(HUMVEE, fob.GetOrigin() + Vector(0, 0, 60));
  SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(vehicle.FindComponent(SCR_DamageManagerComponent));
  SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(fob.m_FobSupply);
  vector repairOrigin = s_Repair.GetOrigin();
  s_Repair.SetOrigin(fob.GetOrigin() + Vector(12, 0, 0));
  s_RepairLight.SetOrigin(fob.GetOrigin() + Vector(-12, 0, 0));
  ACR_Options.Set("acrFobServices", "supplies");
  damage.FullHeal();
  damage.SetHealthScaled(0.2);
  store.SetResourceValue(10);
  manager.m_Logistics.ACR_TestRepairStep(s_RepairLight, vehicle, 10000000);
  float lightGain = damage.GetHealthScaled() - 0.2;
  int lightCost = 10 - store.GetResourceValue();
  damage.FullHeal();
  damage.SetHealthScaled(0.2);
  store.SetResourceValue(10);
  manager.m_Logistics.ACR_TestRepairStep(s_Repair, vehicle, 10000000);
  float heavyGain = damage.GetHealthScaled() - 0.2;
  ACR_TestRegress.Check("repair: heavy bay delivers forty percent more repair", lightGain > 0 && Math.AbsFloat(heavyGain - lightGain * 1.4) < 0.005, string.Format("light %1 heavy %2", lightGain, heavyGain));
  ACR_TestRegress.Check("repair: light and heavy steps both cost ten supplies", lightCost == 10 && store.GetResourceValue() == 0, "");
  store.SetResourceValue(10);
  float health = damage.GetHealthScaled();
  manager.m_Logistics.ACR_TestRepairStep(s_RepairLight, vehicle, 10005000);
  manager.m_Logistics.ACR_TestRepairStep(s_Repair, vehicle, 10005000);
  ACR_TestRegress.Check("repair: both bays retain ten second cooldown", damage.GetHealthScaled() == health && store.GetResourceValue() == 10, "");
  store.SetResourceValue(0);
  manager.m_Logistics.ACR_TestRepairStep(s_Repair, vehicle, 10020000);
  ACR_TestRegress.Check("repair: heavy bay cannot repair without supplies", damage.GetHealthScaled() == health, "");
  store.SetResourceValue(1000);
  ACR_Options.Set("acrFobServices", "free");
  s_Repair.SetOrigin(repairOrigin);
  SCR_EntityHelper.DeleteEntityAndChildren(s_RepairLight);
  SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = ACR_Manager.Ground(fob.GetOrigin() + Vector(80, 0, 0));
  s_GuardBase = ACR_Base.Cast(GetGame().SpawnEntity(ACR_Base, GetGame().GetWorld(), parameters));
  // Isolate the live-group fixture from automatic packing/capture ticks.
  ACR_Base.s_All.RemoveItem(s_GuardBase);
  s_GuardBase.m_sOwner = "US";
  s_GuardBase.m_sName = "Housing test garrison";
  s_GuardBase.m_fRadius = 40;
  s_GuardBase.m_iGarrison = 11;
  s_GuardBase.m_iGarrisonLeft = 11;
  manager.m_Bases.ACR_TestSpawnGuards(s_GuardBase);
 }

 static int GuardCount()
 {
  int count;
  array<AIAgent> agents = {};
  if (s_GuardBase.m_GarrisonGroup) s_GuardBase.m_GarrisonGroup.GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (ACR_Respawn.IsAlive(agent.GetControlledEntity())) count++;
  return count;
 }

 static IEntity EngineeringTable(IEntity entity)
 {
  if (!entity) return null;
  if (entity.FindComponent(SCR_CampaignBuildingProviderComponent)) return entity;
  for (IEntity child = entity.GetChildren(); child; child = child.GetSibling())
  {
   IEntity found = EngineeringTable(child);
   if (found) return found;
  }
  return null;
 }

 static void CheckCatalog(ACR_FobProviderComponent provider, ResourceName prefab)
 {
  SCR_EditableEntityUIInfo info = SCR_EditableEntityUIInfo.ExtractEditableUIInfoFromPrefab(prefab);
  array<EEditableEntityLabel> labels = {};
  if (info) info.GetEntityLabels(labels);
  bool offered;
  foreach (EEditableEntityLabel trait : provider.GetAvailableTraits())
   if (labels.Contains(trait)) offered = true;
  ACR_TestRegress.Check("FOB: service catalog " + FilePath.StripPath(prefab), offered && labels.Contains(EEditableEntityLabel.FACTION_US), "");
 }

 static float NativeCapacity(IEntity entity)
 {
  if (!entity) return 0;
  SCR_ResourceComponent component = SCR_ResourceComponent.Cast(entity.FindComponent(SCR_ResourceComponent));
  SCR_ResourceContainer container;
  if (component) container = component.GetContainer(EResourceType.SUPPLIES);
  if (container && container.GetMaxResourceValue() > 0) return container.GetMaxResourceValue();
  float total;
  for (IEntity child = entity.GetChildren(); child; child = child.GetSibling()) total += NativeCapacity(child);
  return total;
 }

 static void CheckSupplies(ACR_Manager manager, ACR_Base fob)
 {
  ACR_TestRegress.Check("FOB: native command post storage capacity", NativeCapacity(s_HQ) == 1000, NativeCapacity(s_HQ).ToString());
  if (!fob || !fob.m_FobSupply) { ACR_TestRegress.Abort("structures", "FOB supply provider missing"); return; }
  s_Fob = fob;
  SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(fob.m_FobSupply);
  ACR_TestRegress.Check("FOB: starts empty with 1000 capacity", store && ACR_FobSupply.Stock(fob.m_FobSupply) == 0 && store.GetMaxResourceValue() == 1000, "");
  ACR_FobProviderComponent provider = ACR_FobProviderComponent.Cast(fob.m_FobSupply.FindComponent(ACR_FobProviderComponent));
  ACR_TestRegress.Check("FOB: paid balance is physical CP storage", store && store == ACR_FobSupply.CommandPostStore(s_HQ), "");
  IEntity table = EngineeringTable(s_HQ);
  ACR_TestRegress.Check("FOB: physical engineering table opens paid provider", table && ACR_FobSupply.EditorProvider(table) == fob.m_FobSupply, "");
  SCR_CampaignBuildingProviderComponent tableProvider;
  if (table) tableProvider = SCR_CampaignBuildingProviderComponent.Cast(table.FindComponent(SCR_CampaignBuildingProviderComponent));
  ACR_TestRegress.Check("FOB: physical table uses paid resource component", tableProvider && tableProvider.GetResourceComponent() == provider.GetResourceComponent(), "");
  CheckCatalog(provider, HOSPITAL);
  CheckCatalog(provider, REPAIR);
  CheckCatalog(provider, FUEL);
  CheckCatalog(provider, AMMO);
  CheckCatalog(provider, HELIPAD);
  CheckCatalog(provider, ACR_TestFobRelay.RELAY);
  CheckCatalog(provider, "{94D9E9B690F98756}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_US_01.et");
  SCR_CampaignBuildingCompositionComponent hospital = SCR_CampaignBuildingCompositionComponent.Cast(s_Hospital.FindComponent(SCR_CampaignBuildingCompositionComponent));
  SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(s_Hospital.FindComponent(SCR_EditableEntityComponent));
  array<ref SCR_EntityBudgetValue> budgets = {};
  if (editable) editable.GetEntityBudgetCost(budgets, s_Hospital);
  ACR_TestRegress.Check("FOB: empty store refuses real hospital budget", provider && !provider.IsThereEnoughBudgetToSpawn(budgets), "");
  int rp = manager.m_Bases.GetRP("US");
  ACR_FobSupply.Order(manager, "USSR", fob);
  ACR_TestRegress.Check("FOB: enemy request cannot spend RP", manager.m_Bases.GetRP("US") == rp && fob.m_iSupplyDelivery == 0, "");
  manager.m_Bases.GetFaction("US").m_iRP = 0;
  ACR_FobSupply.Order(manager, "US", fob);
  ACR_TestRegress.Check("FOB: unaffordable delivery refused", fob.m_iSupplyDelivery == 0, "");
  manager.m_Bases.GetFaction("US").m_iRP = rp;
  fob.m_bContested = true;
  ACR_FobSupply.Order(manager, "US", fob);
  ACR_TestRegress.Check("FOB: contested order refused without charge", manager.m_Bases.GetRP("US") == rp && fob.m_iSupplyDelivery == 0, "");
  fob.m_bContested = false;
  ACR_FobSupply.Order(manager, "US", fob);
  ACR_FobSupply.Order(manager, "US", fob);
  ACR_TestRegress.Check("FOB: one 500-supply delivery costs 20 RP once", manager.m_Bases.GetRP("US") == rp - 20 && fob.m_iSupplyDelivery == 500 && ACR_FobSupply.Stock(fob.m_FobSupply) == 0, "");
  ACR_FobSupply.Advance(fob, 59000);
  ACR_TestRegress.Check("FOB: delivery does not arrive early", ACR_FobSupply.Stock(fob.m_FobSupply) == 0 && fob.m_fSupplyDeliveryLeft == 1000, "");
  fob.m_bContested = true;
  ACR_FobSupply.Advance(fob, 120000);
  ACR_TestRegress.Check("FOB: contest pauses delivery and building", fob.m_fSupplyDeliveryLeft == 1000 && !ACR_FobSupply.SpendBuild(fob.m_FobSupply, 1), "");
  fob.m_bContested = false;
  ACR_FobSupply.Advance(fob, 1000);
  ACR_TestRegress.Check("FOB: delivery arrives after 60 uncontested seconds", ACR_FobSupply.Stock(fob.m_FobSupply) == 500 && fob.m_iSupplyDelivery == 0, "");
  ACR_TestRegress.Check("FOB: delivery fills native CP and engineering counter", ACR_FobSupply.CommandPostStore(s_HQ).GetResourceValue() == 500 && tableProvider.GetResourceComponent().GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES).GetAggregatedResourceValue() == 500, "");
  ACR_TestRegress.Check("FOB: funded store accepts real hospital budget", provider.IsThereEnoughBudgetToSpawn(budgets), "");
  provider.ClearAccumulatedBudgetChanges();
  // Real blueprint provider assignment consumes stock, completion never charges it again.
  IEntity built = Place(REPAIR, fob.GetOrigin() + Vector(12, 0, 0));
  SCR_CampaignBuildingCompositionComponent composition;
  if (built) composition = SCR_CampaignBuildingCompositionComponent.Cast(built.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (composition) { composition.SetProviderEntity(fob.m_FobSupply); composition.ACR_ChargeFobBuild(); composition.ACR_OnCompositionSpawned(true); composition.SetProviderEntity(fob.m_FobSupply); composition.ACR_ChargeFobBuild(); }
  ACR_TestRegress.Check("FOB: real build paid once from store", composition && ACR_FobSupply.Stock(fob.m_FobSupply) == 500 - composition.GetCompositionCost(), "");
  ACR_TestRegress.Check("FOB: cannot overspend store", !ACR_FobSupply.SpendBuild(fob.m_FobSupply, 2000), "");
  store.SetResourceValue(0);
  s_RejectedBuild = Place(FUEL, fob.GetOrigin() + Vector(-12, 0, 0));
  SCR_CampaignBuildingCompositionComponent rejected;
  if (s_RejectedBuild) rejected = SCR_CampaignBuildingCompositionComponent.Cast(s_RejectedBuild.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (rejected) { rejected.SetProviderEntity(fob.m_FobSupply); rejected.ACR_ChargeFobBuild(); }
  ACR_TestRegress.Check("FOB: unpaid real blueprint rejected before activation", rejected && rejected.ACR_BuildRejected() && ACR_FobSupply.Stock(fob.m_FobSupply) == 0, "");
  store.SetResourceValue(1000);
  SCR_CampaignBuildingCompositionComponent existing = SCR_CampaignBuildingCompositionComponent.Cast(s_Fuel.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (existing) existing.SetProviderEntity(fob.m_FobSupply);
  ACR_TestRegress.Check("FOB: associating existing buildings does not charge stock", existing && !existing.ACR_BuildRejected() && ACR_FobSupply.Stock(fob.m_FobSupply) == 1000, "");
  if (existing) existing.SetProviderEntity(null);
  int beforeFull = manager.m_Bases.GetRP("US");
  ACR_FobSupply.Order(manager, "US", fob);
  ACR_TestRegress.Check("FOB: full store refuses delivery without charge", manager.m_Bases.GetRP("US") == beforeFull && fob.m_iSupplyDelivery == 0, "");
  SCR_ResourceConsumer loader = ACR_Logistics.SupplyContainer(s_Construction).GetComponent().GetConsumer(EResourceGeneratorID.VEHICLE_LOAD, EResourceType.SUPPLIES);
  ACR_TestRegress.Check("FOB: paid store forbids native truck loading", loader && !store.IsAllowed(loader), "");
  // An emptied truck inside the FOB must stay empty across real logistics ticks.
  manager.m_Logistics.Use(s_Construction, manager.m_Logistics.StockOf(s_Construction));
  s_Construction.SetOrigin(fob.GetOrigin() + Vector(0, 0, 18));
  s_EmptyTruckStock = manager.m_Logistics.StockOf(s_Construction);
  if (built) SCR_EntityHelper.DeleteEntityAndChildren(built);
 }

 static IEntity Place(ResourceName prefab, vector at)
 {
  IEntity entity = ACR_Manager.SpawnPrefab(prefab, at);
  PrintFormat("[ACR] STRUCTURETEST placed %1 at %2: %3", FilePath.StripPath(prefab), at, entity != null);
  return entity;
 }

 static int DistanceTo(IEntity a, IEntity b)
 {
  if (!a || !b) return -1;
  return vector.DistanceXZ(a.GetOrigin(), b.GetOrigin());
 }

 static bool InsideABase(vector at, float margin)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (vector.DistanceXZ(base.GetOrigin(), at) <= base.m_fRadius + margin) return true;
  return false;
 }

 static ACR_Base BaseNamed(string prefix)
 {
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_sName.StartsWith(prefix)) return base;
  return null;
 }

 static int EntryIndex(ACR_Manager manager, bool aircraft)
 {
  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (entry.m_iSupportHeli != 0 || entry.m_bMortarTeam) continue;
   if (aircraft && entry.m_bAircraft && !entry.m_sVehicle.IsEmpty()) return i;
   if (!aircraft && !entry.m_bAircraft && entry.m_sVehicle.IsEmpty() && !entry.m_sGroup.IsEmpty()) return i;
  }
  return -1;
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   // The original service fixtures are outside any FOB; test the optional legacy mode.
   ACR_Options.Set("acrFobServices", "free");
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
  }
  if (tick <= 10 && (!alpha || !alpha.GetAnchor())) return;
  // The live sapper's cook-off may take Alpha with it; the final checks need no Alpha.
  if (!alpha && tick <= 94) return;
  if (tick == 3)
  {
   float best = float.MAX;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner != "US") continue;
    float d = vector.DistanceXZ(base.GetOrigin(), alpha.m_Home);
    if (d < best) { best = d; s_Base = base; }
   }
   manager.m_Bases.GetFaction("US").m_iRP = 1000;
   vector home = alpha.GetAnchor().GetOrigin();
   // Field hospital beside Alpha; one of Alpha's soldiers is wounded.
   s_Hospital = Place(HOSPITAL, home + Vector(21, 0, 0));
   array<AIAgent> agents = {};
   alpha.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (!soldier) continue;
    SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(soldier.FindComponent(SCR_CharacterDamageManagerComponent));
    if (!damage) continue;
    HitZone zone = damage.GetDefaultHitZone();
    if (zone) zone.SetHealthScaled(0.5);
    // This fixture checks stationary hospital service, not combat movement.
    // Native reactions could walk the patient away before the dwell completed.
    AIControlComponent patientAI = AIControlComponent.Cast(soldier.FindComponent(AIControlComponent));
    if (patientAI) patientAI.DeactivateAI();
    soldier.SetOrigin(ACR_Manager.Ground(s_Hospital.GetOrigin() + Vector(4, 0, 0)));
    s_Wounded = soldier;
    break;
   }
   PrintFormat("[ACR] STRUCTURETEST wounded soldier health %1", ACR_Logistics.Health(s_Wounded));
   // A yard 120 m off: vehicle maintenance, fuel and ammo storage and a damaged, nearly dry Humvee.
   s_Yard = ACR_Manager.Ground(home + Vector(-120, 0, 40));
   s_Repair = Place(REPAIR, s_Yard + Vector(14, 0, 0));
   s_Fuel = Place(FUEL, s_Yard + Vector(-12, 0, 0));
   s_Construction = Place(CONSTRUCTION, s_Yard + Vector(35, 0, 0));
   if (s_Construction) ACR_Vehicles.SetOwner(s_Construction, "US");
   // (No ammo storage: its vanilla resupply action logs a script error on a dedicated server,
   // which ends the run; it is registered the same way as fuel storage.)
   s_Humvee = Place(HUMVEE, s_Yard);
   if (s_Humvee)
   {
    ACR_Vehicles.SetOwner(s_Humvee, "US");
    SCR_DamageManagerComponent vdamage = SCR_DamageManagerComponent.Cast(s_Humvee.FindComponent(SCR_DamageManagerComponent));
    if (vdamage) vdamage.SetHealthScaled(0.4);
    SCR_FuelManagerComponent tanks = SCR_FuelManagerComponent.Cast(s_Humvee.FindComponent(SCR_FuelManagerComponent));
    if (tanks) tanks.SetTotalFuelPercentage(0.2);
    PrintFormat("[ACR] STRUCTURETEST humvee health %1 fuel %2", ACR_Logistics.Health(s_Humvee), ACR_Logistics.Fuel(s_Humvee));
   }
   // Helipad at the base.
   vector padSpot;
   bool clearPadSpot;
   array<IEntity> padVehicles = {};
   ACR_Vehicles.All(padVehicles);
   // Terrain clearance does not exclude live vehicles. ClearPad rejects one
   // within 10 m, so keep this fixture 14 m clear of the range truck/Humvee.
   for (int padTry = 0; padTry < 32; padTry++)
   {
    vector padCandidate = manager.ClearSpotNear(s_Base, 30, 70, 14);
    bool padBlocked;
    foreach (IEntity padVehicle : padVehicles)
     if (padVehicle && vector.DistanceXZ(padVehicle.GetOrigin(), padCandidate) < 14) { padBlocked = true; break; }
    if (padBlocked) continue;
    padSpot = padCandidate;
    clearPadSpot = true;
    break;
   }
   ACR_TestRegress.Check("structures: helipad fixture is clear of existing vehicles", clearPadSpot);
   if (!clearPadSpot) { ACR_TestRegress.Abort("structures", "no unoccupied helipad fixture site"); return; }
   s_Helipad = Place(HELIPAD, padSpot);
   // Headquarters away from every base.
   vector hqSpot;
   for (int i = 0; i < 24; i++)
   {
    float angle = i * 15 * Math.DEG2RAD;
    vector candidate = ACR_Manager.Ground(home + Vector(Math.Cos(angle) * 450, 0, Math.Sin(angle) * 450));
    if (!InsideABase(candidate, 60) && !BBB_Terrain.IsWaterAt(candidate)) { hqSpot = candidate; break; }
   }
   s_HQ = Place(HQ, hqSpot);
   s_QuartersSmall = Place(QUARTERS_SMALL, hqSpot + Vector(90, 0, 0));
   s_QuartersLarge = Place(QUARTERS_LARGE, hqSpot + Vector(-90, 0, 0));
   s_RepairLight = Place(REPAIR_LIGHT, hqSpot + Vector(0, 0, 90));
   ACR_TestFobRelay.Spawn(hqSpot);
  }
  if (tick == 8)
  {
   ACR_TestRegress.Check("structures: helipad registered as a US pad", ACR_Helipad.s_All.Count() > 0 && ACR_Helipad.s_All[ACR_Helipad.s_All.Count() - 1].Faction() == "US", string.Format("pads %1", ACR_Helipad.s_All.Count()));
   array<IEntity> stations = {};
   manager.m_Logistics.Stations("US", ACR_EService.MEDICAL, stations);
   ACR_TestRegress.Check("structures: field hospital is a medical point", stations.Contains(s_Hospital), string.Format("%1 medical stations", stations.Count()));
   manager.m_Logistics.Stations("US", ACR_EService.FUEL, stations);
   ACR_TestRegress.Check("structures: fuel storage is a fuel point", stations.Contains(s_Fuel), string.Format("%1 fuel stations", stations.Count()));
   SCR_CampaignBuildingCompositionComponent hospital = SCR_CampaignBuildingCompositionComponent.Cast(s_Hospital.FindComponent(SCR_CampaignBuildingCompositionComponent));
   float constructionStock = manager.m_Logistics.StockOf(s_Construction);
   if (hospital) hospital.ACR_SetProviderForTest(s_Construction);
   if (hospital) hospital.ACR_OnCompositionSpawned(true);
   if (hospital) hospital.ACR_OnCompositionSpawned(true);
   float afterConstructionStock = manager.m_Logistics.StockOf(s_Construction);
   ACR_TestRegress.Check("structures: completed build charges its truck once", hospital && afterConstructionStock < constructionStock && Math.AbsFloat((constructionStock - afterConstructionStock) - 10) < 0.01, string.Format("stock %1 -> %2 loads", constructionStock, afterConstructionStock));
   ACR_TestRegress.Check("structures: an ammo storage counts as ammo", ACR_Structures.ServiceOf(ACR_Structures.KindOfName(AMMO)) == ACR_EService.AMMO, "");
   ACR_Base fob = BaseNamed("FOB");
   ACR_TestRegress.Check("structures: headquarters away from bases = a forward base", fob && fob.m_sOwner == "US" && fob.m_bAllowSpawn, "");
   if (fob)
   {
    CheckSupplies(manager, fob);
    ACR_TestLogisticsRules.Run(manager, s_Base, fob, EntryIndex(manager, false));
    s_Depot = Place("{94D9E9B690F98756}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_US_01.et", fob.GetOrigin() + Vector(20, 0, 0));
    s_ExtraDepot = Place("{94D9E9B690F98756}PrefabsEditable/Auto/Compositions/Slotted/SlotFlatSmall/E_SupplyCache_S_US_01.et", fob.GetOrigin() + Vector(0, 0, 25));
    s_FobFuel = Place(FUEL, fob.GetOrigin() + Vector(-16, 0, 0));
    s_RPServiceGroup = SCR_AIGroup.Cast(Place("{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et", fob.GetOrigin() + Vector(0, 0, -18)));
    string bought = manager.PurchaseAt("US", EntryIndex(manager, false), fob);
    ACR_TestRegress.Check("structures: units can be bought at the forward base", bought.Contains("deploying at " + fob.m_sName), bought);
   }
   int heli = EntryIndex(manager, true);
   if (heli >= 0)
   {
    ACR_Helipad pad = ACR_Helipad.s_All[ACR_Helipad.s_All.Count() - 1];
    string boughtHeli = manager.PurchaseAt("US", heli, pad.Base());
    PrintFormat("[ACR] STRUCTURETEST bought helicopter at %1: %2", pad.Base().m_sName, boughtHeli);
    GetGame().GetCallqueue().CallLater(CheckHeliOnPad, 3000, false, pad);
   }
   else Print("[ACR] STRUCTURETEST no helicopter in the US buy list");
  }
  if (tick == 12) CheckHousingAndRepair(manager);
  if (tick > 12 && tick < 26) ACR_Base.s_All.RemoveItem(s_GuardBase);
  if (tick == 16)
  {
   ACR_TestRegress.Check("housing: eleven real defenders deploy", GuardCount() == 11, GuardCount().ToString());
   manager.m_Bases.ResizeGarrison(s_GuardBase, 19);
  }
  if (tick == 20)
  {
   ACR_TestRegress.Check("housing: live garrison gains nineteen real defenders", GuardCount() == 30, GuardCount().ToString());
   array<AIAgent> guards = {};
   s_GuardBase.m_GarrisonGroup.GetAgents(guards);
   foreach (AIAgent guard : guards)
   {
    if (!ACR_Respawn.IsAlive(guard.GetControlledEntity())) continue;
    SCR_EntityHelper.DeleteEntityAndChildren(guard.GetControlledEntity());
    break;
   }
  }
  if (tick == 22)
  {
   manager.m_Bases.ResizeGarrison(s_GuardBase, 0);
   ACR_TestRegress.Check("housing: live casualties persist without replenishment", GuardCount() == 29 && s_GuardBase.m_iGarrisonLeft == 29, GuardCount().ToString());
   manager.m_Bases.ResizeGarrison(s_GuardBase, -19);
  }
  if (tick == 26)
  {
   ACR_TestRegress.Check("housing: capacity loss recalls excess real defenders", GuardCount() == 11 && s_GuardBase.m_iGarrison == 11, GuardCount().ToString());
   manager.m_Bases.ACR_TestPackGuards(s_GuardBase);
   ACR_TestRegress.Check("housing: packing preserves surviving defenders", !s_GuardBase.m_GarrisonGroup && s_GuardBase.m_iGarrisonLeft == 11, "");
   s_GuardBase.Retire("fixture complete");
   SCR_EntityHelper.DeleteEntityAndChildren(s_GuardBase);
  }
  if (tick == 18)
  {
   ACR_TestRegress.Check("FOB: rejected blueprint removed", s_RejectedBuild == null, "");
   ACR_TestRegress.Check("FOB: no free truck refill during real logistics ticks", s_Construction && manager.m_Logistics.StockOf(s_Construction) == s_EmptyTruckStock && ACR_Logistics.GameSupplies(s_Construction) == 0, string.Format("stock %1 / game %2", manager.m_Logistics.StockOf(s_Construction), ACR_Logistics.GameSupplies(s_Construction)));
   SCR_ResourceComponent supply = SCR_ResourceComponent.Cast(s_Fob.m_FobSupply.FindComponent(SCR_ResourceComponent));
   SCR_ResourceConsumer consumer = supply.GetConsumer(EResourceGeneratorID.DEFAULT, EResourceType.SUPPLIES);
   string supplyDetail = "no default construction consumer";
   if (consumer) supplyDetail = string.Format("supplies %1 / %2", consumer.GetAggregatedResourceValue(), consumer.GetAggregatedMaxResourceValue());
   ACR_TestRegress.Check("FOB: building UI sees paid stock after resource update", consumer && consumer.GetAggregatedResourceValue() == 1000 && consumer.GetAggregatedMaxResourceValue() == 3000, supplyDetail);
   ACR_TestRegress.Check("logistics: completed depot expands actual FOB capacity", ACR_FobSupply.CapacityOf(s_Fob) == 3000, ACR_FobSupply.CapacityOf(s_Fob).ToString());
   ACR_TestRegress.Check("logistics: multiple depots do not stack storage bonuses", s_Depot && s_ExtraDepot && manager.m_Structures.CountActive(ACR_EStructure.DEPOT, "US") == 2 && ACR_FobSupply.CapacityOf(s_Fob) == 3000, "");
   ACR_Options.Set("acrFobServices", "supplies");
   SCR_ResourceContainer serviceStore = ACR_Logistics.SupplyContainer(s_Fob.m_FobSupply);
   serviceStore.SetResourceValue(20);
   ACR_TestRegress.Check("logistics: service charges actual FOB supplies", manager.m_Logistics.Use(s_FobFuel, 1) && ACR_FobSupply.Stock(s_Fob.m_FobSupply) == 10, "");
   serviceStore.SetResourceValue(0);
   ACR_TestRegress.Check("logistics: empty FOB refuses service", !manager.m_Logistics.Use(s_FobFuel, 1), "");
   ACR_TestRegress.Check("logistics: service outside a FOB cannot bypass supply cost", !manager.m_Logistics.Use(s_Fuel, 1), "");
   ACR_Squad rpSquad = manager.FindSquadByGroup(s_RPServiceGroup);
   if (!rpSquad || !rpSquad.GetAnchor()) { ACR_TestRegress.Abort("structures", "RP service squad missing"); return; }
   SCR_CharacterDamageManagerComponent rpWounds = SCR_CharacterDamageManagerComponent.Cast(rpSquad.GetAnchor().FindComponent(SCR_CharacterDamageManagerComponent));
   if (rpWounds) rpWounds.GetDefaultHitZone().SetHealthScaled(0.5);
   ACR_TestFobRelay.Guards(manager, s_Fob, rpSquad, EntryIndex(manager, false));
   int rpBefore = manager.m_Bases.GetRP("US");
   int instantServed = manager.m_Requisition.RestockAmmo(rpSquad, s_Fob);
   ACR_TestRegress.Check("logistics: RP fallback heals with an empty supply store", instantServed > 0 && ACR_Logistics.Health(rpSquad.GetAnchor()) > 0.95 && ACR_FobSupply.Stock(s_Fob.m_FobSupply) == 0, "");
   ACR_TestRegress.Check("logistics: RP fallback charges only soldiers actually serviced", rpBefore - manager.m_Bases.GetRP("US") == instantServed * 3, "");
   SCR_EntityHelper.DeleteEntityAndChildren(s_RPServiceGroup);
   ACR_TestFobRelay.RemoveAntenna(s_Fob);
   serviceStore.SetResourceValue(2500);
   ACR_FobSupply.Order(manager, "US", s_Fob);
   if (s_Depot) SCR_EntityHelper.DeleteEntityAndChildren(s_Depot);
   if (s_ExtraDepot) SCR_EntityHelper.DeleteEntityAndChildren(s_ExtraDepot);
   ACR_Options.Set("acrFobServices", "free");
  }
  if (tick == 23)
  {
   ACR_TestRegress.Check("logistics: depot loss restores capacity and removes excess stock", ACR_FobSupply.CapacityOf(s_Fob) == 1000 && ACR_FobSupply.Stock(s_Fob.m_FobSupply) == 1000, "");
   ACR_FobSupply.Advance(s_Fob, 60000);
   ACR_TestRegress.Check("logistics: paid delivery waits for space after depot loss", s_Fob.m_iSupplyDelivery == 500 && ACR_FobSupply.Stock(s_Fob.m_FobSupply) == 1000, "");
   ACR_Logistics.SupplyContainer(s_Fob.m_FobSupply).SetResourceValue(0);
   ACR_FobSupply.Advance(s_Fob, 0);
   ACR_TestRegress.Check("logistics: held delivery arrives once storage is free", s_Fob.m_iSupplyDelivery == 0 && ACR_FobSupply.Stock(s_Fob.m_FobSupply) == 500, "");
   ACR_TestFobRelay.Rebuild(manager, s_Fob, s_FobFuel);
   // Actual parked vehicle service, with finite supplies and no squad/player aboard.
   ACR_Options.Set("acrFobServices", "supplies");
   s_FobVehicle = Place(HUMVEE, s_Fob.GetOrigin() + Vector(-12, 0, 0));
   ACR_Vehicles.SetOwner(s_FobVehicle, "US");
   SCR_FuelManagerComponent fobTanks = SCR_FuelManagerComponent.Cast(s_FobVehicle.FindComponent(SCR_FuelManagerComponent));
   if (fobTanks) fobTanks.SetTotalFuelPercentage(0.2);
   ACR_Logistics.SupplyContainer(s_Fob.m_FobSupply).SetResourceValue(20);
  }
  if (tick == 28) ACR_TestFobRelay.CheckRebuilt(s_Fob);
  if (tick == 48)
  {
   ACR_TestRegress.Check("logistics: native FOB fuel service consumes finite supply", ACR_Logistics.Fuel(s_FobVehicle) > 0.25 && ACR_FobSupply.Stock(s_Fob.m_FobSupply) < 20, string.Format("fuel %1 stock %2", ACR_Logistics.Fuel(s_FobVehicle), ACR_FobSupply.Stock(s_Fob.m_FobSupply)));
   ACR_TestRegress.Check("logistics: empty fuel store stops further service", ACR_Logistics.Fuel(s_FobVehicle) < 0.6 && ACR_FobSupply.Stock(s_Fob.m_FobSupply) == 0, "");
   if (s_FobFuel) SCR_EntityHelper.DeleteEntityAndChildren(s_FobFuel);
   ACR_Options.Set("acrFobServices", "free");
  }
  if (tick == 50)
  {
   // The relay fixtures stand inside FOB 1 and would count as its relay slot (smart slots).
   ACR_TestFobRelay.Cleanup();
   ACR_FobSlots.Cleanup(s_Fob);
   manager.m_Structures.ACR_TestForceTick();
   ACR_TestFobBuild.Run(manager, alpha, s_Construction);
   // Legacy slot fixtures sit on unprepared test terrain: spot checks have their own test.
   ACR_FobSlots.s_bSkipSpotChecksForTest = true;
   ACR_TestFobSlots.Run(manager, s_Fob, alpha, s_Construction);
   ACR_TestFobUpgrades.Run(manager, s_Fob, alpha, s_Construction);
   ACR_FobSlots.s_bSkipSpotChecksForTest = false;
   ACR_TestFobDefences.Run(manager, s_Fob, alpha);
   ACR_TestSapperDemo.Run(manager, s_Fob, alpha);
   ACR_TestFobIntel.Run(manager, s_Fob);
   ACR_TestFobPatrol.Run(manager, s_Fob);
   ACR_Supports.s_bCookOffOff = true; // its charges go off next to the other fixtures
   ACR_TestFobDesign.Run(manager, s_Fob, alpha, s_Construction);
   ACR_Supports.s_bCookOffOff = false;
   ACR_TestFobBuiltRace.Run(manager, alpha, s_Construction);
   ACR_TestSapperLive.Start(manager, alpha, alpha.m_Home);
  }
  if (tick > 50 && tick <= 94) ACR_TestSapperLive.Step(manager, tick);
  if (tick > 8 && tick % 5 == 0)
   PrintFormat("[ACR] STRUCTURETEST t=%1 wounded %2 (%5 m from the hospital), humvee health %3 fuel %4", tick, ACR_Logistics.Health(s_Wounded), ACR_Logistics.Health(s_Humvee), ACR_Logistics.Fuel(s_Humvee), DistanceTo(s_Wounded, s_Hospital));
  if (!s_bHealed && s_Wounded && ACR_Logistics.Health(s_Wounded) > 0.95 && tick > 8)
  {
   s_bHealed = true;
   ACR_TestRegress.Check("structures: the field hospital healed the wounded soldier after a stay", tick >= 22, string.Format("t=%1", tick));
  }
  if (!s_bRepaired && s_Humvee && ACR_Logistics.Health(s_Humvee) > 0.95 && tick > 8)
  {
   s_bRepaired = true;
   ACR_TestRegress.Check("structures: vehicle maintenance repaired the Humvee slowly", tick >= 30, string.Format("t=%1", tick));
  }
  if (!s_bRefuelled && s_Humvee && ACR_Logistics.Fuel(s_Humvee) > 0.9 && tick > 8)
  {
   s_bRefuelled = true;
   ACR_TestRegress.Check("structures: fuel storage refuelled the Humvee", true, string.Format("t=%1", tick));
  }
  // The finite-FOB fixture temporarily disables these outside-FOB stations
  // (supplied mode) until t=48. Allow the remaining native 10-second steps.
  if (tick == 95)
  {
   ACR_TestRegress.Check("structures: healed by t=85", s_bHealed, string.Format("%1", ACR_Logistics.Health(s_Wounded)));
   ACR_TestRegress.Check("structures: repaired by t=85", s_bRepaired, string.Format("%1", ACR_Logistics.Health(s_Humvee)));
   ACR_TestRegress.Check("structures: refuelled by t=85", s_bRefuelled, string.Format("%1", ACR_Logistics.Fuel(s_Humvee)));
   SCR_ResourceContainer pendingStore = ACR_Logistics.SupplyContainer(s_Fob.m_FobSupply);
   if (pendingStore) pendingStore.SetResourceValue(500);
   int beforeLoss = manager.m_Bases.GetRP("US");
   ACR_FobSupply.Order(manager, "US", s_Fob);
   ACR_TestRegress.Check("FOB: paid delivery pending when CP destroyed", s_Fob.m_iSupplyDelivery == 500 && manager.m_Bases.GetRP("US") == beforeLoss - 20, "");
   // FOB 1's upgrades would keep it standing without its HQ: clear them first.
   ACR_FobSlots.Cleanup(s_Fob);
   if (s_HQ) SCR_EntityHelper.DeleteEntityAndChildren(s_HQ);
   if (s_Hospital) SCR_EntityHelper.DeleteEntityAndChildren(s_Hospital);
   if (s_Helipad) SCR_EntityHelper.DeleteEntityAndChildren(s_Helipad);
   ACR_TestFobRelay.Cleanup();
  }
  if (tick == 100)
  {
   ACR_TestRegress.Check("structures: forward base gone with its headquarters", BaseNamed("FOB") == null, "");
   array<IEntity> medical = {};
   manager.m_Logistics.Stations("US", ACR_EService.MEDICAL, medical);
   ACR_TestRegress.Check("structures: no medical point once the hospital is gone", medical.IsEmpty(), string.Format("%1", medical.Count()));
   ACR_Helipad last = ACR_Helipad.s_All[ACR_Helipad.s_All.Count() - 1];
   ACR_TestRegress.Check("structures: pad gone with its helipad", last.Faction() == "", last.Faction());
   ACR_TestRegress.Check("FOB: supply provider removed with command post", s_Fob && !s_Fob.m_FobSupply && s_Fob.m_iSupplyDelivery == 0, "");
   ACR_TestSapperLive.Final();
   ACR_TestRegress.Complete("structures", 85 + ACR_TestLogisticsRules.CHECKS + ACR_TestFobRelay.CHECKS + ACR_TestFobBuild.CHECKS + ACR_TestFobSlots.CHECKS + ACR_TestFobUpgrades.CHECKS + ACR_TestFobDefences.CHECKS + ACR_TestFobIntel.CHECKS + ACR_TestFobPatrol.CHECKS + ACR_TestSapperDemo.CHECKS + ACR_TestFobDesign.CHECKS + ACR_TestFobBuiltRace.CHECKS + ACR_TestSapperLive.CHECKS);
  }
 }

 static void CheckHeliOnPad(ACR_Helipad pad)
 {
  array<IEntity> vehicles = {};
  ACR_Vehicles.All(vehicles);
  float best = float.MAX;
  foreach (IEntity vehicle : vehicles)
  {
   if (!ACR_Logistics.ServiceOf(vehicle) && vehicle.GetPrefabData() && vehicle.GetPrefabData().GetPrefabName().Contains("Helicopter"))
    best = Math.Min(best, vector.DistanceXZ(vehicle.GetOrigin(), pad.GetOrigin()));
  }
  ACR_TestRegress.Check("structures: the bought helicopter appeared on the built helipad", best < 4, string.Format("%1 m", best));
 }
}

modded class ACR_Logistics
{
 void ACR_TestRepairStep(IEntity station, IEntity vehicle, float now)
 {
  ServeVehicle(station, ACR_EService.REPAIR, vehicle, now);
 }
}

modded class ACR_BaseSystem
{
 void ACR_TestSpawnGuards(ACR_Base base) { SpawnGarrison(base); }
 void ACR_TestPackGuards(ACR_Base base) { DespawnGarrison(base, true); }
}
