// Sabotage raids (#96 part 2): mostly at night, a few men slip up to a weakly guarded target and
// blow it: a FOB HQ (the existing demolition rule takes it down and tells its owner,
// ACR_Demolition.OnChargeDetonated; its AI rebuilds through "restore the FOB first") or an
// unmanned roadblock (dismantled by the blast). Never a starting HQ (only built FOBs are targets),
// one raid at a time, no plant with a defender within ACR_Demolition.DEFENDER_RADIUS of it.
//  - A hideout buys it (ACR_FIAGrowth.Spend -> Recruit, RAID_COST): a chance each minute it can,
//    NIGHT_FACTOR times likelier at night (Jacob's open question 2: likelier, not night only).
//  - The team is a defender post (ACR_DefenderPost.m_Raid) but never deploys for a nearby enemy:
//    it walks there unseen (distance / SPEED), appears STAGE m out on the side it came from and
//    walks in. Base garrisons deploy against it like any enemy (ACR_BaseSystem counts every body),
//    and built positions' defenders do too (ACR_Defenders.EnemyWithin counts raiders).
//  - At the target with nobody of its owner within DEFENDER_RADIUS: a vanilla charge on a FUSE_S
//    timed fuze (players can Disarm it), then they leave. Held off for GIVE_UP_MS: they leave.
// Owner-agnostic for #97: Start(owner, target, from) takes any owner; ACR_IED.Target decides who
// the targets are.
enum ACR_ERaidState
{
 TRAVEL,
 APPROACH,
 PLANTED,
 DONE
}

class ACR_Raid : Managed
{
 static const int RAID_COST = 3;
 static const int MEN = 3;
 static const float RAID_REACH = 3000;      // m from the hideout to its target
 static const float SPEED = 1.2;            // m/s on the way there, unseen
 static const float STAGE = 150;            // they appear this far out
 static const float GUARD_REACH = 100;      // weakly guarded: nobody of its owner this close
 static const float PLANT_RADIUS = 9.5;     // a man this close to the target plants
 static const float FUSE_S = 20;
 static const float LEAVE = 300;
 static const float GIVE_UP_MS = 240000;
 static const float CHANCE = 0.1;           // a minute, by day
 static const float NIGHT_FACTOR = 3;
 static const float ROADBLOCK_BLAST = 15;   // a charge this close to a roadblock takes it down
 static bool s_bAlwaysForTest;              // tests: the roll always succeeds
 static float s_fStageForTest;              // tests: appear this close instead (0: STAGE)

 string m_sOwner;
 string m_sTargetSide;
 ACR_Base m_Fob;               // the FOB whose HQ it goes for (or null)
 ACR_Roadblock m_Roadblock;    // ... or this roadblock
 vector m_vTarget;
 ACR_DefenderPost m_Post;
 ACR_DefenderPost m_From;
 int m_eState;
 float m_fArriveAt;
 float m_fAtTargetSince;
 float m_fOrderedAt;
 IEntity m_Charge;
 string m_sName;

 static ref array<ref ACR_Raid> s_aRaids;

 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_aRaids = {};
 }

 static void Reset()
 {
  s_aRaids.Clear();
  s_bAlwaysForTest = false;
  s_fStageForTest = 0;
 }

 static ACR_Raid Running()
 {
  foreach (ACR_Raid raid : s_aRaids) if (raid.m_eState != ACR_ERaidState.DONE) return raid;
  return null;
 }

 static bool IsNight()
 {
  ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
  if (!world || !world.GetTimeAndWeatherManager()) return false;
  TimeAndWeatherManagerEntity time = world.GetTimeAndWeatherManager();
  return time.IsNightHour(time.GetTimeOfTheDay());
 }

 // A hideout's turn to buy (ACR_FIAGrowth.Spend, once a minute).
 static bool Recruit(ACR_DefenderPost hideout)
 {
  if (hideout.m_Cell.m_fPool < RAID_COST || Running()) return false;
  float chance = CHANCE;
  if (IsNight()) chance = CHANCE * NIGHT_FACTOR;
  if (!s_bAlwaysForTest && Math.RandomFloat01() >= chance) return false;
  ACR_Base fob;
  ACR_Roadblock roadblock;
  if (!PickTarget(ACR_Pockets.FACTION, hideout.m_vPos, fob, roadblock)) return false;
  if (!Start(ACR_Pockets.FACTION, fob, roadblock, hideout)) return false;
  hideout.m_Cell.m_fPool -= RAID_COST;
  return true;
 }

 // A target side's weakly guarded FOB HQ or unmanned roadblock nearest 'from' within RAID_REACH.
 static bool PickTarget(string owner, vector from, out ACR_Base fob, out ACR_Roadblock roadblock)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  float best = RAID_REACH;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.m_bBuilt || base.m_bRetired || base.m_bHqLost || !ACR_IED.Target(owner, base.m_sOwner)) continue;
   IEntity hq;
   if (manager.m_Structures) hq = manager.m_Structures.HqEntityOf(base);
   if (!hq) continue;
   float d = vector.DistanceXZ(hq.GetOrigin(), from);
   if (d >= best || ACR_SapperDemo.SoldiersNear(hq.GetOrigin(), base.m_sOwner, GUARD_REACH) > 0) continue;
   best = d;
   fob = base;
   roadblock = null;
  }
  foreach (ACR_Roadblock rb : ACR_Roadblocks.s_aRoadblocks)
  {
   if (!rb || !rb.m_Entity || !ACR_IED.Target(owner, rb.m_sFaction)) continue;
   float gap = vector.DistanceXZ(rb.m_vPos, from);
   if (gap >= best || ACR_SapperDemo.SoldiersNear(rb.m_vPos, rb.m_sFaction, GUARD_REACH) > 0) continue;
   if (rb.m_Defenders && rb.m_Defenders.m_Group) continue;
   best = gap;
   roadblock = rb;
   fob = null;
  }
  return fob != null || roadblock != null;
 }

 // The raid sets off from 'from' (a hideout, or any post for #97) towards its target.
 static bool Start(string owner, ACR_Base fob, ACR_Roadblock roadblock, ACR_DefenderPost from)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !from || (!fob && !roadblock)) return false;
  ACR_Raid raid = new ACR_Raid();
  raid.m_sOwner = owner;
  raid.m_Fob = fob;
  raid.m_Roadblock = roadblock;
  if (fob)
  {
   raid.m_sTargetSide = fob.m_sOwner;
   raid.m_vTarget = manager.m_Structures.HqEntityOf(fob).GetOrigin();
   raid.m_sName = fob.m_sName + " HQ";
  }
  else
  {
   raid.m_sTargetSide = roadblock.m_sFaction;
   raid.m_vTarget = roadblock.m_vPos;
   raid.m_sName = string.Format("the roadblock at %1", ACR_Manager.Grid(roadblock.m_vPos));
  }
  raid.m_From = from;
  float now = GetGame().GetWorld().GetWorldTime();
  float d = vector.DistanceXZ(from.m_vPos, raid.m_vTarget);
  raid.m_fArriveAt = now + d / SPEED * 1000;
  raid.m_eState = ACR_ERaidState.TRAVEL;
  ACR_DefenderPost post = ACR_Defenders.Add(owner, from.m_vPos, string.Format("FIA saboteurs for %1", raid.m_sName), 8);
  post.m_iReserve = MEN;
  post.m_Raid = raid;
  raid.m_Post = post;
  s_aRaids.Insert(raid);
  PrintFormat("[ACR] RAID %1 sets off from %2 for %3 (%4 side, %5 m, there in %6 s, night %7)", owner, from.m_sName, raid.m_sName, raid.m_sTargetSide, (int)d, (int)(d / SPEED), IsNight());
  return true;
 }

 // Still worth blowing: there, its side still a target, and (FOB) its HQ standing.
 protected bool TargetStands()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (m_Fob)
  {
   if (m_Fob.m_bRetired || m_Fob.m_bHqLost || !ACR_IED.Target(m_sOwner, m_Fob.m_sOwner) || !manager || !manager.m_Structures) return false;
   return manager.m_Structures.HqEntityOf(m_Fob) != null;
  }
  return m_Roadblock && m_Roadblock.m_Entity && ACR_Roadblocks.s_aRoadblocks.Contains(m_Roadblock) && ACR_IED.Target(m_sOwner, m_Roadblock.m_sFaction);
 }

 // Every second (ACR_Guerrillas.Tick).
 static void Tick()
 {
  if (s_aRaids.IsEmpty()) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases) return;
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = s_aRaids.Count() - 1; i >= 0; i--)
  {
   ACR_Raid raid = s_aRaids[i];
   if (raid.m_eState == ACR_ERaidState.DONE)
   {
    if (!raid.m_Post || raid.m_Post.m_bGone) s_aRaids.RemoveOrdered(i);
    continue;
   }
   raid.Step(manager, now);
  }
 }

 protected void Step(ACR_Manager manager, float now)
 {
  if (m_eState == ACR_ERaidState.TRAVEL)
  {
   if (now < m_fArriveAt) return;
   if (!TargetStands())
   {
    End("the target is gone");
    return;
   }
   Arrive(manager, now);
   return;
  }
  if (!m_Post.m_Group || manager.m_Bases.GuardsAlive(m_Post.m_Group) <= 0)
  {
   if (m_eState == ACR_ERaidState.APPROACH)
   {
    ACR_FIAIntel.Wiped(m_vTarget, m_From, "dead saboteurs");
    End("the team is dead");
   }
   else if (!m_Charge) End("the charge went");
   return;
  }
  if (m_eState == ACR_ERaidState.APPROACH)
  {
   if (!TargetStands())
   {
    Leave("the target is gone");
    return;
   }
   IEntity planter = Nearest(m_vTarget);
   int defenders = ACR_SapperDemo.SoldiersNear(m_vTarget, m_sTargetSide, ACR_Demolition.DEFENDER_RADIUS);
   if (planter && defenders == 0 && vector.DistanceXZ(planter.GetOrigin(), m_vTarget) <= PLANT_RADIUS)
   {
    Plant(planter);
    return;
   }
   if (defenders > 0) ACR_Guerrillas.SetCombatMode(m_Post.m_Group, EAIGroupCombatMode.FIRE_AT_WILL);
   if (now - m_fAtTargetSince >= GIVE_UP_MS)
   {
    Leave("held off");
    return;
   }
   if (now - m_fOrderedAt >= 20000)
   {
    m_fOrderedAt = now;
    ACR_Guerrillas.Order(m_Post.m_Group, ACR_Squad.WP_MOVE, m_vTarget, 3, false);
   }
   return;
  }
  // Planted: watch the charge go (or get disarmed).
  if (m_Charge)
  {
   SCR_ExplosiveChargeComponent chargeComp = ACR_SapperDemo.ChargeComponent(m_Charge);
   if (!chargeComp || chargeComp.GetUsedFuzeType() != SCR_EFuzeType.NONE) return;
   PrintFormat("[ACR] RAID charge at %1 disarmed", m_sName);
   SCR_EntityHelper.DeleteEntityAndChildren(m_Charge);
   m_Charge = null;
   m_eState = ACR_ERaidState.DONE;
   return;
  }
  Blown();
 }

 // Out of sight STAGE m from the target on the side they came from, walking in, holding their fire.
 protected void Arrive(ACR_Manager manager, float now)
 {
  vector from = m_From.m_vPos - m_vTarget;
  from[1] = 0;
  if (from.LengthSq() < 0.01) from = Vector(1, 0, 0);
  from.Normalize();
  float standOff = STAGE;
  if (s_fStageForTest > 0) standOff = s_fStageForTest;
  vector stage = ACR_Manager.Ground(m_vTarget + from * standOff);
  m_Post.m_vPos = stage;
  m_Post.m_Group = manager.m_Bases.SpawnGuards(m_sOwner, stage, m_Post.m_iReserve, m_Post.m_sName, 8, 6, null);
  if (!m_Post.m_Group)
  {
   End("no men spawned");
   return;
  }
  m_Post.m_iReserve = 0;
  ACR_Guerrillas.SetCombatMode(m_Post.m_Group, EAIGroupCombatMode.HOLD_FIRE);
  ACR_Guerrillas.Order(m_Post.m_Group, ACR_Squad.WP_MOVE, m_vTarget, 3, false);
  m_fOrderedAt = now;
  m_fAtTargetSince = now;
  m_eState = ACR_ERaidState.APPROACH;
  PrintFormat("[ACR] RAID at %1: %2 men %3 m out, going in", m_sName, manager.m_Bases.GuardsAlive(m_Post.m_Group), (int)standOff);
 }

 // The team's living man nearest 'pos' (null: none).
 protected IEntity Nearest(vector pos)
 {
  array<AIAgent> agents = {};
  m_Post.m_Group.GetAgents(agents);
  IEntity best;
  float nearest = 100000;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   float d = vector.DistanceXZ(body.GetOrigin(), pos);
   if (d >= nearest) continue;
   nearest = d;
   best = body;
  }
  return best;
 }

 // A charge at his feet towards the target on a timed fuze (him its user: the losses are his).
 protected void Plant(IEntity planter)
 {
  vector toward = m_vTarget - planter.GetOrigin();
  toward[1] = 0;
  if (toward.LengthSq() > 1) toward.Normalize();
  m_Charge = ACR_Manager.SpawnPrefab(ACR_Demolition.ChargeFor(m_sOwner), ACR_Manager.Ground(planter.GetOrigin() + toward));
  SCR_ExplosiveChargeComponent chargeComp = ACR_SapperDemo.ChargeComponent(m_Charge);
  if (!chargeComp)
  {
   Leave("no charge");
   return;
  }
  if (chargeComp.GetTrigger()) chargeComp.GetTrigger().SetUser(planter);
  chargeComp.SetFuzeTime(FUSE_S, true);
  chargeComp.ArmWithTimedFuze();
  m_eState = ACR_ERaidState.PLANTED;
  PrintFormat("[ACR] RAID charge set at %1 (%2 s fuze), leaving", m_sName, (int)FUSE_S);
  Withdraw();
 }

 // The charge went: a roadblock it was for comes down (a FOB HQ went through ACR_Demolition).
 protected void Blown()
 {
  m_eState = ACR_ERaidState.DONE;
  PrintFormat("[ACR] RAID charge at %1 went off", m_sName);
  if (!m_Roadblock || !m_Roadblock.m_Entity || vector.DistanceXZ(m_Roadblock.m_vPos, m_vTarget) > ROADBLOCK_BLAST) return;
  string side = m_Roadblock.m_sFaction;
  ACR_Roadblocks.DismantleRoadblock(m_Roadblock);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) manager.NotifyFaction(side, string.Format("Roadblock at %1 destroyed by saboteurs", ACR_Manager.Grid(m_vTarget)));
 }

 protected void Leave(string why)
 {
  PrintFormat("[ACR] RAID at %1 called off (%2)", m_sName, why);
  m_eState = ACR_ERaidState.DONE;
  Withdraw();
 }

 // Never got there (or no one left): the post goes, its men back in the hideout's pool.
 protected void End(string why)
 {
  PrintFormat("[ACR] RAID for %1 ends (%2)", m_sName, why);
  m_eState = ACR_ERaidState.DONE;
  if (m_Post && !m_Post.m_Group)
  {
   if (m_From && !m_From.m_bGone && m_From.m_Cell && m_Post.m_iReserve > 0) m_From.m_Cell.m_fPool = Math.Min(ACR_FIAGrowth.PoolCap(), m_From.m_Cell.m_fPool + 1);
   ACR_Defenders.Remove(m_Post);
  }
 }

 protected void Withdraw()
 {
  if (!m_Post || !m_Post.m_Group) return;
  vector away = m_Post.m_vPos - m_vTarget;
  away[1] = 0;
  if (away.LengthSq() < 0.01) away = Vector(1, 0, 0);
  away.Normalize();
  ACR_Guerrillas.SetCombatMode(m_Post.m_Group, EAIGroupCombatMode.RETURN_FIRE);
  ACR_Guerrillas.Order(m_Post.m_Group, ACR_Squad.WP_MOVE, ACR_Manager.Ground(m_vTarget + away * LEAVE), 20, false);
 }

 // ---------------------------------------------------------------- ACR_Defenders hooks

 // Out until the raid is over and no enemy is near its men.
 static bool StayOut(ACR_Manager manager, ACR_DefenderPost post)
 {
  if (post.m_Raid.m_eState != ACR_ERaidState.DONE) return true;
  return ACR_IED.HostileNear(post.m_sFaction, post.m_vPos, ACR_Guerrillas.CLEAR) || ACR_IED.HostileNear(post.m_sFaction, post.m_Raid.m_vTarget, ACR_Guerrillas.CLEAR);
 }

 // Packed up after the raid: the survivors home, a man back in the pool.
 static void PackedUp(ACR_DefenderPost post)
 {
  ACR_Raid raid = post.m_Raid;
  if (raid.m_From && !raid.m_From.m_bGone && raid.m_From.m_Cell && post.m_iReserve > 0) raid.m_From.m_Cell.m_fPool = Math.Min(ACR_FIAGrowth.PoolCap(), raid.m_From.m_Cell.m_fPool + 1);
  raid.m_eState = ACR_ERaidState.DONE;
  PrintFormat("[ACR] RAID %1: %2 men home", post.m_sName, post.m_iReserve);
  ACR_Defenders.Remove(post);
 }

 // A raider's body within range (ACR_Defenders.EnemyWithin: built positions' defenders deploy
 // against them; base garrisons see every body already).
 static bool RaiderWithin(string faction, vector pos, float range)
 {
  foreach (ACR_Raid raid : s_aRaids)
  {
   if (!raid.m_Post || !raid.m_Post.m_Group || !ACR_IED.Target(raid.m_sOwner, faction)) continue;
   array<AIAgent> agents = {};
   raid.m_Post.m_Group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (body && ACR_Respawn.IsAlive(body) && vector.DistanceXZ(body.GetOrigin(), pos) <= range) return true;
   }
  }
  return false;
 }
}
