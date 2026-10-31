// Connects the tests to the game code (see ACR/ACR_TestHooks.c). Not in the player package.
modded class ACR_TestHooks
{
 protected ref ACR_TestHarness m_Harness;

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
  // -acrPadsTest: helipads on this scenario's map (ACR_TestPads).
  if (System.IsCLIParam("acrPadsTest") || System.IsCLIParam("acrPadsLevieTest") || System.IsCLIParam("acrPadsMontignacTest") || System.IsCLIParam("acrPadsPhilippeTest")) ACR_TestPads.Run(null, seconds);
  // -acrSurveyTest: object/terrain dump for placing bases and pads (ACR_TestSurvey).
  if (System.IsCLIParam("acrSurveyTest")) ACR_TestSurvey.Run(null, seconds);
  // -acrPlacesTest: named map locations, for laying out a new map (ACR_TestPlaces).
  if (System.IsCLIParam("acrPlacesTest")) ACR_TestPlaces.Run(seconds);
  // -acrSetupTest: the setup phase (ACR_TestSetup).
  if (ACR_TestSetup.Wanted()) ACR_TestSetup.Run(seconds);
  if (ACR_TestSetup.UiWanted()) ACR_TestSetup.RunUi(seconds);
  if (ACR_TestSetup.ReserveWanted()) ACR_TestSetup.RunReserve(seconds);
  if (ACR_TestSetup.GenWanted()) ACR_TestSetup.RunGen(seconds);
  if (ACR_TestSetup.MapWanted()) ACR_TestSetup.RunMap(seconds);
  if (System.IsCLIParam("acrEveronSurveyTest")) ACR_TestPlaces.SurveyEveron(seconds);
  if (System.IsCLIParam("acrEveronPadsTest")) ACR_TestPads.SurveyEveron(seconds);
  // -acrSkirmishTest / -acrSkirmishSouthTest: skirmish layouts on Everon (ACR_TestSkirmish).
  if (System.IsCLIParam("acrSkirmishTest") || System.IsCLIParam("acrSkirmishSouthTest")) ACR_TestSkirmish.Run(seconds);
  // -acrGenerateTest / -acrGenerateEveronTest: the scenario generator (ACR_TestGenerate).
  if (System.IsCLIParam("acrLiftClearanceTest")) ACR_TestLiftClearance.Run(seconds);
  if (System.IsCLIParam("acrLzBenchTest")) ACR_TestLzBench.Run(seconds);
  // -acrShoreRoutesTest: BBB land routes keep legs off the shore (ACR_TestShoreRoutes).
  if (System.IsCLIParam("acrShoreRoutesTest")) ACR_TestShoreRoutes.Run(seconds);
  if (System.IsCLIParam("acrSlopesTest")) ACR_TestSlopes.Run(seconds);
  // -acrPathProbeTest: can foot squads reach Gallows Base's centre (ACR_TestPathProbe).
  if (System.IsCLIParam("acrPathProbeTest")) ACR_TestPathProbe.Run(seconds);
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
 // auto-spawn did; -acrDeployMenuTest keeps the deploy screen (GitHub #39).
 override string AutoDeploySide()
 {
  if (!System.IsCLIParam("world") || System.IsCLIParam("acrDeployMenuTest")) return "";
  return "west";
 }

 // Tests keep their worlds as authored; the guerrilla test and the balance soaks get guerrillas.
 override string Guerrillas()
 {
  if (System.IsCLIParam("acrQuickStartFIATest")) return "many";
  if (System.IsCLIParam("acrGuerrillasTest") || System.IsCLIParam("acrInsurgencyTest") || System.IsCLIParam("acrFIALeaningTest") || System.IsCLIParam("acrHideoutsTest") || System.IsCLIParam("acrEveronSoakTest") || System.IsCLIParam("acrLongSoakTest")) return "few";
  return "";
 }

 // Tests keep their worlds as authored; the pocket test and the balance soaks get pockets.
 override string Pockets()
 {
  if (System.IsCLIParam("acrPocketsTest") || System.IsCLIParam("acrQuickStartTest") || System.IsCLIParam("acrInsurgencyTest") || System.IsCLIParam("acrFIALeaningTest") || System.IsCLIParam("acrHideoutsTest") || System.IsCLIParam("acrEveronSoakTest") || System.IsCLIParam("acrLongSoakTest")) return "few";
  if (System.IsCLIParam("acrSetupTest") || System.IsCLIParam("acrSetupUiTest") || System.IsCLIParam("acrSetupGenTest") || System.IsCLIParam("acrSetupMapTest")) return "off";
  return "";
 }

 override string QuickStart()
 {
  if (System.IsCLIParam("acrQuickStartTest") || System.IsCLIParam("acrQuickStartFIATest")) return "even";
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
