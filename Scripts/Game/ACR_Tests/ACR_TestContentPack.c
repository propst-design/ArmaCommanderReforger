// -acrContentPackTest 1 (server-capable, ~1 min; preset contentpack): #108, a content pack
// plugged into ACR_ContentSocket. Run it with a pack and its mods loaded (the Cold War pack:
// ACAColdWar and its dependencies). Checks the buy lists keep only the vanilla catalog vehicles
// plus the pack's own, that every listed prefab loads, that garrisons keep vanilla soldiers, that
// the pack's infantry spawns as its 8-man formations, and that the pack's vehicles bought by
// either side come with that side's crewmen and a driver.
class ACR_TestContentPack
{
 static ref array<ref ACR_PendingPurchase> s_Bought = {};
 static ref array<string> s_Sides = {};
 static ref array<ref ACR_PendingPurchase> s_Squads = {};
 static int s_iExpected;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 1)
  {
   ACR_FactionSettings west = ACR_FactionSettings.Find("US");
   if (west) west.m_bAICommander = false;
   ACR_FactionSettings east = ACR_FactionSettings.Find("USSR");
   if (east) east.m_bAICommander = false;
  }
  if (tick == 8) Lists(manager);
  if (tick == 40) Crews();
 }

 protected static void Lists(ACR_Manager manager)
 {
  ACR_ContentSocket socket = ACR_ContentSocket.Get();
  ACR_TestRegress.Check("content: a pack is plugged in", socket.Plugged(), socket.Name());
  s_iExpected++;
  array<string> sides = {"US", "USSR", "FIA"};
  foreach (string side : sides)
  {
   array<ref ACR_RequisitionEntry> entries = {};
   ACR_FactionCatalog.BuildRequisitions(side, ACR_FactionSettings.Find(side), entries);
   int packed;
   int invalid;
   string report;
   foreach (ACR_RequisitionEntry entry : entries)
   {
    if (entry.m_sVehicle.IsEmpty()) continue;
    Resource resource = Resource.Load(entry.m_sVehicle);
    if (!resource || !resource.IsValid()) { invalid++; report += " bad:" + entry.m_sName; }
    // Not a catalog entry the pack keeps: it must be one the pack added itself.
    if (!ACR_FactionCatalog.Allowed(side, EEntityCatalogType.VEHICLE, entry.m_sVehicle))
    {
     packed++;
     report += " +" + entry.m_sName;
    }
   }
   PrintFormat("[ACR] CONTENTTEST %1: %2 entries, pack vehicles%3", side, entries.Count(), report);
   ACR_TestRegress.Check("content: " + side + " buy list prefabs all load", invalid == 0, report);
   int wanted = 5;
   if (side == "FIA") wanted = 3; // technicals
   ACR_TestRegress.Check("content: " + side + " sells the pack's vehicles", packed >= wanted, string.Format("%1:%2", packed, report));
   s_iExpected += 2;
  }
  // Catalog vehicles a mod added are gone from the lists (the mods add dozens to each side).
  array<ref ACR_RequisitionEntry> usList = {};
  ACR_FactionCatalog.BuildRequisitions("US", ACR_FactionSettings.Find("US"), usList);
  int foreign;
  foreach (ACR_RequisitionEntry item : usList)
  {
   string path = item.m_sVehicle;
   if (path.Contains("Tracked/BFV") || path.Contains("SightDebug") || path.Contains("Leopard1_grn_RS80Z") || path.Contains("1a5MV")) foreign++;
  }
  ACR_TestRegress.Check("content: unvetted mod vehicles stay out of the US list", foreign == 0, foreign.ToString());
  s_iExpected++;

  // FIA garrisons (and so FIA cells) draw the pack's guerrilla kits.
  int guerrillas;
  array<ResourceName> fiaPool = {};
  ACR_FactionCatalog.BuildGarrisonPool("FIA", ACR_FactionSettings.Find("FIA"), fiaPool);
  foreach (ResourceName fiaSoldier : fiaPool)
  {
   string fiaPath = fiaSoldier;
   if (fiaPath.Contains("CW_FIA_")) guerrillas++;
  }
  ACR_TestRegress.Check("content: FIA garrisons draw the pack's guerrilla kits", guerrillas > 0, string.Format("%1 of %2", guerrillas, fiaPool.Count()));
  s_iExpected++;

  // Kit mods add their soldiers to the catalogs too: garrisons stay vanilla.
  array<string> armies = {"US", "USSR"};
  foreach (string army : armies)
  {
   array<ResourceName> pool = manager.m_Bases.GetFaction(army).m_aGarrison;
   int fromMods;
   foreach (ResourceName soldier : pool)
   {
    string soldierPath = soldier;
    if (soldierPath.Contains("/ARY_")) fromMods++;
   }
   ACR_TestRegress.Check("content: " + army + " garrisons use only vanilla soldiers", !pool.IsEmpty() && fromMods == 0, string.Format("%1 of %2 from mods", fromMods, pool.Count()));
   s_iExpected++;
  }

  // The pack's command units: the US officer is the pack's own (M1911), and loads.
  string officer = "none";
  bool officerLoads;
  SCR_LoadoutManager loadouts = GetGame().GetLoadoutManager();
  if (loadouts)
  {
   foreach (SCR_BasePlayerLoadout loadout : loadouts.GetPlayerLoadouts())
   {
    SCR_FactionPlayerLoadout factionLoadout = SCR_FactionPlayerLoadout.Cast(loadout);
    if (!factionLoadout || factionLoadout.m_sAffiliatedFaction != "US") continue;
    officer = factionLoadout.m_sLoadoutResource;
    Resource officerResource = Resource.Load(factionLoadout.m_sLoadoutResource);
    officerLoads = officerResource && officerResource.IsValid();
   }
  }
  ACR_TestRegress.Check("content: the US command unit is the pack's officer", officerLoads && officer.Contains("CW_US_Officer"), officer);
  s_iExpected++;

  // The pack's infantry: period soldiers in a formation of 8.
  BuyInfantry(manager, "US", "Reserve Rifle Squad");
  BuyInfantry(manager, "USSR", "Reservist Squad");
  BuyInfantry(manager, "US", "Night Scout Team", 4);
  BuyInfantry(manager, "US", "Dragon AT Team", 3);
  BuyInfantry(manager, "USSR", "Metis ATGM Team", 3);
  // -acrContentPackWinterTest 1 (preset contentpackwinter): a cold map, Soviet soldiers in the heavy coat.
  if (System.IsCLIParam("acrContentPackWinterTest"))
  {
   ACR_RequisitionEntry winter = new ACR_RequisitionEntry();
   winter.Set("Winter test squad", "{8DE0C0830FE0C33D}Prefabs/Groups/OPFOR/Group_USSR_Base.et", "", 10, 1);
   winter.m_aSoldiers = {
    "{5436629450D8387A}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_SL.et",
    "{DCB41B3746FDD1BE}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Rifleman.et",
    "{23ADBBC31B6A3DC6}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_AR.et",
    "{96C784C502AC37DA}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_MG.et",
    "{BF643BE4ADBDFDD3}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_LAT.et",
    "{1C78331E156A3D65}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_AT.et",
    "{AB9726163EC1BD81}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Medic.et",
    "{8E0FE664CE7D1CA9}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_GL.et"
   };
   SpawnSquad(manager, "USSR", winter);
  }
  // An FIA squad of vanilla roles: the pack swaps leader, sharpshooter and riflemen.
  ACR_RequisitionEntry fia = new ACR_RequisitionEntry();
  fia.Set("FIA test squad", "{8DE0C0830FE0C33D}Prefabs/Groups/OPFOR/Group_USSR_Base.et", "", 10, 1);
  fia.m_aSoldiers = {"{677B515F119222C2}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_SL.et", "{CE33AB22F61F3365}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Sharpshooter.et"};
  for (int r = 0; r < 8; r++) fia.m_aSoldiers.Insert("{84B40583F4D1B7A3}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Rifleman.et");
  SpawnSquad(manager, "FIA", fia);

  // Buy the pack's vehicles whose own crews are wrong: Zagoria (USSR riflemen whoever owns it)
  // and WCS (M113 riflemen).
  Buy(manager, "US", "Leopard test", "{A4EA6DD24C1ED74F}Leopard/Leopard1_grn.et", 0);
  Buy(manager, "US", "M113 test", "{953523CFE2DB9769}Prefabs/Vehicles/Tracked/M113/M113_APC_MERDC.et", 1);
  Buy(manager, "USSR", "T-55 test", "{6484A98F55C6AA9E}T-55/T55olive.et", 0);
  Buy(manager, "USSR", "T-72 test", "{BC07885176C7FF8A}Prefabs/Vehicles/Tracked/T72A/T72A_Base.et", 1);
 }

 protected static void Buy(ACR_Manager manager, string side, string name, ResourceName prefab, int slot)
 {
  ACR_Base home;
  foreach (ACR_Base candidate : ACR_Base.s_All)
   if (candidate.m_sOwner == side) { home = candidate; break; }
  if (!home) { PrintFormat("[ACR] CONTENTTEST no %1 base", side); return; }
  vector at = manager.ClearSpotNear(home, 20 + 25 * slot, 90, 6);
  ACR_PendingPurchase purchase = new ACR_PendingPurchase();
  purchase.m_sName = name;
  purchase.m_sFaction = side;
  manager.m_sSpawningFaction = side;
  bool ok = manager.m_Requisition.SpawnParts(purchase, "", prefab, true, "", at, at);
  manager.m_sSpawningFaction = "";
  PrintFormat("[ACR] CONTENTTEST bought %1 for %2: %3 %4", name, side, ok, purchase.m_sError);
  s_Bought.Insert(purchase);
  s_Sides.Insert(side);
 }

 protected static void BuyInfantry(ACR_Manager manager, string side, string name, int size = 8)
 {
  array<ref ACR_RequisitionEntry> entries = {};
  ACR_FactionCatalog.BuildRequisitions(side, ACR_FactionSettings.Find(side), entries);
  ACR_RequisitionEntry entry;
  foreach (ACR_RequisitionEntry candidate : entries)
   if (candidate.m_sName == name) entry = candidate;
  ACR_TestRegress.Check("content: " + side + " sells the " + name, entry && entry.m_aSoldiers && entry.m_aSoldiers.Count() == size, name);
  s_iExpected++;
  if (entry) SpawnSquad(manager, side, entry);
 }

 protected static void SpawnSquad(ACR_Manager manager, string side, ACR_RequisitionEntry entry)
 {
  string name = entry.m_sName;
  ACR_Base home;
  foreach (ACR_Base candidate : ACR_Base.s_All)
   if (candidate.m_sOwner == side) { home = candidate; break; }
  // A side without a base on the test map (FIA) spawns well away from any base, out of the fight.
  float near = 70;
  float far = 120;
  if (!home && !ACR_Base.s_All.IsEmpty())
  {
   home = ACR_Base.s_All[0];
   near = 600;
   far = 900;
  }
  if (!home) return;
  vector at = manager.ClearSpotNear(home, near, far, 6);
  ACR_PendingPurchase purchase = new ACR_PendingPurchase();
  purchase.m_sName = name;
  purchase.m_sFaction = side;
  purchase.m_Entry = entry;
  manager.m_sSpawningFaction = side;
  bool ok = manager.m_Requisition.SpawnParts(purchase, entry.m_sGroup, "", false, "", at, at);
  manager.m_sSpawningFaction = "";
  PrintFormat("[ACR] CONTENTTEST bought %1 for %2: %3 %4", name, side, ok, purchase.m_sError);
  s_Squads.Insert(purchase);
 }

 // What the group's men wear (jacket, pants) and how many items their pockets got.
 protected static string Uniforms(SCR_AIGroup group)
 {
  string report;
  if (!group) return report;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(body.FindComponent(EquipedLoadoutStorageComponent));
   if (!loadout) continue;
   array<IEntity> cloths = {loadout.GetClothFromArea(LoadoutJacketArea), loadout.GetClothFromArea(LoadoutPantsArea)};
   int items;
   array<IEntity> held = {};
   foreach (IEntity cloth : cloths)
   {
    if (!cloth) continue;
    BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(cloth.FindComponent(BaseInventoryStorageComponent));
    if (!storage) continue;
    held.Clear();
    storage.GetAll(held);
    items += held.Count();
   }
   report += string.Format(" | %1 %2 items:%3", Leaf(cloths[0]), Leaf(cloths[1]), items);
  }
  return report;
 }

 // Each man's primary weapon and how many of its magazines he carries (UNARMED / NOAMMO flag a problem).
 protected static string Weapons(SCR_AIGroup group)
 {
  string report;
  if (!group) return report;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
   SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(body.FindComponent(SCR_InventoryStorageManagerComponent));
   if (!weapons || !inventory) continue;
   array<WeaponSlotComponent> slots = {};
   weapons.GetWeaponsSlots(slots);
   IEntity primary;
   foreach (WeaponSlotComponent slot : slots)
    if (slot.GetWeaponSlotIndex() == 0) primary = slot.GetWeaponEntity();
   if (!primary) { report += " UNARMED"; continue; }
   BaseWeaponComponent weapon = BaseWeaponComponent.Cast(primary.FindComponent(BaseWeaponComponent));
   int mags = -1;
   if (weapon) mags = inventory.GetMagazineCountByWeapon(weapon);
   string flag;
   string leaf = Leaf(primary);
   if (mags <= 0 && (leaf.Contains("G3") || leaf.Contains("Uzi") || leaf.Contains("VZ58"))) flag = "NOAMMO";
   // Its optic, if any (night scouts: the PVS-4).
   string optic = "";
   IEntity part = primary.GetChildren();
   while (part)
   {
    string partLeaf = Leaf(part);
    if (partLeaf.Contains("Optic")) optic = "+" + partLeaf;
    part = part.GetSibling();
   }
   if (leaf.Contains("PVS4") && optic.IsEmpty()) flag += "NOOPTIC";
   report += string.Format(" %1%2:%3%4", leaf, optic, mags, flag);
  }
  return report;
 }

 // Each man's launcher (and whether it holds a round) and what he wears on his back.
 protected static string Launchers(SCR_AIGroup group)
 {
  string report;
  if (!group) return report;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
   EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(body.FindComponent(EquipedLoadoutStorageComponent));
   string tube = "-";
   if (weapons)
   {
    array<WeaponSlotComponent> slots = {};
    weapons.GetWeaponsSlots(slots);
    foreach (WeaponSlotComponent slot : slots)
    {
     IEntity weaponEntity = slot.GetWeaponEntity();
     BaseWeaponComponent weapon;
     if (weaponEntity) weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
     if (!weapon || weapon.GetWeaponType() != EWeaponType.WT_ROCKETLAUNCHER) continue;
     tube = Leaf(weaponEntity);
     BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
     if (muzzle && muzzle.GetAmmoCount() > 0) tube += "(loaded)";
     else tube += "(EMPTY)";
    }
   }
   string back = "-";
   if (loadout) back = Leaf(loadout.GetClothFromArea(LoadoutBackpackArea));
   report += string.Format(" %1/%2", tube, back);
  }
  return report;
 }

 static string Leaf(IEntity entity)
 {
  if (!entity || !entity.GetPrefabData()) return "none";
  return FilePath.StripExtension(FilePath.StripPath(entity.GetPrefabData().GetPrefabName()));
 }

 protected static void Crews()
 {
  foreach (ACR_PendingPurchase squad : s_Squads)
  {
   int men;
   int period;
   string roster;
   if (squad.m_Group)
   {
    array<AIAgent> agents = {};
    squad.m_Group.GetAgents(agents);
    foreach (AIAgent agent : agents)
    {
     IEntity body = agent.GetControlledEntity();
     if (!body || !body.GetPrefabData()) continue;
     men++;
     string bodyPath = body.GetPrefabData().GetPrefabName();
     if (bodyPath.Contains("/ARY_") || bodyPath.Contains("/ACR_ColdWar/")) period++;
     roster += " " + FilePath.StripExtension(FilePath.StripPath(bodyPath));
    }
   }
   PrintFormat("[ACR] CONTENTTEST squad %1: %2 men, %3 period:%4%5", squad.m_sName, men, period, roster, Uniforms(squad.m_Group));
   string armed = Weapons(squad.m_Group);
   PrintFormat("[ACR] CONTENTTEST weapons %1:%2", squad.m_sName, armed);
   if (squad.m_sName == "Night Scout Team")
   {
    ACR_TestRegress.Check("content: night scouts carry the M16A2 with the AN/PVS-4", men == 4 && armed.Contains("PVS4") && !armed.Contains("NOOPTIC"), armed);
    s_iExpected++;
    continue;
   }
   if (squad.m_sName == "Dragon AT Team" || squad.m_sName == "Metis ATGM Team")
   {
    string tubes = Launchers(squad.m_Group);
    PrintFormat("[ACR] CONTENTTEST launchers %1:%2", squad.m_sName, tubes);
    string kind = "M47";
    if (squad.m_sName == "Metis ATGM Team") kind = "Metis";
    array<string> men3 = {};
    tubes.Split(" ", men3, true);
    int missiles;
    foreach (string man3 : men3)
     if (man3.Contains("/Ammo_Missile_" + kind)) missiles++;
    ACR_TestRegress.Check("content: the " + squad.m_sName + " gunner holds a loaded " + kind + ", two spare missiles on backs", men == 3 && tubes.Contains("Launcher_" + kind + "(loaded)") && missiles == 2, tubes);
    s_iExpected++;
    continue;
   }
   if (squad.m_sName == "Winter test squad")
   {
    string worn = Uniforms(squad.m_Group);
    array<string> parts = {};
    worn.Split(" | ", parts, true);
    int coats;
    int emptyCoats;
    foreach (string part : parts)
    {
     if (!part.Contains("ARY_Jacket_M88_Heavy")) continue;
     coats++;
     if (part.Contains("items:0")) emptyCoats++;
    }
    ACR_TestRegress.Check("content: on a cold map Soviet soldiers wear the heavy coat with their kit", men == 8 && coats == 8 && emptyCoats == 0, string.Format("%1 coats of %2 men, %3 with empty pockets", coats, men, emptyCoats));
    s_iExpected++;
    continue;
   }
   if (squad.m_sFaction == "FIA")
   {
    // Its recipe, not who is still standing: it spawns beside another side's base and fights.
    int slots;
    int kitted;
    if (squad.m_Group && squad.m_Group.m_aUnitPrefabSlots)
     foreach (ResourceName slot : squad.m_Group.m_aUnitPrefabSlots)
     {
      slots++;
      string slotPath = slot;
      if (slotPath.Contains("CW_FIA_")) kitted++;
     }
    ACR_TestRegress.Check("content: FIA squad members get the pack's guerrilla kits", slots == 10 && kitted >= 6 && men >= 1, string.Format("%1 slots, %2 kitted, %3 alive", slots, kitted, men));
    ACR_TestRegress.Check("content: FIA guerrillas carry their weapons with ammunition", !armed.Contains("NOAMMO") && !armed.Contains("UNARMED"), armed);
    s_iExpected++;
   }
   else ACR_TestRegress.Check("content: " + squad.m_sName + " spawns 8 men with period kit", men == 8 && period >= 3, string.Format("%1 men, %2 period", men, period));
   s_iExpected++;
  }
  foreach (int i, ACR_PendingPurchase purchase : s_Bought)
  {
   string side = s_Sides[i];
   IEntity vehicle = purchase.m_Vehicle;
   int seated;
   int own;
   bool driver;
   string report;
   SCR_BaseCompartmentManagerComponent compartments;
   if (vehicle) compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
   if (compartments)
   {
    array<BaseCompartmentSlot> seats = {};
    compartments.GetCompartments(seats);
    foreach (BaseCompartmentSlot seat : seats)
    {
     IEntity man = seat.GetOccupant();
     if (!man) continue;
     seated++;
     if (seat.GetType() == ECompartmentType.PILOT) driver = true;
     string prefab = man.GetPrefabData().GetPrefabName();
     string faction = ACR_Intel.FactionKeyOf(man);
     bool crewman = prefab.Contains("_Crew") || prefab.Contains("_CC");
     if (faction == side && crewman) own++;
     report += string.Format(" %1:%2", FilePath.StripExtension(FilePath.StripPath(prefab)), faction);
    }
   }
   PrintFormat("[ACR] CONTENTTEST crew %1: %2/%3 own crewmen, driver %4,%5", purchase.m_sName, own, seated, driver, report);
   ACR_TestRegress.Check("content: " + purchase.m_sName + " has a driver and only " + side + " crewmen", vehicle && driver && seated >= 2 && own == seated, string.Format("%1/%2%3", own, seated, report));
   s_iExpected++;
  }
  ACR_TestRegress.Complete("contentpack", s_iExpected);
 }
}
