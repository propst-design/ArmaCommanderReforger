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
 KEY_HINTS,
 BUILD_FOLD,
 UPGRADES_FOLD,
 MORE_FOLD,
 EVAC,                    // containment: set an evac point (ACR_Evac)
 TWOIC_EVAC,              // ... "I'm the 2IC" runs evacs or leaves them to the commander
 TRUCE_OFFER,             // TvTvZ truce (ACR_Truce): offer or accept
 TRUCE_REFUSE,            // ... refuse, withdraw or break
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
 QRF,
 EVAC
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
 protected static const float REQUISITION_HEIGHT = 340;
 protected static const float SUPPORT_HEIGHT = 300;
 protected static const float LIST_MIN_HEIGHT = 160; // an open list gives way to here before the panel scrolls
 protected static const float SCREEN_GAP = 4;        // what the panel keeps from the screen's top and bottom

 Widget m_Root;
 protected Widget m_FeedRoot;
 protected TextWidget m_FeedText;
 protected ref array<TextWidget> m_FeedLines = {};
 protected string m_sFeedShown = "-";
 protected Widget m_Content;
 protected SizeLayoutWidget m_BodyBox;
 protected ScrollLayoutWidget m_BodyScroll;
 protected SizeLayoutWidget m_BodyMark;
 protected SCR_ButtonTextComponent m_MoreButton;
 protected Widget m_MoreSection;
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
 protected SCR_ButtonTextComponent m_FobUpgradesFold;
 protected Widget m_FobUpgradesBox;
 protected ref array<SCR_ButtonTextComponent> m_aFobUpgradeButtons = {};
 protected ref array<Widget> m_aFobUpgradeWidgets = {};
 protected SCR_ButtonTextComponent m_RequisitionButton;
 protected Widget m_RequisitionWidget;
 protected Widget m_RequisitionSection;
 protected SizeLayoutWidget m_RequisitionBox;
 protected ref array<SCR_ButtonTextComponent> m_EntryButtons = {};
 protected SCR_ButtonTextComponent m_SupportButton;
 protected SCR_ButtonTextComponent m_ReportButton;
 protected SCR_ButtonTextComponent m_RTSCameraButton;
 protected TextWidget m_ReportText;
 protected Widget m_SupportWidget;
 protected Widget m_SupportSection;
 protected SizeLayoutWidget m_SupportBox;
 protected ref array<SCR_ButtonTextComponent> m_SupportButtons = {};
 protected SCR_ButtonTextComponent m_GetInButton;
 protected SCR_ButtonTextComponent m_LZButton;
 protected SCR_ButtonTextComponent m_RideButton;
 protected SCR_ButtonTextComponent m_EstablishFobButton;
 protected Widget m_EstablishFobWidget;
 protected ref array<SCR_ButtonTextComponent> m_RoadblockButtons = {}; // by ACR_ERoadblockType
 protected SCR_ButtonTextComponent m_QrfButton;
 protected SCR_ButtonTextComponent m_EvacButton;
 protected Widget m_EvacRow;
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
 protected SCR_ButtonTextComponent m_DebugRPOwn;
 protected SCR_ButtonTextComponent m_DebugRPEnemy;
 protected SCR_ButtonTextComponent m_DebugPause;
 protected SCR_ButtonTextComponent m_DebugReveal;
 protected SCR_ButtonTextComponent m_DebugFobSupply;
 protected SCR_ButtonTextComponent m_DebugFobFinish;
 protected TextWidget m_DebugSides;
 protected TextWidget m_DebugBaseHeader;
 protected string m_sDebugShown = "-";
 protected static const float NOTE_WIDTH = 340;
 protected Widget m_NoteRoot;
 protected TextWidget m_NoteText;
 protected string m_sNote;

 void Create(Widget parent, SCR_PlayerController owner)
 {
  m_Owner = owner;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_Root = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_Root, 1, 0);
  FrameSlot.SetAnchorMax(m_Root, 1, 0);
  FrameSlot.SetAlignment(m_Root, 1, 0);
  FrameSlot.SetSizeToContent(m_Root, true);
  m_Root.SetZOrder(1000);
  ACR_UiPrefs.Load();
  GetGame().GetInputManager().AddActionListener("MouseLeft", EActionTrigger.UP, OnMouseLeftUp);

  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH, PanelBackground(0.86), 0, m_Root);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);

  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, m_Root));
  size.EnableWidthOverride(true);
  size.SetWidthOverride(PanelWidth());
  AlignableSlot.SetPadding(size, 14, 12, 14, 16);
  Widget frame = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, size);
  AlignableSlot.SetHorizontalAlign(frame, LayoutHorizontalAlign.Stretch);

  // The head stays put; everything under it goes in the body, which scrolls if it can't fit (FitBody).
  AddHeading(frame, "AC: ACTUAL");
  m_Economy = AddText(frame, LAYOUT_TEXT, "");
  m_Status = AddText(frame, LAYOUT_TEXT, "");
  m_ReinforceFeedback = AddText(frame, LAYOUT_TEXT, "");
  if (m_ReinforceFeedback) m_ReinforceFeedback.SetVisible(false);
  m_Roster = AddText(frame, LAYOUT_TEXT, "");
  if (m_Roster) m_Roster.SetVisible(false);
  CreateBody(frame);
  m_CommandButton = AddButton(m_Content, "Take command", ACR_EPanelAction.TOGGLE_COMMAND);
  m_RequisitionButton = AddButton(m_Content, "Requisition", ACR_EPanelAction.TOGGLE_REQUISITION);
  m_RequisitionWidget = m_LastButtonWidget;
  // Long buy lists scroll inside a fixed-height box.
  SizeLayoutWidget listBox = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_Content));
  listBox.EnableHeightOverride(true);
  listBox.SetHeightOverride(REQUISITION_HEIGHT);
  AlignableSlot.SetPadding(listBox, 0, 6, 0, 0);
  m_RequisitionBox = listBox;
  // Clip, or the buttons of a long list draw over the whole panel.
  Widget scroll = ws.CreateWidget(WidgetType.ScrollLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.CLIPCHILDREN, Color.FromInt(Color.WHITE), 0, listBox);
  m_RequisitionSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, scroll);

  m_SupportButton = AddButton(m_Content, "Supports", ACR_EPanelAction.TOGGLE_SUPPORTS);
  m_SupportWidget = m_LastButtonWidget;
  SizeLayoutWidget supportBox = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_Content));
  supportBox.EnableHeightOverride(true);
  supportBox.SetHeightOverride(SUPPORT_HEIGHT);
  AlignableSlot.SetPadding(supportBox, 0, 6, 0, 0);
  m_SupportBox = supportBox;
  Widget supportScroll = ws.CreateWidget(WidgetType.ScrollLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.CLIPCHILDREN, Color.FromInt(Color.WHITE), 0, supportBox);
  m_SupportSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, supportScroll);

  // What isn't needed to run the fight folds away under one button.
  m_MoreButton = AddButton(m_Content, "", ACR_EPanelAction.MORE_FOLD);
  m_MoreSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_Content);
  // After-action report (ACR_Report): everyone on the side can open it.
  m_ReportButton = AddButton(m_MoreSection, "Report", ACR_EPanelAction.TOGGLE_REPORT);
  m_ReportText = AddText(m_MoreSection, LAYOUT_TEXT, "");
  if (m_ReportText) { m_ReportText.SetVisible(false); AlignableSlot.SetPadding(m_ReportText, 0, 4, 0, 4); }
  m_RTSCameraButton = AddButton(m_MoreSection, "3D Camera  [F10]", ACR_EPanelAction.TOGGLE_RTS_CAMERA);
  m_KeyHintsButton = AddButton(m_MoreSection, "", ACR_EPanelAction.KEY_HINTS);
  // Zombie containment under "I'm the 2IC" (#107): the server answers whether it applies.
  AddButton(m_MoreSection, "2IC runs evacs: on/off", ACR_EPanelAction.TWOIC_EVAC);
  // TvTvZ truce (#107): the server answers whether one can be had.
  AddButton(m_MoreSection, "Truce: offer / accept", ACR_EPanelAction.TRUCE_OFFER);
  AddButton(m_MoreSection, "Truce: refuse / break", ACR_EPanelAction.TRUCE_REFUSE);
  RefreshKeyHints();
  AddExtraRows(m_MoreSection);
  RefreshMore();

  m_GroupSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_Content);
  AlignableSlot.SetPadding(m_GroupSection, 0, 14, 0, 0);
  m_GroupName = AddText(m_GroupSection, LAYOUT_HEADING, "");
  m_GroupDetail = AddText(m_GroupSection, LAYOUT_TEXT, "");
  m_FobSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, 0, Color.FromInt(Color.WHITE), 0, m_GroupSection);
  m_FobSupplyButton = AddButton(m_FobSection, "Order 500 supplies - 20 RP / 60 s", ACR_EPanelAction.FOB_SUPPLY);
  m_FobBuildButton = AddButton(m_FobSection, "Build at FOB (stand inside its circle)", ACR_EPanelAction.FOB_BUILD);
  m_FobAIButton = AddButton(m_FobSection, "AI manages this FOB: off", ACR_EPanelAction.FOB_AI);
  m_FobUpgradesLayout = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, 0, Color.FromInt(Color.WHITE), 0, m_FobSection);
  // The eight upgrades fold away (all open the panel ran off the screen, a playtester's report). The
  // defences are one button that says what it is, so they stay out.
  m_FobUpgradesFold = AddButton(m_FobUpgradesLayout, "FOB Upgrades", ACR_EPanelAction.UPGRADES_FOLD);
  m_FobUpgradesBox = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_FobUpgradesLayout);
  for (int s = 0; s < ACR_FobSlots.UPGRADE_COUNT; s++)
  {
   Widget slotParent = m_FobUpgradesBox;
   if (s == ACR_FobSlots.SLOT_DEFENCES) slotParent = m_FobUpgradesLayout;
   SCR_ButtonTextComponent upgradeBtn = AddButton(slotParent, ACR_FobSlots.SlotName(s), ACR_EPanelAction.FOB_UPGRADE + s);
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
  // Containment only (#107): the evac row.
  m_EvacRow = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  m_EvacButton = AddButton(m_EvacRow, "Evac point", ACR_EPanelAction.EVAC, true);
  if (m_EvacRow) m_EvacRow.SetVisible(false);
  m_ReinforceButton = AddButton(m_OrderSection, "Reinforce", ACR_EPanelAction.REINFORCE);
  // What a squad builds folds away: with it open and a squad selected the panel took most of the
  // screen and covered the game's hints (a player's report).
  m_BuildFoldButton = AddButton(m_OrderSection, "", ACR_EPanelAction.BUILD_FOLD);
  m_BuildFoldWidget = m_LastButtonWidget;
  m_BuildSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_OrderSection);
  m_EstablishFobButton = AddButton(m_BuildSection, "Establish FOB", ACR_EPanelAction.ESTABLISH_FOB);
  m_EstablishFobWidget = m_LastButtonWidget;
  if (m_EstablishFobWidget) m_EstablishFobWidget.SetVisible(false);
  // Roadblocks: the three checkpoint sizes in a row, a barricade under them.
  m_RoadblockWidget = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_BuildSection);
  m_RoadblockSizes = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_RoadblockWidget);
  m_RoadblockButtons.Insert(AddButton(m_RoadblockSizes, ACR_Roadblocks.Button(ACR_ERoadblockType.SMALL), ACR_EPanelAction.ROADBLOCK_S, true));
  m_RoadblockButtons.Insert(AddButton(m_RoadblockSizes, ACR_Roadblocks.Button(ACR_ERoadblockType.MEDIUM), ACR_EPanelAction.ROADBLOCK, true));
  m_RoadblockButtons.Insert(AddButton(m_RoadblockSizes, ACR_Roadblocks.Button(ACR_ERoadblockType.LARGE), ACR_EPanelAction.ROADBLOCK_L, true));
  m_RoadblockButtons.Insert(AddButton(m_RoadblockWidget, ACR_Roadblocks.Button(ACR_ERoadblockType.BARRICADE), ACR_EPanelAction.BARRICADE));
  if (m_RoadblockWidget) m_RoadblockWidget.SetVisible(false);
  m_MinesWidget = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_BuildSection);
  Widget layMines = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_MinesWidget);
  m_MineButtons.Insert(AddButton(layMines, ACR_Minefields.Button(ACR_EMineKind.AP), ACR_EPanelAction.MINES_AP, true));
  m_MineButtons.Insert(AddButton(layMines, ACR_Minefields.Button(ACR_EMineKind.AT), ACR_EPanelAction.MINES_AT, true));
  m_MineButtons.Insert(AddButton(layMines, ACR_Minefields.Button(ACR_EMineKind.MIXED), ACR_EPanelAction.MINES_MIXED, true));
  m_MineButtons.Insert(AddButton(m_MinesWidget, ACR_Minefields.Button(ACR_EMineKind.DISARM), ACR_EPanelAction.DISARM_MINES));
  if (m_MinesWidget) m_MinesWidget.SetVisible(false);
  m_FortifyButton = AddButton(m_BuildSection, "Fortify", ACR_EPanelAction.FORTIFY);
  m_FortifyWidget = m_LastButtonWidget;
  if (m_FortifyWidget) m_FortifyWidget.SetVisible(false);
  m_Hint = AddText(m_GroupSection, LAYOUT_TEXT, "");
  if (m_Hint) AlignableSlot.SetPadding(m_Hint, 0, 8, 0, 6);
  CreateFeed(parent);
  CreatePicker(parent);
  CreateDebug(parent);
  CreateNote(parent);
  RegisterPanels();
 }

 // The panel's rows below its head (m_Content): in a box as tall as they are, as far as the screen lets
 // it. A mark on the panel's edge shows how far down it is when it scrolls.
 protected void CreateBody(Widget parent)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_BodyBox = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent));
  m_BodyBox.EnableHeightOverride(true);
  m_BodyBox.SetHeightOverride(400);
  AlignableSlot.SetHorizontalAlign(m_BodyBox, LayoutHorizontalAlign.Stretch);
  m_BodyScroll = ScrollLayoutWidget.Cast(ws.CreateWidget(WidgetType.ScrollLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.CLIPCHILDREN, Color.FromInt(Color.WHITE), 0, m_BodyBox));
  AlignableSlot.SetHorizontalAlign(m_BodyScroll, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(m_BodyScroll, LayoutVerticalAlign.Stretch);
  // A scroll layout gives its child any width, and centres a short one.
  SizeLayoutWidget width = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_BodyScroll));
  width.EnableWidthOverride(true);
  width.SetWidthOverride(PanelWidth());
  AlignableSlot.SetVerticalAlign(width, LayoutVerticalAlign.Top);
  m_Content = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, width);
  AlignableSlot.SetHorizontalAlign(m_Content, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(m_Content, LayoutVerticalAlign.Top);

  m_BodyMark = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 6, m_Root));
  m_BodyMark.EnableWidthOverride(true);
  m_BodyMark.SetWidthOverride(4);
  m_BodyMark.EnableHeightOverride(true);
  AlignableSlot.SetHorizontalAlign(m_BodyMark, LayoutHorizontalAlign.Right);
  AlignableSlot.SetVerticalAlign(m_BodyMark, LayoutVerticalAlign.Top);
  Widget mark = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, new Color(1, 1, 1, 0.5), 0, m_BodyMark);
  AlignableSlot.SetHorizontalAlign(mark, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(mark, LayoutVerticalAlign.Stretch);
 }

 // Every frame: the panel is never taller than the screen. A list that's open gives way first, as it
 // scrolls by itself; then the body scrolls.
 protected void FitBody()
 {
  if (!m_BodyBox || !m_Root || !m_Root.IsVisible()) return;
  RefreshMore();
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float width, rootHeight, boxHeight, content;
  m_Root.GetScreenSize(width, rootHeight);
  m_BodyBox.GetScreenSize(width, boxHeight);
  m_Content.GetScreenSize(width, content);
  // Not laid out yet.
  if (content <= 0) return;
  rootHeight = ws.DPIUnscale(rootHeight);
  boxHeight = ws.DPIUnscale(boxHeight);
  content = ws.DPIUnscale(content);
  float room = Math.Max(ws.DPIUnscale(ws.GetHeight()) - 2 * SCREEN_GAP - (rootHeight - boxHeight), LIST_MIN_HEIGHT);
  content = FitList(m_RequisitionBox, REQUISITION_HEIGHT, content, room);
  content = FitList(m_SupportBox, SUPPORT_HEIGHT, content, room);
  float height = Math.Min(content, room);
  if (Math.AbsFloat(height - boxHeight) >= 1) m_BodyBox.SetHeightOverride(height);
  PlaceBodyMark(height, content);
 }

 // How tall the rows are with this list's box at its height for this much room.
 protected float FitList(SizeLayoutWidget box, float full, float content, float room)
 {
  if (!box || !box.IsVisibleInHierarchy()) return content;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float width, now;
  box.GetScreenSize(width, now);
  now = ws.DPIUnscale(now);
  float wanted = Math.Clamp(room - (content - now), LIST_MIN_HEIGHT, full);
  if (Math.AbsFloat(wanted - now) >= 1) box.SetHeightOverride(wanted);
  return content - now + wanted;
 }

 protected void PlaceBodyMark(float height, float content)
 {
  if (!m_BodyMark || !m_BodyScroll) return;
  bool scrolls = content > height + 1;
  m_BodyMark.SetVisible(scrolls);
  if (!scrolls) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float rootX, rootY, boxX, boxY, sliderX, sliderY;
  m_Root.GetScreenPos(rootX, rootY);
  m_BodyBox.GetScreenPos(boxX, boxY);
  m_BodyScroll.GetSliderPos(sliderX, sliderY);
  float markHeight = Math.Max(height * height / content, 24);
  m_BodyMark.SetHeightOverride(markHeight);
  AlignableSlot.SetPadding(m_BodyMark, 0, ws.DPIUnscale(boxY - rootY) + sliderY * (height - markHeight), 5, 0);
 }

 protected void RefreshMore()
 {
  if (m_MoreSection) m_MoreSection.SetVisible(ACR_UiPrefs.s_bMoreOpen);
  if (!m_MoreButton) return;
  if (ACR_UiPrefs.s_bMoreOpen) m_MoreButton.SetText("More  [hide]");
  else m_MoreButton.SetText("More");
 }

 // What an armed order does, in a box of its own beside the panel. At the panel's foot it ran off the
 // screen with the build orders open (their rows add up to about 1150 px of a 1080 px screen), just
 // when the player has asked for it. Clicks go through it to the map.
 protected void CreateNote(Widget parent)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_NoteRoot = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_NoteRoot, 0, 0);
  FrameSlot.SetAnchorMax(m_NoteRoot, 0, 0);
  FrameSlot.SetAlignment(m_NoteRoot, 0, 0);
  FrameSlot.SetSizeToContent(m_NoteRoot, true);
  m_NoteRoot.SetZOrder(1002);
  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, PanelBackground(0.94), 0, m_NoteRoot);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 1, m_NoteRoot));
  size.EnableWidthOverride(true);
  size.SetWidthOverride(NOTE_WIDTH);
  AlignableSlot.SetPadding(size, 10, 8, 10, 8);
  m_NoteText = AddText(size, LAYOUT_TEXT, "");
  if (m_NoteText)
  {
   m_NoteText.SetFlags(WidgetFlags.IGNORE_CURSOR);
   AlignableSlot.SetHorizontalAlign(m_NoteText, LayoutHorizontalAlign.Stretch);
  }
 }

 protected void SetNote(string text)
 {
  if (text == m_sNote) return;
  m_sNote = text;
  if (m_NoteText) m_NoteText.SetText(text);
 }

 // Beside the panel, level with its top; clear of a hint. Only while the panel shows (placing a FOB
 // with the 2IC, its own banner says it all).
 protected void PlaceNote()
 {
  if (!m_NoteRoot) return;
  bool show = !m_sNote.IsEmpty() && m_Root && m_Root.IsVisible();
  if (m_NoteRoot.IsVisible() != show) m_NoteRoot.SetVisible(show);
  if (!show) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float screenW = ws.DPIUnscale(ws.GetWidth());
  float screenH = ws.DPIUnscale(ws.GetHeight());
  float px, py, pw, ph, nw, nh;
  m_Root.GetScreenPos(px, py);
  m_Root.GetScreenSize(pw, ph);
  m_NoteRoot.GetScreenSize(nw, nh);
  nw = ws.DPIUnscale(nw);
  nh = ws.DPIUnscale(nh);
  float x = ACR_UiPrefs.BesideX(ws.DPIUnscale(px), ws.DPIUnscale(pw), nw, screenW);
  float y = ACR_UiPrefs.FitY(ws.DPIUnscale(py), 0, screenH, nh);
  float hx, hy, hw, hh, toX, toY;
  if (ACR_PanelMover.HintRect(hx, hy, hw, hh) && ACR_UiPrefs.StepAside(x, y, nw, nh, hx, hy, hw, hh, screenW, screenH, toX, toY))
  {
   x = toX;
   y = toY;
  }
  FrameSlot.SetPos(m_NoteRoot, x, y);
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
  AlignableSlot.SetPadding(size, 8, 14, 8, 8); // room above the button to hold it by
  Widget column = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, size);
  m_DebugToggle = AddButton(column, "Debug", ACR_EPanelAction.DEBUG_TOGGLE);
  m_DebugSection = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, column);
  // Every button names the side it acts on (SetDebug fills the names in): "us" read as "U.S."
  // (6 Nov live). Grouped by what each needs selected on the map.
  m_DebugSides = AddText(m_DebugSection, LAYOUT_TEXT, "");
  m_DebugStatus = AddText(m_DebugSection, LAYOUT_TEXT, "");
  Widget rp = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_DebugSection);
  m_DebugRPOwn = AddButton(rp, "+500 RP (your side)", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.RP_OWN, true);
  m_DebugRPEnemy = AddButton(rp, "+500 RP (enemy)", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.RP_ENEMY, true);
  m_DebugPause = AddButton(m_DebugSection, "Pause enemy AI commander", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.PAUSE_ENEMY);
  m_DebugAttack = AddButton(m_DebugSection, "Enemy AI attacks now", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.ATTACK_NOW);
  m_DebugBaseHeader = AddText(m_DebugSection, LAYOUT_TEXT, "");
  Widget capture = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_DebugSection);
  m_DebugCaptureOwn = AddButton(capture, "Give to your side", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.CAPTURE_OWN, true);
  m_DebugCaptureEnemy = AddButton(capture, "Give to enemy", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.CAPTURE_ENEMY, true);
  Widget fob = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_DebugSection);
  m_DebugFobSupply = AddButton(fob, "FOB +500 supplies", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.FOB_SUPPLY, true);
  m_DebugFobFinish = AddButton(fob, "Finish FOB upgrade", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.FOB_FINISH, true);
  AddText(m_DebugSection, LAYOUT_TEXT, "Selected squad (none: heal yourself)");
  Widget squad = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_DebugSection);
  AddButton(squad, "Heal + rearm", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.HEAL_SQUAD, true);
  AddButton(squad, "Kill squad", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.KILL_SQUAD, true);
  AddText(m_DebugSection, LAYOUT_TEXT, "World");
  Widget misc = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_DebugSection);
  AddButton(misc, "Time +6h", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.TIME_STEP, true);
  m_DebugReveal = AddButton(misc, "Reveal enemy", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.REVEAL, true);
  AddButton(misc, "Supports ready", ACR_EPanelAction.DEBUG_ACTION + ACR_EDebug.RESET_SUPPORTS, true);
  AddButton(m_DebugSection, "AC:A Think: what the AI commanders think", ACR_EPanelAction.DEBUG_THINK);
  AddText(m_DebugSection, LAYOUT_TEXT, "Console: type /acr [cmd] in chat (/acr help)");
  m_DebugSection.SetVisible(false);
 }

 // allowed = host/admin; baseName = the base selected on the map ("" none); own/enemy = side
 // names; paused = the enemy AI commander is paused; reveal = our intel sees every enemy soldier.
 void SetDebug(bool allowed, bool open, string status, string baseName, string own, string enemy, bool paused, bool reveal)
 {
  if (!m_DebugRoot) return;
  string key = string.Format("%1|%2|%3|%4|%5|%6|%7|%8", allowed, open, status, baseName, own, enemy, paused, reveal);
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
  if (own.IsEmpty()) own = "your side"; // before the first update from the server
  if (enemy.IsEmpty()) enemy = "enemy";
  if (m_DebugSides) m_DebugSides.SetText(string.Format("You play %1. Enemy: %2.", own, enemy));
  if (m_DebugStatus) m_DebugStatus.SetText(status);
  if (m_DebugRPOwn) m_DebugRPOwn.SetText(string.Format("+500 RP to %1", own));
  if (m_DebugRPEnemy) m_DebugRPEnemy.SetText(string.Format("+500 RP to %1", enemy));
  if (m_DebugPause)
  {
   if (paused) m_DebugPause.SetText(string.Format("Resume %1 AI commander", enemy));
   else m_DebugPause.SetText(string.Format("Pause %1 AI commander", enemy));
  }
  if (m_DebugReveal)
  {
   if (reveal) m_DebugReveal.SetText(string.Format("Hide %1", enemy));
   else m_DebugReveal.SetText(string.Format("Reveal %1", enemy));
  }
  bool hasBase = !baseName.IsEmpty();
  if (m_DebugAttack)
  {
   if (hasBase) m_DebugAttack.SetText(string.Format("%1 AI attacks %2 now", enemy, baseName));
   else m_DebugAttack.SetText(string.Format("%1 AI attacks now (its pick)", enemy));
  }
  if (m_DebugBaseHeader)
  {
   if (hasBase) m_DebugBaseHeader.SetText("Selected base: " + baseName);
   else m_DebugBaseHeader.SetText("Selected base: none (click a base)");
  }
  if (m_DebugCaptureOwn)
  {
   m_DebugCaptureOwn.SetEnabled(hasBase, false);
   m_DebugCaptureOwn.SetText("Give to " + own);
  }
  if (m_DebugCaptureEnemy)
  {
   m_DebugCaptureEnemy.SetEnabled(hasBase, false);
   m_DebugCaptureEnemy.SetText("Give to " + enemy);
  }
  if (m_DebugFobSupply) m_DebugFobSupply.SetEnabled(hasBase, false);
  if (m_DebugFobFinish) m_DebugFobFinish.SetEnabled(hasBase, false);
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

 // The INTEL line under the screen point x, y (native px), -1 none. The feed's widgets ignore the
 // cursor, so a click on a line reaches the map (SCR_PlayerController.ACR_OnMapClick) and is matched
 // here by where it fell.
 int FeedLineAt(float x, float y)
 {
  if (!m_FeedRoot || !m_FeedRoot.IsVisible()) return -1;
  float rx, ry, rw, rh;
  m_FeedRoot.GetScreenPos(rx, ry);
  m_FeedRoot.GetScreenSize(rw, rh);
  if (x < rx || x > rx + rw || y < ry || y > ry + rh) return -1;
  foreach (int i, TextWidget line : m_FeedLines)
  {
   if (!line || !line.IsVisible()) continue;
   float lx, ly, lw, lh;
   line.GetScreenPos(lx, ly);
   line.GetScreenSize(lw, lh);
   if (y >= ly && y <= ly + lh) return i;
  }
  return -1;
 }

 // The INTEL line under the cursor that a click would jump from, lit (-1: none).
 protected ref array<ref Color> m_FeedLineColors = {};
 protected int m_iFeedLit = -1;
 void LightFeedLine(int i)
 {
  if (i == m_iFeedLit) return;
  while (m_FeedLineColors.Count() < m_FeedLines.Count())
  {
   TextWidget unlit = m_FeedLines[m_FeedLineColors.Count()];
   Color color = Color.FromInt(Color.WHITE);
   if (unlit) color = unlit.GetColor();
   m_FeedLineColors.Insert(color);
  }
  if (m_iFeedLit >= 0 && m_iFeedLit < m_FeedLines.Count() && m_FeedLines[m_iFeedLit]) m_FeedLines[m_iFeedLit].SetColor(m_FeedLineColors[m_iFeedLit]);
  m_iFeedLit = i;
  if (i >= 0 && i < m_FeedLines.Count() && m_FeedLines[i]) m_FeedLines[i].SetColor(Color.FromRGBA(255, 200, 130, 255));
 }

 // The intel feed's top (the 2IC card moves it down, ACR_TwoICView).
 void SetFeedTop(float y)
 {
  ACR_PanelMover.Natural(ACR_UiPrefs.FEED, ACR_UiPrefs.FEED_HOME_X, y);
 }

 Widget DebugRoot() { return m_DebugRoot; }

 // ---------------------------------------------------------------- moving the panels

 protected SCR_ButtonTextComponent m_KeyHintsButton;

 protected SCR_ButtonTextComponent m_BuildFoldButton;
 protected Widget m_BuildFoldWidget;
 protected Widget m_BuildSection;

 void ToggleBuildFold()
 {
  ACR_UiPrefs.s_bBuildOpen = !ACR_UiPrefs.s_bBuildOpen;
  ACR_UiPrefs.Save();
 }

 void ToggleUpgradesFold()
 {
  ACR_UiPrefs.s_bUpgradesOpen = !ACR_UiPrefs.s_bUpgradesOpen;
  ACR_UiPrefs.Save();
 }

 void ToggleMoreFold()
 {
  ACR_UiPrefs.s_bMoreOpen = !ACR_UiPrefs.s_bMoreOpen;
  ACR_UiPrefs.Save();
 }

 // Open while one of its orders is armed (its button shows that, and its second click cancels). The
 // fold button only shows when the squad can build something.
 protected void RefreshBuildFold(int cursorMode)
 {
  if (!m_BuildSection || !m_BuildFoldButton) return;
  bool armed = cursorMode == ACR_ECursorMode.ESTABLISH_FOB || cursorMode == ACR_ECursorMode.FORTIFY || cursorMode == ACR_ECursorMode.DISARM_MINES;
  if (ACR_CommandPanel.RoadblockOfMode(cursorMode) >= 0) armed = true;
  if (cursorMode >= ACR_ECursorMode.MINES_AP && cursorMode <= ACR_ECursorMode.MINES_MIXED) armed = true;
  bool any = (m_EstablishFobWidget && m_EstablishFobWidget.IsVisible()) || (m_RoadblockWidget && m_RoadblockWidget.IsVisible()) || (m_MinesWidget && m_MinesWidget.IsVisible()) || (m_FortifyWidget && m_FortifyWidget.IsVisible());
  bool open = ACR_UiPrefs.s_bBuildOpen || armed;
  if (m_BuildFoldWidget) m_BuildFoldWidget.SetVisible(any);
  m_BuildSection.SetVisible(any && open);
  if (open) m_BuildFoldButton.SetText("Build orders  [hide]");
  else m_BuildFoldButton.SetText("Build orders");
 }

 // Tests: an armed order's note shows, clear of the panel.
 bool NoteShownForTest() { return m_NoteRoot && m_NoteRoot.IsVisible(); }
 bool NoteClearForTest()
 {
  if (!NoteShownForTest() || !m_Root) return false;
  float x, y, w, h, nx, ny, nw, nh;
  m_Root.GetScreenPos(x, y);
  m_Root.GetScreenSize(w, h);
  m_NoteRoot.GetScreenPos(nx, ny);
  m_NoteRoot.GetScreenSize(nw, nh);
  return nw > 0 && !ACR_UiPrefs.Overlap(x, y, w, h, nx, ny, nw, nh);
 }
 // Tests: the middle of the first line INTEL shows, in screen pixels (x, y; zero: no line).
 vector FeedLineForTest()
 {
  foreach (TextWidget line : m_FeedLines)
  {
   if (!line || !line.IsVisible()) continue;
   float x, y, w, h;
   line.GetScreenPos(x, y);
   line.GetScreenSize(w, h);
   return Vector(x + w * 0.5, y + h * 0.5, 0);
  }
  return vector.Zero;
 }
 // Tests: INTEL's title bar ends above the first line it shows.
 bool FeedTitleClearForTest()
 {
  Widget first = FirstFeedLine();
  vector grip = ACR_PanelMover.GripForTest(ACR_UiPrefs.FEED);
  if (!first || grip == vector.Zero) return false;
  float x, y;
  first.GetScreenPos(x, y);
  return FeedLineAt(grip[0], grip[1]) < 0 && grip[1] < y;
 }
 // Tests: the middle of the key reminders button, in screen pixels (x, y).
 vector KeyHintsButtonForTest()
 {
  if (!m_KeyHintsButton) return vector.Zero;
  float x, y, w, h;
  m_KeyHintsButton.GetRootWidget().GetScreenPos(x, y);
  m_KeyHintsButton.GetRootWidget().GetScreenSize(w, h);
  return Vector(x + w * 0.5, y + h * 0.5, 0);
 }

 void RefreshKeyHints()
 {
  if (!m_KeyHintsButton) return;
  ACR_UiPrefs.Load();
  if (ACR_UiPrefs.s_bKeyHints) m_KeyHintsButton.SetText("Key reminders: on");
  else m_KeyHintsButton.SetText("Key reminders: off");
 }

 // Each panel of this box's, for the one way of moving a panel (ACR_PanelMover). The ACA panel is held
 // by its face or its title; INTEL by its title only: the rest lets the cursor through to the map
 // (a click on a line jumps there).
 protected void RegisterPanels()
 {
  ACR_PanelMover.Add(ACR_UiPrefs.PANEL, m_Root, 1, 0, ACR_UiPrefs.PANEL_HOME_X, ACR_UiPrefs.PANEL_HOME_Y);
  ACR_PanelMover.Grip(ACR_UiPrefs.PANEL, m_Root);
  ACR_PanelMover.TitleBar(ACR_UiPrefs.PANEL, m_Root, 44);
  ACR_PanelMover.Add(ACR_UiPrefs.FEED, m_FeedRoot, 0, 0, ACR_UiPrefs.FEED_HOME_X, ACR_UiPrefs.FEED_HOME_Y);
  ACR_PanelMover.TitleBar(ACR_UiPrefs.FEED, m_FeedRoot, 34);
  ACR_PanelMover.Add(ACR_UiPrefs.DEBUG_BOX, m_DebugRoot, 0, 1, ACR_UiPrefs.FEED_HOME_X, -96);
  ACR_PanelMover.Body(ACR_UiPrefs.DEBUG_BOX, m_DebugRoot);
 }

 // INTEL's first line showing, what its title bar must stop short of.
 protected Widget FirstFeedLine()
 {
  if (m_FeedText && m_FeedText.IsVisible()) return m_FeedText;
  foreach (TextWidget line : m_FeedLines)
  {
   if (line && line.IsVisible()) return line;
  }
  return null;
 }

 // Every frame the map is open (SCR_PlayerController.ACR_UpdatePanel).
 void TickDrag()
 {
  ACR_PanelMover.FitTitleBar(ACR_UiPrefs.PANEL, m_Economy);
  ACR_PanelMover.FitTitleBar(ACR_UiPrefs.FEED, FirstFeedLine());
  FitBody();
  ACR_PanelMover.Tick();
  PlaceNote();
 }

 protected void OnMouseLeftUp(float value = 0, EActionTrigger reason = 0) { ACR_PanelMover.DragEnd(); }

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
  if (name.IsEmpty())
  {
   SetNote("");
   return;
  }
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
  if (m_EvacButton)
  {
   if (cursorMode == ACR_ECursorMode.EVAC) m_EvacButton.SetText("[Evac point]");
   else m_EvacButton.SetText("Evac point");
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
  RefreshBuildFold(cursorMode);
  // An armed order's words stand beside the panel (CreateNote), the plain reminder at its foot.
  bool armedNote = friendly && isCommander && cursorMode != ACR_ECursorMode.MOVE;
  if (armedNote) SetNote(hint);
  else SetNote("");
  if (!m_Hint) return;
  m_Hint.SetVisible(!armedNote);
  if (!armedNote) m_Hint.SetText(hint);
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
  if (m_SquadAIWidget) m_SquadAIWidget.SetVisible(false);
  if (m_EstablishFobWidget) m_EstablishFobWidget.SetVisible(false);
  if (m_RoadblockWidget) m_RoadblockWidget.SetVisible(false);
  if (m_MinesWidget) m_MinesWidget.SetVisible(false);
  if (m_FortifyWidget) m_FortifyWidget.SetVisible(false);
  m_OrderSection.SetVisible(false);
  SetNote("");
  if (m_Hint) m_Hint.SetVisible(false);
 }

 // A FOB's words without its built upgrades: their buttons say so (ACR_FobSlots.Describe). What is
 // being built and what is blocked, and why, are only said here.
 static string FobDetail(string detail)
 {
  if (!detail.Contains("\nUpgrades:")) return detail;
  array<string> lines = {};
  detail.Split("\n", lines, false);
  string kept = "";
  string heading = "";
  bool upgrades = false;
  foreach (string line : lines)
  {
   if (line == "Upgrades:")
   {
    upgrades = true;
    heading = line + "\n";
    continue;
   }
   if (upgrades && line.Contains(" (built)")) continue;
   if (!kept.IsEmpty()) kept += "\n";
   kept += heading + line;
   heading = "";
  }
  return kept;
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

  int slots = 0;
  int builtSlots = 0;
  int buildingPct = -1;
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
   if (i < ACR_FobSlots.SLOT_DEFENCES)
   {
    slots++;
    if (ACR_FobSlots.FeedBuilt(state, tier, nextBlocked)) builtSlots++;
    else if (ACR_FobSlots.FeedBuilding(state, tier, pct)) buildingPct = pct;
   }
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
  if (m_FobUpgradesBox) m_FobUpgradesBox.SetVisible(ACR_UiPrefs.s_bUpgradesOpen);
  if (!m_FobUpgradesFold) return;
  string fold = string.Format("FOB Upgrades  %1/%2", builtSlots, slots);
  if (buildingPct >= 0) fold += string.Format("  (%1%%)", buildingPct);
  if (ACR_UiPrefs.s_bUpgradesOpen) fold += "  [hide]";
  m_FobUpgradesFold.SetText(fold);
 }

 // The evac row, while zombie containment plays.
 void SetEvac(bool visible)
 {
  if (m_EvacRow) m_EvacRow.SetVisible(visible);
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
  ACR_PanelMover.DragEnd();
  ACR_PanelMover.Remove(ACR_UiPrefs.PANEL);
  ACR_PanelMover.Remove(ACR_UiPrefs.FEED);
  ACR_PanelMover.Remove(ACR_UiPrefs.DEBUG_BOX);
  if (m_NoteRoot) m_NoteRoot.RemoveFromHierarchy();
  m_NoteRoot = null;
  m_NoteText = null;
  GetGame().GetInputManager().RemoveActionListener("MouseLeft", EActionTrigger.UP, OnMouseLeftUp);
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
