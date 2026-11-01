// Helipads and the support helicopters stationed on them (3 Oct; Jacob: support helicopters
// need a helipad, a lost one has to be bought again).
//  - ACR_Helipad: placed in a base in World Editor; puts a helipad model there on every machine.
//  - A side buys a support helicopter (buy list: "Support gunship", "Support helicopter (door
//    gunners)") for a free pad at the base it buys at. It waits there with its crew, flies each
//    mission from the pad (ACR_HeliRun with m_Home), comes back, lands and is serviced (rearmed,
//    repaired, crew replaced) before it can fly again. Destroyed = gone: buy another.
//  - With a content pack that names an attack helicopter for the side (ACR_ContentSocket), a third
//    kind: "Support attack helicopter (<airframe>)", flying that side's attack-helicopter support.
//  - A world with no ACR_Helipad keeps the old off-map helicopters.
class ACR_HelipadClass : GenericEntityClass {}
class ACR_Helipad : GenericEntity
{
 [Attribute("1", UIWidgets.CheckBox, "Place a helipad model here (off when the world already has one at this spot)")]
 bool m_bModel;

 [Attribute("", UIWidgets.EditBox, "Base this pad belongs to (its name on the map). Empty = the nearest base")]
 string m_sBase;

 // Built pads (ACR_Structures): owned by the builder's side, not a base; m_bGone once the
 // helipad structure is dismantled or destroyed.
 string m_sFaction;
 bool m_bGone;

 static const ResourceName MODEL_WEST = "{CF1FFB7BAA1A0769}Prefabs/Structures/Military/Camps/HelipadImprovised_01/HelipadImprovised_US_01.et";
 static const ResourceName MODEL_EAST = "{8CDB9C117669AF4A}Prefabs/Structures/Military/Camps/HelipadImprovised_01/HelipadImprovised_USSR_01.et";
 static ref array<ACR_Helipad> s_All = {};
 protected IEntity m_Model;

 void ACR_Helipad(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT);
 }

 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode()) return;
  s_All.Insert(this);
  // Bases register in their own EOnInit: the model goes in on the first frame. (A call queued
  // from EOnInit never ran: the world load drops it, so no pad was ever drawn; live 3 Oct.)
  if (m_bModel) SetEventMask(EntityEvent.FRAME);
 }

 override void EOnFrame(IEntity owner, float timeSlice)
 {
  ClearEventMask(EntityEvent.FRAME);
  if (!m_Model) PlaceModel();
 }

 bool HasModel() { return m_Model != null; }

 // Out of play (its base isn't in the skirmish layout): no side owns it, model removed here.
 void Drop()
 {
  m_bGone = true;
  if (m_Model) SCR_EntityHelper.DeleteEntityAndChildren(m_Model);
  PrintFormat("[ACR] HELIPAD %1 out of play", GetName());
 }

 void ~ACR_Helipad()
 {
  if (s_All) s_All.RemoveItem(this);
  if (m_Model) SCR_EntityHelper.DeleteEntityAndChildren(m_Model);
 }

 // Local on every machine (a static prop, like the world's own).
 protected void PlaceModel()
 {
  ResourceName prefab = MODEL_EAST;
  ACR_Base base = Base();
  if (base && ACR_Supports.IsWestern(base.m_sOwner)) prefab = MODEL_WEST;
  if (base) prefab = ACR_FactionCatalog.ResolvePrefab(base.m_sOwner, prefab, EEntityCatalogType.GROUP);
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(Yaw(), 0, 0), parameters.Transform);
  vector at = GetOrigin();
  at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
  parameters.Transform[3] = at;
  m_Model = GetGame().SpawnEntityPrefabLocal(resource, GetGame().GetWorld(), parameters);
  PrintFormat("[ACR] HELIPAD model %1 at %2 (%3): %4", FilePath.StripPath(prefab), at, base != null, m_Model != null);
 }

 float Yaw()
 {
  return GetYawPitchRoll()[0];
 }

 vector Spot()
 {
  return ACR_Manager.Ground(GetOrigin());
 }

 // The side that owns the pad: a built pad's builder, else its base's owner ("" = nobody).
 string Faction()
 {
  if (m_bGone) return "";
  if (!m_sFaction.IsEmpty()) return m_sFaction;
  ACR_Base base = Base();
  if (!base) return "";
  return base.m_sOwner;
 }

 // The base this pad belongs to (helicopters for it are bought there): the one named in
 // m_sBase, else the nearest (any distance; Jacob 4 Oct: the old radius + 60 m rule was too
 // tight to place pads). A built pad belongs to its side's nearest base.
 ACR_Base Base()
 {
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!m_sFaction.IsEmpty())
   {
    if (base.m_sOwner != m_sFaction) continue;
   }
   else if (m_sBase != "" && (base.m_sName == m_sBase || base.GetLabel() == m_sBase)) return base;
   float d = vector.DistanceXZ(base.GetOrigin(), GetOrigin());
   if (d < bestDistance) { bestDistance = d; best = base; }
  }
  if (m_sFaction.IsEmpty() && m_sBase != "" && !m_bWarned)
  {
   m_bWarned = true;
   PrintFormat("[ACR] HELIPAD %1: no base named '%2', using the nearest (%3)", GetName(), m_sBase, best != null);
  }
  return best;
 }
 protected bool m_bWarned;
}

enum ACR_EHeliState
{
 READY,
 FLYING,
 SERVICING
}

class ACR_StationedHeli : Managed
{
 string m_sFaction;
 string m_sName;
 bool m_bGunship;      // else the door-gunner transport
 bool m_bAttack;       // a content pack's attack helicopter (m_bGunship too)
 IEntity m_Vehicle;
 ACR_Helipad m_Pad;
 ACR_EHeliState m_eState;
 float m_fReadyAt;
 int m_iSorties;

 string Kind()
 {
  if (m_bAttack) return "attack helicopter";
  if (m_bGunship) return "gunship";
  return "door-gunner helicopter";
 }
}

class ACR_HeliStation : Managed
{
 static int SERVICE_SECONDS = 60; // (tests shorten it)
 static const int GUNSHIP_COST = 80;
 static const int TRANSPORT_COST = 50;
 static const int STOCK = 4;
 static const string GUNSHIP_ENTRY = "Support gunship";
 static const string TRANSPORT_ENTRY = "Support helicopter (door gunners)";
 static const int ATTACK_COST = 100;
 static const int ATTACK_STOCK = 2;
 static const string ATTACK_ENTRY = "Support attack helicopter";

 protected ACR_Supports m_Supports;
 ref array<ref ACR_StationedHeli> m_Helis = {};
 protected float m_fNextTick;

 void ACR_HeliStation(ACR_Supports supports) { m_Supports = supports; }

 static bool InUse() { return !ACR_Helipad.s_All.IsEmpty(); }

 // This side's helicopter supports fly from helipads (bought for a pad): any pad in the world,
 // unless a skirmish layout left the side without one (then the off-map helicopters).
 static bool UsesPads(string factionKey)
 {
  return InUse() && ACR_Skirmish.SideHasPads(factionKey);
 }

 static bool IsHeliType(ACR_ESupportType type)
 {
  return type == ACR_ESupportType.HELI_GUN_RUN || type == ACR_ESupportType.HELI_LOITER || type == ACR_ESupportType.HELI_DOOR_GUNS || type == ACR_ESupportType.ATTACK_HELI;
 }

 // Flown by a content pack's attack helicopter (ACR_SocketAttackHeli), not the gunship.
 static bool IsAttack(ACR_ESupportType type)
 {
  return type == ACR_ESupportType.ATTACK_HELI;
 }

 static bool NeedsGunship(ACR_ESupportType type)
 {
  return type != ACR_ESupportType.HELI_DOOR_GUNS;
 }

 // Buy-list entries (only in worlds with helipads).
 static ACR_RequisitionEntry MakeEntry(bool gunship, ACR_FactionSettings settings = null)
 {
  ACR_RequisitionEntry entry = new ACR_RequisitionEntry();
  if (gunship) entry.Set(GUNSHIP_ENTRY, "", "", GUNSHIP_COST, STOCK);
  else entry.Set(TRANSPORT_ENTRY, "", "", TRANSPORT_COST, STOCK);
  entry.m_iSupportHeli = 2;
  if (gunship) entry.m_iSupportHeli = 1;
  entry.m_bAircraft = true; // the AI's normal buying skips it (BuyHelicopter buys it)
  if (settings)
  {
   ACR_ESupportUnitRole role = ACR_ESupportUnitRole.DOOR_GUN_HELI;
   if (gunship) role = ACR_ESupportUnitRole.GUNSHIP;
   ACR_SupportUnitSettings unit = settings.SupportUnit(role);
   if (unit)
   {
    if (!unit.m_sName.IsEmpty()) entry.m_sName = unit.m_sName;
    if (unit.m_iCost >= 0) entry.m_iCost = unit.m_iCost;
    if (unit.m_iStock >= 0) entry.m_iStock = unit.m_iStock;
   }
  }
  return entry;
 }

 // The buy-list entry for the side's attack helicopter (m_iSupportHeli 3), or null: the content
 // pack names none for the side.
 static ACR_RequisitionEntry MakeAttackEntry(string factionKey)
 {
  ACR_SocketAttackHeli heli = ACR_Supports.AttackHelicopterFor(factionKey);
  if (!heli) return null;
  int cost = ATTACK_COST;
  if (heli.m_iStationCost >= 0) cost = heli.m_iStationCost;
  ACR_RequisitionEntry entry = new ACR_RequisitionEntry();
  entry.Set(string.Format("%1 (%2)", ATTACK_ENTRY, heli.m_sName), "", "", cost, ATTACK_STOCK);
  entry.m_iSupportHeli = 3;
  entry.m_bAircraft = true; // the AI's normal buying skips it (BuyHelicopter buys it)
  return entry;
 }

 bool PadTaken(ACR_Helipad pad)
 {
  // (A helicopter whose pad changed hands doesn't hold it for the new owner.)
  foreach (ACR_StationedHeli heli : m_Helis) if (heli.m_Pad == pad && pad.Faction() == heli.m_sFaction) return true;
  return false;
 }

 // A free pad at this base (owned by the side), or null.
 ACR_Helipad FreePad(string factionKey, ACR_Base base)
 {
  return ClearPad(factionKey, base);
 }

 // A free pad at this base with nothing parked on it (bought helicopters land there).
 ACR_Helipad ClearPad(string factionKey, ACR_Base base)
 {
  array<IEntity> vehicles = {};
  ACR_Vehicles.All(vehicles);
  foreach (ACR_Helipad pad : ACR_Helipad.s_All)
  {
   if (pad.Faction() != factionKey || (base && pad.Base() != base) || PadTaken(pad)) continue;
   bool blocked;
   foreach (IEntity vehicle : vehicles)
    if (vector.DistanceXZ(vehicle.GetOrigin(), pad.GetOrigin()) < 10) { blocked = true; break; }
   if (!blocked) return pad;
  }
  return null;
 }

 int PadsAt(ACR_Base base)
 {
  int count;
  foreach (ACR_Helipad pad : ACR_Helipad.s_All) if (!pad.m_bGone && pad.Base() == base) count++;
  return count;
 }

 // Why a helicopter can't be bought at this base ("" = it can).
 string CantBuy(string factionKey, ACR_Base base)
 {
  if (FreePad(factionKey, base)) return "";
  if (PadsAt(base) == 0) return base.m_sName + " has no helipad";
  return "No free helipad at " + base.m_sName;
 }

 // Spawns the helicopter and its crew on a free pad at the base.
 ACR_StationedHeli Station(string factionKey, bool gunship, ACR_Base base, string name, bool attack = false)
 {
  ACR_Helipad pad = FreePad(factionKey, base);
  if (!pad) return null;
  ResourceName prefab = m_Supports.TransportHelicopterFor(factionKey);
  if (gunship) prefab = m_Supports.HelicopterFor(factionKey);
  if (attack)
  {
   ACR_SocketAttackHeli packed = ACR_Supports.AttackHelicopterFor(factionKey);
   if (!packed) return null;
   prefab = packed.m_sPrefab;
   gunship = true;
  }
  Resource resource = Resource.Load(prefab);
  if (!resource.IsValid()) return null;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  Math3D.AnglesToMatrix(Vector(pad.Yaw(), 0, 0), parameters.Transform);
  parameters.Transform[3] = pad.Spot();
  IEntity vehicle = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  if (!vehicle) return null;
  // Registered before the crew exists, so the squad scan never takes them for a squad.
  m_Supports.m_Vehicles.Insert(vehicle);
  ACR_Vehicles.SetOwner(vehicle, factionKey);
  // (A pack's airframe gets its crew from the pack: ACR_ContentSocket.Crewman.)
  ResourceName selectedCrew;
  if (!attack) selectedCrew = m_Supports.ConfiguredCrewPrefab(factionKey, gunship);
  if (!selectedCrew.IsEmpty()) ACR_Vehicles.SetCrewPrefab(vehicle, selectedCrew);
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  array<ECompartmentType> seats = {ECompartmentType.PILOT, ECompartmentType.TURRET};
  if (compartments) compartments.SpawnDefaultOccupants(seats);
  GetGame().GetCallqueue().CallLater(RememberStationCrew, 1000, false, vehicle);
  ACR_StationedHeli heli = new ACR_StationedHeli();
  heli.m_sFaction = factionKey;
  heli.m_sName = name;
  heli.m_bGunship = gunship;
  heli.m_bAttack = attack;
  heli.m_Vehicle = vehicle;
  heli.m_Pad = pad;
  heli.m_eState = ACR_EHeliState.READY;
  m_Helis.Insert(heli);
  PrintFormat("[ACR] HELIPAD %1 %2 (%3) stationed on the pad at %4 (%5)", factionKey, name, prefab, pad.Spot(), base.m_sName);
  return heli;
 }

 protected void RememberStationCrew(IEntity vehicle)
 {
  if (vehicle) m_Supports.RememberCrew(vehicle);
 }

 bool Usable(ACR_StationedHeli heli)
 {
  if (!heli.m_Vehicle || heli.m_eState != ACR_EHeliState.READY) return false;
  return heli.m_Pad && heli.m_Pad.Faction() == heli.m_sFaction;
 }

 // The ready helicopter of the right kind nearest the target.
 ACR_StationedHeli ReadyFor(string factionKey, bool gunship, vector target, bool attack = false)
 {
  ACR_StationedHeli best;
  float bestDistance = float.MAX;
  foreach (ACR_StationedHeli heli : m_Helis)
  {
   if (heli.m_sFaction != factionKey || heli.m_bGunship != gunship || heli.m_bAttack != attack || !Usable(heli)) continue;
   float d = vector.DistanceXZ(heli.m_Vehicle.GetOrigin(), target);
   if (d < bestDistance) { bestDistance = d; best = heli; }
  }
  return best;
 }

 // Refusal text for a helicopter support with none ready.
 string WhyNot(string factionKey, bool gunship, bool attack = false)
 {
  string kind = "door-gunner helicopter";
  string entry = TRANSPORT_ENTRY;
  if (gunship) { kind = "gunship"; entry = GUNSHIP_ENTRY; }
  if (attack) { kind = "attack helicopter"; entry = ATTACK_ENTRY; }
  float now = GetGame().GetWorld().GetWorldTime();
  int soonest = -1;
  bool flying;
  foreach (ACR_StationedHeli heli : m_Helis)
  {
   if (heli.m_sFaction != factionKey || heli.m_bGunship != gunship || heli.m_bAttack != attack || !heli.m_Vehicle) continue;
   if (heli.m_eState == ACR_EHeliState.FLYING) flying = true;
   if (heli.m_eState == ACR_EHeliState.SERVICING)
   {
    int left = Math.Max(0, Math.Ceil((heli.m_fReadyAt - now) / 1000));
    if (soonest < 0 || left < soonest) soonest = left;
   }
  }
  if (soonest >= 0) return string.Format("No %1 ready: being serviced on its pad (%2 s)", kind, soonest);
  if (flying) return string.Format("No %1 ready: it is still out on a mission", kind);
  return string.Format("No %1 on a helipad: buy one (%2)", kind, entry);
 }

 // Short status for the support list: "[1 ready]", "[servicing]", "[none: buy one]".
 string StatusFor(string factionKey, bool gunship, bool attack = false)
 {
  int ready, flying, servicing;
  float now = GetGame().GetWorld().GetWorldTime();
  int soonest = -1;
  foreach (ACR_StationedHeli heli : m_Helis)
  {
   if (heli.m_sFaction != factionKey || heli.m_bGunship != gunship || heli.m_bAttack != attack || !heli.m_Vehicle) continue;
   if (Usable(heli)) ready++;
   else if (heli.m_eState == ACR_EHeliState.FLYING) flying++;
   else if (heli.m_eState == ACR_EHeliState.SERVICING)
   {
    servicing++;
    int left = Math.Max(0, Math.Ceil((heli.m_fReadyAt - now) / 1000));
    if (soonest < 0 || left < soonest) soonest = left;
   }
  }
  if (ready > 0) return string.Format("%1 ready", ready);
  if (servicing > 0) return "servicing"; // no countdown: the panel would resend every second
  if (flying > 0) return "out on a mission";
  return "none: buy one";
 }

 int CountOf(string factionKey, bool gunship, bool attack = false)
 {
  int count;
  foreach (ACR_StationedHeli heli : m_Helis) if (heli.m_sFaction == factionKey && heli.m_bGunship == gunship && heli.m_bAttack == attack && heli.m_Vehicle) count++;
  return count;
 }

 void Launched(ACR_StationedHeli heli)
 {
  heli.m_eState = ACR_EHeliState.FLYING;
  heli.m_iSorties++;
 }

 // Back on its pad: rearm, repair, new crew for the dead, then ready after SERVICE_SECONDS.
 void Landed(ACR_StationedHeli heli)
 {
  heli.m_eState = ACR_EHeliState.SERVICING;
  heli.m_fReadyAt = GetGame().GetWorld().GetWorldTime() + SERVICE_SECONDS * 1000;
  Service(heli);
  m_Supports.m_Manager.NotifyFaction(heli.m_sFaction, string.Format("%1 landed: servicing, ready in %2 s.", heli.m_sName, SERVICE_SECONDS));
  PrintFormat("[ACR] HELIPAD %1 landed on its pad (%2 m off the pad centre), servicing", heli.m_sName, vector.DistanceXZ(heli.m_Vehicle.GetOrigin(), heli.m_Pad.GetOrigin()));
 }

 protected void Service(ACR_StationedHeli heli)
 {
  array<IEntity> vehicles = {heli.m_Vehicle};
  m_Supports.m_Manager.m_Requisition.ServiceVehicles(vehicles);
  ReplaceCrew(heli.m_Vehicle, heli.m_sFaction, heli.m_bGunship, heli.m_bAttack);
 }

 // Empty pilot and gun seats get a new crewman (the dead are thrown out first, the wounded
 // patched up). Crewmen who got out (a helicopter shot up on its pad: the crew bails) are
 // replaced too: 4 Oct live, a repaired helicopter came back short of crew, and the crew on
 // foot was taken for a squad.
 void ReplaceCrew(IEntity vehicle, string factionKey, bool gunship = true, bool attack = false)
 {
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  m_Supports.RememberCrew(vehicle);
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  AIGroup crewGroup;
  foreach (BaseCompartmentSlot seat : slots)
  {
   IEntity occupant = seat.GetOccupant();
   if (!occupant) continue;
   if (!ACR_Respawn.IsAlive(occupant))
   {
    SCR_EntityHelper.DeleteEntityAndChildren(occupant);
    continue;
   }
   SCR_CharacterDamageManagerComponent wounds = SCR_CharacterDamageManagerComponent.Cast(occupant.FindComponent(SCR_CharacterDamageManagerComponent));
   if (wounds && (wounds.GetHealthScaled() < 0.99 || wounds.IsBleeding())) wounds.FullHeal();
   AIControlComponent seated = AIControlComponent.Cast(occupant.FindComponent(AIControlComponent));
   if (!crewGroup && seated && seated.GetControlAIAgent()) crewGroup = seated.GetControlAIAgent().GetParentGroup();
  }
  int stragglers;
  SCR_AIGroup known = m_Supports.CrewOf(vehicle);
  if (known)
  {
   array<AIAgent> agents = {};
   known.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity man = agent.GetControlledEntity();
    if (!man || Aboard(man, vehicle)) continue;
    SCR_EntityHelper.DeleteEntityAndChildren(man);
    stragglers++;
   }
   if (!crewGroup && stragglers < agents.Count()) crewGroup = known;
  }
  if (stragglers > 0) PrintFormat("[ACR] HELIPAD %1 crewmen who got out replaced", stragglers);
  GetGame().GetCallqueue().CallLater(SeatCrew, 300, false, vehicle, factionKey, gunship, crewGroup, attack);
 }

 static bool Aboard(IEntity man, IEntity vehicle)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(man);
  if (!character || !character.GetCompartmentAccessComponent()) return false;
  BaseCompartmentSlot slot = character.GetCompartmentAccessComponent().GetCompartment();
  return slot && slot.GetVehicle() == vehicle;
 }

 protected void SeatCrew(IEntity vehicle, string factionKey, bool gunship, AIGroup crewGroup, bool attack)
 {
  if (!vehicle || Destroyed(vehicle)) return;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  ResourceName crewman = m_Supports.CrewPrefab(factionKey, gunship);
  int seated;
  foreach (BaseCompartmentSlot slot : slots)
  {
   if (slot.GetOccupant()) continue;
   if (slot.GetType() != ECompartmentType.PILOT && slot.GetType() != ECompartmentType.TURRET) continue;
   // A pack's airframe: the pack's pilot or gunner for that seat.
   ResourceName seatCrew = crewman;
   if (attack)
   {
    ResourceName packed = ACR_ContentSocket.Get().Crewman(factionKey, vehicle, slot.GetType());
    if (!packed.IsEmpty()) seatCrew = packed;
   }
   IEntity man = slot.SpawnCharacterInCompartment(seatCrew, crewGroup);
   if (man) seated++;
  }
  if (seated > 0) PrintFormat("[ACR] HELIPAD %1 new crewmen seated", seated);
  m_Supports.RememberCrew(vehicle);
 }

 // Replace the airframe's default occupants when the mission maker selected a crew prefab.
 // Keeping the original crew group preserves the support-crew tracking used by the squad scan.
 void ApplyCrewPrefab(IEntity vehicle, ResourceName crewman)
 {
  if (!vehicle || crewman.IsEmpty()) return;
  Resource resource = Resource.Load(crewman);
  if (!resource.IsValid())
  {
   PrintFormat("[ACR] HELIPAD custom crew prefab invalid: %1; keeping airframe defaults", crewman);
   return;
  }
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> slots = {};
  compartments.GetCompartments(slots);
  AIGroup crewGroup;
  int seats;
  foreach (BaseCompartmentSlot slot : slots)
  {
   if (slot.GetType() != ECompartmentType.PILOT && slot.GetType() != ECompartmentType.TURRET) continue;
   seats++;
   IEntity occupant = slot.GetOccupant();
   if (!occupant) continue;
   AIControlComponent control = AIControlComponent.Cast(occupant.FindComponent(AIControlComponent));
   if (!crewGroup && control && control.GetControlAIAgent()) crewGroup = control.GetControlAIAgent().GetParentGroup();
  }
  int replaced;
  foreach (BaseCompartmentSlot seat : slots)
  {
   if (seat.GetType() != ECompartmentType.PILOT && seat.GetType() != ECompartmentType.TURRET) continue;
   IEntity occupant = seat.GetOccupant();
   if (occupant) SCR_EntityHelper.DeleteEntityAndChildren(occupant);
   if (seat.SpawnCharacterInCompartment(crewman, crewGroup)) replaced++;
  }
  if (replaced < seats)
  {
   foreach (BaseCompartmentSlot seat : slots)
   {
    if (seat.GetType() != ECompartmentType.PILOT && seat.GetType() != ECompartmentType.TURRET) continue;
    IEntity occupant = seat.GetOccupant();
    if (occupant) SCR_EntityHelper.DeleteEntityAndChildren(occupant);
   }
   array<ECompartmentType> defaults = {ECompartmentType.PILOT, ECompartmentType.TURRET};
   compartments.SpawnDefaultOccupants(defaults);
   PrintFormat("[ACR] HELIPAD custom crew %1 seated %2/%3; restored airframe crew", crewman, replaced, seats);
  }
  else PrintFormat("[ACR] HELIPAD custom crew %1 seated in %2 compartment(s)", crewman, replaced);
  m_Supports.RememberCrew(vehicle);
 }

 // Every second (server): losses, end of servicing, damage while parked.
 void Tick(float now)
 {
  if (now < m_fNextTick) return;
  m_fNextTick = now + 1000;
  for (int i = m_Helis.Count() - 1; i >= 0; i--)
  {
   ACR_StationedHeli heli = m_Helis[i];
   if (!heli.m_Vehicle || Destroyed(heli.m_Vehicle))
   {
    Lost(heli);
    continue;
   }
   // Its pad's base was captured, or a built pad dismantled: Usable needs the side's own pad, so the
   // helicopter sat there unusable for good (and blocked the pad). Re-home it to a free pad of the
   // side: it flies from where it stands and lands there after its next run.
   bool parked = heli.m_eState == ACR_EHeliState.READY || heli.m_eState == ACR_EHeliState.SERVICING;
   if (parked && heli.m_Pad && heli.m_Pad.Faction() != heli.m_sFaction)
   {
    ACR_Helipad home = ClearPad(heli.m_sFaction, null);
    if (home)
    {
     heli.m_Pad = home;
     string where = "another pad";
     if (home.Base()) where = home.Base().m_sName;
     m_Supports.m_Manager.NotifyFaction(heli.m_sFaction, string.Format("%1 lost its pad: it is based at %2 now.", heli.m_sName, where));
     PrintFormat("[ACR] HELIPAD %1 re-homed to %2", heli.m_sName, where);
    }
   }
   if (heli.m_eState == ACR_EHeliState.SERVICING && now >= heli.m_fReadyAt)
   {
    heli.m_eState = ACR_EHeliState.READY;
    Service(heli);
    m_Supports.m_Manager.NotifyFaction(heli.m_sFaction, heli.m_sName + " ready on its pad.");
    PrintFormat("[ACR] HELIPAD %1 ready", heli.m_sName);
   }
   // Shot up on the pad: it can still be put right there.
   DamageManagerComponent parkedDamage = DamageManagerComponent.Cast(heli.m_Vehicle.FindComponent(DamageManagerComponent));
   float crippled = 0;
   if (parkedDamage) crippled = parkedDamage.GetMovementDamage();
   if (heli.m_eState == ACR_EHeliState.READY && (ACR_Logistics.Health(heli.m_Vehicle) < 0.6 || crippled >= 0.5))
   {
    heli.m_eState = ACR_EHeliState.SERVICING;
    heli.m_fReadyAt = now + SERVICE_SECONDS * 1000;
    PrintFormat("[ACR] HELIPAD %1 damaged on its pad (health %2): servicing", heli.m_sName, ACR_Logistics.Health(heli.m_Vehicle));
   }
  }
 }

 // Destroyed, or crippled and going down: gone for good (buy another).
 void Lost(ACR_StationedHeli heli)
 {
  if (!m_Helis.Contains(heli)) return;
  m_Supports.m_Manager.NotifyFaction(heli.m_sFaction, string.Format("%1 was lost. Buy a new %2 to fly that support again.", heli.m_sName, heli.Kind()));
  PrintFormat("[ACR] HELIPAD %1 %2 lost (state %3)", heli.m_sFaction, heli.m_sName, heli.m_eState);
  // The wreck leaves after a while (a new one may need the pad).
  if (heli.m_Vehicle) GetGame().GetCallqueue().CallLater(ClearWreck, 90000, false, heli.m_Vehicle);
  m_Helis.RemoveItem(heli);
 }

 protected void ClearWreck(IEntity wreck)
 {
  if (!wreck) return;
  array<IEntity> crew = {};
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(wreck.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (compartments)
  {
   array<BaseCompartmentSlot> slots = {};
   compartments.GetCompartments(slots);
   foreach (BaseCompartmentSlot slot : slots) if (slot.GetOccupant()) crew.Insert(slot.GetOccupant());
  }
  foreach (IEntity member : crew) SCR_EntityHelper.DeleteEntityAndChildren(member);
  SCR_EntityHelper.DeleteEntityAndChildren(wreck);
 }

 static bool Destroyed(IEntity vehicle)
 {
  DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
  return damage && damage.GetState() == EDamageState.DESTROYED;
 }
}
