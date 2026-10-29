enum ACR_EPanelAction
{
 TOGGLE_COMMAND,
 SWITCH,
 MODE_COMBAT,
 MODE_FAST,
 MODE_SILENT,
 DEFEND,
 GET_IN,
 GET_OUT,
 UNLOAD,
 REINFORCE,
 SET_LZ,
 TOGGLE_REQUISITION,
 TOGGLE_SUPPORTS,
 RIDE,
 TAKE_GUNNER,
 HALT,
 PING_LOOK,
 PING_ATTACK,
 PING_DEFEND,
 PING_SUPPORT,
 GARRISON,
 FIRE_AT_WILL,
 TOGGLE_REPORT,
 TOGGLE_RTS_CAMERA,
 FOB_SUPPLY,
 FOB_BUILD,
 FOB_AI,                  // "AI manages this FOB" (ACR_FobAI)
 SQUAD_AI,                // "AI commands this squad" (ACR_SquadAI)
 ESTABLISH_FOB,
 ROADBLOCK,
 MINES_AP,                // the minefield buttons
 MINES_AT,
 MINES_MIXED,
 DISARM_MINES,
 ROADBLOCK_S,
 ROADBLOCK_L,
 BARRICADE,
 FORTIFY,
 PATROL,
 QRF,
 DEBUG_TOGGLE = 50,
 DEBUG_THINK,             // the AC:A Think card (ACR_Think)
 DEBUG_ACTION = 60,       // + ACR_EDebug (below 100)
 REQUISITION_ENTRY = 100, // + entry index (below 200)
 SUPPORT_ENTRY = 200,     // + entry index
 FOB_UPGRADE = 300        // + slot index (below 350)
}

enum ACR_ECursorMode
{
 MOVE,
 DEFEND,
 GET_IN,
 LZ,
 RIDE,
 GARRISON,
 ESTABLISH_FOB,
 ROADBLOCK,   // medium checkpoint
 MINES_AP,    // MINES_AP..DISARM_MINES: the minefield orders (ACR_Minefields.KindOfCursor)
 MINES_AT,
 MINES_MIXED,
 DISARM_MINES,
 ROADBLOCK_S,
 ROADBLOCK_L,
 BARRICADE,
 FORTIFY,
 QRF
}

// Arma Commander side panel on the fullscreen map. Built from script out of the
// game's own widget-library text and button layouts, so it matches vanilla UI.
// Pure view: ACR_PlayerController decides what it shows and handles clicks.
class ACR_CommandPanel : Managed
{
 protected static const ResourceName LAYOUT_BUTTON = "{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout";
 protected static const ResourceName LAYOUT_TEXT = "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout";
 protected static const ResourceName LAYOUT_HEADING = "{0D7FF083FDE39877}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading4.layout";
 protected static const float WIDTH = 340;

 Widget m_Root;
 protected Widget m_FeedRoot;
 protected TextWidget m_FeedText;
 protected ref array<TextWidget> m_FeedLines = {};
 protected string m_sFeedShown = "-";
 protected Widget m_Content;
 protected TextWidget m_Status;
 protected TextWidget m_ReinforceFeedback;
 protected TextWidget m_Economy;
 protected TextWidget m_Roster;
 protected Widget m_PickerRoot;
 protected Widget m_SwitchWidget;
 protected Widget m_LastButtonWidget;
 protected SCR_ButtonTextComponent m_CommandButton;
 protected Widget m_GroupSection;
 protected TextWidget m_GroupName;
 protected TextWidget m_GroupDetail;
 protected SCR_ButtonTextComponent m_SwitchButton;
 protected Widget m_GunnerWidget;
 protected Widget m_OrderSection;
 protected ref array<SCR_ButtonTextComponent> m_ModeButtons = {};
 protected TextWidget m_Hint;
 protected SCR_ButtonTextComponent m_DefendButton;
 protected SCR_ButtonTextComponent m_GarrisonButton;
 protected SCR_ButtonTextComponent m_ReinforceButton;
 protected Widget m_FobSection;
 protected SCR_ButtonTextComponent m_FobSupplyButton;
 protected SCR_ButtonTextComponent m_FobAIButton;
 protected SCR_ButtonTextComponent m_SquadAIButton;
 protected Widget m_SquadAIWidget;
 protected SCR_ButtonTextComponent m_FobBuildButton;
 protected Widget m_FobUpgradesLayout;
 protected ref array<SCR_ButtonTextComponent> m_aFobUpgradeButtons = {};
 protected ref array<Widget> m_aFobUpgradeWidgets = {};
 protected SCR_ButtonTextComponent m_RequisitionButton;
 protected Widget m_RequisitionWidget;
 protected Widget m_RequisitionSection;
 protected Widget m_RequisitionBox;
 protected ref array<SCR_ButtonTextComponent> m_EntryButtons = {};
 protected SCR_ButtonTextComponent m_SupportButton;
 protected SCR_ButtonTextComponent m_ReportButton;
 protected SCR_ButtonTextComponent m_RTSCameraButton;
 protected TextWidget m_ReportText;
 protected Widget m_SupportWidget;
 protected Widget m_SupportSection;
 protected Widget m_SupportBox;
 protected ref array<SCR_ButtonTextComponent> m_SupportButtons = {};
 protected SCR_ButtonTextComponent m_GetInButton;
 protected SCR_ButtonTextComponent m_LZButton;
 protected SCR_ButtonTextComponent m_RideButton;
 protected SCR_ButtonTextComponent m_EstablishFobButton;
 protected Widget m_EstablishFobWidget;
 protected ref array<SCR_ButtonTextComponent> m_RoadblockButtons = {}; // by ACR_ERoadblockType
 protected SCR_ButtonTextComponent m_QrfButton;
 protected Widget m_RoadblockWidget;
 protected SCR_ButtonTextComponent m_PatrolButton;
 protected Widget m_MinesWidget;
 protected ref array<SCR_ButtonTextComponent> m_MineButtons = {};
 protected Widget m_RoadblockSizes;
 protected SCR_ButtonTextComponent m_FortifyButton;
 protected Widget m_FortifyWidget;
 protected ref map<SCR_ButtonBaseComponent, int> m_Actions = new map<SCR_ButtonBaseComponent, int>();
 protected SCR_PlayerController m_Owner;
 protected Widget m_DebugRoot;
 protected Widget m_DebugSection;
 protected TextWidget m_DebugStatus;
 protected SCR_ButtonTextComponent m_DebugToggle;
 protected SCR_ButtonTextComponent m_DebugAttack;
 protected SCR_ButtonTextComponent m_DebugCaptureOwn;
 protected SCR_ButtonTextComponent m_DebugCaptureEnemy;
 protected string m_sDebugShown = "-";

 void Create(Widget parent, SCR_PlayerController owner)
 {
  m_Owner = owner;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_Root = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_Root, 1, 0);
  FrameSlot.SetAnchorMax(m_Root, 1, 0);
  FrameSlot.SetAlignment(m_Root, 1, 0);
  FrameSlot.SetPos(m_Root, -24, 96);
  FrameSlot.SetSizeToContent(m_Root, true);
  m_Root.SetZOrder(1000);

  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, PanelBackground(0.86), 0, m_Root);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);

  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, m_Root));
  size.EnableWidthOverride(true);
  size.SetWidthOverride(PanelWidth());
  AlignableSlot.SetPadding(size, 14, 12, 14, 16);
  m_Content = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, size);

  AddHeading(m_Content, "AC: ACTUAL");
  m_Economy = AddText(m_Content, LAYOUT_TEXT, "");
  m_Status = AddText(m_Content, LAYOUT_TEXT, "");
  m_ReinforceFeedback = AddText(m_Content, LAYOUT_TEXT, "");
  if (m_ReinforceFeedback) m_ReinforceFeedback.SetVisible(false);
  m_Roster = AddText(m_Content, LAYOUT_TEXT, "");
  if (m_Roster) m_Roster.SetVisible(false);
  m_CommandButton = AddButton(m_Content, "Take command", ACR_EPanelAction.TOGGLE_COMMAND);
  m_RequisitionButton = AddButton(m_Content, "Requisition", ACR_EPanelAction.TOGGLE_REQUISITION);
  m_RequisitionWidget = m_LastButtonWidget;
  // Long buy lists scroll inside a fixed-height box.
  SizeLayoutWidget listBox = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_Content));
  listBox.EnableHeightOverride(true);
  listBox.SetHeightOverride(340);
  AlignableSlot.SetPadding(listBox, 0, 6, 0, 0);
  m_RequisitionBox = listBox;
  // Clip, or the buttons of a long list draw over the whole panel.
  Widget scroll = ws.CreateWidget(WidgetType.ScrollLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.CLIPCHILDREN, Color.FromInt(Color.WHITE), 0, listBox);
  m_RequisitionSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, scroll);

  m_SupportButton = AddButton(m_Content, "Supports", ACR_EPanelAction.TOGGLE_SUPPORTS);
  m_SupportWidget = m_LastButtonWidget;
  SizeLayoutWidget supportBox = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_Content));
  supportBox.EnableHeightOverride(true);
  supportBox.SetHeightOverride(300);
  AlignableSlot.SetPadding(supportBox, 0, 6, 0, 0);
  m_SupportBox = supportBox;
  Widget supportScroll = ws.CreateWidget(WidgetType.ScrollLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.CLIPCHILDREN, Color.FromInt(Color.WHITE), 0, supportBox);
  m_SupportSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, supportScroll);

  // After-action report (ACR_Report): everyone on the side can open it.
  m_ReportButton = AddButton(m_Content, "Report", ACR_EPanelAction.TOGGLE_REPORT);
  m_ReportText = AddText(m_Content, LAYOUT_TEXT, "");
  if (m_ReportText) { m_ReportText.SetVisible(false); AlignableSlot.SetPadding(m_ReportText, 0, 4, 0, 4); }
  m_RTSCameraButton = AddButton(m_Content, "3D Camera  [F10]", ACR_EPanelAction.TOGGLE_RTS_CAMERA);
  AddExtraRows(m_Content);

  m_GroupSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_Content);
  AlignableSlot.SetPadding(m_GroupSection, 0, 14, 0, 0);
  m_GroupName = AddText(m_GroupSection, LAYOUT_HEADING, "");
  m_GroupDetail = AddText(m_GroupSection, LAYOUT_TEXT, "");
  m_FobSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, 0, Color.FromInt(Color.WHITE), 0, m_GroupSection);
  m_FobSupplyButton = AddButton(m_FobSection, "Order 500 supplies - 20 RP / 60 s", ACR_EPanelAction.FOB_SUPPLY);
  m_FobBuildButton = AddButton(m_FobSection, "Build at FOB (stand inside its circle)", ACR_EPanelAction.FOB_BUILD);
  m_FobAIButton = AddButton(m_FobSection, "AI manages this FOB: off", ACR_EPanelAction.FOB_AI);
  m_FobUpgradesLayout = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, 0, Color.FromInt(Color.WHITE), 0, m_FobSection);
  AddText(m_FobUpgradesLayout, LAYOUT_TEXT, "FOB Upgrades");
  for (int s = 0; s < ACR_FobSlots.UPGRADE_COUNT; s++)
  {
   if (s == ACR_FobSlots.SLOT_DEFENCES) AddText(m_FobUpgradesLayout, LAYOUT_TEXT, "FOB Defences");
   SCR_ButtonTextComponent upgradeBtn = AddButton(m_FobUpgradesLayout, ACR_FobSlots.SlotName(s), ACR_EPanelAction.FOB_UPGRADE + s);
   m_aFobUpgradeButtons.Insert(upgradeBtn);
   m_aFobUpgradeWidgets.Insert(m_LastButtonWidget);
  }
  m_SwitchButton = AddButton(m_GroupSection, "Switch to squad", ACR_EPanelAction.SWITCH);
  m_SwitchWidget = m_LastButtonWidget;
  AddButton(m_GroupSection, "Take over gunner", ACR_EPanelAction.TAKE_GUNNER);
  m_GunnerWidget = m_LastButtonWidget;
  m_SquadAIButton = AddButton(m_GroupSection, "AI commands this squad: off", ACR_EPanelAction.SQUAD_AI);
  m_SquadAIWidget = m_LastButtonWidget;
  if (m_GunnerWidget) m_GunnerWidget.SetVisible(false);

  m_OrderSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_GroupSection);
  AlignableSlot.SetPadding(m_OrderSection, 0, 10, 0, 0);
  AddText(m_OrderSection, LAYOUT_TEXT, "Movement");
  Widget modes = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  m_ModeButtons.Insert(AddButton(modes, "Combat", ACR_EPanelAction.MODE_COMBAT, true));
  m_ModeButtons.Insert(AddButton(modes, "Fast", ACR_EPanelAction.MODE_FAST, true));
  m_ModeButtons.Insert(AddButton(modes, "Silent", ACR_EPanelAction.MODE_SILENT, true));
  AddText(m_OrderSection, LAYOUT_TEXT, "Orders");
  Widget orders1 = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  m_DefendButton = AddButton(orders1, "Defend", ACR_EPanelAction.DEFEND, true);
  m_GarrisonButton = AddButton(orders1, "Garrison", ACR_EPanelAction.GARRISON, true);
  m_GetInButton = AddButton(orders1, "Get in", ACR_EPanelAction.GET_IN, true);
  Widget orders2 = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  AddButton(orders2, "Get out", ACR_EPanelAction.GET_OUT, true);
  AddButton(orders2, "Unload", ACR_EPanelAction.UNLOAD, true);
  AddButton(orders2, "Halt", ACR_EPanelAction.HALT, true);
  Widget orders3 = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  m_RideButton = AddButton(orders3, "Ride", ACR_EPanelAction.RIDE, true);
  m_LZButton = AddButton(orders3, "Unload at LZ", ACR_EPanelAction.SET_LZ, true);
  AddButton(orders3, "Fire at will", ACR_EPanelAction.FIRE_AT_WILL, true);
  // Standing orders.
  Widget orders4 = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  m_PatrolButton = AddButton(orders4, "Patrol", ACR_EPanelAction.PATROL, true);
  m_QrfButton = AddButton(orders4, "QRF", ACR_EPanelAction.QRF, true);
  m_ReinforceButton = AddButton(m_OrderSection, "Reinforce", ACR_EPanelAction.REINFORCE);
  m_EstablishFobButton = AddButton(m_OrderSection, "Establish FOB", ACR_EPanelAction.ESTABLISH_FOB);
  m_EstablishFobWidget = m_LastButtonWidget;
  if (m_EstablishFobWidget) m_EstablishFobWidget.SetVisible(false);
  // Roadblocks: the three checkpoint sizes in a row, a barricade under them.
  m_RoadblockWidget = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  m_RoadblockSizes = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_RoadblockWidget);
  m_RoadblockButtons.Insert(AddButton(m_RoadblockSizes, ACR_Roadblocks.Button(ACR_ERoadblockType.SMALL), ACR_EPanelAction.ROADBLOCK_S, true));
  m_RoadblockButtons.Insert(AddButton(m_RoadblockSizes, ACR_Roadblocks.Button(ACR_ERoadblockType.MEDIUM), ACR_EPanelAction.ROADBLOCK, true));
  m_RoadblockButtons.Insert(AddButton(m_RoadblockSizes, ACR_Roadblocks.Button(ACR_ERoadblockType.LARGE), ACR_EPanelAction.ROADBLOCK_L, true));
  m_RoadblockButtons.Insert(AddButton(m_RoadblockWidget, ACR_Roadblocks.Button(ACR_ERoadblockType.BARRICADE), ACR_EPanelAction.BARRICADE));
  if (m_RoadblockWidget) m_RoadblockWidget.SetVisible(false);
  m_MinesWidget = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  Widget layMines = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_MinesWidget);
  m_MineButtons.Insert(AddButton(layMines, ACR_Minefields.Button(ACR_EMineKind.AP), ACR_EPanelAction.MINES_AP, true));
  m_MineButtons.Insert(AddButton(layMines, ACR_Minefields.Button(ACR_EMineKind.AT), ACR_EPanelAction.MINES_AT, true));
  m_MineButtons.Insert(AddButton(layMines, ACR_Minefields.Button(ACR_EMineKind.MIXED), ACR_EPanelAction.MINES_MIXED, true));
  m_MineButtons.Insert(AddButton(m_MinesWidget, ACR_Minefields.Button(ACR_EMineKind.DISARM), ACR_EPanelAction.DISARM_MINES));
  if (m_MinesWidget) m_MinesWidget.SetVisible(false);
  m_FortifyButton = AddButton(m_OrderSection, "Fortify", ACR_EPanelAction.FORTIFY);
  m_FortifyWidget = m_LastButtonWidget;
  if (m_FortifyWidget) m_FortifyWidget.SetVisible(false);
  m_Hint = AddText(m_GroupSection, LAYOUT_TEXT, "");
  if (m_Hint) AlignableSlot.SetPadding(m_Hint, 0, 8, 0, 6);
  CreateFeed(parent);
  CreatePicker(parent);
  CreateDebug(parent);
 }

 // Debug menu (host and admins): a small box at the bottom left, closed by default.
 protected void CreateDebug(Widget parent)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_DebugRoot = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, 0, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_DebugRoot, 0, 1);
  FrameSlot.SetAnchorMax(m_DebugRoot, 0, 1);
  FrameSlot.SetAlignment(m_DebugRoot, 0, 1);
  FrameSlot.SetPos(m_DebugRoot, 110, -96);
  FrameSlot.SetSizeToContent(m_DebugRoot, true);
  m_DebugRoot.SetZOrder(1000);
  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, PanelBackground(0.86), 0, m_DebugRoot);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, m_DebugRoot));
  size.EnableWidthOverride(true);
  size.SetWidthOverride(320);
  AlignableSlot.SetPadding(size, 8, 6, 8, 8);
  Widget column = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, size);
  m_DebugToggle = AddButton(column, "Debug", ACR_EPanelAction.DEBUG_TOGGLE);
  m_DebugSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, column);
  m_DebugStatus = AddText(m_DebugSection, LAYOUT_TEXT, "");
  Widget rp = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_DebugSection);
  AddButton(rp, "+500 RP us", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.RP_OWN, true);
  AddButton(rp, "+500 RP enemy", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.RP_ENEMY, true);
  Widget fob = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_DebugSection);
  AddButton(fob, "+500 FOB Supply", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.FOB_SUPPLY, true);
  AddButton(fob, "Finish Upgrade", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.FOB_FINISH, true);
  Widget squad = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_DebugSection);
  AddButton(squad, "Heal Squad", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.HEAL_SQUAD, true);
  AddButton(squad, "Kill Squad", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.KILL_SQUAD, true);
  AddButton(m_DebugSection, "Pause / resume enemy AI", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.PAUSE_ENEMY);
  m_DebugAttack = AddButton(m_DebugSection, "Enemy attacks now", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.ATTACK_NOW);
  Widget capture = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_DebugSection);
  m_DebugCaptureOwn = AddButton(capture, "Base -> us", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.CAPTURE_OWN, true);
  m_DebugCaptureEnemy = AddButton(capture, "Base -> enemy", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.CAPTURE_ENEMY, true);
  Widget misc = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_DebugSection);
  AddButton(misc, "Time +6h", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.TIME_STEP, true);
  AddButton(misc, "Reveal enemy", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.REVEAL, true);
  AddButton(misc, "Reset supports", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.RESET_SUPPORTS, true);
  AddButton(m_DebugSection, "AC:A Think: what the AI commanders think", ACR_EPanelAction.DEBUG_THINK);
  AddText(m_DebugSection, LAYOUT_TEXT, "Console: type /acr [cmd] in chat (/acr help)");
  m_DebugSection.SetVisible(false);
 }

 // allowed = host/admin; baseName = the base selected on the map ("" none).
 void SetDebug(bool allowed, bool open, string status, string baseName)
 {
  if (!m_DebugRoot) return;
  string key = string.Format("%1|%2|%3|%4", allowed, open, status, baseName);
  if (key == m_sDebugShown) return;
  m_sDebugShown = key;
  m_DebugRoot.SetVisible(allowed);
  if (!allowed) return;
  m_DebugSection.SetVisible(open);
  if (m_DebugToggle)
  {
   if (open) m_DebugToggle.SetText("Debug  [close]");
   else m_DebugToggle.SetText("Debug");
  }
  if (m_DebugStatus) m_DebugStatus.SetText(status);
  bool hasBase = !baseName.IsEmpty();
  if (m_DebugAttack)
  {
   if (hasBase) m_DebugAttack.SetText("Enemy attacks " + baseName + " now");
   else m_DebugAttack.SetText("Enemy attacks now (select a base to aim)");
  }
  if (m_DebugCaptureOwn)
  {
   m_DebugCaptureOwn.SetEnabled(hasBase, false);
   if (hasBase) m_DebugCaptureOwn.SetText(baseName + " -> us");
   else m_DebugCaptureOwn.SetText("Base -> us");
  }
  if (m_DebugCaptureEnemy)
  {
   m_DebugCaptureEnemy.SetEnabled(hasBase, false);
   if (hasBase) m_DebugCaptureEnemy.SetText(baseName + " -> enemy");
   else m_DebugCaptureEnemy.SetText("Base -> enemy");
  }
 }

 // The ping picker: four buttons that open at the cursor (Alt+right-click).
 protected void CreatePicker(Widget parent)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_PickerRoot = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, 0, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_PickerRoot, 0, 0);
  FrameSlot.SetAnchorMax(m_PickerRoot, 0, 0);
  FrameSlot.SetSizeToContent(m_PickerRoot, true);
  m_PickerRoot.SetZOrder(1001);
  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, PanelBackground(0.90), 0, m_PickerRoot);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, m_PickerRoot));
  size.EnableWidthOverride(true);
  size.SetWidthOverride(170);
  AlignableSlot.SetPadding(size, 6, 6, 6, 6);
  Widget column = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, size);
  AddButton(column, "1  Look here", ACR_EPanelAction.PING_LOOK);
  AddButton(column, "2  Attack this", ACR_EPanelAction.PING_ATTACK);
  AddButton(column, "3  Defend here", ACR_EPanelAction.PING_DEFEND);
  AddButton(column, "4  Support here", ACR_EPanelAction.PING_SUPPORT);
  m_PickerRoot.SetVisible(false);
 }

 void ShowPicker(bool show, float x, float y)
 {
  if (!m_PickerRoot) return;
  // x, y are the cursor in native pixels: the picker is placed in reference units
  // and kept on screen (it opened below the bottom edge near it, live 1 Oct).
  if (show)
  {
   WorkspaceWidget ws = GetGame().GetWorkspace();
   float ux = ws.DPIUnscale(x);
   float uy = ws.DPIUnscale(y);
   float screenW = ws.DPIUnscale(ws.GetWidth());
   float screenH = ws.DPIUnscale(ws.GetHeight());
   float w, h;
   m_PickerRoot.GetScreenSize(w, h);
   w = ws.DPIUnscale(w);
   h = ws.DPIUnscale(h);
   if (w < 1) w = 182;
   if (h < 1) h = 170;
   float px = ux + 12;
   float py = uy + 12;
   if (px + w > screenW - 8) px = ux - 12 - w;
   if (py + h > screenH - 8) py = uy - 12 - h;
   px = Math.Clamp(px, 8, Math.Max(8, screenW - w - 8));
   py = Math.Clamp(py, 8, Math.Max(8, screenH - h - 8));
   FrameSlot.SetPos(m_PickerRoot, px, py);
  }
  m_PickerRoot.SetVisible(show);
 }

 bool PickerShown() { return m_PickerRoot && m_PickerRoot.IsVisible(); }

 // Other commanders and what each last did (shared command, 2+ commanders).
 void SetRoster(string text)
 {
  if (!m_Roster) return;
  m_Roster.SetVisible(!text.IsEmpty());
  if (!text.IsEmpty()) m_Roster.SetText(text);
 }

 // Intel feed: a small box at the top left, newest sighting first. Clicks go
 // through it to the map.
 protected void CreateFeed(Widget parent)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_FeedRoot = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_FeedRoot, 0, 0);
  FrameSlot.SetAnchorMax(m_FeedRoot, 0, 0);
  FrameSlot.SetAlignment(m_FeedRoot, 0, 0);
  FrameSlot.SetPos(m_FeedRoot, 110, 96); // clear of the map toolbar on the left
  FrameSlot.SetSizeToContent(m_FeedRoot, true);
  m_FeedRoot.SetZOrder(1000);
  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, new Color(0.02, 0.13, 0.30, 0.80), 0, m_FeedRoot);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 1, m_FeedRoot));
  size.EnableWidthOverride(true);
  size.SetWidthOverride(380);
  AlignableSlot.SetPadding(size, 12, 8, 12, 10);
  Widget column = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, size);
  AddText(column, LAYOUT_HEADING, "INTEL");
  // One text widget per line: a single multi-line text doesn't grow the
  // box, so later lines ran off the bottom of the background.
  m_FeedText = AddText(column, LAYOUT_TEXT, "No contacts reported");
  m_FeedLines.Clear();
  for (int i = 0; i < ACR_Intel.FEED_LINES; i++)
  {
   TextWidget line = AddText(column, LAYOUT_TEXT, "");
   if (!line) continue;
   line.SetVisible(false);
   m_FeedLines.Insert(line);
  }
 }

 void SetFeed(array<string> lines)
 {
  if (!m_FeedText) return;
  string key = "";
  if (lines) foreach (string l : lines) key += l + "|";
  if (key == m_sFeedShown) return;
  m_sFeedShown = key;
  bool none = !lines || lines.IsEmpty();
  m_FeedText.SetVisible(none);
  foreach (int i, TextWidget line : m_FeedLines)
  {
   bool used = !none && i < lines.Count();
   line.SetVisible(used);
   if (used) line.SetText(lines[i]);
  }
 }

 // The intel feed's top (the 2IC card moves it down, ACR_TwoICView).
 void SetFeedTop(float y)
 {
  if (m_FeedRoot) FrameSlot.SetPos(m_FeedRoot, 110, y);
 }

 Widget DebugRoot() { return m_DebugRoot; }

 protected TextWidget AddText(Widget parent, ResourceName layout, string text)
 {
  TextWidget widget = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(layout, parent));
  if (widget) widget.SetText(text);
  StyleText(widget, layout == LAYOUT_HEADING);
  return widget;
 }

 // Look hooks: the calm map (ACR_MapDeclutterView) restyles the panel through these.
 protected Color PanelBackground(float alpha) { return new Color(0.02, 0.13, 0.30, alpha); }
 protected float PanelWidth() { return WIDTH; }
 protected TextWidget AddHeading(Widget parent, string text) { return AddText(parent, LAYOUT_HEADING, text); }
 protected void StyleText(TextWidget widget, bool heading) {}
 protected void AddExtraRows(Widget content) {}

 // Arma Commander blue instead of the vanilla orange.
 protected void StyleButton(SCR_ButtonTextComponent button, Widget widget)
 {
  Color normal = new Color(0.16, 0.42, 0.80, 0.40);
  button.SetBackgroundColors(normal, new Color(0.26, 0.56, 0.96, 0.70), new Color(0.30, 0.62, 1.0, 0.95), new Color(0.36, 0.68, 1.0, 1.0), new Color(0.30, 0.62, 1.0, 0.95));
  if (button.m_wBackground) button.m_wBackground.SetColor(normal);
  Widget border = widget.FindAnyWidget("Border");
  if (border) border.SetColor(new Color(0.45, 0.72, 1.0, 1.0));
 }

 protected SCR_ButtonTextComponent AddButton(Widget parent, string text, int action, bool fill = false)
 {
  Widget widget = GetGame().GetWorkspace().CreateWidgets(LAYOUT_BUTTON, parent);
  if (!widget) return null;
  m_LastButtonWidget = widget;
  AlignableSlot.SetPadding(widget, 2, 2, 2, 2);
  if (fill) LayoutSlot.SetSizeMode(widget, LayoutSizeMode.Fill);
  SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(widget);
  if (!button) return null;
  button.SetText(text);
  StyleButton(button, widget);
  button.m_OnClicked.Insert(OnButton);
  m_Actions.Insert(button, action);
  return button;
 }

 protected void OnButton(SCR_ButtonBaseComponent button)
 {
  int action;
  if (m_Owner && m_Actions.Find(button, action)) m_Owner.ACR_OnPanelAction(action);
 }

 // True if the widget is part of this panel (used to ignore map clicks on it).
 bool Contains(Widget widget)
 {
  while (widget)
  {
   if (widget == m_Root || (m_FeedRoot && widget == m_FeedRoot) || (m_PickerRoot && widget == m_PickerRoot) || (m_DebugRoot && widget == m_DebugRoot)) return true;
   widget = widget.GetParent();
  }
  return false;
 }

 void SetCommand(string status, bool isCommander, bool canTake)
 {
  if (m_Status) m_Status.SetText(status);
  if (!m_CommandButton) return;
  if (isCommander) m_CommandButton.SetText("Leave command");
  else if (canTake) m_CommandButton.SetText("Take command");
  else m_CommandButton.SetText("Cannot take command");
  m_CommandButton.SetEnabled(isCommander || canTake, false);
 }

 // name empty = nothing selected
 void SetGroup(string name, string detail, bool friendly, bool isCommander, int mode, int cursorMode, string hint)
 {
  if (m_FobSection) m_FobSection.SetVisible(false);
  if (!m_GroupSection) return;
  m_GroupSection.SetVisible(!name.IsEmpty());
  if (name.IsEmpty()) return;
  m_GroupName.SetText(name);
  m_GroupDetail.SetText(detail);
  if (m_SwitchWidget) m_SwitchWidget.SetVisible(true);
  if (m_SwitchButton) m_SwitchButton.SetEnabled(friendly, false);
  m_OrderSection.SetVisible(friendly && isCommander);
  foreach (int i, SCR_ButtonTextComponent button : m_ModeButtons)
  {
   if (!button) continue;
   string label = ACR_Squad.ModeName(i);
   if (i == mode) label = "[" + label + "]";
   button.SetText(label);
  }
  // Armed right-click order is shown in brackets, like the active movement mode.
  if (m_DefendButton)
  {
   if (cursorMode == ACR_ECursorMode.DEFEND) m_DefendButton.SetText("[Defend]");
   else m_DefendButton.SetText("Defend");
  }
  if (m_GarrisonButton)
  {
   if (cursorMode == ACR_ECursorMode.GARRISON) m_GarrisonButton.SetText("[Garrison]");
   else m_GarrisonButton.SetText("Garrison");
  }
  if (m_GetInButton)
  {
   if (cursorMode == ACR_ECursorMode.GET_IN) m_GetInButton.SetText("[Get in]");
   else m_GetInButton.SetText("Get in");
  }
  if (m_LZButton)
  {
   if (cursorMode == ACR_ECursorMode.LZ) m_LZButton.SetText("[Unload at LZ]");
   else m_LZButton.SetText("Unload at LZ");
  }
  if (m_RideButton)
  {
   if (cursorMode == ACR_ECursorMode.RIDE) m_RideButton.SetText("[Ride]");
   else m_RideButton.SetText("Ride");
  }
  if (m_EstablishFobButton)
  {
   if (cursorMode == ACR_ECursorMode.ESTABLISH_FOB) m_EstablishFobButton.SetText("[Establish FOB]");
   else m_EstablishFobButton.SetText("Establish FOB");
  }
  if (m_QrfButton)
  {
   if (cursorMode == ACR_ECursorMode.QRF) m_QrfButton.SetText("[QRF]");
   else m_QrfButton.SetText("QRF");
  }
  int armed = RoadblockOfMode(cursorMode);
  foreach (int type, SCR_ButtonTextComponent roadblockButton : m_RoadblockButtons)
  {
   if (!roadblockButton) continue;
   if (type == armed) roadblockButton.SetText("[" + ACR_Roadblocks.Button(type) + "]");
   else roadblockButton.SetText(ACR_Roadblocks.Button(type));
  }
  foreach (int mineKind, SCR_ButtonTextComponent mineButton : m_MineButtons)
  {
   if (!mineButton) continue;
   if (ACR_Minefields.KindOfCursor(cursorMode) == mineKind) mineButton.SetText("[" + ACR_Minefields.Button(mineKind) + "]");
   else mineButton.SetText(ACR_Minefields.Button(mineKind));
  }
  if (m_FortifyButton)
  {
   if (cursorMode == ACR_ECursorMode.FORTIFY) m_FortifyButton.SetText("[Fortify]");
   else m_FortifyButton.SetText("Fortify");
  }
  if (m_Hint) m_Hint.SetText(hint);
 }

 // Buy list: one button per entry, rebuilt when the entry count changes.
 // selected = entry armed for deployment (-1 none), open = list expanded.
 void SetReport(bool open, string text)
 {
  if (m_ReportButton)
  {
   if (open) m_ReportButton.SetText("Report  [close]");
   else m_ReportButton.SetText("Report");
  }
  if (!m_ReportText) return;
  m_ReportText.SetVisible(open);
  if (open) m_ReportText.SetText(text);
 }

 void SetRequisitions(bool visible, bool open, array<string> names, array<int> costs, array<int> remaining, int rp, int selected)
 {
  if (!m_RequisitionSection) return;
  if (m_RequisitionWidget) m_RequisitionWidget.SetVisible(visible);
  m_RequisitionSection.SetVisible(visible && open);
  if (m_RequisitionBox) m_RequisitionBox.SetVisible(visible && open);
  if (m_RequisitionButton)
  {
   if (open) m_RequisitionButton.SetText("Requisition  [close]");
   else m_RequisitionButton.SetText("Requisition");
  }
  if (!visible || !open) return;
  if (m_EntryButtons.Count() != names.Count())
  {
   Widget child = m_RequisitionSection.GetChildren();
   while (child)
   {
    Widget next = child.GetSibling();
    child.RemoveFromHierarchy();
    child = next;
   }
   foreach (SCR_ButtonTextComponent old : m_EntryButtons) if (old) m_Actions.Remove(old);
   m_EntryButtons.Clear();
   foreach (int i, string entryName : names)
    m_EntryButtons.Insert(AddButton(m_RequisitionSection, entryName, ACR_EPanelAction.REQUISITION_ENTRY + i));
  }
  foreach (int i, SCR_ButtonTextComponent button : m_EntryButtons)
  {
   if (!button) continue;
   string label = string.Format("%1   %2 RP   (%3 left)", names[i], costs[i], remaining[i]);
   if (ACR_Pricing.Unlimited(remaining[i])) label = string.Format("%1   %2 RP", names[i], costs[i]);
   if (i == selected) label = "[" + label + "]";
   button.SetText(label);
   button.SetEnabled(remaining[i] > 0 && costs[i] <= rp, false);
  }
 }

 // Support list: cost and cooldown per entry. selected = support armed for the next right-click.
 void SetSupports(bool visible, bool open, array<string> names, array<int> costs, array<int> cooldowns, int rp, int selected)
 {
  if (!m_SupportSection) return;
  if (m_SupportWidget) m_SupportWidget.SetVisible(visible && !names.IsEmpty());
  m_SupportSection.SetVisible(visible && open);
  if (m_SupportBox) m_SupportBox.SetVisible(visible && open);
  if (m_SupportButton)
  {
   if (open) m_SupportButton.SetText("Supports  [close]");
   else m_SupportButton.SetText("Supports");
  }
  if (!visible || !open) return;
  if (m_SupportButtons.Count() != names.Count())
  {
   Widget child = m_SupportSection.GetChildren();
   while (child)
   {
    Widget next = child.GetSibling();
    child.RemoveFromHierarchy();
    child = next;
   }
   foreach (SCR_ButtonTextComponent old : m_SupportButtons) if (old) m_Actions.Remove(old);
   m_SupportButtons.Clear();
   foreach (int i, string supportName : names)
    m_SupportButtons.Insert(AddButton(m_SupportSection, supportName, ACR_EPanelAction.SUPPORT_ENTRY + i));
  }
  foreach (int i, SCR_ButtonTextComponent button : m_SupportButtons)
  {
   if (!button) continue;
   string state = "ready";
   if (cooldowns[i] > 0) state = cooldowns[i].ToString() + " s";
   string label = string.Format("%1   %2 RP   (%3)", names[i], costs[i], state);
   if (i == selected) label = "[" + label + "]";
   button.SetText(label);
   button.SetEnabled(cooldowns[i] <= 0 && costs[i] <= rp, false);
  }
 }

 void SetGunner(bool visible)
 {
  if (m_GunnerWidget) m_GunnerWidget.SetVisible(visible);
 }

 // "AI commands this squad" (ACR_SquadAI): one friendly squad selected; on or off.
 void SetSquadAI(bool visible, bool on)
 {
  if (m_SquadAIWidget) m_SquadAIWidget.SetVisible(visible);
  if (!m_SquadAIButton) return;
  if (on) m_SquadAIButton.SetText("[AI commands this squad: on]");
  else m_SquadAIButton.SetText("AI commands this squad: off");
 }

 void SetReinforce(string text, bool enabled)
 {
  if (!m_ReinforceButton) return;
  m_ReinforceButton.SetText(text);
  m_ReinforceButton.SetEnabled(enabled, false);
 }

 // Owner-only server results stay on the map instead of relying on an in-world hint.
 void SetReinforceFeedback(string message)
 {
  if (!m_ReinforceFeedback) return;
  m_ReinforceFeedback.SetText(message);
  m_ReinforceFeedback.SetVisible(!message.IsEmpty());
 }

 void SetEconomy(string text)
 {
  if (m_Economy) m_Economy.SetText(text);
 }

 // A selected base: info only, no squad buttons.
 void SetBase(string name, string detail)
 {
  if (m_FobSection) m_FobSection.SetVisible(false);
  if (!m_GroupSection) return;
  m_GroupSection.SetVisible(true);
  m_GroupName.SetText(name);
  m_GroupDetail.SetText(detail);
  if (m_SwitchWidget) m_SwitchWidget.SetVisible(false);
  if (m_GunnerWidget) m_GunnerWidget.SetVisible(false);
  if (m_EstablishFobWidget) m_EstablishFobWidget.SetVisible(false);
  if (m_RoadblockWidget) m_RoadblockWidget.SetVisible(false);
  if (m_MinesWidget) m_MinesWidget.SetVisible(false);
  if (m_FortifyWidget) m_FortifyWidget.SetVisible(false);
  m_OrderSection.SetVisible(false);
  if (m_Hint) m_Hint.SetText("");
 }

 // "AI manages this FOB" (ACR_FobAI): on or off; only the commander can change it.
 void SetFobAI(bool canToggle, bool on)
 {
  if (!m_FobAIButton) return;
  m_FobAIButton.SetEnabled(canToggle, false);
  if (on) m_FobAIButton.SetText("[AI manages this FOB: on]");
  else m_FobAIButton.SetText("AI manages this FOB: off");
 }

 // Upgrade states: ACR_FobSlots.FeedState (decoded with FeedBuilding / FeedBuilt).
 void SetFob(bool visible, bool canOrder, bool canBuild, array<int> slotStates = null, int stock = 0, bool ready = false, string factionKey = "")
 {
  if (m_FobSection) m_FobSection.SetVisible(visible);
  if (m_FobSupplyButton) m_FobSupplyButton.SetEnabled(canOrder, false);
  if (m_FobBuildButton) m_FobBuildButton.SetEnabled(canBuild, false);
  if (!m_FobUpgradesLayout) return;
  if (!visible || !slotStates)
  {
   m_FobUpgradesLayout.SetVisible(false);
   return;
  }
  m_FobUpgradesLayout.SetVisible(true);

  bool anyBuilding = false;
  int tier;
  int pct;
  bool nextBlocked;
  foreach (int st : slotStates)
  {
   if (ACR_FobSlots.FeedBuilding(st, tier, pct))
   {
    anyBuilding = true;
    break;
   }
  }

  for (int i = 0; i < m_aFobUpgradeButtons.Count(); i++)
  {
   SCR_ButtonTextComponent btn = m_aFobUpgradeButtons[i];
   if (!btn) continue;
   if (i >= slotStates.Count() || slotStates[i] < 0)
   {
    btn.SetEnabled(false, false);
    continue;
   }
   int state = slotStates[i];
   string label;
   bool enabled = false;
   if (ACR_FobSlots.FeedBuilt(state, tier, nextBlocked))
   {
    if (tier >= ACR_FobSlots.MaxTier(i))
    {
     label = string.Format("%1 [Built]", ACR_FobSlots.SlotName(i, tier, factionKey));
    }
    else if (nextBlocked)
    {
     label = string.Format("Upgrade %1 [spot blocked]", ACR_FobSlots.SlotName(i, tier + 1, factionKey));
    }
    else
    {
     int upCost = ACR_FobSlots.SlotCost(i, tier + 1);
     label = string.Format("Upgrade %1 [%2 sup]", ACR_FobSlots.SlotName(i, tier + 1, factionKey), upCost);
     enabled = ready && !anyBuilding && stock >= upCost;
    }
   }
   else if (ACR_FobSlots.FeedBuilding(state, tier, pct))
   {
    label = string.Format("%1 (%2%%)", ACR_FobSlots.SlotName(i, tier, factionKey), pct);
   }
   else if (state == 1)
   {
    label = string.Format("%1 [spot blocked]", ACR_FobSlots.SlotName(i, 0, factionKey));
   }
   else
   {
    int cost = ACR_FobSlots.SlotCost(i, 0);
    label = string.Format("%1 [%2 sup]", ACR_FobSlots.SlotName(i, 0, factionKey), cost);
    enabled = ready && !anyBuilding && stock >= cost;
   }
   btn.SetText(label);
   btn.SetEnabled(enabled, false);
  }
 }

 void SetEstablishFob(bool visible)
 {
  if (m_EstablishFobWidget) m_EstablishFobWidget.SetVisible(visible);
 }

 // "[Patrol]" while the squad patrols.
 void SetPatrol(bool on)
 {
  if (!m_PatrolButton) return;
  if (on) m_PatrolButton.SetText("[Patrol]");
  else m_PatrolButton.SetText("Patrol");
 }

 // sizes false: only the barricade (FIA builds log barricades whatever the order).
 void SetRoadblock(bool visible, bool sizes = true)
 {
  if (m_RoadblockWidget) m_RoadblockWidget.SetVisible(visible);
  if (m_RoadblockSizes) m_RoadblockSizes.SetVisible(sizes);
 }

 // The roadblock a cursor mode arms (-1: none).
 static int RoadblockOfMode(int mode)
 {
  switch (mode)
  {
   case ACR_ECursorMode.ROADBLOCK: return ACR_ERoadblockType.MEDIUM;
   case ACR_ECursorMode.ROADBLOCK_S: return ACR_ERoadblockType.SMALL;
   case ACR_ECursorMode.ROADBLOCK_L: return ACR_ERoadblockType.LARGE;
   case ACR_ECursorMode.BARRICADE: return ACR_ERoadblockType.BARRICADE;
  }
  return -1;
 }

 void SetMines(bool visible)
 {
  if (m_MinesWidget) m_MinesWidget.SetVisible(visible);
 }

 void SetFortify(bool visible)
 {
  if (m_FortifyWidget) m_FortifyWidget.SetVisible(visible);
 }

 void Destroy()
 {
  if (m_Root) m_Root.RemoveFromHierarchy();
  m_Root = null;
  if (m_FeedRoot) m_FeedRoot.RemoveFromHierarchy();
  m_FeedRoot = null;
  if (m_PickerRoot) m_PickerRoot.RemoveFromHierarchy();
  m_PickerRoot = null;
  if (m_DebugRoot) m_DebugRoot.RemoveFromHierarchy();
  m_DebugRoot = null;
  m_FeedText = null;
  m_FeedLines.Clear();
  m_Actions.Clear();
  m_ModeButtons.Clear();
  m_EntryButtons.Clear();
 }
}

// Right-click on the map with a squad selected issues an order instead of
// opening the vanilla contextual radial menu.
modded class SCR_MapRadialUI
{
 override protected void OnInputMenuOpen(float value, EActionTrigger reason)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc && pc.ACR_HandleMapClick()) return;
  super.OnInputMenuOpen(value, reason);
 }

 // Drag to aim. The right button is two actions in the game's input config: a click is
 // MapContextualMenu (above: the order), a hold of 100 ms is MapPanDrag (map panning), so a
 // drag never makes a click. With an order that takes a facing armed, the hold aims instead
 // (SCR_MapCursorModule below doesn't pan then): it starts when the hold does and the order
 // goes out when the button comes up (ACR_AimDown / ACR_AimUp).
 override void OnMapOpen(MapConfiguration config)
 {
  super.OnMapOpen(config);
  GetGame().GetInputManager().AddActionListener("MapPanDrag", EActionTrigger.DOWN, ACR_OnHoldDown);
  GetGame().GetInputManager().AddActionListener("MapPanDrag", EActionTrigger.UP, ACR_OnHoldUp);
 }

 override void OnMapClose(MapConfiguration config)
 {
  GetGame().GetInputManager().RemoveActionListener("MapPanDrag", EActionTrigger.DOWN, ACR_OnHoldDown);
  GetGame().GetInputManager().RemoveActionListener("MapPanDrag", EActionTrigger.UP, ACR_OnHoldUp);
  super.OnMapClose(config);
 }

 protected void ACR_OnHoldDown(float value, EActionTrigger reason)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc) pc.ACR_AimDown();
 }

 protected void ACR_OnHoldUp(float value, EActionTrigger reason)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc) pc.ACR_AimUp();
 }
}

// Drag to aim: while an order that takes a facing is armed, holding the right button aims it
// (ACR_PlayerController.ACR_AimArmed) and doesn't pan the map. Any other time it pans as usual;
// the keyboard and the screen edges pan either way.
// (The container attribute must be repeated on a modded config class: without it the map
// configs no longer know the class, and the map opens with no cursor module: no pan, no zoom.)
[BaseContainerProps()]
modded class SCR_MapCursorModule
{
 override protected void OnInputPanDrag(float value, EActionTrigger reason)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc && pc.ACR_AimArmed()) return;
  super.OnInputPanDrag(value, reason);
 }
}
