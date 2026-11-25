// FOB HQ demolition (Jacob, 18 Oct). Vanilla HQ tents have their destruction switched off, so:
//  - any vanilla demolition charge (M112, TSh-400) that goes off within DESTROY_RADIUS of a FOB HQ
//    destroys that HQ: the FOB goes offline (ACR_Base.MarkHqLost), its upgrades stay;
//  - an enemy may dismantle a FOB HQ with a shovel only while the FOB is contested and no
//    defender is within 30 m (our mode has no Conflict base, so vanilla's capture rule never runs).
// AI sappers place and fire the same vanilla charges (ACR_SapperDemo), so both go through here.
class ACR_Demolition
{
 static const float DESTROY_RADIUS = 11.0;  // HQ centre to charge (its footprint reaches ~8 m)
 static const float DEFENDER_RADIUS = 30.0;

 static const ResourceName CHARGE_WEST = "{33CBDE73AB48172A}Prefabs/Weapons/Explosives/DemoBlock_M112/DemoBlock_M112.et";
 static const ResourceName CHARGE_EAST = "{97064F8597F2D7BF}Prefabs/Weapons/Explosives/DemoBlock_TSh400g/DemoBlock_TSh400g.et";
 static const ResourceName DETONATOR_WEST = "{CE0AF733722B3978}Prefabs/Items/Equipment/Detonators/BlastingMachine_M34/BlastingMachine_M34.et";
 static const ResourceName DETONATOR_EAST = "{90976DC90A223095}Prefabs/Items/Equipment/Detonators/BlastingMachine_KPM_3U1/BlastingMachine_KPM_3U1.et";

 static ResourceName ChargeFor(string faction)
 {
  if (!faction.IsEmpty() && !ACR_Supports.IsWestern(faction)) return CHARGE_EAST;
  return CHARGE_WEST;
 }

 static ResourceName DetonatorFor(string faction)
 {
  if (!faction.IsEmpty() && !ACR_Supports.IsWestern(faction)) return DETONATOR_EAST;
  return DETONATOR_WEST;
 }

 // The built FOB whose HQ this composition root is (null otherwise).
 static ACR_Base FobOfHq(IEntity root)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!root || !manager || !manager.m_Structures) return null;
  ACR_Structure s = manager.m_Structures.FindStructure(root);
  if (!s || s.m_eKind != ACR_EStructure.HQ || !s.m_Fob || s.m_Fob.m_bRetired) return null;
  return s.m_Fob;
 }

 // Server, from every charge that goes off (timed fuze, detonator or script).
 static int OnChargeDetonated(vector position)
 {
  if (!Replication.IsServer()) return 0;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Structures) return 0;
  array<IEntity> hqs = {};
  manager.m_Structures.HqsNear(position, DESTROY_RADIUS, hqs);
  int destroyed;
  foreach (IEntity hq : hqs)
  {
   ACR_Base fob = FobOfHq(hq);
   string name = "an HQ";
   if (fob) name = fob.m_sName + " HQ";
   PrintFormat("[ACR] DEMOLITION charge at %1 destroyed %2 (%3 m away)", position, name, (int)vector.Distance(position, hq.GetOrigin()));
   if (fob)
   {
    manager.NotifyFaction(fob.m_sOwner, string.Format("%1 destroyed by demolition: FOB offline until rebuilt", name));
    ACR_RadioAudio.NotifyFaction(fob.m_sOwner, ACR_ERadioCue.BASE_LOST);
   }
   vector centre = hq.GetOrigin();
   ACR_Navmesh.DeleteStructure(hq);
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   manager.m_Supports.CookOff(centre); // the tent goes up (Jacob, 18 Oct)
   destroyed++;
  }
  if (destroyed > 0) manager.m_Structures.ACR_ForceTick();
  // COIN hand-fired IED (#97 / #244 ruling 5): a blast near army troops marks the road
  // dangerous and gives the army a lead on the supplying hideout, like an AI IED team.
  if (ACR_Coin.On() && manager.m_Bases)
  {
   string army = ACR_Coin.Army();
   if (!army.IsEmpty() && manager.m_Bases.MenWithin(position, 40, army) > 0)
   {
    ACR_Insurgency.OnLoss(army, position, "an IED blast");
    ACR_DefenderPost supplier = ACR_FIAIntel.SupplierOf(position);
    if (supplier) ACR_FIAIntel.Lead(army, supplier, "an IED blast");
   }
  }
  return destroyed;
 }

 // May this character dismantle this composition (server rules; owners always may)?
 static bool CanDismantle(IEntity root, IEntity user, out string reason)
 {
  reason = "";
  ACR_Base fob = FobOfHq(root);
  if (!fob || !user) return true;
  string faction = "";
  FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(user.FindComponent(FactionAffiliationComponent));
  if (aff && aff.GetAffiliatedFaction()) faction = aff.GetAffiliatedFaction().GetFactionKey();
  if (faction == fob.m_sOwner) return true;
  if (!fob.m_bContested)
  {
   reason = string.Format("Contest %1 before dismantling its HQ", fob.m_sName);
   return false;
  }
  int defenders = ACR_SapperDemo.SoldiersNear(root.GetOrigin(), fob.m_sOwner, DEFENDER_RADIUS);
  if (defenders > 0)
  {
   reason = string.Format("Clear the defenders within %1 m of the HQ first (%2 left)", (int)DEFENDER_RADIUS, defenders);
   return false;
  }
  return true;
 }
}

// Every vanilla charge detonation passes through UseTrigger on the authority.
modded class SCR_ExplosiveTriggerComponent
{
 override void UseTrigger()
 {
  vector position = GetOwner().GetOrigin();
  bool authority = Replication.IsServer();
  super.UseTrigger();
  if (authority) ACR_Demolition.OnChargeDetonated(position);
 }
}

// Enemy shovel dismantling of a FOB HQ: only while contested and clear of defenders.
modded class SCR_CampaignBuildingDisassemblyUserAction
{
 override bool CanBePerformedScript(IEntity user)
 {
  if (!super.CanBePerformedScript(user)) return false;
  if (!Replication.IsServer()) return true; // the server decides (PerformAction)
  string why;
  if (!ACR_Demolition.CanDismantle(GetOwner().GetRootParent(), user, why))
  {
   SetCannotPerformReason(why);
   return false;
  }
  return true;
 }

 override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
 {
  if (Replication.IsServer() && pOwnerEntity)
  {
   string why;
   if (!ACR_Demolition.CanDismantle(pOwnerEntity.GetRootParent(), pUserEntity, why))
   {
    PrintFormat("[ACR] DEMOLITION dismantle refused: %1", why);
    int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
    SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
    if (pc) pc.ACR_Feedback(why);
    return;
   }
  }
  super.PerformAction(pOwnerEntity, pUserEntity);
 }
}
