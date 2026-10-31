// AI skill per side and unit (ACR_AISkill, 31 Oct 2026), run from srv:regress: Setup at tick 5
// on Bravo, Check at tick 8 after the squad's tactics ticks applied it; the side's values are
// restored afterwards.
class ACR_TestAISkill
{
 static const int CHECKS = 5;

 protected static ACR_FactionSettings s_Settings;
 protected static int s_iSkill;
 protected static float s_fReaction;
 protected static int s_iShare;
 protected static ref array<ref ACR_AISkillProfile> s_aProfiles;
 protected static IEntity s_Profiled;
 protected static IEntity s_Pinned;
 protected static ResourceName s_sProfiledPrefab;

 static void Setup(ACR_Squad bravo)
 {
  ACR_TestRegress.Check("ai skill: only named levels (60 aims as Regular, not perfectly)", ACR_AISkill.Level(60) == EAISkill.REGULAR && ACR_AISkill.Level(5) == EAISkill.NOOB && ACR_AISkill.Level(100) == EAISkill.CYLON && ACR_AISkill.StepUp(EAISkill.VETERAN) == EAISkill.EXPERT && ACR_AISkill.StepUp(EAISkill.CYLON) == EAISkill.CYLON, "Level 60/5/100, StepUp 70/100");
  array<IEntity> soldiers = Soldiers(bravo);
  if (!bravo || !bravo.GetFaction() || soldiers.Count() < 2) return;
  s_Settings = ACR_FactionSettings.Find(bravo.GetFaction().GetFactionKey());
  if (!s_Settings) return;
  s_iSkill = s_Settings.m_iAISkill;
  s_fReaction = s_Settings.m_fAIReactionScale;
  s_iShare = s_Settings.m_iAIStrongerShare;
  s_aProfiles = s_Settings.m_aAISkillProfiles;
  // The side: Expert, 25% faster. A profile for the first soldier's prefab: Rookie, 25% slower,
  // all of them one level up (Regular). The last soldier is pinned perfect and twice as fast.
  s_Settings.m_iAISkill = EAISkill.EXPERT;
  s_Settings.m_fAIReactionScale = 1.25;
  s_Settings.m_iAIStrongerShare = 0;
  ACR_AISkillProfile profile = new ACR_AISkillProfile();
  s_Profiled = soldiers[0];
  s_sProfiledPrefab = s_Profiled.GetPrefabData().GetPrefabName();
  profile.m_aPrefabs = {s_sProfiledPrefab};
  profile.m_iSkill = EAISkill.ROOKIE;
  profile.m_fReactionScale = 0.75;
  profile.m_iStrongerShare = 100;
  s_Settings.m_aAISkillProfiles = {profile};
  foreach (IEntity soldier : soldiers) ACR_AISkill.Unpin(soldier);
  s_Pinned = soldiers[soldiers.Count() - 1];
  ACR_AISkill.Pin(s_Pinned, EAISkill.CYLON, 2);
 }

 static void Check(ACR_Squad bravo)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!s_Settings || !manager)
  {
   ACR_TestRegress.Skip("ai skill: a unit profile beats the side", "Bravo or its faction settings missing");
   ACR_TestRegress.Skip("ai skill: the side's values reach its other soldiers", "Bravo or its faction settings missing");
   ACR_TestRegress.Skip("ai skill: a pinned soldier keeps the mission's values", "Bravo or its faction settings missing");
   ACR_TestRegress.Skip("ai skill: unpinned, a soldier returns to his side's values", "Bravo or its faction settings missing");
   return;
  }
  float base = manager.m_fAIPerception;
  SCR_AICombatComponent profiled = Combat(s_Profiled);
  bool profileOK = profiled && profiled.GetAISkill() == EAISkill.REGULAR && Math.AbsFloat(profiled.GetPerceptionFactor() - base * 0.75) < 0.001;
  ACR_TestRegress.Check("ai skill: a unit profile beats the side", profileOK, Describe(profiled));

  SCR_AICombatComponent other = null;
  foreach (IEntity soldier : Soldiers(bravo))
   if (soldier != s_Pinned && soldier.GetPrefabData() && soldier.GetPrefabData().GetPrefabName() != s_sProfiledPrefab) other = Combat(soldier);
  if (other)
  {
   bool sideOK = other.GetAISkill() == EAISkill.EXPERT && Math.AbsFloat(other.GetPerceptionFactor() - base * 1.25) < 0.001;
   ACR_TestRegress.Check("ai skill: the side's values reach its other soldiers", sideOK, Describe(other));
  }
  else ACR_TestRegress.Skip("ai skill: the side's values reach its other soldiers", "every Bravo soldier shares the profiled prefab");

  SCR_AICombatComponent pinned = Combat(s_Pinned);
  bool pinOK = pinned && ACR_AISkill.IsPinned(s_Pinned) && pinned.GetAISkill() == EAISkill.CYLON && Math.AbsFloat(pinned.GetPerceptionFactor() - base * 2) < 0.001;
  ACR_TestRegress.Check("ai skill: a pinned soldier keeps the mission's values", pinOK, Describe(pinned));

  // Restore the side, then unpin: the next tactics tick works him out again.
  s_Settings.m_iAISkill = s_iSkill;
  s_Settings.m_fAIReactionScale = s_fReaction;
  s_Settings.m_iAIStrongerShare = s_iShare;
  s_Settings.m_aAISkillProfiles = s_aProfiles;
  foreach (IEntity soldier : Soldiers(bravo)) ACR_AISkill.Unpin(soldier);
  ACR_AISkill.Apply(pinned, bravo.m_Group, manager.m_iAISkill, base);
  bool backOK = pinned && !ACR_AISkill.IsPinned(s_Pinned) && pinned.GetAISkill() == ACR_AISkill.Level(manager.m_iAISkill) && Math.AbsFloat(pinned.GetPerceptionFactor() - base) < 0.001;
  ACR_TestRegress.Check("ai skill: unpinned, a soldier returns to his side's values", backOK, Describe(pinned));
 }

 protected static array<IEntity> Soldiers(ACR_Squad squad)
 {
  array<IEntity> soldiers = {};
  if (!squad) return soldiers;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
   if (agent.GetControlledEntity() && Combat(agent.GetControlledEntity())) soldiers.Insert(agent.GetControlledEntity());
  return soldiers;
 }

 protected static SCR_AICombatComponent Combat(IEntity soldier)
 {
  if (!soldier) return null;
  return SCR_AICombatComponent.Cast(soldier.FindComponent(SCR_AICombatComponent));
 }

 protected static string Describe(SCR_AICombatComponent combat)
 {
  if (!combat) return "no combat component";
  return string.Format("skill %1, perception %2", combat.GetAISkill(), combat.GetPerceptionFactor());
 }
}
