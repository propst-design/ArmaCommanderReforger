// -acrSetupPresetsTest 1 (server, Arland, ~40 s): setup presets (#143, ACR_SetupPresets).
// Runs in its own folder (profile/ACA/SetupPresetsTest, ACR_TestHooks.SetupPresetDir) so its files and
// auto-load mark never reach other test runs. Checks: built-ins listed, save writes the current
// setup, load replaces the screen's choices, a launch parameter beats the preset, values this
// world can't give fall back and are named, rename/auto-load/delete, the auto-load preset comes
// back at the next match after the per-match reset, clipboard text round-trips, and Start plays
// the loaded preset.
class ACR_TestSetupPresets
{
 static const int CHECKS = 14;
 static const string DIR = "$profile:ACA/SetupPresetsTest";
 protected static int s_iStartedAt = -1;

 static bool Wanted() { return System.IsCLIParam("acrSetupPresetsTest"); }

 protected static void Clean()
 {
  array<string> files = SCR_FileIOHelper.FindFiles(DIR, ".json");
  if (files) foreach (string file : files) FileIO.DeleteFile(file);
  FileIO.DeleteFile(DIR + ACR_SetupPresets.AUTO_FILE);
 }

 protected static string Value(string option)
 {
  string value;
  ACR_Options.Get(option, value);
  return value;
 }

 static void Run(int second)
 {
  if (second == 6)
  {
   Clean();
   ACR_Options.ClearSet();
   string rows = ACR_Setup.Rows();
   ACR_TestRegress.Check("presets: the Presets tab lists the built-ins", rows.Contains("presetrow:Default\tDefault (built-in)") && rows.Contains("presetrow:Large battle\t") && ACR_SetupDialog.Tab("presetrow:Default") == ACR_SetupDialog.PRESETS_TAB && rows.Contains("presetnote\tPreset in use: none"), rows);

   // Only the host or an admin (ACR_Setup.Handle), then save the current choices.
   string refused = ACR_Setup.Handle(9999, "preset:save:Stranger");
   ACR_Setup.Cycle("threat");
   ACR_Setup.Cycle("startrp");
   ACR_Setup.Cycle("startrp");
   ACR_Setup.Cycle("east");
   string saved = ACR_SetupPresets.Handle(0, "save:Test setup");
   ACR_SetupPreset file = ACR_SetupPresets.ReadFile(DIR + "/Test_setup.json");
   bool fileOk = file && file.m_sName == "Test setup" && file.m_Values.Get("acrThreat") == "high" && file.m_Values.Get("acrStartRP") == "100" && file.m_Values.Get("acrFactions") == "US,FIA" && file.m_Values.Contains("acrPockets");
   ACR_TestRegress.Check("presets: a stranger can't, the host saves the current setup to a file", refused.Contains("Only the host") && !ACR_SetupPresets.Find("Stranger") && fileOk, saved);
   string builtIn = ACR_SetupPresets.Handle(0, "save:default");
   string noDelete = ACR_SetupPresets.Handle(0, "delete:Large battle");
   ACR_TestRegress.Check("presets: built-ins can't be overwritten or deleted", builtIn.Contains("built in") && noDelete.Contains("built in"), builtIn + " | " + noDelete);

   // Load: the screen's later clicks are replaced by the preset.
   ACR_Options.ClearSet();
   ACR_Setup.Cycle("threat");
   ACR_Setup.Cycle("threat");
   string loaded = ACR_SetupPresets.Handle(0, "load:test SETUP");
   bool loadOk = ACR_Setup.Current("threat") == "high" && ACR_Options.Source("acrThreat") == "preset" && ACR_FactionSelection.East() == "FIA" && Value("acrStartRP") == "100" && ACR_Options.PresetName() == "Test setup" && ACR_Setup.Rows().Contains("Preset in use: Test setup");
   ACR_TestRegress.Check("presets: load replaces the screen's choices (name without case)", loadOk, string.Format("%1 | threat %2 (%3), red %4, RP %5", loaded, ACR_Setup.Current("threat"), ACR_Options.Source("acrThreat"), ACR_FactionSelection.East(), Value("acrStartRP")));
   ACR_Setup.Cycle("threat");
   ACR_TestRegress.Check("presets: a click after loading still beats the preset", ACR_Setup.Current("threat") == "low" && ACR_Options.Source("acrThreat") == "screen", ACR_Setup.Current("threat"));

   // A launch parameter beats a preset (the watcher allows only test flags: this test's own flag
   // stands in for one; a preset that sets it still reads the flag's 1).
   string pasted = ACR_SetupPresets.Handle(0, "import:{\"name\": \"Pasted\", \"values\": {\"acrSetupPresetsTest\": \"0\", \"acrThreat\": \"low\"}}");
   string income = ACR_SetupPresets.Handle(0, "load:Pasted");
   ACR_TestRegress.Check("presets: a launch parameter beats the preset", pasted.Contains("Pasted") && Value("acrSetupPresetsTest") == "1" && ACR_Options.Source("acrSetupPresetsTest") == "flag" && ACR_Setup.Current("threat") == "low" && ACR_Options.Source("acrThreat") == "preset", pasted + " | " + income);

   // Values this world can't give: an unloaded faction, a mod (Project Outbreak), a battlefield
   // Arland doesn't have, a threat that doesn't exist.
   ACR_SetupPresets.Handle(0, "import:{\"name\": \"Elsewhere\", \"values\": {\"acrFactions\": \"RHS_USAF,FIA\", \"acrZombies\": \"many\", \"acrLayout\": \"Nowhere Valley\", \"acrSize\": \"\", \"acrThreat\": \"extreme\", \"acrPockets\": \"many\"}}");
   string fell = ACR_SetupPresets.Handle(0, "load:Elsewhere");
   bool fellOk = ACR_FactionSelection.West() == "US" && ACR_FactionSelection.East() == "FIA" && ACR_Setup.Current("threat") == "normal" && ACR_Setup.Values("layout").Contains(ACR_Setup.Current("layout")) && ACR_Setup.Current("pockets") == "many" && !ACR_Options.PresetValue("acrZombies", income);
   bool told = fell.Contains("Blue side RHS_USAF isn't loaded") && fell.Contains("no zombies") && fell.Contains("battlefield") && fell.Contains("Threat extreme");
   ACR_TestRegress.Check("presets: a faction, mod, battlefield or value this world lacks falls back and is named", fellOk && told, string.Format("%1 | %2 vs %3, threat %4, layout %5", fell, ACR_FactionSelection.West(), ACR_FactionSelection.East(), ACR_Setup.Current("threat"), ACR_Setup.Current("layout")));

   // Rename keeps the auto-load mark; the old file goes.
   string marked = ACR_SetupPresets.Handle(0, "auto:Test setup");
   string renamed = ACR_SetupPresets.Handle(0, "rename:Test setup\tTest renamed");
   bool renameOk = ACR_SetupPresets.AutoName() == "Test renamed" && FileIO.FileExists(DIR + "/Test_renamed.json") && !FileIO.FileExists(DIR + "/Test_setup.json") && ACR_Setup.Rows().Contains("presetrow:Test renamed\tTest renamed, loads every match");
   ACR_TestRegress.Check("presets: rename keeps the every-match mark and moves the file", marked.Contains("every match") && renameOk, marked + " | " + renamed);

   // The next match: the per-match reset (ACR_Manager.Stop) clears the screen and the preset;
   // the marked preset is read again by itself.
   ACR_Options.ClearSet();
   ACR_SetupPresets.Reset();
   ACR_Options.ForgetPreset();
   bool autoOk = ACR_Setup.Current("threat") == "high" && ACR_Options.PresetName() == "Test renamed" && ACR_Options.Source("acrThreat") == "preset";
   ACR_SetupPresets.CheckPending();
   ACR_TestRegress.Check("presets: the every-match preset loads again after the per-match reset", autoOk && ACR_SetupPresets.Report().Contains("loads every match"), string.Format("%1, threat %2", ACR_SetupPresets.Report(), ACR_Setup.Current("threat")));

   // Clipboard text (Copy) parses back to the same preset.
   ACR_SetupPreset renamedPreset = ACR_SetupPresets.Find("Test renamed");
   ACR_SetupPreset back = ACR_SetupPresets.Parse(ACR_SetupPresets.Json(renamedPreset));
   bool same = back && back.m_sName == renamedPreset.m_sName && back.m_Values.Count() == renamedPreset.m_Values.Count();
   if (same) foreach (string key, string value : renamedPreset.m_Values) if (back.m_Values.Get(key) != value) same = false;
   ACR_TestRegress.Check("presets: the clipboard text round-trips", same && !ACR_SetupPresets.Parse("ACADECK1 not a preset"), ACR_SetupPresets.Json(renamedPreset));

   // Delete asks twice and clears the mark.
   string first = ACR_SetupPresets.Handle(0, "delete:Test renamed");
   bool stillThere = FileIO.FileExists(DIR + "/Test_renamed.json");
   string second2 = ACR_SetupPresets.Handle(0, "delete:Test renamed");
   ACR_TestRegress.Check("presets: delete asks twice and clears the every-match mark", first.Contains("again") && stillThere && !FileIO.FileExists(DIR + "/Test_renamed.json") && ACR_SetupPresets.AutoName() == "" && second2.Contains("Deleted"), first + " | " + second2);

   // Built-in Large battle, then Default gives the scenario back.
   ACR_SetupPresets.Handle(0, "load:Large battle");
   bool large = ACR_Setup.Current("threat") == "high" && Value("acrStartRP") == "400" && Value("acrUnitCap") == "16" && ACR_Setup.Current("capscale") == "1" && ACR_Setup.Current("layout") == "full";
   ACR_SetupPresets.Handle(0, "load:Default");
   bool scenario = ACR_Setup.Current("threat") == "normal" && Value("acrStartRP") == "" && ACR_Options.PresetName() == "Default";
   ACR_TestRegress.Check("presets: Large battle sets a bigger match, Default gives the scenario back", large && scenario, string.Format("large %1, default %2 (threat %3)", large, scenario, ACR_Setup.Current("threat")));

   // Play a preset: save one with high threat and FIA red, load it, Start.
   ACR_Setup.Cycle("threat");
   ACR_Setup.Cycle("east");
   ACR_SetupPresets.Handle(0, "save:Played");
   ACR_Options.ClearSet();
   ACR_SetupPresets.Handle(0, "load:Played");
   ACR_Setup.Start("the presets test");
   s_iStartedAt = second;
  }
  if (s_iStartedAt > 0 && second == s_iStartedAt + 8)
  {
   ACR_TestRegress.Check("presets: Start plays the loaded preset", ACR_Setup.Started() && ACR_Manager.ThreatOption() == 2 && ACR_FactionSelection.Applied() && ACR_FactionSelection.East() == "FIA", string.Format("threat %1, red %2", ACR_Manager.ThreatOption(), ACR_FactionSelection.East()));
   int agents;
   array<AIAgent> all = {};
   if (GetGame().GetAIWorld()) GetGame().GetAIWorld().GetAIAgents(all);
   agents = all.Count();
   ACR_TestRegress.Check("presets: the match runs after a preset start", ACR_Skirmish.Applied() && agents > 0, string.Format("AI agents %1", agents));
   Clean();
   ACR_TestRegress.Complete("presets", CHECKS);
  }
 }
}
