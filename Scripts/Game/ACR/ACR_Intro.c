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
// Options: -acrIntro 0 turns it off; -acrIntroMusic picks the sound ("{GUID}path.acp:EVENT", or off).
class ACR_Intro : Managed
{
 static const float ORBIT_S = 16.0;     // the orbit
 static const float ZOOM_S = 4.0;       // the flight into the player's head
 static const float WAIT_S = 20.0;      // how long a received intro waits for the map or a menu to close
 static const float ORBIT_RADIUS = 85.0;
 static const float ORBIT_HEIGHT = 32.0;
 static const float ORBIT_DEGREES = 110.0;
 static const string DEFAULT_MUSIC = "{71AA761577272A1B}Sounds/Music/Campaign/Music_Campaign.acp:SOUND_FOREST";

 // Server: players who already had theirs this match.
 protected static ref array<int> s_Sent;
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

 static bool On()
 {
  string value;
  if (!ACR_Options.Get("acrIntro", value) || value == "") return true;
  value.ToLower();
  return value != "0" && value != "false" && value != "off";
 }

 // "" = no music.
 static string Music()
 {
  string value;
  if (!ACR_Options.Get("acrIntroMusic", value) || value == "") return DEFAULT_MUSIC;
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
   if (s_Sent.Contains(id)) continue;
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc) continue;
   IEntity body = pc.GetControlledEntity();
   if (!ACR_Respawn.IsAlive(body)) continue;
   s_Sent.Insert(id);
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
   if (key != "") line = ACR_PlayerSide.DisplayName(key);
   if (where != "")
   {
    if (line != "") line += "  |  ";
    line += where;
   }
   PrintFormat("[ACR] INTRO sent player=%1 side=%2 hq=%3", id, key, where);
   pc.ACR_SendIntro(at, line, Music());
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
 protected AudioHandle m_Music = AudioHandle.Invalid;
 protected Widget m_Root;
 protected ImageWidget m_Black;
 protected ImageWidget m_BarTop;
 protected ImageWidget m_BarBottom;
 protected TextWidget m_Title;
 protected TextWidget m_Subtitle;
 protected TextWidget m_Line;
 protected TextWidget m_Hint;

 // The server's RPC (ACR_PlayerController.ACR_IntroOwner).
 static void Receive(vector centre, string line, string music)
 {
  if (s_Local || s_bLocalDone) return;
  s_Local = new ACR_Intro();
  s_Local.m_vCentre = centre;
  s_Local.m_sLine = line;
  s_Local.m_sMusic = music;
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
  SCR_MapEntity map = SCR_MapEntity.GetMapInstance();
  if (map && map.IsOpen()) return true;
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
  if (m_Body) ACR_MapHandoff.SetMenuControls(m_Body, false);
  if (m_Root)
  {
   ACR_Hud.Remove(m_Root); // made through ACR_Hud.CreateLayer: untrack it too
   m_Root = null;
  }
  // The music plays on into the match, as it did in Arma 2; a skip cuts it.
  if (m_sMusic != "" && m_fStart >= 0 && GetGame().GetWorld() && (GetGame().GetWorld().GetWorldTime() - m_fStart) / 1000.0 < ORBIT_S + ZOOM_S) AudioSystem.TerminateSound(m_Music);
 }

 // ---------------------------------------------------------------- the screen
 protected ImageWidget Black(WorkspaceWidget ws, float top, float bottom)
 {
  ImageWidget image = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, new Color(0, 0, 0, 1), 0, m_Root));
  if (!image) return null;
  FrameSlot.SetAnchorMin(image, 0, top);
  FrameSlot.SetAnchorMax(image, 1, bottom);
  FrameSlot.SetOffsets(image, 0, 0, 0, 0);
  return image;
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
  m_Line = Text(ws, "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", 0.6, 18);
  m_Hint = Text(ws, "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", 0.95, 14);
  if (m_Title) m_Title.SetText("ARMA  COMMANDER");
  if (m_Subtitle) m_Subtitle.SetText("A C T U A L");
  if (m_Line) m_Line.SetText(m_sLine);
  if (m_Hint) m_Hint.SetText("Space: skip");
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
  float title = Ramp(t, 1.5, 3.5, 10.5, 13.0);
  if (m_Title) m_Title.SetOpacity(title);
  if (m_Subtitle) m_Subtitle.SetOpacity(Ramp(t, 2.3, 4.3, 10.5, 13.0));
  if (m_Line) m_Line.SetOpacity(Ramp(t, 3.5, 5.0, 10.5, 13.0) * 0.85);
  if (m_Hint) m_Hint.SetOpacity(Ramp(t, 1.0, 2.0, ORBIT_S, ORBIT_S + 1.0) * 0.6);
 }
}
