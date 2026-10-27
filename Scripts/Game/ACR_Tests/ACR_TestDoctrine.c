// AI commander doctrines (ACR_AIDoctrine, 24 Oct), run from srv:regress at tick 4: the pick
// follows option > faction settings > test default > random, the standard doctrine keeps the
// commander's old numbers, and a new commander takes its side's pick. Pass 2: the support
// budget follows the doctrine, reviews pick the right doctrine, and discovery (ACR_DoctrineIntel)
// goes hint > confirmed > out of date > noticed.
class ACR_TestDoctrine
{
 static void Run(ACR_Manager manager)
 {
  array<string> keys = {"balanced", "blitz", "firepower", "fortress", "raider"};
  bool allFound = ACR_AIDoctrine.All().Count() == keys.Count();
  foreach (string key : keys) if (!ACR_AIDoctrine.Find(key)) allFound = false;
  allFound = allFound && ACR_AIDoctrine.Find("BLITZ") != null && !ACR_AIDoctrine.Find("random");
  ACR_TestRegress.Check("doctrine: five doctrines, found by key in any case", allFound, string.Format("%1 doctrines", ACR_AIDoctrine.All().Count()));

  // Balanced must stay the commander from before doctrines.
  ACR_AIDoctrine standard = ACR_AIDoctrine.Find("balanced");
  // Short chains: one long && chain is "Formula too complex" for the compiler.
  bool same = standard.m_fAttackRatio == 1 && standard.m_fDefenseRatio == 1 && standard.m_fConcedeRatio == 1;
  same = same && standard.m_iGrabs == 0 && standard.m_iExtraFronts == 3 && standard.m_iSquadsPerFront == 3;
  same = same && standard.m_fCounterNeed == 0.7 && standard.m_fCounterMinutes == 5;
  same = same && standard.m_fQuietMinutes == 4 && standard.m_fPressurePerMinute == 0.05 && standard.m_fMinPressure == 0.65;
  same = same && standard.m_iReserveSquads == 0 && standard.m_fRetreatRatio == 0.4 && standard.m_fAbortRatio == 0.35;
  same = same && standard.m_iHeavyPrep == 12 && standard.m_iRoadblocks == 2 && standard.m_iTransports == 0;
  same = same && standard.m_fVehicleWeight == 1 && standard.m_fSmallTeamWeight == 1 && !standard.m_bSmallTeamsFree;
  same = same && !standard.m_bSupportsFirst && standard.m_iSupportBudgetMax == 60;
  same = same && standard.m_fHoldRatio == 0.4 && standard.m_fForwardShare == 0.5 && standard.m_iDefensiveAirThreat == 8;
  same = same && standard.m_iMinefields == 2 && standard.m_fFobFront == 1500 && standard.m_fFireBaseSeconds == 240;
  same = same && standard.m_fPocketScore == 2.5 && standard.m_fRideDistance == 900 && standard.m_fAirLiftDistance == 2000;
  same = same && standard.m_fRefitRatio == 0.5 && standard.m_fFireMissionGap == 1 && standard.m_fObserverWeight == 1;
  same = same && standard.m_iSupportMissions == 1 && !standard.m_bAirBudget;
  ACR_TestRegress.Check("doctrine: balanced keeps the standard commander's numbers", same, "");

  bool pairs = true;
  foreach (ACR_AIDoctrine each : ACR_AIDoctrine.All())
  {
   if (ACR_AIDoctrine.LookAlike(each.m_sKey) == each.m_sKey || !ACR_AIDoctrine.Find(ACR_AIDoctrine.LookAlike(each.m_sKey))) pairs = false;
   string counter = ACR_AIDoctrine.Counter(each.m_sKey);
   if (!counter.IsEmpty() && (counter == each.m_sKey || !ACR_AIDoctrine.Find(counter))) pairs = false;
  }
  ACR_TestRegress.Check("doctrine: every doctrine has a look-alike and counters are real doctrines", pairs, "");

  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  string source;
  ACR_AIDoctrine plain = ACR_AIDoctrine.Choose(west, source);
  ACR_TestRegress.Check("doctrine: tests default to balanced", plain && plain.m_sKey == "balanced" && source == "test", string.Format("%1 (%2)", plain.m_sKey, source));

  ACR_Options.Set("acrDoctrineWest", "Blitz");
  ACR_Options.Set("acrDoctrineEast", "fortress");
  string westSource, eastSource;
  ACR_AIDoctrine westPick = ACR_AIDoctrine.Choose(west, westSource);
  ACR_AIDoctrine eastPick = ACR_AIDoctrine.Choose(east, eastSource);
  ACR_TestRegress.Check("doctrine: each side takes its own setup choice", westPick.m_sKey == "blitz" && eastPick.m_sKey == "fortress" && westSource == "screen",
   string.Format("west %1 %2 (%3), east %4 %5", west, westPick.m_sKey, westSource, east, eastPick.m_sKey));

  ACR_AICommander made = new ACR_AICommander(manager, west);
  ACR_TestRegress.Check("doctrine: a new AI commander fights to its side's pick", made.Doctrine() && made.Doctrine().m_sKey == "blitz", made.Doctrine().m_sName);

  // The support budget follows the doctrine (it used to be 40 RP for all of them).
  made.SetDoctrineForTest(ACR_AIDoctrine.Find("balanced"));
  int balancedBudget = made.SupportBudgetForTest();
  made.SetDoctrineForTest(ACR_AIDoctrine.Find("firepower"));
  int firepowerBudget = made.SupportBudgetForTest();
  made.SetDoctrineForTest(ACR_AIDoctrine.Find("blitz"));
  int blitzBudget = made.SupportBudgetForTest();
  bool budgets = balancedBudget > 0 && firepowerBudget > balancedBudget && blitzBudget < balancedBudget;
  ACR_TestRegress.Check("doctrine: Firepower keeps more RP for fire missions, Blitz less", budgets,
   string.Format("balanced %1, firepower %2, blitz %3", balancedBudget, firepowerBudget, blitzBudget));

  CheckReviews();
  CheckDiscovery(manager);

  ACR_Options.Set("acrDoctrineWest", "random");
  ACR_AIDoctrine rolled = ACR_AIDoctrine.Choose(west, source);
  ACR_TestRegress.Check("doctrine: an explicit random rolls one", rolled != null && source == "random", string.Format("%1 (%2)", rolled.m_sKey, source));

  // A faction's own setting beats the test default; the host option beats it.
  ACR_Options.Unset("acrDoctrineWest");
  ACR_FactionSettings settings = ACR_FactionSettings.Find(west);
  if (settings)
  {
   ACR_EAIDoctrineChoice was = settings.m_eAIDoctrine;
   settings.m_eAIDoctrine = ACR_EAIDoctrineChoice.RAIDER;
   ACR_AIDoctrine fromSettings = ACR_AIDoctrine.Choose(west, source);
   settings.m_eAIDoctrine = was;
   ACR_TestRegress.Check("doctrine: the faction's m_eAIDoctrine applies when no option is set", fromSettings.m_sKey == "raider" && source == "faction settings", string.Format("%1 (%2)", fromSettings.m_sKey, source));
  }
  else ACR_TestRegress.Skip("doctrine: the faction's m_eAIDoctrine applies when no option is set", "no settings for " + west);

  string row = ACR_Setup.ValueName("doctrineeast", "fortress");
  bool rows = ACR_Setup.Rows().Contains("doctrinewest\tBlue AI doctrine") && ACR_Setup.Option("doctrineeast") == "acrDoctrineEast" && row.StartsWith("Fortress");
  ACR_TestRegress.Check("doctrine: setup screen rows for both sides", rows, row);
  ACR_Options.Unset("acrDoctrineEast");
 }

 protected static void CheckReviews()
 {
  string why;
  string winning = ACR_AICommander.PickDoctrine("blitz", "fortress", 3, 0, 5, 20, why);
  string losing = ACR_AICommander.PickDoctrine("blitz", "", 0, 3, 10, 10, why);
  string losingFortress = ACR_AICommander.PickDoctrine("fortress", "", 1, 3, 10, 10, why);
  string bleeding = ACR_AICommander.PickDoctrine("balanced", "", 1, 1, 20, 8, why);
  string stuck = ACR_AICommander.PickDoctrine("balanced", "", 0, 0, 4, 4, why);
  string stuckBlitz = ACR_AICommander.PickDoctrine("blitz", "", 0, 0, 4, 4, why);
  string counter = ACR_AICommander.PickDoctrine("balanced", "blitz", 1, 1, 6, 6, why);
  string even = ACR_AICommander.PickDoctrine("raider", "", 2, 1, 8, 8, why);
  string countering = ACR_AICommander.PickDoctrine("fortress", "blitz", 0, 0, 6, 6, why);
  // Taking empty bases while losing men 5:1 is not winning (night soak 26 Oct).
  string grabbingBleeding = ACR_AICommander.PickDoctrine("balanced", "", 3, 0, 20, 4, why);
  bool ok = winning == "" && losing == "fortress" && losingFortress == "firepower" && bleeding == "firepower";
  ok = ok && stuck == "blitz" && stuckBlitz == "raider" && counter == "fortress" && even == "" && countering == "";
  ok = ok && grabbingBleeding == "firepower";
  ACR_TestRegress.Check("doctrine: reviews keep a winning doctrine, change a failing one, counter a known enemy", ok,
   string.Format("winning [%1] losing %2/%3 bleeding %4 stuck %5/%6 counter %7 even [%8] countering [%9]", winning, losing, losingFortress, bleeding, stuck, stuckBlitz, counter, even, countering) + " grabbing-bleeding " + grabbingBleeding);

  // #29: a changed doctrine is held 30 min; the one just left doesn't come straight back for the
  // same kind of reason (Fortress -> Firepower losing ground, then back for heavy losses).
  string first = ACR_AICommander.DoctrineChangeBlocked("firepower", "losing ground", -1, "", "");
  string early = ACR_AICommander.DoctrineChangeBlocked("blitz", "getting nowhere", 20, "balanced", "losing ground");
  string swapBack = ACR_AICommander.DoctrineChangeBlocked("fortress", "losses too heavy", 40, "fortress", "losing ground");
  string chaseBack = ACR_AICommander.DoctrineChangeBlocked("balanced", "countering the enemy's Raider", 40, "balanced", "countering the enemy's Blitz");
  string otherReason = ACR_AICommander.DoctrineChangeBlocked("fortress", "getting nowhere", 40, "fortress", "losing ground");
  string elsewhere = ACR_AICommander.DoctrineChangeBlocked("raider", "losses too heavy", 40, "fortress", "losing ground");
  bool holds = first == "" && early.Contains("held") && swapBack.Contains("same reason") && chaseBack.Contains("same reason") && otherReason == "" && elsewhere == "";
  ACR_TestRegress.Check("doctrine: a change is held 30 min and never swaps straight back for the same reason", holds,
   string.Format("first [%1] early [%2] swap [%3] chase [%4] other [%5] elsewhere [%6]", first, early, swapBack, chaseBack, otherReason, elsewhere));
 }

 protected static void CheckDiscovery(ACR_Manager manager)
 {
  ACR_Intel intel = new ACR_Intel(manager, "ACR_TEST_SIDE");
  ACR_DoctrineIntel known = intel.Doctrine();
  known.m_EnemyForTest = ACR_AIDoctrine.Find("blitz");
  known.Add(20, "test");
  bool start = known.m_iLevel == 0 && known.Text().Contains("unknown") && known.Believed() == "";
  known.Add(15, "test");
  bool hint = known.m_iLevel == 1 && known.Text().Contains("Blitz or Raider") && known.Believed() == "";
  known.Add(30, "test");
  bool confirmed = known.m_iLevel == 2 && known.Text().Contains("Blitz (confirmed)") && known.Believed() == "blitz";
  ACR_TestRegress.Check("doctrine: intel goes from unknown to a hint to confirmed", start && hint && confirmed, known.Text());

  // The enemy changes: the side keeps the old picture until new intel shows the change.
  known.m_EnemyForTest = ACR_AIDoctrine.Find("fortress");
  known.OnEnemyChanged("blitz");
  bool unnoticed = known.Believed() == "blitz" && known.Text().Contains("Blitz (confirmed)");
  known.Add(10, "test");
  string afterNotice = known.Text();
  bool noticed = known.m_bChangeNoticed && known.Believed() == "" && afterNotice.Contains("no longer Blitz") && afterNotice.Contains("unknown");
  known.Add(60, "test");
  bool relearned = known.Believed() == "fortress" && !known.m_bStale;
  ACR_TestRegress.Check("doctrine: a change goes unnoticed until new intel, then is worked out again", unnoticed && noticed && relearned, afterNotice + " / " + known.Text());
 }
}
