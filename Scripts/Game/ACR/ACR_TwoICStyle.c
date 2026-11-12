// The 2IC UI's look, from the #14 sketch Jacob agreed on (sketch 1, the plain map view) and its
// declutter sketch: dark panels (24,27,30 at 92%), the orange accent (#F2A33A) for the 2IC's own words, headings and the buttons that
// give it something, the player's words in blue (#8DB8EA), grey (#AEB4BA) labels, outlined buttons
// (#5A6068 border), round pills for the channel's filters and status chips. Sizes are the sketch's
// (made at 1440 px wide) scaled to the 1920 reference the UI is laid out in.
class ACR_TwoICStyle
{
 static const ResourceName LAYOUT_BUTTON = "{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout";
 static const ResourceName LAYOUT_TEXT = "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout";
 static const ResourceName LAYOUT_HEADING = "{0D7FF083FDE39877}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading4.layout";

 static const int BUTTON_OUTLINE = 0;  // a dark button with a grey border ("Open 2IC panel", "Take back")
 static const int BUTTON_FILL = 1;     // the accent, dark text ("Hand to 2IC", "Send truck")
 static const int BUTTON_PILL = 2;     // a filter, off
 static const int BUTTON_PILL_ON = 3;  // a filter, on
 static const int BUTTON_MENU = 4;     // a menu row (the ask box): no border, left-aligned
 static const int BUTTON_GHOST = 5;    // an outline on no fill (a channel line's Cancel, Release)
 static const int BUTTON_LINK = 6;     // blue text on no fill, no border (a suggestion's "Show")
 static const int BUTTON_QUIET = 7;    // grey text on no fill, no border (the card's "Panel F11")
 static const int BUTTON_OFF = 8;      // greyed out ("Buy a truck" the side can't buy now)

 // An sRGB colour (the sketch's hex) as the widgets want it: linear (they gamma it on the way out,
 // so #101317 given as is shows as a mid grey).
 static Color Rgb(int r, int g, int b, float a = 1)
 {
  return new Color(Linear(r), Linear(g), Linear(b), a);
 }

 // A table of the 256 values, made on first use (review P9: three Math.Pow per colour, for every
 // colour drawn every frame). Declared bare and filled here: static initializer budget. The calm map
 // (ACR_MapDeclutter.Rgb) uses the same table.
 protected static ref array<float> s_aLinear;
 static float Linear(int channel)
 {
  if (!s_aLinear)
  {
   s_aLinear = {};
   for (int i = 0; i < 256; i++) s_aLinear.Insert(Math.Pow(i / 255.0, 2.2));
  }
  if (channel < 0) channel = 0;
  if (channel > 255) channel = 255;
  return s_aLinear[channel];
 }

 // The declutter sketch's panel (rgba(24,27,30,0.92)) and 2IC orange (#F2A33A).
 static Color PanelColor() { return Rgb(24, 27, 30, 0.92); }
 static Color Accent() { return Rgb(242, 163, 58); }
 static Color OnAccent() { return Rgb(23, 19, 11); }
 static Color Grey() { return Rgb(174, 180, 186); }
 static Color Body() { return Rgb(214, 211, 204); }
 static Color White() { return Rgb(236, 234, 228); }
 static Color Blue() { return Rgb(141, 184, 234); }
 static Color Track() { return Rgb(44, 50, 56); }
 static Color Line() { return Rgb(31, 37, 43); }
 static Color Block() { return Rgb(26, 31, 36); }
 static Color Border() { return Rgb(90, 96, 104); }
 static Color ButtonDark() { return Rgb(34, 40, 48); }
 static Color Dim() { return Rgb(128, 134, 140); }          // a routine line's time and span
 static Color PinnedFill() { return Rgb(48, 41, 30, 0.96); } // a line that needs the player

 // "r,g,b,a" for rich text colour tags (the linear values, as SetColor takes them).
 static string Tag(Color c)
 {
  return string.Format("%1,%2,%3,%4", (int)(c.R() * 255), (int)(c.G() * 255), (int)(c.B() * 255), (int)(c.A() * 255));
 }

 static string Colored(string text, Color c)
 {
  return string.Format("<color rgba='%1'>%2</color>", Tag(c), text);
 }

 // A dark panel: root (an overlay, for placing), its background, and the padded column inside.
 // width <= 0: as wide as its content.
 static Widget PanelBox(Widget parent, float width, float padX, float padY, out Widget root, Color background = null)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  root = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent);
  // A local: assigning to a parameter that defaults to null doesn't stick (the box came out white).
  Color color = background;
  if (!color) color = PanelColor();
  Fill(root, color);
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, root));
  if (width > 0)
  {
   size.EnableWidthOverride(true);
   size.SetWidthOverride(width);
  }
  AlignableSlot.SetPadding(size, padX, padY, padX, padY);
  Widget column = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, size);
  AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Stretch);
  return column;
 }

 // A stretched colour behind an overlay's other children.
 static Widget Fill(Widget overlay, Color color)
 {
  Widget image = GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, color, 0, overlay);
  AlignableSlot.SetHorizontalAlign(image, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(image, LayoutVerticalAlign.Stretch);
  return image;
 }

 static Widget Row(Widget parent, float gapTop = 0)
 {
  Widget row = GetGame().GetWorkspace().CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent);
  if (gapTop > 0) AlignableSlot.SetPadding(row, 0, gapTop, 0, 0);
  return row;
 }

 static Widget Column(Widget parent, float gapTop = 0)
 {
  Widget column = GetGame().GetWorkspace().CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent);
  if (gapTop > 0) AlignableSlot.SetPadding(column, 0, gapTop, 0, 0);
  // Full width in a fixed box (else it shrinks to its widest text and sits centred).
  AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Stretch);
  return column;
 }

 // A child that takes the row's spare width.
 static void Grow(Widget widget)
 {
  if (widget) LayoutSlot.SetSizeMode(widget, LayoutSizeMode.Fill);
 }

 // A fixed-size box (a dot, a bar's part, a spacer); its colour, alpha 0 for none.
 static SizeLayoutWidget Box(Widget parent, float width, float height, Color color)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  SizeLayoutWidget box = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, parent));
  if (width > 0)
  {
   box.EnableWidthOverride(true);
   box.SetWidthOverride(width);
  }
  if (height > 0)
  {
   box.EnableHeightOverride(true);
   box.SetHeightOverride(height);
  }
  if (color && color.A() > 0)
  {
   Widget fill = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, color, 0, box);
   AlignableSlot.SetHorizontalAlign(fill, LayoutHorizontalAlign.Stretch);
   AlignableSlot.SetVerticalAlign(fill, LayoutVerticalAlign.Stretch);
  }
  return box;
 }

 // A text: the body font (a wrapping rich text: Colored tags work), or the condensed heading font;
 // its colour and size.
 static TextWidget Text(Widget parent, string text, Color color, int size, bool heading = false)
 {
  ResourceName layout = LAYOUT_TEXT;
  if (heading) layout = LAYOUT_HEADING;
  TextWidget widget = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(layout, parent));
  if (!widget) return null;
  widget.SetText(text);
  widget.SetColor(color);
  widget.SetExactFontSize(size);
  return widget;
 }

 // A status chip ("Can't yet", "Coming, 1 min", "Holding"): its colours by the status.
 static TextWidget Chip(Widget parent, out Widget root)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  root = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent);
  Fill(root, Rgb(58, 42, 22));
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, root));
  AlignableSlot.SetPadding(size, 10, 2, 10, 2);
  return Text(size, "", Rgb(243, 194, 122), 16);
 }

 static void SetChip(Widget root, TextWidget text, string status)
 {
  if (!root || !text) return;
  root.SetVisible(!status.IsEmpty());
  if (status.IsEmpty()) return;
  text.SetText(status);
  Color bg = Rgb(58, 42, 22);
  Color fg = Rgb(243, 194, 122);
  if (status == ACR_TwoICCompact.NEEDS_YOU)
  {
   bg = Accent();
   fg = OnAccent();
  }
  else if (status.StartsWith("Coming") || status.StartsWith("Asked") || status.StartsWith("Supporting"))
  {
   bg = Rgb(29, 48, 72);
   fg = Rgb(169, 203, 240);
  }
  else if (status.StartsWith("Holding") || status.StartsWith("Done") || status.StartsWith("Sent") || status.StartsWith("Accepted") || status.StartsWith("Truck sent"))
  {
   bg = Rgb(31, 51, 38);
   fg = Rgb(159, 214, 174);
  }
  else if (status.StartsWith("Cancelled") || status.StartsWith("Dismissed") || status.StartsWith("Withdrawn") || status.StartsWith("Released") || status.StartsWith("Suggested"))
  {
   bg = Track();
   fg = Grey();
  }
  Widget fill = root.GetChildren();
  if (fill) fill.SetColor(bg);
  text.SetColor(fg);
 }

 // A button in one of the BUTTON_ looks; width > 0: a fixed width; height > 0: a fixed height (the
 // sketch's 34 px buttons are 44 here).
 static SCR_ButtonTextComponent Button(Widget parent, string text, int look, float width = 0, int size = 17, float height = 0)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget holder = parent;
  if (width > 0 || height > 0)
  {
   SizeLayoutWidget fixed = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent));
   if (width > 0)
   {
    fixed.EnableWidthOverride(true);
    fixed.SetWidthOverride(width);
   }
   if (height > 0)
   {
    fixed.EnableHeightOverride(true);
    fixed.SetHeightOverride(height);
   }
   if (width <= 0) AlignableSlot.SetHorizontalAlign(fixed, LayoutHorizontalAlign.Stretch);
   holder = fixed;
  }
  Widget widget = ws.CreateWidgets(LAYOUT_BUTTON, holder);
  if (!widget) return null;
  // In its fixed box the button fills it (else it shrinks to its text).
  if (holder != parent)
  {
   AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
   AlignableSlot.SetVerticalAlign(widget, LayoutVerticalAlign.Stretch);
  }
  AlignableSlot.SetPadding(widget, 2, 2, 2, 2);
  SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(widget);
  if (!button) return null;
  button.SetText(text);
  Restyle(button, look, size);
  return button;
 }

 static void Restyle(SCR_ButtonTextComponent button, int look, int size = 17)
 {
  if (!button) return;
  Color normal = ButtonDark();
  Color hover = Rgb(46, 54, 64);
  Color text = White();
  Color border = Border();
  if (look == BUTTON_FILL)
  {
   normal = Accent();
   hover = Rgb(250, 184, 92);
   text = OnAccent();
   border = Accent();
  }
  else if (look == BUTTON_PILL)
  {
   normal = Rgb(0, 0, 0, 0);
   hover = Rgb(44, 50, 56, 0.6);
   text = Grey();
   border = Rgb(0, 0, 0, 0);
  }
  else if (look == BUTTON_PILL_ON)
  {
   normal = Track();
   hover = Track();
   border = Rgb(0, 0, 0, 0);
  }
  else if (look == BUTTON_MENU)
  {
   normal = Rgb(0, 0, 0, 0);
   hover = Rgb(42, 36, 24);
   border = Rgb(0, 0, 0, 0);
  }
  else if (look == BUTTON_GHOST)
  {
   normal = Rgb(0, 0, 0, 0);
   hover = Rgb(44, 50, 56, 0.8);
  }
  else if (look == BUTTON_OFF)
  {
   normal = Rgb(44, 50, 56, 0.7);
   hover = normal;
   text = Dim();
   border = Rgb(0, 0, 0, 0);
  }
  else if (look == BUTTON_QUIET)
  {
   normal = Rgb(0, 0, 0, 0);
   hover = Rgb(44, 50, 56, 0.6);
   text = Grey();
   border = Rgb(0, 0, 0, 0);
  }
  else if (look == BUTTON_LINK)
  {
   normal = Rgb(0, 0, 0, 0);
   hover = Rgb(29, 48, 72, 0.8);
   text = Blue();
   border = Rgb(0, 0, 0, 0);
  }
  button.SetBackgroundColors(normal, hover, hover, Rgb(30, 34, 40, 0.6), hover);
  if (button.m_wBackground) button.m_wBackground.SetColor(normal);
  // The outlined looks keep their border on (the widget library shows it only on focus).
  bool outlined = look == BUTTON_OUTLINE || look == BUTTON_GHOST;
  button.m_bShowBorderOnFocus = false;
  button.m_bShowBorderOnHover = false;
  if (button.m_wBorder)
  {
   button.m_wBorder.SetColor(border);
   if (outlined) button.m_wBorder.SetOpacity(1);
   else button.m_wBorder.SetOpacity(0);
  }
  TextWidget label = button.GetTextWidget();
  if (label)
  {
   label.SetColor(text);
   label.SetExactFontSize(size);
   if (look == BUTTON_MENU) AlignableSlot.SetHorizontalAlign(label, LayoutHorizontalAlign.Left);
  }
  button.m_TextDefault = text;
  button.m_TextToggled = text;
 }
}
