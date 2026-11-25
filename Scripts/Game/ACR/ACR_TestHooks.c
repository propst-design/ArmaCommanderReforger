// Test entry points (R17, Jacob 14 Oct: the player download leaves the test scripts out).
// make_package.py skips Scripts/Game/ACR_Tests and Scripts/Game/BBB/*Test.c; game code
// reaches the tests only through this class, which ACR_Tests/ACR_TestHooksImpl.c mods.
// Without the tests every hook does nothing.
class ACR_TestHooks
{
 protected static ref ACR_TestHooks s_Instance;

 static ACR_TestHooks Get()
 {
  if (!s_Instance) s_Instance = new ACR_TestHooks();
  return s_Instance;
 }

 // Test range entity started (server) / removed.
 void RangeStarted(ACR_TestRange range) {}
 void RangeEnded(ACR_TestRange range) {}
 // Every second of a scenario (ACR_Mode.Second).
 void ScenarioSecond(int seconds) {}
 // Layout a skirmish test asks for, "" = none.
 string SkirmishLayout() { return ""; }
 // FIA pockets a test asks for (few / many / off), "" = the host option (ACR_Pockets).
 string Pockets() { return ""; }
 // Quick start a test asks for (even / uneven), "" = the host option (ACR_QuickStart).
 string QuickStart() { return ""; }
 // FIA guerrillas a test asks for (few / many / off), "" = the host option (ACR_Guerrillas).
 string Guerrillas() { return ""; }
 // Zombies a test asks for (few / many / off), "" = the host option (ACR_Outbreak).
 string Zombies() { return ""; }
 // Zombie mode a test asks for (tvtvz / tvz), "" = the host option (ACR_TvZ).
 string ZombieMode() { return ""; }
 // Match type a test asks for (standard / coin), "" = the host option (ACR_Coin).
 string MatchType() { return ""; }
 // Insurgency run-by mode a test asks for (rules / players / ai), "" = the host option (ACR_InsurgentSide).
 string CoinInsurgency() { return ""; }
 // Outbreak start a test asks for (nests / contain), "" = the host option (ACR_Containment).
 string Outbreak() { return ""; }
 // AI doctrine a test asks for (ACR_AIDoctrine key), "" = the host option or random.
 string AIDoctrine(string factionKey) { return ""; }
 // Side a test's player deploys on without the deploy screen ("west" / "east"), "" = the menu.
 string AutoDeploySide() { return ""; }
 // A test that plays a match unattended (the soaks) skips the setup phase the mission header asks for:
 // pak: servers start from the shipped header, and nobody presses Start (ACR_Setup.Wanted).
 bool SkipSetup() { return false; }
 // Setup presets' folder for a test (ACR_SetupPresets.Dir), "" = profile/ACA/SetupPresets: the
 // test's files and auto-load mark never reach the presets every other test run reads.
 string SetupPresetDir() { return ""; }
 // A side a test says a player commands (ACR_SquadAI.RunsAll): a server test has no players.
 bool PlayerCommandsForTest(string factionKey) { return false; }
 // The value an option takes when no source sets it, for a test that needs another than the game's
 // (fallback): the range keeps the 2IC off (ACR_SquadAI.TwoIC), its tests script the squads themselves.
 string OptionFallback(string option, string fallback) { return fallback; }
 // Build the test classes' statics (ACR_StaticInit, at game creation).
 void InitStatics() {}
}
