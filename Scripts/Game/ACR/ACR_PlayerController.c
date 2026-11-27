modded class SCR_PlayerController
{
 // ---------------------------------------------------------------- menu control flags
 protected IEntity m_ACR_MenuControlBody;
 // Native UpdateControls only reapplies menu-disabled view/weapon/movement
 // flags when menu state changes; move them with the body on possession.
 override protected void UpdateControls()
 {
  super.UpdateControls();
  if (!ACR_Manager.GetInstance() || this != GetGame().GetPlayerController()) return;
  IEntity body = GetControlledEntity();
  if (body == m_ACR_MenuControlBody) return;
  ACR_MapHandoff.SetMenuControls(m_ACR_MenuControlBody, false);
  ACR_MapHandoff.SetMenuControls(body, GetGame().GetMenuManager().IsAnyMenuOpen());
  m_ACR_MenuControlBody = body;
 }

 // ---------------------------------------------------------------- requests
 // Server: when each kind of request last came (review #15: one shared stamp dropped a support call
 // or a purchase that came soon after another kind, silently, while the client thought it went).
 protected ref map<string, float> m_ACR_RequestAt = new map<string, float>();

 // True when this kind of request came sooner than spacing ms after the last one; the player is
 // told, so a double click doesn't leave him thinking the second one went through.
 protected bool ACR_TooSoon(string kind, float spacing)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  float last;
  if (m_ACR_RequestAt.Find(kind, last) && now >= last && now - last < spacing)
  {
   Rpc(ACR_FeedbackOwner, "Too fast: wait a moment and try again");
   return true;
  }
  m_ACR_RequestAt.Set(kind, now);
  return false;
 }
 protected float m_ACR_CommandWindow = -1000;
 protected int m_ACR_CommandsInWindow;
 protected ref ACR_SameClickGuard m_ACR_SameClick = new ACR_SameClickGuard();
 // aim: where a player dragged the order to face (drag to aim), else vector.Zero.
 void ACR_Request(int command, RplId targetId = RplId.Invalid(), vector point = vector.Zero, vector aim = vector.Zero)
 {
  if (!ACR_Manager.GetInstance()) return;
  // F8 (or the COMMAND UNIT marker) while already in the command unit is refused by the server:
  // don't pull the map out of his hand for it.
  bool returning = command == ACR_ERequest.RETURN && GetControlledEntity() != GetMainEntity();
  if (command == ACR_ERequest.POSSESS || returning) ACR_MapHandoff.Release(this, GetControlledEntity(), false);
  Rpc(ACR_CommandServer, command, targetId, point, aim);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_CommandServer(int command, RplId targetId, vector point, vector aim)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  // Flood guard: up to 96 orders a second. One click sends one per selected squad and a box
  // selection has no cap: at 24, ordering 30 squads silently left 6 behind (audit 31 Oct). A
  // dropped order is said, once a window.
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_CommandWindow > 1000)
  {
   m_ACR_CommandWindow = now;
   m_ACR_CommandsInWindow = 0;
  }
  m_ACR_CommandsInWindow++;
  if (m_ACR_CommandsInWindow > 96)
  {
   if (m_ACR_CommandsInWindow == 97) manager.Notify(GetPlayerId(), "Too many orders at once: some were not given, order again");
   return;
  }
  if (m_ACR_SameClick.Drop(command, targetId, now))
  {
   PrintFormat("[ACR] ORDER dropped: a move right after an order that takes a facing for the same squad (the same click), player %1", GetPlayerId());
   return;
  }
  ACR_Requests.s_vAim = aim;
  string message = manager.HandleRequest(this, command, targetId, point);
  ACR_Requests.s_vAim = vector.Zero;
  m_ACR_SameClick.Note(command, targetId, now);
  if (command == ACR_ERequest.REINFORCE) Rpc(ACR_ReinforceFeedbackOwner, message);
  else Rpc(ACR_FeedbackOwner, message);
 }

 string m_ACR_ReinforceFeedback;
 float m_ACR_ReinforceFeedbackAt = -10000;
 protected int m_ACR_ReinforceFeedbackCount;
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_ReinforceFeedbackOwner(string message)
 {
  ACR_FeedbackOwner(message);
  float now = GetGame().GetWorld().GetWorldTime();
  // A multi-selection sends one request per squad. Preserve a few results together
  // so a later successful squad cannot immediately erase an earlier refusal.
  if (now - m_ACR_ReinforceFeedbackAt > 1000)
  {
   m_ACR_ReinforceFeedback = "";
   m_ACR_ReinforceFeedbackCount = 0;
  }
  if (m_ACR_ReinforceFeedbackCount < 3)
  {
   if (!m_ACR_ReinforceFeedback.IsEmpty()) m_ACR_ReinforceFeedback += "\n";
   m_ACR_ReinforceFeedback += message;
  }
  else if (m_ACR_ReinforceFeedbackCount == 3) m_ACR_ReinforceFeedback += "\nFurther squad results are in the game log.";
  m_ACR_ReinforceFeedbackCount++;
  m_ACR_ReinforceFeedbackAt = now;
 }
 // Server: a hint for this player (e.g. a refused dismantle).
 void ACR_Feedback(string message)
 {
  Rpc(ACR_FeedbackOwner, message);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_FeedbackOwner(string message)
 {
  Print("[ACR] " + message);
  SCR_HintManagerComponent.ShowCustomHint(message, "AC: Actual", 5, true);
 }

 // ---------------------------------------------------------------- Tarkov-style death transition
 protected Widget m_DeathBlackoutRoot;
 protected ImageWidget m_DeathBlackoutImage;
 protected TextWidget m_DeathBlackoutText;
 protected bool m_bDeathTransitionActive;
 protected bool m_bDeathOpaqueReported;
 protected float m_fDeathTransitionStart;
 protected float m_fDeathFallDuration;
 protected float m_fDeathHoldDuration;
 protected string m_sDeathTransitionMessage;

 void ACR_SendDeathTransition(float fallSeconds, float holdSeconds, string message)
 {
  Rpc(ACR_DeathTransitionOwner, fallSeconds, holdSeconds, message);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_DeathTransitionOwner(float fallSeconds, float holdSeconds, string message)
 {
  m_bDeathTransitionActive = true;
  m_bDeathOpaqueReported = false;
  PrintFormat("[ACR] DEATH FADE received: %1", message);
  m_fDeathTransitionStart = GetGame().GetWorld().GetWorldTime();
  m_fDeathFallDuration = fallSeconds;
  m_fDeathHoldDuration = holdSeconds;
  m_sDeathTransitionMessage = message;
  ACR_EnsureDeathOverlay();
  if (m_DeathBlackoutImage) m_DeathBlackoutImage.SetOpacity(0);
  if (m_DeathBlackoutText)
  {
   m_DeathBlackoutText.SetText(message);
   m_DeathBlackoutText.SetVisible(false);
  }
 }

 void ACR_CancelDeathTransition()
 {
  Rpc(ACR_CancelDeathTransitionOwner);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_CancelDeathTransitionOwner()
 {
  m_bDeathTransitionActive = false;
  if (m_DeathBlackoutRoot)
  {
   ACR_Hud.Remove(m_DeathBlackoutRoot); // created through ACR_Hud.CreateLayer: untrack it too
   m_DeathBlackoutRoot = null;
   m_DeathBlackoutImage = null;
   m_DeathBlackoutText = null;
  }
  ACR_MapHandoff.SetMenuControls(GetControlledEntity(), false);
 }

 protected void ACR_EnsureDeathOverlay()
 {
  if (m_DeathBlackoutRoot && m_DeathBlackoutImage && m_DeathBlackoutText) return;
  if (m_DeathBlackoutRoot) ACR_Hud.Remove(m_DeathBlackoutRoot);
  m_DeathBlackoutRoot = null;
  m_DeathBlackoutImage = null;
  m_DeathBlackoutText = null;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (!ws) return;
  m_DeathBlackoutRoot = ACR_Hud.CreateLayer(9999);
  if (!m_DeathBlackoutRoot) return;

  m_DeathBlackoutImage = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, new Color(0, 0, 0, 1), 0, m_DeathBlackoutRoot));
  if (!m_DeathBlackoutImage) return;
  m_DeathBlackoutImage.SetOpacity(0);
  FrameSlot.SetAnchorMin(m_DeathBlackoutImage, 0, 0);
  FrameSlot.SetAnchorMax(m_DeathBlackoutImage, 1, 1);
  FrameSlot.SetOffsets(m_DeathBlackoutImage, 0, 0, 0, 0);

  // A raw TextWidget has no font resource. Use the game's actual styled text
  // layout so both glyph rendering and size-to-content have a defined font.
  m_DeathBlackoutText = TextWidget.Cast(ws.CreateWidgets("{0D7FF083FDE39877}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading4.layout", m_DeathBlackoutRoot));
  if (!m_DeathBlackoutText) return;
  m_DeathBlackoutText.SetFlags(WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.IGNORE_CURSOR);
  m_DeathBlackoutText.SetZOrder(1);
  m_DeathBlackoutText.SetText(m_sDeathTransitionMessage);
  FrameSlot.SetAnchorMin(m_DeathBlackoutText, 0.5, 0.5);
  FrameSlot.SetAnchorMax(m_DeathBlackoutText, 0.5, 0.5);
  FrameSlot.SetAlignment(m_DeathBlackoutText, 0.5, 0.5);
  FrameSlot.SetSizeToContent(m_DeathBlackoutText, true);
  m_DeathBlackoutText.SetExactFontSize(22);
  m_DeathBlackoutText.SetColor(Color.FromInt(Color.WHITE));
 }

 void ACR_UpdateDeathTransition(float timeSlice)
 {
  if (!m_bDeathTransitionActive) return;
  float now = GetGame().GetWorld().GetWorldTime();
  float elapsed = (now - m_fDeathTransitionStart) / 1000.0;
  float totalDuration = m_fDeathFallDuration + m_fDeathHoldDuration + 0.5;

  if (elapsed >= totalDuration)
  {
   ACR_CancelDeathTransitionOwner();
   return;
  }

  ACR_EnsureDeathOverlay();
  if (!m_DeathBlackoutImage) return;

  float alpha = 0.0;
  // Phase 1: 0.0 - 0.5 s: camera follows ragdoll down, screen clear
  if (elapsed < 0.5)
  {
   alpha = 0.0;
   if (m_DeathBlackoutText) m_DeathBlackoutText.SetVisible(false);
  }
  // Phase 2: 0.5 s to fallDuration: fade into darkness as corpse lands
  else if (elapsed < m_fDeathFallDuration)
  {
   float span = m_fDeathFallDuration - 0.5;
   if (span > 0) alpha = Math.Clamp((elapsed - 0.5) / span, 0.0, 1.0);
   else alpha = 1.0;
   if (m_DeathBlackoutText) m_DeathBlackoutText.SetVisible(alpha >= 0.85);
  }
  // Phase 3: fallDuration to (fallDuration + holdDuration): pure black, showing status text
  else if (elapsed < (m_fDeathFallDuration + m_fDeathHoldDuration))
  {
   alpha = 1.0;
   if (m_DeathBlackoutText) m_DeathBlackoutText.SetVisible(true);
   ACR_MapHandoff.SetMenuControls(GetControlledEntity(), true);
  }
  // Phase 4: fade up into the new subordinate
  else
  {
   float inElapsed = elapsed - (m_fDeathFallDuration + m_fDeathHoldDuration);
   alpha = Math.Clamp(1.0 - (inElapsed / 0.5), 0.0, 1.0);
   if (m_DeathBlackoutText) m_DeathBlackoutText.SetVisible(false);
   ACR_MapHandoff.SetMenuControls(GetControlledEntity(), false);
  }

  // Keep the image's black tint opaque and animate widget opacity explicitly.
  // A transparent creation tint is not a reliable visibility animation.
  m_DeathBlackoutImage.SetOpacity(alpha);
  if (alpha >= 0.99 && !m_bDeathOpaqueReported)
  {
   m_bDeathOpaqueReported = true;
   float w, h;
   m_DeathBlackoutImage.GetScreenSize(w, h);
   PrintFormat("[ACR] DEATH FADE opaque: %1x%2, text visible=%3, message=%4", w, h, m_DeathBlackoutText && m_DeathBlackoutText.IsVisible(), m_sDeathTransitionMessage);
  }
 }

 // A diagnostic snapshot of the actual client widgets, including measured
 // geometry and rendered text. Also used by the client transition regression.
 bool ACR_DeathTransitionActive() { return m_bDeathTransitionActive; }

 bool ACR_DeathOverlayState(out float opacity, out string message, out bool textVisible)
 {
  opacity = 0;
  message = "";
  textVisible = false;
  if (!m_bDeathTransitionActive || !m_DeathBlackoutRoot || !m_DeathBlackoutImage || !m_DeathBlackoutText) return false;
  float w, h, sw, sh, tw, th;
  m_DeathBlackoutImage.GetScreenSize(w, h);
  GetGame().GetWorkspace().GetScreenSize(sw, sh);
  m_DeathBlackoutText.GetTextSize(tw, th);
  opacity = m_DeathBlackoutImage.GetOpacity();
  message = m_DeathBlackoutText.GetText();
  textVisible = m_DeathBlackoutText.IsVisible();
  return m_DeathBlackoutRoot.IsVisible() && m_DeathBlackoutImage.IsVisible() && sw > 0 && sh > 0 && w >= sw * 0.99 && h >= sh * 0.99 && tw > 0 && th > 0;
 }

 // ---------------------------------------------------------------- mission intro (ACR_Intro)
 void ACR_SendIntro(vector centre, string line, string music, bool zombies)
 {
  Rpc(ACR_IntroOwner, centre, line, music, zombies);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_IntroOwner(vector centre, string line, string music, bool zombies)
 {
  ACR_Intro.Receive(centre, line, music, zombies);
 }

 // ---------------------------------------------------------------- setup phase (ACR_Setup)
 void ACR_SendSetup(bool active, bool allowed, string rows, string info)
 {
  Rpc(ACR_SetupOwner, active, allowed, rows, info);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_SetupOwner(bool active, bool allowed, string rows, string info)
 {
  ACR_SetupClient.Receive(active, allowed, rows, info);
 }
 // The match's two sides (ACR_FactionSelection.SyncClients): the deploy screen lists only them.
 void ACR_SendPlayableSides(string west, string east)
 {
  Rpc(ACR_PlayableSidesOwner, west, east);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_PlayableSidesOwner(string west, string east)
 {
  ACR_FactionSelection.SetPlayable(west, east);
 }
 void ACR_RequestSetup(string key)
 {
  Rpc(ACR_SetupServer, key);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_SetupServer(string key)
 {
  if (ACR_TooSoon("setup", 150)) return;
  if (key.Length() > ACR_SetupPresets.MAX_IMPORT_CHARS + 64)
  {
   Rpc(ACR_FeedbackOwner, "The setup request is too long.");
   return;
  }
  string result = ACR_Setup.Handle(GetPlayerId(), key);
  PrintFormat("[ACR] SETUP player %1: %2", GetPlayerId(), result);
  Rpc(ACR_FeedbackOwner, result);
 }
 // Setup presets (#143): "Copy to the clipboard" on the host's or admin's own machine.
 void ACR_SendSetupClipboard(string text)
 {
  Rpc(ACR_SetupClipboardOwner, text);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_SetupClipboardOwner(string text)
 {
  System.ExportToClipboard(text);
 }
 // Layout editor: move a base of the generated layout (the server checks the spot).
 void ACR_RequestSetupMove(string name, float x, float z)
 {
  Rpc(ACR_SetupMoveServer, name, x, z);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_SetupMoveServer(string name, float x, float z)
 {
  if (ACR_TooSoon("setupmove", 150)) return;
  string result = ACR_Setup.HandleMove(GetPlayerId(), name, x, z);
  PrintFormat("[ACR] SETUP player %1: %2", GetPlayerId(), result);
  Rpc(ACR_FeedbackOwner, result);
 }

 // ---------------------------------------------------------------- debug menu (host/admins)
 bool m_ACR_DebugAllowed;
 bool m_ACR_DebugOpen;
 string m_ACR_DebugStatus;
 string m_ACR_DebugOwn;     // side names for the buttons ("USSR", "RHS US")
 string m_ACR_DebugEnemy;
 bool m_ACR_DebugPaused;    // the enemy AI commander is paused
 bool m_ACR_DebugReveal;    // our intel sees every enemy soldier
 protected float m_ACR_LastDebug = -1000;
 void ACR_SendDebug(bool allowed, string status, string ownName, string enemyName, bool paused, bool reveal)
 {
  Rpc(ACR_DebugOwner, allowed, status, ownName, enemyName, paused, reveal);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_DebugOwner(bool allowed, string status, string ownName, string enemyName, bool paused, bool reveal)
 {
  m_ACR_DebugAllowed = allowed;
  m_ACR_DebugStatus = status;
  m_ACR_DebugOwn = ownName;
  m_ACR_DebugEnemy = enemyName;
  m_ACR_DebugPaused = paused;
  m_ACR_DebugReveal = reveal;
 }
 void ACR_RequestDebug(int action)
 {
  Rpc(ACR_DebugServer, action, m_ACR_SelectedBase, m_ACR_SelectedGroup);
 }
 // group: the squad selected on the caller's map. The server's copy of the controller never has
 // the selection, so a remote admin's Heal/Kill Squad hit the wrong squad or none (audit 31 Oct).
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_DebugServer(int action, string baseName, RplId group)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  // Its own throttle, and it says so: sharing the order throttle dropped the second of two quick
  // clicks without a word (6 Nov live: "+500 RP" pressed for both sides, one side got it).
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastDebug < 250)
  {
   Rpc(ACR_FeedbackOwner, "Debug: too quick, that click was ignored; press it again");
   return;
  }
  m_ACR_LastDebug = now;
  if (!ACR_Manager.IsHostOrAdmin(GetPlayerId()))
  {
   Rpc(ACR_FeedbackOwner, "Debug menu: host and admins only");
   return;
  }
  Faction faction = SCR_FactionManager.SGetPlayerFaction(GetPlayerId());
  if (!faction)
  {
   Rpc(ACR_FeedbackOwner, "Debug menu: join a side first");
   return;
  }
  if (action == ACR_EDebug.HEAL_SQUAD)
  {
   ACR_Squad target = null;
   if (group.IsValid()) target = manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(group)));
   if (!target) manager.m_LastSquad.Find(GetPlayerId(), target);
   if (target)
   {
    ACR_DebugCommand.HealSquad(target);
    Rpc(ACR_FeedbackOwner, "Debug: Healed and rearmed " + target.GetName());
    return;
   }
   IEntity body = GetControlledEntity();
   if (body)
   {
    ACR_DebugCommand.HealEntity(body);
    Rpc(ACR_FeedbackOwner, "Debug: Healed your character");
    return;
   }
   Rpc(ACR_FeedbackOwner, "Debug: Select a squad first");
   return;
  }
  if (action == ACR_EDebug.KILL_SQUAD)
  {
   ACR_Squad target = null;
   if (group.IsValid()) target = manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(group)));
   if (target)
   {
    ACR_DebugCommand.KillSquad(target);
    Rpc(ACR_FeedbackOwner, "Debug: Eliminated squad " + target.GetName());
    return;
   }
   Rpc(ACR_FeedbackOwner, "Debug: Select a squad first");
   return;
  }
  string result = manager.m_Debug.Run(manager, action, faction.GetFactionKey(), baseName);
  PrintFormat("[ACR] DEBUG player %1: %2", GetPlayerId(), result);
  Rpc(ACR_FeedbackOwner, result);
 }

 void ACR_RequestDebugCommand(string cmd)
 {
  vector cursorPos = vector.Zero;
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (mapEntity && mapEntity.IsOpen())
  {
   float cx, cz;
   mapEntity.GetMapCursorWorldPosition(cx, cz);
   cursorPos = Vector(cx, 0, cz);
  }
  Rpc(ACR_DebugCommandServer, cmd, m_ACR_SelectedBase, m_ACR_SelectedGroup, cursorPos);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_DebugCommandServer(string cmd, string baseName, RplId groupId, vector cursorPos)
 {
  string result = ACR_DebugCommand.Execute(this, cmd, baseName, groupId, cursorPos);
  Rpc(ACR_DebugCommandOwner, result);
 }

 bool m_ACR_AdminUnbound; // server: this admin's RTS camera has no leash (/acr unbound)
 void ACR_SendUnbound(bool unbound)
 {
  if (GetPlayerId() == SCR_PlayerController.GetLocalPlayerId()) ACR_RTSCameraManager.s_bAdminUnbound = unbound;
  else Rpc(ACR_UnboundOwner, unbound);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_UnboundOwner(bool unbound)
 {
  ACR_RTSCameraManager.s_bAdminUnbound = unbound;
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_DebugCommandOwner(string message)
 {
  SCR_ChatPanelManager chat = SCR_ChatPanelManager.GetInstance();
  if (chat) chat.OnNewMessage(message);
  SCR_HintManagerComponent.ShowCustomHint(message, "Debug Command", 5, true);
 }

 // ---------------------------------------------------------------- gunner drives
 protected IEntity m_ACR_DriveVehicle;
 protected float m_ACR_DriveThrottle;
 protected float m_ACR_DriveSteer;
 protected float m_ACR_DriveSent;

 // Called every frame by the local manager; sends key changes plus a keep-alive while held.
 // keepAlive (map open): no keys, but a message every 5 s so the session isn't released as idle.
 void ACR_UpdateGunnerDrive(IEntity vehicle, float throttle, float steer, bool keepAlive = false)
 {
  if (m_bDeathTransitionActive) { throttle = 0; steer = 0; }
  float now = GetGame().GetWorld().GetWorldTime();
  if (vehicle != m_ACR_DriveVehicle)
  {
   if (m_ACR_DriveVehicle && (m_ACR_DriveThrottle != 0 || m_ACR_DriveSteer != 0))
    Rpc(ACR_DriveServer, SCR_EntityHelper.EntityToRplId(m_ACR_DriveVehicle), 0, 0);
   m_ACR_DriveVehicle = vehicle;
   m_ACR_DriveThrottle = 0;
   m_ACR_DriveSteer = 0;
  }
  if (!vehicle) return;
  bool changed = throttle != m_ACR_DriveThrottle || steer != m_ACR_DriveSteer;
  bool held = throttle != 0 || steer != 0;
  bool due = (held && now - m_ACR_DriveSent > 250) || (keepAlive && now - m_ACR_DriveSent > 5000);
  if (!changed && !due) return;
  m_ACR_DriveThrottle = throttle;
  m_ACR_DriveSteer = steer;
  m_ACR_DriveSent = now;
  Rpc(ACR_DriveServer, SCR_EntityHelper.EntityToRplId(vehicle), throttle, steer);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_DriveServer(RplId vehicleId, float throttle, float steer)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) manager.GunnerDriveInput(this, SCR_EntityHelper.RplIdToEntity(vehicleId), throttle, steer);
 }
 // Test hook: drive as if the keys were held.
 void ACR_TestDrive(IEntity vehicle, float throttle, float steer)
 {
  Rpc(ACR_DriveServer, SCR_EntityHelper.EntityToRplId(vehicle), throttle, steer);
 }

 // Server: make a new character this player's command unit while they keep
 // controlling a possessed soldier (their old command unit died).
 void ACR_SetMainEntity(IEntity entity)
 {
  RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
  if (!rpl) return;
  rpl.GiveExt(RplIdentity.Local(), false);
  m_MainEntityID = rpl.Id();
  OnRplMainEntityFromID();
  Replication.BumpMe();
  SCR_PossessingManagerComponent possessing = SCR_PossessingManagerComponent.GetInstance();
  if (possessing) possessing.SetMainEntity(GetPlayerId(), GetControlledEntity(), entity, true);
 }

 // ---------------------------------------------------------------- capture bar (HUD)
 protected string m_ACR_CaptureSent = "-";
 protected Widget m_ACR_CaptureRoot;
 protected TextWidget m_ACR_CaptureText;
 protected ImageWidget m_ACR_CaptureFill;
 static const float CAPTURE_BAR_WIDTH = 260;

 void ACR_SendCapture(string label, float progress, int tone)
 {
  string key = label + "|" + ((int)(progress * 100)).ToString() + "|" + tone.ToString();
  if (key == m_ACR_CaptureSent) return;
  m_ACR_CaptureSent = key;
  Rpc(ACR_CaptureOwner, label, progress, tone);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_CaptureOwner(string label, float progress, int tone)
 {
  if (label.IsEmpty())
  {
   if (m_ACR_CaptureRoot) m_ACR_CaptureRoot.SetVisible(false);
   return;
  }
  if (!m_ACR_CaptureRoot) ACR_CreateCaptureBar();
  if (!m_ACR_CaptureRoot) return;
  m_ACR_CaptureRoot.SetVisible(true);
  if (m_ACR_CaptureText) m_ACR_CaptureText.SetText(label + "  " + ((int)(Math.Clamp(progress, 0, 1) * 100)).ToString() + "%");
  if (m_ACR_CaptureFill)
  {
   FrameSlot.SetSize(m_ACR_CaptureFill, CAPTURE_BAR_WIDTH * Math.Clamp(progress, 0, 1), 8);
   Color color = new Color(0.85, 0.85, 0.85, 0.95);
   if (tone == 1) color = new Color(0.30, 0.60, 1.00, 0.95);
   if (tone == 2) color = new Color(1.00, 0.30, 0.20, 0.95);
   m_ACR_CaptureFill.SetColor(color);
  }
 }

 protected void ACR_CreateCaptureBar()
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (!ws) return;
  m_ACR_CaptureRoot = ACR_Hud.Track(ws.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, ACR_Hud.Root()));
  FrameSlot.SetAnchorMin(m_ACR_CaptureRoot, 0.5, 0);
  FrameSlot.SetAnchorMax(m_ACR_CaptureRoot, 0.5, 0);
  FrameSlot.SetAlignment(m_ACR_CaptureRoot, 0.5, 0);
  FrameSlot.SetPos(m_ACR_CaptureRoot, 0, 70);
  FrameSlot.SetSize(m_ACR_CaptureRoot, CAPTURE_BAR_WIDTH, 40);
  m_ACR_CaptureRoot.SetZOrder(900);
  m_ACR_CaptureText = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", m_ACR_CaptureRoot));
  if (m_ACR_CaptureText)
  {
   FrameSlot.SetPos(m_ACR_CaptureText, 0, 0);
   FrameSlot.SetSize(m_ACR_CaptureText, CAPTURE_BAR_WIDTH, 22);
  }
  Widget back = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, new Color(0, 0, 0, 0.55), 0, m_ACR_CaptureRoot);
  FrameSlot.SetPos(back, 0, 26);
  FrameSlot.SetSize(back, CAPTURE_BAR_WIDTH, 8);
  m_ACR_CaptureFill = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, new Color(0.3, 0.6, 1, 0.95), 1, m_ACR_CaptureRoot));
  if (m_ACR_CaptureFill)
  {
   FrameSlot.SetPos(m_ACR_CaptureFill, 0, 26);
   FrameSlot.SetSize(m_ACR_CaptureFill, 0, 8);
  }
 }

 // After taking over a soldier from the command map, close the map: reopening
 // the game view re-initialises the camera and the damage/bleeding screen
 // effects for the new body (otherwise the view can stay zoomed in and the
 // effects may be missing until the map is toggled by hand).
 override void OnControlledEntityChanged(IEntity from, IEntity to)
 {
  super.OnControlledEntityChanged(from, to);
  if (!to || to == GetMainEntity() || !ACR_Manager.GetInstance() || GetGame().GetPlayerController() != this) return;
  GetGame().GetCallqueue().CallLater(ACR_CloseMapAfterSwitch, 250, false);
 }

 protected void ACR_CloseMapAfterSwitch()
 {
  MenuManager menus = GetGame().GetMenuManager();
  if (menus && menus.FindMenuByPreset(ChimeraMenuPreset.MapMenu)) menus.CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
 }

 void ACR_Notify(string message)
 {
  Rpc(ACR_FeedbackOwner, message);
 }

 // ---------------------------------------------------------------- command status
 bool m_ACR_IsCommander;
 bool m_ACR_SharedCommand;
 string m_ACR_CommandText;
 int m_ACR_RP;
 int m_ACR_Income;
 int m_ACR_IncomeSeconds;
 int m_ACR_IncomeAt;   // tick count when m_ACR_IncomeSeconds arrived
 void ACR_SendCommandStatus(bool isCommander, string text, bool shared, int rp, int income, int seconds)
 {
  Rpc(ACR_CommandStatusOwner, isCommander, text, shared, rp, income, seconds);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_CommandStatusOwner(bool isCommander, string text, bool shared, int rp, int income, int seconds)
 {
  m_ACR_RP = rp;
  m_ACR_Income = income;
  m_ACR_IncomeSeconds = seconds;
  m_ACR_IncomeAt = System.GetTickCount();
  m_ACR_IsCommander = isCommander;
  m_ACR_CommandText = text;
  m_ACR_SharedCommand = shared;
 }

 void ACR_PossessAimed()
 {
  vector camera[4];
  GetGame().GetWorld().GetCurrentCamera(camera);
  IEntity hit = ACR_TraceSoldier(camera[3], camera[2]);
  if (!hit)
  {
   SCR_HintManagerComponent.ShowCustomHint("Aim directly at a friendly soldier within 250 metres", "Possess soldier", 4, true);
   return;
  }
  RplId targetId = SCR_EntityHelper.EntityToRplId(hit);
  int index = m_ACR_Ids.Find(targetId);
  if (index < 0)
  {
   SCR_HintManagerComponent.ShowCustomHint("The soldier under the centre of your view is not in a friendly squad", "No eligible target", 4, true);
   return;
  }
  PrintFormat("[ACR] AIM requested %1 id=%2", m_ACR_Labels[index], targetId);
  ACR_Request(ACR_ERequest.POSSESS, targetId);
 }
 IEntity ACR_TraceSoldier(vector start, vector direction)
 {
  TraceParam trace = new TraceParam();
  trace.Start = start;
  trace.End = start + direction * 250;
  trace.Exclude = GetControlledEntity();
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  GetGame().GetWorld().TraceMove(trace, null);
  IEntity hit = trace.TraceEnt;
  while (hit && !ChimeraCharacter.Cast(hit)) hit = hit.GetParent();
  return hit;
 }

 // ---------------------------------------------------------------- soldier roster
 protected ref array<RplId> m_ACR_Ids = {};
 protected ref array<vector> m_ACR_Positions = {};
 protected ref array<int> m_ACR_States = {};
 protected ref array<string> m_ACR_Labels = {};
 protected ref array<RplId> m_ACR_UnitGroups = {};
 protected vector m_ACR_OriginalPos;
 protected ref array<ref ACR_UnitMarker> m_ACR_Markers = {};
 // Soldier markers by the soldier's RplId (the command unit's is m_ACR_OriginalMarker): a casualty
 // or a purchase adds or drops only its own markers (review P1).
 protected ref map<RplId, ACR_UnitMarker> m_ACR_MarkerById = new map<RplId, ACR_UnitMarker>();
 protected ACR_UnitMarker m_ACR_OriginalMarker;
 protected bool m_ACR_MapWasOpen;

 // The roster's names and labels, when they changed (ACR_MapFeed, review P3). Positions come on
 // their own (ACR_PositionsOwner) for the roster they were sent with (its version); until then a
 // soldier who was already on the list keeps his last position, and a new one has none.
 protected int m_ACR_RosterVersion;
 protected int m_ACR_GroupsVersion;
 void ACR_SendRosterInfo(int version, array<RplId> ids, array<int> states, array<string> labels, array<RplId> groupIds)
 {
  Rpc(ACR_RosterInfoOwner, version, ids, states, labels, groupIds);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_RosterInfoOwner(int version, array<RplId> ids, array<int> states, array<string> labels, array<RplId> groupIds)
 {
  ACR_Realign(m_ACR_Ids, m_ACR_Positions, ids);
  m_ACR_RosterVersion = version;
  m_ACR_UnitGroups.Copy(groupIds);
  m_ACR_Ids.Copy(ids);
  m_ACR_States.Copy(states);
  m_ACR_Labels.Copy(labels);
 }

 // Positions follow their ids to the new list's order; ids new to it get vector.Zero (not drawn).
 protected static void ACR_Realign(array<RplId> oldIds, array<vector> positions, array<RplId> newIds)
 {
  map<RplId, vector> byId = new map<RplId, vector>();
  for (int i = 0; i < oldIds.Count() && i < positions.Count(); i++) byId.Set(oldIds[i], positions[i]);
  positions.Clear();
  foreach (RplId id : newIds)
  {
   vector pos = vector.Zero;
   byId.Find(id, pos);
   positions.Insert(pos);
  }
 }

 // Soldier and squad-leader positions: unreliable once a second while this player has the map or
 // the RTS camera open, reliable with the feed's full resend (a lost one is replaced a second
 // later). Kept only for the roster and squad list they were sent for.
 void ACR_SendPositions(int rosterVersion, array<vector> rosterPos, vector original, int groupsVersion, array<vector> leaderPos, bool reliable)
 {
  if (reliable) Rpc(ACR_PositionsReliableOwner, rosterVersion, rosterPos, original, groupsVersion, leaderPos);
  else Rpc(ACR_PositionsOwner, rosterVersion, rosterPos, original, groupsVersion, leaderPos);
 }
 [RplRpc(RplChannel.Unreliable, RplRcver.Owner)]
 protected void ACR_PositionsOwner(int rosterVersion, array<vector> rosterPos, vector original, int groupsVersion, array<vector> leaderPos)
 {
  ACR_TakePositions(rosterVersion, rosterPos, original, groupsVersion, leaderPos);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_PositionsReliableOwner(int rosterVersion, array<vector> rosterPos, vector original, int groupsVersion, array<vector> leaderPos)
 {
  ACR_TakePositions(rosterVersion, rosterPos, original, groupsVersion, leaderPos);
 }
 protected void ACR_TakePositions(int rosterVersion, array<vector> rosterPos, vector original, int groupsVersion, array<vector> leaderPos)
 {
  m_ACR_OriginalPos = original;
  if (rosterVersion == m_ACR_RosterVersion && rosterPos.Count() == m_ACR_Ids.Count()) m_ACR_Positions.Copy(rosterPos);
  if (groupsVersion == m_ACR_GroupsVersion && leaderPos.Count() == m_ACR_Data.m_LeaderIds.Count()) m_ACR_Data.m_LeaderPositions.Copy(leaderPos);
 }

 // Server: whether this player looks at the map or through the RTS camera (positions go to him then).
 protected bool m_bACR_Viewing;
 protected bool m_bACR_ViewingSent;
 bool ACR_IsViewingMap() { return m_bACR_Viewing; }
 void ACR_MarkViewing(bool viewing) { ACR_SendViewing(viewing); }
 protected void ACR_SendViewing(bool viewing)
 {
  if (viewing == m_bACR_ViewingSent) return;
  m_bACR_ViewingSent = viewing;
  m_bACR_Viewing = viewing; // a listen-server host is its own server
  Rpc(ACR_ViewingServer, viewing);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_ViewingServer(bool viewing)
 {
  m_bACR_Viewing = viewing;
 }

 // ---------------------------------------------------------------- group leaders
 ref array<int> m_ACR_GroupSymbols = {};
 protected ref array<int> m_ACR_LeaderStates = {};
 ref array<string> m_ACR_GroupDetails = {};
 protected ref array<ref ACR_UnitMarker> m_ACR_GroupMarkers = {};

 // The squads' names, symbols and details, when they changed (review P3); flags: per squad, the
 // leader's state | commandable << 4. Positions come with ACR_PositionsOwner.
 void ACR_SendGroupInfo(int version, array<RplId> leaders, array<string> names, array<int> symbols, array<int> flags, array<RplId> groupIds, array<string> details)
 {
  Rpc(ACR_GroupInfoOwner, version, leaders, names, symbols, flags, groupIds, details);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_GroupInfoOwner(int version, array<RplId> leaders, array<string> names, array<int> symbols, array<int> flags, array<RplId> groupIds, array<string> details)
 {
  ACR_Realign(m_ACR_Data.m_LeaderIds, m_ACR_Data.m_LeaderPositions, leaders);
  m_ACR_GroupsVersion = version;
  m_ACR_Data.m_GroupIds.Copy(groupIds);
  m_ACR_GroupDetails.Copy(details);
  m_ACR_Data.m_LeaderIds.Copy(leaders);
  m_ACR_Data.m_GroupNames.Copy(names);
  m_ACR_GroupSymbols.Copy(symbols);
  m_ACR_LeaderStates.Clear();
  m_ACR_Data.m_Commandable.Clear();
  foreach (int flag : flags)
  {
   m_ACR_LeaderStates.Insert(flag & 15);
   m_ACR_Data.m_Commandable.Insert((flag >> 4) & 1);
  }
  // Squads that are gone drop out of the selection, so no order is sent for them
  // (the server refuses an order for a group it can't find; R04, 5 Oct review).
  for (int i = m_ACR_Data.m_Selected.Count() - 1; i >= 0; i--)
   if (!groupIds.Contains(m_ACR_Data.m_Selected[i])) m_ACR_Data.m_Selected.RemoveOrdered(i);
  if (m_ACR_SelectedGroup.IsValid() && !groupIds.Contains(m_ACR_SelectedGroup))
  {
   m_ACR_SelectedGroup = RplId.Invalid();
   if (!m_ACR_Data.m_Selected.IsEmpty()) m_ACR_SelectedGroup = m_ACR_Data.m_Selected[0];
  }
 }

 void ACR_SendGroupStatus(array<RplId> groupIds, array<int> status)
 {
  Rpc(ACR_GroupStatusOwner, groupIds, status);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_GroupStatusOwner(array<RplId> groupIds, array<int> status)
 {
  m_ACR_Data.m_StatusGroups.Copy(groupIds);
  m_ACR_Data.m_Status.Copy(status);
  m_ACR_Data.m_iStatusAt = System.GetTickCount();
 }

 // Client-side hint for the command menu; the server re-checks on execution.
 bool ACR_IsAircraftCrew()
 {
  IEntity body = GetControlledEntity();
  IEntity vehicle = ACR_Transport.VehicleOf(body);
  return vehicle && vehicle.FindComponent(VehicleHelicopterSimulation) && ACR_Transport.SeatOf(body) != ECompartmentType.CARGO;
 }

 bool ACR_CanCommandSquad()
 {
  if (!ACR_Squad.NativeCommandAllowed(ACR_IsAircraftCrew())) return false;
  int index = m_ACR_Data.m_LeaderIds.Find(SCR_EntityHelper.EntityToRplId(GetControlledEntity()));
  return index >= 0 && index < m_ACR_Data.m_Commandable.Count() && m_ACR_Data.m_Commandable[index] == 1;
 }

 // ---------------------------------------------------------------- map
 void ACR_ClearMarkers()
 {
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (manager)
  {
   foreach (ACR_UnitMarker marker : m_ACR_Markers) manager.ACR_RemoveMarker(marker);
   foreach (ACR_UnitMarker groupMarker : m_ACR_GroupMarkers) manager.ACR_RemoveMarker(groupMarker);
   foreach (ACR_UnitMarker baseMarker : m_ACR_BaseMarkers) manager.ACR_RemoveMarker(baseMarker);
   foreach (ACR_UnitMarker zoneMarker : m_ACR_ZoneMarkers) manager.ACR_RemoveMarker(zoneMarker);
   foreach (int contactId, ACR_UnitMarker contactMarker : m_ACR_ContactById) manager.ACR_RemoveMarker(contactMarker);
   foreach (ACR_UnitMarker vehicleMarker : m_ACR_VehicleMarkers) manager.ACR_RemoveMarker(vehicleMarker);
  }
  m_ACR_Display.ClearMarkers(manager);
  m_ACR_VehicleMarkers.Clear();
  m_ACR_ContactMarkers.Clear();
  m_ACR_ContactById.Clear();
  m_ACR_ZoneMarkers.Clear();
  m_ACR_BaseMarkers.Clear();
  m_ACR_Markers.Clear();
  m_ACR_MarkerById.Clear();
  m_ACR_OriginalMarker = null;
  m_ACR_GroupMarkers.Clear();
 }
 protected IEntity m_ACR_BodyAtMapClose;
 protected bool m_ACR_SetupMapOpen;
 int ACR_BaseMarkerCount() { return m_ACR_BaseMarkers.Count(); }
 // Setup phase, no body yet: the map shows the planned layout's bases only (ACR_Setup preview).
 void ACR_UpdateSetupMap()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  bool open = mapEntity && mapEntity.IsOpen() && ACR_SetupClient.s_bActive;
  if (!open)
  {
   if (m_ACR_SetupMapOpen)
   {
    ACR_ClearMarkers();
    ACR_CloseSetupMap();
    SCR_MapEntity.GetOnSelection().Remove(ACR_OnSetupMapClick);
    m_ACR_SelectedBase = "";
    m_ACR_SetupPicked = "";
   }
   m_ACR_SetupMapOpen = false;
   return;
  }
  SCR_MapCursorModule cursorModule = SCR_MapCursorModule.Cast(mapEntity.GetMapModule(SCR_MapCursorModule));
  if (cursorModule)
  {
   float wheel = 1;
   if (ACR_PointerOverOwnUI()) wheel = 0;
   cursorModule.SetWheelZoomMultiplier(wheel);
  }
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (!manager) return;
  if (!m_ACR_SetupMapOpen)
  {
   Print("[ACR] SETUP preview map opened");
   ACR_OpenSetupCanvas(mapEntity);
   SCR_MapEntity.GetOnSelection().Insert(ACR_OnSetupMapClick);
   m_ACR_SelectedBase = "";
   m_ACR_SetupPicked = "";
  }
  m_ACR_SetupMapOpen = true;
  // A base picked (its marker or near it): say what the next click does.
  if (m_ACR_SelectedBase != m_ACR_SetupPicked)
  {
   m_ACR_SetupPicked = m_ACR_SelectedBase;
   if (m_ACR_SelectedBase != "" && ACR_SetupClient.s_sRows.Contains("Show and edit"))
    SCR_HintManagerComponent.ShowCustomHint(string.Format("Selected: %1. Click its destination or Delete selected CP. HQs can only be moved.", m_ACR_SelectedBase), "AC: Actual", 6, true);
  }
  ACR_DrawSetupCanvas(mapEntity);
  ACR_UpdateBaseMarkers(manager);
 }
 protected string m_ACR_SetupPicked;

 protected void ACR_OpenSetupCanvas(SCR_MapEntity mapEntity)
 {
  Widget root = mapEntity.GetMapMenuRoot();
  if (!root || m_ACR_Display.GetCanvas()) return;
  CanvasWidget canvas = CanvasWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromInt(Color.WHITE), 0, root));
  if (!canvas) return;
  FrameSlot.SetAnchorMin(canvas, 0, 0);
  FrameSlot.SetAnchorMax(canvas, 1, 1);
  FrameSlot.SetOffsets(canvas, 0, 0, 0, 0);
  canvas.SetZOrder(900);
  m_ACR_Display.SetCanvas(canvas);
 }

 protected void ACR_DrawSetupCanvas(SCR_MapEntity mapEntity)
 {
 }

 protected void ACR_CloseSetupMap()
 {
  if (m_ACR_Display.GetCanvas()) m_ACR_Display.GetCanvas().RemoveFromHierarchy();
  m_ACR_Display.SetCanvas(null);
 }

 protected int ACR_SetupDotAt(int mx, int my)
 {
  return -1;
 }

 void ACR_DeleteSetupSelection()
 {
  ACR_MarkUiClick();
  if (!ACR_SetupClient.s_bActive || !ACR_SetupClient.s_bAllowed) return;
  if (m_ACR_SelectedBase == "")
  {
   SCR_HintManagerComponent.ShowCustomHint("Click a capture point first, then Delete selected CP.", "AC: Actual", 5, true);
   return;
  }
  ACR_RequestSetup("delete:" + m_ACR_SelectedBase);
  m_ACR_SelectedBase = "";
  m_ACR_SetupPicked = "";
 }

 // Layout editor (aca-f07 step 2): click a base to pick it, then click where it should go.
 protected void ACR_OnSetupMapClick(vector screenPos)
 {
  if (System.GetTickCount() - m_ACR_LastUiClick < 400) return; // the click picked a marker
  if (ACR_PointerOverOwnUI()) return; // a click in the deck editor doesn't pick or move a base
  if (!ACR_SetupClient.s_bAllowed || !ACR_SetupClient.s_sRows.Contains("Show and edit")) return;
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return;
  float x, z;
  mapEntity.GetMapCursorWorldPosition(x, z);
  if (m_ACR_SelectedBase == "")
  {
   int mx, my;
   WidgetManager.GetMousePos(mx, my);
   int best = ACR_SetupDotAt(mx, my);
   if (best < 0)
   {
    float bestDistance = float.MAX;
    foreach (int i, vector pos : m_ACR_Data.m_BasePositions)
    {
     float d = vector.DistanceXZ(pos, Vector(x, 0, z));
     float reach = 60;
     if (i < m_ACR_Data.m_BaseRadii.Count()) reach = Math.Max(reach, m_ACR_Data.m_BaseRadii[i]);
     if (d < reach && d < bestDistance) { bestDistance = d; best = i; }
    }
   }
   if (best >= 0 && best < m_ACR_Data.m_BaseNames.Count()) m_ACR_SelectedBase = m_ACR_Data.m_BaseNames[best];
   return;
  }
  string name = m_ACR_SelectedBase;
  m_ACR_SelectedBase = "";
  m_ACR_SetupPicked = "";
  ACR_RequestSetupMove(name, x, z);
 }
 void ACR_UpdateMap()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  bool open = mapEntity && mapEntity.IsOpen();
  ACR_SendViewing(open || ACR_RTSCameraManager.IsActive());
  m_ACR_Data.m_iMyId = GetPlayerId();
  if (!open)
  {
   if (m_ACR_MapWasOpen)
   {
    ACR_ClearMarkers();
    ACR_ClosePanel();
    m_ACR_BodyAtMapClose = GetControlledEntity();
   }
   m_ACR_MapWasOpen = false;
   ACR_PingDisplay.UpdateHud(m_ACR_Data, GetControlledEntity(), true);
   return;
  }
  ACR_PingDisplay.UpdateHud(m_ACR_Data, GetControlledEntity(), false);
  // The mouse wheel over our panel scrolls its lists, not the map: the cursor module's
  // public wheel multiplier is set to 0 there (modding the module itself broke the map).
  SCR_MapCursorModule cursorModule = SCR_MapCursorModule.Cast(mapEntity.GetMapModule(SCR_MapCursorModule));
  if (cursorModule)
  {
   float wheel = 1;
   if (ACR_OverOwnUI(WidgetManager.GetWidgetUnderCursor())) wheel = 0;
   cursorModule.SetWheelZoomMultiplier(wheel);
  }
  ACR_UpdatePicker();
  ACR_SendSelectionIfChanged();
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (!manager) return;
  if (!m_ACR_MapWasOpen)
  {
   // The map opens in a different body than it was closed in (took over a soldier,
   // went back, build mode...): drop the old selection, or the next right-click orders
   // a squad picked long ago (1 Oct live test: the construction truck drove off east).
   if (GetControlledEntity() != m_ACR_BodyAtMapClose)
   {
    m_ACR_Data.m_Selected.Clear();
    m_ACR_SelectedGroup = RplId.Invalid();
    m_ACR_SelectedBase = "";
    m_ACR_CursorMode = ACR_ECursorMode.MOVE;
   }
   // Keep the map's existing pan and zoom; opening it must not jump to the soldier.
   ACR_OpenPanel(mapEntity);
  }
  m_ACR_MapWasOpen = true;
  ACR_UpdatePanel();
  ACR_UpdateBoxLayer();
  ACR_DrawOrders(mapEntity);
  ACR_UpdateSoldierMarkers(manager);
  ACR_UnitMarker original = m_ACR_OriginalMarker;
  original.m_Label = "COMMAND UNIT [F8]";
  // Off the map until its position comes (first open after joining, or no command unit), like
  // the squad and soldier markers: not in the map's corner.
  vector originalAt = m_ACR_OriginalPos;
  if (originalAt == vector.Zero) originalAt = Vector(-100000, 0, -100000);
  original.SetWorldPos(originalAt[0], originalAt[2]);
  original.RefreshStyle();
  ACR_UpdateGroupMarkers(manager);
  ACR_UpdateBaseMarkers(manager);
  ACR_UpdateZoneMarkers(manager);
  ACR_UpdateContactMarkers(manager);
  ACR_UpdateVehicleMarkers(manager);
  ACR_UpdateSharedMarkers(manager);
  if (m_ACR_Panel)
  {
   array<string> feedLines = {};
   ACR_FeedLines(feedLines);
   m_ACR_Panel.SetFeed(feedLines);
  }
 }
 // Soldier markers (review P1). Leaders are drawn by their group marker, and the soldiers of a
 // squad the calm map hides (ACR_SoldiersHidden) get none at all: zoomed out that is every one, so
 // nothing is updated. A marker is kept by its soldier's RplId, so a casualty or a purchase
 // creates or removes only its own; the rest are moved and restyled in place.
 protected void ACR_UpdateSoldierMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (!m_ACR_OriginalMarker)
  {
   m_ACR_OriginalMarker = ACR_NewSoldierMarker(manager);
   m_ACR_OriginalMarker.m_Original = true;
  }
  map<RplId, bool> leaders = new map<RplId, bool>();
  foreach (RplId leaderId : m_ACR_Data.m_LeaderIds) leaders.Set(leaderId, true);
  map<RplId, bool> hiddenGroups = new map<RplId, bool>();
  map<RplId, int> wanted = new map<RplId, int>();
  foreach (int rosterIndex, RplId unitId : m_ACR_Ids)
  {
   if (leaders.Contains(unitId)) continue;
   RplId groupId = m_ACR_UnitGroups[rosterIndex];
   bool hidden;
   if (!hiddenGroups.Find(groupId, hidden))
   {
    hidden = ACR_SoldiersHidden(groupId);
    hiddenGroups.Set(groupId, hidden);
   }
   if (!hidden) wanted.Set(unitId, rosterIndex);
  }
  // Drop the markers of soldiers who are gone or now hidden.
  for (int m = m_ACR_Markers.Count() - 1; m >= 0; m--)
  {
   ACR_UnitMarker old = m_ACR_Markers[m];
   if (old == m_ACR_OriginalMarker) continue;
   if (old && wanted.Contains(old.m_UnitId) && m_ACR_MarkerById.Get(old.m_UnitId) == old) continue;
   if (old)
   {
    if (m_ACR_MarkerById.Get(old.m_UnitId) == old) m_ACR_MarkerById.Remove(old.m_UnitId);
    manager.ACR_RemoveMarker(old);
   }
   m_ACR_Markers.Remove(m);
  }
  foreach (RplId shownId, int r : wanted)
  {
   ACR_UnitMarker marker = m_ACR_MarkerById.Get(shownId);
   if (!marker)
   {
    marker = ACR_NewSoldierMarker(manager);
    m_ACR_MarkerById.Set(shownId, marker);
   }
   marker.m_UnitId = shownId;
   marker.m_State = m_ACR_States[r];
   marker.m_Label = m_ACR_Labels[r];
   marker.m_GroupId = m_ACR_UnitGroups[r];
   marker.m_Selected = ACR_IsSelected(marker.m_GroupId);
   if (marker.m_State == 1) marker.m_Label += " [PLAYER]";
   vector pos = vector.Zero;
   if (r < m_ACR_Positions.Count()) pos = m_ACR_Positions[r];
   IEntity streamed = SCR_EntityHelper.RplIdToEntity(shownId);
   if (streamed) pos = streamed.GetOrigin();
   if (pos == vector.Zero) pos = Vector(-100000, 0, -100000); // off the map until its position comes
   marker.SetWorldPos(pos[0], pos[2]);
   marker.RefreshStyle();
  }
 }

 protected ACR_UnitMarker ACR_NewSoldierMarker(SCR_MapMarkerManagerComponent manager)
 {
  ACR_UnitMarker marker = new ACR_UnitMarker();
  manager.ACR_AddSimpleMarker(marker, m_ACR_Markers);
  return marker;
 }

 // Whether the calm map hides this squad's soldiers (ACR_PlayerMapDeclutter overrides it).
 bool ACR_SoldiersHidden(RplId groupId)
 {
  return false;
 }

 void ACR_UpdateGroupMarkers(SCR_MapMarkerManagerComponent manager)
 {
  // The markers stay in the roster's squad order (other code reads m_ACR_GroupMarkers[g]); when the
  // squads change, each squad keeps its own marker and only new squads get one (review P1).
  bool same = m_ACR_GroupMarkers.Count() == m_ACR_Data.m_LeaderIds.Count();
  for (int s = 0; same && s < m_ACR_GroupMarkers.Count(); s++)
   if (!m_ACR_GroupMarkers[s] || m_ACR_GroupMarkers[s].m_GroupId != m_ACR_Data.m_GroupIds[s]) same = false;
  if (!same)
  {
   map<RplId, ACR_UnitMarker> kept = new map<RplId, ACR_UnitMarker>();
   array<ref ACR_UnitMarker> holding = {};
   foreach (ACR_UnitMarker old : m_ACR_GroupMarkers)
   {
    if (!old) continue;
    holding.Insert(old);
    kept.Set(old.m_GroupId, old);
   }
   m_ACR_GroupMarkers.Clear();
   for (int i = 0; i < m_ACR_Data.m_LeaderIds.Count(); i++)
   {
    int symbolKey = m_ACR_GroupSymbols[i * 4] * 1000 + m_ACR_GroupSymbols[i * 4 + 1] * 10 + m_ACR_GroupSymbols[i * 4 + 2];
    ACR_UnitMarker reuse = kept.Get(m_ACR_Data.m_GroupIds[i]);
    if (reuse && reuse.m_iSymbolKey == symbolKey)
    {
     kept.Remove(m_ACR_Data.m_GroupIds[i]);
     m_ACR_GroupMarkers.Insert(reuse);
     continue;
    }
    ACR_UnitMarker marker = new ACR_UnitMarker();
    marker.m_GroupId = m_ACR_Data.m_GroupIds[i];
    int icons = m_ACR_GroupSymbols[i * 4];
    int dimension = m_ACR_GroupSymbols[i * 4 + 1];
    SCR_MapMarkerBase prepared = manager.PrepareMilitaryMarker(EMilitarySymbolIdentity.BLUFOR, dimension, icons);
    if (prepared)
    {
     marker.SetType(prepared.GetType());
     marker.SetMarkerConfigID(prepared.GetMarkerConfigID());
     marker.SetFlags(prepared.GetFlags());
    }
    else
    {
     marker.SetType(SCR_EMapMarkerType.SIMPLE);
     marker.SetMarkerConfigID(1);
    }
    marker.SetCanBeRemovedByOwner(false);
    m_ACR_GroupMarkers.Insert(marker);
    manager.InsertStaticMarker(marker, true);
    // Not "owned" by the player, so vanilla doesn't open the marker edit dialog on click.
    marker.SetMarkerOwnerID(-1);
   }
   // Squads that are gone, and markers whose symbol changed, go. Every old marker not reused, not
   // just those left in 'kept': separate commander markers all carry RplId.Invalid(), so with two
   // commanders 'kept' remembered only one and the others stayed on the map as frozen ghosts.
   foreach (ACR_UnitMarker gone : holding)
    if (!m_ACR_GroupMarkers.Contains(gone)) manager.ACR_RemoveMarker(gone);
  }
  for (int i = 0; i < m_ACR_GroupMarkers.Count(); i++)
  {
   ACR_UnitMarker marker = m_ACR_GroupMarkers[i];
   marker.m_UnitId = m_ACR_Data.m_LeaderIds[i];
   marker.m_Label = ACR_GroupLabel(i, m_ACR_Data.m_GroupNames[i]);
   marker.m_GroupId = m_ACR_Data.m_GroupIds[i];
   marker.m_bCommandUnit = (m_ACR_GroupSymbols[i * 4 + 3] & 2048) != 0;
   marker.m_Selected = ACR_IsSelected(marker.m_GroupId);
   marker.m_State = m_ACR_LeaderStates[i];
   if (marker.m_State == 1) marker.m_Label += " [PLAYER LEADER]";
   else marker.m_Label += " [AI LEADER]";
   vector pos = m_ACR_Data.m_LeaderPositions[i];
   if (pos == vector.Zero) pos = Vector(-100000, 0, -100000); // a new squad: off the map until its position comes
   marker.SetWorldPos(pos[0], pos[2]);
   if (!marker.m_GroupSymbol) marker.m_GroupSymbol = new SCR_MilitarySymbol();
   marker.m_GroupSymbol.Set(EMilitarySymbolIdentity.BLUFOR, m_ACR_GroupSymbols[i * 4 + 1], m_ACR_GroupSymbols[i * 4], m_ACR_GroupSymbols[i * 4 + 2]);
   marker.m_iSymbolKey = m_ACR_GroupSymbols[i * 4] * 1000 + m_ACR_GroupSymbols[i * 4 + 1] * 10 + m_ACR_GroupSymbols[i * 4 + 2];
   marker.RefreshStyle();
  }
 }
 void ACR_PossessMapCursor()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen()) return;
  float x, z;
  mapEntity.GetMapCursorWorldPosition(x, z);
  float best = 24;
  ACR_UnitMarker selected;
  array<ACR_UnitMarker> candidates = {};
  foreach (ACR_UnitMarker unitMarker : m_ACR_Markers) candidates.Insert(unitMarker);
  foreach (ACR_UnitMarker groupMarker : m_ACR_GroupMarkers) candidates.Insert(groupMarker);
  foreach (ACR_UnitMarker marker : candidates)
  {
   if (marker.m_bCommandUnit) continue;
   int pos[2];
   marker.GetWorldPos(pos);
   float dx = x - pos[0];
   float dz = z - pos[1];
   float pixels = Math.Sqrt(dx * dx + dz * dz) * mapEntity.GetMapWidget().PixelPerUnit();
   if (pixels >= best) continue;
   best = pixels;
   selected = marker;
  }
  if (selected && selected.m_Original) ACR_Request(ACR_ERequest.RETURN);
  else if (selected) ACR_Request(ACR_ERequest.POSSESS, selected.m_UnitId);
  else SCR_HintManagerComponent.ShowCustomHint("Place the cursor over a unit marker, then press F7", "Possess soldier", 4, true);
 }

 // A left-click on a squad's marker: select it (a pile of overlapping markers: the next one in it).
 // The calm map opens a list of the squads behind a "×N" instead (ACR_PlayerMapDeclutter).
 void ACR_PickGroup(ACR_UnitMarker marker)
 {
  ACR_SelectGroup(ACR_CycleOverlap(marker));
 }

 // Squads whose markers overlap (a squad riding in a truck, a crew next to its
 // vehicle): the first click selects the marker on top, each further click on
 // the pile selects the next squad in it.
 RplId ACR_CycleOverlap(ACR_UnitMarker clicked)
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!clicked) return RplId.Invalid();
  if (!mapEntity || !mapEntity.GetMapWidget() || !clicked.m_GroupId.IsValid()) return clicked.m_GroupId;
  int at[2];
  clicked.GetWorldPos(at);
  float pixelsPerMetre = mapEntity.GetMapWidget().PixelPerUnit();
  array<ACR_UnitMarker> candidates = {};
  foreach (ACR_UnitMarker groupMarker : m_ACR_GroupMarkers) candidates.Insert(groupMarker);
  foreach (ACR_UnitMarker unitMarker : m_ACR_Markers) candidates.Insert(unitMarker);
  array<RplId> pile = {};
  foreach (ACR_UnitMarker marker : candidates)
  {
   if (!marker || marker.m_bContact || marker.m_BaseIndex >= 0 || marker.m_Original) continue;
   if (!marker.m_GroupId.IsValid() || pile.Contains(marker.m_GroupId)) continue;
   int pos[2];
   marker.GetWorldPos(pos);
   float dx = pos[0] - at[0];
   float dz = pos[1] - at[1];
   if (Math.Sqrt(dx * dx + dz * dz) * pixelsPerMetre > 14) continue;
   pile.Insert(marker.m_GroupId);
  }
  int index = pile.Find(m_ACR_SelectedGroup);
  if (pile.Count() < 2 || index < 0) return clicked.m_GroupId;
  int next = index + 1;
  if (next >= pile.Count()) next = 0;
  return pile[next];
 }

 // ---------------------------------------------------------------- test harness hooks
 bool ACR_SelectMarker(RplId id, bool groupMarker)
 {
  array<ref ACR_UnitMarker> markers = m_ACR_Markers;
  if (groupMarker) markers = m_ACR_GroupMarkers;
  foreach (ACR_UnitMarker marker : markers)
   if (marker.m_UnitId == id) { ACR_Request(ACR_ERequest.POSSESS, marker.m_UnitId); return true; }
  return false;
 }
 string ACR_DescribeGroupMarker(RplId id)
 {
  foreach (ACR_UnitMarker marker : m_ACR_GroupMarkers)
   if (marker.m_UnitId == id)
    return string.Format("label=%1 playerState=%2 widget=%3", marker.m_Label, marker.m_State, marker.GetRootWidget() != null);
  return "missing";
 }

 // ---------------------------------------------------------------- selection + command panel
 RplId m_ACR_SelectedGroup = RplId.Invalid();
 ACR_ECursorMode m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 ref ACR_CommandPanel m_ACR_Panel;
 protected int m_ACR_LastUiClick;

 RplId ACR_GetSelectedGroup() { return m_ACR_SelectedGroup; }
 void ACR_MarkUiClick() { m_ACR_LastUiClick = System.GetTickCount(); }

 // Several squads can be selected (Ctrl + drag a box, Shift + click to add or
 // remove one). m_ACR_SelectedGroup is the first of them: the one the panel shows.

 bool ACR_IsSelected(RplId groupId)
 {
  return groupId.IsValid() && m_ACR_Data.m_Selected.Contains(groupId);
 }

 protected void ACR_SetSelection(array<RplId> groups)
 {
  m_ACR_Data.m_Selected.Clear();
  foreach (RplId id : groups) if (id.IsValid() && !m_ACR_Data.m_Selected.Contains(id)) m_ACR_Data.m_Selected.Insert(id);
  m_ACR_SelectedGroup = RplId.Invalid();
  if (!m_ACR_Data.m_Selected.IsEmpty()) m_ACR_SelectedGroup = m_ACR_Data.m_Selected[0];
 }

 ACR_MapData ACR_GetData() { return m_ACR_Data; }

 void ACR_ToggleRTSCamera()
 {
  if (GetGame().GetMenuManager().IsAnyMenuOpen())
  {
   GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
   ACR_RTSCameraManager.OpenAfterMap();
   return;
  }
  ACR_RTSCameraManager.Toggle();
 }

 bool ACR_ModifierHeld(string action)
 {
  InputManager input = GetGame().GetInputManager();
  return input && input.GetActionValue(action) > 0.5;
 }

 void ACR_SelectGroup(RplId groupId)
 {
  m_ACR_SelectedBase = "";
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  array<RplId> next = {};
  if (ACR_ModifierHeld("ACR_MapShift"))
  {
   next.Copy(m_ACR_Data.m_Selected);
   if (next.Contains(groupId)) next.RemoveItem(groupId);
   else next.Insert(groupId);
  }
  else next.Insert(groupId);
  ACR_SetSelection(next);
 }

 // Send a command for every selected squad (orders without a map point).
 void ACR_RequestSelected(int command)
 {
  foreach (RplId id : m_ACR_Data.m_Selected) ACR_Request(command, id);
 }

 // ---------------------------------------------------------------- box selection
 // Hold Ctrl: a see-through layer covers the map, so a left-drag draws a box
 // instead of panning. The squads inside are worked out once, on release, in
 // map coordinates, so frame rate, zoom or panning can't throw the result off.
 // A Ctrl+click without dragging (under ~6 px) picks the nearest squad instead.
 protected Widget m_ACR_BoxLayer;
 protected ref ACR_BoxHandler m_ACR_BoxHandler;
 protected bool m_ACR_Boxing;
 protected vector m_ACR_BoxStart;
 protected int m_ACR_BoxStartX;
 protected int m_ACR_BoxStartY;
 static const int BOX_DEAD_ZONE = 6;

 protected void ACR_CreateBoxLayer(Widget root)
 {
  m_ACR_BoxLayer = GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.BLEND | WidgetFlags.STRETCH, new Color(0, 0, 0, 0.01), 0, root);
  if (!m_ACR_BoxLayer) return;
  FrameSlot.SetAnchorMin(m_ACR_BoxLayer, 0, 0);
  FrameSlot.SetAnchorMax(m_ACR_BoxLayer, 1, 1);
  FrameSlot.SetOffsets(m_ACR_BoxLayer, 0, 0, 0, 0);
  m_ACR_BoxLayer.SetZOrder(950); // over the map and markers, under the panel
  m_ACR_BoxLayer.SetVisible(false);
  m_ACR_BoxHandler = new ACR_BoxHandler();
  m_ACR_BoxHandler.m_Owner = this;
  m_ACR_BoxLayer.AddHandler(m_ACR_BoxHandler);
 }

 // Each map frame: the layer is up only while Ctrl is held (or a box is in progress).
 protected void ACR_UpdateBoxLayer()
 {
  if (!m_ACR_BoxLayer) return;
  bool want = m_ACR_Boxing || (m_ACR_IsCommander && ACR_ModifierHeld("ACR_MapCtrl"));
  if (m_ACR_BoxLayer.IsVisible() != want) m_ACR_BoxLayer.SetVisible(want);
 }

 void ACR_BoxDown(int x, int y)
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return;
  float wx, wz;
  mapEntity.GetMapCursorWorldPosition(wx, wz);
  m_ACR_BoxStart = Vector(wx, 0, wz);
  m_ACR_BoxStartX = x;
  m_ACR_BoxStartY = y;
  m_ACR_Boxing = true;
 }

 void ACR_BoxUp(int x, int y)
 {
  if (!m_ACR_Boxing) return;
  m_ACR_Boxing = false;
  ACR_MarkUiClick(); // the map's own click handler must not clear what we just picked
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return;
  float wx, wz;
  mapEntity.GetMapCursorWorldPosition(wx, wz);
  vector end = Vector(wx, 0, wz);
  bool drag = Math.AbsInt(x - m_ACR_BoxStartX) > BOX_DEAD_ZONE || Math.AbsInt(y - m_ACR_BoxStartY) > BOX_DEAD_ZONE;
  array<RplId> picked = {};
  if (ACR_ModifierHeld("ACR_MapShift")) picked.Copy(m_ACR_Data.m_Selected);
  if (drag)
  {
   float minX = Math.Min(m_ACR_BoxStart[0], end[0]);
   float maxX = Math.Max(m_ACR_BoxStart[0], end[0]);
   float minZ = Math.Min(m_ACR_BoxStart[2], end[2]);
   float maxZ = Math.Max(m_ACR_BoxStart[2], end[2]);
   foreach (int i, RplId groupId : m_ACR_Data.m_GroupIds)
   {
    if (i >= m_ACR_Data.m_Commandable.Count() || m_ACR_Data.m_Commandable[i] != 1) continue;
    vector pos = m_ACR_Data.LeaderPos(i);
    if (pos[0] >= minX && pos[0] <= maxX && pos[2] >= minZ && pos[2] <= maxZ && !picked.Contains(groupId)) picked.Insert(groupId);
   }
  }
  else
  {
   // Nearest commandable squad within ~14 px of the click toggles in or out.
   int best = -1;
   float bestDistance = 14;
   foreach (int j, RplId candidate : m_ACR_Data.m_GroupIds)
   {
    if (j >= m_ACR_Data.m_Commandable.Count() || m_ACR_Data.m_Commandable[j] != 1) continue;
    vector leader = m_ACR_Data.LeaderPos(j);
    int sx, sy, cx, cy;
    mapEntity.WorldToScreen(leader[0], leader[2], sx, sy, true);
    mapEntity.WorldToScreen(end[0], end[2], cx, cy, true);
    float d = Math.Sqrt((sx - cx) * (sx - cx) + (sy - cy) * (sy - cy));
    if (d < bestDistance) { bestDistance = d; best = j; }
   }
   if (best >= 0)
   {
    RplId hit = m_ACR_Data.m_GroupIds[best];
    if (picked.Contains(hit)) picked.RemoveItem(hit);
    else picked.Insert(hit);
   }
  }
  m_ACR_SelectedBase = "";
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  ACR_SetSelection(picked);
 }



 // ---------------------------------------------------------------- pending orders
 // Drawn faint the moment you click, until the server's order lines catch up.

 protected void ACR_AddPending(vector from, vector to)
 {
  m_ACR_Data.m_PendingFrom.Insert(from);
  m_ACR_Data.m_PendingTo.Insert(to);
  m_ACR_Data.m_PendingUntil.Insert(System.GetTickCount() + 2500);
 }


 protected void ACR_OpenPanel(SCR_MapEntity mapEntity)
 {
  ACR_ClosePanel();
  Widget root = mapEntity.GetMapMenuRoot();
  if (!root) return;
  m_ACR_Panel = new ACR_CommandPanel();
  m_ACR_Panel.Create(root, this);
  // Full-screen canvas for order lines; ignores the cursor so map clicks pass through.
  CanvasWidget canvas = CanvasWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromInt(Color.WHITE), 0, root));
  if (canvas)
  {
   FrameSlot.SetAnchorMin(canvas, 0, 0);
   FrameSlot.SetAnchorMax(canvas, 1, 1);
   FrameSlot.SetOffsets(canvas, 0, 0, 0, 0);
   canvas.SetZOrder(900);
  }
  m_ACR_Display.SetCanvas(canvas);
  ACR_CreateBoxLayer(root);
  SCR_MapEntity.GetOnSelection().Insert(ACR_OnMapClick);
 }

 // The cursor is over one of our boxes (the panel, the feed, the deck editor, the 2IC's:
 // ACR_TwoICView). Every map input checks it, so a box laid over the map takes the click, the
 // drag and the wheel and the map under it gets none of them: the wheel multiplier
 // (ACR_UpdateMap), the map's select and right-button pan (SCR_MapCursorModule in
 // ACR_CommandPanel.c), right-click orders and drag to aim, setup clicks and the ping key.
 // A new box over the map is added here (or in an override), nowhere else.
 protected bool ACR_OverOwnUI(Widget widget)
 {
  if (m_ACR_Panel && m_ACR_Panel.Contains(widget)) return true;
  return ACR_DeckClient.Contains(widget);
 }

 // The same, for the widget under the mouse (or the gamepad's map cursor, which moves it).
 bool ACR_PointerOverOwnUI()
 {
  return ACR_OverOwnUI(WidgetManager.GetWidgetUnderCursor());
 }

 // A group marker's label (the 2IC tags its squads: ACR_TwoICView).
 protected string ACR_GroupLabel(int index, string label)
 {
  return label;
 }

 protected void ACR_ClosePanel()
 {
  m_ACR_MineCorners.Clear();
  SCR_MapEntity.GetOnSelection().Remove(ACR_OnMapClick);
  m_ACR_AimHeld = false; // closed with the button held: the release listener is gone with the map
  if (m_ACR_Panel) m_ACR_Panel.Destroy();
  m_ACR_Panel = null;
  if (m_ACR_Display.GetCanvas()) m_ACR_Display.GetCanvas().RemoveFromHierarchy();
  m_ACR_Display.SetCanvas(null);
  ACR_ClosePicker();
  if (m_ACR_BoxLayer) m_ACR_BoxLayer.RemoveFromHierarchy();
  m_ACR_BoxLayer = null;
  m_ACR_Boxing = false;
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 }

 // Left-click on empty map clears the selection. Clicks on markers or the panel don't.
 protected void ACR_OnMapClick(vector screenPos)
 {
  if (ACR_EditorOpen()) return;
  if (System.GetTickCount() - m_ACR_LastUiClick < 400) return;
  if (ACR_OverOwnUI(WidgetManager.GetWidgetUnderCursor())) return;
  ACR_ClosePicker();
  if (ACR_ModifierHeld("ACR_MapAlt"))
  {
   SCR_MapEntity pingMap = SCR_MapEntity.GetMapInstance();
   float px, pz;
   if (pingMap)
   {
    pingMap.GetMapCursorWorldPosition(px, pz);
    ACR_RequestPing(ACR_EPing.LOOK, Vector(px, 0, pz), false);
   }
   return;
  }
  if (ACR_ModifierHeld("ACR_MapShift") || ACR_ModifierHeld("ACR_MapCtrl")) return;
  if (m_ACR_PendingEntry >= 0)
  {
   SCR_MapEntity deployMap = SCR_MapEntity.GetMapInstance();
   float dx, dz;
   if (deployMap)
   {
    deployMap.GetMapCursorWorldPosition(dx, dz);
    if (ACR_ClickBaseAt(dx, dz)) return;
   }
  }
  m_ACR_Data.m_Selected.Clear();
  m_ACR_SelectedGroup = RplId.Invalid();
  m_ACR_SelectedBase = "";
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 }

 bool ACR_IsCommanderForHints() { return m_ACR_IsCommander; }
 ACR_CommandPanel ACR_CommandPanelForTest() { return m_ACR_Panel; }

 void ACR_RefreshKeyHintsButton() { ACR_PlayerPanel.ACR_RefreshKeyHintsButton(this); }
 void ACR_OpenPickerPublic(SCR_MapEntity mapEntity, vector pos) { ACR_OpenPicker(mapEntity, pos); }
 void ACR_ClosePickerPublic() { ACR_ClosePicker(); }
 void ACR_SendFob(string label, bool build) { Rpc(ACR_FobServer, label, build); }
 void ACR_SendFobAI(string label) { Rpc(ACR_FobAIServer, label); }
 void ACR_SendGoToGround(string label) { Rpc(ACR_GoToGroundServer, label); }
 void ACR_SendDisperse(RplId groupId) { Rpc(ACR_DisperseServer, groupId); }
 void ACR_SendDrawIedKit() { Rpc(ACR_DrawIedKitServer); }
 void ACR_SendToggleDisguise() { Rpc(ACR_ToggleDisguiseServer); }
 void ACR_SendSupport(int support, vector target, RplId groupId) { Rpc(ACR_SupportServer, support, target, groupId); }
 void ACR_SendNetworkOrder(int order, vector pos) { Rpc(ACR_NetworkOrderServer, order, pos); }

 protected void ACR_UpdatePanel() { ACR_PlayerPanel.ACR_UpdatePanel(this); }

 void ACR_OnPanelAction(int action) { ACR_PlayerPanel.ACR_OnPanelAction(this, action); }

 static bool ACR_EditorOpen() { return ACR_PlayerPanel.ACR_EditorOpen(); }

 // ---------------------------------------------------------------- drag to aim
 // Roadblocks, Fortify and the minefields face the threat (ACR_Intel.ThreatPoint) on a plain
 // right-click, as before. Right-click and drag sets the facing instead: the order goes where
 // the hold began and faces where the button came up, with an arrow on the map while dragging
 // (the map doesn't pan then: see SCR_MapCursorModule in ACR_CommandPanel).
 // The facing goes with the order request (ACR_Requests.s_vAim), so it holds in multiplayer;
 // the AI commander keeps ThreatPoint.
 // Minefield points (#200): the right-clicks so far for the armed mine order, for one squad in one mode.
 ref array<vector> m_ACR_MineCorners = {};
 RplId m_ACR_MineCornersFor;
 int m_ACR_MineCornersMode = -1;
 static const int MINE_SAME_PX = 14;      // px: the last point clicked again
 bool m_ACR_AimHeld;
 vector m_ACR_AimFrom;
 int m_ACR_AimFromX;
 int m_ACR_AimFromY;
 float m_ACR_AimAt;
 static const int AIM_DEAD_ZONE = 12;     // px: a smaller move is a plain click
 static const float AIM_MAX_MS = 15000;   // a press held longer is dropped
 bool m_ACR_AimReleased;        // ACR_AimUp passes its facing to ACR_HandleMapRightClick
 vector m_ACR_AimRelease;
 ref ACR_ClickGate m_ACR_ClickGate = new ACR_ClickGate(); // one right-click, one order

 bool ACR_AimArmed() { return ACR_PlayerPanel.ACR_AimArmed(this); }

 bool ACR_Aimable() { return ACR_PlayerPanel.ACR_Aimable(this); }

 void ACR_AimDown() { ACR_PlayerPanel.ACR_AimDown(this); }

 bool ACR_AimDrag(out vector to) { return ACR_PlayerPanel.ACR_AimDrag(this, to); }

 void ACR_AimUp() { ACR_PlayerPanel.ACR_AimUp(this); }

 bool ACR_HandleMapClick() { return ACR_PlayerPanel.ACR_HandleMapClick(this); }

 bool ACR_HandleMapRightClick() { return ACR_PlayerPanel.ACR_HandleMapRightClick(this); }

 protected void ACR_ToggleCursor(ACR_ECursorMode mode) { ACR_PlayerPanel.ACR_ToggleCursor(this, mode); }

 protected bool ACR_MineShapeMode() { return ACR_PlayerPanel.ACR_MineShapeMode(this); }

 protected void ACR_MineCornersCheck() { ACR_PlayerPanel.ACR_MineCornersCheck(this); }

 protected bool ACR_MineShapeClick(SCR_MapEntity mapEntity, int command, vector point) { return ACR_PlayerPanel.ACR_MineShapeClick(this, mapEntity, command, point); }

 // One point: the square there, as before. Two to four: that shape, laid by the selected squad (one squad
 // lays one field).
 void ACR_LayMineShape(int command)
 {
  array<vector> corners = {};
  corners.Copy(m_ACR_MineCorners);
  m_ACR_MineCorners.Clear();
  if (corners.Count() == 1)
  {
   ACR_IssueOrder(command, corners[0]);
   return;
  }
  array<RplId> squads = {};
  squads.Insert(m_ACR_SelectedGroup);
  if (!ACR_ConfirmOverride(squads)) return;
  Rpc(ACR_ShapeServer, command, m_ACR_SelectedGroup, corners);
 }

 // The hint's line while points are down: how many, and what they make.
 string ACR_MineShapeHint()
 {
  int count = m_ACR_MineCorners.Count();
  if (count == 0) return "";
  if (count == 1) return " 1 point: right-click it again for the 30 m square, or add points.";
  array<vector> outline = {};
  string why = ACR_MineShape.Outline(m_ACR_MineCorners, outline);
  if (!why.IsEmpty()) return string.Format(" %1 points: %2. Add or right-click the last again.", count, why);
  int mines = ACR_Minefields.MinesOf(ACR_Minefields.KindOfCursor(m_ACR_CursorMode));
  float area = ACR_MineShape.Area(outline);
  if (count == 2) return string.Format(" A %1 m line: right-click its end again to lay it, or add points.", (int)vector.DistanceXZ(m_ACR_MineCorners[0], m_ACR_MineCorners[1]));
  float every = Math.Sqrt(area / Math.Max(mines, 1));
  return string.Format(" %1 points, %2 m², a mine every %3 m or so: right-click the last again to lay it.", count, (int)area, (int)Math.Round(every));
 }

 // A shaped minefield order (#200): the corners go with the request (ACR_Requests.s_aCorners), the point is
 // their middle.
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_ShapeServer(int command, RplId targetId, array<vector> corners)
 {
  if (!ACR_Minefields.IsRequest(command) || !corners || corners.Count() < 2 || corners.Count() > ACR_MineShape.MOST_CORNERS) return;
  foreach (vector corner : corners)
  {
   if (!ACR_Manager.OnMap(corner)) return;
  }
  ACR_Requests.s_aCorners = new array<vector>();
  ACR_Requests.s_aCorners.Copy(corners);
  ACR_CommandServer(command, targetId, ACR_MineShape.Middle(corners), vector.Zero);
  ACR_Requests.s_aCorners = null;
 }

 bool ACR_IssueOrder(int command, vector target, bool append = false, vector aim = vector.Zero)
 {
  bool spread = command == ACR_ERequest.MOVE_TO || command == ACR_ERequest.QUEUE_MOVE || command == ACR_ERequest.ORDER_DEFEND || command == ACR_ERequest.QUEUE_DEFEND || command == ACR_ERequest.ORDER_RIDE;
  // Several squads: a loose line across the direction of travel, 40 m apart,
  // each keeping its place in the line (sorted left to right).
  array<RplId> squads = {};
  foreach (RplId selectedId : m_ACR_Data.m_Selected)
  {
   int selectedIndex = m_ACR_Data.m_GroupIds.Find(selectedId);
   if (selectedIndex >= 0 && selectedIndex < m_ACR_Data.m_Commandable.Count() && m_ACR_Data.m_Commandable[selectedIndex] == 1) squads.Insert(selectedId);
  }
  if (!ACR_ConfirmOverride(squads)) return true;
  vector center;
  foreach (RplId sq : squads) center = center + m_ACR_Data.LeaderPos(m_ACR_Data.m_GroupIds.Find(sq));
  if (!squads.IsEmpty()) center = center * (1.0 / squads.Count());
  vector dir = target - center;
  dir[1] = 0;
  if (dir.Length() < 1) dir = "0 0 1";
  dir.Normalize();
  vector side = Vector(dir[2], 0, -dir[0]);
  // order squads by where they stand across the line of travel
  array<float> across = {};
  foreach (RplId sq2 : squads) across.Insert(vector.Dot(m_ACR_Data.LeaderPos(m_ACR_Data.m_GroupIds.Find(sq2)) - center, side));
  for (int a = 1; a < squads.Count(); a++)
  {
   for (int b = a; b > 0 && across[b] < across[b - 1]; b--)
   {
    float t = across[b]; across[b] = across[b - 1]; across[b - 1] = t;
    RplId tid = squads[b]; squads[b] = squads[b - 1]; squads[b - 1] = tid;
   }
  }
  foreach (int k, RplId squadId : squads)
  {
   vector point = target;
   if (spread && squads.Count() > 1) point = target + side * ((k - (squads.Count() - 1) * 0.5) * 40);
   ACR_Request(command, squadId, point, aim);
   if (spread) ACR_AddPending(m_ACR_Data.LeaderPos(m_ACR_Data.m_GroupIds.Find(squadId)), point);
  }
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  return true;
 }

 // ---------------------------------------------------------------- order lines
 // A line from each friendly squad's leader to its current destination, ending
 // in a dot, like the Arma 3 mod. Colour shows the order; the selected squad's
 // line is brighter and thicker.
 ref ACR_MapData m_ACR_Data = new ACR_MapData();
 protected ref ACR_OrderDisplay m_ACR_Display = new ACR_OrderDisplay();

 void ACR_SendOrders(array<RplId> groupIds, array<vector> targets, array<int> kinds, array<int> issuers)
 {
  Rpc(ACR_OrdersOwner, groupIds, targets, kinds, issuers);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_OrdersOwner(array<RplId> groupIds, array<vector> targets, array<int> kinds, array<int> issuers)
 {
  m_ACR_Data.m_OrderGroups.Copy(groupIds);
  m_ACR_Data.m_OrderTargets.Copy(targets);
  m_ACR_Data.m_OrderKinds.Copy(kinds);
  m_ACR_Data.m_OrderIssuers.Copy(issuers);
 }


 // Order lines, status bars, base rings, zones, selections and pings: see ACR_OrderDisplay.
 protected void ACR_DrawOrders(SCR_MapEntity mapEntity)
 {
  m_ACR_Data.m_iMyId = GetPlayerId();
  vector aimTo;
  m_ACR_Data.m_bAiming = ACR_AimDrag(aimTo);
  m_ACR_Data.m_vAimFrom = m_ACR_AimFrom;
  m_ACR_Data.m_vAimTo = aimTo;
  // The minefield points so far and where the cursor is (the preview's last edge).
  ACR_MineCornersCheck();
  m_ACR_Data.m_MineCorners.Copy(m_ACR_MineCorners);
  float mcx, mcz;
  mapEntity.GetMapCursorWorldPosition(mcx, mcz);
  m_ACR_Data.m_vMineCursor = Vector(mcx, 0, mcz);
  m_ACR_Display.Draw(mapEntity, m_ACR_Data, m_ACR_Boxing, m_ACR_BoxStart);
 }



 // ---------------------------------------------------------------- intel
 // Enemies our side has spotted (server-side ACR_Intel), as hostile map symbols
 // that turn "suspected" (dashed) after a while and vanish when forgotten.
 ref array<vector> m_ACR_ContactPositions = {};
 protected ref array<int> m_ACR_ContactKinds = {};
 protected ref array<int> m_ACR_ContactCounts = {};
 protected ref array<int> m_ACR_ContactAges = {};
 protected ref array<int> m_ACR_ContactDetails = {}; // 3 per contact: id, NATO icons, heading (-1 none)
 protected ref array<string> m_ACR_Feed = {};
 protected ref array<ref ACR_UnitMarker> m_ACR_ContactMarkers = {}; // in contact order
 protected ref map<int, ref ACR_UnitMarker> m_ACR_ContactById = new map<int, ref ACR_UnitMarker>();
 static const int ACR_CONTACT_SUSPECTED = 30; // s: older sightings are drawn as suspected

 protected ref array<int> m_ACR_FeedAges = {};
 protected ref array<vector> m_ACR_FeedPos = {};
 // Where each INTEL line as shown happened (vector.Zero: nowhere), as ACR_FeedLines last made them:
 // a click on the line jumps the map there (ACR_FeedClick).
 protected ref array<vector> m_ACR_FeedShownPos = {};
 protected int m_ACR_IntelAt;
 protected int m_ACR_FeedAt;
 void ACR_SendIntel(array<vector> positions, array<int> kinds, array<int> counts, array<int> ages, array<int> details)
 {
  Rpc(ACR_IntelOwner, positions, kinds, counts, ages, details);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_IntelOwner(array<vector> positions, array<int> kinds, array<int> counts, array<int> ages, array<int> details)
 {
  m_ACR_ContactPositions.Copy(positions);
  m_ACR_ContactKinds.Copy(kinds);
  m_ACR_ContactCounts.Copy(counts);
  m_ACR_ContactAges.Copy(ages);
  m_ACR_ContactDetails.Copy(details);
  m_ACR_IntelAt = System.GetTickCount();
 }
 // The side's feed, on its own since review P3 (a contact moving no longer resends the lines).
 void ACR_SendFeed(array<string> feed, array<int> feedAges, array<vector> feedPos)
 {
  Rpc(ACR_FeedOwner, feed, feedAges, feedPos);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_FeedOwner(array<string> feed, array<int> feedAges, array<vector> feedPos)
 {
  m_ACR_Feed.Copy(feed);
  m_ACR_FeedAges.Copy(feedAges);
  m_ACR_FeedPos.Copy(feedPos);
  m_ACR_FeedAt = System.GetTickCount();
 }

 // The feed as shown: "[12s] line", newest first, nothing older than 10 min.
 protected void ACR_FeedLines(notnull array<string> lines)
 {
  lines.Clear();
  m_ACR_FeedShownPos.Clear();
  int since = ACR_MapData.Since(m_ACR_FeedAt);
  foreach (int i, string line : m_ACR_Feed)
  {
   int seconds = since;
   if (i < m_ACR_FeedAges.Count()) seconds += m_ACR_FeedAges[i];
   if (seconds > 600) break;
   vector spot = vector.Zero;
   if (i < m_ACR_FeedPos.Count()) spot = m_ACR_FeedPos[i];
   m_ACR_FeedShownPos.Insert(spot);
   string when = seconds.ToString() + "s";
   if (seconds >= 60)
   {
    int minutes = seconds / 60;
    when = minutes.ToString() + "m";
   }
   lines.Insert("[" + when + "] " + line);
  }
 }

 int ACR_ContactCount() { return m_ACR_ContactPositions.Count(); }

 // Contact i as the map shows it: last known spot, kind, head count and seconds since seen.
 void ACR_GetContact(int i, out vector pos, out int kind, out int count, out int age)
 {
  pos = m_ACR_ContactPositions[i];
  kind = 0;
  if (i < m_ACR_ContactKinds.Count()) kind = m_ACR_ContactKinds[i];
  count = 1;
  if (i < m_ACR_ContactCounts.Count()) count = m_ACR_ContactCounts[i];
  age = ACR_MapData.Since(m_ACR_IntelAt);
  if (i < m_ACR_ContactAges.Count()) age += m_ACR_ContactAges[i];
 }

 // Contact i's stable id, NATO icons and heading (degrees, -1: none seen). A contact without
 // details (a test's) gets an id from its index and its kind's plain symbol.
 void ACR_GetContactSymbol(int i, out int id, out int icons, out int heading)
 {
  id = -(i + 1);
  heading = -1;
  int kind = 0;
  if (i < m_ACR_ContactKinds.Count()) kind = m_ACR_ContactKinds[i];
  icons = ACR_Intel.IconOf(kind);
  if (i * 3 + 2 >= m_ACR_ContactDetails.Count()) return;
  id = m_ACR_ContactDetails[i * 3];
  icons = m_ACR_ContactDetails[i * 3 + 1];
  heading = m_ACR_ContactDetails[i * 3 + 2];
  if (icons == 0) icons = ACR_Intel.IconOf(kind);
 }

 // Index of the enemy contact marker within 16 px of the map cursor, or -1.
 int ACR_ContactUnderCursor(SCR_MapEntity mapEntity, float x, float z)
 {
  int cx, cy;
  mapEntity.WorldToScreen(x, z, cx, cy, true);
  int best = -1;
  float bestPixels = 16;
  foreach (int i, vector pos : m_ACR_ContactPositions)
  {
   int px, py;
   mapEntity.WorldToScreen(pos[0], pos[2], px, py, true);
   float dx = px - cx;
   float dy = py - cy;
   float pixels = Math.Sqrt(dx * dx + dy * dy);
   if (pixels < bestPixels) { bestPixels = pixels; best = i; }
  }
  return best;
 }

 // Index of our empty vehicle marker within 16 px of the map cursor, or -1.
 int ACR_VehicleUnderCursor(SCR_MapEntity mapEntity, float x, float z)
 {
  int cx, cy;
  mapEntity.WorldToScreen(x, z, cx, cy, true);
  int best = -1;
  float bestPixels = 16;
  foreach (int i, vector pos : m_ACR_VehiclePositions)
  {
   int px, py;
   mapEntity.WorldToScreen(pos[0], pos[2], px, py, true);
   float dx = px - cx;
   float dy = py - cy;
   float pixels = Math.Sqrt(dx * dx + dy * dy);
   if (pixels < bestPixels) { bestPixels = pixels; best = i; }
  }
  return best;
 }

 // Index of an owned friendly base marker within 18 px of the map cursor, or -1.
 int ACR_OwnBaseUnderCursor(SCR_MapEntity mapEntity, float x, float z)
 {
  Faction mine = SCR_FactionManager.SGetLocalPlayerFaction();
  if (!mine || !m_ACR_Data) return -1;
  string myKey = mine.GetFactionKey();
  if (myKey.IsEmpty()) return -1;
  int cx, cy;
  mapEntity.WorldToScreen(x, z, cx, cy, true);
  int best = -1;
  float bestPixels = 18;
  foreach (int i, vector pos : m_ACR_Data.m_BasePositions)
  {
   if (i >= m_ACR_Data.m_BaseOwners.Count() || m_ACR_Data.m_BaseOwners[i] != myKey) continue;
   int px, py;
   mapEntity.WorldToScreen(pos[0], pos[2], px, py, true);
   float dx = px - cx;
   float dy = py - cy;
   float pixels = Math.Sqrt(dx * dx + dy * dy);
   if (pixels < bestPixels) { bestPixels = pixels; best = i; }
  }
  return best;
 }
 int ACR_VehicleCount() { return m_ACR_VehiclePositions.Count(); }

 // Our side's empty vehicles (fog of war: only those our people can see,
 // or their last known spot for a while).
 ref array<vector> m_ACR_VehiclePositions = {};
 protected ref array<string> m_ACR_VehicleNames = {};
 protected ref array<int> m_ACR_VehicleAges = {};
 protected ref array<ref ACR_UnitMarker> m_ACR_VehicleMarkers = {};

 void ACR_SendVehicles(array<vector> positions, array<string> names, array<int> ages)
 {
  Rpc(ACR_VehiclesOwner, positions, names, ages);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_VehiclesOwner(array<vector> positions, array<string> names, array<int> ages)
 {
  m_ACR_VehiclePositions.Copy(positions);
  m_ACR_VehicleNames.Copy(names);
  m_ACR_VehicleAges.Copy(ages);
  m_ACR_VehiclesAt = System.GetTickCount();
 }
 protected int m_ACR_VehiclesAt;

 protected void ACR_UpdateVehicleMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (m_ACR_VehicleMarkers.Count() != m_ACR_VehiclePositions.Count())
  {
   foreach (ACR_UnitMarker old : m_ACR_VehicleMarkers) manager.ACR_RemoveMarker(old);
   m_ACR_VehicleMarkers.Clear();
   for (int i = 0; i < m_ACR_VehiclePositions.Count(); i++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    marker.m_bContact = true; // clicks go through: right-click next to it for Get in
    marker.m_State = 4; // before it is inserted: creating its widget styles it (no contact pip)
    manager.ACR_AddSimpleMarker(marker, m_ACR_VehicleMarkers);
   }
  }
  foreach (int v, ACR_UnitMarker vehicleMarker : m_ACR_VehicleMarkers)
  {
   // The server sends the game's name key (e.g. "#AR-Vehicle_..._Name"),
   // maybe followed by ", crew: ...": translate the key here on the client.
   string raw = m_ACR_VehicleNames[v];
   string rest = "";
   int comma = raw.IndexOf(",");
   if (comma >= 0)
   {
    rest = raw.Substring(comma, raw.Length() - comma);
    raw = raw.Substring(0, comma);
   }
   if (raw.StartsWith("#")) raw = WidgetManager.Translate(raw);
   string label = raw + " (empty)" + rest;
   int age = m_ACR_VehicleAges[v] + ACR_MapData.Since(m_ACR_VehiclesAt);
   if (age > 20)
   {
    int minutes = age / 60;
    if (minutes < 1) label += ", seen " + age.ToString() + " s ago";
    else label += ", seen " + minutes.ToString() + " min ago";
   }
   vehicleMarker.m_Label = label;
   vehicleMarker.m_State = 4;
   vector pos = m_ACR_VehiclePositions[v];
   vehicleMarker.SetWorldPos(pos[0], pos[2]);
   vehicleMarker.RefreshStyle();
  }
 }
 int m_ACR_RadioCount; // received radio calls (tests)

 void ACR_SendRadio(string message)
 {
  Rpc(ACR_RadioOwner, message);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_RadioOwner(string message)
 {
  m_ACR_RadioCount++;
  SCR_ChatPanelManager chat = SCR_ChatPanelManager.GetInstance();
  if (chat) chat.OnNewMessage("[Radio] " + message);
  ACR_RadioAudio.Play(ACR_ERadioCue.CONTACT);
 }

 void ACR_SendRadioCue(int cue)
 {
  Rpc(ACR_RadioCueOwner, cue);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_RadioCueOwner(int cue)
 {
  ACR_RadioAudio.Play(cue);
 }
 int ACR_FeedCount() { return m_ACR_Feed.Count(); }

 protected static int ACR_ContactDimension(int kind) { return ACR_Intel.DimensionOf(kind); }

 // Each contact keeps its marker while it is known (keyed by the server's contact id), so markers
 // don't blink when another contact comes or goes; one is only remade when its symbol's shape changes.
 protected void ACR_UpdateContactMarkers(SCR_MapMarkerManagerComponent manager)
 {
  m_ACR_ContactMarkers.Clear();
  array<int> used = {};
  m_ACR_Data.m_ContactArrowPos.Clear();
  m_ACR_Data.m_ContactArrowHeading.Clear();
  m_ACR_Data.m_ContactArrowAlpha.Clear();
  foreach (int i, vector pos : m_ACR_ContactPositions)
  {
   int id, icons, heading;
   ACR_GetContactSymbol(i, id, icons, heading);
   if (used.Contains(id)) id = -(i + 1);
   used.Insert(id);
   int kind = 0;
   if (i < m_ACR_ContactKinds.Count()) kind = m_ACR_ContactKinds[i];
   int count = 1;
   if (i < m_ACR_ContactCounts.Count()) count = m_ACR_ContactCounts[i];
   int age = ACR_MapData.Since(m_ACR_IntelAt);
   if (i < m_ACR_ContactAges.Count()) age += m_ACR_ContactAges[i];
   int dimension = ACR_ContactDimension(kind);
   if (ACR_Intel.Has(icons, EMilitarySymbolIcon.ROTARY_WING) || ACR_Intel.Has(icons, EMilitarySymbolIcon.FIXED_WING)) dimension = EMilitarySymbolDimension.AIR;
   int shape = icons * 10 + dimension;
   ACR_UnitMarker contact = m_ACR_ContactById.Get(id);
   if (contact && contact.m_iContactShape != shape)
   {
    manager.ACR_RemoveMarker(contact);
    contact = null;
   }
   if (!contact)
   {
    contact = new ACR_UnitMarker();
    SCR_MapMarkerBase prepared = manager.PrepareMilitaryMarker(EMilitarySymbolIdentity.OPFOR, dimension, icons);
    if (prepared)
    {
     contact.SetType(prepared.GetType());
     contact.SetMarkerConfigID(prepared.GetMarkerConfigID());
     contact.SetFlags(prepared.GetFlags());
    }
    else
    {
     contact.SetType(SCR_EMapMarkerType.SIMPLE);
     contact.SetMarkerConfigID(1);
    }
    contact.SetCanBeRemovedByOwner(false);
    contact.m_bContact = true;
    contact.m_iContactShape = shape;
    m_ACR_ContactById.Set(id, contact);
    manager.InsertStaticMarker(contact, true);
    contact.SetMarkerOwnerID(-1);
   }
   m_ACR_ContactMarkers.Insert(contact);
   float alpha = ACR_RTSOverlay.ContactAlpha(age);
   contact.m_Label = ACR_Intel.DescribeSymbol(icons, kind, count) + ", " + ACR_Intel.AgeText(age);
   contact.m_State = 3;
   contact.m_bContact = true;
   contact.m_bSuspected = age > ACR_CONTACT_SUSPECTED;
   contact.m_fAlpha = alpha;
   contact.SetWorldPos(pos[0], pos[2]);
   // Hostile while fresh, "assumed hostile" (dashed frame) once it's only a last known spot.
   int identity = EMilitarySymbolIdentity.OPFOR;
   if (contact.m_bSuspected) identity = EMilitarySymbolIdentity.ASSUMED_OPFOR;
   int echelon = ACR_Intel.EchelonOf(kind, count);
   if (!contact.m_GroupSymbol) contact.m_GroupSymbol = new SCR_MilitarySymbol();
   contact.m_GroupSymbol.Set(identity, dimension, icons, echelon);
   contact.m_iSymbolKey = shape * 1000 + echelon * 10 + identity;
   contact.RefreshStyle();
   if (heading < 0) continue;
   m_ACR_Data.m_ContactArrowPos.Insert(pos);
   m_ACR_Data.m_ContactArrowHeading.Insert(heading);
   m_ACR_Data.m_ContactArrowAlpha.Insert(alpha);
  }
  array<int> gone = {};
  foreach (int known, ACR_UnitMarker marker : m_ACR_ContactById) if (!used.Contains(known)) gone.Insert(known);
  foreach (int lost : gone)
  {
   manager.ACR_RemoveMarker(m_ACR_ContactById.Get(lost));
   m_ACR_ContactById.Remove(lost);
  }
 }

 // ---------------------------------------------------------------- bases
 protected ref array<ref ACR_UnitMarker> m_ACR_BaseMarkers = {};
 // Support danger areas (friendly fire missions), red rings with a countdown label.
 protected ref array<ref ACR_UnitMarker> m_ACR_ZoneMarkers = {};

 void ACR_SendZones(array<vector> positions, array<float> radii, array<string> labels)
 {
  Rpc(ACR_ZonesOwner, positions, radii, labels);
 }

 // The side's shaped minefields' outlines (#200, ACR_Minefields.Shapes).
 void ACR_SendMineShapes(array<vector> corners, array<int> counts)
 {
  Rpc(ACR_MineShapesOwner, corners, counts);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_MineShapesOwner(array<vector> corners, array<int> counts)
 {
  m_ACR_Data.m_MineShapeCorners.Copy(corners);
  m_ACR_Data.m_MineShapeCounts.Copy(counts);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_ZonesOwner(array<vector> positions, array<float> radii, array<string> labels)
 {
  m_ACR_Data.m_ZonePositions.Copy(positions);
  m_ACR_Data.m_ZoneRadii.Copy(radii);
  m_ACR_Data.m_ZoneLabels.Copy(labels);
 }

 protected void ACR_UpdateZoneMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (m_ACR_ZoneMarkers.Count() != m_ACR_Data.m_ZoneLabels.Count())
  {
   foreach (ACR_UnitMarker old : m_ACR_ZoneMarkers) manager.ACR_RemoveMarker(old);
   m_ACR_ZoneMarkers.Clear();
   for (int i = 0; i < m_ACR_Data.m_ZoneLabels.Count(); i++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    manager.ACR_AddSimpleMarker(marker, m_ACR_ZoneMarkers);
   }
  }
  foreach (int i, ACR_UnitMarker marker : m_ACR_ZoneMarkers)
  {
   string label = m_ACR_Data.m_ZoneLabels[i];
   // "~": a mortar team's range ring, labelled at the tube.
   bool range = label.StartsWith("~");
   // "%": civilians seen in that square (ACR_Intel.CivilianZones), violet.
   bool civilians = ACR_Intel.IsCivilianZone(label);
   if (range || civilians) label = label.Substring(1, label.Length() - 1);
   marker.m_Label = label;
   vector pos = m_ACR_Data.m_ZonePositions[i];
   marker.SetWorldPos(pos[0], pos[2]);
   marker.RefreshStyle();
   Color color = new Color(1, 0.2, 0.15, 1);
   if (range) color = new Color(0.55, 0.8, 1, 1);
   if (civilians) color = new Color(0.78, 0.6, 1, 1);
   if (marker.GetRootWidget()) marker.ACR_SetColor(color);
  }
 }

 string m_ACR_SelectedBase;
 bool m_ACR_NetworkOpen;
 bool m_ACR_MusterMode;
 int m_ACR_PendingNetwork = -1;

 void ACR_SendBases(array<vector> positions, array<string> names, array<string> owners, array<string> details, array<float> radii, array<int> contested, array<int> supplies)
 {
  Rpc(ACR_BasesOwner, positions, names, owners, details, radii, contested, supplies);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_BasesOwner(array<vector> positions, array<string> names, array<string> owners, array<string> details, array<float> radii, array<int> contested, array<int> supplies)
 {
  m_ACR_Data.m_BasePositions.Copy(positions);
  m_ACR_Data.m_BaseNames.Copy(names);
  m_ACR_Data.m_BaseOwners.Copy(owners);
  m_ACR_Data.m_BaseDetails.Copy(details);
  m_ACR_Data.m_BaseRadii.Copy(radii);
  m_ACR_Data.m_BaseContested.Copy(contested);
  m_ACR_Data.m_BaseSupply.Copy(supplies);
 }

 // FOB panel requests: one spacing per kind, with a reply. They shared one silent 500 ms stamp, so
 // supplies then an upgrade (or the 2IC panel's FOB AI) inside half a second lost the second.
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_FobServer(string label, bool build)
 {
  if (ACR_TooSoon("fobsupply", 500)) return;
  Rpc(ACR_FeedbackOwner, ACR_FobSupply.Request(this, label, build));
 }

 // FOB panel: "AI manages this FOB" (ACR_FobAI).
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_FobAIServer(string label)
 {
  if (ACR_TooSoon("fobai", 500)) return;
  Rpc(ACR_FeedbackOwner, ACR_FobAI.Toggle(this, label));
 }

 // Insurgent panel: order a hideout to go to ground (#97 item 7).
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_GoToGroundServer(string label)
 {
  if (ACR_TooSoon("gotoground", 500)) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.IsCommander(GetPlayerId())) { Rpc(ACR_FeedbackOwner, "Only the commander can do that"); return; }
  Faction faction = SCR_FactionManager.SGetPlayerFaction(GetPlayerId());
  if (!faction) { Rpc(ACR_FeedbackOwner, "No faction"); return; }
  string refusal;
  if (ACR_HideoutHomes.GoToGround(faction.GetFactionKey(), label, refusal))
   Rpc(ACR_FeedbackOwner, label + " went to ground (+15 RP)");
  else
   Rpc(ACR_FeedbackOwner, refusal);
 }

 // Insurgent panel: place a network piece (cell, IED, sabotage raid, technical raid, #97 item 8).
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_NetworkOrderServer(int kind, vector point)
 {
  if (ACR_TooSoon("network", 500)) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.IsCommander(GetPlayerId())) { Rpc(ACR_FeedbackOwner, "Only the commander can do that"); return; }
  Faction faction = SCR_FactionManager.SGetPlayerFaction(GetPlayerId());
  if (!faction) { Rpc(ACR_FeedbackOwner, "No faction"); return; }
  string refusal;
  if (ACR_InsurgentNetwork.Order(faction.GetFactionKey(), kind, point, refusal))
   Rpc(ACR_FeedbackOwner, ACR_InsurgentNetwork.NameOf(kind) + " ordered");
  else
   Rpc(ACR_FeedbackOwner, refusal);
 }

 // Insurgent panel: muster a foot band at a covered map spot (#97 item 9).
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_MusterServer(int entryIndex, vector point)
 {
  if (ACR_TooSoon("muster", 500)) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.IsCommander(GetPlayerId())) { Rpc(ACR_FeedbackOwner, "Only the commander can muster bands"); return; }
  Faction faction = SCR_FactionManager.SGetPlayerFaction(GetPlayerId());
  if (!faction) { Rpc(ACR_FeedbackOwner, "No faction"); return; }
  string refusal;
  if (ACR_Muster.Order(faction.GetFactionKey(), entryIndex, point, refusal))
   Rpc(ACR_FeedbackOwner, "Band mustering...");
  else
   Rpc(ACR_FeedbackOwner, refusal);
 }

 // Insurgent panel: disperse a selected foot band (#97 item 9).
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_DisperseServer(RplId groupId)
 {
  if (ACR_TooSoon("disperse", 500)) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.IsCommander(GetPlayerId())) { Rpc(ACR_FeedbackOwner, "Only the commander can disperse bands"); return; }
  SCR_AIGroup grp = SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(groupId));
  ACR_Squad squad = manager.FindSquadByGroup(grp);
  string refusal;
  if (ACR_Muster.Disperse(squad, refusal))
   Rpc(ACR_FeedbackOwner, "Band dispersed");
  else
   Rpc(ACR_FeedbackOwner, refusal);
 }

 // Insurgent panel: draw a hand-fired IED kit at a hideout (#97 / #244 ruling 5).
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_DrawIedKitServer()
 {
  if (ACR_TooSoon("iedkit", 500)) return;
  Faction faction = SCR_FactionManager.SGetPlayerFaction(GetPlayerId());
  if (!faction) { Rpc(ACR_FeedbackOwner, "No faction"); return; }
  string refusal;
  if (ACR_HideoutHomes.GiveIedKit(GetControlledEntity(), faction.GetFactionKey(), refusal))
   Rpc(ACR_FeedbackOwner, "IED kit drawn (-5 RP)");
  else
   Rpc(ACR_FeedbackOwner, refusal);
 }

 // Insurgent player: go plain clothes / drop disguise (#97 item 10).
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_ToggleDisguiseServer()
 {
  if (ACR_TooSoon("disguise", 500)) return;
  IEntity body = GetControlledEntity();
  if (ACR_Disguise.IsDisguised(body))
  {
   ACR_Disguise.Drop(body);
   Rpc(ACR_FeedbackOwner, "Disguise dropped");
   return;
  }
  Faction faction = SCR_FactionManager.SGetPlayerFaction(GetPlayerId());
  if (!faction) { Rpc(ACR_FeedbackOwner, "No faction"); return; }
  string refusal;
  if (ACR_Disguise.GoPlain(body, faction.GetFactionKey(), GetPlayerId(), refusal))
   Rpc(ACR_FeedbackOwner, "In plain clothes");
  else
   Rpc(ACR_FeedbackOwner, refusal);
 }

 void ACR_SelectBase(string label)
 {
  m_ACR_Data.m_Selected.Clear();
  m_ACR_SelectedGroup = RplId.Invalid();
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  m_ACR_SelectedBase = label;
 }

 string ACR_SelectedBase() { return m_ACR_SelectedBase; }

 void ACR_RequestFobUpgrade(string baseLabel, int slotIndex)
 {
  Rpc(ACR_FobUpgradeServer, baseLabel, slotIndex);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_FobUpgradeServer(string baseLabel, int slotIndex)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (ACR_TooSoon("fobupgrade", 500)) return;
  string refusal;
  string result = ACR_FobSlots.OrderUpgrade(manager, this, baseLabel, slotIndex, refusal);
  if (!refusal.IsEmpty()) Rpc(ACR_FeedbackOwner, refusal);
  else if (!result.IsEmpty()) Rpc(ACR_FeedbackOwner, result);
 }

 void ACR_TestFobUpgrade(string baseLabel, int slotIndex)
 {
  Rpc(ACR_FobUpgradeServer, baseLabel, slotIndex);
 }


 // Text symbols remain readable at every zoom; ownership still supplies the colour.
 static string ACR_BaseCaption(string label, string detail, bool preview)
 {
  string symbol = "[CP] ";
  if (detail.Contains("3 RP")) symbol = "[CP III] ";
  else if (detail.Contains("2 RP")) symbol = "[CP II] ";
  else if (detail.Contains("1 RP")) symbol = "[CP I] ";
  if (detail.StartsWith("HQ (")) symbol = "[HQ] ";
  // The income suffix is already represented by the compact CP value tag.
  label.Replace(" [1]", "");
  label.Replace(" [2]", "");
  label.Replace(" [3]", "");
  if (detail.Contains("CONTESTED")) symbol = "! " + symbol;
  // COIN: the town's support tier as this side knows it (ACR_TownSupport).
  string tier = ACR_TownSupport.CaptionTier(detail);
  if (!tier.IsEmpty()) label += " (" + tier + ")";
  return symbol + label;
 }

 // One marker per base, remade when the count changes (the classic and the calm map both).
 protected void ACR_MatchBaseMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (m_ACR_BaseMarkers.Count() == m_ACR_Data.m_BaseNames.Count()) return;
  foreach (ACR_UnitMarker old : m_ACR_BaseMarkers) manager.ACR_RemoveMarker(old);
  m_ACR_BaseMarkers.Clear();
  for (int i = 0; i < m_ACR_Data.m_BaseNames.Count(); i++)
  {
   ACR_UnitMarker marker = new ACR_UnitMarker();
   marker.m_BaseIndex = i;
   manager.ACR_AddSimpleMarker(marker, m_ACR_BaseMarkers);
  }
 }

 protected void ACR_UpdateBaseMarkers(SCR_MapMarkerManagerComponent manager)
 {
  ACR_MatchBaseMarkers(manager);
  foreach (int i, ACR_UnitMarker marker : m_ACR_BaseMarkers)
  {
   marker.m_BaseIndex = i;
   marker.m_Label = m_ACR_Data.m_BaseNames[i];
   marker.m_Selected = m_ACR_SelectedBase == marker.m_Label;
   string detail = m_ACR_Data.m_BaseDetails[i];
   marker.m_BaseCaption = ACR_BaseCaption(marker.m_Label, detail, ACR_SetupClient.s_bActive);
   string hoverDetail = detail;
   hoverDetail.Replace(" | ", "\n");
   marker.m_BaseHover = marker.m_Label + "\n" + hoverDetail;
   if (marker.m_Selected) marker.m_BaseCaption = "> " + marker.m_BaseCaption + " <";
   vector pos = m_ACR_Data.m_BasePositions[i];
   marker.SetWorldPos(pos[0], pos[2]);
   marker.RefreshStyle();
   if (!marker.m_Selected && marker.GetRootWidget())
   {
    // RefreshStyle colours by unit state; bases use ownership instead.
    marker.ACR_SetColor(ACR_OrderDisplay.BaseColor(m_ACR_Data.m_BaseOwners[i], 1));
   }
  }
 }


 // ---------------------------------------------------------------- requisition
 ref array<string> m_ACR_ReqNames = {};
 ref array<int> m_ACR_ReqCosts = {};
 ref array<int> m_ACR_ReqRemaining = {};
 bool m_ACR_RequisitionOpen;
 int m_ACR_PendingEntry = -1;

 void ACR_SendRequisitions(array<string> names, array<int> costs, array<int> remaining)
 {
  Rpc(ACR_RequisitionsOwner, names, costs, remaining);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_RequisitionsOwner(array<string> names, array<int> costs, array<int> remaining)
 {
  m_ACR_ReqNames.Copy(names);
  m_ACR_ReqCosts.Copy(costs);
  m_ACR_ReqRemaining.Copy(remaining);
 }

 // ---------------------------------------------------------------- supports
 ref array<string> m_ACR_SupNames = {};
 ref array<int> m_ACR_SupCosts = {};
 protected ref array<int> m_ACR_SupCooldowns = {};
 bool m_ACR_SupportOpen;
 int m_ACR_PendingSupport = -1;

 // ---------------------------------------------------------------- after-action report
 bool m_ACR_ReportOpen;
 string m_ACR_ReportText = "No report yet";
 void ACR_SendReport(string text)
 {
  Rpc(ACR_ReportOwner, text);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_ReportOwner(string text)
 {
  m_ACR_ReportText = text;
 }

 void ACR_SendSupports(array<string> names, array<int> costs, array<int> cooldowns)
 {
  Rpc(ACR_SupportsOwner, names, costs, cooldowns);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_SupportsOwner(array<string> names, array<int> costs, array<int> cooldowns)
 {
  m_ACR_SupNames.Copy(names);
  m_ACR_SupCosts.Copy(costs);
  m_ACR_SupCooldowns.Copy(cooldowns);
  m_ACR_SupportsAt = System.GetTickCount();
 }
 protected int m_ACR_SupportsAt;
 void ACR_Cooldowns(notnull array<int> result)
 {
  result.Clear();
  int since = ACR_MapData.Since(m_ACR_SupportsAt);
  foreach (int left : m_ACR_SupCooldowns) result.Insert(Math.Max(0, left - since));
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_SupportServer(int index, vector target, RplId group)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (ACR_TooSoon("support", 300)) return;
  Rpc(ACR_SupportResultOwner, manager.CallSupport(this, index, target, group));
 }
 // The answer to a support call, also kept in the panel for a while (a refusal such as
 // "out of range" was only a hint, easy to miss on the map).
 string m_ACR_SupportResult;
 int m_ACR_SupportResultAt;
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_SupportResultOwner(string message)
 {
  m_ACR_SupportResult = message;
  m_ACR_SupportResultAt = System.GetTickCount();
  ACR_FeedbackOwner(message);
 }
 // Test hook: call a support without the UI.
 void ACR_TestSupport(int index, vector target)
 {
  Rpc(ACR_SupportServer, index, target, m_ACR_SelectedGroup);
 }

 // Base marker clicked: deploy the armed purchase there, otherwise select the base.
 bool ACR_DeployPending() { return m_ACR_PendingEntry >= 0; }

 // Deploy (or select) at the base whose ring contains this map point.
 bool ACR_ClickBaseAt(float x, float z)
 {
  if (m_ACR_PendingEntry >= 0 && m_ACR_MusterMode)
  {
   Rpc(ACR_MusterServer, m_ACR_PendingEntry, Vector(x, 0, z));
   m_ACR_PendingEntry = -1;
   return true;
  }
  int best = -1;
  float bestDistance = float.MAX;
  foreach (int i, vector pos : m_ACR_Data.m_BasePositions)
  {
   float d = vector.DistanceXZ(pos, Vector(x, 0, z));
   float reach = 60;
   if (i < m_ACR_Data.m_BaseRadii.Count()) reach = Math.Max(reach, m_ACR_Data.m_BaseRadii[i]);
   if (d < reach && d < bestDistance) { bestDistance = d; best = i; }
  }
  if (best < 0) return false;
  ACR_ClickBase(m_ACR_Data.m_BaseNames[best], Vector(x, 0, z), true);
  return true;
 }

 // The base whose name, icon or symbol is at screen x, y (native px), -1 none. Markers drawn later sit
 // over the bases' and take the click first (ACR_UnitMarker.PassesClickToBase).
 int ACR_BaseMarkerAt(float x, float y)
 {
  array<float> rects = {};
  array<int> owners = {};
  foreach (int i, ACR_UnitMarker marker : m_ACR_BaseMarkers)
  {
   if (!marker) continue;
   marker.ScreenRects(rects);
   while (owners.Count() * 4 < rects.Count()) owners.Insert(i);
  }
  int hit = ACR_MapMarkerHit.RectAt(rects, x, y);
  if (hit < 0 || hit >= owners.Count()) return -1;
  return owners[hit];
 }

 // Whether a click on this marker falls to the base under it (ACR_UnitMarker.PassesClickToBase).
 bool ACR_PassesClickToBase(ACR_UnitMarker marker)
 {
  if (!marker) return false;
  return marker.PassesClickToBase(m_ACR_GroupMarkers.Contains(marker) || m_ACR_Markers.Contains(marker));
 }

 // A click on a marker with no click of its own, over a base's name: the base's click (select it, list
 // it on the calm map, or deploy the armed purchase there). False when no base is under the cursor,
 // or a modifier makes it a ping or box select.
 bool ACR_ClickBaseUnderCursor()
 {
  if (ACR_EditorOpen() || ACR_ModifierHeld("ACR_MapAlt") || ACR_ModifierHeld("ACR_MapShift") || ACR_ModifierHeld("ACR_MapCtrl")) return false;
  int mx, my;
  WidgetManager.GetMousePos(mx, my);
  int b = ACR_BaseMarkerAt(mx, my);
  if (b < 0 || b >= m_ACR_BaseMarkers.Count() || !m_ACR_BaseMarkers[b]) return false;
  m_ACR_BaseMarkers[b].Select();
  return true;
 }

 // spot: the clicked map point, where a deployed unit appears.
 void ACR_ClickBase(string label, vector spot = "0 0 0", bool hasSpot = false)
 {
  if (m_ACR_PendingEntry >= 0)
  {
   Rpc(ACR_PurchaseServer, m_ACR_PendingEntry, label, spot[0], spot[2], hasSpot);
   m_ACR_PendingEntry = -1;
   return;
  }
  ACR_SelectBase(label);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_PurchaseServer(int entryIndex, string baseLabel, float spotX, float spotZ, bool hasSpot)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (ACR_TooSoon("purchase", 300)) return;
  // A spot that isn't a number inside the world (only a modified client sends one) is ignored:
  // the unit appears at a clear point in the ring, as without a click (review #15).
  vector spot = Vector(spotX, 0, spotZ);
  bool finite = spotX > -1000000 && spotX < 1000000 && spotZ > -1000000 && spotZ < 1000000;
  if (hasSpot && (!finite || !ACR_Manager.OnMap(spot)))
  {
   hasSpot = false;
   spot = vector.Zero;
  }
  Rpc(ACR_FeedbackOwner, manager.Purchase(this, entryIndex, baseLabel, spot, hasSpot));
 }

 // Test hook: buy entry at base without the UI.
 void ACR_TestPurchase(int entryIndex, string baseLabel, vector spot = "0 0 0", bool hasSpot = false)
 {
  Rpc(ACR_PurchaseServer, entryIndex, baseLabel, spot[0], spot[2], hasSpot);
 }
 int ACR_TestFindEntry(string namePart)
 {
  foreach (int i, string entryName : m_ACR_ReqNames)
   if (entryName.Contains(namePart)) return i;
  return -1;
 }

 // ================================================================ shared command (client)

 // ---------------------------------------------------------------- commanders
 void ACR_SendCommanders(array<int> ids, array<string> tags, array<string> actions, array<int> actionAges, array<RplId> selGroups, array<int> selOwners)
 {
  Rpc(ACR_CommandersOwner, ids, tags, actions, actionAges, selGroups, selOwners);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_CommandersOwner(array<int> ids, array<string> tags, array<string> actions, array<int> actionAges, array<RplId> selGroups, array<int> selOwners)
 {
  m_ACR_Data.m_CmdIds.Copy(ids);
  m_ACR_Data.m_CmdTags.Copy(tags);
  m_ACR_Data.m_CmdActions.Copy(actions);
  m_ACR_Data.m_CmdActionAges.Copy(actionAges);
  m_ACR_Data.m_iCmdAt = System.GetTickCount();
  m_ACR_Data.m_SelGroups.Copy(selGroups);
  m_ACR_Data.m_SelOwners.Copy(selOwners);
 }

 // The panel's commander roster: one line per other commander and what he last did.
 string ACR_RosterText()
 {
  if (m_ACR_Data.m_CmdIds.Count() < 2) return "";
  string text = "";
  int since = ACR_MapData.Since(m_ACR_Data.m_iCmdAt);
  foreach (int i, int id : m_ACR_Data.m_CmdIds)
  {
   string line = m_ACR_Data.m_CmdTags[i];
   if (id == GetPlayerId()) line += " (you)";
   string action = "";
   if (i < m_ACR_Data.m_CmdActions.Count()) action = m_ACR_Data.m_CmdActions[i];
   if (!action.IsEmpty())
   {
    int age = m_ACR_Data.m_CmdActionAges[i] + since;
    line += ": " + action + ", " + ACR_Ago(age);
   }
   if (!text.IsEmpty()) text += "\n";
   text += line;
  }
  return text;
 }

 protected static string ACR_Ago(int seconds)
 {
  if (seconds < 60) return seconds.ToString() + " s ago";
  int minutes = seconds / 60;
  return minutes.ToString() + " min ago";
 }

 // "Ordered by Popp, 40 s ago" for the squad panel ("" when nobody/AI).
 string ACR_OrderedByText(RplId groupId)
 {
  int age;
  int by = m_ACR_Data.OrderedBy(groupId, age);
  if (by <= 0 || age < 0) return "";
  string tag = m_ACR_Data.TagOf(by);
  if (by == GetPlayerId()) tag = "you";
  if (tag.IsEmpty()) return "";
  return string.Format("Ordered by %1, %2", tag, ACR_Ago(age));
 }

 // Another commander gave one of these squads an order in the last 30 s: the
 // first right-click warns, a second one within 5 s goes ahead.
 protected int m_ACR_OverrideAsked = -100000;
 protected string m_ACR_OverrideKey;
 protected bool ACR_ConfirmOverride(array<RplId> squads)
 {
  string names = "";
  string key = "";
  foreach (RplId id : squads)
  {
   string tag = m_ACR_Data.RecentOtherOrder(id, 30);
   if (tag.IsEmpty()) continue;
   int index = m_ACR_Data.m_GroupIds.Find(id);
   string squadName = "a squad";
   if (index >= 0 && index < m_ACR_Data.m_GroupNames.Count()) squadName = m_ACR_Data.m_GroupNames[index];
   int age;
   m_ACR_Data.OrderedBy(id, age);
   if (!names.IsEmpty()) names += ", ";
   names += string.Format("%1 (%2, %3 s ago)", squadName, tag, age);
   key += id.AsString() + ";";
  }
  if (names.IsEmpty()) return true;
  int now = System.GetTickCount();
  if (key == m_ACR_OverrideKey && now - m_ACR_OverrideAsked < 5000)
  {
   m_ACR_OverrideKey = "";
   return true;
  }
  m_ACR_OverrideKey = key;
  m_ACR_OverrideAsked = now;
  SCR_HintManagerComponent.ShowCustomHint("Just ordered by another commander: " + names + ". Right-click again to override.", "AC: Actual", 5, true);
  return false;
 }

 // Our selection, for the other commanders' maps (sent when it changes).
 protected string m_ACR_SelectionSent = "-";
 protected int m_ACR_SelectionSentAt;
 protected void ACR_SendSelectionIfChanged()
 {
  if (!m_ACR_IsCommander) return;
  string key = "";
  foreach (RplId id : m_ACR_Data.m_Selected) key += id.AsString() + ";";
  if (key == m_ACR_SelectionSent) return;
  int now = System.GetTickCount();
  if (now - m_ACR_SelectionSentAt < 250) return;
  m_ACR_SelectionSent = key;
  m_ACR_SelectionSentAt = now;
  Rpc(ACR_SelectionServer, m_ACR_Data.m_Selected);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_SelectionServer(array<RplId> groups)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.IsCommander(GetPlayerId())) return;
  manager.Command().SetSelection(GetPlayerId(), groups);
 }

 // ---------------------------------------------------------------- pings
 void ACR_SendPings(array<int> ids, array<int> kinds, array<vector> positions, array<int> owners, array<string> tags, array<int> ages, array<string> answers)
 {
  Rpc(ACR_PingsOwner, ids, kinds, positions, owners, tags, ages, answers);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_PingsOwner(array<int> ids, array<int> kinds, array<vector> positions, array<int> owners, array<string> tags, array<int> ages, array<string> answers)
 {
  m_ACR_Data.m_PingIds.Copy(ids);
  m_ACR_Data.m_PingKinds.Copy(kinds);
  m_ACR_Data.m_PingPositions.Copy(positions);
  m_ACR_Data.m_PingOwners.Copy(owners);
  m_ACR_Data.m_PingTags.Copy(tags);
  m_ACR_Data.m_PingAges.Copy(ages);
  m_ACR_Data.m_PingAnswers.Copy(answers);
  m_ACR_Data.m_iPingsAt = System.GetTickCount();
  m_ACR_Data.m_iMyId = GetPlayerId();
  ACR_PingDisplay.OnPings(m_ACR_Data, GetControlledEntity());
 }

 void ACR_RequestPing(int kind, vector pos, bool fromWorld)
 {
  Rpc(ACR_PingServer, kind, pos, fromWorld);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_PingServer(int kind, vector pos, bool fromWorld)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  string result = manager.Pings().Place(GetPlayerId(), kind, pos, fromWorld, manager.Command().TagOf(GetPlayerId()));
  if (!result.IsEmpty()) Rpc(ACR_FeedbackOwner, result);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_PingRemoveServer(int pingId)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  string result = manager.Pings().Remove(GetPlayerId(), pingId);
  if (!result.IsEmpty()) Rpc(ACR_FeedbackOwner, result);
 }

 bool m_ACR_HasSupportTarget;
 vector m_ACR_SupportTarget;
 string m_ACR_SupportTargetFor;

 // A ping's label was clicked: Alt+click your own to clear it; as a commander,
 // a Support ping opens the support list aimed at it.
 void ACR_ClickPing(int pingId)
 {
  int i = m_ACR_Data.m_PingIds.Find(pingId);
  if (i < 0) return;
  if (ACR_ModifierHeld("ACR_MapAlt"))
  {
   if (m_ACR_Data.m_PingOwners[i] == GetPlayerId()) Rpc(ACR_PingRemoveServer, pingId);
   return;
  }
  if (m_ACR_Data.m_PingKinds[i] == ACR_EPing.SUPPORT && m_ACR_IsCommander && m_ACR_Data.m_PingAnswers[i].IsEmpty())
  {
   m_ACR_HasSupportTarget = true;
   m_ACR_SupportTarget = m_ACR_Data.m_PingPositions[i];
   m_ACR_SupportTargetFor = m_ACR_Data.m_PingTags[i];
   m_ACR_SupportOpen = true;
   m_ACR_RequisitionOpen = false;
   m_ACR_PendingSupport = -1;
   m_ACR_PendingEntry = -1;
   return;
  }
  if (m_ACR_Data.m_PingOwners[i] == GetPlayerId())
   SCR_HintManagerComponent.ShowCustomHint("Alt+click your ping to clear it.", "AC: Actual", 3, true);
 }

 // ---- the four-way picker on the map (Alt+right-click; click or press 1-4)
 protected bool m_ACR_PickerOpen;
 protected vector m_ACR_PickerPos;
 protected int m_ACR_PickerOpenedAt;

 protected void ACR_OpenPicker(SCR_MapEntity mapEntity, vector pos)
 {
  if (!m_ACR_Panel) return;
  int sx, sy;
  mapEntity.WorldToScreen(pos[0], pos[2], sx, sy, true);
  m_ACR_PickerPos = pos;
  m_ACR_PickerOpen = true;
  m_ACR_PickerOpenedAt = System.GetTickCount();
  m_ACR_Panel.ShowPicker(true, sx, sy);
 }

 protected void ACR_ClosePicker()
 {
  m_ACR_PickerOpen = false;
  if (m_ACR_Panel) m_ACR_Panel.ShowPicker(false, 0, 0);
 }

 void ACR_PickPing(int kind)
 {
  if (!m_ACR_PickerOpen) return;
  ACR_RequestPing(kind, m_ACR_PickerPos, false);
  ACR_ClosePicker();
 }

 // Each map frame: number keys pick while the picker is up; it closes by itself after 8 s.
 protected void ACR_UpdatePicker()
 {
  if (!m_ACR_PickerOpen) return;
  if (System.GetTickCount() - m_ACR_PickerOpenedAt > 8000) { ACR_ClosePicker(); return; }
  InputManager input = GetGame().GetInputManager();
  input.ActivateContext("ACR_PickerContext");
  if (input.GetActionTriggered("ACR_Ping1")) ACR_PickPing(ACR_EPing.LOOK);
  else if (input.GetActionTriggered("ACR_Ping2")) ACR_PickPing(ACR_EPing.ATTACK);
  else if (input.GetActionTriggered("ACR_Ping3")) ACR_PickPing(ACR_EPing.DEFEND);
  else if (input.GetActionTriggered("ACR_Ping4")) ACR_PickPing(ACR_EPing.SUPPORT);
 }

 // ---- pings from the 3D view (the "Ping" key, unbound by default)
 // Tap: Look here, where you aim. Hold: the kind cycles every 0.6 s on screen
 // (Look, Attack, Defend, Support); let go to place it.
 static const int PING_TAP_MS = 300;
 static const int PING_CYCLE_MS = 600;
 protected int m_ACR_PingDownAt = -1;
 protected int m_ACR_PingHoldKind;

 protected bool m_ACR_PingOnMap;      // the current press started with the map open
 protected bool m_ACR_PingPickerShown; // map hold: the picker is already up

 // A press that can't finish where it started (camera opened, body lost): forget it, so letting
 // go later doesn't place a ping nobody aimed.
 void ACR_CancelPingPress()
 {
  m_ACR_PingDownAt = -1;
  m_ACR_PingPickerShown = false;
 }

 void ACR_UpdateWorldPing(bool mapOpen)
 {
  InputManager input = GetGame().GetInputManager();
  bool held = input && input.GetActionValue("ACR_Ping") > 0.5;
  int now = System.GetTickCount();
  if (held)
  {
   if (m_ACR_PingDownAt < 0)
   {
    m_ACR_PingDownAt = now;
    m_ACR_PingOnMap = mapOpen;
    m_ACR_PingPickerShown = false;
   }
   int down = now - m_ACR_PingDownAt;
   if (down < PING_TAP_MS) return;
   if (m_ACR_PingOnMap)
   {
    // On the map: holding opens the four-way picker at the cursor.
    if (!m_ACR_PingPickerShown && mapOpen && !ACR_EditorOpen() && !ACR_PointerOverOwnUI())
    {
     SCR_MapEntity holdMap = SCR_MapEntity.GetMapInstance();
     float hx, hz;
     if (holdMap)
     {
      holdMap.GetMapCursorWorldPosition(hx, hz);
      ACR_OpenPicker(holdMap, Vector(hx, 0, hz));
     }
     m_ACR_PingPickerShown = true;
    }
    return;
   }
   m_ACR_PingHoldKind = ((down - PING_TAP_MS) / PING_CYCLE_MS) % 4;
   ACR_PingDisplay.ShowHoldHint(m_ACR_PingHoldKind);
   return;
  }
  if (m_ACR_PingDownAt < 0) return;
  int heldFor = now - m_ACR_PingDownAt;
  m_ACR_PingDownAt = -1;
  ACR_PingDisplay.ShowHoldHint(-1);
  if (m_ACR_PingOnMap || mapOpen)
  {
   // Map tap: Look here at the cursor (a hold already opened the picker).
   if (!m_ACR_PingOnMap || !mapOpen || m_ACR_PingPickerShown || ACR_EditorOpen() || ACR_PointerOverOwnUI()) return; // not at a panel
   SCR_MapEntity tapMap = SCR_MapEntity.GetMapInstance();
   if (!tapMap) return;
   float tx, tz;
   tapMap.GetMapCursorWorldPosition(tx, tz);
   ACR_RequestPing(ACR_EPing.LOOK, Vector(tx, 0, tz), false);
   return;
  }
  int kind = ACR_EPing.LOOK;
  if (heldFor >= PING_TAP_MS) kind = m_ACR_PingHoldKind;
  vector aim;
  if (!ACR_AimPoint(aim))
  {
   SCR_HintManagerComponent.ShowCustomHint("Aim at the ground or something within 1.5 km to ping it.", "AC: Actual", 3, true);
   return;
  }
  ACR_RequestPing(kind, aim, true);
 }

 // Where the view centre hits the world (up to the ping reach).
 protected bool ACR_AimPoint(out vector point)
 {
  vector camera[4];
  GetGame().GetWorld().GetCurrentCamera(camera);
  TraceParam trace = new TraceParam();
  trace.Start = camera[3];
  trace.End = camera[3] + camera[2] * ACR_Pings.WORLD_REACH;
  trace.Exclude = GetControlledEntity();
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = GetGame().GetWorld().TraceMove(trace, null);
  if (fraction >= 1) return false;
  point = trace.Start + (trace.End - trace.Start) * fraction;
  return true;
 }

 // Test hooks.
 ACR_MapData ACR_TestData() { return m_ACR_Data; }
 int ACR_TestHudPings() { return ACR_PingDisplay.HudShown(); }
 string ACR_TestOrderedBy(RplId groupId) { return ACR_OrderedByText(groupId); }
 string ACR_TestRoster() { return ACR_RosterText(); }
 void ACR_TestSelect(array<RplId> groups) { ACR_SetSelection(groups); ACR_SendSelectionIfChanged(); }
 bool ACR_TestConfirmOverride(array<RplId> squads) { return ACR_ConfirmOverride(squads); }
 void ACR_TestClickPing(int pingId) { ACR_ClickPing(pingId); }
 bool ACR_TestHasSupportTarget() { return m_ACR_HasSupportTarget; }
 bool ACR_TestPicker(vector pos, int kind)
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen() || !m_ACR_Panel) return false;
  ACR_OpenPicker(mapEntity, pos);
  bool shown = m_ACR_Panel.PickerShown();
  ACR_OnPanelAction(ACR_EPanelAction.PING_LOOK + kind);
  return shown && !m_ACR_Panel.PickerShown();
 }
 bool ACR_TestMapOpen() { return m_ACR_Panel != null && m_ACR_Display.GetCanvas() != null; }

 protected void ACR_UpdateSharedMarkers(SCR_MapMarkerManagerComponent manager)
 {
  m_ACR_Display.UpdateTags(manager, m_ACR_Data);
  ACR_PingDisplay.UpdateMarkers(manager, m_ACR_Data);
 }
}
