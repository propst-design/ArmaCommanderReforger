// -acrRestartTest 1 (server-capable, ~2 min): a match restarted in the same process starts clean.
// Mission 1 takes a census of the per-match static registries at tick 12, leaves a setup choice
// and a marker file behind and asks the game to restart the scenario (the game-over path,
// GameStateTransitions.RequestScenarioRestart). Mission 2 takes the same census at the same
// tick: a registry nobody cleared comes back bigger or holding deleted things. Statics survive a
// restart in Enforce, so state a Reset() forgets leaks into the next match (BBB ran on the last
// mission's state for weeks, 6f785fe; ACA's world-time caches, 77c9b78).
class ACR_TestRestart
{
 static const int CHECKS = 6;
 static const int CENSUS_TICK = 12;
 static const int RUN_TICK = 45;
 static const string MARKER = "$profile:acr-restart-test.txt";
 static const string PROBE = "acrRestartProbe";
 static bool s_bSecond;            // this mission is the one after the restart
 static int s_iMissions;           // missions this process ran the test in (statics persist?)
 static BBB_Combat s_OldCombat;    // weak: mission 1's BBB combat object

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick == 1)
  {
   s_iMissions++;
   if (FileIO.FileExists(MARKER) && System.GetUnixTime() - MarkerTime() > 180) FileIO.DeleteFile(MARKER);
   s_bSecond = FileIO.FileExists(MARKER);
  }
  if (!s_bSecond)
  {
   if (tick == CENSUS_TICK) FirstMission();
   if (tick == CENSUS_TICK + 60) ACR_TestRegress.Abort("restart", "the scenario did not restart within 60 s");
   return;
  }
  if (tick == CENSUS_TICK) SecondMissionCensus();
  if (tick == RUN_TICK) SecondMissionRuns();
 }

 protected static void FirstMission()
 {
  map<string, ref array<int>> census = Census();
  FileHandle file = FileIO.OpenFile(MARKER, FileMode.WRITE);
  if (!file) { ACR_TestRegress.Abort("restart", "can't write " + MARKER); return; }
  file.WriteLine(string.Format("time %1 0", System.GetUnixTime()));
  foreach (string name, array<int> counts : census) file.WriteLine(string.Format("%1 %2 %3", name, counts[0], counts[1]));
  file.Close();
  ACR_Options.Set(PROBE, "1");
  LeaveSentinels();
  s_OldCombat = BBB.s_Combat;
  PrintFormat("[ACR] RESTART mission 1 census taken (%1 registries), restarting", census.Count());
  if (!GameStateTransitions.RequestScenarioRestart())
  {
   FileIO.DeleteFile(MARKER);
   ACR_TestRegress.Abort("restart", "RequestScenarioRestart refused");
  }
 }

 protected static int MarkerTime()
 {
  FileHandle file = FileIO.OpenFile(MARKER, FileMode.READ);
  if (!file) return 0;
  string line;
  int stamp = 0;
  if (file.ReadLine(line) >= 0 && line.StartsWith("time ")) stamp = line.Substring(5, line.Length() - 7).ToInt();
  file.Close();
  return stamp;
 }

 protected static void SecondMissionCensus()
 {
  map<string, ref array<int>> before = new map<string, ref array<int>>();
  FileHandle file = FileIO.OpenFile(MARKER, FileMode.READ);
  string line;
  while (file && file.ReadLine(line) >= 0)
  {
   array<string> parts = {};
   line.Split(" ", parts, true);
   if (parts.Count() == 3 && parts[0] != "time") before.Set(parts[0], Pair(parts[1].ToInt(), parts[2].ToInt()));
  }
  if (file) file.Close();
  FileIO.DeleteFile(MARKER);
  bool statics = s_iMissions > 1;
  PrintFormat("[ACR] RESTART mission 2: statics %1 the restart (missions seen %2)", ACR_TestRegress.Verdict(statics), s_iMissions);

  map<string, ref array<int>> after = Census();
  string grown = "";
  string stale = "";
  foreach (string name, array<int> now : after)
  {
   array<int> was = before.Get(name);
   int wasCount = 0;
   if (was) wasCount = was[0];
   // Same world, same tick: a fresh registry holds about what mission 1 held. A forgotten one
   // holds mission 1's entries as well (twice as many) or its deleted things (nulls).
   int slack = Math.Max(2, wasCount / 4);
   if (now[0] > wasCount + slack) grown += string.Format(" %1 %2->%3", name, wasCount, now[0]);
   if (now[1] > 0) stale += string.Format(" %1 (%2 of %3)", name, now[1], now[0]);
   PrintFormat("[ACR] RESTART census %1: mission 1 %2, mission 2 %3, nulls %4", name, wasCount, now[0], now[1]);
  }
  ACR_TestRegress.Check("restart: per-match registries don't carry the last match's entries", grown == "", "grew:" + grown);
  ACR_TestRegress.Check("restart: per-match registries hold no deleted things", stale == "", "nulls:" + stale);
  string kept = SentinelsKept();
  ACR_TestRegress.Check("restart: per-match caches drop what the last match put in them", kept == "", "kept:" + kept);

  string probe;
  bool optionKept = ACR_Options.Get(PROBE, probe);
  ACR_TestRegress.Check("restart: setup choices from the last match are gone", !optionKept, string.Format("probe kept %1, statics persist %2", optionKept, statics));

  // BBB.Stop drops its combat object; a BBB that was never stopped keeps mission 1's.
  if (statics) ACR_TestRegress.Check("restart: BBB stopped with the last mission and started again", BBB.IsStarted() && BBB.s_Combat && BBB.s_Combat != s_OldCombat, string.Format("started %1, combat %2, same as mission 1 %3", BBB.IsStarted(), BBB.s_Combat != null, BBB.s_Combat == s_OldCombat));
  else ACR_TestRegress.Skip("restart: BBB stopped with the last mission and started again", "statics did not survive the restart");
 }

 protected static void SecondMissionRuns()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  int squads = 0;
  int dead = 0;
  if (manager)
  {
   foreach (ACR_Squad squad : manager.m_Squads)
   {
    if (!squad || !squad.m_Group) dead++;
    else squads++;
   }
  }
  ACR_TestRegress.Check("restart: the new match runs (manager, live squads only)", manager && squads > 0 && dead == 0, string.Format("manager %1, squads %2, without a group %3", manager != null, squads, dead));
  ACR_TestRegress.Complete("restart", CHECKS);
 }

 // name -> {entries, null entries} for every public per-match static registry. Lookup caches
 // that are the same for every match (faction resolution, perf counters) are left out.
 // One marked entry in each public per-match cache that takes plain values. This world fills few
 // of them on its own, so the census alone can't tell a cleared cache from an empty one.
 static const string SENTINEL = "acr-restart-probe";
 static const vector SENTINEL_AT = "-4242 0 -4242";

 protected static void LeaveSentinels()
 {
  ACR_LandingZone.s_Failed.Set(SENTINEL, 1);
  ACR_Generator.s_Moves.Set(SENTINEL, SENTINEL_AT);
  ACR_QuickStart.s_mPositions.Set(SENTINEL, 1);
  ACR_QuickStart.s_mFields.Set(SENTINEL, 1);
  ACR_Setup.s_PreviewNames.Insert(SENTINEL);
  ACR_Guerrillas.s_aHouses.Insert(SENTINEL_AT);
  ACR_Guerrillas.s_aStarts.Insert(SENTINEL_AT);
  ACR_Guerrillas.s_mTraffic.Set(-4242, 1);
  ACR_Supports.s_aBatteryImpacts.Insert(SENTINEL_AT);
  ACR_MapLayers.Set(0, true);
  ACR_Outbreak.s_iCounters = 1;
  ACR_LocalFlyover.s_iStarted = 1;
  ACR_BaseDefences.s_mBlockers.Set(SENTINEL, 1);
  ACR_BaseRing.s_aLastPath.Insert(SENTINEL_AT);
 }

 protected static string SentinelsKept()
 {
  string kept = "";
  if (ACR_LandingZone.s_Failed.Contains(SENTINEL)) kept += " LandingZone.s_Failed";
  if (ACR_Generator.s_Moves.Contains(SENTINEL)) kept += " Generator.s_Moves";
  if (ACR_QuickStart.s_mPositions.Contains(SENTINEL)) kept += " QuickStart.s_mPositions";
  if (ACR_QuickStart.s_mFields.Contains(SENTINEL)) kept += " QuickStart.s_mFields";
  if (ACR_Setup.s_PreviewNames.Find(SENTINEL) >= 0) kept += " Setup.s_PreviewNames";
  if (ACR_Guerrillas.s_aHouses.Find(SENTINEL_AT) >= 0) kept += " Guerrillas.s_aHouses";
  if (ACR_Guerrillas.s_aStarts.Find(SENTINEL_AT) >= 0) kept += " Guerrillas.s_aStarts";
  if (ACR_Guerrillas.s_mTraffic.Contains(-4242)) kept += " Guerrillas.s_mTraffic";
  if (ACR_Supports.s_aBatteryImpacts.Find(SENTINEL_AT) >= 0) kept += " Supports.s_aBatteryImpacts";
  if (ACR_MapLayers.AnyOff()) kept += " MapLayers.s_aOff";
  if (ACR_Outbreak.s_iCounters != 0) kept += " Outbreak.s_iCounters";
  if (ACR_LocalFlyover.s_iStarted != 0) kept += " LocalFlyover.s_iStarted";
  if (ACR_BaseDefences.s_mBlockers.Contains(SENTINEL)) kept += " BaseDefences.s_mBlockers";
  if (ACR_BaseRing.s_aLastPath.Find(SENTINEL_AT) >= 0) kept += " BaseRing.s_aLastPath";
  return kept;
 }

 // Its values are owned (ref), which the generic key counter can't take.
 static int LaneNulls()
 {
  int n = 0;
  for (int i = 0; i < ACR_RoadblockLane.s_mSteps.Count(); i++)
  {
   if (!ACR_RoadblockLane.s_mSteps.GetKey(i)) n++;
  }
  return n;
 }

 static array<int> Pair(int entries, int nulls)
 {
  array<int> pair = {};
  pair.Insert(entries);
  pair.Insert(nulls);
  return pair;
 }

 static map<string, ref array<int>> Census()
 {
  map<string, ref array<int>> c = new map<string, ref array<int>>();
  c.Set("Base.s_All", Pair(ACR_Base.s_All.Count(), ACR_TestRestartNulls<ACR_Base>.In(ACR_Base.s_All)));
  c.Set("AirLift.s_Active", Pair(ACR_AirLift.s_Active.Count(), ACR_TestRestartNulls<ACR_AirLift>.In(ACR_AirLift.s_Active)));
  c.Set("LandingZone.s_Failed", Pair(ACR_LandingZone.s_Failed.Count(), 0));
  c.Set("BaseDefences.s_aPreview", Pair(ACR_BaseDefences.s_aPreview.Count(), ACR_TestRestartNulls<IEntity>.In(ACR_BaseDefences.s_aPreview)));
  c.Set("Defenders.s_Posts", Pair(ACR_Defenders.s_Posts.Count(), 0));
  c.Set("DoctrineKits.s_Given", Pair(ACR_DoctrineKits.s_Given.Count(), 0));
  c.Set("FactionSettings.s_All", Pair(ACR_FactionSettings.s_All.Count(), ACR_TestRestartNulls<ACR_FactionSettings>.In(ACR_FactionSettings.s_All)));
  c.Set("FireMissions.s_ObserverGroups", Pair(ACR_FireMissions.s_ObserverGroups.Count(), ACR_TestRestartNulls<SCR_AIGroup>.In(ACR_FireMissions.s_ObserverGroups)));
  c.Set("Fortify.s_Positions", Pair(ACR_Fortify.s_Positions.Count(), 0));
  c.Set("Generator.s_Sites", Pair(ACR_Generator.s_Sites.Count(), 0));
  c.Set("Generator.s_Moves", Pair(ACR_Generator.s_Moves.Count(), 0));
  c.Set("Guerrillas.s_aRoads", Pair(ACR_Guerrillas.s_aRoads.Count(), 0));
  c.Set("Guerrillas.s_aHouses", Pair(ACR_Guerrillas.s_aHouses.Count(), 0));
  c.Set("Guerrillas.s_aStarts", Pair(ACR_Guerrillas.s_aStarts.Count(), 0));
  c.Set("Guerrillas.s_mTraffic", Pair(ACR_Guerrillas.s_mTraffic.Count(), 0));
  c.Set("Helipad.s_All", Pair(ACR_Helipad.s_All.Count(), ACR_TestRestartNulls<ACR_Helipad>.In(ACR_Helipad.s_All)));
  c.Set("Insurgency.s_aRoads", Pair(ACR_Insurgency.s_aRoads.Count(), 0));
  c.Set("Insurgency.s_aSides", Pair(ACR_Insurgency.s_aSides.Count(), 0));
  c.Set("Minefields.s_Fields", Pair(ACR_Minefields.s_Fields.Count(), 0));
  c.Set("StartingUnit.s_All", Pair(ACR_StartingUnit.s_All.Count(), ACR_TestRestartNulls<ACR_StartingUnit>.In(ACR_StartingUnit.s_All)));
  c.Set("QuickStart.s_Assigned", Pair(ACR_QuickStart.s_Assigned.Count(), ACR_TestRestartNulls<ACR_Base>.In(ACR_QuickStart.s_Assigned)));
  c.Set("QuickStart.s_Front", Pair(ACR_QuickStart.s_Front.Count(), ACR_TestRestartNulls<ACR_Base>.In(ACR_QuickStart.s_Front)));
  c.Set("QuickStart.s_mPositions", Pair(ACR_QuickStart.s_mPositions.Count(), 0));
  c.Set("QuickStart.s_mFields", Pair(ACR_QuickStart.s_mFields.Count(), 0));
  c.Set("RoadblockLane.s_mSteps", Pair(ACR_RoadblockLane.s_mSteps.Count(), LaneNulls()));
  c.Set("Roadblocks.s_aRoadblocks", Pair(ACR_Roadblocks.s_aRoadblocks.Count(), 0));
  c.Set("Setup.s_PreviewPos", Pair(ACR_Setup.s_PreviewPos.Count(), 0));
  c.Set("Skirmish.s_All", Pair(ACR_Skirmish.s_All.Count(), ACR_TestRestartNulls<ACR_Skirmish>.In(ACR_Skirmish.s_All)));
  c.Set("SquadMarksmen.s_Assigned", Pair(ACR_SquadMarksmen.s_Assigned.Count(), ACR_TestRestartKeys<SCR_AIGroup, IEntity>.Nulls(ACR_SquadMarksmen.s_Assigned)));
  c.Set("SquadMarksmen.s_Pending", Pair(ACR_SquadMarksmen.s_Pending.Count(), 0));
  c.Set("Structures.s_Registered", Pair(ACR_Structures.s_Registered.Count(), ACR_TestRestartNulls<SCR_CampaignBuildingCompositionComponent>.In(ACR_Structures.s_Registered)));
  c.Set("Supports.s_aBatteryImpacts", Pair(ACR_Supports.s_aBatteryImpacts.Count(), 0));
  c.Set("Outbreak.s_aStarts", Pair(ACR_Outbreak.StartCount(), 0));
  c.Set("Outbreak.s_aCounterAt", Pair(ACR_Outbreak.CounterCount(), 0));
  c.Set("LocalFlyover.s_All", Pair(ACR_LocalFlyover.Count(), 0));
  c.Set("AttackGround.s_Building", Pair(ACR_AttackGround.BuildingCount(), 0));
  c.Set("BagHold.s_Places", Pair(ACR_BagHold.PlaceCount(), 0));
  c.Set("BaseDefences.s_mBlockers", Pair(ACR_BaseDefences.s_mBlockers.Count(), 0));
  c.Set("BaseRing.s_aLastPath", Pair(ACR_BaseRing.s_aLastPath.Count(), 0));
  c.Set("Guerrillas.s_aWeights", Pair(ACR_Guerrillas.s_aWeights.Count(), 0));
  c.Set("Guerrillas.s_mTrafficAt", Pair(ACR_Guerrillas.s_mTrafficAt.Count(), 0));
  c.Set("CommandPresence.State", Pair(ACR_CommandPresence.StateCount(), 0));
  c.Set("SupplyLines.State", Pair(ACR_SupplyLines.StateCount(), 0));
  c.Set("HideoutHomes.State", Pair(ACR_HideoutHomes.StateCount(), 0));
  c.Set("Muster.s_Jobs", Pair(ACR_Muster.s_Jobs.Count(), 0));
  c.Set("Disguise.s_States", Pair(ACR_Disguise.s_States.Count(), 0));
  c.Set("GuerrillaAI.State", Pair(ACR_GuerrillaAI.StateCount(), 0));
  c.Set("FIASpoils.s_aTeams", Pair(ACR_FIASpoils.s_aTeams.Count(), 0));
  c.Set("FIASpoils.s_aSwaps", Pair(ACR_FIASpoils.s_aSwaps.Count(), 0));
  c.Set("FIAPrisoners.s_aPows", Pair(ACR_FIAPrisoners.s_aPows.Count(), 0));
  c.Set("FIAIntel.s_aLeads", Pair(ACR_FIAIntel.s_aLeads.Count(), 0));
  c.Set("Depots.s_aDepots", Pair(ACR_Depots.s_aDepots.Count(), 0));
  c.Set("IED.s_aIEDs", Pair(ACR_IED.s_aIEDs.Count(), 0));
  c.Set("Containment.s_aSites", Pair(ACR_Containment.SiteCount(), 0));
  c.Set("Evac.s_aPoints", Pair(ACR_Evac.Count(), 0));
  c.Set("Airdrop.s_aPallets", Pair(ACR_Airdrop.Count(), 0));
  c.Set("Decks.Submitted", Pair(ACR_Decks.Submitted().Count(), 0));
  c.Set("OrganEasterEgg.State", Pair(ACR_OrganEasterEgg.StateCount(), 0));
  c.Set("BBBHooks.s_mIntentSeen", Pair(ACR_BBBHooks.IntentCount(), 0));
  c.Set("TwoICForecast.State", Pair(ACR_TwoICForecast.StateCount(), 0));
  c.Set("UALoadout.State", Pair(ACR_UALoadout.StateCount(), 0));
  return c;
 }
}

class ACR_TestRestartNulls<Class T>
{
 static int In(array<T> list)
 {
  int n = 0;
  if (!list) return n;
  foreach (T item : list)
  {
   if (!item) n++;
  }
  return n;
 }
}

class ACR_TestRestartKeys<Class K, Class V>
{
 static int Nulls(map<K, V> table)
 {
  int n = 0;
  if (!table) return n;
  for (int i = 0; i < table.Count(); i++)
  {
   if (!table.GetKey(i)) n++;
  }
  return n;
 }
}
