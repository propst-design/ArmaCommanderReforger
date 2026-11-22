// The province (COIN, 0.8.0 #244 item 3): how the counter-insurgency stands, and the insurgency's floor.
//  - The province line: Pacified / Simmering / Insurgency / Uprising, from the share of the towns'
//    value that is Restless or Hostile (ACR_TownSupport), plus FOUND_WEIGHT for each live hideout the
//    army has found (the network it knows is part of the picture). Information only, never an end
//    screen: the army's command line reads " | Province: X" (ACR_MapFeed) and a change gets a feed
//    line. A level is left only HYSTERESIS past its bound.
//  - Uprising: the insurgency at its strongest may spread into UPRISING_SPREADS more towns than its
//    cap, always as visible pockets (ACR_FIAGrowth.Spread): ground the army can see and attack.
//  - The floor: the insurgency can't be wiped out. With no hideout left for FLOOR_MS (a long quiet
//    spell), one is raised, small (FLOOR_MEN), among the buildings near the least secure town (the
//    highest support with none of the army within FLOOR_CLEAR), and it grows from there as any other.
//    The army isn't told: finding it is its job.
class ACR_Province
{
 static const int PACIFIED = 0;
 static const int SIMMERING = 1;
 static const int INSURGENCY = 2;
 static const int UPRISING = 3;
 static const float SIMMER_AT = 0.15;     // share of town value Restless or Hostile
 static const float INSURGENCY_AT = 0.4;
 static const float UPRISING_AT = 0.65;
 static const float HYSTERESIS = 0.03;
 static const float FOUND_WEIGHT = 0.05;  // added for each live hideout the army knows
 static const int UPRISING_SPREADS = 2;
 static const float FLOOR_MS = 1200000;   // 20 min with no hideout: one is raised
 static const int FLOOR_MEN = 3;
 static const float FLOOR_CLEAR = 1000;   // m: no army man this close to the town it is raised near
 protected static int s_iLevel;
 protected static float s_fShare;
 protected static float s_fNoHideoutSince;
 static float s_fFloorScaleForTest = 1;   // tests shorten the quiet spell
 static int s_iRaised;                    // tests: hideouts raised by the floor
 static ACR_Base s_FloorTown;             // tests: the town the last one was raised near

 static void Reset()
 {
  s_iLevel = -1;
  s_fShare = 0;
  s_fNoHideoutSince = -1;
  s_fFloorScaleForTest = 1;
  s_iRaised = 0;
  s_FloorTown = null;
 }

 static int Level() { return s_iLevel; }
 static float Share() { return s_fShare; }

 static string LevelName(int level)
 {
  if (level == PACIFIED) return "Pacified";
  if (level == SIMMERING) return "Simmering";
  if (level == INSURGENCY) return "Insurgency";
  if (level == UPRISING) return "Uprising";
  return "unknown";
 }

 // " | Province: Simmering" on the army's command line (ACR_MapFeed); "" for anyone else.
 static string Tag(string side)
 {
  if (!ACR_TownSupport.On() || s_iLevel < 0 || !ACR_Coin.IsArmy(side)) return "";
  return " | Province: " + LevelName(s_iLevel);
 }

 // Server, from ACR_TownSupport.Tick (every TICK_S).
 static void Tick(float now)
 {
  Measure(now);
  Floor(now);
 }

 // ---------------------------------------------------------------- the line

 static float MeasureShare()
 {
  float turned = 0;
  float total = 0;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!ACR_TownSupport.IsTown(base) || base.m_iSupportTier < 0) continue;
   float value = Math.Max(1, base.m_iValue);
   total += value;
   if (base.m_iSupportTier >= ACR_TownSupport.RESTLESS) turned += value;
  }
  float share = 0;
  if (total > 0) share = turned / total;
  string army = ACR_Coin.Army();
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  foreach (ACR_DefenderPost hideout : hideouts) if (hideout.m_Cell.m_aKnownBy.Contains(army)) share += FOUND_WEIGHT;
  return Math.Min(1, share);
 }

 static int LevelOf(float share, int previous)
 {
  int level = UPRISING;
  if (share < SIMMER_AT) level = PACIFIED;
  else if (share < INSURGENCY_AT) level = SIMMERING;
  else if (share < UPRISING_AT) level = INSURGENCY;
  if (previous < 0 || level == previous) return level;
  array<float> bounds = {0, SIMMER_AT, INSURGENCY_AT, UPRISING_AT, 2};
  if (level < previous && share > bounds[previous] - HYSTERESIS) return previous;
  if (level > previous && share < bounds[previous + 1] + HYSTERESIS) return previous;
  return level;
 }

 protected static void Measure(float now)
 {
  s_fShare = MeasureShare();
  int level = LevelOf(s_fShare, s_iLevel);
  if (level == s_iLevel) return;
  int old = s_iLevel;
  s_iLevel = level;
  PrintFormat("[ACR] PROVINCE %1 -> %2 (%3 of the towns' value turned or known FIA)", LevelName(old), LevelName(level), s_fShare);
  if (old < 0) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Intel intel = manager.IntelFor(ACR_Coin.Army());
  if (!intel) return;
  string how = "the towns are turning against us";
  if (level < old) how = "the towns are coming round";
  intel.AddFeed(now, string.Format("Province: %1 (%2)", LevelName(level), how));
 }

 // ---------------------------------------------------------------- the floor

 protected static void Floor(float now)
 {
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  if (!hideouts.IsEmpty() || ACR_Guerrillas.WantedCells() <= 0)
  {
   s_fNoHideoutSince = -1;
   return;
  }
  if (s_fNoHideoutSince < 0) { s_fNoHideoutSince = now; return; }
  if (now - s_fNoHideoutSince < FLOOR_MS / Math.Max(1, s_fFloorScaleForTest)) return;
  ACR_Base town = LeastSecure();
  if (!town) return;
  vector site;
  if (!ACR_Guerrillas.HideoutNear(town.GetOrigin(), site)) return;
  ACR_DefenderPost post = ACR_Guerrillas.AddHideout(site);
  post.m_iReserve = Math.Min(post.m_iReserve, FLOOR_MEN);
  s_fNoHideoutSince = -1;
  s_iRaised++;
  s_FloorTown = town;
  PrintFormat("[ACR] PROVINCE floor: no hideout for a long spell, %1 raised near %2 (%3 men, support %4)", post.m_sName, town.m_sName, post.m_iReserve, Math.Round(town.m_fSupport));
 }

 // The town that backs the insurgency most with none of the army near it.
 static ACR_Base LeastSecure()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases) return null;
  string army = ACR_Coin.Army();
  ACR_Base best;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!ACR_TownSupport.IsTown(base) || base.m_fSupport < 0 || base.m_sOwner == army) continue;
   if (manager.m_Bases.MenWithin(base.GetOrigin(), FLOOR_CLEAR, army) > 0) continue;
   if (!best || base.m_fSupport > best.m_fSupport) best = base;
  }
  return best;
 }
}
