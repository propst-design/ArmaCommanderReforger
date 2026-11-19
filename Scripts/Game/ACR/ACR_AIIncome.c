// AI commander: income grabs when behind (Jacob, 19 Nov: "The US is rolling in dough too. The USSR
// AICOM should consider capping Pisy and Flikonya MB just for extra income." The USSR was broke at
// 6 RP while two empty income bases near its own sat untaken for over an hour: off its lane, too far
// behind its bases for the lane's one rear grab, and the one grab tried was a gun UAZ that parked
// 98 m short of Flikonya MB's ring for ten minutes).
// A side behind on income (its own against the enemy income it can see on the map) or short of RP
// raises the worth of income grabs: empty income bases, and lightly held enemy ones, nearer its own
// bases than the enemy's. The lane lets one more of them in off its axis. Doctrine weighs it
// (m_fIncomeGrab). Like the envelopment term it only reorders what the planners may already do:
// odds, caps, the lane's main effort and truce rules are unchanged. Pure here; ACR_TestLanes checks it.
class ACR_AIIncome
{
 static const float BEHIND_FROM = 0.15;  // behind when the enemy's known income is this share above ours
 static const int BROKE_RP = 50;         // RP below which the side counts as short of money
 static const float GAIN = 2;            // bonus at full urgency, Balanced doctrine
 static const float REACH = 3500;        // m from our nearest base an off-lane income grab may be
 static const int LIGHT_DEFENCE = 6;     // an enemy income base held by at most this (fog included) is a cheap take

 // 0..1: how badly the side needs income. 'own', 'enemy': income per tick (enemy as far as it is
 // known); 'rp': what it has now.
 static float Urgency(int own, int enemy, int rp)
 {
  float behind = 0;
  if (enemy > 0)
  {
   float ratio = own;
   ratio = ratio / enemy;
   behind = Math.Clamp(1 - ratio, 0, 1);
   if (behind < BEHIND_FROM) behind = 0;
  }
  float broke = 0;
  if (rp < BROKE_RP)
  {
   float left = Math.Max(0, rp);
   broke = 1 - left / BROKE_RP;
  }
  return Math.Max(behind, broke);
 }

 // The worth multiplier of an income grab at 'urgency' for a doctrine of 'weight'.
 static float Bonus(float urgency, float weight)
 {
  if (urgency <= 0 || weight <= 0) return 1;
  return 1 + GAIN * weight * Math.Clamp(urgency, 0, 1);
 }

 // An off-lane income base worth going for: 'ownGap' from our nearest base within REACH, and nearer
 // ours than the nearest known enemy base ('enemyGap', float.MAX: none known).
 static bool Reachable(float ownGap, float enemyGap)
 {
  return ownGap <= REACH && ownGap < enemyGap;
 }
}
