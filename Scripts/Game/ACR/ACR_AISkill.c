// AI skill and reaction per side and per unit (31 Oct 2026, a Black Hawk Down mission: militia,
// Rangers and Delta on different skills). ACR_Squad.Tactics sets every soldier of a squad nobody
// leads each second, which erased anything a mission set at spawn. Each soldier's values now come
// from the first of:
//  - ACR_AISkill.Pin from a mission script (kept until Unpin);
//  - the first profile in his side's ACR_FactionSettings whose prefab his body is (or inherits
//    from), then the first whose prefab his group is;
//  - his side's own skill and reaction scale;
//  - the mode's skill and reaction (m_iAISkill/m_fAIPerception, -acrSkill/-acrReaction).
// A side or profile can also lift a share of its soldiers one skill level (a few hard men among
// militia), rolled once per soldier. Vanilla reads skill only as its named levels: any other value
// aims perfectly (SCR_AIGetAimErrorOffset.GetRandomFactor leaves sigma 0), so every result snaps
// to one of them, -acrSkill 60 included.
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sName")]
class ACR_AISkillProfile
{
 [Attribute("", UIWidgets.EditBox, "Name for your own reference (e.g. Delta)")]
 string m_sName;
 [Attribute(desc: "Group or soldier prefabs this applies to; prefabs inheriting from them count too", params: "et")]
 ref array<ResourceName> m_aPrefabs;
 [Attribute("0", UIWidgets.ComboBox, "Skill (NONE = the side's)", "", ParamEnumArray.FromEnum(EAISkill))]
 int m_iSkill;
 [Attribute("0", UIWidgets.EditBox, "Reaction scale: multiplies the mode's reaction speed, e.g. 1.2 spots 20% faster (0 = the side's)")]
 float m_fReactionScale;
 [Attribute("-1", UIWidgets.EditBox, "Percent of these soldiers one skill level higher, 0-100 (-1 = the side's)")]
 int m_iStrongerShare;

 bool Matches(IEntity entity)
 {
  if (!entity || !m_aPrefabs || !entity.GetPrefabData()) return false;
  ResourceName prefab = entity.GetPrefabData().GetPrefabName();
  foreach (ResourceName wanted : m_aPrefabs)
   if (SCR_BaseContainerTools.IsKindOf(prefab, wanted)) return true;
  return false;
 }
}

// Per-soldier state, worked out once (ACR_AISkill.Resolve) and kept on the soldier.
modded class SCR_AICombatComponent
{
 bool m_bACRSkillResolved;
 bool m_bACRSkillPinned;
 int m_iACRSkill;            // 0 = the mode's
 float m_fACRReactionScale = 1;
 bool m_bACRStronger;
}

class ACR_AISkill
{
 // A mission script fixes one soldier's skill and reaction scale (of the mode's reaction); ACA
 // keeps them until Unpin, for soldiers in its squads and garrisons alike.
 static bool Pin(IEntity soldier, EAISkill skill, float reactionScale = 1)
 {
  SCR_AICombatComponent combat = Combat(soldier);
  if (!combat) return false;
  combat.m_bACRSkillResolved = true;
  combat.m_bACRSkillPinned = true;
  combat.m_iACRSkill = skill;
  combat.m_fACRReactionScale = reactionScale;
  if (reactionScale <= 0) combat.m_fACRReactionScale = 1;
  combat.m_bACRStronger = false;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) Apply(combat, null, manager.m_iAISkill, manager.m_fAIPerception);
  return true;
 }

 // Back to the side's and profiles' values on the next tactics tick.
 static void Unpin(IEntity soldier)
 {
  SCR_AICombatComponent combat = Combat(soldier);
  if (!combat) return;
  combat.m_bACRSkillPinned = false;
  combat.m_bACRSkillResolved = false;
 }

 static bool IsPinned(IEntity soldier)
 {
  SCR_AICombatComponent combat = Combat(soldier);
  return combat && combat.m_bACRSkillPinned;
 }

 // Every second from ACR_Squad.Tactics, with the mode's skill and reaction.
 static void Apply(SCR_AICombatComponent combat, SCR_AIGroup group, int skill, float reaction)
 {
  if (!combat) return;
  if (!combat.m_bACRSkillResolved) Resolve(combat, group);
  if (combat.m_iACRSkill > 0) skill = combat.m_iACRSkill;
  skill = Level(skill);
  if (combat.m_bACRStronger) skill = StepUp(skill);
  reaction *= combat.m_fACRReactionScale;
  if (combat.GetAISkill() != skill) combat.SetAISkill(skill);
  if (combat.GetPerceptionFactor() != reaction) combat.SetPerceptionFactor(reaction);
 }

 protected static void Resolve(SCR_AICombatComponent combat, SCR_AIGroup group)
 {
  combat.m_bACRSkillResolved = true;
  combat.m_iACRSkill = 0;
  combat.m_fACRReactionScale = 1;
  combat.m_bACRStronger = false;
  ACR_FactionSettings settings = null;
  if (group && group.GetFaction()) settings = ACR_FactionSettings.Find(group.GetFaction().GetFactionKey());
  if (!settings) return;
  int share = settings.m_iAIStrongerShare;
  combat.m_iACRSkill = settings.m_iAISkill;
  if (settings.m_fAIReactionScale > 0) combat.m_fACRReactionScale = settings.m_fAIReactionScale;
  ACR_AISkillProfile profile = Profile(settings, combat.GetOwner(), group);
  if (profile)
  {
   if (profile.m_iSkill > 0) combat.m_iACRSkill = profile.m_iSkill;
   if (profile.m_fReactionScale > 0) combat.m_fACRReactionScale = profile.m_fReactionScale;
   if (profile.m_iStrongerShare >= 0) share = profile.m_iStrongerShare;
  }
  combat.m_bACRStronger = share > 0 && Math.RandomInt(0, 100) < share;
 }

 // A soldier's own prefab beats his group's: a hard-man prefab inside a militia group.
 static ACR_AISkillProfile Profile(ACR_FactionSettings settings, IEntity body, SCR_AIGroup group)
 {
  if (!settings || !settings.m_aAISkillProfiles) return null;
  foreach (ACR_AISkillProfile profile : settings.m_aAISkillProfiles)
   if (profile && profile.Matches(body)) return profile;
  foreach (ACR_AISkillProfile profile : settings.m_aAISkillProfiles)
   if (profile && profile.Matches(group)) return profile;
  return null;
 }

 // The highest named level at or below skill (at least Novice).
 static int Level(int skill)
 {
  if (skill >= EAISkill.CYLON) return EAISkill.CYLON;
  if (skill >= EAISkill.EXPERT) return EAISkill.EXPERT;
  if (skill >= EAISkill.VETERAN) return EAISkill.VETERAN;
  if (skill >= EAISkill.REGULAR) return EAISkill.REGULAR;
  if (skill >= EAISkill.ROOKIE) return EAISkill.ROOKIE;
  return EAISkill.NOOB;
 }

 static int StepUp(int skill)
 {
  if (skill < EAISkill.ROOKIE) return EAISkill.ROOKIE;
  if (skill < EAISkill.REGULAR) return EAISkill.REGULAR;
  if (skill < EAISkill.VETERAN) return EAISkill.VETERAN;
  if (skill < EAISkill.EXPERT) return EAISkill.EXPERT;
  return EAISkill.CYLON;
 }

 protected static SCR_AICombatComponent Combat(IEntity soldier)
 {
  if (!soldier) return null;
  return SCR_AICombatComponent.Cast(soldier.FindComponent(SCR_AICombatComponent));
 }
}
