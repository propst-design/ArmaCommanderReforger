// Ammo runs (the user, 7 Nov 2026: "when a squad gets low on ammo they can request a ammo truck if
// safe or they will rtb to the closest base to refill on supplies"). The AI commander's side of it is
// ACR_AICommander.ManageAmmoRuns; this file holds a run and the arithmetic, apart so the tests can
// check it without a battle.
//
// Low: the squad's magazines against what its men started with (ACR_Squad.m_iAmmoPct) under its line,
// the doctrine's m_fAmmoLow (45 standard) varied up to LINE_SPREAD per squad (ACR_Variety, seeded), or
// a third of its men nearly dry (ACR_Squad.m_iDryPct: one machine gunner shot out hides in the
// average). Once on a run it stays on it until it is back to DONE_PCT or the refill is over.
// The choice, costed in seconds away from its job plus risk:
// - the truck: a free ammo or arsenal truck meets it on a road nearby, where that is safe;
// - a base: it walks (or rides) back to a quiet base of ours that can restock it;
// - carrying on: it fights on (picking magazines off the fallen, BBB_Rearm) and looks again later,
//   cheap just under the line, dear when it is empty.
// ACR_Variety picks among the near-best (Pick), so two matches don't play the same; -acrVariety 0, or
// tests (ACR_Variety.s_bOff), always take the cheapest. -acrAmmoRuns 0: the old rules (a free squad
// walks to the nearest ammo truck, one holding a position has it brought).
class ACR_AmmoRun : Managed
{
 ACR_Squad m_Crew;      // the truck crew coming to meet it (null: walking back to m_Base)
 ACR_Base m_Base;       // the base it walks back to
 vector m_vMeet;        // where it meets the truck, or the base
 float m_fStartedAt;
 float m_fGiveUpAt;     // not there by then: it looks again (the base is left out a while)
 float m_fArrivedAt;    // the truck stopped at the meeting point, or the squad inside the base (0: not yet)
 float m_fContactSince; // inside the base with enemies near (0: not)
 float m_fServedAt;     // restocked at the base (0: not yet)
 int m_iFromTask;       // its task before the run (ACR_EAITask)
 int m_iStartPct;       // its ammo when it set off
 bool m_bGathered;      // told to gather at the stopped truck
}

class ACR_AmmoRuns
{
 static const int DONE_PCT = 80;        // back to this, the run is over
 static const int DRY_PCT = 34;         // this share of its men nearly dry is low too
 static const float DRY_SHARE = 0.3;    // a man under this share of his starting magazines is nearly dry
 static const int ATTACK_MIN = 25;      // below this a squad isn't sent on a new attack (as ACR_Qrf.MIN_AMMO)
 static const float LINE_SPREAD = 0.15;
 static const float RUN_SHARE = 0.3;    // at most this share of the combat squads on runs at once (the shot dry go anyway)
 // Costs, in seconds away from its job.
 static const float FOOT_MPS = 1.6;
 static const float DRIVE_MPS = 7;
 static const float TRUCK_START_S = 30;
 static const float RIDE_WAIT_S = 120;
 static const float HOLDER_S = 300;     // a holder leaving its post for the walk back
 static const float MISSING_S = 60;     // each man the base can replace while it is there
 static const float WOUNDED_S = 60;     // ... and its wounded patched up
 static const float FRONT_S = 240;      // a meeting point or base at the enemy's door, at most
 static const float SHORT_STOCK_S = 120; // a truck that can top up only some of its men, at most
 static const float WAIT_MIN_S = 300;   // carrying on just under the line ...
 static const float WAIT_MAX_S = 1800;  // ... and empty
 static const float SCALE_S = 300;
 static const float TOPPED_MS = 600000; // a refill that couldn't bring it back: no new run for 10 min ...
 static const int TOPPED_DROP = 20;     // ... unless its ammo falls this much more

 static bool On() { return ACR_LogisticsRules.Choice("acrAmmoRuns", "1") != "0"; }

 // A squad's line: the doctrine's, varied per squad (the key names the side and squad).
 static float Line(float doctrineLine, string key)
 {
  return ACR_Variety.Jitter(doctrineLine, LINE_SPREAD, key);
 }

 // ammoPct -1: nothing to count (unarmed crews).
 static bool Low(int ammoPct, int dryPct, float line)
 {
  if (ammoPct < 0) return false;
  return line > ammoPct || dryPct >= DRY_PCT;
 }

 static bool Done(int ammoPct, int dryPct)
 {
  return ammoPct < 0 || (ammoPct >= DONE_PCT && dryPct < DRY_PCT);
 }

 // How far below its line: 0 at the line, 1 empty (a third of its men dry counts as halfway at least).
 static float Shortfall(int ammoPct, int dryPct, float line)
 {
  float shortfall = 0;
  if (line > 0 && ammoPct >= 0) shortfall = Math.Clamp((line - ammoPct) / line, 0, 1);
  if (dryPct >= DRY_PCT) shortfall = Math.Max(shortfall, 0.5);
  return shortfall;
 }

 // The truck: its drive out or the squad's walk to the meeting point (whichever is longer), the risk
 // there (0-1, FrontRisk) and a stock short of what the squad needs (stockShare: loads per man, 1 = enough).
 static float TruckCost(float truckDist, float walkDist, float frontRisk, float stockShare)
 {
  float cost = Math.Max(truckDist / DRIVE_MPS + TRUCK_START_S, walkDist / FOOT_MPS);
  cost += Math.Clamp(frontRisk, 0, 1) * FRONT_S;
  if (stockShare < 1) cost += (1 - Math.Clamp(stockShare, 0, 1)) * SHORT_STOCK_S;
  return cost;
 }

 // A base: there and back to its job (dist to the base's circle), on foot or by truck.
 static float BaseCost(float dist, bool ride, bool holder, int missing, bool wounded, float frontRisk)
 {
  float leg = dist / FOOT_MPS;
  if (ride) leg = Math.Min(leg, dist / DRIVE_MPS + RIDE_WAIT_S);
  float cost = 2 * leg + Math.Clamp(frontRisk, 0, 1) * FRONT_S;
  if (holder) cost += HOLDER_S;
  cost -= missing * MISSING_S;
  if (wounded) cost -= WOUNDED_S;
  return Math.Max(cost, 0);
 }

 static float WaitCost(float shortfall)
 {
  return WAIT_MIN_S + (WAIT_MAX_S - WAIT_MIN_S) * Math.Clamp(shortfall, 0, 1);
 }

 static float Score(float cost)
 {
  return 1000 / (SCALE_S + Math.Max(cost, 0));
 }

 // The option to take, from their costs (the cheapest is the best): ACR_Variety among the near-best.
 static int Pick(notnull array<float> costs, string key)
 {
  array<float> scores = {};
  foreach (float cost : costs) scores.Insert(Score(cost));
  return ACR_Variety.Pick(scores, key);
 }

 // A refill that couldn't bring it back still holds: within TOPPED_MS, its ammo not much lower since.
 static bool ToppedHolds(float now, float toppedAt, int toppedPct, int ammoPct)
 {
  if (now - toppedAt >= TOPPED_MS) return false;
  return ammoPct > toppedPct - TOPPED_DROP;
 }

 // 0-1: how near the enemy's nearest known base a point is (-1 = none known; 1.5 km or more: 0).
 static float FrontRisk(float enemyBaseDist)
 {
  if (enemyBaseDist < 0) return 0;
  return Math.Clamp(1 - (enemyBaseDist - 400) / 1100, 0, 1);
 }
}
