// Setup phase (Workshop host options, step 2, 13 Oct). With acrSetup on (our shipped
// scenarios and Play/Host launchers turn it on; tests start from a world
// without a header), nothing starts until the host or an admin presses Start: no ticks,
// no skirmish layout, no starting units, no player spawns. Meanwhile they see a dialog
// with the match options; each click cycles one option on the server (ACR_Options.Set).
// A dedicated server with nobody allowed to start begins by itself AUTO_START_S after the
// first player joined.
class ACR_Setup
{
 static const int AUTO_START_S = 300;
 protected static bool s_bDecided;
 protected static bool s_bWanted;
 protected static bool s_bStarted;
 protected static float s_fFirstPlayerAt = -1;
 protected static ref map<int, string> s_Sent = new map<int, string>();
 // Layout preview (aca-f07 editor, step 1): the bases the chosen battlefield will have, sent to
 // the host's map as base markers while the phase lasts.
 protected static string s_sPreviewSig;
 static ref array<vector> s_PreviewPos = {};
 static ref array<string> s_PreviewNames = {};
 protected static ref array<string> s_PreviewOwners = {};
 protected static ref array<string> s_PreviewDetails = {};
 protected static ref array<float> s_PreviewRadii = {};
 protected static ref map<int, string> s_PreviewSent = new map<int, string>();

 static bool Wanted()
 {
  if (!s_bDecided)
  {
   s_bDecided = true;
   string value;
   s_bWanted = ACR_Options.Get("acrSetup", value) && value == "1";
   if (System.IsCLIParam("acrSetupTest") || System.IsCLIParam("acrSetupUiTest") || System.IsCLIParam("acrSetupGenTest") || System.IsCLIParam("acrSetupMapTest")) s_bWanted = true;
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
  int seed = 0;
  if (generated) seed = ACR_Generator.Seed();
  string west = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  string signature = string.Format("%1|%2|%3|%4|%5", layout, seed, west, east, ACR_Generator.s_iMovesVersion);
  if (signature == s_sPreviewSig) return;
  s_sPreviewSig = signature;
  s_PreviewPos.Clear();
  s_PreviewNames.Clear();
  s_PreviewOwners.Clear();
  s_PreviewDetails.Clear();
  s_PreviewRadii.Clear();
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
    AddPreview(site.m_vPos, site.m_sName, owner, detail, site.m_fRadius);
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
    AddPreview(base.GetOrigin(), base.m_sName, baseOwner, baseDetail, base.m_fRadius);
   }
  }
  PrintFormat("[ACR] SETUP preview %1: %2 bases", signature, s_PreviewNames.Count());
 }

 protected static void AddPreview(vector pos, string name, string owner, string detail, float radius)
 {
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
  if (!players.IsEmpty() && s_fFirstPlayerAt < 0) s_fFirstPlayerAt = now;
  if (!GetGame().GetPlayerController() && s_fFirstPlayerAt >= 0 && now - s_fFirstPlayerAt >= AUTO_START_S * 1000)
  {
   Start("the timer (nobody pressed Start)");
   return;
  }
  Broadcast();
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
  if (!s_bStarted) UpdatePreview();
  string rows = Rows();
  foreach (int id : players)
  {
   SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
   if (!pc) continue;
   bool active = !s_bStarted;
   bool allowed = ACR_Manager.IsHostOrAdmin(id);
   if (active && allowed) SendPreview(id, pc);
   string text = rows;
   if (!allowed) text = "";
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
  if (Current("layout").StartsWith("gen:")) info = "Generated layout: support helicopters come from off the map; build a helipad to buy helicopters. ";
  if (left >= 0) info += string.Format("Starts by itself in %1 s.", left);
  return info;
 }

 // ---------------------------------------------------------------- options
 // The rows the dialog shows: "key\tLabel: value" lines.
 static string Rows()
 {
  string rows;
  array<string> keys = {"west", "east", "layout", "seed", "threat", "pricing", "ai", "unitcap", "startrp", "income", "shared", "deployment", "reinforcement", "fobservices", "fobrelay", "depotcapacity", "servicecost", "rpservice", "mines", "minesreal", "minesai"};
  foreach (string key : keys)
  {
   if (key == "seed" && !Current("layout").StartsWith("gen:")) continue;
   rows += key + "\t" + Label(key) + ": " + ValueName(key, Current(key)) + "\n";
  }
  // Opens the map on the host's own screen with the layout drawn on it (client side).
  bool generated = Current("layout").StartsWith("gen:");
  if (!s_PreviewNames.IsEmpty())
  {
   if (generated) rows += string.Format("map\tShow and edit the layout on the map (%1 bases)\n", s_PreviewNames.Count());
   else rows += string.Format("map\tShow the layout on the map (%1 bases)\n", s_PreviewNames.Count());
  }
  int moves = ACR_Generator.MoveCount();
  if (generated && moves > 0) rows += string.Format("undo\tUndo layout edits (%1)\n", moves);
  return rows;
 }

 static string Label(string key)
 {
  if (key == "west") return "Blue side";
  if (key == "east") return "Red side";
  if (key == "layout") return "Battlefield";
  if (key == "seed") return "Layout seed";
  if (key == "threat") return "Threat";
  if (key == "pricing") return "Prices";
  if (key == "ai") return "AI commanders";
  if (key == "unitcap") return "AI squad cap";
  if (key == "startrp") return "Starting RP";
  if (key == "income") return "Income";
  if (key == "shared") return "Shared command";
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
  return key;
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
   values.Insert("gen:flashpoint");
   bool half, flash;
   foreach (ACR_Skirmish layout : ACR_Skirmish.s_All)
   {
    if (layout.m_eSize == ACR_ESkirmishSize.HALF) half = true;
    else flash = true;
   }
   if (half) values.Insert("size:half");
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
  else if (key == "threat") values = {"low", "normal", "high"};
  else if (key == "pricing") values = {"stepped", "stock"};
  else if (key == "ai")
  {
   values = {"both", "none", "west", "east"};
  }
  else if (key == "unitcap") values = {"", "4", "6", "8", "12", "16", "24"};
  else if (key == "startrp") values = {"", "50", "100", "200", "400", "800"};
  else if (key == "income") values = {"", "0.5", "1", "1.5", "2", "3"};
  else if (key == "shared") values = {"1", "0"};
  else if (key == "deployment") values = {"military", "all", "hq"};
  else if (key == "reinforcement") values = {"settlements", "all", "deployment"};
  else if (key == "fobservices") values = {"supplies", "free"};
  else if (key == "fobrelay") values = {"1", "0"};
  else if (key == "depotcapacity") values = {"0", "1000", "2000", "4000"};
  else if (key == "servicecost") values = {"5", "10", "20"};
  else if (key == "rpservice") values = {"0", "3", "5"};
  else if (key == "mines") values = {"1", "0"};
  else if (key == "minesreal" || key == "minesai") values = {"0", "1"};
  return values;
 }

 static string Option(string key)
 {
  if (key == "threat") return "acrThreat";
  if (key == "pricing") return "acrPricing";
  if (key == "ai") return "acrAI";
  if (key == "unitcap") return "acrUnitCap";
  if (key == "startrp") return "acrStartRP";
  if (key == "income") return "acrIncome";
  if (key == "shared") return "acrSharedCommand";
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
  if (!ACR_Options.Get(Option(key), value)) value = "";
  value.ToLower();
  if (value == "")
  {
   if (key == "deployment") return "military";
   if (key == "reinforcement") return "settlements";
   if (key == "fobservices") return "supplies";
   if (key == "fobrelay") return "1";
   if (key == "depotcapacity") return "2000";
   if (key == "servicecost") return "10";
   if (key == "rpservice") return "3";
   if (key == "mines") return "1";
   if (key == "minesreal" || key == "minesai") return "0";
  }
  if (key == "threat" && value == "") return "normal";
  if (key == "pricing" && value == "") return "";
  if (key == "ai" && value == "") return "";
  if (key == "shared" && value == "") return "1";
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
   return "scenario default";
  }
  if (key == "layout")
  {
   if (value == "full") return "whole map";
   if (value == "size:half") return "a random half";
   if (value == "size:flashpoint") return "a random flashpoint";
   if (value == "gen:full") return "generated, whole map";
   if (value == "gen:half") return "generated, half the map";
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
  if (key == "income") return "x" + value;
  if (key == "deployment" && value == "military") return "military bases / HQs / FOBs";
  if (key == "deployment" && value == "hq") return "HQs / FOBs only";
  if (key == "deployment" && value == "all") return "all deployment-enabled points";
  if (key == "reinforcement" && value == "settlements") return "settlements + deployment bases";
  if (key == "reinforcement" && value == "deployment") return "deployment bases only";
  if (key == "reinforcement" && value == "all") return "all reinforcement-enabled points";
  if (key == "minesreal" && value == "1") return "dangerous to all";
  if (key == "minesreal") return "safe for our AI";
  if (key == "shared" || key == "fobrelay" || key == "mines" || key == "minesai")
  {
   if (value == "0") return "off";
   return "on";
  }
  return value;
 }

 // Next value of a row (server, host/admin checked by the caller).
 static string Cycle(string key)
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
  string next = values[(index + 1) % values.Count()];
  SetValue(key, next);
  Broadcast();
  return Label(key) + ": " + ValueName(key, next);
 }

 static void SetValue(string key, string value)
 {
  if (key == "west") { ACR_Options.Set("acrFactions", value + "," + ACR_FactionSelection.East()); return; }
  if (key == "east") { ACR_Options.Set("acrFactions", ACR_FactionSelection.West() + "," + value); return; }
  if (key == "layout")
  {
   if (value == "full") { ACR_Options.Set("acrLayout", ""); ACR_Options.Set("acrSize", "full"); }
   else if (value.StartsWith("gen:")) { ACR_Options.Set("acrLayout", "generated"); ACR_Options.Set("acrSize", value.Substring(4, value.Length() - 4)); }
   else if (value.StartsWith("size:")) { ACR_Options.Set("acrLayout", ""); ACR_Options.Set("acrSize", value.Substring(5, value.Length() - 5)); }
   else { ACR_Options.Set("acrLayout", value.Substring(5, value.Length() - 5)); ACR_Options.Set("acrSize", ""); }
   return;
  }
  string option = Option(key);
  if (option == "") return;
  // "" = back to the scenario's default: drop the screen's value (a flag or header may still set it).
  if (value == "") ACR_Options.Unset(option);
  else ACR_Options.Set(option, value);
 }

 // A client asked (server side).
 static string Handle(int playerId, string key)
 {
  if (!Wanted() || s_bStarted) return "The match has already started";
  if (!ACR_Manager.IsHostOrAdmin(playerId)) return "Only the host or an admin can change the match setup";
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
   return;
  }
  if (!s_bAllowed)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   if (now - s_fHintAt > 15000)
   {
    s_fHintAt = now;
    SCR_HintManagerComponent.ShowCustomHint("Waiting for the host or an admin to set up the match and press Start. " + s_sInfo, "AC: Actual", 14, true);
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
  if (s_Dialog && s_Dialog.GetRootWidget())
  {
   Widget root = s_Dialog.GetRootWidget();
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
  if (s_Dialog) { s_Dialog.Close(); s_Dialog = null; }
  GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.MapMenu);
  string hint = "The planned layout. Press Escape or Return to match setup to go back.";
  if (s_sRows.Contains("Show and edit")) hint = "Click a CP, then its destination or Delete selected CP. HQs can only be moved. Undo layout edits restores moves and deletions. Markers show RP value and hidden sites. Press Escape or Return to match setup to go back.";
  SCR_HintManagerComponent.ShowCustomHint(hint, "AC: Actual", 12, true);
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
  if (s_Dialog) s_Dialog.Close();
  s_Dialog = null;
 }
}

// A bodyless host has no map gadget to close the fullscreen map. Keep the return controls
// on the map menu itself so they have the same lifetime, including repeated preview visits.
modded class SCR_MapMenuUI
{
 protected Widget m_ACR_SetupReturn;
 protected Widget m_ACR_SetupDelete;
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
  if (!ACR_SetupClient.s_bAllowed || !ACR_SetupClient.s_sRows.Contains("Show and edit")) return;
  m_ACR_SetupDelete = GetGame().GetWorkspace().CreateWidgets("{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout", GetRootWidget());
  if (!m_ACR_SetupDelete) return;
  m_ACR_SetupDelete.SetName("ACR_SetupDelete");
  FrameSlot.SetAnchorMin(m_ACR_SetupDelete, 1, 0);
  FrameSlot.SetAnchorMax(m_ACR_SetupDelete, 1, 0);
  FrameSlot.SetAlignment(m_ACR_SetupDelete, 1, 0);
  FrameSlot.SetPos(m_ACR_SetupDelete, -24, 152);
  FrameSlot.SetSize(m_ACR_SetupDelete, 340, 48);
  SCR_ButtonTextComponent deleteButton = SCR_ButtonTextComponent.FindButtonTextComponent(m_ACR_SetupDelete);
  if (deleteButton)
  {
   deleteButton.SetText("Delete selected CP");
   deleteButton.m_OnClicked.Insert(ACR_OnSetupDelete);
  }
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
  super.OnMenuClose();
 }
}

class ACR_SetupDialog : SCR_ConfigurableDialogUi
{
 static const ResourceName PRESETS = "{AC230926A00100F0}Configs/ACR_Dialogs.conf";
 protected static const ResourceName LAYOUT_BUTTON = "{75C912A1C89BE6C2}UI/layouts/WidgetLibrary/Buttons/WLib_ButtonText.layout";
 protected ref map<SCR_ButtonBaseComponent, string> m_Keys = new map<SCR_ButtonBaseComponent, string>();
 protected Widget m_wRows;
 protected bool m_bLogisticsPage;
 protected string m_sRows;
 protected string m_sInfo;

 void Fill(string rows, string info)
 {
  m_sRows = rows;
  m_sInfo = info;
  Widget root = GetRootWidget();
  if (!root) return;
  SetTitle("AC: Actual - match setup");
  SetMessage("Click an option to change it, then press Start. " + info + "\n\nAC: Actual is built and tested by one person. AI agents help write it, but I can't catch every bug or polish every feature alone, so please report any problem you hit: discord.gg/WK8xGSztB (issues channel; no GitHub account needed).");
  if (!m_wRows)
  {
   Widget content = root.FindAnyWidget("ContentVerticalLayout");
   if (!content) content = root.FindAnyWidget("ContentLayoutContainer");
   if (!content) return;
   m_wRows = GetGame().GetWorkspace().CreateWidget(WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(Color.WHITE), 0, content);
  }
  while (m_wRows.GetChildren()) m_wRows.GetChildren().RemoveFromHierarchy();
  m_Keys.Clear();
  array<string> lines = {};
  rows.Split("\n", lines, true);
  foreach (string line : lines)
  {
   array<string> parts = {};
   line.Split("\t", parts, false);
   if (parts.Count() < 2) continue;
   bool logistics = parts[0] == "deployment" || parts[0] == "reinforcement" || parts[0] == "fobservices" || parts[0] == "fobrelay" || parts[0] == "depotcapacity" || parts[0] == "servicecost" || parts[0] == "rpservice" || parts[0] == "mines" || parts[0] == "minesreal" || parts[0] == "minesai";
   if (logistics != m_bLogisticsPage) continue;
   Widget widget = GetGame().GetWorkspace().CreateWidgets(LAYOUT_BUTTON, m_wRows);
   if (!widget) continue;
   AlignableSlot.SetPadding(widget, 2, 2, 2, 2);
   SCR_ButtonTextComponent button = SCR_ButtonTextComponent.FindButtonTextComponent(widget);
   if (!button) continue;
   button.SetText(parts[1]);
   button.m_OnClicked.Insert(OnRow);
   m_Keys.Set(button, parts[0]);
  }
  Widget page = GetGame().GetWorkspace().CreateWidgets(LAYOUT_BUTTON, m_wRows);
  if (!page) return;
  SCR_ButtonTextComponent pageButton = SCR_ButtonTextComponent.FindButtonTextComponent(page);
  if (pageButton)
  {
   string caption = "Logistics settings...";
   if (m_bLogisticsPage) caption = "Back to match settings";
   pageButton.SetText(caption);
   pageButton.m_OnClicked.Insert(OnRow);
   m_Keys.Set(pageButton, "logistics_page");
  }
 }

 protected void OnRow(SCR_ButtonBaseComponent button)
 {
  string key;
  if (!m_Keys.Find(button, key)) return;
  if (key == "logistics_page") { m_bLogisticsPage = !m_bLogisticsPage; Fill(m_sRows, m_sInfo); return; }
  if (key == "map") { ACR_SetupClient.OpenPreviewMap(); return; }
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
