// Host options in one place (Workshop host options plan, 13 Oct). Workshop hosts, rented
// servers and consoles can't pass -acr flags, so every option is read through ACR_Options,
// which resolves it from these sources (later wins):
//  1. the scenario's mission header (ACR_MissionHeader fields in Missions/*.conf; a dedicated
//     server's config can override them under gameProperties.missionHeader),
//  2. -acr command-line flags (our launchers, srv: tests, buddy sessions),
//  3. the in-game setup screen (Set; the screen itself comes next).
// Readers ask with the flag name ("acrThreat") and get the flag's text ("high").
class ACR_Options
{
 protected static ref map<string, string> s_Set = new map<string, string>();
 protected static ref map<string, string> s_Header;

 // The option's text, or false when no source sets it.
 static bool Get(string name, out string value)
 {
  if (s_Set.Find(name, value)) return true;
  if (System.IsCLIParam(name))
  {
   if (!System.GetCLIParam(name, value)) value = "";
   return true;
  }
  if (Header().Find(name, value)) return true;
  value = "";
  return false;
 }

 // On/off options: set and not "0"/"false" (a bare -acrGameMasterAll counts as on).
 static bool Flag(string name)
 {
  string value;
  if (!Get(name, value)) return false;
  value.ToLower();
  return value != "0" && value != "false";
 }

 // Where the option came from: "screen", "flag", "header" or "" (not set).
 static string Source(string name)
 {
  string value;
  if (s_Set.Find(name, value)) return "screen";
  if (System.IsCLIParam(name)) return "flag";
  if (Header().Find(name, value)) return "header";
  return "";
 }

 // The setup screen's choices (server side, before Start).
 static void Set(string name, string value) { s_Set.Set(name, value); }
 static void Unset(string name) { s_Set.Remove(name); }
 static void ClearSet() { s_Set.Clear(); }

 protected static map<string, string> Header()
 {
  if (s_Header) return s_Header;
  s_Header = new map<string, string>();
  ACR_MissionHeader header = ACR_MissionHeader.Cast(GetGame().GetMissionHeader());
  if (header) header.Fill(s_Header);
  return s_Header;
 }

 // Tests: use these header values instead of the mission's (null = read the mission again).
 static void UseHeaderForTest(map<string, string> values)
 {
  s_Header = values;
 }

 // One log line with every option that is set and its source.
 static void Log()
 {
  string line;
  array<string> names = {"acrLayout", "acrSize", "acrFactions", "acrPlayers", "acrThreat", "acrPricing", "acrAI", "acrUnitCap", "acrSoldierCap", "acrStartRP", "acrIncome", "acrSkill", "acrReaction", "acrSharedCommand", "acrRespawnSeconds", "acrGameMasterAll", "acrMarksmen", "acrFirstPlayerHost", "acrSetup", "acrDeployment", "acrReinforcement", "acrFobServices", "acrFobRelay", "acrDepotCapacity", "acrServiceSupplyCost", "acrFobRPServiceCost", "acrMines", "acrMinesRealistic", "acrMinesAI"};
  foreach (string name : names)
  {
   string value;
   if (!Get(name, value)) continue;
   line += string.Format(" %1=%2(%3)", name, value, Source(name));
  }
  string headerName = "none";
  if (GetGame().GetMissionHeader()) headerName = GetGame().GetMissionHeader().ClassName();
  PrintFormat("[ACR] OPTIONS header %1:%2", headerName, line);
 }
}

// The mission header our scenarios use: SCR_MissionHeader plus host option defaults.
// Blank / -1 = not set (the world's own settings or the built-in default apply).
// Field names are what a dedicated server's gameProperties.missionHeader overrides.
class ACR_MissionHeader : SCR_MissionHeader
{
 [Attribute("", UIWidgets.EditBox, "Skirmish layout by name (-acrLayout); blank = by size")]
 string m_sAcrLayout;
 [Attribute("", UIWidgets.EditBox, "Skirmish size: full, half or flashpoint (-acrSize)")]
 string m_sAcrSize;
 [Attribute("", UIWidgets.EditBox, "Faction pair: vanilla, ru_us, ru_ua, us_ua or two faction keys WEST,EAST (-acrFactions)")]
 string m_sAcrFactions;
 [Attribute("", UIWidgets.EditBox, "Put every player on this faction key, or any (-acrPlayers)")]
 string m_sAcrPlayers;
 [Attribute("", UIWidgets.EditBox, "Threat: low, normal or high (-acrThreat)")]
 string m_sAcrThreat;
 [Attribute("", UIWidgets.EditBox, "Pricing: stepped or stock (-acrPricing)")]
 string m_sAcrPricing;
 [Attribute("", UIWidgets.EditBox, "AI commanders: both, none or a faction key (-acrAI)")]
 string m_sAcrAI;
 [Attribute("-1", UIWidgets.EditBox, "AI commander squad cap per side (-acrUnitCap)")]
 int m_iAcrUnitCap;
 [Attribute("-1", UIWidgets.EditBox, "Generated layout seed (-acrSeed); -1 or 0 = a new random layout each match")]
 int m_iAcrSeed;
 [Attribute("-1", UIWidgets.EditBox, "Living AI soldiers per side (-acrSoldierCap)")]
 int m_iAcrSoldierCap;
 [Attribute("-1", UIWidgets.EditBox, "Starting RP per side (-acrStartRP)")]
 int m_iAcrStartRP;
 [Attribute("-1", UIWidgets.EditBox, "Income multiplier (-acrIncome)")]
 float m_fAcrIncome;
 [Attribute("-1", UIWidgets.EditBox, "AI skill (-acrSkill)")]
 int m_iAcrSkill;
 [Attribute("-1", UIWidgets.EditBox, "AI spotting speed (-acrReaction)")]
 float m_fAcrReaction;
 [Attribute("-1", UIWidgets.EditBox, "Shared command: 0 or 1 (-acrSharedCommand)")]
 int m_iAcrSharedCommand;
 [Attribute("-1", UIWidgets.EditBox, "Player respawn seconds (-acrRespawnSeconds)")]
 int m_iAcrRespawnSeconds;
 [Attribute("-1", UIWidgets.EditBox, "Game Master for every player: 0 or 1 (-acrGameMasterAll)")]
 int m_iAcrGameMasterAll;
 [Attribute("-1", UIWidgets.EditBox, "Automatic squad marksmen: 0 or 1 (-acrMarksmen)")]
 int m_iAcrMarksmen;
 [Attribute("-1", UIWidgets.EditBox, "Setup screen before the match: 1 = the host or an admin picks options and presses Start (-acrSetup)")]
 int m_iAcrSetup;

 [Attribute("", UIWidgets.EditBox, "New-unit deployment: military, all or hq (-acrDeployment)")]
 string m_sAcrDeployment;
 [Attribute("", UIWidgets.EditBox, "Infantry replacements: settlements, all or deployment (-acrReinforcement)")]
 string m_sAcrReinforcement;
 [Attribute("", UIWidgets.EditBox, "Automatic FOB services: supplies or free (-acrFobServices)")]
 string m_sAcrFobServices;
 [Attribute("-1", UIWidgets.EditBox, "FOBs require a completed friendly radio relay for deployment/RP reinforcement: 1 default, 0 disables (-acrFobRelay)")]
 int m_iAcrFobRelay;
 [Attribute("-1", UIWidgets.EditBox, "One depot's FOB capacity bonus, 0 disables (-acrDepotCapacity)")]
 int m_iAcrDepotCapacity;
 [Attribute("-1", UIWidgets.EditBox, "Supplies per automatic FOB service load (-acrServiceSupplyCost)")]
 int m_iAcrServiceSupplyCost;
 [Attribute("-1", UIWidgets.EditBox, "RP per soldier actually healed/restocked instantly at FOB (-acrFobRPServiceCost)")]
 int m_iAcrFobRPServiceCost;
 [Attribute("-1", UIWidgets.EditBox, "Commander minefield orders: 1 default, 0 disables (-acrMines)")]
 int m_iAcrMines;
 [Attribute("-1", UIWidgets.EditBox, "Realistic minefields: 1 = the owning side's AI sets off its own mines too (-acrMinesRealistic)")]
 int m_iAcrMinesRealistic;
 [Attribute("-1", UIWidgets.EditBox, "AI commanders lay minefields: 0 default, 1 enables (-acrMinesAI)")]
 int m_iAcrMinesAI;

 void Fill(map<string, string> values)
 {
  Text(values, "acrLayout", m_sAcrLayout);
  Text(values, "acrSize", m_sAcrSize);
  Text(values, "acrFactions", m_sAcrFactions);
  Text(values, "acrPlayers", m_sAcrPlayers);
  Text(values, "acrThreat", m_sAcrThreat);
  Text(values, "acrPricing", m_sAcrPricing);
  Text(values, "acrAI", m_sAcrAI);
  Number(values, "acrUnitCap", m_iAcrUnitCap);
  Number(values, "acrSeed", m_iAcrSeed);
  Number(values, "acrSoldierCap", m_iAcrSoldierCap);
  Number(values, "acrStartRP", m_iAcrStartRP);
  if (m_fAcrIncome >= 0) values.Set("acrIncome", m_fAcrIncome.ToString());
  Number(values, "acrSkill", m_iAcrSkill);
  if (m_fAcrReaction >= 0) values.Set("acrReaction", m_fAcrReaction.ToString());
  Number(values, "acrSharedCommand", m_iAcrSharedCommand);
  Number(values, "acrRespawnSeconds", m_iAcrRespawnSeconds);
  Number(values, "acrGameMasterAll", m_iAcrGameMasterAll);
  Number(values, "acrMarksmen", m_iAcrMarksmen);
  Number(values, "acrSetup", m_iAcrSetup);
  Text(values, "acrDeployment", m_sAcrDeployment);
  Text(values, "acrReinforcement", m_sAcrReinforcement);
  Text(values, "acrFobServices", m_sAcrFobServices);
  Number(values, "acrFobRelay", m_iAcrFobRelay);
  Number(values, "acrDepotCapacity", m_iAcrDepotCapacity);
  Number(values, "acrServiceSupplyCost", m_iAcrServiceSupplyCost);
  Number(values, "acrFobRPServiceCost", m_iAcrFobRPServiceCost);
  Number(values, "acrMines", m_iAcrMines);
  Number(values, "acrMinesRealistic", m_iAcrMinesRealistic);
  Number(values, "acrMinesAI", m_iAcrMinesAI);
 }

 protected static void Text(map<string, string> values, string name, string value)
 {
  string trimmed = value;
  trimmed.Trim();
  if (trimmed != "") values.Set(name, trimmed);
 }
 protected static void Number(map<string, string> values, string name, int value)
 {
  if (value >= 0) values.Set(name, value.ToString());
 }
}
