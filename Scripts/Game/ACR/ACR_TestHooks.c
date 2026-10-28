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
 // AI doctrine a test asks for (ACR_AIDoctrine key), "" = the host option or random.
 string AIDoctrine(string factionKey) { return ""; }
 // Side a test's player deploys on without the deploy screen ("west" / "east"), "" = the menu.
 string AutoDeploySide() { return ""; }
}
