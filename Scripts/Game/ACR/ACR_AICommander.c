enum ACR_EAITask
{
 FREE,      // available for orders
 DEFEND,    // holding a threatened base
 ATTACK,    // part of a base attack
 POSTED,    // idle, moved up to a frontline base
 RESUPPLY,  // on its way to an ammo / repair / fuel / medical vehicle
 BUILD_FOB, // establishing a forward base
 ROADBLOCK, // establishing or holding a forward roadblock
 MINES,     // laying a minefield on an approach (-acrMinesAI)
 FORTIFY,   // digging in or upgrading a position on an approach (-acrFortifyAI)
 SWEEP      // clearing a road where FIA ambushed the side (ACR_Insurgency, #89)
}

enum ACR_ESapperDemoState
{
 NONE,
 APPROACHING,
 PLANTING,
 FALLBACK,
 DETONATED,
 WIRING,    // charge on the ground, being wired/armed
 FAILED     // the sapper fell after wiring: the charge stays until disarmed
}

// Combat AI sapper demolition of an enemy FOB HQ (Jacob, 18 Oct). A sapper plants only once no
// defender is within 30 m of the HQ. One soldier crouches, sets a vanilla charge (M112/TSh-400)
// by the HQ with the game's place animation, wires it to his blasting machine (timed fuze if he
// has none), the attackers pull back, and at the end of the fuse window he fires it from the
// detonator. Defenders defuse in that window: players with the game's Disarm action, AI by
// standing at the charge for 5 s. The blast destroys the HQ only (ACR_Demolition).
class ACR_SapperDemo : Managed
{
 static const float PLANT_RADIUS = 9.5;    // from the HQ centre: its tents and sandbags reach ~8 m
 static const float CLEAR_RADIUS = 30.0;      // no living defender this close to the HQ
 static const float DEFUSE_RADIUS = 3.0;
 static const float DEFUSE_MS = 5000.0;
 static const float RETREAT_DISTANCE = 55.0;
 static const float RETRY_MS = 30000.0;       // after a defusal
 static const float KIT_MS = 2000.0;          // charge/detonator appear in the inventory
 static const float PLACE_MS = 6000.0;        // place animation (or give up waiting for it)
 static const float SETTLE_MS = 1500.0;       // placed charge leaves the inventory
 static const float WIRE_MS = 5000.0;         // arming animation
 static const float RAISE_MS = 1500.0;        // blasting machine to hand
 static const float FIRE_MS = 4000.0;         // detonator animation before the scripted fallback
 static float s_fFuseMs = 45000.0;            // the defenders' window (tests shorten it)
 static const float SAFE_DISTANCE = 35.0;     // his squad is this far from the charge when he fires (the HQ cooks off)
 static const float SAFE_WAIT_MS = 15000.0;   // ... waiting at most this long past the window

 // Kept for old callers/tests: the charge prefabs (full prefabs with fuzes).
 static const ResourceName CHARGE_WEST = "{33CBDE73AB48172A}Prefabs/Weapons/Explosives/DemoBlock_M112/DemoBlock_M112.et";
 static const ResourceName CHARGE_EAST = "{97064F8597F2D7BF}Prefabs/Weapons/Explosives/DemoBlock_TSh400g/DemoBlock_TSh400g.et";

 ACR_Manager m_Manager;
 ACR_AICommander m_Commander;
 ACR_AIAttack m_Attack;
 ACR_ESapperDemoState m_State = ACR_ESapperDemoState.NONE;
 float m_fPlantStart;
 float m_fFuseDeadline;
 IEntity m_DemoCharge;
 ACR_Squad m_SapperSquad;
 vector m_vChargePos;
 vector m_vFallbackPos;
 bool m_bDetonated;
 bool m_bDefused;
 string m_sTargetOwner;
 float m_fDefuseProgress;
 float m_fLastTick;
 float m_fRetryAt;
 bool m_bDefendersCalled;

 IEntity m_Planter;             // the soldier doing the work
 IEntity m_Detonator;           // his blasting machine (null: timed fuze)
 int m_iStep;                   // planting sub-step
 float m_fStepStart;
 bool m_bAnimDone;
 bool m_bAnimStarted;
 bool m_bTimedFuze;
 bool m_bFired;                 // we set it off (detonator or fallback)
 float m_fFiredAt;
 bool m_bRaised;
 float m_fRaisedAt;
 int m_iFireTry;
 string m_sFiredBy = "";        // "detonator" | "script" | "timer"
 string m_sPlacedBy = "";       // "animation" | "timeout" | "spawned"

 void ACR_SapperDemo(ACR_Manager manager, ACR_AICommander commander, ACR_AIAttack attack)
 {
  m_Manager = manager;
  m_Commander = commander;
  m_Attack = attack;
  m_State = ACR_ESapperDemoState.NONE;
 }

 ACR_ESapperDemoState State() { return m_State; }
 void SetStateForTest(ACR_ESapperDemoState state) { m_State = state; }
 IEntity DemoCharge() { return m_DemoCharge; }
 bool IsDetonated() { return m_bDetonated; }
 bool IsDefused() { return m_bDefused; }
 ACR_Squad SapperSquad() { return m_SapperSquad; }
 vector FallbackPos() { return m_vFallbackPos; }
 vector ChargePos() { return m_vChargePos; }
 string TargetOwner() { return m_sTargetOwner; }
 float DefuseProgress() { return m_fDefuseProgress; }
 void SetPlantStartForTest(float time) { m_fPlantStart = time; }
 void SetFuseDeadlineForTest(float time) { m_fFuseDeadline = time; }
 bool DefendersCalled() { return m_bDefendersCalled; }
 void MarkDefendersCalled() { m_bDefendersCalled = true; }
 void SetChargeForTest(IEntity charge) { m_DemoCharge = charge; if (charge) m_vChargePos = charge.GetOrigin(); }
 void SetTargetOwnerForTest(string owner) { m_sTargetOwner = owner; }
 void SetTimedForTest(bool timed) { m_bTimedFuze = timed; }
 IEntity Planter() { return m_Planter; }
 IEntity Detonator() { return m_Detonator; }
 bool UsesTimedFuze() { return m_bTimedFuze; }
 string FiredBy() { return m_sFiredBy; }
 string PlacedBy() { return m_sPlacedBy; }
 bool PlantAnimationStarted() { return m_bAnimStarted; }
 bool InProgress() { return m_State == ACR_ESapperDemoState.PLANTING || m_State == ACR_ESapperDemoState.WIRING || m_State == ACR_ESapperDemoState.FALLBACK; }

 static vector PositionOf(ACR_Squad squad)
 {
  if (!squad) return vector.Zero;
  IEntity anchor = squad.GetAnchor();
  if (anchor) return anchor.GetOrigin();
  return vector.Zero;
 }

 static bool IsSapperSquad(ACR_Squad squad)
 {
  if (!squad || squad.CountAlive() <= 0) return false;
  if (squad.m_Group && squad.m_Group.m_eACRFormation != ACR_EFormation.NONE) return squad.m_Group.m_eACRFormation == ACR_EFormation.ENGINEER;
  string name = squad.GetName();
  name.ToLower();
  if (name.Contains("sapper") || name.Contains("engineer") || name.Contains("demolition") || name.Contains("demo") || name.Contains("pioneer"))
   return true;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   IEntity entity = agent.GetControlledEntity();
   if (!entity) continue;
   if (IsSapperEntity(entity)) return true;
  }
  return false;
 }

 static bool IsSapperEntity(IEntity entity)
 {
  if (!entity) return false;
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  if (character && character.GetCharacterController() && character.GetCharacterController().IsDead()) return false;
  EntityPrefabData data = entity.GetPrefabData();
  if (data)
  {
   string pName = data.GetPrefabName();
   pName.ToLower();
   if (pName.Contains("sapper") || pName.Contains("engineer") || pName.Contains("demolition") || pName.Contains("demo") || pName.Contains("pioneer") || pName.Contains("miner"))
    return true;
  }
  return false;
 }

 // Living soldiers of a side (AI or players) within a radius.
 protected static string s_sCountFaction;
 protected static int s_iCounted;
 static int SoldiersNear(vector position, string faction, float radius)
 {
  if (faction.IsEmpty()) return 0;
  s_sCountFaction = faction;
  s_iCounted = 0;
  GetGame().GetWorld().QueryEntitiesBySphere(position, radius, CountSoldier, null, EQueryEntitiesFlags.DYNAMIC);
  return s_iCounted;
 }

 protected static bool CountSoldier(IEntity e)
 {
  if (!ChimeraCharacter.Cast(e) || !ACR_Respawn.IsAlive(e)) return true;
  FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(e.FindComponent(FactionAffiliationComponent));
  if (aff && aff.GetAffiliatedFaction() && aff.GetAffiliatedFaction().GetFactionKey() == s_sCountFaction) s_iCounted++;
  return true;
 }

 ACR_Squad FindSapperSquad()
 {
  if (!m_Attack || m_Attack.m_Squads.IsEmpty()) return null;
  foreach (ACR_Squad squad : m_Attack.m_Squads)
  {
   if (squad && squad.CountAlive() > 0 && IsSapperSquad(squad)) return squad;
  }
  foreach (ACR_Squad squad : m_Attack.m_Squads)
  {
   if (squad && squad.CountAlive() > 0 && squad.IsCombatUnit()) return squad;
  }
  if (m_Attack.m_Squads.Count() > 0) return m_Attack.m_Squads[0];
  return null;
 }

 IEntity TargetHqEntity()
 {
  if (!m_Attack || !m_Attack.m_Target || !m_Manager || !m_Manager.m_Structures) return null;
  return m_Manager.m_Structures.HqEntityOf(m_Attack.m_Target);
 }

 protected string AttackerFaction()
 {
  if (m_Commander) return m_Commander.Faction();
  if (m_SapperSquad && m_SapperSquad.GetFaction()) return m_SapperSquad.GetFaction().GetFactionKey();
  return "";
 }

 // ---------------------------------------------------------------- the soldier and his kit

 static bool IsPlayerBody(IEntity body)
 {
  return body && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(body) != 0;
 }

 // The squad's living AI soldier nearest the spot.
 static IEntity PlanterFor(ACR_Squad squad, vector spot)
 {
  if (!squad) return null;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  IEntity best;
  float bestDist = float.MAX;
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || IsPlayerBody(body)) continue;
   float d = vector.DistanceXZ(body.GetOrigin(), spot);
   if (d < bestDist)
   {
    bestDist = d;
    best = body;
   }
  }
  return best;
 }

 static SCR_InventoryStorageManagerComponent InventoryOf(IEntity body)
 {
  if (!body) return null;
  return SCR_InventoryStorageManagerComponent.Cast(body.FindComponent(SCR_InventoryStorageManagerComponent));
 }

 static IEntity FindKit(IEntity body, typename component)
 {
  SCR_InventoryStorageManagerComponent inventory = InventoryOf(body);
  if (!inventory) return null;
  array<typename> query = {component};
  return inventory.FindItemWithComponents(query, EStoragePurpose.PURPOSE_ANY);
 }

 // Charge and blasting machine from his kit, else handed to him (most squads carry none).
 protected void EnsureKit()
 {
  SCR_InventoryStorageManagerComponent inventory = InventoryOf(m_Planter);
  if (!inventory) return;
  string faction = AttackerFaction();
  if (!FindKit(m_Planter, SCR_ExplosiveChargeComponent)) inventory.TrySpawnPrefabToStorage(ACR_Demolition.ChargeFor(faction));
  if (!FindKit(m_Planter, SCR_DetonatorGadgetComponent)) inventory.TrySpawnPrefabToStorage(ACR_Demolition.DetonatorFor(faction));
 }

 // The game's own item animation (CMD_Item_Action): 1 = place an item, 3 = work on a placed one.
 protected bool PlayItemAnimation(IEntity item, int kind, vector at)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(m_Planter);
  if (!character || !item) return false;
  SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
  if (!controller || controller.IsUsingItem()) return false;
  if (controller.GetStance() == ECharacterStance.STAND) controller.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOCROUCH);
  CharacterAnimationComponent animation = character.GetAnimationComponent();
  if (!animation) return false;
  vector mat[4];
  character.GetWorldTransform(mat);
  vector facing = at - mat[3];
  facing[1] = 0;
  if (facing.LengthSq() > 0.01)
  {
   facing.Normalize();
   mat[2] = facing;
   mat[1] = vector.Up;
   mat[0] = Vector(facing[2], 0, -facing[0]);
  }
  PointInfo point = new PointInfo();
  point.Set(null, "", mat);
  ItemUseParameters params = new ItemUseParameters();
  params.SetEntity(item);
  params.SetAllowMovementDuringAction(false);
  params.SetKeepInHandAfterSuccess(kind != 1);
  params.SetCommandID(animation.BindCommand("CMD_Item_Action"));
  params.SetCommandIntArg(kind);
  params.SetMaxAnimLength(10.0);
  params.SetAlignmentPoint(point);
  m_bAnimDone = false;
  controller.m_OnItemUseEndedInvoker.Insert(OnAnimationEnded);
  bool started = controller.TryUseItemOverrideParams(params);
  if (!started) controller.m_OnItemUseEndedInvoker.Remove(OnAnimationEnded);
  return started;
 }

 // End any item animation still running (an arming animation can hold the item use open).
 protected void FinishItemUse(bool standUp)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(m_Planter);
  if (!character) return;
  SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
  if (!controller) return;
  controller.m_OnItemUseEndedInvoker.Remove(OnAnimationEnded);
  if (controller.IsUsingItem())
  {
   CharacterAnimationComponent animation = character.GetAnimationComponent();
   if (animation && animation.GetCommandHandler()) animation.GetCommandHandler().CancelItemUse();
  }
  if (standUp && controller.GetStance() != ECharacterStance.STAND) controller.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOERECTED);
 }

 protected void OnAnimationEnded(IEntity item, bool successful, ItemUseParameters animParams)
 {
  m_bAnimDone = true;
  ChimeraCharacter character = ChimeraCharacter.Cast(m_Planter);
  if (!character) return;
  SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
  if (controller) controller.m_OnItemUseEndedInvoker.Remove(OnAnimationEnded);
 }

 // Out of his pockets onto the ground at the spot (the vanilla placement path), else spawned there.
 protected void PlaceCharge()
 {
  IEntity charge = FindKit(m_Planter, SCR_ExplosiveChargeComponent);
  vector spot = ACR_Manager.Ground(m_vChargePos);
  m_vChargePos = spot;
  if (charge)
  {
   SCR_PlaceableInventoryItemComponent placeable = SCR_PlaceableInventoryItemComponent.Cast(charge.FindComponent(SCR_PlaceableInventoryItemComponent));
   SCR_InventoryStorageManagerComponent inventory = InventoryOf(m_Planter);
   RplComponent planterRpl = RplComponent.Cast(m_Planter.FindComponent(RplComponent));
   InventoryItemComponent item = InventoryItemComponent.Cast(charge.FindComponent(InventoryItemComponent));
   if (placeable && inventory && planterRpl && item && item.GetParentSlot())
   {
    vector forward = spot - m_Planter.GetOrigin();
    forward[1] = 0;
    if (forward.LengthSq() < 0.01) forward = "0 0 1";
    forward.Normalize();
    vector right = Vector(forward[2], 0, -forward[0]);
    placeable.SetPlacementPosition(right, vector.Up, forward, spot, planterRpl.Id());
    if (inventory.TryRemoveItemFromStorage(charge, item.GetParentSlot().GetStorage()))
    {
     m_DemoCharge = charge; // PlaceItem once it has left the inventory (TickPlanting)
     return;
    }
   }
  }
  m_DemoCharge = ACR_Manager.SpawnPrefab(ACR_Demolition.ChargeFor(AttackerFaction()), spot);
  m_sPlacedBy = "spawned";
 }

 static SCR_ExplosiveChargeComponent ChargeComponent(IEntity charge)
 {
  if (!charge) return null;
  return SCR_ExplosiveChargeComponent.Cast(charge.FindComponent(SCR_ExplosiveChargeComponent));
 }

 // Wire it to his blasting machine (as the game's Arm action does), else a timed fuze.
 protected void ArmCharge(float now)
 {
  SCR_ExplosiveChargeComponent chargeComp = ChargeComponent(m_DemoCharge);
  if (!chargeComp) return;
  FinishItemUse(true);
  m_Detonator = FindKit(m_Planter, SCR_DetonatorGadgetComponent);
  SCR_DetonatorGadgetComponent detonator;
  if (m_Detonator) detonator = SCR_DetonatorGadgetComponent.Cast(m_Detonator.FindComponent(SCR_DetonatorGadgetComponent));
  RplId detonatorId = RplId.Invalid();
  if (detonator) detonatorId = Replication.FindItemId(detonator);
  if (detonator && detonatorId.IsValid() && chargeComp.GetRplId().IsValid())
  {
   detonator.ConnectNewCharge(chargeComp.GetRplId());
   if (chargeComp.GetTrigger()) chargeComp.GetTrigger().SetUser(m_Planter);
   chargeComp.ConnectDetonator(SCR_EFuzeType.REMOTE, detonatorId);
   m_bTimedFuze = false;
  }
  else
  {
   m_Detonator = null;
   if (chargeComp.GetTrigger() && m_Planter) chargeComp.GetTrigger().SetUser(m_Planter);
   chargeComp.SetFuzeTime(s_fFuseMs / 1000, true);
   chargeComp.ArmWithTimedFuze();
   m_bTimedFuze = true;
  }
  m_State = ACR_ESapperDemoState.FALLBACK;
  m_fFuseDeadline = now + s_fFuseMs;
  m_fDefuseProgress = 0;
  m_bDefendersCalled = false;
  m_bFired = false;
  m_bRaised = false;
  if (m_Manager)
  {
   m_Manager.RegisterDemolition(m_Attack);
   m_Manager.NotifyFaction(m_sTargetOwner, string.Format("Demolition charge on the %1 HQ: disarm it, or stand at it 5 s (%2 s)", m_Attack.m_Target.m_sName, (int)(s_fFuseMs / 1000)));
  }
  string how = "wired to a detonator";
  if (m_bTimedFuze) how = "timed fuze";
  PrintFormat("[ACR] SAPPER %1: charge set on enemy FOB %2 HQ (%3, placed by %4), falling back %5 m", m_SapperSquad.GetName(), m_Attack.m_Target.m_sName, how, m_sPlacedBy, RETREAT_DISTANCE);
  vector hqPos = m_vChargePos;
  IEntity hq = TargetHqEntity();
  if (hq) hqPos = hq.GetOrigin();
  vector dir = m_Planter.GetOrigin() - hqPos;
  dir[1] = 0;
  if (dir.LengthSq() < 0.1) dir = Vector(1, 0, 0);
  else dir.Normalize();
  m_vFallbackPos = ACR_Manager.Ground(hqPos + dir * RETREAT_DISTANCE);
  foreach (ACR_Squad squad : m_Attack.m_Squads)
  {
   if (!squad || squad.CountAlive() <= 0) continue;
   squad.SetOrder("Clearing blast radius", ACR_EOrderState.MOVE);
   squad.SetMode(ACR_EMoveMode.FAST);
   squad.MoveTo(m_vFallbackPos);
  }
 }

 protected void AbortPlanting(string why, vector hqPos)
 {
  PrintFormat("[ACR] SAPPER %1: planting at %2 interrupted (%3)", m_SapperSquad.GetName(), m_Attack.m_Target.m_sName, why);
  // A charge already on the ground but not armed stays where it lies.
  m_State = ACR_ESapperDemoState.APPROACHING;
  m_iStep = 0;
  m_Planter = null;
  m_SapperSquad.Assault(hqPos);
 }

 // ---------------------------------------------------------------- tick

 void Tick(float now)
 {
  float elapsed = 0;
  if (m_fLastTick > 0) elapsed = Math.Max(0, now - m_fLastTick);
  m_fLastTick = now;
  if (m_State == ACR_ESapperDemoState.DETONATED || m_State == ACR_ESapperDemoState.FAILED) return;
  if (!m_Attack || !m_Attack.m_Target || m_Attack.m_Target.m_bRetired)
  {
   if (!m_bFired) RemoveCharge();
   return;
  }

  if (m_State == ACR_ESapperDemoState.FALLBACK)
  {
   TickFuse(now, elapsed);
   return;
  }

  if (m_Commander && m_Attack.m_Target.m_sOwner == m_Commander.Faction()) return;
  // No HQ (already destroyed): the raid becomes an ordinary capture.
  IEntity hq = TargetHqEntity();
  if (!hq || now < m_fRetryAt) return;
  vector hqPos = hq.GetOrigin();
  string owner = m_Attack.m_Target.m_sOwner;

  if (!m_SapperSquad || m_SapperSquad.CountAlive() <= 0)
  {
   m_SapperSquad = FindSapperSquad();
   if (!m_SapperSquad) return;
  }

  int defenders = SoldiersNear(hqPos, owner, CLEAR_RADIUS);

  if (m_State == ACR_ESapperDemoState.NONE)
  {
   m_State = ACR_ESapperDemoState.APPROACHING;
   m_SapperSquad.SetMode(ACR_EMoveMode.COMBAT);
   m_SapperSquad.Assault(hqPos);
   PrintFormat("[ACR] SAPPER %1: advancing to demolish enemy FOB %2 HQ at %3", m_SapperSquad.GetName(), m_Attack.m_Target.m_sName, hqPos);
  }

  if (m_State == ACR_ESapperDemoState.APPROACHING)
  {
   // Clear the defenders first; only then walk up to the HQ and plant.
   if (defenders > 0) return;
   IEntity planter = PlanterFor(m_SapperSquad, hqPos);
   if (!planter) return;
   float planterDist = vector.DistanceXZ(planter.GetOrigin(), hqPos);
   if (planterDist > PLANT_RADIUS)
   {
    if (vector.DistanceXZ(PositionOf(m_SapperSquad), hqPos) <= CLEAR_RADIUS) m_SapperSquad.MoveTo(hqPos);
    return;
   }
   m_State = ACR_ESapperDemoState.PLANTING;
   m_Planter = planter;
   m_fPlantStart = now;
   m_fStepStart = now;
   m_iStep = 0;
   m_bAnimStarted = false;
   m_sPlacedBy = "";
   m_sTargetOwner = owner;
   // The spot: a metre in front of him, towards the HQ.
   vector toward = hqPos - planter.GetOrigin();
   toward[1] = 0;
   if (toward.LengthSq() > 1) toward.Normalize();
   m_vChargePos = ACR_Manager.Ground(planter.GetOrigin() + toward);
   m_SapperSquad.ClearOrders();
   m_SapperSquad.SetOrder("Planting demolition charges", ACR_EOrderState.HOLD);
   EnsureKit();
   if (m_Manager) m_Manager.RegisterDemolition(m_Attack);
   PrintFormat("[ACR] SAPPER %1: planting demolition charges on enemy FOB %2 HQ (dist=%3m)", m_SapperSquad.GetName(), m_Attack.m_Target.m_sName, planterDist);
   return;
  }

  if (m_State == ACR_ESapperDemoState.PLANTING || m_State == ACR_ESapperDemoState.WIRING)
  {
   if (defenders > 0)
   {
    AbortPlanting("defenders", hqPos);
    return;
   }
   if (!m_Planter || !ACR_Respawn.IsAlive(m_Planter))
   {
    string fell = "the sapper fell";
    if (!m_Planter) fell = "the sapper is gone";
    AbortPlanting(fell, hqPos);
    return;
   }
   TickPlanting(now);
  }
 }

 // Kit -> place animation -> charge on the ground -> arming animation -> armed.
 protected void TickPlanting(float now)
 {
  float stepTime = now - m_fStepStart;
  if (m_iStep == 0)
  {
   IEntity kitCharge = FindKit(m_Planter, SCR_ExplosiveChargeComponent);
   if (!kitCharge && stepTime < KIT_MS) return;
   m_bAnimStarted = PlayItemAnimation(kitCharge, 1, m_vChargePos);
   m_iStep = 1;
   m_fStepStart = now;
   return;
  }
  if (m_iStep == 1)
  {
   if (!m_bAnimDone && stepTime < PLACE_MS) return;
   if (m_bAnimDone) m_sPlacedBy = "animation";
   else m_sPlacedBy = "timeout";
   PlaceCharge();
   m_iStep = 2;
   m_fStepStart = now;
   return;
  }
  if (m_iStep == 2)
  {
   // The charge leaves his inventory a moment after the request.
   InventoryItemComponent item;
   if (m_DemoCharge) item = InventoryItemComponent.Cast(m_DemoCharge.FindComponent(InventoryItemComponent));
   bool onGround = m_DemoCharge && (!item || !item.GetParentSlot()) && !m_DemoCharge.GetParent();
   if (!onGround && stepTime < SETTLE_MS) return;
   if (!onGround)
   {
    if (m_DemoCharge) SCR_EntityHelper.DeleteEntityAndChildren(m_DemoCharge);
    m_DemoCharge = ACR_Manager.SpawnPrefab(ACR_Demolition.ChargeFor(AttackerFaction()), m_vChargePos);
    m_sPlacedBy = "spawned";
   }
   else if (m_sPlacedBy != "spawned")
   {
    SCR_PlaceableInventoryItemComponent placed = SCR_PlaceableInventoryItemComponent.Cast(m_DemoCharge.FindComponent(SCR_PlaceableInventoryItemComponent));
    if (placed) placed.PlaceItem();
   }
   m_State = ACR_ESapperDemoState.WIRING;
   PlayItemAnimation(m_DemoCharge, 3, m_vChargePos);
   m_iStep = 3;
   m_fStepStart = now;
   return;
  }
  if (m_iStep == 3)
  {
   if (!m_bAnimDone && stepTime < WIRE_MS) return;
   ArmCharge(now);
  }
 }

 // Armed: defenders may defuse; at the deadline he fires it (a timed fuze fires itself).
 void TickFuse(float now, float elapsed)
 {
  if (m_State != ACR_ESapperDemoState.FALLBACK) return;
  if (!m_DemoCharge)
  {
   if (m_bFired || m_bTimedFuze) MarkDetonated();
   else Defuse("the charge was removed");
   return;
  }
  SCR_ExplosiveChargeComponent chargeComp = ChargeComponent(m_DemoCharge);
  if (chargeComp && chargeComp.GetUsedFuzeType() == SCR_EFuzeType.NONE && !m_bFired)
  {
   Defuse("disarmed");
   return;
  }
  vector chargePos = m_DemoCharge.GetOrigin();
  if (SoldiersNear(chargePos, m_sTargetOwner, DEFUSE_RADIUS) > 0)
  {
   m_fDefuseProgress += Math.Min(elapsed, 2000);
   if (m_fDefuseProgress >= DEFUSE_MS)
   {
    if (chargeComp) chargeComp.DisarmCharge();
    Defuse("defused by defenders");
    return;
   }
  }
  else m_fDefuseProgress = 0;
  if (now < m_fFuseDeadline) return;
  if (!chargeComp)
  {
   Detonate();
   return;
  }
  if (m_bTimedFuze)
  {
   // The game's own timer sets it off; make sure it does.
   if (now >= m_fFuseDeadline + FIRE_MS) Detonate();
   return;
  }
  Fire(now);
 }

 // The blasting machine: in hand, then the game's detonator action; scripted only if that fails.
 protected void Fire(float now)
 {
  if (m_bFired)
  {
   // The game's detonator action first; its own animation path next; script last.
   if (m_iFireTry == 1 && now - m_fFiredAt >= RAISE_MS)
   {
    m_iFireTry = 2;
    SCR_DetonatorGadgetComponent again = SCR_DetonatorGadgetComponent.Cast(m_Detonator.FindComponent(SCR_DetonatorGadgetComponent));
    if (again)
    {
     PrintFormat("[ACR] SAPPER: detonator action did not fire (%1), using its animation directly", DetonatorState());
     again.OnToggleActive(false);
    }
    return;
   }
   if (now - m_fFiredAt >= FIRE_MS + RAISE_MS)
   {
    PrintFormat("[ACR] SAPPER: detonator did not fire (%1), detonating by script", DetonatorState());
    Detonate();
   }
   return;
  }
  if (!m_Planter || !ACR_Respawn.IsAlive(m_Planter) || !m_Detonator)
  {
   PrintFormat("[ACR] SAPPER: nobody left to fire the charge at %1 (sapper %2, alive %3, detonator %4); it stays wired", m_Attack.m_Target.m_sName, m_Planter != null, ACR_Respawn.IsAlive(m_Planter), m_Detonator != null);
   m_State = ACR_ESapperDemoState.FAILED;
   return;
  }
  SCR_DetonatorGadgetComponent detonator = SCR_DetonatorGadgetComponent.Cast(m_Detonator.FindComponent(SCR_DetonatorGadgetComponent));
  if (!detonator || !detonator.IsChargeInRange(m_Detonator.GetOrigin(), ChargeComponent(m_DemoCharge)))
  {
   PrintFormat("[ACR] SAPPER: charge at %1 out of detonator range; it stays wired", m_Attack.m_Target.m_sName);
   m_State = ACR_ESapperDemoState.FAILED;
   return;
  }
  // He fires once he and his squadmates who can move are SAFE_DISTANCE out, unless that takes
  // more than 15 s past the window.
  if (!m_bRaised && SquadInDanger(m_DemoCharge.GetOrigin()) && now < m_fFuseDeadline + SAFE_WAIT_MS) return;
  if (!m_bRaised)
  {
   m_bRaised = true;
   m_fRaisedAt = now;
   PrintFormat("[ACR] SAPPER %1: firing from %2 m (squad clear: %3)", m_Attack.m_Target.m_sName, (int)vector.DistanceXZ(m_Planter.GetOrigin(), m_DemoCharge.GetOrigin()), !SquadInDanger(m_DemoCharge.GetOrigin()));
   // He stops, takes the blasting machine in hand, then squeezes it.
   if (m_SapperSquad) m_SapperSquad.Halt("Firing the charge");
   FinishItemUse(false);
   SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(m_Planter);
   if (gadgets) gadgets.SetGadgetMode(m_Detonator, EGadgetMode.IN_HAND);
   return;
  }
  if (now - m_fRaisedAt < RAISE_MS) return;
  m_bFired = true;
  m_fFiredAt = now;
  m_iFireTry = 1;
  m_sFiredBy = "detonator";
  detonator.ActivateAction(m_Planter);
 }

 // Is the planter, or a conscious soldier of his squad, still within SAFE_DISTANCE of the charge?
 protected bool SquadInDanger(vector charge)
 {
  if (m_Planter && vector.DistanceXZ(m_Planter.GetOrigin(), charge) < SAFE_DISTANCE) return true;
  if (!m_SapperSquad) return false;
  array<AIAgent> agents = {};
  m_SapperSquad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   if (!agent) continue;
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !ACR_Respawn.IsAlive(body) || IsPlayerBody(body)) continue;
   CharacterControllerComponent controller = body.GetCharacterController();
   if (controller && controller.IsUnconscious()) continue;
   if (vector.DistanceXZ(body.GetOrigin(), charge) < SAFE_DISTANCE) return true;
  }
  return false;
 }

 protected string DetonatorState()
 {
  string text = "no detonator";
  if (!m_Detonator) return text;
  SCR_GadgetComponent gadget = SCR_GadgetComponent.Cast(m_Detonator.FindComponent(SCR_GadgetComponent));
  ChimeraCharacter character = ChimeraCharacter.Cast(m_Planter);
  SCR_CharacterControllerComponent controller;
  if (character) controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
  int mode = -1;
  if (gadget) mode = gadget.GetMode();
  bool gesture = controller && controller.CanPlayItemGesture();
  bool usingItem = controller && controller.IsUsingItem();
  return string.Format("mode %1, gesture %2, using item %3", mode, gesture, usingItem);
 }

 void Defuse(string why)
 {
  if (m_State != ACR_ESapperDemoState.FALLBACK) return;
  RemoveCharge();
  m_bDefused = true;
  m_State = ACR_ESapperDemoState.NONE;
  m_fDefuseProgress = 0;
  m_fRetryAt = m_fLastTick + RETRY_MS;
  string name = "FOB";
  if (m_Attack && m_Attack.m_Target) name = m_Attack.m_Target.m_sName;
  PrintFormat("[ACR] SAPPER: charge at %1 neutralised (%2)", name, why);
  if (m_Manager) m_Manager.NotifyFaction(m_sTargetOwner, string.Format("Demolition charge at %1 defused", name));
  if (m_Manager && m_Commander) m_Manager.NotifyFaction(m_Commander.Faction(), string.Format("Our charge at %1 was defused", name));
 }

 protected void RemoveCharge()
 {
  if (m_DemoCharge) SCR_EntityHelper.DeleteEntityAndChildren(m_DemoCharge);
  m_DemoCharge = null;
 }

 protected void MarkDetonated()
 {
  if (m_bDetonated) return;
  m_bDetonated = true;
  m_State = ACR_ESapperDemoState.DETONATED;
  if (m_sFiredBy.IsEmpty() && m_bTimedFuze) m_sFiredBy = "timer";
  string name = "FOB";
  if (m_Attack && m_Attack.m_Target) name = m_Attack.m_Target.m_sName;
  PrintFormat("[ACR] SAPPER: charge at %1 went off (%2)", name, m_sFiredBy);
 }

 // Scripted detonation through the charge's own trigger (the same explosion and HQ rule).
 void Detonate()
 {
  if (m_bDetonated) return;
  SCR_ExplosiveChargeComponent chargeComp = ChargeComponent(m_DemoCharge);
  if (chargeComp && chargeComp.GetTrigger())
  {
   if (m_sFiredBy.IsEmpty() || m_sFiredBy == "detonator") m_sFiredBy = "script";
   if (m_Planter) chargeComp.GetTrigger().SetUser(m_Planter);
   chargeComp.GetTrigger().UseTrigger();
  }
  else
  {
   // No vanilla charge (old saves/tests): a scripted blast, same HQ rule.
   vector blastPos = m_vChargePos;
   if (m_DemoCharge) blastPos = m_DemoCharge.GetOrigin();
   if (m_Manager && m_Manager.m_Supports) m_Manager.m_Supports.ExplodeAt(blastPos);
   RemoveCharge();
   ACR_Demolition.OnChargeDetonated(blastPos);
   if (m_sFiredBy.IsEmpty()) m_sFiredBy = "script";
  }
  MarkDetonated();
 }
}

// One base attack: groups gather at a staging point, then assault together.
class ACR_AIAttack
{
 ACR_Base m_Target;
 ref array<ACR_Squad> m_Squads = {};
 ref array<vector> m_aStaging = {};  // one staging point per squad (pincers)
 ref array<int> m_aSlot = {};        // each squad's approach line (ACR_AICommander.s_aApproachTurns index, -1 none)
 vector m_Staging;                   // main approach, for smoke
 vector m_vApproach;                 // main approach direction, from the target towards the attackers
 bool m_bAssaulting;
 float m_fStageDeadline;
 ACR_Squad m_Suppressor;             // support-by-fire squad
 float m_fFireBaseMoveSince;         // world time the base of fire started moving up to a firing spot
 vector m_vOverwatch;                // where the base of fire holds while no enemy is seen at the objective
 bool m_bOverwatch;
 float m_fStageLimit;                // latest the forming up may be stretched to for late joiners
 int m_iStartStrength;
 bool m_bCounter;
 bool m_bGrab;                       // quick grab of an empty base (doesn't count against the attack limit)
 float m_fAssaultStart;              // world time the assault (or grab) began
 float m_fNextPush;                  // next time idle attackers are sent in again
 float m_fGrabSeconds;               // a grab's own time limit: the trip there plus slack (big maps)
 float m_fTravelSeconds;             // the slowest squad's way there on foot or by ride (#27): the forming up waits for it
 bool m_bFobRaid;                    // demolition raid against an enemy forward operating base
 ref ACR_SapperDemo m_SapperDemo;

 vector StagingOf(ACR_Squad squad)
 {
  int i = m_Squads.Find(squad);
  if (i >= 0 && i < m_aStaging.Count()) return m_aStaging[i];
  return m_Staging;
 }
 void Remove(int index)
 {
  if (m_Suppressor == m_Squads[index]) m_Suppressor = null;
  m_Squads.RemoveOrdered(index);
  if (index < m_aStaging.Count()) m_aStaging.RemoveOrdered(index);
  if (index < m_aSlot.Count()) m_aSlot.RemoveOrdered(index);
 }
 void Add(ACR_Squad squad, vector stage, int slot)
 {
  m_Squads.Insert(squad);
  m_aStaging.Insert(stage);
  m_aSlot.Insert(slot);
 }
}

// Strategic AI for one faction, adapted from the Arma 3 mod's ai_strategic.sqf
// and ai_attack.sqf. Every 15 seconds it values each base, defends the ones
// under threat, attacks the weakest enemy or empty border base, posts idle
// squads to the front, and spends RP on reinforcements. It stands down while a
// player of its faction is in command.
// A build job a far squad rides out to (ACR_AICommander.RideToBuild): given, and paid, once it is
// out at the drop.
class ACR_PendingBuild
{
 ACR_Squad m_Squad;
 int m_iTask;        // ACR_EAITask.ROADBLOCK, MINES or FORTIFY
 ACR_Base m_Base;    // the front base it is for
 vector m_vEnemy;    // that base's enemy base
 vector m_vSite;     // a roadblock's road site (m_fYaw its facing); else where the ride goes (the base)
 float m_fYaw;
 float m_fDeadline;  // world ms
}

class ACR_AICommander : Managed
{
 static const int TICK_SECONDS = 15;          // STRATEGY_TICK
 static const float DEFENSE_DISTANCE = 450;   // squads this close count as defending a base
 static const float THREAT_DISTANCE = 400;    // enemies this far past the ring threaten it
 static const float STAGING_DISTANCE = 210;   // staging point outside the target ring
 static const float STAGING_CLEARANCE = 350;  // m kept between a staging point and any other enemy base ring
 static const float ATTACK_RATIO = 1.8;       // attackers want 1.8x the defence
 static const float DEFENSE_RATIO = 1.1;      // defenders want 1.1x the threat
 static const float CONCEDE_RATIO = 4;        // give a base up against 4x the defence
 static const int PLAYER_STRENGTH = 5;
 static float STAGING_TIMEOUT = 300;          // seconds
 // An assault that hasn't taken the base by now is called off (it would
 // otherwise hold the attack slot forever). A foothold inside the base ring
 // buys it more time. Grabs of empty bases get less.
 static float ASSAULT_TIMEOUT = 600;          // s, nobody of ours inside the ring
 static float ASSAULT_HARD_TIMEOUT = 900;     // s, even with a foothold
 static bool s_bGrabByDistance = true;          // off in tests that shorten GRAB_TIMEOUT
 static float GRAB_TIMEOUT = 480;             // s  (the test harness shortens these)
 static const float STALLED_COOLDOWN = 300;   // s before the same base is tried again
 static const float PUSH_INTERVAL = 120;      // s between re-issuing assault orders
 static const float FIRE_BASE_RANGE = 500;    // m: the base of fire shoots only this close to a seen enemy
 static const float FIRE_BASE_STANDOFF = 300; // m: how far short of them it moves up when too far or blind
 // (s of supporting fire before the base of fire joins the assault anyway: the doctrine's m_fFireBaseSeconds, 240 standard)
 static const float FIRE_BASE_MOVE_S = 120;   // s to reach a firing spot before it joins instead
 static const float RETREAT_COOLDOWN_MS = 60000;
 // A reserve riding or regrouping is on its way: re-posting it would cancel that (20 Oct
 // live: a regrouping reserve was re-posted every 15 s for 15 min and never arrived).
 static bool ReserveMoving(ACR_EOrderState state) { return state == ACR_EOrderState.RIDE || state == ACR_EOrderState.REGROUP; }
 static const float REINFORCE_ATTACK_DISTANCE = 1400;
 // How far a squad can get to an attack (#27, long attacks stalled: squads 2-3 km off were sent
 // on foot because an air lift was ready or rides existed, and the attack went in before they
 // came): on foot WALK_REACH (1.5 times REINFORCE_ATTACK_DISTANCE, as before); further only in its
 // own vehicle, a taxi it can get, or a ready air lift. The forming up waits for the slowest.
 static const float WALK_REACH = 2100;
 static const float WALK_SPEED = 1.2;        // m/s on foot, as a grab's time limit counts it
 static const float RIDE_SPEED = 4;          // m/s by truck door to door (ACR_Rides.Request's estimate)
 static const float MOUNTED_SPEED = 6;
 static const float AIR_LIFT_SECONDS = 240;
 static const float STAGE_TRAVEL_MAX = 1200; // s: the most the forming up waits
 static const float REINFORCE_ATTACK_MOUNTED = 2200;
 static const float ASSAULT_JOIN_EXTRA = 400; // m past the staging ring a squad may be to join an assault already going in
 static const float STAGE_STRETCH = 0.5;      // late joiners may stretch the forming up by half of STAGING_TIMEOUT
 // Initiative: with no attack launched for a while, the commander accepts
 // worse odds (down to 65% of the usual strength) and commits its reserve,
 // so two AI sides don't sit facing each other forever. How long and how far: the doctrine.
 protected float m_fLastOffensive = -1;
 protected float m_fNextWeakLog;
 ACR_Squad m_RefillWaiting;         // a squad at a base that could refill now but for the RP (Requisition holds purchases)
 protected float m_fNextRefillLog;

 protected float Pressure()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (m_fLastOffensive < 0) m_fLastOffensive = now;
  float quiet = (now - m_fLastOffensive) / 60000 - m_Doctrine.m_fQuietMinutes;
  if (quiet <= 0) return 1;
  return Math.Max(m_Doctrine.m_fMinPressure, 1 - quiet * m_Doctrine.m_fPressurePerMinute);
 }

 ACR_Manager m_Manager;
 string m_sFaction;
 string Faction() { return m_sFaction; }
 bool m_bActive;
 protected int m_iSeconds;
 protected ref array<ref ACR_AIAttack> m_Attacks = {};
 protected ref map<ACR_Base, int> m_Defence = new map<ACR_Base, int>();
 protected ref map<ACR_Base, int> m_Threat = new map<ACR_Base, int>();
 protected ref map<ACR_Base, string> m_LastOwner = new map<ACR_Base, string>();
 protected ACR_Base m_CounterTarget;
 protected ref map<ACR_Base, float> m_StalledUntil = new map<ACR_Base, float>();
 protected ref map<ACR_Squad, float> m_RetreatingUntil = new map<ACR_Squad, float>();
 protected ref map<ACR_Squad, float> m_ReinforceRetryAt = new map<ACR_Squad, float>();
 protected ref map<ACR_Squad, float> m_Refitting = new map<ACR_Squad, float>(); // squad walking back to refill -> give up at
 protected int m_iSaveForEntry = -1;
 protected int m_iLastSavedLog = -1;
 protected float m_fNextStatus;
 protected float m_fCounterUntil;
 // Retreat (a squad in contact) and abort (a whole attack) ratios: the doctrine's, 0.4 / 0.35 standard.
 static const int SUPPORT_RESERVE = 15;    // RP the AI keeps back when calling supports

 // Engineering budget (-acrAIBuildShare N, 0 = off by default): N% (at most 30, times the
 // doctrine's m_fBuildShare) of what the side's RP gains between strategy ticks is set aside for
 // roadblocks and minefields (and Fortify positions where that order exists), and at least
 // BUILD_TRICKLE each strategy tick; unit purchases leave it alone, those orders are paid out of
 // it (CanBuild). At most BUILD_BUDGET_MAX, never more than the side holds.
 static const int BUILD_BUDGET_MAX = 100;
 static const float BUILD_TRICKLE = 2;
 float m_fBuildBudget;
 protected int m_iBuildLastRP = -1;
 static int BuildShare() { return Math.ClampInt(ACR_LogisticsRules.Choice("acrAIBuildShare", "0").ToInt(), 0, 30); }
 int BuildBudget() { return (int)m_fBuildBudget; }
 // This side's share: the 2IC panel's for its side (ACR_TwoIC.ShareFor), else the setting.
 int SideShare()
 {
  int share;
  if (ACR_TwoIC.ShareFor(m_sFaction, share)) return share;
  return BuildShare();
 }
 float EffectiveShare() { return ShareAt(m_Manager.m_Bases.GetRP(m_sFaction)); }

 // A rich side digs in (#28): over SURPLUS_FROM banked RP the share rises with the surplus, to twice
 // the share at the RP cap (ACR_BaseSystem.POINTS_CAP), still at most 50%; and the budget is kept at
 // least at that share of what is banked over SURPLUS_FROM, still at most BUILD_BUDGET_MAX. A side
 // at the cap gains no RP to take a share of (overnight Everon games: the leading side sat at the
 // cap from about 105 minutes with nothing left to buy). At or below SURPLUS_FROM nothing changes.
 static const int SURPLUS_FROM = 600;
 // SURPLUS_FROM varied by up to 15% per side (ACR_Variety, seeded from the match seed): a rich side
 // starts digging in at a slightly different bank each match.
 int SurplusFrom() { return (int)ACR_Variety.Jitter(SURPLUS_FROM, 0.15, "surplus " + m_sFaction); }

 // x1 at or below SURPLUS_FROM, x2 at the RP cap.
 float SurplusFactor(int rp)
 {
  float over = (rp - SurplusFrom()) / (float)(ACR_BaseSystem.POINTS_CAP - SurplusFrom());
  return 1 + Math.Clamp(over, 0, 1);
 }

 // A rich side fires more (#28 step 2), by the same x1..x2 surplus factor: the RP held for fire
 // missions and the missions it counts rise, while the threat that earns defensive fire or air
 // strikes and the defenders that earn heavy preparation fall, and its observers call sooner.
 float FireFactor() { return SurplusFactor(m_Manager.m_Bases.GetRP(m_sFaction)); }
 static int Lowered(int threshold, float factor)
 {
  float lowered = (1.0 / factor) * threshold;
  return Math.Max(1, (int)Math.Round(lowered));
 }

 // The share (percent of RP gained) with 'rp' banked.
 float ShareAt(int rp) { return Math.Min(SideShare() * m_Doctrine.m_fBuildShare * SurplusFactor(rp), 50); }

 // The least the budget holds with 'rp' banked: that share of the surplus (0 at or below SURPLUS_FROM).
 float SurplusFloor(int rp)
 {
  if (SideShare() <= 0 || rp <= SurplusFrom()) return 0;
  return Math.Min((rp - SurplusFrom()) * ShareAt(rp) / 100.0, BUILD_BUDGET_MAX);
 }

 // At the start of a strategy tick: the share of the RP gained since the last one, topped up to
 // the surplus floor.
 void AccrueBuildBudget()
 {
  int rp = m_Manager.m_Bases.GetRP(m_sFaction);
  float share = ShareAt(rp);
  if (share <= 0) m_fBuildBudget = 0;
  else if (m_iBuildLastRP >= 0 && rp > m_iBuildLastRP) m_fBuildBudget += (rp - m_iBuildLastRP) * share / 100.0;
  m_fBuildBudget = Math.Max(m_fBuildBudget, SurplusFloor(rp));
  m_fBuildBudget = Math.Clamp(m_fBuildBudget, 0, Math.Min(BUILD_BUDGET_MAX, rp));
  m_iBuildLastRP = rp;
 }

 // Each strategy tick while the share is on: at least a trickle goes in, so a side with a small
 // income still builds (playtest 25 Oct: 15% of a small income held 0-32 RP, and the AI built
 // one roadblock and one hasty position all game).
 void TrickleBuildBudget()
 {
  if (SideShare() <= 0) return;
  int rp = m_Manager.m_Bases.GetRP(m_sFaction);
  m_fBuildBudget = Math.Clamp(m_fBuildBudget + BUILD_TRICKLE * m_Doctrine.m_fBuildShare, 0, Math.Min(BUILD_BUDGET_MAX, rp));
 }

 void SpendBuildBudget(int spent)
 {
  if (spent > 0) m_fBuildBudget = Math.Max(0, m_fBuildBudget - spent);
 }

 // What unit purchases keep back for it.
 int BuildKeep()
 {
  if (SideShare() <= 0) return 0;
  return BuildBudget();
 }

 string BuildStatus()
 {
  if (SideShare() <= 0) return "";
  int holders;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.GetFaction() && squad.GetFaction().GetFactionKey() == m_sFaction && IsHolder(squad)) holders++;
  }
  string surplus = "";
  int rp = m_Manager.m_Bases.GetRP(m_sFaction);
  if (rp > SurplusFrom()) surplus = string.Format(", raised by %1 RP banked over %2", rp - SurplusFrom(), SurplusFrom());
  return string.Format(" | build budget %1 RP (%2%% of income%3), holding %4 of %5", BuildBudget(), (int)EffectiveShare(), surplus, holders, HoldersCap());
 }
 static const int MAX_GRABS = 3;           // empty bases it goes for at the same time (at least)
 static const int MAX_GRABS_BIG = 8;       // ... at most, on maps with many empty bases
 // Planning work one strategic tick may do before further grabs and attacks wait for the next
 // (Everon soak 23 Oct: an air lift search, a ride request and a water route in one tick froze
 // the server for 200 ms; each costs up to ~100 ms on its own).
 static const int PLAN_BUDGET_MS = 50;
 protected int m_iTickStarted;

 // The style this commander fights to this match (ACR_AIDoctrine), chosen when it is made.
 protected ref ACR_AIDoctrine m_Doctrine;
 protected string m_sDoctrineSource;
 // Its home style: the doctrine it started with (its company's, unless the setup screen overrode
 // it). A changed doctrine goes back to it once the trouble that changed it is over (PickDoctrine),
 // never inside #29's hold, and only after a varied wait past it (HomeDriftMin).
 protected string m_sHomeDoctrine;
 ACR_AIDoctrine Doctrine() { return m_Doctrine; }
 string HomeDoctrine() { return m_sHomeDoctrine; }
 int DoctrineChanges() { return m_iDoctrineChanges; }
 void SetDoctrineForTest(ACR_AIDoctrine doctrine) { m_Doctrine = doctrine; m_sDoctrineSource = "test"; }

 void ACR_AICommander(ACR_Manager manager, string factionKey)
 {
  m_Manager = manager;
  m_sFaction = factionKey;
  m_Doctrine = ACR_AIDoctrine.Choose(factionKey, m_sDoctrineSource);
  m_sHomeDoctrine = m_Doctrine.m_sKey;
  Say(string.Format("[ACR] AI commander %1 doctrine: %2 (%3; %4)", m_sFaction, m_Doctrine.m_sName, m_sDoctrineSource, m_Doctrine.m_sSummary));
 }
 protected float RetreatRatio() { return m_Doctrine.m_fRetreatRatio; }
 // Defenders of an overrun base hold to their own ratio (Fortress stands, Raider slips away).
 protected float HoldRatio() { return m_Doctrine.m_fHoldRatio; }

 // Called every second by the manager.
 void Tick(bool noPlayerCommander)
 {
  if (m_bActive != noPlayerCommander)
  {
   m_bActive = noPlayerCommander;
   Say(string.Format("[ACR] AI commander %1: %2", m_sFaction, m_bActive));
   // With the 2IC on its plans carry on for the squads nobody claims (ACR_SquadAI.Adopt).
   if (!m_bActive && !ACR_SquadAI.TwoIC()) ReleaseAll();
   // Back in command: judge its doctrine on what it does from now, not on the player's spell.
   m_fNextDoctrineReview = -1;
  }
  if (!m_bActive) return;
  GatherIntel();
  TickSapperDemos();
  m_iSeconds++;
  if (m_iSeconds % TICK_SECONDS != 1) return;
  int t = System.GetTickCount();
  m_iTickStarted = t;
  AccrueBuildBudget();
  TrickleBuildBudget();
  ReleaseRoadblockBuilders();
  Assess();
  t = ACR_Perf.Lap("ai.assess", t);
  UpdateAttacks();
  t = ACR_Perf.Lap("ai.attacks", t);
  AssignDefenders();
  ReturnToPosts();
  t = ACR_Perf.Lap("ai.defend", t);
  AssignAttacks();
  t = ACR_Perf.Lap("ai.plan", t);
  PostIdle();
  PositionMortars();
  Recrew();
  t = ACR_Perf.Lap("ai.post", t);
  ManageLogistics();
  ManageDangerRoads();
  ManageFobBuild();
  ManageFobUpgrades();
  int beforeBuild = m_Manager.m_Bases.GetRP(m_sFaction);
  ManagePendingBuilds();
  ManageRoadblocks();
  SpendBuildBudget(beforeBuild - m_Manager.m_Bases.GetRP(m_sFaction));
  beforeBuild = m_Manager.m_Bases.GetRP(m_sFaction);
  ManageMinefields();
  SpendBuildBudget(beforeBuild - m_Manager.m_Bases.GetRP(m_sFaction));
  beforeBuild = m_Manager.m_Bases.GetRP(m_sFaction);
  ManageFortify();
  SpendBuildBudget(beforeBuild - m_Manager.m_Bases.GetRP(m_sFaction));
  t = ACR_Perf.Lap("ai.logistics", t);
  Requisition();
  t = ACR_Perf.Lap("ai.buy", t);
  ReviewDoctrine();
  LogStatus();
  m_iBuildLastRP = m_Manager.m_Bases.GetRP(m_sFaction);
 }

 // ---------------------------------------------------------------- doctrine reviews
 // Doctrines can change (Jacob, 24 Oct). Every DOCTRINE_REVIEW_MIN minutes, the first after
 // DOCTRINE_FIRST_REVIEW_MIN, the commander reads its own side's report (ACR_Report: bases won
 // and lost, soldiers lost and enemies killed since the last review) and changes doctrine when
 // the current one isn't working. With the enemy's doctrine worked out from intel
 // (ACR_DoctrineIntel: the same picture its players get, never the truth) it picks the counter.
 // At most one change per review; -acrDoctrineAdapt 0 keeps the first doctrine.
 static const float DOCTRINE_FIRST_REVIEW_MIN = 15;
 static const float DOCTRINE_REVIEW_MIN = 10;
 protected float m_fNextDoctrineReview = -1;
 protected int m_iReviewLost, m_iReviewKills, m_iReviewWon, m_iReviewBasesLost;
 protected int m_iDoctrineChanges;
 protected bool m_bDoctrineSettling; // changed at the last review: the next one only snapshots
 // A changed doctrine is kept DOCTRINE_HOLD_MIN, and the one just left can't come straight back
 // for the same kind of reason (Jacob, #29: a bleeding side swapped Fortress <-> Firepower at
 // every judged review, and counters chased each other all match).
 static const float DOCTRINE_HOLD_MIN = 30;
 protected float m_fDoctrineSince = -1;
 protected string m_sDoctrineLeft, m_sDoctrineLeftWhy;

 protected void SnapshotReview()
 {
  ACR_SideStats s = ACR_Report.Of(m_sFaction);
  if (!s) return;
  m_iReviewLost = s.m_iLost;
  m_iReviewKills = s.m_iKills;
  m_iReviewWon = s.m_iBasesWon;
  m_iReviewBasesLost = s.m_iBasesLost;
 }

 protected void ReviewDoctrine()
 {
  if (!ACR_AIDoctrine.AdaptAllowed()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (m_fNextDoctrineReview < 0)
  {
   m_fNextDoctrineReview = now + DOCTRINE_FIRST_REVIEW_MIN * 60000;
   SnapshotReview();
   return;
  }
  if (now < m_fNextDoctrineReview) return;
  m_fNextDoctrineReview = now + DOCTRINE_REVIEW_MIN * 60000;
  ACR_SideStats s = ACR_Report.Of(m_sFaction);
  if (!s) return;
  // A new doctrine is judged on what it does once settled in, not on the purchases and plans
  // of the old one still arriving (Everon night soak 26 Oct: Balanced -> Firepower ->
  // Fortress, ten minutes apart, both for losses).
  if (m_bDoctrineSettling)
  {
   m_bDoctrineSettling = false;
   SnapshotReview();
   Say(string.Format("[ACR] AI commander %1 doctrine review: %2 settling in, judged from now", m_sFaction, m_Doctrine.m_sName));
   return;
  }
  int lost = s.m_iLost - m_iReviewLost;
  int kills = s.m_iKills - m_iReviewKills;
  int won = s.m_iBasesWon - m_iReviewWon;
  int basesLost = s.m_iBasesLost - m_iReviewBasesLost;
  SnapshotReview();
  string enemyKnown;
  ACR_Intel intel = m_Manager.IntelFor(m_sFaction);
  if (intel) enemyKnown = intel.Doctrine().Believed();
  string why;
  string pick = PickDoctrine(m_Doctrine.m_sKey, enemyKnown, won, basesLost, lost, kills, why, m_sHomeDoctrine);
  Say(string.Format("[ACR] AI commander %1 doctrine review: %2, bases +%3 -%4, lost %5 killed %6, enemy doctrine '%7': %8", m_sFaction, m_Doctrine.m_sName, won, basesLost, lost, kills, enemyKnown, why));
  if (pick.IsEmpty() || pick == m_Doctrine.m_sKey) return;
  float heldMin = -1;
  if (m_fDoctrineSince >= 0) heldMin = (now - m_fDoctrineSince) / 60000;
  string blocked = DoctrineChangeBlocked(pick, why, heldMin, m_sDoctrineLeft, m_sDoctrineLeftWhy);
  if (blocked.IsEmpty() && why == WHY_HOME) blocked = HomeDriftBlocked(heldMin, HomeDriftMin());
  if (!blocked.IsEmpty())
  {
   Say(string.Format("[ACR] AI commander %1 doctrine: keeps %2 instead of %3 (%4)", m_sFaction, m_Doctrine.m_sName, ACR_AIDoctrine.NameOf(pick), blocked));
   return;
  }
  ChangeDoctrine(ACR_AIDoctrine.Find(pick), why);
 }

 // Reasons that lead to the same answer count as one: losing ground and heavy losses both pick
 // whichever of Fortress and Firepower the side isn't on; every counter is a counter.
 static string DoctrineReasonKind(string why)
 {
  if (why.StartsWith("countering")) return "counter";
  if (why == "losing ground" || why == "losses too heavy") return "going badly";
  return why;
 }

 // Why a picked change can't happen yet, "" when it can. heldMin < 0: never changed this match
 // (the starting doctrine has had the first review's wait).
 static string DoctrineChangeBlocked(string pick, string why, float heldMin, string left, string leftWhy)
 {
  if (heldMin >= 0 && heldMin < DOCTRINE_HOLD_MIN)
   return string.Format("held %1 of %2 min", Math.Floor(heldMin), DOCTRINE_HOLD_MIN);
  if (!left.IsEmpty() && pick == left && DoctrineReasonKind(why) == DoctrineReasonKind(leftWhy))
   return "just left it for the same reason: " + why;
  return "";
 }

 // Going back to its home style is a reason of its own (the trouble that changed it is over), so
 // the hold above applies to it but the same-reason rule doesn't. It also waits a varied extra
 // HOME_DRIFT_EXTRA_MIN past the hold (up to twice that, ACR_Variety, seeded from the match seed,
 // the side and its changes so far): sides don't all go home at the first review they may.
 // -acrVariety 0: 40 min.
 static const string WHY_HOME = "the trouble is over, back to its home style";
 static const float HOME_DRIFT_EXTRA_MIN = 10;
 float HomeDriftMin()
 {
  return DOCTRINE_HOLD_MIN + ACR_Variety.Jitter(HOME_DRIFT_EXTRA_MIN, 1, string.Format("home drift %1 %2", m_sFaction, m_iDoctrineChanges));
 }
 static string HomeDriftBlocked(float heldMin, float waitMin)
 {
  if (heldMin >= 0 && heldMin < waitMin)
   return string.Format("back to its home style after %1 min, held %2", Math.Floor(waitMin), Math.Floor(heldMin));
  return "";
 }

 // What a review decides over one review window: the doctrine key to change to, "" to keep. Away
 // from its home style ("" none) a side only goes back to it (see below).
 static string PickDoctrine(string current, string enemyKnown, int won, int basesLost, int lost, int kills, out string why, string home = "")
 {
  bool losingGround = basesLost >= won + 2;
  bool bleeding = lost >= 12 && lost > kills * 1.6;
  // Bases gained count as winning only while not bleeding: early on they are mostly empty
  // sites (Everon night soak 26 Oct: "+3 -0, lost 20 killed 4: it is working").
  bool winning = (won >= basesLost + 2 && !bleeding) || (kills >= 12 && kills > lost * 1.6);
  bool stuck = won == 0 && basesLost == 0;
  if (winning)
  {
   why = "it is working";
   return "";
  }
  string counter = ACR_AIDoctrine.Counter(enemyKnown);
  if (!counter.IsEmpty() && counter != current)
  {
   why = "countering the enemy's " + ACR_AIDoctrine.NameOf(enemyKnown);
   return counter;
  }
  // Already the answer to what the enemy does: only losing ground makes it think again.
  if (!counter.IsEmpty() && !losingGround)
  {
   why = "already countering the enemy's " + ACR_AIDoctrine.NameOf(enemyKnown);
   return "";
  }
  // Away from its home style (its company's, or the one it started with), a side keeps the change
  // while the trouble that made it lasts, and goes back home once that is over: trouble no longer
  // moves it on, or back and forth (#29). Only a known enemy's counter (above) still moves it to
  // another doctrine.
  if (!home.IsEmpty() && current != home)
  {
   if (losingGround || bleeding)
   {
    why = "still in trouble, keeps " + ACR_AIDoctrine.NameOf(current) + " before going back to " + ACR_AIDoctrine.NameOf(home);
    return "";
   }
   why = WHY_HOME;
   return home;
  }
  if (losingGround)
  {
   why = "losing ground";
   if (current == "fortress") return "firepower";
   return "fortress";
  }
  if (bleeding)
  {
   why = "losses too heavy";
   if (current == "firepower") return "fortress";
   return "firepower";
  }
  if (stuck)
  {
   why = "getting nowhere";
   if (current == "blitz") return "raider";
   return "blitz";
  }
  why = "holding its own";
  return "";
 }

 void ChangeDoctrine(ACR_AIDoctrine doctrine, string why)
 {
  if (!doctrine || doctrine == m_Doctrine) return;
  ACR_AIDoctrine was = m_Doctrine;
  m_Doctrine = doctrine;
  m_sDoctrineSource = "changed";
  m_iDoctrineChanges++;
  m_bDoctrineSettling = true;
  m_fDoctrineSince = GetGame().GetWorld().GetWorldTime();
  m_sDoctrineLeft = was.m_sKey;
  m_sDoctrineLeftWhy = why;
  Say(string.Format("[ACR] AI commander %1 doctrine: %2 -> %3 (%4; %5)", m_sFaction, was.m_sName, doctrine.m_sName, why, doctrine.m_sSummary));
  // Its own side hears it (players fighting under this commander); the enemy has to notice.
  string line = string.Format("Commander: switching to %1 (%2)", doctrine.m_sName, why);
  ACR_Intel own = m_Manager.IntelFor(m_sFaction);
  if (own) own.AddFeed(GetGame().GetWorld().GetWorldTime(), line);
  ACR_Report.Event(m_sFaction, string.Format("commander switched to %1 (%2)", doctrine.m_sName, why));
  m_Manager.NotifyFaction(m_sFaction, line);
  ACR_Intel enemy = m_Manager.IntelFor(ACR_Debug.EnemyOf(m_sFaction));
  if (enemy) enemy.Doctrine().OnEnemyChanged(was.m_sKey);
 }

 protected void TickSapperDemos()
 {
  if (m_Attacks.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (attack && attack.m_bAssaulting && attack.m_bFobRaid && attack.m_SapperDemo)
   {
    attack.m_SapperDemo.Tick(now);
   }
  }
 }

 protected ACR_FactionSettings Settings() { return ACR_FactionSettings.Find(m_sFaction); }

 // ---------------------------------------------------------------- helpers

 protected bool IsRetreating(ACR_Squad squad)
 {
  float until;
  if (!m_RetreatingUntil.Find(squad, until)) return false;
  if (GetGame().GetWorld().GetWorldTime() >= until)
  {
   m_RetreatingUntil.Remove(squad);
   return false;
  }
  ACR_Base safe = NearestOwnBase(PositionOf(squad));
  if (safe && ThreatTo(safe) == 0 && vector.DistanceXZ(PositionOf(squad), safe.GetOrigin()) <= safe.m_fRadius && squad.KnownEnemies(300) == 0 && squad.StrengthRatio() >= RetreatRatio() && (squad.m_iAmmoPct < 0 || squad.m_iAmmoPct >= OUT_OF_AMMO))
  {
   m_RetreatingUntil.Remove(squad);
   return false;
  }
  return true;
 }

 protected void MySquads(array<ACR_Squad> result, bool freeOnly)
 {
  result.Clear();
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (m_bHandedOnly && !squad.m_bAICommanded) continue; // a player commands the side: only squads handed to us
   if (squad.HasPlayer() || !squad.GetAnchor()) continue; // players lead themselves
   if (squad.m_bMortar) continue; // mortar teams: PositionMortars
   if (squad.Aircraft()) continue; // helicopter crews: AI can't fly
   if (ACR_AirLift.Busy(squad)) continue; // flown somewhere: leave it until it has landed
   if (freeOnly && squad.m_iAITask != ACR_EAITask.FREE && squad.m_iAITask != ACR_EAITask.POSTED) continue;
   if (freeOnly && !squad.IsCombatUnit()) continue; // lone drivers, unarmed crews: kept out of the fighting
   if (freeOnly && IsRetreating(squad)) continue; // just broke contact: let it fall back instead of re-tasking at once
   result.Insert(squad);
  }
 }

 protected vector PositionOf(ACR_Squad squad)
 {
  IEntity anchor = squad.GetAnchor();
  if (anchor) return anchor.GetOrigin();
  return vector.Zero;
 }

 protected void SortByDistance(array<ACR_Squad> squads, vector point)
 {
  // simple insertion sort; squad counts are small
  for (int i = 1; i < squads.Count(); i++)
  {
   ACR_Squad current = squads[i];
   float d = vector.DistanceXZ(PositionOf(current), point);
   int j = i - 1;
   while (j >= 0 && vector.DistanceXZ(PositionOf(squads[j]), point) > d)
   {
    squads[j + 1] = squads[j];
    j--;
   }
   squads[j + 1] = current;
  }
 }

 protected bool CoolingDown(ACR_Base base)
 {
  float until;
  return m_StalledUntil.Find(base, until) && GetGame().GetWorld().GetWorldTime() < until;
 }

 protected bool Visible(ACR_Base base)
 {
  return base.IsVisibleTo(m_sFaction);
 }

 // The owner as this commander knows it: an unfound FIA pocket looks empty (ACR_Pockets).
 protected string SeenOwner(ACR_Base base)
 {
  return base.OwnerSeenBy(m_sFaction);
 }

 // Held, as this commander knows it, by someone it fights: an FIA pocket of an FIA allied to
 // this side is not (ACR_Insurgency).
 protected bool HostileOwner(ACR_Base base)
 {
  string seen = SeenOwner(base);
  return !seen.IsEmpty() && !ACR_Insurgency.Friendly(seen, m_sFaction);
 }

 bool HasKnownEnemyBase()
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (HostileOwner(base) && Visible(base)) return true;
  return false;
 }

 bool FriendlyBaseThreatened()
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner == m_sFaction && ThreatTo(base) > DefenceOf(base)) return true;
  return false;
 }

 ACR_Base NearestEnemyBase(vector point, ACR_Base except = null)
 {
  ACR_Base nearest;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == m_sFaction || !HostileOwner(base) || !Visible(base) || base == except) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), point);
   if (d < best) { best = d; nearest = base; }
  }
  return nearest;
 }

 static bool IsReconTeam(ACR_Squad squad)
 {
  if (!squad) return false;
  string name = squad.GetName();
  name.ToLower();
  return name.Contains("recon") || name.Contains("scout") || name.Contains("sniper") || name.Contains("observer") || name.Contains("spotter");
 }

 // Soldiers who can walk in and take a base: alive and on foot or riding as passengers
 // (not a vehicle's driver or gunner).
 static int Dismounts(ACR_Squad squad)
 {
  if (!squad) return 0;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  int count = 0;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   if (ACR_Transport.VehicleOf(body) && !ACR_Transport.Passenger(body)) continue;
   count++;
  }
  return count;
 }

 // A squad that can take a held base: fighting infantry, not recon, support, a crew without
 // dismounts, aircraft or mortar (sappers count on a FOB raid). Everon 26 Oct: a counterattack
 // on Meaux was one recon BRDM from 2.5 km out; its infantry partner died on the way, and the
 // BRDM overwatched 400 m short for 14 min until the attack stalled out.
 static bool CanTakeBase(ACR_Squad squad, bool fobRaid)
 {
  if (!squad || squad.Aircraft() || squad.m_bMortar || Dismounts(squad) == 0) return false;
  if (fobRaid && ACR_SapperDemo.IsSapperSquad(squad)) return true;
  return !IsReconTeam(squad) && !IsSupportTeam(squad);
 }

 static int TakersIn(ACR_AIAttack attack, ACR_Squad except = null)
 {
  int count = 0;
  foreach (ACR_Squad squad : attack.m_Squads)
   if (squad && squad != except && CanTakeBase(squad, attack.m_bFobRaid)) count++;
  return count;
 }

 // Own bases nearest to anything not ours: where fights happen.
 protected void FrontlineBases(array<ACR_Base> result)
 {
  result.Clear();
  array<ACR_Base> mine = {};
  array<ACR_Base> others = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == m_sFaction) mine.Insert(base);
   else if (Visible(base)) others.Insert(base);
  }
  if (others.IsEmpty()) { result.Copy(mine); return; }
  array<float> nearest = {};
  foreach (ACR_Base base : mine)
  {
   float gap = float.MAX;
   foreach (ACR_Base other : others) gap = Math.Min(gap, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   nearest.Insert(gap);
  }
  array<int> picked = {};
  FrontlinePick(nearest, picked);
  foreach (int i : picked) result.Insert(mine[i]);
 }

 static const float FRONTLINE_SLACK = 150; // m: bases this much farther than the closest still count

 // The indexes of every base within FRONTLINE_SLACK of the smallest gap. Two passes, so
 // the answer doesn't depend on the order the bases come in (R07, 5 Oct review: the old
 // single pass dropped bases seen before a closer one).
 static void FrontlinePick(notnull array<float> nearest, notnull array<int> result)
 {
  result.Clear();
  float best = float.MAX;
  foreach (float gap : nearest) best = Math.Min(best, gap);
  foreach (int i, float gap : nearest) if (gap <= best + FRONTLINE_SLACK) result.Insert(i);
 }

 // ---------------------------------------------------------------- supports

 protected int SupportIndex(ACR_ESupportType type)
 {
  foreach (int i, ACR_SupportEntry entry : m_Manager.m_Bases.GetFaction(m_sFaction).m_aSupports)
   if (entry.m_eType == type) return i;
  return -1;
 }

 // RP kept back from unit purchases so the fire missions it plans are always affordable:
 // the doctrine's dearest few (mortars; with m_bAirBudget the off-map air and rocket strikes
 // too) plus the usual reserve, capped by the doctrine. (Before 24 Oct pass 2 only the dearest
 // mortar counted: 25 + 15 RP, under every doctrine's cap, so the cap never changed anything.)
 // A rich side (FireFactor > 1) keeps RP for one more mission, counts air strikes whatever its
 // doctrine, and its cap rises with the factor.
 protected int SupportBudget()
 {
  float factor = FireFactor();
  bool rich = factor > 1;
  array<int> costs = {};
  foreach (ACR_SupportEntry entry : m_Manager.m_Bases.GetFaction(m_sFaction).m_aSupports)
  {
   if (!entry.m_bEnabled) continue;
   bool planned = entry.m_eType == ACR_ESupportType.MORTAR_HE_SMALL || entry.m_eType == ACR_ESupportType.MORTAR_HE_LARGE || entry.m_eType == ACR_ESupportType.MORTAR_SMOKE;
   if ((m_Doctrine.m_bAirBudget || rich) && (entry.m_eType == ACR_ESupportType.BOMB_RUN || entry.m_eType == ACR_ESupportType.ROCKET_STRIKE)) planned = true;
   if (planned && entry.m_iCost > 0) costs.Insert(entry.m_iCost);
  }
  if (costs.IsEmpty()) return 0;
  costs.Sort(true);
  int missions = m_Doctrine.m_iSupportMissions;
  if (rich) missions++;
  int budget = SUPPORT_RESERVE;
  for (int i = 0; i < costs.Count() && i < missions; i++) budget += costs[i];
  int cap = factor * m_Doctrine.m_iSupportBudgetMax;
  return Math.Min(cap, budget);
 }
 int SupportBudgetForTest() { return SupportBudget(); }

 // Needing troops beats keeping fire support money: well under strength, or a
 // base threatened by more than it has defending it.
 protected bool UrgentTroops(int combatUnits, int cap)
 {
  if (combatUnits < cap / 2) return true;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner == m_sFaction && ThreatTo(base) > DefenceOf(base)) return true;
  return false;
 }

 bool SupportReady(ACR_ESupportType type)
 {
  int index = SupportIndex(type);
  if (index < 0 || !m_Manager.m_Supports) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  ACR_SupportEntry entry = data.m_aSupports[index];
  float now = GetGame().GetWorld().GetWorldTime();
  bool ready = now >= entry.m_fReadyAt;
  if (!ready && type == ACR_ESupportType.AIR_LIFT && ACR_AirLift.WaveSlotAvailable(m_sFaction, now)) ready = true;
  return ready && data.m_iRP >= entry.CostFor(true) + SUPPORT_RESERVE;
 }

 // Own soldiers (squads and players) within radius: never call fire on them.
 protected bool FriendliesNear(vector point, float radius)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (soldier && vector.DistanceXZ(soldier.GetOrigin(), point) < radius) return true;
   }
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (faction && body && faction.GetFactionKey() == m_sFaction && vector.DistanceXZ(body.GetOrigin(), point) < radius) return true;
  }
  return false;
 }

 protected bool CallSupport(ACR_ESupportType type, vector point, string why)
 {
  if (!SupportReady(type)) return false;
  if (ACR_Mortars.IsMortarType(type) && !m_Manager.Mortars().CanFire(m_sFaction, type, point)) return false;
  if (ACR_HeliStation.IsHeliType(type) && ACR_HeliStation.UsesPads(m_sFaction) && (!m_Manager.m_Supports || !m_Manager.m_Supports.Station().ReadyFor(m_sFaction, ACR_HeliStation.NeedsGunship(type), point))) return false;
  if (FriendliesNear(point, ACR_Supports.ZoneRadius(type) + 40)) return false;
  string result = m_Manager.m_Supports.Call(m_sFaction, SupportIndex(type), point, true);
  Say(string.Format("[ACR] AI %1 support (%2): %3", m_sFaction, why, result));
  return true;
 }

 // ---------------------------------------------------------------- 0. intelligence
 // No map hacks: the commander only knows about enemies its own soldiers have
 // actually spotted (their perception), or that walk into one of its bases
 // (sentries). Each sighting is remembered at the last seen position for
 // INTEL_MEMORY_MS, then forgotten. Threat estimates, defensive fires and
 // attack planning all work from this picture, not from true positions.
 static const float INTEL_MEMORY_MS = 90000;
 static const float SENTRY_RANGE = 60;
 static const int UNKNOWN_DEFENCE = 4;        // assumed extra defenders at an enemy base we can't see into
 protected ref map<IEntity, vector> m_SeenAt = new map<IEntity, vector>();
 protected ref map<IEntity, float> m_SeenTime = new map<IEntity, float>();

 void Note(IEntity enemy, float now)
 {
  if (!enemy || !ACR_Respawn.IsAlive(enemy)) return;
  m_SeenAt.Set(enemy, enemy.GetOrigin());
  m_SeenTime.Set(enemy, now);
 }
 // Tests: a sighting at a given spot, forgetting one, and the assessment on demand.
 void NoteAtForTest(IEntity enemy, vector at, float now) { m_SeenAt.Set(enemy, at); m_SeenTime.Set(enemy, now); }
 void ForgetForTest(IEntity enemy) { m_SeenAt.Remove(enemy); m_SeenTime.Remove(enemy); }
 void AssessForTest() { Assess(); }

 protected string FactionKeyOf(IEntity body)
 {
  if (!body) return "";
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(body.FindComponent(FactionAffiliationComponent));
  if (!affiliation || !affiliation.GetAffiliatedFaction()) return "";
  return affiliation.GetAffiliatedFaction().GetFactionKey();
 }

 // Every second: the side's shared picture (ACR_Intel, gathered by the manager
 // from our squads, sentries and players).
 protected void GatherIntel()
 {
  ACR_Intel intel = m_Manager.IntelFor(m_sFaction);
  if (!intel) return;
  m_SeenAt = intel.m_SeenAt;
  m_SeenTime = intel.m_SeenTime;
 }

 // Where we last saw any member of this enemy squad (false: we haven't).
 protected bool KnownPosition(ACR_Squad squad, out vector position)
 {
  float newest = -1;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   float when;
   if (!body || !m_SeenTime.Find(body, when) || when <= newest) continue;
   newest = when;
   position = m_SeenAt.Get(body);
  }
  return newest >= 0;
 }

 // Known enemy squad nearest to a point (for defensive fires): aimed at where it was last seen.
 protected ACR_Squad EnemyNear(vector point, float radius, out vector seenAt)
 {
  ACR_Squad best;
  float bestDistance = radius;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == m_sFaction) continue;
   vector known;
   if (!KnownPosition(squad, known)) continue;
   float distance = vector.DistanceXZ(known, point);
   if (distance < bestDistance) { bestDistance = distance; best = squad; seenAt = known; }
  }
  return best;
 }

 // ---------------------------------------------------------------- 1. assessment

 protected void Assess()
 {
  m_Defence.Clear();
  m_Threat.Clear();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   int defence, threat;
   vector center = base.GetOrigin();
   // Our own squads we know exactly; enemy squads only where last seen.
   bool ours = base.m_sOwner == m_sFaction;
   foreach (ACR_Squad squad : m_Manager.m_Squads)
   {
    if (!squad.GetFaction() || !squad.GetAnchor()) continue;
    bool mine = squad.GetFaction().GetFactionKey() == m_sFaction;
    vector where = PositionOf(squad);
    if (!mine && !KnownPosition(squad, where)) continue;
    float distance = vector.DistanceXZ(where, center);
    bool friendly = squad.GetFaction().GetFactionKey() == base.m_sOwner;
    if (friendly && distance < DEFENSE_DISTANCE) defence += squad.Strength();
    if (!friendly && distance < THREAT_DISTANCE + base.m_fRadius) threat += squad.Strength();
   }
   // Player bodies not in any squad (command units) count too.
   array<int> players = {};
   GetGame().GetPlayerManager().GetPlayers(players);
   foreach (int id : players)
   {
    IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
    Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
    if (!body || !faction || m_Manager.FindSquadBySoldier(body)) continue;
    vector bodyAt = body.GetOrigin();
    if (faction.GetFactionKey() != m_sFaction)
    {
     if (!m_SeenAt.Contains(body)) continue;
     bodyAt = m_SeenAt.Get(body);
    }
    float d = vector.DistanceXZ(bodyAt, center);
    if (faction.GetFactionKey() == base.m_sOwner && d < DEFENSE_DISTANCE) defence += PLAYER_STRENGTH;
    if (faction.GetFactionKey() != base.m_sOwner && d < THREAT_DISTANCE + base.m_fRadius) threat += PLAYER_STRENGTH;
   }
   if (!base.m_sOwner.IsEmpty()) defence += base.m_iGarrisonLeft;
   if (!base.m_sOwner.IsEmpty()) defence += FortifiedCredit(base);
   if (!ours && !base.m_sOwner.IsEmpty()) defence += UNKNOWN_DEFENCE; // fog of war: expect more than we've seen
   m_Defence.Set(base, defence);
   m_Threat.Set(base, threat);
   string before;
   if (m_LastOwner.Find(base, before) && before == m_sFaction && base.m_sOwner != m_sFaction && !base.m_sOwner.IsEmpty())
   {
    // Hit back while the enemy is still thin on the ground.
    m_CounterTarget = base;
    m_fCounterUntil = GetGame().GetWorld().GetWorldTime() + m_Doctrine.m_fCounterMinutes * 60000;
    Say(string.Format("[ACR] AI %1: lost %2, planning a counterattack", m_sFaction, base.m_sName));
   }
   m_LastOwner.Set(base, base.m_sOwner);
  }
 }

 protected int DefenceOf(ACR_Base base) { int v; m_Defence.Find(base, v); return v; }
 protected int ThreatTo(ACR_Base base) { int v; m_Threat.Find(base, v); return v; }
 int DefenceOfForTest(ACR_Base base) { return DefenceOf(base); }

 // A manned tier II or III position within DEFENSE_DISTANCE of a base counts its holders again,
 // half (tier II) or whole (tier III), on top of the squad itself (and +5 a gun with a gunner in it,
 // as for any armed vehicle): dug in, they hold more than a squad posted at the base (Jacob, #25).
 // An enemy's counts only where we know its holders are. Threat is left alone: a position holds
 // ground, it doesn't take a base.
 static const float MANNED_REACH = 60; // m: its holders this close to it count as manning it
 protected int FortifiedCredit(ACR_Base base)
 {
  int credit;
  bool ours = base.m_sOwner == m_sFaction;
  foreach (ACR_Fortification fort : ACR_Fortify.s_Positions)
  {
   if (fort.m_sFaction != base.m_sOwner || fort.m_iTier < 2 || vector.DistanceXZ(fort.m_vPos, base.GetOrigin()) > DEFENSE_DISTANCE) continue;
   ACR_Squad holder = ManningSquad(fort);
   if (!holder) continue;
   vector where = PositionOf(holder);
   if (!ours && !KnownPosition(holder, where)) continue;
   if (vector.DistanceXZ(where, fort.m_vPos) > MANNED_REACH) continue;
   int men = holder.CountAlive();
   if (fort.m_iTier >= 3) credit += men;
   else credit += men / 2;
  }
  return credit;
 }

 // The squad (AI or player) holding a position, on its way back to one included.
 protected ACR_Squad ManningSquad(ACR_Fortification fort)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.m_Fortification == fort && squad.CountAlive() > 0) return squad;
  }
  return null;
 }
 void SetThreatForTest(ACR_Base base, int threat, int defence) { m_Threat.Set(base, threat); m_Defence.Set(base, defence); }
 void AssignDefendersForTest() { AssignDefenders(); }

 // The AI-led squads of ours holding a roadblock or position within 'reach' of 'pos' (added to 'into').
 protected void HoldersNear(vector pos, float reach, notnull array<ACR_Squad> into)
 {
  foreach (ACR_Squad holder : m_Manager.m_Squads)
  {
   if (!IsHolder(holder) || !holder.GetFaction() || holder.GetFaction().GetFactionKey() != m_sFaction || holder.HasPlayer() || holder.m_Commander != 0) continue;
   if (vector.DistanceXZ(PositionOf(holder), pos) > reach) continue;
   into.Insert(holder);
  }
 }

 // ---------------------------------------------------------------- 2. defence

 protected void AssignDefenders()
 {
  // Release defenders of bases that are no longer threatened (or no longer ours).
  // (only our own: with both sides on AI, the other commander's defenders
  // were set free here every tick, since their base isn't ours)
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad.m_iAITask != ACR_EAITask.DEFEND || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (squad == m_FobEscort && HasActiveFobProject()) continue;
   if (OnRequest(squad)) continue; // a player's request to the 2IC (#14 v1)
   ACR_Base held = squad.m_AITarget;
   if (!held || held.m_sOwner != m_sFaction || ThreatTo(held) == 0)
   {
    if (held && m_Manager.HasPrimedDemolition(held)) continue;
    squad.m_iAITask = ACR_EAITask.FREE;
    continue;
   }
   // A defence that is being overrun: the badly hurt get out rather than die in place.
   bool overrun = ThreatTo(held) > DefenceOf(held) * 2;
   if (overrun && squad.StrengthRatio() < HoldRatio() && squad.KnownEnemies(300) > 0)
    FallBack(squad, "defence of " + held.m_sName + " overrun", held);
   else if (squad.m_iAmmoPct >= 0 && squad.m_iAmmoPct < OUT_OF_AMMO && squad.KnownEnemies(300) > 0)
    FallBack(squad, "out of ammo", held);
  }
  array<ACR_Squad> free = {};
  MySquads(free, true);
  if (m_ReserveSquad) free.RemoveItem(m_ReserveSquad);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction) continue;
   int threat = ThreatTo(base);
   if (threat == 0) continue;
   int defence = DefenceOf(base);
   if (threat > defence * CONCEDE_RATIO * m_Doctrine.m_fConcedeRatio && defence > 0) continue; // lost cause
   float needed = threat * DEFENSE_RATIO * m_Doctrine.m_fDefenseRatio - defence;
   if (needed <= 0) continue;
   // Outnumbered there (the attackers outweigh all we have within DEFENSE_DISTANCE, its garrison
   // included): the squads holding our roadblocks and positions nearby go too, nearest first with
   // the free squads and before the reserve, a Fortress's included (review of #16: on a weak side,
   // holders that stay lock its few squads into static positions). Else they go only as a last
   // resort, below.
   bool outnumbered = threat > defence;
   array<ACR_Squad> pool = {};
   foreach (ACR_Squad spare : free) pool.Insert(spare);
   if (outnumbered) HoldersNear(base.GetOrigin(), DEFENSE_DISTANCE * 3, pool);
   SortByDistance(pool, base.GetOrigin());
   int k = 0;
   while (k < pool.Count() && needed > 0)
   {
    ACR_Squad squad = pool[k];
    if (vector.DistanceXZ(PositionOf(squad), base.GetOrigin()) > DEFENSE_DISTANCE * 3) { k++; continue; }
    if (IsSupportTeam(squad) && free.Count() > 1) { k++; continue; }
    bool holding = IsHolder(squad);
    if (holding) NoteLeftPost(squad);
    squad.SetMode(ACR_EMoveMode.FAST);
    Travel(squad, base.GetOrigin(), true, 0);
    squad.m_iAITask = ACR_EAITask.DEFEND;
    squad.m_AITarget = base;
    needed -= squad.Strength();
    pool.RemoveOrdered(k);
    free.RemoveItem(squad);
    if (holding)
     Say(string.Format("[ACR] AI %1: %2 leaves what it holds to defend %3 (threat %4, outnumbered: defence %5)", m_sFaction, squad.GetName(), base.m_sName, threat, defence));
    else
     Say(string.Format("[ACR] AI %1: %2 defends %3 (threat %4)", m_sFaction, squad.GetName(), base.m_sName, threat));
   }
   if (needed > 0 && m_ReserveSquad && !m_ReserveSquad.HasPlayer() && m_ReserveSquad.CountAlive() > 0 && m_ReserveSquad.KnownEnemies(250) == 0)
   {
    ACR_Squad qrf = m_ReserveSquad;
    m_ReserveSquad = null;
    qrf.SetMode(ACR_EMoveMode.FAST);
    Travel(qrf, base.GetOrigin(), true, 0);
    qrf.m_iAITask = ACR_EAITask.DEFEND;
    qrf.m_AITarget = base;
    needed -= qrf.Strength();
    Say(string.Format("[ACR] AI %1: QRF reserve %2 committed to defend %3 (threat %4)", m_sFaction, qrf.GetName(), base.m_sName, threat));
   }
   // Still short: the builders holding our roadblocks and positions nearby, as a last resort
   // (never a Fortress's while it isn't outnumbered there: its defences stay manned).
   if (needed <= 0 || m_Doctrine.m_bHoldersStay) continue;
   array<ACR_Squad> holders = {};
   HoldersNear(base.GetOrigin(), DEFENSE_DISTANCE * 3, holders);
   foreach (ACR_Squad holder : holders)
   {
    if (needed <= 0) break;
    NoteLeftPost(holder);
    holder.SetMode(ACR_EMoveMode.FAST);
    Travel(holder, base.GetOrigin(), true, 0);
    holder.m_iAITask = ACR_EAITask.DEFEND;
    holder.m_AITarget = base;
    needed -= holder.Strength();
    Say(string.Format("[ACR] AI %1: %2 leaves what it holds to defend %3 (threat %4)", m_sFaction, holder.GetName(), base.m_sName, threat));
   }
  }
  DefendInDepth();
  // Defensive fires: shell the attackers of a base that is losing the fight (a rich side: one
  // that is merely in a fight, down to half its defence at the RP cap).
  float fireFactor = FireFactor();
  foreach (ACR_Base held : ACR_Base.s_All)
  {
   if (held.m_sOwner != m_sFaction || fireFactor * ThreatTo(held) <= DefenceOf(held)) continue;
   vector seenAt;
   ACR_Squad attacker = EnemyNear(held.GetOrigin(), THREAT_DISTANCE + held.m_fRadius, seenAt);
   if (!attacker) continue;
   int airThreat = Lowered(m_Doctrine.m_iDefensiveAirThreat, fireFactor);
   if (ThreatTo(held) >= airThreat && CallSupport(ACR_ESupportType.HELI_GUN_RUN, seenAt, "gun run at " + held.m_sName)) break;
   if (ThreatTo(held) >= airThreat && CallSupport(ACR_ESupportType.ROCKET_STRIKE, seenAt, "rocket strike at " + held.m_sName)) break;
   if (CallSupport(ACR_ESupportType.MORTAR_HE_LARGE, seenAt, "defensive fire at " + held.m_sName)) break;
   if (CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, seenAt, "defensive fire at " + held.m_sName)) break;
   // No mortar in reach: the howitzer battery (slow and dear, but it reaches anywhere).
   if (CallSupport(ACR_ESupportType.ARTILLERY_BATTERY, seenAt, "battery fire at " + held.m_sName)) break;
  }

  // Roadblock interdiction fires: shell enemies spotted approaching forward checkpoints
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (!rb || rb.m_sFaction != m_sFaction) continue;
   vector enemySeen;
   ACR_Squad hostile = EnemyNear(rb.m_vPos, 220, enemySeen);
   if (hostile && vector.DistanceXZ(rb.m_vPos, enemySeen) >= 60.0)
   {
    if (CallSupport(ACR_ESupportType.MORTAR_HE_LARGE, enemySeen, "roadblock interdiction")) break;
    if (CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, enemySeen, "roadblock interdiction")) break;
   }
  }
 }

 // Defence in depth (Jacob 20 Oct: defenders only ever held the point itself). When an
 // attack on a base is seen coming (intel), part of its defenders move out to a forward
 // line 100-300 m towards it (Travel garrisons a building there when there is one) and the
 // rest stay at the point as the reserve (the base garrison counts towards it). The forward
 // squads fall back to the point when the enemy gets too close to hold out there, when they
 // are badly hurt, or when the threat is gone.
 static const float FORWARD_MIN = 100;
 static const float FORWARD_MAX = 300;
 static const float FORWARD_ENEMY_GAP = 80;   // m a forward position stays short of the enemy
 static const float FORWARD_REISSUE = 100;    // m the line must move before squads are moved again
 static const float FORWARD_SPREAD = 60;      // m between forward squads along the line
 protected ref map<ACR_Squad, vector> m_ForwardAt = new map<ACR_Squad, vector>();

 // Where the known attackers of a base are (strength-weighted mean of where they were last
 // seen within the threat ring). False: none seen.
 bool ThreatFrom(ACR_Base base, out vector at)
 {
  vector center = base.GetOrigin();
  vector sum;
  float weight;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == base.m_sOwner) continue;
   vector known;
   if (!KnownPosition(squad, known) || vector.DistanceXZ(known, center) > THREAT_DISTANCE + base.m_fRadius) continue;
   float w = Math.Max(1, squad.Strength());
   sum = sum + known * w;
   weight += w;
  }
  if (weight <= 0) return false;
  at = sum * (1.0 / weight);
  return true;
 }

 bool ForwardSpotOf(ACR_Squad squad, out vector spot)
 {
  return m_ForwardAt.Find(squad, spot);
 }

 protected void RecallForward(ACR_Squad squad, ACR_Base base, string why)
 {
  m_ForwardAt.Remove(squad);
  if (!base) return;
  squad.SetMode(ACR_EMoveMode.FAST);
  Travel(squad, base.GetOrigin(), true, 0);
  Say(string.Format("[ACR] AI %1: %2 falls back from the forward line to %3 (%4)", m_sFaction, squad.GetName(), base.m_sName, why));
 }

 void DefendInDepth()
 {
  // Forget squads that no longer defend (freed, fallen back, taken over, gone).
  array<ACR_Squad> stale = {};
  foreach (ACR_Squad held, vector heldAt : m_ForwardAt)
   if (!held || !m_Manager.m_Squads.Contains(held) || held.HasPlayer() || held.m_iAITask != ACR_EAITask.DEFEND || !held.m_AITarget) stale.Insert(held);
  foreach (ACR_Squad gone : stale) m_ForwardAt.Remove(gone);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || ThreatTo(base) == 0) continue;
   vector center = base.GetOrigin();
   array<ACR_Squad> defenders = {};
   foreach (ACR_Squad squad : m_Manager.m_Squads)
   {
    if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction || squad.HasPlayer() || !squad.GetAnchor()) continue;
    if (m_bHandedOnly && !squad.m_bAICommanded) continue;
    if (squad == m_FobEscort || squad == m_ReserveSquad || squad.m_bMortar || squad.Aircraft() || ACR_AirLift.Busy(squad)) continue;
    bool defending = squad.m_iAITask == ACR_EAITask.DEFEND && squad.m_AITarget == base;
    // Troops posted at a threatened base are its defence now.
    bool posted = squad.m_iAITask == ACR_EAITask.POSTED && squad.IsCombatUnit() && !IsSupportTeam(squad) && vector.DistanceXZ(PositionOf(squad), center) <= base.m_fRadius + 150;
    if (!defending && !posted) continue;
    if (posted)
    {
     squad.m_iAITask = ACR_EAITask.DEFEND;
     squad.m_AITarget = base;
    }
    defenders.Insert(squad);
   }
   vector enemyAt;
   bool seen = ThreatFrom(base, enemyAt);
   float enemyDistance = 0;
   if (seen) enemyDistance = vector.DistanceXZ(enemyAt, center);
   float depth = 0;
   if (seen) depth = Math.Clamp(Math.Max(enemyDistance * 0.5, base.m_fRadius + 40), FORWARD_MIN, FORWARD_MAX);
   bool lineHolds = seen && enemyDistance - depth >= FORWARD_ENEMY_GAP;
   // How many go forward: the doctrine's share (half standard), the base garrison counting as
   // one more squad at the point; at least one squad (or the garrison) stays as the reserve.
   int extra = 0;
   if (base.m_iGarrisonLeft > 0) extra = 1;
   int forward = (int)(m_Doctrine.m_fForwardShare * (defenders.Count() + extra));
   if (extra == 0) forward = Math.Min(forward, defenders.Count() - 1);
   if (!lineHolds) forward = 0;
   // Already forward and fit first (no shuffling), then the nearest to the line.
   vector dir = enemyAt - center;
   dir[1] = 0;
   if (dir.LengthSq() > 1) dir.Normalize();
   else dir = Vector(0, 0, 1);
   vector line = center + dir * depth;
   array<ACR_Squad> picked = {};
   foreach (ACR_Squad was : defenders)
    if (picked.Count() < forward && m_ForwardAt.Contains(was) && was.StrengthRatio() >= 0.5) picked.Insert(was);
   SortByDistance(defenders, line);
   foreach (ACR_Squad near : defenders)
    if (picked.Count() < forward && !picked.Contains(near) && near.StrengthRatio() >= 0.5) picked.Insert(near);
   foreach (ACR_Squad member : defenders)
   {
    if (picked.Contains(member) || !m_ForwardAt.Contains(member)) continue;
    string why = "the enemy is too close";
    if (member.StrengthRatio() < 0.5) why = "badly hurt";
    else if (lineHolds) why = "needed in reserve";
    RecallForward(member, base, why);
   }
   vector across = Vector(-dir[2], 0, dir[0]);
   foreach (int i, ACR_Squad goer : picked)
   {
    float offset = (i - (picked.Count() - 1) * 0.5) * FORWARD_SPREAD;
    vector spot = ACR_Manager.Ground(line + across * offset);
    if (BBB_Terrain.IsWaterAt(spot)) spot = ACR_Manager.Ground(line);
    if (BBB_Terrain.IsWaterAt(spot)) continue;
    vector had;
    if (m_ForwardAt.Find(goer, had) && vector.DistanceXZ(had, spot) < FORWARD_REISSUE) continue;
    m_ForwardAt.Set(goer, spot);
    if (vector.DistanceXZ(PositionOf(goer), spot) > 220 && goer.KnownEnemies(350) == 0) goer.SetMode(ACR_EMoveMode.FAST);
    else goer.SetMode(ACR_EMoveMode.COMBAT);
    Travel(goer, spot, true, 0);
    Say(string.Format("[ACR] AI %1: %2 moves out to a forward line %3 m from %4 towards the attack (enemy seen %5 m out, %6 stay in reserve)", m_sFaction, goer.GetName(), Math.Round(depth), base.m_sName, Math.Round(enemyDistance), defenders.Count() - picked.Count()));
   }
  }
 }

 // ---------------------------------------------------------------- 3. attacks

 static const int REACH_NONE = 0;
 static const int REACH_WALK = 1;
 static const int REACH_OWN = 2;
 static const int REACH_TAXI = 3;
 static const int REACH_AIR_LIFT = 4;

 // How a free squad would get to 'target' for an attack, and in how many seconds: in its own
 // vehicle; on foot within the doctrine's ride distance; by a taxi it can get (a free transport
 // within the pickup range ACR_Rides.Request fetches from, not counted for another squad of this
 // attack: added to 'taxis'); on foot within WALK_REACH; by a ready air lift (one squad an attack,
 // 'liftTaken'); else REACH_NONE.
 protected int Reach(ACR_Squad squad, vector target, notnull array<ACR_Squad> taxis, bool liftTaken, out float seconds)
 {
  float dist = vector.DistanceXZ(PositionOf(squad), target);
  seconds = 0;
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad))
  {
   seconds = dist / MOUNTED_SPEED;
   return REACH_OWN;
  }
  if (dist <= m_Doctrine.m_fRideDistance || !m_Manager.m_Rides)
  {
   if (dist > WALK_REACH) return REACH_NONE;
   seconds = dist / WALK_SPEED;
   return REACH_WALK;
  }
  float fetch;
  ACR_Squad taxi = m_Manager.m_Rides.FindTransport(m_sFaction, PositionOf(squad), dist, squad.CountAlive(), fetch, true, taxis);
  if (taxi)
  {
   taxis.Insert(taxi);
   seconds = fetch / RIDE_SPEED + 45 + dist / RIDE_SPEED;
   return REACH_TAXI;
  }
  if (dist <= WALK_REACH)
  {
   seconds = dist / WALK_SPEED;
   return REACH_WALK;
  }
  if (!liftTaken && SupportReady(ACR_ESupportType.AIR_LIFT))
  {
   seconds = AIR_LIFT_SECONDS;
   return REACH_AIR_LIFT;
  }
  return REACH_NONE;
 }

 // The strength of the free squads that can get to 'target' (Reach), each taxi counted once.
 protected int ReachableStrength(array<ACR_Squad> free, vector target)
 {
  int strength;
  array<ACR_Squad> taxis = {};
  bool liftTaken = false;
  foreach (ACR_Squad squad : free)
  {
   float seconds;
   int how = Reach(squad, target, taxis, liftTaken, seconds);
   if (how == REACH_NONE) continue;
   if (how == REACH_AIR_LIFT) liftTaken = true;
   strength += squad.Strength();
  }
  return strength;
 }

 // How long an attack's forming up waits: STAGING_TIMEOUT, or its slowest squad's way there and
 // a minute, at most STAGE_TRAVEL_MAX.
 static float StageSeconds(float travel)
 {
  return Math.Max(STAGING_TIMEOUT, Math.Min(STAGE_TRAVEL_MAX, travel + 60));
 }

 int ReachForTest(ACR_Squad squad, vector target, notnull array<ACR_Squad> taxis, bool liftTaken, out float seconds) { return Reach(squad, target, taxis, liftTaken, seconds); }
 int ReachableStrengthForTest(array<ACR_Squad> free, vector target) { return ReachableStrength(free, target); }

 static bool IsSupportTeam(ACR_Squad squad)
 {
  if (!squad || squad.FirePower() == 0) return true;
  if (squad.m_Group && squad.m_Group.m_eACRFormation != ACR_EFormation.NONE) return ACR_InfantryCatalog.Specialist(squad.m_Group.m_eACRFormation);
  string name = squad.GetName();
  name.ToLower();
  array<string> words = {"observer", "recon", "scout", "spotter", "medic", "sniper", "sapper", "engineer", "logistic", "supply", "radio", "hq"};
  foreach (string word : words)
   if (name.Contains(word)) return true;
  return false;
 }

 protected float NearestFreeDistance(array<ACR_Squad> free, vector point)
 {
  float best = float.MAX;
  foreach (ACR_Squad squad : free)
   best = Math.Min(best, vector.DistanceXZ(PositionOf(squad), point));
  return best;
 }

 // Strength an attack on a held base wants (it is reinforced up to 1.25x this).
 protected float AttackNeed(ACR_Base target)
 {
  if (target.m_bBuilt) return Math.Max(8, DefenceOf(target) * 0.9 + target.m_iGarrisonLeft * 0.7);
  return Math.Max(10, DefenceOf(target) * ATTACK_RATIO * m_Doctrine.m_fAttackRatio * 0.65) + target.m_iGarrisonLeft;
 }

 protected void ReinforceAttacks(array<ACR_Squad> free)
 {
  if (free.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (free.IsEmpty()) break;
   ACR_Base target = attack.m_Target;
   if (!target || attack.m_bGrab || target.m_sOwner == m_sFaction || target.m_bRetired) continue;
   if (attack.m_SapperDemo && attack.m_SapperDemo.InProgress()) continue;
   int strength;
   foreach (ACR_Squad member : attack.m_Squads) strength += member.Strength();
   float need = AttackNeed(target);
   if (attack.m_bAssaulting && attack.m_fAssaultStart > 0 && now - attack.m_fAssaultStart > 90000) need *= 1.35;
   if (strength >= need * 1.15) continue;
   SortByDistance(free, target.GetOrigin());
   int k = 0;
   while (k < free.Count() && strength < need * 1.25)
   {
    ACR_Squad squad = free[k];
    float dist = vector.DistanceXZ(PositionOf(squad), target.GetOrigin());
    float maxDist = REINFORCE_ATTACK_DISTANCE;
    if (squad.IsMounted()) maxDist = REINFORCE_ATTACK_MOUNTED;
    // Once the assault is going in, only squads close enough to arrive with it join: no
    // waves walking in one after another (Jacob 20 Oct, Etoupe).
    if (attack.m_bAssaulting) maxDist = Math.Min(maxDist, target.m_fRadius + STAGING_DISTANCE + ASSAULT_JOIN_EXTRA);
    if (dist > REINFORCE_ATTACK_MOUNTED) break;
    // Observers, medics, snipers and other support teams don't join an assault.
    if (dist > maxDist || IsSupportTeam(squad)) { k++; continue; }
    vector stage;
    int slot = PickApproach(attack, PositionOf(squad), stage);
    attack.Add(squad, stage, slot);
    // Still forming up: the assault waits for it (within limits) so they go in together.
    if (!attack.m_bAssaulting)
    {
     float speed = 2;
     if (squad.IsMounted()) speed = 8;
     float arrive = now + (vector.DistanceXZ(PositionOf(squad), stage) / speed + 30) * 1000;
     attack.m_fStageDeadline = Math.Max(attack.m_fStageDeadline, Math.Min(arrive, attack.m_fStageLimit));
    }
    strength += squad.Strength();
    if (strength > attack.m_iStartStrength) attack.m_iStartStrength = strength;
    squad.m_iAITask = ACR_EAITask.ATTACK;
    squad.m_AITarget = target;
    if (attack.m_bAssaulting)
    {
     squad.SetMode(ACR_EMoveMode.COMBAT);
     squad.Assault(target.GetOrigin());
    }
    else
    {
     squad.SetMode(ACR_EMoveMode.FAST);
     Travel(squad, stage, false, 0, target);
     // A covered way round is longer: the forming up waits for it (within limits).
     if (m_fCoverDetour > 0)
     {
      float covered = now + ((vector.DistanceXZ(PositionOf(squad), stage) + m_fCoverDetour) / COVER_WALK_SPEED + 30) * 1000;
      attack.m_fStageDeadline = Math.Max(attack.m_fStageDeadline, Math.Min(covered, attack.m_fStageLimit));
     }
    }
    free.RemoveOrdered(k);
    Say(string.Format("[ACR] AI %1: %2 reinforces the attack on %3 (%4/%5), staging at grid %6", m_sFaction, squad.GetName(), target.m_sName, strength, (int)need, ACR_Manager.Grid(stage)));
   }
  }
 }

 protected bool PlanBudgetSpent()
 {
  return System.GetTickCount() - m_iTickStarted > PLAN_BUDGET_MS;
 }

 protected void AssignAttacks()
 {
  ACR_FactionSettings settings = Settings();
  int maxAttacks = 1;
  if (settings) maxAttacks = settings.m_iAIAttacks;
  bool counterDue = m_CounterTarget && m_CounterTarget.m_sOwner != m_sFaction && GetGame().GetWorld().GetWorldTime() < m_fCounterUntil && !IsAttacking(m_CounterTarget);
  array<ACR_Squad> free = {};
  MySquads(free, true);
  // Keep a reserve once the force is big enough, at a secure base behind the front (not the
  // HQ unless nothing else fits: Jacob 14 Oct, reserves at the HQ took too long to respond).
  array<ACR_Squad> all = {};
  MySquads(all, false);
  int minAllForReserve = 5;
  if (ACR_Manager.ThreatOption() == 0) minAllForReserve = 6;
  else if (ACR_Manager.ThreatOption() == 2) minAllForReserve = 4;
  minAllForReserve = (int)Math.Max(3, minAllForReserve + m_Doctrine.m_iReserveSquads);
  if (all.Count() >= minAllForReserve && free.Count() >= 2 && Pressure() >= 0.9)
  {
   ACR_Base home = ReserveBase();
   if (home)
   {
    SortByDistance(free, home.GetOrigin());
    ACR_Squad reserve;
    if (m_ReserveSquad && free.Contains(m_ReserveSquad) && vector.DistanceXZ(PositionOf(m_ReserveSquad), home.GetOrigin()) <= RESERVE_MAX_PULL && !IsSupportTeam(m_ReserveSquad))
     reserve = m_ReserveSquad;
    if (!reserve)
    {
     foreach (ACR_Squad candidateReserve : free)
     {
      if (vector.DistanceXZ(PositionOf(candidateReserve), home.GetOrigin()) > RESERVE_MAX_PULL) break;
      if (!IsSupportTeam(candidateReserve)) { reserve = candidateReserve; break; }
     }
    }
    if (!reserve && vector.DistanceXZ(PositionOf(free[0]), home.GetOrigin()) <= RESERVE_MAX_PULL)
     reserve = free[0];
    if (reserve)
    {
     m_ReserveSquad = reserve;
     free.RemoveItem(reserve);
     bool enRoute = reserve.m_iAITask == ACR_EAITask.POSTED && (ACR_AirLift.Busy(reserve) || ReserveMoving(reserve.OrderState()) || (reserve.m_Group && reserve.m_Group.GetCurrentWaypoint() && vector.DistanceXZ(reserve.m_MoveTarget, home.GetOrigin()) <= home.m_fRadius + 150));
     if (reserve.m_iAITask != ACR_EAITask.POSTED || (!enRoute && vector.DistanceXZ(PositionOf(reserve), home.GetOrigin()) > 150))
     {
      reserve.m_iAITask = ACR_EAITask.POSTED;
      reserve.SetMode(ACR_EMoveMode.COMBAT);
      Travel(reserve, home.GetOrigin(), true, 0);
      Say(string.Format("[ACR] AI %1: %2 held in reserve at %3", m_sFaction, reserve.GetName(), home.m_sName));
     }
    }
   }
  }
  if (free.IsEmpty()) return;

  // Empty bases: several quick grabs at once, each by whichever free squad gets
  // there first (vehicles count as three times faster than walking).
  int grabs = 0;
  int attacks = 0;
  foreach (ACR_AIAttack running : m_Attacks)
  {
   if (running.m_bGrab) grabs++;
   else attacks++;
  }
  // Big maps (Everon soak 5 Oct: 3 grabs at once left each side with 7 of 56 bases after
  // 18 min): one grab per 6 empty bases in sight, 3 to 8.
  int unclaimed;
  foreach (ACR_Base open : ACR_Base.s_All) if (SeenOwner(open).IsEmpty() && Visible(open)) unclaimed++;
  int maxGrabs = Math.Clamp(unclaimed / 6, Math.Max(1, MAX_GRABS + m_Doctrine.m_iGrabs), Math.Max(1, MAX_GRABS_BIG + m_Doctrine.m_iGrabs));
  while (grabs < maxGrabs && !free.IsEmpty() && !PlanBudgetSpent())
  {
   ACR_Base empty;
   ACR_Squad runner;
   float bestTime = float.MAX;
   foreach (ACR_Base candidate : ACR_Base.s_All)
   {
    if (!SeenOwner(candidate).IsEmpty() || !Visible(candidate) || IsAttacking(candidate) || CoolingDown(candidate)) continue;
    foreach (ACR_Squad squad : free)
    {
     // A medic or other support team stays out of it, as out of attacks (#34).
     if (IsSupportTeam(squad)) continue;
     float dist = vector.DistanceXZ(PositionOf(squad), candidate.GetOrigin());
     if (!squad.IsMounted() && dist > 1500) continue;
     float speed = 3;
     if (squad.IsMounted()) speed = 9;
     float time = dist / speed;
     if (time < bestTime) { bestTime = time; empty = candidate; runner = squad; }
    }
   }
   if (!empty) break;
   ACR_AIAttack grab = new ACR_AIAttack();
   grab.m_Target = empty;
   grab.m_bGrab = true;
   grab.m_bAssaulting = true;
   grab.m_fAssaultStart = GetGame().GetWorld().GetWorldTime();
   grab.Add(runner, empty.GetOrigin(), -1);
   grab.m_iStartStrength = runner.Strength();
   // Everon soak 5 Oct: most grabs of bases 2-3 km away timed out on the way at 480 s.
   // Allow the walk (or drive) there at about 1.2 m/s, plus 3 minutes. Foot grabs are already
   // limited to 1500 m above; only mounted grabs, which can be far but fast, cap at 10 min.
   grab.m_fGrabSeconds = vector.DistanceXZ(PositionOf(runner), empty.GetOrigin()) / 1.2 + 180;
   if (runner.IsMounted()) grab.m_fGrabSeconds = Math.Min(600, grab.m_fGrabSeconds);
   runner.m_iAITask = ACR_EAITask.ATTACK;
   runner.m_AITarget = empty;
   runner.SetMode(ACR_EMoveMode.FAST);
   Travel(runner, empty.GetOrigin(), false, 100);
   free.RemoveItem(runner);
   m_Attacks.Insert(grab);
   grabs++;
   Say(string.Format("[ACR] AI %1: %2 grabs empty %3 (about %4 s away)", m_sFaction, runner.GetName(), empty.m_sName, (int)bestTime));
   TwoICReport(string.Format("Taking %1 with %2 (about %3 s away)", empty.m_sName, runner.GetName(), (int)bestTime));
  }
  // A small 2IC force (#14 v1) only grabs empty bases.
  if (SmallForce())
  {
   float smallNow = GetGame().GetWorld().GetWorldTime();
   if (smallNow >= m_fNextSmallLog)
   {
    m_fNextSmallLog = smallNow + 60000;
    PrintFormat("[ACR] 2IC %1: a small force (%2 squads): grabs of empty bases only", m_sFaction, ACR_SquadAI.CountFor(m_sFaction));
   }
   return;
  }
  if (PlanBudgetSpent()) return;
  ReinforceAttacks(free);
  if (free.IsEmpty() || PlanBudgetSpent()) return;
  // Plenty of idle troops: open more fronts. On large maps with many bases and idle armies,
  // allow up to 6 extra attacks so a dominant side doesn't sit idle.
  int maxExtraAttacks = m_Doctrine.m_iExtraFronts;
  if (ACR_Base.s_All.Count() > 24) maxExtraAttacks = maxExtraAttacks * 2;
  maxAttacks += Math.Min(maxExtraAttacks, free.Count() / m_Doctrine.m_iSquadsPerFront);
  // "Take this base" (a player's request to the 2IC, #14 v1): the next attack goes for it.
  ACR_Base requested = RequestedTarget();
  if (requested && IsAttacking(requested)) requested = null;
  if (attacks >= maxAttacks && !counterDue && !requested) return;

  // Target: a counterattack on a base just lost, otherwise the enemy base with
  // the lowest (defence x ratio) scaled by distance -- the best one we can
  // actually muster enough for, rather than waiting forever on one we can't.
  float pressure = Pressure();
  int available;
  foreach (ACR_Squad counted : free) available += counted.Strength();
  ACR_Base target;
  ACR_Base wanted;
  float required = 1;
  float wantedRequired;
  int wantedReach;
  vector from = PositionOf(free[0]);
  array<ACR_Base> candidates = {};
  array<float> scores = {};
  if (counterDue) candidates.Insert(m_CounterTarget);
  else if (requested) candidates.Insert(requested);
  else
  {
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner == m_sFaction || !HostileOwner(base) || !Visible(base) || IsAttacking(base) || CoolingDown(base)) continue;
    float cost = Math.Max(10, DefenceOf(base) * ATTACK_RATIO * m_Doctrine.m_fAttackRatio);
    float dist = NearestFreeDistance(free, base.GetOrigin());
    // FIA pockets: cleared when in the way (near our own bases), after the real enemy.
    if (base.m_bPocket && NearestOwnDistance(base.GetOrigin()) > POCKET_IN_THE_WAY) continue;
    float score = cost * (dist / 1000 + 0.1);
    if (base.m_bPocket) score *= m_Doctrine.m_fPocketScore;
    if (base.m_bContested || (DefenceOf(base) <= UNKNOWN_DEFENCE + 2 && dist < 1200))
     score *= 0.65 * m_Doctrine.m_fWeakTarget;
    if (base.m_bBuilt)
    {
     score *= 0.35 * m_Doctrine.m_fFobTarget;
     float frontDist = float.MAX;
     foreach (ACR_Base ownBase : ACR_Base.s_All)
     {
      if (ownBase && !ownBase.m_bRetired && ownBase.m_sOwner == m_sFaction)
      {
       float d = vector.DistanceXZ(base.GetOrigin(), ownBase.GetOrigin());
       if (d < frontDist) frontDist = d;
      }
     }
     if (frontDist < 1200) score *= 0.6;
    }
    int at = 0;
    while (at < scores.Count() && scores[at] <= score) at++;
    candidates.InsertAt(base, at);
    scores.InsertAt(score, at);
   }
  }
  foreach (ACR_Base candidate : candidates)
  {
   float need = 1;
   if (!candidate.m_sOwner.IsEmpty()) need = AttackNeed(candidate);
   if (counterDue) need = need * m_Doctrine.m_fCounterNeed; // strike before they dig in
   need = need * pressure;
   if (ACR_Manager.ThreatOption() == 2) need = need * 0.85; // High: launch a viable force sooner
   // Only the squads that can get there count (#27): a target beyond their reach is skipped.
   int reachable = ReachableStrength(free, candidate.GetOrigin());
   if (!wanted) { wanted = candidate; wantedRequired = need; wantedReach = reachable; }
   if (reachable >= need) { target = candidate; required = need; break; }
  }
  if (!target)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   if (wanted && now >= m_fNextWeakLog)
   {
    m_fNextWeakLog = now + 60000;
    Say(string.Format("[ACR] AI %1: not strong enough for any target (best: %2, %3/%4 in reach of %5 free, pressure x%6)", m_sFaction, wanted.m_sName, wantedReach, (int)wantedRequired, available, pressure));
   }
   return; // Requisition() will add troops
  }
  SortByDistance(free, target.GetOrigin());
  from = PositionOf(free[0]);
  if (target.m_bBuilt)
  {
   for (int s = free.Count() - 1; s >= 0; s--)
   {
    if (ACR_SapperDemo.IsSapperSquad(free[s]))
    {
     ACR_Squad sapperSquad = free[s];
     free.RemoveOrdered(s);
     free.InsertAt(sapperSquad, 0);
     break;
    }
   }
  }
  ACR_AIAttack attack = new ACR_AIAttack();
  attack.m_Target = target;
  attack.m_bCounter = counterDue;
  if (target.m_bBuilt)
  {
   attack.m_bFobRaid = true;
   attack.m_SapperDemo = new ACR_SapperDemo(m_Manager, this, attack);
   Say(string.Format("[ACR] AI %1: targeted enemy forward base %2 for sapper demolition", m_sFaction, target.m_sName));
  }
  int strength;
  // With troops to spare, send a margin above the bare minimum.
  float margin = 1;
  if (free.Count() > 4) margin = 1 + Math.Min(0.5, (free.Count() - 4) * 0.05);
  // Commit the force together (Jacob 20 Oct, Etoupe: two squads went in and six followed in
  // waves): past the bare minimum, every fighting squad in reach joins up to the strength the
  // attack would otherwise be reinforced to.
  float goal = Math.Max(required * margin, AttackNeed(target) * 1.15);
  if (target.m_sOwner.IsEmpty()) goal = required * margin;
  int remainingPool = available;
  array<ACR_Squad> taxis = {};
  bool liftTaken = false;
  foreach (ACR_Squad squad : free)
  {
   int sqStrength = squad.Strength();
   remainingPool -= sqStrength;
   // Recon teams watch from overwatch; they never go in, so they don't make up an attack.
   if (!target.m_sOwner.IsEmpty() && IsReconTeam(squad)) continue;
   if (squad.StrengthRatio() < 0.55 && strength + remainingPool >= required) continue;
   float distToTarget = vector.DistanceXZ(PositionOf(squad), target.GetOrigin());
   if (strength >= required * margin)
   {
    if (strength >= goal) break;
    float reach = REINFORCE_ATTACK_DISTANCE;
    if (squad.IsMounted()) reach = REINFORCE_ATTACK_MOUNTED;
    if (distToTarget > reach || IsSupportTeam(squad)) continue;
   }
   // A squad that can't get there stays out (no taxi to be had, no air lift); one that can sets
   // how long the forming up waits.
   float travel;
   int how = Reach(squad, target.GetOrigin(), taxis, liftTaken, travel);
   if (how == REACH_NONE) continue;
   if (how == REACH_AIR_LIFT) liftTaken = true;
   attack.m_fTravelSeconds = Math.Max(attack.m_fTravelSeconds, travel);
   attack.m_Squads.Insert(squad);
   strength += sqStrength;
  }
  if (strength < required)
  {
   Say(string.Format("[ACR] AI %1: not strong enough for %2 (%3/%4)", m_sFaction, target.m_sName, strength, required));
   return; // Requisition() will add troops
  }
  if (!target.m_sOwner.IsEmpty() && TakersIn(attack) == 0)
  {
   float noTakersAt = GetGame().GetWorld().GetWorldTime();
   if (noTakersAt >= m_fNextWeakLog)
   {
    m_fNextWeakLog = noTakersAt + 60000;
    Say(string.Format("[ACR] AI %1: not attacking %2: no infantry in reach to take it (only crews, recon or support teams)", m_sFaction, target.m_sName));
   }
   return;
  }
  LaunchAttack(attack, from, strength, required, pressure);
 }

 // Sends the chosen squads off: staging points (pincers), base of fire, orders.
 protected void LaunchAttack(ACR_AIAttack attack, vector from, int strength, float required, float pressure)
 {
  ACR_Base target = attack.m_Target;
  attack.m_iStartStrength = strength;
  // Staging: the main approach from the attackers' side; with several squads
  // they spread out and come in from different angles (pincers).
  vector dir = from - target.GetOrigin();
  dir[1] = 0;
  dir.Normalize();
  attack.m_vApproach = dir;
  float ring = target.m_fRadius + STAGING_DISTANCE;
  attack.m_Staging = ACR_Manager.Ground(target.GetOrigin() + dir * ring);
  // Staging points on dry land with a dry run in (no forming up in a bay).
  vector dryMain;
  if (StagingPoint(target, dir, ring, dryMain)) attack.m_Staging = dryMain;
  attack.m_aStaging.Clear();
  attack.m_aSlot.Clear();
  // Assign foot squads first so they take the direct lines (0, +-60) and mounted units swing wide (+-115).
  for (int pass = 0; pass < 2; pass++)
  {
   foreach (int idx, ACR_Squad member : attack.m_Squads)
   {
    bool mounted = member.IsMounted() || ACR_Transport.CrewedVehicle(member) != null;
    if ((pass == 0 && mounted) || (pass == 1 && !mounted)) continue;
    while (attack.m_aStaging.Count() <= idx)
    {
     attack.m_aStaging.Insert(attack.m_Staging);
     attack.m_aSlot.Insert(-1);
    }
    vector stage;
    int slot = PickApproach(attack, PositionOf(member), stage);
    attack.m_aStaging[idx] = stage;
    attack.m_aSlot[idx] = slot;
   }
  }
  bool empty = target.m_sOwner.IsEmpty();
  // With two or more squads on a held base, one becomes the base of fire
  // while the others manoeuvre.
  // Picked by firepower (a gun vehicle or the biggest rifle squad), never a support team.
  if (!empty && attack.m_Squads.Count() >= 2)
  {
   int bestFire = 2;
   foreach (ACR_Squad fireCandidate : attack.m_Squads)
   {
    if (ACR_SapperDemo.IsSapperSquad(fireCandidate) || IsSupportTeam(fireCandidate)) continue;
    int firepower = fireCandidate.FirePower();
    if (firepower > bestFire) { bestFire = firepower; attack.m_Suppressor = fireCandidate; }
   }
  }
  attack.m_fStageDeadline = GetGame().GetWorld().GetWorldTime() + StageSeconds(attack.m_fTravelSeconds) * 1000;
  attack.m_fStageLimit = attack.m_fStageDeadline + STAGING_TIMEOUT * STAGE_STRETCH * 1000;
  // Motorized assault: an armed carrier (APC) in the attack takes an infantry squad to the
  // staging point and stays there with it as base of fire.
  array<ACR_Squad> carried = {};
  if (!empty && m_Manager.m_Rides) PairCarriers(attack, carried);
  float plannedDeadline = attack.m_fStageDeadline;
  foreach (ACR_Squad squad : attack.m_Squads)
  {
   squad.m_iAITask = ACR_EAITask.ATTACK;
   squad.m_AITarget = target;
   if (carried.Contains(squad)) continue; // the ride has its orders
   squad.SetMode(ACR_EMoveMode.FAST);
   // Nobody to fight at an empty base: go straight in.
   if (empty) Travel(squad, target.GetOrigin(), false, 100);
   else
   {
    Travel(squad, attack.StagingOf(squad), false, 0, target);
    // The travel estimate was the straight way: a covered way round adds its extra length.
    if (m_fCoverDetour > 0) attack.m_fStageDeadline = Math.Max(attack.m_fStageDeadline, Math.Min(attack.m_fStageLimit, plannedDeadline + m_fCoverDetour / COVER_WALK_SPEED * 1000));
   }
  }
  attack.m_bAssaulting = empty;
  m_Attacks.Insert(attack);
  m_fLastOffensive = GetGame().GetWorld().GetWorldTime();
  if (pressure < 1) Say(string.Format("[ACR] AI %1: taking the initiative against %2 (accepting x%3 odds after a quiet spell)", m_sFaction, target.m_sName, pressure));
  string kind = "attacking";
  if (attack.m_bCounter) { kind = "COUNTERATTACKING"; m_CounterTarget = null; }
  Say(string.Format("[ACR] AI %1: %2 %3 with %4 squad(s), strength %5/%6", m_sFaction, kind, target.m_sName, attack.m_Squads.Count(), strength, required));
  if (attack.m_bCounter) TwoICReport(string.Format("Counterattacking %1 with %2", target.m_sName, SquadList(attack.m_Squads)));
  else TwoICReport(string.Format("Attacking %1 with %2", target.m_sName, SquadList(attack.m_Squads)));
  // "Take this base" (a player's request to the 2IC): its attack is out.
  if (m_RequestedTarget && target == m_RequestedTarget)
  {
   m_RequestedTarget = null;
   ACR_TwoICRequest asked = ACR_TwoICRequests.Find(m_iRequestedBy);
   if (asked) asked.Set(ACR_ETwoICState.COMING, string.Format("attacking %1 with %2", target.m_sName, SquadList(attack.m_Squads)));
   PrintFormat("[ACR] 2IC %1: attacking %2 as asked (request %3)", m_sFaction, target.m_sName, m_iRequestedBy);
  }
  if (empty) return;
  string stagingAt;
  foreach (ACR_Squad staged : attack.m_Squads)
   stagingAt += string.Format(" %1 grid %2,", staged.GetName(), ACR_Manager.Grid(attack.StagingOf(staged)));
  Say(string.Format("[ACR] AI %1: staging for %2 at%3", m_sFaction, target.m_sName, stagingAt));
 }

 // Each armed carrier with room (gun manned, 4+ free seats) picks the nearest infantry squad of the
 // attack that fits and is within 400 m, and rides it to that squad's staging point (5 Oct;
 // Jacob: APCs for motorized assaults). The carrier then forms up there instead of its own point.
 static float CARRIER_PICKUP = 400;
 static int s_iCarrierTries; // tests
 protected void PairCarriers(ACR_AIAttack attack, array<ACR_Squad> carried)
 {
  foreach (int c, ACR_Squad carrier : attack.m_Squads)
  {
   IEntity vehicle = ACR_Transport.CrewedVehicle(carrier);
   if (!vehicle || carried.Contains(carrier) || !carrier.HasGunner(vehicle)) continue;
   int seats = ACR_Rides.FreeSeats(vehicle);
   if (seats < 4) continue;
   ACR_Squad best;
   float bestDistance = CARRIER_PICKUP;
   foreach (ACR_Squad rider : attack.m_Squads)
   {
    if (rider == carrier || carried.Contains(rider) || rider.IsMounted() || ACR_Transport.CrewedVehicle(rider)) continue;
    if (rider.CountAlive() > seats) continue;
    float d = vector.DistanceXZ(PositionOf(rider), vehicle.GetOrigin());
    if (d < bestDistance) { bestDistance = d; best = rider; }
   }
   if (!best) continue;
   vector stage = attack.StagingOf(best);
   s_iCarrierTries++;
   string why;
   if (!m_Manager.m_Rides.RequestCarrier(best, carrier, stage, ACR_Squad.WP_MOVE, -1, "Moving", 0, why))
   {
    Say(string.Format("[ACR] AI %1: %2 can't carry %3 (%4)", m_sFaction, carrier.GetName(), best.GetName(), why));
    continue;
   }
   if (c < attack.m_aStaging.Count()) attack.m_aStaging[c] = stage;
   int riderAt = attack.m_Squads.Find(best);
   if (c < attack.m_aSlot.Count() && riderAt >= 0 && riderAt < attack.m_aSlot.Count()) attack.m_aSlot[c] = attack.m_aSlot[riderAt];
   carried.Insert(best);
   carried.Insert(carrier);
   Say(string.Format("[ACR] AI %1: %2 carries %3 to the staging point for %4", m_sFaction, carrier.GetName(), best.GetName(), attack.m_Target.m_sName));
  }
 }

 // Debug menu: attack now with every free squad, ignoring odds, attack limits and
 // cooldowns. target null = the nearest enemy-held base (an empty one if none).
 string ForceAttack(ACR_Base target)
 {
  if (!m_bActive) return string.Format("%1 AI commander isn't in command (a player commands it, or it's held or off)", m_sFaction);
  m_RetreatingUntil.Clear();
  array<ACR_Squad> free = {};
  MySquads(free, true);
  if (free.IsEmpty()) return string.Format("%1 AI has no free squads to attack with", m_sFaction);
  vector from = PositionOf(free[0]);
  if (!target)
  {
   float best = float.MAX;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner == m_sFaction) continue;
    float d = vector.DistanceXZ(base.GetOrigin(), from);
    if (base.m_sOwner.IsEmpty()) d += 1000000; // enemy bases first
    if (d < best) { best = d; target = base; }
   }
  }
  if (!target) return "No base to attack";
  if (target.m_sOwner == m_sFaction) return string.Format("%1 already holds %2", m_sFaction, target.m_sName);
  m_StalledUntil.Remove(target);
  SortByDistance(free, target.GetOrigin());
  ACR_AIAttack attack = new ACR_AIAttack();
  attack.m_Target = target;
  int strength;
  foreach (ACR_Squad squad : free)
  {
   attack.m_Squads.Insert(squad);
   strength += squad.Strength();
  }
  Say(string.Format("[ACR] AI %1: forced attack (debug menu)", m_sFaction));
  LaunchAttack(attack, from, strength, strength, 1);
  return string.Format("%1 AI attacking %2 with %3 squad(s)", m_sFaction, target.m_sName, free.Count());
 }

 // Where the reserve waits: an own base that is safe (not contested, no known threat, at
 // least RESERVE_MIN_FRONT from every enemy base we know of), bigger first (value, radius),
 // not much farther back than RESERVE_IDEAL_FRONT, and not an HQ (IsHQ). The current
 // reserve base gets a bonus so the reserve doesn't shuttle between two near-equal bases.
 // Falls back to HomeBase (the HQ) when nothing else qualifies.
 static const float RESERVE_MIN_FRONT = 600;
 static const float RESERVE_IDEAL_FRONT = 1200;
 static const float RESERVE_MAX_PULL = 1800;
 static const int SAVE_MIN_UNITS = 4;            // save RP for a pricier unit only with this many fighting units out
 static const float FOB_DEPLOY_MIN_FRONT = 500;  // m: a built FOB closer to an enemy/neutral base than this isn't a deploy point
 static const float REINFORCE_RETRY_MS = 120000; // a squad whose AI reinforce failed waits this long before another try
 static const float POCKET_IN_THE_WAY = 1500; // m: a found FIA pocket this close to one of our bases is a target
 // (A pocket's attack score is multiplied by the doctrine's m_fPocketScore, 2.5 standard; idle
 // squads below the doctrine's m_fRefitRatio, 0.5 standard, walk back to a quiet base to refill.)
 static const float REFIT_GIVE_UP_MS = 900000;   // a walk back to refill that hasn't arrived by then is dropped
 protected ACR_Base m_ReserveBase;
 protected ACR_Squad m_ReserveSquad;

 bool ReserveFits(ACR_Base base)
 {
  if (!base || base.m_sOwner != m_sFaction || IsHQ(base) || base.m_bContested || ThreatTo(base) > 0) return false;
  return FrontDistance(base) >= RESERVE_MIN_FRONT;
 }

 // Distance to the nearest enemy-held base we know of (any other base if none is known).
 float FrontDistance(ACR_Base base)
 {
  float enemy = float.MAX;
  float other = float.MAX;
  foreach (ACR_Base candidate : ACR_Base.s_All)
  {
   if (candidate.m_sOwner == m_sFaction || !Visible(candidate)) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), candidate.GetOrigin());
   if (!candidate.m_sOwner.IsEmpty()) enemy = Math.Min(enemy, d);
   other = Math.Min(other, d);
  }
  if (enemy < float.MAX) return enemy;
  return other;
 }

 // An HQ = a base with one of this side's player spawn points (most bases can deploy bought
 // units, so m_bAllowSpawn doesn't tell; the skirmish layout moves the spawn points to its HQs).
 bool IsHQ(ACR_Base base)
 {
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
   if (point && !ACR_SpawnPoints.IsBasePoint(point) && point.GetFactionKey() == m_sFaction && vector.DistanceXZ(point.GetOrigin(), base.GetOrigin()) <= base.m_fRadius + 60) return true;
  return false;
 }

 ACR_Base ReserveBase()
 {
  ACR_Base best;
  float bestScore = -float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!ReserveFits(base)) continue;
   float front = FrontDistance(base);
   float score = base.m_iValue * 150 + base.m_fRadius - Math.Max(0, front - RESERVE_IDEAL_FRONT) * 0.25;
   if (base == m_ReserveBase) score += 250;
   if (score > bestScore) { bestScore = score; best = base; }
  }
  if (!best) best = HomeBase();
  m_ReserveBase = best;
  return best;
 }

 // The own spawn base furthest from the fighting.
 protected ACR_Base HomeBase()
 {
  ACR_Base home;
  float best = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || !base.m_bAllowSpawn) continue;
   float nearestFront = 100000;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction) nearestFront = Math.Min(nearestFront, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (nearestFront > best) { best = nearestFront; home = base; }
  }
  return home;
 }

 protected ACR_Base NearestOwnBase(vector point, ACR_Base except = null)
 {
  ACR_Base nearest;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || base == except) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), point);
   if (d < best) { best = d; nearest = base; }
  }
  return nearest;
 }

 protected float NearestOwnDistance(vector point)
 {
  ACR_Base nearest = NearestOwnBase(point);
  if (!nearest) return float.MAX;
  return vector.DistanceXZ(nearest.GetOrigin(), point);
 }

 static bool IsServiceVehicle(ACR_RequisitionEntry entry)
 {
  if (entry.m_sVehicle.IsEmpty()) return false;
  string text = entry.m_sName + " " + entry.m_sVehicle;
  text.ToLower();
  array<string> words = {"fuel", "repair", "ammo", "supply", "medical", "ambulance", "command", "radio", "tanker"};
  foreach (string word : words)
   if (text.Contains(word)) return true;
  return false;
 }

 // Long trips on foot become rides or air lifts:
 // Air lift (aca-f22 part 2): long moves (~2 km+ or long road detours) when the flight path is clear of known enemies.
 // (How far on foot before a ride, and before an air lift: the doctrine's m_fRideDistance and
 // m_fAirLiftDistance, 900 and 2000 m standard.)
 static const float AIRLIFT_RETRY_MS = 60000;   // after a refused AI air lift
 protected float m_fNextAirLiftTry;
 static const float AIRLIFT_ENEMY_CLEARANCE = 350;

 static float DistanceToSegment(vector p, vector a, vector b)
 {
  vector ab = b - a;
  ab[1] = 0;
  float l2 = ab[0] * ab[0] + ab[2] * ab[2];
  if (l2 < 1.0) return vector.DistanceXZ(p, a);
  vector ap = p - a;
  ap[1] = 0;
  float t = Math.Clamp((ap[0] * ab[0] + ap[2] * ab[2]) / l2, 0.0, 1.0);
  vector closest = Vector(a[0] + ab[0] * t, 0, a[2] + ab[2] * t);
  return vector.DistanceXZ(p, closest);
 }

 bool FlightPathClear(vector from, vector to)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (IEntity enemy, vector seenPos : m_SeenAt)
  {
   if (!enemy || !ACR_Respawn.IsAlive(enemy)) continue;
   float seenTime;
   if (!m_SeenTime.Find(enemy, seenTime) || (now - seenTime) > INTEL_MEMORY_MS) continue;
   if (DistanceToSegment(seenPos, from, to) < AIRLIFT_ENEMY_CLEARANCE) return false;
  }
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == m_sFaction) continue;
   vector known;
   if (!KnownPosition(squad, known)) continue;
   if (DistanceToSegment(known, from, to) < AIRLIFT_ENEMY_CLEARANCE) return false;
  }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == m_sFaction || base.m_sOwner.IsEmpty() || !Visible(base)) continue;
   float dDrop = vector.DistanceXZ(to, base.GetOrigin());
   if (dDrop < base.m_fRadius + 150) return false;
   float dClosest = DistanceToSegment(base.GetOrigin(), from, to);
   if (dClosest < base.m_fRadius + AIRLIFT_ENEMY_CLEARANCE && dClosest < dDrop - 20) return false;
  }
  return true;
 }

 bool TryAirLift(ACR_Squad squad, vector destination, ResourceName prefab, float radius, string order, int kind, float dropShort = 0)
 {
  if (!m_Manager || !m_Manager.m_Supports) return false;
  if (!squad || !squad.m_Group || squad.HasPlayer()) return false;
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad) || squad.m_bMortar || squad.Aircraft()) return false;
  if (ACR_AirLift.Busy(squad)) return false;
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return false;
  vector from = anchor.GetOrigin();
  if (!ACR_Manager.OnMap(destination)) return false;

  float distance = vector.DistanceXZ(from, destination);
  if (distance < ACR_AirLift.MIN_DISTANCE) return false;

  // Cheap checks before the road route and landing zone searches (Jacob 18 Oct: freezes).
  int supportIdx = SupportIndex(ACR_ESupportType.AIR_LIFT);
  if (supportIdx < 0) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data) return false;
  ACR_SupportEntry entry = data.m_aSupports[supportIdx];
  if (!entry || !entry.m_bEnabled) return false;
  float now = GetGame().GetWorld().GetWorldTime();
  bool waveSlot = ACR_AirLift.WaveSlotAvailable(m_sFaction, now);
  if (now < entry.m_fReadyAt && !waveSlot) return false;
  if (now < m_fNextAirLiftTry && !waveSlot) return false;
  int cost = entry.CostFor(true);
  if (data.m_iRP < cost + SUPPORT_RESERVE) return false;
  if (squad.KnownEnemies(300) > 0) return false;

  // AI priority gate: when known enemy bases exist, preserve air lifts for combat attacks
  // and threatened base defense rather than idle squad shuffling or cheap grabs.
  if (squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED)
  {
   if (HasKnownEnemyBase())
   {
    if (FriendlyBaseThreatened() || !m_Attacks.IsEmpty() || data.m_iRP < cost * 2 + SUPPORT_RESERVE) return false;
   }
  }

  bool isLong = distance >= m_Doctrine.m_fAirLiftDistance;
  bool isDetour = false;
  if (!isLong && distance >= m_Doctrine.m_fRideDistance)
  {
   if (BBB_Terrain.WaterAlong(from, destination) > BBB_Terrain.WET_ALLOWED)
   {
    isDetour = true;
   }
   else
   {
    array<vector> roadLegs = {};
    int roadNodes, roadMillis;
    if (!BBB_Roads.Route(from, destination, roadLegs, roadNodes, roadMillis) || BBB_Roads.s_bLastDetour)
    {
     isDetour = true;
    }
    else if (BBB_Roads.s_fLastLength > distance * 1.6)
    {
     isDetour = true;
    }
   }
  }
  if (!isLong && !isDetour) return false;

  vector targetLZ = destination;
  if (dropShort > 0)
  {
   vector dir = destination - from;
   dir[1] = 0;
   if (dir.Length() > 1)
   {
    dir.Normalize();
    targetLZ = destination - dir * Math.Min(dropShort, distance * 0.5);
   }
  }

  if (!FlightPathClear(from, targetLZ)) return false;

  string result = m_Manager.m_Supports.Call(m_sFaction, supportIdx, targetLZ, true, -1, squad, prefab, destination, radius, order, kind);
  if (!ACR_AirLift.Busy(squad))
  {
   // Refused (usually no landing spot): don't search again for every other long move.
   m_fNextAirLiftTry = now + AIRLIFT_RETRY_MS;
   Say(string.Format("[ACR] AI %1: no air lift for %2 (%3), next try in %4 s", m_sFaction, squad.GetName(), result, (int)(AIRLIFT_RETRY_MS / 1000)));
   return false;
  }
  Say(string.Format("[ACR] AI %1: air lift for %2 to %3 (%4 m): %5", m_sFaction, squad.GetName(), ACR_Manager.Grid(destination), (int)distance, result));
  return true;
 }

 // 'approach': the enemy base this is a staging move for, to walk a covered way there on foot.
 protected void Travel(ACR_Squad squad, vector destination, bool defend, float dropShort, ACR_Base approach = null)
 {
  m_fCoverDetour = 0;
  // Defending a base: infantry garrison its buildings (the one nearest the centre,
  // spilling into its neighbours); vehicle crews, and bases without buildings, defend.
  bool garrison;
  vector building;
  if (defend && !ACR_Transport.CrewedVehicle(squad) && ACR_Garrison.AnchorNear(destination, 80, building))
  {
   garrison = true;
   destination = building;
  }
  ResourceName prefab = ACR_Squad.WP_MOVE;
  float radius = -1;
  string order = "Moving";
  int kind = 0;
  if (defend)
  {
   prefab = ACR_Squad.WP_DEFEND;
   radius = 15;
   order = "Defending";
   kind = 1;
  }
  if (garrison)
  {
   prefab = ACR_Squad.WP_GARRISON;
   order = "Garrisoning";
  }
  if (!squad.IsMounted())
  {
   if (TryAirLift(squad, destination, prefab, radius, order, kind, dropShort)) return;
   if (m_Manager.m_Rides && vector.DistanceXZ(PositionOf(squad), destination) > m_Doctrine.m_fRideDistance)
   {
    string why;
    if (m_Manager.m_Rides.Request(squad, destination, dropShort, prefab, radius, order, kind, why, true)) return;
   }
  }
  if (approach && !defend && !squad.IsMounted() && !ACR_Transport.CrewedVehicle(squad) && CoveredApproach(squad, approach, destination)) return;
  // On foot (or its own vehicle): round any water rather than across it.
  if (garrison) squad.RouteTo(destination, ACR_Squad.WP_GARRISON, 15, "Garrisoning", 1);
  else if (defend) squad.RouteTo(destination, ACR_Squad.WP_DEFEND, 15, "Defending", 1);
  else squad.RouteTo(destination, ACR_Squad.WP_MOVE, -1, "Moving", 0);
 }

 // A beaten squad breaks contact and falls back to the nearest own base.
 protected void FallBack(ACR_Squad squad, string why, ACR_Base except = null)
 {
  ACR_Base refuge = NearestOwnBase(PositionOf(squad), except);
  squad.m_iAITask = ACR_EAITask.FREE;
  if (m_ReserveSquad == squad) m_ReserveSquad = null;
  m_RetreatingUntil.Set(squad, GetGame().GetWorld().GetWorldTime() + RETREAT_COOLDOWN_MS);
  squad.SetMode(ACR_EMoveMode.FAST);
  // On foot, round any water; no waiting for a truck under fire.
  if (refuge) squad.RouteTo(ACR_Manager.Ground(refuge.GetOrigin()), ACR_Squad.WP_DEFEND, 15, "Falling back", 1);
  string where = "its position";
  if (refuge) where = refuge.m_sName;
  Say(string.Format("[ACR] AI %1: %2 falls back to %3 (%4)", m_sFaction, squad.GetName(), where, why));
 }

 protected bool IsAttacking(ACR_Base base)
 {
  foreach (ACR_AIAttack attack : m_Attacks)
   if (attack.m_Target == base) return true;
  return false;
 }

 // Base of fire keeps covering until the assault squads are on the objective, then lifts
 // fire and joins in.
 void TickFireBase(ACR_AIAttack attack)
 {
  ACR_Base target = attack.m_Target;
  if (!attack.m_Suppressor || !target) return;
  bool closeIn = false;
  foreach (ACR_Squad assaulter : attack.m_Squads)
   if (assaulter && assaulter != attack.m_Suppressor && vector.DistanceXZ(PositionOf(assaulter), target.GetOrigin()) < target.m_fRadius + 60) closeIn = true;
  if (closeIn) LiftFire(attack, "assault on the objective");
  else UpdateFireBase(attack);
 }

 // Base of fire (Jacob, 20 Oct: a squad stood suppressing an empty base centre for 10 min).
 // It fires only at an enemy this side has seen at the objective (intel, 90 s memory), from
 // within FIRE_BASE_RANGE with clear terrain between. Too far or blind: it moves up to a
 // spot short of the enemy. With nobody seen there yet it holds an overwatch position facing
 // the objective (in range, clear fire line) and engages only what its own men see (Jacob
 // 20 Oct, Everon: 18 of 27 bases of fire lifted "no enemy seen" the moment the assault
 // started). It joins the assault when the assault reaches the objective, nobody is left to
 // cover, it can't reach a firing spot in time, or after the doctrine's m_fFireBaseSeconds.
 protected void UpdateFireBase(ACR_AIAttack attack)
 {
  ACR_Squad fire = attack.m_Suppressor;
  ACR_Base target = attack.m_Target;
  if (!fire || !target) return;
  float now = GetGame().GetWorld().GetWorldTime();
  bool covering = false;
  foreach (ACR_Squad other : attack.m_Squads)
   if (other && other != fire && other.Strength() > 0) covering = true;
  if (!covering) { LiftFire(attack, "nobody left to cover"); return; }
  if (attack.m_fAssaultStart > 0 && now - attack.m_fAssaultStart > m_Doctrine.m_fFireBaseSeconds * 1000) { LiftFire(attack, "time up"); return; }
  vector seenAt;
  ACR_Squad enemy = EnemyNear(target.GetOrigin(), target.m_fRadius + 40, seenAt);
  if (!enemy) { Overwatch(attack); return; }
  vector from = PositionOf(fire);
  float distance = vector.DistanceXZ(from, seenAt);
  if (distance <= FIRE_BASE_RANGE && FireLine(from, seenAt))
  {
   attack.m_fFireBaseMoveSince = 0;
   if (fire.OrderState() == ACR_EOrderState.SUPPRESS && vector.DistanceXZ(fire.m_MoveTarget, seenAt) < 10) return;
   fire.Suppress(seenAt);
   Say(string.Format("[ACR] AI %1: %2 (base of fire) suppresses the enemy seen at %3 (%4 m)", m_sFaction, fire.GetName(), target.m_sName, Math.Round(distance)));
   return;
  }
  float stand = Math.Min(FIRE_BASE_STANDOFF, distance - 100);
  if (stand < 120) { LiftFire(attack, "no firing spot"); return; }
  if (attack.m_fFireBaseMoveSince <= 0) attack.m_fFireBaseMoveSince = now;
  if (now - attack.m_fFireBaseMoveSince > FIRE_BASE_MOVE_S * 1000) { LiftFire(attack, "no firing spot found in time"); return; }
  vector back = from - seenAt;
  back[1] = 0;
  back.Normalize();
  vector spot = seenAt + back * stand;
  spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
  if (fire.OrderState() == ACR_EOrderState.MOVE && vector.DistanceXZ(fire.m_MoveTarget, spot) < 40) return;
  if (vector.DistanceXZ(from, spot) > 80 && fire.KnownEnemies(250) == 0) fire.SetMode(ACR_EMoveMode.FAST);
  else fire.SetMode(ACR_EMoveMode.COMBAT);
  fire.MoveTo(spot);
  Say(string.Format("[ACR] AI %1: %2 (base of fire) moves up to %3 m from the enemy at %4: %5", m_sFaction, fire.GetName(), Math.Round(stand), target.m_sName, spot));
 }

 // No enemy seen at the objective: hold (Defend) an overwatch spot facing it, so the squad
 // shoots only at what it sees itself. Suppression of a spot nobody is seen at any more ends.
 protected void Overwatch(ACR_AIAttack attack)
 {
  ACR_Squad fire = attack.m_Suppressor;
  ACR_Base target = attack.m_Target;
  float now = GetGame().GetWorld().GetWorldTime();
  if (!attack.m_bOverwatch)
  {
   vector found;
   if (!OverwatchSpot(target, PositionOf(fire), found)) { LiftFire(attack, "no firing spot"); return; }
   attack.m_vOverwatch = found;
   attack.m_bOverwatch = true;
   attack.m_fFireBaseMoveSince = 0;
  }
  else if (fire.OrderState() != ACR_EOrderState.DEFEND)
  {
   // Back from suppressing or moving up: where it stands will do if it still has a fire line.
   vector here = PositionOf(fire);
   float range = vector.DistanceXZ(here, target.GetOrigin());
   if (range >= target.m_fRadius + OVERWATCH_MIN_GAP && range <= FIRE_BASE_RANGE && FireLine(here, target.GetOrigin())) attack.m_vOverwatch = here;
  }
  vector spot = attack.m_vOverwatch;
  float away = vector.DistanceXZ(PositionOf(fire), spot);
  if (away > OVERWATCH_REACHED)
  {
   if (attack.m_fFireBaseMoveSince <= 0) attack.m_fFireBaseMoveSince = now;
   if (now - attack.m_fFireBaseMoveSince > FIRE_BASE_MOVE_S * 1000) { LiftFire(attack, "no firing spot found in time"); return; }
  }
  else attack.m_fFireBaseMoveSince = 0;
  if (fire.OrderState() == ACR_EOrderState.DEFEND && vector.DistanceXZ(fire.m_MoveTarget, spot) < 20) return;
  fire.SetMode(ACR_EMoveMode.COMBAT);
  fire.Defend(spot);
  Say(string.Format("[ACR] AI %1: %2 (base of fire) holds overwatch on %3 from %4 m (no enemy seen there yet): %5", m_sFaction, fire.GetName(), target.m_sName, Math.Round(vector.DistanceXZ(spot, target.GetOrigin())), spot));
 }

 // A spot within FIRE_BASE_RANGE of the objective with a clear fire line to it: where the
 // squad stands if that will do, otherwise on rings round the objective on the squad's side.
 static const float OVERWATCH_REACHED = 40;
 static const float OVERWATCH_MIN_GAP = 90;   // m outside the base ring (not part of the assault)
 bool OverwatchSpot(ACR_Base target, vector from, out vector spot)
 {
  if (!target) return false;
  vector center = target.GetOrigin();
  float minRange = target.m_fRadius + OVERWATCH_MIN_GAP;
  float here = vector.DistanceXZ(from, center);
  if (here >= minRange && here <= FIRE_BASE_RANGE && !BBB_Terrain.IsWaterAt(from) && FireLine(from, center))
  {
   spot = from;
   return true;
  }
  vector side = from - center;
  side[1] = 0;
  if (side.LengthSq() < 1) side = Vector(0, 0, 1);
  side.Normalize();
  // The nearest one to the squad (it is walking there while the assault goes in).
  array<float> rings = {200, 250, 300, 350, 400};
  array<float> turns = {0, 10, -10, 20, -20, 30, -30, 45, -45, 60, -60};
  float best = float.MAX;
  foreach (float ring : rings)
  {
   float d = Math.Clamp(ring, minRange, FIRE_BASE_RANGE - 20);
   foreach (float turn : turns)
   {
    vector candidate = ACR_Manager.Ground(center + Turned(side, turn) * d);
    float walk = vector.DistanceXZ(candidate, from);
    if (BBB_Terrain.IsWaterAt(candidate) || NearOtherEnemyBase(candidate, target)) continue;
    if (!FireLine(candidate, center)) continue;
    float heightAdvantage = candidate[1] - center[1];
    float score = walk - Math.Max(0.0, heightAdvantage * 2.5);
    if (score < best)
    {
     spot = candidate;
     best = score;
    }
   }
  }
  return best < float.MAX;
 }

 protected void LiftFire(ACR_AIAttack attack, string reason)
 {
  ACR_Squad fire = attack.m_Suppressor;
  if (!fire || !attack.m_Target) return;
  fire.SetMode(ACR_EMoveMode.COMBAT);
  IEntity vFire = ACR_Transport.CrewedVehicle(fire);
  if (vFire && fire.HasGunner(vFire) && reason != "nobody left to cover")
  {
   vector dir = PositionOf(fire) - attack.m_Target.GetOrigin();
   dir[1] = 0;
   if (dir.LengthSq() < 1) dir = Vector(0, 0, 1);
   dir.Normalize();
   vector perimeterSpot = ACR_Manager.Ground(attack.m_Target.GetOrigin() + dir * (attack.m_Target.m_fRadius + 80));
   fire.Defend(perimeterSpot);
   Say(string.Format("[ACR] AI %1: %2 (armed vehicle) holds perimeter standoff at %3 (%4)", m_sFaction, fire.GetName(), attack.m_Target.m_sName, reason));
  }
  else
  {
   fire.Assault(attack.m_Target.GetOrigin());
   Say(string.Format("[ACR] AI %1: %2 lifts fire and joins the assault on %3 (%4)", m_sFaction, fire.GetName(), attack.m_Target.m_sName, reason));
  }
  attack.m_Suppressor = null;
 }

 // Eye height to just above the aim point with only terrain in the way (trees and
 // houses are what suppressive fire is for). The last 30 m don't count: the enemy
 // may sit in a dip or behind a crest at the objective itself.
 static bool FireLine(vector from, vector aim)
 {
  BaseWorld world = GetGame().GetWorld();
  if (!world) return false;
  TraceParam trace = new TraceParam();
  trace.Start = from + Vector(0, 1.6, 0);
  trace.End = aim + Vector(0, 1.5, 0);
  trace.Flags = TraceFlags.WORLD;
  float fraction = world.TraceMove(trace, null);
  float length = vector.Distance(trace.Start, trace.End);
  return fraction >= 1 || (1 - fraction) * length <= 30;
 }

 // True when an assault or grab has gone on too long without taking the base.
 protected bool Stalled(ACR_AIAttack attack)
 {
  // Every squad left gave up on a spot it can't reach: don't wait out the time limit
  // (Everon night soak 26 Oct: a grab's only squad held "unreachable" for 17 min).
  int live = 0;
  bool allGaveUp = true;
  foreach (ACR_Squad member : attack.m_Squads)
  {
   if (!member || member.Strength() <= 0) continue;
   live++;
   if (!member.GaveUpUnreachable()) allGaveUp = false;
  }
  if (live > 0 && allGaveUp)
  {
   Say(string.Format("[ACR] AI %1: every squad on %2 found it unreachable", m_sFaction, attack.m_Target.m_sName));
   return true;
  }
  if (!attack.m_bAssaulting || attack.m_fAssaultStart <= 0) return false;
  float seconds = (GetGame().GetWorld().GetWorldTime() - attack.m_fAssaultStart) / 1000;
  if (attack.m_bGrab && !s_bGrabByDistance) return seconds > GRAB_TIMEOUT;
  if (attack.m_bGrab) return seconds > Math.Max(GRAB_TIMEOUT, attack.m_fGrabSeconds);
  if (seconds > ASSAULT_HARD_TIMEOUT) return true;
  if (seconds > ASSAULT_TIMEOUT * 0.5 && attack.m_Target && !attack.m_Target.m_bContested)
  {
   bool nearPerimeter = false;
   int currentStrength = 0;
   foreach (ACR_Squad member : attack.m_Squads)
   {
    currentStrength += member.Strength();
    if (vector.DistanceXZ(PositionOf(member), attack.m_Target.GetOrigin()) < attack.m_Target.m_fRadius + 100) nearPerimeter = true;
   }
   if (!nearPerimeter && currentStrength < DefenceOf(attack.m_Target)) return true;
  }
  if (seconds <= ASSAULT_TIMEOUT) return false;
  foreach (ACR_Squad squad : attack.m_Squads)
   if (vector.DistanceXZ(PositionOf(squad), attack.m_Target.GetOrigin()) < attack.m_Target.m_fRadius) return false;
  return true;
 }

 // Every couple of minutes, send attackers that stopped short of the base in again.
 protected void PushAttackers(ACR_AIAttack attack)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (attack.m_bGrab || now < attack.m_fNextPush) return;
  // The others fell: the base of fire is the last squad that can take the base, so it goes in
  // instead of covering an assault that isn't coming.
  if (attack.m_Suppressor && TakersIn(attack, attack.m_Suppressor) == 0 && CanTakeBase(attack.m_Suppressor, attack.m_bFobRaid))
  {
   Say(string.Format("[ACR] AI %1: base of fire %2 goes in on %3 (no other squad left to take it)", m_sFaction, attack.m_Suppressor.GetName(), attack.m_Target.m_sName));
   attack.m_Suppressor = null;
   attack.m_fNextPush = 1; // re-issue below now
  }
  if (attack.m_fNextPush > 0)
  {
   foreach (ACR_Squad squad : attack.m_Squads)
   {
    if (squad == attack.m_Suppressor || squad.m_CombatFlank.Active() || ACR_AirLift.Busy(squad)) continue;
    IEntity veh = ACR_Transport.CrewedVehicle(squad);
    if (veh && squad.HasGunner(veh)) continue;
    if (vector.DistanceXZ(PositionOf(squad), attack.m_Target.GetOrigin()) < attack.m_Target.m_fRadius * 0.6) continue;
    squad.SetMode(ACR_EMoveMode.COMBAT);
    squad.Assault(attack.m_Target.GetOrigin());
   }
  }
  attack.m_fNextPush = now + PUSH_INTERVAL * 1000;
 }

 // A line in the server log every 5 minutes: what this commander is doing.
 protected void LogStatus()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextStatus) return;
  m_fNextStatus = now + 300000;
  Print(StatusText());
 }

 // The status line (LogStatus; the AC:A Think view shows it live).
 string StatusText()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_Squad> all = {};
  MySquads(all, false);
  int nFree = 0;
  int nDefend = 0;
  int nAttack = 0;
  int nOther = 0;
  foreach (ACR_Squad squad : all)
  {
   if (squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED) nFree++;
   else if (squad.m_iAITask == ACR_EAITask.DEFEND) nDefend++;
   else if (squad.m_iAITask == ACR_EAITask.ATTACK) nAttack++;
   else nOther++;
  }
  string attacks;
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (!attack.m_Target) continue;
   string phase = "staging";
   if (attack.m_bGrab) phase = "grab";
   else if (attack.m_bAssaulting) phase = "assault";
   int minutes = 0;
   if (attack.m_fAssaultStart > 0) minutes = (now - attack.m_fAssaultStart) / 60000;
   attacks += string.Format(" %1(%2 %3m)", attack.m_Target.m_sName, phase, minutes);
  }
  array<ACR_Squad> ready = {};
  MySquads(ready, true);
  attacks += string.Format(" | combat-ready idle %1", ready.Count());
  map<string, int> idleAt = new map<string, int>();
  foreach (ACR_Squad idle : ready)
  {
   ACR_Base near = NearestOwnBase(PositionOf(idle));
   string where = "field";
   if (near && vector.DistanceXZ(PositionOf(idle), near.GetOrigin()) < near.m_fRadius + 150) where = near.m_sName;
   int had;
   idleAt.Find(where, had);
   idleAt.Set(where, had + 1);
  }
  foreach (string place, int n : idleAt) attacks += string.Format(" %1:%2", place, n);
  // What the rest of the force is: the AI squad cap counts fighting units only.
  int line, depleted, vehiclesNoGunner, remnants;
  foreach (ACR_Squad member : all)
  {
   if (member.IsCombatUnit())
   {
    if (!IsSpecialistSquad(member)) line++;
    if (!ACR_Transport.CrewedVehicle(member) && member.StrengthRatio() < m_Doctrine.m_fRefitRatio) depleted++;
   }
   else if (ACR_Transport.CrewedVehicle(member)) vehiclesNoGunner++;
   else remnants++;
  }
  attacks += string.Format(" | line %1, depleted %2, refitting %3, remnants %4, unarmed vehicles %5", line, depleted, m_Refitting.Count(), remnants, vehiclesNoGunner);
  int visible;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner != m_sFaction && !base.m_sOwner.IsEmpty() && Visible(base)) visible++;
  attacks += string.Format(" | support budget %1 | doctrine %2", SupportBudget(), m_Doctrine.m_sName) + BuildStatus();
  return string.Format("[ACR] AI %1 status: %2 squads (free %3, defending %4, attacking %5, other %6), attacks:%7, enemy bases known %8, RP %9",
   m_sFaction, all.Count(), nFree, nDefend, nAttack, nOther, attacks, visible, m_Manager.m_Bases.GetRP(m_sFaction));
 }

 // ---------------------------------------------------------------- AC:A Think (ACR_Think)
 // Every "[ACR] AI <side>:" decision goes through Say: printed as before, and the newest
 // THINK_LINES kept (with the match clock) for the host's Think view.
 static const int THINK_LINES = 20;
 protected ref array<string> m_aThink = {};
 void Say(string line)
 {
  Print(line);
  string text = line;
  if (text.StartsWith("[ACR] AI ")) text = text.Substring(9, text.Length() - 9);
  if (text.StartsWith(m_sFaction + ": ")) text = text.Substring(m_sFaction.Length() + 2, text.Length() - m_sFaction.Length() - 2);
  m_aThink.Insert(ACR_Think.Clock() + " " + text);
  if (m_aThink.Count() > THINK_LINES) m_aThink.RemoveOrdered(0);
 }
 void ThinkDecisions(notnull array<string> lines) { lines.Copy(m_aThink); }

 // One line per attack: phase, squads, time in that phase and where it forms up.
 void ThinkAttacks(notnull array<string> lines)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (!attack.m_Target) continue;
   string kind = "attack";
   if (attack.m_bGrab) kind = "grab";
   else if (attack.m_bCounter) kind = "counter";
   else if (attack.m_bFobRaid) kind = "FOB raid";
   string phase;
   if (attack.m_bAssaulting || attack.m_bGrab) phase = string.Format("assault %1 min", (int)((now - attack.m_fAssaultStart) / 60000));
   else phase = string.Format("staging, %1 s left", (int)Math.Max(0, (attack.m_fStageDeadline - now) / 1000));
   string grids;
   foreach (vector stage : attack.m_aStaging)
   {
    string grid = ACR_Manager.Grid(stage);
    if (!grids.Contains(grid)) grids += " " + grid;
   }
   if (grids.IsEmpty()) grids = " " + ACR_Manager.Grid(attack.m_Staging);
   string fire = "none";
   if (attack.m_Suppressor) fire = attack.m_Suppressor.GetName();
   lines.Insert(string.Format("%1 %2: %3, %4 squad(s), staging%5, base of fire %6", kind, attack.m_Target.m_sName, phase, attack.m_Squads.Count(), grids, fire));
  }
  if (m_Refitting.Count() > 0)
  {
   string refit;
   foreach (ACR_Squad squad, float until : m_Refitting)
    if (squad) refit += string.Format(" %1 (%2 s)", squad.GetName(), (int)Math.Max(0, (until - now) / 1000));
   lines.Insert("refitting:" + refit);
  }
 }

 protected void UpdateAttacks()
 {
  for (int i = m_Attacks.Count() - 1; i >= 0; i--)
  {
   ACR_AIAttack attack = m_Attacks[i];
   for (int s = attack.m_Squads.Count() - 1; s >= 0; s--)
   {
    ACR_Squad squad = attack.m_Squads[s];
    if (!squad || !m_Manager.m_Squads.Contains(squad) || squad.HasPlayer()) { attack.Remove(s); continue; }
    // Lost its gunner (or down to one man): no use in an attack.
    if (!squad.IsCombatUnit())
    {
     FallBack(squad, "can no longer fight");
     attack.Remove(s);
     continue;
    }
    // Badly hurt and still under fire: pull out instead of dying in place.
    if (attack.m_bAssaulting && squad.StrengthRatio() < RetreatRatio() && squad.KnownEnemies(300) > 0)
    {
     FallBack(squad, "heavy casualties");
     attack.Remove(s);
     continue;
    }
    // Shot dry: pull back to rearm instead of charging with empty rifles.
    if (squad.m_iAmmoPct >= 0 && squad.m_iAmmoPct < OUT_OF_AMMO)
    {
     FallBack(squad, "out of ammo");
     attack.Remove(s);
    }
   }
   ACR_Base target = attack.m_Target;
   bool won = target && (target.m_sOwner == m_sFaction || target.m_bRetired);
   if (attack.m_bGrab && target && !SeenOwner(target).IsEmpty() && !won)
   {
    Say(string.Format("[ACR] AI %1: %2 was taken first, grab cancelled", m_sFaction, target.m_sName));
    foreach (ACR_Squad late : attack.m_Squads) late.m_iAITask = ACR_EAITask.FREE;
    m_Attacks.RemoveOrdered(i);
    continue;
   }
   int strength;
   foreach (ACR_Squad member : attack.m_Squads) strength += member.Strength();
   bool fusePrimed = attack.m_SapperDemo && attack.m_SapperDemo.InProgress();
   bool broken = !fusePrimed && attack.m_iStartStrength > 0 && strength < attack.m_iStartStrength * m_Doctrine.m_fAbortRatio;
   bool stalled = !fusePrimed && !won && target && Stalled(attack);
   // Nobody left who can walk in (the infantry died or fell back): vehicles and support teams
   // alone never take it, so stop now instead of waiting out the time limit.
   bool noTakers = !fusePrimed && !won && target && !attack.m_bGrab && !target.m_sOwner.IsEmpty() && !attack.m_Squads.IsEmpty() && TakersIn(attack) == 0;
   if (!target || won || (attack.m_Squads.IsEmpty() && !fusePrimed) || broken || stalled || noTakers)
   {
    string outcome = "failed";
    if (won) outcome = "won";
    if (noTakers) outcome = "called off (no infantry left to take it)";
    if (broken)
    {
     outcome = "called off (too many losses)";
     // Not straight back at the base that just beat it (Raider breaks off at half strength
     // and weighs weak bases low, so it used to re-launch at the same one).
     if (target) m_StalledUntil.Set(target, GetGame().GetWorld().GetWorldTime() + STALLED_COOLDOWN * 1000);
    }
    if (stalled)
    {
     // Where the attackers got to (night soak 26 Oct: 46 of 118 grabs and attacks stalled).
     float closest = -1;
     string closestOrder;
     foreach (ACR_Squad stuck : attack.m_Squads)
     {
      if (!stuck) continue;
      float gap = vector.DistanceXZ(PositionOf(stuck), target.GetOrigin());
      if (closest < 0 || gap < closest) { closest = gap; closestOrder = stuck.m_Order; }
     }
     outcome = string.Format("called off (stalled; nearest squad %1 m out, '%2')", (int)closest, closestOrder);
     m_StalledUntil.Set(target, GetGame().GetWorld().GetWorldTime() + STALLED_COOLDOWN * 1000);
    }
    if (target) Say(string.Format("[ACR] AI %1: attack on %2 %3", m_sFaction, target.m_sName, outcome));
    string why = "it failed";
    if (broken) why = "too many losses";
    else if (stalled) why = "it stalled";
    if (target && won) TwoICReport(string.Format("Took %1", target.m_sName));
    else if (target && attack.m_bGrab) TwoICReport(string.Format("Couldn't take %1: %2", target.m_sName, why));
    else if (target) TwoICReport(string.Format("Called off the attack on %1: %2", target.m_sName, why));
    foreach (ACR_Squad squad : attack.m_Squads)
    {
     if (broken) FallBack(squad, "attack called off");
     else squad.m_iAITask = ACR_EAITask.FREE;
    }
    m_Attacks.RemoveOrdered(i);
    continue;
   }
   if (attack.m_bAssaulting)
   {
    bool demoBusy = false;
    if (attack.m_bFobRaid && attack.m_SapperDemo)
    {
     float now = GetGame().GetWorld().GetWorldTime();
     attack.m_SapperDemo.Tick(now);
     ACR_ESapperDemoState demoState = attack.m_SapperDemo.State();
     // Planting or fused: hold the attackers back. After the blast the attack takes the FOB.
     if (demoState == ACR_ESapperDemoState.PLANTING || demoState == ACR_ESapperDemoState.WIRING || demoState == ACR_ESapperDemoState.FALLBACK)
     {
      demoBusy = true;
     }
    }
    if (demoBusy) continue;
    PushAttackers(attack);
    TickFireBase(attack);
    continue;
   }
   bool anyAirLift = false;
   bool gathered = true;
   foreach (ACR_Squad squad : attack.m_Squads)
   {
    if (ACR_AirLift.Busy(squad)) { anyAirLift = true; gathered = false; continue; }
    if (vector.DistanceXZ(PositionOf(squad), attack.StagingOf(squad)) > 120) gathered = false;
   }
   float stageNow = GetGame().GetWorld().GetWorldTime();
   // Out of time with nobody who can take the base anywhere near their staging point (a slow
   // drive by road, held up on the way): going in would be a lone squad far out, so call it off;
   // the base waits out the stall cooldown and the next plan picks squads that can get there.
   if (!gathered && !anyAirLift && stageNow > attack.m_fStageDeadline)
   {
    float nearestTaker = -1;
    foreach (ACR_Squad formingUp : attack.m_Squads)
    {
     if (!CanTakeBase(formingUp, attack.m_bFobRaid)) continue;
     float lateGap = vector.DistanceXZ(PositionOf(formingUp), attack.StagingOf(formingUp));
     if (nearestTaker < 0 || lateGap < nearestTaker) nearestTaker = lateGap;
    }
    if (nearestTaker > ASSAULT_JOIN_EXTRA)
    {
     Say(string.Format("[ACR] AI %1: attack on %2 called off (never formed up; nearest squad %3 m from its staging point)", m_sFaction, target.m_sName, (int)nearestTaker));
     m_StalledUntil.Set(target, stageNow + STALLED_COOLDOWN * 1000);
     foreach (ACR_Squad released : attack.m_Squads) released.m_iAITask = ACR_EAITask.FREE;
     m_Attacks.RemoveOrdered(i);
     continue;
    }
   }
   if (gathered || (!anyAirLift && stageNow > attack.m_fStageDeadline))
   {
    attack.m_bAssaulting = true;
    // The base of fire goes in too when it is the only squad left that can take the base.
    if (attack.m_Suppressor && TakersIn(attack, attack.m_Suppressor) == 0) attack.m_Suppressor = null;
    attack.m_fAssaultStart = GetGame().GetWorld().GetWorldTime();
    // Preparation: shell the objective with heavy assets against fortified FOBs
    bool heavyPrep = target.m_bBuilt || DefenceOf(target) >= Lowered(m_Doctrine.m_iHeavyPrep, FireFactor());
    if (!heavyPrep || !CallSupport(ACR_ESupportType.BOMB_RUN, target.GetOrigin(), "airstrike preparation on " + target.m_sName))
     if (!heavyPrep || !CallSupport(ACR_ESupportType.ROCKET_STRIKE, target.GetOrigin(), "rocket strike preparation on " + target.m_sName))
      if (!heavyPrep || !CallSupport(ACR_ESupportType.HELI_GUN_RUN, target.GetOrigin(), "gun run preparation on " + target.m_sName))
       if (!CallSupport(ACR_ESupportType.MORTAR_HE_LARGE, target.GetOrigin(), "preparation fire on " + target.m_sName))
        if (!CallSupport(ACR_ESupportType.MORTAR_HE_SMALL, target.GetOrigin(), "preparation fire on " + target.m_sName))
         CallSupport(ACR_ESupportType.ARTILLERY_BATTERY, target.GetOrigin(), "battery preparation fire on " + target.m_sName);
    vector screen = attack.m_Staging + (target.GetOrigin() - attack.m_Staging) * 0.55;
    CallSupport(ACR_ESupportType.MORTAR_SMOKE, screen, "smoke screen for " + target.m_sName);
    // Reserve gunship support for an assault, not only the rare counterattack.
    // CallSupport still checks cooldown, range/pad readiness and friendly distance.
    if (!CallSupport(ACR_ESupportType.HELI_LOITER, target.GetOrigin(), "gunship for the assault on " + target.m_sName))
     CallSupport(ACR_ESupportType.HELI_DOOR_GUNS, target.GetOrigin(), "door-gunner heli for the assault on " + target.m_sName);
    foreach (ACR_Squad squad : attack.m_Squads)
    {
     squad.SetMode(ACR_EMoveMode.COMBAT);
     if (squad != attack.m_Suppressor)
     {
      IEntity vSquad = ACR_Transport.CrewedVehicle(squad);
      if (vSquad && squad.HasGunner(vSquad))
      {
       vector vSpot;
       if (OverwatchSpot(target, PositionOf(squad), vSpot)) squad.Defend(vSpot);
       else squad.Assault(target.GetOrigin());
      }
      else
      {
       squad.Assault(target.GetOrigin());
      }
     }
    }
    if (attack.m_Suppressor) UpdateFireBase(attack);
    string suppressor = "none";
    if (attack.m_Suppressor) suppressor = attack.m_Suppressor.GetName();
    Say(string.Format("[ACR] AI %1: assaulting %2 with %3 squad(s) on %4 approach line(s), base of fire: %5", m_sFaction, target.m_sName, attack.m_Squads.Count(), ApproachLines(attack), suppressor));
   }
  }
 }

 // ---------------------------------------------------------------- 4. idle squads

 protected void PostIdle()
 {
  array<ACR_Base> front = {};
  FrontlineBases(front);
  if (front.IsEmpty()) return;
  array<ACR_Squad> free = {};
  MySquads(free, true);
  foreach (ACR_Squad squad : free)
  {
   if (squad.m_iAITask == ACR_EAITask.POSTED) continue;
   if (IsReconTeam(squad))
   {
    ACR_Base enemyBase = NearestEnemyBase(PositionOf(squad));
    if (enemyBase)
    {
     vector vSpot;
     if (OverwatchSpot(enemyBase, PositionOf(squad), vSpot))
     {
      squad.m_iAITask = ACR_EAITask.POSTED;
      squad.SetMode(ACR_EMoveMode.SILENT);
      squad.Defend(vSpot);
      Say(string.Format("[ACR] AI %1: recon team %2 posted to overwatch at %3 (%4 m)", m_sFaction, squad.GetName(), enemyBase.m_sName, (int)vector.DistanceXZ(vSpot, enemyBase.GetOrigin())));
      continue;
     }
    }
   }
   // Nearest frontline base, but spread out: each squad already posted
   // there counts as 400 m farther, so idle troops don't pile up in one place.
   ACR_Base nearest;
   float best = float.MAX;
   float bestScore = float.MAX;
   foreach (ACR_Base base : front)
   {
    float d = vector.DistanceXZ(PositionOf(squad), base.GetOrigin());
    float score = d + PostedAt(base) * 400;
    if (score < bestScore) { bestScore = score; best = d; nearest = base; }
   }
   squad.m_iAITask = ACR_EAITask.POSTED;
   if (!nearest || best < 150) continue;
   squad.SetMode(ACR_EMoveMode.FAST);
   Travel(squad, nearest.GetOrigin(), true, 0);
   Say(string.Format("[ACR] AI %1: %2 posted to %3", m_sFaction, squad.GetName(), nearest.m_sName));
  }
  PositionTransports();
 }

 // Keep idle unarmed transport trucks near safe bases where foot infantry spawn or stage,
 // while leaving their order state as RETURN so IsFreeTransport can claim them en route.
 protected void PositionTransports()
 {
  if (!m_Manager.m_Rides) return;
  array<ACR_Base> hubs = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || base.m_bContested || ThreatTo(base) > 0) continue;
   bool hasFoot = false;
   foreach (ACR_Squad foot : m_Manager.m_Squads)
   {
    if (!foot.GetFaction() || foot.GetFaction().GetFactionKey() != m_sFaction || foot.HasPlayer()) continue;
    if (foot.IsMounted() || ACR_Transport.CrewedVehicle(foot) || foot.m_bMortar || foot.Aircraft()) continue;
    if (foot.m_iAITask != ACR_EAITask.FREE && foot.m_iAITask != ACR_EAITask.POSTED) continue;
    if (vector.DistanceXZ(PositionOf(foot), base.GetOrigin()) <= base.m_fRadius + 150) { hasFoot = true; break; }
   }
   if (hasFoot || base == m_ReserveBase || (ACR_LogisticsRules.CanDeploy(base) && FrontDistance(base) >= FOB_DEPLOY_MIN_FRONT))
    hubs.Insert(base);
  }
  if (hubs.IsEmpty()) return;
  foreach (ACR_Squad taxi : m_Manager.m_Squads)
  {
   if (!m_Manager.m_Rides.IsFreeTransport(taxi, m_sFaction)) continue;
   if (taxi.OrderState() == ACR_EOrderState.RETURN) continue;
   IEntity vehicle = ACR_Transport.CrewedVehicle(taxi);
   if (!vehicle) continue;
   vector here = vehicle.GetOrigin();
   ACR_Base bestHub;
   float bestDist = float.MAX;
   foreach (ACR_Base hub : hubs)
   {
    float d = vector.DistanceXZ(here, hub.GetOrigin());
    if (d < bestDist) { bestDist = d; bestHub = hub; }
   }
   if (!bestHub || bestDist <= 450) continue;
   taxi.SetMode(ACR_EMoveMode.FAST);
   taxi.RouteTo(ACR_Manager.Ground(bestHub.GetOrigin()), ACR_Squad.WP_MOVE, -1, "Returning", ACR_EOrderKind.MOVE);
   taxi.SetOrder("Returning", ACR_EOrderState.RETURN);
   Say(string.Format("[ACR] AI %1: motor pool %2 repositioning to %3 (%4 m)", m_sFaction, taxi.GetName(), bestHub.m_sName, (int)bestDist));
  }
 }

 // A staging point around the target, as close to the wanted direction as
 // possible, that is dry, has a dry run in, and stays clear of every other
 // enemy base we know of (forming up next to one drags the force into a fight
 // it didn't plan).
 protected bool StagingPoint(ACR_Base target, vector dir, float ring, out vector point)
 {
  array<float> turns = {0, 30, -30, 60, -60, 90, -90, 120, -120, 150, -150, 180};
  foreach (float turn : turns)
  {
   float angle = turn * Math.DEG2RAD;
   vector rotated = Vector(dir[0] * Math.Cos(angle) - dir[2] * Math.Sin(angle), 0, dir[0] * Math.Sin(angle) + dir[2] * Math.Cos(angle));
   vector candidate;
   if (!BBB_Terrain.DryApproach(target.GetOrigin(), rotated, ring, candidate)) continue;
   if (NearOtherEnemyBase(candidate, target)) continue;
   point = candidate;
   return true;
  }
  return BBB_Terrain.DryApproach(target.GetOrigin(), dir, ring, point);
 }

 // ---------------------------------------------------------------- covered approaches
 // Jacob 26 Oct (Provins): a squad walking to its flank staging point took the shortest path,
 // across the front of the objective in full view, while a route round behind the rise was
 // hidden. A foot squad's way to its staging point is now chosen from the direct line and an arc
 // round the objective either way, by length plus the stretch the objective's ground can see
 // (terrain only: dead ground; houses and trees aren't counted). Rides and air lifts keep
 // their own routes; contact on the way is BBB's (combat manoeuvre).
 static const float COVER_ARC_STEP = 25;       // degrees between arc waypoints
 static const float COVER_SAMPLE = 40;         // m between sight checks along a path
 static const float COVER_RANGE = 800;         // m from the objective beyond which nothing counts as seen
 static const float COVER_EXPOSED_WEIGHT = 4;  // an exposed metre costs this many extra metres
 static const float COVER_SPIRAL_OUT = 250;    // m past the staging ring an arc may start
 static const float COVER_MAX_STRETCH = 2.5;   // an arc longer than this times the direct way (+300 m) isn't walked
 static const float COVER_MIN_GAIN = 0.15;     // the arc must cut the cost by this share
 static const float COVER_WALK_SPEED = 2;      // m/s on foot, for the forming-up wait
 protected float m_fCoverDetour;               // extra metres of the last covered route Travel chose (0: direct)

 // Where the objective's defenders look from: its centre and four points half way out, 2.2 m up.
 static void ObjectiveEyes(vector center, float radius, notnull array<vector> eyes)
 {
  eyes.Insert(EyeAt(center));
  for (int i = 0; i < 4; i++)
   eyes.Insert(EyeAt(center + Turned(Vector(0, 0, 1), i * 90) * (radius * 0.5)));
 }

 static vector EyeAt(vector point)
 {
  point[1] = GetGame().GetWorld().GetSurfaceY(point[0], point[2]) + 2.2;
  return point;
 }

 // The ground at 'point' is in sight of an eye (terrain only).
 static bool GroundSeen(notnull array<vector> eyes, vector point)
 {
  BaseWorld world = GetGame().GetWorld();
  vector body = point;
  body[1] = world.GetSurfaceY(point[0], point[2]) + 1.0;
  foreach (vector eye : eyes)
  {
   if (vector.DistanceXZ(eye, body) > COVER_RANGE) continue;
   TraceParam trace = new TraceParam();
   trace.Start = eye;
   trace.End = body;
   trace.Flags = TraceFlags.WORLD;
   if (world.TraceMove(trace, null) >= 1) return true;
  }
  return false;
 }

 // Length and exposed length of the path from 'from' through 'legs', sampled every COVER_SAMPLE m.
 static float PathExposure(notnull array<vector> eyes, vector from, notnull array<vector> legs, out float exposed)
 {
  float length = 0;
  exposed = 0;
  vector a = from;
  foreach (vector b : legs)
  {
   float d = vector.DistanceXZ(a, b);
   int steps = (int)Math.Ceil(d / COVER_SAMPLE);
   if (steps < 1) steps = 1;
   for (int i = 1; i <= steps; i++)
   {
    float t = i;
    t /= steps;
    if (GroundSeen(eyes, a + (b - a) * t)) exposed += d / steps;
   }
   length += d;
   a = b;
  }
  return length;
 }

 // Waypoints round 'center' from 'from' to 'stage', turning one way (sense 1 or -1), one every
 // COVER_ARC_STEP degrees. The radius runs from where the squad meets the arc (at most
 // COVER_SPIRAL_OUT past 'ring') to the staging point's, so the squad never walks in closer than that.
 static void ArcLegs(vector center, vector from, vector stage, float ring, int sense, notnull array<vector> legs)
 {
  vector a = from - center;
  a[1] = 0;
  vector b = stage - center;
  b[1] = 0;
  float startAngle = Math.Atan2(a[2], a[0]);
  float sweep = Math.Atan2(b[2], b[0]) - startAngle;
  if (sense > 0) { while (sweep <= 0) sweep += Math.PI2; }
  else { while (sweep >= 0) sweep -= Math.PI2; }
  float startRadius = Math.Clamp(a.Length(), ring, ring + COVER_SPIRAL_OUT);
  float endRadius = b.Length();
  int steps = (int)Math.Ceil(Math.AbsFloat(sweep) * Math.RAD2DEG / COVER_ARC_STEP);
  if (steps < 1) steps = 1;
  for (int i = 0; i < steps; i++)
  {
   float t = i;
   t /= steps;
   float angle = startAngle + sweep * t;
   float r = startRadius + (endRadius - startRadius) * t;
   vector point = center + Vector(Math.Cos(angle) * r, 0, Math.Sin(angle) * r);
   if (i == 0 && vector.DistanceXZ(point, from) < 30) continue;
   legs.Insert(point);
  }
  legs.Insert(stage);
 }

 // A covered way for a foot squad to its staging point for 'target', when one beats the direct
 // line; sets m_fCoverDetour. False: walk the direct way (RouteTo).
 protected bool CoveredApproach(ACR_Squad squad, ACR_Base target, vector stage)
 {
  int started = System.GetTickCount();
  vector from = PositionOf(squad);
  vector center = target.GetOrigin();
  float ring = target.m_fRadius + STAGING_DISTANCE;
  array<vector> eyes = {};
  ObjectiveEyes(center, target.m_fRadius, eyes);
  array<vector> direct = {stage};
  float directSeen;
  float directLength = PathExposure(eyes, from, direct, directSeen);
  float directCost = directLength + directSeen * COVER_EXPOSED_WEIGHT;
  array<vector> best;
  float bestCost = directCost * (1 - COVER_MIN_GAIN);
  float bestLength;
  float bestSeen;
  // Only when the squad isn't already inside the ring (a squad that close takes the short way).
  if (directSeen > 0 && vector.DistanceXZ(from, center) > ring * 0.9)
  {
   for (int sense = -1; sense <= 1; sense += 2)
   {
    array<vector> arc = {};
    ArcLegs(center, from, stage, ring, sense, arc);
    bool usable = true;
    vector prev = from;
    foreach (int i, vector leg : arc)
    {
     arc[i] = ACR_Manager.Ground(leg);
     if (BBB_Terrain.WaterAlong(prev, arc[i]) > 0 || (i < arc.Count() - 1 && NearOtherEnemyBase(arc[i], target))) { usable = false; break; }
     prev = arc[i];
    }
    if (!usable) continue;
    float seen;
    float length = PathExposure(eyes, from, arc, seen);
    if (length > directLength * COVER_MAX_STRETCH + 300) continue;
    float cost = length + seen * COVER_EXPOSED_WEIGHT;
    if (cost >= bestCost) continue;
    best = arc;
    bestCost = cost;
    bestLength = length;
    bestSeen = seen;
   }
  }
  ACR_Perf.Lap("ai.coverRoute", started);
  if (!best) return false;
  m_fCoverDetour = bestLength - directLength;
  squad.WalkLegs(best, ACR_Squad.WP_MOVE, -1, "Moving", 0);
  Say(string.Format("[ACR] AI %1: %2 takes a covered way to its staging point for %3: %4 m, %5 m in sight (direct %6 m, %7 m in sight)", m_sFaction, squad.GetName(), target.m_sName, (int)bestLength, (int)bestSeen, (int)directLength, (int)directSeen));
  return true;
 }

 // A staging point near 'point' on the same approach line that the objective's ground can't see:
 // a little further round or further out, never closer than 150 m past the objective's edge.
 // 'others': the attack's other lines' staging points; a nudge never turns within
 // APPROACH_SEPARATION of one (#52: nudged 20 degrees towards each other, two lines failed the
 // spacing and the last squad was put on the first squad's staging point).
 protected vector HiddenStaging(ACR_Base target, vector point, array<vector> others = null)
 {
  vector center = target.GetOrigin();
  array<vector> eyes = {};
  ObjectiveEyes(center, target.m_fRadius, eyes);
  if (!GroundSeen(eyes, point)) return point;
  vector dir = point - center;
  dir[1] = 0;
  float ring = dir.Length();
  if (ring < 1) return point;
  dir.Normalize();
  array<float> turns = {0, -10, 10, -20, 20};
  array<float> outs = {0, 60, -40, 120};
  foreach (float outward : outs)
  {
   float r = ring + outward;
   if (r < target.m_fRadius + 150) continue;
   foreach (float turn : turns)
   {
    if (outward == 0 && turn == 0) continue;
    vector candidate = ACR_Manager.Ground(center + Turned(dir, turn) * r);
    if (BBB_Terrain.IsWaterAt(candidate) || NearOtherEnemyBase(candidate, target)) continue;
    if (others && !ApartFrom(center, candidate, others)) continue;
    if (!GroundSeen(eyes, candidate)) return candidate;
   }
  }
  return point;
 }

 // Approach lines (Jacob 20 Oct, Etoupe: the big assault came down one main road and was shot
 // up there; reinforcements each staged on their own side, which was the same side). Every
 // squad of an attack gets a line turned off the main approach: the least-used line first, then
 // the one nearest the squad's own side. A line whose staging point ends up (water, other enemy
 // bases) closer than APPROACH_SEPARATION degrees to another line's point is skipped.
 static ref array<float> s_aApproachTurns = {0, 60, -60, 115, -115};
 static const float APPROACH_SEPARATION = 40;

 static vector Turned(vector dir, float degrees)
 {
  float angle = degrees * Math.DEG2RAD;
  return Vector(dir[0] * Math.Cos(angle) - dir[2] * Math.Sin(angle), 0, dir[0] * Math.Sin(angle) + dir[2] * Math.Cos(angle));
 }

 // Degrees between two directions on the ground.
 static float TurnBetween(vector a, vector b)
 {
  a[1] = 0;
  b[1] = 0;
  if (a.LengthSq() < 0.001 || b.LengthSq() < 0.001) return 0;
  a.Normalize();
  b.Normalize();
  return Math.Acos(Math.Clamp(vector.Dot(a, b), -1, 1)) * Math.RAD2DEG;
 }

 // The approach line for a squad joining this attack (slot index, -1: none found, stage is
 // then the main staging point). Squads already on lines are in attack.m_aSlot.
 int PickApproach(ACR_AIAttack attack, vector from, out vector stage)
 {
  ACR_Base target = attack.m_Target;
  stage = attack.m_Staging;
  if (!target) return -1;
  vector center = target.GetOrigin();
  float ring = target.m_fRadius + STAGING_DISTANCE;
  vector ahead = attack.m_vApproach;
  vector own = from - center;
  own[1] = 0;
  if (own.LengthSq() > 1) own.Normalize();
  else own = ahead;
  if (ahead.LengthSq() < 0.001) ahead = own;
  if (ahead.LengthSq() < 0.001) ahead = Vector(0, 0, 1);
  array<int> order = {};
  array<float> keys = {};
  foreach (int slot, float turn : s_aApproachTurns)
  {
   int used = 0;
   foreach (int taken : attack.m_aSlot) if (taken == slot) used++;
   float key = used * 1000 + TurnBetween(Turned(ahead, turn), own);
   int at = 0;
   while (at < keys.Count() && keys[at] <= key) at++;
   order.InsertAt(slot, at);
   keys.InsertAt(key, at);
  }
  foreach (int pick : order)
  {
   vector candidate;
   if (!StagingPoint(target, Turned(ahead, s_aApproachTurns[pick]), ring, candidate)) continue;
   // The other lines' staging points (a squad sharing this line shares its point).
   array<vector> others = {};
   foreach (int i, int other : attack.m_aSlot)
   {
    if (other == pick || other < 0 || i >= attack.m_aStaging.Count()) continue;
    others.Insert(attack.m_aStaging[i]);
   }
   if (!ApartFrom(center, candidate, others)) continue;
   // Form up out of the objective's sight where the ground allows (Jacob 26 Oct, Provins),
   // without closing in on another line, the ones no squad has taken yet included: nudged
   // towards them, the next squads' lines no longer fitted (#52).
   if (!target.m_sOwner.IsEmpty())
   {
    array<vector> keepClear = {};
    keepClear.Copy(others);
    foreach (int free, float freeTurn : s_aApproachTurns)
    {
     if (free != pick && !attack.m_aSlot.Contains(free)) keepClear.Insert(center + Turned(ahead, freeTurn) * ring);
    }
    candidate = HiddenStaging(target, candidate, keepClear);
   }
   stage = candidate;
   return pick;
  }
  return -1;
 }

 // 'point' is at least APPROACH_SEPARATION degrees round 'center' from each of 'others'.
 static bool ApartFrom(vector center, vector point, notnull array<vector> others)
 {
  foreach (vector other : others)
   if (TurnBetween(other - center, point - center) < APPROACH_SEPARATION) return false;
  return true;
 }

 // Distinct approach lines in use (for the log).
 static int ApproachLines(ACR_AIAttack attack)
 {
  array<int> lines = {};
  foreach (int slot : attack.m_aSlot) if (slot >= 0 && !lines.Contains(slot)) lines.Insert(slot);
  return Math.Max(1, lines.Count());
 }

 protected bool NearOtherEnemyBase(vector point, ACR_Base except)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base == except || base.m_sOwner == m_sFaction || base.m_sOwner.IsEmpty() || !Visible(base)) continue;
   if (vector.DistanceXZ(point, base.GetOrigin()) < base.m_fRadius + STAGING_CLEARANCE) return true;
  }
  return false;
 }

 // Crews that ended up on foot next to their own intact, empty vehicle get
 // back in (the game's AI sometimes bails out and leaves a BTR standing).
 protected ref map<ACR_Squad, float> m_NextRecrew = new map<ACR_Squad, float>();
 protected void Recrew()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   IEntity vehicle = squad.m_OwnVehicle;
   if (!vehicle || squad.m_iAITask == ACR_EAITask.RESUPPLY) continue;
   // Or its driver bailed and only part of the crew is back aboard: nobody drives, so every
   // move fails where it stands (ACR_Squad.m_bDriverBailed).
   bool driverless = squad.m_bDriverBailed && squad.IsMounted() && !ACR_Squad.DriverOf(vehicle);
   if (squad.IsMounted() && !driverless) continue;
   if (ACR_Logistics.Health(vehicle) < 0.25 || (!driverless && !ACR_Vehicles.IsEmpty(vehicle))) continue;
   // Run dry (the game's AI gets out of a car with no fuel): wait for the fuel truck, not in and
   // out again (Jacob's 27 Oct game: a Humvee crew left, got back in and left again three times).
   float fuel = ACR_Logistics.Fuel(vehicle);
   if (fuel >= 0 && fuel < 0.02) continue;
   if (vector.DistanceXZ(PositionOf(squad), vehicle.GetOrigin()) > 400 || squad.KnownEnemies(120) > 0) continue;
   float next;
   if (m_NextRecrew.Find(squad, next) && now < next) continue;
   // Not back into a vehicle that just got stuck (the crew bailed, got back in, got stuck again).
   if (now - squad.m_fStuckAt < 600000) continue;
   // Driverless: one man for the driver's seat (the group's Get in seats nobody while part of
   // the crew is aboard); its order stands. Tried again in 30 s if he isn't in by then.
   if (driverless)
   {
    m_NextRecrew.Set(squad, now + 30000);
    if (squad.ReseatDriver(vehicle)) Say(string.Format("[ACR] AI %1: %2's driver gets back into its vehicle", m_sFaction, squad.GetName()));
    continue;
   }
   m_NextRecrew.Set(squad, now + 90000);
   if (squad.GetIn(vehicle.GetOrigin())) Say(string.Format("[ACR] AI %1: %2 gets back into its vehicle", m_sFaction, squad.GetName()));
  }
 }

 void RecrewForTest() { Recrew(); }

 protected int PostedAt(ACR_Base base)
 {
  int count;
  array<ACR_Squad> mine = {};
  MySquads(mine, true);
  foreach (ACR_Squad squad : mine)
   if (squad.m_iAITask == ACR_EAITask.POSTED && vector.DistanceXZ(PositionOf(squad), base.GetOrigin()) < base.m_fRadius + 150) count++;
  return count;
 }

 // ---------------------------------------------------------------- mortar teams
 // One working team at a time, bought once troops aren't short. While no base that isn't
 // ours is in reach it goes to the own base nearest the front (setting up by itself on
 // arrival), so it follows the front as it moves.
 static const float MORTAR_MOVE_MS = 120000;
 static const float MORTAR_MIN_STANDOFF = 400; // don't set up closer than this to an enemy base
 protected float m_fNextMortarMove;

 protected bool BuyMortarTeam(ACR_Base spawn, ACR_FactionData data)
 {
  array<ACR_MortarTeam> teams = {};
  m_Manager.Mortars().TeamsOf(m_sFaction, teams);
  foreach (ACR_MortarTeam team : teams) if (!team.m_bLost) return false;
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (!entry.m_bMortarTeam || entry.m_iRemaining <= 0) continue;
   if (data.m_iRP < entry.m_iCost + SUPPORT_RESERVE) return false;
   Say(string.Format("[ACR] AI %1 requisition (mortar team): %2", m_sFaction, m_Manager.PurchaseAt(m_sFaction, i, spawn)));
   return true;
  }
  return false;
 }

 // One support gunship on a helipad (worlds with pads), bought again when lost.
 protected bool BuyHelicopter(ACR_FactionData data)
 {
  if (!ACR_HeliStation.UsesPads(m_sFaction) || !m_Manager.m_Supports) return false;
  ACR_HeliStation station = m_Manager.m_Supports.Station();
  if (station.CountOf(m_sFaction, true) > 0) return false;
  ACR_Helipad pad = station.FreePad(m_sFaction, null);
  if (!pad) return false;
  ACR_Base base = pad.Base();
  if (!ACR_LogisticsRules.CanDeploy(base) || base.m_bContested) return false;
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (entry.m_iSupportHeli != 1 || entry.m_iRemaining <= 0) continue;
   if (data.m_iRP < entry.m_iCost + SUPPORT_RESERVE) return false;
   Say(string.Format("[ACR] AI %1 requisition (support gunship): %2", m_sFaction, m_Manager.PurchaseAt(m_sFaction, i, base)));
   return true;
  }
  return false;
 }

 // What a mortar is there to cover: a base it knows the enemy (or a pocket) holds, or one this
 // side is attacking. Unclaimed sites don't count (Everon night soak 26 Oct: hidden unclaimed
 // sites next to each HQ kept both teams there all game; one fire mission in an hour).
 protected bool MortarTarget(ACR_Base base)
 {
  if (base.m_sOwner == m_sFaction) return false;
  foreach (ACR_AIAttack attack : m_Attacks) if (attack.m_Target == base) return true;
  return HostileOwner(base);
 }

 // The own, safe base closest to what the mortar covers (but not right on top of any base
 // that isn't ours).
 protected ACR_Base MortarBase()
 {
  ACR_Base best;
  float bestFront = float.MAX;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || base.m_bContested || ThreatTo(base) > 0) continue;
   float blockedUntil;
   if (m_MortarUnreachable.Find(base, blockedUntil) && now < blockedUntil) continue;
   float front = float.MAX;
   float nearest = float.MAX;
   foreach (ACR_Base other : ACR_Base.s_All)
   {
    if (other.m_sOwner == m_sFaction) continue;
    float d = vector.DistanceXZ(base.GetOrigin(), other.GetOrigin());
    nearest = Math.Min(nearest, d);
    if (MortarTarget(other)) front = Math.Min(front, d);
   }
   if (nearest < MORTAR_MIN_STANDOFF || front >= bestFront) continue;
   bestFront = front;
   best = base;
  }
  return best;
 }

 // Where a team was last sent (MortarBase), that order's serial and when. A team that gives up on
 // the way (unreachable) leaves that base out for MORTAR_UNREACHABLE_MS: Jacob's 27 Oct game sent
 // Red's mortar to Bovemont eight times in 30 min, and each time it packed up, failed to path
 // there within a minute and set up again at home. Packing up halts and restores the order, so
 // the serial only bounds it from below (ACR_Squad.GaveUpSince).
 static const float MORTAR_UNREACHABLE_MS = 900000;
 static const float MORTAR_SENT_MS = 600000;
 protected ref map<ACR_Squad, ACR_Base> m_MortarSentTo = new map<ACR_Squad, ACR_Base>();
 protected ref map<ACR_Squad, int> m_MortarSentSerial = new map<ACR_Squad, int>();
 protected ref map<ACR_Squad, float> m_MortarSentAt = new map<ACR_Squad, float>();
 protected ref map<ACR_Base, float> m_MortarUnreachable = new map<ACR_Base, float>();

 // Every AI tick, before PositionMortars' time gate: a team that gives up sets up where it is
 // within the next two minutes.
 protected void NoteMortarGiveUps(float now)
 {
  array<ACR_Squad> sent = {};
  foreach (ACR_Squad sentSquad, ACR_Base sentBase : m_MortarSentTo) sent.Insert(sentSquad);
  foreach (ACR_Squad squad : sent)
  {
   ACR_Base base = m_MortarSentTo.Get(squad);
   bool gaveUp = squad && base && squad.GaveUpSince(m_MortarSentSerial.Get(squad));
   if (gaveUp)
   {
    m_MortarUnreachable.Set(base, now + MORTAR_UNREACHABLE_MS);
    Say(string.Format("[ACR] AI %1 mortar: %2 can't reach %3, not sending a mortar there for %4 min", m_sFaction, squad.GetName(), base.m_sName, (int)(MORTAR_UNREACHABLE_MS / 60000)));
   }
   // Done with it: given up, gone, set up at the base, or long enough ago.
   bool done = gaveUp || !squad || !base || now - m_MortarSentAt.Get(squad) > MORTAR_SENT_MS;
   if (!done)
   {
    ACR_MortarTeam team = m_Manager.Mortars().TeamOf(squad);
    done = !team || (team.m_eState == ACR_EMortarState.DEPLOYED && vector.DistanceXZ(team.m_Spot, base.GetOrigin()) < 80); // sent within 20 m (and only from radius + 80 out)
   }
   if (!done) continue;
   m_MortarSentTo.Remove(squad);
   m_MortarSentSerial.Remove(squad);
   m_MortarSentAt.Remove(squad);
  }
 }

 // srv:mortargiveup: a send as PositionMortars makes it, the give-up check, and the block.
 void SendMortarForTest(ACR_Squad squad, ACR_Base base)
 {
  squad.MoveTo(ACR_Manager.Ground(base.GetOrigin()));
  m_MortarSentTo.Set(squad, base);
  m_MortarSentSerial.Set(squad, squad.m_iOrderSerial);
  m_MortarSentAt.Set(squad, GetGame().GetWorld().GetWorldTime());
 }
 void NoteMortarGiveUpsForTest() { NoteMortarGiveUps(GetGame().GetWorld().GetWorldTime()); }
 bool MortarBlockedForTest(ACR_Base base)
 {
  float until;
  return m_MortarUnreachable.Find(base, until) && GetGame().GetWorld().GetWorldTime() < until;
 }

 protected void PositionMortars()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  NoteMortarGiveUps(now);
  if (now < m_fNextMortarMove) return;
  array<ACR_MortarTeam> teams = {};
  m_Manager.Mortars().TeamsOf(m_sFaction, teams);
  foreach (ACR_MortarTeam team : teams)
  {
   ACR_Squad squad = team.m_Squad;
   if (!squad || squad.HasPlayer() || team.m_bLost || team.m_bFiring) continue;
   if (team.m_eState == ACR_EMortarState.PACKING || team.m_eState == ACR_EMortarState.DEPLOYING) continue;
   // Shoot-and-scoot: after firing a mission, displace slightly so counter-battery misses
   if (team.m_eState == ACR_EMortarState.DEPLOYED && team.m_iFired > 0 && !team.m_bFiring && team.m_fLastFinished > 0 && (now / 1000) > team.m_fLastFinished + 15.0)
   {
    team.m_iFired = 0;
    team.m_fLastFinished = 0;
    vector scoot = ACR_Manager.Ground(team.m_Spot + Vector(Math.RandomFloat(-40, 40), 0, Math.RandomFloat(-40, 40)));
    squad.MoveTo(scoot);
    m_fNextMortarMove = now + MORTAR_MOVE_MS;
    Say(string.Format("[ACR] AI %1 mortar: %2 shoot-and-scoot displacement", m_sFaction, squad.GetName()));
    continue;
   }
   vector here = PositionOf(squad);
   if (team.m_eState == ACR_EMortarState.DEPLOYED) here = team.m_Spot;
   else if (ACR_Mortars.IsMoving(squad, here)) continue; // on its way
   // Stays while it can reach something it covers (with some margin).
   float reach = ACR_Mortars.MaxRange(team.m_sTube) * 0.85;
   bool inReach = false;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (MortarTarget(other) && vector.DistanceXZ(here, other.GetOrigin()) < reach) inReach = true;
   if (inReach) continue;
   ACR_Base front = MortarBase();
   if (!front || vector.DistanceXZ(here, front.GetOrigin()) < front.m_fRadius + 80) continue;
   vector spot = ACR_Manager.Ground(front.GetOrigin() + Vector(Math.RandomFloat(-20, 20), 0, Math.RandomFloat(-20, 20)));
   squad.MoveTo(spot);
   m_MortarSentTo.Set(squad, front);
   m_MortarSentSerial.Set(squad, squad.m_iOrderSerial);
   m_MortarSentAt.Set(squad, now);
   m_fNextMortarMove = now + MORTAR_MOVE_MS;
   Say(string.Format("[ACR] AI %1 mortar: %2 moving to %3", m_sFaction, squad.GetName(), front.m_sName));
  }
 }

 // ---------------------------------------------------------------- 5. requisition

 protected int FullSizeOf(ACR_Squad squad)
 {
  if (!squad) return 0;
  array<ResourceName> missing = {};
  squad.GetMissing(missing);
  return squad.CountAlive() + missing.Count();
 }

 static bool IsSmallTeamEntry(ACR_RequisitionEntry entry)
 {
  if (!entry || !entry.m_sVehicle.IsEmpty() || entry.m_bMortarTeam) return false;
  if (entry.m_aSoldiers) return entry.m_aSoldiers.Count() <= 3;
  int baseCost = entry.m_iBaseCost;
  if (baseCost <= 0) baseCost = entry.m_iCost;
  if (baseCost <= 9) return true;
  string name = entry.m_sName;
  name.ToLower();
  if (name.Contains("squad") || name.Contains("section") || name.Contains("platoon") || name.Contains("fire team") || name.Contains("fireteam")) return false;
  return name.Contains("team");
 }

 // Refill depleted infantry squads out of contact (especially 1-man remnants that
 // dropped out of combat status) instead of leaving them idle and buying 2-man teams.
 protected bool ReinforceDepleted(ACR_FactionData data, int keep)
 {
  if (!data || !m_Manager.m_Requisition || m_Manager.m_Requisition.OverSoldierCap(m_sFaction)) return false;
  array<ACR_Squad> all = {};
  MySquads(all, false);
  array<ACR_Squad> gone = {};
  foreach (ACR_Squad walker, float until : m_Refitting)
   if (!walker || !all.Contains(walker)) gone.Insert(walker);
  foreach (ACR_Squad lost : gone) m_Refitting.Remove(lost);
  ACR_Squad best;
  int bestScore = -1;
  int bestCost = 0;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_Squad squad : all)
  {
   if (!squad || squad.HasPlayer() || squad.m_bMortar || squad.Aircraft() || ACR_Transport.CrewedVehicle(squad)) continue;
   if (InAssault(squad)) continue;
   float retryAt;
   if (m_ReinforceRetryAt.Find(squad, retryAt))
   {
    if (now < retryAt) continue;
    m_ReinforceRetryAt.Remove(squad);
   }
   array<ResourceName> missing = {};
   squad.GetMissing(missing);
   if (missing.IsEmpty())
   {
    m_Refitting.Remove(squad);
    continue;
   }
   // Builders stay on their FOB or roadblock (ManageFobBuild / ManageRoadblocks own them).
   if (squad.m_iAITask == ACR_EAITask.BUILD_FOB || squad.m_iAITask == ACR_EAITask.ROADBLOCK) continue;
   bool remnant = !squad.IsCombatUnit();
   float giveUpAt;
   bool refitting = m_Refitting.Find(squad, giveUpAt);
   if (!remnant && !refitting && squad.m_iAITask != ACR_EAITask.FREE && squad.m_iAITask != ACR_EAITask.POSTED && squad.m_iAITask != ACR_EAITask.DEFEND && squad.m_iAITask != ACR_EAITask.RESUPPLY) continue;
   if (!remnant && squad.StrengthRatio() > 0.65) continue;
   // A base whose circle it is in now, else the nearest one out of the fighting.
   ACR_Base inside;
   ACR_Base refit;
   float insideDist = float.MAX;
   float refitDist = float.MAX;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner != m_sFaction || !ACR_LogisticsRules.CanReinforce(base) || base.m_bContested) continue;
    float d = vector.DistanceXZ(PositionOf(squad), base.GetOrigin());
    if (d <= base.m_fRadius && d < insideDist) { insideDist = d; inside = base; }
    if (ThreatTo(base) == 0 && d < refitDist) { refitDist = d; refit = base; }
   }
   // Only inside the circle: there the replacements come from this base and the squad
   // gets its ammo too. Badly hurt idle squads walk back out of the fighting to refill
   // (posted ones too: their post's waypoint used to keep them at the front for good);
   // others wait until they pass a base.
   if (!inside)
   {
    // Re-sending the walk every tick reset the squad's waypoint recovery, so an unreachable
    // walk never ended until the 15-min limit (Everon 26 Oct: 200 dropped waypoints).
    if (refitting && (now >= giveUpAt || squad.GaveUpUnreachable()))
    {
     m_Refitting.Remove(squad);
     if (squad.m_iAITask == ACR_EAITask.RESUPPLY) squad.m_iAITask = ACR_EAITask.FREE;
     m_ReinforceRetryAt.Set(squad, now + REINFORCE_RETRY_MS);
     Say(string.Format("[ACR] AI %1: %2 gave up walking back to reinforce", m_sFaction, squad.GetName()));
     continue;
    }
    if (refitting && squad.m_Group && (squad.m_Group.GetCurrentWaypoint() || squad.MovePending())) continue;
    // Don't cut short a walk to a service vehicle (ManageLogistics).
    if (!refitting && squad.m_iAITask == ACR_EAITask.RESUPPLY && squad.m_Group && squad.m_Group.GetCurrentWaypoint()) continue;
    bool idle = squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED || squad.m_iAITask == ACR_EAITask.RESUPPLY;
    if (!refit || !(refitting || remnant || (idle && squad.StrengthRatio() < m_Doctrine.m_fRefitRatio))) continue;
    if (!refitting) m_Refitting.Set(squad, now + REFIT_GIVE_UP_MS);
    squad.m_iAITask = ACR_EAITask.RESUPPLY;
    squad.SetMode(ACR_EMoveMode.FAST);
    Travel(squad, ACR_Manager.Ground(refit.GetOrigin()), false, 0);
    Say(string.Format("[ACR] AI %1: depleted squad %2 (%3% strength) moving to %4 to reinforce", m_sFaction, squad.GetName(), (int)(squad.StrengthRatio() * 100), refit.m_sName));
    continue;
   }
   if (m_Manager.m_Requisition.InContact(squad) != "") continue;
   if (ACR_LogisticsRules.RelayBlocked(ACR_FobSupply.Nearby(m_sFaction, PositionOf(squad)))) continue;
   // Same price ReinforceSquad checks: replacements plus a FOB's instant restock fee.
   int cost = missing.Count() * ACR_Requisition.REINFORCE_COST_PER_SOLDIER;
   ACR_Base depot = m_Manager.m_Requisition.DeployBaseAt(m_sFaction, PositionOf(squad));
   if (depot) cost += m_Manager.m_Requisition.InstantRestockCost(squad, depot);
   if (data.m_iRP - keep < cost)
   {
    if (!m_RefillWaiting) m_RefillWaiting = squad;
    continue;
   }
   int score = missing.Count() * 10;
   if (remnant) score += 100;
   if (score > bestScore) { bestScore = score; best = squad; bestCost = cost; }
  }
  if (!best) return false;
  // ReinforceSquad charges only for soldiers who actually joined.
  int rpBefore = m_Manager.m_Bases.GetRP(m_sFaction);
  string outcome = m_Manager.m_Requisition.ReinforceSquad(null, best);
  if (m_Manager.m_Bases.GetRP(m_sFaction) >= rpBefore)
  {
   // Nothing joined: don't pick the same squad again every tick.
   m_ReinforceRetryAt.Set(best, now + REINFORCE_RETRY_MS);
   Say(string.Format("[ACR] AI %1 reinforce failed, retry in %2 s: %3", m_sFaction, (int)(REINFORCE_RETRY_MS / 1000), outcome));
   return false;
  }
  if (best.m_iAITask == ACR_EAITask.RESUPPLY) best.m_iAITask = ACR_EAITask.FREE;
  m_Refitting.Remove(best);
  Say(string.Format("[ACR] AI %1 reinforce: %2", m_sFaction, outcome));
  return true;
 }

 protected void Requisition()
 {
  if (ACR_DoctrineKits.Pending(m_sFaction)) return; // its starting kit first
  ACR_FactionSettings settings = Settings();
  int cap = 8;
  if (settings) cap = settings.m_iAIUnitCap;
  // Money piling up: field a few more squads rather than sit on it.
  int banked = m_Manager.m_Bases.GetRP(m_sFaction);
  int maxCapBonus = 4;
  int myBases = 0;
  foreach (ACR_Base b : ACR_Base.s_All) if (b.m_sOwner == m_sFaction) myBases++;
  if (ACR_Base.s_All.Count() > 24 && myBases > ACR_Base.s_All.Count() / 2 && banked >= 400) maxCapBonus = 8;
  if (banked > 150) cap += Math.Min(maxCapBonus, (banked - 100) / 50);
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data) return;
  // Refills come first and are treated as urgent troops: they may spend the fire-support
  // reserve (only the engineering budget is kept), and while a squad at a base waits for the
  // RP to refill, no new units are bought. Everon 26 Oct: refills kept the whole 75 RP
  // reserve the AI rarely had, so in nearly 3 h RU refilled 3 squads and bought 21 units
  // while 4-7 squads per side sat at bases with 1-3 men.
  m_RefillWaiting = null;
  ReinforceDepleted(data, BuildKeep());
  if (m_RefillWaiting)
  {
   float refillNow = GetGame().GetWorld().GetWorldTime();
   if (refillNow >= m_fNextRefillLog)
   {
    m_fNextRefillLog = refillNow + 60000;
    Say(string.Format("[ACR] AI %1: holding purchases until %2 can be refilled (%3 RP)", m_sFaction, m_RefillWaiting.GetName(), data.m_iRP));
   }
   return;
  }
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  // Only fighting units count against the cap (trucks and lone drivers don't).
  int combatLine = 0;
  float capUsed = 0;
  int supportTeams = 0;
  foreach (ACR_Squad existing : mine)
  {
   if (IsSpecialistSquad(existing)) supportTeams++;
   else if (existing.IsCombatUnit())
   {
    combatLine++;
    capUsed += CapSlots(existing);
   }
  }
  if (capUsed >= cap) return;
  // Deploy at the forward-most safe deployment base (military/airfield/FOB with
  // ThreatTo == 0 and at least FOB_DEPLOY_MIN_FRONT from any non-owned base);
  // fall back to the furthest rear base when all forward bases are under threat.
  ACR_Base spawn;
  ACR_Base forwardSpawn;
  float best = -1;
  float bestForward = 100000;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || !ACR_LogisticsRules.CanDeploy(base) || base.m_bContested) continue;
   // distance to the nearest non-owned base = how far behind the lines it is
   float nearestFront = 100000;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction) nearestFront = Math.Min(nearestFront, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (ThreatTo(base) > 0) nearestFront = 0;
   else if (nearestFront >= FOB_DEPLOY_MIN_FRONT)
   {
    float forwardScore = nearestFront;
    if (base.m_bBuilt) forwardScore -= 150;
    if (forwardScore < bestForward) { bestForward = forwardScore; forwardSpawn = base; }
   }
   if (nearestFront > best) { best = nearestFront; spawn = base; }
  }
  if (forwardSpawn) spawn = forwardSpawn;
  if (!spawn) return;
  // A service truck for a need nobody can meet (squads low on ammo, a wrecked
  // BRDM...), one of each kind at a time.
  if (m_Manager.m_Logistics)
  {
   array<int> services = {ACR_EService.AMMO, ACR_EService.REPAIR, ACR_EService.FUEL, ACR_EService.MEDICAL};
   foreach (int service : services)
   {
    if ((m_iNeeds & service) == 0 || m_Manager.m_Logistics.CountStations(m_sFaction, service) > 0) continue;
    int pick = -1;
    foreach (int e, ACR_RequisitionEntry offer : data.m_aRequisitions)
    {
     if (offer.m_iRemaining <= 0 || offer.m_iCost > data.m_iRP || offer.m_sVehicle.IsEmpty()) continue;
     if ((ACR_Logistics.ServiceOfName(offer.m_sVehicle) & service) == 0) continue;
     if (pick < 0 || offer.m_iCost < data.m_aRequisitions[pick].m_iCost) pick = e;
    }
    if (pick < 0) continue;
    Say(string.Format("[ACR] AI %1 requisition (%2 support): %3", m_sFaction, ACR_Logistics.ServiceName(service), m_Manager.PurchaseAt(m_sFaction, pick, spawn)));
    return;
   }
  }
  // Squads keep having to walk far with no truck to fetch them: buy a transport first.
  int maxTransports = 4;
  if (ACR_Manager.ThreatOption() == 2) maxTransports = 5;
  maxTransports += m_Doctrine.m_iTransports;
  if (m_Manager.m_Rides && m_Manager.m_Rides.ShortOfTransport(m_sFaction) && m_Manager.m_Rides.CountTransports(m_sFaction) < maxTransports)
  {
   // The cheapest truck; a rich side (#28) takes an armed one (an APC) when it has one.
   bool armedRide = FireFactor() > 1;
   int truck = -1;
   foreach (int t, ACR_RequisitionEntry candidate : data.m_aRequisitions)
   {
    if (!candidate.m_bTransport || IsServiceVehicle(candidate) || candidate.m_iRemaining <= 0 || candidate.m_iCost > data.m_iRP) continue;
    if (truck < 0) { truck = t; continue; }
    ACR_RequisitionEntry held = data.m_aRequisitions[truck];
    if (armedRide && candidate.m_bArmed != held.m_bArmed) { if (candidate.m_bArmed) truck = t; continue; }
    if (candidate.m_iCost < held.m_iCost) truck = t;
   }
   if (truck >= 0)
   {
    Say(string.Format("[ACR] AI %1 requisition (transport): %2", m_sFaction, m_Manager.PurchaseAt(m_sFaction, truck, spawn)));
    m_Manager.m_Rides.m_NoTransportAt.Remove(m_sFaction);
    return;
   }
  }
  // Buy up to two units per tick. The pick is weighted by price, so real fighting
  // power (rifle squads, MG teams, armed vehicles) wins over a swarm of cheap
  // two-man teams; specialist teams (engineers, sappers, medics, logistics,
  // recon) are rare and never doubled up.
  // Keep a fire-support budget unless troops are needed more.
  int keep = 0;
  bool highThreat = ACR_Manager.ThreatOption() == 2;
  bool urgent = UrgentTroops(combatLine, cap);
  if (highThreat)
  {
   // High intensity should field its fire support even while the infantry pool
   // is still building. These checks remain stock/RP/pad gated and buy at most
   // one support unit per strategy tick.
   keep = SupportBudget();
   if (BuyMortarTeam(spawn, data)) return;
   if (BuyHelicopter(data)) return;
  }
  else if (!urgent || m_Doctrine.m_bSupportsFirst)
  {
   // Firepower still buys its guns when short of troops, but doesn't sit on mission money then.
   if (!urgent) keep = SupportBudget();
   if (BuyMortarTeam(spawn, data)) return;
   if (BuyHelicopter(data)) return;
  }
  keep += BuildKeep(); // the engineering budget (-acrAIBuildShare) isn't for units
  int smallTeams, lineSquads, vehicles;
  foreach (ACR_Squad existing : mine)
  {
   if (ACR_Transport.CrewedVehicle(existing)) vehicles++;
   else if (FullSizeOf(existing) <= 3) smallTeams++;
   else lineSquads++;
  }
  bool hasLineOffer = false;
  foreach (ACR_RequisitionEntry checkOffer : data.m_aRequisitions)
  {
   if (checkOffer.m_iRemaining > 0 && checkOffer.m_sVehicle.IsEmpty() && !checkOffer.m_bMortarTeam && !IsSmallTeamEntry(checkOffer) && !IsSpecialistTeam(checkOffer))
   {
    hasLineOffer = true;
    break;
   }
  }
  int budget = data.m_iRP - keep;
  // Save up for a pricier unit only with a working force in the field; short of
  // troops, or when troops are urgent, buy what is affordable now.
  int saveMax = Math.Max(budget, 85);
  if (combatLine < SAVE_MIN_UNITS || (urgent && !highThreat)) saveMax = budget;
  if (m_iSaveForEntry >= 0 && m_iSaveForEntry < data.m_aRequisitions.Count())
  {
   ACR_RequisitionEntry savedEntry = data.m_aRequisitions[m_iSaveForEntry];
   if (savedEntry.m_iRemaining <= 0 || savedEntry.m_iCost > saveMax || (combatLine < 2 && savedEntry.m_iCost > budget) || (savedEntry.m_eFormation == ACR_EFormation.SF && HasSpecialForces()))
    m_iSaveForEntry = -1;
   else if (savedEntry.m_iCost > budget)
    return;
  }
  else m_iSaveForEntry = -1;

  bool boughtSF;
  float richFactor = FireFactor();
  // The cap counts fighting units only (as above): trucks, ambulances, crewless
  // vehicles and 1-man remnants filled it on Everon and the AI sat on 1000 RP.
  for (int n = 0; n < 2 && capUsed < cap; n++)
  {
   budget = data.m_iRP - keep;
   int pick = -1;
   if (m_iSaveForEntry >= 0 && m_iSaveForEntry < data.m_aRequisitions.Count() && data.m_aRequisitions[m_iSaveForEntry].m_iRemaining > 0 && data.m_aRequisitions[m_iSaveForEntry].m_iCost <= budget)
   {
    pick = m_iSaveForEntry;
    m_iSaveForEntry = -1;
    m_iLastSavedLog = -1;
   }
   else
   {
    array<int> options = {};
    array<float> weights = {};
    float total;
    foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
    {
     if (entry.m_iRemaining <= 0 || entry.m_iCost > saveMax) continue;
     if (entry.m_eFormation == ACR_EFormation.SF && (boughtSF || HasSpecialForces())) continue;
     if (combatLine < 2 && entry.m_iCost > budget) continue;
     if (supportTeams >= 2 && IsSpecialistTeam(entry)) continue;
     if (entry.m_bMortarTeam) continue; // bought on its own (BuyMortarTeam)
     // AI can't fly: a bought helicopter would just sit at base. Gunships come as supports instead.
     if (entry.m_bAircraft || entry.m_sVehicle.Contains("/Helicopters/") || entry.m_sVehicle.Contains("/Planes/")) continue;
     // Nor service vehicles (bought on need) or unarmed cars (nothing to fight with).
     if (IsServiceVehicle(entry)) continue;
     if (!entry.m_sVehicle.IsEmpty() && !entry.m_bArmed) continue;
     bool isSapper = IsSapperEntry(entry);
     bool smallTeam = IsSmallTeamEntry(entry);
     if (smallTeam && !(isSapper && HasEnemyFob()) && hasLineOffer && smallTeams >= 2 && smallTeams * 2 >= lineSquads)
     {
      // Raiders run more small teams, up to two per line squad.
      if (!m_Doctrine.m_bSmallTeamsFree || (smallTeams >= 3 && smallTeams >= lineSquads * 2)) continue;
     }
     // Weigh by the base price: a unit whose stepped price has climbed isn't worth more.
     float weight = ACR_InfantryCatalog.AIWeight(entry);
     if (entry.m_sVehicle.IsEmpty() && !smallTeam && !IsSpecialistTeam(entry)) weight *= 1.8;
     else if (smallTeam && !(isSapper && HasEnemyFob())) weight *= 0.35;
     if (smallTeam) weight *= m_Doctrine.m_fSmallTeamWeight;
     if (!entry.m_sVehicle.IsEmpty()) weight *= m_Doctrine.m_fVehicleWeight;
     if (highThreat && !entry.m_sVehicle.IsEmpty()) weight *= 1.5;
     if (highThreat && m_sFaction == "RHS_AFRF" && IsRUArmorEntry(entry)) weight *= 2;
     if (!entry.m_sVehicle.IsEmpty() && vehicles == 0 && combatLine >= 3) weight *= 1.6;
     if (isSapper && HasEnemyFob())
     {
      if (!OwnsLike(entry)) weight *= 3.0;
     }
     else if (IsSpecialistTeam(entry))
     {
      if (OwnsLike(entry)) continue;
      weight *= 0.3;
      if (entry.m_bObserverTeam) weight *= m_Doctrine.m_fObserverWeight;
     }
     // Keep infantry the backbone (less so for a rich side, which buys quality: #28).
     if (!entry.m_sVehicle.IsEmpty()) weight *= Math.Min(1, 0.5 * richFactor);
     weight *= QualityWeight(entry, richFactor);
     // A low-threat war is mostly fought on foot.
     if (!entry.m_sVehicle.IsEmpty() && ACR_Manager.ThreatOption() == 0) weight *= 0.4;
     // Price-weighting favours dear units; halve those not yet affordable so saving stays the exception.
     if (entry.m_iCost > budget) weight *= 0.5;
     options.Insert(i);
     weights.Insert(weight);
     total += weight;
    }
    if (options.IsEmpty()) return;
    float roll = Math.RandomFloat(0, total);
    pick = options[options.Count() - 1];
    foreach (int k, float w : weights)
    {
     roll -= w;
     if (roll <= 0) { pick = options[k]; break; }
    }
   }
   ACR_RequisitionEntry chosen = data.m_aRequisitions[pick];
   if (chosen.m_iCost > data.m_iRP - keep)
   {
    m_iSaveForEntry = pick;
    if (m_iLastSavedLog != pick)
    {
     m_iLastSavedLog = pick;
     Say(string.Format("[ACR] AI %1 saving RP for %2 (%3/%4 RP)", m_sFaction, chosen.m_sName, Math.Max(0, data.m_iRP - keep), chosen.m_iCost));
    }
    return;
   }
   int stockBefore = chosen.m_iRemaining;
   string result = m_Manager.PurchaseAt(m_sFaction, pick, spawn);
   if (chosen.m_eFormation == ACR_EFormation.SF && chosen.m_iRemaining < stockBefore) boughtSF = true;
   Say(string.Format("[ACR] AI %1 requisition: %2", m_sFaction, result));
   if (!chosen.m_sVehicle.IsEmpty()) vehicles++;
   else if (IsSmallTeamEntry(chosen)) smallTeams++;
   else lineSquads++;
   if (m_Doctrine.m_bSmallTeamsFree && IsSmallTeamEntry(chosen)) capUsed += 0.5;
   else capUsed += 1;
  }
 }

 // Quality over quantity (#28 step 3): a rich side (factor over 1) weighs units by their base price
 // once more, scaled to the factor: at the RP cap a 76 RP APC weighs three times, a 12 RP team half.
 static float QualityWeight(ACR_RequisitionEntry entry, float factor)
 {
  if (factor <= 1) return 1;
  float price = entry.m_iBaseCost;
  if (price <= 0) price = entry.m_iCost;
  if (price <= 0) return 1;
  return Math.Pow(price / 25.0, factor - 1);
 }

 // Share of the AI squad cap a fighting unit takes: one, or half for a 2-3 man team of a
 // doctrine built on them (Raider), so it fields about as many men as the others
 // (the soldier cap still bounds the total).
 protected float CapSlots(ACR_Squad squad)
 {
  if (m_Doctrine.m_bSmallTeamsFree && !ACR_Transport.CrewedVehicle(squad) && FullSizeOf(squad) <= 3) return 0.5;
  return 1;
 }
 float CapSlotsForTest(ACR_Squad squad) { return CapSlots(squad); }

 protected bool IsRUArmorEntry(ACR_RequisitionEntry entry)
 {
  if (!entry || entry.m_sVehicle.IsEmpty()) return false;
  string vehicle = FilePath.StripExtension(FilePath.StripPath(entry.m_sVehicle));
  vehicle.ToLower();
  return vehicle.Contains("btr70") || vehicle.Contains("brdm2");
 }

 static bool IsSapperEntry(ACR_RequisitionEntry entry)
 {
  if (!entry) return false;
  if (entry.m_eFormation != ACR_EFormation.NONE) return entry.m_eFormation == ACR_EFormation.ENGINEER;
  string name = entry.m_sName;
  name.ToLower();
  if (name.Contains("sapper") || name.Contains("engineer")) return true;
  string grp = entry.m_sGroup;
  grp.ToLower();
  if (grp.Contains("sapper") || grp.Contains("engineer")) return true;
  return false;
 }

 bool HasEnemyFob()
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner == m_sFaction || base.m_sOwner.IsEmpty()) continue;
   if (base.m_bBuilt && Visible(base)) return true;
  }
  return false;
 }

 static bool IsSpecialistSquad(ACR_Squad squad)
 {
  if (!squad || squad.IsMounted() || ACR_Transport.CrewedVehicle(squad) || squad.m_bMortar || squad.Aircraft()) return false;
  if (squad.m_Group && squad.m_Group.m_eACRFormation != ACR_EFormation.NONE) return ACR_InfantryCatalog.Specialist(squad.m_Group.m_eACRFormation);
  string name = squad.GetName();
  name.ToLower();
  array<string> words = {"engineer", "sapper", "medic", "logistic", "supply", "recon", "sniper", "hq", "radio", "observer", "scout", "spotter"};
  foreach (string word : words)
   if (name.Contains(word)) return true;
  return false;
 }

 static bool IsSpecialistTeam(ACR_RequisitionEntry entry)
 {
  if (!entry) return false;
  if (entry.m_eFormation != ACR_EFormation.NONE) return ACR_InfantryCatalog.Specialist(entry.m_eFormation);
  if (entry.m_bObserverTeam) return true;
  string name = entry.m_sName;
  name.ToLower();
  array<string> words = {"engineer", "sapper", "medic", "logistic", "supply", "recon", "sniper", "hq", "radio", "observer", "scout", "spotter"};
  foreach (string word : words)
   if (name.Contains(word)) return true;
  return false;
 }

 protected bool HasSpecialForces()
 {
  // Count all live SF formations, including player-held or currently airlifted
  // ones; those exclusions belong to order assignment, not purchase scarcity.
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (squad && squad.m_Group && squad.GetFaction() && squad.GetFaction().GetFactionKey() == m_sFaction && squad.CountAlive() > 0 && squad.m_Group.m_eACRFormation == ACR_EFormation.SF) return true;
  return false;
 }

 // Already have a unit bought from this entry (names are numbered per purchase).
 protected bool OwnsLike(ACR_RequisitionEntry entry)
 {
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
   if (squad.GetName().StartsWith(entry.m_sName)) return true;
  return false;
 }

 // ---------------------------------------------------------------- 6. logistics
 // Squads low on ammo, hurt, or with a damaged / thirsty vehicle get the matching
 // service vehicle: it drives out to squads holding a position, while free
 // squads go back to it. Idle service vehicles wait at the safest base nearest
 // the front, and go home to restock when they run low. Lose the truck and the
 // front runs dry.
 static const int LOW_AMMO = 45;
 static const int OUT_OF_AMMO = 15;           // below this a squad in a fight pulls back
 static const int LOW_HEALTH = 60;
 static const int LOW_FUEL = 25;
 static const float SERVICE_REACH = 2000;     // farthest a service vehicle is sent or sought
 protected int m_iNeeds;                      // services wanted this tick (for requisition)
 protected ref map<ACR_Squad, ACR_Squad> m_ServiceJobs = new map<ACR_Squad, ACR_Squad>(); // service crew -> squad it is serving

 protected ACR_Squad CrewOf(IEntity vehicle)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (ACR_Transport.CrewedVehicle(squad) == vehicle) return squad;
  return null;
 }

 protected bool InAssault(ACR_Squad squad)
 {
  foreach (ACR_AIAttack attack : m_Attacks)
   if (attack.m_bAssaulting && !attack.m_bGrab && attack.m_Squads.Contains(squad)) return true;
  return false;
 }

 protected int NeedsOf(ACR_Squad squad)
 {
  int need;
  if (squad.m_iAmmoPct >= 0 && squad.m_iAmmoPct < LOW_AMMO) need |= ACR_EService.AMMO;
  if (squad.m_iAvgHpPct < LOW_HEALTH) need |= ACR_EService.MEDICAL;
  IEntity vehicle = ACR_Transport.CrewedVehicle(squad);
  if (vehicle)
  {
   if (ACR_Logistics.Health(vehicle) * 100 < LOW_HEALTH) need |= ACR_EService.REPAIR;
   float fuel = ACR_Logistics.Fuel(vehicle);
   if (fuel >= 0 && fuel * 100 < LOW_FUEL) need |= ACR_EService.FUEL;
  }
  return need;
 }

 protected void ManageLogistics()
 {
  ACR_Logistics logistics = m_Manager.m_Logistics;
  m_iNeeds = 0;
  if (!logistics) return;
  // Finished jobs: the served squad no longer needs it, or either side is gone.
  array<ACR_Squad> done = {};
  foreach (ACR_Squad jobCrew, ACR_Squad jobClient : m_ServiceJobs)
   if (!jobCrew || !jobClient || !m_Manager.m_Squads.Contains(jobCrew) || !m_Manager.m_Squads.Contains(jobClient) || NeedsOf(jobClient) == 0 || jobCrew.OrderState() == ACR_EOrderState.HOLD) done.Insert(jobCrew);
  foreach (ACR_Squad finished : done) m_ServiceJobs.Remove(finished);
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   int need = NeedsOf(squad);
   // Walking back for replacements (ReinforceDepleted) is a resupply with no service need.
   if (squad.m_iAITask == ACR_EAITask.RESUPPLY && need == 0 && !m_Refitting.Contains(squad))
   {
    squad.m_iAITask = ACR_EAITask.FREE;
    Say(string.Format("[ACR] AI %1: %2 resupplied", m_sFaction, squad.GetName()));
   }
   if (need == 0 || InAssault(squad) || ACR_Logistics.ServiceOf(ACR_Transport.CrewedVehicle(squad)) != 0) continue;
   m_iNeeds |= need;
   array<int> services = {ACR_EService.AMMO, ACR_EService.REPAIR, ACR_EService.FUEL, ACR_EService.MEDICAL};
   foreach (int service : services)
   {
    if ((need & service) == 0) continue;
    array<IEntity> stations = {};
    logistics.Stations(m_sFaction, service, stations);
    IEntity station;
    float best = SERVICE_REACH;
    foreach (IEntity candidate : stations)
    {
     float d = vector.DistanceXZ(candidate.GetOrigin(), PositionOf(squad));
     if (d < best) { best = d; station = candidate; }
    }
    if (!station || best < ACR_Logistics.SERVICE_RANGE) continue; // none, or already being served
    ACR_Squad crew = CrewOf(station);
    bool holding = squad.m_iAITask == ACR_EAITask.DEFEND || squad.m_iAITask == ACR_EAITask.POSTED || !squad.m_Group.GetCurrentWaypoint();
    bool crewFree = crew && !crew.HasPlayer() && !m_ServiceJobs.Contains(crew) && !StuckLately(crew);
    string what = ACR_Logistics.ServiceName(service);
    if (crewFree && (holding || squad.IsMounted() == false && best > 800) && !DangerOnTheWay(crew, PositionOf(squad)))
    {
     // Bring the truck to them.
     crew.SetMode(ACR_EMoveMode.FAST);
     crew.RouteTo(ACR_Manager.Ground(PositionOf(squad)), ACR_Squad.WP_MOVE, -1, "Resupplying " + squad.GetName(), 0);
     m_ServiceJobs.Set(crew, squad);
     Say(string.Format("[ACR] AI %1: %2 drives out to %3 (%4)", m_sFaction, crew.GetName(), squad.GetName(), what));
    }
    else if (squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED)
    {
     // Go back to the truck.
     squad.m_iAITask = ACR_EAITask.RESUPPLY;
     squad.SetMode(ACR_EMoveMode.FAST);
     Travel(squad, ACR_Manager.Ground(station.GetOrigin()), false, 0);
     Say(string.Format("[ACR] AI %1: %2 falls back to the %3 truck", m_sFaction, squad.GetName(), what));
    }
    break;
   }
  }
  // Idle service vehicles: restock if low, otherwise wait at the supply point.
  ACR_Base supplyPoint = SupplyPoint();
  foreach (ACR_Squad idleCrew : mine)
  {
   IEntity serviceVehicle = ACR_Transport.CrewedVehicle(idleCrew);
   int serviceMask = ACR_Logistics.ServiceOf(serviceVehicle) & ~ACR_EService.CONSTRUCTION;
   if (!serviceVehicle || serviceMask == 0 || idleCrew.HasPlayer() || m_ServiceJobs.Contains(idleCrew)) continue;
   if (idleCrew.OrderState() == ACR_EOrderState.BUILD_FOB || idleCrew.m_iAITask == ACR_EAITask.BUILD_FOB) continue;
   if (IsAssistingFobUpgrade(idleCrew) || StuckLately(idleCrew)) continue;
   ACR_Base goal = supplyPoint;
   if (logistics.StockPercent(serviceVehicle) < 25) goal = HomeBase();
   if (!goal || vector.DistanceXZ(serviceVehicle.GetOrigin(), goal.GetOrigin()) < goal.m_fRadius) continue;
   if (idleCrew.OrderState() == ACR_EOrderState.MOVE && vector.DistanceXZ(idleCrew.RouteDestination(), goal.GetOrigin()) < goal.m_fRadius + 80) continue;
   if (DangerOnTheWay(idleCrew, goal.GetOrigin())) continue;
   idleCrew.RouteTo(ACR_Manager.Ground(goal.GetOrigin()), ACR_Squad.WP_MOVE, -1, "Moving", 0);
   Say(string.Format("[ACR] AI %1: %2 to %3 (%4)", m_sFaction, idleCrew.GetName(), goal.m_sName, ACR_Logistics.ServiceName(serviceMask)));
  }
 }

 // ---------------------------------------------------------------- dangerous roads (#89)
 // Where FIA cost this side men on a road (ACR_Insurgency): a free combat squad goes to clear it
 // (escorting the road), and supply runs whose way passes it wait until it is cleared or the mark
 // runs out. A road gets two sweepers at most (one that can't get there is replaced once).
 protected float m_fDangerSaidAt = -1000000;
 protected bool DangerOnTheWay(ACR_Squad crew, vector destination)
 {
  ACR_DangerRoad road = ACR_Insurgency.OnTheWay(m_sFaction, PositionOf(crew), destination);
  if (!road) return false;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_fDangerSaidAt > 60000)
  {
   m_fDangerSaidAt = now;
   Say(string.Format("[ACR] AI %1: %2 waits, FIA ambushed the road at %3", m_sFaction, crew.GetName(), ACR_Manager.Grid(road.m_vPos)));
  }
  return true;
 }

 static const float SWEEP_REACH = 3000;
 protected void ManageDangerRoads()
 {
  if (ACR_Insurgency.s_aRoads.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_DangerRoad> cleared = {};
  foreach (ACR_DangerRoad road : ACR_Insurgency.s_aRoads)
  {
   if (road.m_sSide != m_sFaction) continue;
   ACR_Squad sweeper = road.m_Sweeper;
   if (sweeper && (!m_Manager.m_Squads.Contains(sweeper) || sweeper.CountAlive() <= 0 || sweeper.HasPlayer() || sweeper.m_iAITask != ACR_EAITask.SWEEP || sweeper.GaveUpUnreachable()))
   {
    if (sweeper.m_iAITask == ACR_EAITask.SWEEP) sweeper.m_iAITask = ACR_EAITask.FREE;
    road.m_Sweeper = null;
    road.m_fQuietSince = 0;
    sweeper = null;
   }
   if (!sweeper)
   {
    if (road.m_iSweeps >= 2) continue;
    sweeper = FindSweeper(road.m_vPos);
    if (!sweeper) continue;
    road.m_Sweeper = sweeper;
    road.m_iSweeps++;
    sweeper.m_iAITask = ACR_EAITask.SWEEP;
    sweeper.SetMode(ACR_EMoveMode.COMBAT);
    Travel(sweeper, road.m_vPos, false, 0);
    Say(string.Format("[ACR] AI %1: %2 to clear the road at %3 (FIA ambush)", m_sFaction, sweeper.GetName(), ACR_Manager.Grid(road.m_vPos)));
    continue;
   }
   if (vector.DistanceXZ(PositionOf(sweeper), road.m_vPos) > 80 || sweeper.KnownEnemies(300) > 0)
   {
    road.m_fQuietSince = 0;
    continue;
   }
   if (road.m_fQuietSince <= 0) road.m_fQuietSince = now;
   if (now - road.m_fQuietSince >= ACR_Insurgency.CLEAR_QUIET_MS) cleared.Insert(road);
  }
  foreach (ACR_DangerRoad done : cleared) ACR_Insurgency.Clear(done, done.m_Sweeper.GetName() + " found nobody there");
 }

 protected ACR_Squad FindSweeper(vector at)
 {
  array<ACR_Squad> free = {};
  MySquads(free, true);
  SortByDistance(free, at);
  foreach (ACR_Squad squad : free)
  {
   if (squad.CountAlive() < 2 || IsSupportTeam(squad) || !squad.IsCombatUnit()) continue;
   if (vector.DistanceXZ(PositionOf(squad), at) > SWEEP_REACH) return null;
   return squad;
  }
  return null;
 }
 void ManageDangerRoadsForTest() { ManageDangerRoads(); }

 // A service vehicle that gave up stuck (ACR_Squad.m_fStuckAt: holding where it is) waits before
 // it is sent anywhere again, as Recrew does. Jacob's 26 Oct game: an arsenal truck stuck by its
 // spawn at Saint-Philippe Factory was sent to Tyrone 59 times in 2.5 h, 353 stuck lines.
 // Giving up a move as unreachable counts too (RHS soak 28 Oct: a fuel truck that couldn't
 // path off its spot was sent again every 45 s for 22 min, 31 give-ups).
 static const float STUCK_SERVICE_WAIT_MS = 600000;
 protected bool StuckLately(ACR_Squad crew)
 {
  if (!crew) return false;
  float now = GetGame().GetWorld().GetWorldTime();
  return now - crew.m_fStuckAt < STUCK_SERVICE_WAIT_MS || now - crew.m_fGaveUpAt < STUCK_SERVICE_WAIT_MS;
 }
 bool StuckLatelyForTest(ACR_Squad crew) { return StuckLately(crew); }

 protected bool IsAssistingFobUpgrade(ACR_Squad squad)
 {
  if (!squad) return false;
  if (squad.m_Order == "Assisting FOB upgrade construction" && squad.m_Group && squad.m_Group.GetCurrentWaypoint()) return true;
  vector pos = PositionOf(squad);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || !base.m_bBuilt || base.m_sOwner != m_sFaction || !base.m_aFobSlots) continue;
   if (vector.DistanceXZ(pos, base.GetOrigin()) > base.m_fRadius) continue;
   if (ACR_FobSlots.AnyBuilding(base)) return true;
  }
  return false;
 }

 protected ACR_Base m_SupplyPoint;

 // Our base nearest the front that isn't threatened: where service vehicles wait.
 protected ACR_Base SupplyPoint()
 {
  ACR_Base best;
  float bestFront = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || base.m_bContested || ThreatTo(base) > 0) continue;
   float front = float.MAX;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction && Visible(other)) front = Math.Min(front, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (front < 500) continue; // too close to the enemy to park trucks
   float score = front;
   if (base == m_SupplyPoint) score -= 450; // stay put unless another safe base is much closer to the front
   if (score < bestFront) { bestFront = score; best = base; }
  }
  if (!best) best = HomeBase();
  m_SupplyPoint = best;
  return best;
 }

 // ---------------------------------------------------------------- FOB establishment (Stage 3)
 // (Front gap before a FOB: the doctrine's m_fFobFront, 1.5 km standard.)
 static const float FOB_EXISTING_PROXIMITY = 1000;   // don't build within 1 km of existing friendly FOB
 static const int FOB_RP_COMBAT_RESERVE = 30;        // RP reserve required when buying construction truck
 static const float FOB_EVAL_INTERVAL_MS = 30000;    // evaluate every 30 seconds

 static const float FOB_TRUCK_RETRY_MS = 300000;    // at most one construction truck purchase per 5 min
 static const float FOB_SITE_AVOID_MS = 600000;     // an abandoned site is skipped for 10 min
 static const float FOB_SITE_AVOID_RADIUS = 150;
 static const float FOB_SITE_RETRY_MS = 300000;     // after a failed site search
 static const float FOB_SITE_STEP_MS = 5000;        // between the steps of a site search (ACR_FobSiting)

 protected ACR_Squad m_FobBuilder;
 protected ACR_Squad m_FobEscort;
 protected vector m_vFobTargetSite;
 protected float m_fNextFobEval;
 protected float m_fNextRoadblockEval;
 protected float m_fNextMinefieldEval;
 protected float m_fNextFortifyEval;
 protected float m_fFobTruckBoughtAt = -FOB_TRUCK_RETRY_MS;
 protected ref array<vector> m_aAvoidSites = {};
 protected ref array<float> m_aAvoidUntil = {};

 bool HasActiveFobProject()
 {
  if (m_FobBuilder)
  {
   if (m_FobBuilder.CountAlive() > 0 && m_FobBuilder.OrderState() == ACR_EOrderState.BUILD_FOB) return true;
   // Given up (3 min without progress) or wiped out short of completion: avoid that site for a while.
   bool finished = false;
   foreach (ACR_Base built : ACR_Base.s_All)
    if (built && built.m_bBuilt && !built.m_bRetired && vector.DistanceXZ(built.GetOrigin(), m_vFobTargetSite) < 20) finished = true;
   // Finished just now: the FOB appears on the next structures tick, after its HQ (a check in
   // that gap used to avoid the new FOB's own site).
   if (!finished && m_FobBuilder.BuiltFobAt(m_vFobTargetSite)) finished = true;
   if (!finished) AvoidSite(m_vFobTargetSite);
   m_FobBuilder = null;
   if (m_FobEscort && m_FobEscort.m_iAITask == ACR_EAITask.DEFEND && !m_FobEscort.m_AITarget)
   {
    m_FobEscort.m_iAITask = ACR_EAITask.FREE;
   }
   m_FobEscort = null;
  }
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (squad.OrderState() == ACR_EOrderState.BUILD_FOB || squad.m_iAITask == ACR_EAITask.BUILD_FOB) return true;
  }
  return false;
 }

 ACR_Base NearestDeployBase(ACR_Base fromBase)
 {
  if (!fromBase) return null;
  ACR_Base best;
  float bestDist = float.MAX;
  foreach (ACR_Base candidate : ACR_Base.s_All)
  {
   if (candidate.m_sOwner != m_sFaction || candidate.m_bRetired || !ACR_LogisticsRules.CanDeploy(candidate)) continue;
   float d = vector.DistanceXZ(fromBase.GetOrigin(), candidate.GetOrigin());
   if (d < bestDist)
   {
    bestDist = d;
    best = candidate;
   }
  }
  return best;
 }

 ACR_Base NeedsFob()
 {
  array<ACR_Base> frontBases = {};
  FrontlineBases(frontBases);
  if (frontBases.IsEmpty()) return null;

  foreach (ACR_Base front : frontBases)
  {
   if (!front) continue;
   float nearestDeploy = float.MAX;
   foreach (ACR_Base candidate : ACR_Base.s_All)
   {
    if (candidate.m_sOwner != m_sFaction || candidate.m_bRetired || !ACR_LogisticsRules.CanDeploy(candidate)) continue;
    float d = vector.DistanceXZ(front.GetOrigin(), candidate.GetOrigin());
    if (d < nearestDeploy) nearestDeploy = d;
   }

   if (nearestDeploy >= m_Doctrine.m_fFobFront)
   {
    bool hasCloseFob = false;
    foreach (ACR_Base existingFob : ACR_Base.s_All)
    {
     if (existingFob.m_sOwner != m_sFaction || !existingFob.m_bBuilt || existingFob.m_bRetired) continue;
     if (vector.DistanceXZ(front.GetOrigin(), existingFob.GetOrigin()) < FOB_EXISTING_PROXIMITY)
     {
      hasCloseFob = true;
      break;
     }
    }
    if (!hasCloseFob) return front;
   }
  }
  return null;
 }

 // A FOB site behind 'front' (ACR_FobSiting: the old offsets, scored), the whole search at once.
 bool FindFobSite(ACR_Base front, out vector site)
 {
  bool pending;
  return FindFobSiteStep(front, site, -1, pending);
 }

 // The scored search, 'most' more offsets validated (-1: all that are left); 'pending' while some
 // are still to be validated (ManageFobBuild spreads a search over a few ticks).
 protected string m_sFobSiteNone;
 bool FindFobSiteStep(ACR_Base front, out vector site, int most, out bool pending)
 {
  site = vector.Zero;
  pending = false;
  m_sFobSiteNone = "";
  if (!front) return false;
  ACR_Base deployBase = NearestDeployBase(front);
  vector dir = Vector(0, 0, -1);
  if (deployBase)
  {
   vector toDeploy = deployBase.GetOrigin() - front.GetOrigin();
   toDeploy[1] = 0;
   if (toDeploy.LengthSq() > 0.001)
   {
    toDeploy.Normalize();
    dir = toDeploy;
   }
  }
  array<ref ACR_FobSite> sites = {};
  string none;
  if (!ACR_FobSiting.Step(m_sFaction, front, dir, NearestEnemyBaseTo(front), most, sites, none))
  {
   pending = true;
   return false;
  }
  Avoided(vector.Zero); // drops the expired avoided sites
  ACR_FobSite best = ACR_FobSiting.Best(sites, m_aAvoidSites, string.Format("%1 %2 %3", m_sFaction, front.m_sName, m_aAvoidSites.Count()));
  if (!best)
  {
   m_sFobSiteNone = none;
   if (none.IsEmpty()) m_sFobSiteNone = string.Format("all %1 clear sites abandoned recently", sites.Count());
   return false;
  }
  site = best.m_vPos;
  NoteBuild("fob site " + front.m_sName, string.Format("[ACR] AI %1: FOB site behind %2 at %3: of %4 clear sites, %5", m_sFaction, front.m_sName, site, sites.Count(), ACR_FobSiting.Breakdown(best)));
  return true;
 }

 // The nearest base another side holds (vector.Zero: none).
 protected vector NearestEnemyBaseTo(ACR_Base from)
 {
  vector best = vector.Zero;
  float bestDistance = float.MAX;
  foreach (ACR_Base b : ACR_Base.s_All)
  {
   if (!b || b.m_bRetired || b.m_sOwner.IsEmpty() || b.m_sOwner == m_sFaction) continue;
   float d = vector.DistanceXZ(b.GetOrigin(), from.GetOrigin());
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = b.GetOrigin();
  }
  return best;
 }

 ACR_Squad FindConstructionSquad()
 {
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   if (!squad || squad.HasPlayer() || squad.CountAlive() <= 0 || squad == m_FobEscort) continue;
   if (squad.m_iAITask == ACR_EAITask.ATTACK || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
   IEntity truck = squad.ConstructionTruck();
   if (!truck || ACR_Logistics.Health(truck) <= 0) continue;
   if (m_Manager.m_Logistics && m_Manager.m_Logistics.StockOf(truck) >= ACR_FobBuild.LOADS_RESERVED)
    return squad;
  }
  return null;
 }

 // Any of our construction trucks still alive with a full 500-supply load.
 bool HasLoadedConstructionTruck()
 {
  array<IEntity> vehicles = {};
  ACR_Vehicles.All(vehicles);
  foreach (IEntity vehicle : vehicles)
  {
   if (!vehicle || ACR_Logistics.Health(vehicle) <= 0 || (ACR_Logistics.ServiceOf(vehicle) & ACR_EService.CONSTRUCTION) == 0) continue;
   FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
   if (!aff || !aff.GetAffiliatedFaction() || aff.GetAffiliatedFaction().GetFactionKey() != m_sFaction) continue;
   if (m_Manager.m_Logistics && m_Manager.m_Logistics.StockOf(vehicle) >= ACR_FobBuild.LOADS_RESERVED) return true;
  }
  return false;
 }

 bool BuyConstructionTruck(ACR_Base spawn)
 {
  if (!spawn) return false;
  // One purchase at a time: a truck already out there (crew still forming up, or parked) waits.
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_fFobTruckBoughtAt < FOB_TRUCK_RETRY_MS || HasLoadedConstructionTruck()) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data || !data.m_aRequisitions) return false;

  int pick = -1;
  foreach (int e, ACR_RequisitionEntry offer : data.m_aRequisitions)
  {
   if (offer.m_iRemaining <= 0 || offer.m_iCost > data.m_iRP || offer.m_sVehicle.IsEmpty()) continue;
   if ((ACR_Logistics.ServiceOfName(offer.m_sVehicle) & ACR_EService.CONSTRUCTION) == 0) continue;
   if (pick < 0 || offer.m_iCost < data.m_aRequisitions[pick].m_iCost) pick = e;
  }
  if (pick < 0) return false;

  int cost = data.m_aRequisitions[pick].m_iCost;
  if (data.m_iRP < cost + FOB_RP_COMBAT_RESERVE) return false;

  string bought = m_Manager.PurchaseAt(m_sFaction, pick, spawn);
  Say(string.Format("[ACR] AI %1 requisition (FOB construction truck): %2", m_sFaction, bought));
  if (!bought.IsEmpty()) m_fFobTruckBoughtAt = now;
  return !bought.IsEmpty();
 }

 ACR_Squad FindEscortSquad(ACR_Squad builder)
 {
  array<ACR_Squad> free = {};
  MySquads(free, true);
  if (builder) SortByDistance(free, PositionOf(builder));
  foreach (ACR_Squad squad : free)
  {
   if (!squad || squad == builder || squad.HasPlayer() || squad.CountAlive() <= 0 || IsSupportTeam(squad)) continue;
   if (squad.IsCombatUnit()) return squad;
  }
  return null;
 }

 void ManageFobBuild()
 {
  if (HasActiveFobProject()) return;

  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextFobEval) return;
  m_fNextFobEval = now + FOB_EVAL_INTERVAL_MS;

  // Rebuild a destroyed HQ before planning anything new.
  vector site;
  ACR_Base front = HqLostFob();
  if (front) site = front.GetOrigin();
  else
  {
   front = NeedsFob();
   if (!front) return;
   bool pending;
   if (!FindFobSiteStep(front, site, ACR_FobSiting.VALIDATE_PER_STEP, pending))
   {
    // Site searches are expensive (an HQ and 15 spots per candidate): a few candidates a step,
    // the next step soon; with none clear, back off.
    if (pending)
    {
     m_fNextFobEval = now + FOB_SITE_STEP_MS;
     return;
    }
    m_fNextFobEval = now + FOB_SITE_RETRY_MS;
    Say(string.Format("[ACR] AI %1: no clear FOB site near %2 (%3), next look in 5 minutes", m_sFaction, front.m_sName, m_sFobSiteNone));
    return;
   }
  }

  ACR_Squad builder = FindConstructionSquad();
  if (!builder)
  {
   ACR_Base spawn = NearestDeployBase(front);
   if (!spawn) spawn = HomeBase();
   BuyConstructionTruck(spawn);
   return;
  }

  builder.m_iAITask = ACR_EAITask.BUILD_FOB;
  if (!builder.EstablishFob(site))
  {
   builder.m_iAITask = ACR_EAITask.FREE;
   return;
  }

  m_FobBuilder = builder;
  m_vFobTargetSite = site;

  ACR_Squad escort = FindEscortSquad(builder);
  if (escort)
  {
   m_FobEscort = escort;
   escort.m_iAITask = ACR_EAITask.DEFEND;
   escort.SetMode(ACR_EMoveMode.FAST);
   escort.RouteTo(site, ACR_Squad.WP_DEFEND, -1, "Escorting FOB construction", 0);
   Say(string.Format("[ACR] AI %1: %2 assigned as escort for FOB build at %3", m_sFaction, escort.GetName(), site));
  }

  Say(string.Format("[ACR] AI %1: dispatched %2 to establish forward FOB near %3 at %4", m_sFaction, builder.GetName(), front.m_sName, site));
 }

 // Our own FOB whose HQ is gone, quiet enough to rebuild.
 ACR_Base HqLostFob()
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || !base.m_bBuilt || base.m_bRetired || !base.m_bHqLost || base.m_sOwner != m_sFaction) continue;
   if (base.m_bContested || ACR_FobBuild.IsContested(base.GetOrigin(), m_sFaction) || Avoided(base.GetOrigin())) continue;
   return base;
  }
  return null;
 }

 void AvoidSite(vector site)
 {
  m_aAvoidSites.Insert(site);
  m_aAvoidUntil.Insert(GetGame().GetWorld().GetWorldTime() + FOB_SITE_AVOID_MS);
  Say(string.Format("[ACR] AI %1: FOB site %2 abandoned, avoiding it for 10 minutes", m_sFaction, site));
 }

 bool Avoided(vector site)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = m_aAvoidSites.Count() - 1; i >= 0; i--)
  {
   if (now >= m_aAvoidUntil[i])
   {
    m_aAvoidSites.RemoveOrdered(i);
    m_aAvoidUntil.RemoveOrdered(i);
    continue;
   }
   if (vector.DistanceXZ(m_aAvoidSites[i], site) < FOB_SITE_AVOID_RADIUS) return true;
  }
  return false;
 }

 // A demolition charge on one of our FOBs: the nearest squad runs to defuse it.
 bool RespondToCharge(vector chargePos)
 {
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  ACR_Squad best;
  float bestDist = 500;
  foreach (ACR_Squad squad : mine)
  {
   if (!squad || squad.CountAlive() <= 0 || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
   float d = vector.DistanceXZ(PositionOf(squad), chargePos);
   if (d < bestDist)
   {
    bestDist = d;
    best = squad;
   }
  }
  if (!best) return false;
  ACR_Base nearestBase;
  float nearestBaseDist = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction) continue;
   float dist = vector.DistanceXZ(base.GetOrigin(), chargePos);
   if (dist < nearestBaseDist) { nearestBaseDist = dist; nearestBase = base; }
  }
  best.m_iAITask = ACR_EAITask.DEFEND;
  best.m_AITarget = nearestBase;
  best.SetMode(ACR_EMoveMode.FAST);
  best.MoveTo(chargePos);
  Say(string.Format("[ACR] AI %1: %2 sent to defuse a demolition charge at %3 (%4 m)", m_sFaction, best.GetName(), chargePos, (int)bestDist));
  return true;
 }

 static const float FOB_UPGRADE_EVAL_INTERVAL_MS = 15000;
 protected float m_fNextFobUpgradeEval;

 void SetFobUpgradeEvalTimeForTest(float time) { m_fNextFobUpgradeEval = time; }

 // handedOnly: only the FOBs a player handed to the AI (ACR_FobAI), with no squads sent to help.
 void ManageFobUpgrades(bool handedOnly = false)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextFobUpgradeEval) return;
  m_fNextFobUpgradeEval = now + FOB_UPGRADE_EVAL_INTERVAL_MS;

  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data) return;

  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || !base.m_bBuilt || base.m_bHqLost || base.m_sOwner != m_sFaction || !base.m_FobSupply) continue;
   if (base.m_bContested || ACR_FobBuild.IsContested(base.GetOrigin(), m_sFaction)) continue;
   if (handedOnly && !ACR_FobAI.Handed(base)) continue;

   // 1. Supply replenishment check
   OrderFobSupplies(base, data);

   // 2. Upgrade check & engineer acceleration
   if (!base.m_aFobSlots) ACR_FobSlots.InitSlots(base);
   if (ACR_FobSlots.AnyBuilding(base))
   {
    if (!handedOnly) AssistFobConstruction(base);
   }
   else
   {
    int stock = ACR_FobSupply.Stock(base.m_FobSupply);
    OrderFobUpgrade(base, stock);
   }
  }
 }

 // A player commands the side (or it has no AI commander): the FOBs handed to the AI (FOB
 // panel, ACR_FobAI) still get its supply orders and upgrades (ACR_Manager.TickAICommanders).
 void ManageHandedFobs()
 {
  ManageFobUpgrades(true);
 }

 bool OrderFobSupplies(ACR_Base base, ACR_FactionData data)
 {
  if (!base || !base.m_FobSupply || !data) return false;
  if (base.m_iSupplyDelivery > 0) return false;
  if (base.m_bContested || ACR_FobBuild.IsContested(base.GetOrigin(), m_sFaction)) return false;

  int stock = ACR_FobSupply.Stock(base.m_FobSupply);
  int capacity = ACR_FobSupply.CapacityOf(base);
  if (capacity - stock < ACR_FobSupply.DELIVERY_AMOUNT) return false;

  int minNeeded = 500;
  int nextTier = 0;
  int nextSlot = BestUpgradeSlotFor(base, nextTier);
  if (nextSlot >= 0) minNeeded = Math.Max(500, ACR_FobSlots.SlotCost(nextSlot, nextTier));
  if (stock >= minNeeded) return false;

  if (data.m_iRP < ACR_FobSupply.DELIVERY_COST + FOB_RP_COMBAT_RESERVE) return false;

  string result = ACR_FobSupply.Order(m_Manager, m_sFaction, base);
  Say(string.Format("[ACR] AI %1 ordered supplies for %2: %3", m_sFaction, base.m_sName, result));
  return base.m_iSupplyDelivery > 0;
 }

 // The AI's FOB build order: the base buildings, Defences I, the tier-1 buildings,
 // Defences II, the helipad, then Defences III. Blocked spots are skipped.
 int BestUpgradeSlotFor(ACR_Base base, out int targetTier)
 {
  targetTier = 0;
  if (!base) return -1;
  if (!base.m_aFobSlots) ACR_FobSlots.InitSlots(base);

  array<int> order = {
   ACR_FobSlots.SLOT_RELAY, 0,
   ACR_FobSlots.SLOT_BARRACKS, 0,
   ACR_FobSlots.SLOT_REPAIR, 0,
   ACR_FobSlots.SLOT_FUEL, 0,
   ACR_FobSlots.SLOT_HOSPITAL, 0,
   ACR_FobSlots.SLOT_DEPOT, 0,
   ACR_FobSlots.SLOT_AMMO, 0,
   ACR_FobSlots.SLOT_DEFENCES, 0,
   ACR_FobSlots.SLOT_BARRACKS, 1,
   ACR_FobSlots.SLOT_REPAIR, 1,
   ACR_FobSlots.SLOT_DEFENCES, 1,
   ACR_FobSlots.SLOT_HELIPAD, 0,
   ACR_FobSlots.SLOT_DEFENCES, 2
  };

  for (int i = 0; i + 1 < order.Count(); i += 2)
  {
   int slotIdx = order[i];
   int tier = order[i + 1];
   ACR_FobSlot slot = ACR_FobSlots.SlotAt(base, slotIdx);
   if (!slot || slot.m_bBlocked) continue; // its spot is blocked: skip it
   bool mine = ACR_FobSlots.IsBuiltForOwner(slot, m_sFaction);
   bool empty = slot.m_eState == ACR_EFobSlotState.EMPTY || (slot.m_eState == ACR_EFobSlotState.BUILT && !mine);
   if (tier == 0 && empty)
   {
    targetTier = 0;
    return slotIdx;
   }
   if (tier > 0 && mine && slot.m_iTier == tier - 1)
   {
    targetTier = tier;
    return slotIdx;
   }
  }
  return -1;
 }

 bool OrderFobUpgrade(ACR_Base base, int stock)
 {
  if (!base) return false;
  if (!base.m_aFobSlots) ACR_FobSlots.InitSlots(base);

  int targetTier = 0;
  int pickSlot = BestUpgradeSlotFor(base, targetTier);
  if (pickSlot < 0) return false;

  int cost = ACR_FobSlots.SlotCost(pickSlot, targetTier);
  if (stock < cost) return false;

  string refusal;
  string outcome = ACR_FobSlots.OrderUpgradeBase(m_Manager, m_sFaction, base, pickSlot, refusal);
  if (!outcome.IsEmpty())
  {
   Say(string.Format("[ACR] AI %1: ordered FOB %2 upgrade %3 (tier %4)", m_sFaction, base.m_sName, ACR_FobSlots.SlotName(pickSlot, targetTier), targetTier));
   return true;
  }
  return false;
 }

 ACR_Squad AssistFobConstruction(ACR_Base base)
 {
  if (!base || ACR_FobSlots.HasEngineer(base)) return null;

  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   if (!squad || squad.HasPlayer() || squad.CountAlive() <= 0) continue;
   if (squad.m_iAITask == ACR_EAITask.ATTACK || squad.m_iAITask == ACR_EAITask.DEFEND || squad.m_iAITask == ACR_EAITask.BUILD_FOB) continue;
   IEntity truck = squad.ConstructionTruck();
   bool hasTruck = truck && ACR_Logistics.Health(truck) > 0;
   if (!hasTruck && !ACR_SapperDemo.IsSapperSquad(squad)) continue;

   float dist = vector.DistanceXZ(PositionOf(squad), base.GetOrigin());
   if (dist <= 500.0)
   {
    squad.SetMode(ACR_EMoveMode.FAST);
    squad.RouteTo(base.GetOrigin(), ACR_Squad.WP_MOVE, -1, "Assisting FOB upgrade construction", 0, true);
    Say(string.Format("[ACR] AI %1: dispatched %2 to assist upgrade construction at %3", m_sFaction, squad.GetName(), base.m_sName));
    return squad;
   }
  }
  return null;
 }

 void SetFobEvalTimeForTest(float time) { m_fNextFobEval = time; }
 void StartTickForTest() { m_iTickStarted = System.GetTickCount(); }
 void AssignAttacksForTest() { m_iTickStarted = System.GetTickCount(); AssignAttacks(); }
 ACR_Squad FobBuilder() { return m_FobBuilder; }
 ACR_Squad FobEscort() { return m_FobEscort; }
 vector FobTargetSite() { return m_vFobTargetSite; }
 void SetFobBuilderForTest(ACR_Squad builder) { m_FobBuilder = builder; }
 ACR_Base GetHomeBase() { return HomeBase(); }

 // Construction temporarily owns an AI squad. A finished checkpoint keeps its builder holding
 // it while KeepsHolding (the defences manned, playtest 25 Oct: the AI's roadblock and position
 // stood empty); otherwise it is an idle post again that attacks and defence may recall. Also
 // repairs completed ROADBLOCK tasks left by the earlier implementation.
 protected void ReleaseRoadblockBuilders()
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad || squad.m_iAITask != ACR_EAITask.ROADBLOCK) continue;
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (squad.m_Commander != 0 || squad.HasPlayer()) continue;
   if (squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || PendingBuildOf(squad)) continue;
   if (squad.m_ActiveRoadblock && KeepsHolding(squad)) continue;
   squad.m_iAITask = ACR_EAITask.POSTED;
   Say(string.Format("[ACR] AI %1: %2 available for reassignment after roadblock construction", m_sFaction, squad.GetName()));
  }
 }
 void ReleaseBuildersForTest() { ReleaseRoadblockBuilders(); }

 // Holding a roadblock or a position it built (not building one).
 static bool IsHolder(ACR_Squad squad)
 {
  if (!squad) return false;
  if (squad.m_iAITask == ACR_EAITask.ROADBLOCK) return squad.m_ActiveRoadblock && squad.OrderState() != ACR_EOrderState.BUILD_ROADBLOCK;
  if (squad.m_iAITask == ACR_EAITask.FORTIFY) return squad.m_Fortification && !squad.m_FortifyPlan;
  return false;
 }

 // A builder stays on what it built while it holds it (its order), is fit to fight, and fewer
 // than the doctrine's m_iHolders other squads of ours fit to hold have held theirs longer:
 // those aren't sent on attacks, and defend a base when outnumbered there or as a last resort
 // (AssignDefenders; a Fortress's only when outnumbered). Counting all the other holders instead released every one of them when a
 // doctrine switch lowered the cap below their number (Fortress 4 -> Firepower 2: each saw 3).
 protected bool KeepsHolding(ACR_Squad squad)
 {
  if (!FitToHold(squad)) return false;
  int ahead;
  bool passed;
  foreach (ACR_Squad other : m_Manager.m_Squads)
  {
   if (other == squad)
   {
    passed = true;
    continue;
   }
   if (!other || !other.GetFaction() || other.GetFaction().GetFactionKey() != m_sFaction || !IsHolder(other) || !FitToHold(other)) continue;
   if (other.m_fOrderedAt < squad.m_fOrderedAt || (other.m_fOrderedAt == squad.m_fOrderedAt && !passed)) ahead++;
  }
  return ahead < HoldersCap();
 }

 protected bool FitToHold(ACR_Squad squad)
 {
  return squad.OrderState() == ACR_EOrderState.DEFEND && squad.StrengthRatio() >= RetreatRatio();
 }

 // Engineering may spend 'cost' now: out of the build budget while the share is on (it is set
 // aside already), else with 'reserve' RP to spare as well.
 bool CanBuild(int cost, int reserve)
 {
  if (SideShare() > 0) return BuildBudget() >= cost;
  // The 2IC (squads handed to the AI) builds only out of the budget, never from the side's RP,
  // which is the commanding player's (#14): with the share off it builds nothing.
  if (m_bHandedOnly) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  return data && data.m_iRP >= cost + reserve;
 }

 // Our uncontested bases nearest to an enemy base, nearest first (at most 'most'), each with
 // that enemy base's position: where roadblocks, minefields and fortified positions go. (The one
 // base nearest the enemy only, before: a side held one roadblock all game, playtest 25 Oct.)
 static const int FRONT_BASES = 3;
 protected void FrontBases(notnull array<ACR_Base> bases, notnull array<vector> enemies, int most)
 {
  bases.Clear();
  enemies.Clear();
  array<float> gaps = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != m_sFaction || base.m_bContested) continue;
   float nearest = float.MAX;
   vector enemyAt;
   foreach (ACR_Base other : ACR_Base.s_All)
   {
    if (!other || other.m_bRetired || other.m_sOwner == m_sFaction || other.m_sOwner.IsEmpty()) continue;
    float d = vector.DistanceXZ(base.GetOrigin(), other.GetOrigin());
    if (d >= nearest) continue;
    nearest = d;
    enemyAt = other.GetOrigin();
   }
   if (nearest == float.MAX) continue;
   int at = 0;
   while (at < gaps.Count() && gaps[at] <= nearest) at++;
   gaps.InsertAt(nearest, at);
   bases.InsertAt(base, at);
   enemies.InsertAt(enemyAt, at);
  }
  while (bases.Count() > most)
  {
   bases.Remove(bases.Count() - 1);
   enemies.Remove(enemies.Count() - 1);
  }
 }

 void FrontBasesOf(notnull array<ACR_Base> bases, notnull array<vector> enemies, int most) { FrontBases(bases, enemies, most); }
 void FrontBasesForTest(notnull array<ACR_Base> bases, notnull array<vector> enemies) { FrontBases(bases, enemies, FRONT_BASES); }

 // Manage roadblocks / observation posts (aca-f23). Every 15 s, one build at a time, while it
 // holds fewer than its doctrine's m_iRoadblocks and engineering can pay (CanBuild): at the first
 // of our front bases (FrontBases) with no roadblock of ours within 400 m, else at the first with
 // a second road towards the enemy that has none, a free squad within 450 m of it (else the
 // nearest within FAR_BUILDER) establishes one at the best road site 200-450 m out towards the
 // nearest enemy base (ACR_RoadblockSiting: sight down the road, no junction or dead end near,
 // chokepoints; playtest 25 Oct: the first road point found put two at a road's end and a
 // T-crossing).
 void ManageRoadblocks()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextRoadblockEval) return;
  m_fNextRoadblockEval = now + 15000;
  if (!CanBuild(ACR_Roadblocks.ROADBLOCK_COST, 25))
  {
   NoteBuild("roadblock pay", string.Format("[ACR] AI %1: no roadblock for now: engineering can't pay %2 RP", m_sFaction, ACR_Roadblocks.ROADBLOCK_COST));
   return;
  }

  int activeCount = 0;
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (rb && rb.m_sFaction == m_sFaction) activeCount++;
  }
  if (activeCount >= m_Doctrine.m_iRoadblocks) return;
  foreach (ACR_Squad builder : m_Manager.m_Squads)
  {
   if (builder && builder.m_iAITask == ACR_EAITask.ROADBLOCK && builder.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK && builder.GetFaction() && builder.GetFaction().GetFactionKey() == m_sFaction) return;
  }
  if (PendingBuilds(ACR_EAITask.ROADBLOCK) > 0) return;

  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return;
  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  FrontBases(fronts, enemies, FRONT_BASES);
  array<ACR_Squad> squads = {};
  MySquads(squads, true);
  // First the bases with no roadblock of ours near; then a second road into one that has.
  for (int pass = 0; pass < 2; pass++)
  {
   foreach (int f, ACR_Base frontlineBase : fronts)
   {
    bool covered = false;
    foreach (ACR_Roadblock rb2 : ACR_Roadblocks.s_aRoadblocks)
    {
     if (rb2 && rb2.m_sFaction == m_sFaction && vector.DistanceXZ(rb2.m_vPos, frontlineBase.GetOrigin()) < 400.0) covered = true;
    }
    if (covered != (pass == 1)) continue;
    // Scoring a base's road sites traces sight lines: it waits for a tick with budget left.
    if (PlanBudgetSpent())
    {
     NoteBuild("roadblock budget", string.Format("[ACR] AI %1: roadblock siting at %2 waits for a tick with planning budget left", m_sFaction, frontlineBase.m_sName));
     return;
    }
    vector enemyBasePos = enemies[f];
    vector basePos = frontlineBase.GetOrigin();
    int considered;
    string none;
    ACR_RoadSite site = ACR_RoadblockSiting.Best(basePos, enemyBasePos, m_sFaction, pass == 1, considered, none);
    if (!site)
    {
     if (pass == 0) NoteBuild("roadblock site " + frontlineBase.m_sName, string.Format("[ACR] AI %1: no roadblock site at %2: %3", m_sFaction, frontlineBase.m_sName, none));
     continue;
    }
    vector targetRoadPos = site.m_vPos;
    float targetYaw = site.m_fYaw;

    // An idle or posted squad within 450 m of this base, else the nearest within FAR_BUILDER.
    ACR_Squad candidate = null;
    ACR_Squad farther = null;
    float fartherDistance = FAR_BUILDER;
    foreach (ACR_Squad squad : squads)
    {
     if (!squad || squad.HasPlayer() || squad.CountAlive() <= 0 || squad.IsVehicle() || IsSupportTeam(squad)) continue;
     if (squad == m_ReserveSquad || squad.m_Commander != 0) continue;
     if (squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
     if (squad.m_iAITask != ACR_EAITask.POSTED && squad.m_iAITask != ACR_EAITask.FREE) continue;
     if (DueForRefit(squad)) continue;
     float d = vector.DistanceXZ(PositionOf(squad), basePos);
     if (d < 450.0)
     {
      candidate = squad;
      break;
     }
     if (d >= fartherDistance) continue;
     fartherDistance = d;
     farther = squad;
    }
    if (!candidate) candidate = farther;
    if (!candidate)
    {
     NoteBuild("roadblock squad " + frontlineBase.m_sName, string.Format("[ACR] AI %1: a roadblock site at %2 waits for a free squad within %3 m %4", m_sFaction, frontlineBase.m_sName, FAR_BUILDER, NoBuilderWhy(basePos, BUILDER_ROADBLOCK)));
     continue;
    }
    if (RideToBuild(candidate, ACR_EAITask.ROADBLOCK, frontlineBase, enemyBasePos, targetRoadPos, targetYaw))
    {
     Say(string.Format("[ACR] AI %1: %2 rides out to a roadblock site on the approach to %3 at %4: of %5 road sites, %6", m_sFaction, candidate.GetName(), frontlineBase.m_sName, targetRoadPos, considered, ACR_RoadblockSiting.Breakdown(site)));
     return;
    }
    if (!candidate.EstablishRoadblock(targetRoadPos, targetYaw, ACR_ERoadblockType.MEDIUM, false))
    {
     NoteBuild("roadblock order " + frontlineBase.m_sName, string.Format("[ACR] AI %1: %2 can't take the roadblock order at %3", m_sFaction, candidate.GetName(), frontlineBase.m_sName));
     continue;
    }
    candidate.m_iAITask = ACR_EAITask.ROADBLOCK;
    string which = "";
    if (pass == 1) which = " (a second road)";
    Say(string.Format("[ACR] AI %1: dispatched %2%3 to establish roadblock on approach to %4 at %5%6: of %7 road sites, %8", m_sFaction, candidate.GetName(), FromFar(candidate, basePos, 450), frontlineBase.m_sName, targetRoadPos, which, considered, ACR_RoadblockSiting.Breakdown(site)));
    return;
   }
  }
 }

 // What held a roadblock, minefield or position up, at most once per NOTE_S per kind (a playtest
 // log shows whether siting ever stopped one, or the RP, the planning budget or a free squad did;
 // 25 Oct, pn5: no AI minefield or USSR position all game, and nothing in the log said why).
 static const float NOTE_S = 300;
 protected ref map<string, float> m_mBuildNotes = new map<string, float>();
 protected void NoteBuild(string key, string line)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  float last;
  if (m_mBuildNotes.Find(key, last) && now - last < NOTE_S * 1000) return;
  m_mBuildNotes.Set(key, now);
  Say(line);
 }

 // With no free squad near a front base, a roadblock, minefield or position takes the nearest
 // within FAR_BUILDER, on foot (the job's walk deadline grows with the distance). pn5, 25 Oct:
 // 11 notes of a roadblock site waiting for a free squad within 450 m, and no AI field or USSR
 // position all game. The reserve and the squads attacking or defending stay out of it as before.
 static const float FAR_BUILDER = 1500;
 protected string FromFar(ACR_Squad candidate, vector pos, float near)
 {
  float d = vector.DistanceXZ(PositionOf(candidate), pos);
  if (d <= near) return "";
  return string.Format(" (none nearer: from %1 m away)", (int)d);
 }

 // A far builder (more than the doctrine's m_fRideDistance from 'site') rides out in a truck when
 // one is free: the job is given, and paid, once the squad is out at the drop (ManagePendingBuilds;
 // a job given aboard would end the ride at once). False: no ride needed or none to be had, the
 // job is given now and the squad walks.
 protected ref array<ref ACR_PendingBuild> m_aPendingBuilds = {};
 protected bool RideToBuild(ACR_Squad squad, int task, ACR_Base base, vector enemy, vector site, float yaw)
 {
  if (!m_Manager.m_Rides || squad.IsMounted()) return false;
  float distance = vector.DistanceXZ(PositionOf(squad), site);
  if (distance <= m_Doctrine.m_fRideDistance) return false;
  string why;
  if (!m_Manager.m_Rides.Request(squad, site, 0, ACR_Squad.WP_MOVE, -1, "Riding out to build", 0, why, true))
  {
   NoteBuild("ride " + squad.GetName(), string.Format("[ACR] AI %1: no ride for %2 to build at %3, %4 m out (%5): it walks", m_sFaction, squad.GetName(), base.m_sName, (int)distance, why));
   return false;
  }
  ACR_PendingBuild pending = new ACR_PendingBuild();
  pending.m_Squad = squad;
  pending.m_iTask = task;
  pending.m_Base = base;
  pending.m_vEnemy = enemy;
  pending.m_vSite = site;
  pending.m_fYaw = yaw;
  pending.m_fDeadline = GetGame().GetWorld().GetWorldTime() + distance / ACR_Minefields.TRAVEL_SPEED * 1000 + ACR_Squad.FOB_GIVE_UP_MS;
  m_aPendingBuilds.Insert(pending);
  squad.m_iAITask = task;
  if (task != ACR_EAITask.ROADBLOCK) Say(string.Format("[ACR] AI %1: %2 rides out to %3 to %4 there (%5 m)", m_sFaction, squad.GetName(), base.m_sName, BuildName(task), (int)distance));
  return true;
 }

 protected static string BuildName(int task)
 {
  if (task == ACR_EAITask.ROADBLOCK) return "build a roadblock";
  if (task == ACR_EAITask.MINES) return "lay a minefield";
  return "dig in";
 }

 protected ACR_PendingBuild PendingBuildOf(ACR_Squad squad)
 {
  foreach (ACR_PendingBuild pending : m_aPendingBuilds)
  {
   if (pending.m_Squad == squad) return pending;
  }
  return null;
 }

 protected int PendingBuilds(int task)
 {
  int count;
  foreach (ACR_PendingBuild pending : m_aPendingBuilds)
  {
   if (pending.m_iTask == task) count++;
  }
  return count;
 }

 // Squads riding out to build: once out of the truck (the ride over, or fallen through and it
 // walked on), the job as if it stood there; given up past the deadline, or when the squad was
 // re-tasked, taken over or its base lost.
 protected void ManagePendingBuilds()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = m_aPendingBuilds.Count() - 1; i >= 0; i--)
  {
   ACR_PendingBuild pending = m_aPendingBuilds[i];
   ACR_Squad squad = pending.m_Squad;
   string what = BuildName(pending.m_iTask);
   bool gone = !squad || !m_Manager.m_Squads.Contains(squad) || squad.CountAlive() <= 0 || squad.HasPlayer() || squad.m_Commander != 0 || squad.m_iAITask != pending.m_iTask;
   if (gone || !pending.m_Base || pending.m_Base.m_sOwner != m_sFaction || now > pending.m_fDeadline)
   {
    m_aPendingBuilds.Remove(i);
    if (gone) continue;
    squad.m_iAITask = ACR_EAITask.POSTED;
    Say(string.Format("[ACR] AI %1: %2 no longer rides out to %3 (base lost or past its deadline)", m_sFaction, squad.GetName(), what));
    continue;
   }
   if (m_Manager.m_Rides.RideOf(squad) || squad.IsMounted()) continue;
   m_aPendingBuilds.Remove(i);
   string from = string.Format(" (out of its ride, %1 m from %2)", (int)vector.DistanceXZ(PositionOf(squad), pending.m_Base.GetOrigin()), pending.m_Base.m_sName);
   bool ordered;
   bool paid;
   if (pending.m_iTask == ACR_EAITask.ROADBLOCK)
   {
    paid = CanBuild(ACR_Roadblocks.ROADBLOCK_COST, 25);
    ordered = paid && squad.EstablishRoadblock(pending.m_vSite, pending.m_fYaw, ACR_ERoadblockType.MEDIUM, false);
    if (ordered) Say(string.Format("[ACR] AI %1: %2%3 establishing the roadblock at %4", m_sFaction, squad.GetName(), from, pending.m_vSite));
   }
   else if (pending.m_iTask == ACR_EAITask.MINES)
   {
    paid = CanBuild(ACR_Minefields.Cost(ACR_EMineKind.MIXED), 30);
    ordered = paid && LayMines(squad, pending.m_Base, pending.m_vEnemy, from);
   }
   else
   {
    paid = CanBuild(ACR_Fortify.Cost(1), 30);
    ordered = paid && DigIn(squad, pending.m_Base, pending.m_vEnemy, from);
   }
   if (ordered) continue;
   squad.m_iAITask = ACR_EAITask.POSTED;
   string why = "engineering can't pay now";
   if (paid) why = "the order was refused there";
   Say(string.Format("[ACR] AI %1: %2 out of its ride, but can't %3 now (%4)", m_sFaction, squad.GetName(), what, why));
  }
 }
 void ManagePendingBuildsForTest() { ManagePendingBuilds(); }
 bool PendingBuildForTest(ACR_Squad squad) { return PendingBuildOf(squad) != null; }

 // Minefields on the approaches (-acrMinesAI 1; off by default). Every 30 s, one field at a
 // time, while it has fewer than its doctrine's m_iMinefields down and engineering can pay: at the
 // first of our front bases with no field of ours within 300 m, a free infantry squad within 600 m
 // of it (sappers first; else the nearest within FAR_BUILDER) lays one at the best scored spot
 // (ACR_MineSiting: on the road ahead of our roadblock or in front of our position where they cover
 // it, on the enemy's road route, in dead ground; the old ring 150-270 m out last), never within
 // DEFENCE_STANDOFF of our own defences: mixed on a road, AP mines on open ground.
 void ManageMinefields()
 {
  foreach (ACR_Squad layer : m_Manager.m_Squads)
  {
   if (!layer || layer.m_iAITask != ACR_EAITask.MINES || layer.m_MinePlan || PendingBuildOf(layer)) continue;
   if (!layer.GetFaction() || layer.GetFaction().GetFactionKey() != m_sFaction) continue;
   layer.m_iAITask = ACR_EAITask.POSTED;
   Say(string.Format("[ACR] AI %1: %2 available for reassignment after laying mines", m_sFaction, layer.GetName()));
  }
  // Planning a field queries the roads and terrain: it waits for a tick with budget left.
  if (!ACR_Minefields.AIUses()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextMinefieldEval) return;
  if (PlanBudgetSpent())
  {
   NoteBuild("mines budget", string.Format("[ACR] AI %1: minefield planning waits for a tick with planning budget left", m_sFaction));
   return;
  }
  m_fNextMinefieldEval = now + 30000;
  if (ACR_Minefields.CountFields(m_sFaction) >= m_Doctrine.m_iMinefields) return;
  int cost = ACR_Minefields.Cost(ACR_EMineKind.MIXED);
  if (!CanBuild(cost, 30))
  {
   NoteBuild("mines pay", string.Format("[ACR] AI %1: no minefield for now: engineering can't pay %2 RP", m_sFaction, cost));
   return;
  }
  // One field at a time: any squad of ours on its way to lay one. (The free squads alone leave
  // out the one already laying, so two were sent to the same spot, playtest 25 Oct.)
  foreach (ACR_Squad laying : m_Manager.m_Squads)
  {
   if (laying && laying.m_MinePlan && laying.GetFaction() && laying.GetFaction().GetFactionKey() == m_sFaction) return;
  }
  if (PendingBuilds(ACR_EAITask.MINES) > 0) return;

  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  FrontBases(fronts, enemies, FRONT_BASES);
  array<ACR_Squad> squads = {};
  MySquads(squads, true);
  foreach (int f, ACR_Base front : fronts)
  {
   if (ACR_Minefields.FieldNear(m_sFaction, front.GetOrigin(), 300)) continue;
   // A squad that can lay mines within 600 m of the base (sappers first), else the nearest
   // within FAR_BUILDER.
   ACR_Squad candidate = null;
   ACR_Squad farther = null;
   float fartherDistance = FAR_BUILDER;
   foreach (ACR_Squad squad : squads)
   {
    if (!squad || squad.HasPlayer() || squad.m_Commander != 0 || squad == m_ReserveSquad || IsSupportTeam(squad) || !ACR_Minefields.CanLay(squad)) continue;
    if (squad.m_iAITask != ACR_EAITask.POSTED && squad.m_iAITask != ACR_EAITask.FREE) continue;
    if (squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
    if (DueForRefit(squad)) continue;
    float d = vector.DistanceXZ(PositionOf(squad), front.GetOrigin());
    if (d > 600)
    {
     if (d >= fartherDistance) continue;
     fartherDistance = d;
     farther = squad;
     continue;
    }
    if (!candidate || (ACR_SapperDemo.IsSapperSquad(squad) && !ACR_SapperDemo.IsSapperSquad(candidate))) candidate = squad;
   }
   if (!candidate) candidate = farther;
   if (!candidate)
   {
    NoteBuild("mines squad " + front.m_sName, string.Format("[ACR] AI %1: a minefield at %2 waits for a free squad that can lay mines within %3 m %4", m_sFaction, front.m_sName, FAR_BUILDER, NoBuilderWhy(front.GetOrigin(), BUILDER_MINES)));
    continue;
   }

   if (RideToBuild(candidate, ACR_EAITask.MINES, front, enemies[f], front.GetOrigin(), 0)) return;
   if (LayMines(candidate, front, enemies[f], FromFar(candidate, front.GetOrigin(), 600))) return;
  }
 }

 // Lays a field round 'front' with 'candidate' at the best scored spot it may lay at
 // (ACR_MineSiting: covered by our roadblock or position, on the enemy's road, dead ground; the old
 // ring last); true if ordered, else notes why not.
 protected bool LayMines(ACR_Squad candidate, ACR_Base front, vector enemyPos, string from)
 {
  array<ref ACR_MineSite> sites = {};
  int crowded;
  ACR_MineSiting.Candidates(this, m_sFaction, front.GetOrigin(), enemyPos, sites, crowded);
  int refused = 0;
  string refusal = "";
  foreach (ACR_MineSite site : sites)
  {
   if (!ACR_Minefields.Order(candidate, site.m_eKind, site.m_vPos, refusal))
   {
    refused++;
    continue;
   }
   candidate.m_iAITask = ACR_EAITask.MINES;
   Say(string.Format("[ACR] AI %1: %2%3 laying %4 on the approach to %5 at %6: of %7 spots, %8", m_sFaction, candidate.GetName(), from, ACR_Minefields.Name(site.m_eKind), front.m_sName, site.m_vPos, sites.Count(), ACR_MineSiting.Breakdown(site)));
   return true;
  }
  string why = string.Format("%1 spots, %2 more too near our own defences or bases", sites.Count(), crowded);
  if (refused > 0) why += string.Format(", %1 refused (last: %2)", refused, refusal);
  NoteBuild("mines site " + front.m_sName, string.Format("[ACR] AI %1: no minefield site at %2: %3", m_sFaction, front.m_sName, why));
  return false;
 }

 // Our defences keep apart (playtest 25 Oct: twice an AI minefield 10 m from one of its own
 // positions, laid or dug in either order; the AI is spared its own mines, its players are not):
 // a field's centre DEFENCE_STANDOFF m from our positions and roadblocks (also those on their way)
 // and clear of our bases' edges by 40 m, a position as far from our fields.
 static const float DEFENCE_STANDOFF = 60;
 bool ClearOfOwnDefences(vector pos)
 {
  if (ACR_Fortify.NearestOwn(m_sFaction, pos, DEFENCE_STANDOFF)) return false;
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (rb && rb.m_sFaction == m_sFaction && vector.DistanceXZ(rb.m_vPos, pos) < DEFENCE_STANDOFF) return false;
  }
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (squad.m_FortifyPlan && vector.DistanceXZ(squad.m_FortifyPlan.m_vSite, pos) < DEFENCE_STANDOFF) return false;
   if (squad.RoadblockPending() && vector.DistanceXZ(squad.m_vRoadblockSite, pos) < DEFENCE_STANDOFF) return false;
  }
  foreach (ACR_PendingBuild pending : m_aPendingBuilds)
  {
   if (pending.m_iTask == ACR_EAITask.ROADBLOCK && vector.DistanceXZ(pending.m_vSite, pos) < DEFENCE_STANDOFF) return false;
  }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base && !base.m_bRetired && base.m_sOwner == m_sFaction && vector.DistanceXZ(base.GetOrigin(), pos) < base.m_fRadius + 40) return false;
  }
  return true;
 }

 void ResetBuildTimersForTest()
 {
  m_fNextRoadblockEval = 0;
  m_fNextMinefieldEval = 0;
  m_fNextFortifyEval = 0;
 }

 // Our uncontested base nearest to an enemy base, and that enemy base's position.
 protected ACR_Base FrontlineBase(out vector enemyPos)
 {
  ACR_Base front;
  float nearest = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != m_sFaction || base.m_bContested) continue;
   foreach (ACR_Base other : ACR_Base.s_All)
   {
    if (!other || other.m_bRetired || other.m_sOwner == m_sFaction || other.m_sOwner.IsEmpty()) continue;
    float d = vector.DistanceXZ(base.GetOrigin(), other.GetOrigin());
    if (d >= nearest) continue;
    nearest = d;
    front = base;
    enemyPos = other.GetOrigin();
   }
  }
  return front;
 }

 // Fortified positions on the approaches (-acrFortifyAI 1; off by default). Every 60 s, one
 // order at a time: first man an empty position at a front base (ManPositions, free); then, while
 // engineering can pay, upgrade our lowest-tier position in use below tier III (nearest the front
 // first), worked by the squad holding it or the nearest free one within FAR_BUILDER; else, with
 // fewer than its doctrine's m_iFortifyPositions in use (PositionsInUse), dig the nearest free
 // infantry squad (within FAR_BUILDER) in at the best scored spot out from the first of our front
 // bases with no position of ours within 300 m, facing the enemy (DigIn, ACR_PositionSiting), never
 // within DEFENCE_STANDOFF of our own fields. A position whose upgrade was refused is left alone
 // for a while (ACR_Fortify.HoldUpgrades). The squad then holds it while KeepsHolding, else it is
 // posted again.
 void ManageFortify()
 {
  foreach (ACR_Squad digger : m_Manager.m_Squads)
  {
   if (!digger || digger.m_iAITask != ACR_EAITask.FORTIFY || digger.m_FortifyPlan || PendingBuildOf(digger)) continue;
   if (!digger.GetFaction() || digger.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (digger.m_Fortification && KeepsHolding(digger)) continue;
   digger.m_iAITask = ACR_EAITask.POSTED;
  }
  // Planning a position queries the terrain round it: it waits for a tick with budget left.
  if (!ACR_Fortify.AIUses()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextFortifyEval) return;
  if (PlanBudgetSpent())
  {
   NoteBuild("fortify budget", string.Format("[ACR] AI %1: fortify planning waits for a tick with planning budget left", m_sFaction));
   return;
  }
  m_fNextFortifyEval = now + 60000;
  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  FrontBases(fronts, enemies, FRONT_BASES);
  if (fronts.IsEmpty()) return;
  // An empty position first: manning it costs nothing.
  if (ManPositions(fronts)) return;
  foreach (ACR_Squad busy : m_Manager.m_Squads)
  {
   if (busy && busy.m_FortifyPlan && busy.GetFaction() && busy.GetFaction().GetFactionKey() == m_sFaction) return;
  }
  if (PendingBuilds(ACR_EAITask.FORTIFY) > 0) return;
  string refusal;

  ACR_Fortification target;
  foreach (ACR_Fortification fort : ACR_Fortify.s_Positions)
  {
   if (fort.m_sFaction != m_sFaction || fort.m_iTier >= ACR_Fortify.MAX_TIER || ACR_Fortify.BeingBuilt(fort) || ACR_Fortify.UpgradesHeld(fort)) continue;
   if (!InUse(fort, fronts)) continue; // left behind by the front, empty: not worth RP
   if (target && fort.m_iTier > target.m_iTier) continue;
   if (target && fort.m_iTier == target.m_iTier && vector.DistanceXZ(fort.m_vPos, fronts[0].GetOrigin()) >= vector.DistanceXZ(target.m_vPos, fronts[0].GetOrigin())) continue;
   target = fort;
  }
  if (target)
  {
   int upgradeCost = ACR_Fortify.Cost(target.m_iTier + 1);
   ACR_Squad worker = HolderOf(target);
   if (!worker) worker = FortifyWorkerNear(target.m_vPos);
   if (!CanBuild(upgradeCost, 30))
   {
    NoteBuild("fortify upgrade pay", string.Format("[ACR] AI %1: no upgrade of the position at %2 for now: engineering can't pay %3 RP", m_sFaction, target.m_vPos, upgradeCost));
   }
   else if (!worker)
   {
    NoteBuild("fortify upgrade squad", string.Format("[ACR] AI %1: upgrading the position at %2 waits for a free squad within %3 m %4", m_sFaction, target.m_vPos, FAR_BUILDER, NoBuilderWhy(target.m_vPos, BUILDER_FORTIFY)));
   }
   else if (ACR_Fortify.Order(worker, target.m_vPos, refusal))
   {
    worker.m_iAITask = ACR_EAITask.FORTIFY;
    Say(string.Format("[ACR] AI %1: %2%3 upgrading the position at %4 to %5", m_sFaction, worker.GetName(), FromFar(worker, target.m_vPos, FORTIFY_NEAR), target.m_vPos, ACR_Fortify.TierName(target.m_iTier + 1)));
    return;
   }
   else
   {
    NoteBuild("fortify upgrade order", string.Format("[ACR] AI %1: no upgrade of the position at %2: %3", m_sFaction, target.m_vPos, refusal));
   }
  }

  if (PositionsInUse(fronts) >= m_Doctrine.m_iFortifyPositions) return;
  if (!CanBuild(ACR_Fortify.Cost(1), 30))
  {
   NoteBuild("fortify pay", string.Format("[ACR] AI %1: no new position for now: engineering can't pay %2 RP", m_sFaction, ACR_Fortify.Cost(1)));
   return;
  }
  foreach (int f, ACR_Base front : fronts)
  {
   if (ACR_Fortify.NearestOwn(m_sFaction, front.GetOrigin(), 300)) continue;
   ACR_Squad candidate = FortifyWorkerNear(front.GetOrigin());
   if (!candidate)
   {
    NoteBuild("fortify squad " + front.m_sName, string.Format("[ACR] AI %1: a position at %2 waits for a free squad within %3 m %4", m_sFaction, front.m_sName, FAR_BUILDER, NoBuilderWhy(front.GetOrigin(), BUILDER_FORTIFY)));
    continue;
   }
   if (RideToBuild(candidate, ACR_EAITask.FORTIFY, front, enemies[f], front.GetOrigin(), 0)) return;
   if (DigIn(candidate, front, enemies[f], FromFar(candidate, front.GetOrigin(), FORTIFY_NEAR))) return;
  }
 }

 // A position of ours at a front base that stands empty (its holder killed, gone to defend or
 // posted) gets the nearest free squad within FAR_BUILDER sent back to hold it, while fewer than
 // the doctrine's m_iHolders squads hold what we built (roadblocks included; more would be posted
 // again at once). Before, nothing sent anyone back (Jacob, #25). True if a squad was sent.
 static const float FRONT_POSITION_REACH = 600; // m from a front base: a position there is part of its defence
 protected bool ManPositions(array<ACR_Base> fronts)
 {
  int holding;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.GetFaction() && squad.GetFaction().GetFactionKey() == m_sFaction && IsHolder(squad) && FitToHold(squad)) holding++;
  }
  if (holding >= HoldersCap()) return false;
  foreach (ACR_Fortification fort : ACR_Fortify.s_Positions)
  {
   if (fort.m_sFaction != m_sFaction || ManningSquad(fort) != null || ACR_Fortify.BeingBuilt(fort) || !AtFront(fort, fronts)) continue;
   ACR_Squad worker = FortifyWorkerNear(fort.m_vPos);
   if (!worker)
   {
    NoteBuild("man " + fort.m_iId.ToString(), string.Format("[ACR] AI %1: the empty position at %2 waits for a free squad within %3 m %4", m_sFaction, fort.m_vPos, FAR_BUILDER, NoBuilderWhy(fort.m_vPos, BUILDER_FORTIFY)));
    continue;
   }
   if (!worker.ManFortification(fort)) continue;
   worker.m_iAITask = ACR_EAITask.FORTIFY;
   Say(string.Format("[ACR] AI %1: %2%3 mans the empty %4 at %5", m_sFaction, worker.GetName(), FromFar(worker, fort.m_vPos, FORTIFY_NEAR), ACR_Fortify.TierName(fort.m_iTier), fort.m_vPos));
   return true;
  }
  return false;
 }

 protected bool AtFront(ACR_Fortification fort, array<ACR_Base> fronts)
 {
  foreach (ACR_Base front : fronts)
  {
   if (vector.DistanceXZ(fort.m_vPos, front.GetOrigin()) <= FRONT_POSITION_REACH) return true;
  }
  return false;
 }

 // Manned, or at a front base (to be manned): an empty one the front has left behind is not.
 protected bool InUse(ACR_Fortification fort, array<ACR_Base> fronts)
 {
  return ManningSquad(fort) != null || AtFront(fort, fronts);
 }

 // Our positions that count towards the doctrine's m_iFortifyPositions: those in use. Empty ones
 // the front left behind no longer stop new ones at the front.
 protected int PositionsInUse(array<ACR_Base> fronts)
 {
  int count;
  foreach (ACR_Fortification fort : ACR_Fortify.s_Positions)
  {
   if (fort.m_sFaction == m_sFaction && InUse(fort, fronts)) count++;
  }
  return count;
 }
 int PositionsInUseForTest()
 {
  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  FrontBases(fronts, enemies, FRONT_BASES);
  return PositionsInUse(fronts);
 }

 // Digs 'candidate' in at the best scored spot out from 'front' towards 'enemyPos' that takes it
 // (ACR_PositionSiting: field of fire, slight high ground, no dead ground close in front, cover
 // behind, its upgrades fit, overlooking our roadblock or field; the old line spots too), clear of
 // our fields; true if ordered, else notes why not.
 protected bool DigIn(ACR_Squad candidate, ACR_Base front, vector enemyPos, string from)
 {
  array<ref ACR_PositionSite> sites = {};
  int nearFields;
  ACR_PositionSiting.Candidates(candidate, m_sFaction, front.GetOrigin(), enemyPos, sites, nearFields);
  int refused = 0;
  string refusal = "";
  foreach (ACR_PositionSite site : sites)
  {
   if (!ACR_Fortify.Order(candidate, site.m_vPos, refusal, enemyPos))
   {
    refused++;
    continue;
   }
   candidate.m_iAITask = ACR_EAITask.FORTIFY;
   // Better spots the order refused, so a low score can be told from poor ground.
   string skipped = "";
   if (refused > 0) skipped = string.Format("; %1 better spots refused (last: %2)", refused, refusal);
   Say(string.Format("[ACR] AI %1: %2%3 digging in on the approach to %4 at %5: of %6 spots, %7%8", m_sFaction, candidate.GetName(), from, front.m_sName, site.m_vPos, sites.Count(), ACR_PositionSiting.Breakdown(site), skipped));
   return true;
  }
  string why = string.Format("%1 spots, %2 more near our own fields", sites.Count(), nearFields);
  if (refused > 0) why += string.Format(", %1 refused (last: %2)", refused, refusal);
  NoteBuild("fortify site " + front.m_sName, string.Format("[ACR] AI %1: no position site at %2: %3", m_sFaction, front.m_sName, why));
  return false;
 }

 // The squad of ours holding this position, if any.
 protected ACR_Squad HolderOf(ACR_Fortification fort)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.m_Fortification == fort && IsHolder(squad) && !squad.HasPlayer() && squad.m_Commander == 0) return squad;
  }
  return null;
 }

 // The nearest free infantry squad within FAR_BUILDER of 'pos' that can dig in (not off a
 // roadblock or position it holds); within FORTIFY_NEAR only, before.
 static const float FORTIFY_NEAR = 500;
 // ---------------------------------------------------------------- holders back to their posts
 // A holder that leaves its position to defend a base (outnumbered there, or as a last resort)
 // goes back to it once that defence is over, before new attacks take it: the pn9 playtest
 // (26 Oct) had two Military Hill holders go to defend it, then onto an attack, and the positions
 // stood empty for 15 minutes. Only while the position is still ours, standing and unmanned, the
 // doctrine's holders allow it, and within RETURN_MS of leaving. A roadblock's holder the same
 // (#64 review): it has m_ActiveRoadblock, not m_Fortification.
 static const float RETURN_MS = 1200000;
 protected ref map<ACR_Squad, ACR_Fortification> m_LeftPost = new map<ACR_Squad, ACR_Fortification>();
 protected ref map<ACR_Squad, ACR_Roadblock> m_LeftRoadblock = new map<ACR_Squad, ACR_Roadblock>();
 protected ref map<ACR_Squad, float> m_LeftPostAt = new map<ACR_Squad, float>();

 // RETURN_MS varied by up to 15% per squad (ACR_Variety, seeded from the match seed): holders
 // don't all stop looking for their posts on the same tick.
 protected float ReturnLimit(ACR_Squad squad)
 {
  return ACR_Variety.Jitter(RETURN_MS, 0.15, string.Format("return %1 %2", m_sFaction, squad.GetName()));
 }

 protected void NoteLeftPost(ACR_Squad squad)
 {
  if (!squad) return;
  if (squad.m_Fortification) m_LeftPost.Set(squad, squad.m_Fortification);
  else if (squad.m_ActiveRoadblock) m_LeftRoadblock.Set(squad, squad.m_ActiveRoadblock);
  else return;
  m_LeftPostAt.Set(squad, GetGame().GetWorld().GetWorldTime());
 }

 protected void ReturnToPosts()
 {
  if (m_LeftPost.IsEmpty() && m_LeftRoadblock.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_Squad> done = {};
  foreach (ACR_Squad squad, ACR_Fortification fort : m_LeftPost)
  {
   float at = 0;
   m_LeftPostAt.Find(squad, at);
   if (!squad || !fort || !m_Manager.m_Squads.Contains(squad) || squad.CountAlive() <= 0 || !ACR_Fortify.s_Positions.Contains(fort) || fort.m_sFaction != m_sFaction || now - at > ReturnLimit(squad))
   {
    done.Insert(squad);
    continue;
   }
   // Still defending, or taken by something else since: it waits.
   if (squad.m_iAITask != ACR_EAITask.FREE && squad.m_iAITask != ACR_EAITask.POSTED) continue;
   done.Insert(squad);
   if (squad.HasPlayer() || squad.m_Commander != 0 || ManningSquad(fort) != null) continue;
   if (Holding() >= m_Doctrine.m_iHolders) continue;
   if (!squad.ManFortification(fort)) continue;
   squad.m_iAITask = ACR_EAITask.FORTIFY;
   PrintFormat("[ACR] AI %1: %2 goes back to hold the %3 at %4 it left to defend", m_sFaction, squad.GetName(), ACR_Fortify.TierName(fort.m_iTier), fort.m_vPos);
  }
  foreach (ACR_Squad left, ACR_Roadblock rb : m_LeftRoadblock)
  {
   float leftAt = 0;
   m_LeftPostAt.Find(left, leftAt);
   if (!left || !rb || !m_Manager.m_Squads.Contains(left) || left.CountAlive() <= 0 || !ACR_Roadblocks.s_aRoadblocks.Contains(rb) || !rb.m_Entity || rb.m_sFaction != m_sFaction || now - leftAt > ReturnLimit(left))
   {
    done.Insert(left);
    continue;
   }
   if (left.m_iAITask != ACR_EAITask.FREE && left.m_iAITask != ACR_EAITask.POSTED) continue;
   done.Insert(left);
   if (left.HasPlayer() || left.m_Commander != 0 || RoadblockManningSquad(rb) != null) continue;
   if (Holding() >= m_Doctrine.m_iHolders) continue;
   if (!left.ManRoadblock(rb)) continue;
   left.m_iAITask = ACR_EAITask.ROADBLOCK;
   PrintFormat("[ACR] AI %1: %2 goes back to hold the %3 at %4 it left to defend", m_sFaction, left.GetName(), ACR_Roadblocks.Name(rb.m_iType), rb.m_vPos);
  }
  foreach (ACR_Squad gone : done)
  {
   m_LeftPost.Remove(gone);
   m_LeftRoadblock.Remove(gone);
   m_LeftPostAt.Remove(gone);
  }
 }

 // The squad (AI or player) holding a roadblock, on its way back to one included.
 protected ACR_Squad RoadblockManningSquad(ACR_Roadblock rb)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.m_ActiveRoadblock == rb && squad.CountAlive() > 0) return squad;
  }
  return null;
 }

 // The squads of ours holding what we built (and fit to).
 protected int Holding()
 {
  int holding;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.GetFaction() && squad.GetFaction().GetFactionKey() == m_sFaction && IsHolder(squad) && FitToHold(squad)) holding++;
  }
  return holding;
 }

 int LeftPostsForTest() { return m_LeftPost.Count() + m_LeftRoadblock.Count(); }
 void ReturnToPostsForTest() { ReturnToPosts(); }
 void NoteLeftPostForTest(ACR_Squad squad) { NoteLeftPost(squad); }

 // ---------------------------------------------------------------- why nobody builds
 static const int BUILDER_FORTIFY = 0;
 static const int BUILDER_ROADBLOCK = 1;
 static const int BUILDER_MINES = 2;

 // Why no squad was found for a build at pos: what the side's free squads are, and how many of the
 // rest are busy. The pn9 playtest (26 Oct) logged "waits for a free squad" while the status line
 // said "free 3": the three were unarmed trucks. E.g. "(free: 3 unarmed vehicles; busy: 5
 // defending, 2 attacking)".
 protected string NoBuilderWhy(vector pos, int kind)
 {
  array<ACR_Squad> free = {};
  MySquads(free, true);
  int unarmed, crews, support, reserve, refitting, jobs, unfit, far, inReach, players;
  float nearest = -1;
  foreach (ACR_Squad squad : free)
  {
   if (!squad) continue;
   if (squad.HasPlayer() || squad.m_Commander != 0) players++;
   else if (squad == m_ReserveSquad) reserve++;
   else if (IsSupportTeam(squad)) support++;
   else if (squad.IsVehicle() && !squad.IsCombatUnit()) unarmed++;
   else if (squad.IsVehicle() && kind != BUILDER_MINES) crews++;
   else if (DueForRefit(squad)) refitting++;
   else if (squad.m_MinePlan || squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || squad.OrderState() == ACR_EOrderState.BUILD_FOB || squad.m_ActiveRoadblock || squad.m_Fortification) jobs++;
   else if ((kind == BUILDER_FORTIFY && !ACR_Fortify.CanFortify(squad)) || (kind == BUILDER_MINES && !ACR_Minefields.CanLay(squad))) unfit++;
   else
   {
    float d = vector.DistanceXZ(PositionOf(squad), pos);
    if (d < FAR_BUILDER)
    {
     inReach++;
     continue;
    }
    far++;
    if (nearest < 0 || d < nearest) nearest = d;
   }
  }
  string parts = "";
  if (unarmed > 0) parts += ", " + Several(unarmed, "unarmed vehicle", "unarmed vehicles");
  if (crews > 0) parts += ", " + Several(crews, "vehicle crew", "vehicle crews");
  if (support > 0) parts += ", " + Several(support, "support team", "support teams");
  if (reserve > 0) parts += ", the reserve";
  if (refitting > 0) parts += string.Format(", %1 due to refit", refitting);
  if (jobs > 0) parts += ", " + Several(jobs, "on another job", "on other jobs");
  if (unfit > 0 && kind == BUILDER_MINES) parts += string.Format(", %1 that can't lay mines", unfit);
  else if (unfit > 0) parts += string.Format(", %1 that can't dig in", unfit);
  if (inReach > 0) parts += string.Format(", %1 in reach but not free for this job", inReach);
  if (far > 0) parts += string.Format(", %1 further out (nearest %2 m)", far, (int)nearest);
  if (players > 0) parts += ", " + Several(players, "led by a player", "led by players");
  int defending, attacking, other;
  foreach (ACR_Squad busy : m_Manager.m_Squads)
  {
   if (!busy || !busy.GetFaction() || busy.GetFaction().GetFactionKey() != m_sFaction || free.Contains(busy)) continue;
   if (busy.m_iAITask == ACR_EAITask.DEFEND) defending++;
   else if (busy.m_iAITask == ACR_EAITask.ATTACK) attacking++;
   else other++;
  }
  string why = "(none free";
  if (!parts.IsEmpty()) why = "(free: " + parts.Substring(2, parts.Length() - 2);
  why += string.Format("; busy: %1 defending, %2 attacking, %3 other)", defending, attacking, other);
  return why;
 }

 // "1 support team", "2 support teams".
 protected static string Several(int n, string one, string many)
 {
  if (n == 1) return "1 " + one;
  return n.ToString() + " " + many;
 }

 string NoBuilderWhyForTest(vector pos, int kind) { return NoBuilderWhy(pos, kind); }

 protected ACR_Squad FortifyWorkerNear(vector pos)
 {
  ACR_Squad candidate;
  float candidateDistance = FAR_BUILDER;
  array<ACR_Squad> squads = {};
  MySquads(squads, true);
  foreach (ACR_Squad squad : squads)
  {
   if (!squad || squad.HasPlayer() || squad.m_Commander != 0 || squad == m_ReserveSquad || IsSupportTeam(squad) || !ACR_Fortify.CanFortify(squad)) continue;
   if (squad.m_MinePlan || squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
   if (squad.m_ActiveRoadblock || squad.m_Fortification) continue;
   if (DueForRefit(squad)) continue;
   float d = vector.DistanceXZ(PositionOf(squad), pos);
   if (d >= candidateDistance) continue;
   candidateDistance = d;
   candidate = squad;
  }
  return candidate;
 }

 // A squad walking back to refill, or weak enough that it soon will (ManageReinforcements), isn't
 // sent to build: the refit would take it off the job. Soak, 26 Oct: a 3-man squad, still on its
 // way to refill, was posted, rode out to dig in and was called back a minute later.
 protected bool DueForRefit(ACR_Squad squad)
 {
  return m_Refitting.Contains(squad) || squad.StrengthRatio() < m_Doctrine.m_fRefitRatio;
 }

 void SetRefittingForTest(ACR_Squad squad, bool on)
 {
  if (on) m_Refitting.Set(squad, GetGame().GetWorld().GetWorldTime() + REFIT_GIVE_UP_MS);
  else m_Refitting.Remove(squad);
 }

 // ---------------------------------------------------------------- hand-over

 // A player took command: leave every squad as it is, but forget AI tasks.
 // ---------------------------------------------------------------- squads handed to the AI
 // A player commands the side (or it has no AI commander): the squad logic runs for the squads
 // handed to the AI (squad panel, ACR_SquadAI) and only them: MySquads and DefendInDepth see
 // no other squad. No purchases, logistics, FOB building, mortars or crews for the side.
 protected bool m_bHandedOnly;
 protected int m_iHandedSeconds;
 void TickHanded()
 {
  m_bHandedOnly = true;
  GatherIntel();
  TickSapperDemos(); // its FOB raids' charges, as in Tick (AssignAttacks starts them)
  m_iHandedSeconds++;
  if (m_iHandedSeconds % TICK_SECONDS == 1)
  {
   m_iTickStarted = System.GetTickCount();
   // Its engineering is paid out of the build budget only (CanBuild), set aside as in Tick.
   AccrueBuildBudget();
   TrickleBuildBudget();
   ReleaseRoadblockBuilders();
   Assess();
   UpdateAttacks();
   AssignDefenders();
   AssignAttacks();
   PostIdle();
   int beforeBuild = m_Manager.m_Bases.GetRP(m_sFaction);
   ManageRoadblocks();
   SpendBuildBudget(beforeBuild - m_Manager.m_Bases.GetRP(m_sFaction));
   beforeBuild = m_Manager.m_Bases.GetRP(m_sFaction);
   ManageMinefields();
   SpendBuildBudget(beforeBuild - m_Manager.m_Bases.GetRP(m_sFaction));
   PrintFormat("[ACR] AI %1: commanding %2 squad(s) handed to it, %3 attack(s)", m_sFaction, ACR_SquadAI.CountFor(m_sFaction), m_Attacks.Count());
  }
  m_bHandedOnly = false;
 }

 // ---------------------------------------------------------------- the 2IC's map UI (#14 v1)
 // A line in the side's 2IC channel (ACR_TwoIC), while this runs the 2IC's squads.
 protected void TwoICReport(string text)
 {
  if (m_bHandedOnly) ACR_TwoIC.Report(m_sFaction, text);
 }

 // "Alpha 1", "Alpha 1 and Bravo 1", "3 squads".
 protected static string SquadList(array<ACR_Squad> squads)
 {
  array<string> names = {};
  foreach (ACR_Squad squad : squads)
   if (squad) names.Insert(squad.GetName());
  if (names.Count() == 1) return names[0];
  if (names.Count() == 2) return names[0] + " and " + names[1];
  return string.Format("%1 squads", names.Count());
 }

 // What the 2IC is doing, for its card: its grabs and attacks, then how many of its squads
 // hold, build, refit and wait. The first three, then "+N more".
 string TwoICDoing()
 {
  array<string> parts = {};
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (!attack.m_Target) continue;
   array<ACR_Squad> mine = {};
   foreach (ACR_Squad member : attack.m_Squads)
    if (member && member.m_bAICommanded) mine.Insert(member);
   if (mine.IsEmpty()) continue;
   string kind = "attacking";
   if (attack.m_bGrab) kind = "taking";
   else if (attack.m_bFobRaid) kind = "raiding";
   else if (attack.m_bCounter) kind = "counterattacking";
   parts.Insert(string.Format("%1 %2 (%3)", kind, attack.m_Target.m_sName, SquadList(mine)));
  }
  int holding, building, refitting, waiting;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad || !squad.m_bAICommanded || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   int task = squad.m_iAITask;
   if (task == ACR_EAITask.DEFEND) holding++;
   else if (task == ACR_EAITask.BUILD_FOB || task == ACR_EAITask.ROADBLOCK || task == ACR_EAITask.MINES || task == ACR_EAITask.FORTIFY) building++;
   else if (task == ACR_EAITask.RESUPPLY || m_Refitting.Contains(squad)) refitting++;
   else if (task == ACR_EAITask.FREE || task == ACR_EAITask.POSTED) waiting++;
  }
  if (holding > 0) parts.Insert(string.Format("%1 holding", holding));
  if (building > 0) parts.Insert(string.Format("%1 building", building));
  if (refitting > 0) parts.Insert(string.Format("%1 refitting", refitting));
  if (waiting > 0) parts.Insert(string.Format("%1 in reserve", waiting));
  if (parts.IsEmpty()) return "nothing yet";
  string doing = parts[0];
  for (int i = 1; i < parts.Count() && i < 3; i++) doing += ", " + parts[i];
  if (parts.Count() > 3) doing += string.Format(", +%1 more", parts.Count() - 3);
  return doing;
 }

 // A 2IC with fewer than ACR_TwoIC.SMALL_FORCE squads: it only grabs empty bases, and holds with at
 // most half its force.
 protected float m_fNextSmallLog;
 bool SmallForce()
 {
  return m_bHandedOnly && ACR_TwoIC.On() && ACR_TwoIC.SmallForce(m_sFaction);
 }

 // The most squads that stay on what they built: the doctrine's m_iHolders, a small force's half.
 protected int HoldersCap()
 {
  if (!SmallForce()) return m_Doctrine.m_iHolders;
  return Math.Min(m_Doctrine.m_iHolders, ACR_SquadAI.CountFor(m_sFaction) / 2);
 }

 bool SmallForceForTest()
 {
  bool handed = m_bHandedOnly;
  m_bHandedOnly = true;
  bool small = SmallForce();
  m_bHandedOnly = handed;
  return small;
 }

 int HoldersCapForTest()
 {
  bool handed = m_bHandedOnly;
  m_bHandedOnly = true;
  int cap = HoldersCap();
  m_bHandedOnly = handed;
  return cap;
 }

 // What a squad of the 2IC's is doing, for its panel: "taking Morton", "holding Lamentin", ...
 string TaskText(ACR_Squad squad)
 {
  if (!squad) return "";
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (!attack.m_Target || !attack.m_Squads.Contains(squad)) continue;
   if (attack.m_bGrab) return "taking " + attack.m_Target.m_sName;
   if (attack.m_bFobRaid) return "raiding " + attack.m_Target.m_sName;
   if (attack.m_bCounter) return "counterattacking " + attack.m_Target.m_sName;
   return "attacking " + attack.m_Target.m_sName;
  }
  string at = "";
  if (squad.m_AITarget) at = " " + squad.m_AITarget.m_sName;
  int task = squad.m_iAITask;
  if (task == ACR_EAITask.DEFEND) return "holding" + at;
  if (task == ACR_EAITask.POSTED) return "posted" + at;
  if (task == ACR_EAITask.RESUPPLY || m_Refitting.Contains(squad)) return "refitting";
  if (task == ACR_EAITask.BUILD_FOB) return "building a FOB";
  if (task == ACR_EAITask.ROADBLOCK) return "on a roadblock";
  if (task == ACR_EAITask.MINES) return "laying mines";
  if (task == ACR_EAITask.FORTIFY) return "digging in";
  if (task == ACR_EAITask.SWEEP) return "clearing a road";
  if (task == ACR_EAITask.ATTACK) return "attacking" + at;
  return "in reserve";
 }

 // ---------------------------------------------------------------- the 2IC's requests (#14 v1)
 // A player's "Ask the 2IC" (ACR_TwoICRequests), answered each second ahead of the 2IC's own
 // plans: Support me here and Hold this send the nearest free squad (a small force takes one off a
 // grab: requests first), which stays on it (AssignDefenders leaves it) until the support runs out
 // or the hold is released; Take this base grabs an empty base at once and makes an enemy one the
 // next attack's target.
 static const float REQUEST_REACH = 2500;   // m: the farthest a squad is sent to support or hold
 static const float SUPPORT_MS = 600000;    // a support stays 10 min once there
 static const float ARRIVED = 60;           // m from the spot: there
 protected ref map<ACR_Squad, int> m_OnRequest = new map<ACR_Squad, int>(); // squad -> request id
 protected ACR_Base m_RequestedTarget;
 protected int m_iRequestedBy;

 bool OnRequest(ACR_Squad squad)
 {
  return squad && m_OnRequest.Contains(squad);
 }

 // "Take this base" for an enemy base, while it stands.
 ACR_Base RequestedTarget()
 {
  if (m_RequestedTarget && (m_RequestedTarget.m_bRetired || m_RequestedTarget.m_sOwner == m_sFaction)) m_RequestedTarget = null;
  return m_RequestedTarget;
 }

 void AnswerTwoIC(ACR_TwoICRequest r)
 {
  bool handed = m_bHandedOnly;
  m_bHandedOnly = true;
  AnswerRequest(r);
  m_bHandedOnly = handed;
 }

 protected void AnswerRequest(ACR_TwoICRequest r)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  // Its squad is gone (lost, or a player took it back): another one, or why not.
  if (!r.m_sSquad.IsEmpty() && (!r.m_Squad || !m_Manager.m_Squads.Contains(r.m_Squad) || !r.m_Squad.m_bAICommanded || r.m_Squad.CountAlive() == 0))
  {
   string lost = r.m_sSquad;
   EndTwoICRequest(r);
   r.Set(ACR_ETwoICState.WAITING, lost + " can't any more: finding another");
  }
  int state = r.m_iState;
  if (state == ACR_ETwoICState.WAITING || (state == ACR_ETwoICState.CANT && r.m_bQueued && now >= r.m_fNextTry))
  {
   TryRequest(r);
   if (r.m_iState == ACR_ETwoICState.CANT)
   {
    r.m_fNextTry = now + ACR_TwoICRequests.RETRY_MS;
    if (!r.m_bQueued && r.m_fUntil <= 0) r.m_fUntil = now + ACR_TwoICRequests.DROP_MS;
   }
   return;
  }
  if (r.m_iKind == ACR_ETwoICAsk.TAKE)
  {
   if (!r.m_Base) return;
   if (r.m_Base.m_sOwner == m_sFaction)
   {
    EndTwoICRequest(r);
    r.Set(ACR_ETwoICState.DONE, "took " + r.m_Base.m_sName);
    return;
   }
   // The grab or the attack ended without it.
   if (state == ACR_ETwoICState.COMING && !IsAttacking(r.m_Base) && RequestedTarget() != r.m_Base)
   {
    EndTwoICRequest(r);
    r.Set(ACR_ETwoICState.CANT, "the attack on " + r.m_Base.m_sName + " didn't take it");
    r.m_fNextTry = now + ACR_TwoICRequests.RETRY_MS;
    if (!r.m_bQueued) r.m_fUntil = now + ACR_TwoICRequests.DROP_MS;
   }
   return;
  }
  if (state == ACR_ETwoICState.COMING && r.m_Squad && vector.DistanceXZ(PositionOf(r.m_Squad), r.m_vPos) <= ARRIVED)
  {
   if (r.m_iKind == ACR_ETwoICAsk.HOLD) r.Set(ACR_ETwoICState.HOLDING, string.Format("%1 holds %2 until you release it", r.m_sSquad, r.m_sPlace));
   else
   {
    r.m_fUntil = now + SUPPORT_MS;
    r.Set(ACR_ETwoICState.SUPPORTING, string.Format("%1 supports you at %2 for 10 min", r.m_sSquad, r.m_sPlace));
   }
   return;
  }
  if (state == ACR_ETwoICState.SUPPORTING && now >= r.m_fUntil)
  {
   string done = r.m_sSquad;
   EndTwoICRequest(r);
   r.Set(ACR_ETwoICState.DONE, done + " is done supporting and free again");
  }
 }

 protected void TryRequest(ACR_TwoICRequest r)
 {
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  if (mine.IsEmpty())
  {
   r.Set(ACR_ETwoICState.CANT, "I have no squads: every squad of ours is a player's");
   return;
  }
  if (r.m_iKind == ACR_ETwoICAsk.TAKE)
  {
   TryTake(r, mine);
   return;
  }
  ACR_Squad squad = RequestSquad(mine, r.m_vPos);
  if (!squad)
  {
   r.Set(ACR_ETwoICState.CANT, string.Format("none of my %1 squads is free (%2)", mine.Count(), BusyList(mine)));
   return;
  }
  int dist = (int)vector.DistanceXZ(PositionOf(squad), r.m_vPos);
  if (dist > REQUEST_REACH)
  {
   r.Set(ACR_ETwoICState.CANT, string.Format("my nearest free squad, %1, is %2 m away", squad.GetName(), dist));
   return;
  }
  string from = ACR_TwoIC.Where(squad);
  Forget(squad);
  squad.m_iAITask = ACR_EAITask.DEFEND;
  squad.m_AITarget = null;
  if (r.m_iKind == ACR_ETwoICAsk.HOLD) squad.SetMode(ACR_EMoveMode.COMBAT);
  else squad.SetMode(ACR_EMoveMode.FAST);
  Travel(squad, r.m_vPos, true, 0);
  m_OnRequest.Set(squad, r.m_iId);
  r.m_Squad = squad;
  r.m_sSquad = squad.GetName();
  r.m_fUntil = 0;
  r.Set(ACR_ETwoICState.COMING, string.Format("%1, %2", squad.GetName(), from));
  PrintFormat("[ACR] 2IC %1: %2 answers request %3 (%4 m)", m_sFaction, squad.GetName(), r.m_iId, dist);
 }

 protected void TryTake(ACR_TwoICRequest r, array<ACR_Squad> mine)
 {
  ACR_Base base = r.m_Base;
  if (!base || base.m_bRetired)
  {
   r.Set(ACR_ETwoICState.DONE, "there's no base there any more");
   return;
  }
  if (base.m_sOwner == m_sFaction)
  {
   r.Set(ACR_ETwoICState.DONE, base.m_sName + " is ours");
   return;
  }
  if (IsAttacking(base))
  {
   r.Set(ACR_ETwoICState.COMING, "already attacking " + base.m_sName);
   return;
  }
  if (base.m_sOwner.IsEmpty())
  {
   ACR_Squad runner = RequestSquad(mine, base.GetOrigin());
   if (!runner)
   {
    r.Set(ACR_ETwoICState.CANT, string.Format("none of my %1 squads is free to take %2 (%3)", mine.Count(), base.m_sName, BusyList(mine)));
    return;
   }
   int dist = (int)vector.DistanceXZ(PositionOf(runner), base.GetOrigin());
   RequestGrab(runner, base);
   r.m_Squad = runner;
   r.m_sSquad = runner.GetName();
   r.m_fUntil = 0;
   r.Set(ACR_ETwoICState.COMING, string.Format("%1 is going to take %2 (%3 m away)", runner.GetName(), base.m_sName, dist));
   return;
  }
  if (SmallForce())
  {
   r.Set(ACR_ETwoICState.CANT, string.Format("with %1 squads I only take empty bases, not %2", ACR_SquadAI.CountFor(m_sFaction), base.m_sName));
   return;
  }
  float need = AttackNeed(base) * Pressure();
  array<ACR_Squad> free = {};
  MySquads(free, true);
  int available;
  foreach (ACR_Squad counted : free) available += counted.Strength();
  m_RequestedTarget = base;
  m_iRequestedBy = r.m_iId;
  r.m_fUntil = 0;
  if (available >= need) r.Set(ACR_ETwoICState.COMING, string.Format("going for %1 with my free squads (strength %2 of the %3 it needs)", base.m_sName, available, (int)need));
  else r.Set(ACR_ETwoICState.CANT, string.Format("not yet: I have %1 of the %2 strength it needs free", available, (int)need));
 }

 // A request ends (done, cancelled, released, its squad lost): its squad goes back to the 2IC's
 // own plans; a grab made for it is called off; an attack target it set is dropped.
 void EndTwoICRequest(ACR_TwoICRequest r)
 {
  if (m_iRequestedBy == r.m_iId)
  {
   m_RequestedTarget = null;
   m_iRequestedBy = 0;
  }
  ACR_Squad squad = r.m_Squad;
  r.m_Squad = null;
  r.m_sSquad = "";
  if (!squad) return;
  int id;
  if (m_OnRequest.Find(squad, id) && id == r.m_iId)
  {
   m_OnRequest.Remove(squad);
   if (squad.m_iAITask == ACR_EAITask.DEFEND && !squad.m_AITarget) squad.m_iAITask = ACR_EAITask.FREE;
  }
  if (r.m_iKind == ACR_ETwoICAsk.TAKE && r.m_Base && r.m_Base.m_sOwner != m_sFaction && OnGrab(squad))
  {
   Forget(squad);
   squad.m_iAITask = ACR_EAITask.FREE;
  }
 }

 // The nearest squad of ours free to answer: a fighting squad that isn't holding what it built,
 // on another request or falling back; a small force takes one off a grab too (requests first).
 protected ACR_Squad RequestSquad(array<ACR_Squad> mine, vector pos)
 {
  bool small = SmallForce();
  ACR_Squad best;
  float bestDistance = float.MAX;
  foreach (ACR_Squad squad : mine)
  {
   if (!squad.IsCombatUnit() || IsSupportTeam(squad) || OnRequest(squad) || IsHolder(squad) || IsRetreating(squad)) continue;
   bool free = squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED;
   if (!free && !(small && squad.m_iAITask == ACR_EAITask.ATTACK && OnGrab(squad))) continue;
   float d = vector.DistanceXZ(PositionOf(squad), pos);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = squad;
  }
  return best;
 }

 protected bool OnGrab(ACR_Squad squad)
 {
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (attack.m_bGrab && attack.m_Squads.Contains(squad)) return true;
  }
  return false;
 }

 // What the squads are busy with, for a "none free" answer.
 protected string BusyList(array<ACR_Squad> mine)
 {
  int attacking, defending, holding, requests, support, other;
  foreach (ACR_Squad squad : mine)
  {
   if (OnRequest(squad)) requests++;
   else if (IsHolder(squad)) holding++;
   else if (IsSupportTeam(squad) || !squad.IsCombatUnit()) support++;
   else if (squad.m_iAITask == ACR_EAITask.ATTACK) attacking++;
   else if (squad.m_iAITask == ACR_EAITask.DEFEND) defending++;
   else other++;
  }
  string list = "";
  if (attacking > 0) list += string.Format(", %1 attacking", attacking);
  if (defending > 0) list += string.Format(", %1 defending", defending);
  if (holding > 0) list += string.Format(", %1 holding what they built", holding);
  if (requests > 0) list += string.Format(", %1 on requests", requests);
  if (support > 0) list += string.Format(", %1 support teams", support);
  if (other > 0) list += string.Format(", %1 busy otherwise", other);
  if (list.IsEmpty()) return "all busy";
  return list.Substring(2, list.Length() - 2);
 }

 // A grab of an empty base by this squad, as AssignAttacks makes one.
 protected void RequestGrab(ACR_Squad runner, ACR_Base empty)
 {
  Forget(runner);
  ACR_AIAttack grab = new ACR_AIAttack();
  grab.m_Target = empty;
  grab.m_bGrab = true;
  grab.m_bAssaulting = true;
  grab.m_fAssaultStart = GetGame().GetWorld().GetWorldTime();
  grab.Add(runner, empty.GetOrigin(), -1);
  grab.m_iStartStrength = runner.Strength();
  grab.m_fGrabSeconds = vector.DistanceXZ(PositionOf(runner), empty.GetOrigin()) / 1.2 + 180;
  if (runner.IsMounted()) grab.m_fGrabSeconds = Math.Min(600, grab.m_fGrabSeconds);
  runner.m_iAITask = ACR_EAITask.ATTACK;
  runner.m_AITarget = empty;
  runner.SetMode(ACR_EMoveMode.FAST);
  Travel(runner, empty.GetOrigin(), false, 100);
  m_Attacks.Insert(grab);
  PrintFormat("[ACR] 2IC %1: %2 grabs %3 as asked", m_sFaction, runner.GetName(), empty.m_sName);
 }

 // The scored FOB sites behind 'front' (2IC suggestions and Establish FOB, ACR_TwoICSuggest), as
 // FindFobSiteStep searches them: 'most' more validated; true when the search is complete.
 bool TwoICFobSites(ACR_Base front, int most, notnull array<ref ACR_FobSite> sites)
 {
  if (!front) return true;
  ACR_Base deployBase = NearestDeployBase(front);
  vector dir = Vector(0, 0, -1);
  if (deployBase)
  {
   vector toDeploy = deployBase.GetOrigin() - front.GetOrigin();
   toDeploy[1] = 0;
   if (toDeploy.LengthSq() > 0.001)
   {
    toDeploy.Normalize();
    dir = toDeploy;
   }
  }
  string none;
  return ACR_FobSiting.Step(m_sFaction, front, dir, NearestEnemyBaseTo(front), most, sites, none);
 }

 bool IsAttackingForTest(ACR_Base base)
 {
  return IsAttacking(base);
 }

 bool CanBuildHandedForTest(int cost, int reserve)
 {
  m_bHandedOnly = true;
  bool ok = CanBuild(cost, reserve);
  m_bHandedOnly = false;
  return ok;
 }

 // A squad given back to its player: out of every plan of ours.
 void Forget(ACR_Squad squad)
 {
  m_OnRequest.Remove(squad);
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   int index = attack.m_Squads.Find(squad);
   if (index >= 0) attack.Remove(index);
  }
  m_RetreatingUntil.Remove(squad);
  m_ReinforceRetryAt.Remove(squad);
  m_Refitting.Remove(squad);
  m_ForwardAt.Remove(squad);
  if (m_ReserveSquad == squad) m_ReserveSquad = null;
  if (m_FobBuilder == squad) m_FobBuilder = null;
  if (m_FobEscort == squad) m_FobEscort = null;
 }

 bool PlansFor(ACR_Squad squad)
 {
  foreach (ACR_AIAttack attack : m_Attacks)
   if (attack.m_Squads.Contains(squad)) return true;
  return squad == m_ReserveSquad || m_Refitting.Contains(squad);
 }

 protected void ReleaseAll()
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == m_sFaction) squad.m_iAITask = ACR_EAITask.FREE;
  m_Attacks.Clear();
  m_RetreatingUntil.Clear();
  m_ReinforceRetryAt.Clear();
  m_Refitting.Clear();
  m_ReserveSquad = null;
  m_iSaveForEntry = -1;
  m_iLastSavedLog = -1;
  m_FobBuilder = null;
  m_FobEscort = null;
 }
}
