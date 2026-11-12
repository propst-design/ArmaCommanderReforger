// Host options resolution (ACR_Options, 13 Oct), run from srv:regress at tick 4.
// Header values come from a test map (the srv: test server starts from a world, so the
// real mission header may be missing); the real header's class is reported when present.
class ACR_TestOptions
{
 static const int CHECKS = 5;

 static void Run()
 {
  map<string, string> header = new map<string, string>();
  ACR_MissionHeader fields = ACR_MissionHeader.Cast(GetGame().GetMissionHeader());
  if (GetGame().GetMissionHeader())
   ACR_TestRegress.Check("options: our scenarios use ACR_MissionHeader", fields != null, GetGame().GetMissionHeader().ClassName());
  else
   ACR_TestRegress.Skip("options: our scenarios use ACR_MissionHeader", "no mission header on this server (started from a world)");

  // The flag this regress run was started with (regressdriver runs it with the scripted driver on).
  string flag = "acrRegressTest";
  if (System.IsCLIParam("acrRegressDriverTest")) flag = "acrRegressDriverTest";
  header.Set("acrThreat", "high");
  header.Set("acrGameMasterAll", "0");
  header.Set(flag, "header");
  ACR_Options.UseHeaderForTest(header);
  string value;
  bool fromHeader = ACR_Options.Get("acrThreat", value) && value == "high" && ACR_Options.Source("acrThreat") == "header";
  ACR_TestRegress.Check("options: a header value applies when nothing else sets it", fromHeader, string.Format("acrThreat=%1 (%2)", value, ACR_Options.Source("acrThreat")));

  string flagValue;
  bool flagWins = ACR_Options.Get(flag, flagValue) && flagValue == "1" && ACR_Options.Source(flag) == "flag";
  ACR_TestRegress.Check("options: a command-line flag beats the header", flagWins, string.Format("%1=%2 (%3)", flag, flagValue, ACR_Options.Source(flag)));

  ACR_Options.Set("acrThreat", "low");
  ACR_Options.Set(flag, "screen");
  string screenThreat, screenFlag;
  ACR_Options.Get("acrThreat", screenThreat);
  ACR_Options.Get(flag, screenFlag);
  bool screenWins = screenThreat == "low" && screenFlag == "screen" && ACR_Options.Source("acrThreat") == "screen";
  ACR_Options.ClearSet();
  string after;
  ACR_Options.Get("acrThreat", after);
  ACR_TestRegress.Check("options: the setup screen beats flag and header, and clears", screenWins && after == "high", string.Format("screen %1/%2, after clear %3", screenThreat, screenFlag, after));

  bool offFlag = !ACR_Options.Flag("acrGameMasterAll");
  bool unset = !ACR_Options.Get("acrLayout", value) && value == "" && ACR_Options.Source("acrLayout") == "";
  ACR_TestRegress.Check("options: header 0 turns a flag off; unset options stay unset", offFlag && unset, string.Format("gmAll %1, layout '%2'", ACR_Options.Flag("acrGameMasterAll"), value));
  ACR_Options.UseHeaderForTest(null);
  ACR_Options.Log();
 }
}
