// The calm command map's drawing (the rules are in ACR_MapDeclutter): the dark ACA panels with blue
// headers, INTEL's merged lines and, zoomed out, the one-line bars; base dots with size pips and a
// red ring while contested; squads at a base gathered under its marker as one icon with "×N" and a
// corner mark (moving, fighting, holding), and the legend for those marks; the UI size; the dark map
// (a moon button on the map's tool bar). With -acrMapDeclutter 0 every override here hands straight
// back to the classic code. (Named to load after ACR_PlayerController.c, whose classes it mods.)

// ---------------------------------------------------------------- marker widgets
modded class SCR_MapMarkerWidgetComponent
{
 ImageWidget ACR_Icon() { return m_wMarkerIcon; }
 TextWidget ACR_Text() { return m_wMarkerText; }
 Widget ACR_Symbol() { return m_wSymbolRoot; }
 bool ACR_Hovered() { return m_ACR_BaseHovered; }

 // On the dark map a base's name is white (a marker's text is black).
 override void SetColor(Color color)
 {
  super.SetColor(color);
  ACR_UnitMarker marker = ACR_UnitMarker.Cast(m_MarkerObject);
  if (!marker || marker.m_BaseIndex < 0 || !m_wMarkerText || !ACR_MapDeclutter.On() || !ACR_MapDeclutter.Dark()) return;
  m_wMarkerText.SetColor(ACR_MapDeclutter.Rgb(238, 240, 242));
 }
}

// ---------------------------------------------------------------- map data
// What the calm map worked out this frame (ACR_DeclLayout), for the canvas and the markers.
modded class ACR_MapData
{
 bool m_bDeclOn;
 bool m_bDeclFar;
 bool m_bDeclZonesOff;   // the Zones layer is off (ACR_MapLayers): no capture rings, no mortar ranges
 string m_sDeclSelectedBase;
 // per base
 ref array<int> m_DeclSide = {};
 ref array<int> m_DeclPips = {};
 ref array<bool> m_DeclHQ = {};
 ref array<bool> m_DeclKeep = {};
 ref array<float> m_DeclCaption = {}; // 4 per base: its caption's rectangle from its marker's point (x1 <= x0: none)
 ref array<float> m_DeclDots = {};    // 2 per base: its dot on screen (px)
 ref array<float> m_DeclShift = {};   // 2 per base: how far its name moved so names don't overlap (px)
 ref array<float> m_DeclNames = {};   // 4 per base: its name where it is drawn (px; x1 <= x0: none)
 ref array<float> m_DeclObstacles = {}; // 4 per rectangle: every base's dot and name (px)
 // per group (as m_GroupIds)
 ref array<int> m_DeclBaseOf = {};
 ref array<int> m_DeclCount = {};
 ref array<bool> m_DeclRep = {};
 ref array<int> m_DeclMark = {};       // ACR_ESquadMark, -1 none
 ref array<bool> m_DeclHidden = {};    // its kind's layer is off (ACR_MapLayers): no icon, soldiers or order lines
 ref array<vector> m_DeclShown = {};
 ref array<float> m_DeclSymW = {};     // the symbol's size on screen (px)
 ref array<float> m_DeclSymH = {};
 ref array<float> m_DeclSymX = {};     // the symbol's centre from its marker's point (px)
 ref array<float> m_DeclSymY = {};
 // the legend's glyphs: x, y, mark
 ref array<float> m_DeclLegend = {};
 // The squad under the cursor in a "×N"'s list (ACR_SquadPicker): a ring where it really stands.
 RplId m_PickerHover = RplId.Invalid();
 // A pulse on a spot the player was sent to (the 2IC panel's Show): until m_iPulseUntil (ms).
 vector m_vPulseAt;
 int m_iPulseUntil;

 // Where a squad's icon is shown: under its base's marker when it is at one.
 vector ShownPos(int g)
 {
  if (m_bDeclOn && g >= 0 && g < m_DeclShown.Count()) return m_DeclShown[g];
  return LeaderPos(g);
 }

 // A squad shown by another squad's icon at the same base ("×N").
 bool ShownByOther(int g)
 {
  return m_bDeclOn && g >= 0 && g < m_DeclRep.Count() && !m_DeclRep[g];
 }

 // A squad whose kind's layer is off (a hidden squad counts as shown by none: ShownByOther).
 bool DeclHidden(int g)
 {
  return m_bDeclOn && g >= 0 && g < m_DeclHidden.Count() && m_DeclHidden[g];
 }
}

// ---------------------------------------------------------------- the canvas
modded class ACR_OrderDisplay
{
 static int s_iDeclFrames; // frames drawn calm (the shots test checks it)

 override void Draw(SCR_MapEntity mapEntity, ACR_MapData data, bool boxing, vector boxStart)
 {
  super.Draw(mapEntity, data, boxing, boxStart);
  if (!m_Canvas || !data || !data.m_bDeclOn) return;
  DeclLegendGlyphs(data);
  DeclMoon();
  s_iDeclFrames++;
  m_Canvas.SetDrawCommands(m_Commands);
 }

 protected void DeclCanvasSize(out float width, out float height)
 {
  width = 0;
  height = 0;
  if (m_Canvas) m_Canvas.GetScreenSize(width, height);
  WorkspaceWidget workspace = GetGame().GetWorkspace();
  if (!workspace) return;
  width = Math.Max(width, workspace.GetWidth());
  height = Math.Max(height, workspace.GetHeight());
 }

 // Filled shapes go through here (ACR_MapDeclutter.ShapeOk; AddPolygon once #62 is in).
 protected void DeclFill(PolygonDrawCommand polygon)
 {
  float width, height;
  DeclCanvasSize(width, height);
  if (ACR_MapDeclutter.ShapeOk(polygon.m_Vertices, width, height)) m_Commands.Insert(polygon);
 }

 protected void DeclDisc(float x, float y, float radius, int color)
 {
  if (radius < 1) return;
  PolygonDrawCommand disc = new PolygonDrawCommand();
  disc.m_iColor = color;
  disc.m_Vertices = {};
  m_Canvas.TessellateCircle(Vector(x, y, 0), radius, 16, disc.m_Vertices);
  DeclFill(disc);
 }

 protected void DeclShape(array<float> vertices, int color)
 {
  PolygonDrawCommand shape = new PolygonDrawCommand();
  shape.m_iColor = color;
  shape.m_Vertices = vertices;
  DeclFill(shape);
 }

 protected float Px(float size)
 {
  return GetGame().GetWorkspace().DPIScale(size);
 }

 // Bases: a dot in the owner's colour with a white edge (smaller for a plain dot when zoomed out),
 // pips after the name, a red ring while contested, a white one when selected; near, a faint
 // capture ring.
 override protected void DrawBaseAreas(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  if (!data.m_bDeclOn)
  {
   super.DrawBaseAreas(mapEntity, data);
   return;
  }
  int white = ACR_MapDeclutter.Rgb(250, 250, 250).PackToInt();
  int red = ACR_MapDeclutter.Rgb(229, 62, 62).PackToInt();
  foreach (int i, vector pos : data.m_BasePositions)
  {
   int cx, cy;
   mapEntity.WorldToScreen(pos[0], pos[2], cx, cy, true);
   int side = ACR_EBaseSide.FREE;
   if (i < data.m_DeclSide.Count()) side = data.m_DeclSide[i];
   bool keep = i >= data.m_DeclKeep.Count() || data.m_DeclKeep[i];
   bool contested = i < data.m_BaseContested.Count() && data.m_BaseContested[i] == 1;
   int color = ACR_MapDeclutter.SideColor(side).PackToInt();
   if (!data.m_bDeclFar && !data.m_bDeclZonesOff && i < data.m_BaseRadii.Count())
   {
    int ex, ey;
    mapEntity.WorldToScreen(pos[0] + data.m_BaseRadii[i], pos[2], ex, ey, true);
    float radius = Math.AbsFloat(ex - cx);
    if (radius > Px(24)) Ring(cx, cy, radius, ACR_MapDeclutter.SideColor(side, 0.35).PackToInt(), Px(1.5), 48);
   }
   float dot = Px(7);
   if (!keep) dot = Px(4.5);
   DeclDisc(cx, cy, dot + Px(2), white);
   DeclDisc(cx, cy, dot, color);
   if (contested) Ring(cx, cy, dot + Px(7), red, Px(2.5), 32);
   if (i < data.m_BaseNames.Count() && data.m_BaseNames[i] == data.m_sDeclSelectedBase) Ring(cx, cy, dot + Px(4), white, Px(2), 32);
   // The pips after the name, where the name is (moved off its spot or not).
   if (!keep || i * 4 + 3 >= data.m_DeclCaption.Count() || i * 2 + 1 >= data.m_DeclShift.Count() || i >= data.m_DeclPips.Count()) continue;
   float x0 = data.m_DeclCaption[i * 4];
   float x1 = data.m_DeclCaption[i * 4 + 2];
   if (x1 <= x0) continue;
   // (The caption's rectangle includes its pips.)
   float px = cx + data.m_DeclShift[i * 2] + x1 - Px(4 + 7 * data.m_DeclPips[i]);
   float py = cy + data.m_DeclShift[i * 2 + 1] + (data.m_DeclCaption[i * 4 + 1] + data.m_DeclCaption[i * 4 + 3]) * 0.5;
   for (int p = 0; p < data.m_DeclPips[i]; p++)
    DeclDisc(px + Px(6 + p * 7), py, Px(2.6), color);
  }
 }

 // Zoomed out only the selected squads' orders; lines start at the icon as shown.
 override protected void DrawOrders(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  if (!data.m_bDeclOn)
  {
   super.DrawOrders(mapEntity, data);
   return;
  }
  // Orders ending at a base end at its dot (one mark for them all); other ends close together
  // get one dot.
  array<float> ends = {};
  float lastX, lastY;
  foreach (int i, RplId groupId : data.m_OrderGroups)
  {
   int g = data.m_GroupIds.Find(groupId);
   if (g < 0 || g >= data.m_LeaderIds.Count() || data.DeclHidden(g)) continue;
   bool selected = data.IsSelected(groupId);
   if (data.m_bDeclFar && !selected) continue;
   vector from = data.ShownPos(g);
   int sx0, sy0, sx1, sy1;
   mapEntity.WorldToScreen(from[0], from[2], sx0, sy0, true);
   float x0 = sx0;
   float y0 = sy0;
   bool chained = i > 0 && data.m_OrderGroups[i - 1] == groupId;
   if (chained)
   {
    x0 = lastX;
    y0 = lastY;
   }
   vector to = data.m_OrderTargets[i];
   mapEntity.WorldToScreen(to[0], to[2], sx1, sy1, true);
   float x1 = sx1;
   float y1 = sy1;
   lastX = x1;
   lastY = y1;
   int base = ACR_MapDeclutter.SnapToBase(x1, y1, data.m_DeclDots, Px(26), data.m_DeclNames);
   if (base >= 0)
   {
    float bx = data.m_DeclDots[base * 2];
    float by = data.m_DeclDots[base * 2 + 1];
    lastX = bx;
    lastY = by;
    float dx = bx - x0;
    float dy = by - y0;
    float length = Math.Sqrt(dx * dx + dy * dy);
    float stop = Px(12);
    if (length <= stop) continue;
    x1 = bx - dx / length * stop;
    y1 = by - dy / length * stop;
   }
   float alpha = 0.75;
   if (selected) alpha = 1;
   int color = OrderColor(data.m_OrderKinds[i], alpha);
   float width = 2;
   if (selected) width = 3;
   int issuer = 0;
   if (i < data.m_OrderIssuers.Count()) issuer = data.m_OrderIssuers[i];
   if (issuer > 0 && issuer != data.m_iMyId) DashedLine(x0, y0, x1, y1, color, width);
   else Line(x0, y0, x1, y1, color, width);
   if (base >= 0) continue;
   bool shared;
   for (int e = 0; e + 1 < ends.Count(); e += 2)
    if (Math.AbsFloat(ends[e] - x1) < Px(8) && Math.AbsFloat(ends[e + 1] - y1) < Px(8)) shared = true;
   if (shared) continue;
   Dot(x1, y1, 6, color);
   ends.Insert(x1);
   ends.Insert(y1);
  }
 }

 override protected void DrawOtherSelections(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  if (!data.m_bDeclOn)
  {
   super.DrawOtherSelections(mapEntity, data);
   return;
  }
  Color white = new Color(1, 1, 1, 0.8);
  foreach (int s, RplId groupId : data.m_SelGroups)
  {
   if (data.m_SelOwners[s] == data.m_iMyId) continue;
   int g = data.m_GroupIds.Find(groupId);
   if (g < 0 || g >= data.m_LeaderIds.Count() || data.ShownByOther(g)) continue;
   vector pos = data.ShownPos(g);
   int sx, sy;
   mapEntity.WorldToScreen(pos[0], pos[2], sx, sy, true);
   Ring(sx, sy, 17, white.PackToInt(), 1.5);
  }
 }

 // Near: the status bars under each icon shown (not under squads shown by another's icon).
 // Zoomed out: none. Then the corner marks.
 override protected void DrawStatusBars(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  DrawFocus(mapEntity, data);
  if (!data.m_bDeclOn)
  {
   super.DrawStatusBars(mapEntity, data);
   return;
  }
  if (!data.m_bDeclFar)
  {
   foreach (int i, RplId groupId : data.m_StatusGroups)
   {
    if (i * 6 + 1 >= data.m_Status.Count()) break;
    int health = data.m_Status[i * 6];
    int ammo = data.m_Status[i * 6 + 1];
    if (health < 0) continue;
    int g = data.m_GroupIds.Find(groupId);
    if (g < 0 || g >= data.m_LeaderPositions.Count() || data.ShownByOther(g)) continue;
    vector pos = data.ShownPos(g);
    int sx, sy;
    mapEntity.WorldToScreen(pos[0], pos[2], sx, sy, true);
    float x = sx - 15;
    float y = sy + 16;
    Color healthColor = new Color(0.30, 0.85, 0.35, 0.95);
    if (health < 60) healthColor = new Color(1.00, 0.75, 0.20, 0.95);
    if (health < 30) healthColor = new Color(1.00, 0.25, 0.20, 0.95);
    Bar(x, y, 30, 4, health / 100.0, healthColor);
    if (ammo >= 0)
    {
     Color ammoColor = new Color(0.35, 0.70, 1.00, 0.95);
     if (ammo < 30) ammoColor = new Color(1.00, 0.25, 0.20, 0.95);
     Bar(x, y + 6, 30, 4, ammo / 100.0, ammoColor);
    }
   }
  }
  DeclMarks(mapEntity, data);
 }

 // Where the player's eye is sent: the squad under the cursor in a "×N"'s list (a ring where it
 // really stands, a line from the icon it is shown by) and the pulse after the 2IC panel's Show (two
 // rings growing out from the spot, for PULSE_MS).
 static const int PULSE_MS = 2400;
 protected void DrawFocus(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  int white = Color.FromRGBA(255, 255, 255, 235).PackToInt();
  if (data.m_PickerHover.IsValid())
  {
   int g = data.m_GroupIds.Find(data.m_PickerHover);
   if (g >= 0 && g < data.m_LeaderPositions.Count())
   {
    vector real = data.LeaderPos(g);
    vector shown = data.ShownPos(g);
    int rx, ry, ix, iy;
    mapEntity.WorldToScreen(real[0], real[2], rx, ry, true);
    mapEntity.WorldToScreen(shown[0], shown[2], ix, iy, true);
    if (Math.AbsFloat(rx - ix) + Math.AbsFloat(ry - iy) > Px(6)) Line(ix, iy, rx, ry, white, Px(1.5));
    Ring(rx, ry, Px(13), white, Px(2.5), 28);
   }
  }
  int now = System.GetTickCount();
  if (data.m_iPulseUntil > now)
  {
   int px, py;
   mapEntity.WorldToScreen(data.m_vPulseAt[0], data.m_vPulseAt[2], px, py, true);
   int elapsed = PULSE_MS - (data.m_iPulseUntil - now);
   int cycle = elapsed - (elapsed / 800) * 800;
   float phase = cycle / 800.0;
   for (int k = 0; k < 2; k++)
   {
    float t = phase + k * 0.5;
    if (t >= 1) t -= 1;
    int alpha = (int)(230 * (1 - t));
    Ring(px, py, Px(10 + 34 * t), Color.FromRGBA(242, 163, 58, alpha).PackToInt(), Px(3), 32);
   }
  }
 }

 // The corner mark at the bottom left of each icon shown.
 protected void DeclMarks(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  foreach (int g, int mark : data.m_DeclMark)
  {
   if (mark < 0 || data.ShownByOther(g) || g >= data.m_LeaderPositions.Count()) continue;
   vector pos = data.ShownPos(g);
   int sx, sy;
   mapEntity.WorldToScreen(pos[0], pos[2], sx, sy, true);
   float w = Px(30);
   float h = Px(22);
   float ox, oy;
   if (g < data.m_DeclSymW.Count() && data.m_DeclSymW[g] > 4)
   {
    w = data.m_DeclSymW[g];
    h = data.m_DeclSymH[g];
    ox = data.m_DeclSymX[g];
    oy = data.m_DeclSymY[g];
   }
   DeclMark(sx + ox - w * 0.5, sy + oy + h * 0.5, Px(6), mark);
  }
 }

 // White disc with a dark play triangle (moving) or square (holding); red with a white cross (fighting).
 protected void DeclMark(float x, float y, float r, int mark)
 {
  int white = ACR_MapDeclutter.Rgb(250, 250, 250).PackToInt();
  int ink = ACR_MapDeclutter.Rgb(28, 30, 33).PackToInt();
  if (mark == ACR_ESquadMark.FIGHTING)
  {
   DeclDisc(x, y, r + Px(1), white);
   DeclDisc(x, y, r, ACR_MapDeclutter.Rgb(229, 62, 62).PackToInt());
   float k = r * 0.45;
   Line(x - k, y - k, x + k, y + k, white, Px(1.6));
   Line(x - k, y + k, x + k, y - k, white, Px(1.6));
   return;
  }
  DeclDisc(x, y, r + Px(1), ink);
  DeclDisc(x, y, r, white);
  if (mark == ACR_ESquadMark.MOVING)
  {
   array<float> triangle = {x - r * 0.32, y - r * 0.5, x + r * 0.55, y, x - r * 0.32, y + r * 0.5};
   DeclShape(triangle, ink);
   return;
  }
  float half = r * 0.38;
  array<float> square = {x - half, y - half, x + half, y - half, x + half, y + half, x - half, y + half};
  DeclShape(square, ink);
 }

 protected void DeclLegendGlyphs(ACR_MapData data)
 {
  for (int i = 0; i + 2 < data.m_DeclLegend.Count(); i += 3)
   DeclMark(data.m_DeclLegend[i], data.m_DeclLegend[i + 1], Px(6), data.m_DeclLegend[i + 2]);
 }

 // The dark map button's moon (its tool bar glyph is hidden): a crescent outline, blue while on.
 protected void DeclMoon()
 {
  SCR_MapToolEntry entry = SCR_MapToolMenuUI.s_ACR_MoonEntry;
  if (!entry || !entry.m_ButtonComp || !entry.m_ButtonComp.m_wImage || !entry.m_ButtonComp.GetRootWidget().IsVisible()) return;
  float x, y, w, h;
  entry.m_ButtonComp.m_wImage.GetScreenPos(x, y);
  entry.m_ButtonComp.m_wImage.GetScreenSize(w, h);
  float size = Math.Min(w, h);
  if (size < 4) return;
  float cx = x + w * 0.5;
  float cy = y + h * 0.5;
  float r = size * 0.36;
  // Outer circle at the centre, a same-size circle 0.6 r to the right cut out of it, tilted.
  float shift = r * 0.6;
  float outer = Math.Atan2(Math.Sqrt(r * r - shift * shift * 0.25), shift * 0.5);
  float inner = Math.Atan2(Math.Sqrt(r * r - shift * shift * 0.25), -shift * 0.5);
  float tilt = -0.5;
  LineDrawCommand moon = new LineDrawCommand();
  moon.m_iColor = ACR_MapDeclutter.Rgb(226, 228, 230).PackToInt();
  if (ACR_MapDeclutter.Dark()) moon.m_iColor = ACR_MapDeclutter.AccentBlue().PackToInt();
  moon.m_fWidth = Math.Max(1.5, size * 0.07);
  moon.m_bShouldEnclose = true;
  moon.m_Vertices = {};
  int steps = 14;
  for (int s = 0; s <= steps; s++)
  {
   float a = outer + (Math.PI2 - 2 * outer) * s / steps;
   DeclMoonPoint(moon.m_Vertices, cx, cy, Math.Cos(a) * r, Math.Sin(a) * r, tilt);
  }
  for (int t = 0; t <= steps; t++)
  {
   float b = (Math.PI2 - inner) - (Math.PI2 - 2 * inner) * t / steps;
   DeclMoonPoint(moon.m_Vertices, cx, cy, shift + Math.Cos(b) * r, Math.Sin(b) * r, tilt);
  }
  m_Commands.Insert(moon);
 }

 protected void DeclMoonPoint(array<float> vertices, float cx, float cy, float px, float py, float tilt)
 {
  float c = Math.Cos(tilt);
  float s = Math.Sin(tilt);
  vertices.Insert(cx + px * c - py * s);
  vertices.Insert(cy + px * s + py * c);
 }
}

// ---------------------------------------------------------------- the ACA panel and INTEL
modded class ACR_CommandPanel
{
 protected Widget m_DeclAcaBar;
 protected Widget m_DeclIntelBar;
 protected TextWidget m_DeclAcaBarText;
 protected TextWidget m_DeclIntelHeadline;
 protected TextWidget m_DeclIntelNew;
 protected TextWidget m_DeclOlder;
 protected SCR_ButtonTextComponent m_DeclCompact;
 protected SCR_ButtonTextComponent m_DeclNormal;
 protected string m_sDeclBars = "-";

 static int DeclBodySize()
 {
  if (ACR_MapDeclutter.Compact()) return 15;
  return 18;
 }

 static int DeclHeadingSize()
 {
  if (ACR_MapDeclutter.Compact()) return 22;
  return 28;
 }

 override protected Color PanelBackground(float alpha)
 {
  if (!ACR_MapDeclutter.On()) return super.PanelBackground(alpha);
  return ACR_MapDeclutter.PanelColor();
 }

 override protected float PanelWidth()
 {
  if (!ACR_MapDeclutter.On()) return super.PanelWidth();
  if (ACR_MapDeclutter.Compact()) return 270;
  return 340;
 }

 override protected void StyleText(TextWidget widget, bool heading)
 {
  super.StyleText(widget, heading);
  if (!widget || !ACR_MapDeclutter.On()) return;
  widget.SetColor(ACR_MapDeclutter.TextColor());
  if (heading) widget.SetExactFontSize(DeclHeadingSize() - 4);
  else widget.SetExactFontSize(DeclBodySize());
 }

 // The title: a small blue square and the name in ACA blue.
 override protected TextWidget AddHeading(Widget parent, string text)
 {
  if (!ACR_MapDeclutter.On()) return super.AddHeading(parent, text);
  return DeclTitle(parent, text, DeclHeadingSize());
 }

 protected TextWidget DeclTitle(Widget parent, string text, int size)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget row = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, parent);
  DeclSquare(row, size * 0.42);
  TextWidget title = TextWidget.Cast(ws.CreateWidgets(LAYOUT_HEADING, row));
  if (!title) return null;
  title.SetText(text);
  title.SetColor(ACR_MapDeclutter.AccentBlue());
  title.SetExactFontSize(size);
  AlignableSlot.SetVerticalAlign(title, LayoutVerticalAlign.Center);
  return title;
 }

 protected void DeclSquare(Widget row, float size)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  SizeLayoutWidget box = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, row));
  box.EnableWidthOverride(true);
  box.SetWidthOverride(size);
  box.EnableHeightOverride(true);
  box.SetHeightOverride(size);
  AlignableSlot.SetVerticalAlign(box, LayoutVerticalAlign.Center);
  AlignableSlot.SetPadding(box, 0, 0, 8, 0);
  Widget fill = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, ACR_MapDeclutter.AccentBlue(), 0, box);
  AlignableSlot.SetHorizontalAlign(fill, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(fill, LayoutVerticalAlign.Stretch);
 }

 override protected void StyleButton(SCR_ButtonTextComponent button, Widget widget)
 {
  if (!ACR_MapDeclutter.On())
  {
   super.StyleButton(button, widget);
   return;
  }
  DeclStyle(button, false);
 }

 // A dark blue button, or (picked) the accent with dark text; no border.
 protected void DeclStyle(SCR_ButtonTextComponent button, bool picked)
 {
  if (!button) return;
  Color normal = ACR_MapDeclutter.ButtonColor();
  Color hover = ACR_MapDeclutter.ButtonHover();
  Color text = ACR_MapDeclutter.TextColor();
  if (picked)
  {
   normal = ACR_MapDeclutter.AccentBlue();
   hover = ACR_MapDeclutter.Rgb(120, 186, 248);
   text = ACR_MapDeclutter.Rgb(16, 22, 30);
  }
  button.SetBackgroundColors(normal, hover, hover, ACR_MapDeclutter.Rgb(30, 34, 40, 0.6), hover);
  if (button.m_wBackground) button.m_wBackground.SetColor(normal);
  button.m_bShowBorderOnFocus = false;
  button.m_bShowBorderOnHover = false;
  if (button.m_wBorder) button.m_wBorder.SetOpacity(0);
  TextWidget label = button.GetTextWidget();
  if (label)
  {
   label.SetColor(text);
   label.SetExactFontSize(DeclBodySize());
  }
  button.m_TextDefault = text;
  button.m_TextToggled = text;
 }

 // "UI size  [Compact] [Normal]" under the main buttons.
 override protected void AddExtraRows(Widget content)
 {
  super.AddExtraRows(content);
  if (!ACR_MapDeclutter.On()) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget row = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, content);
  AlignableSlot.SetPadding(row, 0, 8, 0, 0);
  TextWidget label = AddText(row, LAYOUT_TEXT, "UI size");
  if (label)
  {
   label.SetColor(ACR_MapDeclutter.GreyText());
   LayoutSlot.SetSizeMode(label, LayoutSizeMode.Fill);
   AlignableSlot.SetVerticalAlign(label, LayoutVerticalAlign.Center);
  }
  // Fixed widths that fit the words ("Compac" was clipped in the pn12f shots).
  float width = 112;
  if (ACR_MapDeclutter.Compact()) width = 92;
  m_DeclCompact = DeclButton(row, "Compact", width);
  if (m_DeclCompact) m_DeclCompact.m_OnClicked.Insert(OnDeclCompact);
  m_DeclNormal = DeclButton(row, "Normal", width);
  if (m_DeclNormal) m_DeclNormal.m_OnClicked.Insert(OnDeclNormal);
  DeclStyle(m_DeclCompact, ACR_MapDeclutter.Compact());
  DeclStyle(m_DeclNormal, !ACR_MapDeclutter.Compact());
  DeclSmallText(m_DeclCompact);
  DeclSmallText(m_DeclNormal);
 }

 protected void DeclSmallText(SCR_ButtonTextComponent button)
 {
  if (button && button.GetTextWidget()) button.GetTextWidget().SetExactFontSize(DeclBodySize() - 2);
 }

 // A button; width > 0: in a box that wide, the button filling it.
 protected SCR_ButtonTextComponent DeclButton(Widget parent, string text, float width = 0)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget holder = parent;
  if (width > 0)
  {
   SizeLayoutWidget box = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, parent));
   box.EnableWidthOverride(true);
   box.SetWidthOverride(width);
   AlignableSlot.SetVerticalAlign(box, LayoutVerticalAlign.Center);
   holder = box;
  }
  Widget widget = ws.CreateWidgets(LAYOUT_BUTTON, holder);
  if (!widget) return null;
  if (holder != parent)
  {
   AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
   AlignableSlot.SetVerticalAlign(widget, LayoutVerticalAlign.Stretch);
  }
  AlignableSlot.SetPadding(widget, 2, 2, 2, 2);
  SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(widget);
  if (!button) return null;
  button.SetText(text);
  DeclStyle(button, false);
  return button;
 }

 protected void OnDeclCompact(SCR_ButtonBaseComponent button)
 {
  if (m_Owner) m_Owner.ACR_DeclSetCompact(true);
 }

 protected void OnDeclNormal(SCR_ButtonBaseComponent button)
 {
  if (m_Owner) m_Owner.ACR_DeclSetCompact(false);
 }

 protected void OnDeclOpen(SCR_ButtonBaseComponent button)
 {
  if (m_Owner) m_Owner.ACR_DeclOpenPanel();
 }

 // INTEL: the dark box, "■ INTEL ... +N older", the last INTEL_SHOWN merged lines. Then the bars.
 override protected void CreateFeed(Widget parent)
 {
  if (!ACR_MapDeclutter.On())
  {
   super.CreateFeed(parent);
   return;
  }
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_FeedRoot = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_FeedRoot, 0, 0);
  FrameSlot.SetAnchorMax(m_FeedRoot, 0, 0);
  FrameSlot.SetAlignment(m_FeedRoot, 0, 0);
  FrameSlot.SetPos(m_FeedRoot, 110, 96); // clear of the map toolbar on the left
  FrameSlot.SetSizeToContent(m_FeedRoot, true);
  m_FeedRoot.SetZOrder(1000);
  DeclFill(m_FeedRoot);
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 1, m_FeedRoot));
  size.EnableWidthOverride(true);
  if (ACR_MapDeclutter.Compact()) size.SetWidthOverride(330);
  else size.SetWidthOverride(390);
  AlignableSlot.SetPadding(size, 12, 8, 12, 10);
  Widget column = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, size);
  AlignableSlot.SetHorizontalAlign(column, LayoutHorizontalAlign.Stretch);
  Widget header = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, column);
  AlignableSlot.SetHorizontalAlign(header, LayoutHorizontalAlign.Stretch);
  TextWidget title = DeclTitle(header, "INTEL", DeclBodySize() + 3);
  if (title) LayoutSlot.SetSizeMode(title.GetParent(), LayoutSizeMode.Fill);
  m_DeclOlder = AddText(header, LAYOUT_TEXT, "");
  if (m_DeclOlder)
  {
   m_DeclOlder.SetColor(ACR_MapDeclutter.GreyText());
   m_DeclOlder.SetExactFontSize(DeclBodySize() - 3);
   AlignableSlot.SetVerticalAlign(m_DeclOlder, LayoutVerticalAlign.Center);
   m_DeclOlder.SetVisible(false);
  }
  m_FeedText = AddText(column, LAYOUT_TEXT, "No contacts reported");
  if (m_FeedText) m_FeedText.SetColor(ACR_MapDeclutter.GreyText());
  m_FeedLines.Clear();
  for (int i = 0; i < ACR_MapDeclutter.INTEL_SHOWN; i++)
  {
   TextWidget line = AddText(column, LAYOUT_TEXT, "");
   if (!line) continue;
   line.SetVisible(false);
   m_FeedLines.Insert(line);
  }
  DeclBars(parent);
 }

 // A stretched panel colour behind an overlay's other children.
 protected void DeclFill(Widget overlay)
 {
  Widget image = GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, ACR_MapDeclutter.PanelColor(), 0, overlay);
  AlignableSlot.SetHorizontalAlign(image, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(image, LayoutVerticalAlign.Stretch);
 }

 // The lines are already merged (SCR_PlayerController.ACR_FeedLines): the first INTEL_SHOWN, and
 // "+N older" for the rest.
 override void SetFeed(array<string> lines)
 {
  if (!ACR_MapDeclutter.On())
  {
   super.SetFeed(lines);
   return;
  }
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
  int older = 0;
  if (lines) older = ACR_MapDeclutter.Older(lines.Count());
  if (!m_DeclOlder) return;
  m_DeclOlder.SetVisible(older > 0);
  m_DeclOlder.SetText(string.Format("+%1 older", older));
 }

 // Zoomed out: "■ INTEL  headline · N new" top left, "■ AC: ACTUAL  RP 48 · +9  [Open]" top right.
 protected void DeclBars(Widget parent)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_DeclIntelBar = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_DeclIntelBar, 0, 0);
  FrameSlot.SetAnchorMax(m_DeclIntelBar, 0, 0);
  FrameSlot.SetAlignment(m_DeclIntelBar, 0, 0);
  FrameSlot.SetPos(m_DeclIntelBar, 110, 96);
  FrameSlot.SetSizeToContent(m_DeclIntelBar, true);
  m_DeclIntelBar.SetZOrder(1000);
  DeclFill(m_DeclIntelBar);
  SizeLayoutWidget intelSize = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 1, m_DeclIntelBar));
  AlignableSlot.SetPadding(intelSize, 12, 6, 14, 6);
  Widget intelRow = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, intelSize);
  DeclTitle(intelRow, "INTEL", DeclBodySize());
  m_DeclIntelHeadline = AddText(intelRow, LAYOUT_TEXT, "");
  if (m_DeclIntelHeadline)
  {
   AlignableSlot.SetPadding(m_DeclIntelHeadline, 10, 0, 0, 0);
   AlignableSlot.SetVerticalAlign(m_DeclIntelHeadline, LayoutVerticalAlign.Center);
  }
  m_DeclIntelNew = AddText(intelRow, LAYOUT_TEXT, "");
  if (m_DeclIntelNew)
  {
   m_DeclIntelNew.SetColor(ACR_MapDeclutter.GreyText());
   AlignableSlot.SetPadding(m_DeclIntelNew, 6, 0, 0, 0);
   AlignableSlot.SetVerticalAlign(m_DeclIntelNew, LayoutVerticalAlign.Center);
  }

  m_DeclAcaBar = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, 0, Color.FromInt(Color.WHITE), 0, parent);
  FrameSlot.SetAnchorMin(m_DeclAcaBar, 1, 0);
  FrameSlot.SetAnchorMax(m_DeclAcaBar, 1, 0);
  FrameSlot.SetAlignment(m_DeclAcaBar, 1, 0);
  FrameSlot.SetPos(m_DeclAcaBar, -24, 96);
  FrameSlot.SetSizeToContent(m_DeclAcaBar, true);
  m_DeclAcaBar.SetZOrder(1000);
  DeclFill(m_DeclAcaBar);
  SizeLayoutWidget acaSize = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 1, m_DeclAcaBar));
  AlignableSlot.SetPadding(acaSize, 12, 4, 6, 4);
  Widget acaRow = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, acaSize);
  DeclTitle(acaRow, "AC: ACTUAL", DeclBodySize());
  m_DeclAcaBarText = AddText(acaRow, LAYOUT_TEXT, "");
  if (m_DeclAcaBarText)
  {
   AlignableSlot.SetPadding(m_DeclAcaBarText, 10, 0, 10, 0);
   AlignableSlot.SetVerticalAlign(m_DeclAcaBarText, LayoutVerticalAlign.Center);
  }
  SCR_ButtonTextComponent open = DeclButton(acaRow, "Open");
  if (open) open.m_OnClicked.Insert(OnDeclOpen);
 }

 // INTEL stands under the 2IC's card when that shows (part A names its root "ACR_TwoICCard"; no compile
 // dependency) and hasn't been put elsewhere; else at the top. The player's own spots, and the rest,
 // are ACR_PanelMover's.
 void DeclPlace()
 {
  if (!m_Root) return;
  Widget menuRoot = m_Root.GetParent();
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (!menuRoot || !ws) return;
  float intelY = ACR_UiPrefs.FEED_HOME_Y;
  Widget card = menuRoot.FindAnyWidget("ACR_TwoICCard");
  if (!ACR_UiPrefs.Moved(ACR_UiPrefs.CARD) && card && card.IsVisibleInHierarchy())
  {
   float cx, cy, cw, ch;
   card.GetScreenPos(cx, cy);
   card.GetScreenSize(cw, ch);
   if (ch > 0) intelY = ws.DPIUnscale(cy + ch) + 8;
  }
  ACR_PanelMover.Natural(ACR_UiPrefs.FEED, ACR_UiPrefs.FEED_HOME_X, intelY);
 }

 // Folded, each panel is held by its bar, and stands where it would unfolded.
 override protected void RegisterPanels()
 {
  super.RegisterPanels();
  ACR_PanelMover.AddBox(ACR_UiPrefs.PANEL, m_DeclAcaBar);
  ACR_PanelMover.Body(ACR_UiPrefs.PANEL, m_DeclAcaBar);
  ACR_PanelMover.AddBox(ACR_UiPrefs.FEED, m_DeclIntelBar);
  ACR_PanelMover.TitleBar(ACR_UiPrefs.FEED, m_DeclIntelBar, 0);
 }

 // Collapse to the bars (or back): aca / intel as asked; the bars' texts.
 void SetCollapsed(bool aca, bool intel, string acaText, string headline, int fresh)
 {
  if (!m_DeclAcaBar || !m_DeclIntelBar) return;
  string key = string.Format("%1|%2|%3|%4|%5", aca, intel, acaText, headline, fresh);
  if (key == m_sDeclBars) return;
  m_sDeclBars = key;
  if (m_Root) m_Root.SetVisible(!aca);
  m_DeclAcaBar.SetVisible(aca);
  if (m_FeedRoot) m_FeedRoot.SetVisible(!intel);
  m_DeclIntelBar.SetVisible(intel);
  if (m_DeclAcaBarText) m_DeclAcaBarText.SetText(acaText);
  if (m_DeclIntelHeadline) m_DeclIntelHeadline.SetText(headline);
  if (!m_DeclIntelNew) return;
  m_DeclIntelNew.SetVisible(fresh > 0);
  m_DeclIntelNew.SetText(string.Format("· %1 new", fresh));
 }

 bool DeclCollapsed() { return m_DeclAcaBar && m_DeclAcaBar.IsVisible(); }
 bool DeclIntelCollapsed() { return m_DeclIntelBar && m_DeclIntelBar.IsVisible(); }
 bool DeclStyled() { return m_DeclCompact != null && m_DeclOlder != null; }

 override bool Contains(Widget widget)
 {
  if (super.Contains(widget)) return true;
  while (widget)
  {
   if ((m_DeclAcaBar && widget == m_DeclAcaBar) || (m_DeclIntelBar && widget == m_DeclIntelBar)) return true;
   widget = widget.GetParent();
  }
  return false;
 }

 override void Destroy()
 {
  if (m_DeclAcaBar) m_DeclAcaBar.RemoveFromHierarchy();
  m_DeclAcaBar = null;
  if (m_DeclIntelBar) m_DeclIntelBar.RemoveFromHierarchy();
  m_DeclIntelBar = null;
  m_DeclOlder = null;
  m_DeclCompact = null;
  m_DeclNormal = null;
  super.Destroy();
 }
}

// ---------------------------------------------------------------- the player's map
modded class SCR_PlayerController
{
 // ---------------------------------------------------------------- the squads behind a "×N"
 protected ref ACR_SquadPicker m_ACR_SquadPicker;

 // A click on an icon that stands for several squads opens the list of them (ACR_SquadPicker);
 // Shift + click, a lone squad and the classic map keep the plain select.
 override void ACR_PickGroup(ACR_UnitMarker marker)
 {
  if (ACR_DoubleClick(marker)) return;
  ACR_CloseSquadPicker();
  int g = -1;
  if (marker && m_ACR_Data) g = m_ACR_Data.m_GroupIds.Find(marker.m_GroupId);
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (g < 0 || !m_ACR_Data.m_bDeclOn || ACR_ModifierHeld("ACR_MapShift") || g >= m_ACR_Data.m_DeclCount.Count() || m_ACR_Data.m_DeclCount[g] < 2 || g >= m_ACR_Data.m_DeclBaseOf.Count() || !mapEntity || !mapEntity.GetMapMenuRoot())
  {
   super.ACR_PickGroup(marker);
   return;
  }
  int base = m_ACR_Data.m_DeclBaseOf[g];
  array<RplId> ids = {};
  array<string> names = {};
  array<string> types = {};
  array<string> doings = {};
  array<string> strengths = {};
  array<bool> selected = {};
  foreach (int k, int at : m_ACR_Data.m_DeclBaseOf)
  {
   if (at != base || k >= m_ACR_Data.m_GroupIds.Count()) continue;
   RplId id = m_ACR_Data.m_GroupIds[k];
   ids.Insert(id);
   string name = "Squad";
   if (k < m_ACR_Data.m_GroupNames.Count()) name = m_ACR_Data.m_GroupNames[k];
   names.Insert(name);
   types.Insert(ACR_SquadPicker.TypeWord(ACR_GroupIcons(id)));
   string doing = "";
   if (k < m_ACR_Data.m_DeclMark.Count() && m_ACR_Data.m_DeclMark[k] >= 0) doing = ACR_MapDeclutter.MarkName(m_ACR_Data.m_DeclMark[k]);
   doings.Insert(doing);
   int health = -1;
   int ammo = -1;
   int s = m_ACR_Data.m_StatusGroups.Find(id);
   if (s >= 0 && s * 6 + 1 < m_ACR_Data.m_Status.Count())
   {
    health = m_ACR_Data.m_Status[s * 6];
    ammo = m_ACR_Data.m_Status[s * 6 + 1];
   }
   strengths.Insert(ACR_SquadPicker.Strength(health, ammo));
   selected.Insert(ACR_IsSelected(id));
  }
  string where = "here";
  if (base >= 0 && base < m_ACR_Data.m_BaseNames.Count()) where = m_ACR_Data.m_BaseNames[base];
  // Without the size tag ("Farm [1]": the calm map shows size as pips). ToUpper changes the string
  // itself and returns its length (the title read "5 SQUADS AT 8").
  int tag = where.IndexOf(" [");
  if (tag > 0) where = where.Substring(0, tag);
  where.ToUpper();
  int mx, my;
  WidgetManager.GetMousePos(mx, my);
  m_ACR_SquadPicker = new ACR_SquadPicker();
  m_ACR_SquadPicker.Create(mapEntity.GetMapMenuRoot(), this, string.Format("%1 SQUADS AT %2", ids.Count(), where), ids, names, types, doings, strengths, selected, mx, my);
 }

 // Squad g's kind has its layer off (ACR_PlayerMapSquads); a selected squad always shows.
 protected bool ACR_LayerHides(int g)
 {
  return false;
 }

 // A squad's symbol icons (EMilitarySymbolIcon), from its marker; 0 unknown.
 protected int ACR_GroupIcons(RplId groupId)
 {
  foreach (ACR_UnitMarker marker : m_ACR_GroupMarkers)
  {
   if (marker && marker.m_GroupId == groupId && marker.m_GroupSymbol) return marker.m_GroupSymbol.GetIcons();
  }
  return 0;
 }

 void ACR_SquadPickerHover(RplId groupId)
 {
  if (m_ACR_Data) m_ACR_Data.m_PickerHover = groupId;
 }

 // Number keys 1-9 pick a row while the list is open.
 override protected void ACR_UpdatePicker()
 {
  super.ACR_UpdatePicker();
  if (!ACR_SquadPickerOpen()) return;
  InputManager input = GetGame().GetInputManager();
  if (!input) return;
  input.ActivateContext("ACR_SquadPickerContext");
  array<string> keys = {"ACR_Ping1", "ACR_Ping2", "ACR_Ping3", "ACR_Ping4", "ACR_Pick5", "ACR_Pick6", "ACR_Pick7", "ACR_Pick8", "ACR_Pick9"};
  foreach (int k, string action : keys)
  {
   if (!input.GetActionTriggered(action)) continue;
   m_ACR_SquadPicker.ChooseIndex(k);
   return;
  }
 }

 // The pulse on a spot the player was sent to (ACR_FocusMap).
 void ACR_PulseAt(vector pos)
 {
  if (!m_ACR_Data) return;
  m_ACR_Data.m_vPulseAt = pos;
  m_ACR_Data.m_iPulseUntil = System.GetTickCount() + ACR_OrderDisplay.PULSE_MS;
 }

 // Sends the player's eye to 'at' (the 2IC panel's Show, an INTEL line, a double-click on a squad):
 // the map pans there, zooms in to about 'span' m across the screen when it is further out than that
 // (zoomed right out a squad was centred but tiny), and a pulse marks the spot.
 static const float SHOW_SPAN = 3000;
 static const float DOUBLE_CLICK_SPAN = 1200;
 void ACR_FocusMap(vector at, float span = SHOW_SPAN)
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen()) return;
  float zoom = Math.Max(mapEntity.GetCurrentZoom(), GetGame().GetWorkspace().GetWidth() / span);
  mapEntity.ZoomPanSmooth(zoom, at[0], at[2], 0.3);
  ACR_PulseAt(at);
  ACR_MarkUiClick();
 }

 // ---------------------------------------------------------------- INTEL lines that jump
 // A click on an INTEL line that names a place (a sighting, a fire mission, a minefield laid, a
 // position dug in, a support called, a ping: ACR_Intel.AddFeed's spot) sends the map there; the
 // line under the cursor that would is lit. The feed's widgets ignore the cursor, so the click
 // reaches the map and is matched here by where it fell.
 protected bool ACR_FeedSpotAt(int x, int y, out vector spot, out int line)
 {
  line = -1;
  if (!m_ACR_Panel) return false;
  line = m_ACR_Panel.FeedLineAt(x, y);
  if (line < 0 || line >= m_ACR_FeedShownPos.Count()) return false;
  spot = m_ACR_FeedShownPos[line];
  return spot != vector.Zero;
 }

 override protected void ACR_OnMapClick(vector screenPos)
 {
  int mx, my;
  WidgetManager.GetMousePos(mx, my);
  vector spot;
  int line;
  if (!ACR_EditorOpen() && ACR_FeedSpotAt(mx, my, spot, line))
  {
   m_bACR_FeedJumpedForTest = true;
   ACR_FocusMap(spot);
   return;
  }
  super.ACR_OnMapClick(screenPos);
 }

 protected void ACR_LightFeed()
 {
  if (!m_ACR_Panel) return;
  int mx, my;
  WidgetManager.GetMousePos(mx, my);
  vector spot;
  int line;
  if (ACR_FeedSpotAt(mx, my, spot, line)) m_ACR_Panel.LightFeedLine(line);
  else m_ACR_Panel.LightFeedLine(-1);
 }

 // Tests: a real click on an INTEL line has jumped the map.
 protected bool m_bACR_FeedJumpedForTest;
 bool ACR_FeedJumpedForTest() { return m_bACR_FeedJumpedForTest; }

 // Tests: the first INTEL line shown that names a place, clicked; its spot (vector.Zero: none).
 vector ACR_FeedJumpForTest()
 {
  foreach (vector spot : m_ACR_FeedShownPos)
  {
   if (spot == vector.Zero) continue;
   ACR_FocusMap(spot);
   return spot;
  }
  return vector.Zero;
 }

 // Tests: a double-click on the first squad's own icon (not one in a "×N"); where it stands.
 vector ACR_DoubleClickForTest()
 {
  if (!m_ACR_Data) return vector.Zero;
  foreach (ACR_UnitMarker marker : m_ACR_GroupMarkers)
  {
   if (!marker || !marker.m_GroupId.IsValid()) continue;
   int g = m_ACR_Data.m_GroupIds.Find(marker.m_GroupId);
   if (g < 0 || m_ACR_Data.ShownByOther(g)) continue;
   m_ACR_LastPickAt = 0;
   ACR_PickGroup(marker);
   ACR_PickGroup(marker);
   return m_ACR_Data.LeaderPos(g);
  }
  return vector.Zero;
 }

 // ---------------------------------------------------------------- double-click on a squad
 static const int DOUBLE_CLICK_MS = 400;
 protected RplId m_ACR_LastPickId = RplId.Invalid();
 protected int m_ACR_LastPickAt;

 // A second click on the same squad's icon within DOUBLE_CLICK_MS: selected, the map centred and
 // zoomed in on it (as in most RTS games).
 protected bool ACR_DoubleClick(ACR_UnitMarker marker)
 {
  if (!marker || !marker.m_GroupId.IsValid() || !m_ACR_Data || ACR_ModifierHeld("ACR_MapShift")) return false;
  int now = System.GetTickCount();
  bool twice = marker.m_GroupId == m_ACR_LastPickId && now - m_ACR_LastPickAt < DOUBLE_CLICK_MS;
  m_ACR_LastPickId = marker.m_GroupId;
  m_ACR_LastPickAt = now;
  if (!twice) return false;
  m_ACR_LastPickAt = 0;
  int g = m_ACR_Data.m_GroupIds.Find(marker.m_GroupId);
  if (g < 0) return false;
  ACR_CloseSquadPicker();
  ACR_SelectGroup(marker.m_GroupId);
  ACR_FocusMap(m_ACR_Data.LeaderPos(g), DOUBLE_CLICK_SPAN);
  return true;
 }

 void ACR_SquadPickerChoose(RplId id)
 {
  ACR_MarkUiClick();
  ACR_CloseSquadPicker();
  ACR_SelectGroup(id);
 }

 void ACR_SquadPickerAll(array<RplId> ids)
 {
  ACR_MarkUiClick();
  array<RplId> all = {};
  all.Copy(ids);
  ACR_CloseSquadPicker();
  m_ACR_SelectedBase = "";
  m_ACR_CursorMode = ACR_ECursorMode.MOVE;
  ACR_SetSelection(all);
 }

 void ACR_CloseSquadPicker()
 {
  if (m_ACR_SquadPicker) m_ACR_SquadPicker.Destroy();
  m_ACR_SquadPicker = null;
 }

 bool ACR_SquadPickerOpen() { return m_ACR_SquadPicker && m_ACR_SquadPicker.m_Root; }

 void ACR_ClearSelectionForTest()
 {
  array<RplId> none = {};
  ACR_SetSelection(none);
 }

 // Tests: a row hovered, a row's squad, a row picked as its number key would.
 bool ACR_SquadPickerHoverForTest(int i)
 {
  if (!m_ACR_SquadPicker || !m_ACR_Data) return false;
  m_ACR_SquadPicker.Hover(i);
  return m_ACR_Data.m_PickerHover.IsValid();
 }

 RplId ACR_SquadPickerIdForTest(int i)
 {
  if (!m_ACR_SquadPicker) return RplId.Invalid();
  return m_ACR_SquadPicker.IdAt(i);
 }

 void ACR_SquadPickerKeyForTest(int i)
 {
  if (m_ACR_SquadPicker) m_ACR_SquadPicker.ChooseIndex(i);
 }

 // Tests: opens the list for the first "×N" icon shown, as a click on it would.
 bool ACR_SquadPickerForTest()
 {
  if (!m_ACR_Data) return false;
  foreach (ACR_UnitMarker marker : m_ACR_GroupMarkers)
  {
   if (!marker) continue;
   int g = m_ACR_Data.m_GroupIds.Find(marker.m_GroupId);
   if (g < 0 || g >= m_ACR_Data.m_DeclCount.Count() || g >= m_ACR_Data.m_DeclRep.Count() || m_ACR_Data.m_DeclCount[g] < 2 || !m_ACR_Data.m_DeclRep[g]) continue;
   ACR_PickGroup(marker);
   return ACR_SquadPickerOpen();
  }
  return false;
 }

 // A click elsewhere on the map, or the map closing, closes it with the ping picker.
 override protected void ACR_ClosePicker()
 {
  ACR_CloseSquadPicker();
  super.ACR_ClosePicker();
 }

 override protected bool ACR_OverOwnUI(Widget widget)
 {
  if (m_ACR_SquadPicker && m_ACR_SquadPicker.Contains(widget)) return true;
  return super.ACR_OverOwnUI(widget);
 }

 override bool ACR_HandleMapRightClick()
 {
  if (m_ACR_SquadPicker && m_ACR_SquadPicker.Contains(WidgetManager.GetWidgetUnderCursor())) return true;
  return super.ACR_HandleMapRightClick();
 }

 protected ref array<Widget> m_aDeclBadges = {};
 protected ref array<TextWidget> m_aDeclBadgeTexts = {};
 protected Widget m_wDeclLegend;
 protected ref array<Widget> m_aDeclLegendSpots = {};
 protected Widget m_wDeclTint;
 protected bool m_bDeclForcedOpen;   // [Open] on the bar: the panel stays open until zoomed in again
 protected bool m_bDeclTouched;      // markers were hidden or moved: put back if the option goes off
 protected int m_iDeclIntelSeenAt;

 void ACR_SendMapStyle(bool on)
 {
  Rpc(ACR_MapStyleOwner, on);
 }

 [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
 protected void ACR_MapStyleOwner(bool on)
 {
  ACR_MapDeclutter.SetOn(on);
 }

 // The calm map's layout first, then the canvas.
 override protected void ACR_DrawOrders(SCR_MapEntity mapEntity)
 {
  ACR_DeclLayout(mapEntity);
  super.ACR_DrawOrders(mapEntity);
 }

 protected void ACR_DeclLayout(SCR_MapEntity mapEntity)
 {
  ACR_MapData data = m_ACR_Data;
  data.m_bDeclOn = ACR_MapDeclutter.On();
  data.m_bDeclFar = false;
  if (!data.m_bDeclOn) return;
  m_bDeclTouched = true;
  // Really far out: near the map's widest zoom.
  data.m_bDeclFar = ACR_MapDeclutter.IsFar(mapEntity.GetCurrentZoom(), mapEntity.GetMinZoom());
  ACR_DeclBases(data);
  ACR_DeclCaptions(mapEntity, data);
  ACR_DeclSquads(mapEntity, data);
 }

 // Each base's side, pips, HQ and whether it keeps its name.
 protected void ACR_DeclBases(ACR_MapData data)
 {
  string mine = "";
  Faction faction = SCR_FactionManager.SGetLocalPlayerFaction();
  if (faction) mine = faction.GetFactionKey();
  data.m_sDeclSelectedBase = m_ACR_SelectedBase;
  data.m_DeclSide.Clear();
  data.m_DeclPips.Clear();
  data.m_DeclHQ.Clear();
  data.m_DeclKeep.Clear();
  foreach (int i, string name : data.m_BaseNames)
  {
   string owner = "";
   if (i < data.m_BaseOwners.Count()) owner = data.m_BaseOwners[i];
   string detail = "";
   if (i < data.m_BaseDetails.Count()) detail = data.m_BaseDetails[i];
   data.m_DeclSide.Insert(ACR_MapDeclutter.SideOf(owner, mine));
   data.m_DeclPips.Insert(ACR_MapDeclutter.Pips(name, detail));
   data.m_DeclHQ.Insert(ACR_MapDeclutter.IsHQ(detail));
  }
  array<bool> front = {};
  ACR_MapDeclutter.FrontBases(data.m_BasePositions, data.m_DeclSide, front);
  foreach (int b, string baseName : data.m_BaseNames)
  {
   bool contested = b < data.m_BaseContested.Count() && data.m_BaseContested[b] == 1;
   bool isFront = b < front.Count() && front[b];
   data.m_DeclKeep.Insert(ACR_MapDeclutter.KeepsName(data.m_bDeclFar, data.m_DeclSide[b], data.m_DeclHQ[b], contested, isFront, baseName == m_ACR_SelectedBase));
  }
 }

 // A marker's own point on screen (where its world position is drawn), from its root as laid out.
 protected static bool ACR_DeclPoint(ACR_UnitMarker marker, out float x, out float y)
 {
  if (!marker) return false;
  Widget root = marker.GetRootWidget();
  if (!root || !root.IsVisible()) return false;
  float rx, ry, rw, rh;
  root.GetScreenPos(rx, ry);
  root.GetScreenSize(rw, rh);
  if (rw <= 0 || rh <= 0) return false;
  vector align = FrameSlot.GetAlignment(root);
  x = rx + rw * align[0];
  y = ry + rh * align[1];
  return true;
 }

 // Each base's dot, its caption's rectangle from its marker's point (as laid out last frame; kept
 // while the pointer is on it, when it grows to the full details), and where the names go so that
 // none covers another or a dot: the selected base, bases under attack, HQs and ours choose first.
 protected void ACR_DeclCaptions(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  int count = data.m_BaseNames.Count();
  while (data.m_DeclCaption.Count() < count * 4) data.m_DeclCaption.Insert(0);
  while (data.m_DeclCaption.Count() > count * 4) data.m_DeclCaption.Remove(data.m_DeclCaption.Count() - 1);
  data.m_DeclDots.Clear();
  for (int i = 0; i < count; i++)
  {
   vector pos = data.m_BasePositions[i];
   int sx, sy;
   mapEntity.WorldToScreen(pos[0], pos[2], sx, sy, true);
   data.m_DeclDots.Insert(sx);
   data.m_DeclDots.Insert(sy);
   ACR_UnitMarker marker;
   if (i < m_ACR_BaseMarkers.Count()) marker = m_ACR_BaseMarkers[i];
   SCR_MapMarkerWidgetComponent comp;
   if (marker) comp = marker.GetMarkerComponent();
   if (comp && comp.ACR_Hovered()) continue;
   float x0, y0, x1, y1;
   float px, py;
   bool named = i < data.m_DeclKeep.Count() && data.m_DeclKeep[i] && marker && !marker.m_BaseCaption.IsEmpty();
   if (named && comp && comp.ACR_Text() && comp.ACR_Text().IsVisible() && ACR_DeclPoint(marker, px, py))
   {
    float tx, ty, tw, th;
    comp.ACR_Text().GetScreenPos(tx, ty);
    comp.ACR_Text().GetScreenSize(tw, th);
    if (tw > 0 && th > 0)
    {
     x0 = tx - px;
     y0 = ty - py;
     x1 = x0 + tw;
     y1 = y0 + th;
     // The pips belong to the name.
     if (i < data.m_DeclPips.Count() && data.m_DeclPips[i] > 0) x1 += ws.DPIScale(4 + 7 * data.m_DeclPips[i]);
    }
   }
   data.m_DeclCaption[i * 4] = x0;
   data.m_DeclCaption[i * 4 + 1] = y0;
   data.m_DeclCaption[i * 4 + 2] = x1;
   data.m_DeclCaption[i * 4 + 3] = y1;
  }
  array<int> order = {};
  for (int pass = 0; pass < 2; pass++)
  {
   for (int b = 0; b < count; b++)
   {
    bool first = data.m_BaseNames[b] == m_ACR_SelectedBase;
    first = first || (b < data.m_BaseContested.Count() && data.m_BaseContested[b] == 1);
    first = first || (b < data.m_DeclHQ.Count() && data.m_DeclHQ[b]) || (b < data.m_DeclSide.Count() && data.m_DeclSide[b] == ACR_EBaseSide.OWN);
    if (first == (pass == 0)) order.Insert(b);
   }
  }
  ACR_MapDeclutter.PlaceNames(data.m_DeclDots, data.m_DeclCaption, order, ws.DPIScale(10), ws.DPIScale(4), data.m_DeclShift);
  data.m_DeclNames.Clear();
  for (int n = 0; n < count; n++)
  {
   float n0, n1, n2, n3;
   ACR_DeclNameRect(data, n, n0, n1, n2, n3);
   data.m_DeclNames.Insert(n0);
   data.m_DeclNames.Insert(n1);
   data.m_DeclNames.Insert(n2);
   data.m_DeclNames.Insert(n3);
  }
 }

 // A base's name where it is drawn (screen px), or false for none.
 protected static bool ACR_DeclNameRect(ACR_MapData data, int b, out float x0, out float y0, out float x1, out float y1)
 {
  if (b * 4 + 3 >= data.m_DeclCaption.Count() || b * 2 + 1 >= data.m_DeclShift.Count() || b * 2 + 1 >= data.m_DeclDots.Count()) return false;
  if (data.m_DeclCaption[b * 4 + 2] <= data.m_DeclCaption[b * 4]) return false;
  float cx = data.m_DeclDots[b * 2] + data.m_DeclShift[b * 2];
  float cy = data.m_DeclDots[b * 2 + 1] + data.m_DeclShift[b * 2 + 1];
  x0 = cx + data.m_DeclCaption[b * 4];
  y0 = cy + data.m_DeclCaption[b * 4 + 1];
  x1 = cx + data.m_DeclCaption[b * 4 + 2];
  y1 = cy + data.m_DeclCaption[b * 4 + 3];
  return true;
 }

 // Squads at a base: one icon under its dot and name. Every icon shown keeps clear of the bases'
 // names and dots (it moves below them, or aside). Each squad's corner mark.
 protected void ACR_DeclSquads(SCR_MapEntity mapEntity, ACR_MapData data)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  int groups = data.m_LeaderPositions.Count();
  array<bool> clusterable = {};
  array<bool> selected = {};
  data.m_DeclSymW.Clear();
  data.m_DeclSymH.Clear();
  data.m_DeclSymX.Clear();
  data.m_DeclSymY.Clear();
  data.m_DeclHidden.Clear();
  for (int g = 0; g < groups; g++)
  {
   bool hidden = ACR_LayerHides(g);
   data.m_DeclHidden.Insert(hidden);
   clusterable.Insert(!hidden && g < data.m_Commandable.Count() && data.m_Commandable[g] == 1);
   bool picked = g < data.m_GroupIds.Count() && data.IsSelected(data.m_GroupIds[g]);
   selected.Insert(picked);
   float w = ws.DPIScale(30);
   float h = ws.DPIScale(22);
   float ox, oy;
   ACR_UnitMarker marker;
   if (g < m_ACR_GroupMarkers.Count()) marker = m_ACR_GroupMarkers[g];
   if (marker && marker.GetMarkerComponent())
   {
    Widget symbol = marker.GetMarkerComponent().ACR_Symbol();
    float px, py;
    if (symbol && symbol.IsVisible() && ACR_DeclPoint(marker, px, py))
    {
     float sx, sy, sw, sh;
     symbol.GetScreenPos(sx, sy);
     symbol.GetScreenSize(sw, sh);
     if (sw > 4 && sh > 4)
     {
      w = sw;
      h = sh;
      ox = sx + sw * 0.5 - px;
      oy = sy + sh * 0.5 - py;
     }
    }
   }
   data.m_DeclSymW.Insert(w);
   data.m_DeclSymH.Insert(h);
   data.m_DeclSymX.Insert(ox);
   data.m_DeclSymY.Insert(oy);
  }
  ACR_MapDeclutter.Clusters(data.m_LeaderPositions, clusterable, selected, data.m_BasePositions, data.m_BaseRadii, data.m_DeclBaseOf, data.m_DeclCount, data.m_DeclRep);
  // A hidden squad is in no "×N" and its icon goes (shown by none).
  foreach (int hg, bool off : data.m_DeclHidden)
  {
   if (off && hg < data.m_DeclRep.Count()) data.m_DeclRep[hg] = false;
  }
  // What icons keep clear of: every base's dot, and its name where it is now.
  float dot = ws.DPIScale(10);
  float gap = ws.DPIScale(4);
  array<float> obstacles = {};
  int bases = data.m_DeclDots.Count() / 2;
  for (int b = 0; b < bases; b++)
  {
   float dx = data.m_DeclDots[b * 2];
   float dy = data.m_DeclDots[b * 2 + 1];
   obstacles.Insert(dx - dot);
   obstacles.Insert(dy - dot);
   obstacles.Insert(dx + dot);
   obstacles.Insert(dy + dot);
   float n0, n1, n2, n3;
   if (!ACR_DeclNameRect(data, b, n0, n1, n2, n3)) continue;
   obstacles.Insert(n0);
   obstacles.Insert(n1);
   obstacles.Insert(n2);
   obstacles.Insert(n3);
  }
  data.m_DeclObstacles.Copy(obstacles);
  data.m_DeclShown.Clear();
  foreach (int i, vector leader : data.m_LeaderPositions)
  {
   vector shown = leader;
   int at = data.m_DeclBaseOf[i];
   if (!data.ShownByOther(i))
   {
    int lx, ly;
    mapEntity.WorldToScreen(leader[0], leader[2], lx, ly, true);
    float cx = lx + data.m_DeclSymX[i];
    float cy = ly + data.m_DeclSymY[i];
    if (at >= 0 && at < bases)
    {
     // Under the dot, and under the name when the name is under the dot.
     cx = data.m_DeclDots[at * 2];
     float below = data.m_DeclDots[at * 2 + 1] + dot;
     float r0, r1, r2, r3;
     if (ACR_DeclNameRect(data, at, r0, r1, r2, r3) && r1 >= data.m_DeclDots[at * 2 + 1]) below = Math.Max(below, r3);
     cy = below + gap + data.m_DeclSymH[i] * 0.5;
    }
    vector clear = ACR_MapDeclutter.Avoid(cx, cy, data.m_DeclSymW[i], data.m_DeclSymH[i], obstacles, gap);
    bool moved = at >= 0 || Math.AbsFloat(clear[0] - (lx + data.m_DeclSymX[i])) > 0.5 || Math.AbsFloat(clear[1] - (ly + data.m_DeclSymY[i])) > 0.5;
    if (moved)
    {
     float wx, wz;
     mapEntity.ScreenToWorld(clear[0] - data.m_DeclSymX[i], clear[1] - data.m_DeclSymY[i], wx, wz);
     shown = Vector(wx, leader[1], wz);
    }
   }
   data.m_DeclShown.Insert(shown);
  }
  // Squads shown by another's icon stand where that icon is.
  foreach (int k, int base : data.m_DeclBaseOf)
  {
   if (base < 0 || !data.ShownByOther(k)) continue;
   foreach (int j, int other : data.m_DeclBaseOf)
   {
    if (other != base || data.ShownByOther(j)) continue;
    data.m_DeclShown[k] = data.m_DeclShown[j];
    break;
   }
  }
  // Corner marks: fighting (a fresh contact close), moving (not there yet), holding.
  data.m_DeclMark.Clear();
  int since = ACR_MapData.Since(m_ACR_IntelAt);
  foreach (int m, vector pos : data.m_LeaderPositions)
  {
   if (m >= data.m_Commandable.Count() || data.m_Commandable[m] != 1 || m >= data.m_GroupIds.Count())
   {
    data.m_DeclMark.Insert(-1);
    continue;
   }
   int order = data.m_OrderGroups.Find(data.m_GroupIds[m]);
   float toTarget = -1;
   if (order >= 0 && order < data.m_OrderTargets.Count()) toTarget = vector.DistanceXZ(pos, data.m_OrderTargets[order]);
   float contact = -1;
   foreach (int c, vector contactPos : m_ACR_ContactPositions)
   {
    if (c < m_ACR_ContactAges.Count() && m_ACR_ContactAges[c] + since > ACR_MapDeclutter.FIGHT_AGE) continue;
    float d = vector.DistanceXZ(pos, contactPos);
    if (contact < 0 || d < contact) contact = d;
   }
   data.m_DeclMark.Insert(ACR_MapDeclutter.SquadMark(order >= 0, toTarget, contact));
  }
 }

 // Squads shown by another's icon are hidden; the icon shown sits under its base.
 override void ACR_UpdateGroupMarkers(SCR_MapMarkerManagerComponent manager)
 {
  super.ACR_UpdateGroupMarkers(manager);
  bool on = ACR_MapDeclutter.On();
  if (!on && !m_bDeclTouched) return;
  foreach (int i, ACR_UnitMarker marker : m_ACR_GroupMarkers)
  {
   if (!marker) continue;
   bool show = true;
   if (on && i < m_ACR_Data.m_DeclRep.Count() && i < m_ACR_Data.m_DeclShown.Count())
   {
    show = m_ACR_Data.m_DeclRep[i];
    // Gathered at a base, or moved off a base's name or dot.
    vector shown = m_ACR_Data.m_DeclShown[i];
    if (i < m_ACR_Data.m_LeaderPositions.Count() && vector.DistanceXZ(shown, m_ACR_Data.m_LeaderPositions[i]) > 0.5) marker.SetWorldPos(shown[0], shown[2]);
   }
   Widget root = marker.GetRootWidget();
   if (root && root.IsVisible() != show) root.SetVisible(show);
   if (!show) ACR_Park(marker);
  }
 }

 // Base markers: the name alone (or nothing: a plain dot), the details on hover; the canvas draws
 // the dot, so the marker's own icon is hidden.
 override protected void ACR_UpdateBaseMarkers(SCR_MapMarkerManagerComponent manager)
 {
  bool on = ACR_MapDeclutter.On();
  if (!on)
  {
   super.ACR_UpdateBaseMarkers(manager);
   if (m_bDeclTouched) ACR_DeclBaseIcons(1);
   return;
  }
  if (m_ACR_BaseMarkers.Count() != m_ACR_Data.m_BaseNames.Count())
  {
   foreach (ACR_UnitMarker old : m_ACR_BaseMarkers) manager.ACR_RemoveMarker(old);
   m_ACR_BaseMarkers.Clear();
   for (int i = 0; i < m_ACR_Data.m_BaseNames.Count(); i++)
   {
    ACR_UnitMarker created = new ACR_UnitMarker();
    created.SetType(SCR_EMapMarkerType.SIMPLE);
    created.SetMarkerConfigID(1);
    created.SetCanBeRemovedByOwner(false);
    created.m_BaseIndex = i;
    m_ACR_BaseMarkers.Insert(created);
    manager.InsertStaticMarker(created, true);
    created.SetMarkerOwnerID(-1);
   }
  }
  foreach (int b, ACR_UnitMarker marker : m_ACR_BaseMarkers)
  {
   marker.m_BaseIndex = b;
   marker.m_Label = m_ACR_Data.m_BaseNames[b];
   marker.m_Selected = m_ACR_SelectedBase == marker.m_Label;
   string detail = m_ACR_Data.m_BaseDetails[b];
   int side = ACR_EBaseSide.FREE;
   if (b < m_ACR_Data.m_DeclSide.Count()) side = m_ACR_Data.m_DeclSide[b];
   bool keep = b >= m_ACR_Data.m_DeclKeep.Count() || m_ACR_Data.m_DeclKeep[b];
   marker.m_BaseCaption = ACR_MapDeclutter.Caption(marker.m_Label, detail, side, keep);
   string hoverDetail = detail;
   hoverDetail.Replace(" | ", "\n");
   marker.m_BaseHover = ACR_MapDeclutter.ShortName(marker.m_Label) + "\n" + hoverDetail;
   // Its name moved off another's (the canvas draws the dot where the base is).
   vector pos = m_ACR_Data.m_BasePositions[b];
   float wx = pos[0];
   float wz = pos[2];
   SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
   if (mapEntity && b * 2 + 1 < m_ACR_Data.m_DeclShift.Count() && b * 2 + 1 < m_ACR_Data.m_DeclDots.Count())
   {
    float dx = m_ACR_Data.m_DeclShift[b * 2];
    float dy = m_ACR_Data.m_DeclShift[b * 2 + 1];
    if (dx != 0 || dy != 0) mapEntity.ScreenToWorld(m_ACR_Data.m_DeclDots[b * 2] + dx, m_ACR_Data.m_DeclDots[b * 2 + 1] + dy, wx, wz);
   }
   marker.SetWorldPos(wx, wz);
   marker.RefreshStyle();
   if (marker.GetRootWidget()) marker.ACR_SetColor(ACR_MapDeclutter.SideColor(side));
  }
  ACR_DeclBaseIcons(0);
 }

 protected void ACR_DeclBaseIcons(float opacity)
 {
  foreach (ACR_UnitMarker marker : m_ACR_BaseMarkers)
  {
   if (!marker || !marker.GetMarkerComponent()) continue;
   ImageWidget icon = marker.GetMarkerComponent().ACR_Icon();
   if (icon && icon.GetOpacity() != opacity) icon.SetOpacity(opacity);
  }
 }

 // INTEL's lines merged, the RP part in grey.
 override protected void ACR_FeedLines(notnull array<string> lines)
 {
  if (!ACR_MapDeclutter.On())
  {
   super.ACR_FeedLines(lines);
   return;
  }
  lines.Clear();
  array<string> texts = {};
  array<int> ages = {};
  ACR_DeclFeed(texts, ages);
  string grey = ACR_DeclTag(ACR_MapDeclutter.GreyText());
  foreach (string text : texts)
  {
   string shown = text;
   int cost = text.LastIndexOf(" (-");
   if (cost > 0 && text.EndsWith(" RP)")) shown = text.Substring(0, cost) + string.Format(" <color rgba='%1'>%2</color>", grey, text.Substring(cost + 1, text.Length() - cost - 1));
   lines.Insert(shown);
  }
 }

 protected static string ACR_DeclTag(Color c)
 {
  return string.Format("%1,%2,%3,%4", (int)(c.R() * 255), (int)(c.G() * 255), (int)(c.B() * 255), (int)(c.A() * 255));
 }

 // The feed merged (ACR_MapDeclutter.MergeFeed): texts and ages, newest first, 10 minutes back.
 protected void ACR_DeclFeed(notnull array<string> texts, notnull array<int> ages)
 {
  int since = ACR_MapData.Since(m_ACR_IntelAt);
  array<string> raw = {};
  array<int> rawAges = {};
  array<vector> rawSpots = {};
  foreach (int i, string line : m_ACR_Feed)
  {
   int seconds = since;
   if (i < m_ACR_FeedAges.Count()) seconds += m_ACR_FeedAges[i];
   if (seconds > 600) break;
   raw.Insert(line);
   rawAges.Insert(seconds);
   vector spot = vector.Zero;
   if (i < m_ACR_FeedPos.Count()) spot = m_ACR_FeedPos[i];
   rawSpots.Insert(spot);
  }
  ACR_MapDeclutter.MergeFeed(raw, rawAges, m_ACR_Data.TagOf(GetPlayerId()), texts, ages, rawSpots, m_ACR_FeedShownPos);
 }

 // After the classic frame: soldiers, "×N" badges, the legend, the bars and the dark map.
 override void ACR_UpdateMap()
 {
  super.ACR_UpdateMap();
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.IsOpen() || !m_ACR_Panel) return;
  ACR_LightFeed();
  bool on = ACR_MapDeclutter.On();
  ACR_DeclTint(mapEntity, on && ACR_MapDeclutter.Dark());
  if (!on && !m_bDeclTouched) return;
  ACR_DeclSoldiers(on);
  if (on) ACR_DeclOffNames(mapEntity);
  ACR_DeclBadges(mapEntity, on);
  ACR_DeclLegend(on);
  if (on) ACR_DeclBars();
 }

 // The other round markers (pn12f v2 shots: soldiers' cyan discs, the command unit's green one and
 // an empty vehicle's pale one buried "Farm · HQ"): a soldier's on a base's name or dot is hidden
 // (his squad's icon shows him); the command unit's and our empty vehicles' move off it.
 protected void ACR_DeclOffNames(SCR_MapEntity mapEntity)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  float r = ws.DPIScale(ACR_MapDeclutter.ROUND_MARKER);
  float gap = ws.DPIScale(4);
  array<float> obstacles = m_ACR_Data.m_DeclObstacles;
  array<ACR_UnitMarker> movers = {};
  foreach (ACR_UnitMarker marker : m_ACR_Markers)
  {
   if (!marker || !marker.GetRootWidget()) continue;
   if (marker.m_Original)
   {
    movers.Insert(marker);
    continue;
   }
   if (!marker.GetRootWidget().IsVisible()) continue;
   int at[2];
   marker.GetWorldPos(at);
   int sx, sy;
   mapEntity.WorldToScreen(at[0], at[1], sx, sy, true);
   if (!ACR_MapDeclutter.OnAName(sx, sy, r, obstacles)) continue;
   marker.GetRootWidget().SetVisible(false);
   ACR_Park(marker);
  }
  foreach (ACR_UnitMarker vehicle : m_ACR_VehicleMarkers) if (vehicle) movers.Insert(vehicle);
  foreach (ACR_UnitMarker mover : movers)
  {
   if (!mover.GetRootWidget() || !mover.GetRootWidget().IsVisible()) continue;
   int pos[2];
   mover.GetWorldPos(pos);
   int mx, my;
   mapEntity.WorldToScreen(pos[0], pos[1], mx, my, true);
   vector clear = ACR_MapDeclutter.Avoid(mx, my, r * 2, r * 2, obstacles, gap);
   if (Math.AbsFloat(clear[0] - mx) < 0.5 && Math.AbsFloat(clear[1] - my) < 0.5) continue;
   float wx, wz;
   mapEntity.ScreenToWorld(clear[0], clear[1], wx, wz);
   mover.SetWorldPos(wx, wz);
  }
 }

 // A squad's soldiers: hidden zoomed out, and while it sits at a base (unless selected). Our
 // empty vehicles: hidden zoomed out.
 protected void ACR_DeclSoldiers(bool on)
 {
  foreach (ACR_UnitMarker vehicle : m_ACR_VehicleMarkers)
  {
   if (!vehicle || !vehicle.GetRootWidget()) continue;
   bool showVehicle = !on || !m_ACR_Data.m_bDeclFar;
   if (vehicle.GetRootWidget().IsVisible() != showVehicle) vehicle.GetRootWidget().SetVisible(showVehicle);
   if (!showVehicle) ACR_Park(vehicle);
  }
  foreach (ACR_UnitMarker marker : m_ACR_Markers)
  {
   if (!marker) continue;
   bool show = true;
   if (on && !marker.m_Original)
   {
    if (m_ACR_Data.m_bDeclFar) show = false;
    else if (!m_ACR_Data.IsSelected(marker.m_GroupId))
    {
     int g = m_ACR_Data.m_GroupIds.Find(marker.m_GroupId);
     if (g >= 0 && g < m_ACR_Data.m_DeclBaseOf.Count() && m_ACR_Data.m_DeclBaseOf[g] >= 0) show = false;
     if (m_ACR_Data.DeclHidden(g)) show = false;
    }
   }
   Widget root = marker.GetRootWidget();
   if (root && root.IsVisible() != show) root.SetVisible(show);
   if (!show) ACR_Park(marker);
  }
 }

 // A marker the calm map hides goes off the map as well: the game shows every marker's widget
 // again each frame (SCR_MapMarkerManagerComponent.Update: SetStaticMarkerDisabled(false) ->
 // SetUpdateDisabled(false)), so a hidden one came back before it was drawn (30 Oct: the soldiers
 // of the squads at a base, and the squads under a "×N" stacked beneath its icon, all showed). Off
 // the map it is out of sight and out of the cursor's way; its own update puts it back each frame
 // before this.
 static const int PARK = -100000;
 protected void ACR_Park(ACR_UnitMarker marker)
 {
  if (marker) marker.SetWorldPos(PARK, PARK);
 }

 // "×N" right of each icon that stands for several squads.
 protected void ACR_DeclBadges(SCR_MapEntity mapEntity, bool on)
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  int used;
  if (on)
  {
   foreach (int g, int count : m_ACR_Data.m_DeclCount)
   {
    if (count < 2 || g >= m_ACR_Data.m_DeclRep.Count() || !m_ACR_Data.m_DeclRep[g] || g >= m_ACR_Data.m_DeclSymW.Count()) continue;
    if (used >= m_aDeclBadges.Count()) ACR_DeclNewBadge(mapEntity.GetMapMenuRoot());
    if (used >= m_aDeclBadges.Count()) break;
    vector shown = m_ACR_Data.ShownPos(g);
    int sx, sy;
    mapEntity.WorldToScreen(shown[0], shown[2], sx, sy, true);
    Widget badge = m_aDeclBadges[used];
    FrameSlot.SetPos(badge, ws.DPIUnscale(sx + m_ACR_Data.m_DeclSymX[g] + m_ACR_Data.m_DeclSymW[g] * 0.5) + 3, ws.DPIUnscale(sy + m_ACR_Data.m_DeclSymY[g]));
    m_aDeclBadgeTexts[used].SetText("×" + count.ToString());
    if (!badge.IsVisible()) badge.SetVisible(true);
    used++;
   }
  }
  for (int k = used; k < m_aDeclBadges.Count(); k++)
   if (m_aDeclBadges[k].IsVisible()) m_aDeclBadges[k].SetVisible(false);
 }

 protected void ACR_DeclNewBadge(Widget root)
 {
  if (!root) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget badge = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, root);
  if (!badge) return;
  FrameSlot.SetAnchorMin(badge, 0, 0);
  FrameSlot.SetAnchorMax(badge, 0, 0);
  FrameSlot.SetAlignment(badge, 0, 0.5);
  FrameSlot.SetSizeToContent(badge, true);
  badge.SetZOrder(905); // over the map canvas (900), under the selection box layer (950)
  Widget fill = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, ACR_MapDeclutter.Rgb(20, 22, 25, 0.92), 0, badge);
  AlignableSlot.SetHorizontalAlign(fill, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(fill, LayoutVerticalAlign.Stretch);
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 1, badge));
  AlignableSlot.SetPadding(size, 5, 0, 5, 0);
  TextWidget text = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", size));
  if (!text)
  {
   badge.RemoveFromHierarchy();
   return;
  }
  text.SetColor(ACR_MapDeclutter.Rgb(240, 242, 244));
  text.SetExactFontSize(13);
  m_aDeclBadges.Insert(badge);
  m_aDeclBadgeTexts.Insert(text);
 }

 // The legend at the bottom right (near, with marks to explain): its glyphs are drawn on the
 // canvas at its spots.
 protected void ACR_DeclLegend(bool on)
 {
  m_ACR_Data.m_DeclLegend.Clear();
  bool any;
  foreach (int mark : m_ACR_Data.m_DeclMark) if (mark >= 0) any = true;
  bool want = on && any && !m_ACR_Data.m_bDeclFar;
  if (!m_wDeclLegend && want) ACR_DeclNewLegend();
  if (!m_wDeclLegend) return;
  if (m_wDeclLegend.IsVisible() != want) m_wDeclLegend.SetVisible(want);
  if (!want) return;
  array<int> marks = {ACR_ESquadMark.MOVING, ACR_ESquadMark.FIGHTING, ACR_ESquadMark.HOLDING};
  foreach (int k, Widget spot : m_aDeclLegendSpots)
  {
   float x, y, w, h;
   spot.GetScreenPos(x, y);
   spot.GetScreenSize(w, h);
   if (w <= 0 || k >= marks.Count()) continue;
   m_ACR_Data.m_DeclLegend.Insert(x + w * 0.5);
   m_ACR_Data.m_DeclLegend.Insert(y + h * 0.5);
   m_ACR_Data.m_DeclLegend.Insert(marks[k]);
  }
 }

 protected void ACR_DeclNewLegend()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!mapEntity || !mapEntity.GetMapMenuRoot()) return;
  WorkspaceWidget ws = GetGame().GetWorkspace();
  m_wDeclLegend = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, mapEntity.GetMapMenuRoot());
  FrameSlot.SetAnchorMin(m_wDeclLegend, 1, 1);
  FrameSlot.SetAnchorMax(m_wDeclLegend, 1, 1);
  FrameSlot.SetAlignment(m_wDeclLegend, 1, 1);
  FrameSlot.SetPos(m_wDeclLegend, -24, -24);
  FrameSlot.SetSizeToContent(m_wDeclLegend, true);
  m_wDeclLegend.SetZOrder(890); // under the map canvas (900), which draws the glyphs over it
  Widget fill = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, ACR_MapDeclutter.PanelColor(), 0, m_wDeclLegend);
  AlignableSlot.SetHorizontalAlign(fill, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(fill, LayoutVerticalAlign.Stretch);
  SizeLayoutWidget size = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 1, m_wDeclLegend));
  AlignableSlot.SetPadding(size, 10, 5, 4, 5);
  Widget row = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, size);
  m_aDeclLegendSpots.Clear();
  array<int> marks = {ACR_ESquadMark.MOVING, ACR_ESquadMark.FIGHTING, ACR_ESquadMark.HOLDING};
  foreach (int mark : marks)
  {
   SizeLayoutWidget spot = SizeLayoutWidget.Cast(ws.CreateWidget(WidgetType.SizeLayoutWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, row));
   spot.EnableWidthOverride(true);
   spot.SetWidthOverride(16);
   spot.EnableHeightOverride(true);
   spot.SetHeightOverride(16);
   AlignableSlot.SetVerticalAlign(spot, LayoutVerticalAlign.Center);
   m_aDeclLegendSpots.Insert(spot);
   TextWidget label = TextWidget.Cast(ws.CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", row));
   if (!label) continue;
   label.SetText(ACR_MapDeclutter.MarkName(mark));
   label.SetColor(ACR_MapDeclutter.TextColor());
   label.SetExactFontSize(14);
   AlignableSlot.SetPadding(label, 5, 0, 12, 0);
   AlignableSlot.SetVerticalAlign(label, LayoutVerticalAlign.Center);
  }
 }

 // Zoomed out the panels fold to one-line bars: INTEL always, the ACA panel unless something is
 // selected or under way, or [Open] was pressed (until zoomed in again).
 protected void ACR_DeclBars()
 {
  bool far = m_ACR_Data.m_bDeclFar;
  if (!far) m_bDeclForcedOpen = false;
  bool busy = !m_ACR_Data.m_Selected.IsEmpty() || !m_ACR_SelectedBase.IsEmpty() || m_ACR_PendingEntry >= 0 || m_ACR_PendingSupport >= 0 || m_ACR_HasSupportTarget;
  busy = busy || m_ACR_RequisitionOpen || m_ACR_SupportOpen || m_ACR_ReportOpen;
  bool acaBar = far && !busy && !m_bDeclForcedOpen;
  int now = System.GetTickCount();
  if (!far || m_iDeclIntelSeenAt == 0) m_iDeclIntelSeenAt = now;
  array<string> texts = {};
  array<int> ages = {};
  ACR_DeclFeed(texts, ages);
  int fresh = ACR_MapDeclutter.NewCount(ages, (now - m_iDeclIntelSeenAt) / 1000);
  string attacked = "";
  foreach (int b, string name : m_ACR_Data.m_BaseNames)
  {
   if (b >= m_ACR_Data.m_DeclSide.Count() || m_ACR_Data.m_DeclSide[b] != ACR_EBaseSide.OWN) continue;
   if (b < m_ACR_Data.m_BaseContested.Count() && m_ACR_Data.m_BaseContested[b] == 1)
   {
    attacked = ACR_MapDeclutter.ShortName(name);
    break;
   }
  }
  string aca = string.Format("RP %1 · +%2", m_ACR_RP, m_ACR_Income);
  m_ACR_Panel.SetCollapsed(acaBar, far, aca, ACR_MapDeclutter.Headline(attacked, texts), fresh);
  m_ACR_Panel.DeclPlace();
 }

 // The dark map: a tint inside the map's frame, over the map (town names and grid dim with it),
 // under the markers and ACA's canvases.
 protected void ACR_DeclTint(SCR_MapEntity mapEntity, bool want)
 {
  if (!m_wDeclTint && want)
  {
   Widget root = mapEntity.GetMapMenuRoot();
   Widget frame;
   if (root) frame = root.FindAnyWidget("MapFrame");
   if (frame)
   {
    m_wDeclTint = GetGame().GetWorkspace().CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, ACR_MapDeclutter.DarkTint(), -900, frame);
    if (m_wDeclTint)
    {
     FrameSlot.SetAnchorMin(m_wDeclTint, 0, 0);
     FrameSlot.SetAnchorMax(m_wDeclTint, 1, 1);
     FrameSlot.SetOffsets(m_wDeclTint, 0, 0, 0, 0);
     m_wDeclTint.SetZOrder(-900);
    }
   }
  }
  if (m_wDeclTint && m_wDeclTint.IsVisible() != want) m_wDeclTint.SetVisible(want);
 }

 override protected void ACR_ClosePanel()
 {
  super.ACR_ClosePanel();
  foreach (Widget badge : m_aDeclBadges) if (badge) badge.RemoveFromHierarchy();
  m_aDeclBadges.Clear();
  m_aDeclBadgeTexts.Clear();
  if (m_wDeclLegend) m_wDeclLegend.RemoveFromHierarchy();
  m_wDeclLegend = null;
  m_aDeclLegendSpots.Clear();
  if (m_wDeclTint) m_wDeclTint.RemoveFromHierarchy();
  m_wDeclTint = null;
  m_iDeclIntelSeenAt = 0;
  m_bDeclForcedOpen = false;
 }

 // The panel's [Compact] / [Normal]: kept in the profile, the panel built again.
 void ACR_DeclSetCompact(bool compact)
 {
  ACR_MarkUiClick();
  if (ACR_MapDeclutter.Compact() == compact) return;
  ACR_MapDeclutter.SetCompact(compact);
  // Not from inside the button's own click: the panel is rebuilt a frame later.
  GetGame().GetCallqueue().CallLater(ACR_DeclRebuildPanel, 1, false);
 }

 protected void ACR_DeclRebuildPanel()
 {
  SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
  if (!m_ACR_Panel || !mapEntity || !mapEntity.IsOpen() || !mapEntity.GetMapMenuRoot()) return;
  m_ACR_Panel.Destroy();
  m_ACR_Panel = new ACR_CommandPanel();
  m_ACR_Panel.Create(mapEntity.GetMapMenuRoot(), this);
 }

 // The bar's [Open]: the full panel while zoomed out.
 void ACR_DeclOpenPanel()
 {
  ACR_MarkUiClick();
  m_bDeclForcedOpen = true;
 }

 // The moon button: the dark map on or off; whether it is on now.
 bool ACR_DeclToggleDark()
 {
  ACR_MarkUiClick();
  ACR_MapDeclutter.SetDark(!ACR_MapDeclutter.Dark());
  return ACR_MapDeclutter.Dark();
 }

 // Tests (the shots test): the panel's state.
 bool ACR_DeclPanelCollapsed() { return m_ACR_Panel && m_ACR_Panel.DeclCollapsed(); }
 bool ACR_DeclIntelCollapsed() { return m_ACR_Panel && m_ACR_Panel.DeclIntelCollapsed(); }
 // Tests: of the markers the calm map hides now (soldiers zoomed out or of an unselected squad at a
 // base, squads under a "×N"), how many there are (hidden) and how many are still on the screen.
 int ACR_DeclHiddenOnScreenForTest(out int hidden)
 {
  hidden = 0;
  int onScreen;
  array<ACR_UnitMarker> markers = {};
  foreach (ACR_UnitMarker soldier : m_ACR_Markers)
  {
   if (!soldier || soldier.m_Original) continue;
   bool hide = m_ACR_Data.m_bDeclFar;
   int g = m_ACR_Data.m_GroupIds.Find(soldier.m_GroupId);
   if (!hide && !m_ACR_Data.IsSelected(soldier.m_GroupId) && g >= 0 && g < m_ACR_Data.m_DeclBaseOf.Count() && m_ACR_Data.m_DeclBaseOf[g] >= 0) hide = true;
   if (hide) markers.Insert(soldier);
  }
  foreach (int i, ACR_UnitMarker squad : m_ACR_GroupMarkers)
  {
   if (squad && i < m_ACR_Data.m_DeclRep.Count() && !m_ACR_Data.m_DeclRep[i]) markers.Insert(squad);
  }
  WorkspaceWidget ws = GetGame().GetWorkspace();
  foreach (ACR_UnitMarker marker : markers)
  {
   hidden++;
   Widget root = marker.GetRootWidget();
   if (!root || !root.IsVisible()) continue;
   float x, y;
   root.GetScreenPos(x, y);
   if (x > -200 && y > -200 && x < ws.GetWidth() + 200 && y < ws.GetHeight() + 200) onScreen++;
  }
  return onScreen;
 }
 bool ACR_DeclPanelStyled() { return m_ACR_Panel && m_ACR_Panel.DeclStyled(); }
 bool ACR_DeclTintShown() { return m_wDeclTint && m_wDeclTint.IsVisible(); }
 int ACR_DeclBadgesShown()
 {
  int shown;
  foreach (Widget badge : m_aDeclBadges) if (badge && badge.IsVisible()) shown++;
  return shown;
 }
}

// ---------------------------------------------------------------- the dark map button
// A moon on the map's tool bar, under the 2IC's button (sort 90) and the vanilla tools (0-20). Its
// own glyph is hidden: ACR_OrderDisplay draws the moon over it (DeclMoon). Lit while the map is dark.
modded class SCR_MapToolMenuUI
{
 protected SCR_MapToolEntry m_ACR_MoonEntry;
 static SCR_MapToolEntry s_ACR_MoonEntry;
 static const int ACR_MOON_SORT = 95;

 override void OnMapOpen(MapConfiguration config)
 {
  bool fullscreen = config && config.MapEntityMode == EMapEntityMode.FULLSCREEN;
  if (!m_ACR_MoonEntry && fullscreen && ACR_MapDeclutter.On())
  {
   m_ACR_MoonEntry = RegisterToolMenuEntry(s_sToolMenuIcons, "time", ACR_MOON_SORT);
   m_ACR_MoonEntry.m_OnClick.Insert(ACR_OnMoon);
  }
  super.OnMapOpen(config);
  if (!m_ACR_MoonEntry) return;
  m_ACR_MoonEntry.SetVisible(fullscreen && ACR_MapDeclutter.On());
  m_ACR_MoonEntry.SetActive(ACR_MapDeclutter.Dark());
  if (m_ACR_MoonEntry.m_ButtonComp && m_ACR_MoonEntry.m_ButtonComp.m_wImage) m_ACR_MoonEntry.m_ButtonComp.m_wImage.SetOpacity(0);
  s_ACR_MoonEntry = m_ACR_MoonEntry;
 }

 override void OnMapClose(MapConfiguration config)
 {
  s_ACR_MoonEntry = null;
  super.OnMapClose(config);
 }

 protected void ACR_OnMoon()
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (!pc || !m_ACR_MoonEntry) return;
  m_ACR_MoonEntry.SetActive(pc.ACR_DeclToggleDark());
 }
}
