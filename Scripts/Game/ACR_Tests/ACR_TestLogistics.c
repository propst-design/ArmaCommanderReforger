// Requisition, logistics, supplies, spawn spot, cleanup and launcher tests (split out of ACR_TestHarness, 2 Oct).
// VERDICT lines (ACR_TestRegress): supplies 4, launcher 4. Requisition, logistics (service trucks), spawn spot
// and cleanup only log what happens (no verdict).
class ACR_TestLogistics : Managed
{
 protected ACR_TestHarness h;

 void ACR_TestLogistics(ACR_TestHarness harness) { h = harness; }

 // ---------------------------------------------------------------- -acrRequisitionTest
 void TestRequisition()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!pc || !manager) return;
  array<ref ACR_RequisitionEntry> entries = manager.m_Bases.GetFaction("US").m_aRequisitions;
  if (h.m_Ticks == 8)
  {
   PrintFormat("[ACR] REQ rp=%1 entries=%2 garrisonPool=%3", manager.m_Bases.GetRP("US"), entries.Count(), manager.m_Bases.GetFaction("US").m_aGarrison.Count());
   foreach (int i, ACR_RequisitionEntry e : entries)
    PrintFormat("[ACR] REQ %1: %2 | %3 RP | stock %4 | vehicle=%5", i, e.m_sName, e.m_iCost, e.m_iStock, !e.m_sVehicle.IsEmpty());
  }
  if (h.m_Ticks == 10)
  {
   foreach (int i, ACR_RequisitionEntry e : entries)
    if (e.m_sVehicle.IsEmpty()) { pc.ACR_TestPurchase(i, "Farm [1]"); break; }
  }
  if (h.m_Ticks == 12)
  {
   int pick = -1;
   foreach (int i, ACR_RequisitionEntry e : entries)
   {
    if (e.m_sVehicle.IsEmpty()) continue;
    if (pick < 0) pick = i;
    if (e.m_sVehicle.Contains("M923")) { pick = i; break; }
   }
   if (pick >= 0) pc.ACR_TestPurchase(pick, "Farm [1]");
  }
  if (h.m_Ticks == 14) pc.ACR_TestPurchase(0, "Outpost"); // enemy base: must be refused
  if (h.m_Ticks == 30 || h.m_Ticks == 65)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
    PrintFormat("[ACR] REQ squad %1 (%2): %3", squad.GetName(), squad.GetFaction().GetFactionKey(), squad.Describe());
   PrintFormat("[ACR] REQ rp=%1", manager.m_Bases.GetRP("US"));
  }
  if (h.m_Ticks == 35 && h.m_Range.m_Group)
  {
   array<AIAgent> agents = {};
   h.m_Range.m_Group.GetAgents(agents);
   if (agents.Count() > 1) SCR_EntityHelper.DeleteEntityAndChildren(agents[agents.Count() - 1].GetControlledEntity());
  }
  if (h.m_Ticks == 40) pc.ACR_Request(ACR_ERequest.REINFORCE, SCR_EntityHelper.EntityToRplId(h.m_Range.m_Group));
  if (h.m_Ticks == 70) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrLogisticsTest
 // Alpha's spare magazines are taken away, then it walks to an ammo truck and
 // must be topped up (the truck's stock goes down). A gun truck is damaged and
 // drained of fuel next to a repair truck and a tanker, and must be fixed up.
 ACR_Squad m_GunCrew;
 IEntity m_AmmoTruck;

 void TestLogistics()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (h.m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings ours = ACR_FactionSettings.Find("US");
   if (ours) ours.m_bAICommander = false; // (on a dedicated server nobody holds US command)
  }
  if (h.m_Ticks == 5)
  {
   manager.SpawnUnit("Ammo Truck", "", "{92264FF932676C13}Prefabs/Vehicles/Wheeled/M923A1/M923A1_ammo.et", true, "", h.Ground("1400 0 2832"), h.Ground("1405 0 2836"));
   manager.SpawnUnit("Repair Truck", "", "{A042ACE5C2B13206}Prefabs/Vehicles/Wheeled/M923A1/M923A1_repair.et", true, "", h.Ground("1450 0 2800"), h.Ground("1455 0 2805"));
   manager.SpawnUnit("Fuel Truck", "", "{2BE1F8B9299B67C1}Prefabs/Vehicles/Wheeled/M923A1/M923A1_tanker.et", true, "", h.Ground("1450 0 2815"), h.Ground("1455 0 2820"));
   manager.SpawnUnit("Gun Truck", "", "{3EA6F47D95867114}Prefabs/Vehicles/Wheeled/M998/M1025_armed_M2HB.et", true, "", h.Ground("1465 0 2807"), h.Ground("1470 0 2810"));
  }
  ACR_Squad alpha = h.Squad();
  if (h.m_Ticks == 14)
  {
   m_GunCrew = h.NamedSquad(manager, "Gun Truck");
   ACR_Squad ammoCrew = h.NamedSquad(manager, "Ammo Truck");
   if (ammoCrew) m_AmmoTruck = ACR_Transport.CrewedVehicle(ammoCrew);
   PrintFormat("[ACR] LOGI service flags: ammo=%1 repair=%2 fuel=%3", ACR_Logistics.ServiceOf(m_AmmoTruck), ACR_Logistics.ServiceOf(ACR_Transport.CrewedVehicle(h.NamedSquad(manager, "Repair Truck"))), ACR_Logistics.ServiceOf(ACR_Transport.CrewedVehicle(h.NamedSquad(manager, "Fuel Truck"))));
   // Take Alpha's spare magazines.
   int taken;
   array<AIAgent> agents = {};
   alpha.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(soldier.FindComponent(InventoryStorageManagerComponent));
    if (!inventory) continue;
    array<IEntity> items = {};
    inventory.GetItems(items);
    foreach (IEntity item : items)
    {
     if (!item.FindComponent(BaseMagazineComponent)) continue;
     if (item.GetParent() && item.GetParent().FindComponent(BaseWeaponComponent)) continue; // loaded
     if (inventory.TryDeleteItem(item)) taken++;
    }
   }
   // Damage and drain the gun truck.
   IEntity gun = ACR_Transport.CrewedVehicle(m_GunCrew);
   if (gun)
   {
    SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(gun.FindComponent(SCR_DamageManagerComponent));
    if (damage) damage.SetHealthScaled(0.35);
    SCR_FuelManagerComponent fuel = SCR_FuelManagerComponent.Cast(gun.FindComponent(SCR_FuelManagerComponent));
    if (fuel) fuel.SetTotalFuelPercentage(0.1);
    array<BaseMagazineComponent> belts = {};
    ACR_Logistics.TurretMagazines(gun, belts);
    foreach (BaseMagazineComponent belt : belts) belt.SetAmmoCount(0);
   }
   PrintFormat("[ACR] LOGI drained: %1 magazines taken from Alpha; gun truck health %2 fuel %3 gun ammo %4", taken, ACR_Logistics.Health(gun), ACR_Logistics.Fuel(gun), ACR_Logistics.VehicleAmmo(gun));
  }
  if (h.m_Ticks == 22 && alpha && m_AmmoTruck) alpha.MoveTo(m_AmmoTruck.GetOrigin() + Vector(-8, 0, 0));
  if (h.m_Ticks == 30)
  {
   // The gun truck drives up to the ammo truck too, for its M2 belt.
   IEntity gunVehicle = ACR_Transport.CrewedVehicle(m_GunCrew);
   PrintFormat("[ACR] LOGI gun truck after repair/fuel stop: health %1 fuel %2", ACR_Logistics.Health(gunVehicle), ACR_Logistics.Fuel(gunVehicle));
   if (m_GunCrew && m_AmmoTruck) m_GunCrew.MoveTo(m_AmmoTruck.GetOrigin() + Vector(12, 0, 8));
  }
  if (h.m_Ticks % 5 == 0 && h.m_Ticks >= 15 && alpha)
  {
   IEntity gunNow = ACR_Transport.CrewedVehicle(m_GunCrew);
   PrintFormat("[ACR] LOGI t=%1 alpha: %2 | gun truck: health %3 fuel %4 gunAmmo %5 | ammo truck stock %6 pct", h.m_Ticks, alpha.Describe(), ACR_Logistics.Health(gunNow), ACR_Logistics.Fuel(gunNow), ACR_Logistics.VehicleAmmo(gunNow), manager.m_Logistics.StockPercent(m_AmmoTruck));
  }
  if (h.m_Ticks == 100) Print("[ACR] TEST COMPLETE");
 }

 // ---------------------------------------------------------------- -acrSuppliesTest
 // Service trucks carry game supplies synced with the mod's stock: arsenal,
 // ammo and construction trucks start full; supplies taken the game's way
 // (as the arsenal does) lower the mod's stock, and stock the AI uses lowers
 // the game supplies. Runs on the server (no player needed).
 IEntity m_ArsenalTruck;
 IEntity m_SupplyAmmoTruck;
 IEntity m_EngineerTruck;
 float m_fSuppliesBefore;

 void LogSupplies(string label, IEntity truck)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  SCR_ResourceContainer container = ACR_Logistics.SupplyContainer(truck);
  float max = -1;
  if (container) max = container.GetMaxResourceValue();
  PrintFormat("[ACR] SUPPLY %1 t=%2: flags=%3 container=%4 supplies=%5/%6 stock=%7 (%8 pct)", label, h.m_Ticks, ACR_Logistics.ServiceOf(truck), container != null, ACR_Logistics.GameSupplies(truck), max, manager.m_Logistics.StockOf(truck), manager.m_Logistics.StockPercent(truck));
 }

 void TestSupplies()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (h.m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings ours = ACR_FactionSettings.Find("US");
   if (ours) ours.m_bAICommander = false;
   PrintFormat("[ACR] SUPPLY global supplies enabled=%1 building manager=%2", SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled(), GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent) != null);
  }
  if (h.m_Ticks == 5)
  {
   manager.SpawnUnit("Arsenal Truck", "", "{1FF144F8A209239D}Prefabs/Vehicles/Wheeled/M923A1/M923A1_arsenal.et", true, "", h.Ground("1400 0 2832"), h.Ground("1405 0 2836"));
   manager.SpawnUnit("Ammo Truck", "", "{92264FF932676C13}Prefabs/Vehicles/Wheeled/M923A1/M923A1_ammo.et", true, "", h.Ground("1420 0 2832"), h.Ground("1425 0 2836"));
   manager.SpawnUnit("Engineer Truck", "", "{2D74C39A650A3030}Prefabs/Vehicles/Wheeled/M923A1/M923A1_engineer.et", true, "", h.Ground("1440 0 2832"), h.Ground("1445 0 2836"));
  }
  if (h.m_Ticks == 16)
  {
   m_ArsenalTruck = ACR_Transport.CrewedVehicle(h.NamedSquad(manager, "Arsenal Truck"));
   m_SupplyAmmoTruck = ACR_Transport.CrewedVehicle(h.NamedSquad(manager, "Ammo Truck"));
   m_EngineerTruck = ACR_Transport.CrewedVehicle(h.NamedSquad(manager, "Engineer Truck"));
   LogSupplies("arsenal start", m_ArsenalTruck);
   LogSupplies("ammo start", m_SupplyAmmoTruck);
   LogSupplies("engineer start", m_EngineerTruck);
   bool provider;
   if (m_EngineerTruck)
   {
    IEntity part = m_EngineerTruck.GetChildren();
    while (part)
    {
     if (part.FindComponent(SCR_CampaignBuildingProviderComponent)) provider = true;
     part = part.GetSibling();
    }
   }
   PrintFormat("[ACR] SUPPLY engineer truck building provider=%1", provider);
   // Placed compositions need an outline (the silhouette players dig out with the shovel).
   SCR_CampaignBuildingManagerComponent building = SCR_CampaignBuildingManagerComponent.Cast(GetGame().GetGameMode().FindComponent(SCR_CampaignBuildingManagerComponent));
   IEntity barricade = ACR_Manager.SpawnPrefab("{6FE74C9C75DA8A52}PrefabsEditable/Auto/Compositions/Slotted/SlotRoadMedium/E_Barricade_M_US_01.et", h.Ground("1460 0 2832"));
   SCR_EditableEntityComponent editable;
   if (barricade) editable = SCR_EditableEntityComponent.Cast(barricade.FindComponent(SCR_EditableEntityComponent));
   string outline;
   if (building && building.GetOutlineManager() && editable) outline = building.GetOutlineManager().GetCompositionOutline(editable);
   PrintFormat("[ACR] SUPPLY CHECK placed compositions get a build outline: %1 (%2)", !outline.IsEmpty(), outline);
   ACR_TestRegress.Check("supplies: placed compositions get a build outline", !outline.IsEmpty(), outline);
   // A player takes gear from the arsenal: the game's own consume path.
   m_fSuppliesBefore = ACR_Logistics.GameSupplies(m_ArsenalTruck);
   SCR_ResourceComponent resources = SCR_ResourceComponent.FindResourceComponent(m_ArsenalTruck);
   int reason = -1;
   if (resources) reason = SCR_ResourceSystemHelper.ConsumeResources(resources, 200, true);
   PrintFormat("[ACR] SUPPLY arsenal visit: consumed 200 (reason %1), supplies now %2", reason, ACR_Logistics.GameSupplies(m_ArsenalTruck));
  }
  if (h.m_Ticks == 21)
  {
   LogSupplies("arsenal after visit", m_ArsenalTruck);
   float stock = manager.m_Logistics.StockOf(m_ArsenalTruck);
   bool stockFollows = Math.AbsFloat(stock - (ACR_Logistics.CAPACITY_AMMO - 200 / ACR_Logistics.SUPPLIES_PER_LOAD_AMMO)) < 0.6;
   PrintFormat("[ACR] SUPPLY CHECK player use reaches the stock: %1 (stock %2, expected about %3)", stockFollows, stock, ACR_Logistics.CAPACITY_AMMO - 200 / ACR_Logistics.SUPPLIES_PER_LOAD_AMMO);
   ACR_TestRegress.Check("supplies: player use reaches the stock", stockFollows, string.Format("stock %1", stock));
   // The AI resupplies from the ammo truck: 10 loads.
   manager.m_Logistics.Use(m_SupplyAmmoTruck, 10);
  }
  if (h.m_Ticks == 26)
  {
   LogSupplies("ammo after AI use", m_SupplyAmmoTruck);
   float expected = (ACR_Logistics.CAPACITY_AMMO - 10) * ACR_Logistics.SUPPLIES_PER_LOAD_AMMO;
   bool suppliesFollow = Math.AbsFloat(ACR_Logistics.GameSupplies(m_SupplyAmmoTruck) - expected) < 1;
   PrintFormat("[ACR] SUPPLY CHECK AI use reaches the game supplies: %1 (supplies %2, expected %3)", suppliesFollow, ACR_Logistics.GameSupplies(m_SupplyAmmoTruck), expected);
   ACR_TestRegress.Check("supplies: AI use reaches the game supplies", suppliesFollow, string.Format("supplies %1, expected %2", ACR_Logistics.GameSupplies(m_SupplyAmmoTruck), expected));
   LogSupplies("arsenal steady", m_ArsenalTruck);
   LogSupplies("engineer steady", m_EngineerTruck);
   bool full = ACR_Logistics.GameSupplies(m_EngineerTruck) >= ACR_Logistics.CAPACITY_CONSTRUCTION * ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION - 1;
   PrintFormat("[ACR] SUPPLY CHECK construction truck full of supplies: %1", full);
   ACR_TestRegress.Check("supplies: construction truck full of supplies", full, ACR_Logistics.GameSupplies(m_EngineerTruck).ToString());
   ACR_TestRegress.Complete("supplies", 4);
  }
 }

 // ---------------------------------------------------------------- -acrSpawnSpotTest
 // Requisition at a clicked spot: infantry and a vehicle appear at the click, a click
 // outside the ring is pulled in to its edge.
 void TestSpawnSpot()
 {
  if (h.m_Ticks != 8) { if (h.m_Ticks == 12) Print("[ACR] TEST COMPLETE"); return; }
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Base base;
  foreach (ACR_Base candidate : ACR_Base.s_All)
   if (candidate.m_sOwner == "US" && candidate.m_bAllowSpawn && !candidate.m_bContested) { base = candidate; break; }
  if (!base) { Print("[ACR] SPAWNSPOT no US base"); return; }
  ACR_FactionData data = manager.m_Bases.GetFaction("US");
  data.m_iRP = 10000;
  int infantry = -1, vehicle = -1;
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (entry.m_iRemaining <= 0) continue;
   if (infantry < 0 && entry.m_sVehicle.IsEmpty()) infantry = i;
   if (vehicle < 0 && !entry.m_sVehicle.IsEmpty()) vehicle = i;
  }
  float ring = Math.Max(base.m_fRadius, 30);
  vector inside = base.GetOrigin() + Vector(ring * 0.6, 0, 0);
  vector outside = base.GetOrigin() + Vector(0, 0, ring * 3);
  PrintFormat("[ACR] SPAWNSPOT base %1 radius %2 centre %3", base.m_sName, ring, base.GetOrigin());
  if (infantry >= 0) PrintFormat("[ACR] SPAWNSPOT infantry inside: %1", manager.PurchaseAt("US", infantry, base, inside, true));
  if (vehicle >= 0) PrintFormat("[ACR] SPAWNSPOT vehicle inside: %1", manager.PurchaseAt("US", vehicle, base, inside + Vector(0, 0, 12), true));
  if (infantry >= 0) PrintFormat("[ACR] SPAWNSPOT infantry outside: %1", manager.PurchaseAt("US", infantry, base, outside, true));
 }

 // ---------------------------------------------------------------- -acrCleanupTest
 // Bravo is killed and the range truck destroyed. Cleanup times are shortened.
 // While the player stands next to them nothing may go; once he's moved away
 // the bodies and wreck are removed. Unattributed loose gear is preserved.
 IEntity m_CleanupTruck;
 int m_iBravoBodies;

 void TestCleanup()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Cleanup) return;
  if (h.m_Ticks == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   manager.m_Cleanup.m_fBodySeconds = 15;
   manager.m_Cleanup.m_fWreckSeconds = 15;
   manager.m_Cleanup.m_iCheckMs = 2000;
  }
  if (h.m_Ticks == 10)
  {
   ACR_Squad bravo = manager.FindSquadByGroup(h.m_Range.m_Bravo);
   array<AIAgent> agents = {};
   if (bravo) bravo.GetAgents(agents);
   foreach (AIAgent agent : agents) { h.Kill(agent.GetControlledEntity()); m_iBravoBodies++; }
   m_CleanupTruck = h.m_Range.m_Truck;
   SCR_DamageManagerComponent damage;
   if (m_CleanupTruck) damage = SCR_DamageManagerComponent.Cast(m_CleanupTruck.FindComponent(SCR_DamageManagerComponent));
   if (damage) damage.SetHealthScaled(0);
   if (m_CleanupTruck) manager.m_Cleanup.Watch(m_CleanupTruck);
   PrintFormat("[ACR] CLEANUP killed %1 of Bravo, truck destroyed=%2", m_iBravoBodies, damage != null && damage.IsDestroyed());
  }
  if (h.m_Ticks % 5 == 0 && h.m_Ticks > 10) PrintFormat("[ACR] CLEANUP t=%1 %2 | truck exists=%3", h.m_Ticks, manager.m_Cleanup.Status(), m_CleanupTruck != null);
  if (h.m_Ticks == 40)
  {
   // Walk away: move the player's body 1.5 km off.
   IEntity body;
   if (pc) body = pc.GetControlledEntity();
   if (body) body.SetOrigin(h.Ground("1320 0 1300"));
   Print("[ACR] CLEANUP player moved away");
  }
  if (h.m_Ticks == 75) Print("[ACR] TEST COMPLETE");
 }
 // ---------------------------------------------------------------- -acrLauncherTest
 // An AI soldier's disposable launcher is emptied (as if fired): he must drop
 // it, and a rearm must give him a loaded one of the same kind back.
 IEntity m_LauncherMan;
 void TestLauncher()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Launchers) return;
  if (h.m_Ticks == 2)
  {
   ACR_FactionSettings sov = ACR_FactionSettings.Find("USSR");
   if (sov) sov.m_bAICommander = false;
  }
  if (h.m_Ticks == 8)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    array<AIAgent> agents = {};
    squad.GetAgents(agents);
    foreach (AIAgent agent : agents)
    {
     IEntity body = agent.GetControlledEntity();
     if (body && BBB_Launchers.DisposableOf(body) && ACR_Squad.FindController(body) == 0) { m_LauncherMan = body; break; }
    }
    if (m_LauncherMan) break;
   }
   if (!m_LauncherMan)
   {
    Print("[ACR] LAUNCHERTEST no soldier with a disposable launcher found");
    ACR_TestRegress.Abort("launcher", "no soldier with a disposable launcher");
    return;
   }
   IEntity tube = BBB_Launchers.DisposableOf(m_LauncherMan);
   BaseWeaponComponent weapon = BaseWeaponComponent.Cast(tube.FindComponent(BaseWeaponComponent));
   PrintFormat("[ACR] LAUNCHERTEST soldier has %1 with %2 round(s)", ACR_Vehicles.NameOf(tube), BBB_Launchers.Loaded(tube));
   if (weapon && weapon.GetCurrentMagazine()) weapon.GetCurrentMagazine().SetAmmoCount(0);
   if (weapon)
   {
    array<BaseMuzzleComponent> muzzles = {};
    weapon.GetMuzzlesList(muzzles);
    foreach (BaseMuzzleComponent muzzle : muzzles)
     for (int b = 0; b < muzzle.GetBarrelsCount(); b++) muzzle.ClearChamber(b);
   }
   PrintFormat("[ACR] LAUNCHERTEST emptied: %1 round(s) left", BBB_Launchers.Loaded(tube));
  }
  if (h.m_Ticks == 16 && m_LauncherMan)
  {
   PrintFormat("[ACR] LAUNCHERTEST dropped %1 (want 1), still carries one: %2 (want 0), owed: %3", BBB.s_Launchers.m_iDropped, BBB_Launchers.DisposableOf(m_LauncherMan) != null, manager.m_Launchers.IsOwed(m_LauncherMan));
   ACR_TestRegress.Check("launcher: the emptied tube is dropped", BBB.s_Launchers.m_iDropped == 1, BBB.s_Launchers.m_iDropped.ToString());
   ACR_TestRegress.Check("launcher: he no longer carries it", BBB_Launchers.DisposableOf(m_LauncherMan) == null);
   PrintFormat("[ACR] LAUNCHERTEST rearm: %1", manager.m_Launchers.Rearm(m_LauncherMan));
  }
  if (h.m_Ticks == 20 && m_LauncherMan)
  {
   IEntity fresh = BBB_Launchers.DisposableOf(m_LauncherMan);
   int rounds = -1;
   if (fresh) rounds = BBB_Launchers.Loaded(fresh);
   // BBB_Launchers.Loaded counts the muzzle and its magazine, so a loaded tube reads 2 (BBB documents
   // it in BBB_LauncherInfantryTest; game callers only ask > 0).
   PrintFormat("[ACR] LAUNCHERTEST after rearm: has launcher %1 (want 1), rounds %2 (want > 0)", fresh != null, rounds);
   ACR_TestRegress.Check("launcher: a rearm gives him a launcher back", fresh != null);
   ACR_TestRegress.Check("launcher: the new launcher is loaded", rounds > 0, rounds.ToString());
  }
  if (h.m_Ticks == 22) ACR_TestRegress.Complete("launcher", 4);
 }
}
