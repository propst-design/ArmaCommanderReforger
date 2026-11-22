// One living side (lifted out of TvZ for COIN, 0.8.0 #244): the match types where only one of the
// two selected sides plays and the other never shows up. Today that is TvZ (one side against the
// zombies, ACR_TvZ) and counter-insurgency (one army against the FIA insurgency, ACR_Coin). Both ask
// this class who plays and who is absent; neither keeps its own copy.
// The absent side: its bases start as open towns, its starting units and spawn points are dropped,
// it has no AI commander and isn't offered on the deploy screen; every player joins the living side.
// What becomes of its HQ is the mode's: the hive (TvZ), the insurgency's stronghold (COIN).
class ACR_OneSide
{
 protected static bool s_bApplied;
 protected static string s_sLiving;
 protected static string s_sAbsent;
 protected static ACR_Base s_Home;        // the living side's HQ
 protected static ACR_Base s_AbsentHQ;    // what was the absent side's HQ (now open)

 static void Reset()
 {
  s_bApplied = false;
  s_sLiving = "";
  s_sAbsent = "";
  s_Home = null;
  s_AbsentHQ = null;
 }

 // A one-side match is being played.
 static bool On()
 {
  return ACR_Coin.On() || ACR_TvZ.On();
 }

 // The side that plays. COIN: the army row (-acrCoinArmy west|east, ACR_Coin.ArmyOption); else
 // -acrPlayers if it names one of the pair ("west"/"east" too); else West.
 static string Living()
 {
  if (!s_sLiving.IsEmpty()) return s_sLiving;
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  string value;
  if (ACR_Coin.On()) value = ACR_Coin.ArmyOption();
  else if (!ACR_Options.Get("acrPlayers", value)) return west;
  value.TrimInPlace();
  value.ToLower();
  string eastLower = east;
  eastLower.ToLower();
  if (value == "east" || value == eastLower) return east;
  return west;
 }

 static string Absent()
 {
  if (!s_sAbsent.IsEmpty()) return s_sAbsent;
  if (Living() == ACR_FactionSelection.East()) return ACR_FactionSelection.West();
  return ACR_FactionSelection.East();
 }

 static bool IsAbsent(string factionKey)
 {
  return On() && !factionKey.IsEmpty() && factionKey == Absent();
 }

 static ACR_Base Home() { return s_Home; }
 static ACR_Base AbsentHQ() { return s_AbsentHQ; }

 // Server, after the layout and quick start, before the mode's own Apply, FIA and zombies
 // (ACR_Skirmish.ApplyOnce).
 static void Apply()
 {
  if (s_bApplied || !On()) return;
  s_bApplied = true;
  s_sLiving = Living();
  s_sAbsent = Absent();
  // Both HQs are found while the absent side still owns its bases.
  s_Home = HQOf(s_sLiving);
  s_AbsentHQ = HQOf(s_sAbsent);
  // Starting units belong to the side whose base is nearest (as ACR_Mode spawns them): the absent
  // side's are dropped before its bases open up, or they'd join the living side.
  int units;
  foreach (ACR_StartingUnit unit : ACR_StartingUnit.s_All)
  {
   if (NearestOwner(unit.GetOrigin()) != s_sAbsent) continue;
   unit.m_bDropped = true;
   units++;
  }
  int bases;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_sOwner != s_sAbsent) continue;
   base.m_sOwner = "";
   base.m_fScore = 0;
   base.m_sCapturer = "";
   base.m_iGarrisonLeft = base.m_iGarrison;
   bases++;
  }
  int points;
  array<SCR_SpawnPoint> spawns = {};
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints()) if (point && point.GetFactionKey() == s_sAbsent) spawns.Insert(point);
  foreach (SCR_SpawnPoint gone : spawns) { SCR_EntityHelper.DeleteEntityAndChildren(gone); points++; }
  ACR_Skirmish.RecountPads();
  ACR_FactionSettings settings = ACR_FactionSettings.Find(s_sAbsent);
  if (settings) settings.m_bAICommander = false;
  ACR_PlayerSide.Force(s_sLiving);
  ACR_FactionSelection.SetPlayable(s_sLiving, s_sLiving);
  string home = "none";
  string absentHQ = "none";
  if (s_Home) home = s_Home.m_sName;
  if (s_AbsentHQ) absentHQ = s_AbsentHQ.m_sName;
  string mode = "TVZ";
  if (ACR_Coin.On()) mode = "COIN";
  PrintFormat("[ACR] ONESIDE %1: %2 plays, %3 absent: %4 bases opened, %5 starting units and %6 spawn points dropped; home %7, absent HQ %8", mode, s_sLiving, s_sAbsent, bases, units, points, home, absentHQ);
 }

 // ---------------------------------------------------------------- helpers

 // A side's HQ: its authored HQ-role base, else its owned base nearest its spawn point.
 static ACR_Base HQOf(string side)
 {
  foreach (ACR_Base hq : ACR_Base.s_All)
   if (!hq.m_bRetired && hq.m_sOwner == side && !hq.m_bPocket && hq.m_eLogisticsRole == ACR_EBaseRole.HQ) return hq;
  vector home;
  bool found;
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
   if (point && !ACR_SpawnPoints.IsBasePoint(point) && point.GetFactionKey() == side) { home = point.GetOrigin(); found = true; break; }
  ACR_Base best;
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_sOwner != side || base.m_bPocket) continue;
   float distance = 0;
   if (found) distance = vector.DistanceXZ(base.GetOrigin(), home);
   if (distance < bestDistance) { bestDistance = distance; best = base; }
  }
  return best;
 }

 protected static string NearestOwner(vector position)
 {
  string owner = "";
  float bestDistance = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_sOwner == "" || base.m_bPocket) continue;
   float distance = vector.DistanceXZ(base.GetOrigin(), position);
   if (distance < bestDistance) { bestDistance = distance; owner = base.m_sOwner; }
  }
  return owner;
 }
}
