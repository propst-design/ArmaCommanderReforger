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
 protected float m_fApproachAt = -100000;      // last walk-up order to the HQ

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

 // A plant nobody can finish. While planting or wiring the attack counts as primed and none of
 // its ways of ending apply (UpdateAttacks), so it held its attack slot, and the defenders their
 // DEFEND squads, for the rest of the match.
 protected void DropPlanting(string why)
 {
  if (m_State != ACR_ESapperDemoState.PLANTING && m_State != ACR_ESapperDemoState.WIRING) return;
  PrintFormat("[ACR] SAPPER: planting at %1 dropped (%2)", m_Attack.m_Target.m_sName, why);
  m_State = ACR_ESapperDemoState.NONE;
  m_iStep = 0;
  m_Planter = null;
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
  if (!hq) DropPlanting("the HQ is gone");
  if (!hq || now < m_fRetryAt) return;
  vector hqPos = hq.GetOrigin();
  string owner = m_Attack.m_Target.m_sOwner;

  if (!m_SapperSquad || m_SapperSquad.CountAlive() <= 0)
  {
   m_SapperSquad = FindSapperSquad();
   if (!m_SapperSquad)
   {
    DropPlanting("no sappers left");
    return;
   }
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
    // At most every 20 s: re-issued every second it reset the squad's waypoint recovery, so an HQ
    // it can't reach never gave up.
    if (vector.DistanceXZ(PositionOf(m_SapperSquad), hqPos) <= CLEAR_RADIUS && now - m_fApproachAt > 20000)
    {
     m_fApproachAt = now;
     m_SapperSquad.MoveTo(hqPos);
    }
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
