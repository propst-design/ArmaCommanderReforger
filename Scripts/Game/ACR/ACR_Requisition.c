// Buying units, reinforcements and the ammo restock at bases
// (split out of ACR_Manager, 2 Oct; the manager forwards its old entry points).
class ACR_Requisition : Managed
{
 protected ACR_Manager m_Manager;

 void ACR_Requisition(ACR_Manager manager) { m_Manager = manager; }

 // Mod catalogs sometimes point at a vehicle/group prefab whose built-in faction
 // key is still vanilla US/USSR. Bind every spawned unit to the selected world side.
 protected static void SetUnitFaction(SCR_AIGroup group, string factionKey)
 {
  if (!group) return;
  Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
  if (!faction) return;
  group.SetFaction(faction);
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body) continue;
   SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(body.FindComponent(SCR_FactionAffiliationComponent));
   if (affiliation) affiliation.SetAffiliatedFaction(faction);
  }
 }

 protected static void SetOccupantFaction(IEntity vehicle, string factionKey)
 {
  array<IEntity> occupants = {};
  ACR_PendingPurchase.Occupants(vehicle, occupants);
  Faction faction = GetGame().GetFactionManager().GetFactionByKey(factionKey);
  if (!faction) return;
  foreach (IEntity body : occupants)
  {
   if (!body) continue;
   AIControlComponent control = AIControlComponent.Cast(body.FindComponent(AIControlComponent));
   AIAgent agent;
   if (control) agent = control.GetControlAIAgent();
   if (agent) SetUnitFaction(SCR_AIGroup.Cast(agent.GetParentGroup()), factionKey);
   SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(body.FindComponent(SCR_FactionAffiliationComponent));
   if (affiliation) affiliation.SetAffiliatedFaction(faction);
  }
 }

 protected void SetOccupantFactionLater(IEntity vehicle, string factionKey)
 {
  SetOccupantFaction(vehicle, factionKey);
 }

 static const int REINFORCE_COST_PER_SOLDIER = 3;

 ACR_Base FindSpawnBase(string factionKey, string label)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.GetLabel() == label && base.m_sOwner == factionKey) return base;
  return null;
 }

 ACR_Base NearestSpawnBase(string factionKey, vector position)
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != factionKey || !ACR_LogisticsRules.CanReinforce(base) || base.m_bContested) continue;
   float distance = vector.Distance(base.GetOrigin(), position);
   if (distance >= bestDistance) continue;
   bestDistance = distance;
   best = base;
  }
  return best;
 }

 vector ClearSpotNear(ACR_Base base, float minDistance, float maxDistance, float clearance) { return SpawnPoint(base, minDistance, maxDistance, clearance); }

 // Random point in a ring around the base, nudged to a spot clear of walls,
 // revetments and buildings (clearance = radius of the free cylinder needed).
 vector SpawnPoint(ACR_Base base, float minDistance, float maxDistance, float clearance = 1)
 {
  vector fallback;
  for (int attempt = 0; attempt < 8; attempt++)
  {
   float angle = Math.RandomFloat(0, Math.PI2);
   float distance = Math.RandomFloat(minDistance, maxDistance + attempt * 5);
   vector candidate = ACR_Manager.Ground(base.GetOrigin() + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance));
   if (attempt == 0) fallback = candidate;
   vector free;
   if (ACR_BaseSystem.ClearGround(free, candidate, 8, clearance)) return free;
  }
  // None in the ring (a crowded base): the nearest clear ground further out, before a spot
  // nobody checked (#87: a Humvee in an airport hangar).
  vector wide;
  if (ACR_BaseSystem.ClearGround(wide, fallback, 60, clearance)) return wide;
  return fallback;
 }

 // A clear spot near the commander's click, pulled inside the base ring.
 vector SpotInBase(ACR_Base base, vector spot, float clearance)
 {
  vector centre = base.GetOrigin();
  float ring = Math.Max(base.m_fRadius, 30);
  vector flat = Vector(spot[0] - centre[0], 0, spot[2] - centre[2]);
  if (flat.Length() > ring) spot = centre + flat.Normalized() * ring;
  vector candidate = ACR_Manager.Ground(spot);
  vector free;
  if (ACR_BaseSystem.ClearGround(free, candidate, 15, clearance)) return free;
  return SpawnPoint(base, 0, ring, clearance);
 }

 string Purchase(SCR_PlayerController pc, int entryIndex, string baseLabel, vector spot = "0 0 0", bool hasSpot = false)
 {
  Faction faction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
  if (!faction) return "You have no faction";
  if (!m_Manager.IsCommander(pc.GetPlayerId())) return "Only the commander can requisition units";
  string key = faction.GetFactionKey();
  int before = m_Manager.m_Bases.GetRP(key);
  ACR_Base spawnBase = FindSpawnBase(key, baseLabel);
  string result = PurchaseAt(key, entryIndex, spawnBase, spot, hasSpot);
  if (m_Manager.m_Bases.GetRP(key) < before && spawnBase)
  {
   string unit = m_Manager.m_Bases.GetFaction(key).m_aRequisitions[entryIndex].m_sName;
   m_Manager.LogSpend(pc, key, before, "requisitioned " + unit + " at " + spawnBase.m_sName);
  }
  return result;
 }

 // Shared by the player commander and the AI commander.
 // hasSpot: the commander clicked a point in the base ring; the unit appears there
 // (the AI commander and old callers get a random clear point in the ring).
 string PurchaseAt(string key, int entryIndex, ACR_Base base, vector spot = "0 0 0", bool hasSpot = false)
 {
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(key);
  if (entryIndex < 0 || entryIndex >= data.m_aRequisitions.Count()) return "Unknown unit";
  ACR_RequisitionEntry entry = data.m_aRequisitions[entryIndex];
  if (entry.m_iRemaining <= 0) return entry.m_sName + ": none left";
  if (!base || base.m_sOwner != key) return "Deploy at one of your own bases";
  if (ACR_LogisticsRules.RelayBlocked(base)) return base.m_sName + " needs a completed friendly radio relay inside its circle before deploying new units";
  if (!ACR_LogisticsRules.CanDeploy(base)) return base.m_sName + " cannot deploy new units (use a military base, HQ or FOB)";
  if (base.m_bContested) return base.m_sName + " is under attack";
  if (OverSoldierCap(key)) return string.Format("Too many soldiers in the field (limit %1)", m_Manager.m_iSoldierCap);
  if (!m_Manager.m_Supports) m_Manager.m_Supports = new ACR_Supports(m_Manager);
  if (entry.m_iSupportHeli > 0)
  {
   string padProblem = m_Manager.m_Supports.Station().CantBuy(key, base);
   if (!padProblem.IsEmpty()) return padProblem;
  }
  if (!m_Manager.m_Bases.Spend(key, entry.m_iCost)) return string.Format("Not enough RP (%1 needed)", entry.m_iCost);
  // The purchase is a transaction from here on (R01, 5 Oct review): RP, stock and the
  // price step are taken now and given back by Refund() if the unit never turns up.
  ACR_PendingPurchase purchase = new ACR_PendingPurchase();
  purchase.m_sFaction = key;
  purchase.m_Entry = entry;
  purchase.m_iPaid = entry.m_iCost;
  entry.m_iRemaining--;
  entry.m_iBought++;
  entry.m_iNamed++;
  ACR_Pricing.Reprice(entry);
  // Names count every purchase ever made (a refunded one isn't reused, so two units
  // never share a name when a pending purchase fails after a later one).
  string name = string.Format("%1 %2", entry.m_sName, entry.m_iNamed);
  purchase.m_sName = name;
  int paid = purchase.m_iPaid;

  vector vehiclePos, groupPos;
  if (hasSpot)
  {
   vehiclePos = SpotInBase(base, spot, 4);
   // Infantry alone stands on the click; with a vehicle, beside it.
   if (entry.m_sVehicle.IsEmpty()) groupPos = SpotInBase(base, spot, 1.5);
   else groupPos = SpotInBase(base, vehiclePos + Vector(6, 0, 6), 1.5);
   PrintFormat("[ACR] PURCHASE spot %1 -> vehicle %2 group %3", spot, vehiclePos, groupPos);
  }
  else
  {
   vehiclePos = SpawnPoint(base, 10, 25, 4);
   groupPos = SpawnPoint(base, 5, 15, 1.5);
  }
  // Bought helicopters appear on a free helipad of the base when it has one.
  if (entry.m_bAircraft && entry.m_iSupportHeli == 0 && !entry.m_sVehicle.IsEmpty())
  {
   ACR_Helipad pad = m_Manager.m_Supports.Station().ClearPad(key, base);
   if (pad)
   {
    vehiclePos = pad.Spot();
    PrintFormat("[ACR] PURCHASE %1 on the helipad at %2", entry.m_sName, vehiclePos);
   }
  }
  if (entry.m_iSupportHeli > 0)
  {
   if (!m_Manager.m_Supports.Station().Station(key, entry.m_iSupportHeli == 1, base, name))
   {
    Refund(purchase, "no helipad");
    return name + " could not be placed on a helipad";
   }
   PrintFormat("[ACR] PURCHASE %1 at %2 for %3 RP (%4 left, next %5 RP)", name, base.m_sName, paid, entry.m_iRemaining, entry.m_iCost);
   return string.Format("%1 waiting on the helipad at %2", name, base.m_sName);
  }
  bool spawned;
  if (entry.m_bMortarTeam) spawned = m_Manager.Mortars().SpawnTeam(key, name, groupPos);
  else if (entry.m_bObserverTeam) spawned = m_Manager.FireMissions().SpawnTeam(key, name, groupPos);
  else
  {
   ResourceName group = ACR_FactionCatalog.ResolvePrefab(key, entry.m_sGroup, EEntityCatalogType.GROUP);
   ResourceName vehicle = ACR_FactionCatalog.ResolvePrefab(key, entry.m_sVehicle, EEntityCatalogType.VEHICLE);
   ResourceName passengers = ACR_FactionCatalog.ResolvePrefab(key, entry.m_sPassengers, EEntityCatalogType.GROUP);
   spawned = SpawnParts(purchase, group, vehicle, entry.m_bCrewed, passengers, vehiclePos, groupPos);
  }
  if (!spawned)
  {
   if (purchase.m_sError.IsEmpty()) purchase.m_sError = "its crew did not spawn";
   bool retained = Rollback(purchase);
   if (retained) return string.Format("%1 could not be fully deployed (%2); its vehicle was retained, so %3 RP was not refunded", name, purchase.m_sError, paid);
   Refund(purchase, purchase.m_sError);
   return string.Format("%1 could not be deployed (%2); %3 RP refunded", name, purchase.m_sError, paid);
  }
  // Delayed crews/members: watched until the unit is usable, refunded after a timeout.
  if (purchase.NeedsWatch())
  {
   purchase.m_fDeadline = GetGame().GetWorld().GetWorldTime() + s_fPendingTimeoutMs;
   m_aPending.Insert(purchase);
  }
  PrintFormat("[ACR] PURCHASE %1 at %2 for %3 RP (%4 left, next %5 RP)", name, base.m_sName, paid, entry.m_iRemaining, entry.m_iCost);
  return string.Format("%1 deploying at %2", name, base.m_sName);
 }

 // ---------------------------------------------------------------- purchase transactions

 static float s_fPendingTimeoutMs = 30000; // a bought unit with nobody in it after this is refunded (tests shorten it)
 static bool s_bTestNoCrew; // tests: crewed vehicles get no crew (a crew that never turns up)
 ref array<ref ACR_PendingPurchase> m_aPending = {};
 // Tests: the next N soldier/vehicle/group spawns in buying and reinforcing fail.
 static int s_iTestFailSpawns;
 static int s_iTestPassSpawns; // ...after letting this many through

 static IEntity SpawnForPurchase(ResourceName prefab, vector pos)
 {
  if (s_iTestFailSpawns > 0)
  {
   if (s_iTestPassSpawns > 0) s_iTestPassSpawns--;
   else { s_iTestFailSpawns--; return null; }
  }
  return ACR_Manager.SpawnPrefab(prefab, pos);
 }

 // Spawns each prefab into the group; false (and everything it made removed) unless
 // every one of them joined.
 static bool FillGroup(SCR_AIGroup group, notnull array<ResourceName> men, vector pos)
 {
  array<IEntity> made = {};
  bool all = true;
  IEntity navAgent = ACR_Garrison.AnyAgent();
  foreach (int i, ResourceName prefab : men)
  {
   // A line beside the spot, each man on his own clear ground (#87: not inside a heap).
   vector at = ACR_Manager.Ground(pos + Vector(1.5 * i, 0, 1));
   if (!ACR_BaseSystem.RoomAt(at) || ACR_BaseSystem.Covered(at)) at = ACR_BaseSystem.SoldierSpot(at, 2, navAgent);
   IEntity man = SpawnForPurchase(prefab, at);
   if (!man) { all = false; break; }
   made.Insert(man);
   if (!group.AddAIEntityToGroup(man)) { all = false; break; }
  }
  if (all) return true;
  foreach (IEntity man : made) if (man) SCR_EntityHelper.DeleteEntityAndChildren(man);
  SCR_EntityHelper.DeleteEntityAndChildren(group);
  return false;
 }

 // Spawns a named unit: an infantry group, a vehicle (optionally crewed; the crew squad
 // takes the name), and/or passengers that board the vehicle. Every part the catalog
 // entry names must appear, or the purchase fails (all or nothing); what was made is
 // recorded on the purchase so a failure removes only its own entities.
 bool SpawnParts(ACR_PendingPurchase purchase, ResourceName group, ResourceName vehicleprefab, bool crewed, ResourceName passengers, vector vehiclePos, vector groupPos)
 {
  string faction = purchase.m_sFaction;
  string name = purchase.m_sName;
  IEntity vehicle;
  if (!vehicleprefab.IsEmpty())
  {
   vehicle = SpawnForPurchase(vehicleprefab, vehiclePos);
   if (!vehicle) { purchase.m_sError = "vehicle did not spawn"; return false; }
   purchase.m_Vehicle = vehicle;
   if (m_Manager.m_Cleanup) m_Manager.m_Cleanup.Watch(vehicle);
   ACR_Vehicles.SetOwner(vehicle, faction);
   if (crewed)
   {
    purchase.m_bCrewed = true;
    m_Manager.m_PendingCrewNames.Set(vehicle, name);
    SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
    if (!compartments) { purchase.m_sError = "vehicle has no seats for a crew"; return false; }
    array<ECompartmentType> seats = {ECompartmentType.PILOT, ECompartmentType.TURRET};
    if (!s_bTestNoCrew) compartments.SpawnDefaultOccupants(seats);
    SetOccupantFaction(vehicle, faction);
    if (!s_bTestNoCrew) GetGame().GetCallqueue().CallLater(SetOccupantFactionLater, 500, false, vehicle, faction);
    // A construction truck's crew gets the building shovel (the default crew has none).
    if ((ACR_Logistics.ServiceOfName(vehicleprefab) & ACR_EService.CONSTRUCTION) != 0)
     GetGame().GetCallqueue().CallLater(GiveShovels, 1500, false, vehicle, faction);
   }
  }
  if (!group.IsEmpty())
  {
   ACR_RequisitionEntry formation = purchase.m_Entry;
   if (!formation) formation = ACR_InfantryCatalog.Starting(faction, group);
   SCR_AIGroup spawned = SpawnFormation(faction, group, formation, groupPos);
   if (!spawned) { purchase.m_sError = "group did not spawn"; return false; }
   SetUnitFaction(spawned, faction);
   purchase.m_Group = spawned;
   spawned.SetCustomName(name, 0);
  }
  if (!passengers.IsEmpty())
  {
   ACR_RequisitionEntry passengerFormation;
   if (!purchase.m_Entry) passengerFormation = ACR_InfantryCatalog.Starting(faction, passengers);
   SCR_AIGroup riders = SpawnFormation(faction, passengers, passengerFormation, groupPos);
   if (!riders) { purchase.m_sError = "passengers did not spawn"; return false; }
   SetUnitFaction(riders, faction);
   purchase.m_Riders = riders;
   riders.SetCustomName(name + " infantry", 0);
   if (vehicle) m_Manager.m_PendingBoarding.Set(riders, vehicle);
  }
  return true;
 }

 // The same native group construction is used for purchased infantry and starting riders.
 // A content pack's soldier swaps (ACR_ContentSocket.Soldier) turn a catalog group into a
 // formation of the swapped soldiers, which is then also its reinforcement recipe.
 protected SCR_AIGroup SpawnFormation(string faction, ResourceName group, ACR_RequisitionEntry formation, vector position)
 {
  array<ResourceName> soldiers = {};
  if (formation && formation.m_aSoldiers) soldiers.Copy(formation.m_aSoldiers);
  ACR_ContentSocket.Get().SwapSoldiers(faction, group, soldiers);
  bool customFormation = !soldiers.IsEmpty();
  if (customFormation) SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup spawned = SCR_AIGroup.Cast(SpawnForPurchase(group, position));
  if (customFormation) SCR_AIGroup.IgnoreSpawning(false);
  if (spawned && customFormation)
  {
   if (!spawned.m_aUnitPrefabSlots) spawned.m_aUnitPrefabSlots = {};
   spawned.m_aUnitPrefabSlots.Copy(soldiers);
   if (formation) spawned.m_eACRFormation = formation.m_eFormation;
   if (!FillGroup(spawned, soldiers, position)) return null;
  }
  else if (spawned) ACR_BaseSystem.SettleSoon(spawned); // the game's own formation spawn
  return spawned;
 }

 // Units that aren't bought (starting units, tests): same spawning, no transaction.
 // The side is m_Manager.m_sSpawningFaction, as before.
 bool SpawnUnit(string name, ResourceName group, ResourceName vehicleprefab, bool crewed, ResourceName passengers, vector vehiclePos, vector groupPos)
 {
  ACR_PendingPurchase unit = new ACR_PendingPurchase();
  unit.m_sName = name;
  unit.m_sFaction = m_Manager.m_sSpawningFaction;
  bool ok = SpawnParts(unit, group, vehicleprefab, crewed, passengers, vehiclePos, groupPos);
  if (!ok) PrintFormat("[ACR] SPAWN %1 incomplete: %2", name, unit.m_sError);
  return ok;
 }

 // Removes what this purchase made (and only that): crews and members with their groups,
 // the vehicle unless a player sits in it, and its pending crew-name/boarding entries.
 bool Rollback(ACR_PendingPurchase purchase)
 {
  bool retainedForPlayer = purchase.HasPlayerOccupant();
  if (purchase.m_Vehicle)
  {
   m_Manager.m_PendingCrewNames.Remove(purchase.m_Vehicle);
   array<IEntity> crew = {};
   ACR_PendingPurchase.Occupants(purchase.m_Vehicle, crew);
   bool player;
   foreach (IEntity man : crew) if (EntityUtils.IsPlayer(man)) player = true;
   if (!player)
   {
    foreach (IEntity man : crew) SCR_EntityHelper.DeleteEntityAndChildren(man);
    SCR_EntityHelper.DeleteEntityAndChildren(purchase.m_Vehicle);
   }
  }
  RemoveGroup(purchase.m_Group);
  if (purchase.m_Riders) m_Manager.m_PendingBoarding.Remove(purchase.m_Riders);
  RemoveGroup(purchase.m_Riders);
  return retainedForPlayer;
 }

 protected static void RemoveGroup(SCR_AIGroup group)
 {
  if (!group) return;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man && !EntityUtils.IsPlayer(man)) SCR_EntityHelper.DeleteEntityAndChildren(man);
  }
  SCR_EntityHelper.DeleteEntityAndChildren(group);
 }

 // Gives back exactly what this purchase took: its own RP, one unit of stock and one
 // price step (later purchases keep theirs; names are never reused).
 void Refund(ACR_PendingPurchase purchase, string why)
 {
  m_Manager.m_Bases.Refund(purchase.m_sFaction, purchase.m_iPaid);
  ACR_RequisitionEntry entry = purchase.m_Entry;
  if (entry)
  {
   entry.m_iRemaining++;
   entry.m_iBought = Math.Max(0, entry.m_iBought - 1);
   ACR_Pricing.Reprice(entry);
  }
  purchase.m_bRefunded = true;
  PrintFormat("[ACR] PURCHASE %1 failed (%2): %3 RP refunded, next %4 RP", purchase.m_sName, why, purchase.m_iPaid, entry.m_iCost);
 }

 // Every tick: bought units waiting for their crew/members. Ready = every part has
 // somebody in it; after s_fPendingTimeoutMs without that the purchase is undone.
 void TickPending()
 {
  if (m_aPending.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = m_aPending.Count() - 1; i >= 0; i--)
  {
   ACR_PendingPurchase purchase = m_aPending[i];
   if (purchase.Ready())
   {
    PrintFormat("[ACR] PURCHASE %1 ready", purchase.m_sName);
    m_aPending.RemoveOrdered(i);
    continue;
   }
   if (now < purchase.m_fDeadline) continue;
   m_aPending.RemoveOrdered(i);
   bool retained = Rollback(purchase);
   if (retained)
   {
    PrintFormat("[ACR] PURCHASE %1 failed to fully deploy; vehicle retained for player, no refund", purchase.m_sName);
    if (m_Manager.Command()) m_Manager.Command().Feed(purchase.m_sFaction, string.Format("%1 did not fully deploy; its vehicle was retained, so no RP was refunded", purchase.m_sName));
    continue;
   }
   Refund(purchase, "nobody turned up in time");
   if (m_Manager.Command()) m_Manager.Command().Feed(purchase.m_sFaction, string.Format("%1 failed to deploy; %2 RP refunded", purchase.m_sName, purchase.m_iPaid));
  }
 }

 // Soldiers of pending purchases not yet counted by the squad registry.
 int PendingSoldiers(string faction)
 {
  int count;
  foreach (ACR_PendingPurchase purchase : m_aPending)
   if (purchase.m_sFaction == faction) count += purchase.Unregistered(m_Manager);
  return count;
 }

 // The soldier limit is a soft threshold (Jacob, 5 Oct): nothing more is bought or
 // reinforced once a side has this many in the field, counting units still spawning.
 bool OverSoldierCap(string faction)
 {
  if (!m_Manager.m_Cleanup) return false;
  return m_Manager.m_Cleanup.SoldiersOf(faction) + PendingSoldiers(faction) >= m_Manager.m_iSoldierCap;
 }

 static const ResourceName SHOVEL_WEST = "{6E35D94130954509}Prefabs/Items/Equipment/Accessories/ETool_ALICE/ETool_ALICE_FreeRoamBuilding_Gadget.et";
 static const ResourceName SHOVEL_EAST = "{062E2F1D7F6739D6}Prefabs/Items/Equipment/Accessories/ETool_MPL50/ETool_MPL50_FreeRoamBuilding_Gadget.et";
 // Returns how many crewmen got one.
 int GiveShovels(IEntity vehicle, string factionKey)
 {
  if (!vehicle) return 0;
  ResourceName shovel = SHOVEL_EAST;
  if (ACR_Supports.IsWestern(factionKey)) shovel = SHOVEL_WEST;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return 0;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  int given;
  foreach (BaseCompartmentSlot slot : slots)
  {
   IEntity crewman = slot.GetOccupant();
   if (!crewman || HasShovel(crewman)) continue;
   FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(crewman.FindComponent(FactionAffiliationComponent));
   if (affiliation && affiliation.GetAffiliatedFaction())
   {
    shovel = SHOVEL_EAST;
    if (ACR_Supports.IsWestern(affiliation.GetAffiliatedFaction().GetFactionKey())) shovel = SHOVEL_WEST;
   }
   SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(crewman.FindComponent(SCR_InventoryStorageManagerComponent));
   if (!inventory) continue;
   bool ok = inventory.TrySpawnPrefabToStorage(shovel, null, -1, EStoragePurpose.PURPOSE_GADGET_PROXY);
   if (!ok) ok = inventory.TrySpawnPrefabToStorage(shovel);
   if (ok) given++;
  }
  PrintFormat("[ACR] Construction crew given shovels: %1", given);
  return given;
 }
 static string s_sLastShovel;
 static bool HasShovel(IEntity character)
 {
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return false;
  array<IEntity> items = {};
  inventory.GetItems(items);
  foreach (IEntity item : items)
  {
   if (!item || !item.GetPrefabData()) continue;
   string prefab = item.GetPrefabData().GetPrefabName();
   // (the pouch, "ETool_..._carrier", is not the shovel)
   if (prefab.Contains("ETool") && !prefab.Contains("carrier")) { s_sLastShovel = prefab; return true; }
  }
  return false;
 }

 static int REINFORCE_CLEAR_RADIUS = 300; // no Reinforce with enemy troops this close (tests may change it)

 // Refusal text when enemy troops are too close to the squad to reinforce it ("" = clear).
 string InContact(ACR_Squad squad)
 {
  IEntity anchor = squad.GetAnchor();
  if (!anchor || !squad.GetFaction()) return "";
  if (!m_Manager.m_Supports) m_Manager.m_Supports = new ACR_Supports(m_Manager);
  if (!m_Manager.m_Supports.FindEnemyNear(squad.GetFaction().GetFactionKey(), anchor.GetOrigin(), REINFORCE_CLEAR_RADIUS)) return "";
  PrintFormat("[ACR] REINFORCE %1 refused: enemy within %2 m", squad.GetName(), REINFORCE_CLEAR_RADIUS);
  return string.Format("Can't reinforce %1: enemy troops within %2 m. Pull back or clear the area first", squad.GetName(), REINFORCE_CLEAR_RADIUS);
 }

 string Reinforce(SCR_PlayerController pc, RplId groupId)
 {
  ACR_Squad squad = m_Manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId)));
  if (!squad || !squad.m_Group || !squad.GetFaction()) return "Select a friendly squad first";
  if (SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId()) != squad.GetFaction()) return "That squad belongs to another faction";
  if (!m_Manager.IsCommander(pc.GetPlayerId())) return "Only the commander can reinforce squads";
  return ReinforceSquad(pc, squad);
 }

 // The reinforcement itself, once the player may order it (pc may be null in tests).
 string ReinforceSquad(SCR_PlayerController pc, ACR_Squad squad)
 {
  string key = squad.GetFaction().GetFactionKey();
  array<ResourceName> missing = {};
  squad.GetMissing(missing);
  IEntity anchor = squad.GetAnchor();
  vector near = vector.Zero;
  if (anchor) near = anchor.GetOrigin();
  // No reinforcing in contact (Jacob, 4 Oct): nothing while enemy troops are within 300 m.
  string contact = InContact(squad);
  if (contact != "") return contact;
  ACR_Base localFob = ACR_FobSupply.Nearby(key, near);
  if (ACR_LogisticsRules.RelayBlocked(localFob)) return localFob.m_sName + " needs a completed friendly radio relay inside its circle before RP reinforcement";
  // Inside a base that can deploy units, the squad also gets its ammo back.
  ACR_Base depot = DeployBaseAt(key, near);
  // A mortar team whose mortar was destroyed gets a new one with its reinforcements.
  ACR_MortarTeam mortar = m_Manager.Mortars().TeamOf(squad);
  bool newTube = mortar && mortar.m_bLost;
  if (newTube && depot && !ACR_LogisticsRules.CanDeploy(depot)) return "A replacement mortar needs a military base, HQ or FOB";
  // A full team gets its new tube where reinforcements could deploy, as a team missing men does.
  if (missing.IsEmpty() && newTube && !depot) return "A replacement mortar needs a military base, HQ or FOB: bring the team in";
  if (missing.IsEmpty() && newTube)
  {
   if (!m_Manager.m_Bases.Spend(key, ACR_Mortars.TUBE_COST)) return string.Format("Not enough RP for a new mortar (%1 needed)", ACR_Mortars.TUBE_COST);
   m_Manager.LogSpend(pc, key, m_Manager.m_Bases.GetRP(key) + ACR_Mortars.TUBE_COST, "a new mortar for " + squad.GetName());
   m_Manager.Mortars().Rearmed(mortar);
   return squad.GetName() + ": a new mortar is on its way, they set up again once they stop";
  }
  // Its vehicles inside the base are rearmed and repaired, priced by what's missing (Jacob, 2 Oct).
  array<IEntity> serviced = {};
  int vehicleCost;
  if (depot && ACR_LogisticsRules.CanDeploy(depot))
  {
   SquadVehiclesIn(squad, depot, serviced);
   foreach (IEntity vehicle : serviced) vehicleCost += ServiceCost(vehicle);
  }
  if (missing.IsEmpty())
  {
   if (!depot) return squad.GetName() + " is at full strength (ammo only inside a base that can deploy units)";
   int instantCost = InstantRestockCost(squad, depot);
   if (m_Manager.m_Bases.GetRP(key) < vehicleCost + instantCost) return "Not enough RP for instant service; use supplied FOB service buildings or replenish RP";
   if (vehicleCost > 0)
   {
    if (!m_Manager.m_Bases.Spend(key, vehicleCost)) return string.Format("Not enough RP to rearm and repair %1 (%2 needed)", squad.GetName(), vehicleCost);
    m_Manager.LogSpend(pc, key, m_Manager.m_Bases.GetRP(key) + vehicleCost, "rearmed and repaired " + squad.GetName());
   }
   int restocked = RestockAmmo(squad, depot);
   int fixedUp = ServiceVehicles(serviced);
   PrintFormat("[ACR] REINFORCE %1 ammo only at %2: %3 soldiers restocked, %4 vehicles rearmed/repaired for %5 RP", squad.GetName(), depot.m_sName, restocked, fixedUp, vehicleCost);
   if (!restocked && !fixedUp) return squad.GetName() + " is at full strength and fully stocked";
   string done = string.Format("%1 at %2:", squad.GetName(), depot.m_sName);
   if (restocked) done += string.Format(" %1 soldiers healed and restocked", restocked);
   if (fixedUp) done += string.Format(" vehicle rearmed and repaired (%1 RP)", vehicleCost);
   return done;
  }
  ACR_Base base = depot;
  if (!base) base = NearestSpawnBase(key, near);
  if (!base) return "No friendly base can send reinforcements";
  if (newTube && !ACR_LogisticsRules.CanDeploy(base)) return "A replacement mortar needs a military base, HQ or FOB";
  // The soldier limit counts reinforcements too (soft threshold, as for buying).
  if (OverSoldierCap(key)) return string.Format("Too many soldiers in the field (limit %1)", m_Manager.m_iSoldierCap);
  int extras = vehicleCost;
  if (newTube) extras += ACR_Mortars.TUBE_COST;
  int most = missing.Count() * REINFORCE_COST_PER_SOLDIER + extras;
  if (depot) most += InstantRestockCost(squad, depot);
  if (m_Manager.m_Bases.GetRP(key) < most) return string.Format("Not enough RP (%1 needed)", most);
  // Paid per soldier who actually joined (Jacob, 5 Oct; R01 review): spawn first, then
  // charge. Vehicle service and a new mortar are charged on their own.
  int delivered;
  foreach (ResourceName prefab : missing)
  {
   IEntity soldier = SpawnForPurchase(prefab, SpawnPoint(base, 4, 12, 0.8));
   if (!soldier) continue;
   if (!squad.m_Group.AddAIEntityToGroup(soldier)) { SCR_EntityHelper.DeleteEntityAndChildren(soldier); continue; }
   delivered++;
  }
  int failed = missing.Count() - delivered;
  int cost = delivered * REINFORCE_COST_PER_SOLDIER + extras;
  int rpBefore = m_Manager.m_Bases.GetRP(key);
  if (cost > 0 && m_Manager.m_Bases.Spend(key, cost)) m_Manager.LogSpend(pc, key, rpBefore, "reinforced " + squad.GetName());
  if (failed > 0) PrintFormat("[ACR] REINFORCE %1: %2 of %3 replacements could not be spawned (not charged)", squad.GetName(), failed, missing.Count());
  int resupplied;
  if (depot) resupplied = RestockAmmo(squad, depot);
  if (depot && ServiceVehicles(serviced) > 0) resupplied++;
  PrintFormat("[ACR] REINFORCE %1 +%2 of %3 from %4 for %5 RP, %6 restocked", squad.GetName(), delivered, missing.Count(), base.m_sName, cost, resupplied);
  string result = string.Format("%1: %2 reinforcements from %3", squad.GetName(), delivered, base.m_sName);
  if (failed > 0) result = string.Format("%1: %2 of %3 reinforcements from %4 (%5 couldn't be sent, not charged)", squad.GetName(), delivered, missing.Count(), base.m_sName, failed);
  if (newTube)
  {
   m_Manager.Mortars().Rearmed(mortar);
   result += " with a new mortar";
  }
  if (resupplied) result += string.Format(", %1 healed, restocked or rearmed", resupplied);
  return result;
 }

 static const float REPAIR_COST_FULL = 15; // RP to repair a vehicle from wrecked to whole

 // The squad's vehicles (its own and any it crews) standing inside the base circle.
 void SquadVehiclesIn(ACR_Squad squad, ACR_Base base, notnull array<IEntity> result)
 {
  result.Clear();
  array<IEntity> found = {};
  squad.GetVehicles(found);
  if (squad.m_OwnVehicle && !found.Contains(squad.m_OwnVehicle)) found.Insert(squad.m_OwnVehicle);
  foreach (IEntity vehicle : found)
  {
   if (!vehicle || !Vehicle.Cast(vehicle) || ACR_Logistics.Health(vehicle) <= 0) continue;
   if (vector.DistanceXZ(vehicle.GetOrigin(), base.GetOrigin()) <= base.m_fRadius) result.Insert(vehicle);
  }
 }

 // RP to rearm and repair a vehicle: each gun's missing share of a load times what that ammo
 // is worth (missiles and rockets dearest, then cannon, then machine guns), plus the damage.
 static int ServiceCost(IEntity vehicle)
 {
  float cost;
  array<BaseMagazineComponent> magazines = {};
  ACR_Logistics.TurretMagazines(vehicle, magazines);
  foreach (BaseMagazineComponent magazine : magazines)
  {
   int max = magazine.GetMaxAmmoCount();
   if (max <= 0) continue;
   float empty = 1 - magazine.GetAmmoCount() / (float)max;
   cost += empty * AmmoWorth(magazine);
  }
  // Engine, rotor and wheel damage is repaired too (ServiceVehicles), so it is priced too.
  float worn = 1 - ACR_Logistics.Health(vehicle);
  SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(vehicle.FindComponent(SCR_DamageManagerComponent));
  if (damage) worn = Math.Max(worn, damage.GetMovementDamage());
  if (worn > 0.01) cost += worn * REPAIR_COST_FULL;
  return Math.Ceil(cost);
 }

 static float AmmoWorth(BaseMagazineComponent magazine)
 {
  string name = "";
  IEntity item = magazine.GetOwner();
  if (item && item.GetPrefabData()) name = item.GetPrefabData().GetPrefabName();
  name.ToLower();
  if (name.Contains("atgm") || name.Contains("rocket") || name.Contains("missile") || name.Contains("9m1") || name.Contains("tow") || name.Contains("spg")) return 10;
  if (name.Contains("x137") || name.Contains("x165") || name.Contains("23x") || name.Contains("30x") || name.Contains("25x") || name.Contains("73mm") || name.Contains("cannon")) return 6;
  return 2;
 }

 // Full guns and full health. Returns how many vehicles needed something.
 int ServiceVehicles(array<IEntity> vehicles)
 {
  int served;
  foreach (IEntity vehicle : vehicles)
  {
   bool needed;
   array<BaseMagazineComponent> magazines = {};
   ACR_Logistics.TurretMagazines(vehicle, magazines);
   foreach (BaseMagazineComponent magazine : magazines)
   {
    if (magazine.GetAmmoCount() >= magazine.GetMaxAmmoCount()) continue;
    magazine.SetAmmoCount(magazine.GetMaxAmmoCount());
    needed = true;
   }
   SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.Cast(vehicle.FindComponent(SCR_DamageManagerComponent));
   // Engine, rotor or wheel damage counts too: a gunship at full health with its engine
   // shot was "damaged on its pad" again after every service (Everon night soak 26 Oct:
   // 36 cycles in 36 min, the side never had it back or bought another).
   if (damage && !damage.IsDestroyed() && (damage.GetHealthScaled() < 0.99 || damage.GetMovementDamage() > 0.01))
   {
    damage.HealHitZones(damage.GetMaxHealth() * 10);
    damage.FullHeal();
    damage.SetHealthScaled(1);
    damage.SetMovementDamage(0);
    needed = true;
   }
   if (needed) served++;
  }
  return served;
 }

 // A friendly, uncontested infantry-replacement base, with the point inside its circle.
 ACR_Base DeployBaseAt(string factionKey, vector position)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != factionKey || !ACR_LogisticsRules.CanReinforce(base) || base.m_bContested) continue;
   if (vector.DistanceXZ(base.GetOrigin(), position) <= base.m_fRadius) return base;
  }
  return null;
 }

 // Budget the optional instant FOB infantry-service fee before replacements/vehicle charges.
 int InstantRestockCost(ACR_Squad squad, ACR_Base base)
 {
  if (!base || !base.m_bBuilt || !ACR_LogisticsRules.SuppliedServices()) return 0;
  int count;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (!soldier || !ACR_Respawn.IsAlive(soldier) || vector.DistanceXZ(soldier.GetOrigin(), base.GetOrigin()) > base.m_fRadius) continue;
   SCR_CharacterDamageManagerComponent wounds = SCR_CharacterDamageManagerComponent.Cast(soldier.FindComponent(SCR_CharacterDamageManagerComponent));
   if ((wounds && (wounds.GetHealthScaled() < 0.99 || wounds.IsBleeding())) || ACR_Logistics.SoldierMagazines(soldier) < squad.MagazineTarget(soldier) || (m_Manager.m_Launchers && m_Manager.m_Launchers.IsOwed(soldier))) count++;
  }
  return count * ACR_LogisticsRules.InstantServiceFee();
 }

 // Heal/restock inside a reinforcement base. Supplied-mode FOBs charge RP only
 // for soldiers actually served; the manual action never needs automatic-service supplies.
 int RestockAmmo(ACR_Squad squad, ACR_Base base)
 {
  if (ACR_LogisticsRules.RelayBlocked(base)) return 0;
  int served;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (!soldier || !ACR_Respawn.IsAlive(soldier)) continue;
   if (vector.DistanceXZ(soldier.GetOrigin(), base.GetOrigin()) > base.m_fRadius) continue;
   int fee = 0;
   if (base.m_bBuilt && ACR_LogisticsRules.SuppliedServices()) fee = ACR_LogisticsRules.InstantServiceFee();
   string side = squad.GetFaction().GetFactionKey();
   if (m_Manager.m_Bases.GetRP(side) < fee) continue;
   bool got;
   // ...and the wounded patched up (Jacob, 3 Oct: the crew wasn't healed).
   SCR_CharacterDamageManagerComponent wounds = SCR_CharacterDamageManagerComponent.Cast(soldier.FindComponent(SCR_CharacterDamageManagerComponent));
   if (wounds && (wounds.GetHealthScaled() < 0.99 || wounds.IsBleeding()))
   {
    wounds.FullHeal();
    got = true;
   }
   if (m_Manager.m_Launchers && m_Manager.m_Launchers.IsOwed(soldier) && m_Manager.m_Launchers.Rearm(soldier)) got = true;
   int target = squad.MagazineTarget(soldier);
   if (ACR_Logistics.SoldierMagazines(soldier) < target)
   {
    SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
    // Same count the ammo trucks hand out (the loaded magazine is the last one).
    int magazines = Math.Max(2, target - 1);
    if (inventory && inventory.IsResupplyMagazinesAvailable(magazines))
    {
     inventory.ResupplyMagazines(magazines);
     got = true;
    }
   }
   if (got)
   {
    if (fee > 0 && m_Manager.m_Bases.Spend(side, fee)) m_Manager.LogSpend(null, side, m_Manager.m_Bases.GetRP(side) + fee, "instant infantry service at " + base.m_sName);
    served++;
   }
  }
  return served;
 }

}
