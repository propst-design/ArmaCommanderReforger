// Tactical 3D Commander Camera (aca-f02).
// Overhead RTS perspective hovering above the battlefield with smooth WASD ground movement,
// terrain-following altitude (ATL 12m-160m), RMB orbit rotation (a drag to aim while an order that
// takes a facing is armed), mouse wheel zoom, 300m proximity
// leash to friendly squads and bases, and full 3D selection and authoritative order dispatching.

class ACR_RTSCameraHandler : ScriptedWidgetEventHandler
{
 ACR_RTSCameraManager m_Manager;
 protected bool m_bLeftDown;

 void ACR_RTSCameraHandler(ACR_RTSCameraManager manager) { m_Manager = manager; }

 override bool OnMouseButtonDown(Widget w, int x, int y, int button)
 {
  if (!m_Manager) return false;
  if (button == 0) m_bLeftDown = true;
  if (button == 1) m_Manager.BeginOrbit(x, y);
  return true;
 }

 override bool OnMouseButtonUp(Widget w, int x, int y, int button)
 {
  if (!m_Manager) return false;
  if (button == 0 && m_bLeftDown)
  {
   m_bLeftDown = false;
   if (WidgetManager.GetWidgetUnderCursor() == w) m_Manager.HandleLeftClick(x, y);
  }
  if (button == 1) m_Manager.EndOrbit(x, y, WidgetManager.GetWidgetUnderCursor() == w);
  return true;
 }
}

// The makeover (30 Oct, Jacob's OK on Discord: "lower the floor on the camera and add a hide UI
// button"): the bars in the 2IC's dark style with blue headings (ACR_TwoICStyle), an armed tool lit
// in the accent instead of "[ON]", round-cornered toasts, cleaner markers; the floor at about eye
// height, riding over roofs and tree tops; Hide UI (H); Follow (F, or a double-click on a squad's
// marker): the camera glides after the squad and turns round it.
class ACR_RTSCameraManager : Managed
{
	protected static const ResourceName LAYOUT_BUTTON = "{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout";
	protected static const ResourceName LAYOUT_TEXT = "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout";
	protected static const ResourceName LAYOUT_HEADING = "{0D7FF083FDE39877}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading4.layout";


	static ref ACR_RTSCameraManager s_Instance;
	static bool s_bAdminUnbound = false;

 protected CameraBase m_Camera;
 protected CameraBase m_PreviousCamera;
 protected IEntity m_Body;
 protected int m_iPointCommand;
 protected bool m_bOrbit;
 protected bool m_bDragged;
 protected bool m_bAiming;        // drag to aim: this right-drag aims the armed order (AimArmed)
 protected vector m_vAimFrom;     // ... from where the button went down
 protected IEntity m_AimFromEnt;
 protected bool m_bAimToastShown;
 protected int m_iMouseX;
 protected int m_iMouseY;
 protected int m_iDownX;
 protected int m_iDownY;
 protected CanvasWidget m_AimCanvas;    // the drag-to-aim arrow, in native pixels
 protected ref array<ref CanvasWidgetCommand> m_AimCommands = {};
 protected ref array<RplId> m_LastSelected = {}; // last frame's selection: a change disarms the tool
 protected static const float AIM_MIN_PX = 8;     // no arrow while its ends are this close
	protected Widget m_RootWidget;
	protected Widget m_InteractionLayer;
	protected TextWidget m_HeadingText;
	protected TextWidget m_LeashText;
	protected TextWidget m_WarningText;
	protected TextWidget m_SquadText;
	protected TextWidget m_SquadDetailText;
	protected TextWidget m_ToastText;

	protected Widget m_BottomBar;
	protected Widget m_UnitActionsRow;
	protected SCR_ButtonTextComponent m_ExitBtn;
	protected SCR_ButtonTextComponent m_MapBtn;
	protected SCR_ButtonTextComponent m_DefendBtn;
	protected SCR_ButtonTextComponent m_GarrisonBtn;
	protected SCR_ButtonTextComponent m_GetInBtn;
	protected SCR_ButtonTextComponent m_SwitchBtn;
	protected SCR_ButtonTextComponent m_HaltBtn;
	protected SCR_ButtonTextComponent m_RoadblockBtn;
	protected bool m_bRoadblockMode = false;
	// The Minefield button (#4): -1 off, else the ACR_EMineKind a right-click lays (AP, AT, mixed).
	protected SCR_ButtonTextComponent m_MinesBtn;
	protected int m_iMineKind = -1;
	// The Fortify button: a right-click digs the selected squad in there, or upgrades our position there.
	protected SCR_ButtonTextComponent m_FortifyBtn;
	protected bool m_bFortifyMode = false;

	protected ref map<SCR_ButtonBaseComponent, int> m_Actions = new map<SCR_ButtonBaseComponent, int>();
	protected ref ACR_RTSCameraHandler m_Handler;

	protected float m_fLastWarningAt = -10000;
	protected float m_fToastUntil = 0;
	protected float m_fLastHealth = 100;
	protected bool m_bDefendMode = false;
	protected bool m_bContextActivated; // last frame's cursor-context activation (tests read it)
	protected Widget m_TopBar;
	protected Widget m_ToastRoot;
	protected Widget m_HiddenHint;      // "UI hidden" for a moment after H
	protected TextWidget m_FollowText;
	protected SCR_ButtonTextComponent m_FollowBtn;
	protected SCR_ButtonTextComponent m_HideBtn;
	protected ref map<SCR_ButtonTextComponent, string> m_Lit = new map<SCR_ButtonTextComponent, string>();
	protected bool m_bUiHidden;
	protected float m_fHintUntil;
	protected RplId m_FollowGroup = RplId.Invalid();
	protected vector m_vFollowOffset;  // from the squad to the camera
	protected vector m_vFollowAt;      // the squad, smoothed
	protected vector m_vFollowGlide;   // what is left of the way to the follow spot
	protected float m_fFollowLength;   // the wheel's distance from the squad
	protected float m_fFloorY = -100000; // the top of what is under the camera (ground, roof, tree), smoothed
	protected int m_iLastMarkerClick = -1;
	protected int m_iLastMarkerClickAt;

	// Altitude above terrain: the wheel sets a target, the camera glides to it.
	protected static const float ATL_MIN = 2.0;   // about eye height (it was 12 m)
	protected static const float ATL_MAX = 160.0;
	protected static const float ATL_PER_NOTCH = 0.87; // each notch scales height ~13%
	protected static const float CLEARANCE = 1.8;  // m above a roof or a tree top under the camera
	protected static const float LOW = 12.0;       // under this the camera may tilt up past the horizon
	protected static const float HIGH = 60.0;      // over this it looks down at least 15 degrees
	protected static const float FOLLOW_MIN = 6.0; // m from the squad, following
	protected static const float FOLLOW_MAX = 250.0;
	protected static const int DOUBLE_CLICK_MS = 400;
	protected static const float FOLLOW_EASE = 5.0;  // per second: the followed point after a soldier we can see
	protected static const float FOLLOW_EASE_FAR = 1.5; // ... after the server's position, which jumps once a second
	protected static const float FOLLOW_NEAR = 60.0;  // m: a soldier further than this from the server's position isn't trusted
	protected float m_fTargetAtl = 35.0;

	// World markers: one pin per friendly squad and base, re-projected every frame.
	// Screen positions are kept (reference pixels, z = depth) so clicks pick markers.
	protected static const float MARKER_PICK_RADIUS = 28.0;
	protected Widget m_MarkerLayer;
	protected ref array<Widget> m_SquadMarkers = {};
	protected ref array<Widget> m_BaseMarkers = {};
	protected ref array<vector> m_SquadMarkerPos = {};
	protected ref array<vector> m_BaseMarkerPos = {};
	protected ref ACR_RTSOverlay m_Overlay; // order lines and spotted enemies (#66)

	// ---------------------------------------------------------------- lifecycle
	static bool IsActive()
	{
		return s_Instance != null && s_Instance.m_Camera != null;
	}

	static void Toggle()
	{
		if (IsActive()) Close();
		else Open();
	}

	// From the map (its panel's button, F10): the map takes a moment to shut, and the camera closes
	// itself on any frame the map is open (the button only closed the map, playtest pn15). So the
	// camera opens once the map has shut (Frame).
	protected static bool s_bOpenPending;
	protected static int s_iOpenBy;
	static void OpenAfterMap()
	{
		s_bOpenPending = true;
		s_iOpenBy = System.GetTickCount() + 5000;
	}

	static void Open()
	{
		if (IsActive()) return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc) return;
		IEntity localBody = pc.GetControlledEntity();
		if (!ACR_Respawn.IsAlive(localBody)) return;

		s_Instance = new ACR_RTSCameraManager();
		s_Instance.InitCamera(pc, localBody);
		if (!IsActive()) Close();
	}

	// F9 or the Map button: straight back to the command map. The camera shuts first (the map would
	// close it on its next frame anyway); ACR_Manager skips its own F9 on that frame, the camera
	// having been open at its start, so the map isn't toggled shut again.
	static void ToMap()
	{
		Close();
		MenuManager menus = GetGame().GetMenuManager();
		if (menus && !menus.IsAnyMenuOpen()) menus.OpenMenu(ChimeraMenuPreset.MapMenu);
	}

	static void Close()
	{
		s_bOpenPending = false;
		if (!s_Instance) return;
		s_Instance.Cleanup();
		s_Instance = null;
	}

	static void Frame(SCR_PlayerController pc)
	{
		if (s_bOpenPending)
		{
			SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
			if ((!mapEntity || !mapEntity.IsOpen()) && !GetGame().GetMenuManager().IsAnyMenuOpen())
			{
				s_bOpenPending = false;
				Open();
			}
			else if (System.GetTickCount() > s_iOpenBy) s_bOpenPending = false;
		}
		if (s_Instance) s_Instance.FrameUpdate(pc);
	}

	// ---------------------------------------------------------------- initialization
	protected void InitCamera(SCR_PlayerController pc, IEntity localBody)
	{
		// F10 during the mission intro: end it first, so its camera hands back to the player's before we
		// save it as the one to return to (it is deleted when the intro ends).
		ACR_Intro.Stop("rts camera");
		m_Body = localBody;
		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras) m_PreviousCamera = cameras.CurrentCamera();
		// 1. Mute physical soldier controls so commander doesn't move or shoot while surveying
		ACR_MapHandoff.ClearCharacterInput(localBody);
		ACR_MapHandoff.SetMenuControls(localBody, true);

		SCR_CharacterDamageManagerComponent dmg = SCR_CharacterDamageManagerComponent.Cast(localBody.FindComponent(SCR_CharacterDamageManagerComponent));
		if (dmg) m_fLastHealth = dmg.GetHealth();

		// 2. Anchor position & orientation: selected squad / base or player's location
		vector eye = vector.Zero;
		vector angles = Vector(0, -45, 0); // Yaw, Pitch (-45 deg down), Roll

		ACR_MapData data = pc.ACR_GetData();
		RplId selSquad = pc.ACR_GetSelectedGroup();
		if (selSquad.IsValid() && data)
		{
			int sqIdx = data.m_GroupIds.Find(selSquad);
			if (sqIdx >= 0) eye = data.LeaderPos(sqIdx);
		}
		else if (!pc.ACR_SelectedBase().IsEmpty() && data)
		{
			int bIdx = data.m_BaseNames.Find(pc.ACR_SelectedBase());
			if (bIdx >= 0) eye = data.m_BasePositions[bIdx];
		}

		if (eye == vector.Zero)
		{
			eye = localBody.GetOrigin();
			vector bodyAngles = localBody.GetYawPitchRoll();
			angles[0] = bodyAngles[0]; // Face player's facing yaw
		}

		float groundY = GetGame().GetWorld().GetSurfaceY(eye[0], eye[2]);
		eye[1] = Math.Max(groundY, eye[1]) + 35.0; // Initial 35m altitude

		// 3. Spawn Camera Entity
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(angles, params.Transform);
		params.Transform[3] = eye;

  // Keep the native spectator rendering setup, but let RTS alone own input
  // and transforms instead of the editor's free-flight component stack.
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
  m_Camera.SetWorldTransform(params.Transform);
  m_Camera.SetVerticalFOV(60);
  m_Camera.SetNearPlane(0.2); // close to the ground at eye height
  m_Camera.SetFarPlane(8000);
  if (cameras) cameras.SetCamera(m_Camera);

		// 4. Build HUD Overlay & Interactive Canvas
		CreateUI();
		if (!m_InteractionLayer) { Cleanup(); return; }
		GetGame().GetInputManager().ActivateContext("ACR_RTSContext");
		ShowToast("3D camera: F9 back to the map, F10 or Esc to leave");
	}

	// ---------------------------------------------------------------- UI creation
	protected void CreateUI()
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws) return;

		// Root overlay covering full screen
		m_RootWidget = ACR_Hud.CreateLayer(900, true);
		if (!m_RootWidget) return;

		// Fullscreen interactive click layer
		m_InteractionLayer = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, new Color(0, 0, 0, 0.001), 0, m_RootWidget);
		FrameSlot.SetAnchorMin(m_InteractionLayer, 0, 0);
		FrameSlot.SetAnchorMax(m_InteractionLayer, 1, 1);
		FrameSlot.SetOffsets(m_InteractionLayer, 0, 0, 0, 0);
		m_Handler = new ACR_RTSCameraHandler(this);
		m_InteractionLayer.AddHandler(m_Handler);

		// Markers sit above the click layer but ignore the cursor, so clicks still land on it.
		m_MarkerLayer = ws.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, m_RootWidget);
		FrameSlot.SetAnchorMin(m_MarkerLayer, 0, 0);
		FrameSlot.SetAnchorMax(m_MarkerLayer, 1, 1);
		FrameSlot.SetOffsets(m_MarkerLayer, 0, 0, 0, 0);
		m_Overlay = new ACR_RTSOverlay(m_MarkerLayer);

		// The drag-to-aim arrow, over the scene and ignoring the cursor like the markers.
		m_AimCanvas = CanvasWidget.Cast(ws.CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromInt(Color.WHITE), 0, m_RootWidget));
		if (m_AimCanvas)
		{
			FrameSlot.SetAnchorMin(m_AimCanvas, 0, 0);
			FrameSlot.SetAnchorMax(m_AimCanvas, 1, 1);
			FrameSlot.SetOffsets(m_AimCanvas, 0, 0, 0, 0);
		}

		// The top bar: what this is, the leash, who is followed, a warning; Follow, Hide UI and Exit.
		Widget topCol = ACR_TwoICStyle.PanelBox(m_RootWidget, 980, 16, 10, m_TopBar, ACR_MapDeclutter.PanelColor());
		FrameSlot.SetAnchorMin(m_TopBar, 0.5, 0);
		FrameSlot.SetAnchorMax(m_TopBar, 0.5, 0);
		FrameSlot.SetAlignment(m_TopBar, 0.5, 0);
		FrameSlot.SetPos(m_TopBar, 0, 16);
		FrameSlot.SetSizeToContent(m_TopBar, true);
		Widget topRow = ACR_TwoICStyle.Row(topCol);
		Widget topLeft = ACR_TwoICStyle.Column(topRow);
		ACR_TwoICStyle.Grow(topLeft);
		Widget headRow = ACR_TwoICStyle.Row(topLeft);
		SizeLayoutWidget mark = ACR_TwoICStyle.Box(headRow, 12, 12, ACR_MapDeclutter.AccentBlue());
		if (mark)
		{
			AlignableSlot.SetVerticalAlign(mark, LayoutVerticalAlign.Center);
			AlignableSlot.SetPadding(mark, 0, 0, 10, 0);
		}
		m_HeadingText = ACR_TwoICStyle.Text(headRow, "3D CAMERA", ACR_MapDeclutter.AccentBlue(), 22, true);
		m_LeashText = ACR_TwoICStyle.Text(topLeft, "", ACR_TwoICStyle.Body(), 16);
		m_FollowText = ACR_TwoICStyle.Text(topLeft, "", ACR_TwoICStyle.Accent(), 16);
		m_WarningText = ACR_TwoICStyle.Text(topLeft, "", ACR_TwoICStyle.Rgb(240, 96, 84), 16);
		Widget topRight = ACR_TwoICStyle.Row(topRow);
		AlignableSlot.SetVerticalAlign(topRight, LayoutVerticalAlign.Center);
		m_FollowBtn = AddButton(topRight, "Follow  [F]", 10);
		m_HideBtn = AddButton(topRight, "Hide UI  [H]", 11);
		m_MapBtn = AddButton(topRight, "Map  [F9]", 12);
		m_ExitBtn = AddButton(topRight, "Exit  [F10]", 1);

		// The bottom bar: the selection, what it is doing, its orders, the keys.
		Widget botCol = ACR_TwoICStyle.PanelBox(m_RootWidget, 980, 16, 10, m_BottomBar, ACR_MapDeclutter.PanelColor());
		FrameSlot.SetAnchorMin(m_BottomBar, 0.5, 1);
		FrameSlot.SetAnchorMax(m_BottomBar, 0.5, 1);
		FrameSlot.SetAlignment(m_BottomBar, 0.5, 1);
		FrameSlot.SetPos(m_BottomBar, 0, -20);
		FrameSlot.SetSizeToContent(m_BottomBar, true);
		m_SquadText = ACR_TwoICStyle.Text(botCol, "Tactical overview", ACR_TwoICStyle.White(), 20, true);
		m_SquadDetailText = ACR_TwoICStyle.Text(botCol, "", ACR_TwoICStyle.Body(), 16);
		m_UnitActionsRow = ACR_TwoICStyle.Row(botCol, 8);
		m_DefendBtn = AddButton(m_UnitActionsRow, "Defend", 2, true);
		m_GarrisonBtn = AddButton(m_UnitActionsRow, "Garrison", 3, true);
		m_GetInBtn = AddButton(m_UnitActionsRow, "Get in", 4, true);
		m_SwitchBtn = AddButton(m_UnitActionsRow, "Take over", 5, true);
		m_HaltBtn = AddButton(m_UnitActionsRow, "Halt", 6, true);
		m_RoadblockBtn = AddButton(m_UnitActionsRow, "Roadblock", 7, true);
		m_MinesBtn = AddButton(m_UnitActionsRow, "Minefield", 8, true);
		m_FortifyBtn = AddButton(m_UnitActionsRow, "Fortify", 9, true);
		TextWidget keys = ACR_TwoICStyle.Text(botCol, "WASD pan  ·  Shift faster  ·  Wheel height  ·  Right-drag turn  ·  Left-click select  ·  Right-click order (Shift: queue)  ·  F follow  ·  H hide UI  ·  F9 map  ·  F10 leave", ACR_TwoICStyle.Dim(), 14);
		if (keys) AlignableSlot.SetPadding(keys, 0, 8, 0, 0);

		// The toast: a dark pill over the lower middle of the scene.
		Widget toastCol = ACR_TwoICStyle.PanelBox(m_RootWidget, 0, 18, 8, m_ToastRoot, ACR_MapDeclutter.PanelColor());
		FrameSlot.SetAnchorMin(m_ToastRoot, 0.5, 0.74);
		FrameSlot.SetAnchorMax(m_ToastRoot, 0.5, 0.74);
		FrameSlot.SetAlignment(m_ToastRoot, 0.5, 0.5);
		FrameSlot.SetSizeToContent(m_ToastRoot, true);
		m_ToastRoot.SetFlags(WidgetFlags.IGNORE_CURSOR);
		m_ToastText = ACR_TwoICStyle.Text(toastCol, "", ACR_TwoICStyle.White(), 18);
		m_ToastRoot.SetVisible(false);

		// With the UI hidden: a small hint at the bottom for a moment.
		Widget hintCol = ACR_TwoICStyle.PanelBox(m_RootWidget, 0, 14, 6, m_HiddenHint, ACR_MapDeclutter.PanelColor());
		FrameSlot.SetAnchorMin(m_HiddenHint, 0.5, 1);
		FrameSlot.SetAnchorMax(m_HiddenHint, 0.5, 1);
		FrameSlot.SetAlignment(m_HiddenHint, 0.5, 1);
		FrameSlot.SetPos(m_HiddenHint, 0, -24);
		FrameSlot.SetSizeToContent(m_HiddenHint, true);
		m_HiddenHint.SetFlags(WidgetFlags.IGNORE_CURSOR);
		ACR_TwoICStyle.Text(hintCol, "UI hidden  ·  H to show", ACR_TwoICStyle.Grey(), 16);
		m_HiddenHint.SetVisible(false);

		WidgetManager.SetCursor(0);
	}

	protected TextWidget AddText(Widget parent, ResourceName layout, string text)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		TextWidget tw = TextWidget.Cast(ws.CreateWidgets(layout, parent));
		if (tw) tw.SetText(text);
		return tw;
	}

	// A button in the 2IC's outline look; small: one of the bottom row's eight.
	protected SCR_ButtonTextComponent AddButton(Widget parent, string text, int action, bool small = false)
	{
		float width = 150;
		if (small) width = 112;
		SCR_ButtonTextComponent btn = ACR_TwoICStyle.Button(parent, text, ACR_TwoICStyle.BUTTON_OUTLINE, width, 16, 38);
		if (btn)
		{
			btn.m_OnClicked.Insert(OnButtonClicked);
			m_Actions.Insert(btn, action);
		}
		return btn;
	}

	// An armed tool (Defend, Roadblock, a minefield, Fortify, Garrison, Get in) or Follow is lit in
	// the accent, the rest outlined; restyled only when it changes.
	protected void Lit(SCR_ButtonTextComponent btn, bool on, string text)
	{
		if (!btn) return;
		string key = text + "|off";
		if (on) key = text + "|on";
		string had;
		if (m_Lit.Find(btn, had) && had == key) return;
		m_Lit.Set(btn, key);
		int look = ACR_TwoICStyle.BUTTON_OUTLINE;
		if (on) look = ACR_TwoICStyle.BUTTON_FILL;
		ACR_TwoICStyle.Restyle(btn, look, 16);
		btn.SetText(text);
	}

	bool LitForTest(SCR_ButtonTextComponent btn)
	{
		string had;
		return btn && m_Lit.Find(btn, had) && had.EndsWith("|on");
	}

	protected void OnButtonClicked(SCR_ButtonBaseComponent btn)
	{
		if (!m_Actions.Contains(btn)) return;
		int action = m_Actions.Get(btn);
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());

		switch (action)
		{
			case 1: // Exit
				Close();
				return;
			case 12: // Back to the map
				ToMap();
				return;
			case 2: // Defend toggle
				m_iPointCommand = 0;
				m_bDefendMode = !m_bDefendMode;
				if (m_bDefendMode)
				{
					m_bRoadblockMode = false;
					m_iMineKind = -1;
					m_bFortifyMode = false;
				}
				if (m_bDefendMode) ShowToast("Defend mode armed: Right-click ground to place defence");
				else ShowToast("Defend mode off");
				return;
   case 3: // Garrison: choose the building in the scene.
    m_iPointCommand = ACR_ERequest.ORDER_GARRISON;
    m_bDefendMode = false;
    m_bRoadblockMode = false;
    m_iMineKind = -1;
    m_bFortifyMode = false;
    ShowToast("Right-click a building to garrison");
    return;
   case 4: // Get in: choose the vehicle in the scene.
    m_iPointCommand = ACR_ERequest.ORDER_GET_IN;
    m_bDefendMode = false;
    m_bRoadblockMode = false;
    m_iMineKind = -1;
    m_bFortifyMode = false;
    ShowToast("Right-click a vehicle to board");
    return;
			case 5: // Switch / Possess
				if (pc && pc.ACR_GetSelectedGroup().IsValid())
				{
					pc.ACR_Request(ACR_ERequest.SWITCH_TO_GROUP, pc.ACR_GetSelectedGroup());
					Close();
				}
				return;
			case 6: // Halt
				if (pc && pc.ACR_GetSelectedGroup().IsValid())
				{
					pc.ACR_Request(ACR_ERequest.ORDER_HALT, pc.ACR_GetSelectedGroup());
					ShowToast("Halt order dispatched");
				}
				return;
			case 7: // Roadblock toggle
				m_iPointCommand = 0;
				m_bRoadblockMode = !m_bRoadblockMode;
				if (m_bRoadblockMode)
				{
					m_bDefendMode = false;
					m_iMineKind = -1;
					m_bFortifyMode = false;
				}
				if (m_bRoadblockMode) ShowToast(string.Format("Roadblock mode armed: Right-click road to construct (%1 RP or %2 truck supplies)", ACR_Roadblocks.Cost(ACR_ERoadblockType.MEDIUM), ACR_Roadblocks.Supplies(ACR_ERoadblockType.MEDIUM)));
				else ShowToast("Roadblock mode off");
				return;
			case 8: // Minefield: off, AP, AT, mixed in turn (#4)
				m_iPointCommand = 0;
				m_bDefendMode = false;
				m_bRoadblockMode = false;
				m_bFortifyMode = false;
				m_iMineKind = NextMineKind(m_iMineKind);
				ShowToast(MineToast(m_iMineKind));
				return;
			case 9: // Fortify: dig in where right-clicked, or upgrade our position there
				m_iPointCommand = 0;
				m_bDefendMode = false;
				m_bRoadblockMode = false;
				m_iMineKind = -1;
				m_bFortifyMode = !m_bFortifyMode;
				ShowToast(FortifyToast(m_bFortifyMode));
				return;
			case 10: // Follow the selected squad, or let go
				if (Following()) StopFollow("Stopped following");
				else if (pc) StartFollow(pc);
				return;
			case 11: // Hide the UI (H shows it again)
				SetUiHidden(true);
				return;
		}
	}

	// ---------------------------------------------------------------- the Minefield button (#4)
	// Off, then AP, AT and mixed mines in turn; a right-click lays them with the selected squad
	// through the map panel's order path (ACR_IssueOrder, ACR_Minefields), at the same price.
	static int NextMineKind(int kind)
	{
		if (kind < 0) return ACR_EMineKind.AP;
		if (kind == ACR_EMineKind.AP) return ACR_EMineKind.AT;
		if (kind == ACR_EMineKind.AT) return ACR_EMineKind.MIXED;
		return -1;
	}

	static int MineCommand(int kind)
	{
		if (kind == ACR_EMineKind.AT) return ACR_ERequest.ORDER_MINES_AT;
		if (kind == ACR_EMineKind.MIXED) return ACR_ERequest.ORDER_MINES_MIXED;
		return ACR_ERequest.ORDER_MINES_AP;
	}

	static string MineButton(int kind)
	{
		if (kind < 0) return "Minefield";
		return ACR_Minefields.Button(kind) + " [ON]";
	}

	static string MineToast(int kind)
	{
		if (kind < 0) return "Minefield mode off";
		return string.Format("%1 armed: right-click ground to lay them (%2 RP); click Minefield again for the next kind", ACR_Minefields.Button(kind), ACR_Minefields.Cost(kind));
	}

	// ---------------------------------------------------------------- the Fortify button
	// A right-click digs the selected squad in there, or upgrades our position there, through the map
	// panel's order path (ACR_IssueOrder, ACR_Fortify plans the tier, the facing and the price).
	static int FortifyCommand()
	{
		return ACR_ERequest.ORDER_FORTIFY;
	}

	static string FortifyButton(bool on)
	{
		if (on) return "Fortify [ON]";
		return "Fortify";
	}

	static string FortifyToast(bool on)
	{
		if (!on) return "Fortify mode off";
		return string.Format("Fortify armed: right-click ground to dig in (tier I, %1 RP), or near one of our positions to upgrade it; right-drag to set the way it faces", ACR_Fortify.Cost(1));
	}

	// ---------------------------------------------------------------- screen raycasting
	static bool ScreenToWorld(int mouseX, int mouseY, out vector worldPos, out IEntity hitEntity)
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		if (!ws || !world) return false;

		vector outDir;
		vector startPos = ws.ProjScreenToWorld(ws.DPIUnscale(mouseX), ws.DPIUnscale(mouseY), outDir, world, -1);
		outDir *= 1600.0;

		TraceParam trace = new TraceParam();
		trace.Start = startPos;
		trace.End = startPos + outDir;
		trace.Flags = TraceFlags.WORLD | TraceFlags.OCEAN | TraceFlags.ENTS;
		trace.LayerMask = EPhysicsLayerPresets.Projectile;

		float fraction = world.TraceMove(trace, null);
		if (fraction >= 1.0) return false;

		worldPos = startPos + outDir * fraction;
		hitEntity = trace.TraceEnt;
		return true;
	}

	// ---------------------------------------------------------------- click handlers
	void HandleLeftClick(int x, int y)
	{
		vector hitPos;
		IEntity hitEnt;
		if (!ScreenToWorld(x, y, hitPos, hitEnt)) return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc) return;
		ACR_MapData data = pc.ACR_GetData();
		if (!data) return;

		// 0. Markers first: what the player sees is what they pick.
		WorkspaceWidget ws = GetGame().GetWorkspace();
		int markerSq = NearestMarker(m_SquadMarkerPos, ws.DPIUnscale(x), ws.DPIUnscale(y));
		if (markerSq >= 0 && markerSq < data.m_GroupIds.Count())
		{
			int now = System.GetTickCount();
			bool twice = markerSq == m_iLastMarkerClick && now - m_iLastMarkerClickAt < DOUBLE_CLICK_MS;
			m_iLastMarkerClick = markerSq;
			m_iLastMarkerClickAt = now;
			pc.ACR_SelectGroup(data.m_GroupIds[markerSq]);
			if (twice)
			{
				StartFollow(pc);
				return;
			}
			string markerName = "Squad";
			if (markerSq < data.m_GroupNames.Count()) markerName = data.m_GroupNames[markerSq];
			ShowToast("Selected: " + markerName);
			return;
		}
		int markerBase = NearestMarker(m_BaseMarkerPos, ws.DPIUnscale(x), ws.DPIUnscale(y));
		if (markerBase >= 0 && markerBase < data.m_BaseNames.Count())
		{
			pc.ACR_SelectBase(data.m_BaseNames[markerBase]);
			ShowToast("Selected Base: " + data.m_BaseNames[markerBase]);
			return;
		}

		// 1. Select squad near hit
		int bestSq = -1;
		float bestSqDist = 20.0;
		for (int s = 0; s < data.m_LeaderPositions.Count(); s++)
		{
			float d = vector.DistanceXZ(hitPos, data.LeaderPos(s));
			if (d < bestSqDist)
			{
				bestSqDist = d;
				bestSq = s;
			}
		}

		if (bestSq >= 0)
		{
			pc.ACR_SelectGroup(data.m_GroupIds[bestSq]);
			string sqName = "Squad";
			if (bestSq < data.m_GroupNames.Count()) sqName = data.m_GroupNames[bestSq];
			ShowToast("Selected: " + sqName);
			return;
		}

		// 2. Select base near hit
		int bestBase = -1;
		float bestBaseDist = 45.0;
		for (int b = 0; b < data.m_BasePositions.Count(); b++)
		{
			float rad = 35.0;
			if (b < data.m_BaseRadii.Count()) rad = Math.Max(rad, data.m_BaseRadii[b]);
			float d = vector.DistanceXZ(hitPos, data.m_BasePositions[b]);
			if (d < rad && d < bestBaseDist)
			{
				bestBaseDist = d;
				bestBase = b;
			}
		}

		if (bestBase >= 0)
		{
			pc.ACR_SelectBase(data.m_BaseNames[bestBase]);
			ShowToast("Selected Base: " + data.m_BaseNames[bestBase]);
			return;
		}

		// 3. Clicked empty terrain -> clear selection
		pc.ACR_SelectBase("");
		pc.ACR_SelectGroup(RplId.Invalid());
	}

	void HandleRightClick(int x, int y)
	{
		// A spotted enemy's marker under the cursor (it may stand against the sky).
		int hunted = -1;
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (m_Overlay) hunted = NearestMarker(m_Overlay.ContactScreen(), ws.DPIUnscale(x), ws.DPIUnscale(y));
		vector hitPos;
		IEntity hitEnt;
		bool onGround = ScreenToWorld(x, y, hitPos, hitEnt);
		OrderAt(hitPos, hitEnt, vector.Zero, onGround, hunted);
	}

	// The right-click's order at hitPos; aim: where a drag to aim came up (vector.Zero: none, the
	// order faces the threat as before). The facing goes with the order (ACR_IssueOrder).
	// hunted: the spotted enemy whose marker was clicked (-1 none); onGround false: the ray hit
	// nothing, so only that hunt can be ordered.
	void OrderAt(vector hitPos, IEntity hitEnt, vector aim, bool onGround = true, int hunted = -1)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc) return;
		// The markers are last frame's: an intel update since may have shortened the list.
		if (hunted >= pc.ACR_ContactCount()) hunted = -1;
		if (!onGround && hunted < 0) return;
		ACR_MapData data = pc.ACR_GetData();
		if (!data || data.m_Selected.IsEmpty())
		{
			ShowToast("Select a friendly squad first [Left-Click]");
			return;
		}

		bool append = pc.ACR_ModifierHeld("ACR_MapShift");
		int command = ACR_ERequest.MOVE_TO;
		string actionName = "Move";

		if (m_bFortifyMode)
		{
			command = FortifyCommand();
			actionName = "Fortify";
		}
		else if (m_iMineKind >= 0)
		{
			command = MineCommand(m_iMineKind);
			actionName = ACR_Minefields.Button(m_iMineKind);
		}
		else if (m_bRoadblockMode)
		{
			command = ACR_ERequest.ORDER_ROADBLOCK;
			actionName = "Roadblock";
		}
		else if (m_bDefendMode)
		{
			command = ACR_ERequest.ORDER_DEFEND;
			actionName = "Defend";
		}
  else if (m_iPointCommand != 0)
  {
   command = m_iPointCommand;
   actionName = "Garrison";
   if (command == ACR_ERequest.ORDER_GET_IN) actionName = "Get In";
  }
		// Right-click on a spotted enemy: hunt it down, as on the map.
		else if (hunted >= 0 && !append)
		{
			int kind, members, age;
			pc.ACR_GetContact(hunted, hitPos, kind, members, age);
			command = ACR_ERequest.ORDER_HUNT;
			actionName = "Hunt " + ACR_Intel.Describe(kind, members);
		}
		else if (hitEnt && Vehicle.Cast(hitEnt))
		{
			command = ACR_ERequest.ORDER_GET_IN;
			actionName = "Get In";
		}
		else if (hitEnt && SCR_DestructibleBuildingEntity.Cast(hitEnt))
		{
			command = ACR_ERequest.ORDER_GARRISON;
			actionName = "Garrison";
		}

		if (append)
		{
			if (command == ACR_ERequest.MOVE_TO) command = ACR_ERequest.QUEUE_MOVE;
			else if (command == ACR_ERequest.ORDER_DEFEND) command = ACR_ERequest.QUEUE_DEFEND;
			else if (command == ACR_ERequest.ORDER_GARRISON) command = ACR_ERequest.QUEUE_GARRISON;
			actionName = "Queued " + actionName;
		}
		if (!onGround && command != ACR_ERequest.ORDER_HUNT) return;

		if (pc.ACR_IssueOrder(command, hitPos, append, aim))
		{
			m_iPointCommand = 0;
			if (aim != vector.Zero) ShowToast(string.Format("%1 order requested, facing %2 degrees", actionName, (int)Bearing(hitPos, aim)));
			else ShowToast(actionName + " order requested");
		}
	}

	// ---------------------------------------------------------------- drag to aim
	// As on the map (#11): with Fortify, Roadblock or a minefield armed, a right-drag aims the
	// order instead of turning the camera. The order goes where the button went down and faces where
	// it came up, with an arrow on the ground while dragging; a plain right-click orders as before.
	bool AimArmed()
	{
		return m_bFortifyMode || m_bRoadblockMode || m_iMineKind >= 0; // not Defend: it takes no facing yet (#67 review)
	}

	// The gold arrow from where the button went down to the cursor (native pixels), drawn on the HUD
	// as the map draws its own (ACR_OrderDisplay.DrawAim), over a dark edge for bright ground. It was a
	// debug shape, which the game doesn't draw outside Workbench (pn12d: no arrow). Lines only, none
	// while the ends are under AIM_MIN_PX apart or the start is behind the camera.
	protected void AimArrow(int x, int y)
	{
		m_AimCommands.Clear();
		WorkspaceWidget ws = GetGame().GetWorkspace();
		vector from = ws.ProjWorldToScreenNative(m_vAimFrom + Vector(0, 0.5, 0), GetGame().GetWorld());
		float dx = x - from[0];
		float dy = y - from[1];
		if (from[2] > 0 && dx * dx + dy * dy >= AIM_MIN_PX * AIM_MIN_PX)
		{
			float width = ws.DPIScale(3);
			float head = ws.DPIScale(16);
			float angle = Math.Atan2(dy, dx);
			Color edge = new Color(0, 0, 0, 0.55);
			Color gold = new Color(1, 0.85, 0.2, 0.95);
			array<int> colors = {edge.PackToInt(), gold.PackToInt()};
			array<float> widths = {width + ws.DPIScale(2), width};
			foreach (int k, int color : colors)
			{
				AimLine(from[0], from[1], x, y, color, widths[k]);
				for (int side = -1; side <= 1; side += 2)
				{
					float back = angle + Math.PI + side * 0.5;
					AimLine(x, y, x + Math.Cos(back) * head, y + Math.Sin(back) * head, color, widths[k]);
				}
			}
		}
		if (m_AimCanvas) m_AimCanvas.SetDrawCommands(m_AimCommands);
	}

	protected void AimLine(float x0, float y0, float x1, float y1, int color, float width)
	{
		LineDrawCommand line = new LineDrawCommand();
		line.m_iColor = color;
		line.m_fWidth = width;
		line.m_Vertices = {x0, y0, x1, y1};
		m_AimCommands.Insert(line);
	}

	protected void ClearAim()
	{
		if (m_AimCommands.IsEmpty()) return;
		m_AimCommands.Clear();
		if (m_AimCanvas) m_AimCanvas.SetDrawCommands(m_AimCommands);
	}

	// pn12d: with Roadblock armed, deselecting the squad left the tool armed, so a right-drag aimed
	// instead of turning the camera until a squad was reselected and the tool turned off. Emptying the
	// selection or changing it disarms the tool; arming with nothing selected, then selecting, keeps it.
	protected void WatchSelection(SCR_PlayerController pc)
	{
		ACR_MapData data = pc.ACR_GetData();
		if (!data) return;
		bool changed = data.m_Selected.Count() != m_LastSelected.Count();
		foreach (RplId id : data.m_Selected) if (!m_LastSelected.Contains(id)) changed = true;
		bool wasEmpty = m_LastSelected.IsEmpty();
		m_LastSelected.Copy(data.m_Selected);
		if (!changed || wasEmpty || (!AimArmed() && m_iPointCommand == 0)) return;
		Disarm();
		if (data.m_Selected.IsEmpty()) ShowToast("No squad selected: tool off");
		else ShowToast("Selection changed: tool off");
	}

	// Every tool off (the buttons follow in UpdateTacticalUI); a drag under way stops aiming.
	void Disarm()
	{
		m_bDefendMode = false;
		m_bRoadblockMode = false;
		m_iMineKind = -1;
		m_bFortifyMode = false;
		m_iPointCommand = 0;
		m_bAiming = false;
		ClearAim();
	}

	static float Bearing(vector from, vector to)
	{
		float bearing = Math.Atan2(to[0] - from[0], to[2] - from[2]) * Math.RAD2DEG;
		if (bearing < 0) bearing += 360;
		return bearing;
	}

	protected void DrawAim(int x, int y)
	{
		vector to;
		IEntity toEnt;
		if (!ScreenToWorld(x, y, to, toEnt))
		{
			ClearAim();
			return;
		}
		AimArrow(x, y);
		if (m_bAimToastShown) return;
		m_bAimToastShown = true;
		ShowToast("Aiming: let go where it should face");
	}

 void BeginOrbit(int x, int y)
 {
  m_bOrbit = true;
  m_bDragged = false;
  m_iMouseX = x;
  m_iMouseY = y;
  m_iDownX = x;
  m_iDownY = y;
  m_bAimToastShown = false;
  ClearAim();
  m_bAiming = AimArmed() && ScreenToWorld(x, y, m_vAimFrom, m_AimFromEnt);
 }

 void EndOrbit(int x, int y, bool onScene)
 {
  if (!m_bOrbit) return;
  if (Math.AbsInt(x - m_iDownX) >= 8 || Math.AbsInt(y - m_iDownY) >= 8) m_bDragged = true;
  m_bOrbit = false;
  bool aiming = m_bAiming;
  m_bAiming = false;
  ClearAim();
  if (aiming && m_bDragged)
  {
   vector to;
   IEntity toEnt;
   if (ScreenToWorld(x, y, to, toEnt)) OrderAt(m_vAimFrom, m_AimFromEnt, to);
   return;
  }
  if (!m_bDragged && onScene) HandleRightClick(x, y);
 }

 protected void ApplyOrbit(int x, int y)
 {
  if (Math.AbsInt(x - m_iDownX) >= 8 || Math.AbsInt(y - m_iDownY) >= 8) m_bDragged = true;
  if (m_bDragged && m_bAiming) DrawAim(x, y);
  else if (m_bDragged && Following()) OrbitFollow(x - m_iMouseX, y - m_iMouseY);
  else if (m_bDragged)
  {
   vector angles = m_Camera.GetYawPitchRoll();
   angles[0] = angles[0] + (x - m_iMouseX) * 0.2;
   angles[1] = Math.Clamp(angles[1] - (y - m_iMouseY) * 0.2, -85, MaxPitch());
   m_Camera.SetYawPitchRoll(angles);
  }
  m_iMouseX = x;
  m_iMouseY = y;
 }

 // How far up the camera may look: past the horizon near the ground (to watch a squad at eye
 // height), at least 15 degrees down high up, as before.
 protected float MaxPitch()
 {
  vector pos = m_Camera.GetOrigin();
  float height = pos[1] - GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
  float t = Math.Clamp((height - LOW) / (HIGH - LOW), 0, 1);
  return Math.Lerp(10, -15, t);
 }

 // The top of what is under pos: the ground, or a roof, a tree top, a vehicle (traced down from
 // above), smoothed so the camera rises quickly onto a roof and settles slowly off it.
 protected float FloorAt(vector pos, float dt)
 {
  BaseWorld world = GetGame().GetWorld();
  float ground = world.GetSurfaceY(pos[0], pos[2]);
  float top = ground;
  TraceParam trace = new TraceParam();
  trace.Start = Vector(pos[0], Math.Max(pos[1], ground) + 60, pos[2]);
  trace.End = Vector(pos[0], ground - 0.5, pos[2]);
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  trace.LayerMask = EPhysicsLayerPresets.Projectile;
  float fraction = world.TraceMove(trace, FloorFilter);
  if (fraction < 1) top = Math.Max(ground, trace.Start[1] + (trace.End[1] - trace.Start[1]) * fraction);
  if (m_fFloorY < -99999 || Math.AbsFloat(m_fFloorY - top) > 80) m_fFloorY = top;
  else if (top > m_fFloorY) m_fFloorY = m_fFloorY + (top - m_fFloorY) * Math.Min(1.0, dt * 12.0);
  else m_fFloorY = m_fFloorY + (top - m_fFloorY) * Math.Min(1.0, dt * 3.0);
  return m_fFloorY;
 }

 // A soldier is no roof: the camera would bob over every man walking under it.
 protected bool FloorFilter(notnull IEntity ent)
 {
  return !ChimeraCharacter.Cast(ent);
 }

 protected void ApplyInput(InputManager input)
 {
  if (m_bOrbit)
  {
   int mouseX, mouseY;
   WidgetManager.GetMousePos(mouseX, mouseY);
   ApplyOrbit(mouseX, mouseY);
  }
  vector angles = m_Camera.GetYawPitchRoll();
  vector heading[4];
  Math3D.AnglesToMatrix(Vector(angles[0], 0, 0), heading);
  vector movement = heading[2] * (input.GetActionValue("ACR_RTSPanForward") - input.GetActionValue("ACR_RTSPanBack"));
  movement = movement + heading[0] * (input.GetActionValue("ACR_RTSPanRight") - input.GetActionValue("ACR_RTSPanLeft"));
  if (movement.LengthSq() > 1) movement.Normalize();
  float dt = Math.Min(input.GetLastTimeDelta(), 0.1);
  bool overScene = WidgetManager.GetWidgetUnderCursor() == m_InteractionLayer;
  // Following: the wheel moves in and out, the squad leads; panning lets go.
  if (Following())
  {
   if (movement.LengthSq() > 0.01) StopFollow("Stopped following");
   else
   {
    if (overScene) ApplyZoom(input.GetActionValue("MouseWheel"));
    FollowStep(SCR_PlayerController.Cast(GetGame().GetPlayerController()), dt);
    return;
   }
  }
  vector pos = m_Camera.GetOrigin();
  BaseWorld world = GetGame().GetWorld();
  float height = pos[1] - world.GetSurfaceY(pos[0], pos[2]);
  // Pan faster when high, so the ground moves across the screen at a similar rate; walking pace at
  // eye height.
  float speed = 35.0 * Math.Clamp(height / 35.0, 0.12, 3.0);
  if (input.GetActionValue("ACR_MapShift") > 0.5) speed = Math.Max(speed, Math.Min(speed * 3, 150.0));
  pos = pos + movement * speed * dt;
  if (overScene) ApplyZoom(input.GetActionValue("MouseWheel"));
  height = height + (m_fTargetAtl - height) * Math.Min(1.0, dt * 10.0);
  pos[1] = world.GetSurfaceY(pos[0], pos[2]) + Math.Clamp(height, ATL_MIN, ATL_MAX);
  // Never into a roof or a tree: over one, its top plus CLEARANCE.
  pos[1] = Math.Max(pos[1], FloorAt(pos, dt) + CLEARANCE);
  m_Camera.SetOrigin(pos);
 }

 // Raw wheel values are large (120 per notch on Windows; vanilla divides by 1000),
 // so count notches rather than metres; any nonzero value is at least one notch.
 // Following, a notch moves the camera in or out from the squad instead.
 protected void ApplyZoom(float wheel)
 {
  if (wheel == 0) return;
  float notches = Math.Clamp(Math.Round(Math.AbsFloat(wheel) / 120.0), 1, 4);
  if (wheel < 0) notches = -notches;
  if (Following())
  {
   m_fFollowLength = Math.Clamp(m_fFollowLength * Math.Pow(ATL_PER_NOTCH, notches), FOLLOW_MIN, FOLLOW_MAX);
   return;
  }
  m_fTargetAtl = Math.Clamp(m_fTargetAtl * Math.Pow(ATL_PER_NOTCH, notches), ATL_MIN, ATL_MAX);
 }

 // ---------------------------------------------------------------- Follow (F)
 // The camera glides after the selected squad's leader and keeps looking at it, from where it is
 // (or 40 m behind and 25 m up, when further than FOLLOW_MAX): a right-drag turns round the squad,
 // the wheel moves in and out, WASD or F lets go. A double-click on a squad's marker follows it too.
 // It follows the leader's own soldier where he is streamed: the map data's position is the
 // server's, which jumps once a second, and easing after that made the camera surge and slow behind
 // a squad walking steadily (playtest pn15). The camera keeps a fixed offset from one smoothed
 // point, so it never turns or sways on its own.
 bool Following() { return m_FollowGroup.IsValid(); }

 void StartFollow(SCR_PlayerController pc)
 {
  ACR_MapData data = pc.ACR_GetData();
  RplId id = pc.ACR_GetSelectedGroup();
  int g = -1;
  if (data && id.IsValid()) g = data.m_GroupIds.Find(id);
  if (g < 0)
  {
   ShowToast("Select a squad to follow [Left-click]");
   return;
  }
  bool seen;
  vector at = FollowTarget(data, g, seen);
  vector offset = m_Camera.GetOrigin() - at;
  vector glide = vector.Zero;
  if (offset.Length() > FOLLOW_MAX || offset.Length() < FOLLOW_MIN)
  {
   vector angles = m_Camera.GetYawPitchRoll();
   vector heading[4];
   Math3D.AnglesToMatrix(Vector(angles[0], 0, 0), heading);
   offset = Vector(0, 25, 0) - heading[2] * 40;
   glide = m_Camera.GetOrigin() - at - offset;
  }
  m_FollowGroup = id;
  m_vFollowOffset = offset;
  m_fFollowLength = offset.Length();
  m_vFollowGlide = glide;
  m_vFollowAt = at;
  string name = "the squad";
  if (g < data.m_GroupNames.Count()) name = data.m_GroupNames[g];
  ShowToast("Following " + name);
 }

 void StopFollow(string why)
 {
  if (!Following()) return;
  m_FollowGroup = RplId.Invalid();
  if (!why.IsEmpty()) ShowToast(why);
  // Height and zoom carry on from where the follow left the camera.
  vector pos = m_Camera.GetOrigin();
  m_fTargetAtl = Math.Clamp(pos[1] - GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]), ATL_MIN, ATL_MAX);
 }

 protected void FollowStep(SCR_PlayerController pc, float dt)
 {
  ACR_MapData data;
  if (pc) data = pc.ACR_GetData();
  int g = -1;
  if (data) g = data.m_GroupIds.Find(m_FollowGroup);
  if (g < 0)
  {
   StopFollow("The squad is gone: stopped following");
   return;
  }
  bool seen;
  vector target = FollowTarget(data, g, seen);
  float ease = FOLLOW_EASE_FAR;
  if (seen) ease = FOLLOW_EASE;
  m_vFollowAt = m_vFollowAt + (target - m_vFollowAt) * Eased(ease, dt);
  m_vFollowGlide = m_vFollowGlide * (1 - Eased(2.5, dt));
  float length = m_vFollowOffset.Length();
  if (length > 0.1) m_vFollowOffset = m_vFollowOffset * ((length + (m_fFollowLength - length) * Eased(6.0, dt)) / length);
  vector pos = m_vFollowAt + m_vFollowOffset + m_vFollowGlide;
  float floor = Math.Max(GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]) + ATL_MIN, FloorAt(pos, dt) + CLEARANCE);
  if (pos[1] < floor) pos[1] = floor;
  m_Camera.SetOrigin(pos);
  LookAt(m_vFollowAt);
 }

 // The share of the way to a target to cover this frame, whatever the frame rate.
 protected static float Eased(float rate, float dt)
 {
  return 1 - Math.Pow(2.71828, -rate * dt);
 }

 // Where to look: the leader's own soldier if he is streamed here and where the server says he is
 // (a mounted command unit's position is its vehicle's), else the server's position.
 protected vector FollowTarget(ACR_MapData data, int g, out bool seen)
 {
  vector snapshot = data.LeaderPos(g);
  seen = false;
  if (g < data.m_LeaderIds.Count())
  {
   IEntity leader = SCR_EntityHelper.RplIdToEntity(data.m_LeaderIds[g]);
   if (leader && vector.DistanceXZ(leader.GetOrigin(), snapshot) < FOLLOW_NEAR)
   {
    seen = true;
    return leader.GetOrigin() + Vector(0, 1.5, 0);
   }
  }
  return snapshot + Vector(0, 1.5, 0);
 }

 // Turns the camera to look at 'at': the yaw and pitch whose forward axis points there (the
 // engine's angle signs checked against AnglesToMatrix rather than assumed).
 protected void LookAt(vector at)
 {
  vector dir = at - m_Camera.GetOrigin();
  if (dir.LengthSq() < 0.01) return;
  dir.Normalize();
  float yaw = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
  float pitch = Math.Asin(Math.Clamp(dir[1], -1, 1)) * Math.RAD2DEG;
  vector best = Vector(yaw, pitch, 0);
  float bestDot = -2;
  array<vector> tries = {Vector(yaw, pitch, 0), Vector(-yaw, pitch, 0), Vector(yaw, -pitch, 0), Vector(-yaw, -pitch, 0)};
  foreach (vector angles : tries)
  {
   vector m[4];
   Math3D.AnglesToMatrix(angles, m);
   float dot = vector.Dot(m[2], dir);
   if (dot <= bestDot) continue;
   bestDot = dot;
   best = angles;
  }
  m_Camera.SetYawPitchRoll(best);
 }

 // A right-drag while following: round the squad (dx) and up or down over it (dy), 5-80 degrees.
 protected void OrbitFollow(int dx, int dy)
 {
  float length = m_vFollowOffset.Length();
  if (length < 0.1) return;
  float flat = Math.Sqrt(m_vFollowOffset[0] * m_vFollowOffset[0] + m_vFollowOffset[2] * m_vFollowOffset[2]);
  float around = Math.Atan2(m_vFollowOffset[0], m_vFollowOffset[2]) - dx * 0.3 * Math.DEG2RAD;
  float up = Math.Clamp(Math.Atan2(m_vFollowOffset[1], flat) + dy * 0.2 * Math.DEG2RAD, 5 * Math.DEG2RAD, 80 * Math.DEG2RAD);
  m_vFollowOffset = Vector(Math.Sin(around) * Math.Cos(up), Math.Sin(up), Math.Cos(around) * Math.Cos(up)) * length;
 }

 // ---------------------------------------------------------------- Hide UI (H)
 // The bars, the markers, the order lines and the toasts go; the scene stays clickable. A small
 // hint says how to bring them back, for a moment.
 void SetUiHidden(bool hidden)
 {
  m_bUiHidden = hidden;
  if (m_TopBar) m_TopBar.SetVisible(!hidden);
  if (m_BottomBar) m_BottomBar.SetVisible(!hidden);
  if (m_MarkerLayer) m_MarkerLayer.SetVisible(!hidden);
  if (m_ToastRoot && hidden) m_ToastRoot.SetVisible(false);
  if (m_HiddenHint) m_HiddenHint.SetVisible(hidden);
  m_fHintUntil = System.GetTickCount() + 3000;
 }

 bool UiHidden() { return m_bUiHidden; }

	// ---------------------------------------------------------------- frame updates
	protected void FrameUpdate(SCR_PlayerController pc)
	{
		if (!m_Camera) return;

		IEntity localBody = pc.GetControlledEntity();
		if (!localBody || localBody != m_Body)
		{
			Close();
			return;
		}

  ChimeraCharacter character = ChimeraCharacter.Cast(localBody);
  if (character && character.GetCharacterController().IsUnconscious()) { Close(); return; }
  // Game Master opened over it, or the match is over (end screen): the camera gives way. The pause
  // menu doesn't close it (resume comes back to it).
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (ACR_Manager.EditorOpenFor(pc.GetPlayerId()) || (mode && mode.GetState() == SCR_EGameModeState.POSTGAME)) { Close(); return; }
  ACR_MapHandoff.SetMenuControls(localBody, true);
  ACR_MapHandoff.ClearCharacterInput(localBody);

		// 1. Vitality check: commander damage/death transitions back immediately
		SCR_CharacterDamageManagerComponent dmg = SCR_CharacterDamageManagerComponent.Cast(localBody.FindComponent(SCR_CharacterDamageManagerComponent));
		if (dmg)
		{
			if (dmg.GetState() == EDamageState.DESTROYED || dmg.GetHealth() <= 0)
			{
				Close();
				return;
			}
			if (dmg.GetHealth() < m_fLastHealth - 0.5)
			{
				m_fLastHealth = dmg.GetHealth();
				ShowWarning("WARNING: Commander body is taking damage!");
			}
		}

  // Menus own their cursor; discard a drag interrupted by a dialog.
  if (GetGame().GetMenuManager().IsAnyMenuOpen() || GetGame().GetMenuManager().IsAnyDialogOpen())
  {
   m_bOrbit = false;
   return;
  }

		// 2. Escape or MenuBack exits camera
		InputManager input = GetGame().GetInputManager();
		m_bContextActivated = input.ActivateContext("ACR_RTSContext");
		if (input.GetActionTriggered("MenuBack") || input.GetActionTriggered("ACR_RTSCamera"))
		{
			Close();
			return;
		}
		if (input.GetActionTriggered("ACR_Map"))
		{
			ToMap();
			return;
		}
		if (input.GetActionTriggered("ACR_RTSHideUI")) SetUiHidden(!m_bUiHidden);
		if (input.GetActionTriggered("ACR_RTSFollow"))
		{
			if (Following()) StopFollow("Stopped following");
			else StartFollow(pc);
		}

		ApplyInput(input);
		vector trans[4];
		m_Camera.GetWorldTransform(trans);
		vector camPos = trans[3];
		BaseWorld world = GetGame().GetWorld();
		float surfaceY = world.GetSurfaceY(camPos[0], camPos[2]);

		// 3. Altitude clamp: [ATL_MIN, ATL_MAX] above the ground
		float atl = camPos[1] - surfaceY;
		bool modifiedPos = false;
		if (atl < ATL_MIN)
		{
			camPos[1] = surfaceY + ATL_MIN;
			modifiedPos = true;
		}
		else if (atl > ATL_MAX)
		{
			camPos[1] = surfaceY + ATL_MAX;
			modifiedPos = true;
		}

		// 4. 300m Proximity Leash
		if (!s_bAdminUnbound)
		{
			float minDisSq = vector.DistanceSqXZ(camPos, localBody.GetOrigin());
			vector closestAnchor = localBody.GetOrigin();

			ACR_MapData data = pc.ACR_GetData();
			if (data)
			{
				for (int i = 0; i < data.m_LeaderPositions.Count(); i++)
				{
					float d = vector.DistanceSqXZ(camPos, data.LeaderPos(i));
					if (d < minDisSq)
					{
						minDisSq = d;
						closestAnchor = data.LeaderPos(i);
					}
				}
				Faction myFaction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
				for (int b = 0; b < data.m_BasePositions.Count(); b++)
				{
					if (myFaction && b < data.m_BaseOwners.Count() && data.m_BaseOwners[b] == myFaction.GetFactionKey())
					{
						float d = vector.DistanceSqXZ(camPos, data.m_BasePositions[b]);
						if (d < minDisSq)
						{
							minDisSq = d;
							closestAnchor = data.m_BasePositions[b];
						}
					}
				}
			}

			float curDis = Math.Sqrt(minDisSq);
			if (curDis > 300.0)
			{
				vector dir = camPos - closestAnchor;
				dir[1] = 0;
				dir.Normalize();
				camPos[0] = closestAnchor[0] + dir[0] * 300.0;
				camPos[2] = closestAnchor[2] + dir[2] * 300.0;
				camPos[1] = world.GetSurfaceY(camPos[0], camPos[2]) + Math.Clamp(atl, ATL_MIN, ATL_MAX);
				curDis = 300.0;
				modifiedPos = true;
				ShowWarning("LEASH LIMIT: Tethered within 300m of friendly forces");
			}
			UpdateLeashUI(curDis);
		}
		else
		{
			UpdateLeashUI(-1);
		}

		if (modifiedPos)
		{
			trans[3] = camPos;
			m_Camera.SetWorldTransform(trans);
		}

		WatchSelection(pc);
		UpdateTacticalUI(pc);
		UpdateMarkers(pc);

		// Toast and hint timeouts
		if (m_ToastRoot && m_ToastRoot.IsVisible() && System.GetTickCount() > m_fToastUntil)
		{
			m_ToastRoot.SetVisible(false);
		}
		if (m_HiddenHint && m_HiddenHint.IsVisible() && System.GetTickCount() > m_fHintUntil)
		{
			m_HiddenHint.SetVisible(false);
		}

		// Clear warning after 4s
		if (m_WarningText && !m_WarningText.GetText().IsEmpty() && (System.GetTickCount() - m_fLastWarningAt > 4000))
		{
			m_WarningText.SetText("");
		}
	}

	static string OrderKindName(int kind)
	{
		switch (kind)
		{
			case ACR_EOrderKind.MOVE: return "Moving";
			case ACR_EOrderKind.DEFEND: return "Defending";
			case ACR_EOrderKind.GET_IN: return "Mounting Vehicle";
			case ACR_EOrderKind.GET_OUT: return "Dismounting";
			case ACR_EOrderKind.HUNT: return "Hunting Contact";
		}
		return "Moving";
	}

	protected void UpdateLeashUI(float curDis)
	{
		if (!m_LeashText) return;
		if (curDis < 0)
		{
			m_LeashText.SetText("Leash off (admin)");
			m_LeashText.SetColor(ACR_TwoICStyle.Blue());
			return;
		}
		if (curDis < 25) m_LeashText.SetText("Over our forces  ·  leash 300 m");
		else m_LeashText.SetText(string.Format("%1 m from our forces  ·  leash 300 m", Math.Round(curDis)));
		if (curDis > 270.0) m_LeashText.SetColor(ACR_TwoICStyle.Accent());
		else m_LeashText.SetColor(ACR_TwoICStyle.Body());
	}

	protected void UpdateTacticalUI(SCR_PlayerController pc)
	{
		Lit(m_DefendBtn, m_bDefendMode, "Defend");
		Lit(m_GarrisonBtn, m_iPointCommand == ACR_ERequest.ORDER_GARRISON, "Garrison");
		Lit(m_GetInBtn, m_iPointCommand == ACR_ERequest.ORDER_GET_IN, "Get in");
		Lit(m_RoadblockBtn, m_bRoadblockMode, "Roadblock");
		string mines = "Minefield";
		if (m_iMineKind >= 0) mines = ACR_Minefields.Button(m_iMineKind);
		Lit(m_MinesBtn, m_iMineKind >= 0, mines);
		Lit(m_FortifyBtn, m_bFortifyMode, "Fortify");
		Lit(m_FollowBtn, Following(), "Follow  [F]");
		if (!m_SquadText) return;
		ACR_MapData data = pc.ACR_GetData();
		if (!data) return;

		if (m_FollowText)
		{
			string follow = "";
			int fg = data.m_GroupIds.Find(m_FollowGroup);
			if (Following() && fg >= 0 && fg < data.m_GroupNames.Count()) follow = "Following " + data.m_GroupNames[fg] + "  ·  right-drag to turn round it  ·  F lets go";
			m_FollowText.SetVisible(!follow.IsEmpty());
			m_FollowText.SetText(follow);
		}
		if (m_WarningText) m_WarningText.SetVisible(!m_WarningText.GetText().IsEmpty());

		RplId selSquad = pc.ACR_GetSelectedGroup();
		if (selSquad.IsValid())
		{
			int sqIdx = data.m_GroupIds.Find(selSquad);
			string name = "Selected squad";
			if (sqIdx >= 0 && sqIdx < data.m_GroupNames.Count()) name = data.m_GroupNames[sqIdx];
			if (data.m_Selected.Count() > 1) name += string.Format("  +%1", data.m_Selected.Count() - 1);
			m_SquadText.SetText(name);
			m_SquadDetailText.SetText(SquadDetail(data, selSquad, sqIdx));
			if (m_UnitActionsRow) m_UnitActionsRow.SetVisible(true);
		}
		else if (!pc.ACR_SelectedBase().IsEmpty())
		{
			string bName = pc.ACR_SelectedBase();
			int bIdx = data.m_BaseNames.Find(bName);
			string detail = "Base";
			if (bIdx >= 0)
			{
				string owner = "Free";
				Faction mine = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
				if (bIdx < data.m_BaseOwners.Count() && !data.m_BaseOwners[bIdx].IsEmpty())
				{
					owner = "Enemy";
					if (mine && data.m_BaseOwners[bIdx] == mine.GetFactionKey()) owner = "Ours";
				}
				int sup = 0;
				// m_BaseSupply holds 3 + UPGRADE_COUNT numbers per base (ACR_MapFeed); stock first, -1 = no FOB.
				int supplyOffset = bIdx * (3 + ACR_FobSlots.UPGRADE_COUNT);
				if (supplyOffset < data.m_BaseSupply.Count()) sup = data.m_BaseSupply[supplyOffset];
				string supplies = sup.ToString();
				if (sup < 0) supplies = "none";
				detail = string.Format("%1  ·  supplies %2", owner, supplies);
			}
			m_SquadText.SetText(bName);
			m_SquadDetailText.SetText(detail);
			if (m_UnitActionsRow) m_UnitActionsRow.SetVisible(false);
		}
		else
		{
			m_SquadText.SetText("Tactical overview");
			m_SquadDetailText.SetText("Left-click a squad or a base to select it  ·  double-click a squad to follow it");
			if (m_UnitActionsRow) m_UnitActionsRow.SetVisible(false);
		}
	}

	// "Moving  ·  120 m away  ·  HP 80%  ·  ammo 35%".
	protected string SquadDetail(ACR_MapData data, RplId id, int g)
	{
		string detail = "Holding";
		int ordIdx = data.m_OrderGroups.Find(id);
		if (ordIdx >= 0 && ordIdx < data.m_OrderKinds.Count()) detail = OrderKindName(data.m_OrderKinds[ordIdx]);
		if (g >= 0 && m_Camera) detail += string.Format("  ·  %1 m away", Math.Round(vector.Distance(m_Camera.GetOrigin(), data.LeaderPos(g))));
		int s = data.m_StatusGroups.Find(id);
		if (s >= 0 && s * 6 + 1 < data.m_Status.Count())
		{
			if (data.m_Status[s * 6] >= 0) detail += string.Format("  ·  HP %1%%", data.m_Status[s * 6]);
			if (data.m_Status[s * 6 + 1] >= 0) detail += string.Format("  ·  ammo %1%%", data.m_Status[s * 6 + 1]);
		}
		return detail;
	}

	// ---------------------------------------------------------------- world markers
	protected static int NearestMarker(array<vector> positions, float x, float y)
	{
		int best = -1;
		float bestSq = MARKER_PICK_RADIUS * MARKER_PICK_RADIUS;
		for (int i = 0; i < positions.Count(); i++)
		{
			vector p = positions[i];
			if (p[2] <= 0) continue;
			float dx = p[0] - x;
			float dy = p[1] - y;
			float d = dx * dx + dy * dy;
			if (d < bestSq)
			{
				bestSq = d;
				best = i;
			}
		}
		return best;
	}

	protected Widget CreateMarker()
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		Widget root = ws.CreateWidget(WidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, m_MarkerLayer);
		FrameSlot.SetSize(root, 0, 0);
		Widget edge = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, ACR_TwoICStyle.Rgb(16, 19, 23, 0.9), 0, root);
		edge.SetName("Edge");
		Widget pin = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 1, root);
		pin.SetName("Pin");
		TextWidget label = TextWidget.Cast(ws.CreateWidgets(LAYOUT_TEXT, root));
		if (label)
		{
			label.SetName("Label");
			label.SetFlags(WidgetFlags.IGNORE_CURSOR);
			FrameSlot.SetAlignment(label, 0.5, 1);
			FrameSlot.SetPos(label, 0, -14);
			FrameSlot.SetSizeToContent(label, true);
			label.SetOutline(1);
			// One line ("Bravo 1  ·  49 m"): in a zero-size marker the body text wrapped every word.
			label.SetTextWrapping(false);
			label.SetExactFontSize(18);
		}
		return root;
	}

	// Places (or hides) one marker; returns its screen position, z <= 0 when hidden.
	protected vector PlaceMarker(array<Widget> pool, int index, vector worldPos, string text, Color color, bool selected)
	{
		while (pool.Count() <= index) pool.Insert(CreateMarker());
		Widget root = pool[index];
		WorkspaceWidget ws = GetGame().GetWorkspace();
		vector screen = ws.ProjWorldToScreen(worldPos, GetGame().GetWorld());
		root.SetVisible(screen[2] > 0);
		if (screen[2] <= 0) return screen;
		FrameSlot.SetPos(root, screen[0], screen[1]);
		float size = 11;
		if (selected) size = 15;
		Widget edge = root.FindAnyWidget("Edge");
		if (edge)
		{
			float outer = size + 4;
			if (selected) outer = size + 6;
			FrameSlot.SetPos(edge, -outer * 0.5, -outer * 0.5);
			FrameSlot.SetSize(edge, outer, outer);
			Color ring = ACR_TwoICStyle.Rgb(16, 19, 23, 0.9);
			if (selected) ring = ACR_TwoICStyle.White();
			edge.SetColor(ring);
		}
		Widget pin = root.FindAnyWidget("Pin");
		if (pin)
		{
			FrameSlot.SetPos(pin, -size * 0.5, -size * 0.5);
			FrameSlot.SetSize(pin, size, size);
			pin.SetColor(color);
		}
		TextWidget label = TextWidget.Cast(root.FindAnyWidget("Label"));
		if (label)
		{
			label.SetText(text);
			Color ink = ACR_TwoICStyle.White();
			if (selected) ink = color;
			label.SetColor(ink);
		}
		return screen;
	}

	protected void UpdateMarkers(SCR_PlayerController pc)
	{
		if (!m_MarkerLayer) return;
		m_SquadMarkerPos.Clear();
		m_BaseMarkerPos.Clear();
		ACR_MapData data = pc.ACR_GetData();
		int squads = 0;
		int bases = 0;
		if (data)
		{
			vector eye = m_Camera.GetOrigin();
			// The calm map's colours: ours blue, a fellow player's squad green, the selection in the accent.
			Color selectedColor = ACR_TwoICStyle.Accent();
			Color commandColor = ACR_MapDeclutter.AccentBlue();
			Color playerColor = ACR_TwoICStyle.Rgb(120, 200, 130);
			squads = Math.Min(data.m_LeaderPositions.Count(), data.m_GroupIds.Count());
			for (int i = 0; i < squads; i++)
			{
				vector pos = data.LeaderPos(i);
				bool selected = data.IsSelected(data.m_GroupIds[i]);
				Color color = playerColor;
				if (i < data.m_Commandable.Count() && data.m_Commandable[i] == 1) color = commandColor;
				if (selected) color = selectedColor;
				string name = "Squad";
				if (i < data.m_GroupNames.Count()) name = data.m_GroupNames[i];
				string text = string.Format("%1  ·  %2 m", name, Math.Round(vector.Distance(eye, pos)));
				m_SquadMarkerPos.Insert(PlaceMarker(m_SquadMarkers, i, pos + Vector(0, 2.5, 0), text, color, selected));
			}

			Faction myFaction = SCR_FactionManager.SGetPlayerFaction(pc.GetPlayerId());
			string selectedBase = pc.ACR_SelectedBase();
			bases = data.m_BasePositions.Count();
			for (int b = 0; b < bases; b++)
			{
				string baseName = "";
				if (b < data.m_BaseNames.Count()) baseName = data.m_BaseNames[b];
				Color baseColor = ACR_TwoICStyle.Rgb(240, 200, 70); // free
				if (b < data.m_BaseOwners.Count() && !data.m_BaseOwners[b].IsEmpty())
				{
					if (myFaction && data.m_BaseOwners[b] == myFaction.GetFactionKey()) baseColor = commandColor;
					else baseColor = ACR_TwoICStyle.Rgb(229, 62, 62);
				}
				bool baseSelected = !baseName.IsEmpty() && baseName == selectedBase;
				if (baseSelected) baseColor = selectedColor;
				m_BaseMarkerPos.Insert(PlaceMarker(m_BaseMarkers, b, data.m_BasePositions[b] + Vector(0, 6, 0), baseName, baseColor, baseSelected));
			}
		}
		for (int h = squads; h < m_SquadMarkers.Count(); h++) m_SquadMarkers[h].SetVisible(false);
		for (int k = bases; k < m_BaseMarkers.Count(); k++) m_BaseMarkers[k].SetVisible(false);
		if (m_Overlay) m_Overlay.Update(pc, data, m_Camera);
	}

	void ShowToast(string message)
	{
		if (!m_ToastText) return;
		m_ToastText.SetText(message);
		if (m_ToastRoot) m_ToastRoot.SetVisible(!m_bUiHidden);
		m_fToastUntil = System.GetTickCount() + 2500;
	}

	void ShowWarning(string warning)
	{
		if (!m_WarningText) return;
		m_WarningText.SetText(warning);
		m_fLastWarningAt = System.GetTickCount();
	}

	// ---------------------------------------------------------------- cleanup
	protected void Cleanup()
	{
		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras && cameras.CurrentCamera() == m_Camera)
		{
			cameras.SetCamera(m_PreviousCamera);
		}
		if (m_Camera)
		{
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

		if (m_RootWidget)
		{
			ACR_Hud.Remove(m_RootWidget);
			m_RootWidget = null;
		}
		m_MarkerLayer = null;
		m_Overlay = null;
		m_TopBar = null;
		m_ToastRoot = null;
		m_HiddenHint = null;
		m_Lit.Clear();
		m_FollowGroup = RplId.Invalid();
		m_bUiHidden = false;
		m_SquadMarkers.Clear();
		m_BaseMarkers.Clear();
		m_SquadMarkerPos.Clear();
		m_BaseMarkerPos.Clear();

  ACR_MapHandoff.ClearCharacterInput(m_Body);
  // Closed under a menu (the pause menu, the map, the end screen): the body stays still until it shuts.
  ACR_MapHandoff.SetMenuControls(m_Body, GetGame().GetMenuManager().IsAnyMenuOpen());
  m_Body = null;
  m_Handler = null;
  m_InteractionLayer = null;
  m_bOrbit = false;
 }
}
