// Client: pings on the command map (a shape on the canvas plus a clickable
// label marker) and on the HUD in the 3D view (the same shape with
// "Popp: Attack · 340 m"). Plays one short sound per new ping that concerns
// you: a commander hears every ping of his side, everyone else those within
// HEAR_RANGE of them or placed by a commander.
class ACR_PingDisplay
{
 static const float HEAR_RANGE = 1000;
 protected static ref array<ref ACR_UnitMarker> s_Markers;
 protected static ref array<int> s_Heard;
 protected static CanvasWidget s_HudCanvas;
 protected static ref array<ref CanvasWidgetCommand> s_HudCommands;
 protected static ref array<TextWidget> s_HudLabels;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_Markers = {};
  s_Heard = {};
  s_HudCommands = {};
  s_HudLabels = {};
 }
 protected static ref ACR_OrderDisplay s_HudPainter;

 static string KindName(int kind)
 {
  if (kind == ACR_EPing.ATTACK) return "Attack";
  if (kind == ACR_EPing.DEFEND) return "Defend";
  if (kind == ACR_EPing.SUPPORT) return "Support";
  return "Look";
 }

 static Color KindColor(int kind, float alpha)
 {
  if (kind == ACR_EPing.ATTACK) return new Color(1.00, 0.25, 0.20, alpha);   // like hunt
  if (kind == ACR_EPing.DEFEND) return new Color(0.35, 0.90, 0.40, alpha);   // like defend
  if (kind == ACR_EPing.SUPPORT) return new Color(0.75, 0.45, 1.00, alpha);  // purple: used nowhere else
  return new Color(1, 1, 1, alpha);
 }

 // Seconds left for ping i (client estimate), <= 0 means gone.
 static float SecondsLeft(ACR_MapData data, int i)
 {
  float life = ACR_Pings.Lifetime(data.m_PingKinds[i]) / 1000;
  if (!data.m_PingAnswers[i].IsEmpty()) life = data.m_PingAges[i] + ACR_Pings.ANSWERED_MS / 1000;
  return life - data.m_PingAges[i] - ACR_MapData.Since(data.m_iPingsAt);
 }

 // ---------------------------------------------------------------- shapes
 // Drawn around (x, y) in screen units on any canvas.
 static void DrawShape(ACR_OrderDisplay painter, float x, float y, int kind, float scale, float alpha)
 {
  int color = KindColor(kind, alpha).PackToInt();
  Color dark = new Color(0, 0, 0, alpha * 0.6);
  int outline = dark.PackToInt();
  float s = scale;
  if (kind == ACR_EPing.LOOK)
  {
   // Pulsing ring.
   float pulse = 1 + 0.25 * Math.Sin(System.GetTickCount() / 180.0);
   painter.Ring(x, y, 11 * s * pulse + 1, outline, 4);
   painter.Ring(x, y, 11 * s * pulse, color, 2.5);
   painter.Dot(x, y, 2.5 * s, color);
   return;
  }
  if (kind == ACR_EPing.ATTACK)
  {
   // Crosshair.
   painter.Ring(x, y, 9 * s, color, 2.5);
   painter.Line(x - 15 * s, y, x - 4 * s, y, color, 2.5);
   painter.Line(x + 4 * s, y, x + 15 * s, y, color, 2.5);
   painter.Line(x, y - 15 * s, x, y - 4 * s, color, 2.5);
   painter.Line(x, y + 4 * s, x, y + 15 * s, color, 2.5);
   return;
  }
  if (kind == ACR_EPing.DEFEND)
  {
   // Shield.
   LineDrawCommand shield = new LineDrawCommand();
   shield.m_iColor = color;
   shield.m_fWidth = 2.5;
   shield.m_bShouldEnclose = true;
   shield.m_Vertices = {x - 10 * s, y - 11 * s, x + 10 * s, y - 11 * s, x + 10 * s, y + 1 * s, x, y + 12 * s, x - 10 * s, y + 1 * s};
   painter.Add(shield);
   painter.Line(x, y - 7 * s, x, y + 6 * s, color, 2);
   return;
  }
  // Support: diamond with a filled centre.
  LineDrawCommand diamond = new LineDrawCommand();
  diamond.m_iColor = color;
  diamond.m_fWidth = 2.5;
  diamond.m_bShouldEnclose = true;
  diamond.m_Vertices = {x, y - 12 * s, x + 12 * s, y, x, y + 12 * s, x - 12 * s, y};
  painter.Add(diamond);
  painter.Dot(x, y, 3.5 * s, color);
 }

 // ---------------------------------------------------------------- map
 static void DrawOnMap(ACR_OrderDisplay painter, SCR_MapEntity mapEntity, ACR_MapData data)
 {
  foreach (int i, int id : data.m_PingIds)
  {
   if (SecondsLeft(data, i) <= 0) continue;
   vector pos = data.m_PingPositions[i];
   int sx, sy;
   mapEntity.WorldToScreen(pos[0], pos[2], sx, sy, true);
   float alpha = 1;
   if (!data.m_PingAnswers[i].IsEmpty()) alpha = 0.5;
   DrawShape(painter, sx, sy, data.m_PingKinds[i], 1, alpha);
  }
 }

 // Label markers (clickable: Alt+click your own to clear it; a commander
 // clicks a Support ping to aim his next support call at it).
 static void UpdateMarkers(SCR_MapMarkerManagerComponent manager, ACR_MapData data)
 {
  array<int> live = {};
  foreach (int i, int id : data.m_PingIds) if (SecondsLeft(data, i) > 0) live.Insert(i);
  if (s_Markers.Count() != live.Count())
  {
   foreach (ACR_UnitMarker old : s_Markers) manager.ACR_RemoveMarker(old);
   s_Markers.Clear();
   for (int m = 0; m < live.Count(); m++)
   {
    ACR_UnitMarker marker = new ACR_UnitMarker();
    manager.ACR_AddSimpleMarker(marker, s_Markers);
   }
  }
  foreach (int k, ACR_UnitMarker marker : s_Markers)
  {
   int p = live[k];
   marker.m_iPingId = data.m_PingIds[p];
   string label = data.m_PingTags[p] + ": " + KindName(data.m_PingKinds[p]);
   if (!data.m_PingAnswers[p].IsEmpty()) label = data.m_PingAnswers[p];
   marker.m_Label = label;
   marker.m_State = 6;
   vector pos = data.m_PingPositions[p];
   // The label sits just below the shape.
   marker.SetWorldPos(pos[0], pos[2]);
   marker.RefreshStyle();
   if (marker.GetRootWidget()) marker.ACR_SetColor(KindColor(data.m_PingKinds[p], 1));
  }
 }

 static void ClearMarkers(SCR_MapMarkerManagerComponent manager)
 {
  if (manager) foreach (ACR_UnitMarker old : s_Markers) manager.ACR_RemoveMarker(old);
  s_Markers.Clear();
 }

 // ---------------------------------------------------------------- sound
 // Called when a new ping list arrives.
 static void OnPings(ACR_MapData data, IEntity body)
 {
  bool commander = data.AmCommander();
  foreach (int i, int id : data.m_PingIds)
  {
   if (s_Heard.Contains(id)) continue;
   s_Heard.Insert(id);
   if (data.m_PingAges[i] > 3) continue; // old news (just joined, or a resend)
   if (data.m_PingOwners[i] == data.m_iMyId) continue;
   bool near = body && vector.DistanceXZ(body.GetOrigin(), data.m_PingPositions[i]) < HEAR_RANGE;
   if (!commander && !near && !data.IsCommanderId(data.m_PingOwners[i])) continue;
   string sound = SCR_SoundEvent.SOUND_MAP_CLICK_POINT_ON;
   if (data.m_PingKinds[i] == ACR_EPing.ATTACK) sound = SCR_SoundEvent.SOUND_MAP_HOVER_ENEMY;
   if (data.m_PingKinds[i] == ACR_EPing.DEFEND) sound = SCR_SoundEvent.SOUND_MAP_HOVER_BASE;
   if (data.m_PingKinds[i] == ACR_EPing.SUPPORT) sound = SCR_SoundEvent.SOUND_RADIO_SIGNAL_GAIN;
   SCR_UISoundEntity.SoundEvent(sound);
  }
  while (s_Heard.Count() > 200) s_Heard.RemoveOrdered(0);
 }

 // ---------------------------------------------------------------- HUD
 // Every frame while the map is closed.
 static void UpdateHud(ACR_MapData data, IEntity body, bool show)
 {
  if (!s_HudCanvas)
  {
   if (!show || data.m_PingIds.IsEmpty()) return;
   CreateHud();
   if (!s_HudCanvas) return;
  }
  s_HudCommands.Clear();
  int used = 0;
  if (show && body)
  {
   WorkspaceWidget workspace = GetGame().GetWorkspace();
   BaseWorld world = GetGame().GetWorld();
   foreach (int i, int id : data.m_PingIds)
   {
    if (SecondsLeft(data, i) <= 0) continue;
    vector pos = data.m_PingPositions[i] + Vector(0, 1.5, 0);
    vector screen = workspace.ProjWorldToScreen(pos, world);
    if (screen[2] <= 0) continue; // behind us
    float distance = vector.Distance(body.GetOrigin(), data.m_PingPositions[i]);
    float alpha = 0.95;
    if (!data.m_PingAnswers[i].IsEmpty()) alpha = 0.5;
    // The canvas draws in native pixels; ProjWorldToScreen is in reference units
    // (labels). Mixing them put the icon off the ping on screens other than 1080p.
    vector nativePos = workspace.ProjWorldToScreenNative(pos, world);
    DrawShape(s_HudPainter, nativePos[0], nativePos[1], data.m_PingKinds[i], 0.9, alpha);
    TextWidget label = HudLabel(used);
    used++;
    if (!label) continue;
    int metres = Math.Round(distance / 10) * 10;
    label.SetText(string.Format("%1: %2 · %3 m", data.m_PingTags[i], KindName(data.m_PingKinds[i]), metres));
    label.SetColor(KindColor(data.m_PingKinds[i], alpha));
    FrameSlot.SetPos(label, screen[0] - 100, screen[1] + 16);
    label.SetVisible(true);
   }
  }
  for (int h = used; h < s_HudLabels.Count(); h++) s_HudLabels[h].SetVisible(false);
  s_HudCanvas.SetDrawCommands(s_HudCommands);
 }

 protected static void CreateHud()
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  if (!ws) return;
  s_HudCanvas = CanvasWidget.Cast(ACR_Hud.Track(ws.CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.BLEND, Color.FromInt(Color.WHITE), 0, ACR_Hud.Root())));
  if (!s_HudCanvas) return;
  FrameSlot.SetAnchorMin(s_HudCanvas, 0, 0);
  FrameSlot.SetAnchorMax(s_HudCanvas, 1, 1);
  FrameSlot.SetOffsets(s_HudCanvas, 0, 0, 0, 0);
  s_HudCanvas.SetZOrder(800);
  s_HudPainter = new ACR_OrderDisplay();
  s_HudPainter.SetCanvas(s_HudCanvas);
  s_HudPainter.UseCommands(s_HudCommands);
 }

 protected static TextWidget HudLabel(int index)
 {
  while (s_HudLabels.Count() <= index)
  {
   TextWidget text = TextWidget.Cast(ACR_Hud.Track(GetGame().GetWorkspace().CreateWidgets("{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout", ACR_Hud.Root())));
   if (!text) return null;
   text.SetFlags(WidgetFlags.IGNORE_CURSOR);
   FrameSlot.SetSize(text, 200, 22);
   text.SetZOrder(801);
   s_HudLabels.Insert(text);
  }
  return s_HudLabels[index];
 }

 // While the Ping key is held: which kind will be placed (-1 hides it).
 protected static TextWidget s_HoldHint;
 static void ShowHoldHint(int kind)
 {
  if (kind < 0)
  {
   if (s_HoldHint) s_HoldHint.SetVisible(false);
   return;
  }
  if (!s_HoldHint)
  {
   WorkspaceWidget ws = GetGame().GetWorkspace();
   s_HoldHint = TextWidget.Cast(ACR_Hud.Track(ws.CreateWidgets("{0D7FF083FDE39877}UI/layouts/WidgetLibrary/TextWidgets/Text_Heading4.layout", ACR_Hud.Root())));
   if (!s_HoldHint) return;
   s_HoldHint.SetFlags(WidgetFlags.IGNORE_CURSOR);
   FrameSlot.SetAnchorMin(s_HoldHint, 0.5, 0.5);
   FrameSlot.SetAnchorMax(s_HoldHint, 0.5, 0.5);
   FrameSlot.SetAlignment(s_HoldHint, 0.5, 0);
   FrameSlot.SetPos(s_HoldHint, 0, 60);
   FrameSlot.SetSize(s_HoldHint, 520, 34);
   s_HoldHint.SetZOrder(802);
  }
  string text = "";
  for (int k = 0; k <= ACR_EPing.SUPPORT; k++)
  {
   string name = KindName(k);
   if (k == kind) name = "[" + name + "]";
   if (!text.IsEmpty()) text += "   ";
   text += name;
  }
  s_HoldHint.SetText("Ping: " + text + "   (let go to place)");
  s_HoldHint.SetColor(KindColor(kind, 1));
  s_HoldHint.SetVisible(true);
 }

 // The mode stopped (ACR_Hud removed our widgets): start fresh next mission.
 static void Forget()
 {
  s_HudCanvas = null;
  s_HudCommands.Clear();
  s_HudLabels.Clear();
  s_HudPainter = null;
  s_HoldHint = null;
  s_Markers.Clear();
  s_Heard.Clear();
 }

 // Test hook: how many pings the HUD drew last frame.
 static int HudShown()
 {
  int n;
  foreach (TextWidget label : s_HudLabels) if (label.IsVisible()) n++;
  return n;
 }
}
