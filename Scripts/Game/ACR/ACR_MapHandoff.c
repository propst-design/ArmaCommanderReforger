// End the old soldier's physical map use without closing the RTS map window.
// Native map ModeClear skips local focus cleanup once possession has changed.
modded class SCR_MapGadgetComponent
{
 void ACR_EndMapUse()
 {
  m_bFocused = false;
  m_bIsMapOpen = false;
  GetGame().GetCallqueue().Remove(ToggleMapGadget);
  SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpen);
  SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);
 }
}
class ACR_MapHandoff
{
 static void ClearCharacterInput(IEntity entity)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  if (!character) return;
  CharacterControllerComponent controller = character.GetCharacterController();
  if (!controller) return;
  CharacterInputContext context = controller.GetInputContext();
  controller.SetMovement(0, vector.Zero);
  if (!context) return;
  context.SetMovement(0, vector.Zero);
  context.SetWeaponPullTrigger(false);
  context.SetAimChange(vector.Zero);
 }
 static void SetMenuControls(IEntity entity, bool disabled)
 {
  ChimeraCharacter body = ChimeraCharacter.Cast(entity);
  if (!body) return;
  CharacterControllerComponent controller = body.GetCharacterController();
  if (!controller) return;
  controller.SetDisableViewControls(disabled);
  controller.SetDisableWeaponControls(disabled);
  controller.SetDisableMovementControls(disabled);
 }
 static void Release(SCR_PlayerController pc, IEntity entity, bool releaseControls = true)
 {
  ChimeraCharacter body = ChimeraCharacter.Cast(entity);
  if (!body) return;
  CharacterControllerComponent controller = body.GetCharacterController();
  if (!controller) return;
  SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(body);
  if (gadgets)
  {
   IEntity mapItem = gadgets.GetGadgetByType(EGadgetType.MAP);
   if (mapItem)
   {
    SCR_MapGadgetComponent mapComponent = SCR_MapGadgetComponent.Cast(mapItem.FindComponent(SCR_MapGadgetComponent));
    if (mapComponent) mapComponent.ACR_EndMapUse();
    CharacterInputContext input = controller.GetInputContext();
    if (controller.GetAttachedGadgetAtLeftHandSlot() == mapItem || (input && input.GetWantedLefHandGadgetEntity() == mapItem))
     controller.RemoveGadgetFromHand(true);
    // Removal can schedule map callbacks through native gadget events.
    if (mapComponent) mapComponent.ACR_EndMapUse();
   }
  }
  if (releaseControls) SetMenuControls(body, false);
  if (pc) pc.SetGadgetFocus(false);
  controller.ResetPersistentStates();
  controller.SetForcedFreeLook(false);
  controller.SetFreeLook(false, false, false);
  CharacterInputContext context = controller.GetInputContext();
  if (context) context.SetFreelook(false);
  ClearCharacterInput(body);
  SanitizePossessedWeapons(pc, body, false);
 }
 static void SanitizePossessedWeapons(SCR_PlayerController pc, ChimeraCharacter target, bool possessing)
 {
  if (!target || !pc) return;
  CharacterControllerComponent controller = target.GetCharacterController();
  if (!controller) return;

  CharacterAnimationComponent animComp = CharacterAnimationComponent.Cast(target.FindComponent(CharacterAnimationComponent));
  if (animComp)
  {
   CharacterCommandHandlerComponent handler = animComp.GetCommandHandler();
   if (handler) handler.CancelItemUse();
  }

  BaseWeaponManagerComponent wpnMan = controller.GetWeaponManagerComponent();
  BaseWeaponComponent currentWpn = null;
  if (wpnMan) currentWpn = wpnMan.GetCurrentWeapon();

  // If caught mid-reload, item switch, or weapon action, reset weapon into hands without animations
  if (possessing)
  {
   if (controller.IsReloading() || controller.IsChangingItem() || controller.IsUsingItem() || (currentWpn && !controller.CanFire()))
   {
    if (currentWpn && currentWpn.GetOwner())
    {
     controller.TryEquipRightHandItem(null, EEquipItemType.EEquipTypeUnarmedContextual, true);
     controller.TryEquipRightHandItem(currentWpn.GetOwner(), EEquipItemType.EEquipTypeWeapon, true);
    }
   }
  }

  // Ensure weapon and magazine replication ownership matches the controlling connection
  RplIdentity identity;
  if (possessing) identity = pc.GetRplIdentity();
  else identity = RplIdentity.Local();

  array<IEntity> weapons = {};
  if (wpnMan) wpnMan.GetWeaponsList(weapons);
  foreach (IEntity wpnEnt : weapons)
  {
   if (!wpnEnt) continue;
   RplComponent wpnRpl = RplComponent.Cast(wpnEnt.FindComponent(RplComponent));
   if (wpnRpl) wpnRpl.GiveExt(identity, false);

   BaseWeaponComponent bwc = BaseWeaponComponent.Cast(wpnEnt.FindComponent(BaseWeaponComponent));
   if (bwc)
   {
    BaseMagazineComponent mag = bwc.GetCurrentMagazine();
    if (mag && mag.GetOwner())
    {
     RplComponent magRpl = RplComponent.Cast(mag.GetOwner().FindComponent(RplComponent));
     if (magRpl) magRpl.GiveExt(identity, false);
    }
   }
  }
 }
}

