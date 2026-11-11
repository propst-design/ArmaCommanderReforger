// Counter-battery (8 Nov 2026): a mortar that fires gives itself away.
//  - Sound ranging: every round a tube fires may be heard by each hostile side, likelier the nearer
//    that side's men are to the tube (HEAR_NEAR_CHANCE within HEAR_NEAR, fading to nothing at
//    HEAR_FAR), and with at least SHELLED_CHANCE for a side whose men the rounds land near.
//  - A side that hears a tube gets an estimate of where it stands: a ring centred off the real spot
//    (never on it), FIX_START wide at the first round and narrower with every round heard, down to
//    FIX_MIN. It shows on that side's map ("~Enemy mortar") and in its intel feed ("heard", then
//    "pinned down" once the ring is FIX_TIGHT or less). It is the spot the tube stood on: a team
//    that moves away leaves it behind, and it is forgotten FIX_FORGET_MS after the last round heard.
//  - Mortar teams that fire at will (every AI-led side's) shell a fresh estimate that is tight
//    enough (ACR_FireMissions.FreeFire) instead of the newest contact: always when the ring is
//    FIX_TIGHT or less, otherwise weighted against the contact (ACR_Variety, seeded). The rounds are
//    spread over the ring. Players see the ring and decide for themselves.
//  - An AI commander whose teams leave a pinned-down ring alone for CB_PAID_WAIT_MS (none in reach,
//    or all busy) pays for counter-battery: its mortar support when a team reaches it, otherwise the
//    howitzer battery if it can spare it (rich, or a doctrine that fires more: CB_BATTERY_EAGER).
//  - Shoot-and-scoot: an AI commander's mortar team that counter-battery rounds land near packs up
//    and moves DISPLACE_MIN..DISPLACE_MAX away from where they came from (ACR_AICommander
//    PositionMortars), so the estimate the other side built goes stale.
// -acrCounterBattery 0: no sound ranging, so none of this. Server only.
class ACR_CBFix : Managed
{
 string m_sKey;        // the firing team: side and team name
 string m_sSide;       // its side
 vector m_vTrue;       // where its tube stood (bookkeeping, never shown)
 vector m_vCentre;     // the estimate the listening side sees
 float m_fRadius;
 float m_fFirst;       // world time (ms) first heard there
 float m_fHeard;       // ... last heard
 int m_iRounds;        // rounds heard from there
 float m_fShelledAt;   // when this side's counter-battery last fired on it (ms, 0 never)
 bool m_bToldTight;    // the feed said "pinned down" already
 float m_fPinnedAt;    // ...when (world ms)
}

class ACR_CounterBattery
{
 static const float FIX_START = 300;        // m: the first round heard puts the tube within this
 static const float FIX_TIGHTEN = 0.78;     // each further round heard: the ring this much narrower
 static const float FIX_MIN = 35;
 static const float FIX_TIGHT = 100;        // at or under this: "pinned down"; shelled without weighing
 static const float FIX_SAME_M = 80;        // a tube this close to a fix's spot is heard as the same spot
 static const float FIX_FORGET_MS = 600000;
 static const int MAX_FIXES = 6;            // per listening side
 static const float CENTRE_NEAR = 0.25;     // the ring's centre lies this share of its radius off the tube...
 static const float CENTRE_FAR = 0.6;       // ...at least, and at most this
 static const float HEAR_NEAR = 1200;       // a listener's man this close to the tube: HEAR_NEAR_CHANCE a round
 static const float HEAR_FAR = 3500;        // ...fading to nothing out here
 static const float HEAR_NEAR_CHANCE = 0.6;
 static const float SHELLED_M = 250;        // rounds landing this close to a listener's man...
 static const float SHELLED_CHANCE = 0.45;  // ...give at least this
 static const float CB_FRESH_MS = 120000;   // counter-battery fires on a fix heard this recently
 static const float CB_MAX_RADIUS = 160;    // ...at most this wide
 static const float CB_AGAIN_MS = 60000;    // ...and not on the same fix again before this
 static const float CB_SPREAD = 0.7;        // its rounds land within this share of the ring
 static const int CB_ROUNDS = 3;
 static const float INCOMING_NEAR = 120;    // counter-battery aimed this close to a team's tube counts as on it
 static const float INCOMING_MS = 90000;    // the team reacts within this of the rounds landing
 static const float DISPLACE_MIN = 150;
 static const float DISPLACE_MAX = 300;
 static const float CB_PAID_WAIT_MS = 20000; // an AI commander pays for counter-battery on a ring pinned this long that its teams left alone
 static const float MORTAR_LANDS_MS = 30000;  // a paid mortar mission's first rounds land about then (its call says so)
 static const float BATTERY_LANDS_MS = 90000; // ...the battery's ("ETA about 90 s")
 static const float CB_BATTERY_EAGER = 1.4;  // ...and calls the battery when its fire factor over its doctrine's fire-mission gap is this or more

 static int s_iHeard;              // tests
 static int s_iCounterFires;       // tests
 static vector s_vLastAim;         // tests: where the last counter-battery mission aimed
 static int s_iPaidFires;          // tests: counter-battery an AI commander paid for
 static float s_fForceChance = -1; // tests: a fixed chance for a listener in hearing range (-1: worked out)

 static bool On() { return ACR_LogisticsRules.Choice("acrCounterBattery", "1") != "0"; }

 // ---------------------------------------------------------------- hearing

 // A round left the team's tube (ACR_Mortars.PrepareGunner): each hostile side may hear it.
 static void Fired(ACR_MortarTeam team)
 {
  if (!team || !team.m_Tube || !On()) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  vector tube = team.m_Tube.GetOrigin();
  string key = team.m_sFaction + ":" + team.m_sName;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All)
  {
   string listener = settings.m_sFaction;
   if (listener.IsEmpty() || listener == team.m_sFaction || ACR_Insurgency.Friendly(listener, team.m_sFaction)) continue;
   ACR_Intel intel = manager.IntelFor(listener);
   if (!intel) continue;
   float chance = HearChance(manager, listener, tube, team.m_vAim);
   if (chance <= 0 || Math.RandomFloat01() > chance) continue;
   Heard(intel, team.m_sFaction, key, tube);
  }
 }

 // The chance a side hears one round: from its man nearest the tube, and from its man nearest to
 // where the round is aimed (0: nobody of that side within hearing).
 static float HearChance(ACR_Manager manager, string listener, vector tube, vector aim)
 {
  float nearTube = float.MAX;
  float nearAim = float.MAX;
  NearestMen(manager, listener, tube, aim, nearTube, nearAim);
  float chance = 0;
  if (nearTube <= HEAR_NEAR) chance = HEAR_NEAR_CHANCE;
  else if (nearTube < HEAR_FAR) chance = HEAR_NEAR_CHANCE * (HEAR_FAR - nearTube) / (HEAR_FAR - HEAR_NEAR);
  if (aim != vector.Zero && nearAim <= SHELLED_M) chance = Math.Max(chance, SHELLED_CHANCE);
  if (chance > 0 && s_fForceChance >= 0) chance = s_fForceChance;
  return chance;
 }

 static void NearestMen(ACR_Manager manager, string side, vector tube, vector aim, out float nearTube, out float nearAim)
 {
  nearTube = float.MAX;
  nearAim = float.MAX;
  array<AIAgent> agents = {};
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != side) continue;
   agents.Clear();
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body || !ACR_Respawn.IsAlive(body)) continue;
    nearTube = Math.Min(nearTube, vector.DistanceXZ(body.GetOrigin(), tube));
    if (aim != vector.Zero) nearAim = Math.Min(nearAim, vector.DistanceXZ(body.GetOrigin(), aim));
   }
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (!player || ACR_Intel.FactionKeyOf(player) != side || !ACR_Respawn.IsAlive(player)) continue;
   nearTube = Math.Min(nearTube, vector.DistanceXZ(player.GetOrigin(), tube));
   if (aim != vector.Zero) nearAim = Math.Min(nearAim, vector.DistanceXZ(player.GetOrigin(), aim));
  }
 }

 // One round heard from 'tube' (fired by 'key' of side 'side'): a new estimate, or the one for that
 // spot narrowed and moved. Its centre is drawn afresh each round, a quarter to three fifths of the
 // ring off the tube, so the ring always holds the tube and its middle is never on it.
 static ACR_CBFix Heard(ACR_Intel intel, string side, string key, vector tube)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  Forget(intel, now);
  s_iHeard++;
  ACR_CBFix fix;
  foreach (ACR_CBFix known : intel.m_CBFixes)
  {
   if (known.m_sKey == key && vector.DistanceXZ(known.m_vTrue, tube) < FIX_SAME_M) fix = known;
  }
  bool fresh = !fix;
  if (fresh)
  {
   fix = new ACR_CBFix();
   fix.m_sKey = key;
   fix.m_sSide = side;
   fix.m_vTrue = tube;
   fix.m_fRadius = FIX_START;
   fix.m_fFirst = now;
   intel.m_CBFixes.Insert(fix);
   while (intel.m_CBFixes.Count() > MAX_FIXES) intel.m_CBFixes.RemoveOrdered(0);
  }
  else
  {
   fix.m_fRadius = Math.Max(FIX_MIN, fix.m_fRadius * FIX_TIGHTEN);
  }
  fix.m_iRounds++;
  fix.m_fHeard = now;
  RandomGenerator rng = ACR_Variety.For(string.Format("cb %1 %2 %3", key, fix.m_iRounds, Math.Round(now / 1000)));
  float angle = rng.RandFloat01() * Math.PI2;
  float off = fix.m_fRadius * (CENTRE_NEAR + (CENTRE_FAR - CENTRE_NEAR) * rng.RandFloat01());
  vector centre = tube + Vector(Math.Cos(angle) * off, 0, Math.Sin(angle) * off);
  centre[1] = GetGame().GetWorld().GetSurfaceY(centre[0], centre[2]);
  fix.m_vCentre = centre;
  int within = Math.Round(fix.m_fRadius);
  PrintFormat("[ACR] COUNTERBATTERY %1 heard %2 (%3 round(s)): ring %4 m at %5, the tube %6 m off its middle", intel.m_sFaction, key, fix.m_iRounds, within, centre, Math.Round(vector.DistanceXZ(centre, tube)));
  ACR_Manager manager = ACR_Manager.GetInstance();
  string line;
  if (fresh) line = string.Format("Enemy mortar heard firing %1 (within %2 m)", intel.Place(centre), within);
  else if (!fix.m_bToldTight && fix.m_fRadius <= FIX_TIGHT)
  {
   fix.m_bToldTight = true;
   fix.m_fPinnedAt = now;
   line = string.Format("Enemy mortar pinned down %1 (within %2 m)", intel.Place(centre), within);
  }
  if (!line.IsEmpty())
  {
   intel.AddFeed(now, line, centre);
   if (manager) manager.NotifyFaction(intel.m_sFaction, line);
  }
  return fix;
 }

 static void Forget(ACR_Intel intel, float now)
 {
  for (int i = intel.m_CBFixes.Count() - 1; i >= 0; i--)
  {
   if (now - intel.m_CBFixes[i].m_fHeard > FIX_FORGET_MS) intel.m_CBFixes.RemoveOrdered(i);
  }
 }

 // Pale rings on the side's map (the "~" label draws a range ring, not a danger area).
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Intel intel = manager.IntelFor(faction);
  if (!intel) return;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_CBFix fix : intel.m_CBFixes)
  {
   if (now - fix.m_fHeard > FIX_FORGET_MS) continue;
   positions.Insert(fix.m_vCentre);
   radii.Insert(fix.m_fRadius);
   labels.Insert("~Enemy mortar");
  }
 }

 // ---------------------------------------------------------------- answering

 // The estimate a team of this side should shell now (null: none): heard within CB_FRESH_MS, at most
 // CB_MAX_RADIUS wide, not shelled by us within CB_AGAIN_MS; of those the one heard most often (the
 // likeliest still to be there), then the freshest.
 static ACR_CBFix Target(ACR_Intel intel, float nowMs)
 {
  if (!intel || !On()) return null;
  ACR_CBFix best;
  foreach (ACR_CBFix fix : intel.m_CBFixes)
  {
   if (nowMs - fix.m_fHeard > CB_FRESH_MS || fix.m_fRadius > CB_MAX_RADIUS) continue;
   if (fix.m_fShelledAt > 0 && nowMs - fix.m_fShelledAt < CB_AGAIN_MS) continue;
   if (best && (fix.m_iRounds < best.m_iRounds || (fix.m_iRounds == best.m_iRounds && fix.m_fHeard <= best.m_fHeard))) continue;
   best = fix;
  }
  return best;
 }

 // Shell the estimate rather than the contact? Always when it is tight; otherwise weighed (the
 // estimate a little ahead), seeded per team and estimate, so the same match makes the same calls.
 static bool Prefer(ACR_CBFix fix, bool contactToo, string key)
 {
  if (!fix) return false;
  if (!contactToo || fix.m_fRadius <= FIX_TIGHT) return true;
  array<float> scores = {1.0, 0.95};
  return ACR_Variety.Pick(scores, key) == 0;
 }

 // Where one of our counter-battery missions aims: within CB_SPREAD of the ring (seeded).
 static vector AimAt(ACR_CBFix fix, string key)
 {
  RandomGenerator rng = ACR_Variety.For(key);
  float angle = rng.RandFloat01() * Math.PI2;
  float off = fix.m_fRadius * CB_SPREAD * Math.Sqrt(rng.RandFloat01());
  return ACR_Manager.Ground(fix.m_vCentre + Vector(Math.Cos(angle) * off, 0, Math.Sin(angle) * off));
 }

 // Our counter-battery mission on 'fix' is under way from 'from': the enemy team it was heard from,
 // if its tube still stands within INCOMING_NEAR of where the rounds go, learns of it when they land.
 static void Shelled(ACR_CBFix fix, vector aim, vector from, float landsAtMs)
 {
  fix.m_fShelledAt = GetGame().GetWorld().GetWorldTime();
  s_iCounterFires++;
  s_vLastAim = aim;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  array<ACR_MortarTeam> teams = {};
  manager.Mortars().TeamsOf(fix.m_sSide, teams);
  foreach (ACR_MortarTeam team : teams)
  {
   if (team.m_sFaction + ":" + team.m_sName != fix.m_sKey || !team.m_Tube) continue;
   if (vector.DistanceXZ(team.m_Tube.GetOrigin(), aim) > INCOMING_NEAR) continue;
   team.m_fIncomingAt = landsAtMs;
   team.m_vIncomingFrom = from;
   // Its side's players hear it coming in (an AI commander's team moves by itself: DisplaceUnderFire).
   if (team.m_Squad && (team.m_Squad.HasPlayer() || team.m_Squad.m_bClaimed || team.m_Squad.m_Commander != 0))
    manager.NotifyFaction(team.m_sFaction, string.Format("Counter-battery fire coming in on %1: move it", team.m_sName));
  }
 }

 // Counter-battery rounds have landed near this team's tube lately (and from where they came).
 static bool UnderFire(ACR_MortarTeam team, float nowMs, out vector from)
 {
  from = vector.Zero;
  if (!team || team.m_fIncomingAt <= 0 || nowMs < team.m_fIncomingAt || nowMs - team.m_fIncomingAt > INCOMING_MS) return false;
  from = team.m_vIncomingFrom;
  return true;
 }

 // Where a team under counter-battery fire moves to: DISPLACE_MIN..DISPLACE_MAX away from where the
 // rounds came from, turned up to 60 degrees either way (seeded), on ground a tube can stand on when
 // there is some near.
 static vector DisplaceSpot(ACR_MortarTeam team, vector from)
 {
  vector here = team.m_Spot;
  if (team.m_Tube) here = team.m_Tube.GetOrigin();
  RandomGenerator rng = ACR_Variety.For(string.Format("cb displace %1 %2", team.m_sName, Math.Round(team.m_fIncomingAt / 1000)));
  vector away = here - from;
  away[1] = 0;
  float turn = (rng.RandFloat01() * 2 - 1) * 60 * Math.DEG2RAD;
  // Fired from off the map (the battery): any way will do.
  if (from == vector.Zero || away.Length() < 1)
  {
   away = Vector(1, 0, 0);
   turn = rng.RandFloat01() * Math.PI2;
  }
  away = away.Normalized();
  float distance = DISPLACE_MIN + (DISPLACE_MAX - DISPLACE_MIN) * rng.RandFloat01();
  float c = Math.Cos(turn);
  float s = Math.Sin(turn);
  vector dir = Vector(away[0] * c - away[2] * s, 0, away[0] * s + away[2] * c);
  vector goal = ACR_Manager.Ground(here + dir * distance);
  vector spot;
  if (ACR_Mortars.FindSpot(goal, spot)) return spot;
  return goal;
 }
}
