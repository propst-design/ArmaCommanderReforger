// -acrForecastDefendTest 1 (server-capable, ~4-6 min): defence step 2 end to end (-acrDefenceForecast 2;
// Jacob, 4 Nov: the screen into step 2, and scripted attacks with known ways so it is asserted, not read
// from soaks). As forecastattack, but nobody is spotted for us: our side's eyes are its own squads.
// A base of ours faces an enemy base across dry ground; five fire teams of ours stand by (one at the base,
// four 700 m behind it). Our commander's defence runs each strategic tick (threat count, forecast,
// defenders, screens; nothing else of it). Once its screen is out, two enemy fire teams walk at the base
// from the enemy's side, holding fire.
// Checks: a screen goes out toward the enemy's way; its sightings forecast the attack LEAD_LEAST s before
// the threat ring; defenders are sent before the ring; the screen pulls back to the base rather than stay
// out front; the self-check counts a hit.
class ACR_TestForecastDefend
{
 static const int CHECKS = 5;
 static const float START_PAST_RING = 400; // m outside the threat ring the attackers start
 static const float BEHIND = 700;          // m behind the base our four fire teams wait
 static const float LEAD_LEAST = 60;       // s of forecast wanted before the ring
 static const float SENT_LEAST = 20;       // s defenders are sent at least this long before the ring
 static const float WAY_SLACK = 45;        // degrees: the screen watches the enemy's way within this
 static const int SCREEN_WAIT = 200;       // s for the screen to get out before the attack starts anyway
 static const int GIVE_UP = 360;           // s after the attack starts with nobody at the ring
 static const int JUDGE_WAIT = 60;         // s past the ring for the self-check to judge it
 static const int DEFENCE_TICK = 5;        // s between the commander's defence ticks (a strategic tick is 15)

 static int s_iStep;
 static int s_iAttackTick = -1;
 static string s_sUs, s_sEnemy;
 static ACR_Base s_Base, s_Enemy;
 static vector s_vDir;
 static ref array<SCR_AIGroup> s_aAttackers;
 static ref array<SCR_AIGroup> s_aOurs;
 static ref map<ACR_Base, string> s_OwnerWas;
 static bool s_bEnemyHidden;
 static int s_iGarrisonWas;
 static ACR_AICommander s_Ai;
 static ACR_Squad s_Screen;
 static float s_fScreenBearing = -1;
 static float s_fNotedAt = -1;
 static float s_fSentAt = -1;
 static float s_fRingAt = -1;
 static bool s_bPulledBack;
 static bool s_bPulledHome;            // ... to defend the base, when it left the screen

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  // From the first second: left on, both commanders bought squads that took a third base and fought the
  // attack (fcast3-defend-a).
  if (s_iStep == 0)
  {
   foreach (ACR_FactionSettings off : ACR_FactionSettings.s_All) off.m_bAICommander = false;
  }
  if (s_iStep == 0 && tick >= 8) Setup(h, manager, tick);
  if (s_iStep == 1 || s_iStep == 2) Follow(h, manager, tick);
  if (s_iStep == 3) Finish(h, manager);
 }

 protected static void Setup(ACR_TestHarness h, ACR_Manager manager, int tick)
 {
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (!alpha || !alpha.GetFaction())
  {
   Abort(h, "no Alpha squad");
   return;
  }
  s_sUs = alpha.GetFaction().GetFactionKey();
  s_sEnemy = "USSR";
  if (s_sUs == "USSR") s_sEnemy = "US";
  // An empty range: nobody but the squads this test spawns (the range's own squads near the base would
  // count as its defence, and anything bought before the commanders went off joins the fight).
  foreach (ACR_Squad present : manager.m_Squads)
  {
   if (!present || !present.m_Group) continue;
   array<AIAgent> agents = {};
   present.m_Group.GetAgents(agents);
   foreach (AIAgent agent : agents) h.Kill(agent.GetControlledEntity());
  }
  s_Ai = manager.AICommanderFor(s_sUs);
  if (!s_Ai)
  {
   Abort(h, "no AI commander for " + s_sUs);
   return;
  }
  string why;
  if (!Pair(why))
  {
   Abort(h, "no pair of bases with dry ground between them: " + why);
   return;
  }
  ACR_Options.Set("acrDefenceForecast", "2");
  // Ours: the base; his: the enemy base; the rest empty (the forecast and screens read only these two).
  s_OwnerWas = new map<ACR_Base, string>();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   s_OwnerWas.Set(base, base.m_sOwner);
   if (base == s_Enemy) base.m_sOwner = s_sEnemy;
   else if (base == s_Base) base.m_sOwner = s_sUs;
   else base.m_sOwner = "";
  }
  // No stored garrison: the defence is the squads, so a firm forecast has to call some.
  s_iGarrisonWas = s_Base.m_iGarrisonLeft;
  s_Base.m_iGarrisonLeft = 0;
  s_bEnemyHidden = s_Enemy.m_bHidden;
  s_Enemy.m_bHidden = false;
  vector center = s_Base.GetOrigin();
  s_aOurs = {};
  ResourceName team = h.FireTeamOf(manager, s_sUs);
  vector back = ACR_Manager.Ground(center - s_vDir * BEHIND);
  vector across = Vector(-s_vDir[2], 0, s_vDir[0]);
  for (int i = 0; i < 5; i++)
  {
   vector at = back + across * ((i - 2) * 30);
   if (i == 0) at = center;
   SCR_AIGroup ours = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(team, ACR_Manager.Ground(at)));
   if (ours) s_aOurs.Insert(ours);
  }
  if (s_aOurs.Count() < 5)
  {
   Abort(h, "our fire teams didn't spawn");
   return;
  }
  s_iStep = 1;
  PrintFormat("[ACR] FORECAST DEFEND TEST base %1 (ring %2 m), enemy base %3 to the %4, %5 m off", s_Base.m_sName, Math.Round(Ring()), s_Enemy.m_sName, ACR_DefenceForecast.CompassOf(ACR_Intel.Bearing(center, s_Enemy.GetOrigin())), Math.Round(vector.DistanceXZ(center, s_Enemy.GetOrigin())));
 }

 static float Ring() { return ACR_AICommander.THREAT_DISTANCE + s_Base.m_fRadius; }

 // Two bases a lane's length apart with dry ground from the first out past its threat ring toward the
 // second, and behind it where our fire teams wait.
 protected static bool Pair(out string why)
 {
  foreach (ACR_Base a : ACR_Base.s_All)
  {
   if (!a || a.m_bRetired) continue;
   foreach (ACR_Base b : ACR_Base.s_All)
   {
    if (!b || b == a || b.m_bRetired || vector.DistanceXZ(a.GetOrigin(), b.GetOrigin()) < ACR_AILanes.MIN_LENGTH) continue;
    vector d = b.GetOrigin() - a.GetOrigin();
    d[1] = 0;
    d.Normalize();
    float ring = ACR_AICommander.THREAT_DISTANCE + a.m_fRadius;
    int wet = 0;
    for (float m = -BEHIND; m <= ring + START_PAST_RING + 20; m += 40)
     if (BBB_Terrain.IsWaterAt(ACR_Manager.Ground(a.GetOrigin() + d * m))) wet++;
    why += string.Format("%1-%2 %3 wet; ", a.m_sName, b.m_sName, wet);
    if (wet > 0) continue;
    s_Base = a;
    s_Enemy = b;
    s_vDir = d;
    return true;
   }
  }
  return false;
 }

 protected static void Follow(ACR_TestHarness h, ACR_Manager manager, int tick)
 {
  if (tick % DEFENCE_TICK == 0) s_Ai.DefenceTickForTest();
  ACR_ScreenPost post = s_Ai.ScreenOf(s_Base);
  if (post && post.m_Squad && !s_Screen)
  {
   s_Screen = post.m_Squad;
   s_fScreenBearing = post.m_fBearing;
   PrintFormat("[ACR] FORECAST DEFEND TEST t=%1: screen %2 out at %3, watching %4 deg", tick, s_Screen.GetName(), post.m_vSpot, Math.Round(post.m_fBearing));
  }
  if (s_iStep == 1)
  {
   bool there = post && post.m_Squad && post.m_Squad.GetAnchor() && vector.DistanceXZ(post.m_Squad.GetAnchor().GetOrigin(), post.m_vSpot) <= 40;
   if (there || tick >= SCREEN_WAIT) StartAttack(h, manager, tick);
   return;
  }
  vector center = s_Base.GetOrigin();
  float nearestGap = -1;
  foreach (SCR_AIGroup g : s_aAttackers)
  {
   if (!g) continue;
   float gap = vector.DistanceXZ(g.GetOrigin(), center);
   if (nearestGap < 0 || gap < nearestGap) nearestGap = gap;
  }
  bool inside = nearestGap >= 0 && Ring() >= nearestGap;
  ACR_DefenceForecast forecast = s_Ai.Forecast();
  if (forecast && s_fNotedAt < 0)
  {
   ACR_BaseForecast entry = forecast.Of(s_Base);
   if (entry && entry.m_fExpected >= ACR_DefenceForecast.NOTE) s_fNotedAt = tick;
  }
  if (s_fSentAt < 0)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (!squad || squad == s_Screen || squad.m_iAITask != ACR_EAITask.DEFEND || squad.m_AITarget != s_Base) continue;
    s_fSentAt = tick;
    PrintFormat("[ACR] FORECAST DEFEND TEST t=%1: %2 sent to defend %3, nearest attacker %4 m out", tick - s_iAttackTick, squad.GetName(), s_Base.m_sName, Math.Round(nearestGap));
    break;
   }
  }
  if (s_Screen && !s_bPulledBack && !s_Ai.IsScreen(s_Screen))
  {
   s_bPulledBack = true;
   s_bPulledHome = s_Screen.m_iAITask == ACR_EAITask.DEFEND && s_Screen.m_AITarget == s_Base;
   PrintFormat("[ACR] FORECAST DEFEND TEST t=%1: screen %2 off the screen (task %3), nearest attacker %4 m out", tick - s_iAttackTick, s_Screen.GetName(), typename.EnumToString(ACR_EAITask, s_Screen.m_iAITask), Math.Round(nearestGap));
  }
  if ((tick - s_iAttackTick) % 10 == 0) Picture(manager, tick, nearestGap);
  if (inside && s_fRingAt < 0) s_fRingAt = tick;
  // Past the ring: until the self-check has judged it (someone has to see them inside), or a minute.
  bool judged = forecast && forecast.m_iHits + forecast.m_iWrongWay + forecast.m_iUnforecast > 0;
  // The give-up clock is for an attack that never reaches the ring: one that does always gets its wait
  // (exp4 runs, 5 Nov: ring at t=216 was cut at t=240 before the self-check saw it inside).
  if (s_fRingAt >= 0 && (judged || tick - s_fRingAt >= JUDGE_WAIT)) s_iStep = 3;
  else if (s_fRingAt < 0 && tick - s_iAttackTick >= GIVE_UP) s_iStep = 3;
 }

 // What our side knows and forecasts, every 10 s (reading a failed run).
 protected static void Picture(ACR_Manager manager, int tick, float nearestGap)
 {
  ACR_Intel intel = manager.IntelFor(s_sUs);
  string known = "";
  if (intel)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   foreach (ACR_Contact contact : intel.m_Contacts)
   {
    if (!contact) continue;
    ACR_ForecastContact seen = ACR_DefenceForecast.FromContact(contact, now);
    known += string.Format(" [%1 m off, heading %2, speed %3, strength %4, %5 s old]", Math.Round(vector.DistanceXZ(seen.m_vPos, s_Base.GetOrigin())), Math.Round(seen.m_fHeading), seen.m_fSpeed, seen.m_fStrength, Math.Round(seen.m_fAgeS));
   }
  }
  string said = "none";
  ACR_DefenceForecast forecast = s_Ai.Forecast();
  if (forecast && forecast.Of(s_Base)) said = forecast.Of(s_Base).Line();
  PrintFormat("[ACR] FORECAST DEFEND TEST t=%1: nearest attacker %2 m out; contacts:%3; forecast %4", tick - s_iAttackTick, Math.Round(nearestGap), known, said);
 }

 protected static void StartAttack(ACR_TestHarness h, ACR_Manager manager, int tick)
 {
  vector center = s_Base.GetOrigin();
  vector start = ACR_Manager.Ground(center + s_vDir * (Ring() + START_PAST_RING));
  vector across = Vector(-s_vDir[2], 0, s_vDir[0]);
  s_aAttackers = {};
  ResourceName team = h.FireTeamOf(manager, s_sEnemy);
  for (int i = 0; i < 2; i++)
  {
   vector side = across * (i * 160);
   SCR_AIGroup attacker = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(team, ACR_Manager.Ground(start + side)));
   if (!attacker) continue;
   s_aAttackers.Insert(attacker);
   ACR_TestForecastAttack.MoveTo(attacker, center + side);
  }
  if (s_aAttackers.Count() < 2)
  {
   Abort(h, "enemy fire teams didn't spawn");
   return;
  }
  s_iAttackTick = tick;
  s_iStep = 2;
  PrintFormat("[ACR] FORECAST DEFEND TEST t=%1: attack starts %2 m out from the %3 (screen %4)", tick, Math.Round(Ring() + START_PAST_RING), ACR_DefenceForecast.CompassOf(ACR_Intel.Bearing(center, start)), s_Screen != null);
 }

 protected static void Finish(ACR_TestHarness h, ACR_Manager manager)
 {
  s_iStep = 4;
  vector center = s_Base.GetOrigin();
  float attackBearing = ACR_Intel.Bearing(center, center + s_vDir);
  bool screenRight = s_Screen && ACR_DefenceForecast.AngleBetween(s_fScreenBearing, attackBearing) <= WAY_SLACK;
  ACR_TestRegress.Check("forecastdefend: a screen goes out toward the enemy's way", screenRight, string.Format("screen %1, watching %2 deg, the enemy comes from %3 deg", s_Screen != null, Math.Round(s_fScreenBearing), Math.Round(attackBearing)));
  float lead = -1;
  if (s_fNotedAt >= 0 && s_fRingAt >= 0) lead = s_fRingAt - s_fNotedAt;
  ACR_TestRegress.Check("forecastdefend: its sightings forecast the attack before the threat ring", s_fRingAt >= 0 && lead >= LEAD_LEAST, string.Format("forecast at t=%1, ring at t=%2 (s after the attack started), lead %3 s", s_fNotedAt - s_iAttackTick, s_fRingAt - s_iAttackTick, lead));
  float sentLead = -1;
  if (s_fSentAt >= 0 && s_fRingAt >= 0) sentLead = s_fRingAt - s_fSentAt;
  ACR_TestRegress.Check("forecastdefend: defenders are sent before the attackers reach the ring", s_fRingAt >= 0 && sentLead >= SENT_LEAST, string.Format("sent at t=%1, ring at t=%2, %3 s ahead", s_fSentAt - s_iAttackTick, s_fRingAt - s_iAttackTick, sentLead));
  bool home = s_bPulledBack && s_bPulledHome;
  string task = "none";
  if (s_Screen) task = typename.EnumToString(ACR_EAITask, s_Screen.m_iAITask);
  ACR_TestRegress.Check("forecastdefend: the screen pulls back to defend the base rather than stay out front", home, string.Format("pulled back %1, to defend it %2, task now %3", s_bPulledBack, s_bPulledHome, task));
  ACR_DefenceForecast forecast = s_Ai.Forecast();
  int hits = -1;
  int wrong = -1;
  int unforecast = -1;
  if (forecast)
  {
   hits = forecast.m_iHits;
   wrong = forecast.m_iWrongWay;
   unforecast = forecast.m_iUnforecast;
  }
  ACR_TestRegress.Check("forecastdefend: the self-check counts it a hit", hits >= 1 && unforecast == 0, string.Format("hits %1, wrong way %2, not forecast %3", hits, wrong, unforecast));
  Restore(h);
  ACR_TestRegress.Complete("forecastdefend", CHECKS);
 }

 protected static void Restore(ACR_TestHarness h)
 {
  if (s_aAttackers)
  {
   foreach (SCR_AIGroup a : s_aAttackers) ACR_TestForecastAttack.Kill(h, a);
  }
  if (s_aOurs)
  {
   foreach (SCR_AIGroup o : s_aOurs) ACR_TestForecastAttack.Kill(h, o);
  }
  if (s_OwnerWas)
  {
   foreach (ACR_Base base, string owner : s_OwnerWas)
    if (base) base.m_sOwner = owner;
  }
  if (s_Enemy) s_Enemy.m_bHidden = s_bEnemyHidden;
  if (s_Base && s_OwnerWas) s_Base.m_iGarrisonLeft = s_iGarrisonWas;
  ACR_Options.Unset("acrDefenceForecast");
 }

 protected static void Abort(ACR_TestHarness h, string why)
 {
  s_iStep = 4;
  Restore(h);
  ACR_TestRegress.Abort("forecastdefend", why);
 }
}
