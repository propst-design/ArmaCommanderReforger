// Map layers (playtest 30 Oct: a busy map is hard to read): a row of switches above the legend on
// the calm map, "Show: Infantry · Vehicles · Support · Contacts · Zones". Switching a kind off
// takes its squads' icons, soldiers, bars and order lines off the map, and out of any "×N" (by
// their symbol: ACR_MapLayers.KindOf); Vehicles also takes the empty vehicles. Contacts are the
// enemy sighted; Zones the bases' capture rings and the mortar teams' ranges. A selected squad
// always shows, and a friendly fire mission's danger area always shows: it is there to keep our
// own people out of it. The switches hold while the game runs (SCR_PlayerController in
// ACR_PlayerMapSquads).
class ACR_MapLayers
{
 static const int INFANTRY = 0;
 static const int VEHICLES = 1;
 static const int SUPPORT = 2;
 static const int CONTACTS = 3;
 static const int ZONES = 4;
 static const int COUNT = 5;

 protected static ref array<bool> s_aOff = {false, false, false, false, false};

 static string Name(int layer)
 {
  switch (layer)
  {
   case INFANTRY: return "Infantry";
   case VEHICLES: return "Vehicles";
   case SUPPORT: return "Support";
   case CONTACTS: return "Contacts";
   case ZONES: return "Zones";
  }
  return "";
 }

 static bool Off(int layer)
 {
  return layer >= 0 && layer < s_aOff.Count() && s_aOff[layer];
 }

 static void Set(int layer, bool off)
 {
  if (layer >= 0 && layer < s_aOff.Count()) s_aOff[layer] = off;
 }

 static void Toggle(int layer)
 {
  Set(layer, !Off(layer));
 }

 static bool AnyOff()
 {
  foreach (bool off : s_aOff)
  {
   if (off) return true;
  }
  return false;
 }

 // A squad's layer by its symbol (EMilitarySymbolIcon): supply, medical, repair, mortar and
 // artillery are Support; motorized, armour and aircraft are Vehicles; the rest (and a squad with
 // no symbol yet) Infantry.
 static int KindOf(int icons)
 {
  int support = EMilitarySymbolIcon.SUPPLY | EMilitarySymbolIcon.MEDICAL | EMilitarySymbolIcon.MAINTENANCE | EMilitarySymbolIcon.MORTAR | EMilitarySymbolIcon.ARTILLERY;
  int vehicles = EMilitarySymbolIcon.MOTORIZED | EMilitarySymbolIcon.ARMOR | EMilitarySymbolIcon.ROTARY_WING | EMilitarySymbolIcon.FIXED_WING;
  if (icons & support) return SUPPORT;
  if (icons & vehicles) return VEHICLES;
  return INFANTRY;
 }
}

// The row of switches, bottom right above the legend until the player moves them (held by the
// "Show" word and the edges between the switches; ACR_PanelMover), the legend going with them.
class ACR_MapLayerBar : Managed
{
 static const float PILL_WIDTH = 86;

 Widget m_Root;
 protected SCR_PlayerController m_Owner;
 protected ref map<SCR_ButtonBaseComponent, int> m_Buttons = new map<SCR_ButtonBaseComponent, int>();
 protected ref array<SCR_ButtonTextComponent> m_Order = {};
 protected string m_sStyled = "-";

 void Create(Widget parent, SCR_PlayerController owner)
 {
  m_Owner = owner;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_Root = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_Root, 1, 1);
  FrameSlot.SetAnchorMax(m_Root, 1, 1);
  FrameSlot.SetAlignment(m_Root, 1, 1);
  FrameSlot.SetSizeToContent(m_Root, true);
  m_Root.SetZOrder(1000);
  ACR_TwoICStyle.Fill(m_Root, ACR_MapDeclutter.PanelColor());
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, m_Root));
  AlignableSlot.SetPadding(size, 10, 4, 6, 4);
  Widget row = ACR_TwoICStyle.Row(size);
  TextWidget label = ACR_TwoICStyle.Text(row, "Show", ACR_TwoICStyle.Dim(), 15);
  if (label)
  {
   AlignableSlot.SetVerticalAlign(label, LayoutVerticalAlign.Center);
   AlignableSlot.SetPadding(label, 0, 0, 6, 0);
  }
  for (int i = 0; i < ACR_MapLayers.COUNT; i++)
  {
   SCR_ButtonTextComponent button = ACR_TwoICStyle.Button(row, ACR_MapLayers.Name(i), ACR_TwoICStyle.BUTTON_PILL_ON, PILL_WIDTH, 15, 30);
   if (!button) continue;
   button.m_OnClicked.Insert(OnButton);
   m_Buttons.Insert(button, i);
   m_Order.Insert(button);
  }
  ACR_PanelMover.Add(ACR_UiPrefs.LAYERS, m_Root, 1, 1, -24, -24);
  ACR_PanelMover.Body(ACR_UiPrefs.LAYERS, m_Root);
  Refresh();
 }

 // On: a lit pill, white text; off: dim text on nothing (the first shots: the 2IC filters' pills
 // read alike on the dark bar).
 void Refresh()
 {
  string key = "";
  for (int i = 0; i < ACR_MapLayers.COUNT; i++) key += ACR_MapLayers.Off(i).ToString();
  if (key == m_sStyled) return;
  m_sStyled = key;
  foreach (int i, SCR_ButtonTextComponent button : m_Order)
  {
   if (!button) continue;
   bool off = ACR_MapLayers.Off(i);
   int look = ACR_TwoICStyle.BUTTON_PILL_ON;
   if (off) look = ACR_TwoICStyle.BUTTON_PILL;
   ACR_TwoICStyle.Restyle(button, look, 15);
   Color fill = ACR_TwoICStyle.Rgb(56, 72, 92);
   Color hover = ACR_TwoICStyle.Rgb(66, 84, 106);
   Color text = ACR_TwoICStyle.White();
   if (off)
   {
    fill = ACR_TwoICStyle.Rgb(0, 0, 0, 0);
    hover = ACR_TwoICStyle.Rgb(44, 50, 56, 0.6);
    text = ACR_TwoICStyle.Dim();
   }
   button.SetBackgroundColors(fill, hover, hover, ACR_TwoICStyle.Rgb(30, 34, 40, 0.6), hover);
   if (button.m_wBackground) button.m_wBackground.SetColor(fill);
   TextWidget label = button.GetTextWidget();
   if (label) label.SetColor(text);
   button.m_TextDefault = text;
   button.m_TextToggled = text;
  }
 }

 // bottom, right: how far from the screen's bottom and right edges (unscaled px).
 void Place(float bottom, float right)
 {
  ACR_PanelMover.Natural(ACR_UiPrefs.LAYERS, -right, -bottom);
 }

 void SetVisible(bool visible)
 {
  if (m_Root && m_Root.IsVisible() != visible) m_Root.SetVisible(visible);
 }

 bool Contains(Widget widget)
 {
  while (widget)
  {
   if (widget == m_Root) return true;
   widget = widget.GetParent();
  }
  return false;
 }

 void Destroy()
 {
  ACR_PanelMover.Remove(ACR_UiPrefs.LAYERS);
  if (m_Root) m_Root.RemoveFromHierarchy();
  m_Root = null;
 }

 // A test's click on layer i.
 void ClickForTest(int i)
 {
  if (i >= 0 && i < m_Order.Count()) OnButton(m_Order[i]);
 }

 protected void OnButton(SCR_ButtonBaseComponent button)
 {
  int i;
  if (!m_Buttons.Find(button, i)) return;
  ACR_MapLayers.Toggle(i);
  Refresh();
  if (m_Owner) m_Owner.ACR_MarkUiClick();
 }
}
