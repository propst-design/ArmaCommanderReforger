// Gunners stay on held guns (-acrHoldGuns 1, the default; setup row "Gunners stay on held
// guns"): BBB does the holding (BBB_HeldGuns: vanilla's "something is outside the arc, get out"
// is overruled unless an enemy the gunner saw in the last 10 s is within 60 m, and a held gun
// that stands empty for 8 s is manned again by the nearest man on foot). ACA says which guns a
// squad holds (ACR_BBBHooks.GetHeldGuns): a squad holding a spot (an order in the DEFEND
// state: Defending, Holding roadblock, Holding position, a Fortify hold) holds its roadblock's
// gun and the static guns its men sat in there, within its hold radius plus HOLD_REACH:
// checkpoint guns, Fortify nests, FOB defence guns. In a playtest (25 Oct) a roadblock MG left
// its gun every 3 minutes, often with no enemy near. Vehicles keep vanilla's (and BBB's) rules.
// Winchester (#154): BBB gets a gunner off a gun that is out of ammo and doesn't hold or re-man it
// (BBB_HeldGuns, -bbbGunWinchester), so a held gun lasts only as long as its ammo. A held gun (a
// squad's or a reserve crew's) carries RESERVE_BOXES boxes beyond its own: its storage holds only
// the stock spares (an M2 tripod: three 100-round boxes, 15 l), so the reserve tops the spares back
// up as the gunner loads them (Restock), and the gun goes dry once the reserve is spent too.
class ACR_GunStock : Managed
{
 ResourceName m_sBox;   // the gun's own box
 int m_iSpares;         // spares it stows when full (stock count, at first sight)
 int m_iReserve;        // boxes left to top them up with
}

class ACR_GunHold
{
 static const float HOLD_REACH = 25;   // a gun this far past the order's radius still counts
 static const int RESERVE_BOXES = 6;   // M2: 4 boxes stock (400 rounds), 10 held (1000)
 protected static ref map<IEntity, ref ACR_GunStock> s_mStock = new map<IEntity, ref ACR_GunStock>();
 static int s_iRestocked; // boxes added (tests)

 static bool Enabled() { return ACR_LogisticsRules.Choice("acrHoldGuns", "1") != "0"; }

 // A static gun (not a vehicle) at the spot this squad holds.
 static bool IsHeld(ACR_Squad squad, IEntity gun)
 {
  if (!squad || !gun || Vehicle.Cast(gun) || squad.HasPlayer() || squad.m_Commander != 0) return false;
  if (squad.OrderState() != ACR_EOrderState.DEFEND) return false;
  float reach = Math.Max(squad.m_fOrderRadius, 15) + HOLD_REACH;
  return vector.DistanceXZ(gun.GetOrigin(), squad.m_MoveTarget) <= reach;
 }

 // The guns BBB keeps manned for this squad.
 static void HeldGuns(ACR_Squad squad, notnull array<IEntity> guns)
 {
  guns.Clear();
  if (!squad || !Enabled()) return;
  foreach (IEntity gun : squad.m_aHeldGuns)
   if (gun && IsHeld(squad, gun)) guns.Insert(gun);
  if (squad.m_ActiveRoadblock)
  {
   IEntity turret = squad.m_ActiveRoadblock.m_Turret;
   if (turret && !guns.Contains(turret) && IsHeld(squad, turret)) guns.Insert(turret);
  }
 }

 // A new order (ACR_Squad.NewOrder): the squad leaves the spot it held, so it holds no gun there any
 // more, and a re-man BBB queued a moment before for one of them (BBB_HeldGuns: an empty held gun is
 // manned again after 8 s) is called off. Before, that soldier climbed back on after the order and the
 // squad stayed put: srv:roadblocks' gun rounds failed about 1 in 10 on main, and half the time with
 // the roadblock lane's post nearer the gun ("Stuck trying to get into vehicle", then the gun manned).
 static int Release(ACR_Squad squad)
 {
  if (!squad) return 0;
  array<IEntity> guns = {};
  foreach (IEntity held : squad.m_aHeldGuns) if (held) guns.Insert(held);
  if (squad.m_RoadblockTurret && !guns.Contains(squad.m_RoadblockTurret)) guns.Insert(squad.m_RoadblockTurret);
  if (squad.m_ActiveRoadblock && squad.m_ActiveRoadblock.m_Turret && !guns.Contains(squad.m_ActiveRoadblock.m_Turret)) guns.Insert(squad.m_ActiveRoadblock.m_Turret);
  squad.m_aHeldGuns.Clear();
  if (guns.IsEmpty()) return 0;
  int cancelled;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
   if (!utility) continue;
   SCR_AIGetInVehicle getIn = SCR_AIGetInVehicle.Cast(utility.FindActionOfType(SCR_AIGetInVehicle));
   if (!getIn || !getIn.m_Vehicle || !guns.Contains(getIn.m_Vehicle.m_Value)) continue;
   getIn.Fail();
   cancelled++;
  }
  s_iReleased += cancelled;
  if (cancelled > 0) PrintFormat("[ACR] SQUAD %1: called off %2 re-man(s) of the gun it held, for its new order", squad.GetName(), cancelled);
  return cancelled;
 }

 static int s_iReleased; // re-mans called off (tests)

 // Every tactics tick of a squad: note the held guns its men sit in (they stay held while the
 // squad holds the spot, manned or not).
 static void Tick(ACR_Squad squad)
 {
  if (!squad.m_Group) return;
  if (!Enabled() || squad.OrderState() != ACR_EOrderState.DEFEND || squad.HasPlayer())
  {
   squad.m_aHeldGuns.Clear();
   return;
  }
  for (int i = squad.m_aHeldGuns.Count() - 1; i >= 0; i--)
  {
   IEntity held = squad.m_aHeldGuns[i];
   if (!held || ACR_Logistics.Health(held) <= 0 || !IsHeld(squad, held)) squad.m_aHeldGuns.Remove(i);
  }
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || ACR_Transport.SeatOf(body) != ECompartmentType.TURRET) continue;
   IEntity gun = ACR_Transport.VehicleOf(body);
   if (!gun || squad.m_aHeldGuns.Contains(gun) || !IsHeld(squad, gun)) continue;
   squad.m_aHeldGuns.Insert(gun);
  }
  foreach (IEntity heldGun : squad.m_aHeldGuns) Restock(heldGun);
  if (squad.m_ActiveRoadblock && IsHeld(squad, squad.m_ActiveRoadblock.m_Turret)) Restock(squad.m_ActiveRoadblock.m_Turret);
 }

 // A held gun's spares topped back up from its reserve (see the header). Once a second per
 // holding squad or crew; the first call gives the gun its reserve.
 static void Restock(IEntity gun)
 {
  if (!gun || !Enabled() || ACR_Logistics.Health(gun) <= 0) return;
  TurretControllerComponent controller = BBB_VehicleHelpers.TurretControllerOf(BBB_HeldGuns.GunSeat(gun));
  if (!controller || !controller.GetWeaponManager()) return;
  BaseWeaponComponent weapon = controller.GetWeaponManager().GetCurrentWeapon();
  InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(controller.GetOwner().FindComponent(InventoryStorageManagerComponent));
  if (!weapon || !inventory) return;
  int spares = inventory.GetMagazineCountByWeapon(weapon);
  ACR_GunStock stock = s_mStock.Get(gun);
  if (!stock)
  {
   PruneStock();
   stock = new ACR_GunStock();
   stock.m_sBox = BoxOf(weapon, inventory);
   stock.m_iSpares = Math.Max(spares, 1);
   stock.m_iReserve = RESERVE_BOXES;
   s_mStock.Set(gun, stock);
  }
  if (stock.m_iReserve <= 0 || stock.m_sBox.IsEmpty() || spares >= stock.m_iSpares) return;
  if (!inventory.TrySpawnPrefabToStorage(stock.m_sBox)) return;
  stock.m_iReserve--;
  s_iRestocked++;
  PrintFormat("[ACR] GUNHOLD %1: a box from the held gun's reserve (%2 left)", ACR_Vehicles.NameOf(gun), stock.m_iReserve);
 }

 // Boxes left in this gun's reserve (-1: it has none yet).
 static int Reserve(IEntity gun)
 {
  ACR_GunStock stock = s_mStock.Get(gun);
  if (!stock) return -1;
  return stock.m_iReserve;
 }

 static void SetReserveForTest(IEntity gun, int boxes)
 {
  ACR_GunStock stock = s_mStock.Get(gun);
  if (stock) stock.m_iReserve = boxes;
 }

 // The box the gun is loaded with, or one it stows.
 protected static ResourceName BoxOf(BaseWeaponComponent weapon, InventoryStorageManagerComponent inventory)
 {
  BaseMagazineComponent loaded = weapon.GetCurrentMagazine();
  if (loaded && loaded.GetOwner() && loaded.GetOwner().GetPrefabData()) return loaded.GetOwner().GetPrefabData().GetPrefabName();
  array<IEntity> items = {};
  inventory.GetItems(items);
  foreach (IEntity item : items)
   if (item.FindComponent(BaseMagazineComponent) && item.GetPrefabData()) return item.GetPrefabData().GetPrefabName();
  return ResourceName.Empty;
 }

 // Deleted guns leave null keys behind (pruned by index).
 protected static void PruneStock()
 {
  for (int i = s_mStock.Count() - 1; i >= 0; i--)
   if (!s_mStock.GetKey(i)) s_mStock.RemoveElement(i);
 }

 static void Reset()
 {
  s_mStock.Clear();
  s_iRestocked = 0;
 }
}
