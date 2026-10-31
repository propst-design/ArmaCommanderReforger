// -acrWorksShotsTest 1 (a game client, not srv; ~6 min, Kolguyev): Fortify a whole base as it stands, for
// a look. Builds all three tiers on two towns at once, then photographs each town low and wide from
// its threatened side, each line from its end along it and from out in front at a man-on-a-roof height
// (does it read as one thing?), and each gun and the command post from behind along the way it faces and
// from straight above. Daylight. Shots: $profile:acr-shots/works_*.
class ACR_TestWorksShots
{
 static const float STEP_S = 4; // for the world to stream in round the camera
 static const string TOWNS = "Bovemont Base,Ugoldar Base,Ugoldar Harbor 2";
 protected static ref array<ACR_Base> s_aBases = {};
 protected static ref array<vector> s_aEyes = {};
 protected static ref array<vector> s_aAngles = {};
 protected static ref array<string> s_aNames = {};
 protected static CameraBase s_Camera;
 protected static int s_iNext;
 protected static int s_iAt;
 protected static int s_iBuilt;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 8) Build();
  if (second < 12 || s_aNames.IsEmpty()) return;
  if (second == 12) Camera();
  if (!s_Camera) return;
  CloseMenus();
  int step = (second - 12) / STEP_S;
  if (step == s_iAt || s_iNext >= s_aNames.Count()) return;
  s_iAt = step;
  // The frame now on screen is the one the last step set up.
  if (s_iNext >= 0) Shot();
  s_iNext++;
  if (s_iNext < s_aNames.Count()) Aim(s_iNext);
  else Finish();
 }

 protected static void Build()
 {
  ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
  TimeAndWeatherManagerEntity time;
  if (world) time = world.GetTimeAndWeatherManager();
  if (time) time.SetHoursMinutesSeconds(11, 0, 0, true);
  array<string> wanted = {};
  TOWNS.Split(",", wanted, true);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_bHidden || base.m_bBuilt || !wanted.Contains(base.m_sName)) continue;
   s_aBases.Insert(base);
  }
  // Another map: the two bases with the most round them.
  if (s_aBases.IsEmpty())
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other && !other.m_bRetired && !other.m_bHidden && !other.m_bBuilt && other.m_sOwner != ACR_FactionSelection.West() && s_aBases.Count() < 2) s_aBases.Insert(other);
  ACR_TestRegress.Check("worksshots: towns to look at", !s_aBases.IsEmpty(), string.Format("%1", s_aBases.Count()));
  string side = ACR_FactionSelection.East();
  foreach (ACR_Base town : s_aBases)
  {
   town.m_sOwner = side;
   int built = ACR_BaseWorks.BuildAllForTest(town, side);
   s_iBuilt += built;
   PrintFormat("[ACR] WORKSSHOTS %1: %2 sites built", town.m_sName, built);
   Plan(town, side);
  }
  ACR_TestRegress.Check("worksshots: sites built", s_iBuilt > 0, string.Format("%1", s_iBuilt));
  PrintFormat("[ACR] WORKSSHOTS %1 shots to take, %2 s each", s_aNames.Count(), STEP_S);
 }

 protected static void Plan(ACR_Base town, string side)
 {
  string tag = town.m_sName;
  tag.Replace(" ", "");
  vector center = town.GetOrigin();
  ACR_BaseWork work = ACR_BaseWorks.WorkFor(town, side);
  // Low and wide: from out in front of the worst line, looking in at the town over it.
  vector look = Vector(0, 0, 1);
  if (!work.m_aLines.IsEmpty()) look = work.m_aLines[0].m_vFacing;
  float towards = ACR_BaseDefences.Bearing(look * -1);
  AddShot(center + look * 140 + Vector(0, 40, 0), Vector(towards, -14, 0), string.Format("works_%1_overview", tag), "the whole town, low");
  // High and oblique, from the threatened side: can the whole base's coverage be judged?
  float high = ACR_BaseDefences.SearchRadius(town) * 0.9;
  AddShot(center + look * high + Vector(0, Math.Clamp(high, 90, 120), 0), Vector(towards, -45, 0), string.Format("works_%1_high", tag), "the whole town, high");
  foreach (int l, ACR_BaseLine line : work.m_aLines) LineShots(line, tag, l);
  foreach (int i, ACR_BaseSite site : work.m_aSites)
  {
   if (!site.m_bBuilt) continue;
   // A line's stretches are seen in its shots.
   if (site.m_Plan && site.m_Plan.m_eKind != ACR_EBaseDefenceKind.POST) continue;
   float bearing = site.m_fYaw;
   float rad = bearing * Math.DEG2RAD;
   vector along = Vector(Math.Sin(rad), 0, Math.Cos(rad));
   string name = string.Format("works_%1_t%2_%3", tag, site.m_iTier, i);
   string words = string.Format("%1, tier %2, at %3 facing %4", ACR_BaseWorks.SiteWords(site), site.m_iTier, site.m_vPos, (int)bearing);
   AddShot(site.m_vPos - along * 13 + Vector(0, 5, 0), Vector(bearing, -14, 0), name + "_behind", words);
   AddShot(site.m_vPos + Vector(0, 38, 0), Vector(bearing, -88, 0), name + "_above", words);
  }
 }

 // Along a line from just beyond its end, a little behind and above it; and from out in front.
 protected static void LineShots(ACR_BaseLine line, string tag, int index)
 {
  int count = line.m_aPieces.Count();
  if (count == 0) return;
  ACR_LinePiece first = line.m_aPieces[0];
  ACR_LinePiece last = line.m_aPieces[count - 1];
  string words = string.Format("line %1, tier %2: %3 pieces, %4 m, ends %5 and %6", index, line.m_iTier, count, (int)line.m_fLength, line.m_sEndA, line.m_sEndB);
  vector along = ACR_BaseDefences.FlatDir(first.m_vPos, last.m_vPos);
  float way = ACR_BaseDefences.Bearing(along);
  string name = string.Format("works_%1_line%2_t%3", tag, index, line.m_iTier);
  vector eye = first.m_vPos - along * 8 - line.m_vFacing * 4 + Vector(0, 5, 0);
  eye = ClearEye(eye, line.m_vMiddle);
  AddShot(eye, Aim(eye, line.m_vMiddle), name + "_along", words);
  float stand = Math.Max(line.m_fLength * 0.55, 22);
  eye = ClearEye(line.m_vMiddle + line.m_vFacing * stand + Vector(0, 7, 0), line.m_vMiddle);
  AddShot(eye, Aim(eye, line.m_vMiddle), name + "_front", words);
 }

 // Angles from an eye to a target a little over the ground.
 protected static vector Aim(vector eye, vector target)
 {
  vector to = target + Vector(0, 1, 0) - eye;
  float flat = Math.Sqrt(to[0] * to[0] + to[2] * to[2]);
  return Vector(ACR_BaseDefences.Bearing(to), Math.Atan2(to[1], Math.Max(flat, 0.1)) * Math.RAD2DEG, 0);
 }

 protected static bool NotChar(notnull IEntity e) { return ChimeraCharacter.Cast(e) == null; }

 // Clear from the eye to the target, and not inside anything.
 protected static bool SeesFrom(vector eye, vector target)
 {
  BaseWorld world = GetGame().GetWorld();
  float ground = world.GetSurfaceY(eye[0], eye[2]);
  if (eye[1] < ground + 2) eye[1] = ground + 2;
  // Inside a building or a tree: something overhead, or a box round the eye.
  TraceParam up = new TraceParam();
  up.Start = eye;
  up.End = eye + Vector(0, 25, 0);
  up.Flags = TraceFlags.ENTS;
  if (world.TraceMove(up, NotChar) < 1) return false;
  TraceParam look = new TraceParam();
  look.Start = eye;
  look.End = target + Vector(0, 1, 0);
  look.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  return world.TraceMove(look, NotChar) >= 0.97;
 }

 // The eye, moved until it sees the target: up, then to either side and further back.
 protected static vector ClearEye(vector eye, vector target)
 {
  vector away = ACR_BaseDefences.FlatDir(target, eye);
  vector across = Vector(away[2], 0, -away[0]);
  array<float> ups = {0, 4, 8, 14, 22};
  array<float> sides = {0, 8, -8, 16, -16, 26, -26};
  array<float> backs = {0, 8};
  foreach (float rise : ups)
  {
   foreach (float back : backs)
   {
    foreach (float side : sides)
    {
     vector tried = eye + Vector(0, rise, 0) + across * side + away * back;
     if (SeesFrom(tried, target)) return tried;
    }
   }
  }
  return eye + Vector(0, 22, 0);
 }

 protected static void AddShot(vector eye, vector angles, string name, string words)
 {
  // Never under the ground on a slope.
  float ground = GetGame().GetWorld().GetSurfaceY(eye[0], eye[2]);
  if (eye[1] < ground + 2) eye[1] = ground + 2;
  s_aEyes.Insert(eye);
  s_aAngles.Insert(angles);
  s_aNames.Insert(name);
  PrintFormat("[ACR] WORKSSHOTS plan %1: %2", name, words);
 }

 protected static void Camera()
 {
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  Math3D.MatrixIdentity4(params.Transform);
  params.Transform[3] = s_aEyes[0];
  Resource prefab = Resource.Load("{E1FF38EC8894C5F3}Prefabs/Editor/Camera/ManualCameraSpectate.et");
  if (prefab && prefab.IsValid()) s_Camera = CameraBase.Cast(GetGame().SpawnEntityPrefab(prefab, GetGame().GetWorld(), params));
  if (!s_Camera) s_Camera = CameraBase.Cast(GetGame().SpawnEntity(CameraBase, GetGame().GetWorld(), params));
  if (!s_Camera) { ACR_TestRegress.Abort("worksshots", "no camera"); return; }
  SCR_ManualCamera manual = SCR_ManualCamera.Cast(s_Camera);
  if (manual)
  {
   manual.SetInputEnabled(false);
   manual.ClearEventMask(EntityEvent.POSTFRAME);
  }
  s_Camera.SetVerticalFOV(70);
  s_Camera.SetNearPlane(0.3);
  s_Camera.SetFarPlane(6000);
  GetGame().GetCameraManager().SetCamera(s_Camera);
  s_iNext = 0;
  s_iAt = 0;
  Aim(0);
 }

 protected static void Aim(int index)
 {
  s_Camera.SetOrigin(s_aEyes[index]);
  s_Camera.SetYawPitchRoll(s_aAngles[index]);
  s_Camera.Update();
  GetGame().GetCameraManager().SetCamera(s_Camera);
 }

 protected static void Shot()
 {
  FileIO.MakeDirectory("$profile:acr-shots");
  System.MakeScreenshot("$profile:acr-shots/" + s_aNames[s_iNext]);
  PrintFormat("[ACR] WORKSSHOTS shot %1", s_aNames[s_iNext]);
 }

 // A deploy or setup screen would cover the picture.
 protected static void CloseMenus()
 {
  MenuManager menus = GetGame().GetMenuManager();
  if (menus) menus.CloseAllMenus();
 }

 protected static void Finish()
 {
  ACR_TestRegress.Check("worksshots: every shot taken", s_iNext == s_aNames.Count(), string.Format("%1", s_iNext));
  ACR_TestRegress.Complete("worksshots", 3);
 }
}
