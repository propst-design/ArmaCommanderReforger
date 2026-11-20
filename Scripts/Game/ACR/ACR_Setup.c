// Setup phase (Workshop host options, step 2, 13 Oct). With acrSetup on (our shipped
// scenarios and Play/Host launchers turn it on; tests start from a world
// without a header), nothing starts until the host or an admin presses Start: no ticks,
// no skirmish layout, no starting units, no player spawns. Meanwhile they see a dialog
// with the match options; each click cycles one option on the server (ACR_Options.Set).
// A dedicated server with nobody allowed to start begins by itself AUTO_START_S after a
// player joined; the countdown holds while a host or admin is on and clears when the server empties.
class ACR_Setup
{
 static const int AUTO_START_S = 300;
 protected static bool s_bDecided;
 protected static bool s_bWanted;
 protected static bool s_bStarted;
 protected static float s_fFirstPlayerAt = -1;
 protected static ref map<int, string> s_Sent;
 // Layout preview (aca-f07 editor, step 1): the bases the chosen battlefield will have, sent to
 // the host's map as base markers while the phase lasts.
 protected static string s_sPreviewSig;
 static ref array<vector> s_PreviewPos;
 static ref array<string> s_PreviewNames;
 protected static ref array<string> s_PreviewOwners;
 protected static ref array<string> s_PreviewDetails;
 protected static ref array<float> s_PreviewRadii;
 protected static ref array<float> s_PreviewWeights; // quick start split (ACR_QuickStart.Weight)
 protected static ref array<bool> s_PreviewHidden;
 protected static ref map<int, string> s_PreviewSent;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_Sent = new map<int, string>();
  s_PreviewPos = {};
  s_PreviewNames = {};
  s_PreviewOwners = {};
  s_PreviewDetails = {};
  s_PreviewRadii = {};
  s_PreviewWeights = {};
  s_PreviewHidden = {};
  s_PreviewSent = new map<int, string>();
 }

 static bool Wanted()
 {
  if (!s_bDecided)
  {
   s_bDecided = true;
   string value;
   s_bWanted = ACR_Options.Get("acrSetup", value) && value == "1";
   if (System.IsCLIParam("acrSetupTest") || System.IsCLIParam("acrSetupUiTest") || System.IsCLIParam("acrSetupGenTest") || System.IsCLIParam("acrSetupMapTest") || System.IsCLIParam("acrSetupPresetsTest") || System.IsCLIParam("acrSetupLobbyTest")) s_bWanted = true;
   else if (ACR_TestHooks.Get().SkipSetup()) s_bWanted = false;
   if (s_bWanted) Print("[ACR] SETUP phase: waiting for the host or an admin to press Start");
  }
  return s_bWanted;
 }

 // Gameplay runs (server). Always true without a setup phase.
 static bool Started()
 {
  if (!Replication.IsServer()) return true;
  return s_bStarted || !Wanted();
 }

 static void Reset()
 {
  s_bDecided = false;
  s_bWanted = false;
  s_bStarted = false;
  s_fFirstPlayerAt = -1;
  s_Sent.Clear();
  s_sPreviewSig = "";
  s_PreviewSent.Clear();
  s_PreviewPos.Clear();
  s_PreviewNames.Clear();
 }

 // The layout the current choices give: a generated plan (its seed fixed now, so Start builds
 // exactly this), else the world's own bases. Recomputed only when the choices change.
 static void UpdatePreview()
 {
  string layout = Current("layout");
  bool generated = layout.StartsWith("gen:");
  string quick = Current("quickstart");
  int seed = 0;
  if (generated || quick != "off") seed = ACR_Generator.Seed();
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  string signature = string.Format("%1|%2|%3|%4|%5|%6", layout, seed, west, east, ACR_Generator.s_iMovesVersion, quick);
  if (signature == s_sPreviewSig) return;
  s_sPreviewSig = signature;
  s_PreviewPos.Clear();
  s_PreviewNames.Clear();
  s_PreviewOwners.Clear();
  s_PreviewDetails.Clear();
  s_PreviewRadii.Clear();
  s_PreviewWeights.Clear();
  s_PreviewHidden.Clear();
  if (generated)
  {
   array<ref ACR_GenSite> plan = {};
   array<ACR_GenSite> hqs = {};
   if (!ACR_Generator.Plan(seed, ACR_Skirmish.SizeOption(), plan, hqs)) return;
   foreach (ACR_GenSite site : plan)
   {
    string owner = "";
    string detail = site.Describe();
    if (hqs.Count() == 2 && site == hqs[0]) { owner = west; detail = "HQ (" + ACR_FactionSelection.DisplayName(west) + "), 2 RP"; }
    if (hqs.Count() == 2 && site == hqs[1]) { owner = east; detail = "HQ (" + ACR_FactionSelection.DisplayName(east) + "), 2 RP"; }
    AddPreview(site.m_vPos, site.m_sName, owner, detail, site.m_fRadius, ACR_QuickStart.Weight(site.m_bIncome, site.m_iValue), site.m_sKind == "landmark");
   }
  }
  else
  {
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    string baseOwner = base.m_sOwner;
    if (baseOwner == "US") baseOwner = west;
    else if (baseOwner == "USSR") baseOwner = east;
    string baseDetail = string.Format("Control point, %1 RP", base.m_iValue);
    if (!base.m_bIncome) baseDetail = "Control point, no income";
    if (base.m_bHidden) baseDetail += " | Hidden";
    AddPreview(base.GetOrigin(), base.m_sName, baseOwner, baseDetail, base.m_fRadius, ACR_QuickStart.Weight(base.m_bIncome, base.m_iValue), base.m_bHidden);
   }
  }
  if (quick != "off") PreviewQuickStart(quick, seed, west, east);
  PrintFormat("[ACR] SETUP preview %1: %2 bases", signature, s_PreviewNames.Count());
 }

 // Quick start (#88): the preview shows the split ACR_QuickStart.Apply will make (same seed and
 // bases). Hidden landmarks and held bases stay as they are.
 protected static void PreviewQuickStart(string mode, int seed, string west, string east)
 {
  array<int> sides = {};
  foreach (int i, string owner : s_PreviewOwners)
  {
   int side = -1;
   if (owner == west) side = 0;
   else if (owner == east) side = 1;
   else if (owner != "" || s_PreviewHidden[i]) side = -2;
   sides.Insert(side);
  }
  string front = ACR_QuickStart.Split(s_PreviewPos, s_PreviewWeights, sides, seed, mode, ACR_Skirmish.SizeOption() == ACR_ESkirmishSize.FLASHPOINT);
  foreach (int j, int given : sides)
  {
   if (s_PreviewOwners[j] != "") continue;
   if (given == 0) s_PreviewOwners[j] = west;
   if (given == 1) s_PreviewOwners[j] = east;
  }
  PrintFormat("[ACR] SETUP quick start preview %1 seed %2: %3", mode, seed, front);
 }

 protected static void AddPreview(vector pos, string name, string owner, string detail, float radius, float weight = 0.5, bool hidden = false)
 {
  s_PreviewWeights.Insert(weight);
  s_PreviewHidden.Insert(hidden);
  s_PreviewPos.Insert(pos);
  s_PreviewNames.Insert(name);
  s_PreviewOwners.Insert(owner);
  s_PreviewDetails.Insert(detail);
  s_PreviewRadii.Insert(radius);
 }

 protected static void SendPreview(int playerId, SCR_PlayerController pc)
 {
  string sent;
  if (s_PreviewSent.Find(playerId, sent) && sent == s_sPreviewSig) return;
  s_PreviewSent.Set(playerId, s_sPreviewSig);
  array<int> contested = {};
  array<int> supplies = {};
  foreach (string n : s_PreviewNames) contested.Insert(0);
  pc.ACR_SendBases(s_PreviewPos, s_PreviewNames, s_PreviewOwners, s_PreviewDetails, s_PreviewRadii, contested, supplies);
 }

 // Tests: force the phase on or off.
 static void ForceForTest(bool wanted)
 {
  s_bDecided = true;
  s_bWanted = wanted;
  s_bStarted = false;
 }

 static void Start(string who)
 {
  if (s_bStarted) return;
  s_bStarted = true;
  ACR_FactionSelection.Apply(); // the chosen pair becomes playable; the world's US/USSR owners map to it
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) manager.m_Bases.ForgetFactions(); // buy lists and RP are built with the chosen options
  PrintFormat("[ACR] SETUP started by %1", who);
  ACR_Options.Log();
  Broadcast();
 }

 // Every second on the server while the phase lasts (from ACR_Manager.Tick).
 static void Tick()
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  float now = GetGame().GetWorld().GetWorldTime();
  // Players who left: their dialog and preview go again if they come back (a reconnect).
  Prune(s_Sent, players);
  Prune(s_PreviewSent, players);
  // The countdown runs only while players are on and none of them may press Start: an admin who is on
  // holds it (it starts afresh when he leaves), and an empty server clears it (review 0.5.13 #10).
  if (players.IsEmpty() || StarterPresent(players)) s_fFirstPlayerAt = -1;
  else if (s_fFirstPlayerAt < 0) s_fFirstPlayerAt = now;
  if (!GetGame().GetPlayerController() && s_fFirstPlayerAt >= 0 && now - s_fFirstPlayerAt >= AUTO_START_S * 1000)
  {
   Start("the timer (nobody pressed Start)");
   return;
  }
  Broadcast();
 }

 protected static bool StarterPresent(notnull array<int> players)
 {
  foreach (int id : players) if (ACR_Manager.IsHostOrAdmin(id)) return true;
  return false;
 }

 protected static void Prune(notnull map<int, string> sent, notnull array<int> players)
 {
  array<int> gone = {};
  foreach (int id, string unused : sent) if (!players.Contains(id)) gone.Insert(id);
  foreach (int id : gone) sent.Remove(id);
 }

 static int SecondsLeft()
 {
  if (GetGame().GetPlayerController() || s_fFirstPlayerAt < 0) return -1;
  return Math.Max(0, AUTO_START_S - (GetGame().GetWorld().GetWorldTime() - s_fFirstPlayerAt) / 1000);
 }

 // Each player's view of the phase, sent when it changes.
 protected static void Broadcast()
 {
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  // A preset loaded at the start of the match is checked against this world once it is up.
  if (!s_bStarted) ACR_SetupPresets.CheckPending();
  if (!s_bStarted) UpdatePreview();
  string rows = Rows();
  foreach (int id : players)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc) continue;
   bool active = !s_bStarted;
   bool allowed = ACR_Manager.IsHostOrAdmin(id);
   if (active && allowed) SendPreview(id, pc);
   // Everyone gets the rows: the setup lobby shows them read-only to players who can't change
   // them (the classic dialog still shows those players a hint only).
   string text = rows;
   int left = SecondsLeft();
   string info = Info(left);
   string signature = string.Format("%1|%2|%3|%4", active, allowed, text, info);
   string sent;
   if (s_Sent.Find(id, sent) && sent == signature) continue;
   s_Sent.Set(id, signature);
   pc.ACR_SendSetup(active, allowed, text, info);
  }
 }

 protected static string Info(int left)
 {
  string info = "";
  // What the preset in use said (a value that fell back, a launch option that kept its own).
  string preset = ACR_SetupPresets.Report();
  if (!preset.IsEmpty()) info = preset + " ";
  if (Current("layout").StartsWith("gen:")) info += "Generated layout: support helicopters come from off the map; build a helipad to buy helicopters. ";
  if (left >= 0) info += string.Format("Starts by itself in %1 s.", left);
  return info;
 }

 // ---------------------------------------------------------------- options
 // The rows the dialog shows: "key\tLabel: value" lines.
 static string Rows()
 {
  string rows;
  bool generated = Current("layout").StartsWith("gen:");
  array<string> keys = RowKeys();
  foreach (string key : keys)
  {
   if (key == "seed" && !generated && Current("quickstart") == "off") continue;
   // Only with Project Outbreak loaded (ACR_Outbreak).
   if ((key == "zombies" || key == "zombiemode" || key == "outbreak") && !ACR_Outbreak.Loaded()) continue;
   // Opens the map on the host's own screen with the layout drawn on it (client side).
   if (key == "map")
   {
    if (s_PreviewNames.IsEmpty()) continue;
    if (generated) rows += Line("map", string.Format("Show and edit the layout on the map (%1 bases)", s_PreviewNames.Count()));
    else rows += Line("map", string.Format("Show the layout on the map (%1 bases)", s_PreviewNames.Count()));
    continue;
   }
   // Deck builder (#106): the editor opens on this player's own screen (client side).
   if (key == "deckeditor")
   {
    if (Current("decks") != "0") rows += Line("deckeditor", "Build decks and hand one in");
    continue;
   }
   if ((key == "deckpoints" || key == "deckwest" || key == "deckeast") && Current("decks") == "0") continue;
   if (key == "undo")
   {
    int moves = ACR_Generator.MoveCount();
    if (generated && moves > 0) rows += Line("undo", string.Format("Undo layout edits (%1)", moves));
    continue;
   }
   rows += Line(key, Label(key) + ": " + ValueName(key, Current(key)));
   // The company and doctrine rows show a name; what it means goes on a line under the tab's rows
   // (pn10b, 26 Oct: in the rows, the descriptions ran past the buttons).
   if (key == "companywest" || key == "companyeast" || key == "doctrinewest" || key == "doctrineeast") rows += Line("about" + key, About(key));
  }
  // The setup lobby's map sketch colours bases by side: the two sides' faction keys (the classic dialog skips it).
  rows += "lobbysides\t" + ACR_FactionSelection.West() + "," + ACR_FactionSelection.East() + "\n";
  // The Presets tab (#143): the server's presets, managed from the host's dialog.
  rows += ACR_SetupPresets.Rows();
  return rows;
 }

 // ---------------------------------------------------------------- setup lobby (0.7.0)
 // One row line: "key\tLabel: value\ttab\tparent\tflags\n". The first two fields are what the
 // classic dialog reads; the lobby (ACR_SetupLobby) also reads its tab (LobbyTab), the row it is
 // indented under (Parent) and flags: "c" = differs from the server's default (a dot), "h" = its
 // parent is off, so the lobby hides it. The presets' own lines (ACR_SetupPresets.Rows) keep two fields.
 static string Line(string key, string text)
 {
  string flags = "";
  if (Changed(key)) flags += "c";
  if (!ParentOn(key)) flags += "h";
  return string.Format("%1\t%2\t%3\t%4\t%5\n", key, text, LobbyTab(key), Parent(key), flags);
 }

 // The lobby's tabs, in the host's order: what the match is first, then who fights, the world they
 // fight in, money and supply, building, what is still being tried, and saved setups.
 static const int LOBBY_TABS = 7;
 static const int LOBBY_PRESETS = 6;
 static string LobbyTabName(int tab)
 {
  if (tab == 1) return "Forces & AI";
  if (tab == 2) return "World";
  if (tab == 3) return "Economy & Logistics";
  if (tab == 4) return "Engineering";
  if (tab == 5) return "Experimental";
  if (tab == LOBBY_PRESETS) return "Presets";
  return "Match";
 }

 static int LobbyTab(string key)
 {
  if (key.StartsWith("about")) return LobbyTab(key.Substring(5, key.Length() - 5));
  if (key.StartsWith("preset")) return LOBBY_PRESETS;
  if (key == "west" || key == "east" || key == "layout" || key == "seed" || key == "map" || key == "undo" || key == "quickstart" || key == "threat" || key == "ai" || key == "startrp") return 0;
  if (key == "aicommander" || key == "doctrinewest" || key == "doctrineeast" || key == "doctrineadapt" || key == "unitcap" || key == "capscale" || key == "aibuild" || key == "holdguns" || key == "shared" || key == "declutter" || key == "intro") return 1;
  if (key == "pockets" || key == "guerrillas" || key == "fiaallegiance" || key == "fialeaning" || key == "fiahidden" || key == "zombies" || key == "zombiemode" || key == "outbreak") return 2;
  if (key == "pricing" || key == "income" || key == "companywest" || key == "companyeast" || key == "decks" || key == "deckeditor" || key == "deckpoints" || key == "deckwest" || key == "deckeast") return 3;
  if (key == "deployment" || key == "reinforcement" || key == "fobservices" || key == "servicecost" || key == "rpservice" || key == "fobrelay" || key == "depotcapacity") return 3;
  if (key == "mines" || key == "minesreal" || key == "minesai" || key == "fortify" || key == "fortifyai" || key == "baseworks") return 4;
  if (Experimental(key)) return 5;
  return 0;
 }

 // The row a row belongs under ("" = a top-level row).
 static string Parent(string key)
 {
  if (key.StartsWith("about")) return key.Substring(5, key.Length() - 5);
  if (key == "seed") return "layout";
  if (key == "undo") return "map";
  if (key == "fiaallegiance" || key == "fialeaning" || key == "fiahidden") return "guerrillas";
  if (key == "zombiemode" || key == "outbreak") return "zombies";
  if (key == "deckeditor" || key == "deckpoints" || key == "deckwest" || key == "deckeast") return "decks";
  if (key == "minesreal" || key == "minesai") return "mines";
  if (key == "fortifyai" || key == "baseworks") return "fortify";
  if (key == "servicecost") return "fobservices";
  return "";
 }

 // False when the row's parent switches what it sets off (the lobby then hides the row; its value
 // stays as it is and still counts, as a preset or a launch option may have set it).
 static bool ParentOn(string key)
 {
  string parent = Parent(key);
  // The FIA rows matter with either kind of FIA on the map: the resistance pockets or the guerrillas.
  if (parent == "guerrillas") return Current("guerrillas") != "off" || Current("pockets") != "off";
  if (parent == "zombies") return Current("zombies") != "off";
  if (parent == "decks") return Current("decks") != "0";
  if (parent == "mines") return Current("mines") != "0";
  if (parent == "fortify") return Current("fortify") != "0";
  if (parent == "fobservices") return Current("fobservices") == "supplies";
  return true;
 }

 // The row's value with nothing from the setup screen or a preset: what the scenario and the launch
 // options give.
 static string DefaultValue(string key)
 {
  ACR_Options.s_bDefaults = true;
  string value = Current(key);
  ACR_Options.s_bDefaults = false;
  return value;
 }

 // The row differs from the server's default (the lobby's dot). Action and text rows never do; the
 // seed is random until someone sets one, so it has no default to differ from.
 static bool Changed(string key)
 {
  if (key == "seed" || key == "map" || key == "undo" || key == "deckeditor" || key.StartsWith("about")) return false;
  string now = Current(key);
  string fallback = DefaultValue(key);
  now.ToLower();
  fallback.ToLower();
  return now != fallback;
 }

 // Reset tab: every row of a lobby tab back to the server's default (its screen choices dropped; a
 // preset in use still applies, as it does after a restart).
 static string ResetTab(int tab)
 {
  int count = 0;
  array<string> keys = RowKeys();
  foreach (string key : keys)
  {
   if (LobbyTab(key) != tab || key == "map" || key == "undo" || key == "deckeditor") continue;
   map<string, string> options = new map<string, string>();
   OptionsFor(key, Current(key), options);
   foreach (string option, string unused : options)
   {
    if (ACR_Options.Source(option) != "screen") continue;
    ACR_Options.Unset(option);
    count++;
   }
  }
  Broadcast();
  return string.Format("%1 reset (%2 settings back to the default)", LobbyTabName(tab), count);
 }

 // Every row of the setup screen in order; setup presets (ACR_SetupPresets) save the options
 // behind these rows. The map preview sits right under the battlefield and its seed (25 Oct:
 // it was last, a page away).
 static array<string> RowKeys()
 {
  array<string> keys = {"west", "east", "layout", "seed", "map", "undo", "pockets", "quickstart", "guerrillas", "fiaallegiance", "fialeaning", "fiahidden", "zombies", "zombiemode", "outbreak", "threat", "pricing", "ai", "doctrinewest", "doctrineeast", "doctrineadapt", "aicommander", "twoic", "unitcap", "capscale", "startrp", "income", "companywest", "companyeast", "decks", "deckeditor", "deckpoints", "deckwest", "deckeast", "shared", "holdguns", "declutter", "intro", "periodjets", "deployment", "reinforcement", "fobservices", "fobrelay", "depotcapacity", "servicecost", "rpservice", "pullover", "mines", "minesreal", "minesai", "fortify", "fortifyai", "aibuild", "baseworks", "fieldsoffire", "squadtactics", "evadearmour", "supplyruns", "hubs", "saveroutes"};
  return keys;
 }

 // The line under the tab's rows for a company or doctrine row: "Blue: Combat Engineers: two tier I\n // positions at its front", "Red: Fortress: holds in depth, counterattacks lost bases".
 static string About(string key)
 {
  string side = "Blue: ";
  string faction = ACR_FactionSelection.West();
  if (key == "companyeast" || key == "doctrineeast")
  {
   side = "Red: ";
   faction = ACR_FactionSelection.East();
  }
  if (key == "companywest" || key == "companyeast") return side + ACR_DoctrineKits.CompanySummary(ACR_DoctrineKits.CompanyOf(faction));
  string value = Current(key);
  if (value == ACR_AIDoctrine.RANDOM) return side + "a random doctrine each match";
  ACR_AIDoctrine doctrine = ACR_AIDoctrine.Find(value);
  if (!doctrine) doctrine = ACR_AIDoctrine.Find(ACR_AIDoctrine.FollowedKey(faction));
  return side + doctrine.m_sName + ": " + doctrine.m_sBrief;
 }

 // A row's text ("Label: value") fits its button at 1080p in this many characters (pn10b, 26 Oct:
 // "Blue AI doctrine: Fortress: defends in depth behind manned roadblocks, ..." was cut off at
 // about 70); srv:setup checks every row at every value.
 static const int ROW_CHARS = 60;

 static string Label(string key)
 {
  if (key == "west") return "Blue side";
  if (key == "east") return "Red side";
  if (key == "layout") return "Battlefield";
  if (key == "seed") return "Layout seed";
  if (key == "pockets") return "FIA resistance";
  if (key == "quickstart") return "Quick start";
  if (key == "guerrillas") return "FIA guerrillas";
  if (key == "fiahidden") return "Hidden FIA";
  if (key == "fiaallegiance") return "FIA sides with";
  if (key == "fialeaning") return "FIA leans by conduct";
  if (key == "zombies") return "Zombies";
  if (key == "zombiemode") return "Zombie mode";
  if (key == "outbreak") return "Outbreak start";
  if (key == "threat") return "Threat";
  if (key == "pricing") return "Prices";
  if (key == "ai") return "AI commanders";
  if (key == "doctrinewest") return "Blue AI doctrine";
  if (key == "doctrineeast") return "Red AI doctrine";
  if (key == "doctrineadapt") return "AI doctrines adapt";
  if (key == "decks") return "Decks";
  if (key == "deckpoints") return "Deck points";
  if (key == "deckwest") return "Blue deck";
  if (key == "deckeast") return "Red deck";
  if (key == "companywest") return "Blue company";
  if (key == "companyeast") return "Red company";
  if (key == "unitcap") return "AI squad cap";
  if (key == "aibuild") return "AI build budget";
  if (key == "capscale") return "Squad caps scale with map";
  if (key == "startrp") return "Starting RP";
  if (key == "income") return "Income";
  if (key == "shared") return "Shared command";
  if (key == "holdguns") return "Gunners stay on held guns";
  if (key == "declutter") return "Calm command map";
  if (key == "intro") return "Mission intro";
  if (key == "deployment") return "New-unit deployment";
  if (key == "reinforcement") return "Infantry replacements";
  if (key == "fobservices") return "FOB service payment";
  if (key == "fobrelay") return "FOB radio relay required";
  if (key == "depotcapacity") return "Depot storage bonus";
  if (key == "servicecost") return "FOB supplies per service load";
  if (key == "rpservice") return "FOB instant infantry service (RP)";
  if (key == "mines") return "Minefield orders";
  if (key == "minesreal") return "Own mines";
  if (key == "minesai") return "AI commanders lay mines";
  if (key == "fortify") return "Fortify orders";
  if (key == "fortifyai") return "AI commanders fortify";
  if (key == "baseworks") return "Fortify a whole base";
  if (key == "aicommander") return "AI commander";
  // Out of Experimental, on by default (Jacob, 19 Nov: played and trusted).
  if (key == "periodjets") return "Period jets (no RHS)";
  if (key == "pullover") return "Pull over before getting out";
  if (key == "fieldsoffire") return "Guns clear bushes round them";
  if (key == "twoic") return "Second-in-command (2IC)";
  // Experimental tab (Jacob, 5 Nov): features shipped off until they have been played.
  if (key == "squadtactics") return "AI squad tactics in objectives";
  if (key == "evadearmour") return "Unarmed vehicles evade armour";
  if (key == "supplyruns") return "Supply runs (trucks supply FOBs)";
  if (key == "hubs") return "Logistics hubs";
  if (key == "saveroutes") return "Save supply routes";
  return key;
 }

 // The Experimental tab's rows: off by default, on/off unless Values says more.
 static bool Experimental(string key)
 {
  // Period jets, pull over, fields of fire and the 2IC left it on 19 Nov (Jacob: played and trusted),
  // on by default on their own tabs (ACR_SetupDialog.Tab).
  return key == "squadtactics" || key == "evadearmour" || key == "supplyruns" || key == "hubs" || key == "saveroutes";
 }

 // What a row does, shown under the rows when it is pointed at or focused (ACR_SetupDialog.OnRowFocus,
 // on the Experimental tab). Two short lines at most: the text under the rows doesn't wrap.
 static string Help(string key)
 {
  // The setup lobby's right panel (Jacob 19 Nov: the help of the focused row): the main rows too.
  if (key == "west" || key == "east") return "The two sides that fight. Any playable faction of this world;\nthe world's US and USSR bases go to the chosen pair at Start.";
  if (key == "layout") return "Which bases are in play: the world's own (whole map or a part)\nor a layout generated for this match, whose bases you can move.";
  if (key == "seed") return "The number a generated layout and a quick start split come from.\nType one to replay a layout; the same seed gives the same map.";
  if (key == "map") return "Opens the planned layout on the map. With a generated layout,\nclick a base, then where it should go; HQs can only be moved.";
  if (key == "undo") return "Puts every moved or deleted base of this seed back.";
  if (key == "quickstart") return "Off: both sides start from their HQ. Even: the map is split along\na front. Uneven: one side holds two thirds, the other has double RP.";
  if (key == "threat") return "Prices, stock, the soldier limit and the AI squad cap, and how\nmany attacks an AI commander runs at once.";
  if (key == "ai") return "Which sides an AI commander runs.";
  if (key == "startrp") return "Requisition points each side starts with.";
  if (key == "aicommander") return "New: lanes, the allocator and the defence forecast.\nClassic: the commander from before 0.6.0.";
  if (key == "doctrinewest" || key == "doctrineeast") return "How this side's AI commander fights: blitz, firepower, fortress,\nraider or balanced. The line under the rows says what it does.";
  if (key == "doctrineadapt") return "A commander that keeps losing switches to another doctrine.";
  if (key == "unitcap") return "Most AI squads each side's AI commander keeps at once.";
  if (key == "capscale") return "Bigger maps get bigger armies: the AI squad cap and the soldier\ncap grow with the map, between x1 and x2.";
  if (key == "aibuild") return "Share of each AI commander's income set aside for roadblocks,\nminefields and Fortify positions.";
  if (key == "holdguns") return "Gunners stay on the static guns of a spot their squad holds\ninstead of wandering off.";
  if (key == "shared") return "Several players can share a side's command (the scenario's\nshared-command setting).";
  if (key == "declutter") return "The calm command map: dark panels, merged lines, squads at a base\ngathered into one icon. Off: the classic map.";
  if (key == "intro") return "A short orbit of your side's HQ with the title, the first time\neach player takes control in a match.";
  if (key == "pockets") return "On whole and half maps, neutral bases away from both starts\nbegin held by an FIA garrison.";
  if (key == "guerrillas") return "Small FIA cells outside the bases on whole and half maps: road\nambushes that follow the traffic. FIA rows appear under this one.";
  if (key == "fiaallegiance") return "Which main side the FIA fights for, or neither.";
  if (key == "fialeaning") return "FIA treats each side by its conduct; the standing shows on the\ncommand panel.";
  if (key == "fiahidden") return "No FIA messages: pockets are found by walking into them.";
  if (key == "zombies") return "Project Outbreak zombie nests on open bases and in the country.\nThe mode and the start appear under this row.";
  if (key == "zombiemode") return "Both sides and the zombies, or one side against the zombies\nwhile the other sits out.";
  if (key == "outbreak") return "Nests across the map, or one ground zero that spreads while\nyou rescue survivors.";
  if (key == "pricing") return "Stepped: each purchase makes the next of that unit dearer.\nFixed stock: set prices, but only so many of each unit.";
  if (key == "income") return "Multiplies every side's RP income.";
  if (key == "companywest" || key == "companyeast") return "The kind of company this side fields. The line under the rows\nsays what it brings.";
  if (key == "decks") return "Each side buys from a deck of cards instead of the whole list.\nThe deck rows appear under this one.";
  if (key == "deckeditor") return "Opens the deck builder on your screen. Hand a deck in to use it.";
  if (key == "deckpoints") return "How many points a deck may cost. By threat: 100 low, 150 normal,\n250 high.";
  if (key == "deckwest" || key == "deckeast") return "The deck this side buys from.";
  if (key == "deployment") return "Where new units may be deployed: military bases, HQs and FOBs,\nonly HQs and FOBs, or every deployment point.";
  if (key == "reinforcement") return "Where infantry replacements walk in from.";
  if (key == "fobservices") return "Supplies: a FOB's service buildings draw on its supply store.\nFree: unlimited free service anywhere.";
  if (key == "servicecost") return "Supplies one service load takes from the FOB's store.";
  if (key == "rpservice") return "RP per soldier serviced instantly at a FOB when services cost\nsupplies. 0 makes it free.";
  if (key == "fobrelay") return "A built FOB needs a finished radio relay before new units deploy\nthere or infantry is reinforced there.";
  if (key == "depotcapacity") return "Extra supplies a FOB's supply depot can store.";
  if (key == "mines") return "Squads can be ordered to lay minefields. Its two rows appear\nunder this one.";
  if (key == "minesreal") return "Own mines are dangerous to everyone, our AI included.";
  if (key == "minesai") return "AI commanders lay minefields too.";
  if (key == "fortify") return "Squads can be ordered to dig in and fortify a position.";
  if (key == "fortifyai") return "AI commanders fortify their positions too.";
  if (key == "baseworks") return "Fortify on the middle of a base we hold works the whole town,\nsite by site.";
  if (key == "periodjets") return "Without RHS: recon flights and bomb runs show period jets\n(RF-4C, Su-17, A-10, Su-25) with engine sound and falling bombs.";
  if (key == "squadtactics") return "AI squads use Better Battle Buddies' squad tactics (flank, bound,\nsearch) inside a base they attack or defend, never leaving it.";
  if (key == "pullover") return "A squad told to get out on a road first has its driver pull\nonto the verge. Under fire it gets out where it stands.";
  if (key == "evadearmour") return "An unarmed vehicle that sees an armed enemy vehicle drives 300 m\naway, then carries on. Needs Better Battle Buddies' setting on too.";
  if (key == "fieldsoffire") return "Built guns, checkpoint posts and Fortify pieces clear the bushes\nround them and in front of a gun, the same on every screen.";
  if (key == "twoic") return "2IC: the AI runs the squads nobody leads while you buy and build.\nI'm the 2IC: the AI commander runs the side; you keep your squads.";
  if (key == "supplyruns") return "FOB supplies come by construction truck from a hub (20 RP to load);\nthe old delivery stays as a fallback at 40 RP / 180 s. Spawns stay on RP.";
  if (key == "hubs") return "Trucks load and restock only at the HQ, ports and FOBs with\na supply depot, not at every base. Needs supply runs to matter most.";
  if (key == "saveroutes") return "Routes supply trucks drove are kept in the host's profile for\nthis map and used in later matches. Nothing leaves this machine.";
  return "";
 }

 // The values a row cycles through ("" = the scenario's default).
 static array<string> Values(string key)
 {
  array<string> values = {};
  if (key == "layout")
  {
   // Generated layouts work on any map (aca-f07); a map without bases of its own has only those.
   if (ACR_Generator.HasOwnBases()) values.Insert("full");
   values.Insert("gen:full");
   values.Insert("gen:half");
   values.Insert("gen:quarter");
   values.Insert("gen:flashpoint");
   bool half, quarter, flash;
   foreach (ACR_Skirmish layout : ACR_Skirmish.s_All)
   {
    if (layout.m_eSize == ACR_ESkirmishSize.HALF) half = true;
    else if (layout.m_eSize == ACR_ESkirmishSize.QUARTER) quarter = true;
    else flash = true;
   }
   if (half) values.Insert("size:half");
   if (quarter) values.Insert("size:quarter");
   if (flash) values.Insert("size:flashpoint");
   foreach (ACR_Skirmish named : ACR_Skirmish.s_All) values.Insert("name:" + named.m_sName);
  }
  else if (key == "west" || key == "east")
  {
   // Every playable faction except the other side's.
   string other = ACR_FactionSelection.East();
   if (key == "east") other = ACR_FactionSelection.West();
   array<string> keys = {};
   ACR_FactionSelection.Candidates(keys);
   foreach (string candidate : keys) if (candidate != other) values.Insert(candidate);
  }
  else if (key == "pockets") values = {"few", "many", "off"};
  else if (key == "quickstart") values = {"off", "even", "uneven"};
  else if (key == "guerrillas") values = {"few", "many", "off"};
  else if (key == "fiahidden") values = {"0", "1"};
  else if (key == "fiaallegiance") values = {"hostile", "west", "east"};
  else if (key == "fialeaning") values = {"0", "1"};
  else if (key == "zombies") values = {"off", "few", "many"};
  else if (key == "zombiemode") values = {"tvtvz", "tvz"};
  else if (key == "outbreak") values = {"nests", "contain"};
  else if (key == "threat") values = {"low", "normal", "high"};
  else if (key == "pricing") values = {"stepped", "stock"};
  else if (key == "ai")
  {
   values = {"both", "none", "west", "east"};
  }
  else if (key == "doctrinewest" || key == "doctrineeast") values = {"", "random", "balanced", "blitz", "firepower", "fortress", "raider"};
  else if (key == "doctrineadapt") values = {"1", "0"};
  else if (key == "companywest" || key == "companyeast") values = {"", "mechanized", "weapons", "engineers", "recon"};
  else if (key == "decks") values = {"1", "0"};
  else if (key == "deckpoints") values = {"", "50", "75", "100", "150", "200", "300", "0"};
  else if (key == "deckwest") values = ACR_Decks.RowValues(ACR_FactionSelection.West());
  else if (key == "deckeast") values = ACR_Decks.RowValues(ACR_FactionSelection.East());
  else if (key == "unitcap") values = {"", "4", "6", "8", "12", "16", "24"};
  else if (key == "aibuild") values = {"0", "10", "15", "25"};
  else if (key == "capscale") values = {"0", "1"};
  else if (key == "startrp") values = {"", "50", "100", "200", "400", "800"};
  else if (key == "income") values = {"", "0.5", "1", "1.5", "2", "3"};
  else if (key == "shared") values = {"1", "0"};
  else if (key == "holdguns") values = {"1", "0"};
  else if (key == "declutter") values = {"1", "0"};
  else if (key == "intro") values = {"1", "0"};
  else if (key == "deployment") values = {"military", "all", "hq"};
  else if (key == "reinforcement") values = {"settlements", "all", "deployment"};
  else if (key == "fobservices") values = {"supplies", "free"};
  else if (key == "fobrelay") values = {"1", "0"};
  else if (key == "depotcapacity") values = {"0", "1000", "2000", "4000"};
  else if (key == "servicecost") values = {"5", "10", "20"};
  else if (key == "rpservice") values = {"0", "3", "5"};
  else if (key == "mines" || key == "fortify" || key == "baseworks") values = {"1", "0"};
  else if (key == "minesreal" || key == "minesai" || key == "fortifyai") values = {"0", "1"};
  else if (key == "aicommander") values = {"new", "classic"};
  else if (key == "twoic") values = {"1", "2", "0"};
  else if (key == "periodjets" || key == "pullover" || key == "fieldsoffire") values = {"1", "0"};
  else if (Experimental(key)) values = {"0", "1"};
  return values;
 }

 static string Option(string key)
 {
  if (key == "pockets") return "acrPockets";
  if (key == "quickstart") return "acrQuickStart";
  if (key == "guerrillas") return "acrGuerrillas";
  if (key == "fiahidden") return "acrFIAHidden";
  if (key == "fiaallegiance") return "acrFIAAllegiance";
  if (key == "fialeaning") return "acrFIALeaning";
  if (key == "zombies") return "acrZombies";
  if (key == "zombiemode") return "acrZombieMode";
  if (key == "outbreak") return "acrOutbreak";
  if (key == "threat") return "acrThreat";
  if (key == "decks") return "acrDecks";
  if (key == "deckpoints") return "acrDeckPoints";
  if (key == "deckwest") return "acrDeckWest";
  if (key == "deckeast") return "acrDeckEast";
  if (key == "pricing") return "acrPricing";
  if (key == "ai") return "acrAI";
  if (key == "doctrinewest") return "acrDoctrineWest";
  if (key == "doctrineeast") return "acrDoctrineEast";
  if (key == "doctrineadapt") return "acrDoctrineAdapt";
  if (key == "companywest") return "acrCompanyWest";
  if (key == "companyeast") return "acrCompanyEast";
  if (key == "unitcap") return "acrUnitCap";
  if (key == "aibuild") return "acrAIBuildShare";
  if (key == "capscale") return "acrCapScale";
  if (key == "startrp") return "acrStartRP";
  if (key == "income") return "acrIncome";
  if (key == "shared") return "acrSharedCommand";
  if (key == "holdguns") return "acrHoldGuns";
  if (key == "declutter") return ACR_MapDeclutter.OPTION;
  if (key == "intro") return "acrIntro";
  if (key == "deployment") return "acrDeployment";
  if (key == "reinforcement") return "acrReinforcement";
  if (key == "fobservices") return "acrFobServices";
  if (key == "fobrelay") return "acrFobRelay";
  if (key == "depotcapacity") return "acrDepotCapacity";
  if (key == "servicecost") return "acrServiceSupplyCost";
  if (key == "rpservice") return "acrFobRPServiceCost";
  if (key == "mines") return "acrMines";
  if (key == "minesreal") return "acrMinesRealistic";
  if (key == "minesai") return "acrMinesAI";
  if (key == "fortify") return "acrFortify";
  if (key == "fortifyai") return "acrFortifyAI";
  if (key == "baseworks") return "acrBaseWorks";
  if (key == "aicommander") return "acrAICommander";
  if (key == "periodjets") return "acrPeriodJets";
  if (key == "squadtactics") return "acrSquadTactics";
  if (key == "pullover") return "acrPullOver";
  if (key == "evadearmour") return "acrEvadeArmour";
  if (key == "fieldsoffire") return "acrFieldsOfFire";
  if (key == "twoic") return "acr2IC";
  if (key == "supplyruns") return "acrSupplyRuns";
  if (key == "hubs") return "acrLogisticsHubs";
  if (key == "saveroutes") return "acrSaveRoutes";
  return "";
 }

 static string Current(string key)
 {
  string value;
  if (key == "west") return ACR_FactionSelection.West();
  if (key == "east") return ACR_FactionSelection.East();
  if (key == "layout")
  {
   string size;
   if (!ACR_Options.Get("acrSize", size) || size == "") size = "full";
   size.ToLower();
   string layout;
   ACR_Options.Get("acrLayout", layout);
   string layoutLower = layout;
   layoutLower.ToLower();
   if (layoutLower == "generated" || !ACR_Generator.HasOwnBases()) return "gen:" + size;
   if (layout != "") return "name:" + layout;
   if (size != "full") return "size:" + size;
   return "full";
  }
  if (key == "seed")
  {
   if (ACR_Options.Get("acrSeed", value) && value != "" && value != "0") return value;
   return "";
  }
  // The 2IC is a flag: a bare -acr2IC is on (ACR_SquadAI.TwoIC), which the empty value would show as off;
  // unset it is on too (the plain 2IC by default, 19 Nov). 2 is "I'm the 2IC" (ACR_SquadAI.ImThe2IC).
  if (key == "twoic")
  {
   if (ACR_SquadAI.ImThe2IC()) return "2";
   if (ACR_SquadAI.TwoIC()) return "1";
   return "0";
  }
  // On by default (out of Experimental, 19 Nov); period jets only without RHS (ACR_Aircraft).
  if (key == "periodjets")
  {
   if (ACR_Aircraft.PeriodJetsOn()) return "1";
   return "0";
  }
  if (!ACR_Options.Get(Option(key), value)) value = "";
  value.ToLower();
  if (value == "")
  {
   if (key == "pockets") return "few";
   if (key == "quickstart") return "off";
   if (key == "guerrillas") return "few";
   if (key == "fiahidden") return "0";
   if (key == "fiaallegiance") return "hostile";
   if (key == "fialeaning") return "0";
   if (key == "zombies") return "off";
   if (key == "zombiemode") return "tvtvz";
   if (key == "outbreak") return "nests";
   if (key == "deployment") return "military";
   if (key == "reinforcement") return "settlements";
   if (key == "fobservices") return "supplies";
   if (key == "fobrelay") return "1";
   if (key == "depotcapacity") return "2000";
   if (key == "servicecost") return "10";
   if (key == "rpservice") return "3";
   if (key == "mines" || key == "doctrineadapt" || key == "fortify" || key == "baseworks") return "1";
   if (key == "minesreal" || key == "minesai" || key == "fortifyai") return "0";
   if (key == "aibuild") return "0";
   if (key == "capscale") return "0";
   if (key == "aicommander") return "new";
   if (key == "decks") return "1";
   if (key == "pullover" || key == "fieldsoffire") return "1";
   if (Experimental(key)) return "0";
  }
  if (key == "threat" && value == "") return "normal";
  if (key == "pricing" && value == "") return "";
  if (key == "ai" && value == "") return "";
  if (key == "shared" && value == "") return "1";
  if (key == "holdguns" && value == "") return "1";
  if (key == "declutter" && value == "") return "1";
  if (key == "intro" && value == "") return "1";
  return value;
 }

 static string ValueName(string key, string value)
 {
  if (value == "")
  {
   if (key == "deployment") return "military bases / HQs / FOBs (scenario default)";
   if (key == "reinforcement") return "settlements + deployment bases (scenario default)";
   if (key == "fobservices") return "supplies (scenario default)";
   if (key == "fobrelay") return "on (scenario default)";
   if (key == "depotcapacity") return "+2000 supplies (scenario default)";
   if (key == "servicecost") return "10 supplies (scenario default)";
   if (key == "rpservice") return "3 RP per serviced soldier (scenario default)";
   if (key == "seed") return "random (click to roll one)";
   if (key == "pricing" || key == "ai") return "scenario's choice";
   if (key == "doctrinewest") return ACR_AIDoctrine.FollowsRow(ACR_FactionSelection.West());
   if (key == "doctrineeast") return ACR_AIDoctrine.FollowsRow(ACR_FactionSelection.East());
   if (key == "companywest" || key == "companyeast") return ACR_DoctrineKits.CompanyName("rifle");
   if (key == "deckpoints") return "by threat (" + ACR_DeckRules.BudgetName(ACR_DeckRules.Budget()) + ")";
   if (key == "deckwest") return ACR_Decks.RowName(ACR_FactionSelection.West(), "");
   if (key == "deckeast") return ACR_Decks.RowName(ACR_FactionSelection.East(), "");
   return "scenario default";
  }
  if (key == "layout")
  {
   if (value == "full") return "whole map";
   if (value == "size:half") return "a random half";
   if (value == "size:quarter") return "a random quarter";
   if (value == "size:flashpoint") return "a random flashpoint";
   if (value == "gen:full") return "generated, whole map";
   if (value == "gen:half") return "generated, half the map";
   if (value == "gen:quarter") return "generated, a quarter of the map";
   if (value == "gen:flashpoint") return "generated flashpoint";
   return value.Substring(5, value.Length() - 5);
  }
  if (key == "pricing" && value == "stock") return "fixed stock";
  if (key == "pricing") return "stepped";
  if (key == "west" || key == "east") return ACR_FactionSelection.DisplayName(value);
  if (key == "ai")
  {
   if (value == "both" || value == "none") return value;
   string side = value;
   if (value == "west") side = ACR_FactionSelection.West();
   else if (value == "east") side = ACR_FactionSelection.East();
   return ACR_FactionSelection.DisplayName(side) + " only";
  }
  if (key == "doctrinewest" || key == "doctrineeast")
  {
   ACR_AIDoctrine doctrine = ACR_AIDoctrine.Find(value);
   if (!doctrine) return "random each match";
   return doctrine.m_sName;
  }
  if (key == "companywest" || key == "companyeast") return ACR_DoctrineKits.CompanyName(value);
  if (key == "decks" && value == "0") return "off (the whole list, as before)";
  if (key == "decks") return "on (each side buys from its deck)";
  if (key == "deckpoints" && value == "0") return "unlimited";
  if (key == "deckpoints") return value;
  if (key == "deckwest") return ACR_Decks.RowName(ACR_FactionSelection.West(), value);
  if (key == "deckeast") return ACR_Decks.RowName(ACR_FactionSelection.East(), value);
  if (key == "income") return "x" + value;
  if (key == "pockets" && value == "few") return "a few pockets (whole/half map)";
  if (key == "pockets" && value == "many") return "many pockets (whole/half map)";
  if (key == "pockets" && value == "off") return "off";
  if (key == "quickstart" && value == "even") return "the map split along a front, roadblocks up";
  if (key == "quickstart" && value == "uneven") return "one side holds 2/3, the other has double RP";
  if (key == "quickstart") return "off (both sides start from their HQ)";
  if (key == "guerrillas" && value == "few") return "a few cells (whole/half map)";
  if (key == "guerrillas" && value == "many") return "many cells (whole/half map)";
  if (key == "guerrillas" && value == "off") return "off";
  if (key == "fiahidden" && value == "1") return "on: no FIA messages, pockets found by entering";
  if (key == "fiahidden" && value == "0") return "off";
  if (key == "fiaallegiance" && value == "hostile") return "nobody: hostile to both";
  if (key == "fiaallegiance" && value == "west") return "the Blue side";
  if (key == "fiaallegiance" && value == "east") return "the Red side";
  if (key == "fialeaning" && value == "1") return "on: FIA treats sides by their conduct";
  if (key == "fialeaning" && value == "0") return "off: as FIA sides with";
  if (key == "zombies" && value == "few") return "a few nests (open bases, country)";
  if (key == "zombies" && value == "many") return "many nests (open bases, country)";
  if (key == "zombies" && value == "off") return "off";
  if (key == "zombiemode" && value == "tvtvz") return "both sides + zombies";
  if (key == "zombiemode" && value == "tvz") return "one side vs zombies (the other side sits out)";
  if (key == "outbreak" && value == "nests") return "nests across the map";
  if (key == "outbreak" && value == "contain") return "one ground zero that spreads; rescue survivors";
  if (key == "deployment" && value == "military") return "military bases / HQs / FOBs";
  if (key == "deployment" && value == "hq") return "HQs / FOBs only";
  if (key == "deployment" && value == "all") return "all deployment-enabled points";
  if (key == "reinforcement" && value == "settlements") return "settlements + deployment bases";
  if (key == "reinforcement" && value == "deployment") return "deployment bases only";
  if (key == "reinforcement" && value == "all") return "all reinforcement-enabled points";
  if (key == "aibuild" && value == "0") return "off";
  if (key == "aibuild") return value + "% of income";
  if (key == "holdguns" && value == "0") return "off (vanilla)";
  if (key == "holdguns") return "on";
  if (key == "capscale" && value == "1") return "on";
  if (key == "capscale") return "off";
  if (key == "declutter" && value == "0") return "off (the classic map)";
  if (key == "declutter") return "on";
  if (key == "aicommander" && value == "classic") return "classic (before 0.6.0)";
  if (key == "aicommander") return "new (lanes, allocator, forecast)";
  if (key == "evadearmour" && value == "1") return "on (needs BBB's setting too)";
  if (key == "twoic" && value == "1") return "2IC: runs the squads nobody leads";
  if (key == "twoic" && value == "2") return "I'm the 2IC: the AI commands";
  if (Experimental(key) || key == "twoic" || key == "periodjets" || key == "pullover" || key == "fieldsoffire")
  {
   if (value == "0") return "off";
   return "on";
  }
  if (key == "minesreal" && value == "1") return "dangerous to all";
  if (key == "minesreal") return "safe for our AI";
  if (key == "shared" || key == "fobrelay" || key == "mines" || key == "minesai" || key == "fortify" || key == "fortifyai" || key == "baseworks")
  {
   if (value == "0") return "off";
   return "on";
  }
  if (key == "doctrineadapt")
  {
   if (value == "0" || value == "off" || value == "false") return "off: each keeps its first doctrine";
   return "on: a losing commander changes doctrine";
  }
  return value;
 }

 // Next value of a row (server, host/admin checked by the caller); step -1 = the one before (the
 // setup lobby's left arrow).
 static string Cycle(string key, int step = 1)
 {
  if (s_bStarted) return "The match has started";
  // The seed row rolls a new layout seed on every click.
  if (key == "seed")
  {
   int seed = Math.RandomInt(1, 100000);
   if (seed < 0) seed = -seed;
   if (seed == 0) seed = 1;
   ACR_Options.Set("acrSeed", seed.ToString());
   Broadcast();
   return Label(key) + ": " + seed.ToString();
  }
  array<string> values = Values(key);
  if (values.IsEmpty()) return "Unknown option";
  string current = Current(key);
  int index = -1;
  foreach (int i, string candidate : values)
  {
   string lower = candidate;
   lower.ToLower();
   string currentLower = current;
   currentLower.ToLower();
   if (lower == currentLower) index = i;
  }
  int count = values.Count();
  int target = (index + 1) % count;
  if (step < 0)
  {
   target = index - 1;
   if (target < 0) target = count - 1;
  }
  string next = values[target];
  SetValue(key, next);
  Broadcast();
  return Label(key) + ": " + ValueName(key, next);
 }

 static void SetValue(string key, string value)
 {
  map<string, string> options = new map<string, string>();
  OptionsFor(key, value, options);
  foreach (string option, string optionValue : options)
  {
   // The battlefield's two options always take their value ("" = no named layout).
   if (option == "acrLayout" || option == "acrSize" || optionValue != "") ACR_Options.Set(option, optionValue);
   // "" = back to the scenario's default: drop the screen's value (a flag, preset or header may still set it).
   else ACR_Options.Unset(option);
  }
 }

 // The host options behind a row's value (SetValue, and setup presets saving the current setup).
 static void OptionsFor(string key, string value, notnull map<string, string> into)
 {
  if (key == "west") { into.Set("acrFactions", value + "," + ACR_FactionSelection.East()); return; }
  if (key == "east") { into.Set("acrFactions", ACR_FactionSelection.West() + "," + value); return; }
  if (key == "seed") { into.Set("acrSeed", value); return; }
  if (key == "layout")
  {
   if (value == "full") { into.Set("acrLayout", ""); into.Set("acrSize", "full"); }
   else if (value.StartsWith("gen:")) { into.Set("acrLayout", "generated"); into.Set("acrSize", value.Substring(4, value.Length() - 4)); }
   else if (value.StartsWith("size:")) { into.Set("acrLayout", ""); into.Set("acrSize", value.Substring(5, value.Length() - 5)); }
   else if (value.StartsWith("name:")) { into.Set("acrLayout", value.Substring(5, value.Length() - 5)); into.Set("acrSize", ""); }
   return;
  }
  string option = Option(key);
  if (option != "") into.Set(option, value);
 }

 // A client asked (server side).
 static string Handle(int playerId, string key)
 {
  if (!Wanted() || s_bStarted) return "The match has already started";
  if (!ACR_Manager.IsHostOrAdmin(playerId)) return "Only the host or an admin can change the match setup";
  return Act(playerId, key);
 }

 // A request the host or an admin made (Handle checked who asked; tests call it directly).
 static string Act(int playerId, string key)
 {
  if (s_bStarted) return "The match has already started";
  if (key == "start")
  {
   Start(string.Format("player %1", playerId));
   return "Match started";
  }
  if (key == "undo")
  {
   ACR_Generator.ClearMoves();
   Broadcast();
   return "Layout edits undone";
  }
  // Setup presets (#143): save, load, rename, delete, auto-load, copy and paste.
  if (key.StartsWith("preset:"))
  {
   string presetResult = ACR_SetupPresets.Handle(playerId, key.Substring(7, key.Length() - 7));
   Broadcast();
   return presetResult;
  }
  // The setup lobby: a row's left arrow, the seed typed in, Reset tab.
  if (key.StartsWith("prev:")) return Cycle(key.Substring(5, key.Length() - 5), -1);
  if (key.StartsWith("seedset:"))
  {
   int typed = key.Substring(8, key.Length() - 8).ToInt();
   if (typed <= 0) return "A seed is a whole number above 0";
   ACR_Options.Set("acrSeed", typed.ToString());
   Broadcast();
   return Label("seed") + ": " + typed.ToString();
  }
  if (key.StartsWith("resettab:"))
  {
   int tab = key.Substring(9, key.Length() - 9).ToInt();
   if (tab < 0 || tab >= LOBBY_PRESETS) return "Nothing to reset on that tab";
   return ResetTab(tab);
  }
  if (key.StartsWith("delete:"))
  {
   if (!Current("layout").StartsWith("gen:")) return "Only generated battlefields can be edited";
   string reason;
   ACR_Generator.Delete(key.Substring(7, key.Length() - 7), reason);
   Broadcast();
   return reason;
  }
  return Cycle(key);
 }

 // The host moved a base on the setup map (server).
 static string HandleMove(int playerId, string name, float x, float z)
 {
  if (!Wanted() || s_bStarted) return "The match has already started";
  if (!ACR_Manager.IsHostOrAdmin(playerId)) return "Only the host or an admin can change the match setup";
  if (!Current("layout").StartsWith("gen:")) return "Only generated battlefields can be edited: pick a Generated battlefield first";
  string reason;
  ACR_Generator.Move(name, x, z, reason);
  Broadcast();
  return reason;
 }
}

// ---------------------------------------------------------------- client
// The dialog for the host/admin; a hint for everyone else.
class ACR_SetupClient
{
 static bool s_bActive;
 static bool IsActive() { return s_bActive; }
 static bool s_bAllowed;
 static string s_sRows;
 static string s_sInfo;
 protected static ref ACR_SetupDialog s_Dialog;
 // The setup lobby (ACR_SetupLobby, 0.7.0); the classic dialog above with -acrSetupClassic 1, or when
 // the lobby's layout can't be drawn on this machine.
 protected static ref ACR_SetupLobby s_Lobby;
 protected static bool s_bLobbyFailed;
 // Before anyone spawns there is no camera, so nothing is drawn (the loading screen stays on
 // screen, dialog included: Jacob 13 Oct heard the buttons but saw only the loading screen).
 // A plain camera over the battlefield until the player has a body.
 protected static CameraBase s_Camera;
 protected static const ResourceName CAMERA = "{E1FF38EC8894C5F3}Prefabs/Editor/Camera/ManualCameraSpectate.et";
 protected static float s_fDiagAt;
 protected static float s_fHintAt = -100000;
 protected static bool s_bChanged;

 static void Receive(bool active, bool allowed, string rows, string info)
 {
  s_bActive = active;
  s_bAllowed = allowed;
  s_sRows = rows;
  s_sInfo = info;
  s_bChanged = true;
 }

 // Every frame on clients (ACR_Manager.OnFrame).
 static void Frame()
 {
  PlayerController local = GetGame().GetPlayerController();
  bool bodyless = local && !local.GetControlledEntity();
  if (s_bActive && bodyless)
  {
   ShowCamera();
   // The respawn system covers the screen with its loading placeholder until the first spawn
   // (Jacob 13 Oct, third look: the dialog worked, clicks registered, but this image hid it).
   SCR_RespawnSystemComponent respawn = SCR_RespawnSystemComponent.GetInstance();
   if (respawn) respawn.DestroyLoadingPlaceholder();
  }
  else DropCamera();
  if (s_bActive) Diagnose();
  if (!s_bActive)
  {
   if (s_Dialog) { s_Dialog.Close(); s_Dialog = null; }
   if (s_Lobby) { s_Lobby.Close(); s_Lobby = null; }
   return;
  }
  if (!Classic())
  {
   ShowLobby();
   return;
  }
  if (!s_bAllowed)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   if (now - s_fHintAt > 15000)
   {
    s_fHintAt = now;
    SCR_HintManagerComponent.ShowCustomHint("Waiting for the host or an admin to set up the match and press Start. F3: build your deck and hand it in. " + s_sInfo, "AC: Actual", 14, true);
   }
   return;
  }
  // The layout preview: the map is open over the setup view; the dialog comes back when it closes.
  SCR_MapEntity previewMap = SCR_MapEntity.GetMapInstance();
  if (previewMap && previewMap.IsOpen())
  {
   // Keep it alive across the input/world update boundary (the host has no character tick).
   GetGame().GetInputManager().ActivateContext("ACR_SetupMapContext", 100);
   return;
  }
  if (!s_Dialog || !s_Dialog.GetRootWidget())
  {
   s_Dialog = new ACR_SetupDialog();
   SCR_ConfigurableDialogUi.CreateFromPreset(ACR_SetupDialog.PRESETS, "ACR_Setup", s_Dialog);
   s_bChanged = true;
  }
  if (s_bChanged && s_Dialog)
  {
   s_bChanged = false;
   s_Dialog.Fill(s_sRows, s_sInfo);
  }
 }

 // The classic dialog: asked for with -acrSetupClassic 1 on the machine that shows the screen (the
 // fallback for one release), kept for the classic setup UI and map tests, or the lobby failed here.
 static bool Classic()
 {
  if (s_bLobbyFailed) return true;
  if (System.IsCLIParam("acrSetupUiTest") || System.IsCLIParam("acrSetupMapTest")) return true;
  return ACR_Options.Flag("acrSetupClassic");
 }

 // The lobby for everyone: the host and admins change the rows, other players see them read-only.
 protected static void ShowLobby()
 {
  // The layout preview or the deck builder: the map is open over the setup view; the lobby comes back when it closes.
  SCR_MapEntity previewMap = SCR_MapEntity.GetMapInstance();
  if (previewMap && previewMap.IsOpen())
  {
   GetGame().GetInputManager().ActivateContext("ACR_SetupMapContext", 100);
   return;
  }
  if (!s_Lobby || !s_Lobby.GetRootWidget())
  {
   s_Lobby = new ACR_SetupLobby();
   SCR_ConfigurableDialogUi.CreateFromPreset(ACR_SetupDialog.PRESETS, ACR_SetupLobby.TAG, s_Lobby);
   if (!s_Lobby.GetRootWidget() || !s_Lobby.Ready())
   {
    // Never leave the host without a way to start: the classic dialog takes over.
    Print("[ACR] SETUP lobby layout could not be drawn: using the classic setup dialog", LogLevel.WARNING);
    s_Lobby.Close();
    s_Lobby = null;
    s_bLobbyFailed = true;
    return;
   }
   s_bChanged = true;
  }
  if (s_bChanged)
  {
   s_bChanged = false;
   s_Lobby.Fill(s_sRows, s_sInfo, s_bAllowed);
  }
 }

 // The lobby on screen (tests).
 static ACR_SetupLobby Lobby() { return s_Lobby; }

 protected static void ShowCamera()
 {
  CameraManager cameras = GetGame().GetCameraManager();
  if (!cameras) return;
  if (!s_Camera)
  {
   vector mins, maxs;
   GetGame().GetWorld().GetBoundBox(mins, maxs);
   vector centre = (mins + maxs) * 0.5;
   // South of the centre, looking north and down over the map.
   vector eye = Vector(centre[0], 0, centre[2] - (maxs[2] - mins[2]) * 0.25);
   eye[1] = Math.Max(GetGame().GetWorld().GetSurfaceY(eye[0], eye[2]), 0) + 600;
   EntitySpawnParams parameters = new EntitySpawnParams();
   parameters.TransformMode = ETransformMode.WORLD;
   Math3D.AnglesToMatrix(Vector(0, -30, 0), parameters.Transform);
   parameters.Transform[3] = eye;
   // The game's own spectator camera (Game Master uses this family): a plain CameraBase drew
   // nothing (Jacob 13 Oct, second look).
   Resource prefab = Resource.Load(CAMERA);
   if (prefab && prefab.IsValid()) s_Camera = CameraBase.Cast(GetGame().SpawnEntityPrefab(prefab, GetGame().GetWorld(), parameters));
   if (!s_Camera) s_Camera = CameraBase.Cast(GetGame().SpawnEntity(CameraBase, GetGame().GetWorld(), parameters));
   if (!s_Camera) return;
   s_Camera.SetVerticalFOV(60);
   s_Camera.SetNearPlane(0.5);
   s_Camera.SetFarPlane(8000);
   Print("[ACR] SETUP camera over the battlefield while nobody has spawned");
  }
  if (cameras.CurrentCamera() != s_Camera) cameras.SetCamera(s_Camera);
 }

 // While nobody has seen the dialog work: what the client has on screen, every 3 s.
 protected static void Diagnose()
 {
  float now = System.GetTickCount();
  if (now - s_fDiagAt < 3000) return;
  s_fDiagAt = now;
  string camera = "none";
  CameraManager cameras = GetGame().GetCameraManager();
  if (cameras && cameras.CurrentCamera()) camera = cameras.CurrentCamera().ClassName() + " at " + cameras.CurrentCamera().GetOrigin().ToString();
  string dialog = "none";
  SCR_ConfigurableDialogUi shown = s_Dialog;
  if (s_Lobby) shown = s_Lobby;
  if (shown && shown.GetRootWidget())
  {
   Widget root = shown.GetRootWidget();
   float w, h;
   root.GetScreenSize(w, h);
   dialog = string.Format("visible %1 opacity %2 size %3x%4", root.IsVisible(), root.GetOpacity(), w, h);
  }
  MenuBase top = GetGame().GetMenuManager().GetTopMenu();
  string menu = "none";
  if (top) menu = top.ClassName();
  PrintFormat("[ACR] SETUP screen: loading screen %1, camera %2, dialog %3, top menu %4, allowed %5", ArmaReforgerLoadingAnim.IsOpen(), camera, dialog, menu, s_bAllowed);
 }

 protected static void DropCamera()
 {
  if (!s_Camera) return;
  delete s_Camera;
  s_Camera = null;
 }

 // "Show the layout on the map": the game's own full-screen map menu (works without a body).
 static void OpenPreviewMap()
 {
  OpenMap();
  string hint = "The planned layout. Press Escape or Return to match setup to go back.";
  if (s_sRows.Contains("Show and edit")) hint = "Click a CP, then its destination or Delete selected CP. HQs can only be moved. Undo layout edits restores moves and deletions; New seed rolls a different layout (edits belong to one seed). Markers show RP value and hidden sites. Press Escape or Return to match setup to go back.";
  SCR_HintManagerComponent.ShowCustomHint(hint, "AC: Actual", 12, true);
 }

 // The game's own full-screen map menu over the setup view (the layout preview, the deck editor).
 static void OpenMap()
 {
  if (s_Dialog) { s_Dialog.Close(); s_Dialog = null; }
  if (s_Lobby) { s_Lobby.Close(); s_Lobby = null; }
  GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
 }

 static void Request(string key)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc) pc.ACR_RequestSetup(key);
 }

 static void Reset()
 {
  DropCamera();
  s_bActive = false;
  s_bAllowed = false;
  // World-time stamps: the next mission's clock starts near 0 again.
  s_fHintAt = -100000;
  s_fDiagAt = 0;
  s_sRows = "";
  s_sInfo = "";
  s_bChanged = false;
  if (s_Dialog) s_Dialog.Close();
  s_Dialog = null;
  if (s_Lobby) s_Lobby.Close();
  s_Lobby = null;
  s_bLobbyFailed = false;
 }
}

// A bodyless host has no map gadget to close the fullscreen map. Keep the return controls
// on the map menu itself so they have the same lifetime, including repeated preview visits.
modded class SCR_MapMenuUI
{
 protected Widget m_ACR_SetupReturn;
 protected Widget m_ACR_SetupDelete;
 protected Widget m_ACR_SetupSeed;
 protected bool m_ACR_SetupPreview;

 override void OnMenuOpen()
 {
  super.OnMenuOpen();
  m_ACR_SetupPreview = ACR_SetupClient.s_bActive;
  if (!m_ACR_SetupPreview) return;
  InputManager input = GetGame().GetInputManager();
  input.AddActionListener("MenuBack", EActionTrigger.DOWN, ACR_ReturnToSetup);
  input.AddActionListener("GadgetMap", EActionTrigger.DOWN, ACR_ReturnToSetup);
  m_ACR_SetupReturn = GetGame().GetWorkspace().CreateWidgets("{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout", GetRootWidget());
  if (!m_ACR_SetupReturn) return;
  m_ACR_SetupReturn.SetName("ACR_SetupReturn");
  FrameSlot.SetAnchorMin(m_ACR_SetupReturn, 1, 0);
  FrameSlot.SetAnchorMax(m_ACR_SetupReturn, 1, 0);
  FrameSlot.SetAlignment(m_ACR_SetupReturn, 1, 0);
  FrameSlot.SetPos(m_ACR_SetupReturn, -24, 96);
  FrameSlot.SetSize(m_ACR_SetupReturn, 340, 48);
  SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(m_ACR_SetupReturn);
  if (button)
  {
   button.SetText("Return to match setup");
   button.m_OnClicked.Insert(ACR_OnSetupReturn);
  }
  if (!ACR_SetupClient.s_bAllowed) return;
  // New seed: the setup's Layout seed row (a generated layout, or a quick start's split) without
  // going back to the dialog; the server sends the new preview and the map redraws it.
  if (ACR_SetupClient.s_sRows.Contains("seed\t"))
  {
   m_ACR_SetupSeed = ACR_SetupButton("ACR_SetupSeed", 152, "New seed");
   SCR_ButtonTextComponent seedButton = SCR_ButtonTextComponent.FindButtonTextComponent(m_ACR_SetupSeed);
   if (seedButton) seedButton.m_OnClicked.Insert(ACR_OnSetupSeed);
  }
  if (!ACR_SetupClient.s_sRows.Contains("Show and edit")) return;
  m_ACR_SetupDelete = GetGame().GetWorkspace().CreateWidgets("{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout", GetRootWidget());
  if (!m_ACR_SetupDelete) return;
  m_ACR_SetupDelete.SetName("ACR_SetupDelete");
  FrameSlot.SetAnchorMin(m_ACR_SetupDelete, 1, 0);
  FrameSlot.SetAnchorMax(m_ACR_SetupDelete, 1, 0);
  FrameSlot.SetAlignment(m_ACR_SetupDelete, 1, 0);
  float deleteY = 152;
  if (m_ACR_SetupSeed) deleteY = 208;
  FrameSlot.SetPos(m_ACR_SetupDelete, -24, deleteY);
  FrameSlot.SetSize(m_ACR_SetupDelete, 340, 48);
  SCR_ButtonTextComponent deleteButton = SCR_ButtonTextComponent.FindButtonTextComponent(m_ACR_SetupDelete);
  if (deleteButton)
  {
   deleteButton.SetText("Delete selected CP");
   deleteButton.m_OnClicked.Insert(ACR_OnSetupDelete);
  }
 }

 protected Widget ACR_SetupButton(string name, float y, string text)
 {
  Widget root = GetGame().GetWorkspace().CreateWidgets("{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout", GetRootWidget());
  if (!root) return null;
  root.SetName(name);
  FrameSlot.SetAnchorMin(root, 1, 0);
  FrameSlot.SetAnchorMax(root, 1, 0);
  FrameSlot.SetAlignment(root, 1, 0);
  FrameSlot.SetPos(root, -24, y);
  FrameSlot.SetSize(root, 340, 48);
  SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(root);
  if (button) button.SetText(text);
  return root;
 }

 protected void ACR_OnSetupSeed(SCR_ButtonBaseComponent button)
 {
  ACR_SetupClient.Request("seed");
 }

 protected void ACR_OnSetupDelete(SCR_ButtonBaseComponent button)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
  if (pc) pc.ACR_DeleteSetupSelection();
 }

 void ACR_ReturnToSetup()
 {
  if (!m_ACR_SetupPreview || GetGame().GetMenuManager().GetTopMenu() != this) return;
  GetGame().GetMenuManager().CloseMenuByPreset(ChimeraMenuPreset.MapMenu);
 }

 protected void ACR_OnSetupReturn(SCR_ButtonBaseComponent button)
 {
  ACR_ReturnToSetup();
 }

 override void OnMenuClose()
 {
  if (m_ACR_SetupPreview)
  {
   InputManager input = GetGame().GetInputManager();
   input.RemoveActionListener("MenuBack", EActionTrigger.DOWN, ACR_ReturnToSetup);
   input.RemoveActionListener("GadgetMap", EActionTrigger.DOWN, ACR_ReturnToSetup);
   m_ACR_SetupPreview = false;
  }
  if (m_ACR_SetupReturn) m_ACR_SetupReturn.RemoveFromHierarchy();
  m_ACR_SetupReturn = null;
  if (m_ACR_SetupDelete) m_ACR_SetupDelete.RemoveFromHierarchy();
  m_ACR_SetupDelete = null;
  if (m_ACR_SetupSeed) m_ACR_SetupSeed.RemoveFromHierarchy();
  m_ACR_SetupSeed = null;
  super.OnMenuClose();
 }
}

class ACR_SetupDialog : SCR_ConfigurableDialogUi
{
 static const ResourceName PRESETS = "{AC230926A00100F0}Configs/ACR_Dialogs.conf";
 protected static const ResourceName LAYOUT_BUTTON = "{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout";
 protected static const ResourceName LAYOUT_TEXT = "{B537936D5B7E2BA1}UI/layouts/WidgetLibrary/TextWidgets/Text_BodySmall.layout";
 protected ref map<SCR_ButtonBaseComponent, string> m_Keys = new map<SCR_ButtonBaseComponent, string>();
 protected Widget m_wTabs;
 protected Widget m_wRows;
 protected TextWidget m_wHint; // the Experimental tab's line saying what the pointed-at row does
 protected string m_sHintKey;  // the row it describes: a click rebuilds the rows, the line stays
 protected int m_iTab;
 protected int m_iPage;
 protected int m_iPages = 1;
 protected string m_sRows;
 protected string m_sInfo;

 // Rows a page holds (25 Oct: with the doctrine and AI rows the match settings ran off a 1080p
 // screen, hiding "Show the layout on the map" and the way to the logistics page): what fits
 // under the dialog's title, text and buttons (CHROME, in UI units) at ROW_HEIGHT each, less two
 // for the tab bar and a page button. Tabs normally fit on one page; pages remain for small screens.
 static const float ROW_HEIGHT = 44;
 static const float CHROME = 380;
 static int PageRows(float height)
 {
  int fit = (int)((height - CHROME) / ROW_HEIGHT) - 2;
  if (fit < 6) fit = 6;
  return fit;
 }

 // The screen's height in UI units (1080 for any 16:9 screen).
 static float ScreenHeight()
 {
  WorkspaceWidget workspace = GetGame().GetWorkspace();
  if (!workspace || workspace.GetHeight() <= 0) return 1080;
  return workspace.DPIUnscale(workspace.GetHeight());
 }

 // The tabs across the top (25 Oct redesign): the battlefield first, with its seed and map
 // preview together, then who commands, money, logistics and engineering.
// Experimental: features shipped off by default until they have been played (Jacob, 5 Nov).
 // Presets (#143): save the whole setup under a name, load, share.
 static const int TABS = 7;
 static const int PRESETS_TAB = 6;
 static string TabName(int tab)
 {
  if (tab == 1) return "Command";
  if (tab == 2) return "Economy";
  if (tab == 3) return "Logistics";
  if (tab == 4) return "Engineers";
  if (tab == 5) return "Experimental";
  if (tab == PRESETS_TAB) return "Presets";
  return "Battlefield";
 }

 // The tab a row belongs to; rows nobody sorted yet land on the first tab, where they are seen
 // (the mission intro row sits there on purpose: Command is full at 13 rows, 11 Nov). The 2IC row
 // took Command's last place on 19 Nov; the AI build budget moved to Engineers beside the AI's
 // mines and fortifying rows (srv:setup's one-page check).
 static int Tab(string key)
 {
  // A row's description line goes with its row.
  if (key.StartsWith("about")) return Tab(key.Substring(5, key.Length() - 5));
  if (key.StartsWith("preset")) return PRESETS_TAB;
  if (key == "shared" || key == "ai" || key == "doctrinewest" || key == "doctrineeast" || key == "doctrineadapt" || key == "aicommander" || key == "twoic" || key == "unitcap" || key == "holdguns" || key == "declutter" || key == "capscale") return 1;
  if (key == "pricing" || key == "startrp" || key == "income" || key == "companywest" || key == "companyeast") return 2;
  if (key == "decks" || key == "deckeditor" || key == "deckpoints" || key == "deckwest" || key == "deckeast") return 2;
  if (key == "deployment" || key == "reinforcement" || key == "fobservices" || key == "fobrelay" || key == "depotcapacity" || key == "servicecost" || key == "rpservice" || key == "pullover") return 3;
  if (key == "mines" || key == "minesreal" || key == "minesai" || key == "fortify" || key == "fortifyai" || key == "aibuild" || key == "baseworks" || key == "fieldsoffire") return 4;
  if (ACR_Setup.Experimental(key)) return 5;
  return 0;
 }

 // "US vs USSR, generated, whole map, seed 123": the main choices, seen from every tab.
 static string Summary(string rows)
 {
  map<string, string> values = new map<string, string>();
  array<string> lines = {};
  rows.Split("\n", lines, true);
  foreach (string line : lines)
  {
   array<string> parts = {};
   line.Split("\t", parts, false);
   if (parts.Count() < 2) continue;
   int colon = parts[1].IndexOf(": ");
   if (colon >= 0) values.Set(parts[0], parts[1].Substring(colon + 2, parts[1].Length() - colon - 2));
  }
  string summary = values.Get("west") + " vs " + values.Get("east") + ", " + values.Get("layout");
  string seed;
  if (values.Find("seed", seed))
  {
   if (seed.StartsWith("random")) summary += ", random seed";
   else summary += ", seed " + seed;
  }
  return summary;
 }

 void Fill(string rows, string info)
 {
  m_sRows = rows;
  m_sInfo = info;
  Widget root = GetRootWidget();
  if (!root) return;
  SetTitle("AC: Actual - match setup");
  SetMessage(Summary(rows) + "\nPick a tab, click an option to change it, then press Start. " + info + "\n\nAC: Actual is built and tested by two people. AI agents help write it, but we can't catch every bug or polish every feature ourselves, so please report any problem you hit: discord.gg/WK8xGSztB (issues channel; no GitHub account needed).");
  if (!m_wRows)
  {
   Widget content = root.FindAnyWidget("ContentVerticalLayout");
   if (!content) content = root.FindAnyWidget("ContentLayoutContainer");
   if (!content) return;
   m_wTabs = GetGame().GetWorkspace().CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, content);
   m_wRows = GetGame().GetWorkspace().CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, content);
   AlignableSlot.SetPadding(m_wRows, 0, 8, 0, 0);
  }
  // The Presets tab's name box is rebuilt with the rows: keep what was typed.
  if (m_PresetName) m_sPresetTyped = m_PresetName.GetValue();
  m_PresetName = null;
  while (m_wTabs.GetChildren()) m_wTabs.GetChildren().RemoveFromHierarchy();
  while (m_wRows.GetChildren()) m_wRows.GetChildren().RemoveFromHierarchy();
  m_Keys.Clear();
  for (int tab = 0; tab < TABS; tab++)
  {
   string name = TabName(tab);
   if (tab == m_iTab) name = "[" + name + "]";
   AddButton(m_wTabs, "tab:" + tab.ToString(), name, true);
  }
  array<string> lines = {};
  rows.Split("\n", lines, true);
  if (m_iTab == PRESETS_TAB)
  {
   FillPresets(lines);
   return;
  }
  array<string> keys = {};
  array<string> texts = {};
  array<string> abouts = {};
  foreach (string line : lines)
  {
   array<string> parts = {};
   line.Split("\t", parts, false);
   if (parts.Count() < 2 || Tab(parts[0]) != m_iTab || parts[0].StartsWith("lobby")) continue;
   // A description line, shown under the rows (not a button).
   if (parts[0].StartsWith("about"))
   {
    abouts.Insert(parts[1]);
    continue;
   }
   keys.Insert(parts[0]);
   texts.Insert(parts[1]);
  }
  // This tab's rows, then a page button if the screen is too short for them all.
  int perPage = PageRows(ScreenHeight());
  m_iPages = (keys.Count() + perPage - 1) / perPage;
  if (m_iPages < 1) m_iPages = 1;
  if (m_iPage >= m_iPages) m_iPage = m_iPages - 1;
  int first = m_iPage * perPage;
  for (int i = first; i < keys.Count() && i < first + perPage; i++) AddButton(m_wRows, keys[i], texts[i]);
  foreach (string about : abouts) AddText(m_wRows, about);
  m_wHint = null;
  if (m_iTab == 5)
  {
   string hint = ACR_Setup.Help(m_sHintKey);
   if (hint.IsEmpty()) hint = HINT_IDLE;
   m_wHint = AddText(m_wRows, hint);
  }
  if (m_iPages > 1)
  {
   string more = string.Format("More settings (page %1 of %2)...", m_iPage + 1, m_iPages);
   if (m_iPage == m_iPages - 1) more = string.Format("Back to the first page (page %1 of %2)", m_iPage + 1, m_iPages);
   AddButton(m_wRows, "next_page", more);
  }
 }

 // ---------------------------------------------------------------- Presets tab (#143)
 protected static const ResourceName LAYOUT_EDIT = "{0022F0B45ADBC5AC}UI/layouts/WidgetLibrary/EditBox/WLib_EditBox.layout";
 protected SCR_EditBoxComponent m_PresetName;
 protected string m_sPresetTyped;
 protected string m_sPresetSelected;
 protected ref array<string> m_aPresetNames = {};

 // The name box, Save and Paste; the presets (click one to pick it); what to do with the picked one.
 protected void FillPresets(notnull array<string> lines)
 {
  m_aPresetNames.Clear();
  array<string> shown = {};
  string note;
  foreach (string line : lines)
  {
   array<string> parts = {};
   line.Split("\t", parts, false);
   if (parts.Count() < 2) continue;
   if (parts[0] == "presetnote") note = parts[1];
   else if (parts[0].StartsWith("presetrow:"))
   {
    m_aPresetNames.Insert(parts[0].Substring(10, parts[0].Length() - 10));
    shown.Insert(parts[1]);
   }
  }
  if (!m_aPresetNames.Contains(m_sPresetSelected)) m_sPresetSelected = "";
  if (!note.IsEmpty()) AddText(m_wRows, note);
  Widget edit = GetGame().GetWorkspace().CreateWidgets(LAYOUT_EDIT, m_wRows);
  if (edit)
  {
   AlignableSlot.SetPadding(edit, 2, 2, 2, 2);
   m_PresetName = SCR_EditBoxComponent.Cast(edit.FindHandler(SCR_EditBoxComponent));
   if (m_PresetName)
   {
    m_PresetName.SetLabel("Name");
    m_PresetName.SetValue(m_sPresetTyped);
   }
  }
  Widget top = GetGame().GetWorkspace().CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_wRows);
  AddButton(top, "presetsave", "Save the setup under this name", true);
  AddButton(top, "presetimport", "Paste from the clipboard", true);
  // The list, paged on a short screen (the name box and both action lines take five rows).
  int perPage = PageRows(ScreenHeight()) - 5;
  if (perPage < 3) perPage = 3;
  m_iPages = (m_aPresetNames.Count() + perPage - 1) / perPage;
  if (m_iPages < 1) m_iPages = 1;
  if (m_iPage >= m_iPages) m_iPage = m_iPages - 1;
  int first = m_iPage * perPage;
  for (int i = first; i < m_aPresetNames.Count() && i < first + perPage; i++)
  {
   string text = shown[i];
   if (m_aPresetNames[i] == m_sPresetSelected) text = "> " + text + " <";
   AddButton(m_wRows, "presetpick:" + i.ToString(), text);
  }
  if (m_iPages > 1) AddButton(m_wRows, "next_page", string.Format("More presets (page %1 of %2)...", m_iPage + 1, m_iPages));
  if (m_sPresetSelected.IsEmpty())
  {
   AddText(m_wRows, "Pick a preset to load, rename, delete or copy it,\nor to load it at the start of every match.");
  }
  else
  {
   Widget acts = GetGame().GetWorkspace().CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_wRows);
   AddButton(acts, "presetload", "Load", true);
   AddButton(acts, "presetrename", "Rename to the name above", true);
   AddButton(acts, "presetdelete", "Delete", true);
   Widget more = GetGame().GetWorkspace().CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, m_wRows);
   bool every = shown[m_aPresetNames.Find(m_sPresetSelected)].Contains(", loads every match");
   string everyText = "Load at every match: off";
   if (every) everyText = "Load at every match: on";
   AddButton(more, "presetauto", everyText, true);
   AddButton(more, "presetexport", "Copy to the clipboard", true);
  }
  AddText(m_wRows, "Kept on the server in profile/ACA/SetupPresets. A server loads one\nwith -acrPreset <name>; launch options still win over a preset.");
 }

 protected string PresetTyped()
 {
  if (m_PresetName) m_sPresetTyped = m_PresetName.GetValue();
  return m_sPresetTyped;
 }

 // A Presets tab button: true when it was one.
 protected bool OnPresetRow(string key)
 {
  if (key.StartsWith("presetpick:"))
  {
   int index = key.Substring(11, key.Length() - 11).ToInt();
   if (index >= 0 && index < m_aPresetNames.Count())
   {
    m_sPresetSelected = m_aPresetNames[index];
    // The picked name fills the box: Save then updates it, or type a new name to rename it.
    m_sPresetTyped = m_sPresetSelected;
    if (m_PresetName) m_PresetName.SetValue(m_sPresetTyped);
   }
   Fill(m_sRows, m_sInfo);
   return true;
  }
  if (key == "presetsave") { ACR_SetupClient.Request("preset:save:" + PresetTyped()); return true; }
  if (key == "presetimport") { ACR_SetupClient.Request("preset:import:" + System.ImportFromClipboard()); return true; }
  if (!key.StartsWith("preset") || m_sPresetSelected.IsEmpty()) return key.StartsWith("preset");
  if (key == "presetload") ACR_SetupClient.Request("preset:load:" + m_sPresetSelected);
  else if (key == "presetdelete") ACR_SetupClient.Request("preset:delete:" + m_sPresetSelected);
  else if (key == "presetauto") ACR_SetupClient.Request("preset:auto:" + m_sPresetSelected);
  else if (key == "presetexport") ACR_SetupClient.Request("preset:export:" + m_sPresetSelected);
  else if (key == "presetrename")
  {
   string to = PresetTyped();
   ACR_SetupClient.Request("preset:rename:" + m_sPresetSelected + "\t" + to);
   m_sPresetSelected = ACR_SetupPresets.CleanName(to);
  }
  return true;
 }

 void ShowTab(int tab)
 {
  m_iTab = tab;
  m_iPage = 0;
  Fill(m_sRows, m_sInfo);
 }

 // A row on screen now (the setup UI test).
 bool ShowsRow(string key)
 {
  foreach (SCR_ButtonBaseComponent button, string shown : m_Keys) if (shown == key) return true;
  return false;
 }

 protected TextWidget AddText(Widget parent, string text)
 {
  TextWidget widget = TextWidget.Cast(GetGame().GetWorkspace().CreateWidgets(LAYOUT_TEXT, parent));
  if (!widget) return null;
  AlignableSlot.SetPadding(widget, 8, 4, 2, 0);
  widget.SetText(text);
  return widget;
 }

 protected static const string HINT_IDLE = "These ship switched off until they have been played.\nPoint at a row to see what it does.";

 // A row pointed at (a mouse over it focuses it) or reached with a pad: say what it does.
 protected void OnRowFocus(Widget w)
 {
  if (!m_wHint || !w) return;
  SCR_ButtonBaseComponent button = SCR_ButtonBaseComponent.Cast(w.FindHandler(SCR_ButtonBaseComponent));
  string key;
  if (!button || !m_Keys.Find(button, key)) return;
  string help = ACR_Setup.Help(key);
  if (help.IsEmpty()) return;
  m_sHintKey = key;
  m_wHint.SetText(help);
 }

 protected SCR_ButtonTextComponent AddButton(Widget parent, string key, string text, bool fill = false)
 {
  Widget widget = GetGame().GetWorkspace().CreateWidgets(LAYOUT_BUTTON, parent);
  if (!widget) return null;
  AlignableSlot.SetPadding(widget, 2, 2, 2, 2);
  if (fill) LayoutSlot.SetSizeMode(widget, LayoutSizeMode.Fill);
  SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(widget);
  if (!button) return null;
  button.SetText(text);
  // The command panel's blue: the vanilla hover is white behind white text, hiding the row.
  Color normal = new Color(0.16, 0.42, 0.80, 0.40);
  button.SetBackgroundColors(normal, new Color(0.26, 0.56, 0.96, 0.70), new Color(0.30, 0.62, 1.0, 0.95), new Color(0.36, 0.68, 1.0, 1.0), new Color(0.30, 0.62, 1.0, 0.95));
  if (button.m_wBackground) button.m_wBackground.SetColor(normal);
  Widget border = widget.FindAnyWidget("Border");
  if (border) border.SetColor(new Color(0.45, 0.72, 1.0, 1.0));
  button.m_OnClicked.Insert(OnRow);
  button.m_OnFocus.Insert(OnRowFocus);
  m_Keys.Set(button, key);
  return button;
 }

 protected void OnRow(SCR_ButtonBaseComponent button)
 {
  string key;
  if (!m_Keys.Find(button, key)) return;
  if (key.StartsWith("tab:")) { ShowTab(key.Substring(4, key.Length() - 4).ToInt()); return; }
  if (key == "next_page") { m_iPage = (m_iPage + 1) % m_iPages; Fill(m_sRows, m_sInfo); return; }
  if (key == "map") { ACR_SetupClient.OpenPreviewMap(); return; }
  if (key == "deckeditor") { ACR_DeckClient.OpenFromSetup(); return; }
  if (OnPresetRow(key)) return;
  ACR_SetupClient.Request(key);
 }

 // "Start": the server ends the phase and closes the dialog everywhere.
 override protected void OnConfirm()
 {
  ACR_SetupClient.Request("start");
 }

 override protected void OnCancel()
 {
 }
}
