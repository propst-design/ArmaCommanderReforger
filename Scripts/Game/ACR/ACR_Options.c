// Host options in one place (Workshop host options plan, 13 Oct). Workshop hosts, rented
// servers and consoles can't pass -acr flags, so every option is read through ACR_Options,
// which resolves it from these sources (later wins):
//  1. the scenario's mission header (ACR_MissionHeader fields in Missions/*.conf; a dedicated
//     server's config can override them under gameProperties.missionHeader),
//  2. -acr command-line flags (our launchers, srv: tests, buddy sessions),
//  3. the in-game setup screen (Set; the screen itself comes next).
// A setup preset (#143, ACR_SetupPresets) sits between 1 and 2: it beats the mission header,
// launch parameters still beat it, and the setup screen's clicks beat everything.
// Readers ask with the flag name ("acrThreat") and get the flag's text ("high").
class ACR_Options
{
 protected static ref map<string, string> s_Set;
 protected static ref map<string, string> s_NoPreset;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_Set = new map<string, string>();
  s_NoPreset = new map<string, string>();
 }
 protected static ref map<string, string> s_Header;
 // The preset in use (null = not read yet this match) and its name ("" = none).
 protected static ref map<string, string> s_Preset;
 protected static string s_sPresetName;

 // While true, Get answers as if the setup screen and the preset had set nothing: the server's own
 // default (ACR_Setup.DefaultValue, the setup lobby's "changed" dot). Set and cleared in one call.
 static bool s_bDefaults;

 // The option's text, or false when no source sets it.
 static bool Get(string name, out string value)
 {
  if (!s_bDefaults && s_Set.Find(name, value)) return true;
  if (System.IsCLIParam(name))
  {
   if (!System.GetCLIParam(name, value)) value = "";
   return true;
  }
  if (!s_bDefaults && Preset().Find(name, value)) return true;
  if (Header().Find(name, value)) return true;
  value = "";
  return false;
 }

 // Only the launch parameter or the mission header (what picks the preset itself: -acrPreset).
 static bool Launch(string name, out string value)
 {
  if (System.IsCLIParam(name))
  {
   if (!System.GetCLIParam(name, value)) value = "";
   return true;
  }
  if (Header().Find(name, value)) return true;
  value = "";
  return false;
 }

 // The preset layer. Read on the server only: a client's own profile (its auto-load preset as a
 // host) must never change what it reads while it plays on somebody else's server.
 protected static map<string, string> Preset()
 {
  if (RplSession.Mode() == RplMode.Client) return s_NoPreset;
  if (s_Preset) return s_Preset;
  s_Preset = new map<string, string>();
  s_sPresetName = ACR_SetupPresets.AutoLoad(s_Preset);
  return s_Preset;
 }

 // The setup screen loaded a preset (server): it replaces the one in use.
 static void UsePreset(string name, notnull map<string, string> values)
 {
  s_Preset = new map<string, string>();
  foreach (string key, string value : values) s_Preset.Set(key, value);
  s_sPresetName = name;
 }
 static string PresetName()
 {
  Preset();
  return s_sPresetName;
 }
 // A value the preset sets (Get for the whole stack); false when the preset doesn't set it.
 static bool PresetValue(string name, out string value)
 {
  return Preset().Find(name, value);
 }
 // A preset value that doesn't fit this world (ACR_SetupPresets.Check) is dropped.
 static void DropPresetValue(string name)
 {
  Preset().Remove(name);
 }
 static void SetPresetValue(string name, string value)
 {
  Preset().Set(name, value);
 }
 // Next match: the auto-load preset is read again (setup choices reset per match, Jacob 31 Oct).
 static void ForgetPreset()
 {
  s_Preset = null;
  s_sPresetName = "";
 }

 // On/off options: set and not "0"/"false" (a bare -acrGameMasterAll counts as on).
 static bool Flag(string name)
 {
  string value;
  if (!Get(name, value)) return false;
  value.ToLower();
  return value != "0" && value != "false";
 }

 // Where the option came from: "screen", "flag", "preset", "header" or "" (not set).
 static string Source(string name)
 {
  string value;
  if (s_Set.Find(name, value)) return "screen";
  if (System.IsCLIParam(name)) return "flag";
  if (Preset().Find(name, value)) return "preset";
  if (Header().Find(name, value)) return "header";
  return "";
 }

 // The setup screen's choices (server side, before Start).
 static void Set(string name, string value) { s_Set.Set(name, value); }
 static void Unset(string name) { s_Set.Remove(name); }
 static void ClearSet() { s_Set.Clear(); }
 static void ForgetHeader() { s_Header = null; }

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
  array<string> names = {"acrLayout", "acrSize", "acrPockets", "acrQuickStart", "acrGuerrillas", "acrFIAAllegiance", "acrFIALeaning", "acrFIASpoils", "acrFIAGrievance", "acrFIAHidden", "acrMatchType", "acrCoinArmy", "acrCoinInsurgency", "acrCoinInsurgent", "acrCoinInsurgentIncome", "acrCoinStrength", "acrCoinPosture", "acrCoinHostileIncome", "acrZombies", "acrZombieMode", "acrOutbreak", "acrFactions", "acrPlayers", "acrThreat", "acrPricing", "acrAI", "acrDoctrineWest", "acrDoctrineEast", "acrDoctrineAdapt", "acrCompanyWest", "acrCompanyEast", "acrUnitCap", "acrSoldierCap", "acrStartRP", "acrIncome", "acrSkill", "acrReaction", "acrSharedCommand", "acrRespawnSeconds", "acrGameMasterAll", "acrMarksmen", "acrFirstPlayerHost", "acrSetup", "acrDeployment", "acrReinforcement", "acrFobServices", "acrFobRelay", "acrDepotCapacity", "acrServiceSupplyCost", "acrFobRPServiceCost", "acrMines", "acrMinesRealistic", "acrMinesAI", "acrCapScale", "acrAIBuildShare", "acrFortify", "acrFortifyAI", "acrBaseWorks", "acrHQWorks", "acrBaseBays", "acrBaseSmallGuns", "acrWings", "acrBaseStyles", "acrBasePlacement", "acrEmplacements", "acrWingMen", "acrBaseDress", "acrBaseObstacles", "acrBagHold", "acrBaseRing", "acrKillGround", "acrBaseFinalWalk", "acrHoldGuns", "acrMapDeclutter", "acrIntro", "acrIntroMusic", "acrIntroMusicZombies", "acrSquadTactics", "acrRoadFlow", "acrFortifyDefenders1", "acrFortifyDefenders2", "acrFortifyDefenders3", "acrRoadblockDefenders", "acrFieldRoutes", "acrAICommander", "acrAIStrategy", "acrAIAllocator", "acrPeriodJets", "acrCoveredFallBack", "acrDefenceForecast", "acrAttackGround", "acrFieldsOfFire", "acrPullOver", "acrEvadeArmour", "acr2IC", "acrSupplyRuns", "acrLogisticsHubs", "acrSaveRoutes"};
  foreach (string name : names)
  {
   string value;
   if (!Get(name, value)) continue;
   line += string.Format(" %1=%2(%3)", name, value, Source(name));
  }
  string headerName = "none";
  if (GetGame().GetMissionHeader()) headerName = GetGame().GetMissionHeader().ClassName();
  string preset = PresetName();
  if (preset != "") headerName += ", preset " + preset;
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
 [Attribute("", UIWidgets.EditBox, "Skirmish size: full, half, quarter or flashpoint (-acrSize)")]
 string m_sAcrSize;
 [Attribute("", UIWidgets.EditBox, "FIA pockets of resistance on whole/half maps: few (default), many or off (-acrPockets)")]
 string m_sAcrPockets;
 [Attribute("", UIWidgets.EditBox, "Quick start, the map already split between the sides: off (default), even or uneven (-acrQuickStart)")]
 string m_sAcrQuickStart;
 [Attribute("", UIWidgets.EditBox, "FIA guerrillas (road ambushes, houses) on whole/half maps: few (default), many or off (-acrGuerrillas)")]
 string m_sAcrGuerrillas;
 [Attribute("", UIWidgets.EditBox, "Hidden FIA: 1 = no messages about FIA; pockets are only found by entering them (-acrFIAHidden)")]
 string m_sAcrFIAHidden;
 [Attribute("", UIWidgets.EditBox, "Match type: standard (default, two armies) or coin (counter-insurgency: one army against the FIA insurgency) (-acrMatchType)")]
 string m_sAcrMatchType;
 [Attribute("", UIWidgets.EditBox, "Counter-insurgency: which side is the army, west (default) or east (-acrCoinArmy)")]
 string m_sAcrCoinArmy;
 [Attribute("", UIWidgets.EditBox, "Counter-insurgency: who runs the insurgency, rules (default), players or ai (-acrCoinInsurgency)")]
 string m_sAcrCoinInsurgency;
 [Attribute("", UIWidgets.EditBox, "Counter-insurgency: faction key holding the insurgent role (default FIA) (-acrCoinInsurgent)")]
 string m_sAcrCoinInsurgent;
 [Attribute("", UIWidgets.EditBox, "Counter-insurgency: insurgent town-support income multiplier (default 1, 0 = base income) (-acrCoinInsurgentIncome)")]
 string m_sAcrCoinInsurgentIncome;
 [Attribute("", UIWidgets.EditBox, "Counter-insurgency: insurgency strength, few, many (default) or heavy (every growth cap x1.5) (-acrCoinStrength)")]
 string m_sAcrCoinStrength;
 [Attribute("", UIWidgets.EditBox, "Counter-insurgency: the AI army's posture, auto (default: Blue side airmobile, Red side garrison and convoy), airmobile or convoy (-acrCoinPosture)")]
 string m_sAcrCoinPosture;
 [Attribute("", UIWidgets.EditBox, "Counter-insurgency: share of a Hostile town's income the army still earns, 0 to 1 (default 0.5) (-acrCoinHostileIncome)")]
 string m_sAcrCoinHostileIncome;
 [Attribute("", UIWidgets.EditBox, "Zombies with Project Outbreak loaded: off (default), few or many nests in open bases and the country (-acrZombies)")]
 string m_sAcrZombies;
 [Attribute("", UIWidgets.EditBox, "Zombie mode with Project Outbreak loaded: tvtvz (default, both sides + zombies) or tvz (the players' side alone against the zombies) (-acrZombieMode)")]
 string m_sAcrZombieMode;
 [Attribute("", UIWidgets.EditBox, "Outbreak start with Project Outbreak loaded: nests (default, nests across the map) or contain (one ground zero that spreads, survivors to rescue) (-acrOutbreak)")]
 string m_sAcrOutbreak;
 [Attribute("", UIWidgets.EditBox, "Who FIA sides with: hostile (both, default), west or east (-acrFIAAllegiance)")]
 string m_sAcrFIAAllegiance;
 [Attribute("", UIWidgets.EditBox, "FIA leans by conduct: 1 = each side's standing with FIA rises and falls with how it treats FIA (-acrFIALeaning)")]
 string m_sAcrFIALeaning;
 [Attribute("", UIWidgets.EditBox, "FIA spoils: 0 = FIA don't loot the fallen, steal vehicles or take prisoners (default 1) (-acrFIASpoils)")]
 string m_sAcrFIASpoils;
 [Attribute("", UIWidgets.EditBox, "FIA grievance: 0 = harm to civilians and towns doesn't grow FIA's recruiting (default 1) (-acrFIAGrievance)")]
 string m_sAcrFIAGrievance;
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
 [Attribute("", UIWidgets.EditBox, "AI commander: new (default: lanes, job allocator, defence forecast, covered fall-back) or classic (as before 0.6.0) (-acrAICommander)")]
 string m_sAcrAICommander;
 [Attribute("", UIWidgets.EditBox, "Blue side AI doctrine: random, balanced, blitz, firepower, fortress or raider (-acrDoctrineWest)")]
 string m_sAcrDoctrineWest;
 [Attribute("", UIWidgets.EditBox, "Red side AI doctrine: random, balanced, blitz, firepower, fortress or raider (-acrDoctrineEast)")]
 string m_sAcrDoctrineEast;
 [Attribute("", UIWidgets.EditBox, "AI commanders change doctrine when theirs isn't working: 1 (default) or 0 (-acrDoctrineAdapt)")]
 string m_sAcrDoctrineAdapt;
 [Attribute("", UIWidgets.EditBox, "Blue side company, its starting kit: rifle (default), mechanized, weapons, engineers or recon (-acrCompanyWest)")]
 string m_sAcrCompanyWest;
 [Attribute("", UIWidgets.EditBox, "Red side company, its starting kit: rifle (default), mechanized, weapons, engineers or recon (-acrCompanyEast)")]
 string m_sAcrCompanyEast;
 [Attribute("-1", UIWidgets.EditBox, "AI commander squad cap per side (-acrUnitCap)")]
 int m_iAcrUnitCap;
 [Attribute("-1", UIWidgets.EditBox, "Generated layout seed (-acrSeed); -1 or 0 = a new random layout each match")]
 int m_iAcrSeed;
 [Attribute("-1", UIWidgets.EditBox, "Living AI soldiers per side (-acrSoldierCap)")]
 int m_iAcrSoldierCap;
 [Attribute("-1", UIWidgets.EditBox, "Scale the AI squad cap and soldier cap with the map's bases / size: 0 default (off), 1 on (-acrCapScale)")]
 int m_iAcrCapScale;
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
 [Attribute("", UIWidgets.EditBox, "Setup preset loaded at the start of every match, by name, from the server profile's ACA/SetupPresets or built in (Default, Large battle); launch parameters still beat it (-acrPreset)")]
 string m_sAcrPreset;

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
 [Attribute("-1", UIWidgets.EditBox, "Gunners stay on the static guns of a spot their squad holds: 1 default, 0 vanilla (-acrHoldGuns)")]
 int m_iAcrHoldGuns;
 [Attribute("-1", UIWidgets.EditBox, "The calm command map: 1 default, 0 the classic map (-acrMapDeclutter)")]
 int m_iAcrMapDeclutter;
 [Attribute("-1", UIWidgets.EditBox, "Mission intro (orbit of the HQ, title, flight into the player): 1 default, 0 off (-acrIntro)")]
 int m_iAcrIntro;
 [Attribute("", UIWidgets.EditBox, "Mission intro music: {GUID}path.acp:EVENT, or off; blank = Cliffhanger (-acrIntroMusic)")]
 string m_sAcrIntroMusic;

 [Attribute("", UIWidgets.EditBox, "Mission intro music when the match has zombies: {GUID}path.acp:EVENT, or off; blank = Lost As Shit (-acrIntroMusicZombies)")]
 string m_sAcrIntroMusicZombies;
 [Attribute("-1", UIWidgets.EditBox, "AI commanders lay minefields: 0 default, 1 enables (-acrMinesAI)")]
 int m_iAcrMinesAI;
 [Attribute("-1", UIWidgets.EditBox, "Commander Fortify orders: 1 default, 0 disables (-acrFortify)")]
 int m_iAcrFortify;
 [Attribute("-1", UIWidgets.EditBox, "AI commanders fortify: 0 default, 1 enables (-acrFortifyAI)")]
 int m_iAcrFortifyAI;
 [Attribute("-1", UIWidgets.EditBox, "Fortify on the middle of a base fortifies the town: 1 default, 0 disables (-acrBaseWorks)")]
 int m_iAcrBaseWorks;
 [Attribute("-1", UIWidgets.EditBox, "Living sides' HQs start with tier-1 fortifications and site dressing: 1 default, 0 disables (-acrHQWorks)")]
 int m_iAcrHQWorks;
 [Attribute("-1", UIWidgets.EditBox, "A fortified town's long lines step in and out with fire bays: 1 default, 0 disables (-acrBaseBays)")]
 int m_iAcrBaseBays;
 [Attribute("-1", UIWidgets.EditBox, "A small gun position takes one piece of a line where a big nest has no room: 1 default, 0 disables (-acrBaseSmallGuns)")]
 int m_iAcrBaseSmallGuns;
 [Attribute("-1", UIWidgets.EditBox, "New Fortify positions and squad-built checkpoints get sandbag wings: 1 default, 0 disables (-acrWings)")]
 int m_iAcrWings;
 [Attribute("-1", UIWidgets.EditBox, "Each fortified town gets a defence style from its ground: 1 default, 0 makes every town a wall (-acrBaseStyles)")]
 int m_iAcrBaseStyles;
 [Attribute("-1", UIWidgets.EditBox, "A fortified town's lines keep out of yards and off squares, and its guns look for the ways in: 1 default, 0 the older placement (-acrBasePlacement)")]
 int m_iAcrBasePlacement;
 [Attribute("-1", UIWidgets.EditBox, "Guns stand in emplacements built from parts as well as the game's ready-made nests: 1 default, 0 ready-made only (-acrEmplacements)")]
 int m_iAcrEmplacements;
 [Attribute("-1", UIWidgets.EditBox, "A Fortify position's reserve goes behind its wings when an enemy comes: 1 default, 0 stays round the position. -1 leaves it to the command line.")]
 int m_iAcrWingMen;
 [Attribute("-1", UIWidgets.EditBox, "Men with a place behind bags hold it in a fight and shoot over them: 1 default, 0 the game's own movement. -1 leaves it to the command line.")]
 int m_iAcrBagHold;
 [Attribute("-1", UIWidgets.EditBox, "Lines have stretches of low or netted bags and dirt in front, not only plain high bags: 1 default, 0 plain. -1 leaves it to the command line.")]
 int m_iAcrBaseDress;
 [Attribute("-1", UIWidgets.EditBox, "Hedgehogs or log barricades out in front of a line either side of its road: 1 default, 0 only wire. -1 leaves it to the command line.")]
 int m_iAcrBaseObstacles;
 [Attribute("-1", UIWidgets.EditBox, "A town's lines go round the town by the cheapest way that closes it: 1 default, 0 walked out from seeds. -1 leaves it to the command line (-acrBaseRing)")]
 int m_iAcrBaseRing;
 [Attribute("-1", UIWidgets.EditBox, "A town's bags and guns are chosen by what they do to the enemy on the ways he would walk: 1 default, 0 by threat per sector and view. -1 leaves it to the command line (-acrKillGround)")]
 int m_iAcrKillGround;
 [Attribute("-1", UIWidgets.EditBox, "Once a town's works are planned the enemy is walked in against them once more and the last pieces go on his softest way: 1 default, 0 not. -1 leaves it to the command line (-acrBaseFinalWalk)")]
 int m_iAcrBaseFinalWalk;
 [Attribute("-1", UIWidgets.EditBox, "Defenders in reserve at a Fortify tier I position: 1 default, 0 none (-acrFortifyDefenders1)")]
 int m_iAcrFortifyDefenders1;
 [Attribute("-1", UIWidgets.EditBox, "Defenders in reserve at a Fortify tier II position: 2 default, 0 none (-acrFortifyDefenders2)")]
 int m_iAcrFortifyDefenders2;
 [Attribute("-1", UIWidgets.EditBox, "Defenders in reserve at a Fortify tier III position: 4 default, 0 none (-acrFortifyDefenders3)")]
 int m_iAcrFortifyDefenders3;
 [Attribute("-1", UIWidgets.EditBox, "Defenders in reserve at a checkpoint: 0 default (none), at most 8 (-acrRoadblockDefenders)")]
 int m_iAcrRoadblockDefenders;
 [Attribute("-1", UIWidgets.EditBox, "AI commanders set this percent of income aside for building: 0 default (off), at most 30 (-acrAIBuildShare)")]
 int m_iAcrAIBuildShare;
 // Out of Experimental, on by default (19 Nov): a console or rented host turns them off here.
 [Attribute("-1", UIWidgets.EditBox, "AC: Actual's period jets over recon flights and bomb runs: 1 default without RHS (off with RHS), 0 off (-acrPeriodJets)")]
 int m_iAcrPeriodJets;
 [Attribute("-1", UIWidgets.EditBox, "Built guns, checkpoint posts and Fortify pieces clear the bushes round them: 1 default, 0 off (-acrFieldsOfFire)")]
 int m_iAcrFieldsOfFire;
 [Attribute("-1", UIWidgets.EditBox, "A squad told to get out on a road has its driver pull onto the verge first: 1 default, 0 off (-acrPullOver)")]
 int m_iAcrPullOver;
 [Attribute("-1", UIWidgets.EditBox, "Second-in-command: 1 default (the 2IC runs the squads nobody leads), 2 I'm the 2IC (the AI commander runs the side), 0 off (-acr2IC)")]
 int m_iAcr2IC;

 void Fill(map<string, string> values)
 {
  Text(values, "acrLayout", m_sAcrLayout);
  Text(values, "acrSize", m_sAcrSize);
  Text(values, "acrPockets", m_sAcrPockets);
  Text(values, "acrQuickStart", m_sAcrQuickStart);
  Text(values, "acrGuerrillas", m_sAcrGuerrillas);
  Text(values, "acrFIAHidden", m_sAcrFIAHidden);
  Text(values, "acrMatchType", m_sAcrMatchType);
  Text(values, "acrCoinArmy", m_sAcrCoinArmy);
  Text(values, "acrCoinInsurgency", m_sAcrCoinInsurgency);
  Text(values, "acrCoinInsurgent", m_sAcrCoinInsurgent);
  Text(values, "acrCoinInsurgentIncome", m_sAcrCoinInsurgentIncome);
  Text(values, "acrCoinStrength", m_sAcrCoinStrength);
  Text(values, "acrCoinPosture", m_sAcrCoinPosture);
  Text(values, "acrCoinHostileIncome", m_sAcrCoinHostileIncome);
  Text(values, "acrZombies", m_sAcrZombies);
  Text(values, "acrZombieMode", m_sAcrZombieMode);
  Text(values, "acrOutbreak", m_sAcrOutbreak);
  Text(values, "acrFIAAllegiance", m_sAcrFIAAllegiance);
  Text(values, "acrFIALeaning", m_sAcrFIALeaning);
  Text(values, "acrFIASpoils", m_sAcrFIASpoils);
  Text(values, "acrFIAGrievance", m_sAcrFIAGrievance);
  Text(values, "acrFactions", m_sAcrFactions);
  Text(values, "acrPlayers", m_sAcrPlayers);
  Text(values, "acrThreat", m_sAcrThreat);
  Text(values, "acrPricing", m_sAcrPricing);
  Text(values, "acrAI", m_sAcrAI);
  Text(values, "acrAICommander", m_sAcrAICommander);
  Text(values, "acrDoctrineWest", m_sAcrDoctrineWest);
  Text(values, "acrDoctrineEast", m_sAcrDoctrineEast);
  Text(values, "acrDoctrineAdapt", m_sAcrDoctrineAdapt);
  Text(values, "acrCompanyWest", m_sAcrCompanyWest);
  Text(values, "acrCompanyEast", m_sAcrCompanyEast);
  Number(values, "acrUnitCap", m_iAcrUnitCap);
  Number(values, "acrSeed", m_iAcrSeed);
  Number(values, "acrSoldierCap", m_iAcrSoldierCap);
  Number(values, "acrCapScale", m_iAcrCapScale);
  Number(values, "acrStartRP", m_iAcrStartRP);
  if (m_fAcrIncome >= 0) values.Set("acrIncome", m_fAcrIncome.ToString());
  Number(values, "acrSkill", m_iAcrSkill);
  if (m_fAcrReaction >= 0) values.Set("acrReaction", m_fAcrReaction.ToString());
  Number(values, "acrSharedCommand", m_iAcrSharedCommand);
  Number(values, "acrRespawnSeconds", m_iAcrRespawnSeconds);
  Number(values, "acrGameMasterAll", m_iAcrGameMasterAll);
  Number(values, "acrMarksmen", m_iAcrMarksmen);
  Number(values, "acrSetup", m_iAcrSetup);
  Text(values, "acrPreset", m_sAcrPreset);
  Text(values, "acrDeployment", m_sAcrDeployment);
  Text(values, "acrReinforcement", m_sAcrReinforcement);
  Text(values, "acrFobServices", m_sAcrFobServices);
  Number(values, "acrFobRelay", m_iAcrFobRelay);
  Number(values, "acrDepotCapacity", m_iAcrDepotCapacity);
  Number(values, "acrServiceSupplyCost", m_iAcrServiceSupplyCost);
  Number(values, "acrFobRPServiceCost", m_iAcrFobRPServiceCost);
  Number(values, "acrMines", m_iAcrMines);
  Number(values, "acrMinesRealistic", m_iAcrMinesRealistic);
  Number(values, "acrHoldGuns", m_iAcrHoldGuns);
  Number(values, "acrMapDeclutter", m_iAcrMapDeclutter);
  Number(values, "acrIntro", m_iAcrIntro);
  Text(values, "acrIntroMusic", m_sAcrIntroMusic);
  Text(values, "acrIntroMusicZombies", m_sAcrIntroMusicZombies);
  Number(values, "acrMinesAI", m_iAcrMinesAI);
  Number(values, "acrFortify", m_iAcrFortify);
  Number(values, "acrFortifyAI", m_iAcrFortifyAI);
  Number(values, "acrBaseWorks", m_iAcrBaseWorks);
  Number(values, "acrHQWorks", m_iAcrHQWorks);
  Number(values, "acrBaseBays", m_iAcrBaseBays);
  Number(values, "acrBaseSmallGuns", m_iAcrBaseSmallGuns);
  Number(values, "acrWings", m_iAcrWings);
  Number(values, "acrBaseStyles", m_iAcrBaseStyles);
  Number(values, "acrBasePlacement", m_iAcrBasePlacement);
  Number(values, "acrEmplacements", m_iAcrEmplacements);
  Number(values, "acrWingMen", m_iAcrWingMen);
  Number(values, "acrBagHold", m_iAcrBagHold);
  Number(values, "acrBaseDress", m_iAcrBaseDress);
  Number(values, "acrBaseObstacles", m_iAcrBaseObstacles);
  Number(values, "acrBaseRing", m_iAcrBaseRing);
  Number(values, "acrKillGround", m_iAcrKillGround);
  Number(values, "acrBaseFinalWalk", m_iAcrBaseFinalWalk);
  Number(values, "acrFortifyDefenders1", m_iAcrFortifyDefenders1);
  Number(values, "acrFortifyDefenders2", m_iAcrFortifyDefenders2);
  Number(values, "acrFortifyDefenders3", m_iAcrFortifyDefenders3);
  Number(values, "acrRoadblockDefenders", m_iAcrRoadblockDefenders);
  Number(values, "acrAIBuildShare", m_iAcrAIBuildShare);
  Number(values, "acrPeriodJets", m_iAcrPeriodJets);
  Number(values, "acrFieldsOfFire", m_iAcrFieldsOfFire);
  Number(values, "acrPullOver", m_iAcrPullOver);
  Number(values, "acr2IC", m_iAcr2IC);
 }

 protected static void Text(map<string, string> values, string name, string value)
 {
  string trimmed = value;
  trimmed.TrimInPlace();
  if (trimmed != "") values.Set(name, trimmed);
 }
 protected static void Number(map<string, string> values, string name, int value)
 {
  if (value >= 0) values.Set(name, value.ToString());
 }
}
