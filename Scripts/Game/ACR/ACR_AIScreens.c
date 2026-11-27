// AI commander front-line move-up, screening posts, and transport staging; extracted from ACR_AICommander.c.
class ACR_AIScreens
{
 // How far a base is from the nearest enemy base this side knows of (-1: none known).
 static float EnemyGap(ACR_AICommander cmd, ACR_Base base)
 {
  ACR_Base enemy = cmd.NearestEnemyBase(base.GetOrigin());
  if (!enemy) return -1;
  return vector.DistanceXZ(base.GetOrigin(), enemy.GetOrigin());
 }

 static void UpdateFrontGap(ACR_AICommander cmd)
 {
  cmd.m_fFrontGap = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != cmd.m_sFaction) continue;
   float gap = EnemyGap(cmd, base);
   if (gap < 0) return; // no enemy base known: no front, no rear
   if (cmd.m_fFrontGap < 0 || cmd.m_fFrontGap > gap) cmd.m_fFrontGap = gap;
  }
 }

 // Ours, quiet and far behind the front: its posted squads move up (MoveUp), none are posted to it.
 static bool RearBase(ACR_AICommander cmd, ACR_Base base)
 {
  if (cmd.m_fFrontGap < 0 || !base || base.m_sOwner != cmd.m_sFaction) return false;
  if (EnemyGap(cmd, base) <= cmd.m_fFrontGap + ACR_AICommander.MOVEUP_DEPTH) return false;
  return cmd.ThreatTo(base) <= 0 && cmd.DefenceThreat(base) <= 0;
 }

 // Drops the rear bases from a list of posts. All of them rear (FrontlineBases picked bases facing
 // only empty ones): the posts become our bases nearest the enemy instead.
 static void RearOut(ACR_AICommander cmd, array<ACR_Base> posts)
 {
  array<ACR_Base> forward = {};
  foreach (ACR_Base post : posts) if (!RearBase(cmd, post)) forward.Insert(post);
  if (forward.IsEmpty() && !posts.IsEmpty())
  {
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (!base || base.m_bRetired || base.m_sOwner != cmd.m_sFaction) continue;
    float gap = EnemyGap(cmd, base);
    if (gap >= 0 && gap <= cmd.m_fFrontGap + ACR_AICommander.FRONTLINE_SLACK) forward.Insert(base);
   }
  }
  if (!forward.IsEmpty()) posts.Copy(forward);
 }

 static void MoveUp(ACR_AICommander cmd)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (cmd.m_fFrontGap < 0 || now < cmd.m_fNextMoveUp) return;
  cmd.m_fNextMoveUp = now + ACR_AICommander.MOVEUP_INTERVAL_MS;
  // The posted squads at rear bases, farthest back first.
  array<ACR_Squad> free = {};
  cmd.MySquads(free, true);
  array<ACR_Squad> movers = {};
  array<ACR_Base> froms = {};
  array<float> gaps = {};
  foreach (ACR_Squad squad : free)
  {
   if (squad.m_iAITask != ACR_EAITask.POSTED || squad == cmd.m_ReserveSquad || squad == cmd.m_FobEscort || cmd.OnRequest(squad) || ACR_AICommander.IsReconTeam(squad)) continue;
   if (squad.KnownEnemies(ACR_AICommander.MOVEUP_CALM) > 0) continue;
   ACR_Base from = cmd.NearestOwnBase(cmd.PositionOf(squad));
   if (!from || vector.DistanceXZ(cmd.PositionOf(squad), from.GetOrigin()) > from.m_fRadius + ACR_AICommander.DEFENSE_DISTANCE || !RearBase(cmd, from)) continue;
   float gap = EnemyGap(cmd, from);
   int at = 0;
   while (at < gaps.Count() && gaps[at] >= gap) at++;
   movers.InsertAt(squad, at);
   froms.InsertAt(from, at);
   gaps.InsertAt(gap, at);
  }
  foreach (int i, ACR_Squad mover : movers)
  {
   bool byTruck;
   ACR_Base to = MoveUpTo(cmd, mover, gaps[i], byTruck);
   if (!to) continue;
   mover.SetAITask(ACR_EAITask.POSTED);
   mover.SetMode(ACR_EMoveMode.FAST);
   cmd.Travel(mover, to.GetOrigin(), true, 0);
   string how = "on foot";
   if (byTruck) how = "by truck";
   cmd.Say(string.Format("[ACR] AI %1: %2 moves up from %3 to %4 (%5 m from the enemy, our front is %6 m; %7)", cmd.m_sFaction, mover.GetName(), froms[i].m_sName, to.m_sName, (int)gaps[i], (int)cmd.m_fFrontGap, how));
   return;
  }
 }

 // Where a squad at a rear base 'fromGap' m from the enemy moves up to (MoveUpPick on our bases).
 // null: nowhere yet. 'byTruck': it counts on a free truck to get there.
 static ACR_Base MoveUpTo(ACR_AICommander cmd, ACR_Squad squad, float fromGap, out bool byTruck)
 {
  vector at = cmd.PositionOf(squad);
  byTruck = cmd.AllocTruckNear(squad);
  array<ACR_Base> bases = {};
  array<float> gaps = {};
  array<float> dists = {};
  array<float> scores = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != cmd.m_sFaction) continue;
   float d = vector.DistanceXZ(at, base.GetOrigin());
   float score = d + cmd.PostedAt(base) * 400;
   if (cmd.LaneActive() && cmd.m_Lane.Contains(base.GetOrigin())) score -= ACR_AILanes.POST_PULL;
   bases.Insert(base);
   gaps.Insert(EnemyGap(cmd, base));
   dists.Insert(d);
   scores.Insert(score);
  }
  bool atFront;
  int pick = MoveUpPick(gaps, dists, scores, fromGap, cmd.m_fFrontGap, byTruck, atFront);
  if (pick < 0) return null;
  if (!atFront) byTruck = false;
  return bases[pick];
 }

 // Pure (tests): of bases 'gaps' m from the enemy (-1: none known), 'dists' m away, posting 'scores'
 // (lower first), the one a squad at a base 'fromGap' m from the enemy moves up to: every pick is at
 // least MOVEUP_GAIN nearer the enemy. A base at the front ('frontGap' + MOVEUP_DEPTH) by truck or
 // within MOVEUP_WALK, best score first ('atFront'); else the walk within MOVEUP_WALK that ends
 // nearest the enemy. -1: none.
 static int MoveUpPick(notnull array<float> gaps, notnull array<float> dists, notnull array<float> scores, float fromGap, float frontGap, bool truck, out bool atFront)
 {
  atFront = false;
  int front = -1;
  int hop = -1;
  foreach (int i, float gap : gaps)
  {
   if (gap < 0 || gap > fromGap - ACR_AICommander.MOVEUP_GAIN) continue;
   if (gap <= frontGap + ACR_AICommander.MOVEUP_DEPTH && (truck || dists[i] <= ACR_AICommander.MOVEUP_WALK) && (front < 0 || scores[i] < scores[front])) front = i;
   if (dists[i] <= ACR_AICommander.MOVEUP_WALK && (hop < 0 || gap < gaps[hop])) hop = i;
  }
  if (front >= 0)
  {
   atFront = true;
   return front;
  }
  return hop;
 }

 static bool IsScreen(ACR_AICommander cmd, ACR_Squad squad)
 {
  if (!squad) return false;
  foreach (ACR_ScreenPost post : cmd.m_Screens)
   if (post.m_Squad == squad) return true;
  return false;
 }

 static ACR_ScreenPost ScreenOf(ACR_AICommander cmd, ACR_Base base)
 {
  foreach (ACR_ScreenPost post : cmd.m_Screens)
   if (post.m_Base == base) return post;
  return null;
 }

 static void ManageScreens(ACR_AICommander cmd)
 {
  if (!cmd.m_Forecast || !ACR_DefenceForecast.Acts())
  {
   cmd.m_Screens.Clear();
   return;
  }
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_Base> enemyBases = {};
  foreach (ACR_Base seen : ACR_Base.s_All)
   if (seen && !seen.m_bRetired && seen.m_sOwner != cmd.m_sFaction && cmd.Visible(seen) && cmd.HostileOwner(seen)) enemyBases.Insert(seen);
  array<ACR_Base> wanted = {};
  ScreensWanted(cmd, enemyBases, now, wanted);
  // The ones out: kept, pulled back, moved or released.
  for (int i = cmd.m_Screens.Count() - 1; i >= 0; i--)
  {
   ACR_ScreenPost post = cmd.m_Screens[i];
   ACR_Squad squad = post.m_Squad;
   // Someone else has it now (a player, a defence, a refit), or it's gone.
   if (!squad || !cmd.m_Manager.m_Squads.Contains(squad) || squad.CountAlive() == 0 || squad.PlayerSteers() || !cmd.Ours(squad) || squad.m_iAITask != ACR_EAITask.POSTED || !post.m_Base || post.m_Base.m_sOwner != cmd.m_sFaction)
   {
    cmd.m_Screens.Remove(i);
    continue;
   }
   if (squad.KnownEnemies(ACR_AICommander.SCREEN_CONTACT) > 0 || squad.StrengthRatio() < ACR_AICommander.SCREEN_HURT)
   {
    string why = "the enemy is close";
    if (squad.KnownEnemies(ACR_AICommander.SCREEN_CONTACT) == 0) why = "men lost";
    PullBackScreen(cmd, post, why, now);
    cmd.m_Screens.Remove(i);
    continue;
   }
   if (!wanted.Contains(post.m_Base))
   {
    if (post.m_fQuietSince < 0) post.m_fQuietSince = now;
    if (now - post.m_fQuietSince >= ACR_AICommander.SCREEN_QUIET_MS)
    {
     squad.SetAITask(ACR_EAITask.FREE);
     cmd.Say(string.Format("[ACR] AI %1: screen %2 at %3 released (no attack expected there now)", cmd.m_sFaction, squad.GetName(), post.m_Base.m_sName));
     cmd.m_Screens.Remove(i);
    }
    continue;
   }
   post.m_fQuietSince = -1;
   float bearing = ScreenBearing(cmd, post.m_Base, enemyBases);
   if (bearing < 0 || ACR_AICommander.SCREEN_RESITE_ANGLE >= ACR_DefenceForecast.AngleBetween(bearing, post.m_fBearing) || now - post.m_fSince < ACR_AICommander.SCREEN_RESITE_MS) continue;
   vector moved;
   int movedSees;
   if (!ACR_DefenceScreen.Spot(post.m_Base, bearing, enemyBases, moved, movedSees)) continue;
   SendScreen(cmd, post, squad, moved, bearing, movedSees, now, "moves to watch the " + ACR_DefenceForecast.CompassOf(bearing));
  }
  // New ones.
  foreach (ACR_Base base : wanted)
  {
   float again;
   if (ScreenOf(cmd, base) || (cmd.m_ScreenAgain.Find(base, again) && now < again)) continue; // its screen just pulled back
   float way = ScreenBearing(cmd, base, enemyBases);
   if (way < 0) continue;
   vector spot;
   int sees;
   if (!ACR_DefenceScreen.Spot(base, way, enemyBases, spot, sees)) continue;
   ACR_Squad screen = ScreenSquad(cmd, spot);
   if (!screen) continue;
   ACR_ScreenPost fresh = new ACR_ScreenPost();
   fresh.m_Base = base;
   cmd.m_Screens.Insert(fresh);
   SendScreen(cmd, fresh, screen, spot, way, sees, now, "posted");
  }
 }

 // Our front bases worth a screen, likeliest first, as many as the force allows.
 static void ScreensWanted(ACR_AICommander cmd, notnull array<ACR_Base> enemyBases, float now, notnull array<ACR_Base> wanted)
 {
  wanted.Clear();
  if (enemyBases.IsEmpty()) return;
  array<ACR_Squad> all = {};
  cmd.MySquads(all, false);
  int combat;
  foreach (ACR_Squad squad : all) if (squad.IsCombatUnit()) combat++;
  int most = (int)Math.Min(ACR_AICommander.SCREEN_MOST, combat / ACR_AICommander.SCREEN_SQUADS_PER);
  if (most <= 0) return;
  array<float> weights = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != cmd.m_sFaction || cmd.FrontDistance(base) > ACR_AICommander.SCREEN_FRONT) continue;
   float again;
   if (cmd.m_ScreenAgain.Find(base, again) && now < again && !ScreenOf(cmd, base)) continue;
   ACR_BaseForecast entry = cmd.m_Forecast.Of(base);
   if (!entry) continue;
   float weight = entry.m_fExpected + entry.m_fLane * ACR_AICommander.SCREEN_LANE_WEIGHT;
   if (weight < ACR_AICommander.SCREEN_LEAST) continue;
   int at = 0;
   while (at < weights.Count() && weights[at] >= weight) at++;
   wanted.InsertAt(base, at);
   weights.InsertAt(weight, at);
  }
  while (wanted.Count() > most) wanted.Remove(wanted.Count() - 1);
 }

 // The way a base's screen watches: the forecast's main way in, else toward the nearest enemy base we
 // know of. -1: neither.
 static float ScreenBearing(ACR_AICommander cmd, ACR_Base base, notnull array<ACR_Base> enemyBases)
 {
  ACR_BaseForecast entry = cmd.m_Forecast.Of(base);
  if (entry && entry.m_fExpected >= ACR_DefenceForecast.NOTE)
  {
   ACR_ForecastWay lead = entry.LeadWay();
   if (lead) return lead.m_fBearing;
  }
  ACR_Base nearest;
  float best = float.MAX;
  foreach (ACR_Base enemy : enemyBases)
  {
   float d = vector.DistanceXZ(enemy.GetOrigin(), base.GetOrigin());
   if (d < best) { best = d; nearest = enemy; }
  }
  if (!nearest) return -1;
  return ACR_Intel.Bearing(base.GetOrigin(), nearest.GetOrigin());
 }

 // A recon team first, else the smallest free fighting squad, within reach of the spot.
 static ACR_Squad ScreenSquad(ACR_AICommander cmd, vector spot)
 {
  array<ACR_Squad> free = {};
  cmd.MySquads(free, true);
  ACR_Squad best;
  float bestScore = float.MAX;
  foreach (ACR_Squad squad : free)
  {
   if (squad == cmd.m_ReserveSquad || squad == cmd.m_FobEscort || squad.m_Commander != 0 || ACR_AICommander.IsSupportTeam(squad) || squad.CountAlive() < 2) continue;
   if (squad.KnownEnemies(ACR_AICommander.SCREEN_CONTACT) > 0 || cmd.DueForRefit(squad)) continue;
   float d = vector.DistanceXZ(cmd.PositionOf(squad), spot);
   if (d > ACR_AICommander.SCREEN_REACH) continue;
   float score = squad.Strength() * 100 + d * 0.1;
   if (ACR_AICommander.IsReconTeam(squad)) score -= 100000;
   if (score < bestScore) { bestScore = score; best = squad; }
  }
  return best;
 }

 static void SendScreen(ACR_AICommander cmd, ACR_ScreenPost post, ACR_Squad squad, vector spot, float bearing, int sees, float now, string what)
 {
  post.m_Squad = squad;
  post.m_vSpot = spot;
  post.m_fBearing = bearing;
  post.m_fSince = now;
  post.m_iSees = sees;
  post.m_fQuietSince = -1;
  if (squad == cmd.m_ReserveSquad) cmd.m_ReserveSquad = null;
  cmd.LeaveAttacks(squad);
  squad.SetAITask(ACR_EAITask.POSTED);
  squad.SetMode(ACR_EMoveMode.COMBAT);
  squad.Defend(spot);
  cmd.Say(string.Format("[ACR] AI %1: screen %2 %3 %4 m out from %5, watching the %6 (sees %7/%8 of the way out)", cmd.m_sFaction, squad.GetName(), what, Math.Round(vector.DistanceXZ(spot, post.m_Base.GetOrigin())), post.m_Base.m_sName, ACR_DefenceForecast.CompassOf(bearing), sees, ACR_DefenceScreen.SAMPLES));
 }

 // Back to the base it screened, as one of its defenders (AssignDefenders lets it go when nothing comes).
 static void PullBackScreen(ACR_AICommander cmd, ACR_ScreenPost post, string why, float now)
 {
  ACR_Squad squad = post.m_Squad;
  cmd.m_ScreenAgain.Set(post.m_Base, now + ACR_AICommander.SCREEN_AGAIN_MS);
  squad.SetMode(ACR_EMoveMode.FAST);
  cmd.Travel(squad, post.m_Base.GetOrigin(), true, 0);
  // Pulled back with the enemy close or men lost: a fall-back, so BBB bounds it back in covered halves
  // (as FallBack does; it was left for after the covered fall-back merge, #232).
  if (ACR_Squad.CoveredFallBackOn()) squad.MarkFallBack(ACR_Manager.Ground(post.m_Base.GetOrigin()));
  squad.SetAITask(ACR_EAITask.DEFEND);
  squad.m_AITarget = post.m_Base;
  cmd.Say(string.Format("[ACR] AI %1: screen %2 pulls back to %3 (%4)", cmd.m_sFaction, squad.GetName(), post.m_Base.m_sName, why));
 }

 // Tests (forecastdefend): the defence part of a strategic tick (threat count, forecast, defenders, screens), nothing else.
 static void DefenceTickForTest(ACR_AICommander cmd)
 {
  cmd.GatherIntel();
  cmd.Assess();
  cmd.UpdateForecast();
  cmd.AssignDefenders();
  ManageScreens(cmd);
 }

 static void ThinkScreens(ACR_AICommander cmd, notnull array<string> lines)
 {
  if (cmd.m_Screens.IsEmpty()) return;
  lines.Insert("Screens:");
  foreach (ACR_ScreenPost post : cmd.m_Screens)
   if (post.m_Squad && post.m_Base) lines.Insert(string.Format("  %1 at %2, watching the %3 (%4 m out, sees %5/%6)", post.m_Squad.GetName(), post.m_Base.m_sName, ACR_DefenceForecast.CompassOf(post.m_fBearing), Math.Round(vector.DistanceXZ(post.m_vSpot, post.m_Base.GetOrigin())), post.m_iSees, ACR_DefenceScreen.SAMPLES));
 }

 static void PositionTransports(ACR_AICommander cmd)
 {
  if (!cmd.m_Manager.m_Rides) return;
  array<ACR_Base> hubs = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != cmd.m_sFaction || base.m_bContested || cmd.ThreatTo(base) > 0) continue;
   bool hasFoot = false;
   foreach (ACR_Squad foot : cmd.m_Manager.m_Squads)
   {
    if (!foot.GetFaction() || foot.GetFaction().GetFactionKey() != cmd.m_sFaction || foot.PlayerSteers()) continue;
    if (foot.IsMounted() || ACR_Transport.CrewedVehicle(foot) || foot.m_bMortar || foot.Aircraft()) continue;
    if (foot.m_iAITask != ACR_EAITask.FREE && foot.m_iAITask != ACR_EAITask.POSTED) continue;
    if (vector.DistanceXZ(cmd.PositionOf(foot), base.GetOrigin()) <= base.m_fRadius + 150) { hasFoot = true; break; }
   }
   if (hasFoot || base == cmd.m_ReserveBase || (ACR_LogisticsRules.CanDeploy(base) && cmd.FrontDistance(base) >= ACR_AICommander.FOB_DEPLOY_MIN_FRONT))
    hubs.Insert(base);
  }
  if (hubs.IsEmpty()) return;
  // Who is parked at each hub already (trucks on their way count once there).
  array<ACR_Squad> taxis = {};
  array<int> parked = {};
  foreach (ACR_Base countHub : hubs) parked.Insert(0);
  foreach (ACR_Squad idle : cmd.m_Manager.m_Squads)
  {
   if (!cmd.m_Manager.m_Rides.IsFreeTransport(idle, cmd.m_sFaction)) continue;
   if (cmd.m_bHandedOnly && !idle.m_bAICommanded) continue; // a player commands the side: his trucks stay where he parked them
   taxis.Insert(idle);
   if (idle.OrderState() == ACR_EOrderState.RETURN) continue; // on the move: counted once it has parked
   int at = HubAt(hubs, ACR_Transport.CrewedVehicle(idle).GetOrigin());
   if (at >= 0) parked[at] = parked[at] + 1;
  }
  foreach (ACR_Squad taxi : taxis)
  {
   if (taxi.OrderState() == ACR_EOrderState.RETURN) continue;
   // Gave up stuck lately: sending it again just repeats that (6 Nov: one Ural re-sent 39 times).
   if (GetGame().GetWorld().GetWorldTime() - taxi.m_fStuckAt < 600000) continue;
   vector here = ACR_Transport.CrewedVehicle(taxi).GetOrigin();
   int home = HubAt(hubs, here);
   if (home >= 0 && parked[home] <= ACR_AICommander.MOTOR_POOL_PER_HUB) continue; // parked, and the hub isn't crowded
   // The nearest hub with room; with none, the nearest at all (a truck already at a hub stays).
   int pickHub = -1;
   int anyHub = -1;
   float roomDist = float.MAX;
   float anyDist = float.MAX;
   foreach (int h, ACR_Base hub : hubs)
   {
    if (h == home) continue;
    float d = vector.DistanceXZ(here, hub.GetOrigin());
    if (d < anyDist) { anyDist = d; anyHub = h; }
    if (parked[h] < ACR_AICommander.MOTOR_POOL_PER_HUB && d < roomDist) { roomDist = d; pickHub = h; }
   }
   if (pickHub < 0)
   {
    if (home >= 0) continue;
    pickHub = anyHub;
   }
   if (pickHub < 0) continue;
   if (home >= 0) parked[home] = parked[home] - 1;
   parked[pickHub] = parked[pickHub] + 1;
   ACR_Base bestHub = hubs[pickHub];
   float bestDist = vector.DistanceXZ(here, bestHub.GetOrigin());
   taxi.SetMode(ACR_EMoveMode.FAST);
   taxi.RouteTo(ACR_Manager.Ground(bestHub.GetOrigin()), ACR_Squad.WP_MOVE, -1, "Returning", ACR_EOrderKind.MOVE);
   taxi.SetOrder("Returning", ACR_EOrderState.RETURN);
   cmd.Say(string.Format("[ACR] AI %1: motor pool %2 repositioning to %3 (%4 m)", cmd.m_sFaction, taxi.GetName(), bestHub.m_sName, (int)bestDist));
  }
 }

 // Index of the hub 'pos' is parked at (within MOTOR_POOL_PARKED of its centre, nearest first); -1: none.
 static int HubAt(notnull array<ACR_Base> hubs, vector pos)
 {
  int found = -1;
  float best = ACR_AICommander.MOTOR_POOL_PARKED;
  foreach (int i, ACR_Base hub : hubs)
  {
   float d = vector.DistanceXZ(pos, hub.GetOrigin());
   if (d <= best) { best = d; found = i; }
  }
  return found;
 }
}
