// Socket for an era content pack (#108: Cold War vehicles and kit from Workshop mods). ACA never
// names a pack's mods: a compat add-on that depends on ACA and on exactly the mods it vets plugs
// in with `modded class ACR_ContentSocket` and overrides the hooks below. Unplugged, every hook
// keeps ACA as before.
// Why a filter: vehicle and kit mods add themselves to the vanilla US/USSR/FIA entity catalogs,
// which feed the auto-filled buy lists, garrisons, crewmen and prefab swaps. Loading one put its raw
// prefabs (wrong crews, test variants, out-of-period kit) straight into the buy menu. The pack
// decides which catalog entries stay and adds its own curated entries instead.
enum ACR_ESocketVehicle
{
 ARMED = 1,         // has a weapon the AI can fight with
 TRANSPORT = 2,     // unarmed truck the AI buys to ferry squads
 CARRIER = 4,       // armed APC/IFV (Blitz's starting kit)
 SQUAD_CARRIER = 8, // carrier that seats a whole squad
 AIRCRAFT = 16,     // the AI commander never buys these
 RARE = 32          // fixed stock (armour, aircraft)
}

// A pack's attack helicopter for one side (AttackHelicopter): that side's commander gets an
// attack-helicopter support beside the gunship and door gunners, flown by script like them
// (ACR_HeliRun: nose guns fly attack passes, rocket salvos), and in worlds with helipads a support
// attack helicopter to buy for a pad. Its crew comes from Crewman.
class ACR_SocketAttackHeli : Managed
{
 string m_sName;           // the airframe in the panel and buy list ("AH-1S Cobra")
 ResourceName m_sPrefab;
 int m_iCost = -1;         // RP for each call (-1 = ACA's default)
 int m_iStationCost = -1;  // RP to buy one for a helipad (-1 = ACA's default)
 ResourceName m_sRocket;   // rocket it fires ("" = the side's usual)
}

class ACR_ContentSocket : Managed
{
 protected static ref ACR_ContentSocket s_Instance;
 protected bool m_bLogged;

 static ACR_ContentSocket Get()
 {
  if (!s_Instance) s_Instance = new ACR_ContentSocket();
  if (!s_Instance.m_bLogged && s_Instance.Plugged())
  {
   s_Instance.m_bLogged = true;
   PrintFormat("[ACR] CONTENT pack plugged in: %1", s_Instance.Name());
  }
  return s_Instance;
 }

 // ---------------------------------------------------------------- hooks (override in a compat add-on)

 // The plugged-in pack's name for logs; empty while nothing is plugged in.
 string Name() { return ""; }
 bool Plugged() { return !Name().IsEmpty(); }

 // Whether a faction catalog entry may be used (buy lists, garrisons, crewmen, prefab swaps).
 bool AllowsCatalogEntry(string factionKey, EEntityCatalogType type, ResourceName prefab) { return true; }

 // The pack's own buy-list entries for a side, added after the catalog ones (auto-filled lists only:
 // a mission's own list stays as authored). Use AddVehicle and AddFormation.
 void AddRequisitions(string factionKey, array<ref ACR_RequisitionEntry> result) {}

 // The crewman (or pilot) for a seat of a vehicle this side bought; "" keeps ACA's choice.
 // Also fills crew seats the vehicle leaves without a default occupant.
 ResourceName Crewman(string factionKey, IEntity vehicle, ECompartmentType seat) { return ""; }

 // The soldier to spawn in place of one of the side's soldiers: members of bought and starting
 // squads, riders, garrisons (and so FIA cells). May vary per call; the prefab itself keeps it.
 ResourceName Soldier(string factionKey, ResourceName prefab) { return prefab; }

 // The side's attack helicopter for the attack-helicopter support, or null (no such support).
 // Use MakeAttackHelicopter; chain to super for the sides the pack leaves alone.
 ACR_SocketAttackHeli AttackHelicopter(string factionKey) { return null; }

 // The side's command-unit (player officer) prefab. Must give the same answer on every machine:
 // loadouts replicate as list indexes.
 ResourceName Officer(string factionKey, ResourceName prefab) { return prefab; }

 // ---------------------------------------------------------------- used by ACA

 protected static ref map<ResourceName, ref array<ResourceName>> s_GroupSlots = new map<ResourceName, ref array<ResourceName>>();

 // Puts Soldier()'s swaps into a formation: the listed soldiers, or the group prefab's own members
 // when the list is empty. False (list untouched) when nothing was swapped.
 bool SwapSoldiers(string factionKey, ResourceName group, notnull array<ResourceName> soldiers)
 {
  if (!Plugged() || factionKey.IsEmpty()) return false;
  array<ResourceName> men = {};
  if (!soldiers.IsEmpty()) men.Copy(soldiers);
  else if (!GroupSlots(group, men)) return false;
  bool changed;
  foreach (int i, ResourceName man : men)
  {
   ResourceName swapped = Soldier(factionKey, man);
   if (swapped.IsEmpty() || swapped == man) continue;
   men[i] = swapped;
   changed = true;
  }
  if (!changed) return false;
  soldiers.Copy(men);
  return true;
 }

 // A group prefab's members (m_aUnitPrefabSlots, inherited values included).
 protected static bool GroupSlots(ResourceName group, notnull array<ResourceName> result)
 {
  array<ResourceName> cached;
  if (!s_GroupSlots.Find(group, cached))
  {
   cached = {};
   Resource resource = Resource.Load(group);
   if (resource && resource.IsValid())
   {
    IEntitySource source = resource.GetResource().ToEntitySource();
    if (source) source.Get("m_aUnitPrefabSlots", cached);
   }
   s_GroupSlots.Set(group, cached);
  }
  result.Copy(cached);
  return !result.IsEmpty();
 }

 // ---------------------------------------------------------------- helpers for the pack

 // A buyable vehicle, skipped when the list already has one of that prefab or name.
 static ACR_RequisitionEntry AddVehicle(array<ref ACR_RequisitionEntry> result, string name, ResourceName prefab, int cost, int stock, int flags)
 {
  foreach (ACR_RequisitionEntry existing : result)
   if (existing.m_sVehicle == prefab || existing.m_sName == name) return null;
  ACR_RequisitionEntry item = new ACR_RequisitionEntry();
  item.Set(name, "", prefab, cost, stock);
  item.m_bArmed = (flags & ACR_ESocketVehicle.ARMED) != 0;
  item.m_bTransport = (flags & ACR_ESocketVehicle.TRANSPORT) != 0;
  item.m_bCarrier = (flags & ACR_ESocketVehicle.CARRIER) != 0;
  item.m_bSquadCarrier = (flags & ACR_ESocketVehicle.SQUAD_CARRIER) != 0;
  item.m_bAircraft = (flags & ACR_ESocketVehicle.AIRCRAFT) != 0;
  item.m_bRare = (flags & ACR_ESocketVehicle.RARE) != 0 || item.m_bAircraft;
  result.Insert(item);
  return item;
 }

 // An attack helicopter for AttackHelicopter.
 static ACR_SocketAttackHeli MakeAttackHelicopter(string name, ResourceName prefab, int cost = -1, int stationCost = -1)
 {
  ACR_SocketAttackHeli heli = new ACR_SocketAttackHeli();
  heli.m_sName = name;
  heli.m_sPrefab = prefab;
  heli.m_iCost = cost;
  heli.m_iStationCost = stationCost;
  return heli;
 }

 // A buyable infantry formation: these soldiers, in this order, spawned into the side's base
 // group (as ACR_InfantryCatalog does); they are also its reinforcement recipe. Skipped when the
 // list already has an entry of that name.
 static ACR_RequisitionEntry AddFormation(array<ref ACR_RequisitionEntry> result, string name, ResourceName group, int cost, ACR_EFormation formation, array<ResourceName> soldiers)
 {
  foreach (ACR_RequisitionEntry existing : result)
   if (existing.m_sName == name) return null;
  ACR_RequisitionEntry item = new ACR_RequisitionEntry();
  item.Set(name, group, "", cost, 4);
  item.m_eFormation = formation;
  item.m_aSoldiers = {};
  item.m_aSoldiers.Copy(soldiers);
  result.Insert(item);
  return item;
 }
}
