// -acrFlyDemoTest 1 (game client, ~2 min): ACA's four jets fly straight over the player's character,
// 300 m up, one every GAP_S seconds (RF-4C, Su-17M4R, A-10A, Su-25), each on its own heading, so a person
// can look and listen. Checks that each passed within 350 m of the character, visible, with both engine
// layers playing as it went over. First each is parked side-on 60 m in front of the character for a screenshot
// ($profile:acr-shots/flydemo_<jet>.png), to see that it is drawn, then an A-10 crosses 400 m in front at 80 m, over the trees
// (flydemo_crossing.png), to see that a moving one is drawn where it is. Last, the A-10 then the Su-25 bomb a
// spot 220 m in front of the character, flying across at 150 m: four bombs off their pylons, falling onto local
// blasts (sight and sound only: nothing is hit), with screenshots mid-fall (flydemo_bombs_<jet>_<n>.png). Closer
// and lower than a real run (300 m up), where a 2 m bomb is a few pixels: Jacob didn't see them at 450 m (4 Nov).
class ACR_TestFlyDemo
{
 static const int FIRST_TICK = 26; // after the screenshots (ticks 4-22)
 static const int CROSS_TICK = 17;  // a jet crossing in front, low, for a screenshot of a moving one
 static const int GAP_S = 15;
 static const float SPEED = 200;
 static const float LEAD_S = 28;   // from spawn to overhead: 5.6 km out, inside the far sound's 5.8 km start
 static const float HEIGHT = 300;
 static const int BOMB_TICK = 121; // after the overhead passes (check at tick 119)
 static const int BOMB_GAP = 22;
 static const float BOMB_LEAD_S = 22;
 static const float BOMB_HEIGHT = 150;
 static const float BOMB_AHEAD = 220;
 static ref array<ref ACR_LocalFlyover> s_Bombers;
 static ref array<int> s_Hung;
 static ref array<ref ACR_LocalFlyover> s_Flights;
 static ref array<float> s_Closest;
 static IEntity s_Parked;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick == 1)
  {
   s_Flights = {};
   s_Closest = {};
   s_Bombers = {};
   s_Hung = {};
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
  if (tick == BOMB_TICK) Bomb(0);
  if (tick == BOMB_TICK + BOMB_GAP) Bomb(1);
  if (tick == BOMB_TICK + 2 * BOMB_GAP + 8)
  {
   string bombs = "";
   int dropped = 0;
   foreach (int b, ACR_LocalFlyover bomber : s_Bombers)
   {
    int landed = 0;
    if (bomber) landed = bomber.m_iLanded; // counted as they land: a jet gone out of range has cleared its list
    if (s_Hung[b] == 4 && landed == 4) dropped++;
    bombs += string.Format(" %1: hung %2, landed %3;", b, s_Hung[b], landed);
   }
   ACR_TestRegress.Check("flydemo: the A-10 and Su-25 each carried four bombs on their pylons and dropped them onto the spot", s_Bombers.Count() == 2 && dropped == 2, bombs);
  }
  if (tick == BOMB_TICK + 2 * BOMB_GAP + 9) ACR_TestRegress.Complete("flydemo", 2);
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

 // A bomb run across the character's front: four bombs 28 m apart around a spot BOMB_AHEAD ahead, each with
 // a local blast when it lands, and two screenshots while the first falls.
 static void Bomb(int index)
 {
  array<ResourceName> jets = {ACR_Supports.BOMB_JET_WEST, ACR_Supports.BOMB_JET_EAST};
  PlayerController controller = GetGame().GetPlayerController();
  if (!controller || !controller.GetControlledEntity()) return;
  IEntity body = controller.GetControlledEntity();
  vector m[4];
  body.GetTransform(m);
  vector forward = m[2];
  forward[1] = 0;
  forward.Normalize();
  vector right = Vector(forward[2], 0, -forward[0]);
  vector target = ACR_Manager.Ground(body.GetOrigin() + forward * BOMB_AHEAD);
  float altitude = target[1] + BOMB_HEIGHT;
  vector start = target - right * (SPEED * BOMB_LEAD_S);
  start[1] = altitude;
  ACR_LocalFlyover.Start(jets[index], start, right, SPEED, altitude, BOMB_LEAD_S + 2500 / SPEED);
  ACR_LocalFlyover bomber = ACR_LocalFlyover.Latest();
  s_Bombers.Insert(bomber);
  int hung = 0;
  array<vector> pylons = {};
  ResourceName dropped = ACR_LocalFlyover.Hardpoints(jets[index], pylons);
  for (int b = 0; b < 4 && bomber; b++)
  {
   float along = (b - 1.5) * 28;
   vector impact = target + right * along;
   // as the server times it: a Snakeye lands after the jet has passed
   float drop = altitude - ACR_FallingBomb.PYLON_DROP - GetGame().GetWorld().GetSurfaceY(impact[0], impact[2]);
   float at = BOMB_LEAD_S + along / SPEED + ACR_FallingBomb.Lag(dropped, drop, SPEED);
   bomber.AddBomb(impact, at);
   if (bomber.m_aBombs[b].m_bHung && bomber.m_aBombs[b].m_Body) hung++;
   GetGame().GetCallqueue().CallLater(Blast, Math.Round(at * 1000), false, impact);
  }
  s_Hung.Insert(hung);
  if (bomber && !bomber.m_aBombs.IsEmpty())
  {
   ACR_FallingBomb first = bomber.m_aBombs[0];
   float fall = first.m_fImpactAt - first.m_fReleaseAt;
   array<string> names = {"A10A", "Su25"};
   GetGame().GetCallqueue().CallLater(BombShot, Math.Round((first.m_fReleaseAt + 0.4) * 1000), false, names[index] + "_1", bomber);
   GetGame().GetCallqueue().CallLater(BombShot, Math.Round((first.m_fReleaseAt + fall * 0.6) * 1000), false, names[index] + "_2", bomber);
  }
  PrintFormat("[ACR] FLYDEMO bomb run %1 on %2, %3 bombs hung", FilePath.StripPath(jets[index]), target, hung);
 }

 static void BombShot(string name, ACR_LocalFlyover bomber)
 {
  FileIO.MakeDirectory("$profile:acr-shots");
  System.MakeScreenshot("$profile:acr-shots/flydemo_bombs_" + name);
  string where = "";
  if (bomber) foreach (ACR_FallingBomb bomb : bomber.m_aBombs) if (bomb.m_Body) where += string.Format(" %1 falling %2 drawn %3;", bomb.m_Body.GetOrigin(), bomb.m_bFalling, bomb.m_Body.GetVObject() != null && (bomb.m_Body.GetFlags() & EntityFlags.VISIBLE));
  PrintFormat("[ACR] FLYDEMO shot bombs %1:%2", name, where);
 }

 static void Blast(vector at)
 {
  at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 0.3;
  ACR_Supports.SpawnLocal(ACR_Supports.BOMB, at, vector.Zero);
  ACR_Supports.PlayBlast(at);
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
