// Counter-insurgency (COIN, 0.8.0 #244): one army against the FIA insurgency, with no second army.
//  - Match type: -acrMatchType standard|coin (setup row "Match type", Match tab); the army is one of
//    the two selected factions, -acrCoinArmy west|east (row "Army", shown in COIN), so any faction
//    the match can field can be the army. The other faction is absent (ACR_OneSide): its bases open,
//    its units and spawn points go, every player joins the army.
//  - The absent army's HQ becomes the insurgency's stronghold: a large FIA pocket (STRONGHOLD_MEN)
//    with a depot (ACR_Depots.Apply). Pockets then take more of the open towns as usual.
//  - More FIA than a normal match: pockets, cells and hideouts x FIA_SCALE, and the FIA rows default
//    to "many"; "off" plays as "few" (a COIN match always has an insurgency). The "FIA sides with"
//    and "FIA leans by conduct" rows hide: FIA is the army's enemy (ACR_Insurgency.AllyOption /
//    LeaningOption) and town support (ACR_TownSupport) takes their place.
//  - Not on flashpoints, and only where the FIA faction exists and isn't one of the two sides.
//  - "Insurgency run by" (rules / AI commander / players) arrives with the insurgent seat (#97):
//    until then the insurgency always runs on its own rules.
class ACR_Coin
{
 static const float FIA_SCALE = 1.5;        // pockets, cells and hideouts against a normal match
 static const int STRONGHOLD_MIN = 16;      // the stronghold's garrison
 static const int STRONGHOLD_MAX = 20;
 static const float HOSTILE_INCOME = 0.5;   // a Hostile town pays the army this share (host option)
 protected static ACR_Base s_Stronghold;
 protected static bool s_bApplied;

 static void Reset()
 {
  s_Stronghold = null;
  s_bApplied = false;
 }

 // -acrMatchType as asked ("standard" unless "coin").
 static string MatchOption()
 {
  string value = ACR_TestHooks.Get().MatchType();
  if (value == "" && !ACR_Options.Get("acrMatchType", value)) value = "";
  value.ToLower();
  if (value == "coin") return value;
  return "standard";
 }

 // COIN is being played: asked for, not a flashpoint, FIA in the world and not one of the sides.
 static bool On()
 {
  if (MatchOption() != "coin") return false;
  string fia = ACR_Pockets.FACTION;
  if (ACR_FactionSelection.West() == fia || ACR_FactionSelection.East() == fia) return false;
  FactionManager factions = GetGame().GetFactionManager();
  if (!factions || !factions.GetFactionByKey(fia)) return false;
  int size = ACR_Skirmish.SizeOption();
  if (ACR_Skirmish.s_Active) size = ACR_Skirmish.s_Active.m_eSize;
  return size != ACR_ESkirmishSize.FLASHPOINT;
 }

 // The army row: "west" / "east" (or a faction key of the pair), else -acrPlayers, else "west".
 static string ArmyOption()
 {
  string value;
  if (ACR_Options.Get("acrCoinArmy", value) && !value.IsEmpty()) return value;
  if (ACR_Options.Get("acrPlayers", value) && !value.IsEmpty()) return value;
  return "west";
 }

 // The army's faction key ("" outside COIN).
 static string Army()
 {
  if (!On()) return "";
  return ACR_OneSide.Living();
 }

 static bool IsArmy(string factionKey)
 {
  return On() && !factionKey.IsEmpty() && factionKey == ACR_OneSide.Living();
 }

 static ACR_Base Stronghold() { return s_Stronghold; }

 // The AI army's posture (#244 ruling 6; -acrCoinPosture auto|airmobile|convoy, row "AI posture"):
 // Airmobile (the Blue side's default: US factions) keeps few town posts and a bigger reserve that
 // moves by air lift; Garrison and convoy (the Red side's default: Soviet factions) keeps more posts
 // and sends an escort with every aid truck. Read by ACR_AICommanderSupport.
 static const int AIRMOBILE = 0;
 static const int CONVOY = 1;
 static string PostureOption()
 {
  string value;
  if (!ACR_Options.Get("acrCoinPosture", value)) value = "";
  value.ToLower();
  if (value == "airmobile" || value == "convoy") return value;
  return "auto";
 }

 static int Posture(string faction)
 {
  string value = PostureOption();
  if (value == "airmobile") return AIRMOBILE;
  if (value == "convoy") return CONVOY;
  // auto: by the side's slot, not the faction's name (the Blue slot holds the US-style factions).
  if (faction == ACR_FactionSelection.East()) return CONVOY;
  return AIRMOBILE;
 }

 static string PostureName(int posture)
 {
  if (posture == CONVOY) return "garrison and convoy";
  return "airmobile";
 }

 // Towns the AI army keeps a squad in.
 static int TownPosts(string faction)
 {
  if (Posture(faction) == CONVOY) return 4;
  return 2;
 }

 // Free squads it never sends on a COIN job: the reaction force.
 static int ReserveKeep(string faction)
 {
  if (Posture(faction) == CONVOY) return 1;
  return 2;
 }

 // "Insurgency strength" (-acrCoinStrength few|many|heavy, row under Match type, default many):
 // what the FIA rows default to, and (heavy) every growth cap x HEAVY_CAPS (ACR_FIAGrowth).
 static const float HEAVY_CAPS = 1.5;
 static string StrengthOption()
 {
  string value;
  if (!ACR_Options.Get("acrCoinStrength", value)) value = "";
  value.ToLower();
  if (value == "few" || value == "heavy") return value;
  return "many";
 }

 static float CapScale()
 {
  if (On() && StrengthOption() == "heavy") return HEAVY_CAPS;
  return 1;
 }

 // A FIA row's level in COIN: the strength's when unset ("few" or "many"), "few" for "off".
 static string FIALevel(string value)
 {
  string lower = value;
  lower.ToLower();
  if (lower == "" && StrengthOption() == "few") return "few";
  if (lower == "") return "many";
  if (lower == "off" || lower == "0") return "few";
  return lower;
 }

 // A FIA count, scaled up in COIN.
 static int Scale(int count)
 {
  if (count <= 0 || !On()) return count;
  return Math.Ceil(count * FIA_SCALE);
 }

 // Share of a Hostile town's value the army earns (-acrCoinHostileIncome, 0..1, default 0.5).
 static float HostileIncome()
 {
  string value;
  if (!ACR_Options.Get("acrCoinHostileIncome", value) || value.IsEmpty()) return HOSTILE_INCOME;
  return Math.Clamp(value.ToFloat(), 0, 1);
 }

 // Server, right after ACR_OneSide.Apply opened the absent army's ground, before the pockets
 // (ACR_Skirmish.ApplyOnce): its HQ becomes the stronghold.
 static void Apply()
 {
  if (s_bApplied || !On()) return;
  s_bApplied = true;
  ACR_Base hq = ACR_OneSide.AbsentHQ();
  string stronghold = "none";
  if (hq && !hq.m_bRetired && hq.m_sOwner.IsEmpty())
  {
   s_Stronghold = hq;
   ACR_Pockets.Add(hq, Math.RandomIntInclusive(STRONGHOLD_MIN, STRONGHOLD_MAX));
   stronghold = string.Format("%1 (%2 men)", hq.m_sName, hq.m_iGarrison);
  }
  float far = 0;
  ACR_Base home = ACR_OneSide.Home();
  if (home && s_Stronghold) far = vector.DistanceXZ(home.GetOrigin(), s_Stronghold.GetOrigin());
  PrintFormat("[ACR] COIN %1 against the insurgency (%2 absent): stronghold %3, %4 m from the army's HQ; FIA x%5", ACR_OneSide.Living(), ACR_OneSide.Absent(), stronghold, (int)far, FIA_SCALE);
  ACR_InsurgentSide.Apply();
 }
}
