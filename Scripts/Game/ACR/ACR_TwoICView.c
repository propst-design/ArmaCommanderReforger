// The 2IC's map UI (#14 v1, -acr2IC 1): its card top left (ACA's intel feed moves down under it),
// its channel bottom left (above the debug panel), its ask box beside the ping picker, the FOB here
// line and the Establish FOB banner, and an orange corner on its squads' markers. Shown only while
// the player has the 2IC button of the map's tool bar on: off when the map first opens, the choice
// kept for the match; while it is off, the button counts the lines that need the player. The look is
// the #14 sketch's, made calmer by the declutter sketch (ACR_TwoICStyle, ACR_TwoICCompact): a
// three-line card (the full card on hover, a one-line pill zoomed out or while placing a FOB), the
// channel's newest 3 lines with what needs the player pinned on top, and while placing a FOB only
// the banner and the numbered sites. Pure view: the server sends the card and the channel
// (ACR_TwoIC, ACR_MapFeed.PublishTwoIC).
enum ACR_ETwoICFilter
{
 ALL,
 REQUESTS,
 SUGGESTIONS
}

class ACR_TwoICView : Managed
{
 static const float WIDTH = 432;          // the card, its padding in (the declutter sketch's 360 px at 1600)
 static const float CHANNEL_WIDTH = 564;
 static const float LEFT = 110;           // clear of the map's tool bar, as ACA's feed
 static const float TOP = 96;
 static const float BOTTOM = -96;
 static const float GAP = 8;
 static const int CHANNEL_SHOWN = 5;      // newest lines of the filter on hover (3 by default)
 static const float BAR_WIDTH = 396;      // the full card's bar (WIDTH less its padding)
 static const float MINI_BAR = 56;        // the compact card's
 static const float TIME_WIDTH = 56;
 static const float ASK_WIDTH = 350;
 static const float SITE_CARD_WIDTH = 300;
 static const float SUGGEST_WIDTH = 340;
 static const int SITES_SHOWN = 3;
 static const int RING_PILLS = 3;
 static const int HOVER_OPEN_MS = 350;    // the cursor rests this long on the card: the full card
 static const int HOVER_KEEP_MS = 300;    // which stays this long after the cursor left
 static const int FORM_COMPACT = 0;
 static const int FORM_FULL = 1;
 static const int FORM_PILL = 2;
 static const string CARD_NAME = "ACR_TwoICCard"; // the card's root widget, top left

 Widget m_CardRoot;
 Widget m_ChannelRoot;
 // The card's three forms.
 protected Widget m_Compact;
 protected Widget m_Full;
 protected Widget m_Pill;
 protected int m_iCardForm = -1;
 protected int m_iCardOverAt = -100000;
 protected int m_iCardEnterAt = -1;
 protected float m_fCompactHeight;
 protected bool m_bTestHover;
 protected TextWidget m_CompactType;
 protected TextWidget m_CompactStats;
 protected TextWidget m_CompactDoing;
 protected SizeLayoutWidget m_MiniFill;
 protected SizeLayoutWidget m_MiniRest;
 protected TextWidget m_PillText;
 protected TextWidget m_Type;
 protected TextWidget m_RunsValue;
 protected TextWidget m_ManagesValue;
 protected TextWidget m_ReserveLabel;
 protected TextWidget m_ReserveValue;
 protected Widget m_Bar;
 protected SizeLayoutWidget m_BarFill;
 protected SizeLayoutWidget m_BarRest;
 protected TextWidget m_Doing;
 protected SCR_ButtonTextComponent m_PanelButton;
 // The channel: a block per shown row.
 protected TextWidget m_ChannelEmpty;
 protected ref array<Widget> m_ChannelBlocks = {};
 protected ref array<Widget> m_BlockEdges = {};
 protected ref array<Widget> m_BlockFills = {};
 protected ref array<SizeLayoutWidget> m_BlockPads = {};
 protected ref array<TextWidget> m_ChannelTimes = {};
 protected ref array<TextWidget> m_ChannelLines = {};     // per block: the line, its chip, its answer
 protected ref array<Widget> m_ChipRoots = {};
 protected ref array<TextWidget> m_ChipTexts = {};
 protected ref array<TextWidget> m_AnswerTimes = {};
 protected ref array<TextWidget> m_ChannelAnswers = {};
 protected ref array<Widget> m_AnswerRows = {};
 protected ref array<Widget> m_ButtonRows = {};
 protected ref array<SCR_ButtonTextComponent> m_KeepButtons = {};
 protected ref array<SCR_ButtonTextComponent> m_CancelButtons = {};
 protected ref array<SCR_ButtonTextComponent> m_ReleaseButtons = {};
 protected ref array<SCR_ButtonTextComponent> m_AcceptButtons = {};
 protected ref array<SCR_ButtonTextComponent> m_ShowButtons = {};
 protected ref array<SCR_ButtonTextComponent> m_BuyButtons = {};
 protected ref array<TextWidget> m_BuyWhys = {};           // why "Buy a truck" is greyed out
 protected int m_iTruckCost = -1;                           // the offer (ACR_TwoICSuggest.OfferFor)
 protected string m_sTruckBase = "";
 protected string m_sTruckWhy = "";
 protected ref map<SCR_ButtonBaseComponent, int> m_LineActions = new map<SCR_ButtonBaseComponent, int>(); // button -> block * 16 + action
 protected ref array<int> m_BlockIds = {};                // the line id each block shows
 protected int m_iShownRows;
 protected int m_iShownPinned;
 protected int m_iShownOneLine;
 protected int m_iMore;                                    // "+N more" (All: what didn't fit)
 protected SCR_ButtonTextComponent m_MoreButton;
 protected int m_iOpenId;                                  // the open request whose buttons show (under the cursor)
 protected int m_iOpenAt;
 // Establish FOB.
 Widget m_FobHereRoot;
 protected SizeLayoutWidget m_FobHereMark;
 protected TextWidget m_FobHereLine;
 protected string m_sFobHereShown = "-";
 protected bool m_bFobHereOk;
 Widget m_EstablishRoot;
 protected TextWidget m_EstablishText;
 protected ref array<Widget> m_SiteRoots = {};
 protected ref array<Widget> m_SitePills = {};
 protected ref array<TextWidget> m_SiteTexts = {};
 Widget m_SiteCardRoot;
 protected TextWidget m_SiteCardTitle;
 protected ref array<TextWidget> m_SiteCardLines = {};
 protected int m_iCardSite = -1;
 protected ref array<Widget> m_RingPills = {};
 protected ref array<TextWidget> m_RingTexts = {};
 // The suggestion's card.
 Widget m_SuggestRoot;
 protected TextWidget m_SuggestFront;
 protected TextWidget m_SuggestText;
 protected int m_iSuggestId;
 // The tool bar button's count.
 protected TextWidget m_BadgeText;
 protected int m_iBadge;
 Widget m_AskRoot;
 protected SCR_ButtonTextComponent m_AskTake;
 protected ref map<SCR_ButtonBaseComponent, int> m_AskKinds = new map<SCR_ButtonBaseComponent, int>();
 protected ref map<SCR_ButtonBaseComponent, int> m_PingKinds = new map<SCR_ButtonBaseComponent, int>();
 protected ref array<SCR_ButtonTextComponent> m_FilterButtons = {};
 protected ref map<SCR_ButtonBaseComponent, int> m_Filters = new map<SCR_ButtonBaseComponent, int>();
 protected int m_iFilter = ACR_ETwoICFilter.ALL;
 protected string m_sCardShown = "-";
 protected string m_sChannelShown = "-";
 protected SCR_PlayerController m_Owner;

 void Create(Widget parent, SCR_PlayerController owner)
 {
  m_Owner = owner;
  CreateCard(parent);
  CreateChannel(parent);
  CreateAsk(parent);
  CreateFobHere(parent);
  CreateEstablish(parent);
  CreateSites(parent);
  CreateSiteCard(parent);
  CreateRingPills(parent);
  CreateSuggestion(parent);
  CreateBadge(parent);
  SetVisible(false);
 }

 // ---------------------------------------------------------------- the card
 // The declutter sketch 1: three lines by default (the title and the panel's key; squads, FOBs and
 // the reserve with a thin bar; what it does first and how many more); on hover the full card (the
 // #14 sketch's: Runs / Manages, the reserve and its bar, what it is doing, "Open 2IC panel"); zoomed
 // out or while placing a FOB (sketch 2) a one-line pill.
 protected void CreateCard(Widget parent)
 {
  Widget box = Place(parent, ACR_TwoICStyle.PanelBox(parent, 0, 0, 0, m_CardRoot), 0, TOP);
  if (m_CardRoot)
  {
   // Over the intel feed while the full card opens on hover; named for the map's own UI, whose
   // INTEL goes just below it while it shows.
   m_CardRoot.SetZOrder(1001);
   m_CardRoot.SetName(CARD_NAME);
  }
  CreateCompact(box);
  CreateFull(box);
  CreatePill(box);
  SetCardForm(FORM_COMPACT);
 }

 protected void CreateCompact(Widget box)
 {
  Widget column = Section(box, WIDTH - 32, 16, 12, m_Compact);
  Widget title = ACR_TwoICStyle.Row(column);
  SizeLayoutWidget dot = ACR_TwoICStyle.Box(title, 10, 10, ACR_TwoICStyle.Accent());
  AlignableSlot.SetVerticalAlign(dot, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(dot, 0, 0, 9, 0);
  TextWidget name = ACR_TwoICStyle.Text(title, "2IC", ACR_TwoICStyle.Accent(), 22, true);
  AlignableSlot.SetVerticalAlign(name, LayoutVerticalAlign.Center);
  m_CompactType = ACR_TwoICStyle.Text(title, "Standard", ACR_TwoICStyle.Grey(), 16);
  AlignableSlot.SetVerticalAlign(m_CompactType, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(m_CompactType, 10, 2, 0, 0);
  ACR_TwoICStyle.Grow(ACR_TwoICStyle.Box(title, 0, 1, null));
  SCR_ButtonTextComponent panel = ACR_TwoICStyle.Button(title, "Panel  F11", ACR_TwoICStyle.BUTTON_QUIET, 96, 14, 28);
  if (panel)
  {
   AlignableSlot.SetVerticalAlign(panel.GetRootWidget().GetParent(), LayoutVerticalAlign.Center);
   panel.m_OnClicked.Insert(OnPanel);
  }
  Widget stats = ACR_TwoICStyle.Row(column, 6);
  m_CompactStats = ACR_TwoICStyle.Text(stats, "", ACR_TwoICStyle.White(), 17);
  AlignableSlot.SetVerticalAlign(m_CompactStats, LayoutVerticalAlign.Center);
  ACR_TwoICStyle.Grow(m_CompactStats);
  Widget bar = ACR_TwoICStyle.Row(stats);
  AlignableSlot.SetVerticalAlign(bar, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(bar, 10, 0, 0, 0);
  m_MiniFill = ACR_TwoICStyle.Box(bar, MINI_BAR / 2, 4, ACR_TwoICStyle.Accent());
  m_MiniRest = ACR_TwoICStyle.Box(bar, MINI_BAR / 2, 4, ACR_TwoICStyle.Track());
  m_CompactDoing = ACR_TwoICStyle.Text(column, "", ACR_TwoICStyle.Grey(), 16);
  AlignableSlot.SetPadding(m_CompactDoing, 0, 4, 0, 0);
 }

 protected void CreateFull(Widget box)
 {
  Widget card = Section(box, WIDTH - 36, 18, 16, m_Full);
  Widget title = ACR_TwoICStyle.Row(card);
  SizeLayoutWidget dot = ACR_TwoICStyle.Box(title, 12, 12, ACR_TwoICStyle.Accent());
  AlignableSlot.SetVerticalAlign(dot, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(dot, 0, 0, 10, 0);
  TextWidget name = ACR_TwoICStyle.Text(title, "2IC", ACR_TwoICStyle.Accent(), 25, true);
  AlignableSlot.SetVerticalAlign(name, LayoutVerticalAlign.Center);
  m_Type = ACR_TwoICStyle.Text(title, "Standard", ACR_TwoICStyle.Grey(), 17);
  AlignableSlot.SetVerticalAlign(m_Type, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(m_Type, 12, 4, 0, 0);
  ACR_TwoICStyle.Grow(ACR_TwoICStyle.Box(title, 0, 1, null));
  TextWidget key = ACR_TwoICStyle.Text(title, "F11", ACR_TwoICStyle.Grey(), 15);
  AlignableSlot.SetVerticalAlign(key, LayoutVerticalAlign.Center);

  Widget stats = ACR_TwoICStyle.Row(card, 12);
  Widget runs = ACR_TwoICStyle.Column(ACR_TwoICStyle.Box(stats, 206, 0, null));
  ACR_TwoICStyle.Text(runs, "Runs", ACR_TwoICStyle.Grey(), 16);
  m_RunsValue = ACR_TwoICStyle.Text(runs, "", ACR_TwoICStyle.White(), 22);
  Widget manages = ACR_TwoICStyle.Column(ACR_TwoICStyle.Box(stats, 180, 0, null));
  ACR_TwoICStyle.Text(manages, "Manages", ACR_TwoICStyle.Grey(), 16);
  m_ManagesValue = ACR_TwoICStyle.Text(manages, "", ACR_TwoICStyle.White(), 22);

  Widget reserve = ACR_TwoICStyle.Row(card, 14);
  m_ReserveLabel = ACR_TwoICStyle.Text(reserve, "", ACR_TwoICStyle.White(), 17);
  ACR_TwoICStyle.Grow(m_ReserveLabel);
  m_ReserveValue = ACR_TwoICStyle.Text(reserve, "", ACR_TwoICStyle.Grey(), 17);
  m_Bar = ACR_TwoICStyle.Row(card, 6);
  m_BarFill = ACR_TwoICStyle.Box(m_Bar, BAR_WIDTH / 2, 6, ACR_TwoICStyle.Accent());
  m_BarRest = ACR_TwoICStyle.Box(m_Bar, BAR_WIDTH / 2, 6, ACR_TwoICStyle.Track());

  m_Doing = ACR_TwoICStyle.Text(card, "", ACR_TwoICStyle.Body(), 17);
  AlignableSlot.SetPadding(m_Doing, 0, 14, 0, 0);
  m_PanelButton = ACR_TwoICStyle.Button(card, "Open 2IC panel", ACR_TwoICStyle.BUTTON_OUTLINE, 0, 17, 42);
  if (m_PanelButton)
  {
   AlignableSlot.SetPadding(m_PanelButton.GetRootWidget().GetParent(), 0, 12, 0, 0);
   m_PanelButton.m_OnClicked.Insert(OnPanel);
  }
 }

 protected void CreatePill(Widget box)
 {
  Widget column = Section(box, 0, 14, 8, m_Pill);
  Widget row = ACR_TwoICStyle.Row(column);
  SizeLayoutWidget dot = ACR_TwoICStyle.Box(row, 8, 8, ACR_TwoICStyle.Accent());
  AlignableSlot.SetVerticalAlign(dot, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(dot, 0, 0, 9, 0);
  m_PillText = ACR_TwoICStyle.Text(row, "2IC", ACR_TwoICStyle.Body(), 15);
  AlignableSlot.SetVerticalAlign(m_PillText, LayoutVerticalAlign.Center);
 }

 // A part of a box: a padded column (width > 0: that wide); root: its own, to show or hide it.
 protected static Widget Section(Widget parent, float width, float padX, float padY, out Widget root)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent));
  if (width > 0)
  {
   size.EnableWidthOverride(true);
   size.SetWidthOverride(width);
  }
  AlignableSlot.SetPadding(size, padX, padY, padX, padY);
  root = size;
  Widget column = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, size);
  AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Stretch);
  return column;
 }

 protected void SetCardForm(int form)
 {
  m_iCardForm = form;
  if (m_Compact) m_Compact.SetVisible(form == FORM_COMPACT);
  if (m_Full) m_Full.SetVisible(form == FORM_FULL);
  if (m_Pill) m_Pill.SetVisible(form == FORM_PILL);
 }

 int CardForm() { return m_iCardForm; }

 // Each map frame: pill (zoomed out, or placing a FOB), else the full card once the cursor rests
 // on the card (HOVER_OPEN_MS: a quick click on "Panel F11" stays a click) until HOVER_KEEP_MS
 // after it left, else the three lines.
 void UpdateCard(bool pill)
 {
  if (!m_CardRoot) return;
  int now = System.GetTickCount();
  bool over = UnderMouse(m_CardRoot);
  if (!over) m_iCardEnterAt = -1;
  else
  {
   if (m_iCardEnterAt < 0) m_iCardEnterAt = now;
   m_iCardOverAt = now;
  }
  bool rested = over && now - m_iCardEnterAt >= HOVER_OPEN_MS;
  bool kept = m_iCardForm == FORM_FULL && now - m_iCardOverAt < HOVER_KEEP_MS;
  int form = FORM_COMPACT;
  if (pill) form = FORM_PILL;
  else if (m_bTestHover || rested || kept) form = FORM_FULL;
  if (form != m_iCardForm) SetCardForm(form);
 }

 // The card and the channel as if the cursor were on them (the client shots).
 void SetHoverForTest(bool hover)
 {
  m_bTestHover = hover;
  m_sChannelShown = "-";
 }

 // The cursor (native px) is within the widget's box.
 static bool UnderMouse(Widget widget)
 {
  if (!widget || !widget.IsVisible()) return false;
  int mx, my;
  WidgetManager.GetMousePos(mx, my);
  float x, y, w, h;
  widget.GetScreenPos(x, y);
  widget.GetScreenSize(w, h);
  return w > 0 && h > 0 && x <= mx && mx <= x + w && y <= my && my <= y + h; // float first (int-left compares in ints)
 }

 // ---------------------------------------------------------------- the channel
 // "2IC CHANNEL" with its filter pills; then what needs the player, pinned in an orange box with
 // "Needs you" and its buttons; then the newest lines (3, or 5 on hover or a filter), newest first:
 // its time, "You:" in blue or "2IC:" in the accent, a status chip, the 2IC's answer and the
 // line's buttons; its own reports dimmed, repeats merged.
 protected void CreateChannel(Widget parent)
 {
  Widget channel = Place(parent, ACR_TwoICStyle.PanelBox(parent, CHANNEL_WIDTH - 32, 16, 12, m_ChannelRoot), 1, BOTTOM);
  Widget header = ACR_TwoICStyle.Row(channel);
  TextWidget heading = ACR_TwoICStyle.Text(header, "2IC CHANNEL", ACR_TwoICStyle.Accent(), 20, true);
  AlignableSlot.SetVerticalAlign(heading, LayoutVerticalAlign.Center);
  ACR_TwoICStyle.Grow(heading);
  array<string> names = {"All", "Requests", "Suggestions"};
  array<float> widths = {52, 94, 112};
  foreach (int f, string label : names)
  {
   SCR_ButtonTextComponent pill = ACR_TwoICStyle.Button(header, label, ACR_TwoICStyle.BUTTON_PILL, widths[f], 14, 30);
   m_FilterButtons.Insert(pill);
   if (!pill) continue;
   AlignableSlot.SetVerticalAlign(pill.GetRootWidget().GetParent(), LayoutVerticalAlign.Center);
   pill.m_OnClicked.Insert(OnFilter);
   m_Filters.Insert(pill, f);
  }
  m_ChannelEmpty = ACR_TwoICStyle.Text(channel, "Nothing yet", ACR_TwoICStyle.Grey(), 16);
  AlignableSlot.SetPadding(m_ChannelEmpty, 0, 8, 0, 0);
  for (int i = 0; i < ACR_TwoICCompact.BLOCKS; i++) CreateBlock(channel, i);
  // "+N more" under a capped All: opens Requests.
  m_MoreButton = ACR_TwoICStyle.Button(channel, "+1 more", ACR_TwoICStyle.BUTTON_LINK, 150, 15, 28);
  if (m_MoreButton)
  {
   AlignableSlot.SetPadding(m_MoreButton.GetRootWidget().GetParent(), TIME_WIDTH - 8, 4, 0, 0);
   m_MoreButton.m_OnClicked.Insert(OnMore);
   m_MoreButton.GetRootWidget().GetParent().SetVisible(false);
  }
  ShowFilter();
 }

 protected void CreateBlock(Widget channel, int i)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget block = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, channel);
  AlignableSlot.SetPadding(block, 0, 6, 0, 0);
  AlignableSlot.SetHorizontalAlign(block, LayoutHorizontalAlign.Stretch);
  m_ChannelBlocks.Insert(block);
  // A pinned row's orange edge: the accent behind, its fill 1 px in.
  Widget edge = ACR_TwoICStyle.Fill(block, ACR_TwoICStyle.Accent());
  m_BlockEdges.Insert(edge);
  Widget fill = ACR_TwoICStyle.Fill(block, ACR_TwoICStyle.Block());
  AlignableSlot.SetPadding(fill, 1, 1, 1, 1);
  fill.SetZOrder(1);
  m_BlockFills.Insert(fill);
  SizeLayoutWidget pad = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 2, block));
  AlignableSlot.SetHorizontalAlign(pad, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(pad, LayoutVerticalAlign.Stretch);
  m_BlockPads.Insert(pad);
  Widget column = ACR_TwoICStyle.Column(pad);
  AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Stretch);
  Widget head = ACR_TwoICStyle.Row(column);
  SizeLayoutWidget timeBox = ACR_TwoICStyle.Box(head, TIME_WIDTH, 0, null);
  m_ChannelTimes.Insert(ACR_TwoICStyle.Text(timeBox, "", ACR_TwoICStyle.Grey(), 16));
  TextWidget line = ACR_TwoICStyle.Text(head, "", ACR_TwoICStyle.White(), 17);
  ACR_TwoICStyle.Grow(line);
  m_ChannelLines.Insert(line);
  // An open suggestion's "Show" (sketch 3): the map pans to its marker.
  SCR_ButtonTextComponent show = ACR_TwoICStyle.Button(head, "Show", ACR_TwoICStyle.BUTTON_LINK, 60, 15, 26);
  if (show)
  {
   AlignableSlot.SetVerticalAlign(show.GetRootWidget().GetParent(), LayoutVerticalAlign.Top);
   show.m_OnClicked.Insert(OnShow);
   show.GetRootWidget().GetParent().SetVisible(false);
  }
  m_ShowButtons.Insert(show);
  Widget chip;
  TextWidget chipText = ACR_TwoICStyle.Chip(head, chip);
  AlignableSlot.SetVerticalAlign(chip, LayoutVerticalAlign.Top);
  AlignableSlot.SetPadding(chip, 8, 0, 0, 0);
  m_ChipRoots.Insert(chip);
  m_ChipTexts.Insert(chipText);
  // The 2IC's answer as a line of its own, with the time (sketch 3: "10:24 2IC: Construction
  // Truck 1 is on its way to build it").
  Widget answerRow = ACR_TwoICStyle.Row(column, 3);
  SizeLayoutWidget answerTime = ACR_TwoICStyle.Box(answerRow, TIME_WIDTH, 0, null);
  m_AnswerTimes.Insert(ACR_TwoICStyle.Text(answerTime, "", ACR_TwoICStyle.Grey(), 16));
  TextWidget answer = ACR_TwoICStyle.Text(answerRow, "", ACR_TwoICStyle.White(), 17);
  ACR_TwoICStyle.Grow(answer);
  m_AnswerRows.Insert(answerRow);
  m_ChannelAnswers.Insert(answer);
  Widget buttons = ACR_TwoICStyle.Row(column, 6);
  ACR_TwoICStyle.Box(buttons, TIME_WIDTH - 2, 0, null);
  m_ButtonRows.Insert(buttons);
  m_AcceptButtons.Insert(LineButton(buttons, "Send truck", i, ACR_TwoICSuggest.ACT_ACCEPT, ACR_TwoICStyle.BUTTON_FILL, 136));
  // The pinned no-truck line's "Buy a truck (21 RP)": the player's own requisition.
  SCR_ButtonTextComponent buy = ACR_TwoICStyle.Button(buttons, "Buy a truck", ACR_TwoICStyle.BUTTON_FILL, 184, 15, 36);
  if (buy)
  {
   AlignableSlot.SetPadding(buy.GetRootWidget().GetParent(), 0, 0, 8, 0);
   buy.m_OnClicked.Insert(OnBuyTruck);
   buy.GetRootWidget().GetParent().SetVisible(false);
  }
  m_BuyButtons.Insert(buy);
  m_KeepButtons.Insert(LineButton(buttons, "Keep it queued", i, ACR_TwoICRequests.ACT_KEEP, ACR_TwoICStyle.BUTTON_GHOST, 156));
  m_CancelButtons.Insert(LineButton(buttons, "Cancel", i, ACR_TwoICRequests.ACT_CANCEL, ACR_TwoICStyle.BUTTON_GHOST, 104));
  m_ReleaseButtons.Insert(LineButton(buttons, "Release", i, ACR_TwoICRequests.ACT_RELEASE, ACR_TwoICStyle.BUTTON_GHOST, 104));
  TextWidget buyWhy = ACR_TwoICStyle.Text(column, "", ACR_TwoICStyle.Grey(), 15);
  if (buyWhy)
  {
   AlignableSlot.SetPadding(buyWhy, TIME_WIDTH, 4, 0, 0);
   buyWhy.SetVisible(false);
  }
  m_BuyWhys.Insert(buyWhy);
  m_BlockIds.Insert(0);
  block.SetVisible(false);
 }

 // A panel box placed top left (anchorY 0) or bottom left (1); the column passes through.
 protected Widget Place(Widget parent, Widget column, float anchorY, float y)
 {
  Widget root = m_CardRoot;
  if (anchorY > 0) root = m_ChannelRoot;
  if (!root) return column;
  FrameSlot.SetAnchorMin(root, 0, anchorY);
  FrameSlot.SetAnchorMax(root, 0, anchorY);
  FrameSlot.SetAlignment(root, 0, anchorY);
  FrameSlot.SetPos(root, LEFT, y);
  FrameSlot.SetSizeToContent(root, true);
  root.SetZOrder(1000);
  return column;
 }

 // A box with a 1 px edge (the suggestion's and a FOB site's card): root (for placing) and the
 // padded column inside.
 protected static Widget EdgedBox(Widget parent, float width, float padX, float padY, out Widget root, Color edgeColor)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  root = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, 0, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(root, 0, 0);
  FrameSlot.SetAnchorMax(root, 0, 0);
  FrameSlot.SetSizeToContent(root, true);
  ACR_TwoICStyle.Fill(root, edgeColor);
  SizeLayoutWidget edge = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, root));
  AlignableSlot.SetPadding(edge, 1, 1, 1, 1);
  Widget inner;
  return ACR_TwoICStyle.PanelBox(edge, width, padX, padY, inner, ACR_TwoICStyle.Rgb(24, 27, 30, 1));
 }

 // ---------------------------------------------------------------- the FOB here line
 // By the cursor while Establish FOB is armed (sketch 2): one line, why a FOB fits there or the
 // single reason it can't (ACR_TwoICCompact.FobHereLine), a green or red mark before it.
 protected void CreateFobHere(Widget parent)
 {
  Widget column = ACR_TwoICStyle.PanelBox(parent, 0, 12, 7, m_FobHereRoot, ACR_TwoICStyle.Rgb(24, 27, 30, 0.95));
  FrameSlot.SetAnchorMin(m_FobHereRoot, 0, 0);
  FrameSlot.SetAnchorMax(m_FobHereRoot, 0, 0);
  FrameSlot.SetSizeToContent(m_FobHereRoot, true);
  m_FobHereRoot.SetFlags(WidgetFlags.IGNORE_CURSOR);
  m_FobHereRoot.SetZOrder(1002);
  Widget row = ACR_TwoICStyle.Row(column);
  m_FobHereMark = ACR_TwoICStyle.Box(row, 10, 10, ACR_TwoICStyle.Rgb(226, 84, 72));
  AlignableSlot.SetVerticalAlign(m_FobHereMark, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(m_FobHereMark, 0, 0, 10, 0);
  m_FobHereLine = ACR_TwoICStyle.Text(row, "", ACR_TwoICStyle.White(), 16);
  if (m_FobHereLine)
  {
   m_FobHereLine.SetFlags(WidgetFlags.IGNORE_CURSOR);
   AlignableSlot.SetVerticalAlign(m_FobHereLine, LayoutVerticalAlign.Center);
  }
  m_FobHereRoot.SetVisible(false);
 }

 // text: the server's FOB here card (ACR_TwoICSuggest.FobHereText); x, y: the cursor in native
 // pixels.
 void ShowFobHere(bool show, string text, float x, float y)
 {
  if (!m_FobHereRoot) return;
  m_FobHereRoot.SetVisible(show);
  if (!show) return;
  if (text != m_sFobHereShown)
  {
   m_sFobHereShown = text;
   bool ok;
   string line = ACR_TwoICCompact.FobHereLine(text, ok);
   m_bFobHereOk = ok;
   if (m_FobHereLine) m_FobHereLine.SetText(line);
   Widget mark = m_FobHereMark.GetChildren();
   Color color = ACR_TwoICStyle.Rgb(226, 84, 72);
   if (ok) color = ACR_TwoICStyle.Rgb(104, 190, 120);
   if (mark) mark.SetColor(color);
  }
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float w, h;
  m_FobHereRoot.GetScreenSize(w, h);
  float width = ws.DPIUnscale(w);
  if (width < 40) width = 420;
  float px = ws.DPIUnscale(x) - 10;
  float py = ws.DPIUnscale(y) + 28;
  float screenW = ws.DPIUnscale(ws.GetWidth());
  float screenH = ws.DPIUnscale(ws.GetHeight());
  if (px + width > screenW - 8) px = screenW - 8 - width;
  if (py + 40 > screenH - 8) py = ws.DPIUnscale(y) - 28 - 40;
  FrameSlot.SetPos(m_FobHereRoot, Math.Max(8, px), py);
 }

 // ---------------------------------------------------------------- the Establish FOB banner
 // Top centre while Establish FOB is armed: "ESTABLISH FOB  <truck> · pick a numbered site, or
 // right-click anywhere  [Cancel]".
 protected void CreateEstablish(Widget parent)
 {
  Widget column = ACR_TwoICStyle.PanelBox(parent, 0, 20, 10, m_EstablishRoot, ACR_TwoICStyle.Rgb(24, 27, 30, 0.95));
  FrameSlot.SetAnchorMin(m_EstablishRoot, 0.5, 0);
  FrameSlot.SetAnchorMax(m_EstablishRoot, 0.5, 0);
  FrameSlot.SetAlignment(m_EstablishRoot, 0.5, 0);
  FrameSlot.SetPos(m_EstablishRoot, 0, 72);
  FrameSlot.SetSizeToContent(m_EstablishRoot, true);
  m_EstablishRoot.SetZOrder(1001);
  Widget row = ACR_TwoICStyle.Row(column);
  TextWidget title = ACR_TwoICStyle.Text(row, "ESTABLISH FOB", ACR_TwoICStyle.White(), 22, true);
  AlignableSlot.SetVerticalAlign(title, LayoutVerticalAlign.Center);
  m_EstablishText = ACR_TwoICStyle.Text(row, "", ACR_TwoICStyle.Rgb(201, 198, 191), 18);
  AlignableSlot.SetVerticalAlign(m_EstablishText, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(m_EstablishText, 18, 0, 18, 0);
  SCR_ButtonTextComponent cancel = ACR_TwoICStyle.Button(row, "Cancel", ACR_TwoICStyle.BUTTON_GHOST, 100, 16, 38);
  if (cancel) cancel.m_OnClicked.Insert(OnCancelEstablish);
  m_EstablishRoot.SetVisible(false);
 }

 // truck: the squad's name; sites: whether the 2IC has numbered sites for it.
 void ShowEstablish(bool show, string truck, bool sites)
 {
  if (!m_EstablishRoot) return;
  m_EstablishRoot.SetVisible(show);
  if (!show || !m_EstablishText) return;
  string pick = "right-click where it goes";
  if (sites) pick = "pick a numbered site, or right-click anywhere";
  if (truck.IsEmpty()) m_EstablishText.SetText(pick);
  else m_EstablishText.SetText(truck + "  ·  " + pick);
 }

 protected void OnCancelEstablish(SCR_ButtonBaseComponent button)
 {
  if (m_Owner) m_Owner.ACR_TwoICCancelEstablish();
 }

 // ---------------------------------------------------------------- the 2IC's numbered FOB sites
 // Sketch 2: the canvas draws each site's disc (ACR_OrderDisplay); here its number on it and ONE
 // short line beside it ("Best · 6/8 upgrades · hidden · 443 m back"). The site under the cursor
 // opens its card beside it: why the 2IC likes it, and Build here.
 protected void CreateSites(Widget parent)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  for (int i = 0; i < SITES_SHOWN; i++)
  {
   Widget root = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, parent);
   FrameSlot.SetAnchorMin(root, 0, 0);
   FrameSlot.SetAnchorMax(root, 0, 0);
   FrameSlot.SetAlignment(root, 0, 0.5);
   FrameSlot.SetSizeToContent(root, true);
   root.SetZOrder(999);
   SizeLayoutWidget disc = ACR_TwoICStyle.Box(root, 32, 32, null);
   int shownNumber = i + 1;
   TextWidget number = ACR_TwoICStyle.Text(disc, shownNumber.ToString(), ACR_TwoICStyle.OnAccent(), 17, true);
   if (number)
   {
    number.SetFlags(WidgetFlags.IGNORE_CURSOR);
    AlignableSlot.SetHorizontalAlign(number, LayoutHorizontalAlign.Center);
    AlignableSlot.SetVerticalAlign(number, LayoutVerticalAlign.Center);
   }
   Widget pill;
   Widget column = ACR_TwoICStyle.PanelBox(root, 0, 10, 4, pill, ACR_TwoICStyle.Rgb(24, 27, 30, 0.92));
   pill.SetFlags(WidgetFlags.IGNORE_CURSOR);
   AlignableSlot.SetVerticalAlign(pill, LayoutVerticalAlign.Center);
   AlignableSlot.SetPadding(pill, 10, 0, 0, 0);
   TextWidget text = ACR_TwoICStyle.Text(column, "", ACR_TwoICStyle.Body(), 15);
   if (text) text.SetFlags(WidgetFlags.IGNORE_CURSOR);
   m_SitePills.Insert(pill);
   m_SiteTexts.Insert(text);
   root.SetVisible(false);
   m_SiteRoots.Insert(root);
  }
 }

 protected void CreateSiteCard(Widget parent)
 {
  Widget column = EdgedBox(parent, SITE_CARD_WIDTH, 14, 12, m_SiteCardRoot, ACR_TwoICStyle.Accent());
  m_SiteCardRoot.SetZOrder(1002);
  m_SiteCardTitle = ACR_TwoICStyle.Text(column, "", ACR_TwoICStyle.White(), 17, true);
  for (int i = 0; i < 7; i++)
  {
   TextWidget line = ACR_TwoICStyle.Text(column, "", ACR_TwoICStyle.Body(), 16);
   if (line) AlignableSlot.SetPadding(line, 0, 2, 0, 0);
   m_SiteCardLines.Insert(line);
  }
  SCR_ButtonTextComponent build = ACR_TwoICStyle.Button(column, "Build here", ACR_TwoICStyle.BUTTON_FILL, 0, 17, 40);
  if (build)
  {
   AlignableSlot.SetPadding(build.GetRootWidget().GetParent(), 0, 10, 0, 0);
   build.m_OnClicked.Insert(OnBuildSite);
  }
  m_SiteCardRoot.SetVisible(false);
 }

 // xs, ys: each disc in native pixels (none: hidden); facts: their ACR_TwoICCompact.FACTS
 // numbers; hover: the site under the cursor (-1 none): its card opens beside it, clear of the
 // other sites' lines and the banner.
 void ShowSites(array<float> xs, array<float> ys, array<int> facts, int hover)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  array<float> boxes = {};
  foreach (int i, Widget root : m_SiteRoots)
  {
   bool shown = xs && ys && i < xs.Count() && i < ys.Count();
   root.SetVisible(shown);
   if (!shown) continue;
   string line = ACR_TwoICCompact.SiteLine(i, facts);
   m_SitePills[i].SetVisible(!line.IsEmpty());
   if (i == 0 && line.StartsWith("Best")) m_SiteTexts[i].SetText(ACR_TwoICStyle.Colored("Best", ACR_TwoICStyle.Accent()) + line.Substring(4, line.Length() - 4));
   else m_SiteTexts[i].SetText(line);
   float x = ws.DPIUnscale(xs[i]);
   float y = ws.DPIUnscale(ys[i]);
   FrameSlot.SetPos(root, x - 16, y);
   float w, h;
   root.GetScreenSize(w, h);
   float bw = ws.DPIUnscale(w);
   float bh = ws.DPIUnscale(h);
   if (bw < 40)
   {
    bw = 52 + 7.5 * line.Length();
    bh = 32;
   }
   boxes.Insert(x - 16);
   boxes.Insert(y - bh / 2);
   boxes.Insert(bw);
   boxes.Insert(bh);
  }
  bool card = hover >= 0 && xs && ys && hover < xs.Count() && hover < ys.Count();
  if (!card)
  {
   m_iCardSite = -1;
   if (m_SiteCardRoot) m_SiteCardRoot.SetVisible(false);
   return;
  }
  ShowSiteCard(hover, ws.DPIUnscale(xs[hover]), ws.DPIUnscale(ys[hover]), facts, boxes);
 }

 protected void ShowSiteCard(int site, float x, float y, array<int> facts, array<float> boxes)
 {
  if (!m_SiteCardRoot) return;
  m_iCardSite = site;
  array<string> lines = {};
  ACR_TwoICCompact.SiteCard(site, facts, lines);
  if (lines.IsEmpty())
  {
   m_SiteCardRoot.SetVisible(false);
   return;
  }
  m_SiteCardTitle.SetText(lines[0]);
  foreach (int k, TextWidget text : m_SiteCardLines)
  {
   if (!text) continue;
   bool used = k + 1 < lines.Count();
   text.SetVisible(used);
   if (used) text.SetText(lines[k + 1]);
  }
  m_SiteCardRoot.SetVisible(true);
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float w, h;
  m_SiteCardRoot.GetScreenSize(w, h);
  float cw = ws.DPIUnscale(w);
  float ch = ws.DPIUnscale(h);
  if (cw < 40)
  {
   cw = SITE_CARD_WIDTH + 30;
   ch = 92 + 22 * lines.Count();
  }
  // The banner too.
  if (m_EstablishRoot && m_EstablishRoot.IsVisible())
  {
   float bx, by, bw, bh;
   m_EstablishRoot.GetScreenPos(bx, by);
   m_EstablishRoot.GetScreenSize(bw, bh);
   boxes.Insert(ws.DPIUnscale(bx));
   boxes.Insert(ws.DPIUnscale(by));
   boxes.Insert(ws.DPIUnscale(bw));
   boxes.Insert(ws.DPIUnscale(bh));
  }
  // Under its line (the sketch's), above it, then the same left of its disc.
  array<float> xs = {x + 14, x + 14, x - 20 - cw, x - 20 - cw};
  array<float> ys = {y + 22, y - 22 - ch, y + 22, y - 22 - ch};
  float screenW = ws.DPIUnscale(ws.GetWidth());
  float screenH = ws.DPIUnscale(ws.GetHeight());
  int spot = ACR_TwoICCompact.PickSpot(xs, ys, cw, ch, boxes, screenW, screenH);
  FrameSlot.SetPos(m_SiteCardRoot, xs[spot], ys[spot]);
 }

 bool SiteCardUnderMouse() { return UnderMouse(m_SiteCardRoot); }
 int CardSite() { return m_iCardSite; }

 protected void OnBuildSite(SCR_ButtonBaseComponent button)
 {
  if (m_Owner && m_iCardSite >= 0) m_Owner.ACR_TwoICBuildSite(m_iCardSite);
 }

 // ---------------------------------------------------------------- the rings' labels
 // A dark pill right of a suggested site's disc (when its card isn't shown) or of a site a truck is
 // on its way to ("FOB site · truck on the way · about 4 min", sketch 3).
 protected void CreateRingPills(Widget parent)
 {
  for (int i = 0; i < RING_PILLS; i++)
  {
   Widget root;
   Widget column = ACR_TwoICStyle.PanelBox(parent, 0, 10, 4, root, ACR_TwoICStyle.Rgb(24, 27, 30, 0.92));
   FrameSlot.SetAnchorMin(root, 0, 0);
   FrameSlot.SetAnchorMax(root, 0, 0);
   FrameSlot.SetAlignment(root, 0, 0.5);
   FrameSlot.SetSizeToContent(root, true);
   root.SetFlags(WidgetFlags.IGNORE_CURSOR);
   root.SetZOrder(998);
   TextWidget text = ACR_TwoICStyle.Text(column, "", ACR_TwoICStyle.White(), 15);
   if (text) text.SetFlags(WidgetFlags.IGNORE_CURSOR);
   m_RingTexts.Insert(text);
   root.SetVisible(false);
   m_RingPills.Insert(root);
  }
 }

 // labels and their rings' centres in native pixels.
 void ShowRingPills(array<string> labels, array<float> xs, array<float> ys)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  foreach (int i, Widget root : m_RingPills)
  {
   bool shown = labels && xs && ys && i < labels.Count() && i < xs.Count() && i < ys.Count();
   root.SetVisible(shown);
   if (!shown) continue;
   if (m_RingTexts[i]) m_RingTexts[i].SetText(labels[i]);
   FrameSlot.SetPos(root, ws.DPIUnscale(xs[i]) + 24, ws.DPIUnscale(ys[i]));
  }
 }

 // ---------------------------------------------------------------- the suggestion's card
 // Beside a live suggestion's disc, as the declutter sketch 3: "2IC SUGGESTS · FOB SITE" in the
 // accent, "Behind Le Moule Sawmill", its short reasons, Send truck and Dismiss.
 protected void CreateSuggestion(Widget parent)
 {
  Widget column = EdgedBox(parent, SUGGEST_WIDTH, 16, 14, m_SuggestRoot, ACR_TwoICStyle.Accent());
  m_SuggestRoot.SetZOrder(1001);
  ACR_TwoICStyle.Text(column, "2IC SUGGESTS  ·  FOB SITE", ACR_TwoICStyle.Accent(), 17, true);
  m_SuggestFront = ACR_TwoICStyle.Text(column, "", ACR_TwoICStyle.White(), 18);
  AlignableSlot.SetPadding(m_SuggestFront, 0, 6, 0, 0);
  m_SuggestText = ACR_TwoICStyle.Text(column, "", ACR_TwoICStyle.Grey(), 15);
  AlignableSlot.SetPadding(m_SuggestText, 0, 4, 0, 0);
  Widget buttons = ACR_TwoICStyle.Row(column, 12);
  SCR_ButtonTextComponent send = ACR_TwoICStyle.Button(buttons, "Send truck", ACR_TwoICStyle.BUTTON_FILL, 0, 17, 42);
  if (send)
  {
   ACR_TwoICStyle.Grow(send.GetRootWidget().GetParent());
   send.m_OnClicked.Insert(OnSuggestAccept);
  }
  SCR_ButtonTextComponent dismiss = ACR_TwoICStyle.Button(buttons, "Dismiss", ACR_TwoICStyle.BUTTON_GHOST, 110, 17, 42);
  if (dismiss)
  {
   AlignableSlot.SetPadding(dismiss.GetRootWidget().GetParent(), 8, 0, 0, 0);
   dismiss.m_OnClicked.Insert(OnSuggestDismiss);
  }
  m_SuggestRoot.SetVisible(false);
 }

 // id: the suggestion's channel line (0: none, hidden); reasons: its answer; x, y: its marker in
 // native pixels; front: "Behind Le Moule Sawmill".
 void ShowSuggestion(int id, string reasons, float x, float y, string front = "")
 {
  if (!m_SuggestRoot) return;
  m_iSuggestId = id;
  m_SuggestRoot.SetVisible(id > 0);
  if (id <= 0) return;
  reasons = ACR_TwoICCompact.Reasons(reasons);
  if (reasons.StartsWith("2IC: ")) reasons = reasons.Substring(5, reasons.Length() - 5);
  m_SuggestText.SetText(reasons);
  m_SuggestFront.SetText(front);
  m_SuggestFront.SetVisible(!front.IsEmpty());
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float px = ws.DPIUnscale(x) + 34;
  float py = ws.DPIUnscale(y) - 64;
  float screenW = ws.DPIUnscale(ws.GetWidth());
  if (px + SUGGEST_WIDTH + 40 > screenW - 8) px = ws.DPIUnscale(x) - 34 - SUGGEST_WIDTH - 34;
  py = Math.Max(py, 90);
  FrameSlot.SetPos(m_SuggestRoot, px, py);
 }

 bool SuggestionShown() { return m_SuggestRoot && m_SuggestRoot.IsVisible(); }

 protected void OnSuggestAccept(SCR_ButtonBaseComponent button)
 {
  if (!m_Owner || m_iSuggestId <= 0) return;
  m_Owner.ACR_TwoICAct(m_iSuggestId, ACR_TwoICSuggest.ACT_ACCEPT);
  Answered(m_iSuggestId);
 }

 protected void OnSuggestDismiss(SCR_ButtonBaseComponent button)
 {
  if (!m_Owner || m_iSuggestId <= 0) return;
  m_Owner.ACR_TwoICAct(m_iSuggestId, ACR_TwoICRequests.ACT_CANCEL);
  Answered(m_iSuggestId);
 }

 // A suggestion answered here (Send truck or Dismiss): its card closes now, not when the server's
 // line update comes back (pn12d: the card stayed open after Send truck).
 protected int m_iAnsweredId;
 int AnsweredId() { return m_iAnsweredId; }
 protected void Answered(int id)
 {
  m_iAnsweredId = id;
  if (id == m_iSuggestId) ShowSuggestion(0, "", 0, 0);
 }

 // ---------------------------------------------------------------- the tool bar's count
 // Sketch 4: with the 2IC hidden, an orange disc on the 2IC button's top right (the canvas draws it,
 // ACR_OrderDisplay.DrawTwoICIcon) with how many lines need the player.
 protected void CreateBadge(Widget parent)
 {
  m_BadgeText = ACR_TwoICStyle.Text(parent, "", ACR_TwoICStyle.OnAccent(), 13, true);
  if (!m_BadgeText) return;
  m_BadgeText.SetFlags(WidgetFlags.IGNORE_CURSOR);
  FrameSlot.SetAnchorMin(m_BadgeText, 0, 0);
  FrameSlot.SetAnchorMax(m_BadgeText, 0, 0);
  FrameSlot.SetAlignment(m_BadgeText, 0.5, 0.5);
  FrameSlot.SetSizeToContent(m_BadgeText, true);
  m_BadgeText.SetZOrder(1006);
  m_BadgeText.SetVisible(false);
 }

 // count: 0 hides it; x, y: the disc's centre in native pixels.
 void ShowBadge(int count, float x, float y)
 {
  m_iBadge = count;
  if (!m_BadgeText) return;
  m_BadgeText.SetVisible(count > 0);
  if (count <= 0) return;
  string text = count.ToString();
  if (count > 9) text = "9+";
  m_BadgeText.SetText(text);
  WorkspaceWidget ws = GetGame().GetWorkspace();
  FrameSlot.SetPos(m_BadgeText, ws.DPIUnscale(x), ws.DPIUnscale(y));
 }

 int Badge() { return m_iBadge; }

 // ---------------------------------------------------------------- the ask box
 // "Ask the 2IC": a menu beside ACA's ping picker (Alt+right-click), at the same spot, as the
 // sketch's right-click menu: its heading in the accent, then the requests.
 protected void CreateAsk(Widget parent)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_AskRoot = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, 0, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_AskRoot, 0, 0);
  FrameSlot.SetAnchorMax(m_AskRoot, 0, 0);
  FrameSlot.SetSizeToContent(m_AskRoot, true);
  m_AskRoot.SetZOrder(1001);
  // A 1 px border (#343B43) round the menu (#12161A).
  ACR_TwoICStyle.Fill(m_AskRoot, ACR_TwoICStyle.Rgb(52, 59, 67));
  SizeLayoutWidget edge = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, m_AskRoot));
  AlignableSlot.SetPadding(edge, 1, 1, 1, 1);
  Widget inner;
  Widget column = ACR_TwoICStyle.PanelBox(edge, ASK_WIDTH, 8, 8, inner, ACR_TwoICStyle.Rgb(18, 22, 26, 1));
  // One menu, as sketch 3: ACA's four pings (its picker hides while the 2IC shows; 1-4 still pick),
  // a line, then the asks.
  PingButton(column, "1   Look here", ACR_EPing.LOOK);
  PingButton(column, "2   Attack this", ACR_EPing.ATTACK);
  PingButton(column, "3   Defend here", ACR_EPing.DEFEND);
  PingButton(column, "4   Support here", ACR_EPing.SUPPORT);
  SizeLayoutWidget rule = ACR_TwoICStyle.Box(column, 0, 1, ACR_TwoICStyle.Rgb(52, 59, 67));
  AlignableSlot.SetHorizontalAlign(rule, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetPadding(rule, 0, 6, 0, 6);
  TextWidget heading = ACR_TwoICStyle.Text(column, "ASK THE 2IC", ACR_TwoICStyle.Accent(), 17, true);
  AlignableSlot.SetPadding(heading, 12, 4, 0, 4);
  AskButton(column, "Support me here", ACR_ETwoICAsk.SUPPORT);
  AskButton(column, "Hold this", ACR_ETwoICAsk.HOLD);
  m_AskTake = AskButton(column, "Take this base", ACR_ETwoICAsk.TAKE);
  m_AskRoot.SetVisible(false);
 }

 // x, y: the cursor in native pixels, as ACA's picker (ACR_CommandPanel.ShowPicker); base: the
 // base under it that isn't ours ("" none: no Take button).
 void ShowAsk(bool show, float x, float y, string base)
 {
  if (!m_AskRoot) return;
  if (show)
  {
   if (m_AskTake)
   {
    m_AskTake.SetText("Take this base: " + base);
    m_AskTake.GetRootWidget().SetVisible(!base.IsEmpty());
   }
   WorkspaceWidget ws = GetGame().GetWorkspace();
   float ux = ws.DPIUnscale(x);
   float uy = ws.DPIUnscale(y);
   float screenW = ws.DPIUnscale(ws.GetWidth());
   float screenH = ws.DPIUnscale(ws.GetHeight());
   float h = 440;
   // At the cursor (ACA's picker is hidden: its pings are this menu's first rows), or left of it
   // near the right edge.
   float px = ux + 12;
   if (px + ASK_WIDTH + 20 > screenW - 8) px = ux - 12 - ASK_WIDTH - 20;
   float py = uy + 12;
   if (py + h > screenH - 8) py = uy - 12 - h;
   px = Math.Clamp(px, 8, Math.Max(8, screenW - ASK_WIDTH - 28));
   py = Math.Clamp(py, 8, Math.Max(8, screenH - h - 8));
   FrameSlot.SetPos(m_AskRoot, px, py);
  }
  m_AskRoot.SetVisible(show);
 }

 bool AskShown() { return m_AskRoot && m_AskRoot.IsVisible(); }

 void Destroy()
 {
  array<Widget> roots = {m_CardRoot, m_ChannelRoot, m_AskRoot, m_FobHereRoot, m_EstablishRoot, m_SuggestRoot, m_SiteCardRoot, m_BadgeText};
  foreach (Widget site : m_SiteRoots) roots.Insert(site);
  foreach (Widget pill : m_RingPills) roots.Insert(pill);
  foreach (Widget root : roots)
  {
   if (root) root.RemoveFromHierarchy();
  }
  m_SiteRoots.Clear();
  m_SitePills.Clear();
  m_SiteTexts.Clear();
  m_SiteCardLines.Clear();
  m_RingPills.Clear();
  m_RingTexts.Clear();
  m_CardRoot = null;
  m_ChannelRoot = null;
  m_AskRoot = null;
  m_FobHereRoot = null;
  m_EstablishRoot = null;
  m_SiteCardRoot = null;
  m_BadgeText = null;
  m_AcceptButtons.Clear();
  m_ChannelLines.Clear();
  m_ChannelTimes.Clear();
  m_AnswerTimes.Clear();
  m_ChannelAnswers.Clear();
  m_AnswerRows.Clear();
  m_ButtonRows.Clear();
  m_ChipRoots.Clear();
  m_ChipTexts.Clear();
  m_ChannelBlocks.Clear();
  m_BlockEdges.Clear();
  m_BlockFills.Clear();
  m_BlockPads.Clear();
  m_KeepButtons.Clear();
  m_CancelButtons.Clear();
  m_ReleaseButtons.Clear();
  m_ShowButtons.Clear();
  m_BuyButtons.Clear();
  m_BuyWhys.Clear();
  m_LineActions.Clear();
  m_AskKinds.Clear();
  m_PingKinds.Clear();
  m_FilterButtons.Clear();
  m_Filters.Clear();
 }

 void SetVisible(bool shown)
 {
  if (m_CardRoot) m_CardRoot.SetVisible(shown);
  if (m_ChannelRoot) m_ChannelRoot.SetVisible(shown);
  if (!shown && m_AskRoot) m_AskRoot.SetVisible(false);
  if (!shown && m_SuggestRoot) m_SuggestRoot.SetVisible(false);
  if (!shown && m_SiteCardRoot) m_SiteCardRoot.SetVisible(false);
  if (!shown) ShowRingPills(null, null, null);
 }

 bool IsVisible()
 {
  return m_CardRoot && m_CardRoot.IsVisible();
 }

 // The channel hides while a FOB is placed (sketch 2: only the banner and the sites).
 void ShowChannel(bool shown)
 {
  if (m_ChannelRoot && m_ChannelRoot.IsVisible() != shown) m_ChannelRoot.SetVisible(shown);
 }

 bool ChannelShown() { return m_ChannelRoot && m_ChannelRoot.IsVisible(); }

 // numbers: ACR_TwoICCard.Pack (running, squads, of, FOBs, of, reserve, max, share %).
 void SetCard(array<int> numbers, string doing)
 {
  if (!m_RunsValue || !numbers || numbers.Count() < 8) return;
  string key = "";
  foreach (int n : numbers) key += n.ToString() + "|";
  key += doing;
  if (key == m_sCardShown) return;
  m_sCardShown = key;
  bool running = numbers[0] != 0;
  // The three lines and the pill.
  if (m_CompactStats) m_CompactStats.SetText(ACR_TwoICCompact.Stats(numbers));
  string first = ACR_TwoICCompact.Doing(doing);
  if (m_CompactDoing)
  {
   m_CompactDoing.SetText(first);
   m_CompactDoing.SetVisible(!first.IsEmpty());
  }
  if (m_PillText) m_PillText.SetText(ACR_TwoICCompact.Pill(numbers));
  // The full card.
  if (running) m_RunsValue.SetText(string.Format("%1 of %2 squads", numbers[1], numbers[2]));
  else m_RunsValue.SetText("Stands by");
  m_ManagesValue.SetText(ACR_TwoICCompact.Fobs(numbers[3]));
  int share = numbers[7];
  if (share > 0)
  {
   m_ReserveLabel.SetText(string.Format("Reserve (%1%% of income)", share));
   m_ReserveValue.SetText(string.Format("%1 / %2 RP", numbers[5], numbers[6]));
  }
  else
  {
   m_ReserveLabel.SetText("Reserve off");
   m_ReserveValue.SetText("builds nothing");
  }
  float filled = 0;
  if (share > 0 && numbers[6] > 0) filled = Math.Clamp(numbers[5] / (float)numbers[6], 0, 1);
  m_BarFill.SetWidthOverride(Math.Max(1, BAR_WIDTH * filled));
  m_BarRest.SetWidthOverride(Math.Max(1, BAR_WIDTH * (1 - filled)));
  if (m_MiniFill) m_MiniFill.SetWidthOverride(Math.Max(1, MINI_BAR * filled));
  if (m_MiniRest) m_MiniRest.SetWidthOverride(Math.Max(1, MINI_BAR * (1 - filled)));
  string said = doing;
  if (!said.IsEmpty())
  {
   string initial = said.Substring(0, 1);
   initial.ToUpper();
   said = initial + said.Substring(1, said.Length() - 1);
  }
  if (running && !said.IsEmpty() && !said.EndsWith(".")) said += ".";
  m_Doing.SetText(said);
 }

 // The rows to show (ACR_TwoICCompact.Rows / Pick), top to bottom: what needs the player, then the
 // newest lines, newest first. ages: seconds at send time; since: seconds since then; clock: the
 // server's clock at send time (s, 0 unknown: the ages are shown); flags: a request's buttons + 16
 // x the player who asked (he and the commander get them). The player's own name reads "You".
 void SetChannel(array<int> ids, array<int> kinds, array<string> texts, array<string> answers, array<string> statuses, array<int> ages, array<int> flags, int since, int myId, string myName, bool commander, int clock = 0)
 {
  if (!m_ChannelEmpty || !kinds || !ids || ids.Count() != kinds.Count()) return;
  bool more = m_bTestHover || UnderMouse(m_ChannelRoot) || m_iFilter != ACR_ETwoICFilter.ALL;
  int shown = ACR_TwoICCompact.SHOWN;
  if (more) shown = ACR_TwoICCompact.SHOWN_MORE;
  array<ref ACR_TwoICRow> rows = {};
  ACR_TwoICCompact.Rows(ids, kinds, texts, answers, statuses, ages, flags, m_iFilter, myId, commander, rows);
  array<ACR_TwoICRow> picked = {};
  int cut = 0;
  if (m_iFilter == ACR_ETwoICFilter.ALL) cut = ACR_TwoICCompact.PickAll(rows, shown, myId, commander, picked);
  else ACR_TwoICCompact.Pick(rows, shown, myId, commander, picked);
  UpdateOpenRow();
  string key = string.Format("%1|%2|%3|%4|%5|%6|%7|", m_iFilter, commander, myId, clock, shown, cut, m_iOpenId);
  foreach (ACR_TwoICRow k : picked) key += string.Format("%1;%2;%3;%4;%5;%6;%7|", k.m_iId, When(k.m_iAge, since, clock), k.m_sText, k.m_sAnswer, k.m_sStatus, k.m_iFlags, ACR_TwoICCompact.Range(k, since, clock));
  if (key == m_sChannelShown) return;
  m_sChannelShown = key;
  m_ChannelEmpty.SetVisible(picked.IsEmpty());
  m_iShownRows = picked.Count();
  m_iShownPinned = 0;
  m_iShownOneLine = 0;
  m_iMore = cut;
  if (m_MoreButton)
  {
   m_MoreButton.GetRootWidget().GetParent().SetVisible(cut > 0);
   m_MoreButton.SetText(string.Format("+%1 more", cut));
  }
  // On Requests (and Suggestions) each line is a block of its own, as the sketch's request cards.
  bool blocks = m_iFilter != ACR_ETwoICFilter.ALL;
  foreach (int n, Widget block : m_ChannelBlocks)
  {
   bool used = n < picked.Count();
   block.SetVisible(used);
   if (!used) continue;
   if (picked[n].m_bPinned) m_iShownPinned++;
   bool oneLine = ACR_TwoICCompact.OneLine(picked[n], m_iFilter, m_iOpenId);
   if (oneLine) m_iShownOneLine++;
   ShowRow(n, picked[n], blocks, since, clock, myName, Actionable(picked[n], myId, commander), oneLine);
  }
 }

 // The open request under the cursor shows its buttons, until 300 ms after the cursor left it.
 protected void UpdateOpenRow()
 {
  int now = System.GetTickCount();
  int under = 0;
  foreach (int n, Widget block : m_ChannelBlocks)
  {
   if (block.IsVisible() && UnderMouse(block)) under = m_BlockIds[n];
  }
  if (under > 0)
  {
   m_iOpenId = under;
   m_iOpenAt = now;
  }
  else if (m_iOpenId != 0 && now - m_iOpenAt > 300) m_iOpenId = 0;
 }

 protected void OnMore(SCR_ButtonBaseComponent button)
 {
  SetFilter(ACR_ETwoICFilter.REQUESTS);
 }

 int ShownOneLine() { return m_iShownOneLine; }
 int ShownMore() { return m_iMore; }

 protected static bool Actionable(ACR_TwoICRow row, int myId, bool commander)
 {
  return ACR_TwoICCompact.Actionable(row.m_iKind, row.m_iFlags, myId, commander);
 }

 // Block n shows the row: pinned (an orange edge, "Needs you"), routine (dimmed), or a request
 // or suggestion as before; mayAct: its buttons are this player's.
 protected void ShowRow(int n, ACR_TwoICRow row, bool blocks, int since, int clock, string myName, bool mayAct, bool oneLine = false)
 {
  // One line: no answer, no buttons (they show for the line under the cursor).
  if (oneLine) mayAct = false;
  m_BlockIds[n] = row.m_iId;
  bool pinned = row.m_bPinned;
  m_BlockEdges[n].SetVisible(pinned);
  m_BlockFills[n].SetVisible(pinned || blocks);
  if (pinned) m_BlockFills[n].SetColor(ACR_TwoICStyle.PinnedFill());
  else m_BlockFills[n].SetColor(ACR_TwoICStyle.Block());
  if (pinned || blocks) AlignableSlot.SetPadding(m_BlockPads[n], 12, 8, 12, 10);
  else AlignableSlot.SetPadding(m_BlockPads[n], 0, 2, 0, 2);
  string time = When(row.m_iAge, since, clock);
  if (m_ChannelTimes[n])
  {
   m_ChannelTimes[n].SetText(time);
   if (row.m_bRoutine) m_ChannelTimes[n].SetColor(ACR_TwoICStyle.Dim());
   else m_ChannelTimes[n].SetColor(ACR_TwoICStyle.Grey());
  }
  int kind = row.m_iKind;
  string text;
  Color color = ACR_TwoICStyle.White();
  if (row.m_bRoutine)
  {
   text = ACR_TwoICCompact.RoutineText(row.m_sText);
   string range = ACR_TwoICCompact.Range(row, since, clock);
   if (!range.IsEmpty()) text += ACR_TwoICStyle.Colored(range, ACR_TwoICStyle.Dim());
   color = ACR_TwoICStyle.Grey();
  }
  else if (pinned && kind == ACR_ETwoICLine.SUGGESTION) text = Said(ACR_TwoICCompact.NeedText(row.m_sAnswer));
  else text = Line(kind, row.m_sText, myName);
  if (m_ChannelLines[n])
  {
   m_ChannelLines[n].SetText(text);
   m_ChannelLines[n].SetColor(color);
  }
  string status = row.m_sStatus;
  if (pinned) status = ACR_TwoICCompact.NEEDS_YOU;
  if (row.m_bRoutine) status = "";
  ACR_TwoICStyle.SetChip(m_ChipRoots[n], m_ChipTexts[n], status);
  int actions = row.m_iFlags % 16;
  bool accept = mayAct && (actions & ACR_TwoICSuggest.ACT_ACCEPT) != 0;
  // The answer as a line of its own; an open suggestion's reasons are on its card on the map, and a
  // pinned one's line says what it needs.
  string answer = row.m_sAnswer;
  if (kind == ACR_ETwoICLine.SUGGESTION) answer = ACR_TwoICCompact.Reasons(answer);
  bool answered = !oneLine && !answer.IsEmpty() && !row.m_bRoutine && !(kind == ACR_ETwoICLine.SUGGESTION && (accept || pinned));
  m_AnswerRows[n].SetVisible(answered);
  if (answered)
  {
   if (m_ChannelAnswers[n]) m_ChannelAnswers[n].SetText(Said(answer));
   if (m_AnswerTimes[n]) m_AnswerTimes[n].SetText(time);
  }
  bool keep = mayAct && (actions & ACR_TwoICRequests.ACT_KEEP) != 0;
  bool cancel = mayAct && (actions & ACR_TwoICRequests.ACT_CANCEL) != 0;
  bool release = mayAct && (actions & ACR_TwoICRequests.ACT_RELEASE) != 0;
  // No construction truck: "Buy a truck" (the player's requisition) instead of Send truck, which
  // comes back once a truck exists (ACR_TwoICSuggest.TruckReady).
  bool buy = pinned && accept && kind == ACR_ETwoICLine.SUGGESTION && ACR_TwoICCompact.TruckState(row.m_sAnswer) == ACR_TwoICCompact.TRUCK_BUY;
  if (buy) accept = false;
  ShowBuy(n, buy);
  ShowButton(m_AcceptButtons[n], accept);
  ShowButton(m_ShowButtons[n], kind == ACR_ETwoICLine.SUGGESTION && (actions & ACR_TwoICSuggest.ACT_ACCEPT) != 0 && !pinned);
  ShowButton(m_KeepButtons[n], keep);
  ShowButton(m_CancelButtons[n], cancel);
  if (m_CancelButtons[n])
  {
   if (kind == ACR_ETwoICLine.SUGGESTION) m_CancelButtons[n].SetText("Dismiss");
   else m_CancelButtons[n].SetText("Cancel");
  }
  ShowButton(m_ReleaseButtons[n], release);
  m_ButtonRows[n].SetVisible(accept || buy || keep || cancel || release);
 }

 // Block n's "Buy a truck (21 RP)": greyed out, with the reason under it, while the side can't buy one.
 protected void ShowBuy(int n, bool shown)
 {
  SCR_ButtonTextComponent button = m_BuyButtons[n];
  TextWidget why = m_BuyWhys[n];
  ShowButton(button, shown);
  bool can = m_iTruckCost >= 0 && m_sTruckWhy.IsEmpty();
  if (why)
  {
   why.SetVisible(shown && !m_sTruckWhy.IsEmpty());
   why.SetText("Can't buy one now: " + m_sTruckWhy);
  }
  if (!shown || !button) return;
  int look = ACR_TwoICStyle.BUTTON_OFF;
  if (can) look = ACR_TwoICStyle.BUTTON_FILL;
  ACR_TwoICStyle.Restyle(button, look, 15);
  string label = ACR_TwoICCompact.BuyLabel(m_iTruckCost, m_sTruckBase);
  button.SetText(label);
  // As wide as "Buy a truck at Le Moule Sawmill (21 RP)" needs.
  SizeLayoutWidget holder = SizeLayoutWidget.Cast(button.GetRootWidget().GetParent());
  if (holder) holder.SetWidthOverride(Math.Max(184, 28 + 8 * label.Length()));
  button.SetEnabled(can);
 }

 // The server's offer for the pinned no-truck line (cost -1: none, or not known yet); base: where
 // the truck deploys.
 void SetTruckOffer(int cost, string base, string why)
 {
  if (cost == m_iTruckCost && base == m_sTruckBase && why == m_sTruckWhy) return;
  m_iTruckCost = cost;
  m_sTruckBase = base;
  m_sTruckWhy = why;
  m_sChannelShown = "-";
 }

 bool BuyShown(out bool enabled, out string label)
 {
  enabled = false;
  label = "";
  foreach (int n, SCR_ButtonTextComponent button : m_BuyButtons)
  {
   if (!button || !button.GetRootWidget().GetParent().IsVisible() || !m_ChannelBlocks[n].IsVisible()) continue;
   enabled = button.IsEnabled();
   label = button.GetText();
   return true;
  }
  return false;
 }

 protected void OnBuyTruck(SCR_ButtonBaseComponent button)
 {
  if (m_Owner && m_iTruckCost >= 0 && m_sTruckWhy.IsEmpty()) m_Owner.ACR_TwoICBuyTruck();
 }

 int ShownRows() { return m_iShownRows; }
 int ShownPinned() { return m_iShownPinned; }

 // A line as the sketch writes it: "You:" (or another player's name) in blue before a request,
 // "2IC:" or "2IC suggests:" in the accent before the 2IC's own words.
 static string Line(int kind, string text, string myName)
 {
  if (kind == ACR_ETwoICLine.SUGGESTION)
  {
   if (text.StartsWith("2IC suggests:")) return ACR_TwoICStyle.Colored("2IC suggests:", ACR_TwoICStyle.Accent()) + text.Substring(13, text.Length() - 13);
   return ACR_TwoICStyle.Colored("2IC suggests:", ACR_TwoICStyle.Accent()) + " " + text;
  }
  if (kind == ACR_ETwoICLine.REQUEST)
  {
   int colon = text.IndexOf(": ");
   if (colon > 0)
   {
    string who = text.Substring(0, colon);
    if (!myName.IsEmpty() && who == myName) who = "You";
    return ACR_TwoICStyle.Colored(who + ":", ACR_TwoICStyle.Blue()) + text.Substring(colon + 1, text.Length() - colon - 1);
   }
   return text;
  }
  return Said(text);
 }

 // The 2IC's words: "2IC:" in the accent.
 static string Said(string text)
 {
  if (text.StartsWith("2IC: ")) text = text.Substring(5, text.Length() - 5);
  return ACR_TwoICStyle.Colored("2IC:", ACR_TwoICStyle.Accent()) + " " + text;
 }

 // A line's time: the clock when it was said ("34:05"), else its age ("4 min").
 static string When(int age, int since, int clock)
 {
  if (clock > 0) return Clock(Math.Max(0, clock - age));
  return Age(age + since);
 }

 // "34:05", "1:02:10".
 static string Clock(int seconds)
 {
  int hours = seconds / 3600;
  int minutes = (seconds / 60) % 60;
  int secs = seconds % 60;
  string mm = minutes.ToString();
  if (minutes < 10 && hours > 0) mm = "0" + mm;
  string ss = secs.ToString();
  if (secs < 10) ss = "0" + ss;
  if (hours > 0) return string.Format("%1:%2:%3", hours, mm, ss);
  return string.Format("%1:%2", mm, ss);
 }

 protected void ShowButton(SCR_ButtonTextComponent button, bool shown)
 {
  if (button) button.GetRootWidget().GetParent().SetVisible(shown);
 }

 protected SCR_ButtonTextComponent LineButton(Widget parent, string text, int block, int action, int look, float width)
 {
  SCR_ButtonTextComponent button = ACR_TwoICStyle.Button(parent, text, look, width, 15, 36);
  if (!button) return null;
  AlignableSlot.SetPadding(button.GetRootWidget().GetParent(), 0, 0, 8, 0);
  button.m_OnClicked.Insert(OnLineButton);
  // block and action in one int: the actions go up to ACT_ACCEPT (8), so 16 a block (8 made the
  // channel's Accept act as nothing on the next line).
  m_LineActions.Insert(button, block * 16 + action);
  button.GetRootWidget().GetParent().SetVisible(false);
  return button;
 }

 protected void OnShow(SCR_ButtonBaseComponent button)
 {
  if (m_Owner) m_Owner.ACR_TwoICShowSuggestion();
 }

 protected void OnLineButton(SCR_ButtonBaseComponent button)
 {
  int code;
  if (!m_Owner || !m_LineActions.Find(button, code)) return;
  int block = code / 16;
  int action = code % 16;
  if (block >= m_BlockIds.Count() || m_BlockIds[block] <= 0) return;
  m_Owner.ACR_TwoICAct(m_BlockIds[block], action);
  if (action == ACR_TwoICSuggest.ACT_ACCEPT || action == ACR_TwoICRequests.ACT_CANCEL) Answered(m_BlockIds[block]);
 }

 protected SCR_ButtonTextComponent AskButton(Widget parent, string text, int kind)
 {
  SCR_ButtonTextComponent button = ACR_TwoICStyle.Button(parent, text, ACR_TwoICStyle.BUTTON_MENU, 0, 19, 46);
  if (!button) return null;
  button.m_OnClicked.Insert(OnAsk);
  m_AskKinds.Insert(button, kind);
  return button;
 }

 protected SCR_ButtonTextComponent PingButton(Widget parent, string text, int kind)
 {
  SCR_ButtonTextComponent button = ACR_TwoICStyle.Button(parent, text, ACR_TwoICStyle.BUTTON_MENU, 0, 19, 46);
  if (!button) return null;
  button.m_OnClicked.Insert(OnPing);
  m_PingKinds.Insert(button, kind);
  return button;
 }

 protected void OnPing(SCR_ButtonBaseComponent button)
 {
  int kind;
  if (m_Owner && m_PingKinds.Find(button, kind)) m_Owner.ACR_TwoICPing(kind);
 }

 protected void OnAsk(SCR_ButtonBaseComponent button)
 {
  int kind;
  if (m_Owner && m_AskKinds.Find(button, kind)) m_Owner.ACR_TwoICAsk(kind);
 }

 // "now", "4 min", "1 h 5 min".
 static string Age(int seconds)
 {
  if (seconds < 60) return "now";
  int minutes = seconds / 60;
  if (minutes < 60) return string.Format("%1 min", minutes);
  return string.Format("%1 h %2 min", minutes / 60, minutes % 60);
 }

 // The card's height (layout units), for the feed under it: 0 while hidden. The full card opens
 // over the feed, so the feed keeps to the three-line card's height.
 float CardHeight()
 {
  if (!IsVisible()) return 0;
  if (m_iCardForm == FORM_FULL && m_fCompactHeight > 0) return m_fCompactHeight;
  float w, h;
  m_CardRoot.GetScreenSize(w, h);
  float height = GetGame().GetWorkspace().DPIUnscale(h);
  if (m_iCardForm == FORM_COMPACT && height > 0) m_fCompactHeight = height;
  return height;
 }

 // The channel sits above the debug panel when that shows.
 void PlaceChannel(Widget debugRoot)
 {
  if (!m_ChannelRoot) return;
  float bottom = BOTTOM;
  if (debugRoot && debugRoot.IsVisible())
  {
   float w, h;
   debugRoot.GetScreenSize(w, h);
   bottom -= GetGame().GetWorkspace().DPIUnscale(h) + GAP;
  }
  FrameSlot.SetPos(m_ChannelRoot, LEFT, bottom);
 }

 bool Contains(Widget widget)
 {
  while (widget)
  {
   if ((m_CardRoot && widget == m_CardRoot) || (m_ChannelRoot && widget == m_ChannelRoot) || (m_AskRoot && widget == m_AskRoot) || (m_EstablishRoot && widget == m_EstablishRoot)) return true;
   if ((m_SuggestRoot && widget == m_SuggestRoot) || (m_SiteCardRoot && widget == m_SiteCardRoot)) return true;
   widget = widget.GetParent();
  }
  return false;
 }

 protected void OnPanel(SCR_ButtonBaseComponent button)
 {
  if (m_Owner) m_Owner.ACR_TwoICTogglePanel();
 }

 protected void OnFilter(SCR_ButtonBaseComponent button)
 {
  int filter;
  if (m_Filters.Find(button, filter)) SetFilter(filter);
 }

 // The channel's filter (an ACR_ETwoICFilter), as its pills set it.
 void SetFilter(int filter)
 {
  m_iFilter = filter;
  m_sChannelShown = "-";
  ShowFilter();
 }

 protected void ShowFilter()
 {
  foreach (int i, SCR_ButtonTextComponent button : m_FilterButtons)
  {
   if (!button) continue;
   int look = ACR_TwoICStyle.BUTTON_PILL;
   if (i == m_iFilter) look = ACR_TwoICStyle.BUTTON_PILL_ON;
   ACR_TwoICStyle.Restyle(button, look, 14);
  }
 }
}

// The 2IC's client state and its map hooks.
modded class SCR_PlayerController
{
 protected bool m_ACR_TwoICOn;                     // the server's 2IC is on (it sends the card)
 protected bool m_ACR_TwoICShown;                  // the tool bar's 2IC button, kept for the match
 protected ref array<int> m_ACR_TwoICCard = {};
 protected string m_ACR_TwoICDoing;
 protected ref array<int> m_ACR_TwoICIds = {};
 protected ref array<int> m_ACR_TwoICKinds = {};
 protected ref array<string> m_ACR_TwoICTexts = {};
 protected ref array<string> m_ACR_TwoICAnswers = {};
 protected ref array<string> m_ACR_TwoICStatuses = {};
 protected ref array<int> m_ACR_TwoICAges = {};
 protected ref array<int> m_ACR_TwoICFlags = {};
 protected ref array<ref ACR_UnitMarker> m_ACR_TwoICRingMarkers = {};
 protected bool m_ACR_TwoICEstablishing;            // Establish FOB armed (its sites and the FOB here card)
 protected string m_ACR_TwoICFobHere;
 protected vector m_ACR_TwoICFobHerePos;
 protected int m_ACR_TwoICFobHereSent;
 protected int m_ACR_TwoICHoverSite = -1;          // the FOB site whose card is open
 protected int m_ACR_TwoICHoverAt;
 protected int m_ACR_TwoICForcedSite = -1;         // tests: that site's card, as if under the cursor
 protected int m_ACR_TwoICTruckCost = -1;          // "Buy a truck": its price (-1 none)
 protected string m_ACR_TwoICTruckBase;            // the base it deploys at
 protected string m_ACR_TwoICTruckWhy;             // and why it can't be bought now ("" it can)
 protected int m_ACR_TwoICChannelAt;
 protected int m_ACR_TwoICClock;                   // the server's clock when the channel came (s)
 protected ref ACR_TwoICView m_ACR_TwoICView;
 protected ref ACR_TwoICPanel m_ACR_TwoICPanel;
 protected ref array<RplId> m_ACR_TwoICSquadIds = {};
 protected ref array<string> m_ACR_TwoICSquadNames = {};
 protected ref array<string> m_ACR_TwoICWheres = {};
 protected ref array<string> m_ACR_TwoICDoings = {};
 protected ref array<string> m_ACR_TwoICRunBys = {};
 protected ref array<int> m_ACR_TwoICSquadFlags = {};
 protected ref array<string> m_ACR_TwoICFobs = {};
 protected ref array<int> m_ACR_TwoICFobFlags = {};
 protected int m_ACR_TwoICShare;
 protected string m_ACR_TwoICSmall;

 bool ACR_TwoICOn() { return m_ACR_TwoICOn; }
 bool ACR_TwoICShown() { return m_ACR_TwoICOn && m_ACR_TwoICShown; }

 // The tool bar's button: the new state.
 bool ACR_ToggleTwoIC()
 {
  m_ACR_TwoICShown = !m_ACR_TwoICShown;
  return ACR_TwoICShown();
 }

 // Server: the card (empty numbers: the 2IC is off).
 void ACR_SendTwoICCard(array<int> numbers, string doing)
 {
  Rpc(ACR_TwoICCardOwner, numbers, doing);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_TwoICCardOwner(array<int> numbers, string doing)
 {
  m_ACR_TwoICCard.Copy(numbers);
  m_ACR_TwoICDoing = doing;
  m_ACR_TwoICOn = !numbers.IsEmpty();
 }

 // clock: the server's clock (s, its world time) when sent, for the lines' times.
 void ACR_SendTwoICChannel(array<int> ids, array<int> kinds, array<string> texts, array<string> answers, array<string> statuses, array<int> ages, array<int> flags, int clock)
 {
  Rpc(ACR_TwoICChannelOwner, ids, kinds, texts, answers, statuses, ages, flags, clock);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_TwoICChannelOwner(array<int> ids, array<int> kinds, array<string> texts, array<string> answers, array<string> statuses, array<int> ages, array<int> flags, int clock)
 {
  m_ACR_TwoICClock = clock;
  m_ACR_TwoICIds.Copy(ids);
  m_ACR_TwoICKinds.Copy(kinds);
  m_ACR_TwoICTexts.Copy(texts);
  m_ACR_TwoICAnswers.Copy(answers);
  m_ACR_TwoICStatuses.Copy(statuses);
  m_ACR_TwoICAges.Copy(ages);
  m_ACR_TwoICFlags.Copy(flags);
  m_ACR_TwoICChannelAt = System.GetTickCount();
 }

 // Server: the side's live requests, for their rings on the map.
 void ACR_SendTwoICRings(array<vector> positions, array<string> labels, array<int> kinds)
 {
  Rpc(ACR_TwoICRingsOwner, positions, labels, kinds);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_TwoICRingsOwner(array<vector> positions, array<string> labels, array<int> kinds)
 {
  m_ACR_Data.m_TwoICRings.Copy(positions);
  m_ACR_Data.m_TwoICRingLabels.Copy(labels);
  m_ACR_Data.m_TwoICRingKinds.Copy(kinds);
 }

 // ---------------------------------------------------------------- requests
 // The ask box's buttons (beside the ping picker): at the picker's spot.
 // A ping from the 2IC's menu, as ACA's picker places it.
 void ACR_TwoICPing(int kind)
 {
  ACR_PickPing(kind);
 }

 void ACR_TwoICAsk(int kind)
 {
  Rpc(ACR_TwoICAskServer, kind, m_ACR_PickerPos);
  ACR_ClosePicker();
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_TwoICAskServer(int kind, vector pos)
 {
  Rpc(ACR_FeedbackOwner, ACR_TwoICRequests.Ask(this, kind, pos));
 }

 // Server: the pinned no-truck line's "Buy a truck" offer (ACR_TwoICSuggest.OfferFor).
 void ACR_SendTwoICTruck(int cost, string base, string why)
 {
  Rpc(ACR_TwoICTruckOwner, cost, base, why);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_TwoICTruckOwner(int cost, string base, string why)
 {
  m_ACR_TwoICTruckCost = cost;
  m_ACR_TwoICTruckBase = base;
  m_ACR_TwoICTruckWhy = why;
 }

 // "Buy a truck": the player's own requisition of a construction truck, through the Requisition
 // menu's purchase (ACR_TwoICSuggest.BuyTruck); the 2IC never buys one by itself.
 void ACR_TwoICBuyTruck()
 {
  Rpc(ACR_TwoICBuyTruckServer);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_TwoICBuyTruckServer()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  Faction faction = SCR_FactionManager.SGetPlayerFaction(GetPlayerId());
  if (!manager || !faction) return;
  // The menu's purchase spacing (ACR_PurchaseServer): a double click buys once.
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_ACR_LastCommand < 300) return;
  m_ACR_LastCommand = now;
  Rpc(ACR_FeedbackOwner, ACR_TwoICSuggest.BuyTruck(manager, this, faction.GetFactionKey()));
 }

 // A channel line's buttons: Keep it queued, Cancel, Release.
 void ACR_TwoICAct(int id, int action)
 {
  Rpc(ACR_TwoICActServer, id, action);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_TwoICActServer(int id, int action)
 {
  Rpc(ACR_FeedbackOwner, ACR_TwoICRequests.Act(this, id, action));
 }

 // ---------------------------------------------------------------- Establish FOB
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_TwoICSitesServer(RplId groupId)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  Faction faction = SCR_FactionManager.SGetPlayerFaction(GetPlayerId());
  if (!manager || !faction) return;
  // The unit establishing it: an ACA squad (a construction truck) or the player's command unit (a
  // group of his side but no ACA squad); only where it is matters.
  // (The sites are only shown, so any unit the id names will do.)
  IEntity unit = SCR_EntityHelper.RplIdToEntity(groupId);
  if (!unit)
  {
   PrintFormat("[ACR] 2IC sites: player %1's selection is gone", GetPlayerId());
   return;
  }
  vector at = unit.GetOrigin();
  SCR_AIGroup group = SCR_AIGroup.Cast(unit);
  if (group && group.GetLeaderEntity()) at = group.GetLeaderEntity().GetOrigin();
  ACR_Squad squad;
  if (group) squad = manager.FindSquadByGroup(group);
  if (squad && squad.GetAnchor()) at = squad.GetAnchor().GetOrigin();
  string front = ACR_TwoICSuggest.AskSites(manager, GetPlayerId(), faction.GetFactionKey(), at);
  PrintFormat("[ACR] 2IC sites: player %1 armed Establish FOB with a %2 at %3: sites behind '%4'", GetPlayerId(), unit.ClassName(), at, front);
 }

 // Server: the 2IC's best FOB sites, numbered, once scored; facts: each one's
 // ACR_TwoICCompact.FACTS numbers (its one-line label and its card).
 void ACR_SendTwoICSites(array<vector> positions, array<string> labels, array<int> facts)
 {
  Rpc(ACR_TwoICSitesOwner, positions, labels, facts);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_TwoICSitesOwner(array<vector> positions, array<string> labels, array<int> facts)
 {
  if (!m_ACR_TwoICEstablishing) return;
  m_ACR_Data.m_TwoICSites.Copy(positions);
  m_ACR_Data.m_TwoICSiteLabels.Copy(labels);
  m_ACR_Data.m_TwoICSiteFacts.Copy(facts);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_TwoICFobHereServer(vector pos)
 {
  string text = ACR_TwoICSuggest.FobHere(GetPlayerId(), pos);
  if (!text.IsEmpty()) Rpc(ACR_TwoICFobHereOwner, text);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_TwoICFobHereOwner(string text)
 {
  if (m_ACR_TwoICEstablishing) m_ACR_TwoICFobHere = text;
 }

 // Each map frame: Establish FOB armed or not; while armed, the cursor's spot is checked (at most
 // every 300 ms, after the cursor moved 8 m) and its one line follows the cursor; over a numbered
 // site, that site's card opens instead (sketch 2).
 protected void ACR_TwoICEstablishTick()
 {
  bool establishing = m_ACR_TwoICOn && m_ACR_CursorMode == ACR_ECursorMode.ESTABLISH_FOB;
  if (establishing != m_ACR_TwoICEstablishing)
  {
   m_ACR_TwoICEstablishing = establishing;
   m_ACR_Data.m_TwoICSites.Clear();
   m_ACR_Data.m_TwoICSiteLabels.Clear();
   m_ACR_Data.m_TwoICSiteFacts.Clear();
   m_ACR_TwoICFobHere = "";
   m_ACR_TwoICFobHerePos = vector.Zero;
   m_ACR_TwoICHoverSite = -1;
   if (establishing) Rpc(ACR_TwoICSitesServer, m_ACR_SelectedGroup);
  }
  m_ACR_Data.m_bTwoICEstablishing = establishing;
  if (m_ACR_TwoICView) m_ACR_TwoICView.ShowEstablish(establishing && ACR_TwoICShown(), ACR_TwoICGroupName(m_ACR_SelectedGroup), !m_ACR_Data.m_TwoICSites.IsEmpty());
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  float x, z;
  int sx, sy;
  if (establishing && mapEntity)
  {
   mapEntity.GetMapCursorWorldPosition(x, z);
   mapEntity.WorldToScreen(x, z, sx, sy, true);
  }
  int hover = -1;
  if (m_ACR_TwoICView)
  {
   array<float> siteXs = {};
   array<float> siteYs = {};
   if (establishing && mapEntity && ACR_TwoICShown())
   {
    // The disc under the cursor (within 24 px of its centre).
    float nearest = GetGame().GetWorkspace().DPIScale(24);
    foreach (int n, vector site : m_ACR_Data.m_TwoICSites)
    {
     int sx0, sy0;
     mapEntity.WorldToScreen(site[0], site[2], sx0, sy0, true);
     siteXs.Insert(sx0);
     siteYs.Insert(sy0);
     float dx = sx0 - sx;
     float dy = sy0 - sy;
     float d = Math.Sqrt(dx * dx + dy * dy);
     if (d > nearest) continue;
     nearest = d;
     hover = n;
    }
    hover = ACR_TwoICKeepHover(hover);
   }
   m_ACR_TwoICView.ShowSites(siteXs, siteYs, m_ACR_Data.m_TwoICSiteFacts, hover);
  }
  if (!establishing || !mapEntity || hover >= 0 || ACR_OverOwnUI(WidgetManager.GetWidgetUnderCursor()))
  {
   if (m_ACR_TwoICView) m_ACR_TwoICView.ShowFobHere(false, "", 0, 0);
   return;
  }
  vector pos = Vector(x, 0, z);
  int now = System.GetTickCount();
  if (now - m_ACR_TwoICFobHereSent >= 300 && vector.DistanceXZ(pos, m_ACR_TwoICFobHerePos) >= 8)
  {
   m_ACR_TwoICFobHereSent = now;
   m_ACR_TwoICFobHerePos = pos;
   Rpc(ACR_TwoICFobHereServer, pos);
  }
  if (m_ACR_TwoICView) m_ACR_TwoICView.ShowFobHere(!m_ACR_TwoICFobHere.IsEmpty(), m_ACR_TwoICFobHere, sx, sy);
 }

 // The site whose card is open stays open while the cursor is on the card, and 400 ms after the
 // cursor left its disc (on its way to the card's Build here). hover: the disc under the cursor.
 protected int ACR_TwoICKeepHover(int hover)
 {
  if (m_ACR_TwoICForcedSite >= 0 && m_ACR_TwoICForcedSite < m_ACR_Data.m_TwoICSites.Count()) return m_ACR_TwoICForcedSite;
  int now = System.GetTickCount();
  if (hover >= 0)
  {
   m_ACR_TwoICHoverSite = hover;
   m_ACR_TwoICHoverAt = now;
   return hover;
  }
  if (m_ACR_TwoICHoverSite < 0 || m_ACR_TwoICHoverSite >= m_ACR_Data.m_TwoICSites.Count()) return -1;
  if (m_ACR_TwoICView && m_ACR_TwoICView.SiteCardUnderMouse()) m_ACR_TwoICHoverAt = now;
  if (now - m_ACR_TwoICHoverAt <= 400) return m_ACR_TwoICHoverSite;
  m_ACR_TwoICHoverSite = -1;
  return -1;
 }

 // A site card's Build here: Establish FOB there, as a right-click on its disc would.
 void ACR_TwoICBuildSite(int site)
 {
  ACR_MarkUiClick();
  if (site < 0 || site >= m_ACR_Data.m_TwoICSites.Count() || m_ACR_CursorMode != ACR_ECursorMode.ESTABLISH_FOB) return;
  if (!m_ACR_IsCommander)
  {
   SCR_HintManagerComponent.ShowCustomHint("Only the commander can give map orders. Take command in the panel.", "AC: Actual", 4, true);
   return;
  }
  ACR_IssueOrder(ACR_ERequest.ORDER_ESTABLISH_FOB, m_ACR_Data.m_TwoICSites[site]);
 }

 // The suggestion's card by its marker: the newest Suggestions line with Accept (the commander's),
 // at the side's suggestion ring; hidden while Establish FOB is armed (its own sites show then)
 // and while the panel is open.
 protected void ACR_TwoICSuggestionCard()
 {
  int id;
  string reasons;
  string front;
  for (int i = m_ACR_TwoICKinds.Count() - 1; i >= 0 && id == 0; i--)
  {
   if (m_ACR_TwoICKinds[i] != ACR_ETwoICLine.SUGGESTION || i >= m_ACR_TwoICFlags.Count()) continue;
   if (((m_ACR_TwoICFlags[i] % 16) & ACR_TwoICSuggest.ACT_ACCEPT) == 0 || !m_ACR_IsCommander) continue;
   if (m_ACR_TwoICView && m_ACR_TwoICIds[i] == m_ACR_TwoICView.AnsweredId()) continue;
   id = m_ACR_TwoICIds[i];
   if (i < m_ACR_TwoICAnswers.Count()) reasons = ACR_TwoICCompact.Reasons(m_ACR_TwoICAnswers[i]);
   if (i < m_ACR_TwoICTexts.Count()) front = ACR_TwoICCompact.SuggestTitle(m_ACR_TwoICTexts[i]);
  }
  int ring = -1;
  foreach (int r, int kind : m_ACR_Data.m_TwoICRingKinds)
  {
   if (kind == ACR_TwoICSuggest.RING_KIND && ring < 0 && r < m_ACR_Data.m_TwoICRings.Count()) ring = r;
  }
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (id == 0 || ring < 0 || !mapEntity || m_ACR_TwoICEstablishing || m_ACR_TwoICPanel)
  {
   m_ACR_TwoICView.ShowSuggestion(0, "", 0, 0);
  }
  else
  {
   vector at = m_ACR_Data.m_TwoICRings[ring];
   int sx, sy;
   mapEntity.WorldToScreen(at[0], at[2], sx, sy, true);
   m_ACR_TwoICView.ShowSuggestion(id, reasons, sx, sy, front);
  }
  ACR_TwoICRingPills(mapEntity);
 }

 // A suggested site's label while its card isn't shown, and a site a truck is on its way to
 // ("FOB site · truck on the way · about 4 min", sketch 3).
 protected void ACR_TwoICRingPills(SCR_MapEntity mapEntity)
 {
  array<string> labels = {};
  array<float> xs = {};
  array<float> ys = {};
  bool card = m_ACR_TwoICView.SuggestionShown();
  if (mapEntity)
  {
   foreach (int r, int kind : m_ACR_Data.m_TwoICRingKinds)
   {
    if (r >= m_ACR_Data.m_TwoICRings.Count() || r >= m_ACR_Data.m_TwoICRingLabels.Count()) break;
    bool pill = kind == ACR_TwoICSuggest.RING_SENT || (kind == ACR_TwoICSuggest.RING_KIND && !card && !m_ACR_TwoICEstablishing);
    if (!pill) continue;
    vector at = m_ACR_Data.m_TwoICRings[r];
    int sx, sy;
    mapEntity.WorldToScreen(at[0], at[2], sx, sy, true);
    labels.Insert(m_ACR_Data.m_TwoICRingLabels[r]);
    xs.Insert(sx);
    ys.Insert(sy);
   }
  }
  m_ACR_TwoICView.ShowRingPills(labels, xs, ys);
 }

 // The 2IC panel's "Show" on a squad's row (playtest 30 Oct: finding a squad on a crowded map was
 // hard): the panel closes, the squad is selected (its icon stands on its own, out of any "×N"), the
 // map pans to it and a pulse marks it (ACR_FocusMap).
 void ACR_TwoICShowSquad(RplId groupId)
 {
  if (!m_ACR_Data) return;
  int g = m_ACR_Data.m_GroupIds.Find(groupId);
  if (g < 0) return;
  vector at = m_ACR_Data.LeaderPos(g);
  if (m_ACR_TwoICPanel) ACR_TwoICTogglePanel();
  ACR_SelectGroup(groupId);
  ACR_FocusMap(at);
 }

 // The panel's "Show" on a FOB's row: the same, for the FOB's base marker.
 void ACR_TwoICShowFob(string label)
 {
  if (!m_ACR_Data) return;
  foreach (int b, string name : m_ACR_Data.m_BaseNames)
  {
   if (name != label && ACR_MapDeclutter.ShortName(name) != label) continue;
   if (b >= m_ACR_Data.m_BasePositions.Count()) return;
   if (m_ACR_TwoICPanel) ACR_TwoICTogglePanel();
   ACR_FocusMap(m_ACR_Data.m_BasePositions[b]);
   return;
  }
 }

 bool ACR_TwoICPanelOpen() { return m_ACR_TwoICPanel != null; }

 // The channel's "Show" on an open suggestion: the map pans to its marker (its ring), at the same zoom.
 void ACR_TwoICShowSuggestion()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen() || !m_ACR_Data) return;
  foreach (int r, int kind : m_ACR_Data.m_TwoICRingKinds)
  {
   if (kind != ACR_TwoICSuggest.RING_KIND || r >= m_ACR_Data.m_TwoICRings.Count()) continue;
   vector at = m_ACR_Data.m_TwoICRings[r];
   mapEntity.ZoomPanSmooth(mapEntity.GetCurrentZoom(), at[0], at[2], 0.3);
   ACR_MarkUiClick();
   return;
  }
 }

 // The banner's Cancel: Establish FOB disarmed.
 void ACR_TwoICCancelEstablish()
 {
  if (m_ACR_CursorMode == ACR_ECursorMode.ESTABLISH_FOB) m_ACR_CursorMode = ACR_ECursorMode.MOVE;
 }

 // A group's name as the map data has it ("" unknown).
 protected string ACR_TwoICGroupName(RplId id)
 {
  int index = m_ACR_Data.m_GroupIds.Find(id);
  if (index < 0 || index >= m_ACR_Data.m_GroupNames.Count()) return "";
  return m_ACR_Data.m_GroupNames[index];
 }

 // Alt+right-click: with the 2IC shown, one menu (sketch 3): ACA's pings, then the asks. ACA's own
 // picker is hidden but stays open, so 1-4 and its 8 s close work as before.
 override protected void ACR_OpenPicker(SCR_MapEntity mapEntity, vector pos)
 {
  super.ACR_OpenPicker(mapEntity, pos);
  if (!m_ACR_TwoICView || !ACR_TwoICShown() || !m_ACR_PickerOpen) return;
  if (m_ACR_Panel) m_ACR_Panel.ShowPicker(false, 0, 0);
  int sx, sy;
  mapEntity.WorldToScreen(pos[0], pos[2], sx, sy, true);
  m_ACR_TwoICView.ShowAsk(true, sx, sy, ACR_TwoICBaseAt(pos));
 }

 override protected void ACR_ClosePicker()
 {
  super.ACR_ClosePicker();
  if (m_ACR_TwoICView) m_ACR_TwoICView.ShowAsk(false, 0, 0, "");
 }

 // The base at the cursor that isn't ours, for "Take this base" ("" none).
 protected string ACR_TwoICBaseAt(vector pos)
 {
  Faction mine = SCR_FactionManager.SGetLocalPlayerFaction();
  string best = "";
  float bestDistance = float.MAX;
  foreach (int i, vector at : m_ACR_Data.m_BasePositions)
  {
   if (i >= m_ACR_Data.m_BaseNames.Count() || i >= m_ACR_Data.m_BaseRadii.Count() || i >= m_ACR_Data.m_BaseOwners.Count()) break;
   float d = vector.DistanceXZ(at, pos);
   if (d > m_ACR_Data.m_BaseRadii[i] + ACR_TwoICRequests.TAKE_RADIUS || d >= bestDistance) continue;
   bestDistance = d;
   best = "";
   if (!mine || m_ACR_Data.m_BaseOwners[i] != mine.GetFactionKey()) best = m_ACR_Data.m_BaseNames[i];
  }
  return best;
 }

 override void ACR_ClearMarkers()
 {
  SCR_MapMarkerManagerComponent manager = SCR_MapMarkerManagerComponent.GetInstance();
  if (manager)
   foreach (ACR_UnitMarker ring : m_ACR_TwoICRingMarkers) manager.ACR_RemoveMarker(ring);
  m_ACR_TwoICRingMarkers.Clear();
  super.ACR_ClearMarkers();
 }

 // The live requests' labels by their rings (the rings: ACR_OrderDisplay). A suggested site and a
 // sent truck's site have labels of their own (ACR_TwoICView.ShowRingPills).
 override protected void ACR_UpdateZoneMarkers(SCR_MapMarkerManagerComponent manager)
 {
  super.ACR_UpdateZoneMarkers(manager);
  array<int> marked = {};
  if (ACR_TwoICShown())
  {
   int rings = Math.Min(m_ACR_Data.m_TwoICRingLabels.Count(), m_ACR_Data.m_TwoICRings.Count());
   for (int r = 0; r < rings; r++)
   {
    int kind = -1;
    if (r < m_ACR_Data.m_TwoICRingKinds.Count()) kind = m_ACR_Data.m_TwoICRingKinds[r];
    if (kind == ACR_TwoICSuggest.RING_KIND || kind == ACR_TwoICSuggest.RING_SENT) continue;
    marked.Insert(r);
   }
  }
  int wanted = marked.Count();
  if (m_ACR_TwoICRingMarkers.Count() != wanted)
  {
   foreach (ACR_UnitMarker old : m_ACR_TwoICRingMarkers) manager.ACR_RemoveMarker(old);
   m_ACR_TwoICRingMarkers.Clear();
   for (int i = 0; i < wanted; i++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    marker.SetType(SCR_EMapMarkerType.SIMPLE);
    marker.SetMarkerConfigID(1);
    marker.SetCanBeRemovedByOwner(false);
    m_ACR_TwoICRingMarkers.Insert(marker);
    manager.InsertStaticMarker(marker, true);
    marker.SetMarkerOwnerID(-1);
   }
  }
  foreach (int n, ACR_UnitMarker ring : m_ACR_TwoICRingMarkers)
  {
   if (n >= marked.Count()) break;
   int at = marked[n];
   ring.m_Label = m_ACR_Data.m_TwoICRingLabels[at];
   vector pos = m_ACR_Data.m_TwoICRings[at];
   ring.SetWorldPos(pos[0], pos[2] - ACR_AICommander.ARRIVED);
   ring.RefreshStyle();
   if (ring.GetRootWidget()) ring.ACR_SetColor(new Color(1.0, 0.62, 0.15, 1));
  }
 }

 // ---------------------------------------------------------------- the panel
 // Server: its rows, only while the player has it open (ACR_TwoIC.SetPanelOpen).
 void ACR_SendTwoICSquads(array<RplId> ids, array<string> names, array<string> wheres, array<string> doings, array<string> runBys, array<int> flags)
 {
  Rpc(ACR_TwoICSquadsOwner, ids, names, wheres, doings, runBys, flags);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_TwoICSquadsOwner(array<RplId> ids, array<string> names, array<string> wheres, array<string> doings, array<string> runBys, array<int> flags)
 {
  m_ACR_TwoICSquadIds.Copy(ids);
  m_ACR_TwoICSquadNames.Copy(names);
  m_ACR_TwoICWheres.Copy(wheres);
  m_ACR_TwoICDoings.Copy(doings);
  m_ACR_TwoICRunBys.Copy(runBys);
  m_ACR_TwoICSquadFlags.Copy(flags);
 }

 void ACR_SendTwoICPanel(array<string> fobs, array<int> fobFlags, int share, string small)
 {
  Rpc(ACR_TwoICPanelOwner, fobs, fobFlags, share, small);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_TwoICPanelOwner(array<string> fobs, array<int> fobFlags, int share, string small)
 {
  m_ACR_TwoICFobs.Copy(fobs);
  m_ACR_TwoICFobFlags.Copy(fobFlags);
  m_ACR_TwoICShare = share;
  m_ACR_TwoICSmall = small;
 }

 // The card's button, the panel's Close and its key.
 void ACR_TwoICTogglePanel()
 {
  if (m_ACR_TwoICPanel)
  {
   ACR_TwoICClosePanel();
   return;
  }
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen() || !mapEntity.GetMapMenuRoot() || !ACR_TwoICShown()) return;
  m_ACR_TwoICPanel = new ACR_TwoICPanel();
  m_ACR_TwoICPanel.Create(mapEntity.GetMapMenuRoot(), this);
  Rpc(ACR_TwoICPanelServer, true);
 }

 protected void ACR_TwoICClosePanel()
 {
  if (!m_ACR_TwoICPanel) return;
  m_ACR_TwoICPanel.Destroy();
  m_ACR_TwoICPanel = null;
  Rpc(ACR_TwoICPanelServer, false);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_TwoICPanelServer(bool open)
 {
  ACR_TwoIC.SetPanelOpen(GetPlayerId(), open);
 }

 // The panel's FOB button: the FOB panel's own toggle (ACR_FobAI.Toggle).
 void ACR_TwoICFob(string label)
 {
  Rpc(ACR_FobAIServer, label);
 }

 // The panel's reserve slider.
 void ACR_TwoICShare(int share)
 {
  Rpc(ACR_TwoICShareServer, share);
 }
 [RplRpc(RplChannel.Reliable, RplRcver.Server)]
 protected void ACR_TwoICShareServer(int share)
 {
  Rpc(ACR_FeedbackOwner, ACR_TwoIC.SetShare(this, share));
 }

 override protected void ACR_OpenPanel(SCR_MapEntity mapEntity)
 {
  super.ACR_OpenPanel(mapEntity);
  Widget root = mapEntity.GetMapMenuRoot();
  if (!root) return;
  m_ACR_TwoICView = new ACR_TwoICView();
  m_ACR_TwoICView.Create(root, this);
 }

 override protected void ACR_ClosePanel()
 {
  ACR_TwoICClosePanel();
  if (m_ACR_TwoICView) m_ACR_TwoICView.Destroy();
  m_ACR_TwoICView = null;
  super.ACR_ClosePanel();
 }

 override protected void ACR_UpdatePanel()
 {
  super.ACR_UpdatePanel();
  if (!m_ACR_TwoICView || !m_ACR_Panel) return;
  bool shown = ACR_TwoICShown();
  m_ACR_Data.m_bTwoICShown = shown;
  if (shown != m_ACR_TwoICView.IsVisible()) m_ACR_TwoICView.SetVisible(shown);
  if (!shown) ACR_TwoICClosePanel();
  ACR_TwoICEstablishTick();
  // Placing a FOB (the declutter sketch 2): only the banner and the sites, the card a pill; ACA's
  // panel and its intel feed hide, and come back after.
  bool placing = shown && m_ACR_TwoICEstablishing;
  m_ACR_Panel.ACR_TwoICPlacing(placing);
  if (shown)
  {
   m_ACR_TwoICView.SetCard(m_ACR_TwoICCard, m_ACR_TwoICDoing);
   m_ACR_TwoICView.UpdateCard(placing || ACR_TwoICZoomedOut());
   m_ACR_TwoICView.ShowChannel(!placing);
   m_ACR_TwoICView.SetTruckOffer(m_ACR_TwoICTruckCost, m_ACR_TwoICTruckBase, m_ACR_TwoICTruckWhy);
   string me = GetGame().GetPlayerManager().GetPlayerName(GetPlayerId());
   m_ACR_TwoICView.SetChannel(m_ACR_TwoICIds, m_ACR_TwoICKinds, m_ACR_TwoICTexts, m_ACR_TwoICAnswers, m_ACR_TwoICStatuses, m_ACR_TwoICAges, m_ACR_TwoICFlags, (System.GetTickCount() - m_ACR_TwoICChannelAt) / 1000, GetPlayerId(), me, m_ACR_IsCommander, m_ACR_TwoICClock);
   m_ACR_TwoICView.PlaceChannel(m_ACR_Panel.DebugRoot());
   ACR_TwoICSuggestionCard();
   InputManager input = GetGame().GetInputManager();
   if (input && input.GetActionTriggered("ACR_TwoICPanel")) ACR_TwoICTogglePanel();
  }
  ACR_TwoICBadge(shown);
  if (m_ACR_TwoICPanel)
  {
   int reserve, reserveMax;
   if (m_ACR_TwoICCard.Count() >= 8)
   {
    reserve = m_ACR_TwoICCard[5];
    reserveMax = m_ACR_TwoICCard[6];
   }
   string myName = GetGame().GetPlayerManager().GetPlayerName(GetPlayerId());
   m_ACR_TwoICPanel.SetData(m_ACR_TwoICSquadIds, m_ACR_TwoICSquadNames, m_ACR_TwoICWheres, m_ACR_TwoICDoings, m_ACR_TwoICRunBys, m_ACR_TwoICSquadFlags, m_ACR_TwoICFobs, m_ACR_TwoICFobFlags, m_ACR_TwoICShare, m_ACR_TwoICSmall, reserve, reserveMax, m_ACR_IsCommander, myName);
  }
  float feedTop = ACR_TwoICView.TOP;
  if (shown) feedTop += m_ACR_TwoICView.CardHeight() + ACR_TwoICView.GAP;
  m_ACR_Panel.SetFeedTop(feedTop);
 }

 // Zoomed out (ACR_TwoICCompact.ZoomedOut): the card is a pill.
 protected bool ACR_TwoICZoomedOut()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity) return false;
  return ACR_TwoICCompact.ZoomedOut(mapEntity.GetCurrentZoom(), mapEntity.GetMinZoom());
 }

 // The 2IC hidden: the tool bar's 2IC button counts the lines that need the player (sketch 4).
 protected void ACR_TwoICBadge(bool shown)
 {
  int count = 0;
  if (m_ACR_TwoICOn && !shown) count = ACR_TwoICCompact.NeedsYouCount(m_ACR_TwoICKinds, m_ACR_TwoICAnswers, m_ACR_TwoICFlags, GetPlayerId(), m_ACR_IsCommander);
  float x, y, size, cx, cy, radius;
  if (count > 0 && SCR_MapToolMenuUI.ACR_TwoICIconRect(x, y, size)) SCR_MapToolMenuUI.ACR_TwoICBadgeAt(x, y, size, cx, cy, radius);
  else count = 0;
  m_ACR_Data.m_iTwoICBadge = count;
  m_ACR_TwoICView.ShowBadge(count, cx, cy);
 }

 // Clicks and the wheel over the 2IC's boxes don't reach the map.
 override protected bool ACR_OverOwnUI(Widget widget)
 {
  if (m_ACR_TwoICView && m_ACR_TwoICView.Contains(widget)) return true;
  if (m_ACR_TwoICPanel && m_ACR_TwoICPanel.Contains(widget)) return true;
  return super.ACR_OverOwnUI(widget);
 }

 // A right-click on the 2IC's boxes orders nothing.
 override bool ACR_HandleMapRightClick()
 {
  Widget under = WidgetManager.GetWidgetUnderCursor();
  if ((m_ACR_TwoICView && m_ACR_TwoICView.Contains(under)) || (m_ACR_TwoICPanel && m_ACR_TwoICPanel.Contains(under))) return true;
  return super.ACR_HandleMapRightClick();
 }

 // The 2IC's squads, while the 2IC shows: an orange "2IC" badge by the symbol (sketch 1) instead
 // of the [AI] tag.
 protected bool ACR_TwoICRuns(int index)
 {
  return ACR_TwoICShown() && index * 4 + 3 < m_ACR_GroupSymbols.Count() && (m_ACR_GroupSymbols[index * 4 + 3] & 1024) != 0;
 }

 override protected string ACR_GroupLabel(int index, string label)
 {
  // The other tags first (a control group's number, ACR_PlayerMapGroups).
  label = super.ACR_GroupLabel(index, label);
  if (!ACR_TwoICRuns(index)) return label;
  label.Replace(" [AI]", "");
  return label;
 }

 override void ACR_UpdateGroupMarkers(SCR_MapMarkerManagerComponent manager)
 {
  super.ACR_UpdateGroupMarkers(manager);
  foreach (int i, ACR_UnitMarker marker : m_ACR_GroupMarkers) marker.ACR_SetTwoICBadge(ACR_TwoICRuns(i));
 }
}

// The small orange square on the top right corner of a 2IC squad's symbol (the declutter sketch 1;
// it was a "2IC" tag): a 1 px dark edge round the accent.
modded class ACR_UnitMarker
{
 static const float BADGE_X = 9;    // from the marker's centre to the square's centre: on the
 static const float BADGE_Y = -20;  // symbol's top right corner
 static const float BADGE_SIZE = 10;
 protected Widget m_wTwoICBadge;

 override void OnCreateMarker(bool skipProfanityFilter = false)
 {
  m_wTwoICBadge = null; // it went with the old root
  super.OnCreateMarker(skipProfanityFilter);
 }

 void ACR_SetTwoICBadge(bool shown)
 {
  if (!m_wRoot) return;
  if (shown && !m_wTwoICBadge)
  {
   WorkspaceWidget ws = GetGame().GetWorkspace();
   if (!ws) return;
   m_wTwoICBadge = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 98, m_wRoot);
   ACR_TwoICStyle.Fill(m_wTwoICBadge, ACR_TwoICStyle.Rgb(24, 27, 30));
   Widget fill = ACR_TwoICStyle.Fill(m_wTwoICBadge, ACR_TwoICStyle.Accent());
   AlignableSlot.SetPadding(fill, 1, 1, 1, 1);
   fill.SetZOrder(1);
   FrameSlot.SetAnchorMin(m_wTwoICBadge, 0.5, 0.5);
   FrameSlot.SetAnchorMax(m_wTwoICBadge, 0.5, 0.5);
   FrameSlot.SetAlignment(m_wTwoICBadge, 0.5, 0.5);
   FrameSlot.SetPos(m_wTwoICBadge, BADGE_X, BADGE_Y);
   FrameSlot.SetSize(m_wTwoICBadge, BADGE_SIZE, BADGE_SIZE);
  }
  if (m_wTwoICBadge) m_wTwoICBadge.SetVisible(shown);
 }
}

// The 2IC's requests on the client's map data, and their rings on the command map's canvas.
modded class ACR_MapData
{
 bool m_bTwoICShown;
 ref array<vector> m_TwoICRings = {};
 ref array<string> m_TwoICRingLabels = {};
 ref array<int> m_TwoICRingKinds = {};
 bool m_bTwoICEstablishing;
 ref array<vector> m_TwoICSites = {};
 ref array<string> m_TwoICSiteLabels = {};
 ref array<int> m_TwoICSiteFacts = {};
 int m_iTwoICBadge;                 // the tool bar's count (0: none)
}

modded class ACR_OrderDisplay
{
 override void Draw(SCR_MapEntity mapEntity, ACR_MapData data, bool boxing, vector boxStart)
 {
  super.Draw(mapEntity, data, boxing, boxStart);
  if (m_Canvas) DrawTwoICIcon(data);
  if (!m_Canvas || !data || !data.m_bTwoICShown) return;
  bool sites = data.m_bTwoICEstablishing && !data.m_TwoICSites.IsEmpty();
  if (data.m_TwoICRings.IsEmpty() && !sites) return;
  Color orangeColor = new Color(1.0, 0.62, 0.15, 0.95);
  int orange = orangeColor.PackToInt();
  foreach (int i, vector pos : data.m_TwoICRings)
  {
   int kind = -1;
   if (i < data.m_TwoICRingKinds.Count()) kind = data.m_TwoICRingKinds[i];
   // The declutter sketch 3: a suggested site is a pale disc with an orange edge; a site a truck is
   // on its way to, a dashed orange ring.
   if (kind == ACR_TwoICSuggest.RING_KIND) TwoICSuggested(mapEntity, pos);
   else if (kind == ACR_TwoICSuggest.RING_SENT) TwoICSent(mapEntity, pos);
   else TwoICRing(mapEntity, pos, orange);
  }
  if (sites)
  {
   // The sketch's numbered sites: the best a filled accent disc, the others pale, a dark edge.
   int best = ACR_TwoICStyle.Accent().PackToInt();
   int other = ACR_TwoICStyle.Rgb(243, 217, 174).PackToInt();
   int edge = ACR_TwoICStyle.OnAccent().PackToInt();
   foreach (int n, vector site : data.m_TwoICSites)
   {
    int fill = other;
    if (n == 0) fill = best;
    TwoICSite(mapEntity, site, fill, edge);
   }
  }
  m_Canvas.SetDrawCommands(m_Commands);
 }

 protected void TwoICSite(SCR_MapEntity mapEntity, vector pos, int fill, int edge)
 {
  int cx, cy;
  mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
  float radius = GetGame().GetWorkspace().DPIScale(16);
  Dot(cx, cy, radius, fill);
  Ring(cx, cy, radius, edge, GetGame().GetWorkspace().DPIScale(2), 24);
 }

 // The 2IC button's icon: Jacob's ACA icon in colour (tools/branding/aca-icon.svg, a 100-unit box:
 // the blue HQ frame with its light edge and staff, two light chevrons), over the tool bar button whose
 // own glyph is hidden, so his blue stands out among the grey vanilla icons. Drawn here, on the map
 // canvas, rather than imported as a texture: no .edds to build, sharp at any size.
 protected void DrawTwoICIcon(ACR_MapData data)
 {
  float ox, oy, size;
  if (!SCR_MapToolMenuUI.ACR_TwoICIconRect(ox, oy, size)) return;
  float u = size / 100;
  int blue = ACR_TwoICStyle.Rgb(31, 79, 150).PackToInt();
  int edge = ACR_TwoICStyle.Rgb(232, 238, 247).PackToInt();
  int chevron = ACR_TwoICStyle.Rgb(241, 239, 230).PackToInt();
  float x0 = ox + 10 * u;
  float y0 = oy + 12 * u;
  float x1 = ox + 90 * u;
  float y1 = oy + 70 * u;
  PolygonDrawCommand fill1 = new PolygonDrawCommand();
  fill1.m_iColor = blue;
  fill1.m_Vertices = {x0, y0, x1, y0, x1, y1};
  m_Commands.Insert(fill1);
  PolygonDrawCommand fill2 = new PolygonDrawCommand();
  fill2.m_iColor = blue;
  fill2.m_Vertices = {x0, y0, x1, y1, x0, y1};
  m_Commands.Insert(fill2);
  LineDrawCommand frame = new LineDrawCommand();
  frame.m_iColor = edge;
  frame.m_fWidth = 4 * u;
  frame.m_bShouldEnclose = true;
  frame.m_Vertices = {x0, y0, x1, y0, x1, y1, x0, y1};
  m_Commands.Insert(frame);
  Line(ox + 12 * u, oy + 70 * u, ox + 12 * u, oy + 92 * u, edge, 5 * u);
  for (int row = 0; row < 2; row++)
  {
   float top = 24 + row * 18;
   LineDrawCommand chevronLine = new LineDrawCommand();
   chevronLine.m_iColor = chevron;
   chevronLine.m_fWidth = 8 * u;
   chevronLine.m_Vertices = {ox + 30 * u, oy + (top + 14) * u, ox + 50 * u, oy + top * u, ox + 70 * u, oy + (top + 14) * u};
   m_Commands.Insert(chevronLine);
  }
  s_iTwoICIconDrawn++;
  // The 2IC hidden and something needs the player: an orange disc on the top right (its number is
  // a widget over it, ACR_TwoICView.ShowBadge), sketch 4.
  if (!data || data.m_iTwoICBadge <= 0) return;
  float cx, cy, radius;
  SCR_MapToolMenuUI.ACR_TwoICBadgeAt(ox, oy, size, cx, cy, radius);
  Dot(cx, cy, radius, ACR_TwoICStyle.Accent().PackToInt());
  Ring(cx, cy, radius, ACR_TwoICStyle.Rgb(24, 27, 30).PackToInt(), GetGame().GetWorkspace().DPIScale(1.5), 20);
  s_iTwoICBadgeDrawn++;
 }

 static int s_iTwoICBadgeDrawn; // frames the count was drawn (twoicshots checks it)

 // A suggested site: a pale disc, an orange edge.
 protected void TwoICSuggested(SCR_MapEntity mapEntity, vector pos)
 {
  int cx, cy;
  mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float radius = ws.DPIScale(14);
  Dot(cx, cy, radius, ACR_TwoICStyle.Rgb(222, 196, 160, 0.9).PackToInt());
  Ring(cx, cy, radius, ACR_TwoICStyle.Accent().PackToInt(), ws.DPIScale(2.5), 28);
 }

 // A site a truck is on its way to: a dashed orange ring.
 protected void TwoICSent(SCR_MapEntity mapEntity, vector pos)
 {
  int cx, cy;
  mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float radius = ws.DPIScale(14);
  float width = ws.DPIScale(2.5);
  int color = ACR_TwoICStyle.Accent().PackToInt();
  int dashes = 14;
  float step = Math.PI2 / dashes;
  for (int d = 0; d < dashes; d++)
  {
   float a0 = d * step;
   float a1 = a0 + step * 0.55;
   Line(cx + Math.Cos(a0) * radius, cy + Math.Sin(a0) * radius, cx + Math.Cos(a1) * radius, cy + Math.Sin(a1) * radius, color, width);
  }
 }

 static int s_iTwoICIconDrawn; // frames the icon was drawn (twoicshots checks it)

 protected void TwoICRing(SCR_MapEntity mapEntity, vector pos, int color)
 {
  int cx, cy, ex, ey;
  mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
  mapEntity.WorldToScreen(pos[0] + ACR_AICommander.ARRIVED, pos[2], ex, ey, true);
  Ring(cx, cy, Math.Max(Math.AbsFloat(ex - cx), 10), color, 3, 48);
 }
}

// The 2IC button of the fullscreen map's tool bar (with the vanilla compass, ruler and the rest),
// once the server has said the 2IC is on. Its vanilla glyph is hidden: ACR_OrderDisplay draws the ACA
// icon over it (DrawTwoICIcon). Lit (the vanilla active background) while the 2IC is shown.
modded class SCR_MapToolMenuUI
{
 protected SCR_MapToolEntry m_ACR_TwoICEntry;
 static SCR_MapToolEntry s_ACR_TwoICEntry;

 override void OnMapOpen(MapConfiguration config)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!m_ACR_TwoICEntry && pc && pc.ACR_TwoICOn() && config && config.MapEntityMode == EMapEntityMode.FULLSCREEN)
  {
   m_ACR_TwoICEntry = RegisterToolMenuEntry(s_sToolMenuIcons, "squad", 90);
   m_ACR_TwoICEntry.m_OnClick.Insert(ACR_OnTwoIC);
  }
  super.OnMapOpen(config);
  if (m_ACR_TwoICEntry && pc)
  {
   m_ACR_TwoICEntry.SetVisible(pc.ACR_TwoICOn());
   m_ACR_TwoICEntry.SetActive(pc.ACR_TwoICShown());
   if (m_ACR_TwoICEntry.m_ButtonComp && m_ACR_TwoICEntry.m_ButtonComp.m_wImage) m_ACR_TwoICEntry.m_ButtonComp.m_wImage.SetOpacity(0);
   s_ACR_TwoICEntry = m_ACR_TwoICEntry;
  }
 }

 override void OnMapClose(MapConfiguration config)
 {
  s_ACR_TwoICEntry = null;
  super.OnMapClose(config);
 }

 // The 2IC button's icon square (native px): its top left and size; false while it isn't shown.
 static bool ACR_TwoICIconRect(out float x, out float y, out float size)
 {
  SCR_MapToolEntry entry = s_ACR_TwoICEntry;
  if (!entry || !entry.m_ButtonComp || !entry.m_ButtonComp.m_wImage || !entry.m_ButtonComp.GetRootWidget().IsVisible()) return false;
  float ix, iy, w, h;
  entry.m_ButtonComp.m_wImage.GetScreenPos(ix, iy);
  entry.m_ButtonComp.m_wImage.GetScreenSize(w, h);
  size = Math.Min(w, h);
  if (size < 4) return false;
  x = ix + (w - size) * 0.5;
  y = iy + (h - size) * 0.5;
  return true;
 }

 // Its count's disc (native px): its centre on the icon's top right corner, and its radius.
 static void ACR_TwoICBadgeAt(float x, float y, float size, out float cx, out float cy, out float radius)
 {
  radius = Math.Max(size * 0.24, GetGame().GetWorkspace().DPIScale(8));
  cx = x + size * 0.94;
  cy = y + size * 0.06;
 }

 protected void ACR_OnTwoIC()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc || !m_ACR_TwoICEntry) return;
  m_ACR_TwoICEntry.SetActive(pc.ACR_ToggleTwoIC());
 }
}

// ACA's panel and its intel feed step aside while the player places a FOB with the 2IC shown (the
// declutter sketch 2: only the banner and the numbered sites), and come back as they were after.
modded class ACR_CommandPanel
{
 protected bool m_bACR_TwoICPlacing;
 protected bool m_bACR_RootWasShown = true;
 protected bool m_bACR_FeedWasShown = true;

 void ACR_TwoICPlacing(bool placing)
 {
  if (placing == m_bACR_TwoICPlacing) return;
  m_bACR_TwoICPlacing = placing;
  if (placing)
  {
   if (m_Root)
   {
    m_bACR_RootWasShown = m_Root.IsVisible();
    m_Root.SetVisible(false);
   }
   if (m_FeedRoot)
   {
    m_bACR_FeedWasShown = m_FeedRoot.IsVisible();
    m_FeedRoot.SetVisible(false);
   }
   return;
  }
  if (m_Root) m_Root.SetVisible(m_bACR_RootWasShown);
  if (m_FeedRoot) m_FeedRoot.SetVisible(m_bACR_FeedWasShown);
 }

 bool ACR_TwoICPlacingHidden() { return m_bACR_TwoICPlacing; }
 bool ACR_FeedShown() { return m_FeedRoot && m_FeedRoot.IsVisible(); }
}
