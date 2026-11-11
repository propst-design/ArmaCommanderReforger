// -acrSkillBBBTest 1 (server-capable, under a minute): ACA's every-second skill and spotting update lets
// Better Battle Buddies' Settings-tab values through (ACR_BBBHooks.ModeSkill). With a BBB preset setting
// skill Expert and spotting 2: an ACA soldier gets them instead of the mode's (-acrSkill, -acrReaction);
// a pinned soldier keeps his pin; with the preset gone, the mode's values come back. VERDICT: 5 checks.
class ACR_TestSkillBBB
{
 static const int CHECKS = 5;
 static ref BBB_Preset s_Preset;

 protected static SCR_AICombatComponent Combat(ACR_TestHarness h, int index)
 {
  ACR_Squad squad = h.Squad();
  if (!squad) return null;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  if (index >= agents.Count() || !agents[index].GetControlledEntity()) return null;
  return SCR_AICombatComponent.Cast(agents[index].GetControlledEntity().FindComponent(SCR_AICombatComponent));
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (tick == 6)
  {
   SCR_AICombatComponent combat = Combat(h, 0);
   ACR_TestRegress.Check("skill bbb: a soldier to test", combat != null);
   if (!combat) { ACR_TestRegress.Complete("skillbbb", CHECKS); return; }
   ACR_AISkill.Apply(combat, h.Squad().m_Group, manager.m_iAISkill, manager.m_fAIPerception);
   int modeSkill = combat.GetAISkill();
   float modeReaction = combat.GetPerceptionFactor();
   PrintFormat("[ACR] SKILLBBB mode: skill %1, spotting %2", modeSkill, modeReaction);
   // A BBB preset with skill and spotting, as the Settings tab saves one.
   s_Preset = new BBB_Preset();
   s_Preset.m_sName = "ACA skill test";
   s_Preset.SetValue(BBB_Settings.Find("bbbAISkill"), "80");
   s_Preset.SetValue(BBB_Settings.Find("bbbAIPerception"), "2");
   s_Preset.Save();
   BBB_Settings.s_bTestPick = true;
   BBB_Settings.s_sTestPreset = s_Preset.m_sName;
   BBB_Settings.Invalidate();
   ACR_AISkill.Apply(combat, h.Squad().m_Group, manager.m_iAISkill, manager.m_fAIPerception);
   PrintFormat("[ACR] SKILLBBB with BBB's preset: skill %1, spotting %2", combat.GetAISkill(), combat.GetPerceptionFactor());
   ACR_TestRegress.Check("skill bbb: BBB's skill stands in for the mode's", combat.GetAISkill() == EAISkill.EXPERT);
   ACR_TestRegress.Check("skill bbb: BBB's spotting stands in for the mode's", Math.AbsFloat(combat.GetPerceptionFactor() - 2) < 0.01);
   // A pinned soldier keeps his pin.
   SCR_AICombatComponent pinned = Combat(h, 1);
   if (!pinned) pinned = combat;
   ACR_AISkill.Pin(pinned.GetOwner(), EAISkill.ROOKIE, 1);
   ACR_AISkill.Apply(pinned, h.Squad().m_Group, manager.m_iAISkill, manager.m_fAIPerception);
   ACR_TestRegress.Check("skill bbb: a mission's pin still comes first", pinned.GetAISkill() == EAISkill.ROOKIE && Math.AbsFloat(pinned.GetPerceptionFactor() - manager.m_fAIPerception) < 0.01);
   ACR_AISkill.Unpin(pinned.GetOwner());
   // The preset gone: the mode's values again.
   BBB_Settings.s_sTestPreset = "";
   BBB_Settings.Invalidate();
   ACR_AISkill.Apply(combat, h.Squad().m_Group, manager.m_iAISkill, manager.m_fAIPerception);
   ACR_TestRegress.Check("skill bbb: without BBB's values, the mode's", combat.GetAISkill() == modeSkill && Math.AbsFloat(combat.GetPerceptionFactor() - modeReaction) < 0.01);
   s_Preset.Delete();
   BBB_Settings.s_bTestPick = false;
   BBB_Settings.Invalidate();
   ACR_TestRegress.Complete("skillbbb", CHECKS);
  }
 }
}
