// Player-built structures (4 Oct; Jacob: built HQ structures should count in the mode).
// Compositions built with a construction truck (the game's free-roam building) register here
// once finished; what they do depends on the composition:
//  - Field Hospital: soldiers of its side nearby are healed (after a short stay).
//  - Vehicle Maintenance: vehicles nearby are slowly repaired; heavy bays repair 40% more.
//  - Ammo Storage: soldiers nearby get magazines/launchers, vehicles' guns are refilled.
//  - Fuel Storage: vehicles nearby are slowly refuelled.
//    (These use local FOB supplies by default; the free-services option restores unlimited
//    field points. AI squads in need also walk to points with available stock.)
//  - Helipad: a pad for support helicopters, and where bought helicopters appear.
//  - Headquarters / Player Hub: a forward base ("FOB n") where purchases and reinforcements
//    deploy, unless it stands inside an existing base. Hidden from the enemy until found.
// Built anywhere; owned by the builder's side. Removed when the structure is gone.
// Supply depots inside a friendly FOB expand its storage once, without adding supplies.
// Living quarters inside a friendly FOB add 11/19 garrison defenders (small/large).
enum ACR_EStructure
{
 NONE,
 HOSPITAL,
 REPAIR,
 AMMO,
 FUEL,
 HELIPAD,
 HQ,
 DEPOT,
 RELAY,
 BARRACKS,
 DEFENCES
}

modded class SCR_CampaignBuildingCompositionComponent
{
 [RplProp()]
 protected RplId m_ACRFobProvider = RplId.Invalid();

 void ACR_SetFobProvider(IEntity provider)
 {
  RplComponent component = RplComponent.Cast(provider.FindComponent(RplComponent));
  if (component) m_ACRFobProvider = component.Id();
  Replication.BumpMe();
 }

 IEntity ACR_FobProvider()
 {
  RplComponent component = RplComponent.Cast(Replication.FindItem(m_ACRFobProvider));
  if (component) return component.GetEntity();
  return null;
 }

	protected bool m_bACRBuildCharged;
	protected bool m_bACRBuildRejected;

	// Charge paid FOB stock when the blueprint is placed, before another player can
	// spend it. Vanilla's plain game mode never charges the campaign budget itself.
	void ACR_ChargeFobBuild()
	{
		IEntity newOwner = GetProviderEntity();
		if (!Replication.IsServer() || m_bACRBuildCharged || m_bACRBuildRejected || !newOwner) return;
		if (!ACR_FobProviderComponent.Cast(newOwner.FindComponent(ACR_FobProviderComponent))) return;
		if (ACR_FobSupply.SpendBuild(newOwner, GetCompositionCost())) m_bACRBuildCharged = true;
		else
		{
			m_bACRBuildRejected = true;
			GetGame().GetCallqueue().CallLater(ACR_DeleteRejectedBuild, 0, false);
		}
	}

	void ACR_DeleteRejectedBuild()
	{
		if (GetOwner()) SCR_EntityHelper.DeleteEntityAndChildren(GetOwner());
	}

	bool ACR_BuildRejected() { return m_bACRBuildRejected; }

	void ACR_SetCompositionSpawned(bool spawned)
	{
		m_bCompositionIsSpawned = spawned;
	}

	void ACR_MarkBuildCharged()
	{
		m_bACRBuildCharged = true;
	}

	protected string m_sACRFaction;
	void ACR_SetFaction(string factionKey) { m_sACRFaction = factionKey; }
	string ACR_Faction() { return m_sACRFaction; }

	// FOB upgrade tier (server). FIA has no large living area or medium maintenance bay:
	// its tier-1 upgrades keep the small composition but give the tier-1 effect.
	protected int m_iACRTier;
	void ACR_SetTier(int tier) { m_iACRTier = tier; }
	int ACR_Tier() { return m_iACRTier; }

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (GetGame().InPlayMode() && Replication.IsServer()) GetOnCompositionSpawned().Insert(ACR_OnCompositionSpawned);
		if (GetGame().InPlayMode() && ACR_Structures.KindOf(owner) != ACR_EStructure.NONE) ACR_Structures.s_Registered.Insert(this);
	}

	void ACR_OnCompositionSpawned(bool spawned)
	{
		if (spawned && !m_bACRBuildRejected && Replication.IsServer()) ACR_Navmesh.RebuildAfterSpawn(GetOwner());
		if (!spawned || m_bACRBuildCharged || m_bACRBuildRejected || !Replication.IsServer()) return;
		if (GetProviderEntity() && ACR_FobProviderComponent.Cast(GetProviderEntity().FindComponent(ACR_FobProviderComponent)))
		{
			ACR_ChargeFobBuild();
			return;
		}
		if (ACR_Structures.ChargeBuild(this, GetProviderEntity())) m_bACRBuildCharged = true;
	}

	int ACR_CompositionCost()
	{
		return GetCompositionCost();
	}

	void ACR_SetProviderForTest(IEntity provider)
	{
		SetProviderEntity(provider);
	}
}

// Entering building mode also assigns providers to existing nearby compositions.
// Charge new placements here so opening the menu cannot bill or delete existing buildings.
modded class SCR_CampaignBuildingPlacingEditorComponent
{
 override protected void SetProviderAndBuilder(notnull SCR_CampaignBuildingCompositionComponent compositionComponent)
 {
  super.SetProviderAndBuilder(compositionComponent);
  compositionComponent.ACR_ChargeFobBuild();
 }
}

class ACR_Structure : Managed
{
 SCR_CampaignBuildingCompositionComponent m_Composition;
 ACR_EStructure m_eKind;
 string m_sFaction;
 vector m_vPos;
 ACR_Helipad m_Pad;
 ACR_Base m_Fob;
 IEntity m_Entity;
 ref array<IEntity> m_aMajorParts = {}; // destructible sub-compositions (tents, shelters)
}

class ACR_Structures : Managed
{
 static ref array<SCR_CampaignBuildingCompositionComponent> s_Registered = {};
 static const float FOB_RADIUS = 40;
 static const int TICK_MS = 2000;
 protected static int s_iFobs;

 // Per match (ACR_Manager.Stop): the next match's first FOB is FOB 1 again. Registered
 // compositions stay (the next world's register themselves as they load); only gone ones go.
 static void Reset()
 {
  s_iFobs = 0;
  for (int i = s_Registered.Count() - 1; i >= 0; i--) if (!s_Registered[i]) s_Registered.Remove(i);
 }

 protected ACR_Manager m_Manager;
 protected ref array<ref ACR_Structure> m_Active = {};
 protected ref array<SCR_CampaignBuildingCompositionComponent> m_Ignored = {}; // sideless: logged once
 protected float m_fNextTick;

 void ACR_Structures(ACR_Manager manager)
 {
  m_Manager = manager;
 }

 static ACR_EStructure KindOfName(string prefab)
 {
  string name = prefab;
  name.ToLower();
  if (name.Contains("fieldhospital")) return ACR_EStructure.HOSPITAL;
  if (name.Contains("vehiclemaintenance")) return ACR_EStructure.REPAIR;
  if (name.Contains("ammostorage")) return ACR_EStructure.AMMO;
  if (name.Contains("fuelstorage")) return ACR_EStructure.FUEL;
  if (name.Contains("helipad_l")) return ACR_EStructure.HELIPAD;
  if (name.Contains("headquarters") || name.Contains("playerhub")) return ACR_EStructure.HQ;
  if (name.Contains("supplycache_") || name.Contains("supplystorage_")) return ACR_EStructure.DEPOT;
  if (name.Contains("antenna_s_")) return ACR_EStructure.RELAY;
  if (name.Contains("livingarea_") || name.Contains("barracks")) return ACR_EStructure.BARRACKS;
  if (name.Contains("sandbagposition_s_") || name.Contains("machinegunnest_") || name.Contains("bunker_s_") || name.Contains("guardtower_s_")) return ACR_EStructure.DEFENCES;
  return ACR_EStructure.NONE;
 }

 static ACR_EStructure KindOf(IEntity entity)
 {
  if (!entity || !entity.GetPrefabData()) return ACR_EStructure.NONE;
  return KindOfName(entity.GetPrefabData().GetPrefabName());
 }

 static string KindName(ACR_EStructure kind)
 {
  switch (kind)
  {
   case ACR_EStructure.HOSPITAL: return "field hospital";
   case ACR_EStructure.REPAIR: return "vehicle maintenance";
   case ACR_EStructure.AMMO: return "ammo storage";
   case ACR_EStructure.FUEL: return "fuel storage";
   case ACR_EStructure.HELIPAD: return "helipad";
   case ACR_EStructure.HQ: return "headquarters";
   case ACR_EStructure.DEPOT: return "supply depot";
   case ACR_EStructure.RELAY: return "radio relay";
   case ACR_EStructure.BARRACKS: return "barracks";
   case ACR_EStructure.DEFENCES: return "fortification";
  }
  return "structure";
 }

 // ACR's worlds use the plain game mode, which does not deduct construction supplies.
 // Charge the vanilla editable budget once the composition reports that it was built.
 static bool ChargeBuild(SCR_CampaignBuildingCompositionComponent composition, IEntity provider)
 {
  if (!composition || !provider) return false;
  IEntity truck = provider;
  for (int i = 0; truck && i < 8; i++)
  {
   if ((ACR_Logistics.ServiceOf(truck) & ACR_EService.CONSTRUCTION) != 0) break;
   truck = truck.GetParent();
  }
  if (!truck || (ACR_Logistics.ServiceOf(truck) & ACR_EService.CONSTRUCTION) == 0) return false;

  IEntity owner = composition.GetOwner();
  if (!owner || !owner.GetPrefabData()) return false;
  int supplyCost = composition.ACR_CompositionCost();
  if (supplyCost <= 0) return false;

  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  if (!manager.m_Logistics) manager.m_Logistics = new ACR_Logistics(manager);
  float loads = supplyCost / ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION;
  if (!manager.m_Logistics.Use(truck, loads))
  {
   PrintFormat("[ACR] BUILD %1 needs %2 supplies; construction truck has %3", FilePath.StripPath(owner.GetPrefabData().GetPrefabName()), supplyCost, manager.m_Logistics.StockOf(truck) * ACR_Logistics.SUPPLIES_PER_LOAD_CONSTRUCTION);
   return false;
  }
  PrintFormat("[ACR] BUILD %1 charged %2 supplies (%3 loads)", FilePath.StripPath(owner.GetPrefabData().GetPrefabName()), supplyCost, loads);
  return true;
 }

 // Logistics service flags of a kind (0 = not a service point).
 static int ServiceOf(ACR_EStructure kind)
 {
  switch (kind)
  {
   case ACR_EStructure.HOSPITAL: return ACR_EService.MEDICAL;
   case ACR_EStructure.REPAIR: return ACR_EService.REPAIR;
   case ACR_EStructure.AMMO: return ACR_EService.AMMO;
   case ACR_EStructure.FUEL: return ACR_EService.FUEL;
  }
  return 0;
 }

 // The builder's side, else the side in the composition's name (..._US_..., ..._USSR_...).
 static string FactionOf(SCR_CampaignBuildingCompositionComponent composition)
 {
  if (!composition.ACR_Faction().IsEmpty()) return composition.ACR_Faction();
  int builder = composition.GetBuilderId();
  if (builder > 0)
  {
   Faction faction = SCR_FactionManager.SGetPlayerFaction(builder);
   if (faction) return faction.GetFactionKey();
  }
  IEntity owner = composition.GetOwner();
  if (!owner || !owner.GetPrefabData()) return "";
  string name = owner.GetPrefabData().GetPrefabName();
  name = FilePath.StripExtension(FilePath.StripPath(name));
  array<string> parts = {};
  name.Split("_", parts, true);
  FactionManager factions = GetGame().GetFactionManager();
  foreach (string part : parts)
  {
   if (part == "US") return ACR_FactionSelection.West();
   if (part == "USSR") return ACR_FactionSelection.East();
   if (factions && factions.GetFactionByKey(part)) return part;
  }
  return "";
 }

 bool IsActive(SCR_CampaignBuildingCompositionComponent composition)
 {
  foreach (ACR_Structure s : m_Active) if (s.m_Composition == composition) return true;
  return false;
 }

 void Tick()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextTick) return;
  m_fNextTick = now + TICK_MS;
  // Every built FOB ticks, including one whose HQ is gone (its upgrades stay).
  array<ACR_Base> fobs = {};
  foreach (ACR_Base candidate : ACR_Base.s_All)
   if (candidate && candidate.m_bBuilt && !candidate.m_bRetired) fobs.Insert(candidate);
  foreach (ACR_Base fobTick : fobs)
  {
   ACR_FobSupply.Tick(fobTick);
   ACR_FobSlots.Tick(fobTick);
  }
  // Wrecked (every destructible tent/shelter destroyed): remove it so it can be rebuilt.
  foreach (ACR_Structure wreck : m_Active)
  {
   if (!wreck.m_Entity || !IsWrecked(wreck)) continue;
   PrintFormat("[ACR] STRUCTURE %1 %2 at %3 destroyed", wreck.m_sFaction, KindName(wreck.m_eKind), wreck.m_vPos);
   IEntity wreckRoot = wreck.m_Entity;
   wreck.m_Entity = null;
   ACR_Navmesh.DeleteStructure(wreckRoot);
  }
  // Gone (dismantled, destroyed): undo.
  for (int i = m_Active.Count() - 1; i >= 0; i--)
  {
   ACR_Structure s = m_Active[i];
   if (s.m_Composition && s.m_Entity) continue;
   Deactivate(s);
   m_Active.RemoveOrdered(i);
  }
  for (int r = s_Registered.Count() - 1; r >= 0; r--)
  {
   SCR_CampaignBuildingCompositionComponent composition = s_Registered[r];
   if (!composition || !composition.GetOwner()) { s_Registered.RemoveOrdered(r); continue; }
   if (composition.ACR_BuildRejected() || !composition.IsCompositionSpawned() || IsActive(composition) || m_Ignored.Contains(composition)) continue;
   Activate(composition);
  }
  for (int g = m_Ignored.Count() - 1; g >= 0; g--)
   if (!m_Ignored[g] || !m_Ignored[g].GetOwner()) m_Ignored.Remove(g);
  foreach (ACR_Base fob : fobs)
  {
   if (fob.m_bRetired) continue;
   UpdateCapacity(fob);
   UpdateHousing(fob);
   // An HQ-less FOB with nothing of its owner's left standing is gone for good.
   if (fob.m_bHqLost && !HasStructuresAt(fob) && !ACR_FobSlots.AnyBuilding(fob)) fob.Retire("its HQ and buildings are gone");
  }
 }

 // Any active structure of the FOB owner inside its circle.
 bool HasStructuresAt(ACR_Base base)
 {
  if (!base) return false;
  foreach (ACR_Structure s : m_Active)
   if (s.m_Entity && s.m_sFaction == base.m_sOwner && vector.DistanceXZ(s.m_vPos, base.GetOrigin()) <= base.m_fRadius) return true;
  return false;
 }

 // Active compositions of a kind and side inside a base circle.
 void StructuresAt(ACR_Base base, ACR_EStructure kind, string faction, notnull array<IEntity> found)
 {
  found.Clear();
  if (!base) return;
  foreach (ACR_Structure s : m_Active)
  {
   if (!s.m_Entity || s.m_eKind != kind || s.m_sFaction != faction) continue;
   if (vector.DistanceXZ(s.m_vPos, base.GetOrigin()) <= base.m_fRadius) found.Insert(s.m_Entity);
  }
 }

 // Approximate footprint radius (metres) of a vanilla composition kind and tier.
 static float KindRadius(ACR_EStructure kind, int tier = 0)
 {
  switch (kind)
  {
   case ACR_EStructure.HOSPITAL: return 14;
   case ACR_EStructure.REPAIR:
    if (tier > 0) return 17;
    return 8;
   case ACR_EStructure.AMMO: return 8;
   case ACR_EStructure.FUEL: return 8;
   case ACR_EStructure.HELIPAD: return 21;
   case ACR_EStructure.HQ: return 10;
   case ACR_EStructure.DEPOT: return 9;
   case ACR_EStructure.RELAY: return 8;
   case ACR_EStructure.BARRACKS:
    if (tier > 0) return 22;
    return 10;
   case ACR_EStructure.DEFENCES: return ACR_FobDefences.PIECE_RADIUS;
  }
  return 8;
 }

 // A registered composition (built or a pending blueprint) whose footprint overlaps the circle,
 // other than those listed in ignore.
 static IEntity CompositionOverlapping(vector position, float radius, array<IEntity> ignore = null)
 {
  foreach (SCR_CampaignBuildingCompositionComponent composition : s_Registered)
  {
   if (!composition || composition.ACR_BuildRejected()) continue;
   IEntity root = composition.GetOwner();
   if (!root || (ignore && ignore.Contains(root))) continue;
   ACR_EStructure kind = KindOf(root);
   if (vector.DistanceXZ(root.GetOrigin(), position) < radius + KindRadius(kind, TierOf(root))) return root;
  }
  return null;
 }

 // Destructible sub-compositions directly under the root: the tents and shelters that make it.
 static void MajorParts(IEntity root, notnull array<IEntity> parts)
 {
  parts.Clear();
  if (!root) return;
  for (IEntity child = root.GetChildren(); child; child = child.GetSibling())
  {
   if (!child.GetPrefabData()) continue;
   string name = child.GetPrefabData().GetPrefabName();
   if (!name.Contains("/SubCompositions/")) continue;
   if (!SCR_DestructionMultiPhaseComponent.Cast(child.FindComponent(SCR_DestructionMultiPhaseComponent)) && !DamageManagerComponent.Cast(child.FindComponent(DamageManagerComponent))) continue;
   parts.Insert(child);
  }
 }

 static bool PartDestroyed(IEntity part)
 {
  if (!part) return true;
  SCR_DestructionMultiPhaseComponent phases = SCR_DestructionMultiPhaseComponent.Cast(part.FindComponent(SCR_DestructionMultiPhaseComponent));
  if (phases && phases.GetDestroyed()) return true;
  DamageManagerComponent damage = DamageManagerComponent.Cast(part.FindComponent(DamageManagerComponent));
  return damage && damage.IsDestroyed();
 }

 // Every major part destroyed (a composition without any never counts as wrecked).
 static bool IsWrecked(ACR_Structure s)
 {
  if (!s || !s.m_Entity) return false;
  if (ACR_Logistics.Health(s.m_Entity) <= 0) return true;
  if (s.m_aMajorParts.IsEmpty()) return false;
  foreach (IEntity part : s.m_aMajorParts)
   if (!PartDestroyed(part)) return false;
  return true;
 }

 // Active HQ compositions within a radius of a point.
 void HqsNear(vector position, float radius, notnull array<IEntity> found)
 {
  found.Clear();
  foreach (ACR_Structure s : m_Active)
   if (s.m_Entity && s.m_eKind == ACR_EStructure.HQ && vector.DistanceXZ(s.m_Entity.GetOrigin(), position) <= radius) found.Insert(s.m_Entity);
 }

 // Re-run the structures tick now (a demolition should take effect at once).
 void ACR_ForceTick()
 {
  m_fNextTick = 0;
  Tick();
 }

 ACR_Structure FindStructure(IEntity root)
 {
  foreach (ACR_Structure s : m_Active) if (s.m_Entity == root) return s;
  return null;
 }

 // A captured FOB's buildings change hands with it.
 void TransferFob(ACR_Base base, string faction)
 {
  if (!base || !base.m_bBuilt) return;
  foreach (ACR_Structure s : m_Active)
  {
   if (!s.m_Entity || vector.DistanceXZ(s.m_vPos, base.GetOrigin()) > base.m_fRadius) continue;
   if (s.m_sFaction == faction) continue;
   s.m_sFaction = faction;
   if (s.m_Composition) s.m_Composition.ACR_SetFaction(faction);
   int service = ServiceOf(s.m_eKind);
   if (service != 0 && m_Manager.m_Logistics) m_Manager.m_Logistics.AddPoint(s.m_Entity, service, faction);
   if (s.m_Pad) s.m_Pad.m_sFaction = faction;
  }
  ACR_FobSlots.TransferOwner(base, faction);
  PrintFormat("[ACR] FOB %1 buildings now belong to %2", base.m_sName, faction);
 }

 // Reconcile capacity changes only: surviving housing never refills casualties.
 // Recount after registration so housing completed before the CP also works.
 void UpdateHousing(ACR_Base base)
 {
  if (!base || !base.m_bBuilt || base.m_bRetired) return;
  int bonus;
  foreach (ACR_Structure structure : m_Active)
  {
   if (structure.m_eKind != ACR_EStructure.BARRACKS || structure.m_sFaction != base.m_sOwner) continue;
   if (!structure.m_Entity || !structure.m_Composition || structure.m_Composition.ACR_BuildRejected() || !structure.m_Composition.IsCompositionSpawned()) continue;
   if (vector.DistanceXZ(structure.m_Entity.GetOrigin(), base.GetOrigin()) <= base.m_fRadius) bonus += HousingDefenders(structure.m_Entity);
  }
  int change = bonus - base.m_iHousingGarrison;
  if (change == 0) return;
  m_Manager.m_Bases.ResizeGarrison(base, change);
  base.m_iHousingGarrison = bonus;
  PrintFormat("[ACR] FOB %1 housing: %2 defenders (garrison %3, %4 in reserve until enemies come within %5 m)", base.m_sName, bonus, base.m_iGarrison, base.m_iGarrisonLeft, (int)ACR_BaseSystem.SPAWN_RANGE);
 }

 static int HousingDefenders(IEntity housing)
 {
  if (!housing || !housing.GetPrefabData()) return 0;
  if (TierOf(housing) >= 1) return 19;
  // Native living quarters cost 275 / 475: one defender per 25 supplies.
  return 11;
 }

 static float RepairFraction(IEntity station)
 {
  if (!station || !station.GetPrefabData()) return 0.15;
  if (TierOf(station) >= 1) return 0.21;
  return 0.15;
 }

 // Tier of a living area / maintenance composition: large/medium prefabs, or a tier set by a FOB slot.
 static int TierOf(IEntity composition)
 {
  if (!composition || !composition.GetPrefabData()) return 0;
  SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(composition.FindComponent(SCR_CampaignBuildingCompositionComponent));
  if (comp && comp.ACR_Tier() >= 1) return 1;
  string name = composition.GetPrefabData().GetPrefabName();
  name.ToLower();
  if (name.Contains("livingarea_l_") || name.Contains("vehiclemaintenance_m_") || name.Contains("vehiclemaintenance_l_")) return 1;
  return 0;
 }

 void UpdateCapacity(ACR_Base base)
 {
  bool depot;
  foreach (ACR_Structure structure : m_Active)
  {
   if (!structure.m_Entity || !structure.m_Composition || structure.m_eKind != ACR_EStructure.DEPOT || structure.m_sFaction != base.m_sOwner) continue;
   if (vector.DistanceXZ(structure.m_vPos, base.GetOrigin()) <= base.m_fRadius) depot = true;
  }
  int capacity = ACR_FobSupply.CAPACITY;
  if (depot) capacity += ACR_LogisticsRules.DepotBonus();
  SCR_ResourceContainer store = ACR_Logistics.SupplyContainer(base.m_FobSupply);
  if (!store) return;
  if (store.GetMaxResourceValue() != capacity) store.SetMaxResourceValue(capacity);
  if (store.GetResourceValue() > capacity) store.SetResourceValue(capacity);
 }

 // A completed relay composition must still contain its working antenna.
 // Read live entities so removal/movement closes the gate before the next tick.
 static IEntity RelayAntenna(IEntity entity)
 {
  if (!entity || ACR_Logistics.Health(entity) <= 0) return null;
  if (entity.FindComponent(ScriptedRadioComponent)) return entity;
  for (IEntity child = entity.GetChildren(); child; child = child.GetSibling())
  {
   IEntity antenna = RelayAntenna(child);
   if (antenna) return antenna;
  }
  return null;
 }

 bool HasRelay(ACR_Base base)
 {
  if (!base || base.m_bRetired) return false;
  foreach (ACR_Structure structure : m_Active)
  {
   if (structure.m_eKind != ACR_EStructure.RELAY || structure.m_sFaction != base.m_sOwner) continue;
   if (!structure.m_Entity || !structure.m_Composition || structure.m_Composition.ACR_BuildRejected() || !structure.m_Composition.IsCompositionSpawned()) continue;
   if (vector.DistanceXZ(structure.m_Entity.GetOrigin(), base.GetOrigin()) > base.m_fRadius) continue;
   if (RelayAntenna(structure.m_Entity)) return true;
  }
  return false;
 }

 IEntity HqEntityOf(ACR_Base base)
 {
  if (!base || base.m_bRetired) return null;
  foreach (ACR_Structure structure : m_Active)
  {
   if (structure.m_Fob == base && structure.m_Entity) return structure.m_Entity;
  }
  return null;
 }

 protected void Activate(SCR_CampaignBuildingCompositionComponent composition)
 {
  if (!composition) return;
  IEntity owner = composition.GetOwner();
  if (!owner) return;
  ACR_Structure s = new ACR_Structure();
  s.m_Composition = composition;
  s.m_Entity = owner;
  s.m_eKind = KindOf(s.m_Entity);
  s.m_sFaction = FactionOf(composition);
  s.m_vPos = s.m_Entity.GetOrigin();
  if (s.m_sFaction.IsEmpty())
  {
   PrintFormat("[ACR] STRUCTURE %1 at %2: no side, ignored", KindName(s.m_eKind), s.m_vPos);
   m_Ignored.Insert(composition);
   return;
  }
  MajorParts(owner, s.m_aMajorParts);
  m_Active.Insert(s);
  int service = ServiceOf(s.m_eKind);
  if (service != 0)
  {
   if (!m_Manager.m_Logistics) m_Manager.m_Logistics = new ACR_Logistics(m_Manager);
   m_Manager.m_Logistics.AddPoint(s.m_Entity, service, s.m_sFaction);
  }
  if (s.m_eKind == ACR_EStructure.HELIPAD)
  {
   EntitySpawnParams parameters = new EntitySpawnParams();
   parameters.TransformMode = ETransformMode.WORLD;
   s.m_Entity.GetWorldTransform(parameters.Transform);
   s.m_Pad = ACR_Helipad.Cast(GetGame().SpawnEntity(ACR_Helipad, GetGame().GetWorld(), parameters));
   if (s.m_Pad) s.m_Pad.m_sFaction = s.m_sFaction;
  }
  if (s.m_eKind == ACR_EStructure.HQ)
  {
   s.m_Fob = RestoreFob(s);
   if (!s.m_Fob) s.m_Fob = MakeFob(s);
  }
  string where = "";
  if (s.m_Fob) where = " = " + s.m_Fob.m_sName;
  else if (s.m_Pad && s.m_Pad.Base()) where = " (pad of " + s.m_Pad.Base().m_sName + ")";
  PrintFormat("[ACR] STRUCTURE %1 %2 built at %3%4 (%5 destructible parts)", s.m_sFaction, KindName(s.m_eKind), s.m_vPos, where, s.m_aMajorParts.Count());
 }

 // A new HQ at the centre of its side's HQ-less FOB brings that FOB back online.
 protected ACR_Base RestoreFob(ACR_Structure s)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || !base.m_bBuilt || base.m_bRetired || !base.m_bHqLost || base.m_sOwner != s.m_sFaction) continue;
   if (vector.DistanceXZ(base.GetOrigin(), s.m_vPos) > FOB_RADIUS * 0.5) continue;
   base.m_bHqLost = false;
   if (!base.m_FobSupply) ACR_FobSupply.Create(base, s.m_Entity);
   else ACR_FobSupply.Bind(base, s.m_Entity);
   PrintFormat("[ACR] BASE %1 back online (HQ rebuilt)", base.m_sName);
   ACR_Intel.FobEvent(base, base.m_sName + " back online: HQ rebuilt", base.m_sName + " HQ rebuilt");
   return base;
  }
  return null;
 }

 protected ACR_Base MakeFob(ACR_Structure s)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (vector.DistanceXZ(base.GetOrigin(), s.m_vPos) <= base.m_fRadius + FOB_RADIUS) return null; // inside a base already
  // Worded before the FOB exists, so the place named is not the FOB itself.
  string place = "";
  ACR_Intel intel = m_Manager.IntelFor(s.m_sFaction);
  if (intel) place = " " + intel.Place(s.m_vPos);
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  parameters.Transform[3] = s.m_vPos;
  ACR_Base fob = ACR_Base.Cast(GetGame().SpawnEntity(ACR_Base, GetGame().GetWorld(), parameters));
  if (!fob) return null;
  s_iFobs++;
  fob.m_sName = string.Format("FOB %1", s_iFobs);
  fob.m_sOwner = s.m_sFaction;
  fob.m_fScore = 100;
  fob.m_iGarrison = 0;
  fob.m_iGarrisonLeft = 0;
  fob.m_iValue = 0;
  fob.m_bIncome = false;
  fob.m_bHidden = true;
  fob.m_bAllowSpawn = true;
  fob.m_bAllowReinforce = true;
  fob.m_fRadius = FOB_RADIUS;
  fob.m_bBuilt = true;
  ACR_FobSupply.Create(fob, s.m_Entity);
  ACR_FobSlots.InitSlots(fob);
  ACR_SideStats stats = ACR_Report.Of(fob.m_sOwner);
  if (stats) stats.m_iFobsBuilt++;
  ACR_Intel.FobEvent(fob, fob.m_sName + " established" + place, "established " + fob.m_sName);
  return fob;
 }

 protected void Deactivate(ACR_Structure s)
 {
  if (m_Manager.m_Logistics && s.m_Entity) m_Manager.m_Logistics.RemovePoint(s.m_Entity);
  if (m_Manager.m_Logistics) m_Manager.m_Logistics.PrunePoints();
  if (s.m_Pad) s.m_Pad.m_bGone = true;
  // The FOB outlives its HQ while its upgrades stand (retired in Tick once nothing is left).
  if (s.m_Fob) s.m_Fob.MarkHqLost();
  PrintFormat("[ACR] STRUCTURE %1 %2 at %3 is gone", s.m_sFaction, KindName(s.m_eKind), s.m_vPos);
 }

 int CountActive(ACR_EStructure kind, string faction)
 {
  int count;
  foreach (ACR_Structure s : m_Active) if (s.m_eKind == kind && s.m_sFaction == faction && s.m_Entity) count++;
  return count;
 }

 ACR_Structure Find(IEntity entity)
 {
  foreach (ACR_Structure s : m_Active) if (s.m_Entity == entity) return s;
  return null;
 }
}

// Built ammo/vehicle compositions carry vanilla self-resupply actions that log a script error on
// a dedicated server when they find no storage manager (vanilla then just skips the check). Skip
// it quietly instead: same behaviour, no false script error in logs and test runs.
modded class SCR_ResupplySelfSupportStationAction
{
 override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
 {
  if (m_bCheckIfItemInStorage && pOwnerEntity)
  {
   bool found = pOwnerEntity.FindComponent(InventoryStorageManagerComponent) != null;
   if (!found && m_bAllowGetStorageFromParent && pOwnerEntity.GetParent())
    found = pOwnerEntity.GetParent().FindComponent(InventoryStorageManagerComponent) != null;
   if (!found) m_bCheckIfItemInStorage = false;
  }
  super.Init(pOwnerEntity, pManagerComponent);
	}
}

// Vanilla resupply stations log an error when their game-mode catalog manager is not
// ready at their first delayed init. They can still use their configured fallback cost.
modded class SCR_BaseItemSupportStationComponent
{
	protected override bool InitValidSetup()
	{
		m_EntityCatalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
		return super.InitValidSetup();
	}

	protected override int GetSupplyAmountAction(IEntity actionOwner, IEntity actionUser, SCR_BaseUseSupportStationAction action)
	{
		if (!AreSuppliesEnabled()) return 0;
		SCR_BaseItemHolderSupportStationAction itemHolder = SCR_BaseItemHolderSupportStationAction.Cast(action);
		if (!itemHolder) return 0;

		if (!m_EntityCatalogManager) m_EntityCatalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
		if (!m_EntityCatalogManager)
			return Math.ClampInt(m_iFallbackItemSupplyCost + m_iBaseSupplyCostOnUse, 0, int.MAX);

		SCR_EntityCatalogEntry catalogEntry = m_EntityCatalogManager.GetEntryWithPrefabFromAnyCatalog(EEntityCatalogType.ITEM, itemHolder.GetItemPrefab(), GetFaction());
		if (!catalogEntry) return Math.ClampInt(m_iFallbackItemSupplyCost + m_iBaseSupplyCostOnUse, 0, int.MAX);
		SCR_ArsenalItem arsenalData = SCR_ArsenalItem.Cast(catalogEntry.GetEntityDataOfType(SCR_ArsenalItem));
		if (!arsenalData) return Math.ClampInt(m_iFallbackItemSupplyCost + m_iBaseSupplyCostOnUse, 0, int.MAX);

		return Math.ClampInt(arsenalData.GetSupplyCost(m_eSupplyCostType) + m_iBaseSupplyCostOnUse, 0, int.MAX);
	}
}

// Preserve the resupply component's own range validation while allowing its catalog
// manager to become available after entity initialization.
modded class SCR_ResupplySupportStationComponent
{
	protected override bool InitValidSetup()
	{
		if (UsesRange())
		{
			Print("'SCR_ResupplySupportStationComponent' does not support range. Make sure m_fRange is set to -1", LogLevel.ERROR);
			return false;
		}

		if (!m_EntityCatalogManager) m_EntityCatalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
		if (!m_EntityCatalogManager) return true;
		return super.InitValidSetup();
	}
}
