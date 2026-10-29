// -acrQrfTest 1 (server-capable, ~2 min): the QRF standing order. Alpha stands by at home (its
// side only sees the ring); an enemy fire team its side knows of 450 m out (a spotter's report,
// fed to the side's intel every second) doesn't move it; one 150 m out sends it hunting; once
// that team is dead it walks back to its post as a QRF still; a new order ends it.
class ACR_TestQrf
{
 static const int CHECKS = 5;
 static int s_iStep;
 static int s_iStepAt;
 static vector s_vPost;
 static string s_sEnemy;
 static SCR_AIGroup s_Enemy;
 static ref ACR_QrfPlan s_Plan;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Variety.s_bOff = true; // the target pick: the best-scored contact
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor() || !alpha.GetFaction())
  {
   if (tick == 60) ACR_TestRegress.Abort("qrf", "no Alpha squad");
   return;
  }
  if (tick > 250) ACR_TestRegress.Abort("qrf", string.Format("timed out in step %1", s_iStep));
  if (tick < 5) return;
  string us = alpha.GetFaction().GetFactionKey();
  int waited = tick - s_iStepAt;
  // The enemy holds its fire, and a spotter keeps the side's intel on it.
  if (s_Enemy) Spot(manager, us, s_Enemy);

  if (s_iStep == 0)
  {
   s_sEnemy = "USSR";
   if (us == "USSR") s_sEnemy = "US";
   s_vPost = alpha.GetAnchor().GetOrigin();
   bool ordered = alpha.Qrf(s_vPost);
   s_Plan = alpha.m_Qrf;
   bool ours = HasRing(us);
   bool theirs = HasRing(s_sEnemy);
   ACR_TestRegress.Check("qrf: stands by at its post, its own side sees the area it covers", ordered && s_Plan && alpha.m_Order == "QRF: standing by" && ours && !theirs, string.Format("ordered %1, order '%2', ring for %3 %4 / %5 %6", ordered, alpha.m_Order, us, ours, s_sEnemy, theirs));
   s_Enemy = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, s_sEnemy), ACR_Manager.Ground(s_vPost + Vector(450, 0, 0))));
   PrintFormat("[ACR] QRF TEST enemy fire team 450 m out: %1", s_Enemy != null);
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   if (waited < 12) return;
   ACR_TestRegress.Check("qrf: an enemy the side knows of outside its area doesn't move it", s_Enemy && !s_Plan.m_bOut && !alpha.IsHunting() && alpha.m_Order == "QRF: standing by", string.Format("enemy %1, out %2, hunting %3, order '%4'", s_Enemy != null, s_Plan.m_bOut, alpha.IsHunting(), alpha.m_Order));
   KillGroup(h, s_Enemy);
   s_Enemy = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, s_sEnemy), ACR_Manager.Ground(s_vPost + Vector(150, 0, 0))));
   PrintFormat("[ACR] QRF TEST enemy fire team 150 m out: %1", s_Enemy != null);
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   if (!s_Plan.m_bOut && waited < 20) return;
   bool fed = Fed(manager, us, alpha.GetName() + " (QRF) moving out");
   ACR_TestRegress.Check("qrf: an enemy inside its area sends it hunting, the side told", s_Plan.m_bOut && alpha.IsHunting() && alpha.m_Order.StartsWith("Hunting") && alpha.m_Qrf == s_Plan && fed, string.Format("out %1 after %2 s, hunting %3, order '%4', target '%5', feed %6", s_Plan.m_bOut, waited, alpha.IsHunting(), alpha.m_Order, s_Plan.m_sTarget, fed));
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   // Let it close in a little, then the team is wiped out.
   if (waited == 5) KillGroup(h, s_Enemy);
   if (waited == 6) s_Enemy = null;
   if (waited < 6 || (s_Plan.m_bOut && waited < 40)) return;
   bool home = vector.DistanceXZ(alpha.m_MoveTarget, s_vPost) < 1;
   bool back = alpha.m_Order == "QRF: returning" || alpha.m_Order == "QRF: standing by";
   ACR_TestRegress.Check("qrf: once the target is dead it heads back to its post, still a QRF", !s_Plan.m_bOut && !alpha.IsHunting() && alpha.m_Qrf == s_Plan && back && home, string.Format("out %1 after %2 s, hunting %3, order '%4', heading %5 m from its post", s_Plan.m_bOut, waited, alpha.IsHunting(), alpha.m_Order, vector.DistanceXZ(alpha.m_MoveTarget, s_vPost)));
   // A new order ends it.
   alpha.MoveTo(s_vPost + Vector(-10, 0, 0));
   ACR_TestRegress.Check("qrf: a new order ends it (no ring)", !alpha.m_Qrf && !HasRing(us) && alpha.m_Order == "Moving", string.Format("qrf %1, ring %2, order '%3'", alpha.m_Qrf != null, HasRing(us), alpha.m_Order));
   ACR_TestRegress.Complete("qrf", CHECKS);
   s_iStep = 4;
  }
 }

 // Holding fire, and seen: every living member noted in the side's intel where he stands.
 protected static void Spot(ACR_Manager manager, string side, SCR_AIGroup group)
 {
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (utility && utility.GetCombatModeExternal() != EAIGroupCombatMode.HOLD_FIRE) utility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
  ACR_Intel intel = manager.IntelFor(side);
  if (!intel) return;
  float now = GetGame().GetWorld().GetWorldTime();
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   intel.m_SeenAt.Set(body, body.GetOrigin());
   intel.m_SeenTime.Set(body, now);
  }
 }

 protected static bool HasRing(string faction)
 {
  array<vector> positions = {};
  array<float> radii = {};
  array<string> labels = {};
  ACR_Qrf.Zones(faction, positions, radii, labels);
  foreach (int i, vector p : positions)
   if (vector.DistanceXZ(p, s_vPost) < 1 && labels[i].StartsWith("~QRF")) return true;
  return false;
 }

 protected static bool Fed(ACR_Manager manager, string faction, string text)
 {
  ACR_Intel intel = manager.IntelFor(faction);
  if (!intel) return false;
  foreach (string line : intel.m_Feed)
   if (line.Contains(text)) return true;
  return false;
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
}
