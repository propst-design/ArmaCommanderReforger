// -acrCounterBatteryTest 1 (server-capable, ~4 min, the test range): counter-battery (ACR_CounterBattery).
//  1-6, bookkeeping on the West side's intel (no units): the first round heard gives a 300 m ring that
//     holds the tube off its middle; each round narrows it (never under 35 m) and it keeps holding the
//     tube; a tube heard from elsewhere is a new ring; the rings show on the hearing side's map only;
//     a ring shelled lately or too wide isn't a target; rings are forgotten.
//  7-10, live: an East and a West mortar team set up 0.8-1.2 km apart (hearing forced to certain while
//     in range); East fires eight rounds, West hears it and gets a tight ring holding the East tube,
//     West's team (fire at will) answers with counter-battery fire on the ring, and the East team,
//     under it, is sent 150-300 m away from where the rounds came from.
//  11, -acrCounterBattery 0: a round fired is heard by nobody, and no ring is a target.
//  12, East's AI commander pays for counter-battery on a pinned ring of West's tube its own teams left
//     alone, not before they had CB_PAID_WAIT_MS: the battery, since its mortar is on the move.
class ACR_TestCounterBattery
{
 static const int EXPECTED = 12;
 static int s_iStage;
 static int s_iStageTick;
 static ACR_MortarTeam s_West;
 static ACR_MortarTeam s_East;
 static vector s_vWest;
 static vector s_vEast;
 static int s_iHeardBefore;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  if (tick == 1)
  {
   foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
   ACR_Mortars.DEPLOY_SECONDS = 10;
   ACR_Variety.s_bOff = true;
   ACR_CounterBattery.s_fForceChance = 1;
  }
  if (tick == 2) Bookkeeping(manager, west, east);
  if (tick == 4) SpawnTeams(manager, west, east);
  if (tick > 4) Live(manager, west, east, tick);
 }

 static vector Somewhere(ACR_Manager manager, string west)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == west) return ACR_Manager.Ground(base.GetOrigin() + Vector(60, 0, 60));
  }
  return ACR_Manager.Ground(Vector(1000, 0, 1000));
 }

 static void Bookkeeping(ACR_Manager manager, string west, string east)
 {
  ACR_Intel intel = manager.IntelFor(west);
  ACR_Intel theirs = manager.IntelFor(east);
  if (!intel || !theirs)
  {
   ACR_TestRegress.Check("counterbattery: both sides have intel", false, "");
   return;
  }
  intel.m_CBFixes.Clear();
  vector tube = Somewhere(manager, west);
  string key = east + ":Test tube";
  ACR_CBFix first = ACR_CounterBattery.Heard(intel, east, key, tube);
  float off = vector.DistanceXZ(first.m_vCentre, tube);
  bool firstOk = first.m_fRadius == ACR_CounterBattery.FIX_START && off >= first.m_fRadius * ACR_CounterBattery.CENTRE_NEAR - 1 && off <= first.m_fRadius * ACR_CounterBattery.CENTRE_FAR + 1;
  ACR_TestRegress.Check("counterbattery: the first round heard gives a 300 m ring holding the tube, off its middle", firstOk, string.Format("ring %1 m, the tube %2 m off its middle", first.m_fRadius, off));

  bool same = true;
  bool holds = true;
  for (int i = 0; i < 7; i++)
  {
   ACR_CBFix again = ACR_CounterBattery.Heard(intel, east, key, tube + Vector(i * 3, 0, 0));
   if (again != first) same = false;
   if (vector.DistanceXZ(again.m_vCentre, tube) > again.m_fRadius + 1) holds = false;
  }
  float afterEight = first.m_fRadius;
  for (int j = 0; j < 12; j++) ACR_CounterBattery.Heard(intel, east, key, tube);
  float afterTwenty = first.m_fRadius;
  ACR_TestRegress.Check("counterbattery: each round narrows the ring, never under 35 m, and it keeps holding the tube", same && holds && afterEight < 60 && afterEight >= ACR_CounterBattery.FIX_MIN && afterTwenty == ACR_CounterBattery.FIX_MIN, string.Format("same ring %1, holds %2, after 8 rounds %3 m, after 20 %4 m", same, holds, afterEight, afterTwenty));

  ACR_CBFix moved = ACR_CounterBattery.Heard(intel, east, key, tube + Vector(220, 0, 0));
  ACR_TestRegress.Check("counterbattery: a tube heard from 220 m away is a new ring, the old one stays", moved != first && intel.m_CBFixes.Count() == 2, string.Format("%1 rings", intel.m_CBFixes.Count()));

  array<vector> pos = {};
  array<float> radii = {};
  array<string> labels = {};
  ACR_CounterBattery.Zones(west, pos, radii, labels);
  int rings;
  foreach (string label : labels) if (label == "~Enemy mortar") rings++;
  array<vector> pos2 = {};
  array<float> radii2 = {};
  array<string> labels2 = {};
  ACR_CounterBattery.Zones(east, pos2, radii2, labels2);
  ACR_TestRegress.Check("counterbattery: the rings show on the hearing side's map, not the firing side's", rings == 2 && labels2.IsEmpty(), string.Format("%1 on ours, %2 on theirs", rings, labels2.Count()));

  float now = GetGame().GetWorld().GetWorldTime();
  ACR_CBFix target = ACR_CounterBattery.Target(intel, now);
  first.m_fShelledAt = now;
  ACR_CBFix afterShelling = ACR_CounterBattery.Target(intel, now);
  first.m_fShelledAt = 0;
  ACR_TestRegress.Check("counterbattery: the tight ring is the target; once shelled it waits a minute, and a 300 m ring is too wide", target == first && afterShelling == null, string.Format("target is the tight ring %1, after shelling %2", target == first, afterShelling != null));

  foreach (ACR_CBFix fix : intel.m_CBFixes) fix.m_fHeard = now - ACR_CounterBattery.FIX_FORGET_MS - 1000;
  ACR_CounterBattery.Forget(intel, now);
  ACR_TestRegress.Check("counterbattery: rings are forgotten 10 minutes after the last round heard", intel.m_CBFixes.IsEmpty(), string.Format("%1 left", intel.m_CBFixes.Count()));
 }

 // West's team on its base, East's 0.8-1.2 km off on dry ground a tube can stand on.
 static void SpawnTeams(ACR_Manager manager, string west, string east)
 {
  s_vWest = Somewhere(manager, west);
  vector spot;
  if (ACR_Mortars.FindSpot(s_vWest, spot)) s_vWest = spot;
  s_vEast = vector.Zero;
  for (int k = 0; k < 24 && s_vEast == vector.Zero; k++)
  {
   float a = k * 0.2618;
   int band = k - (k / 3) * 3;
   float d = 800 + band * 200;
   vector candidate = ACR_Manager.Ground(s_vWest + Vector(Math.Cos(a) * d, 0, Math.Sin(a) * d));
   if (BBB_Terrain.IsWaterAt(candidate)) continue;
   if (ACR_Mortars.FindSpot(candidate, spot)) s_vEast = spot;
  }
  bool westOk = manager.Mortars().SpawnTeam(west, "CB West mortar", s_vWest);
  bool eastOk = s_vEast != vector.Zero && manager.Mortars().SpawnTeam(east, "CB East mortar", s_vEast);
  PrintFormat("[ACR] CBTEST teams: West %1 at %2, East %3 at %4 (%5 m apart)", westOk, s_vWest, eastOk, s_vEast, Math.Round(vector.DistanceXZ(s_vWest, s_vEast)));
  s_iStage = 1;
  s_iStageTick = 4;
 }

 static ACR_MortarTeam TeamOf(ACR_Manager manager, string side)
 {
  array<ACR_MortarTeam> teams = {};
  manager.Mortars().TeamsOf(side, teams);
  if (teams.IsEmpty()) return null;
  return teams[0];
 }

 static void Next(int tick)
 {
  s_iStage++;
  s_iStageTick = tick;
 }

 static void Live(ACR_Manager manager, string west, string east, int tick)
 {
  if (s_iStage == 0 || s_iStage >= 9) return;
  int waited = tick - s_iStageTick;
  if (!s_West) s_West = TeamOf(manager, west);
  if (!s_East) s_East = TeamOf(manager, east);
  if (tick % 10 == 0)
  {
   string w = "none";
   string e = "none";
   if (s_West) w = s_West.m_sStatus;
   if (s_East) e = s_East.m_sStatus;
   PrintFormat("[ACR] CBTEST t=%1 stage %2 | West %3 | East %4 | heard %5 counter-fires %6", tick, s_iStage, w, e, ACR_CounterBattery.s_iHeard, ACR_CounterBattery.s_iCounterFires);
  }
  // 1: both set up.
  if (s_iStage == 1)
  {
   bool up = s_West && s_East && s_West.m_eState == ACR_EMortarState.DEPLOYED && s_East.m_eState == ACR_EMortarState.DEPLOYED;
   if (up)
   {
    if (s_West.m_Squad) s_West.m_Squad.m_bFireAtWill = true;
    if (s_East.m_Squad) s_East.m_Squad.m_bFireAtWill = false;
    ACR_Intel intel = manager.IntelFor(west);
    if (intel) intel.m_CBFixes.Clear();
    Next(tick);
   }
   else if (waited > 120) { FailLive("the two mortar teams never set up"); return; }
  }
  // 2: East fires eight rounds at a spot 350 m beyond West's tube (away from East).
  if (s_iStage == 2)
  {
   vector dir = s_vWest - s_vEast;
   dir[1] = 0;
   vector aim = ACR_Manager.Ground(s_vWest + dir.Normalized() * 350);
   string message;
   int eta;
   bool fired = manager.Mortars().Fire(east, ACR_ESupportType.MORTAR_HE_LARGE, aim, message, eta);
   PrintFormat("[ACR] CBTEST East mission: %1 (%2)", fired, message);
   if (!fired && waited > 30) { FailLive("East's team wouldn't fire: " + message); return; }
   if (fired) Next(tick);
  }
  // 3: West has a tight ring holding East's tube.
  if (s_iStage == 3)
  {
   ACR_Intel westIntel = manager.IntelFor(west);
   ACR_CBFix fix;
   if (westIntel) foreach (ACR_CBFix f : westIntel.m_CBFixes) if (f.m_sSide == east) fix = f;
   if (fix && fix.m_iRounds >= 4 && s_East.m_Tube)
   {
    float off = vector.DistanceXZ(fix.m_vCentre, s_East.m_Tube.GetOrigin());
    ACR_TestRegress.Check("counterbattery: West hears East's rounds and gets a ring holding East's tube", off <= fix.m_fRadius + 1 && fix.m_fRadius < ACR_CounterBattery.FIX_START, string.Format("%1 rounds heard, ring %2 m, the tube %3 m off its middle", fix.m_iRounds, Math.Round(fix.m_fRadius), Math.Round(off)));
    Next(tick);
   }
   else if (waited > 120) { FailLive("West heard too little of East's mission"); return; }
  }
  // 4: West's team answers with counter-battery fire.
  if (s_iStage == 4)
  {
   if (ACR_CounterBattery.s_iCounterFires > 0)
   {
    float aimOff = vector.DistanceXZ(ACR_CounterBattery.s_vLastAim, s_vEast);
    ACR_TestRegress.Check("counterbattery: West's team (fire at will) shells the ring", aimOff < 200, string.Format("aimed %1 m from East's tube", Math.Round(aimOff)));
    Next(tick);
   }
   else if (waited > 120) { FailLive("West's team never fired counter-battery"); return; }
  }
  // 5: once the rounds land, East's team is under fire and is sent away from West.
  if (s_iStage == 5)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   vector from;
   // A team still firing its own mission finishes it first (DisplaceUnderFire), as a crew would.
   if (ACR_CounterBattery.UnderFire(s_East, now, from) && !s_East.m_bFiring)
   {
    ACR_AICommander ai = manager.EnsureAICommander(east);
    vector before = s_East.m_Spot;
    ai.DisplaceUnderFireForTest(now);
    vector goal = vector.Zero;
    if (s_East.m_Squad) goal = s_East.m_Squad.RouteDestination();
    float moved = vector.DistanceXZ(goal, before);
    bool away = vector.DistanceXZ(goal, from) > vector.DistanceXZ(before, from);
    ACR_TestRegress.Check("counterbattery: East's team under fire knows where it came from", vector.DistanceXZ(from, s_vWest) < 40, string.Format("from %1 m off West's tube", Math.Round(vector.DistanceXZ(from, s_vWest))));
    ACR_TestRegress.Check("counterbattery: and is sent 150-300 m away from it", moved >= 120 && moved <= 340 && away, string.Format("sent %1 m, away from the fire %2", Math.Round(moved), away));
    Next(tick);
   }
   else if (waited > 90) { FailLive("East's team never came under the counter-battery fire (aimed too far off, or never landed)"); return; }
  }
  // 6: off.
  if (s_iStage == 6)
  {
   ACR_Options.Set("acrCounterBattery", "0");
   s_iHeardBefore = ACR_CounterBattery.s_iHeard;
   for (int i = 0; i < 5; i++) ACR_CounterBattery.Fired(s_East);
   ACR_Intel w = manager.IntelFor(west);
   ACR_CBFix still;
   if (w) still = ACR_CounterBattery.Target(w, GetGame().GetWorld().GetWorldTime());
   bool quiet = ACR_CounterBattery.s_iHeard == s_iHeardBefore && !still;
   ACR_Options.Unset("acrCounterBattery");
   ACR_TestRegress.Check("counterbattery: -acrCounterBattery 0: nobody hears a round, no ring is a target", quiet, string.Format("heard %1 more, target %2", ACR_CounterBattery.s_iHeard - s_iHeardBefore, still != null));
   Next(tick);
   return;
  }
  // 7: paid counter-battery by East's AI commander on West's tube.
  if (s_iStage == 7)
  {
   ACR_AICommander payer = manager.EnsureAICommander(east);
   ACR_Intel eastIntel = manager.IntelFor(east);
   if (!payer || !eastIntel || !s_West || !s_West.m_Tube) { FailLive("no East commander or West tube for the paid counter-battery"); return; }
   if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
   manager.m_Bases.GetFaction(east).m_iRP = 1000;
   eastIntel.m_CBFixes.Clear();
   string ringKey = west + ":" + s_West.m_sName;
   ACR_CBFix ring;
   for (int r = 0; r < 8; r++) ring = ACR_CounterBattery.Heard(eastIntel, west, ringKey, s_West.m_Tube.GetOrigin());
   int paidBefore = ACR_CounterBattery.s_iPaidFires;
   payer.CounterBatterySupportForTest(payer.FireFactor());
   bool heldBack = ACR_CounterBattery.s_iPaidFires == paidBefore;
   ring.m_fPinnedAt = GetGame().GetWorld().GetWorldTime() - ACR_CounterBattery.CB_PAID_WAIT_MS - 1000;
   payer.CounterBatterySupportForTest(payer.FireFactor());
   bool paid = ACR_CounterBattery.s_iPaidFires == paidBefore + 1 && ring.m_fShelledAt > 0;
   ACR_TestRegress.Check("counterbattery: an AI commander pays for counter-battery on a pinned ring its teams left alone, after giving them their go", heldBack && paid && ring.m_fPinnedAt > 0, string.Format("pinned ring %1 m, waited first %2, paid after %3 (fire factor %4)", Math.Round(ring.m_fRadius), heldBack, paid, payer.FireFactor()));
   Finish();
  }
 }

 static void FailLive(string why)
 {
  PrintFormat("[ACR] CBTEST live part stopped: %1", why);
  ACR_TestRegress.Check("counterbattery: live duel: " + why, false, "");
  Finish();
 }

 static void Finish()
 {
  s_iStage = 9;
  ACR_CounterBattery.s_fForceChance = -1;
  ACR_Variety.s_bOff = false;
  ACR_TestRegress.Complete("counterbattery", EXPECTED);
 }
}
