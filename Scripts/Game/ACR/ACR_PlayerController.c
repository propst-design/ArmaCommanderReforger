// Map markers for soldiers and group leaders. Clicking one requests possession.
class ACR_UnitMarker : SCR_MapMarkerBase
{
 RplId m_UnitId;
 string m_Label;
 string m_BaseCaption; // display only; requests retain the exact label
 string m_BaseHover;
 int m_State;          // 0 AI, 1 player-controlled, 2 dead
 bool m_Original;      // the player's command unit
 bool m_bCommandUnit;  // persistent marker for a fellow commander, not a possession target
 bool m_Selected;
 RplId m_GroupId;      // squad this marker belongs to
 int m_BaseIndex = -1; // >= 0: this marker is a base
 bool m_bContact;      // spotted enemy: clicks go through to the map
 int m_iPingId = -1;   // >= 0: this marker is a ping's label
 int m_iSymbolKey;     // changes when m_GroupSymbol is set differently
 protected Widget m_wPip; // ground-level reticle pip for spotted contacts
 protected string m_sStyled = "-"; // what RefreshStyle last applied
 ref ACR_MarkerClick m_Click;
 ref SCR_MilitarySymbol m_GroupSymbol;
 override void OnCreateMarker(bool skipProfanityFilter = false)
 {
  // Map-open and local insertion can both create this marker. Remove the old
  // widget before replacing its reference, or its caption stays at the origin.
  if (m_wRoot) m_wRoot.RemoveFromHierarchy();
  m_wRoot = null;
  m_MarkerWidgetComp = null;
  // Always update: units move into view without the map being panned.
  m_bTestVisibleFrame = false;
  m_sStyled = "-";
  super.OnCreateMarker(true);
  if (!m_wRoot) return;
  m_Click = new ACR_MarkerClick();
  m_Click.m_Marker = this;
  m_wRoot.AddHandler(m_Click);
  OnUpdate();
  RefreshStyle();
 }
 override void SetWorldPos(int posX, int posY)
 {
  super.SetWorldPos(posX, posY);
  // Position immediately; persistent captions must never wait at (0, 0).
  if (m_wRoot && m_MapEntity) OnUpdate();
 }
 void ACR_SetColor(Color color)
 {
  if (m_MarkerWidgetComp) m_MarkerWidgetComp.SetColor(color);
 }
 // Called every map frame: the widget is only touched when something changed.
 void RefreshStyle()
 {
  if (!m_MarkerWidgetComp) return;
  string shown = m_Label;
  if (m_BaseIndex >= 0) shown = m_BaseCaption;
  string key = string.Format("%1|%2|%3|%4|%5|%6", shown, m_State, m_Original, m_Selected, m_iSymbolKey, m_BaseHover);
  if (key == m_sStyled) return;
  m_sStyled = key;
  if (m_BaseIndex >= 0) m_MarkerWidgetComp.SetEventListening(true);
  m_MarkerWidgetComp.SetText(shown, true);
  m_MarkerWidgetComp.SetTextVisible(true);
  Color color = new Color(0.1, 0.65, 1, 1);
  if (m_State == 1) color = new Color(1, 0.75, 0.1, 1);
  if (m_State == 2) color = new Color(0.5, 0.5, 0.5, 1);
  if (m_State == 3) color = new Color(1, 0.25, 0.2, 1);   // spotted enemy
  if (m_State == 4) color = new Color(0.65, 0.8, 0.95, 1); // our empty vehicle
  if (m_State == 5) color = new Color(0.92, 0.92, 0.92, 1); // another commander's tag on an order
  if (m_State == 6) color = new Color(1, 1, 1, 1);          // ping label (coloured by kind afterwards)
  if (m_Original) color = new Color(0.2, 1, 0.3, 1);
  if (m_Selected) color = new Color(1, 1, 1, 1);
  m_MarkerWidgetComp.SetColor(color);
  if (m_GroupSymbol) m_MarkerWidgetComp.UpdateMilitarySymbol(m_GroupSymbol);
  if (m_bContact && m_wRoot && !m_wPip)
  {
   WorkspaceWidget ws = GetGame().GetWorkspace();
   if (ws)
   {
    m_wPip = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, new Color(1, 0.22, 0.18, 0.9), 99, m_wRoot);
    FrameSlot.SetAnchorMin(m_wPip, 0.5, 0.5);
    FrameSlot.SetAnchorMax(m_wPip, 0.5, 0.5);
    FrameSlot.SetAlignment(m_wPip, 0.5, 0.5);
    FrameSlot.SetSize(m_wPip, 6, 6);
   }
  }
  if (m_wPip)
  {
   float pipAlpha = 0.95;
   if (m_State == 3 && m_Label.Contains("?")) pipAlpha = 0.55;
   m_wPip.SetColor(new Color(1, 0.22, 0.18, pipAlpha));
  }
 }
 // Left-click: select this marker's squad (Arma Commander style). The command
 // unit marker instead returns you to your command unit.
 void Select()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc) return;
  pc.ACR_MarkUiClick();
  if (m_iPingId >= 0)
  {
   pc.ACR_ClickPing(m_iPingId);
   return;
  }
  // Deploying: a click on a squad standing on a base counts as a click on the base.
  if (m_BaseIndex < 0 && pc.ACR_DeployPending())
  {
   int pos[2];
   GetWorldPos(pos);
   pc.ACR_ClickBaseAt(pos[0], pos[1]);
   return;
  }
  // Deploying on a base marker: the unit appears where the cursor is.
  if (m_BaseIndex >= 0 && pc.ACR_DeployPending())
  {
   SCR_MapEntity markerMap = SCR_MapEntity.GetMapInstance();
   float cx, cz;
   if (markerMap)
   {
    markerMap.GetMapCursorWorldPosition(cx, cz);
    pc.ACR_ClickBase(m_Label, Vector(cx, 0, cz), true);
    return;
   }
  }
  if (m_Original) pc.ACR_Request(ACR_ERequest.RETURN);
  else if (m_BaseIndex >= 0) pc.ACR_ClickBase(m_Label);
  else pc.ACR_SelectGroup(pc.ACR_CycleOverlap(this));
 }
}
class ACR_MarkerClick : ScriptedWidgetEventHandler
{
 ACR_UnitMarker m_Marker;
 override bool OnMouseButtonDown(Widget w, int x, int y, int button)
 {
  if (button != 0 || !m_Marker || m_Marker.m_bContact) return false;
  m_Marker.Select();
  return true;
 }
}

// Ctrl + drag on the command map (see ACR_BoxDown / ACR_BoxUp).
class ACR_BoxHandler : ScriptedWidgetEventHandler
{
 SCR_PlayerController m_Owner;
 override bool OnMouseButtonDown(Widget w, int x, int y, int button)
 {
  if (button != 0 || !m_Owner) return false;
  m_Owner.ACR_BoxDown(x, y);
  return true;
 }
 override bool OnMouseButtonUp(Widget w, int x, int y, int button)
 {
  if (button != 0 || !m_Owner) return false;
  m_Owner.ACR_BoxUp(x, y);
  return true;
 }
}

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
 protected float m_ACR_LastCommand = -1000;
 protected float m_ACR_CommandWindow = -1000;
 protected int m_ACR_CommandsInWindow;
 void ACR_Request(int command, RplId targetId = RplId.Invalid(), vector point = vector.Zero)
 {
  if (!ACR_Manager.GetInstance()) return;
  if (command == ACR_ERequest.POSSESS || command == ACR_ERequest.RETURN) ACR_MapHandoff.Release(this, GetControlledEntity(), false);
  Rpc(ACR_CommandServer, command, targetId, point);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_CommandServer(int command, RplId targetId, vector point)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  // Flood guard: up to 24 orders a second (one click can order a dozen squads).
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_CommandWindow > 1000)
  {
   m_ACR_CommandWindow = now;
   m_ACR_CommandsInWindow = 0;
  }
  m_ACR_CommandsInWindow++;
  if (m_ACR_CommandsInWindow > 24) return;
  string message = manager.HandleRequest(this, command, targetId, point);
  if (command == ACR_ERequest.REINFORCE) Rpc(ACR_ReinforceFeedbackOwner, message);
  else Rpc(ACR_FeedbackOwner, message);
 }

 protected string m_ACR_ReinforceFeedback;
 protected float m_ACR_ReinforceFeedbackAt = -10000;
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
   m_DeathBlackoutRoot.RemoveFromHierarchy();
   m_DeathBlackoutRoot = null;
   m_DeathBlackoutImage = null;
   m_DeathBlackoutText = null;
  }
  ACR_MapHandoff.SetMenuControls(GetControlledEntity(), false);
 }

 protected void ACR_EnsureDeathOverlay()
 {
  if (m_DeathBlackoutRoot && m_DeathBlackoutImage && m_DeathBlackoutText) return;
  if (m_DeathBlackoutRoot) m_DeathBlackoutRoot.RemoveFromHierarchy();
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
 void ACR_RequestSetup(string key)
 {
  Rpc(ACR_SetupServer, key);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_SetupServer(string key)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastCommand < 150) return;
  m_ACR_LastCommand = now;
  string result = ACR_Setup.Handle(GetPlayerId(), key);
  PrintFormat("[ACR] SETUP player %1: %2", GetPlayerId(), result);
  Rpc(ACR_FeedbackOwner, result);
 }
 // Layout editor: move a base of the generated layout (the server checks the spot).
 void ACR_RequestSetupMove(string name, float x, float z)
 {
  Rpc(ACR_SetupMoveServer, name, x, z);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_SetupMoveServer(string name, float x, float z)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastCommand < 150) return;
  m_ACR_LastCommand = now;
  string result = ACR_Setup.HandleMove(GetPlayerId(), name, x, z);
  PrintFormat("[ACR] SETUP player %1: %2", GetPlayerId(), result);
  Rpc(ACR_FeedbackOwner, result);
 }

 // ---------------------------------------------------------------- debug menu (host/admins)
 protected bool m_ACR_DebugAllowed;
 protected bool m_ACR_DebugOpen;
 protected string m_ACR_DebugStatus;
 void ACR_SendDebug(bool allowed, string status)
 {
  Rpc(ACR_DebugOwner, allowed, status);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_DebugOwner(bool allowed, string status)
 {
  m_ACR_DebugAllowed = allowed;
  m_ACR_DebugStatus = status;
 }
 void ACR_RequestDebug(int action)
 {
  Rpc(ACR_DebugServer, action, m_ACR_SelectedBase);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_DebugServer(int action, string baseName)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastCommand < 300) return;
  m_ACR_LastCommand = now;
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
   if (m_ACR_SelectedGroup.IsValid()) target = manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(m_ACR_SelectedGroup)));
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
   if (m_ACR_SelectedGroup.IsValid()) target = manager.FindSquadByGroup(SCR_AIGroup.Cast(SCR_EntityHelper.RplIdToEntity(m_ACR_SelectedGroup)));
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
 protected bool m_ACR_IsCommander;
 protected bool m_ACR_SharedCommand;
 protected string m_ACR_CommandText;
 protected int m_ACR_RP;
 protected int m_ACR_Income;
 protected int m_ACR_IncomeSeconds;
 protected int m_ACR_IncomeAt;   // tick count when m_ACR_IncomeSeconds arrived
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
 protected bool m_ACR_MapWasOpen;

 void ACR_SendRoster(array<RplId> ids, array<vector> positions, array<int> states, array<string> labels, array<RplId> groupIds, vector original)
 {
  Rpc(ACR_RosterOwner, ids, positions, states, labels, groupIds, original);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_RosterOwner(array<RplId> ids, array<vector> positions, array<int> states, array<string> labels, array<RplId> groupIds, vector original)
 {
  m_ACR_UnitGroups.Copy(groupIds);
  m_ACR_Ids.Copy(ids);
  m_ACR_Positions.Copy(positions);
  m_ACR_States.Copy(states);
  m_ACR_Labels.Copy(labels);
  m_ACR_OriginalPos = original;
 }

 // ---------------------------------------------------------------- group leaders
 protected ref array<int> m_ACR_GroupSymbols = {};
 protected ref array<int> m_ACR_LeaderStates = {};
 protected ref array<string> m_ACR_GroupDetails = {};
 protected ref array<ref ACR_UnitMarker> m_ACR_GroupMarkers = {};

 void ACR_SendGroupMarkers(array<RplId> leaders, array<vector> positions, array<string> names, array<int> symbols, array<int> states, array<int> commandable, array<RplId> groupIds, array<string> details)
 {
  Rpc(ACR_GroupMarkersOwner, leaders, positions, names, symbols, states, commandable, groupIds, details);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_GroupMarkersOwner(array<RplId> leaders, array<vector> positions, array<string> names, array<int> symbols, array<int> states, array<int> commandable, array<RplId> groupIds, array<string> details)
 {
  m_ACR_Data.m_GroupIds.Copy(groupIds);
  m_ACR_GroupDetails.Copy(details);
  m_ACR_Data.m_LeaderIds.Copy(leaders);
  m_ACR_Data.m_LeaderPositions.Copy(positions);
  m_ACR_Data.m_GroupNames.Copy(names);
  m_ACR_GroupSymbols.Copy(symbols);
  m_ACR_LeaderStates.Copy(states);
  m_ACR_Data.m_Commandable.Copy(commandable);
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
   foreach (ACR_UnitMarker contactMarker : m_ACR_ContactMarkers) manager.ACR_RemoveMarker(contactMarker);
   foreach (ACR_UnitMarker vehicleMarker : m_ACR_VehicleMarkers) manager.ACR_RemoveMarker(vehicleMarker);
  }
  m_ACR_Display.ClearMarkers(manager);
  m_ACR_VehicleMarkers.Clear();
  m_ACR_ContactMarkers.Clear();
  m_ACR_ZoneMarkers.Clear();
  m_ACR_BaseMarkers.Clear();
  m_ACR_Markers.Clear();
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
    SCR_MapEntity.GetOnSelection().Remove(ACR_OnSetupMapClick);
    m_ACR_SelectedBase = "";
    m_ACR_SetupPicked = "";
   }
   m_ACR_SetupMapOpen = false;
   return;
  }
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (!manager) return;
  if (!m_ACR_SetupMapOpen)
  {
   Print("[ACR] SETUP preview map opened");
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
  ACR_UpdateBaseMarkers(manager);
 }
 protected string m_ACR_SetupPicked;

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
  if (!ACR_SetupClient.s_bAllowed || !ACR_SetupClient.s_sRows.Contains("Show and edit")) return;
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return;
  float x, z;
  mapEntity.GetMapCursorWorldPosition(x, z);
  if (m_ACR_SelectedBase == "")
  {
   int best = -1;
   float bestDistance = float.MAX;
   foreach (int i, vector pos : m_ACR_Data.m_BasePositions)
   {
    float d = vector.DistanceXZ(pos, Vector(x, 0, z));
    float reach = 60;
    if (i < m_ACR_Data.m_BaseRadii.Count()) reach = Math.Max(reach, m_ACR_Data.m_BaseRadii[i]);
    if (d < reach && d < bestDistance) { bestDistance = d; best = i; }
   }
   if (best >= 0) m_ACR_SelectedBase = m_ACR_Data.m_BaseNames[best];
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
   if (m_ACR_Panel && m_ACR_Panel.Contains(WidgetManager.GetWidgetUnderCursor())) wheel = 0;
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
  // Leaders are drawn by their group marker, so they get no plain soldier marker.
  array<int> shown = {};
  foreach (int rosterIndex, RplId unitId : m_ACR_Ids)
   if (!m_ACR_Data.m_LeaderIds.Contains(unitId)) shown.Insert(rosterIndex);
  int count = shown.Count();
  if (m_ACR_Markers.Count() != count + 1)
  {
   foreach (ACR_UnitMarker old : m_ACR_Markers) manager.ACR_RemoveMarker(old);
   m_ACR_Markers.Clear();
   for (int i = 0; i <= count; i++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    marker.SetType(SCR_EMapMarkerType.SIMPLE);
    marker.SetMarkerConfigID(1);
    marker.SetCanBeRemovedByOwner(false);
    marker.m_Original = i == count;
    m_ACR_Markers.Insert(marker);
    manager.InsertStaticMarker(marker, true);
    // Not "owned" by the player, so vanilla doesn't open the marker edit dialog on click.
    marker.SetMarkerOwnerID(-1);
   }
  }
  for (int i = 0; i < count; i++)
  {
   int r = shown[i];
   ACR_UnitMarker marker = m_ACR_Markers[i];
   marker.m_UnitId = m_ACR_Ids[r];
   marker.m_State = m_ACR_States[r];
   marker.m_Label = m_ACR_Labels[r];
   marker.m_GroupId = m_ACR_UnitGroups[r];
   marker.m_Selected = ACR_IsSelected(marker.m_GroupId);
   if (marker.m_State == 1) marker.m_Label += " [PLAYER]";
   vector pos = m_ACR_Positions[r];
   IEntity streamed = SCR_EntityHelper.RplIdToEntity(marker.m_UnitId);
   if (streamed) pos = streamed.GetOrigin();
   marker.SetWorldPos(pos[0], pos[2]);
   marker.RefreshStyle();
  }
  ACR_UnitMarker original = m_ACR_Markers[count];
  original.m_Label = "COMMAND UNIT [F8]";
  original.SetWorldPos(m_ACR_OriginalPos[0], m_ACR_OriginalPos[2]);
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
 void ACR_UpdateGroupMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (m_ACR_GroupMarkers.Count() != m_ACR_Data.m_LeaderIds.Count())
  {
   foreach (ACR_UnitMarker old : m_ACR_GroupMarkers) manager.ACR_RemoveMarker(old);
   m_ACR_GroupMarkers.Clear();
   for (int i = 0; i < m_ACR_Data.m_LeaderIds.Count(); i++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
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
  }
  for (int i = 0; i < m_ACR_GroupMarkers.Count(); i++)
  {
   ACR_UnitMarker marker = m_ACR_GroupMarkers[i];
   marker.m_UnitId = m_ACR_Data.m_LeaderIds[i];
   marker.m_Label = m_ACR_Data.m_GroupNames[i];
   marker.m_GroupId = m_ACR_Data.m_GroupIds[i];
   marker.m_bCommandUnit = (m_ACR_GroupSymbols[i * 4 + 3] & 32) != 0;
   marker.m_Selected = ACR_IsSelected(marker.m_GroupId);
   marker.m_State = m_ACR_LeaderStates[i];
   if (marker.m_State == 1) marker.m_Label += " [PLAYER LEADER]";
   else marker.m_Label += " [AI LEADER]";
   vector pos = m_ACR_Data.m_LeaderPositions[i];
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
 protected RplId m_ACR_SelectedGroup = RplId.Invalid();
 protected ACR_ECursorMode m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 protected ref ACR_CommandPanel m_ACR_Panel;
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
   GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
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
 protected void ACR_RequestSelected(int command)
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

 protected void ACR_ClosePanel()
 {
  SCR_MapEntity.GetOnSelection().Remove(ACR_OnMapClick);
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
  if (m_ACR_Panel && m_ACR_Panel.Contains(WidgetManager.GetWidgetUnderCursor())) return;
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

 protected void ACR_UpdatePanel()
 {
  if (!m_ACR_Panel) return;
  string reinforceFeedback = "";
  if (GetGame().GetWorld().GetWorldTime() - m_ACR_ReinforceFeedbackAt < 12000) reinforceFeedback = m_ACR_ReinforceFeedback;
  m_ACR_Panel.SetReinforceFeedback(reinforceFeedback);
  m_ACR_Panel.SetDebug(m_ACR_DebugAllowed, m_ACR_DebugOpen, m_ACR_DebugStatus, m_ACR_SelectedBase);
  bool canTake = !m_ACR_IsCommander && (m_ACR_SharedCommand || m_ACR_CommandText == "Command: AI");
  m_ACR_Panel.SetCommand(m_ACR_CommandText, m_ACR_IsCommander, canTake);
  int incomeIn = Math.Max(0, m_ACR_IncomeSeconds - ACR_MapData.Since(m_ACR_IncomeAt));
  m_ACR_Panel.SetEconomy(string.Format("RP %1   +%2 in %3s", m_ACR_RP, m_ACR_Income, incomeIn));
  m_ACR_Panel.SetRoster(ACR_RosterText());
  if (!m_ACR_IsCommander) m_ACR_PendingEntry = -1;
  if (!m_ACR_IsCommander) m_ACR_PendingSupport = -1;
  m_ACR_Panel.SetRequisitions(m_ACR_IsCommander, m_ACR_RequisitionOpen, m_ACR_ReqNames, m_ACR_ReqCosts, m_ACR_ReqRemaining, m_ACR_RP, m_ACR_PendingEntry);
  array<int> cooldowns = {};
  ACR_Cooldowns(cooldowns);
  m_ACR_Panel.SetSupports(m_ACR_IsCommander, m_ACR_SupportOpen, m_ACR_SupNames, m_ACR_SupCosts, cooldowns, m_ACR_RP, m_ACR_PendingSupport);
  m_ACR_Panel.SetReport(m_ACR_ReportOpen, m_ACR_ReportText);
  if (m_ACR_HasSupportTarget && m_ACR_SupportOpen)
  {
   m_ACR_Panel.SetBase("Support for " + m_ACR_SupportTargetFor, "Pick a support above: it fires on the ping. Click Supports to cancel.");
   return;
  }
  if (m_ACR_PendingSupport >= 0 && m_ACR_PendingSupport < m_ACR_SupNames.Count())
  {
   string supportHint = "Right-click the target on the map. Click the entry again to cancel.";
   string supportName = m_ACR_SupNames[m_ACR_PendingSupport];
   supportName.ToLower();
   if (supportName.Contains("air lift")) supportHint = "Select the squad to fly, then right-click where it should land. Click the entry again to cancel.";
   m_ACR_Panel.SetBase(m_ACR_SupNames[m_ACR_PendingSupport], supportHint);
   return;
  }
  if (!m_ACR_SupportResult.IsEmpty() && System.GetTickCount() - m_ACR_SupportResultAt < 10000)
  {
   m_ACR_Panel.SetBase("Support call", m_ACR_SupportResult);
   return;
  }
  if (m_ACR_PendingEntry >= 0 && m_ACR_PendingEntry < m_ACR_ReqNames.Count())
  {
   m_ACR_Panel.SetBase(m_ACR_ReqNames[m_ACR_PendingEntry], "Click one of your bases to deploy. Right-click to cancel.");
   return;
  }
  if (!m_ACR_SelectedBase.IsEmpty())
  {
   int baseIndex = m_ACR_Data.m_BaseNames.Find(m_ACR_SelectedBase);
   if (baseIndex >= 0)
   {
    m_ACR_Panel.SetBase(m_ACR_Data.m_BaseNames[baseIndex], m_ACR_Data.m_BaseDetails[baseIndex]);
    int stride = 3 + ACR_FobSlots.UPGRADE_COUNT;
    int supplyOffset = baseIndex * stride;
    if (m_ACR_Data.m_BaseSupply.Count() >= supplyOffset + stride)
    {
     int stock = m_ACR_Data.m_BaseSupply[supplyOffset];
     int capacity = m_ACR_Data.m_BaseSupply[supplyOffset + 1];
     int delivery = m_ACR_Data.m_BaseSupply[supplyOffset + 2];
     // delivery -1: the FOB's HQ is destroyed and it is offline until rebuilt.
     bool ready = m_ACR_IsCommander && m_ACR_Data.m_BaseContested[baseIndex] == 0 && delivery >= 0;
     string factionKey = "";
     Faction myFaction = SCR_FactionManager.SGetPlayerFaction(GetPlayerId());
     if (myFaction) factionKey = myFaction.GetFactionKey();
     array<int> slotStates = {};
     for (int s = 0; s < ACR_FobSlots.UPGRADE_COUNT; s++)
     {
      slotStates.Insert(m_ACR_Data.m_BaseSupply[supplyOffset + 3 + s]);
     }
     m_ACR_Panel.SetFob(stock >= 0, ready && capacity - stock >= ACR_FobSupply.DELIVERY_AMOUNT && delivery == 0 && m_ACR_RP >= ACR_FobSupply.DELIVERY_COST, ready && stock > 0, slotStates, stock, ready, factionKey);
    }
    else if (m_ACR_Data.m_BaseSupply.Count() > supplyOffset + 2)
    {
     int stock = m_ACR_Data.m_BaseSupply[supplyOffset];
     int capacity = m_ACR_Data.m_BaseSupply[supplyOffset + 1];
     int delivery = m_ACR_Data.m_BaseSupply[supplyOffset + 2];
     bool ready = m_ACR_IsCommander && m_ACR_Data.m_BaseContested[baseIndex] == 0;
     m_ACR_Panel.SetFob(stock >= 0, ready && capacity - stock >= ACR_FobSupply.DELIVERY_AMOUNT && delivery == 0 && m_ACR_RP >= ACR_FobSupply.DELIVERY_COST, ready && stock > 0);
    }
    return;
   }
   m_ACR_SelectedBase = "";
  }
  int index = -1;
  if (m_ACR_SelectedGroup.IsValid()) index = m_ACR_Data.m_GroupIds.Find(m_ACR_SelectedGroup);
  if (index < 0)
  {
   // Selection vanished (squad wiped out, or not in the latest snapshot yet).
   m_ACR_Panel.SetGroup("", "", false, false, 0, 0, "");
   return;
  }
  bool friendly = index < m_ACR_Data.m_Commandable.Count() && m_ACR_Data.m_Commandable[index] == 1;
  int modeAndFlags = m_ACR_GroupSymbols[index * 4 + 3];
  bool canEstablishFob = friendly && (modeAndFlags & 32) != 0 && m_ACR_Data.m_Selected.Count() <= 1;
  if (!canEstablishFob && m_ACR_CursorMode == ACR_ECursorMode.ESTABLISH_FOB)
   m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  bool canRoadblock = friendly && (modeAndFlags & 64) != 0 && m_ACR_Data.m_Selected.Count() <= 1;
  if (!canRoadblock && m_ACR_CursorMode == ACR_ECursorMode.ROADBLOCK)
   m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  bool canMines = friendly && (modeAndFlags & 128) != 0 && m_ACR_Data.m_Selected.Count() <= 1;
  if (!canMines && m_ACR_CursorMode >= ACR_ECursorMode.MINES_AP && m_ACR_CursorMode <= ACR_ECursorMode.DISARM_MINES)
   m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  string hint;
  if (!friendly) hint = "Player group: not commandable";
  else if (!m_ACR_IsCommander) hint = "Take command to give orders";
  else if (m_ACR_CursorMode == ACR_ECursorMode.DEFEND) hint = "Right-click: defend there (click Defend again to cancel)";
  else if (m_ACR_CursorMode == ACR_ECursorMode.GARRISON) hint = "Right-click a building: they hold its windows and doors, filling the next buildings if needed (click Garrison again to cancel)";
  else if (m_ACR_CursorMode == ACR_ECursorMode.GET_IN) hint = "Right-click near a vehicle: get in (click Get in again to cancel)";
  else if (m_ACR_CursorMode == ACR_ECursorMode.LZ) hint = "Right-click the landing zone: they jump out when their ride sets down there";
  else if (m_ACR_CursorMode == ACR_ECursorMode.RIDE) hint = "Right-click the destination: a free truck nearby picks them up and drops them there";
  else if (m_ACR_CursorMode == ACR_ECursorMode.ESTABLISH_FOB) hint = "Right-click a clear site: establish forward base (click Establish FOB again to cancel)";
  else if (m_ACR_CursorMode == ACR_ECursorMode.ROADBLOCK) hint = "Right-click a road: establish fortified roadblock (20 RP; click Roadblock again to cancel)";
  else if (m_ACR_CursorMode >= ACR_ECursorMode.MINES_AP && m_ACR_CursorMode <= ACR_ECursorMode.DISARM_MINES) hint = ACR_Minefields.Hint(ACR_Minefields.KindOfCursor(m_ACR_CursorMode));
  else hint = "Right-click: move there";
  string title = m_ACR_Data.m_GroupNames[index];
  string detail = m_ACR_GroupDetails[index];
  string orderedBy = ACR_OrderedByText(m_ACR_SelectedGroup);
  if (!orderedBy.IsEmpty()) detail += "\n" + orderedBy;
  if (m_ACR_Data.m_Selected.Count() > 1)
  {
   title = string.Format("%1 squads selected", m_ACR_Data.m_Selected.Count());
   string names;
   foreach (RplId member : m_ACR_Data.m_Selected)
   {
    int memberIndex = m_ACR_Data.m_GroupIds.Find(member);
    if (memberIndex < 0) continue;
    if (!names.IsEmpty()) names += ", ";
    names += m_ACR_Data.m_GroupNames[memberIndex];
   }
   detail = names + " | orders go to all (switch: first)";
  }
  if (friendly && m_ACR_IsCommander && m_ACR_CursorMode == ACR_ECursorMode.MOVE) hint += "  |  Shift+right-click: add waypoint  |  Ctrl+drag: select squads";
  m_ACR_Panel.SetGroup(title, detail, friendly, m_ACR_IsCommander, modeAndFlags & 15, m_ACR_CursorMode, hint);
  m_ACR_Panel.SetGunner(friendly && (modeAndFlags & 16) != 0 && m_ACR_Data.m_Selected.Count() <= 1);
  m_ACR_Panel.SetEstablishFob(canEstablishFob);
  m_ACR_Panel.SetRoadblock(canRoadblock);
  m_ACR_Panel.SetMines(canMines);
  // Also for a full squad low on ammo: inside a deploy base Reinforce restocks it
  // (the server says so when it isn't in one).
  string details = m_ACR_GroupDetails[index];
  bool missing = details.Contains("missing");
  bool lowAmmo = details.Contains("| ammo ") && !details.Contains("| ammo 100%");
  m_ACR_Panel.SetReinforce("Reinforce (3 RP/soldier; FOB instant service extra)", missing || lowAmmo);
 }

 void ACR_OnPanelAction(int action)
 {
  ACR_MarkUiClick();
  switch (action)
  {
   case ACR_EPanelAction.TOGGLE_COMMAND:
    if (m_ACR_IsCommander) ACR_Request(ACR_ERequest.LEAVE_COMMAND);
    else ACR_Request(ACR_ERequest.TAKE_COMMAND);
    return;
   case ACR_EPanelAction.TOGGLE_RTS_CAMERA:
    ACR_ToggleRTSCamera();
    return;
   case ACR_EPanelAction.SWITCH: ACR_Request(ACR_ERequest.SWITCH_TO_GROUP, m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.TAKE_GUNNER: ACR_Request(ACR_ERequest.TAKE_GUNNER, m_ACR_SelectedGroup); return;
   case ACR_EPanelAction.MODE_COMBAT: ACR_RequestSelected(ACR_ERequest.MODE_COMBAT); return;
   case ACR_EPanelAction.MODE_FAST: ACR_RequestSelected(ACR_ERequest.MODE_FAST); return;
   case ACR_EPanelAction.MODE_SILENT: ACR_RequestSelected(ACR_ERequest.MODE_SILENT); return;
   // Defend / Get in arm the next right-click; pressing the same button again
   // cancels back to a plain move.
   case ACR_EPanelAction.DEFEND:
    if (m_ACR_CursorMode == ACR_ECursorMode.DEFEND) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else m_ACR_CursorMode = ACR_ECursorMode.DEFEND;
    return;
   case ACR_EPanelAction.GARRISON:
    if (m_ACR_CursorMode == ACR_ECursorMode.GARRISON) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else m_ACR_CursorMode = ACR_ECursorMode.GARRISON;
    return;
   case ACR_EPanelAction.GET_IN:
    if (m_ACR_CursorMode == ACR_ECursorMode.GET_IN) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else m_ACR_CursorMode = ACR_ECursorMode.GET_IN;
    return;
   case ACR_EPanelAction.SET_LZ:
    if (m_ACR_CursorMode == ACR_ECursorMode.LZ) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else m_ACR_CursorMode = ACR_ECursorMode.LZ;
    return;
   case ACR_EPanelAction.RIDE:
    if (m_ACR_CursorMode == ACR_ECursorMode.RIDE) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else m_ACR_CursorMode = ACR_ECursorMode.RIDE;
    return;
   case ACR_EPanelAction.GET_OUT: ACR_RequestSelected(ACR_ERequest.ORDER_GET_OUT); return;
   case ACR_EPanelAction.UNLOAD: ACR_RequestSelected(ACR_ERequest.ORDER_UNLOAD); return;
   case ACR_EPanelAction.REINFORCE: ACR_RequestSelected(ACR_ERequest.REINFORCE); return;
   case ACR_EPanelAction.FOB_SUPPLY: Rpc(ACR_FobServer, m_ACR_SelectedBase, false); return;
   case ACR_EPanelAction.FOB_BUILD: Rpc(ACR_FobServer, m_ACR_SelectedBase, true); return;
   case ACR_EPanelAction.ESTABLISH_FOB:
    if (m_ACR_CursorMode == ACR_ECursorMode.ESTABLISH_FOB) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else m_ACR_CursorMode = ACR_ECursorMode.ESTABLISH_FOB;
    return;
   case ACR_EPanelAction.ROADBLOCK:
    if (m_ACR_CursorMode == ACR_ECursorMode.ROADBLOCK) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
    else m_ACR_CursorMode = ACR_ECursorMode.ROADBLOCK;
    return;
   case ACR_EPanelAction.MINES_AP: ACR_ToggleCursor(ACR_ECursorMode.MINES_AP); return;
   case ACR_EPanelAction.MINES_AT: ACR_ToggleCursor(ACR_ECursorMode.MINES_AT); return;
   case ACR_EPanelAction.MINES_MIXED: ACR_ToggleCursor(ACR_ECursorMode.MINES_MIXED); return;
   case ACR_EPanelAction.DISARM_MINES: ACR_ToggleCursor(ACR_ECursorMode.DISARM_MINES); return;
   case ACR_EPanelAction.HALT: ACR_RequestSelected(ACR_ERequest.ORDER_HALT); return;
   case ACR_EPanelAction.FIRE_AT_WILL: ACR_RequestSelected(ACR_ERequest.ORDER_FIRE_AT_WILL); return;
   case ACR_EPanelAction.PING_LOOK: ACR_PickPing(ACR_EPing.LOOK); return;
   case ACR_EPanelAction.PING_ATTACK: ACR_PickPing(ACR_EPing.ATTACK); return;
   case ACR_EPanelAction.PING_DEFEND: ACR_PickPing(ACR_EPing.DEFEND); return;
   case ACR_EPanelAction.PING_SUPPORT: ACR_PickPing(ACR_EPing.SUPPORT); return;
   case ACR_EPanelAction.DEBUG_TOGGLE: m_ACR_DebugOpen = !m_ACR_DebugOpen; return;
   case ACR_EPanelAction.TOGGLE_REQUISITION:
    m_ACR_RequisitionOpen = !m_ACR_RequisitionOpen;
    if (m_ACR_RequisitionOpen) m_ACR_SupportOpen = false;
    m_ACR_PendingEntry = -1;
    m_ACR_PendingSupport = -1;
    return;
   case ACR_EPanelAction.TOGGLE_REPORT: m_ACR_ReportOpen = !m_ACR_ReportOpen; return;
   case ACR_EPanelAction.TOGGLE_SUPPORTS:
    m_ACR_SupportOpen = !m_ACR_SupportOpen;
    m_ACR_HasSupportTarget = false;
    if (m_ACR_SupportOpen) m_ACR_RequisitionOpen = false;
    m_ACR_PendingEntry = -1;
    m_ACR_PendingSupport = -1;
    return;
  }
  if (action >= ACR_EPanelAction.DEBUG_ACTION && action < ACR_EPanelAction.REQUISITION_ENTRY)
  {
   ACR_RequestDebug(action - ACR_EPanelAction.DEBUG_ACTION);
   return;
  }
  if (action >= ACR_EPanelAction.FOB_UPGRADE && action < ACR_EPanelAction.FOB_UPGRADE + ACR_FobSlots.UPGRADE_COUNT)
  {
   int slot = action - ACR_EPanelAction.FOB_UPGRADE;
   ACR_RequestFobUpgrade(m_ACR_SelectedBase, slot);
   return;
  }
  if (action >= ACR_EPanelAction.SUPPORT_ENTRY)
  {
   int support = action - ACR_EPanelAction.SUPPORT_ENTRY;
   // Aimed from a Support ping: fire at it straight away.
   if (m_ACR_HasSupportTarget)
   {
    m_ACR_HasSupportTarget = false;
    Rpc(ACR_SupportServer, support, m_ACR_SupportTarget, m_ACR_SelectedGroup);
    m_ACR_PendingSupport = -1;
    m_ACR_SupportOpen = false;
    return;
   }
   if (m_ACR_PendingSupport == support) m_ACR_PendingSupport = -1;
   else m_ACR_PendingSupport = support;
   m_ACR_PendingEntry = -1;
   return;
  }
  if (action >= ACR_EPanelAction.REQUISITION_ENTRY)
  {
   int entry = action - ACR_EPanelAction.REQUISITION_ENTRY;
   if (m_ACR_PendingEntry == entry) m_ACR_PendingEntry = -1; // click again to cancel
   else m_ACR_PendingEntry = entry;
  }
 }

 // Called by the map radial menu hook. Returns true if the right-click was used.
 // Game Master or a construction truck's build mode is open: its map clicks are its own
 // (in build mode they used to reach a possessed vehicle's squad as move orders).
 static bool ACR_EditorOpen()
 {
  SCR_EditorManagerEntity editor = SCR_EditorManagerEntity.GetInstance();
  return editor && editor.IsOpened();
 }

 bool ACR_HandleMapRightClick()
 {
  if (!ACR_Manager.GetInstance()) return false;
  if (ACR_EditorOpen()) return false;
  if (ACR_ModifierHeld("ACR_MapAlt"))
  {
   SCR_MapEntity pickerMap = SCR_MapEntity.GetMapInstance();
   if (!pickerMap) return false;
   float cx, cz;
   pickerMap.GetMapCursorWorldPosition(cx, cz);
   ACR_OpenPicker(pickerMap, Vector(cx, 0, cz));
   return true;
  }
  ACR_ClosePicker();
  if (m_ACR_PendingSupport >= 0)
  {
   SCR_MapEntity supportMap = SCR_MapEntity.GetMapInstance();
   if (!supportMap) return false;
   float sx, sz;
   supportMap.GetMapCursorWorldPosition(sx, sz);
   Rpc(ACR_SupportServer, m_ACR_PendingSupport, Vector(sx, 0, sz), m_ACR_SelectedGroup);
   m_ACR_PendingSupport = -1;
   return true;
  }
  if (m_ACR_PendingEntry >= 0)
  {
   m_ACR_PendingEntry = -1;
   return true;
  }
  if (!m_ACR_SelectedGroup.IsValid()) return false;
  int index = m_ACR_Data.m_GroupIds.Find(m_ACR_SelectedGroup);
  if (index < 0 || index >= m_ACR_Data.m_Commandable.Count() || m_ACR_Data.m_Commandable[index] != 1) return false;
  if (!m_ACR_IsCommander)
  {
   SCR_HintManagerComponent.ShowCustomHint("Only the commander can give map orders. Take command in the panel.", "AC: Actual", 4, true);
   return true;
  }
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return false;
  float x, z;
  mapEntity.GetMapCursorWorldPosition(x, z);
  bool append = ACR_ModifierHeld("ACR_MapShift");
  int command = ACR_ERequest.MOVE_TO;
  if (append) command = ACR_ERequest.QUEUE_MOVE;
  if (m_ACR_CursorMode == ACR_ECursorMode.DEFEND)
  {
   command = ACR_ERequest.ORDER_DEFEND;
   if (append) command = ACR_ERequest.QUEUE_DEFEND;
  }
  if (m_ACR_CursorMode == ACR_ECursorMode.GARRISON)
  {
   command = ACR_ERequest.ORDER_GARRISON;
   if (append) command = ACR_ERequest.QUEUE_GARRISON;
  }
  if (m_ACR_CursorMode == ACR_ECursorMode.GET_IN) command = ACR_ERequest.ORDER_GET_IN;
  if (m_ACR_CursorMode == ACR_ECursorMode.LZ) command = ACR_ERequest.ORDER_SET_LZ;
  if (m_ACR_CursorMode == ACR_ECursorMode.RIDE) command = ACR_ERequest.ORDER_RIDE;
  if (m_ACR_CursorMode == ACR_ECursorMode.ESTABLISH_FOB) command = ACR_ERequest.ORDER_ESTABLISH_FOB;
  if (m_ACR_CursorMode == ACR_ECursorMode.ROADBLOCK) command = ACR_ERequest.ORDER_ROADBLOCK;
  if (m_ACR_CursorMode == ACR_ECursorMode.MINES_AP) command = ACR_ERequest.ORDER_MINES_AP;
  if (m_ACR_CursorMode == ACR_ECursorMode.MINES_AT) command = ACR_ERequest.ORDER_MINES_AT;
  if (m_ACR_CursorMode == ACR_ECursorMode.MINES_MIXED) command = ACR_ERequest.ORDER_MINES_MIXED;
  if (m_ACR_CursorMode == ACR_ECursorMode.DISARM_MINES) command = ACR_ERequest.ORDER_DISARM_MINES;
  vector target = Vector(x, 0, z);
  // Right-click on a spotted enemy: hunt it down.
  if (m_ACR_CursorMode == ACR_ECursorMode.MOVE && !append)
  {
   int hunted = ACR_ContactUnderCursor(mapEntity, x, z);
   if (hunted >= 0)
   {
    command = ACR_ERequest.ORDER_HUNT;
    target = m_ACR_ContactPositions[hunted];
   }
  }
  ACR_IssueOrder(command, target, append);
  return true;
 }

 // An order button arms its right-click order; a second click disarms it.
 protected void ACR_ToggleCursor(ACR_ECursorMode mode)
 {
  if (m_ACR_CursorMode == mode) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  else m_ACR_CursorMode = mode;
 }

 bool ACR_IssueOrder(int command, vector target, bool append = false)
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
   ACR_Request(command, squadId, point);
   if (spread) ACR_AddPending(m_ACR_Data.LeaderPos(m_ACR_Data.m_GroupIds.Find(squadId)), point);
  }
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  return true;
 }

 // ---------------------------------------------------------------- order lines
 // A line from each friendly squad's leader to its current destination, ending
 // in a dot, like the Arma 3 mod. Colour shows the order; the selected squad's
 // line is brighter and thicker.
 protected ref ACR_MapData m_ACR_Data = new ACR_MapData();
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
  m_ACR_Display.Draw(mapEntity, m_ACR_Data, m_ACR_Boxing, m_ACR_BoxStart);
 }



 // ---------------------------------------------------------------- intel
 // Enemies our side has spotted (server-side ACR_Intel), as hostile map symbols
 // that turn "suspected" (dashed) after a while and vanish when forgotten.
 protected ref array<vector> m_ACR_ContactPositions = {};
 protected ref array<int> m_ACR_ContactKinds = {};
 protected ref array<int> m_ACR_ContactCounts = {};
 protected ref array<int> m_ACR_ContactAges = {};
 protected ref array<string> m_ACR_Feed = {};
 protected ref array<ref ACR_UnitMarker> m_ACR_ContactMarkers = {};
 protected ref array<int> m_ACR_ContactMarkerKinds = {};
 static const int ACR_CONTACT_SUSPECTED = 30; // s: older sightings are drawn as suspected

 protected ref array<int> m_ACR_FeedAges = {};
 protected int m_ACR_IntelAt;
 void ACR_SendIntel(array<vector> positions, array<int> kinds, array<int> counts, array<int> ages, array<string> feed, array<int> feedAges)
 {
  Rpc(ACR_IntelOwner, positions, kinds, counts, ages, feed, feedAges);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_IntelOwner(array<vector> positions, array<int> kinds, array<int> counts, array<int> ages, array<string> feed, array<int> feedAges)
 {
  m_ACR_ContactPositions.Copy(positions);
  m_ACR_ContactKinds.Copy(kinds);
  m_ACR_ContactCounts.Copy(counts);
  m_ACR_ContactAges.Copy(ages);
  m_ACR_Feed.Copy(feed);
  m_ACR_FeedAges.Copy(feedAges);
  m_ACR_IntelAt = System.GetTickCount();
 }

 // The feed as shown: "[12s] line", newest first, nothing older than 10 min.
 protected void ACR_FeedLines(notnull array<string> lines)
 {
  lines.Clear();
  int since = ACR_MapData.Since(m_ACR_IntelAt);
  foreach (int i, string line : m_ACR_Feed)
  {
   int seconds = since;
   if (i < m_ACR_FeedAges.Count()) seconds += m_ACR_FeedAges[i];
   if (seconds > 600) break;
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

 // Index of the enemy contact marker within 16 px of the map cursor, or -1.
 protected int ACR_ContactUnderCursor(SCR_MapEntity mapEntity, float x, float z)
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
 int ACR_VehicleCount() { return m_ACR_VehiclePositions.Count(); }

 // Our side's empty vehicles (fog of war: only those our people can see,
 // or their last known spot for a while).
 protected ref array<vector> m_ACR_VehiclePositions = {};
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
    marker.SetType(SCR_EMapMarkerType.SIMPLE);
    marker.SetMarkerConfigID(1);
    marker.SetCanBeRemovedByOwner(false);
    m_ACR_VehicleMarkers.Insert(marker);
    manager.InsertStaticMarker(marker, true);
    marker.SetMarkerOwnerID(-1);
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
 }
 int ACR_FeedCount() { return m_ACR_Feed.Count(); }

 protected static int ACR_ContactIcon(int kind) { return ACR_Intel.IconOf(kind); }
 protected static int ACR_ContactDimension(int kind) { return ACR_Intel.DimensionOf(kind); }

 protected void ACR_UpdateContactMarkers(SCR_MapMarkerManagerComponent manager)
 {
  // Rebuild when the number or the kinds change (the symbol shape is set at creation).
  bool rebuild = m_ACR_ContactMarkers.Count() != m_ACR_ContactKinds.Count();
  if (!rebuild)
   foreach (int k, int kind : m_ACR_ContactKinds)
    if (m_ACR_ContactMarkerKinds[k] != kind) rebuild = true;
  if (rebuild)
  {
   foreach (ACR_UnitMarker old : m_ACR_ContactMarkers) manager.ACR_RemoveMarker(old);
   m_ACR_ContactMarkers.Clear();
   m_ACR_ContactMarkerKinds.Clear();
   foreach (int kindToMake : m_ACR_ContactKinds)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    marker.m_bContact = true;
    SCR_MapMarkerBase prepared = manager.PrepareMilitaryMarker(EMilitarySymbolIdentity.OPFOR, ACR_ContactDimension(kindToMake), ACR_ContactIcon(kindToMake));
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
    marker.m_bContact = true;
    m_ACR_ContactMarkers.Insert(marker);
    m_ACR_ContactMarkerKinds.Insert(kindToMake);
    manager.InsertStaticMarker(marker, true);
    marker.SetMarkerOwnerID(-1);
   }
  }
  foreach (int i, ACR_UnitMarker contact : m_ACR_ContactMarkers)
  {
   int age = m_ACR_ContactAges[i] + ACR_MapData.Since(m_ACR_IntelAt);
   int kindNow = m_ACR_ContactKinds[i];
   contact.m_Label = ACR_Intel.Describe(kindNow, m_ACR_ContactCounts[i]) + " (" + age.ToString() + "s)";
   contact.m_State = 3;
   contact.m_bContact = true;
   vector pos = m_ACR_ContactPositions[i];
   contact.SetWorldPos(pos[0], pos[2]);
   int identity = EMilitarySymbolIdentity.OPFOR;
   if (age > ACR_CONTACT_SUSPECTED) identity = EMilitarySymbolIdentity.ASSUMED_OPFOR;
   if (!contact.m_GroupSymbol) contact.m_GroupSymbol = new SCR_MilitarySymbol();
   contact.m_GroupSymbol.Set(identity, ACR_ContactDimension(kindNow), ACR_ContactIcon(kindNow), EMilitarySymbolAmplifier.NONE);
   contact.m_iSymbolKey = ACR_ContactIcon(kindNow) * 1000 + ACR_ContactDimension(kindNow) * 10 + identity;
   contact.RefreshStyle();
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
    marker.SetType(SCR_EMapMarkerType.SIMPLE);
    marker.SetMarkerConfigID(1);
    marker.SetCanBeRemovedByOwner(false);
    m_ACR_ZoneMarkers.Insert(marker);
    manager.InsertStaticMarker(marker, true);
    marker.SetMarkerOwnerID(-1);
   }
  }
  foreach (int i, ACR_UnitMarker marker : m_ACR_ZoneMarkers)
  {
   string label = m_ACR_Data.m_ZoneLabels[i];
   // "~": a mortar team's range ring, labelled at the tube.
   bool range = label.StartsWith("~");
   if (range) label = label.Substring(1, label.Length() - 1);
   marker.m_Label = label;
   vector pos = m_ACR_Data.m_ZonePositions[i];
   marker.SetWorldPos(pos[0], pos[2]);
   marker.RefreshStyle();
   Color color = new Color(1, 0.2, 0.15, 1);
   if (range) color = new Color(0.55, 0.8, 1, 1);
   if (marker.GetRootWidget()) marker.ACR_SetColor(color);
  }
 }

 protected string m_ACR_SelectedBase;

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

 protected float m_ACR_LastFobRequest = -1000;
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_FobServer(string label, bool build)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastFobRequest < 500) return;
  m_ACR_LastFobRequest = now;
  Rpc(ACR_FeedbackOwner, ACR_FobSupply.Request(this, label, build));
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
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastFobRequest < 500) return;
  m_ACR_LastFobRequest = now;
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
  return symbol + label;
 }

 protected void ACR_UpdateBaseMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (m_ACR_BaseMarkers.Count() != m_ACR_Data.m_BaseNames.Count())
  {
   foreach (ACR_UnitMarker old : m_ACR_BaseMarkers) manager.ACR_RemoveMarker(old);
   m_ACR_BaseMarkers.Clear();
   for (int i = 0; i < m_ACR_Data.m_BaseNames.Count(); i++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    marker.SetType(SCR_EMapMarkerType.SIMPLE);
    marker.SetMarkerConfigID(1);
    marker.SetCanBeRemovedByOwner(false);
    marker.m_BaseIndex = i;
    m_ACR_BaseMarkers.Insert(marker);
    manager.InsertStaticMarker(marker, true);
    marker.SetMarkerOwnerID(-1);
   }
  }
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
 protected ref array<string> m_ACR_ReqNames = {};
 protected ref array<int> m_ACR_ReqCosts = {};
 protected ref array<int> m_ACR_ReqRemaining = {};
 protected bool m_ACR_RequisitionOpen;
 protected int m_ACR_PendingEntry = -1;

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
 protected ref array<string> m_ACR_SupNames = {};
 protected ref array<int> m_ACR_SupCosts = {};
 protected ref array<int> m_ACR_SupCooldowns = {};
 protected bool m_ACR_SupportOpen;
 protected int m_ACR_PendingSupport = -1;

 // ---------------------------------------------------------------- after-action report
 protected bool m_ACR_ReportOpen;
 protected string m_ACR_ReportText = "No report yet";
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
 protected void ACR_Cooldowns(notnull array<int> result)
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
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastCommand < 300) return;
  m_ACR_LastCommand = now;
  Rpc(ACR_SupportResultOwner, manager.CallSupport(this, index, target, group));
 }
 // The answer to a support call, also kept in the panel for a while (a refusal such as
 // "out of range" was only a hint, easy to miss on the map).
 protected string m_ACR_SupportResult;
 protected int m_ACR_SupportResultAt;
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
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastCommand < 300) return;
  m_ACR_LastCommand = now;
  Rpc(ACR_FeedbackOwner, manager.Purchase(this, entryIndex, baseLabel, Vector(spotX, 0, spotZ), hasSpot));
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
 protected string ACR_RosterText()
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
 protected string ACR_OrderedByText(RplId groupId)
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

 protected bool m_ACR_HasSupportTarget;
 protected vector m_ACR_SupportTarget;
 protected string m_ACR_SupportTargetFor;

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

 protected void ACR_PickPing(int kind)
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
    if (!m_ACR_PingPickerShown && mapOpen && !ACR_EditorOpen())
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
   if (!m_ACR_PingOnMap || !mapOpen || m_ACR_PingPickerShown || ACR_EditorOpen()) return;
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
