// Connects the tests to the game code (see ACR/ACR_TestHooks.c). Not in the player package.
modded class ACR_TestHooks
{
 override void InitStatics()
 {
  ACR_TestStaticInit.Run();
 }

 protected ref ACR_TestHarness m_Harness;

 override bool PlayerCommandsForTest(string factionKey)
 {
  return ACR_TestTwoICAll.s_sCommanded == factionKey && !factionKey.IsEmpty();
 }

 // The 2IC is on by default (19 Nov): on the range it would take on the squads every test scripts, so it
 // stays off there unless the test sets -acr2IC itself (the twoic tests). The FIA scenario tests switch
 // the AI commander off to script their squads (bait, sweepers, spoils fixtures): with the 2IC on, it
 // adopted them and sent them to grab bases, so no free squad was left to sweep (srv:insurgency,
 // srv:hideouts, srv:fiaspoils failing since 7a7657a). Off there too.
 override string OptionFallback(string option, string fallback)
 {
  if (option == "acr2IC" && (m_Harness || ScriptsSquadsWithoutCommander())) return "0";
  return fallback;
 }

 // Scenario tests that turn the AI commander off and order their own squads.
 protected static bool ScriptsSquadsWithoutCommander()
 {
  return System.IsCLIParam("acrGuerrillasTest") || System.IsCLIParam("acrInsurgencyTest") || System.IsCLIParam("acrFIALeaningTest") || System.IsCLIParam("acrHideoutsTest") || System.IsCLIParam("acrFIASpoilsTest") || System.IsCLIParam("acrFIAGrievanceTest") || System.IsCLIParam("acrSupportTest") || System.IsCLIParam("acrCoinGrowthTest");
 }

 override void RangeStarted(ACR_TestRange range)
 {
  if (System.IsCLIParam("acrAirLiftTest") || System.IsCLIParam("acrAirLiftTwoTest") || System.IsCLIParam("acrAirLiftContactTest")) ACR_Options.Set("acrAI", "none");
  m_Harness = ACR_TestHarness.CreateFromCommandLine(range);
  if (m_Harness) GetGame().GetCallqueue().CallLater(m_Harness.Tick, 1000, true);
  // A scenario test that landed on the range (wrong world): end at once.
  if (System.IsCLIParam("acrScenarioTest")) ACR_TestRegress.Abort("scenario", "wrong world: restart the test launcher");
 }

 override void RangeEnded(ACR_TestRange range)
 {
  if (m_Harness && GetGame()) GetGame().GetCallqueue().Remove(m_Harness.Tick);
  m_Harness = null;
 }

 override void ScenarioSecond(int seconds)
 {
  // -acrPullOverBankTest 1 (srv:pulloverbank, Everon): a pull-over on a banked stretch, truck and APC.
  if (System.IsCLIParam("acrPullOverBankTest")) ACR_TestPullOverBank.Run(seconds);
  // -acrAllocPerfTest / -acrAllocSoakTest: the soaks with the AI job allocator observing.
  if (seconds <= 5 && (System.IsCLIParam("acrAllocPerfTest") || System.IsCLIParam("acrAllocSoakTest"))) ACR_Options.Set("acrAIAllocator", "1");
  if (seconds <= 5 && (System.IsCLIParam("acrAllocControlPerfTest") || System.IsCLIParam("acrAllocControlSoakTest") || System.IsCLIParam("acrAllocControlLongSoakTest"))) ACR_Options.Set("acrAIAllocator", "2");
  // -acrClassicSoakTest / -acrClassicLongSoakTest: the Everon soaks with the commander from before #232
  // (ACR_AIRework), the A arm of its A/B; the plain soaks run the new default.
  if (seconds <= 5 && (System.IsCLIParam("acrClassicSoakTest") || System.IsCLIParam("acrClassicLongSoakTest"))) ACR_Options.Set("acrAICommander", "classic");
  // -acrForecastSoakTest: the Everon soak with the defence forecast on (ACR_DefenceForecast).
  if (seconds <= 5 && (System.IsCLIParam("acrForecastSoakTest") || System.IsCLIParam("acrForecastSkirmishTest"))) ACR_Options.Set("acrDefenceForecast", "1");
  // -acrPadsTest: helipads on this scenario's map (ACR_TestPads).
  if (System.IsCLIParam("acrPadsTest") || System.IsCLIParam("acrPadsLevieTest") || System.IsCLIParam("acrPadsMontignacTest") || System.IsCLIParam("acrPadsPhilippeTest")) ACR_TestPads.Run(null, seconds);
  // -acrSurveyTest: object/terrain dump for placing bases and pads (ACR_TestSurvey).
  if (System.IsCLIParam("acrSurveyTest")) ACR_TestSurvey.Run(null, seconds);
  // -acrPlacesTest: named map locations, for laying out a new map (ACR_TestPlaces).
  if (System.IsCLIParam("acrPlacesTest")) ACR_TestPlaces.Run(seconds);
  // -acrVehicleExitTest: bought vehicles get spots they can drive out of (ACR_TestVehicleExit).
  if (System.IsCLIParam("acrVehicleExitTest")) ACR_TestVehicleExit.Run(seconds);
  // -acrSetupTest: the setup phase (ACR_TestSetup).
  if (ACR_TestSetup.Wanted()) ACR_TestSetup.Run(seconds);
  if (ACR_TestSetup.UiWanted()) ACR_TestSetup.RunUi(seconds);
  if (ACR_TestSetup.ReserveWanted()) ACR_TestSetup.RunReserve(seconds);
  if (ACR_TestSetup.GenWanted()) ACR_TestSetup.RunGen(seconds);
  if (ACR_TestSetup.MapWanted()) ACR_TestSetup.RunMap(seconds);
  // -acrSetupPresetsTest: setup presets (ACR_TestSetupPresets, #143).
  if (ACR_TestSetupPresets.Wanted()) ACR_TestSetupPresets.Run(seconds);
  // -acrSetupLobbyTest: the setup lobby's rows, requests, layout and dialog preset (ACR_TestSetupLobby, 0.7.0).
  if (ACR_TestSetupLobby.Wanted()) ACR_TestSetupLobby.Run(seconds);
  if (System.IsCLIParam("acrEveronSurveyTest")) ACR_TestPlaces.SurveyEveron(seconds);
  if (System.IsCLIParam("acrEveronPadsTest")) ACR_TestPads.SurveyEveron(seconds);
  // -acrSkirmishTest / -acrSkirmishSouthTest: skirmish layouts on Everon (ACR_TestSkirmish).
  if (System.IsCLIParam("acrSkirmishTest") || System.IsCLIParam("acrSkirmishSouthTest")) ACR_TestSkirmish.Run(seconds);
  // -acrGenerateTest / -acrGenerateEveronTest: the scenario generator (ACR_TestGenerate).
  if (System.IsCLIParam("acrLiftClearanceTest")) ACR_TestLiftClearance.Run(seconds);
  if (System.IsCLIParam("acrLzBenchTest")) ACR_TestLzBench.Run(seconds);
  // -acrShoreRoutesTest: BBB land routes keep legs off the shore (ACR_TestShoreRoutes).
  if (System.IsCLIParam("acrShoreRoutesTest")) ACR_TestShoreRoutes.Run(seconds);
  // -acrFieldBridgesTest: foot routes over bridges on Everon (ACR_TestFieldRoutes).
  if (System.IsCLIParam("acrFieldBridgesTest")) ACR_TestFieldRoutes.RunBridges(seconds);
  if (System.IsCLIParam("acrSlopesTest")) ACR_TestSlopes.Run(seconds);
  // -acrPathProbeTest: can foot squads reach Gallows Base's centre (ACR_TestPathProbe).
  if (System.IsCLIParam("acrPathProbeTest")) ACR_TestPathProbe.Run(seconds);
  // -acrWorksShotsTest: Fortify a whole base, photographed (ACR_TestWorksShots).
  if (System.IsCLIParam("acrPlaceAuditTest") || System.IsCLIParam("acrPlaceAuditOldTest")) ACR_TestPlaceAudit.Run(seconds);
  // -acrVoidSpawnTest: soldiers never placed in sealed spaces (ACR_TestVoidSpawn).
  if (System.IsCLIParam("acrVoidSpawnTest")) ACR_TestVoidSpawn.Run(seconds);
  // -acrDriveRoutesTest / -acrDriveRoutesBBBTest off the test range (Everon, Chernarus): BBB driver A/B.
  if (!m_Harness && (System.IsCLIParam("acrDriveRoutesTest") || System.IsCLIParam("acrDriveRoutesBBBTest"))) ACR_TestDriveRoutes.Run(null, seconds);
  // -acrTruckParkTest: where idle service trucks wait at a base (ACR_TestTruckPark).
  if (System.IsCLIParam("acrTruckParkTest")) ACR_TestTruckPark.Run(seconds);
  // -acrLightRidesTest: the AI commander buys jeeps for small parties' rides (ACR_TestLightRides).
  if (System.IsCLIParam("acrLightRidesTest")) ACR_TestLightRides.Run(seconds);
  if (System.IsCLIParam("acrPartsTest")) ACR_TestParts.Run(seconds);
  if (System.IsCLIParam("acrWingsTest") || System.IsCLIParam("acrWingsOffTest") || System.IsCLIParam("acrWingShotsTest")) ACR_TestWings.Run(seconds);
  if (System.IsCLIParam("acrWorksShotsTest") || System.IsCLIParam("acrWorksShotsCompactTest") || ACR_TestBaseWorks.StyleTest("acrStyleShots")) ACR_TestWorksShots.Run(seconds);
  // -acrBaseWorksTest: Fortify on a base (ACR_TestBaseWorks, #105).
  if (ACR_TestBaseWorks.StyleTest("acrStylePlan")) ACR_TestBaseWorks.Run(seconds);
  if (System.IsCLIParam("acrBaseWorksTest") || System.IsCLIParam("acrBaseWorksCompactTest") || System.IsCLIParam("acrBaseWorksAITest") || System.IsCLIParam("acrBaseWorksPlanTest")) ACR_TestBaseWorks.Run(seconds);
  // -acrWorksFightTest / -acrWorksFightOldTest: what a fortified town costs an attacker (ACR_TestWorksFight).
  if (System.IsCLIParam("acrWorksFightTest") || System.IsCLIParam("acrWorksFightValueOffTest") || System.IsCLIParam("acrWorksFightVillageTest") || System.IsCLIParam("acrWorksFightVillageOffTest") || System.IsCLIParam("acrWorksFightOldTest") || System.IsCLIParam("acrWorksFightCompactTest") || System.IsCLIParam("acrWorksFightOffTest") || System.IsCLIParam("acrWorksFightBaseTest")) ACR_TestWorksFight.Run(seconds);
  // -acrNightFlaresTest: BBB's night flares under ACA's policy (ACR_TestNightFlares).
  if (System.IsCLIParam("acrNightFlaresTest")) ACR_TestNightFlares.Run(seconds);
  // -acrBagHoldTest / -acrBagHoldLateTest: men behind bags in a fight (ACR_TestBagHold).
  if (System.IsCLIParam("acrBagHoldTest") || System.IsCLIParam("acrBagHoldLateTest") || System.IsCLIParam("acrBagHoldOffTest") || System.IsCLIParam("acrBagHoldLowTest")) ACR_TestBagHold.Run(seconds);
  if (System.IsCLIParam("acrBagHoldSquadsTest")) ACR_TestBagHoldSquads.Run(seconds);
  // -acrWorksAISoakTest: does the AI fortify (ACR_TestWorksSoak).
  if (System.IsCLIParam("acrWorksAISoakTest")) ACR_TestWorksSoak.Run(seconds);
  // -acrKeyHintsTest: the key reminders and UI prefs (ACR_TestKeyHints).
  if (System.IsCLIParam("acrKeyHintsTest")) ACR_TestKeyHints.Run(seconds);
  // -acrQuickStartFIATest: FIA's cells on a Quick start (ACR_TestQuickStartFIA).
  if (System.IsCLIParam("acrQuickStartFIATest")) ACR_TestQuickStartFIA.Run(seconds);
  // -acrGuerrillasTest: FIA guerrilla cells (ACR_TestGuerrillas, #89).
  if (System.IsCLIParam("acrGuerrillasTest")) ACR_TestGuerrillas.Run(seconds);
  // -acrInsurgencyTest: FIA allegiance and dangerous roads (ACR_TestInsurgency, #89).
  if (System.IsCLIParam("acrInsurgencyTest")) ACR_TestInsurgency.Run(seconds);
  // -acrFIALeaningTest: FIA leans by conduct (ACR_TestFIALeaning, #89 part 2).
  if (System.IsCLIParam("acrFIALeaningTest")) ACR_TestFIALeaning.Run(seconds);
  // -acrHideoutsTest: FIA hideouts (ACR_TestHideouts, #89).
  if (System.IsCLIParam("acrHideoutsTest")) ACR_TestHideouts.Run(seconds);
  // -acrFIASpoilsTest: FIA loot, stolen vehicles and prisoners (ACR_TestFIASpoils, insurgency part 4).
  if (System.IsCLIParam("acrFIASpoilsTest")) ACR_TestFIASpoils.Run(seconds);
  // -acrFIAGrievanceTest: harm to civilians and towns grows FIA (ACR_TestFIAGrievance).
  if (System.IsCLIParam("acrFIAGrievanceTest")) ACR_TestFIAGrievance.Run(seconds);
  // -acrZombiesTest: zombie nests with Project Outbreak, or nothing without it (ACR_TestOutbreak, #107).
  if (System.IsCLIParam("acrZombiesTest")) ACR_TestOutbreak.Run(seconds);
  // -acrTvZTest: one side against the zombies (ACR_TestTvZ, #107).
  if (System.IsCLIParam("acrTvZTest")) ACR_TestTvZ.Run(seconds);
  // -acrCoinTest / -acrSupportTest: counter-insurgency and town support (ACR_TestCoin, #244).
  if (System.IsCLIParam("acrCoinTest")) ACR_TestCoin.RunCoin(seconds);
  if (System.IsCLIParam("acrSupportTest")) ACR_TestCoin.RunSupport(seconds);
  if (System.IsCLIParam("acrCoinGrowthTest")) ACR_TestCoinGrowth.Run(seconds);
  if (System.IsCLIParam("acrCoinAITest")) ACR_TestCoinAI.Run(seconds);
  if (System.IsCLIParam("acrFiaSideTest")) ACR_TestFiaSide.Run(seconds);
  if (System.IsCLIParam("acrFiaHomeTest")) ACR_TestFiaHome.Run(seconds);
  if (System.IsCLIParam("acrFiaNetworkTest")) ACR_TestFiaNetwork.Run(seconds);
  // -acrContainmentTest: ground zero, survivors and rescues (ACR_TestContainment, #107).
  if (System.IsCLIParam("acrContainmentTest")) ACR_TestContainment.Run(seconds);
  if (System.IsCLIParam("acrGenerateTest")) ACR_TestGenerate.Run(seconds);
  if (System.IsCLIParam("acrGenerateEveronTest")) ACR_TestGenerate.RunEveron(seconds);
  if (System.IsCLIParam("acrGenerateMapTest")) ACR_TestGenerate.RunAnyMap(seconds);
  // -acrPocketsTest: FIA pockets of resistance (ACR_TestPockets).
  if (System.IsCLIParam("acrPocketsTest")) ACR_TestPockets.Run(seconds);
  // -acrQuickStartTest: the map already split between the sides (ACR_TestQuickStart).
  if (System.IsCLIParam("acrQuickStartTest")) ACR_TestQuickStart.Run(seconds);
  // -acrStuckMenTest: a squad doesn't keep stopping for men who can't walk (ACR_TestStuckMen).
  if (System.IsCLIParam("acrStuckMenTest")) ACR_TestStuckMen.Run(seconds);
  // -acrBaseDefencesTest: the fortification planner on Everon (ACR_TestBaseDefences).
  if (System.IsCLIParam("acrBaseDefencesTest")) ACR_TestBaseDefences.Run(seconds);
  if (System.IsCLIParam("acrBaseDefencesSurveyTest")) ACR_TestBaseDefences.RunSurvey(seconds);
  if (System.IsCLIParam("acrBaseDefencesModelsTest")) ACR_TestBaseDefences.RunModels(seconds);
  // -acrPositionSitingArlandTest: no Fortify spot on Arland's runway (ACR_TestPositionSiting).
  if (System.IsCLIParam("acrPositionSitingArlandTest")) ACR_TestPositionSiting.RunArland(seconds);
 }

 // Game tests (launched straight into a world) deploy their player at once, as the old
 // auto-spawn did; -acrDeployMenuTest keeps the deploy screen (GitHub #39). The launcher's
 // play launches pass -world too, with -acrSetup: those are real matches, deploy screen on
 // (7 Nov: Jacob, solo on Zimnitrita, never got back after his command unit died).
 override string AutoDeploySide()
 {
  if (!System.IsCLIParam("world") || System.IsCLIParam("acrSetup") || System.IsCLIParam("acrDeployMenuTest")) return "";
  return "west";
 }

 // The soaks play unattended: on a pak: server (started from the mission header, setup phase on)
 // they waited for a Start nobody pressed and ran 2 h with no squads (11 Nov, #232 A/B).
 // The presets test keeps its files out of the real presets folder (ACR_SetupPresets.Dir).
 override string SetupPresetDir()
 {
  if (ACR_TestSetupPresets.Wanted()) return ACR_TestSetupPresets.DIR;
  return "";
 }

 override bool SkipSetup()
 {
  if (System.IsCLIParam("acrSoakTest") || System.IsCLIParam("acrPerfTest") || System.IsCLIParam("acrPerfScaleTest") || System.IsCLIParam("acrAllocPerfTest") || System.IsCLIParam("acrAllocControlPerfTest")) return true;
  if (System.IsCLIParam("acrEveronSoakTest") || System.IsCLIParam("acrForecastSoakTest") || System.IsCLIParam("acrAllocSoakTest") || System.IsCLIParam("acrAllocControlSoakTest") || System.IsCLIParam("acrClassicSoakTest")) return true;
  return System.IsCLIParam("acrForecastSkirmishTest") || System.IsCLIParam("acrLongSoakTest") || System.IsCLIParam("acrAllocControlLongSoakTest") || System.IsCLIParam("acrClassicLongSoakTest") || System.IsCLIParam("acrWorksAISoakTest");
 }

 // Tests keep their worlds as authored; the guerrilla test and the balance soaks get guerrillas.
 override string Guerrillas()
 {
  if (System.IsCLIParam("acrQuickStartFIATest") || System.IsCLIParam("acrWorksAISoakTest")) return "many";
  if (System.IsCLIParam("acrGuerrillasTest") || System.IsCLIParam("acrInsurgencyTest") || System.IsCLIParam("acrFIALeaningTest") || System.IsCLIParam("acrHideoutsTest") || System.IsCLIParam("acrFIASpoilsTest") || System.IsCLIParam("acrFIAGrievanceTest") || System.IsCLIParam("acrEveronSoakTest") || System.IsCLIParam("acrForecastSoakTest") || System.IsCLIParam("acrAllocSoakTest") || System.IsCLIParam("acrAllocControlSoakTest") || System.IsCLIParam("acrLongSoakTest") || System.IsCLIParam("acrAllocControlLongSoakTest") || System.IsCLIParam("acrClassicSoakTest") || System.IsCLIParam("acrClassicLongSoakTest")) return "few";
  return "";
 }

 // Only the zombie test asks for zombies (and only Outbreak worlds have any).
 override string Zombies()
 {
  if (System.IsCLIParam("acrZombiesTest")) return "many";
  return "";
 }

 override string MatchType()
 {
  if (System.IsCLIParam("acrCoinTest") || System.IsCLIParam("acrSupportTest") || System.IsCLIParam("acrCoinGrowthTest") || System.IsCLIParam("acrCoinAITest") || System.IsCLIParam("acrFiaSideTest") || System.IsCLIParam("acrFiaHomeTest") || System.IsCLIParam("acrFiaNetworkTest")) return "coin";
  return "";
 }

 override string CoinInsurgency()
 {
  if (System.IsCLIParam("acrFiaSideTest") || System.IsCLIParam("acrFiaHomeTest") || System.IsCLIParam("acrFiaNetworkTest")) return "players";
  return "";
 }

 override string ZombieMode()
 {
  if (System.IsCLIParam("acrTvZTest")) return "tvz";
  return "";
 }

 override string Outbreak()
 {
  if (System.IsCLIParam("acrContainmentTest")) return "contain";
  return "";
 }

 // Tests keep their worlds as authored; the pocket test and the balance soaks get pockets.
 override string Pockets()
 {
  if (System.IsCLIParam("acrPocketsTest") || System.IsCLIParam("acrQuickStartTest") || System.IsCLIParam("acrInsurgencyTest") || System.IsCLIParam("acrFIALeaningTest") || System.IsCLIParam("acrHideoutsTest") || System.IsCLIParam("acrFIASpoilsTest") || System.IsCLIParam("acrFIAGrievanceTest") || System.IsCLIParam("acrEveronSoakTest") || System.IsCLIParam("acrForecastSoakTest") || System.IsCLIParam("acrAllocSoakTest") || System.IsCLIParam("acrAllocControlSoakTest") || System.IsCLIParam("acrLongSoakTest") || System.IsCLIParam("acrAllocControlLongSoakTest") || System.IsCLIParam("acrClassicSoakTest") || System.IsCLIParam("acrClassicLongSoakTest")) return "few";
  if (System.IsCLIParam("acrSetupTest") || System.IsCLIParam("acrSetupUiTest") || System.IsCLIParam("acrSetupGenTest") || System.IsCLIParam("acrSetupMapTest")) return "off";
  return "";
 }

 override string QuickStart()
 {
  if (System.IsCLIParam("acrQuickStartTest") || System.IsCLIParam("acrQuickStartFIATest") || System.IsCLIParam("acrWorksAISoakTest")) return "even";
  return "";
 }

 // Tests keep the standard commander (a random doctrine would make runs differ); a test
 // that wants one sets -acrDoctrineWest/-acrDoctrineEast or ACR_Options.
 override string AIDoctrine(string factionKey)
 {
  return "balanced";
 }

 override string SkirmishLayout()
 {
  if (ACR_TestSkirmish.Wanted()) return ACR_TestSkirmish.Layout();
  return "";
 }
}
