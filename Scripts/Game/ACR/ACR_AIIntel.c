// AI commander intel assessment, threat forecasting, recon flights, and fire-support calling; extracted from ACR_AICommander.c.
class ACR_AIIntel
{
 static int SupportIndex(ACR_AICommander cmd, ACR_ESupportType type)
 {
  foreach (int i, ACR_SupportEntry entry : cmd.m_Manager.m_Bases.GetFaction(cmd.m_sFaction).m_aSupports)
   if (entry.m_eType == type) return i;
  return -1;
 }

 // RP kept back from unit purchases so the fire missions it plans are always affordable:
 // the doctrine's dearest few (mortars; with m_bAirBudget the off-map air and rocket strikes
 // too) plus the usual reserve, capped by the doctrine. (Before 24 Oct pass 2 only the dearest
 // mortar counted: 25 + 15 RP, under every doctrine's cap, so the cap never changed anything.)
 // A rich side (FireFactor > 1) keeps RP for one more mission, counts air strikes whatever its
 // doctrine, and its cap rises with the factor.
 static int SupportBudget(ACR_AICommander cmd)
 {
  float factor = cmd.FireFactor();
  bool rich = factor > 1;
  array<int> costs = {};
  foreach (ACR_SupportEntry entry : cmd.m_Manager.m_Bases.GetFaction(cmd.m_sFaction).m_aSupports)
  {
   if (!entry.m_bEnabled) continue;
   bool planned = entry.m_eType == ACR_ESupportType.MORTAR_HE_SMALL || entry.m_eType == ACR_ESupportType.MORTAR_HE_LARGE || entry.m_eType == ACR_ESupportType.MORTAR_SMOKE;
   if ((cmd.m_Doctrine.m_bAirBudget || rich) && (entry.m_eType == ACR_ESupportType.BOMB_RUN || entry.m_eType == ACR_ESupportType.ROCKET_STRIKE)) planned = true;
   if (planned && entry.m_iCost > 0) costs.Insert(entry.m_iCost);
  }
  if (costs.IsEmpty()) return 0;
  costs.Sort(true);
  int missions = cmd.m_Doctrine.m_iSupportMissions;
  if (rich) missions++;
  int budget = ACR_AICommander.SUPPORT_RESERVE;
  for (int i = 0; i < costs.Count() && i < missions; i++) budget += costs[i];
  int cap = factor * cmd.m_Doctrine.m_iSupportBudgetMax;
  return Math.Min(cap, budget);
 }

 static int SupportBudgetForTest(ACR_AICommander cmd) { return SupportBudget(cmd); }

 // Needing troops beats keeping fire support money: well under strength, or a
 // base threatened by more than it has defending it.
 static bool UrgentTroops(ACR_AICommander cmd, int combatUnits, int cap)
 {
  if (combatUnits < cap / 2) return true;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner == cmd.m_sFaction && ThreatTo(cmd, base) > DefenceOf(cmd, base)) return true;
  return false;
 }

 static bool SupportReady(ACR_AICommander cmd, ACR_ESupportType type)
 {
  int index = SupportIndex(cmd, type);
  if (index < 0 || !cmd.m_Manager.m_Supports) return false;
  ACR_FactionData data = cmd.m_Manager.m_Bases.GetFaction(cmd.m_sFaction);
  ACR_SupportEntry entry = data.m_aSupports[index];
  float now = GetGame().GetWorld().GetWorldTime();
  bool ready = now >= entry.m_fReadyAt;
  if (!ready && type == ACR_ESupportType.AIR_LIFT && ACR_AirLift.WaveSlotAvailable(cmd.m_sFaction, now)) ready = true;
  return ready && cmd.Purse(data) >= entry.CostFor(true) + ACR_AICommander.SUPPORT_RESERVE;
 }

 // Own soldiers (squads and players) within radius: never call fire on them.
 static bool FriendliesNear(ACR_AICommander cmd, vector point, float radius)
 {
  foreach (ACR_Squad squad : cmd.m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != cmd.m_sFaction) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (soldier && vector.DistanceXZ(soldier.GetOrigin(), point) < radius) return true;
   }
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (faction && body && faction.GetFactionKey() == cmd.m_sFaction && vector.DistanceXZ(body.GetOrigin(), point) < radius) return true;
  }
  return false;
 }

 static bool CallSupport(ACR_AICommander cmd, ACR_ESupportType type, vector point, string why)
 {
  // Commanding squads a player handed over (2IC), it spends nothing of the side's RP: that is the
  // player's to spend (MISSION-MAKING, -acr2IC; as CanBuild).
  if (cmd.m_bHandedOnly && !cmd.m_bTwoICAll) return false; // the 2IC with a player in command: the side's to run, down to the player's keep (Purse)
  if (!SupportReady(cmd, type)) return false;
  if (ACR_Mortars.IsMortarType(type) && !cmd.m_Manager.Mortars().CanFire(cmd.m_sFaction, type, point)) return false;
  if (ACR_HeliStation.IsHeliType(type) && ACR_HeliStation.UsesPads(cmd.m_sFaction) && (!cmd.m_Manager.m_Supports || !cmd.m_Manager.m_Supports.Station().ReadyFor(cmd.m_sFaction, ACR_HeliStation.NeedsGunship(type), point, ACR_HeliStation.IsAttack(type)))) return false;
  if (type != ACR_ESupportType.RECON_FLIGHT && FriendliesNear(cmd, point, ACR_Supports.ZoneRadius(type) + 40)) return false;
  // COIN (#244 item 5): weigh the town before firing on it (ACR_TownSupport.FireRestraint).
  string restraint = ACR_TownSupport.FireRestraint(cmd.m_sFaction, type, point);
  if (!restraint.IsEmpty())
  {
   cmd.Say(string.Format("[ACR] AI %1 holds fire (%2): %3", cmd.m_sFaction, why, restraint));
   return false;
  }
  string result = cmd.m_Manager.m_Supports.Call(cmd.m_sFaction, SupportIndex(cmd, type), point, true);
  cmd.Say(string.Format("[ACR] AI %1 support (%2): %3", cmd.m_sFaction, why, result));
  // Call refuses some missions after SupportReady passed (no bomb-run prefab, no helicopter,
  // a pad's refusal) without spending or starting the cooldown. Reporting those as fired
  // stopped every fallback chain on the broken entry (no prep fire, no defensive fire).
  ACR_SupportEntry entry = cmd.m_Manager.m_Bases.GetFaction(cmd.m_sFaction).m_aSupports[SupportIndex(cmd, type)];
  return entry.m_iCooldown <= 0 || entry.m_fReadyAt > GetGame().GetWorld().GetWorldTime();
 }

 static void Note(ACR_AICommander cmd, IEntity enemy, float now)
 {
  if (!enemy || !ACR_Respawn.IsAlive(enemy)) return;
  cmd.m_SeenAt.Set(enemy, enemy.GetOrigin());
  cmd.m_SeenTime.Set(enemy, now);
 }

 // Tests: a sighting at a given spot, forgetting one, and the assessment on demand.
 static void NoteAtForTest(ACR_AICommander cmd, IEntity enemy, vector at, float now) { cmd.m_SeenAt.Set(enemy, at); cmd.m_SeenTime.Set(enemy, now); }

 static void ForgetForTest(ACR_AICommander cmd, IEntity enemy) { cmd.m_SeenAt.Remove(enemy); cmd.m_SeenTime.Remove(enemy); }

 static void AssessForTest(ACR_AICommander cmd) { Assess(cmd); }

 static string FactionKeyOf(ACR_AICommander cmd, IEntity body)
 {
  if (!body) return "";
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(body.FindComponent(FactionAffiliationComponent));
  if (!affiliation || !affiliation.GetAffiliatedFaction()) return "";
  return affiliation.GetAffiliatedFaction().GetFactionKey();
 }

 // Every second: the side's shared picture (ACR_Intel, gathered by the manager
 // from our squads, sentries and players).
 static void GatherIntel(ACR_AICommander cmd)
 {
  ACR_Intel intel = cmd.m_Manager.IntelFor(cmd.m_sFaction);
  if (!intel) return;
  cmd.m_SeenAt = intel.m_SeenAt;
  cmd.m_SeenTime = intel.m_SeenTime;
 }

 // Where we last saw any member of this enemy squad (false: we haven't).
 static bool KnownPosition(ACR_AICommander cmd, ACR_Squad squad, out vector position)
 {
  float newest = -1;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   float when;
   if (!body || !cmd.m_SeenTime.Find(body, when) || when <= newest) continue;
   newest = when;
   position = cmd.m_SeenAt.Get(body);
  }
  return newest >= 0;
 }

 // Known enemy squad nearest to a point (for defensive fires): aimed at where it was last seen.
 static ACR_Squad EnemyNear(ACR_AICommander cmd, vector point, float radius, out vector seenAt)
 {
  ACR_Squad best;
  float bestDistance = radius;
  foreach (ACR_Squad squad : cmd.m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == cmd.m_sFaction) continue;
   vector known;
   if (!KnownPosition(cmd, squad, known)) continue;
   float distance = vector.DistanceXZ(known, point);
   if (distance < bestDistance) { bestDistance = distance; best = squad; seenAt = known; }
  }
  return best;
 }

 static void Assess(ACR_AICommander cmd)
 {
  cmd.m_Defence.Clear();
  cmd.m_Threat.Clear();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   int defence, threat;
   vector center = base.GetOrigin();
   // Our own squads we know exactly; enemy squads only where last seen.
   bool ours = base.m_sOwner == cmd.m_sFaction;
   foreach (ACR_Squad squad : cmd.m_Manager.m_Squads)
   {
    if (!squad.GetFaction() || !squad.GetAnchor()) continue;
    bool mine = squad.GetFaction().GetFactionKey() == cmd.m_sFaction;
    vector where = cmd.PositionOf(squad);
    if (!mine && !KnownPosition(cmd, squad, where)) continue;
    float distance = vector.DistanceXZ(where, center);
    bool friendly = squad.GetFaction().GetFactionKey() == base.m_sOwner;
    // A screen out front isn't the base's defence until it comes back (forecastdefend: one 330 m out made a base
    // look held, and nobody was sent).
    if (friendly && distance < ACR_AICommander.DEFENSE_DISTANCE && !cmd.IsScreen(squad)) defence += squad.Strength();
    if (!friendly && distance < ACR_AICommander.THREAT_DISTANCE + base.m_fRadius) threat += squad.Strength();
   }
   // Player bodies not in any squad (command units) count too.
   array<int> players = {};
   GetGame().GetPlayerManager().GetPlayers(players);
   foreach (int id : players)
   {
    IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
    Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
    if (!body || !faction || cmd.m_Manager.FindSquadBySoldier(body)) continue;
    vector bodyAt = body.GetOrigin();
    if (faction.GetFactionKey() != cmd.m_sFaction)
    {
     if (!cmd.m_SeenAt.Contains(body)) continue;
     bodyAt = cmd.m_SeenAt.Get(body);
    }
    float d = vector.DistanceXZ(bodyAt, center);
    if (faction.GetFactionKey() == base.m_sOwner && d < ACR_AICommander.DEFENSE_DISTANCE) defence += ACR_AICommander.PLAYER_STRENGTH;
    if (faction.GetFactionKey() != base.m_sOwner && d < ACR_AICommander.THREAT_DISTANCE + base.m_fRadius) threat += ACR_AICommander.PLAYER_STRENGTH;
   }
   if (!base.m_sOwner.IsEmpty()) defence += base.m_iGarrisonLeft;
   if (!base.m_sOwner.IsEmpty()) defence += cmd.FortifiedCredit(base);
   if (ours) defence += ACR_BaseWorks.Credit(base);
   // Fog of war: expect more than we've seen, unless a recon flight photographed it lately
   // (then what it saw is the count: ACR_ReconRun).
   if (!ours && !base.m_sOwner.IsEmpty() && !Photographed(cmd, base)) defence += ACR_AICommander.UNKNOWN_DEFENCE;
   cmd.m_Defence.Set(base, defence);
   cmd.m_Threat.Set(base, threat);
   string before;
   if (cmd.m_LastOwner.Find(base, before) && before == cmd.m_sFaction && base.m_sOwner != cmd.m_sFaction && !base.m_sOwner.IsEmpty())
   {
    // Hit back while the enemy is still thin on the ground.
    cmd.m_CounterTarget = base;
    if (cmd.m_Forecast) cmd.m_Forecast.NoteLost(base, GetGame().GetWorld().GetWorldTime());
    cmd.m_fCounterUntil = GetGame().GetWorld().GetWorldTime() + cmd.m_Doctrine.m_fCounterMinutes * 60000;
    cmd.Say(string.Format("[ACR] AI %1: lost %2, planning a counterattack", cmd.m_sFaction, base.m_sName));
   }
   cmd.m_LastOwner.Set(base, base.m_sOwner);
  }
 }

 static bool Photographed(ACR_AICommander cmd, ACR_Base base)
 {
  ACR_Intel intel = cmd.m_Manager.IntelFor(cmd.m_sFaction);
  return intel && intel.PhotographedRecently(base, GetGame().GetWorld().GetWorldTime());
 }

 static void ManageRecon(ACR_AICommander cmd)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  // Not in the opening minutes: its RP goes on its first squads, and nothing has moved yet.
  if (cmd.m_fNextRecon <= 0) cmd.m_fNextRecon = now + ACR_AICommander.RECON_FIRST_MS;
  if (now < cmd.m_fNextRecon || !SupportReady(cmd, ACR_ESupportType.RECON_FLIGHT)) return;
  string why;
  vector point = ReconTarget(cmd, why);
  if (point == vector.Zero) return;
  if (!CallSupport(cmd, ACR_ESupportType.RECON_FLIGHT, point, why)) return;
  cmd.m_fNextRecon = now + ACR_AICommander.RECON_GAP_MS / Math.Max(0.5, cmd.m_Doctrine.m_fObserverWeight);
 }

 static vector ReconTarget(ACR_AICommander cmd, out string why)
 {
  ACR_Intel intel = cmd.m_Manager.IntelFor(cmd.m_sFaction);
  if (!intel) return vector.Zero;
  float now = GetGame().GetWorld().GetWorldTime();
  ACR_Intercept freshest;
  foreach (ACR_Intercept ring : intel.m_Intercepts)
  {
   if (now - ring.m_fHeard > ACR_AICommander.INTERCEPT_FRESH_MS || (freshest && ring.m_fHeard <= freshest.m_fHeard)) continue;
   ACR_Base mine = cmd.NearestOwnBase(ring.m_vCentre);
   if (!mine || vector.DistanceXZ(mine.GetOrigin(), ring.m_vCentre) > ACR_AICommander.INTERCEPT_NEAR) continue;
   freshest = ring;
  }
  if (freshest)
  {
   why = "recon over a radio intercept " + intel.Place(freshest.m_vCentre);
   return freshest.m_vCentre;
  }
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner.IsEmpty() || base.m_sOwner == cmd.m_sFaction || !base.IsVisibleTo(cmd.m_sFaction)) continue;
   if (ACR_Insurgency.Friendly(base.m_sOwner, cmd.m_sFaction) || intel.PhotographedRecently(base, now)) continue;
   ACR_Base own = cmd.NearestOwnBase(base.GetOrigin());
   if (!own) continue;
   float d = vector.DistanceXZ(own.GetOrigin(), base.GetOrigin());
   if (d < bestDistance) { bestDistance = d; best = base; }
  }
  if (!best) return vector.Zero;
  why = "recon over " + best.m_sName;
  return best.GetOrigin();
 }

 static int DefenceOf(ACR_AICommander cmd, ACR_Base base) { int v; cmd.m_Defence.Find(base, v); return v; }

 static ACR_DefenceForecast Forecast(ACR_AICommander cmd) { return cmd.m_Forecast; }

 static void UpdateForecast(ACR_AICommander cmd)
 {
  if (!ACR_DefenceForecast.On()) return;
  ACR_Intel intel = cmd.m_Manager.IntelFor(cmd.m_sFaction);
  if (!intel) return;
  if (!cmd.m_Forecast) cmd.m_Forecast = new ACR_DefenceForecast(cmd.m_sFaction);
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_Base> ours = {};
  array<int> defence = {};
  array<ACR_Base> enemy = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   if (base.m_sOwner == cmd.m_sFaction)
   {
    ours.Insert(base);
    defence.Insert(DefenceOf(cmd, base));
    // The check against what came uses last tick's forecast, so it runs first.
    vector from;
    bool seen = cmd.ThreatFrom(base, from);
    cmd.m_Forecast.NoteThreat(base, ThreatTo(cmd, base), seen, from, now);
   }
   else if (cmd.Visible(base) && cmd.HostileOwner(base)) enemy.Insert(base);
  }
  string believed;
  if (intel.Doctrine()) believed = intel.Doctrine().Believed();
  ACR_AIDoctrine theirs = ACR_AIDoctrine.Find(believed);
  if (!theirs) theirs = ACR_AIDoctrine.Find("balanced");
  float laneWidth = cmd.m_Doctrine.m_fLaneWidth;
  if (theirs) laneWidth = theirs.m_fLaneWidth;
  cmd.m_Forecast.Update(intel.m_Contacts, ours, defence, enemy, laneWidth, believed, now);
  foreach (string line : cmd.m_Forecast.m_aSaid) cmd.Say(string.Format("[ACR] AI %1: %2", cmd.m_sFaction, line));
  cmd.m_Forecast.m_aSaid.Clear();
 }

 static void ThinkForecast(ACR_AICommander cmd, notnull array<string> lines)
 {
  if (cmd.m_Forecast && ACR_DefenceForecast.On()) cmd.m_Forecast.Think(lines);
  cmd.ThinkScreens(lines);
 }

 static int ThreatTo(ACR_AICommander cmd, ACR_Base base) { int v; cmd.m_Threat.Find(base, v); return v; }

 // What a base of ours is defended against: the enemies at it, or (-acrDefenceForecast 2, defence
 // step 2) the attack forecast on it within lead time, whichever is more. Defenders are sent, kept and
 // released by this, so a firm forecast calls them minutes before the attackers reach the threat ring,
 // and one that fades (a feint, a squad that turned away) lets them go by the same rule.
 static int DefenceThreat(ACR_AICommander cmd, ACR_Base base)
 {
  int threat = ThreatTo(cmd, base);
  // A player's side (squads handed to the AI, the 2IC): the player commander decides, with the forecast
  // as a 2IC advisement (ACR_TwoICForecast) whose Accept is their own order.
  if (!cmd.m_Forecast || !ACR_DefenceForecast.Acts() || cmd.m_bHandedOnly) return threat;
  return (int)Math.Max(threat, cmd.m_Forecast.Demand(base));
 }

 // The 2IC on a player's side (ACR_TwoICForecast): this side's forecast, refreshed. False: off.
 static bool ForecastForTwoIC(ACR_AICommander cmd)
 {
  if (!ACR_DefenceForecast.Acts()) return false;
  Assess(cmd);
  UpdateForecast(cmd);
  return cmd.m_Forecast != null;
 }

 // The squad the 2IC would send to hold 'pos' (as it answers a Hold request), null if none in reach.
 static ACR_Squad TwoICDefender(ACR_AICommander cmd, vector pos)
 {
  bool handed = cmd.m_bHandedOnly;
  cmd.m_bHandedOnly = true;
  array<ACR_Squad> mine = {};
  cmd.MySquads(mine, false);
  ACR_Squad squad = cmd.RequestSquad(mine, pos);
  cmd.m_bHandedOnly = handed;
  if (!squad || vector.DistanceXZ(cmd.PositionOf(squad), pos) > ACR_AICommander.REQUEST_REACH) return null;
  return squad;
 }

 // Strength of our squads sent to defend 'base' that aren't there yet (outside DEFENSE_DISTANCE).
 static int EnRouteTo(ACR_AICommander cmd, ACR_Base base)
 {
  int strength;
  foreach (ACR_Squad squad : cmd.m_Manager.m_Squads)
  {
   if (!squad || squad.m_iAITask != ACR_EAITask.DEFEND || squad.m_AITarget != base || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != cmd.m_sFaction) continue;
   if (vector.DistanceXZ(cmd.PositionOf(squad), base.GetOrigin()) >= ACR_AICommander.DEFENSE_DISTANCE) strength += squad.Strength();
  }
  return strength;
 }
}
