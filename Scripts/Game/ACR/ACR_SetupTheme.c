// The setup lobby's look in one table (Jacob 19 Nov): every colour, heading font and text style the
// lobby (ACR_SetupLobby) draws with comes from the theme in use; no widget code picks a colour of its
// own. The theme only styles: row labels, values, buttons and help stay plain English in every skin
// (Jacob: style yes, obscure wording no).
// Skins, picked from the content socket's era (ACR_ContentSocket.Name) or -acrSetupSkin:
//  vanilla - the command panel's navy and pale blue (the first and default skin),
//  crt     - Cold War pack: a 1980s green-phosphor terminal (upper case, mono headings, > cursor, * marks),
//  tablet  - modern stack: a ruggedised C2 tablet (dark green-grey, amber accents, a status line).
// Fonts: script can't set a text widget's font, so a skin names a heading layout whose text widget
// carries the font (headings only; rows keep the game's normal UI font and size).
class ACR_SetupTheme : Managed
{
 string m_sName;
 // Page
 ref Color m_Backdrop;     // behind everything
 ref Color m_Panel;        // the map sketch's ground, the side panels' frame
 ref Color m_Heading;      // title and tab heading
 ref Color m_Decor;        // the one decoration line (terminal header, status bar)
 ref Color m_Subtitle;     // the instruction line
 ref Color m_Text;         // row labels
 ref Color m_TextDim;      // about lines, notes
 ref Color m_Help;         // the right panel's help
 ref Color m_Status;       // countdown, preset report, bug-report line
 ref Color m_Changed;      // the label of a row that differs from the server's default
 // Buttons (tabs, actions, footer)
 ref Color m_Button;
 ref Color m_ButtonHover;
 ref Color m_ButtonPress;
 ref Color m_ButtonSelected; // the open tab
 ref Color m_ButtonBorder;
 // Sides (side panels, map sketch)
 ref Color m_West;
 ref Color m_East;
 ref Color m_Neutral;
 ref Color m_Other;        // FIA and any third faction
 ref Color m_WestPanel;    // the side panels' fill
 ref Color m_EastPanel;
 // Text style
 bool m_bUpper;            // upper-case labels, values, tabs and buttons (help stays as written)
 string m_sChanged;        // put before a changed row's label
 string m_sCursor;         // put before the focused row's label ("" = none)
 string m_sDecor;          // one decoration line above the title ("" = none)
 string m_sKeys;           // the keys line in the footer ("" = none); real bindings only
 ResourceName m_sHeadingLayout; // a text layout with the heading font ("" = the lobby layout's own)

 protected static ref ACR_SetupTheme s_Current;
 static const ResourceName HEADING_MONO = "{AC230926A00100F7}UI/layouts/ACR/ACR_SetupHeadingMono.layout";

 // The skins' colours are sRGB (as a designer picks them); widgets want linear and gamma them on the
 // way out, so a colour given as is shows washed out (the terminal's near-black screen as a mid green).
 // Buttons too (20 Nov): they were raw Color values, which the engine showed far lighter (the tablet's
 // tabs light grey under white labels, 1.6-3.5:1). Every button state now gives its white label at
 // least 5:1 (WCAG AA 4.5:1), worked out on these sRGB values blended over the backdrop.
 protected static Color Srgb(int r, int g, int b, float a = 1)
 {
  return ACR_TwoICStyle.Rgb(r, g, b, a);
 }

 // The skin for this machine: -acrSetupSkin, else the content socket's era, else vanilla.
 static ACR_SetupTheme Current()
 {
  if (!s_Current)
  {
   s_Current = Named(Chosen());
   // Which skin and why, for play-test logs (20 Nov: an era pack the engine dropped for a missing
   // dependency left the socket empty, and the vanilla skin looked like a bug in the choice).
   string era = ACR_ContentSocket.Get().Name();
   if (era.IsEmpty()) era = "none plugged in";
   PrintFormat("[ACR] SETUP skin %1 (era pack: %2)", s_Current.m_sName, era);
  }
  return s_Current;
 }

 static void Reset() { s_Current = null; }

 static string Chosen()
 {
  string skin;
  if (ACR_Options.Get("acrSetupSkin", skin) && !skin.IsEmpty())
  {
   skin.ToLower();
   if (skin == "crt" || skin == "tablet" || skin == "vanilla") return skin;
  }
  string era = ACR_ContentSocket.Get().Name();
  if (era.Contains("Cold War")) return "crt";
  if (era.Contains("Modern")) return "tablet";
  return "vanilla";
 }

 static ACR_SetupTheme Named(string name)
 {
  if (name == "crt") return Crt();
  if (name == "tablet") return Tablet();
  return Vanilla();
 }

 // The command panel: navy ground, pale-blue text, the panel's blue buttons.
 static ACR_SetupTheme Vanilla()
 {
  ACR_SetupTheme theme = new ACR_SetupTheme();
  theme.m_sName = "vanilla";
  theme.m_Backdrop = Srgb(5, 9, 15, 0.902);
  theme.m_Panel = Srgb(18, 26, 23);
  theme.m_Heading = Srgb(255, 255, 255);
  theme.m_Decor = Srgb(150, 178, 215);
  theme.m_Subtitle = Srgb(158, 189, 230);
  theme.m_Text = Srgb(255, 255, 255);
  theme.m_TextDim = Srgb(170, 190, 215);
  theme.m_Help = Srgb(209, 219, 230);
  theme.m_Status = Srgb(242, 184, 89);
  theme.m_Changed = Srgb(255, 178, 80);
  theme.m_Button = Srgb(30, 60, 110, 0.80);
  theme.m_ButtonHover = Srgb(40, 85, 150, 0.92);
  theme.m_ButtonPress = Srgb(28, 72, 140, 1.0);
  theme.m_ButtonSelected = Srgb(35, 95, 175, 0.95);
  theme.m_ButtonBorder = Srgb(115, 175, 255);
  theme.m_West = Srgb(80, 140, 255);
  theme.m_East = Srgb(240, 80, 70);
  theme.m_Neutral = Srgb(170, 170, 170);
  theme.m_Other = Srgb(90, 190, 90);
  theme.m_WestPanel = Srgb(30, 60, 120, 0.784);
  theme.m_EastPanel = Srgb(120, 30, 28, 0.784);
  theme.m_sChanged = "• ";
  return theme;
 }

 // Green phosphor on black. The sides keep blue and red (dimmed to the screen) so they read at a glance.
 static ACR_SetupTheme Crt()
 {
  ACR_SetupTheme theme = new ACR_SetupTheme();
  theme.m_sName = "crt";
  theme.m_Backdrop = Srgb(2, 8, 3, 0.961);
  theme.m_Panel = Srgb(6, 22, 8);
  theme.m_Heading = Srgb(120, 255, 140);
  theme.m_Decor = Srgb(60, 170, 75);
  theme.m_Subtitle = Srgb(90, 220, 110);
  theme.m_Text = Srgb(110, 245, 130);
  theme.m_TextDim = Srgb(70, 170, 85);
  theme.m_Help = Srgb(140, 255, 160);
  theme.m_Status = Srgb(200, 255, 120);
  theme.m_Changed = Srgb(220, 255, 140);
  theme.m_Button = Srgb(10, 45, 15, 0.85);
  theme.m_ButtonHover = Srgb(20, 80, 30, 0.92);
  theme.m_ButtonPress = Srgb(15, 100, 30, 1.0);
  theme.m_ButtonSelected = Srgb(25, 110, 40, 0.95);
  theme.m_ButtonBorder = Srgb(90, 240, 105);
  theme.m_West = Srgb(110, 170, 255);
  theme.m_East = Srgb(255, 110, 90);
  theme.m_Neutral = Srgb(90, 200, 105);
  theme.m_Other = Srgb(230, 230, 120);
  theme.m_WestPanel = Srgb(15, 40, 80, 0.824);
  theme.m_EastPanel = Srgb(80, 22, 18, 0.824);
  theme.m_bUpper = true;
  theme.m_sChanged = "* ";
  theme.m_sCursor = "> ";
  theme.m_sDecor = "FT-80 FIELD TERMINAL  //  MATCH SETUP  //  READY";
  theme.m_sKeys = "SPACE: START    Q / E: CHANGE TAB    LEFT / RIGHT: CHANGE A VALUE";
  theme.m_sHeadingLayout = HEADING_MONO;
  return theme;
 }

 // A rugged tablet: dark green-grey, amber accents, a status line.
 static ACR_SetupTheme Tablet()
 {
  ACR_SetupTheme theme = new ACR_SetupTheme();
  theme.m_sName = "tablet";
  theme.m_Backdrop = Srgb(24, 30, 27, 0.941);
  theme.m_Panel = Srgb(36, 44, 40);
  theme.m_Heading = Srgb(235, 238, 230);
  theme.m_Decor = Srgb(150, 165, 150);
  theme.m_Subtitle = Srgb(200, 205, 190);
  theme.m_Text = Srgb(232, 235, 228);
  theme.m_TextDim = Srgb(165, 175, 160);
  theme.m_Help = Srgb(215, 220, 210);
  theme.m_Status = Srgb(255, 190, 60);
  theme.m_Changed = Srgb(255, 190, 60);
  theme.m_Button = Srgb(52, 62, 56, 0.90);
  theme.m_ButtonHover = Srgb(74, 88, 80, 0.95);
  theme.m_ButtonPress = Srgb(150, 100, 20, 1.0);
  theme.m_ButtonSelected = Srgb(125, 85, 15, 0.95);
  theme.m_ButtonBorder = Srgb(140, 158, 143);
  theme.m_West = Srgb(90, 150, 240);
  theme.m_East = Srgb(230, 85, 70);
  theme.m_Neutral = Srgb(165, 172, 165);
  theme.m_Other = Srgb(120, 200, 110);
  theme.m_WestPanel = Srgb(30, 52, 90, 0.863);
  theme.m_EastPanel = Srgb(95, 34, 28, 0.863);
  theme.m_sChanged = "• ";
  theme.m_sDecor = "C2 TABLET  |  MATCH SETUP  |  LINK OK";
  theme.m_sKeys = "Space: Start    Q / E: change tab    Left / right: change a value";
  return theme;
 }

 // A white button label's contrast on a button fill (WCAG ratio), the fill blended over this skin's
 // backdrop over a dark scene. The colours are linear here, so their weighted sum is the luminance.
 float LabelContrast(Color fill)
 {
  float back = Luminance(m_Backdrop) * m_Backdrop.A();
  float lum = Luminance(fill) * fill.A() + back * (1 - fill.A());
  return 1.05 / (lum + 0.05);
 }

 protected static float Luminance(Color c)
 {
  return 0.2126 * c.R() + 0.7152 * c.G() + 0.0722 * c.B();
 }

 // A label, value, tab or button in this skin's case (help text is not passed through here).
 string Text(string text)
 {
  if (!m_bUpper) return text;
  string upper = text;
  upper.ToUpper();
  return upper;
 }

 // The colour for a base's owner on the map sketch.
 Color Owner(string owner, string west, string east)
 {
  if (!owner.IsEmpty() && owner == west) return m_West;
  if (!owner.IsEmpty() && owner == east) return m_East;
  if (owner.IsEmpty()) return m_Neutral;
  return m_Other;
 }
}
