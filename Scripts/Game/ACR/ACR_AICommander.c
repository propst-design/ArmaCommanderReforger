enum ACR_EAITask
{
 FREE,      // available for orders
 DEFEND,    // holding a threatened base
 ATTACK,    // part of a base attack
 POSTED,    // idle, moved up to a frontline base
 RESUPPLY,  // on its way to an ammo / repair / fuel / medical vehicle
 BUILD_FOB, // establishing a forward base
 ROADBLOCK, // establishing or holding a forward roadblock
 MINES      // laying a minefield on an approach (-acrMinesAI)
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
 static const float FIRE_BASE_MAX_S = 240;    // s of supporting fire before it joins the assault anyway
 static const float FIRE_BASE_MOVE_S = 120;   // s to reach a firing spot before it joins instead
 static const float RETREAT_COOLDOWN_MS = 60000;
 // A reserve riding or regrouping is on its way: re-posting it would cancel that (20 Oct
 // live: a regrouping reserve was re-posted every 15 s for 15 min and never arrived).
 static bool ReserveMoving(ACR_EOrderState state) { return state == ACR_EOrderState.RIDE || state == ACR_EOrderState.REGROUP; }
 static const float REINFORCE_ATTACK_DISTANCE = 1400;
 static const float REINFORCE_ATTACK_MOUNTED = 2200;
 static const float ASSAULT_JOIN_EXTRA = 400; // m past the staging ring a squad may be to join an assault already going in
 static const float STAGE_STRETCH = 0.5;      // late joiners may stretch the forming up by half of STAGING_TIMEOUT
 // Initiative: with no attack launched for a while, the commander accepts
 // worse odds (down to 65% of the usual strength) and commits its reserve,
 // so two AI sides don't sit facing each other forever.
 static const float QUIET_MINUTES = 4;
 static const float PRESSURE_PER_MINUTE = 0.05;
 static const float MIN_PRESSURE = 0.65;
 protected float m_fLastOffensive = -1;
 protected float m_fNextWeakLog;

 protected float Pressure()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (m_fLastOffensive < 0) m_fLastOffensive = now;
  float quiet = (now - m_fLastOffensive) / 60000 - QUIET_MINUTES;
  if (quiet <= 0) return 1;
  return Math.Max(MIN_PRESSURE, 1 - quiet * PRESSURE_PER_MINUTE);
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
 static const float RETREAT_RATIO = 0.4;   // a squad below 40% in contact falls back
 static const float ABORT_RATIO = 0.35;    // an attack below 35% of its strength is called off
 static const int SUPPORT_RESERVE = 15;    // RP the AI keeps back when calling supports
 static const int SUPPORT_BUDGET_MAX = 60;  // most RP it holds back from buying for supports
 static const int MAX_GRABS = 3;           // empty bases it goes for at the same time (at least)
 static const int MAX_GRABS_BIG = 8;       // ... at most, on maps with many empty bases
 // Planning work one strategic tick may do before further grabs and attacks wait for the next
 // (Everon soak 23 Oct: an air lift search, a ride request and a water route in one tick froze
 // the server for 200 ms; each costs up to ~100 ms on its own).
 static const int PLAN_BUDGET_MS = 50;
 protected int m_iTickStarted;

 void ACR_AICommander(ACR_Manager manager, string factionKey)
 {
  m_Manager = manager;
  m_sFaction = factionKey;
 }

 // Called every second by the manager.
 void Tick(bool noPlayerCommander)
 {
  if (m_bActive != noPlayerCommander)
  {
   m_bActive = noPlayerCommander;
   PrintFormat("[ACR] AI commander %1: %2", m_sFaction, m_bActive);
   if (!m_bActive) ReleaseAll();
  }
  if (!m_bActive) return;
  GatherIntel();
  TickSapperDemos();
  m_iSeconds++;
  if (m_iSeconds % TICK_SECONDS != 1) return;
  int t = System.GetTickCount();
  m_iTickStarted = t;
  ReleaseRoadblockBuilders();
  Assess();
  t = ACR_Perf.Lap("ai.assess", t);
  UpdateAttacks();
  t = ACR_Perf.Lap("ai.attacks", t);
  AssignDefenders();
  t = ACR_Perf.Lap("ai.defend", t);
  AssignAttacks();
  t = ACR_Perf.Lap("ai.plan", t);
  PostIdle();
  PositionMortars();
  Recrew();
  t = ACR_Perf.Lap("ai.post", t);
  ManageLogistics();
  ManageFobBuild();
  ManageFobUpgrades();
  ManageRoadblocks();
  ManageMinefields();
  t = ACR_Perf.Lap("ai.logistics", t);
  Requisition();
  t = ACR_Perf.Lap("ai.buy", t);
  LogStatus();
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
  if (safe && ThreatTo(safe) == 0 && vector.DistanceXZ(PositionOf(squad), safe.GetOrigin()) <= safe.m_fRadius && squad.KnownEnemies(300) == 0 && squad.StrengthRatio() >= RETREAT_RATIO && (squad.m_iAmmoPct < 0 || squad.m_iAmmoPct >= OUT_OF_AMMO))
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

 bool HasKnownEnemyBase()
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (!base.m_sOwner.IsEmpty() && base.m_sOwner != m_sFaction && Visible(base)) return true;
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
   if (base.m_sOwner == m_sFaction || base.m_sOwner.IsEmpty() || !Visible(base) || base == except) continue;
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

 // RP kept back from unit purchases so a fire mission is always affordable:
 // the dearest mortar mission plus the usual reserve (capped).
 protected int SupportBudget()
 {
  int dearest;
  foreach (ACR_SupportEntry entry : m_Manager.m_Bases.GetFaction(m_sFaction).m_aSupports)
  {
   if (entry.m_eType != ACR_ESupportType.MORTAR_HE_SMALL && entry.m_eType != ACR_ESupportType.MORTAR_HE_LARGE && entry.m_eType != ACR_ESupportType.MORTAR_SMOKE) continue;
   dearest = Math.Max(dearest, entry.m_iCost);
  }
  if (dearest == 0) return 0;
  return Math.Min(SUPPORT_BUDGET_MAX, dearest + SUPPORT_RESERVE);
 }

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
  PrintFormat("[ACR] AI %1 support (%2): %3", m_sFaction, why, result);
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
   if (!ours && !base.m_sOwner.IsEmpty()) defence += UNKNOWN_DEFENCE; // fog of war: expect more than we've seen
   m_Defence.Set(base, defence);
   m_Threat.Set(base, threat);
   string before;
   if (m_LastOwner.Find(base, before) && before == m_sFaction && base.m_sOwner != m_sFaction && !base.m_sOwner.IsEmpty())
   {
    // Hit back while the enemy is still thin on the ground.
    m_CounterTarget = base;
    m_fCounterUntil = GetGame().GetWorld().GetWorldTime() + 300000;
    PrintFormat("[ACR] AI %1: lost %2, planning a counterattack", m_sFaction, base.m_sName);
   }
   m_LastOwner.Set(base, base.m_sOwner);
  }
 }

 protected int DefenceOf(ACR_Base base) { int v; m_Defence.Find(base, v); return v; }
 protected int ThreatTo(ACR_Base base) { int v; m_Threat.Find(base, v); return v; }

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
   ACR_Base held = squad.m_AITarget;
   if (!held || held.m_sOwner != m_sFaction || ThreatTo(held) == 0)
   {
    if (held && m_Manager.HasPrimedDemolition(held)) continue;
    squad.m_iAITask = ACR_EAITask.FREE;
    continue;
   }
   // A defence that is being overrun: the badly hurt get out rather than die in place.
   bool overrun = ThreatTo(held) > DefenceOf(held) * 2;
   if (overrun && squad.StrengthRatio() < RETREAT_RATIO && squad.KnownEnemies(300) > 0)
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
   if (threat > defence * CONCEDE_RATIO && defence > 0) continue; // lost cause
   float needed = threat * DEFENSE_RATIO - defence;
   if (needed <= 0) continue;
   SortByDistance(free, base.GetOrigin());
   int k = 0;
   while (k < free.Count() && needed > 0)
   {
    ACR_Squad squad = free[k];
    if (vector.DistanceXZ(PositionOf(squad), base.GetOrigin()) > DEFENSE_DISTANCE * 3) { k++; continue; }
    if (IsSupportTeam(squad) && free.Count() > 1) { k++; continue; }
    squad.SetMode(ACR_EMoveMode.FAST);
    Travel(squad, base.GetOrigin(), true, 0);
    squad.m_iAITask = ACR_EAITask.DEFEND;
    squad.m_AITarget = base;
    needed -= squad.Strength();
    free.RemoveOrdered(k);
    PrintFormat("[ACR] AI %1: %2 defends %3 (threat %4)", m_sFaction, squad.GetName(), base.m_sName, threat);
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
    PrintFormat("[ACR] AI %1: QRF reserve %2 committed to defend %3 (threat %4)", m_sFaction, qrf.GetName(), base.m_sName, threat);
   }
  }
  DefendInDepth();
  // Defensive fires: shell the attackers of a base that is losing the fight.
  foreach (ACR_Base held : ACR_Base.s_All)
  {
   if (held.m_sOwner != m_sFaction || ThreatTo(held) <= DefenceOf(held)) continue;
   vector seenAt;
   ACR_Squad attacker = EnemyNear(held.GetOrigin(), THREAT_DISTANCE + held.m_fRadius, seenAt);
   if (!attacker) continue;
   if (ThreatTo(held) >= 8 && CallSupport(ACR_ESupportType.HELI_GUN_RUN, seenAt, "gun run at " + held.m_sName)) break;
   if (ThreatTo(held) >= 8 && CallSupport(ACR_ESupportType.ROCKET_STRIKE, seenAt, "rocket strike at " + held.m_sName)) break;
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
  PrintFormat("[ACR] AI %1: %2 falls back from the forward line to %3 (%4)", m_sFaction, squad.GetName(), base.m_sName, why);
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
   // How many go forward: half, the base garrison counting as one more squad at the point;
   // at least one squad (or the garrison) stays as the reserve.
   int extra = 0;
   if (base.m_iGarrisonLeft > 0) extra = 1;
   int forward = (defenders.Count() + extra) / 2;
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
    PrintFormat("[ACR] AI %1: %2 moves out to a forward line %3 m from %4 towards the attack (enemy seen %5 m out, %6 stay in reserve)", m_sFaction, goer.GetName(), Math.Round(depth), base.m_sName, Math.Round(enemyDistance), defenders.Count() - picked.Count());
   }
  }
 }

 // ---------------------------------------------------------------- 3. attacks

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
  return Math.Max(10, DefenceOf(target) * ATTACK_RATIO * 0.65) + target.m_iGarrisonLeft;
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
     Travel(squad, stage, false, 0);
    }
    free.RemoveOrdered(k);
    PrintFormat("[ACR] AI %1: %2 reinforces the attack on %3 (%4/%5)", m_sFaction, squad.GetName(), target.m_sName, strength, (int)need);
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
      PrintFormat("[ACR] AI %1: %2 held in reserve at %3", m_sFaction, reserve.GetName(), home.m_sName);
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
  foreach (ACR_Base open : ACR_Base.s_All) if (open.m_sOwner.IsEmpty() && Visible(open)) unclaimed++;
  int maxGrabs = Math.Clamp(unclaimed / 6, MAX_GRABS, MAX_GRABS_BIG);
  while (grabs < maxGrabs && !free.IsEmpty() && !PlanBudgetSpent())
  {
   ACR_Base empty;
   ACR_Squad runner;
   float bestTime = float.MAX;
   foreach (ACR_Base candidate : ACR_Base.s_All)
   {
    if (!candidate.m_sOwner.IsEmpty() || !Visible(candidate) || IsAttacking(candidate) || CoolingDown(candidate)) continue;
    foreach (ACR_Squad squad : free)
    {
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
   PrintFormat("[ACR] AI %1: %2 grabs empty %3 (about %4 s away)", m_sFaction, runner.GetName(), empty.m_sName, (int)bestTime);
  }
  if (PlanBudgetSpent()) return;
  ReinforceAttacks(free);
  if (free.IsEmpty() || PlanBudgetSpent()) return;
  // Plenty of idle troops: open more fronts. On large maps with many bases and idle armies,
  // allow up to 6 extra attacks so a dominant side doesn't sit idle.
  int maxExtraAttacks = 3;
  if (ACR_Base.s_All.Count() > 24) maxExtraAttacks = 6;
  maxAttacks += Math.Min(maxExtraAttacks, free.Count() / 3);
  if (attacks >= maxAttacks && !counterDue) return;

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
  vector from = PositionOf(free[0]);
  array<ACR_Base> candidates = {};
  array<float> scores = {};
  if (counterDue) candidates.Insert(m_CounterTarget);
  else
  {
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_sOwner == m_sFaction || base.m_sOwner.IsEmpty() || !Visible(base) || IsAttacking(base) || CoolingDown(base)) continue;
    float cost = Math.Max(10, DefenceOf(base) * ATTACK_RATIO);
    float dist = NearestFreeDistance(free, base.GetOrigin());
    float score = cost * (dist / 1000 + 0.1);
    if (base.m_bContested || (DefenceOf(base) <= UNKNOWN_DEFENCE + 2 && dist < 1200))
     score *= 0.65;
    if (base.m_bBuilt)
    {
     score *= 0.35;
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
   if (counterDue) need = need * 0.7; // strike before they dig in
   need = need * pressure;
   if (ACR_Manager.ThreatOption() == 2) need = need * 0.85; // High: launch a viable force sooner
   if (!wanted) { wanted = candidate; wantedRequired = need; }
   if (available >= need) { target = candidate; required = need; break; }
  }
  if (!target)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   if (wanted && now >= m_fNextWeakLog)
   {
    m_fNextWeakLog = now + 60000;
    PrintFormat("[ACR] AI %1: not strong enough for any target (best: %2, %3/%4, pressure x%5)", m_sFaction, wanted.m_sName, available, (int)wantedRequired, pressure);
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
   PrintFormat("[ACR] AI %1: targeted enemy forward base %2 for sapper demolition", m_sFaction, target.m_sName);
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
  bool airLiftReady = SupportReady(ACR_ESupportType.AIR_LIFT);
  foreach (ACR_Squad squad : free)
  {
   int sqStrength = squad.Strength();
   remainingPool -= sqStrength;
   if (squad.StrengthRatio() < 0.55 && strength + remainingPool >= required) continue;
   float distToTarget = vector.DistanceXZ(PositionOf(squad), target.GetOrigin());
   if (strength >= required * margin)
   {
    if (strength >= goal) break;
    float reach = REINFORCE_ATTACK_DISTANCE;
    if (squad.IsMounted()) reach = REINFORCE_ATTACK_MOUNTED;
    if (distToTarget > reach || IsSupportTeam(squad)) continue;
   }
   else if (!squad.IsMounted() && !airLiftReady && distToTarget > REINFORCE_ATTACK_DISTANCE * 1.5 && strength >= required)
   {
    continue;
   }
   attack.m_Squads.Insert(squad);
   strength += sqStrength;
  }
  if (strength < required)
  {
   PrintFormat("[ACR] AI %1: not strong enough for %2 (%3/%4)", m_sFaction, target.m_sName, strength, required);
   return; // Requisition() will add troops
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
  attack.m_fStageDeadline = GetGame().GetWorld().GetWorldTime() + STAGING_TIMEOUT * 1000;
  attack.m_fStageLimit = attack.m_fStageDeadline + STAGING_TIMEOUT * STAGE_STRETCH * 1000;
  // Motorized assault: an armed carrier (APC) in the attack takes an infantry squad to the
  // staging point and stays there with it as base of fire.
  array<ACR_Squad> carried = {};
  if (!empty && m_Manager.m_Rides) PairCarriers(attack, carried);
  foreach (ACR_Squad squad : attack.m_Squads)
  {
   squad.m_iAITask = ACR_EAITask.ATTACK;
   squad.m_AITarget = target;
   if (carried.Contains(squad)) continue; // the ride has its orders
   squad.SetMode(ACR_EMoveMode.FAST);
   // Nobody to fight at an empty base: go straight in.
   if (empty) Travel(squad, target.GetOrigin(), false, 100);
   else Travel(squad, attack.StagingOf(squad), false, 0);
  }
  attack.m_bAssaulting = empty;
  m_Attacks.Insert(attack);
  m_fLastOffensive = GetGame().GetWorld().GetWorldTime();
  if (pressure < 1) PrintFormat("[ACR] AI %1: taking the initiative against %2 (accepting x%3 odds after a quiet spell)", m_sFaction, target.m_sName, pressure);
  string kind = "attacking";
  if (attack.m_bCounter) { kind = "COUNTERATTACKING"; m_CounterTarget = null; }
  PrintFormat("[ACR] AI %1: %2 %3 with %4 squad(s), strength %5/%6", m_sFaction, kind, target.m_sName, attack.m_Squads.Count(), strength, required);
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
    PrintFormat("[ACR] AI %1: %2 can't carry %3 (%4)", m_sFaction, carrier.GetName(), best.GetName(), why);
    continue;
   }
   if (c < attack.m_aStaging.Count()) attack.m_aStaging[c] = stage;
   int riderAt = attack.m_Squads.Find(best);
   if (c < attack.m_aSlot.Count() && riderAt >= 0 && riderAt < attack.m_aSlot.Count()) attack.m_aSlot[c] = attack.m_aSlot[riderAt];
   carried.Insert(best);
   carried.Insert(carrier);
   PrintFormat("[ACR] AI %1: %2 carries %3 to the staging point for %4", m_sFaction, carrier.GetName(), best.GetName(), attack.m_Target.m_sName);
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
  PrintFormat("[ACR] AI %1: forced attack (debug menu)", m_sFaction);
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
 static const float REFIT_WALK_RATIO = 0.5;       // idle squads below this strength walk back to a quiet base to refill
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
   if (point && point.GetFactionKey() == m_sFaction && vector.DistanceXZ(point.GetOrigin(), base.GetOrigin()) <= base.m_fRadius + 60) return true;
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
 static const float RIDE_DISTANCE = 900;
 static const float AIRLIFT_DISTANCE = 2000;
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

  bool isLong = distance >= AIRLIFT_DISTANCE;
  bool isDetour = false;
  if (!isLong && distance >= RIDE_DISTANCE)
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
   PrintFormat("[ACR] AI %1: no air lift for %2 (%3), next try in %4 s", m_sFaction, squad.GetName(), result, (int)(AIRLIFT_RETRY_MS / 1000));
   return false;
  }
  PrintFormat("[ACR] AI %1: air lift for %2 to %3 (%4 m): %5", m_sFaction, squad.GetName(), ACR_Manager.Grid(destination), (int)distance, result);
  return true;
 }

 protected void Travel(ACR_Squad squad, vector destination, bool defend, float dropShort)
 {
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
   if (m_Manager.m_Rides && vector.DistanceXZ(PositionOf(squad), destination) > RIDE_DISTANCE)
   {
    string why;
    if (m_Manager.m_Rides.Request(squad, destination, dropShort, prefab, radius, order, kind, why, true)) return;
   }
  }
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
  PrintFormat("[ACR] AI %1: %2 falls back to %3 (%4)", m_sFaction, squad.GetName(), where, why);
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
 // cover, it can't reach a firing spot in time, or after FIRE_BASE_MAX_S.
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
  if (attack.m_fAssaultStart > 0 && now - attack.m_fAssaultStart > FIRE_BASE_MAX_S * 1000) { LiftFire(attack, "time up"); return; }
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
   PrintFormat("[ACR] AI %1: %2 (base of fire) suppresses the enemy seen at %3 (%4 m)", m_sFaction, fire.GetName(), target.m_sName, Math.Round(distance));
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
  PrintFormat("[ACR] AI %1: %2 (base of fire) moves up to %3 m from the enemy at %4: %5", m_sFaction, fire.GetName(), Math.Round(stand), target.m_sName, spot);
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
  PrintFormat("[ACR] AI %1: %2 (base of fire) holds overwatch on %3 from %4 m (no enemy seen there yet): %5", m_sFaction, fire.GetName(), target.m_sName, Math.Round(vector.DistanceXZ(spot, target.GetOrigin())), spot);
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
   PrintFormat("[ACR] AI %1: %2 (armed vehicle) holds perimeter standoff at %3 (%4)", m_sFaction, fire.GetName(), attack.m_Target.m_sName, reason);
  }
  else
  {
   fire.Assault(attack.m_Target.GetOrigin());
   PrintFormat("[ACR] AI %1: %2 lifts fire and joins the assault on %3 (%4)", m_sFaction, fire.GetName(), attack.m_Target.m_sName, reason);
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
    if (!ACR_Transport.CrewedVehicle(member) && member.StrengthRatio() < REFIT_WALK_RATIO) depleted++;
   }
   else if (ACR_Transport.CrewedVehicle(member)) vehiclesNoGunner++;
   else remnants++;
  }
  attacks += string.Format(" | line %1, depleted %2, refitting %3, remnants %4, unarmed vehicles %5", line, depleted, m_Refitting.Count(), remnants, vehiclesNoGunner);
  int visible;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner != m_sFaction && !base.m_sOwner.IsEmpty() && Visible(base)) visible++;
  attacks += string.Format(" | support budget %1", SupportBudget());
  PrintFormat("[ACR] AI %1 status: %2 squads (free %3, defending %4, attacking %5, other %6), attacks:%7, enemy bases known %8, RP %9",
   m_sFaction, all.Count(), nFree, nDefend, nAttack, nOther, attacks, visible, m_Manager.m_Bases.GetRP(m_sFaction));
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
    if (attack.m_bAssaulting && squad.StrengthRatio() < RETREAT_RATIO && squad.KnownEnemies(300) > 0)
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
   if (attack.m_bGrab && target && !target.m_sOwner.IsEmpty() && !won)
   {
    PrintFormat("[ACR] AI %1: %2 was taken first, grab cancelled", m_sFaction, target.m_sName);
    foreach (ACR_Squad late : attack.m_Squads) late.m_iAITask = ACR_EAITask.FREE;
    m_Attacks.RemoveOrdered(i);
    continue;
   }
   int strength;
   foreach (ACR_Squad member : attack.m_Squads) strength += member.Strength();
   bool fusePrimed = attack.m_SapperDemo && attack.m_SapperDemo.InProgress();
   bool broken = !fusePrimed && attack.m_iStartStrength > 0 && strength < attack.m_iStartStrength * ABORT_RATIO;
   bool stalled = !fusePrimed && !won && target && Stalled(attack);
   if (!target || won || (attack.m_Squads.IsEmpty() && !fusePrimed) || broken || stalled)
   {
    string outcome = "failed";
    if (won) outcome = "won";
    if (broken) outcome = "called off (too many losses)";
    if (stalled)
    {
     outcome = "called off (stalled)";
     m_StalledUntil.Set(target, GetGame().GetWorld().GetWorldTime() + STALLED_COOLDOWN * 1000);
    }
    if (target) PrintFormat("[ACR] AI %1: attack on %2 %3", m_sFaction, target.m_sName, outcome);
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
   if (gathered || (!anyAirLift && GetGame().GetWorld().GetWorldTime() > attack.m_fStageDeadline))
   {
    attack.m_bAssaulting = true;
    attack.m_fAssaultStart = GetGame().GetWorld().GetWorldTime();
    // Preparation: shell the objective with heavy assets against fortified FOBs
    bool heavyPrep = target.m_bBuilt || DefenceOf(target) >= 12;
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
    PrintFormat("[ACR] AI %1: assaulting %2 with %3 squad(s) on %4 approach line(s), base of fire: %5", m_sFaction, target.m_sName, attack.m_Squads.Count(), ApproachLines(attack), suppressor);
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
      PrintFormat("[ACR] AI %1: recon team %2 posted to overwatch at %3 (%4 m)", m_sFaction, squad.GetName(), enemyBase.m_sName, (int)vector.DistanceXZ(vSpot, enemyBase.GetOrigin()));
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
   PrintFormat("[ACR] AI %1: %2 posted to %3", m_sFaction, squad.GetName(), nearest.m_sName);
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
   PrintFormat("[ACR] AI %1: motor pool %2 repositioning to %3 (%4 m)", m_sFaction, taxi.GetName(), bestHub.m_sName, (int)bestDist);
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
   bool apart = true;
   foreach (int i, int other : attack.m_aSlot)
   {
    if (other == pick || other < 0 || i >= attack.m_aStaging.Count()) continue;
    if (TurnBetween(attack.m_aStaging[i] - center, candidate - center) < APPROACH_SEPARATION) apart = false;
   }
   if (!apart) continue;
   stage = candidate;
   return pick;
  }
  return -1;
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
   if (!vehicle || squad.IsMounted() || squad.m_iAITask == ACR_EAITask.RESUPPLY) continue;
   if (ACR_Logistics.Health(vehicle) < 0.25 || !ACR_Vehicles.IsEmpty(vehicle)) continue;
   if (vector.DistanceXZ(PositionOf(squad), vehicle.GetOrigin()) > 400 || squad.KnownEnemies(120) > 0) continue;
   float next;
   if (m_NextRecrew.Find(squad, next) && now < next) continue;
   // Not back into a vehicle that just got stuck (the crew bailed, got back in, got stuck again).
   if (now - squad.m_fStuckAt < 600000) continue;
   m_NextRecrew.Set(squad, now + 90000);
   if (squad.GetIn(vehicle.GetOrigin())) PrintFormat("[ACR] AI %1: %2 gets back into its vehicle", m_sFaction, squad.GetName());
  }
 }

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
   PrintFormat("[ACR] AI %1 requisition (mortar team): %2", m_sFaction, m_Manager.PurchaseAt(m_sFaction, i, spawn));
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
   PrintFormat("[ACR] AI %1 requisition (support gunship): %2", m_sFaction, m_Manager.PurchaseAt(m_sFaction, i, base));
   return true;
  }
  return false;
 }

 // The own, safe base closest to the enemy (but not right on top of it).
 protected ACR_Base MortarBase()
 {
  ACR_Base best;
  float bestFront = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || base.m_bContested || ThreatTo(base) > 0) continue;
   float front = float.MAX;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction) front = Math.Min(front, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (front < MORTAR_MIN_STANDOFF || front >= bestFront) continue;
   bestFront = front;
   best = base;
  }
  return best;
 }

 protected void PositionMortars()
 {
  float now = GetGame().GetWorld().GetWorldTime();
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
    PrintFormat("[ACR] AI %1 mortar: %2 shoot-and-scoot displacement", m_sFaction, squad.GetName());
    continue;
   }
   vector here = PositionOf(squad);
   if (team.m_eState == ACR_EMortarState.DEPLOYED) here = team.m_Spot;
   else if (ACR_Mortars.IsMoving(squad, here)) continue; // on its way
   // Stays while it can reach a base that isn't ours (with some margin).
   float reach = ACR_Mortars.MaxRange(team.m_sTube) * 0.85;
   bool inReach = false;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction && vector.DistanceXZ(here, other.GetOrigin()) < reach) inReach = true;
   if (inReach) continue;
   ACR_Base front = MortarBase();
   if (!front || vector.DistanceXZ(here, front.GetOrigin()) < front.m_fRadius + 80) continue;
   vector spot = ACR_Manager.Ground(front.GetOrigin() + Vector(Math.RandomFloat(-20, 20), 0, Math.RandomFloat(-20, 20)));
   squad.MoveTo(spot);
   m_fNextMortarMove = now + MORTAR_MOVE_MS;
   PrintFormat("[ACR] AI %1 mortar: %2 moving to %3", m_sFaction, squad.GetName(), front.m_sName);
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
    if (refitting && now >= giveUpAt)
    {
     m_Refitting.Remove(squad);
     if (squad.m_iAITask == ACR_EAITask.RESUPPLY) squad.m_iAITask = ACR_EAITask.FREE;
     m_ReinforceRetryAt.Set(squad, now + REINFORCE_RETRY_MS);
     PrintFormat("[ACR] AI %1: %2 gave up walking back to reinforce", m_sFaction, squad.GetName());
     continue;
    }
    if (refitting && squad.m_Group && squad.m_Group.GetCurrentWaypoint()) continue;
    // Don't cut short a walk to a service vehicle (ManageLogistics).
    if (!refitting && squad.m_iAITask == ACR_EAITask.RESUPPLY && squad.m_Group && squad.m_Group.GetCurrentWaypoint()) continue;
    bool idle = squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED || squad.m_iAITask == ACR_EAITask.RESUPPLY;
    if (!refit || !(refitting || remnant || (idle && squad.StrengthRatio() < REFIT_WALK_RATIO))) continue;
    if (!refitting) m_Refitting.Set(squad, now + REFIT_GIVE_UP_MS);
    squad.m_iAITask = ACR_EAITask.RESUPPLY;
    squad.SetMode(ACR_EMoveMode.FAST);
    Travel(squad, ACR_Manager.Ground(refit.GetOrigin()), false, 0);
    PrintFormat("[ACR] AI %1: depleted squad %2 (%3% strength) moving to %4 to reinforce", m_sFaction, squad.GetName(), (int)(squad.StrengthRatio() * 100), refit.m_sName);
    continue;
   }
   if (m_Manager.m_Requisition.InContact(squad) != "") continue;
   if (ACR_LogisticsRules.RelayBlocked(ACR_FobSupply.Nearby(m_sFaction, PositionOf(squad)))) continue;
   // Same price ReinforceSquad checks: replacements plus a FOB's instant restock fee.
   int cost = missing.Count() * ACR_Requisition.REINFORCE_COST_PER_SOLDIER;
   ACR_Base depot = m_Manager.m_Requisition.DeployBaseAt(m_sFaction, PositionOf(squad));
   if (depot) cost += m_Manager.m_Requisition.InstantRestockCost(squad, depot);
   if (data.m_iRP - keep < cost) continue;
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
   PrintFormat("[ACR] AI %1 reinforce failed, retry in %2 s: %3", m_sFaction, (int)(REINFORCE_RETRY_MS / 1000), outcome);
   return false;
  }
  if (best.m_iAITask == ACR_EAITask.RESUPPLY) best.m_iAITask = ACR_EAITask.FREE;
  m_Refitting.Remove(best);
  PrintFormat("[ACR] AI %1 reinforce: %2", m_sFaction, outcome);
  return true;
 }

 protected void Requisition()
 {
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
  ReinforceDepleted(data, SupportBudget());
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  // Only fighting units count against the cap (trucks and lone drivers don't).
  int combatLine = 0;
  int supportTeams = 0;
  foreach (ACR_Squad existing : mine)
  {
   if (IsSpecialistSquad(existing)) supportTeams++;
   else if (existing.IsCombatUnit()) combatLine++;
  }
  if (combatLine >= cap) return;
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
    PrintFormat("[ACR] AI %1 requisition (%2 support): %3", m_sFaction, ACR_Logistics.ServiceName(service), m_Manager.PurchaseAt(m_sFaction, pick, spawn));
    return;
   }
  }
  // Squads keep having to walk far with no truck to fetch them: buy a transport first.
  int maxTransports = 4;
  if (ACR_Manager.ThreatOption() == 2) maxTransports = 5;
  if (m_Manager.m_Rides && m_Manager.m_Rides.ShortOfTransport(m_sFaction) && m_Manager.m_Rides.CountTransports(m_sFaction) < maxTransports)
  {
   int truck = -1;
   foreach (int t, ACR_RequisitionEntry candidate : data.m_aRequisitions)
    if (candidate.m_bTransport && !IsServiceVehicle(candidate) && candidate.m_iRemaining > 0 && candidate.m_iCost <= data.m_iRP && (truck < 0 || candidate.m_iCost < data.m_aRequisitions[truck].m_iCost)) truck = t;
   if (truck >= 0)
   {
    PrintFormat("[ACR] AI %1 requisition (transport): %2", m_sFaction, m_Manager.PurchaseAt(m_sFaction, truck, spawn));
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
  else if (!urgent)
  {
   keep = SupportBudget();
   if (BuyMortarTeam(spawn, data)) return;
   if (BuyHelicopter(data)) return;
  }
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
  // The cap counts fighting units only (as above): trucks, ambulances, crewless
  // vehicles and 1-man remnants filled it on Everon and the AI sat on 1000 RP.
  for (int n = 0; n < 2 && combatLine + n < cap; n++)
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
      continue;
     // Weigh by the base price: a unit whose stepped price has climbed isn't worth more.
     float weight = ACR_InfantryCatalog.AIWeight(entry);
     if (entry.m_sVehicle.IsEmpty() && !smallTeam && !IsSpecialistTeam(entry)) weight *= 1.8;
     else if (smallTeam && !(isSapper && HasEnemyFob())) weight *= 0.35;
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
     }
     // Keep infantry the backbone.
     if (!entry.m_sVehicle.IsEmpty()) weight *= 0.5;
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
     PrintFormat("[ACR] AI %1 saving RP for %2 (%3/%4 RP)", m_sFaction, chosen.m_sName, Math.Max(0, data.m_iRP - keep), chosen.m_iCost);
    }
    return;
   }
   int stockBefore = chosen.m_iRemaining;
   string result = m_Manager.PurchaseAt(m_sFaction, pick, spawn);
   if (chosen.m_eFormation == ACR_EFormation.SF && chosen.m_iRemaining < stockBefore) boughtSF = true;
   PrintFormat("[ACR] AI %1 requisition: %2", m_sFaction, result);
   if (!chosen.m_sVehicle.IsEmpty()) vehicles++;
   else if (IsSmallTeamEntry(chosen)) smallTeams++;
   else lineSquads++;
  }
 }

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
    PrintFormat("[ACR] AI %1: %2 resupplied", m_sFaction, squad.GetName());
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
    bool crewFree = crew && !crew.HasPlayer() && !m_ServiceJobs.Contains(crew);
    string what = ACR_Logistics.ServiceName(service);
    if (crewFree && (holding || squad.IsMounted() == false && best > 800))
    {
     // Bring the truck to them.
     crew.SetMode(ACR_EMoveMode.FAST);
     crew.RouteTo(ACR_Manager.Ground(PositionOf(squad)), ACR_Squad.WP_MOVE, -1, "Resupplying " + squad.GetName(), 0);
     m_ServiceJobs.Set(crew, squad);
     PrintFormat("[ACR] AI %1: %2 drives out to %3 (%4)", m_sFaction, crew.GetName(), squad.GetName(), what);
    }
    else if (squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED)
    {
     // Go back to the truck.
     squad.m_iAITask = ACR_EAITask.RESUPPLY;
     squad.SetMode(ACR_EMoveMode.FAST);
     Travel(squad, ACR_Manager.Ground(station.GetOrigin()), false, 0);
     PrintFormat("[ACR] AI %1: %2 falls back to the %3 truck", m_sFaction, squad.GetName(), what);
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
   if (IsAssistingFobUpgrade(idleCrew)) continue;
   ACR_Base goal = supplyPoint;
   if (logistics.StockPercent(serviceVehicle) < 25) goal = HomeBase();
   if (!goal || vector.DistanceXZ(serviceVehicle.GetOrigin(), goal.GetOrigin()) < goal.m_fRadius) continue;
   if (idleCrew.OrderState() == ACR_EOrderState.MOVE && vector.DistanceXZ(idleCrew.RouteDestination(), goal.GetOrigin()) < goal.m_fRadius + 80) continue;
   idleCrew.RouteTo(ACR_Manager.Ground(goal.GetOrigin()), ACR_Squad.WP_MOVE, -1, "Moving", 0);
   PrintFormat("[ACR] AI %1: %2 to %3 (%4)", m_sFaction, idleCrew.GetName(), goal.m_sName, ACR_Logistics.ServiceName(serviceMask));
  }
 }

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
 static const float FOB_FRONT_DISTANCE = 1500;       // 1.5 km front gap requirement
 static const float FOB_EXISTING_PROXIMITY = 1000;   // don't build within 1 km of existing friendly FOB
 static const int FOB_RP_COMBAT_RESERVE = 30;        // RP reserve required when buying construction truck
 static const float FOB_EVAL_INTERVAL_MS = 30000;    // evaluate every 30 seconds

 static const float FOB_TRUCK_RETRY_MS = 300000;    // at most one construction truck purchase per 5 min
 static const float FOB_SITE_AVOID_MS = 600000;     // an abandoned site is skipped for 10 min
 static const float FOB_SITE_AVOID_RADIUS = 150;
 static const float FOB_SITE_RETRY_MS = 300000;     // after a failed site search

 protected ACR_Squad m_FobBuilder;
 protected ACR_Squad m_FobEscort;
 protected vector m_vFobTargetSite;
 protected float m_fNextFobEval;
 protected float m_fNextRoadblockEval;
 protected float m_fNextMinefieldEval;
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

   if (nearestDeploy >= FOB_FRONT_DISTANCE)
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

 bool FindFobSite(ACR_Base front, out vector site)
 {
  site = Vector(0, 0, 0);
  if (!front) return false;
  ACR_Base deployBase = NearestDeployBase(front);
  vector dir;
  if (deployBase)
  {
   dir = deployBase.GetOrigin() - front.GetOrigin();
   dir[1] = 0;
   if (dir.LengthSq() > 0.001) dir.Normalize();
   else dir = Vector(0, 0, -1);
  }
  else
  {
   dir = Vector(0, 0, -1);
  }

  array<float> dists = {450.0, 550.0, 350.0, 650.0, 750.0};
  array<float> angles = {0.0, 25.0, -25.0, 50.0, -50.0, 75.0, -75.0};

  foreach (float d : dists)
  {
   foreach (float a : angles)
   {
    float rad = a * Math.DEG2RAD;
    float cosA = Math.Cos(rad);
    float sinA = Math.Sin(rad);
    vector rotatedDir = Vector(dir[0] * cosA - dir[2] * sinA, 0, dir[0] * sinA + dir[2] * cosA);
    vector candidate = front.GetOrigin() + rotatedDir * d;
    candidate[1] = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);

    if (Avoided(candidate)) continue;
    vector validSpot;
    string refusal;
    if (!ACR_FobBuild.ValidateSite(candidate, validSpot, refusal, "", 20)) continue;

    bool safe = true;
    foreach (ACR_Base b : ACR_Base.s_All)
    {
     if (b.m_sOwner != m_sFaction && !b.m_sOwner.IsEmpty() && !b.m_bRetired)
     {
      if (vector.DistanceXZ(b.GetOrigin(), validSpot) < 600.0)
      {
       safe = false;
       break;
      }
     }
    }
    if (!safe || ACR_FobBuild.IsContested(validSpot, m_sFaction)) continue;

    site = validSpot;
    return true;
   }
  }
  return false;
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
  PrintFormat("[ACR] AI %1 requisition (FOB construction truck): %2", m_sFaction, bought);
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
   if (!FindFobSite(front, site))
   {
    // Site searches are expensive (HQ and eight upgrade spots per candidate): back off.
    m_fNextFobEval = now + FOB_SITE_RETRY_MS;
    PrintFormat("[ACR] AI %1: no clear FOB site near %2, next look in 5 minutes", m_sFaction, front.m_sName);
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
   PrintFormat("[ACR] AI %1: %2 assigned as escort for FOB build at %3", m_sFaction, escort.GetName(), site);
  }

  PrintFormat("[ACR] AI %1: dispatched %2 to establish forward FOB near %3 at %4", m_sFaction, builder.GetName(), front.m_sName, site);
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
  PrintFormat("[ACR] AI %1: FOB site %2 abandoned, avoiding it for 10 minutes", m_sFaction, site);
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
  PrintFormat("[ACR] AI %1: %2 sent to defuse a demolition charge at %3 (%4 m)", m_sFaction, best.GetName(), chargePos, (int)bestDist);
  return true;
 }

 static const float FOB_UPGRADE_EVAL_INTERVAL_MS = 15000;
 protected float m_fNextFobUpgradeEval;

 void SetFobUpgradeEvalTimeForTest(float time) { m_fNextFobUpgradeEval = time; }

 void ManageFobUpgrades()
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

   // 1. Supply replenishment check
   OrderFobSupplies(base, data);

   // 2. Upgrade check & engineer acceleration
   if (!base.m_aFobSlots) ACR_FobSlots.InitSlots(base);
   if (ACR_FobSlots.AnyBuilding(base))
   {
    AssistFobConstruction(base);
   }
   else
   {
    int stock = ACR_FobSupply.Stock(base.m_FobSupply);
    OrderFobUpgrade(base, stock);
   }
  }
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
  PrintFormat("[ACR] AI %1 ordered supplies for %2: %3", m_sFaction, base.m_sName, result);
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
   PrintFormat("[ACR] AI %1: ordered FOB %2 upgrade %3 (tier %4)", m_sFaction, base.m_sName, ACR_FobSlots.SlotName(pickSlot, targetTier), targetTier);
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
    PrintFormat("[ACR] AI %1: dispatched %2 to assist upgrade construction at %3", m_sFaction, squad.GetName(), base.m_sName);
    return squad;
   }
  }
  return null;
 }

 void SetFobEvalTimeForTest(float time) { m_fNextFobEval = time; }
 void StartTickForTest() { m_iTickStarted = System.GetTickCount(); }
 ACR_Squad FobBuilder() { return m_FobBuilder; }
 ACR_Squad FobEscort() { return m_FobEscort; }
 vector FobTargetSite() { return m_vFobTargetSite; }
 void SetFobBuilderForTest(ACR_Squad builder) { m_FobBuilder = builder; }
 ACR_Base GetHomeBase() { return HomeBase(); }

 // Construction temporarily owns an AI squad. A finished checkpoint is just an
 // idle defensive post: normal attacks/defence may recall its entire squad.
 // Also repairs completed ROADBLOCK tasks left by the earlier implementation.
 protected void ReleaseRoadblockBuilders()
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad || squad.m_iAITask != ACR_EAITask.ROADBLOCK) continue;
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (squad.m_Commander != 0 || squad.HasPlayer()) continue;
   if (squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK) continue;
   squad.m_iAITask = ACR_EAITask.POSTED;
   PrintFormat("[ACR] AI %1: %2 available for reassignment after roadblock construction", m_sFaction, squad.GetName());
  }
 }

 // Manage roadblocks / observation posts (aca-f23)
 void ManageRoadblocks()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextRoadblockEval) return;
  m_fNextRoadblockEval = now + 15000;

  // Economy gate: at least 45 RP (20 RP cost + 25 RP reserve)
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data || data.m_iRP < 45) return;

  // Cap: max 2 active roadblocks per faction
  int activeCount = 0;
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (rb && rb.m_sFaction == m_sFaction) activeCount++;
  }
  if (activeCount >= 2) return;

  // Find frontline base to protect
  ACR_Base frontlineBase = null;
  float minEnemyDist = 999999.0;
  vector enemyBasePos = vector.Zero;

  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != m_sFaction || base.m_bContested) continue;
   foreach (ACR_Base other : ACR_Base.s_All)
   {
    if (!other || other.m_bRetired || other.m_sOwner == m_sFaction || other.m_sOwner.IsEmpty()) continue;
    float d = vector.DistanceXZ(base.GetOrigin(), other.GetOrigin());
    if (d < minEnemyDist)
    {
     minEnemyDist = d;
     frontlineBase = base;
     enemyBasePos = other.GetOrigin();
    }
   }
  }

  if (!frontlineBase || enemyBasePos == vector.Zero) return;

  // Check if this base already has a roadblock within 400 m
  foreach (ACR_Roadblock rb2 : ACR_Roadblocks.s_aRoadblocks)
  {
   if (rb2 && rb2.m_sFaction == m_sFaction && vector.DistanceXZ(rb2.m_vPos, frontlineBase.GetOrigin()) < 400.0)
    return;
  }

  // Find road corridor between frontlineBase and enemyBasePos (250m to 380m out)
  ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
  if (!aiWorld || !aiWorld.GetRoadNetworkManager()) return;

  vector basePos = frontlineBase.GetOrigin();
  vector toEnemy = enemyBasePos - basePos;
  toEnemy[1] = 0;
  toEnemy.Normalize();

  vector targetRoadPos = vector.Zero;
  float targetYaw = 0;
  bool foundRoad = false;

  for (float testDist = 250.0; testDist <= 380.0; testDist += 30.0)
  {
   vector samplePos = basePos + toEnemy * testDist;
   samplePos[1] = GetGame().GetWorld().GetSurfaceY(samplePos[0], samplePos[2]);

   vector validPos;
   float validYaw;
   string refusal;
   if (ACR_Roadblocks.ValidateSite(samplePos, validPos, validYaw, refusal, m_sFaction, enemyBasePos))
   {
    targetRoadPos = validPos;
    targetYaw = validYaw;
    foundRoad = true;
    break;
   }
  }

  if (!foundRoad) return;

  // Find an idle or posted squad near this base
  ACR_Squad candidate = null;
  array<ACR_Squad> squads = {};
  MySquads(squads, true);
  foreach (ACR_Squad squad : squads)
  {
   if (!squad || squad.HasPlayer() || squad.CountAlive() <= 0 || squad.IsVehicle() || IsSupportTeam(squad)) continue;
   if (squad == m_ReserveSquad || squad.m_Commander != 0) continue;
   if (squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
   if (squad.m_iAITask == ACR_EAITask.POSTED || squad.m_iAITask == ACR_EAITask.FREE)
   {
    float dist = vector.DistanceXZ(PositionOf(squad), basePos);
    if (dist < 450.0)
    {
     candidate = squad;
     break;
    }
   }
  }

  if (!candidate) return;

  if (candidate.EstablishRoadblock(targetRoadPos, targetYaw))
  {
   candidate.m_iAITask = ACR_EAITask.ROADBLOCK;
   PrintFormat("[ACR] AI %1: dispatched %2 to establish roadblock on approach to %3 at %4", m_sFaction, candidate.GetName(), frontlineBase.m_sName, targetRoadPos);
  }
 }

 // Minefields on the approaches (-acrMinesAI 1; off by default). Every 30 s, with RP to spare
 // and fewer than two fields down, a free infantry squad near the frontline base (sappers
 // first) lays one 150-270 m out towards the nearest enemy base: mixed on a road, AP mines on
 // open ground.
 void ManageMinefields()
 {
  foreach (ACR_Squad layer : m_Manager.m_Squads)
  {
   if (!layer || layer.m_iAITask != ACR_EAITask.MINES || layer.m_MinePlan) continue;
   if (!layer.GetFaction() || layer.GetFaction().GetFactionKey() != m_sFaction) continue;
   layer.m_iAITask = ACR_EAITask.POSTED;
   PrintFormat("[ACR] AI %1: %2 available for reassignment after laying mines", m_sFaction, layer.GetName());
  }
  // Planning a field queries the roads and terrain: it waits for a tick with budget left.
  if (!ACR_Minefields.AIUses() || PlanBudgetSpent()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextMinefieldEval) return;
  m_fNextMinefieldEval = now + 30000;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data || data.m_iRP < ACR_Minefields.Cost(ACR_EMineKind.MIXED) + 30) return;
  if (ACR_Minefields.CountFields(m_sFaction) >= 2) return;

  ACR_Base front;
  vector enemyPos;
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
  if (!front) return;

  ACR_Squad candidate;
  array<ACR_Squad> squads = {};
  MySquads(squads, true);
  foreach (ACR_Squad squad : squads)
  {
   if (!squad || squad.HasPlayer() || squad.m_Commander != 0 || squad == m_ReserveSquad || IsSupportTeam(squad) || !ACR_Minefields.CanLay(squad)) continue;
   if (squad.m_MinePlan) return; // one field at a time
   if (squad.m_iAITask != ACR_EAITask.POSTED && squad.m_iAITask != ACR_EAITask.FREE) continue;
   if (squad.OrderState() == ACR_EOrderState.BUILD_ROADBLOCK || squad.OrderState() == ACR_EOrderState.BUILD_FOB) continue;
   if (vector.DistanceXZ(PositionOf(squad), front.GetOrigin()) > 600) continue;
   if (!candidate || (ACR_SapperDemo.IsSapperSquad(squad) && !ACR_SapperDemo.IsSapperSquad(candidate))) candidate = squad;
  }
  if (!candidate) return;

  vector toEnemy = enemyPos - front.GetOrigin();
  toEnemy[1] = 0;
  toEnemy.Normalize();
  for (float reach = 150; reach <= 270; reach += 40)
  {
   vector sample = ACR_Manager.Ground(front.GetOrigin() + toEnemy * reach);
   vector roadPoint, roadDir;
   float roadWidth;
   ACR_EMineKind kind = ACR_EMineKind.AP;
   if (ACR_Minefields.Snap(sample, ACR_Minefields.ROAD_SNAP, roadPoint, roadDir, roadWidth)) kind = ACR_EMineKind.MIXED;
   string refusal;
   if (!ACR_Minefields.Order(candidate, kind, sample, refusal)) continue;
   candidate.m_iAITask = ACR_EAITask.MINES;
   PrintFormat("[ACR] AI %1: %2 laying %3 on the approach to %4 at %5", m_sFaction, candidate.GetName(), ACR_Minefields.Name(kind), front.m_sName, sample);
   return;
  }
 }

 // ---------------------------------------------------------------- hand-over

 // A player took command: leave every squad as it is, but forget AI tasks.
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
