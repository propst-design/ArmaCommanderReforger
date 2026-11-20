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

 // The skin for this machine: -acrSetupSkin, else the content socket's era, else vanilla.
 static ACR_SetupTheme Current()
 {
  if (!s_Current) s_Current = Named(Chosen());
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
  theme.m_Backdrop = Color.FromRGBA(5, 9, 15, 230);
  theme.m_Panel = Color.FromRGBA(18, 26, 23, 255);
  theme.m_Heading = Color.FromRGBA(255, 255, 255, 255);
  theme.m_Decor = Color.FromRGBA(120, 150, 190, 255);
  theme.m_Subtitle = Color.FromRGBA(158, 189, 230, 255);
  theme.m_Text = Color.FromRGBA(255, 255, 255, 255);
  theme.m_TextDim = Color.FromRGBA(170, 190, 215, 255);
  theme.m_Help = Color.FromRGBA(209, 219, 230, 255);
  theme.m_Status = Color.FromRGBA(242, 184, 89, 255);
  theme.m_Changed = Color.FromRGBA(255, 178, 80, 255);
  theme.m_Button = new Color(0.16, 0.42, 0.80, 0.40);
  theme.m_ButtonHover = new Color(0.26, 0.56, 0.96, 0.70);
  theme.m_ButtonPress = new Color(0.30, 0.62, 1.0, 0.95);
  theme.m_ButtonSelected = new Color(0.26, 0.56, 0.96, 0.85);
  theme.m_ButtonBorder = new Color(0.45, 0.72, 1.0, 1.0);
  theme.m_West = Color.FromRGBA(80, 140, 255, 255);
  theme.m_East = Color.FromRGBA(240, 80, 70, 255);
  theme.m_Neutral = Color.FromRGBA(170, 170, 170, 255);
  theme.m_Other = Color.FromRGBA(90, 190, 90, 255);
  theme.m_WestPanel = Color.FromRGBA(30, 60, 120, 200);
  theme.m_EastPanel = Color.FromRGBA(120, 30, 28, 200);
  theme.m_sChanged = "• ";
  return theme;
 }

 // Green phosphor on black. The sides keep blue and red (dimmed to the screen) so they read at a glance.
 static ACR_SetupTheme Crt()
 {
  ACR_SetupTheme theme = new ACR_SetupTheme();
  theme.m_sName = "crt";
  theme.m_Backdrop = Color.FromRGBA(2, 8, 3, 245);
  theme.m_Panel = Color.FromRGBA(6, 22, 8, 255);
  theme.m_Heading = Color.FromRGBA(120, 255, 140, 255);
  theme.m_Decor = Color.FromRGBA(60, 170, 75, 255);
  theme.m_Subtitle = Color.FromRGBA(90, 220, 110, 255);
  theme.m_Text = Color.FromRGBA(110, 245, 130, 255);
  theme.m_TextDim = Color.FromRGBA(70, 170, 85, 255);
  theme.m_Help = Color.FromRGBA(140, 255, 160, 255);
  theme.m_Status = Color.FromRGBA(200, 255, 120, 255);
  theme.m_Changed = Color.FromRGBA(220, 255, 140, 255);
  theme.m_Button = new Color(0.05, 0.30, 0.08, 0.55);
  theme.m_ButtonHover = new Color(0.10, 0.50, 0.15, 0.80);
  theme.m_ButtonPress = new Color(0.20, 0.70, 0.25, 0.95);
  theme.m_ButtonSelected = new Color(0.12, 0.55, 0.18, 0.90);
  theme.m_ButtonBorder = new Color(0.35, 0.95, 0.40, 1.0);
  theme.m_West = Color.FromRGBA(110, 170, 255, 255);
  theme.m_East = Color.FromRGBA(255, 110, 90, 255);
  theme.m_Neutral = Color.FromRGBA(90, 200, 105, 255);
  theme.m_Other = Color.FromRGBA(230, 230, 120, 255);
  theme.m_WestPanel = Color.FromRGBA(15, 40, 80, 210);
  theme.m_EastPanel = Color.FromRGBA(80, 22, 18, 210);
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
  theme.m_Backdrop = Color.FromRGBA(24, 30, 27, 240);
  theme.m_Panel = Color.FromRGBA(36, 44, 40, 255);
  theme.m_Heading = Color.FromRGBA(235, 238, 230, 255);
  theme.m_Decor = Color.FromRGBA(150, 165, 150, 255);
  theme.m_Subtitle = Color.FromRGBA(200, 205, 190, 255);
  theme.m_Text = Color.FromRGBA(232, 235, 228, 255);
  theme.m_TextDim = Color.FromRGBA(165, 175, 160, 255);
  theme.m_Help = Color.FromRGBA(215, 220, 210, 255);
  theme.m_Status = Color.FromRGBA(255, 190, 60, 255);
  theme.m_Changed = Color.FromRGBA(255, 190, 60, 255);
  theme.m_Button = new Color(0.25, 0.30, 0.27, 0.85);
  theme.m_ButtonHover = new Color(0.35, 0.42, 0.37, 0.95);
  theme.m_ButtonPress = new Color(0.85, 0.62, 0.20, 0.95);
  theme.m_ButtonSelected = new Color(0.72, 0.52, 0.16, 0.95);
  theme.m_ButtonBorder = new Color(0.55, 0.62, 0.56, 1.0);
  theme.m_West = Color.FromRGBA(90, 150, 240, 255);
  theme.m_East = Color.FromRGBA(230, 85, 70, 255);
  theme.m_Neutral = Color.FromRGBA(165, 172, 165, 255);
  theme.m_Other = Color.FromRGBA(120, 200, 110, 255);
  theme.m_WestPanel = Color.FromRGBA(30, 52, 90, 220);
  theme.m_EastPanel = Color.FromRGBA(95, 34, 28, 220);
  theme.m_sChanged = "• ";
  theme.m_sDecor = "C2 TABLET  |  MATCH SETUP  |  LINK OK";
  theme.m_sKeys = "Space: Start    Q / E: change tab    Left / right: change a value";
  return theme;
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
