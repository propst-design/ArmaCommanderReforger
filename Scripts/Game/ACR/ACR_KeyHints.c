// Per-player UI choices kept in the profile: where the command map's panels stand (dragged there,
// ACR_PanelMover) and whether the key reminders show. A panel the player hasn't placed has no spot,
// and stands where its own code puts it. In the file a spot is "place.<panel>=x,y"; a file from before
// every panel could move has panelx, panely, feedx and feedy, which are still read.
// A spot is in the terms of the panel's anchor: from the edge or middle of the screen it stands by,
// to the same edge or middle of the panel (anchor 0: left or top, 0.5: middle, 1: right or bottom).
class ACR_UiPrefs
{
 static const string PREFS = "$profile:ACA_CommandUi.txt";
 static const string TEST_PREFS = "$profile:ACA_CommandUi_test.txt";
 static const string PANEL = "aca";
 static const string FEED = "intel";
 static const string SQUADS = "squads";
 static const string LAYERS = "layers";
 static const string DEBUG_BOX = "debug";
 static const string CARD = "2ic";
 static const string CHANNEL = "2ic-channel";
 static const string TWOIC_PANEL = "2ic-panel";
 static const float PANEL_HOME_X = -24; // from the screen's right edge
 static const float PANEL_HOME_Y = 96;
 static const float FEED_HOME_X = 110;  // clear of the map toolbar on the left
 static const float FEED_HOME_Y = 96;
 static const float GRAB = 40;          // this much of a panel stays on the screen, to grab it by
 static const float STEP_GAP = 8;       // what a panel keeps clear of a hint, and a note of its panel
 protected static ref map<string, vector> s_Spots = new map<string, vector>();
 protected static string s_sFile = PREFS;
 static bool s_bKeyHints = true;
 static bool s_bBuildOpen;              // the ACA panel's build orders are unfolded
 protected static bool s_bRead;

 static bool Moved(string id) { return s_Spots.Contains(id); }

 // The player's spot for a panel; false when it has none.
 static bool Spot(string id, out float x, out float y)
 {
  vector spot;
  if (!s_Spots.Find(id, spot)) return false;
  x = spot[0];
  y = spot[1];
  return true;
 }

 static void SetSpot(string id, float x, float y) { s_Spots.Set(id, Vector(x, y, 0)); }
 static void Forget(string id) { s_Spots.Remove(id); }

 static void Load()
 {
  if (s_bRead) return;
  s_bRead = true;
  if (!FileIO.FileExists(s_sFile)) return;
  FileHandle file = FileIO.OpenFile(s_sFile, FileMode.READ);
  if (!file) return;
  float oldPanelX = PANEL_HOME_X;
  float oldPanelY = PANEL_HOME_Y;
  float oldFeedX = FEED_HOME_X;
  float oldFeedY = FEED_HOME_Y;
  string line;
  while (file.ReadLine(line) >= 0)
  {
   line = line.Trim();
   if (line == "keys=0") s_bKeyHints = false;
   if (line == "keys=1") s_bKeyHints = true;
   if (line == "build=1") s_bBuildOpen = true;
   if (line.StartsWith("panelx=")) oldPanelX = line.Substring(7, line.Length() - 7).ToFloat();
   if (line.StartsWith("panely=")) oldPanelY = line.Substring(7, line.Length() - 7).ToFloat();
   if (line.StartsWith("feedx=")) oldFeedX = line.Substring(6, line.Length() - 6).ToFloat();
   if (line.StartsWith("feedy=")) oldFeedY = line.Substring(6, line.Length() - 6).ToFloat();
   if (line.StartsWith("place.")) ReadSpot(line);
  }
  file.Close();
  if (!Moved(PANEL) && (oldPanelX != PANEL_HOME_X || oldPanelY != PANEL_HOME_Y)) SetSpot(PANEL, oldPanelX, oldPanelY);
  if (!Moved(FEED) && (oldFeedX != FEED_HOME_X || oldFeedY != FEED_HOME_Y)) SetSpot(FEED, oldFeedX, oldFeedY);
 }

 // "place.intel=300,500"
 protected static void ReadSpot(string line)
 {
  int equals = line.IndexOf("=");
  if (equals <= 6) return;
  string id = line.Substring(6, equals - 6);
  string value = line.Substring(equals + 1, line.Length() - equals - 1);
  array<string> parts = {};
  value.Split(",", parts, false);
  if (parts.Count() == 2) SetSpot(id, parts[0].ToFloat(), parts[1].ToFloat());
 }

 static void Save()
 {
  FileHandle file = FileIO.OpenFile(s_sFile, FileMode.WRITE);
  if (!file) return;
  if (s_bKeyHints) file.WriteLine("keys=1");
  else file.WriteLine("keys=0");
  if (s_bBuildOpen) file.WriteLine("build=1");
  foreach (string id, vector spot : s_Spots)
  {
   float x = spot[0];
   float y = spot[1];
   file.WriteLine("place." + id + "=" + x.ToString() + "," + y.ToString());
  }
  file.Close();
 }

 // All of a panel's width stays on the screen (a smaller screen than it was dragged on brings it
 // back on).
 static float ClampX(float x, float anchor, float screenW, float width)
 {
  float low = (width - screenW) * anchor;
  return Math.Clamp(x, low, low + Math.Max(screenW - width, 0));
 }

 // A panel is held by its top: that stays on the screen, with GRAB below it.
 static float ClampY(float y, float anchor, float screenH, float height)
 {
  float low = (height - screenH) * anchor;
  return Math.Clamp(y, low, low + Math.Max(screenH - GRAB, 0));
 }

 // Its top edge from the screen's top (or left edge from the left), from a spot; and back.
 static float ToEdge(float spot, float anchor, float screen, float size) { return spot + (screen - size) * anchor; }
 static float FromEdge(float edge, float anchor, float screen, float size) { return edge - (screen - size) * anchor; }

 // A spot's y with all of the panel on the screen, if it fits; its top on the screen if it doesn't.
 static float FitY(float y, float anchor, float screenH, float height)
 {
  float top = ToEdge(y, anchor, screenH, height);
  top = Math.Clamp(top, 0, Math.Max(screenH - height, 0));
  return FromEdge(top, anchor, screenH, height);
 }

 static bool Overlap(float x, float y, float w, float h, float ox, float oy, float ow, float oh)
 {
  return x < ox + ow && x + w > ox && y < oy + oh && y + h > oy;
 }

 // A panel with its top left at x, y while a hint box shows: where it stands is left as it is if clear,
 // else the nearest of under the hint, above it, to its left and to its right that has all of the panel
 // on the screen. False when there is none, and it stays.
 static bool StepAside(float x, float y, float w, float h, float hintX, float hintY, float hintW, float hintH, float screenW, float screenH, out float toX, out float toY)
 {
  toX = x;
  toY = y;
  if (!Overlap(x, y, w, h, hintX, hintY, hintW, hintH)) return true;
  array<float> xs = {x, x, hintX - STEP_GAP - w, hintX + hintW + STEP_GAP};
  array<float> ys = {hintY + hintH + STEP_GAP, hintY - STEP_GAP - h, y, y};
  float best = -1;
  foreach (int i, float spotX : xs)
  {
   float spotY = ys[i];
   if (spotX < 0 || spotY < 0 || spotX + w > screenW || spotY + h > screenH) continue;
   float cost = Math.AbsFloat(spotX - x) + Math.AbsFloat(spotY - y);
   if (best >= 0 && cost >= best) continue;
   best = cost;
   toX = spotX;
   toY = spotY;
  }
  return best >= 0;
 }

 // A note's left edge beside its panel: on the left, or the right if the screen ends there.
 static float BesideX(float panelX, float panelW, float noteW, float screenW)
 {
  float left = panelX - STEP_GAP - noteW;
  if (left >= 0) return left;
  float right = panelX + panelW + STEP_GAP;
  if (right + noteW <= screenW) return right;
  return 0;
 }

 // Tests: the file the player's real one is safe from (UseTestFileForTest), and the defaults again,
 // reading the file on the next Load.
 static void ResetForTest()
 {
  s_Spots.Clear();
  s_bKeyHints = true;
  s_bBuildOpen = false;
  s_bRead = false;
 }

 static void UseTestFileForTest()
 {
  s_sFile = TEST_PREFS;
  FileIO.DeleteFile(TEST_PREFS);
  ResetForTest();
 }
}

// The F keys as a reminder along the top of the screen, off the map (a player's request: they're easy
// to forget). The game draws each key as it is bound, rebound ones included. On by default; the
// button on the ACA panel or a key of the player's choosing (Controls, unbound by default) turns it
// off for good.
class ACR_KeyHints
{
 protected static Widget s_Box;
 protected static RichTextWidget s_Text;
 protected static string s_sShown;
 protected static const ResourceName LAYOUT_TEXT = "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout";

 protected static string Key(string action, string words)
 {
  return string.Format("<action name='%1' scale='1.1'/> %2", action, words);
 }

 // What the keys do now. Squad orders and the camera are the commander's.
 static string Line(bool commander, bool inVehicle)
 {
  string line = Key("ACR_Map", "Map");
  line += "     " + Key("ACR_Possess", "Take over") + "     " + Key("ACR_Return", "Command unit");
  if (!commander) return line;
  line += "     " + Key("ACR_Move", "Squad ahead");
  if (inVehicle) line += "     " + Key("ACR_Rally", "Drop passengers");
  else line += "     " + Key("ACR_Rally", "Squad rally");
  line += "     " + Key("ACR_RTSCamera", "3D camera");
  return line;
 }

 static void Toggle()
 {
  ACR_UiPrefs.Load();
  ACR_UiPrefs.s_bKeyHints = !ACR_UiPrefs.s_bKeyHints;
  ACR_UiPrefs.Save();
 }

 // Every frame (ACR_Manager, the local player).
 static void Update(bool commander, bool mapOpen, IEntity body)
 {
  ACR_UiPrefs.Load();
  bool wanted = ACR_UiPrefs.s_bKeyHints && !mapOpen && body && !ACR_RTSCameraManager.IsActive() && !GetGame().GetMenuManager().IsAnyMenuOpen();
  if (!wanted)
  {
   if (s_Box) s_Box.SetVisible(false);
   return;
  }
  if (!s_Box && !Build()) return;
  s_Box.SetVisible(true);
  string line = Line(commander, ACR_Transport.VehicleOf(body) != null);
  if (line == s_sShown) return;
  s_sShown = line;
  s_Text.SetText(line);
 }

 protected static bool Build()
 {
  WorkspaceWidget ws = GetGame().GetWorkspace();
  Widget root = ACR_Hud.Root();
  if (!ws || !root) return false;
  s_Box = ws.CreateWidget(WidgetType.OverlayWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(Color.WHITE), 0, root);
  if (!s_Box) return false;
  FrameSlot.SetAnchorMin(s_Box, 0.5, 0);
  FrameSlot.SetAnchorMax(s_Box, 0.5, 0);
  FrameSlot.SetAlignment(s_Box, 0.5, 0);
  FrameSlot.SetPos(s_Box, 0, 6);
  FrameSlot.SetSizeToContent(s_Box, true);
  Widget background = ws.CreateWidget(WidgetType.ImageWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.STRETCH | WidgetFlags.IGNORE_CURSOR, Color.FromRGBA(16, 22, 28, 150), 0, s_Box);
  AlignableSlot.SetHorizontalAlign(background, LayoutHorizontalAlign.Stretch);
  AlignableSlot.SetVerticalAlign(background, LayoutVerticalAlign.Stretch);
  s_Text = RichTextWidget.Cast(ws.CreateWidgets(LAYOUT_TEXT, s_Box));
  if (!s_Text)
  {
   ACR_Hud.Remove(s_Box);
   s_Box = null;
   return false;
  }
  AlignableSlot.SetPadding(s_Text, 12, 3, 12, 4);
  s_sShown = "";
  return true;
 }

 static bool ShownForTest() { return s_Box && s_Box.IsVisible(); }

 // The HUD layer went with the mission (ACR_Hud.Clear).
 static void Forget()
 {
  s_Box = null;
  s_Text = null;
  s_sShown = "";
 }
}
