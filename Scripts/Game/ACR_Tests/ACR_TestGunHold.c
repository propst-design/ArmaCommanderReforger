// -acrGunHoldTest 1 (server-capable, ~1 min): gunners stay on held guns (ACR_GunHold says which,
// BBB_HeldGuns holds them). A tripod MG 20 m north of Alpha, Alpha defending it, one man seated:
// the gun counts as held and the hook lists it to BBB; vanilla's "a threat outside the arc, get
// out" (DismountTurretCondition) is overruled with no fresh enemy close, and stands with
// -acrHoldGuns 0; a gunner taken off the gun is replaced within ~8 s; the held gun's spare boxes are
// topped up from its reserve; fired dry with the reserve spent (the test empties it), its gunner gets
// off, stays near on foot and nobody climbs back on until it is refilled (#154, BBB Winchester); a
// fresh enemy close by lets him get out; the setup row; a squad that moves on holds nothing.
class ACR_TestGunHold
{
 static const int CHECKS = 9;
 static const ResourceName MG = "{887DB60486A18575}Prefabs/Weapons/Tripods/Tripod_M122_MG_M60.et";
 static int s_iStep;
 static int s_iStepAt;
 static IEntity s_Gun;
 static SCR_AIGroup s_Enemy;
 static ChimeraCharacter s_DryGunner;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor())
  {
   if (tick == 60) ACR_TestRegress.Abort("gunhold", "no Alpha squad");
   return;
  }
  if (tick > 260) ACR_TestRegress.Abort("gunhold", string.Format("timed out in step %1", s_iStep));
  if (tick < 8) return;
  int waited = tick - s_iStepAt;
  if (s_Enemy) HoldFire(s_Enemy);

  if (s_iStep == 0)
  {
   vector at = ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(0, 0, 20));
   s_Gun = ACR_Manager.SpawnPrefab(MG, at);
   alpha.Defend(at);
   PrintFormat("[ACR] GUNHOLD TEST gun at %1: %2", at, s_Gun != null);
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   if (waited < 3) return;
   if (!Gunner())
   {
    // A man next to the gun climbs in (vanilla Defend may already have seated one).
    array<AIAgent> agents = {};
    alpha.GetAgents(agents);
    foreach (AIAgent agent : agents)
    {
     ChimeraCharacter man = ChimeraCharacter.Cast(agent.GetControlledEntity());
     if (man && man != alpha.GetLeader() && !man.IsInVehicle() && ACR_Roadblocks.SeatSoldierInTurret(s_Gun, man)) break;
    }
   }
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   ChimeraCharacter gunner = Gunner();
   array<IEntity> listed = {};
   BBB.s_Hooks.GetHeldGuns(alpha.m_Group, listed);
   if ((!gunner || !listed.Contains(s_Gun)) && waited < 10) return;
   ACR_TestRegress.Check("gunhold: a static gun at the spot the squad holds counts as held, and BBB is told", gunner && ACR_GunHold.IsHeld(alpha, s_Gun) && listed.Contains(s_Gun), string.Format("gunner %1, order '%2', listed %3", gunner != null, alpha.m_Order, listed.Contains(s_Gun)));
   // Vanilla's dismount check for a point behind the gun (outside its arc), no enemy near.
   vector behind = s_Gun.GetOrigin() - s_Gun.GetTransformAxis(2) * 80;
   SCR_AICombatComponent combat = CombatOf(gunner);
   float priority;
   bool held = combat && !combat.DismountTurretCondition(behind, true, priority);
   ACR_Options.Set("acrHoldGuns", "0");
   bool stockOut = combat && combat.DismountTurretCondition(behind, true, priority);
   ACR_Options.Unset("acrHoldGuns");
   ACR_TestRegress.Check("gunhold: a threat outside the arc with no fresh enemy close doesn't get him out (vanilla would)", held && stockOut, string.Format("held %1, vanilla dismounts %2, kept %3", held, stockOut, BBB_HeldGuns.s_iKept));
   // He is taken off the gun (as vanilla would): someone mans it again.
   if (gunner && gunner.GetCompartmentAccessComponent()) gunner.GetCompartmentAccessComponent().GetOutVehicle(EGetOutType.TELEPORT, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   if (waited < 3 || (!Gunner() && waited < 30)) return;
   ACR_TestRegress.Check("gunhold: an empty held gun is manned again", Gunner() != null, string.Format("manned %1 after %2 s, re-seat orders %3", Gunner() != null, waited, BBB_HeldGuns.s_iRemounted));
   // One spare box gone: the reserve puts it back.
   TakeSpares(1);
   SetStep(10, tick);
   return;
  }

  if (s_iStep == 10)
  {
   int reserve = ACR_GunHold.Reserve(s_Gun);
   bool restocked = reserve == ACR_GunHold.RESERVE_BOXES - 1 && Spares() >= 1;
   if (!restocked && waited < 10) return;
   ACR_TestRegress.Check("gunhold: a held gun's spare boxes are topped up from its reserve", restocked && ACR_GunHold.s_iRestocked >= 1, string.Format("reserve %1 of %2, spares %3, boxes added %4", reserve, ACR_GunHold.RESERVE_BOXES, Spares(), ACR_GunHold.s_iRestocked));
   // Reserve spent, gun fired dry.
   ACR_GunHold.SetReserveForTest(s_Gun, 0);
   s_DryGunner = Gunner();
   Drain();
   SetStep(11, tick);
   return;
  }

  if (s_iStep == 11)
  {
   bool off = s_DryGunner && !s_DryGunner.IsInVehicle() && !Gunner();
   if (!off && waited < 20) return;
   float distance = -1;
   if (s_DryGunner) distance = vector.Distance(s_DryGunner.GetOrigin(), s_Gun.GetOrigin());
   ACR_TestRegress.Check("gunhold: a held gun out of ammo loses its gunner, who stays near on foot", off && ACR_Respawn.IsAlive(s_DryGunner) && distance < 40, string.Format("off %1 after %2 s, %3 m from the gun, ammo %4", off, waited, Math.Round(distance), BBB_VehicleHelpers.TurretHasAmmo(BBB_HeldGuns.GunSeat(s_Gun))));
   NextStep(tick);
   return;
  }

  if (s_iStep == 12)
  {
   if (waited < 14) return;
   ACR_TestRegress.Check("gunhold: nobody climbs back on the dry gun", !Gunner() && !ACR_Defenders.FreeTurret(s_Gun), string.Format("gunner %1 after %2 s, re-seat orders %3", Gunner() != null, waited, BBB_HeldGuns.s_iRemounted));
   // Ammo again: the gun is held and manned again before the next step.
   Refill();
   NextStep(tick);
   return;
  }

  if (s_iStep == 13)
  {
   if (!Gunner() && waited < 30) return;
   PrintFormat("[ACR] GUNHOLD TEST refilled gun manned %1 after %2 s", Gunner() != null, waited);
   // A fresh enemy close by: an enemy fire team 35 m in front of the gun (holding its fire).
   vector front = ACR_Manager.Ground(s_Gun.GetOrigin() + s_Gun.GetTransformAxis(2) * 35);
   string enemyKey = "USSR";
   if (alpha.GetFaction() && alpha.GetFaction().GetFactionKey() == "USSR") enemyKey = "US";
   s_Enemy = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, enemyKey), front));
   SetStep(4, tick);
   return;
  }

  if (s_iStep == 4)
  {
   ChimeraCharacter gunner = Gunner();
   bool fresh = gunner && BBB_HeldGuns.FreshClose(gunner);
   if (!fresh && waited < 30) return;
   bool released = fresh && !BBB_HeldGuns.Keep(gunner, s_Gun);
   ACR_TestRegress.Check("gunhold: a fresh enemy close by lets the gunner get out", released, string.Format("gunner %1, fresh close enemy %2 after %3 s", gunner != null, fresh, waited));
   KillGroup(h, s_Enemy);
   s_Enemy = null;
   // The setup row.
   string onRow = ACR_Setup.Rows();
   ACR_Options.Set("acrHoldGuns", "0");
   string offRow = ACR_Setup.Rows();
   ACR_Options.Unset("acrHoldGuns");
   ACR_TestRegress.Check("gunhold: a setup row, on by default", onRow.Contains("holdguns\tGunners stay on held guns: on") && offRow.Contains("holdguns\tGunners stay on held guns: off") && ACR_Setup.Option("holdguns") == "acrHoldGuns", "");
   // A squad that moves on holds nothing.
   alpha.MoveTo(alpha.GetAnchor().GetOrigin() + Vector(0, 0, -60));
   array<IEntity> after = {};
   BBB.s_Hooks.GetHeldGuns(alpha.m_Group, after);
   ACR_TestRegress.Check("gunhold: a squad that moves on holds no gun", !ACR_GunHold.IsHeld(alpha, s_Gun) && after.IsEmpty(), string.Format("order '%1', listed %2", alpha.m_Order, after.Count()));
   ACR_TestRegress.Complete("gunhold", CHECKS);
   s_iStep = 5;
  }
 }

 protected static InventoryStorageManagerComponent GunInventory(out BaseWeaponComponent weapon)
 {
  weapon = null;
  TurretControllerComponent controller = BBB_VehicleHelpers.TurretControllerOf(BBB_HeldGuns.GunSeat(s_Gun));
  if (!controller || !controller.GetWeaponManager()) return null;
  weapon = controller.GetWeaponManager().GetCurrentWeapon();
  return InventoryStorageManagerComponent.Cast(controller.GetOwner().FindComponent(InventoryStorageManagerComponent));
 }

 protected static int Spares()
 {
  BaseWeaponComponent weapon;
  InventoryStorageManagerComponent inventory = GunInventory(weapon);
  if (!inventory || !weapon) return -1;
  return inventory.GetMagazineCountByWeapon(weapon);
 }

 // Deletes up to 'count' stowed boxes (all with -1).
 protected static void TakeSpares(int count)
 {
  BaseWeaponComponent weapon;
  InventoryStorageManagerComponent inventory = GunInventory(weapon);
  if (!inventory) return;
  IEntity loaded;
  if (weapon && weapon.GetCurrentMagazine()) loaded = weapon.GetCurrentMagazine().GetOwner();
  array<IEntity> items = {};
  inventory.GetItems(items);
  foreach (IEntity item : items)
  {
   if (count == 0) return;
   if (item == loaded || !item.FindComponent(BaseMagazineComponent)) continue;
   if (!inventory.TryDeleteItem(item)) SCR_EntityHelper.DeleteEntityAndChildren(item);
   count--;
  }
 }

 protected static void Drain()
 {
  TakeSpares(-1);
  BaseWeaponComponent weapon;
  GunInventory(weapon);
  if (!weapon) return;
  if (weapon.GetCurrentMagazine()) weapon.GetCurrentMagazine().SetAmmoCount(0);
  array<BaseMuzzleComponent> muzzles = {};
  weapon.GetMuzzlesList(muzzles);
  foreach (BaseMuzzleComponent muzzle : muzzles)
   for (int b = 0; b < muzzle.GetBarrelsCount(); b++) muzzle.ClearChamber(b);
 }

 protected static void Refill()
 {
  BaseWeaponComponent weapon;
  GunInventory(weapon);
  if (weapon && weapon.GetCurrentMagazine()) weapon.GetCurrentMagazine().SetAmmoCount(weapon.GetCurrentMagazine().GetMaxAmmoCount());
 }

 protected static ChimeraCharacter Gunner()
 {
  BaseCompartmentSlot seat = BBB_HeldGuns.GunSeat(s_Gun);
  if (!seat) return null;
  return ChimeraCharacter.Cast(seat.GetOccupant());
 }

 protected static SCR_AICombatComponent CombatOf(IEntity soldier)
 {
  SCR_AIUtilityComponent utility = BBB_StaticWeapons.UtilityOf(soldier);
  if (!utility) return null;
  return utility.m_CombatComponent;
 }

 protected static void HoldFire(SCR_AIGroup group)
 {
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (utility && utility.GetCombatModeExternal() != EAIGroupCombatMode.HOLD_FIRE) utility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
 }

 protected static void KillGroup(ACR_TestHarness h, SCR_AIGroup group)
 {
  if (!group) return;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents) h.Kill(agent.GetControlledEntity());
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }

 protected static void SetStep(int step, int tick)
 {
  s_iStep = step;
  s_iStepAt = tick;
 }
}
