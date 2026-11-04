// -acrFlyDemoTest 1 (game client, ~1.7 min): ACA's four jets fly straight over the player's character,
// 300 m up, one every GAP_S seconds (RF-4C, Su-17M4R, A-10A, Su-25), each on its own heading, so a person
// can look and listen. Checks that each passed within 350 m of the character, visible, with both engine
// layers playing as it went over.
class ACR_TestFlyDemo
{
 static const int FIRST_TICK = 8;
 static const int GAP_S = 15;
 static const float SPEED = 200;
 static const float LEAD_S = 28;   // from spawn to overhead: 5.6 km out, inside the far sound's 5.8 km start
 static const float HEIGHT = 300;
 static ref array<ref ACR_LocalFlyover> s_Flights;
 static ref array<float> s_Closest;
 static int s_iGood;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick == 1)
  {
   s_Flights = {};
   s_Closest = {};
  }
  int k = tick - FIRST_TICK;
  if (k >= 0 && k % GAP_S == 0 && k / GAP_S < 4) Fly(k / GAP_S);
  vector player;
  bool here = ACR_LocalFlyover.Listener(player);
  foreach (int i, ACR_LocalFlyover flight : s_Flights)
  {
   if (!here || !flight.m_Plane) continue;
   float d = vector.Distance(player, flight.m_Plane.GetOrigin());
   if (d < s_Closest[i])
   {
    s_Closest[i] = d;
    // overhead: both layers must be playing by now, and the model drawn
    if (d < 350 && flight.m_hEngine != AudioHandle.Invalid && flight.m_hEngineFar != AudioHandle.Invalid && (flight.m_Plane.GetFlags() & EntityFlags.VISIBLE) && flight.m_Plane.GetVObject())
     flight.m_bDemoOverhead = true;
   }
  }
  if (tick == FIRST_TICK + 3 * GAP_S + LEAD_S + 20)
  {
   string report = "";
   int good = 0;
   foreach (int j, ACR_LocalFlyover f : s_Flights)
   {
    if (f.m_bDemoOverhead) good++;
    report += string.Format(" %1 closest %2 m;", j, Math.Round(s_Closest[j]));
   }
   ACR_TestRegress.Check("flydemo: four jets passed over the player's head, drawn, both engine layers playing", s_Flights.Count() == 4 && good == 4, string.Format("good %1 of %2:%3", good, s_Flights.Count(), report));
   ACR_TestRegress.Complete("flydemo", 1);
  }
 }

 static void Fly(int index)
 {
  array<ResourceName> jets = {ACR_ReconRun.RECON_JET_WEST, ACR_ReconRun.RECON_JET_EAST, ACR_Supports.BOMB_JET_WEST, ACR_Supports.BOMB_JET_EAST};
  vector over;
  if (!ACR_LocalFlyover.Listener(over)) return;
  float heading = index * 70 * Math.DEG2RAD;
  vector dir = Vector(Math.Sin(heading), 0, Math.Cos(heading));
  float altitude = GetGame().GetWorld().GetSurfaceY(over[0], over[2]) + HEIGHT;
  vector start = over - dir * (SPEED * LEAD_S);
  start[1] = altitude;
  ACR_LocalFlyover.Start(jets[index], start, dir, SPEED, altitude, LEAD_S + 2500 / SPEED);
  s_Flights.Insert(ACR_LocalFlyover.Latest());
  s_Closest.Insert(1000000);
  PrintFormat("[ACR] FLYDEMO %1 over %2 in %3 s, heading %4", FilePath.StripPath(jets[index]), over, LEAD_S, index * 70);
 }
}
