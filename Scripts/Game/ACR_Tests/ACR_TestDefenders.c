// -acrDefendersTest 1 (server-capable, ~1 min): defenders for Fortify positions. A position built
// at tier I, II and III (straight from plans, no work): 1 in reserve at tier I, 2 at tier II,
// 4 at tier III, the option sizes; an enemy fire team (holding its fire) 300 m out deploys them
// (their own group, not a squad, defending the position); one killed, then every enemy dead:
// they pack up with 3 back in reserve; a position that is gone takes its reserve with it.
// Checkpoints: none by default, -acrRoadblockDefenders 2 gives one 2 (a barricade none), and a
// dismantled one takes them with it.
class ACR_TestDefenders
{
 static const int CHECKS = 8;
 static int s_iStep;
 static int s_iStepAt;
 static ACR_Fortification s_Fort;
 static ref ACR_DefenderPost s_Post;
 static SCR_AIGroup s_Enemy;
 static string s_sEnemy;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  }
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetAnchor() || !alpha.GetFaction())
  {
   if (tick == 60) ACR_TestRegress.Abort("defenders", "no Alpha squad");
   return;
  }
  if (tick > 170) ACR_TestRegress.Abort("defenders", string.Format("timed out in step %1", s_iStep));
  if (tick < 5) return;
  int waited = tick - s_iStepAt;
  string us = alpha.GetFaction().GetFactionKey();
  if (s_Enemy) HoldFire(s_Enemy);

  if (s_iStep == 0)
  {
   s_sEnemy = "USSR";
   if (us == "USSR") s_sEnemy = "US";
   // The side can pay for each tier (the AI's first strategy tick, before tick 1 turns it off,
   // may spend it all: a LAV-25 once took 76 of 76 RP).
   ACR_FactionData side = manager.m_Bases.GetFaction(us);
   if (side) side.m_iRP = 500;
   // No enemy near while the position goes up (they would deploy at once).
   KillSide(h, manager, s_sEnemy);
   vector site;
   if (!FindSite(alpha, site))
   {
    ACR_TestRegress.Abort("defenders", "no site for a position");
    return;
   }
   string why;
   s_Fort = BuildTier(alpha, site, why);
   ACR_TestRegress.Check("defenders: a tier I position holds 1 in reserve", s_Fort && s_Fort.m_iTier == 1 && s_Fort.m_Defenders && s_Fort.m_Defenders.m_iReserve == 1, string.Format("position %1 (%2), reserve %3", s_Fort != null, why, ReserveOf(s_Fort)));
   if (!s_Fort) { ACR_TestRegress.Abort("defenders", "tier I not built: " + why); return; }
   bool second = BuildTier(alpha, s_Fort.m_vPos, why) == s_Fort && s_Fort.m_iTier == 2;
   s_Post = s_Fort.m_Defenders;
   ACR_TestRegress.Check("defenders: tier II holds 2 in reserve", second && s_Post && s_Post.m_iReserve == 2 && !s_Post.m_Group, string.Format("tier %1 (%2), reserve %3", s_Fort.m_iTier, why, Reserve()));
   bool third = BuildTier(alpha, s_Fort.m_vPos, why) == s_Fort && s_Fort.m_iTier == 3;
   ACR_TestRegress.Check("defenders: tier III adds the difference (4 in reserve)", third && s_Post && s_Post.m_iReserve == 4, string.Format("tier %1 (%2), reserve %3", s_Fort.m_iTier, why, Reserve()));
   // The sizes are options.
   ACR_Options.Set("acrFortifyDefenders1", "0");
   ACR_Options.Set("acrFortifyDefenders2", "0");
   ACR_Options.Set("acrFortifyDefenders3", "20");
   bool sized = ACR_Fortify.Defenders(1) == 0 && ACR_Fortify.Defenders(2) == 0 && ACR_Fortify.Defenders(3) == 12;
   ACR_Options.Unset("acrFortifyDefenders1");
   ACR_Options.Unset("acrFortifyDefenders2");
   ACR_Options.Unset("acrFortifyDefenders3");
   ACR_TestRegress.Check("defenders: the sizes per tier are options (0 none, at most 12)", sized && ACR_Fortify.Defenders(0) == 0 && ACR_Fortify.Defenders(1) == 1 && ACR_Fortify.Defenders(2) == 2 && ACR_Fortify.Defenders(3) == 4, "");
   // An enemy fire team 300 m out.
   vector enemyAt = ACR_Manager.Ground(s_Fort.m_vPos + Vector(300, 0, 0));
   s_Enemy = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(h.FireTeamOf(manager, s_sEnemy), enemyAt));
   PrintFormat("[ACR] DEFENDERS TEST enemy fire team 300 m out: %1", s_Enemy != null);
   NextStep(tick);
   return;
  }

  if (s_iStep == 1)
  {
   if (!s_Post.m_Group && waited < 15) return;
   int alive = manager.m_Bases.GuardsAlive(s_Post.m_Group);
   bool notSquad = s_Post.m_Group && !manager.FindSquadByGroup(s_Post.m_Group) && manager.m_Bases.IsGarrison(s_Post.m_Group);
   // #155: they hold behind the line, each on a post behind a sandbag position's wall.
   bool defending = s_Post.m_Group && s_Post.m_Group.GetCurrentWaypoint() && vector.DistanceXZ(s_Post.m_Group.GetCurrentWaypoint().GetOrigin(), ACR_Fortify.HoldCentre(s_Fort)) < 1;
   array<IEntity> posts = {};
   ACR_Garrison.PostEntities(s_Post.m_Group, posts);
   int behind;
   foreach (IEntity post : posts) if (post && ACR_TestFortify.BagZ(s_Fort, post.GetOrigin()) < ACR_TestFortify.WALL_Z) behind++;
   ACR_TestRegress.Check("defenders: an enemy 300 m out deploys them, defending the position from posts behind its bags (not a squad)", alive == 4 && s_Post.m_iReserve == 0 && notSquad && defending && posts.Count() == 4 && behind == 4, string.Format("out %1 after %2 s, %3 alive, reserve %4, not a squad %5, defending %6, %7 of %8 posts behind the bags", s_Post.m_Group != null, waited, alive, s_Post.m_iReserve, notSquad, defending, behind, posts.Count()));
   // One is killed; then every enemy is dead.
   array<AIAgent> agents = {};
   if (s_Post.m_Group) s_Post.m_Group.GetAgents(agents);
   if (!agents.IsEmpty()) h.Kill(agents[0].GetControlledEntity());
   KillSide(h, manager, s_sEnemy);
   s_Enemy = null;
   NextStep(tick);
   return;
  }

  if (s_iStep == 2)
  {
   if (s_Post.m_Group && waited < 15) return;
   ACR_TestRegress.Check("defenders: with no enemy left they pack up, used up as they die (3 back)", !s_Post.m_Group && s_Post.m_iReserve == 3, string.Format("out %1 after %2 s, reserve %3", s_Post.m_Group != null, waited, s_Post.m_iReserve));
   // Every piece gone: the position, and its reserve, go.
   foreach (IEntity piece : s_Fort.m_aPieces) if (piece) SCR_EntityHelper.DeleteEntityAndChildren(piece);
   NextStep(tick);
   return;
  }

  if (s_iStep == 3)
  {
   bool gone = !ACR_Defenders.s_Posts.Contains(s_Post);
   if (!gone && waited < 15) return;
   ACR_TestRegress.Check("defenders: a position that is gone takes its reserve with it", gone && s_Post.m_bGone && s_Post.m_iReserve == 0 && !ACR_Fortify.s_Positions.Contains(s_Fort), string.Format("post kept %1, gone %2, reserve %3", !gone, s_Post.m_bGone, s_Post.m_iReserve));
   // Checkpoints (option, off by default).
   vector rbAt = ACR_Manager.Ground(alpha.GetAnchor().GetOrigin() + Vector(-60, 0, 40));
   ACR_Roadblock plain = ACR_Roadblocks.SpawnRoadblock(us, rbAt, 0, "test");
   ACR_Options.Set("acrRoadblockDefenders", "2");
   ACR_Roadblock held = ACR_Roadblocks.SpawnRoadblock(us, rbAt + Vector(0, 0, 40), 0, "test");
   bool barricade = ACR_Roadblocks.Defenders(ACR_ERoadblockType.BARRICADE) == 0;
   ACR_Options.Unset("acrRoadblockDefenders");
   ACR_DefenderPost rbPost;
   if (held) rbPost = held.m_Defenders;
   bool sized = plain && !plain.m_Defenders && rbPost && rbPost.m_iReserve == 2 && barricade;
   if (plain) ACR_Roadblocks.DismantleRoadblock(plain);
   if (held) ACR_Roadblocks.DismantleRoadblock(held);
   ACR_TestRegress.Check("defenders: a checkpoint gets them only with -acrRoadblockDefenders (2 here, a barricade none), dismantled they go", sized && rbPost.m_bGone, string.Format("plain %1 (defenders %2), held %3 (reserve %4, gone %5), barricade %6", plain != null, plain && plain.m_Defenders != null, held != null, rbPost && rbPost.m_iReserve, rbPost && rbPost.m_bGone, barricade));
   ACR_TestRegress.Complete("defenders", CHECKS);
   s_iStep = 4;
  }
 }

 // Plan the next tier at point and put its pieces in at once (no work).
 protected static ACR_Fortification BuildTier(ACR_Squad squad, vector point, out string why)
 {
  ACR_FortifyPlan plan = ACR_Fortify.Plan(squad, point, why);
  if (!plan) return null;
  return ACR_Fortify.Build(plan, why);
 }

 protected static bool FindSite(ACR_Squad squad, out vector site)
 {
  vector from = squad.GetAnchor().GetOrigin();
  for (int r = 0; r < 4; r++)
  {
   for (int a = 0; a < 12; a++)
   {
    float angle = Math.PI2 * a / 12;
    vector point = ACR_Manager.Ground(from + Vector(Math.Cos(angle), 0, Math.Sin(angle)) * (50 + r * 20));
    string why;
    if (!ACR_Fortify.Plan(squad, point, why)) continue;
    site = point;
    return true;
   }
  }
  return false;
 }

 protected static int Reserve()
 {
  if (!s_Post) return -1;
  return s_Post.m_iReserve;
 }

 protected static int ReserveOf(ACR_Fortification fort)
 {
  if (!fort || !fort.m_Defenders) return -1;
  return fort.m_Defenders.m_iReserve;
 }

 protected static void HoldFire(SCR_AIGroup group)
 {
  SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
  if (utility && utility.GetCombatModeExternal() != EAIGroupCombatMode.HOLD_FIRE) utility.SetCombatMode(EAIGroupCombatMode.HOLD_FIRE);
 }

 // Every soldier of the side's squads (the AI commanders' first buys, a spawned team).
 protected static void KillSide(ACR_TestHarness h, ACR_Manager manager, string faction)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents) h.Kill(agent.GetControlledEntity());
  }
 }

 protected static void NextStep(int tick)
 {
  s_iStep++;
  s_iStepAt = tick;
 }
}
