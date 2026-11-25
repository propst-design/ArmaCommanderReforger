// Setup presets (#143, 19 Nov 2026). Setup screen choices reset at the end of every match (Jacob,
// 31 Oct); a preset is how a host keeps a favourite: a name and the host options the setup
// screen controls, by their -acr names ({"name": "...", "values": {"acrThreat": "high", ...}}).
// Files live in the server's profile (profile/ACA/SetupPresets/*.json): a listen host's or
// singleplayer game's own profile, a dedicated server's profile. Only the host or an admin
// manages them, from the setup screen's Presets tab (ACR_Setup.Handle checks); clients never
// read presets (ACR_Options.Preset) and see only the applied rows.
// A preset is an ACR_Options layer: it beats the mission header, launch parameters still beat it,
// and clicks on the setup screen beat everything. Loading one replaces the screen's choices.
// At the start of every match the server loads the preset named by -acrPreset (or the header's
// m_sAcrPreset), else the one marked "loads every match" (AutoLoad.txt), else none.
// A value that doesn't fit this world (a faction or mod that isn't loaded, a battlefield this
// map doesn't have, a deck this host doesn't own) falls back to the default and says so (Check).
// Not kept: generated-layout edits (base moves/deletions belong to one seed and one map).
class ACR_SetupPreset
{
 string m_sName;
 string m_sFile;
 bool m_bBuiltIn;
 ref map<string, string> m_Values = new map<string, string>();
}

class ACR_SetupPresets
{
 static const string DIR = "$profile:ACA/SetupPresets";
 static const string AUTO_FILE = "/AutoLoad.txt";
 static const string DEFAULT_NAME = "Default";
 static const string LARGE_NAME = "Large battle";
 static const string COIN_NAME = "Counter-insurgency";
 static const int NAME_CHARS = 28;
 static const int MAX_IMPORT_CHARS = 8192;
 // A preset was put in use and not yet checked against this world (Check).
 protected static bool s_bPending;
 // What the last load said (shown on the setup screen and logged).
 protected static string s_sReport;
 protected static string s_sLoadedBy;
 // Delete asks twice (the deck editor's way): the name armed by the first press.
 protected static string s_sDeleteArmed;

 static void Reset()
 {
  s_bPending = false;
  s_sReport = "";
  s_sLoadedBy = "";
  s_sDeleteArmed = "";
  s_sRows = "";
  s_fRowsAt = 0;
 }

 static string Dir()
 {
  string dir = ACR_TestHooks.Get().SetupPresetDir();
  if (dir.IsEmpty()) dir = DIR;
  return dir;
 }

 static string Report() { return s_sReport; }

 // ---------------------------------------------------------------- the list
 static array<ref ACR_SetupPreset> BuiltIn()
 {
  array<ref ACR_SetupPreset> presets = {};
  // Default: nothing set, so the scenario's own setup (header and launch options) applies.
  ACR_SetupPreset preset = new ACR_SetupPreset();
  preset.m_sName = DEFAULT_NAME;
  preset.m_bBuiltIn = true;
  presets.Insert(preset);
  // Large battle: the whole map, higher threat, more AI squads and money, more FIA.
  preset = new ACR_SetupPreset();
  preset.m_sName = LARGE_NAME;
  preset.m_bBuiltIn = true;
  preset.m_Values.Set("acrLayout", "");
  preset.m_Values.Set("acrSize", "full");
  preset.m_Values.Set("acrThreat", "high");
  preset.m_Values.Set("acrUnitCap", "16");
  preset.m_Values.Set("acrCapScale", "1");
  preset.m_Values.Set("acrStartRP", "400");
  preset.m_Values.Set("acrPockets", "many");
  preset.m_Values.Set("acrGuerrillas", "many");
  presets.Insert(preset);
  // Counter-insurgency (0.8.0, ACR_Coin): one army on the whole map against many FIA, the army's
  // own AI commander on (players can take it over), normal threat. The army stays the Blue side
  // unless the host picks the other one after loading it.
  preset = new ACR_SetupPreset();
  preset.m_sName = COIN_NAME;
  preset.m_bBuiltIn = true;
  preset.m_Values.Set("acrMatchType", "coin");
  preset.m_Values.Set("acrLayout", "");
  preset.m_Values.Set("acrSize", "full");
  preset.m_Values.Set("acrQuickStart", "off");
  preset.m_Values.Set("acrThreat", "normal");
  preset.m_Values.Set("acrPockets", "many");
  preset.m_Values.Set("acrGuerrillas", "many");
  presets.Insert(preset);
  return presets;
 }

 // Built-in presets first, then the server's files by name.
 static array<ref ACR_SetupPreset> List()
 {
  array<ref ACR_SetupPreset> presets = BuiltIn();
  array<string> files = SCR_FileIOHelper.FindFiles(Dir(), ".json");
  if (!files) return presets;
  array<ref ACR_SetupPreset> own = {};
  foreach (string file : files)
  {
   ACR_SetupPreset preset = ReadFile(file);
   if (!preset || Taken(presets, preset.m_sName) || Taken(own, preset.m_sName)) continue;
   int at = 0;
   while (at < own.Count() && own[at].m_sName.Compare(preset.m_sName, false) < 0) at++;
   own.InsertAt(preset, at);
  }
  foreach (ACR_SetupPreset ownPreset : own) presets.Insert(ownPreset);
  return presets;
 }

 protected static bool Taken(notnull array<ref ACR_SetupPreset> presets, string name)
 {
  string lower = name;
  lower.ToLower();
  foreach (ACR_SetupPreset preset : presets)
  {
   string other = preset.m_sName;
   other.ToLower();
   if (other == lower) return true;
  }
  return false;
 }

 // By name or file name (with or without .json), without case.
 static ACR_SetupPreset Find(string name)
 {
  name.TrimInPlace();
  string lower = name;
  lower.ToLower();
  array<ref ACR_SetupPreset> presets = List();
  foreach (ACR_SetupPreset preset : presets)
  {
   string presetName = preset.m_sName;
   presetName.ToLower();
   string file = FileName(preset.m_sName);
   file.ToLower();
   if (presetName == lower || file == lower || file + ".json" == lower) return preset;
  }
  return null;
 }

 // A name fit for a row and a request: no tabs, new lines or colons, at most NAME_CHARS.
 static string CleanName(string name)
 {
  string clean = "";
  for (int i = 0; i < name.Length(); i++)
  {
   string c = name.Get(i);
   if (c == "\t" || c == "\n" || c == "\r" || c == ":" || c == "\"" || c == "\\") continue;
   clean += c;
  }
  clean.TrimInPlace();
  if (clean.Length() > NAME_CHARS) clean = clean.Substring(0, NAME_CHARS);
  clean.TrimInPlace();
  return clean;
 }

 static string FileName(string name)
 {
  string file = "";
  for (int i = 0; i < name.Length(); i++)
  {
   string c = name.Get(i);
   int code = c.ToAscii();
   bool keep = (code >= 48 && code <= 57) || (code >= 65 && code <= 90) || (code >= 97 && code <= 122) || c == "-" || c == "_";
   if (keep) file += c;
   else if (c == " ") file += "_";
  }
  if (file.IsEmpty()) file = "preset";
  return file;
 }

 // "My setup", "My setup 2", ...: a name no other preset has.
 static string FreeName(string wanted)
 {
  string name = wanted;
  int n = 2;
  while (Find(name) || FileIO.FileExists(Dir() + "/" + FileName(name) + ".json"))
  {
   name = wanted + " " + n.ToString();
   n++;
  }
  return name;
 }

 // ---------------------------------------------------------------- the match's preset
 // Called once per match from ACR_Options' preset layer (server): -acrPreset (launch parameter or
 // header), else the marked auto-load preset. Returns the name in use, "" for none.
 static string AutoLoad(notnull map<string, string> into)
 {
  string name;
  string by = "-acrPreset";
  if (!ACR_Options.Launch("acrPreset", name)) name = "";
  name.TrimInPlace();
  if (name.IsEmpty())
  {
   name = AutoName();
   by = "loads every match";
  }
  if (name.IsEmpty()) return "";
  ACR_SetupPreset preset = Find(name);
  if (!preset)
  {
   s_sReport = string.Format("Preset %1 (%2) wasn't found in profile/ACA/SetupPresets: the scenario's setup applies.", name, by);
   PrintFormat("[ACR] PRESET %1", s_sReport, level: LogLevel.WARNING);
   return "";
  }
  foreach (string key, string value : preset.m_Values) into.Set(key, value);
  s_sLoadedBy = by;
  s_sReport = "";
  s_bPending = true;
  Changed();
  PrintFormat("[ACR] PRESET %1 in use (%2): %3 values", preset.m_sName, by, preset.m_Values.Count());
  return preset.m_sName;
 }

 static string AutoName()
 {
  array<string> lines = SCR_FileIOHelper.ReadFileContent(Dir() + AUTO_FILE, false);
  if (!lines) return "";
  foreach (string line : lines)
  {
   line.TrimInPlace();
   if (!line.IsEmpty()) return line;
  }
  return "";
 }

 protected static void SetAutoName(string name)
 {
  FileIO.MakeDirectory("$profile:ACA");
  FileIO.MakeDirectory(Dir());
  if (name.IsEmpty()) FileIO.DeleteFile(Dir() + AUTO_FILE);
  else SCR_FileIOHelper.WriteFileContent(Dir() + AUTO_FILE, {name});
 }

 // ---------------------------------------------------------------- saving the current setup
 // Every option the setup screen controls, as it stands now (whatever set it: header, launch
 // parameter, preset or a click), so the preset gives back exactly what the host saw.
 static void Snapshot(notnull map<string, string> into)
 {
  into.Clear();
  into.Set("acrFactions", ACR_FactionSelection.West() + "," + ACR_FactionSelection.East());
  array<string> keys = ACR_Setup.RowKeys();
  foreach (string key : keys)
  {
   if (key == "west" || key == "east" || key == "map" || key == "undo" || key == "deckeditor") continue;
   string current = ACR_Setup.Current(key);
   // The seed only matters for a generated layout or a quick start's split.
   if (key == "seed" && !ACR_Setup.Current("layout").StartsWith("gen:") && ACR_Setup.Current("quickstart") == "off") continue;
   // Zombie rows only with Project Outbreak (their rows are hidden without it).
   if ((key == "zombies" || key == "zombiemode" || key == "outbreak") && !ACR_Outbreak.Loaded()) continue;
   ACR_Setup.OptionsFor(key, current, into);
  }
  // "" = the scenario's default: left out, so the preset doesn't pin it.
  array<string> empty = {};
  foreach (string option, string value : into) if (value.IsEmpty() && option != "acrLayout" && option != "acrSize") empty.Insert(option);
  foreach (string drop : empty) into.Remove(drop);
 }

 // ---------------------------------------------------------------- checking against this world
 // Once after a preset was put in use (ACR_Setup.Broadcast, ACR_Manager start): values this world
 // can't give fall back to the default, and the host is told; launch options that keep their own
 // value over the preset are named.
 static void CheckPending()
 {
  if (!s_bPending) return;
  s_bPending = false;
  string name = ACR_Options.PresetName();
  if (name.IsEmpty()) return;
  array<string> notes = {};
  CheckFactions(notes);
  array<string> keys = ACR_Setup.RowKeys();
  foreach (string key : keys) CheckRow(key, notes);
  string seed;
  if (ACR_Options.PresetValue("acrSeed", seed) && seed.ToInt() <= 0) ACR_Options.DropPresetValue("acrSeed");
  // Launch parameters beat a preset (as they beat the header): say which.
  array<string> pinned = {};
  foreach (string pinKey : keys)
  {
   string option = ACR_Setup.Option(pinKey);
   if (pinKey == "layout") option = "acrSize";
   if (pinKey == "west") option = "acrFactions";
   if (pinKey == "seed") option = "acrSeed";
   string value;
   if (option.IsEmpty() || !ACR_Options.PresetValue(option, value) || ACR_Options.Source(option) != "flag") continue;
   string label = ACR_Setup.Label(pinKey);
   if (pinKey == "west") label = "Sides";
   if (!pinned.Contains(label)) pinned.Insert(label);
  }
  string report = "Preset " + name + " (" + s_sLoadedBy + ")";
  if (notes.IsEmpty()) report += " loaded.";
  else report += ": " + Join(notes, "; ") + ".";
  if (!pinned.IsEmpty()) report += " Launch options keep " + Join(pinned, ", ") + ".";
  s_sReport = report;
  PrintFormat("[ACR] PRESET %1", report);
 }

 protected static string Join(notnull array<string> parts, string glue)
 {
  string text = "";
  foreach (int i, string part : parts)
  {
   if (i > 0) text += glue;
   text += part;
  }
  return text;
 }

 // Two faction keys this world doesn't list: each missing side falls back to the scenario's.
 protected static void CheckFactions(notnull array<string> notes)
 {
  string pair;
  if (!ACR_Options.PresetValue("acrFactions", pair) || ACR_Options.Source("acrFactions") != "preset") return;
  array<string> parts = {};
  pair.Split(",", parts, true);
  // A legacy pair name (vanilla, ru_us, ...): ACR_FactionSelection.Choice checks those itself.
  if (parts.Count() != 2) return;
  string west = ACR_FactionSelection.Key(parts[0]);
  string east = ACR_FactionSelection.Key(parts[1]);
  if (!west.IsEmpty() && !east.IsEmpty() && west != east) return;
  ACR_Options.DropPresetValue("acrFactions");
  string fallbackWest = ACR_FactionSelection.West();
  string fallbackEast = ACR_FactionSelection.East();
  if (west.IsEmpty()) notes.Insert(string.Format("Blue side %1 isn't loaded, %2 instead", parts[0].Trim(), ACR_FactionSelection.DisplayName(fallbackWest)));
  if (east.IsEmpty()) notes.Insert(string.Format("Red side %1 isn't loaded, %2 instead", parts[1].Trim(), ACR_FactionSelection.DisplayName(fallbackEast)));
  if (west.IsEmpty()) west = fallbackWest;
  if (east.IsEmpty()) east = fallbackEast;
  if (west == east)
  {
   west = fallbackWest;
   east = fallbackEast;
  }
  if (west != fallbackWest || east != fallbackEast) ACR_Options.SetPresetValue("acrFactions", west + "," + east);
 }

 protected static void CheckRow(string key, notnull array<string> notes)
 {
  if (key == "west" || key == "east" || key == "seed" || key == "map" || key == "undo" || key == "deckeditor") return;
  if (key == "layout")
  {
   if (ACR_Options.Source("acrLayout") != "preset" && ACR_Options.Source("acrSize") != "preset") return;
   string layout = ACR_Setup.Current("layout");
   if (Offered(ACR_Setup.Values("layout"), layout)) return;
   string wanted = ACR_Setup.ValueName("layout", layout);
   ACR_Options.DropPresetValue("acrLayout");
   ACR_Options.DropPresetValue("acrSize");
   notes.Insert(string.Format("battlefield %1 isn't on this map, %2 instead", wanted, ACR_Setup.ValueName("layout", ACR_Setup.Current("layout"))));
   return;
  }
  string option = ACR_Setup.Option(key);
  string value;
  if (option.IsEmpty() || !ACR_Options.PresetValue(option, value) || ACR_Options.Source(option) != "preset") return;
  // A socketed mod that isn't loaded: Project Outbreak's rows.
  if ((key == "zombies" || key == "zombiemode" || key == "outbreak") && !ACR_Outbreak.Loaded())
  {
   ACR_Options.DropPresetValue(option);
   string lower = value;
   lower.ToLower();
   if (key == "zombies" && lower != "off") notes.Insert("no zombies: Project Outbreak isn't loaded");
   return;
  }
  array<string> values = ACR_Setup.Values(key);
  if (values.IsEmpty() || Offered(values, ACR_Setup.Current(key))) return;
  ACR_Options.DropPresetValue(option);
  notes.Insert(string.Format("%1 %2 isn't available here, %3 instead", ACR_Setup.Label(key), value, ACR_Setup.ValueName(key, ACR_Setup.Current(key))));
 }

 protected static bool Offered(notnull array<string> values, string value)
 {
  value.ToLower();
  foreach (string candidate : values)
  {
   string lower = candidate;
   lower.ToLower();
   if (lower == value) return true;
  }
  return false;
 }

 // ---------------------------------------------------------------- the setup screen
 // The Presets tab's rows: "presetnote\t..." (text) and one "presetrow:<name>\t<shown>" each.
 // The setup broadcast asks every second: the files are read again after a change made here, or
 // every ROWS_REFRESH_MS for a file dropped into the folder by hand.
 static const float ROWS_REFRESH_MS = 10000;
 protected static string s_sRows;
 protected static float s_fRowsAt;
 static void Changed() { s_sRows = ""; }

 static string Rows()
 {
  float now = 0;
  if (GetGame().GetWorld()) now = GetGame().GetWorld().GetWorldTime();
  if (!s_sRows.IsEmpty() && now >= s_fRowsAt && now - s_fRowsAt < ROWS_REFRESH_MS) return s_sRows;
  s_fRowsAt = now;
  s_sRows = ReadRows();
  return s_sRows;
 }

 protected static string ReadRows()
 {
  string inUse = ACR_Options.PresetName();
  if (inUse.IsEmpty()) inUse = "none (the scenario's setup)";
  string rows = "presetnote\tPreset in use: " + inUse + "\n";
  string marked = AutoName();
  string autoLower = marked;
  autoLower.ToLower();
  array<ref ACR_SetupPreset> presets = List();
  foreach (ACR_SetupPreset preset : presets)
  {
   string shown = preset.m_sName;
   if (preset.m_bBuiltIn) shown += " (built-in)";
   string lower = preset.m_sName;
   lower.ToLower();
   if (!marked.IsEmpty() && lower == autoLower) shown += ", loads every match";
   rows += "presetrow:" + preset.m_sName + "\t" + shown + "\n";
  }
  return rows;
 }

 // A Presets tab request (server; ACR_Setup.Handle checked host/admin and the phase).
 // "save:<name>", "load:<name>", "rename:<old>\t<new>", "delete:<name>", "auto:<name>",
 // "export:<name>", "import:<json>".
 static string Handle(int playerId, string request)
 {
  int colon = request.IndexOf(":");
  if (colon < 0) return "Unknown preset request";
  string verb = request.Substring(0, colon);
  string arg = request.Substring(colon + 1, request.Length() - colon - 1);
  Changed();
  if (verb != "delete") s_sDeleteArmed = "";
  if (verb == "save") return Save(arg);
  if (verb == "import") return Import(arg);
  if (verb == "rename")
  {
   array<string> names = {};
   arg.Split("\t", names, false);
   if (names.Count() != 2) return "Type the new name first";
   return Rename(names[0], names[1]);
  }
  ACR_SetupPreset preset = Find(arg);
  if (!preset) return "No preset called " + arg;
  if (verb == "load") return Load(preset, "loaded on the setup screen");
  if (verb == "delete") return Delete(preset);
  if (verb == "auto") return ToggleAuto(preset);
  if (verb == "export") return Export(playerId, preset);
  return "Unknown preset request";
 }

 // Load: the preset replaces the screen's choices (and the layout edits, which belong to a seed).
 static string Load(notnull ACR_SetupPreset preset, string by)
 {
  ACR_Options.ClearSet();
  ACR_Generator.ClearMoves();
  ACR_Options.UsePreset(preset.m_sName, preset.m_Values);
  s_sLoadedBy = by;
  s_bPending = true;
  CheckPending();
  return s_sReport;
 }

 protected static string Save(string name)
 {
  name = CleanName(name);
  if (name.IsEmpty()) name = FreeName("My setup");
  ACR_SetupPreset old = Find(name);
  if (old && old.m_bBuiltIn) return name + " is built in and can't be changed: type another name";
  ACR_SetupPreset preset = new ACR_SetupPreset();
  preset.m_sName = name;
  if (old) preset.m_sFile = old.m_sFile;
  Snapshot(preset.m_Values);
  if (!WriteFile(preset)) return "Couldn't write " + Dir() + "/" + FileName(name) + ".json";
  if (old) return string.Format("Saved %1 over the old one (%2 settings)", name, preset.m_Values.Count());
  return string.Format("Saved %1 (%2 settings) in profile/ACA/SetupPresets", name, preset.m_Values.Count());
 }

 protected static string Rename(string from, string to)
 {
  ACR_SetupPreset preset = Find(from);
  if (!preset) return "No preset called " + from;
  if (preset.m_bBuiltIn) return preset.m_sName + " is built in and can't be renamed";
  to = CleanName(to);
  if (to.IsEmpty()) return "Type the new name first";
  string lowerTo = to;
  lowerTo.ToLower();
  string lowerFrom = preset.m_sName;
  lowerFrom.ToLower();
  if (lowerTo != lowerFrom && (Find(to) || FileIO.FileExists(Dir() + "/" + FileName(to) + ".json"))) return "There is already a preset called " + to;
  string was = preset.m_sName;
  string autoLower = AutoName();
  autoLower.ToLower();
  bool wasAuto = !autoLower.IsEmpty() && autoLower == lowerFrom;
  preset.m_sName = to;
  if (!WriteFile(preset)) return "Couldn't write " + FileName(to) + ".json";
  if (wasAuto) SetAutoName(to);
  if (ACR_Options.PresetName() == was)
  {
   ACR_Options.UsePreset(to, preset.m_Values);
   s_bPending = true;
   CheckPending();
  }
  return string.Format("Renamed %1 to %2", was, to);
 }

 protected static string Delete(notnull ACR_SetupPreset preset)
 {
  if (preset.m_bBuiltIn) return preset.m_sName + " is built in and can't be deleted";
  if (s_sDeleteArmed != preset.m_sName)
  {
   s_sDeleteArmed = preset.m_sName;
   return "Press Delete again to delete " + preset.m_sName + " for good";
  }
  s_sDeleteArmed = "";
  FileIO.DeleteFile(preset.m_sFile);
  string lower = preset.m_sName;
  lower.ToLower();
  string autoLower = AutoName();
  autoLower.ToLower();
  if (!autoLower.IsEmpty() && autoLower == lower) SetAutoName("");
  // The choices it gave stay on the screen until something else changes them.
  return "Deleted " + preset.m_sName;
 }

 protected static string ToggleAuto(notnull ACR_SetupPreset preset)
 {
  string lower = preset.m_sName;
  lower.ToLower();
  string marked = AutoName();
  marked.ToLower();
  if (marked == lower || preset.m_sName == DEFAULT_NAME)
  {
   SetAutoName("");
   return "No preset loads by itself now: each match starts from the scenario's setup";
  }
  SetAutoName(preset.m_sName);
  return preset.m_sName + " loads at the start of every match (launch option -acrPreset still wins)";
 }

 protected static string Export(int playerId, notnull ACR_SetupPreset preset)
 {
  SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
  if (pc) pc.ACR_SendSetupClipboard(Json(preset));
  string where;
  if (preset.m_bBuiltIn) where = "a server picks a built-in preset with -acrPreset \"" + preset.m_sName + "\"";
  else where = "the file is profile/ACA/SetupPresets/" + FileName(preset.m_sName) + ".json; a server loads it with -acrPreset " + FileName(preset.m_sName);
  return "Copied " + preset.m_sName + " to the clipboard: " + where;
 }

 protected static string Import(string text)
 {
  if (text.Length() > MAX_IMPORT_CHARS) return "The clipboard text is too long to be a setup preset";
  ACR_SetupPreset preset = Parse(text);
  if (!preset) return "The clipboard holds no setup preset (it starts with {\"name\": ..., \"values\": {...}})";
  string name = CleanName(preset.m_sName);
  if (name.IsEmpty()) name = "Pasted setup";
  preset.m_sName = FreeName(name);
  preset.m_bBuiltIn = false;
  if (!WriteFile(preset)) return "Couldn't write " + FileName(preset.m_sName) + ".json";
  return "Pasted " + preset.m_sName + ": load it to use it";
 }

 // ---------------------------------------------------------------- files
 static bool WriteFile(notnull ACR_SetupPreset preset)
 {
  if (preset.m_bBuiltIn) return false;
  FileIO.MakeDirectory("$profile:ACA");
  FileIO.MakeDirectory(Dir());
  string file = Dir() + "/" + FileName(preset.m_sName) + ".json";
  string oldFile = preset.m_sFile;
  array<string> lines = {};
  JsonLines(preset, lines);
  if (!SCR_FileIOHelper.WriteFileContent(file, lines)) return false;
  string oldLower = oldFile;
  oldLower.ToLower();
  string fileLower = file;
  fileLower.ToLower();
  if (!oldFile.IsEmpty() && oldLower != fileLower) FileIO.DeleteFile(oldFile);
  preset.m_sFile = file;
  return true;
 }

 static void JsonLines(notnull ACR_SetupPreset preset, notnull array<string> lines)
 {
  lines.Insert("{");
  lines.Insert("  \"name\": \"" + Escape(preset.m_sName) + "\",");
  lines.Insert("  \"values\": {");
  array<string> keys = {};
  foreach (string key, string unused : preset.m_Values) keys.Insert(key);
  keys.Sort();
  foreach (int i, string sorted : keys)
  {
   string pair = "    \"" + Escape(sorted) + "\": \"" + Escape(preset.m_Values.Get(sorted)) + "\"";
   if (i < keys.Count() - 1) pair += ",";
   lines.Insert(pair);
  }
  lines.Insert("  }");
  lines.Insert("}");
 }

 static string Json(notnull ACR_SetupPreset preset)
 {
  array<string> lines = {};
  JsonLines(preset, lines);
  string text = "";
  foreach (string line : lines) text += line + "\n";
  return text;
 }

 static ACR_SetupPreset ReadFile(string file)
 {
  array<string> lines = SCR_FileIOHelper.ReadFileContent(file, false);
  if (!lines) return null;
  string text = "";
  foreach (string line : lines) text += line + "\n";
  ACR_SetupPreset preset = Parse(text);
  if (!preset)
  {
   PrintFormat("[ACR] PRESET %1 is not a setup preset (expected {\"name\": ..., \"values\": {...}})", file, level: LogLevel.WARNING);
   return null;
  }
  preset.m_sFile = file;
  preset.m_sName = CleanName(preset.m_sName);
  if (preset.m_sName.IsEmpty()) preset.m_sName = CleanName(FilePath.StripExtension(FilePath.StripPath(file)));
  return preset;
 }

 // The flat JSON this class writes. Values may be strings, numbers or true/false; keys that
 // aren't acr options are kept but nothing reads them; other top-level fields are skipped.
 static ACR_SetupPreset Parse(string text)
 {
  array<string> tokens = {};
  if (!Tokens(text, tokens)) return null;
  if (tokens.Count() < 2 || tokens[0] != "{") return null;
  ACR_SetupPreset preset = new ACR_SetupPreset();
  int i = 1;
  bool inValues;
  bool sawValues;
  while (i < tokens.Count())
  {
   string token = tokens[i];
   if (token == "}")
   {
    if (!inValues)
    {
     if (!sawValues) return null;
     return preset;
    }
    inValues = false;
    i++;
    continue;
   }
   if (token == ",")
   {
    i++;
    continue;
   }
   if (i + 2 >= tokens.Count() || tokens[i + 1] != ":") return null;
   string key = Unquote(token);
   string value = tokens[i + 2];
   if (inValues)
   {
    if (value == "{" || value == "[") return null;
    preset.m_Values.Set(key, Unquote(value));
    i += 3;
    continue;
   }
   if (key == "values" && value == "{")
   {
    inValues = true;
    sawValues = true;
    i += 3;
    continue;
   }
   if (value == "{" || value == "[") return null;
   if (key == "name") preset.m_sName = Unquote(value);
   i += 3;
  }
  return null;
 }

 // Strings keep their opening quote (escapes resolved) so a value can't pass for punctuation.
 protected static bool Tokens(string text, notnull array<string> tokens)
 {
  int n = text.Length();
  int i = 0;
  while (i < n)
  {
   string c = text.Get(i);
   if (c == " " || c == "\n" || c == "\r" || c == "\t")
   {
    i++;
    continue;
   }
   if (c == "{" || c == "}" || c == ":" || c == "," || c == "[" || c == "]")
   {
    tokens.Insert(c);
    i++;
    continue;
   }
   if (c == "\"")
   {
    string s = "\"";
    i++;
    bool closed;
    while (i < n)
    {
     string d = text.Get(i);
     if (d == "\\" && i + 1 < n)
     {
      s += text.Get(i + 1);
      i += 2;
      continue;
     }
     i++;
     if (d == "\"")
     {
      closed = true;
      break;
     }
     s += d;
    }
    if (!closed) return false;
    tokens.Insert(s);
    continue;
   }
   string word = "";
   while (i < n)
   {
    string w = text.Get(i);
    if (w == " " || w == "\n" || w == "\r" || w == "\t" || w == "," || w == "}" || w == "]" || w == ":") break;
    word += w;
    i++;
   }
   if (word == "true") word = "1";
   if (word == "false") word = "0";
   if (word.IsEmpty()) return false;
   tokens.Insert(word);
  }
  return true;
 }

 protected static string Unquote(string token)
 {
  if (token.StartsWith("\"")) return token.Substring(1, token.Length() - 1);
  return token;
 }

 protected static string Escape(string value)
 {
  value.Replace("\\", "\\\\");
  value.Replace("\"", "\\\"");
  return value;
 }
}
