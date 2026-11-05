// -acrFlyDemoTest 1 (game client, ~2 min): ACA's four jets fly straight over the player's character,
// 300 m up, one every GAP_S seconds (RF-4C, Su-17M4R, A-10A, Su-25), each on its own heading, so a person
// can look and listen. Checks that each passed within 350 m of the character, visible, with both engine
// layers playing as it went over. First each is parked side-on 60 m in front of the character for a screenshot
// ($profile:acr-shots/flydemo_<jet>.png), to see that it is drawn, then an A-10 crosses 400 m in front at 80 m, over the trees
// (flydemo_crossing.png), to see that a moving one is drawn where it is.
class ACR_TestFlyDemo
{
 static const int FIRST_TICK = 26; // after the screenshots (ticks 4-22)
 static const int CROSS_TICK = 17;  // a jet crossing in front, low, for a screenshot of a moving one
 static const int GAP_S = 15;
 static const float SPEED = 200;
 static const float LEAD_S = 28;   // from spawn to overhead: 5.6 km out, inside the far sound's 5.8 km start
 static const float HEIGHT = 300;
 static ref array<ref ACR_LocalFlyover> s_Flights;
 static ref array<float> s_Closest;
 static IEntity s_Parked;

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
  }
  int shot = tick - 4;
  if (shot >= 0 && shot < 12)
  {
   if (shot % 3 == 0) Park(shot / 3);
   if (shot % 3 == 2) Shoot(shot / 3);
  }
  if (tick == CROSS_TICK) Cross();
  if (tick == CROSS_TICK + 5)
  {
   FileIO.MakeDirectory("$profile:acr-shots");
   System.MakeScreenshot("$profile:acr-shots/flydemo_crossing");
   ACR_LocalFlyover crossing = ACR_LocalFlyover.Latest();
   if (crossing && crossing.m_Plane) PrintFormat("[ACR] FLYDEMO shot crossing at %1", crossing.m_Plane.GetOrigin());
  }
  if (tick == FIRST_TICK + 3 * GAP_S + LEAD_S + 21) ACR_TestRegress.Complete("flydemo", 1);
 }

 static void Park(int index)
 {
  array<ResourceName> jets = {ACR_ReconRun.RECON_JET_WEST, ACR_ReconRun.RECON_JET_EAST, ACR_Supports.BOMB_JET_WEST, ACR_Supports.BOMB_JET_EAST};
  PlayerController controller = GetGame().GetPlayerController();
  if (!controller || !controller.GetControlledEntity()) return;
  IEntity body = controller.GetControlledEntity();
  vector m[4];
  body.GetTransform(m);
  vector forward = m[2];
  forward[1] = 0;
  forward.Normalize();
  // SpawnLocal points the model's +Z along `forward`; the nose is +X, so the jet shows its side
  s_Parked = ACR_Supports.SpawnLocal(jets[index], body.GetOrigin() + forward * 60 + "0 6 0", forward);
 }

 static void Cross()
 {
  PlayerController controller = GetGame().GetPlayerController();
  if (!controller || !controller.GetControlledEntity()) return;
  IEntity body = controller.GetControlledEntity();
  vector m[4];
  body.GetTransform(m);
  vector forward = m[2];
  forward[1] = 0;
  forward.Normalize();
  vector right = Vector(forward[2], 0, -forward[0]);
  vector centre = body.GetOrigin() + forward * 400;
  float altitude = GetGame().GetWorld().GetSurfaceY(centre[0], centre[2]) + 80;
  vector start = centre - right * (SPEED * 5);
  start[1] = altitude;
  ACR_LocalFlyover.Start(ACR_Supports.BOMB_JET_WEST, start, right, SPEED, altitude, 8);
 }

 static void Shoot(int index)
 {
  array<string> names = {"RF4C", "Su17M4R", "A10A", "Su25"};
  FileIO.MakeDirectory("$profile:acr-shots");
  System.MakeScreenshot("$profile:acr-shots/flydemo_" + names[index]);
  PrintFormat("[ACR] FLYDEMO shot %1 parked %2", names[index], s_Parked != null);
  if (s_Parked) SCR_EntityHelper.DeleteEntityAndChildren(s_Parked);
  s_Parked = null;
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
