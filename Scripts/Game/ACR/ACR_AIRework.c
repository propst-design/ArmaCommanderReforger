// The AI commander rework (#232): lanes (ACR_AILanes), the job allocator (ACR_AICommander.AllocatorMode),
// the defence forecast with screens (ACR_DefenceForecast, ACR_DefenceScreen) and covered fall-back
// (ACR_Squad.CoveredFallBackOn). On by default from 0.6.0 (they were the 0.5.9 Experimental tab, off).
// One switch keeps the commander from before for one release so players can compare (#232 "Done when"):
// -acrAICommander classic (setup row "AI commander", mission header m_sAcrAICommander). Each part's own
// flag still wins over it (-acrAIStrategy, -acrAIAllocator, -acrDefenceForecast, -acrCoveredFallBack),
// so one part can be tried alone. Once the new paths have been on by default for a release, the old
// paths and the timers they replaced go (#232), and this class with them.
class ACR_AIRework
{
 // -acrAICommander: "new" (default) or "classic". "old" and "0" read as classic too.
 static bool Classic()
 {
  string value = ACR_LogisticsRules.Choice("acrAICommander", "new");
  return value == "classic" || value == "old" || value == "0";
 }

 // The default of one of the rework's own flags: its value in the new commander, "0" in the classic one.
 static string Default(string newValue)
 {
  if (Classic()) return "0";
  return newValue;
 }
}
