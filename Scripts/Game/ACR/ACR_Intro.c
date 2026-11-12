// Mission intro in the Arma 2 style (Jacob, 10 Nov 2026): when a player first takes control in a match,
// the camera circles their side's HQ slowly with music playing and the title in the middle of the
// screen, then flies into the player's head and hands over. Once per player per match: respawns,
// possession and reconnects don't replay it; a player joining late gets it on their first spawn.
//
// Server (Tick, after ACR_Setup.Started, so never behind the setup screen): picks the HQ and sends one
// RPC to that player's controller. Client (Frame, from ACR_Manager.OnFrame): a local camera nobody
// else sees, local widgets and local music. Skippable (Space / Enter: ACR_IntroSkip); a commander who
// opens the command map or the RTS camera ends it at once, and it waits up to WAIT_S for the map,
// a menu or the death fade to close before it starts (else it is dropped).
// Options: -acrIntro 0 turns it off; -acrIntroMusic picks the sound ("{GUID}path.acp:EVENT", or off);
// -acrIntroMusicZombies does the same for matches with zombies (ACR_Outbreak.On).
// Jacob, 10 Nov 2026: main modes play "Cliffhanger", zombie matches "Lost As Shit" (both vanilla tracks,
// played through ACA's Sounds/Music/ACR_IntroMusic.acp; no audio of ours shipped). 15 s at most from
// start to handover; the music fades out over the end of the zoom and stops at the handover (a skip or
// any early end stops it at once): it never plays on into the match. Zombie matches add a light strike
// across the title on an onset in the music (STRIKE_S), then the word ZOMBIES.
class ACR_Intro : Managed
{
 static const float ORBIT_S = 11.0;     // the orbit
 static const float ZOOM_S = 4.0;       // the flight into the player's head; ORBIT_S + ZOOM_S <= 15
 static const float FADE_S = 1.5;       // the music fades out over the last FADE_S (full volume until then)
 static const float STRIKE_S = 4.6;     // zombie matches: the strike, on an onset 4.6 s into Lost As Shit
 static const float WAIT_S = 20.0;      // how long a received intro waits for the map or a menu to close
 static const float ORBIT_RADIUS = 85.0;
 static const float ORBIT_HEIGHT = 32.0;
 static const float ORBIT_DEGREES = 80.0;
 static const string DEFAULT_MUSIC = "{C2C5BC05AC9625CD}Sounds/Music/ACR_IntroMusic.acp:SOUND_CLIFFHANGER";
 static const string ZOMBIE_MUSIC = "{C2C5BC05AC9625CD}Sounds/Music/ACR_IntroMusic.acp:SOUND_LOST_AS_SHIT";

 // Server: players who already had theirs this match, by identity (ACR_SpawnPoints.PlayerKey), so a
 // reconnect that brings a new player id doesn't replay it.
 protected static ref array<string> s_Sent;
 // Client: the intro playing (or waiting) on this machine, and whether this machine had one.
 protected static ref ACR_Intro s_Local;
 protected static bool s_bLocalDone;

 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_Sent = {};
 }

 static void Reset()
 {
  if (s_Sent) s_Sent.Clear();
  Stop("reset");
  s_bLocalDone = false;
 }

 // For the srv:intro test: an intro received (waiting or playing) here, and one already had here.
 static bool HasLocal() { return s_Local != null; }
 static bool LocalDone() { return s_bLocalDone; }

 static bool On()
 {
  string value;
  if (!ACR_Options.Get("acrIntro", value) || value == "") return true;
  value.ToLower();
  return value != "0" && value != "false" && value != "off";
 }

 // "" = no music. Zombie matches have their own option and default.
 static string MusicOption(bool zombies = false)
 {
  string value;
  if (zombies)
  {
   if (!ACR_Options.Get("acrIntroMusicZombies", value) || value == "") return ZOMBIE_MUSIC;
  }
  else if (!ACR_Options.Get("acrIntroMusic", value) || value == "") return DEFAULT_MUSIC;
  string lower = value;
  lower.ToLower();
  if (lower == "0" || lower == "off" || lower == "none") return "";
  return value;
 }

 // ---------------------------------------------------------------- server
 // Every second once the match runs (ACR_Manager.Tick).
 static void Tick()
 {
  if (!s_Sent) return;
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   string who = ACR_SpawnPoints.PlayerKey(id);
   if (s_Sent.Contains(who)) continue;
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc) continue;
   IEntity body = pc.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   s_Sent.Insert(who);
   if (!On()) continue;
   Faction faction = SCR_FactionManager.SGetPlayerFaction(id);
   string key = "";
   if (faction) key = faction.GetFactionKey();
   vector at = body.GetOrigin();
   string where = "";
   ACR_Base hq = null;
   if (key != "") hq = ACR_DoctrineKits.HQOf(key);
   if (hq)
   {
    at = hq.GetOrigin();
    where = hq.m_sName;
   }
   string line = "";
   if (key != "") line = ACR_FactionSelection.DisplayName(key);
   if (where != "")
   {
    if (line != "") line += "  |  ";
    line += where;
   }
   bool zombies = ACR_Outbreak.On();
   string music = MusicOption(zombies);
   PrintFormat("[ACR] INTRO sent player=%1 side=%2 hq=%3 zombies=%4 music=%5", id, key, where, zombies, music);
   pc.ACR_SendIntro(at, line, music, zombies);
  }
 }

 // ---------------------------------------------------------------- client
 protected vector m_vCentre;
 protected string m_sLine;
 protected string m_sMusic;
 protected float m_fReceived;   // world time (ms)
 protected float m_fStart = -1; // world time (ms) the camera started, -1 = still waiting
 protected float m_fStartYaw;
 protected IEntity m_Body;
 protected CameraBase m_Camera;
 protected CameraBase m_PreviousCamera;
 protected bool m_bZombies;
 protected AudioHandle m_Music = AudioHandle.Invalid;
 protected bool m_bFading;
 protected Widget m_Root;
 protected ImageWidget m_Black;
 protected ImageWidget m_BarTop;
 protected ImageWidget m_BarBottom;
 protected ImageWidget m_Flash;   // zombies: the white flash of the strike
 protected ImageWidget m_Streak;  // zombies: the bolt of light sweeping across the title
 protected ImageWidget m_Glow;    // zombies: the soft halo around the bolt
 protected TextWidget m_Title;
 protected TextWidget m_Subtitle;
 protected TextWidget m_Zombies;
 protected TextWidget m_Line;
 protected TextWidget m_Hint;

 // The server's RPC (ACR_PlayerController.ACR_IntroOwner).
 static void Receive(vector centre, string line, string music, bool zombies)
 {
  if (s_Local || s_bLocalDone) return;
  s_Local = new ACR_Intro();
  s_Local.m_vCentre = centre;
  s_Local.m_sLine = line;
  s_Local.m_sMusic = music;
  s_Local.m_bZombies = zombies;
  s_Local.m_fReceived = GetGame().GetWorld().GetWorldTime();
  Print("[ACR] INTRO received");
 }

 static void Stop(string why)
 {
  if (!s_Local) return;
  s_Local.Finish();
  if (s_Local.m_fStart >= 0) PrintFormat("[ACR] INTRO ended (%1)", why);
  s_Local = null;
  s_bLocalDone = true;
 }

 // Every frame on every machine with a player (ACR_Manager.OnFrame).
 static void Frame(SCR_PlayerController pc)
 {
  if (!s_Local) return;
  s_Local.Update(pc);
 }

 protected static bool Busy(SCR_PlayerController pc)
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (mapEntity && mapEntity.IsOpen()) return true;
  if (ACR_RTSCameraManager.IsActive()) return true;
  if (ACR_SetupClient.IsActive()) return true;
  if (pc.ACR_DeathTransitionActive()) return true;
  return false;
 }

 protected void Update(SCR_PlayerController pc)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  IEntity body = pc.GetControlledEntity();
  if (m_fStart < 0)
  {
   if (!pc || !ACR_Respawn.IsAlive(body) || Busy(pc) || GetGame().GetMenuManager().IsAnyMenuOpen())
   {
    if ((now - m_fReceived) / 1000.0 > WAIT_S) Stop("never free to start");
    return;
   }
   Begin(body);
   if (m_fStart < 0) { Stop("no camera"); return; }
  }
  // The commander went to the map or the RTS camera, the body died or changed: hand over now.
  if (body != m_Body || !ACR_Respawn.IsAlive(body)) { Stop("body changed"); return; }
  if (Busy(pc)) { Stop("map or camera opened"); return; }
  InputManager input = GetGame().GetInputManager();
  input.ActivateContext("ACR_IntroContext");
  if (input.GetActionTriggered("ACR_IntroSkip")) { Stop("skipped"); return; }
  ACR_MapHandoff.ClearCharacterInput(body);

  float t = (now - m_fStart) / 1000.0;
  if (t >= ORBIT_S + ZOOM_S) { Stop("done"); return; }
  // The music fades out as the camera comes down to the player. The engine's fade time is in ms, like
  // every time in a sound project; Finish stops the sound at the handover whatever the fade did.
  if (!m_bFading && t >= MusicFadeFrom())
  {
   m_bFading = true;
   if (m_sMusic != "") AudioSystem.TerminateSoundFadeOut(m_Music, true, FADE_S * 1000.0);
  }
  vector transform[4];
  if (t < ORBIT_S) Orbit(t, transform);
  else Zoom((t - ORBIT_S) / ZOOM_S, transform);
  m_Camera.SetWorldTransform(transform);
  Draw(t);
 }

 // Where the orbit is at u (0..1).
 protected vector OrbitPoint(float u)
 {
  float yaw = (m_fStartYaw + ORBIT_DEGREES * u) * Math.DEG2RAD;
  vector p = m_vCentre + Vector(Math.Sin(yaw) * ORBIT_RADIUS, 0, Math.Cos(yaw) * ORBIT_RADIUS);
  float ground = GetGame().GetWorld().GetSurfaceY(p[0], p[2]);
  float centreGround = GetGame().GetWorld().GetSurfaceY(m_vCentre[0], m_vCentre[2]);
  p[1] = Math.Max(ground + 12.0, centreGround + ORBIT_HEIGHT);
  return p;
 }

 protected static void LookAt(vector from, vector target, out vector transform[4])
 {
  vector dir = target - from;
  if (dir.LengthSq() < 0.0001) dir = "0 0 1";
  vector angles = dir.Normalized().VectorToAngles();
  Math3D.AnglesToMatrix(angles, transform);
  transform[3] = from;
 }

 protected void Orbit(float t, out vector transform[4])
 {
  float centreGround = GetGame().GetWorld().GetSurfaceY(m_vCentre[0], m_vCentre[2]);
  vector target = Vector(m_vCentre[0], centreGround + 4.0, m_vCentre[2]);
  LookAt(OrbitPoint(t / ORBIT_S), target, transform);
 }

 // From the end of the orbit into the player's eyes, over the ground on the way.
 protected void Zoom(float u, out vector transform[4])
 {
  float s = u * u * (3.0 - 2.0 * u); // ease in and out
  vector from = OrbitPoint(1.0);
  ChimeraCharacter character = ChimeraCharacter.Cast(m_Body);
  vector eye = m_Body.GetOrigin() + "0 1.7 0";
  if (character) eye = character.EyePosition();
  vector facing = m_Body.GetTransformAxis(2);
  facing[1] = 0;
  if (facing.LengthSq() < 0.0001) facing = "0 0 1";
  facing.Normalize();
  vector p = from + (eye - from) * s;
  float hop = Math.Min(vector.Distance(from, eye) * 0.25, 250.0) * Math.Sin(s * Math.PI);
  p[1] = p[1] + hop;
  if (s < 0.85) p[1] = Math.Max(p[1], GetGame().GetWorld().GetSurfaceY(p[0], p[2]) + 3.0);
  float centreGround = GetGame().GetWorld().GetSurfaceY(m_vCentre[0], m_vCentre[2]);
  vector hqTarget = Vector(m_vCentre[0], centreGround + 4.0, m_vCentre[2]);
  vector endTarget = eye + facing * 30.0;
  vector target = hqTarget + (endTarget - hqTarget) * Math.Clamp(s * 1.4, 0.0, 1.0);
  LookAt(p, target, transform);
 }

 protected void Begin(IEntity body)
 {
  m_Body = body;
  CameraManager cameras = GetGame().GetCameraManager();
  if (!cameras) return;
  m_PreviousCamera = cameras.CurrentCamera();
  // Start on the side of the HQ facing away from the player, so the flight in crosses the base.
  vector away = m_vCentre - body.GetOrigin();
  if (away.LengthSq() < 1.0) away = body.GetTransformAxis(2) * -1.0;
  m_fStartYaw = Math.Atan2(away[0], away[2]) * Math.RAD2DEG - ORBIT_DEGREES * 0.5;
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  vector start[4];
  Orbit(0, start);
  params.Transform[0] = start[0];
  params.Transform[1] = start[1];
  params.Transform[2] = start[2];
  params.Transform[3] = start[3];
  // The spectator camera prefab keeps the native rendering setup (as ACR_RTSCamera); its own input and
  // free flight are switched off so this script alone moves it.
  Resource prefab = Resource.Load("{E1FF38EC8894C5F3}Prefabs/Editor/Camera/ManualCameraSpectate.et");
  if (prefab && prefab.IsValid()) m_Camera = CameraBase.Cast(GetGame().SpawnEntityPrefab(prefab, GetGame().GetWorld(), params));
  if (!m_Camera) m_Camera = CameraBase.Cast(GetGame().SpawnEntity(CameraBase, GetGame().GetWorld(), params));
  if (!m_Camera) return;
  SCR_ManualCamera manual = SCR_ManualCamera.Cast(m_Camera);
  if (manual)
  {
   manual.SetInputEnabled(false);
   manual.ClearEventMask(EntityEvent.POSTFRAME);
  }
  m_Camera.SetWorldTransform(start);
  m_Camera.SetVerticalFOV(55);
  m_Camera.SetNearPlane(0.1);
  m_Camera.SetFarPlane(6000);
  cameras.SetCamera(m_Camera);
  ACR_MapHandoff.ClearCharacterInput(body);
  ACR_MapHandoff.SetMenuControls(body, true);
  BuildUI();
  if (m_sMusic != "")
  {
   int split = m_sMusic.LastIndexOf(":");
   if (split > 0)
   {
    string acp = m_sMusic.Substring(0, split);
    string eventName = m_sMusic.Substring(split + 1, m_sMusic.Length() - split - 1);
    vector at[4];
    Math3D.MatrixIdentity4(at);
    at[3] = start[3];
    m_Music = AudioSystem.PlayEvent(acp, eventName, at);
   }
  }
  m_fStart = GetGame().GetWorld().GetWorldTime();
  PrintFormat("[ACR] INTRO started at %1, music %2", m_vCentre, m_sMusic);
 }

 protected void Finish()
 {
  CameraManager cameras = GetGame().GetCameraManager();
  if (m_Camera)
  {
   if (cameras && cameras.CurrentCamera() == m_Camera && m_PreviousCamera) cameras.SetCamera(m_PreviousCamera);
   SCR_ManualCamera manual = SCR_ManualCamera.Cast(m_Camera);
   if (manual)
   {
    manual.Terminate();
    Widget cameraWidget = manual.GetWidget();
    if (cameraWidget) cameraWidget.RemoveFromHierarchy();
   }
   delete m_Camera;
   m_Camera = null;
  }
  // Ended because the map or the RTS camera opened: the body's controls stay off under it (they come
  // back when that closes), as when the player opens it normally.
  if (m_Body) ACR_MapHandoff.SetMenuControls(m_Body, GetGame().GetMenuManager().IsAnyMenuOpen() || ACR_RTSCameraManager.IsActive());
  if (m_Root)
  {
   ACR_Hud.Remove(m_Root); // made through ACR_Hud.CreateLayer: untrack it too
   m_Root = null;
  }
  // The music never plays on into the match (Jacob): the handover, a skip or any early end stops it.
  if (m_sMusic != "" && m_fStart >= 0) AudioSystem.TerminateSound(m_Music);
  m_Music = AudioHandle.Invalid;
 }

 // ---------------------------------------------------------------- the screen
 protected ImageWidget Panel(WorkspaceWidget ws, Color color, float top, float bottom, int z)
 {
  ImageWidget image = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, color, 0, m_Root));
  if (!image) return null;
  FrameSlot.SetAnchorMin(image, 0, top);
  FrameSlot.SetAnchorMax(image, 1, bottom);
  FrameSlot.SetOffsets(image, 0, 0, 0, 0);
  image.SetZOrder(z);
  return image;
 }

 protected ImageWidget Black(WorkspaceWidget ws, float top, float bottom)
 {
  return Panel(ws, new Color(0, 0, 0, 1), top, bottom, 0);
 }

 // Places a panel at x0..x1, y0..y1 (fractions of the screen; it may run off either edge).
 protected static void Place(ImageWidget image, float x0, float y0, float x1, float y1)
 {
  if (!image) return;
  FrameSlot.SetAnchorMin(image, x0, y0);
  FrameSlot.SetAnchorMax(image, x1, y1);
  FrameSlot.SetOffsets(image, 0, 0, 0, 0);
 }

 protected TextWidget Text(WorkspaceWidget ws, ResourceName layout, float y, int size)
 {
  TextWidget text = TextWidget.Cast(ws.CreateWidgets(layout, m_Root));
  if (!text) return null;
  text.SetFlags(WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.IGNORE_CURSOR);
  text.SetZOrder(2);
  FrameSlot.SetAnchorMin(text, 0.5, y);
  FrameSlot.SetAnchorMax(text, 0.5, y);
  FrameSlot.SetAlignment(text, 0.5, 0.5);
  FrameSlot.SetSizeToContent(text, true);
  text.SetExactFontSize(size);
  text.SetColor(Color.FromInt(Color.WHITE));
  text.SetOpacity(0);
  return text;
 }

 protected void BuildUI()
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (!ws) return;
  m_Root = ACR_Hud.CreateLayer(9990);
  if (!m_Root) return;
  // Letterbox bars, and a black frame that fades in from the start and out at the handover.
  m_BarTop = Black(ws, 0, 0.1);
  m_BarBottom = Black(ws, 0.9, 1);
  m_Black = Black(ws, 0, 1);
  // A raw TextWidget has no font: use the game's styled text layouts (as the death fade does).
  m_Title = Text(ws, "{0D7FF083FDE39877}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading4.layout", 0.44, 72);
  m_Subtitle = Text(ws, "{0D7FF083FDE39877}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading4.layout", 0.53, 30);
  float lineY = 0.6;
  if (m_bZombies) lineY = 0.73;
  m_Line = Text(ws, "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", lineY, 18);
  m_Hint = Text(ws, "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", 0.95, 14);
  if (m_Title) m_Title.SetText("ARMA  COMMANDER");
  if (m_Subtitle) m_Subtitle.SetText("A C T U A L");
  if (m_Line) m_Line.SetText(m_sLine);
  if (m_Hint) m_Hint.SetText("Space: skip");
  if (!m_bZombies) return;
  // Zombie matches: the word under the logo, a bolt of light that sweeps across the logo, its halo,
  // and a white flash over everything at the moment of the strike.
  m_Zombies = Text(ws, "{0D7FF083FDE39877}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading4.layout", 0.635, 96);
  if (m_Zombies)
  {
   m_Zombies.SetText("Z O M B I E S");
   m_Zombies.SetColor(new Color(0.78, 0.06, 0.04, 1));
  }
  m_Glow = Panel(ws, new Color(0.75, 0.88, 1, 1), 0.36, 0.58, 3);
  m_Streak = Panel(ws, new Color(1, 1, 1, 1), 0.38, 0.56, 4);
  m_Flash = Panel(ws, new Color(0.92, 0.96, 1, 1), 0, 1, 5);
  if (m_Glow) m_Glow.SetOpacity(0);
  if (m_Streak) m_Streak.SetOpacity(0);
  if (m_Flash) m_Flash.SetOpacity(0);
 }

 // 0 before a, rising to 1 by b, holding, falling from c to 0 at d.
 protected static float Ramp(float t, float a, float b, float c, float d)
 {
  if (t <= a || t >= d) return 0;
  if (t < b) return (t - a) / (b - a);
  if (t <= c) return 1;
  return (d - t) / (d - c);
 }

 protected void Draw(float t)
 {
  float total = ORBIT_S + ZOOM_S;
  // Black: out of black over the first second; a short dip at the very end hides the camera swap.
  float black = 0;
  if (t < 1.2) black = 1.0 - t / 1.2;
  if (t > total - 0.25) black = (t - (total - 0.25)) / 0.25;
  if (m_Black) m_Black.SetOpacity(Math.Clamp(black, 0, 1));
  // Bars slide away during the zoom.
  float bars = 1.0;
  if (t > ORBIT_S) bars = Math.Clamp(1.0 - (t - ORBIT_S) / (ZOOM_S * 0.6), 0, 1);
  if (m_BarTop) m_BarTop.SetOpacity(bars);
  if (m_BarBottom) m_BarBottom.SetOpacity(bars);
  // Titles: in over the first seconds, out before the orbit ends (all inside the 15 s).
  if (m_Title) m_Title.SetOpacity(TitleOpacity(t));
  if (m_Subtitle) m_Subtitle.SetOpacity(Ramp(t, 1.6, 3.0, TitlesOutFrom(), TitlesOutBy()));
  float lineIn = 2.6;
  if (m_bZombies) lineIn = STRIKE_S + 0.9;
  if (m_Line) m_Line.SetOpacity(Ramp(t, lineIn, lineIn + 1.2, TitlesOutFrom(), TitlesOutBy()) * 0.85);
  if (m_Hint) m_Hint.SetOpacity(Ramp(t, 1.0, 2.0, ORBIT_S, ORBIT_S + 1.0) * 0.6);
  if (!m_bZombies) return;
  // Zombie matches, around STRIKE_S: the bolt sweeps across the logo and lands on the beat, the screen
  // flashes white and dies away, and ZOMBIES is there as the flash clears.
  float x = StreakX(t);
  float streak = StreakOpacity(t);
  Place(m_Streak, x - 0.012, 0.38, x + 0.012, 0.56);
  Place(m_Glow, x - 0.07, 0.36, x + 0.07, 0.58);
  if (m_Streak) m_Streak.SetOpacity(streak);
  if (m_Glow) m_Glow.SetOpacity(streak * 0.35);
  if (m_Flash) m_Flash.SetOpacity(FlashOpacity(t));
  if (m_Zombies) m_Zombies.SetOpacity(ZombiesOpacity(t));
 }

 // ---------------------------------------------------------------- the timeline (t: seconds since start)
 // Static so the srv:intro test can check them without a screen.
 static float TitlesOutFrom() { return ORBIT_S - 2.6; }
 static float TitlesOutBy() { return ORBIT_S - 1.0; }
 static float MusicFadeFrom() { return ORBIT_S + ZOOM_S - FADE_S; }
 static float TitleOpacity(float t) { return Ramp(t, 1.0, 2.4, TitlesOutFrom(), TitlesOutBy()); }
 static float ZombiesOpacity(float t) { return Ramp(t, STRIKE_S, STRIKE_S + 0.12, TitlesOutFrom(), TitlesOutBy()); }

 // The bolt's centre (fraction of the screen width): off the left edge 0.3 s before the strike, off the
 // right edge on it.
 static float StreakX(float t)
 {
  return -0.1 + 1.2 * Math.Clamp((t - STRIKE_S + 0.3) / 0.3, 0, 1);
 }

 // Full while it sweeps, fading over 0.25 s after the strike.
 static float StreakOpacity(float t)
 {
  float d = t - STRIKE_S;
  if (d > -0.3 && d < 0) return 1;
  if (d >= 0 && d < 0.25) return 1.0 - d / 0.25;
  return 0;
 }

 // Up to 0.85 over the last 60 ms of the sweep, gone 0.7 s after the strike.
 static float FlashOpacity(float t)
 {
  float d = t - STRIKE_S;
  if (d > -0.06 && d < 0) return (d + 0.06) / 0.06 * 0.85;
  if (d >= 0 && d < 0.7) return 0.85 * (1.0 - d / 0.7) * (1.0 - d / 0.7);
  return 0;
 }
}
