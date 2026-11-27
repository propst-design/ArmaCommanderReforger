// Shared Enfusion entity/component lookup helpers so callers do not repeat
// boilerplate FindComponent + Cast + null-check chains across Scripts/Game/ACR.
class ACR_EntityUtil
{
 // Affiliated faction key for an entity (character, vehicle, etc.), or "" if none.
 static string FactionKeyOf(IEntity entity)
 {
  if (!entity) return "";
  FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
  if (!aff) return "";
  Faction faction = aff.GetAffiliatedFaction();
  if (!faction) return "";
  return faction.GetFactionKey();
 }

 // Sets an entity's affiliated faction by key. Returns true if the component existed.
 static bool SetFactionByKey(IEntity entity, string factionKey)
 {
  if (!entity) return false;
  FactionAffiliationComponent aff = FactionAffiliationComponent.Cast(entity.FindComponent(FactionAffiliationComponent));
  if (!aff) return false;
  aff.SetAffiliatedFactionByKey(factionKey);
  return true;
 }

 static AIControlComponent AIControl(IEntity entity)
 {
  if (!entity) return null;
  return AIControlComponent.Cast(entity.FindComponent(AIControlComponent));
 }

 static AIAgent ControlAIAgent(IEntity entity)
 {
  AIControlComponent control = AIControl(entity);
  if (!control) return null;
  return control.GetControlAIAgent();
 }

 static SCR_AIUtilityComponent AgentUtility(AIAgent agent)
 {
  if (!agent) return null;
  return SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
 }

 static SCR_AIUtilityComponent AIUtility(IEntity entity)
 {
  return AgentUtility(ControlAIAgent(entity));
 }

 static DamageManagerComponent DamageManager(IEntity entity)
 {
  if (!entity) return null;
  return DamageManagerComponent.Cast(entity.FindComponent(DamageManagerComponent));
 }

 static SCR_DamageManagerComponent SCRDamageManager(IEntity entity)
 {
  if (!entity) return null;
  return SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
 }
}
