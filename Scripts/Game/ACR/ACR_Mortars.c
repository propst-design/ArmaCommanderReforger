// Mortar teams (Checkpoint 2, from 2 Oct). A real crew with a real tube: bought like any
// unit ("Mortar team" in Requisition), walks (or rides) packed up, sets up by itself once it
// holds still, and packs up again before moving off. Mortar supports (HE, creeping, smoke,
// illumination) are fired by a deployed team that can reach the target: the game's own
// artillery AI (SCR_AIWaypointArtillerySupport) aims, loads and fires real shells.
// Server side; ticked once a second by ACR_Manager.
enum ACR_EMortarState
{
 PACKED,
 DEPLOYING,
 DEPLOYED,
 PACKING
}

class ACR_MortarTeam : Managed
{
 ACR_Squad m_Squad;
 SCR_AIGroup m_Group;
 string m_sFaction;
 string m_sName;
 ResourceName m_sTube;
 IEntity m_Tube;
 ACR_EMortarState m_eState = ACR_EMortarState.PACKED;
 float m_fStateUntil;
 float m_fStillSince;          // when it came to a halt (sets up DEPLOY_WAIT later)
 float m_fNextSpotTry;
 vector m_Spot;                // where the tube stands (or will)
 int m_iSerial = -1;           // the squad's order serial last seen
 ref ACR_OrderSnapshot m_Resume; // the order to carry out once packed up
 int m_iResumeBy;              // ...and who gave it
 float m_fResumeAt;
 bool m_bLost;                 // its tube was destroyed
 string m_sStatus = "packed up";
 // fire mission
 bool m_bFiring;
 float m_fMissionEnd;
 float m_fLastFinished;
 string m_sMission;
 int m_iRounds;
 IEntity m_Gunner;
 ResourceName m_sShell;
 int m_iFired;
 int m_iLoadingDismounts; // observed native aiming -> on-foot loading transitions
 int m_iReserveRounds; // paid mission rounds not yet issued to a gunner
 int m_iDryMissions;   // missions in a row that timed out with nothing fired (reset when the tube goes up again)
 int m_iPaid;          // RP this mission cost its side (given back if it fires nothing)
 bool m_bNoSpotLogged;
 ref map<IEntity, ResourceName> m_Backpacks = new map<IEntity, ResourceName>(); // carriers' own packs while they carry parts
 bool m_bPartsChecked;

 IEntity Anchor()
 {
  if (m_Squad) return m_Squad.GetAnchor();
  return null;
 }
}

class ACR_Mortars : Managed
{
 ACR_MortarTeam m_LastStarted; // the team the last Fire gave its mission to (Supports notes what it paid)
 static float DEPLOY_SECONDS = 30;
 static float PACK_SECONDS = 20;
 static float DEPLOY_WAIT = 3;          // holding still this long before setting up
 static const float MAX_SLOPE_DEG = 8;  // steeper ground: the baseplate floats or sinks visibly
 static const float PLATE_RADIUS = 0.7;
 static const float SEARCH_RADIUS = 40;  // m: wide enough to step out of a base's vehicle ring (CLEAR_OF_BASE)
 static const float CLEAR_OF_ROAD = 5;   // m from a road's edge (#33)
 static const float CLEAR_OF_BASE = 30;  // m from a base's centre: bought vehicles appear 10-25 m out (#33)
 static const float MIN_ELEVATION = 45;
 static const float MAX_ELEVATION = 85;
 static const float MISSION_TIMEOUT = 150;
 static const int DRY_MISSIONS = 2;      // missions in a row that fire nothing before the team is taken off fire missions
 static const float MIN_RANGE = 100;
 static const int COST = 30;
 static const int TUBE_COST = 10; // a new mortar with a Reinforce, after the old one was destroyed
 static const int STOCK = 2;
 static const string ENTRY_NAME = "Mortar team";

 static const ResourceName TUBE_WEST = "{8094D99689ABE241}Prefabs/Weapons/Mortars/M252/Mortar_M252.et";
 static const ResourceName TUBE_EAST = "{D1FFE458E8AC4BDB}Prefabs/Weapons/Mortars/2B14/Mortar_2B14.et";
 static const ResourceName WP_FIRE = "{C524700A27CFECDD}Prefabs/AI/Waypoints/AIWaypoint_ArtillerySupport.et";
 static const ResourceName GROUP_WEST = "{EACD97CF4A702FAE}Prefabs/Groups/BLUFOR/Group_US_Base.et";
 static const ResourceName GROUP_EAST = "{8DE0C0830FE0C33D}Prefabs/Groups/OPFOR/Group_USSR_Base.et";
 static const ResourceName GROUP_FIA = "{242BC3C6BCE96EA5}Prefabs/Groups/INDFOR/Group_FIA_Base.et";
 static const ResourceName CREW_WEST = "{E75A35996E5C4505}Prefabs/Characters/Campaign/Final/BLUFOR/US_army/Regular/Campaign_US_Player_MortarCrew.et";
 static const ResourceName CREW_EAST = "{86E565387A634103}Prefabs/Characters/Campaign/Final/OPFOR/USSR_Army/Regular/Campaign_USSR_Player_MortarCrew.et";
 // Packed up, two of the crew carry the baseplate and the barrel on their backs (Jacob, 2 Oct:
 // easier to spot a mortar team in first person).
 static const ResourceName BARREL_WEST = "{62B704DD79A3B5BE}Prefabs/Items/Equipment/Mortars/M252/Part_M252_Barrel.et";
 static const ResourceName PLATE_WEST = "{0D540E16A6E53433}Prefabs/Items/Equipment/Mortars/M252/Part_M252_BasePlate.et";
 static const ResourceName BARREL_EAST = "{F62E5FBCD723FF73}Prefabs/Items/Equipment/Mortars/2B14/Part_2B14_Barrel.et";
 static const ResourceName PLATE_EAST = "{4323046B2CC94818}Prefabs/Items/Equipment/Mortars/2B14/Part_2B14_BasePlate.et";
 static const ResourceName AMMO_FIA = "{48F96834C3289E04}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Ammo.et";
 static const ResourceName RIFLE_FIA = "{84B40583F4D1B7A3}Prefabs/Characters/Factions/INDFOR/FIA/Character_FIA_Rifleman.et";

 protected ACR_Manager m_Manager;
 ref array<ref ACR_MortarTeam> m_Teams = {};
 protected static ref map<ResourceName, float> s_MaxRange = new map<ResourceName, float>();
 static int s_iLastFired = -1; // rounds the last finished mission fired (tests)

 void ACR_Mortars(ACR_Manager manager) { m_Manager = manager; }

 static bool IsMortarType(ACR_ESupportType type)
 {
  return type == ACR_ESupportType.MORTAR_HE_SMALL || type == ACR_ESupportType.MORTAR_HE_LARGE || type == ACR_ESupportType.MORTAR_CREEPING || type == ACR_ESupportType.MORTAR_SMOKE || type == ACR_ESupportType.ILLUMINATION;
 }

 // ---------------------------------------------------------------- buying

 // The requisition entry every faction gets (ACR_FactionCatalog.BuildRequisitions).
 static ACR_RequisitionEntry MakeEntry()
 {
  ACR_RequisitionEntry entry = new ACR_RequisitionEntry();
  entry.Set(ENTRY_NAME, "", "", COST, STOCK);
  entry.m_bMortarTeam = true;
  entry.m_bArmed = false;
  return entry;
 }

 bool SpawnTeam(string factionKey, string name, vector pos)
 {
  ResourceName group = GROUP_EAST;
  // All mortar crewmen: whoever ends up nearest the tube (the game's gunner pick) can carry shells.
  array<ResourceName> crew = {CREW_EAST, CREW_EAST, CREW_EAST};
  ResourceName tube = TUBE_EAST;
  if (ACR_Supports.IsWestern(factionKey))
  {
   group = GROUP_WEST;
   crew = {CREW_WEST, CREW_WEST, CREW_WEST};
   tube = TUBE_WEST;
  }
  else if (factionKey.Contains("FIA"))
  {
   group = GROUP_FIA;
   crew = {AMMO_FIA, RIFLE_FIA, RIFLE_FIA};
  }
  // Exact mission-maker mappings can replace every piece of the mortar set. The
  // vanilla values above remain the fallback when a faction has no mapping.
  group = ACR_FactionCatalog.ResolvePrefab(factionKey, group, EEntityCatalogType.GROUP);
  for (int i = 0; i < crew.Count(); i++)
  {
   if (ACR_FactionCatalog.StandardPath(factionKey) != "") crew[i] = ACR_FactionCatalog.StandardRifleman(factionKey, crew[i]);
   else crew[i] = ACR_FactionCatalog.ResolvePrefab(factionKey, crew[i], EEntityCatalogType.CHARACTER);
  }
  tube = ACR_FactionCatalog.ResolvePrefab(factionKey, tube, EEntityCatalogType.VEHICLE);
  // The faction base groups come with their own members (USSR: six riflemen), which the
  // game's spawn queue keeps topping up to: the crew become the group's member slots instead.
  SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup spawned = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(group, pos));
  SCR_AIGroup.IgnoreSpawning(false);
  if (!spawned) return false;
  if (!spawned.m_aUnitPrefabSlots) spawned.m_aUnitPrefabSlots = {};
  spawned.m_aUnitPrefabSlots.Copy(crew);
  spawned.SetCustomName(name, 0);
  // All of the crew or nothing (R01, 5 Oct review): a team short of men is taken back
  // and the purchase refunded by the caller.
  if (!ACR_Requisition.FillGroup(spawned, crew, pos))
  {
   PrintFormat("[ACR] MORTAR team %1 (%2) failed to spawn its crew: removed", name, factionKey);
   return false;
  }
  ACR_MortarTeam team = new ACR_MortarTeam();
  team.m_Group = spawned;
  team.m_sFaction = factionKey;
  team.m_sName = name;
  team.m_sTube = tube;
  // A moment before setting up: the AI commander may send it on first.
  team.m_fNextSpotTry = GetGame().GetWorld().GetWorldTime() / 1000 + 12;
  m_Teams.Insert(team);
  PrintFormat("[ACR] MORTAR team %1 (%2) spawned at %3, %4 crew", name, factionKey, pos, spawned.GetAgentsCount());
  return true;
 }

 void TeamsOf(string factionKey, notnull array<ACR_MortarTeam> result)
 {
  result.Clear();
  foreach (ACR_MortarTeam team : m_Teams) if (team.m_sFaction == factionKey) result.Insert(team);
 }

 ACR_MortarTeam TeamOf(ACR_Squad squad)
 {
  foreach (ACR_MortarTeam team : m_Teams) if (team.m_Squad == squad) return team;
  return null;
 }

 // ---------------------------------------------------------------- tick

 void Tick()
 {
  float now = GetGame().GetWorld().GetWorldTime() / 1000;
  for (int i = m_Teams.Count() - 1; i >= 0; i--)
  {
   ACR_MortarTeam team = m_Teams[i];
   if (!team.m_Squad && team.m_Group)
   {
    team.m_Squad = m_Manager.FindSquadByGroup(team.m_Group);
    if (team.m_Squad)
    {
     team.m_Squad.m_bMortar = true;
     team.m_iSerial = team.m_Squad.m_iOrderSerial;
    }
   }
   // Packed teams carry the parts (also new members after a Reinforce).
   if (team.m_Squad && team.m_eState != ACR_EMortarState.DEPLOYED && !team.m_bLost && !team.m_bPartsChecked)
   {
    CarryParts(team, true);
    team.m_bPartsChecked = true;
   }
   bool gone = !team.m_Group || (team.m_Squad && (!m_Manager.m_Squads.Contains(team.m_Squad) || team.m_Squad.CountAlive() == 0));
   if (gone)
   {
    Tell(team, team.m_sName + " was wiped out", true);
    m_Teams.Remove(i);
    continue;
   }
   if (team.m_Squad) Update(team, now);
  }
 }

 protected void Tell(ACR_MortarTeam team, string line, bool toast = false)
 {
  PrintFormat("[ACR] MORTAR %1", line);
  m_Manager.Command().Feed(team.m_sFaction, line);
  if (toast) m_Manager.NotifyFaction(team.m_sFaction, line);
 }

 // Not standing still: an order that takes it somewhere, or riding.
 static bool IsMoving(ACR_Squad squad, vector from)
 {
  ACR_EOrderState state = squad.OrderState();
  if (state == ACR_EOrderState.MOVE || state == ACR_EOrderState.ASSAULT || state == ACR_EOrderState.SUPPRESS) return true;
  if (state == ACR_EOrderState.GET_IN || state == ACR_EOrderState.GET_OUT || state == ACR_EOrderState.HUNT) return true;
  if (state == ACR_EOrderState.REGROUP || state == ACR_EOrderState.RIDE || state == ACR_EOrderState.RETURN) return true;
  if (state == ACR_EOrderState.DISMOUNT || state == ACR_EOrderState.COMMANDED || state == ACR_EOrderState.DRIVEN) return true;
  if (state == ACR_EOrderState.DEFEND && vector.DistanceXZ(squad.m_MoveTarget, from) > 30) return true;
  return false;
 }

 protected bool Riding(ACR_Squad squad)
 {
  array<IEntity> vehicles = {};
  squad.GetVehicles(vehicles);
  foreach (IEntity vehicle : vehicles) if (Vehicle.Cast(vehicle)) return true;
  return false;
 }

 protected void Update(ACR_MortarTeam team, float now)
 {
  ACR_Squad squad = team.m_Squad;
  IEntity anchor = team.Anchor();
  if (!anchor) return;
  if ((team.m_eState == ACR_EMortarState.DEPLOYED || team.m_eState == ACR_EMortarState.PACKING) && TubeDestroyed(team.m_Tube))
  {
   team.m_bLost = true;
   team.m_bFiring = false;
   team.m_Tube = null;
   team.m_eState = ACR_EMortarState.PACKED;
   squad.SetOrder("Mortar destroyed", ACR_EOrderState.HOLD);
   Tell(team, team.m_sName + "'s mortar was destroyed (Reinforce brings a new one)", true);
  }
  if (squad.HasPlayer())
  {
   team.m_sStatus = "led by a player";
   team.m_iSerial = squad.m_iOrderSerial;
   return;
  }
  bool newOrder = squad.m_iOrderSerial != team.m_iSerial;
  team.m_iSerial = squad.m_iOrderSerial;
  switch (team.m_eState)
  {
   case ACR_EMortarState.PACKED:
   {
    if (team.m_bLost) { team.m_sStatus = "has lost its mortar"; return; }
    if (IsMoving(squad, anchor.GetOrigin()) || Riding(squad))
    {
     team.m_fStillSince = 0;
     team.m_sStatus = "packed up, moving";
     return;
    }
    if (team.m_fStillSince == 0) team.m_fStillSince = now;
    if (now - team.m_fStillSince < DEPLOY_WAIT || now < team.m_fNextSpotTry) return;
    vector spot;
    if (!FindSpot(anchor.GetOrigin(), spot))
    {
     team.m_fNextSpotTry = now + 10;
     team.m_sStatus = "no level ground to set up on";
     if (!team.m_bNoSpotLogged) Tell(team, team.m_sName + ": no level ground to set up the mortar here");
     team.m_bNoSpotLogged = true;
     return;
    }
    team.m_bNoSpotLogged = false;
    team.m_Spot = spot;
    team.m_eState = ACR_EMortarState.DEPLOYING;
    team.m_fStateUntil = now + DEPLOY_SECONDS;
    squad.IssueWaypoint(ACR_Squad.WP_DEFEND, spot, 8);
    squad.SetOrder("Setting up mortar", ACR_EOrderState.HOLD);
    squad.m_OrderKind = ACR_EOrderKind.DEFEND;
    PrintFormat("[ACR] MORTAR %1 setting up at %2 (%3 m from %4)", team.m_sName, spot, vector.DistanceXZ(spot, anchor.GetOrigin()), anchor.GetOrigin());
    return;
   }
   case ACR_EMortarState.DEPLOYING:
   {
    if (newOrder)
    {
     team.m_eState = ACR_EMortarState.PACKED;
     team.m_fStillSince = 0;
     PrintFormat("[ACR] MORTAR %1 set-up cancelled by a new order", team.m_sName);
     return;
    }
    team.m_sStatus = string.Format("setting up, %1 s", Math.Ceil(team.m_fStateUntil - now));
    if (now < team.m_fStateUntil) return;
    // End the setup Defend before anyone mounts the tube: deselection sends
    // get-out messages to turret occupants, even when the next order is artillery.
    squad.ClearOrders();
    team.m_Tube = SpawnTube(team);
    if (!team.m_Tube)
    {
     team.m_eState = ACR_EMortarState.PACKED;
     team.m_fNextSpotTry = now + 10;
     return;
    }
    team.m_eState = ACR_EMortarState.DEPLOYED;
    team.m_sStatus = "ready";
    CarryParts(team, false);
    squad.SetOrder("Mortar ready", ACR_EOrderState.HOLD);
    Tell(team, string.Format("%1 set up at %2, reaches %3 km", team.m_sName, ACR_Manager.Grid(team.m_Spot), Km(MaxRange(team.m_sTube, team.m_sFaction))));
    return;
   }
   case ACR_EMortarState.DEPLOYED:
   {
    if (newOrder && IsMoving(squad, team.m_Spot))
    {
     StartPacking(team, now);
     return;
    }
    if (team.m_bFiring) UpdateMission(team, now);
    else team.m_sStatus = "ready";
    return;
   }
   case ACR_EMortarState.PACKING:
   {
    if (newOrder)
    {
     if (IsMoving(squad, team.m_Spot)) SaveAndHalt(team);
     else
     {
      // Told to stay after all: back to the tube.
      team.m_Resume = null;
      team.m_eState = ACR_EMortarState.DEPLOYED;
      squad.ClearOrders();
      squad.SetOrder("Mortar ready", ACR_EOrderState.HOLD);
      return;
     }
    }
    team.m_sStatus = string.Format("packing up, %1 s", Math.Ceil(team.m_fStateUntil - now));
    if (now < team.m_fStateUntil) return;
    FinishPacking(team);
    return;
   }
  }
 }

 // A move order while set up: stand still for PACK_SECONDS, then carry it out.
 protected void StartPacking(ACR_MortarTeam team, float now)
 {
  ACR_Squad squad = team.m_Squad;
  EndMission(team, "packing up");
  // A ride can't wait (the transport is already on its way): pack at once.
  if (squad.OrderState() == ACR_EOrderState.RIDE || Riding(squad))
  {
   team.m_Resume = null;
   FinishPacking(team);
   return;
  }
  SaveAndHalt(team);
  team.m_eState = ACR_EMortarState.PACKING;
  team.m_fStateUntil = now + PACK_SECONDS;
  Tell(team, team.m_sName + " packing up the mortar");
 }

 protected void SaveAndHalt(ACR_MortarTeam team)
 {
  ACR_Squad squad = team.m_Squad;
  team.m_Resume = squad.SnapshotOrder();
  team.m_iResumeBy = squad.m_iOrderedBy;
  team.m_fResumeAt = squad.m_fOrderedAt;
  squad.Halt("Packing up mortar");
  squad.m_iOrderedBy = team.m_iResumeBy;
  squad.m_fOrderedAt = team.m_fResumeAt;
  team.m_iSerial = squad.m_iOrderSerial;
 }

 protected void FinishPacking(ACR_MortarTeam team)
 {
  ACR_Squad squad = team.m_Squad;
  if (team.m_Tube) SCR_EntityHelper.DeleteEntityAndChildren(team.m_Tube);
  team.m_Tube = null;
  team.m_eState = ACR_EMortarState.PACKED;
  team.m_fStillSince = 0;
  team.m_sStatus = "packed up";
  team.m_bPartsChecked = false;
  if (team.m_Resume)
  {
   squad.RestoreOrder(team.m_Resume);
   squad.m_iOrderedBy = team.m_iResumeBy;
   squad.m_fOrderedAt = team.m_fResumeAt;
  }
  team.m_Resume = null;
  team.m_iSerial = squad.m_iOrderSerial;
  PrintFormat("[ACR] MORTAR %1 packed up (%2)", team.m_sName, squad.m_Order);
 }

 // carry: the first two living crewmen swap their packs for the baseplate and the barrel;
 // otherwise the parts go and their own packs come back (empty: shells are handed out per mission).
 protected void CarryParts(ACR_MortarTeam team, bool carry)
 {
  bool west = ACR_Supports.IsWestern(team.m_sFaction);
  array<ResourceName> parts = {PLATE_EAST, BARREL_EAST};
  if (west) parts = {PLATE_WEST, BARREL_WEST};
  for (int p = 0; p < parts.Count(); p++) parts[p] = ACR_FactionCatalog.ResolvePrefab(team.m_sFaction, parts[p], EEntityCatalogType.VEHICLE);
  array<AIAgent> agents = {};
  team.m_Squad.GetAgents(agents);
  int next;
  foreach (AIAgent agent : agents)
  {
   if (next >= parts.Count()) break;
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || ACR_Squad.FindController(body)) continue;
   SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(body.FindComponent(SCR_InventoryStorageManagerComponent));
   EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(body.FindComponent(EquipedLoadoutStorageComponent));
   if (!inventory || !loadout) continue;
   IEntity back = loadout.GetClothFromArea(LoadoutBackpackArea);
   ResourceName backName;
   if (back && back.GetPrefabData()) backName = back.GetPrefabData().GetPrefabName();
   bool carrying = backName.Contains("/Mortars/");
   if (carry)
   {
    next++;
    if (carrying) continue;
    team.m_Backpacks.Set(body, backName);
    if (back) inventory.TryDeleteItem(back);
    bool worn = inventory.TrySpawnPrefabToStorage(parts[next - 1], inventory.GetCharacterStorage());
    PrintFormat("[ACR] MORTAR %1: crewman carries %2 (%3)", team.m_sName, FilePath.StripPath(parts[next - 1]), worn);
   }
   else
   {
    if (!carrying) continue;
    next++;
    inventory.TryDeleteItem(back);
    ResourceName own;
    if (team.m_Backpacks.Find(body, own) && !own.IsEmpty()) inventory.TrySpawnPrefabToStorage(own, inventory.GetCharacterStorage());
   }
  }
 }

 // What a crewman wears on his back (tests).
 static string BackOf(IEntity body)
 {
  EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(body.FindComponent(EquipedLoadoutStorageComponent));
  if (!loadout) return "";
  IEntity back = loadout.GetClothFromArea(LoadoutBackpackArea);
  if (!back || !back.GetPrefabData()) return "";
  return FilePath.StripPath(back.GetPrefabData().GetPrefabName());
 }

 static bool TubeDestroyed(IEntity tube)
 {
  if (!tube) return true;
  SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(tube);
  return damage && damage.GetState() == EDamageState.DESTROYED;
 }

 // ---------------------------------------------------------------- ground

 // A level, clear spot for the baseplate at or near the point (nearest first).
 static bool FindSpot(vector near, out vector spot)
 {
  for (int ring = 0; ring * 3 <= SEARCH_RADIUS; ring++)
  {
   float radius = ring * 3;
   int steps = 1;
   if (ring > 0) steps = 6 + ring * 2;
   float start = Math.RandomFloat(0, Math.PI2);
   for (int s = 0; s < steps; s++)
   {
    float angle = start + s * Math.PI2 / steps;
    vector candidate = near + Vector(Math.Cos(angle) * radius, 0, Math.Sin(angle) * radius);
    if (CheckSpot(candidate, spot)) return true;
   }
  }
  return false;
 }

 // Level enough that the plate neither floats nor sinks visibly, dry, and nothing in the way.
 static bool CheckSpot(vector candidate, out vector spot)
 {
  candidate = ACR_Manager.Ground(candidate);
  if (BBB_Terrain.IsWaterAt(candidate)) return false;
  vector free;
  if (!SCR_WorldTools.FindEmptyTerrainPosition(free, candidate, 1.5, 1.3, 2.2)) return false;
  float low, high, mean;
  Level(free, low, high, mean);
  if (high - low > Math.Tan(MAX_SLOPE_DEG * Math.DEG2RAD) * PLATE_RADIUS * 2) return false;
  // On open ground, not on or under an object (a hangar's sloping roof sits on level terrain).
  if (OnObject(free)) return false;
  // Out of the way of traffic: off the roads and clear of where a base's bought vehicles appear.
  if (InTheWay(free) != "") return false;
  spot = free;
  // Between the lowest point and the average: no gap under the downhill edge.
  spot[1] = (low + mean) * 0.5;
  return true;
 }

 // Why a baseplate at 'at' would be in the way of vehicles ("" when it isn't): on or by a road,
 // or in the ring round a base's centre where bought vehicles appear (ACR_Requisition.SpawnPoint).
 static string InTheWay(vector at)
 {
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (aiWorld && aiWorld.GetRoadNetworkManager())
  {
   BaseRoad road;
   float distance;
   aiWorld.GetRoadNetworkManager().GetClosestRoad(at, road, distance);
   if (road && distance - road.GetWidth() * 0.5 < CLEAR_OF_ROAD + PLATE_RADIUS) return "by a road";
  }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base && !base.m_bRetired && vector.DistanceXZ(base.GetOrigin(), at) < CLEAR_OF_BASE) return "where vehicles appear at " + base.m_sName;
  }
  return "";
 }

 // Anything solid (not people) over the ground at the plate's centre or edge.
 static bool OnObject(vector at)
 {
  float ground = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
  for (int s = 0; s < 5; s++)
  {
   vector point = Vector(at[0], ground, at[2]);
   if (s > 0)
   {
    float angle = s * Math.PI_HALF;
    point = point + Vector(Math.Cos(angle) * PLATE_RADIUS, 0, Math.Sin(angle) * PLATE_RADIUS);
    point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]);
   }
   TraceParam trace = new TraceParam();
   trace.Start = point + Vector(0, 8, 0);
   trace.End = point - Vector(0, 0.3, 0);
   trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
   float fraction = GetGame().GetWorld().TraceMove(trace, NotPeople);
   if (fraction >= 1) continue;
   float hitY = trace.Start[1] + (trace.End[1] - trace.Start[1]) * fraction;
   if (hitY > point[1] + 0.3) return true;
  }
  return false;
 }

 static bool NotPeople(notnull IEntity entity)
 {
  return !ChimeraCharacter.Cast(entity);
 }

 static void Level(vector at, out float low, out float high, out float mean)
 {
  BaseWorld world = GetGame().GetWorld();
  low = float.MAX;
  high = -float.MAX;
  float sum;
  int count;
  for (int ring = 0; ring < 3; ring++)
  {
   float radius = ring * PLATE_RADIUS * 0.5;
   int steps = 1;
   if (ring > 0) steps = 8;
   for (int s = 0; s < steps; s++)
   {
    float angle = s * Math.PI2 / steps;
    float y = world.GetSurfaceY(at[0] + Math.Cos(angle) * radius, at[2] + Math.Sin(angle) * radius);
    low = Math.Min(low, y);
    high = Math.Max(high, y);
    sum += y;
    count++;
   }
  }
  mean = sum / count;
 }

 // Slope (degrees) over the baseplate at a point: for tests.
 static float SlopeAt(vector at)
 {
  float low, high, mean;
  Level(at, low, high, mean);
  return Math.Atan2(high - low, PLATE_RADIUS * 2) * Math.RAD2DEG;
 }

 protected IEntity SpawnTube(ACR_MortarTeam team)
 {
  Resource resource = Resource.Load(team.m_sTube);
  if (!resource.IsValid()) return null;
  // Facing the nearest enemy base.
  float yaw;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == team.m_sFaction) continue;
   float distance = vector.DistanceXZ(base.GetOrigin(), team.m_Spot);
   if (distance >= best) continue;
   best = distance;
   vector dir = base.GetOrigin() - team.m_Spot;
   yaw = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
  }
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(yaw, 0, 0), parameters.Transform);
  parameters.Transform[3] = team.m_Spot;
  IEntity tube = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  if (!tube) return null;
  SCR_AIVehicleUsageComponent usage = SCR_AIVehicleUsageComponent.Cast(tube.FindComponent(SCR_AIVehicleUsageComponent));
  SCR_AIGroupUtilityComponent utility = team.m_Group.GetGroupUtilityComponent();
  if (usage && utility) utility.AddUsableVehicle(usage);
  PrintFormat("[ACR] MORTAR %1 tube up at %2, slope %3 deg, usable %4", team.m_sName, team.m_Spot, SlopeAt(team.m_Spot), usage != null);
  team.m_iDryMissions = 0;
  return tube;
 }

 // ---------------------------------------------------------------- ballistics

 // Can a shell from this tube land on the target? flight = seconds in the air.
 static bool CanReach(vector from, ResourceName shell, vector target, out float flight)
 {
  flight = 0;
  float distance = vector.DistanceXZ(from, target);
  if (distance < MIN_RANGE) return false;
  array<float> coefficients = {};
  int defaultConfig;
  array<float> found = SCR_MortarShellGadgetComponent.GetPrefabInitSpeedCoef(shell, defaultConfig);
  if (found) coefficients.Copy(found);
  if (coefficients.IsEmpty()) coefficients.Insert(1.0);
  Resource resource = Resource.Load(shell);
  if (!resource.IsValid()) return false;
  IEntitySource source = resource.GetResource().ToEntitySource();
  if (!source) return false;
  float rise = target[1] - from[1];
  float tanMin = Math.Tan(MIN_ELEVATION * Math.DEG2RAD);
  float tanMax = Math.Tan(MAX_ELEVATION * Math.DEG2RAD);
  foreach (float coefficient : coefficients)
  {
   float offset, time;
   if (!BallisticTable.GetAimHeightOfProjectileAltitudeFromSource(distance, offset, time, source, rise, coefficient)) continue;
   float tangent = (offset + rise) / distance;
   if (tangent < tanMin || tangent > tanMax) continue;
   flight = time;
   return true;
  }
  return false;
 }

 static ResourceName HeShell(ResourceName tube, string factionKey = "")
 {
  ResourceName shell = "{98EC9C526AFBA282}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_HE_O832DU.et";
  if ((!factionKey.IsEmpty() && ACR_Supports.IsWestern(factionKey)) || (factionKey.IsEmpty() && tube == TUBE_WEST))
   shell = "{38BAE094333E31BF}Prefabs/Weapons/Ammo/Ammo_Shell_81mm_HE_M821.et";
  if (!factionKey.IsEmpty()) shell = ACR_FactionCatalog.ResolvePrefab(factionKey, shell, EEntityCatalogType.VEHICLE);
  return shell;
 }

 // Furthest HE reach on level ground (cached per tube).
 static float MaxRange(ResourceName tube, string factionKey = "")
 {
  ResourceName shell = HeShell(tube, factionKey);
  float cached;
  if (s_MaxRange.Find(shell, cached)) return cached;
  float best;
  float flight;
  for (float d = 200; d <= 8000; d += 50)
  {
   if (CanReach(vector.Zero, shell, Vector(0, 0, d), flight)) best = d;
   else if (best > 0) break;
  }
  s_MaxRange.Set(shell, best);
  PrintFormat("[ACR] MORTAR %1 reaches %2 m", FilePath.StripPath(tube), best);
  return best;
 }

 static string Km(float metres)
 {
  return (Math.Round(metres / 100) / 10).ToString();
 }

 // ---------------------------------------------------------------- fire missions

 // A mortar support call: the nearest deployed team that can reach the target fires it.
 // false (and why in message) when none can.
 bool Fire(string factionKey, ACR_ESupportType type, vector target, out string message, out int eta)
 {
  ACR_MortarTeam best;
  float bestDistance = float.MAX;
  float bestFlight;
  string why = "no mortar team (buy one in Requisition)";
  foreach (ACR_MortarTeam team : m_Teams)
  {
   if (team.m_sFaction != factionKey) continue;
   string reason;
   float flight;
   if (!Ready(team, type, target, reason, flight))
   {
    why = reason;
    continue;
   }
   float distance = vector.DistanceXZ(team.m_Spot, target);
   if (distance >= bestDistance) continue;
   bestDistance = distance;
   best = team;
   bestFlight = flight;
  }
  if (!best)
  {
   message = "Mortars: " + why;
   return false;
  }
  StartMission(best, type, target);
  eta = 15 + Math.Ceil(bestFlight);
  message = best.m_sName;
  return true;
 }

 // Could this team fire on the target now? (also used by the AI commander)
 bool Ready(ACR_MortarTeam team, ACR_ESupportType type, vector target, out string reason, out float flight)
 {
  if (team.m_bLost) { reason = team.m_sName + " has lost its mortar"; return false; }
  if (!team.m_Squad || team.m_Squad.HasPlayer()) { reason = team.m_sName + " is led by a player"; return false; }
  if (team.m_eState != ACR_EMortarState.DEPLOYED || !team.m_Tube) { reason = string.Format("%1 is not set up (%2)", team.m_sName, team.m_sStatus); return false; }
  if (team.m_bFiring) { reason = team.m_sName + " is busy with a fire mission"; return false; }
  // Two missions in a row that fired nothing: the crew can't work the tube where it stands
  // (Everon night soak 26 Oct: free fire kept queueing 0-of-2 missions every 2.5 min). It takes
  // missions again once the tube has been set up anew (a move, shoot-and-scoot, Reinforce).
  if (team.m_iDryMissions >= DRY_MISSIONS) { reason = team.m_sName + " isn't firing (its last two missions fired nothing); move it to set up again"; return false; }
  ResourceName shell = ShellFor(team, type);
  if (shell.IsEmpty()) { reason = team.m_sName + " has no shells of that kind"; return false; }
  float distance = vector.DistanceXZ(team.m_Tube.GetOrigin(), target);
  if (!CanReach(team.m_Tube.GetOrigin(), shell, target, flight))
  {
   if (distance < MIN_RANGE) reason = string.Format("target too close to %1 (under %2 m)", team.m_sName, MIN_RANGE);
   else reason = string.Format("target is %1 km from %2, out of range (%3 km)", Km(distance), team.m_sName, Km(MaxRange(team.m_sTube, team.m_sFaction)));
   return false;
  }
  return true;
 }

 bool CanFire(string factionKey, ACR_ESupportType type, vector target)
 {
  foreach (ACR_MortarTeam team : m_Teams)
  {
   if (team.m_sFaction != factionKey) continue;
   string reason;
   float flight;
   if (Ready(team, type, target, reason, flight)) return true;
  }
  return false;
 }

 static SCR_EAIArtilleryAmmoType AmmoFor(ACR_ESupportType type)
 {
  if (type == ACR_ESupportType.MORTAR_SMOKE) return SCR_EAIArtilleryAmmoType.SMOKE;
  if (type == ACR_ESupportType.ILLUMINATION) return SCR_EAIArtilleryAmmoType.ILLUMINATION;
  return SCR_EAIArtilleryAmmoType.HIGH_EXPLOSIVE;
 }

 protected ResourceName ShellFor(ACR_MortarTeam team, ACR_ESupportType type)
 {
  if (!team.m_Tube) return "";
  SCR_AIStaticArtilleryVehicleUsageComponent usage = SCR_AIStaticArtilleryVehicleUsageComponent.Cast(team.m_Tube.FindComponent(SCR_AIStaticArtilleryVehicleUsageComponent));
  if (!usage) return "";
  return usage.GetAmmoResourceName(AmmoFor(type));
 }

 static int RoundsFor(ACR_ESupportType type)
 {
  switch (type)
  {
   case ACR_ESupportType.MORTAR_HE_SMALL: return 4;
   case ACR_ESupportType.MORTAR_HE_LARGE: return 8;
   case ACR_ESupportType.MORTAR_CREEPING: return 3; // per step, three steps
   case ACR_ESupportType.MORTAR_SMOKE: return 4;
   case ACR_ESupportType.ILLUMINATION: return 2;
  }
  return 4;
 }

 // Fire at will (ACR_FireMissions): a few HE rounds of its own, no RP.
 void StartFreeFire(ACR_MortarTeam team, vector target, int rounds)
 {
  StartMission(team, ACR_ESupportType.MORTAR_HE_SMALL, target, rounds);
  team.m_sMission = "FIRE_AT_WILL";
 }

 protected void StartMission(ACR_MortarTeam team, ACR_ESupportType type, vector target, int roundsOverride = 0)
 {
  ACR_Squad squad = team.m_Squad;
  SCR_EAIArtilleryAmmoType ammo = AmmoFor(type);
  int rounds = RoundsFor(type);
  if (roundsOverride > 0) rounds = roundsOverride;
  array<vector> aims = {target};
  // A creeping barrage walks away from the tube in three steps of 40 m.
  if (type == ACR_ESupportType.MORTAR_CREEPING)
  {
   vector dir = target - team.m_Tube.GetOrigin();
   dir[1] = 0;
   dir = dir.Normalized();
   aims = {ACR_Manager.Ground(target - dir * 40), target, ACR_Manager.Ground(target + dir * 40)};
  }
  team.m_sShell = ShellFor(team, type);
  team.m_iFired = 0;
  int total = rounds * aims.Count();
  team.m_iReserveRounds = total;
  team.m_iRounds = total;
  foreach (int i, vector aim : aims)
  {
   // Small batches fit even ordinary riflemen's inventories. Supply the actual
   // gunner when each behavior starts, never while he is loading/firing a shell.
   for (int remaining = rounds; remaining > 0; remaining -= 3)
   {
    SCR_AIWaypointArtillerySupport waypoint = MakeWaypoint(aim, ammo, Math.Min(3, remaining));
    if (!waypoint) continue;
    if (i == 0 && remaining == rounds) squad.AdoptWaypoint(waypoint, WP_FIRE, aim);
    else team.m_Group.AddWaypoint(waypoint);
   }
  }
  squad.SetOrder("Firing", ACR_EOrderState.HOLD);
  squad.m_OrderKind = ACR_EOrderKind.HUNT;
  team.m_bFiring = true;
  team.m_iPaid = 0;
  m_LastStarted = team;
  team.m_iRounds = total;
  team.m_fMissionEnd = GetGame().GetWorld().GetWorldTime() / 1000 + MISSION_TIMEOUT;
  team.m_sMission = typename.EnumToString(ACR_ESupportType, type);
  team.m_sStatus = "firing";
  PrintFormat("[ACR] MORTAR %1 fire mission %2: shell %3, %4 rounds at %5 (%6 m), ammo budget %7", team.m_sName, team.m_sMission, team.m_sShell, total, target, vector.DistanceXZ(team.m_Tube.GetOrigin(), target), team.m_iReserveRounds);
 }

 protected SCR_AIWaypointArtillerySupport MakeWaypoint(vector aim, SCR_EAIArtilleryAmmoType ammo, int rounds)
 {
  Resource resource = Resource.Load(WP_FIRE);
  if (!resource.IsValid()) return null;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = aim;
  SCR_AIWaypointArtillerySupport waypoint = SCR_AIWaypointArtillerySupport.Cast(GetGame().SpawnEntityPrefabLocal(resource, null, parameters));
  if (!waypoint) return null;
  waypoint.SetTargetShotCount(rounds, false);
  waypoint.SetAmmoType(ammo, false);
  waypoint.SetActive(true, false);
  return waypoint;
 }

 static int CountShells(IEntity soldier, ResourceName shell)
 {
  if (!soldier) return 0;
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(soldier.FindComponent(SCR_InventoryStorageManagerComponent));
  if (!inventory) return 0;
  array<IEntity> items = {};
  inventory.GetItems(items);
  int count;
  foreach (IEntity item : items)
   if (item && item.GetPrefabData() && item.GetPrefabData().GetPrefabName() == shell) count++;
  return count;
 }

 // Vanilla artillery loads only from the selected gunner's inventory. Supply a
 // batch before his loading behavior starts; changing inventory during a shot
 // can invalidate the shell reference held by the behavior tree.
 void PrepareGunner(IEntity tube, IEntity gunner, bool fired = false)
 {
  foreach (ACR_MortarTeam team : m_Teams)
  {
   if (!team.m_bFiring || team.m_Tube != tube) continue;
   if (fired) team.m_iFired++;
   if (!gunner || ACR_Squad.FindController(gunner)) return;
   team.m_Gunner = gunner;
   if (fired) return; // Let the shot's inventory removal finish before moving another shell.
   SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(gunner.FindComponent(SCR_InventoryStorageManagerComponent));
   if (!inventory) return;
   int held = CountShells(gunner, team.m_sShell);
   int wanted = Math.Min(3, team.m_iRounds - team.m_iFired);
   for (int added = held; added < wanted && team.m_iReserveRounds > 0; added++)
   {
    if (!inventory.TrySpawnPrefabToStorage(team.m_sShell)) break;
    team.m_iReserveRounds--;
   }
   if (System.IsCLIParam("acrMortarTest")) PrintFormat("[ACR] MORTAR %1 batch: fired %2/%3, gunner holds %4, reserve %5", team.m_sName, team.m_iFired, team.m_iRounds, CountShells(gunner, team.m_sShell), team.m_iReserveRounds);
   return;
  }
 }

 // Native StaticArtilleryBehavior explicitly gets out after aiming to load
 // shells on foot. Recognize only that executing behavior on this team's tube.
 bool IsLoadingDeparture(ACR_Squad squad, AIAgent agent)
 {
  SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
  if (!utility) return false;
  SCR_AIStaticArtilleryBehavior action = SCR_AIStaticArtilleryBehavior.Cast(utility.GetExecutedAction());
  if (!action) return false;
  foreach (ACR_MortarTeam team : m_Teams)
  {
   if (team.m_Squad != squad || !team.m_bFiring || team.m_Gunner != agent.GetControlledEntity() || !team.m_Tube) continue;
   if (!action.ACR_UsesTube(team.m_Tube)) continue;
   team.m_iLoadingDismounts++;
   return true;
  }
  return false;
 }

 protected void UpdateMission(ACR_MortarTeam team, float now)
 {
  array<AIWaypoint> waypoints = {};
  team.m_Group.GetWaypoints(waypoints);
  bool firing;
  foreach (AIWaypoint waypoint : waypoints) if (SCR_AIWaypointArtillerySupport.Cast(waypoint)) firing = true;
  if (firing && now < team.m_fMissionEnd) return;
  string why = "done";
  if (firing) why = "timed out";
  EndMission(team, why);
 }

 protected void EndMission(ACR_MortarTeam team, string why)
 {
  if (!team.m_bFiring) return;
  team.m_bFiring = false;
  team.m_iReserveRounds = 0;
  team.m_fLastFinished = GetGame().GetWorld().GetWorldTime() / 1000;
  s_iLastFired = team.m_iFired;
  if (team.m_iFired > 0) team.m_iDryMissions = 0;
  else if (why == "timed out") team.m_iDryMissions++;
  // Not a round left the tube: the side gets the mission's RP back.
  if (team.m_iFired == 0 && team.m_iPaid > 0)
  {
   ACR_Manager owner = ACR_Manager.GetInstance();
   if (owner && owner.m_Bases) owner.m_Bases.Refund(team.m_sFaction, team.m_iPaid);
   PrintFormat("[ACR] MORTAR %1 fired nothing: %2 RP refunded to %3", team.m_sName, team.m_iPaid, team.m_sFaction);
  }
  team.m_iPaid = 0;
  PrintFormat("[ACR] MORTAR %1 fire mission %2 %3: %4 of %5 rounds fired", team.m_sName, team.m_sMission, why, s_iLastFired, team.m_iRounds);
  ACR_Squad squad = team.m_Squad;
  if (squad && team.m_eState == ACR_EMortarState.DEPLOYED && why != "packing up")
  {
   // Idle at the deployed tube without a Defend waypoint that orders its
   // gunner out when the next fire mission replaces it.
   squad.ClearOrders();
   squad.SetOrder("Mortar ready", ACR_EOrderState.HOLD);
   squad.m_OrderKind = ACR_EOrderKind.DEFEND;
  }
  team.m_sStatus = "ready";
  // After the idle order above: an AI commander's team sets up again a little way off, as after
  // shoot-and-scoot (a team in range never moves on its own); a player's team waits for an order.
  bool relocate = team.m_iDryMissions == DRY_MISSIONS && team.m_Squad && !team.m_Squad.HasPlayer() && team.m_Squad.m_iOrderedBy == 0;
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_AICommander ai;
  if (manager) ai = manager.AICommanderFor(team.m_sFaction);
  if (relocate && ai && ai.m_bActive)
  {
   team.m_Squad.MoveTo(ACR_Manager.Ground(team.m_Spot + Vector(Math.RandomFloat(-40, 40), 0, Math.RandomFloat(-40, 40))));
   PrintFormat("[ACR] MORTAR %1 fired nothing in %2 missions: setting up again nearby", team.m_sName, DRY_MISSIONS);
  }
 }

 // ---------------------------------------------------------------- map and panel

 // One word for the support list: how many teams can fire.
 string StatusFor(string factionKey)
 {
  int teams, ready;
  string first;
  foreach (ACR_MortarTeam team : m_Teams)
  {
   if (team.m_sFaction != factionKey) continue;
   teams++;
   if (team.m_eState == ACR_EMortarState.DEPLOYED && !team.m_bFiring && team.m_Squad && !team.m_Squad.HasPlayer()) ready++;
   if (first.IsEmpty()) first = team.m_sStatus;
  }
  if (teams == 0) return "no mortar team";
  if (ready > 1) return string.Format("%1 teams ready", ready);
  if (ready == 1) return "1 team ready";
  if (teams == 1) return first;
  return "no team ready";
 }

 // Range rings of set-up teams (drawn with the support zones, label starts with "~").
 void Ranges(string factionKey, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  foreach (ACR_MortarTeam team : m_Teams)
  {
   if (team.m_sFaction != factionKey || team.m_eState != ACR_EMortarState.DEPLOYED || !team.m_Tube) continue;
   positions.Insert(team.m_Tube.GetOrigin());
   radii.Insert(MaxRange(team.m_sTube, team.m_sFaction));
   labels.Insert(string.Format("~%1 range %2 km", team.m_sName, Km(MaxRange(team.m_sTube, team.m_sFaction))));
  }
 }

 // Reinforce brought a new mortar: the team sets up again once it holds still.
 void Rearmed(ACR_MortarTeam team)
 {
  team.m_bLost = false;
  team.m_bPartsChecked = false;
  team.m_eState = ACR_EMortarState.PACKED;
  team.m_fStillSince = 0;
  team.m_sStatus = "packed up";
  Tell(team, team.m_sName + " has a new mortar");
 }
}

// Observe the actual artillery shot event instead of treating missing inventory
// items as shots. Keep the hook scoped to AC:A's registered, firing mortar tubes.
modded class SCR_AIStaticArtilleryBehavior
{
 bool ACR_UsesTube(IEntity tube)
 {
  return m_ArtilleryEntity.m_Value == tube;
 }

 override void OnActionSelected()
 {
  super.OnActionSelected();
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && m_Utility)
   manager.Mortars().PrepareGunner(m_ArtilleryEntity.m_Value, m_Utility.m_OwnerEntity);
 }

 override void Internal_OnArtilleryFired()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager && m_Utility)
   manager.Mortars().PrepareGunner(m_ArtilleryEntity.m_Value, m_Utility.m_OwnerEntity, true);
  super.Internal_OnArtilleryFired();
 }
}
