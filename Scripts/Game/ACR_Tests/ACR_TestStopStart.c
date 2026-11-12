// -acrStopStartTest 1 (server-capable, ~25 s): a second match in one session (broad review #8). ACR_TestRestart
// restarts the scenario and counts registries; this one calls ACR_Manager.Stop and Start directly, so the per-match
// leftovers Stop should clear (the 2IC forecast, #7; intro; battery log; civilian feed; /acr unbound) are checked by name.
// The match runs a few seconds, state is seeded, Stop must clear it, and Start must bring a manager that ticks again.
class ACR_TestStopStart
{
 static const int CHECKS = 5;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick == 8)
  {
   bool running = ACR_Manager.GetInstance() != null;
   // What a match leaves behind: an open 2IC look, a waiting intro, battery impacts, /acr unbound.
   ACR_TwoICForecast.MarkLookedForTest("US");
   ACR_Intro.Receive("0 0 0", "test", "", false);
   ACR_Supports.s_aBatteryImpacts.Insert(Vector(1, 0, 1));
   ACR_Supports.s_iBatteryImpacts = 1;
   ACR_RTSCameraManager.s_bAdminUnbound = true;
   bool seeded = ACR_TwoICForecast.StateCount() > 0 && ACR_Intro.HasLocal();
   PrintFormat("[ACR] STOPSTART stopping the match (running %1, seeded %2)", running, seeded);
   ACR_Manager.Stop();

   ACR_TestRegress.Check("stopstart: Stop ends the manager", running && seeded && ACR_Manager.GetInstance() == null, string.Format("running %1, seeded %2", running, seeded));
   int forecast = ACR_TwoICForecast.StateCount();
   ACR_TestRegress.Check("stopstart: the 2IC defence advice is cleared (no stale base, a fresh look)", forecast == 0, string.Format("%1 left", forecast));
   bool intro = !ACR_Intro.HasLocal() && !ACR_Intro.LocalDone();
   bool supports = ACR_Supports.s_aBatteryImpacts.IsEmpty() && ACR_Supports.s_iBatteryImpacts == 0;
   int pending = ACR_CivilianSocket.Get().PendingDeaths();
   ACR_TestRegress.Check("stopstart: intro, battery log, civilian feed batch and /acr unbound are cleared", intro && supports && pending == 0 && !ACR_RTSCameraManager.s_bAdminUnbound, string.Format("intro %1, supports %2, civilian batch %3, unbound %4", intro, supports, pending, ACR_RTSCameraManager.s_bAdminUnbound));
   ACR_Manager.Start();
   ACR_TestRegress.Check("stopstart: Start brings a new manager", ACR_Manager.GetInstance() != null, "");
   return;
  }
  if (tick == 16)
  {
   ACR_Manager manager = ACR_Manager.GetInstance();
   bool ticks = manager && manager.HostOptionsApplied();
   ACR_TestRegress.Check("stopstart: the new match ticks (host options applied on its first tick)", ticks, string.Format("manager %1", manager != null));
   ACR_TestRegress.Complete("stopstart", CHECKS);
  }
 }
}
