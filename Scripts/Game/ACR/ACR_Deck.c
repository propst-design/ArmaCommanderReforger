// Deck builder (#106): a side's buy list is a deck of cards picked from the faction's whole
// catalogue (today's list, extra kitted teams and the player's own custom teams) within a points
// budget. The deck's cards are the buy list in the match; the buy menu itself is unchanged.
// Shared by server (ACR_Decks, ACR_DeckCatalog) and clients (ACR_DeckEditor, ACR_DeckStore).

// What a card is, for grouping in the editor and for the AI's pick.
enum ACR_EDeckRole
{
 LOGISTICS, // service, construction and transport trucks
 INFANTRY,  // squads and fire teams
 TEAMS,     // machine gun, anti-tank, engineer and medical teams
 RECON,     // scouts and snipers
 SF,        // special forces
 VEHICLES,  // cars and armed trucks
 CARRIERS,  // armed APCs and IFVs
 ARMOUR,    // tanks and other armour
 SUPPORT,   // mortar team, forward observers
 AIR        // helicopters and aircraft
}

// Card flags sent to the editor with the catalogue.
class ACR_DeckFlag
{
 static const int STANDARD = 1; // in the faction's standard deck (today's list)
 static const int LOCKED = 2;   // always in every deck, free (service and construction trucks)
 static const int EXTRA = 4;    // a kitted card that isn't in today's list
 static const int RARE = 8;     // fixed stock (aircraft, armour, special forces, supports)
 static const int SF = 16;      // special forces: the AI fields one at a time
 static const int TRANSPORT = 32; // a vehicle a custom team can ride in
}

class ACR_DeckRules
{
 static const string FORMAT = "ACADECK1";
 // Points: a card's base price in RP over this (a squad of 24 RP is 8 points, a 76 RP tank 25:
 // Jacob's placeholder ratio in #106), at least 1. Locked cards are free.
 static const float RP_PER_POINT = 3;
 static const float SF_POINTS = 1.5;   // x for special forces: few, but the best men on the map
 // The budget for a deck a player builds and for the AI's own. Today's lists come to 190-340 points
 // (srv:deck logs each: US 293, USSR 343, FIA 189), so #106's placeholder 50/100/200 would have cut
 // them in half; the standard deck (today's list) is never trimmed, and these leave a built deck
 // room for most of what it would want while still making it choose.
 static const int BUDGET_LOW = 100;
 static const int BUDGET_NORMAL = 150;
 static const int BUDGET_HIGH = 250;
 static const int MAX_CARDS = 60;      // the buy menu holds 100 (ACR_EPanelAction), supports included
 static const int MAX_CUSTOM = 12;     // custom cards per deck
 static const int CUSTOM_MIN_MEN = 2;
 static const int CUSTOM_MAX_MEN = 12;
 static const int NAME_CHARS = 28;
 static const int CUSTOM_STOCK = 4;    // as the curated teams (ACR_InfantryCatalog.Add)

 static bool Enabled()
 {
  return ACR_LogisticsRules.Choice("acrDecks", "1") != "0";
 }

 // The points a side may spend: the setup row, else by threat level.
 static int Budget()
 {
  string value = ACR_LogisticsRules.Choice("acrDeckPoints", "");
  if (value != "" && value != "auto")
  {
   int fixed = value.ToInt(-1);
   if (fixed == 0) return 0; // unlimited
   if (fixed > 0) return fixed;
  }
  int threat = ACR_Manager.ThreatOption();
  if (threat == 0) return BUDGET_LOW;
  if (threat == 2) return BUDGET_HIGH;
  return BUDGET_NORMAL;
 }

 static string BudgetName(int budget)
 {
  if (budget <= 0) return "unlimited";
  return budget.ToString();
 }

 static bool Locked(ACR_RequisitionEntry entry)
 {
  if (!entry || entry.m_sVehicle.IsEmpty() || entry.m_bAircraft || entry.m_iSupportHeli > 0) return false;
  return ACR_Logistics.ServiceOfName(entry.m_sVehicle) != 0;
 }

 static int Points(ACR_RequisitionEntry entry)
 {
  if (!entry || Locked(entry)) return 0;
  int price = entry.m_iBaseCost;
  if (price <= 0) price = entry.m_iCost;
  return PointsOfPrice(price, entry.m_eFormation == ACR_EFormation.SF);
 }

 // The editor prices custom cards with this too (the server's ACR_DeckCatalog.CustomEntry decides).
 static int PointsOfPrice(int price, bool sf)
 {
  float points = price / RP_PER_POINT;
  if (sf) points *= SF_POINTS;
  int rounded = (int)Math.Round(points);
  if (rounded < 1) return 1;
  return rounded;
 }

 static int RoleOf(ACR_RequisitionEntry entry)
 {
  if (!entry) return ACR_EDeckRole.INFANTRY;
  if (entry.m_iSupportHeli > 0 || entry.m_bAircraft) return ACR_EDeckRole.AIR;
  if (entry.m_bMortarTeam || entry.m_bObserverTeam) return ACR_EDeckRole.SUPPORT;
  if (!entry.m_sVehicle.IsEmpty() && entry.m_sGroup.IsEmpty())
  {
   if (ACR_Logistics.ServiceOfName(entry.m_sVehicle) != 0 || entry.m_bTransport) return ACR_EDeckRole.LOGISTICS;
   if (entry.m_bCarrier) return ACR_EDeckRole.CARRIERS;
   if (entry.m_bRare) return ACR_EDeckRole.ARMOUR;
   return ACR_EDeckRole.VEHICLES;
  }
  if (entry.m_eFormation == ACR_EFormation.SF) return ACR_EDeckRole.SF;
  if (entry.m_eFormation == ACR_EFormation.SCOUT || entry.m_eFormation == ACR_EFormation.SNIPER) return ACR_EDeckRole.RECON;
  if (entry.m_eFormation == ACR_EFormation.MG || entry.m_eFormation == ACR_EFormation.AT || entry.m_eFormation == ACR_EFormation.ENGINEER || entry.m_eFormation == ACR_EFormation.MEDIC) return ACR_EDeckRole.TEAMS;
  if (entry.m_eFormation != ACR_EFormation.NONE) return ACR_EDeckRole.INFANTRY;
  // A Game Master group: by its name.
  string name = entry.m_sName;
  name.ToLower();
  if (name.Contains("special") || name.Contains("spec ops") || name.Contains("spetsnaz")) return ACR_EDeckRole.SF;
  if (name.Contains("sniper") || name.Contains("recon") || name.Contains("scout") || name.Contains("sharpshooter")) return ACR_EDeckRole.RECON;
  if (name.Contains("machine") || name.Contains("mg ") || name.EndsWith(" mg") || name.Contains("anti-tank") || name.Contains("anti tank") || name.Contains(" at ") || name.EndsWith(" at") || name.Contains("medic") || name.Contains("engineer") || name.Contains("sapper") || name.Contains("weapons")) return ACR_EDeckRole.TEAMS;
  return ACR_EDeckRole.INFANTRY;
 }

 static string RoleName(int role)
 {
  switch (role)
  {
   case ACR_EDeckRole.LOGISTICS: return "Logistics";
   case ACR_EDeckRole.INFANTRY: return "Infantry";
   case ACR_EDeckRole.TEAMS: return "Weapons teams";
   case ACR_EDeckRole.RECON: return "Recon";
   case ACR_EDeckRole.SF: return "Special forces";
   case ACR_EDeckRole.VEHICLES: return "Vehicles";
   case ACR_EDeckRole.CARRIERS: return "APCs and IFVs";
   case ACR_EDeckRole.ARMOUR: return "Armour";
   case ACR_EDeckRole.SUPPORT: return "Support";
   case ACR_EDeckRole.AIR: return "Air";
  }
  return "Other";
 }

 static int RoleCount() { return 10; }

 // A name typed by a player: no separators of the deck code, trimmed, capped.
 static string CleanName(string name)
 {
  string clean = name;
  clean.Replace("|", "");
  clean.Replace("^", "");
  clean.Replace("~", "");
  clean.Replace("!", "");
  clean.Replace(",", "");
  clean.Replace("\t", " ");
  clean.Replace("\n", " ");
  clean.Replace("\r", "");
  clean = clean.Trim();
  if (clean.Length() > NAME_CHARS) clean = clean.Substring(0, NAME_CHARS).Trim();
  return clean;
 }

 // ---------------------------------------------------------------- soldiers for custom cards
 // A soldier's RP in a custom team, by role (close to the curated teams' prices: an MG team of
 // leader, gunner and assistant is 12 RP, as ACR_InfantryCatalog's).
 static int SoldierRP(int role)
 {
  switch (role)
  {
   case ACR_EUARole.LEADER: return 4;
   case ACR_EUARole.MG: return 5;
   case ACR_EUARole.AR: return 4;
   case ACR_EUARole.GL: return 4;
   case ACR_EUARole.LAT: return 6;
   case ACR_EUARole.MARKSMAN: return 5;
   case ACR_EUARole.UNARMED: return 2;
   case ACR_EUARole.ENGINEER: return 4;
   case ACR_EUARole.LIGHT_AT: return 4;
   case ACR_EUARole.SNIPER: return 8;
   case ACR_EUARole.SPOTTER: return 6;
  }
  return 3; // rifleman, assistants, medic, crew
 }

 static string SoldierRoleName(int role)
 {
  switch (role)
  {
   case ACR_EUARole.RIFLEMAN: return "Rifleman";
   case ACR_EUARole.LEADER: return "Leader";
   case ACR_EUARole.MG: return "Machine gunner";
   case ACR_EUARole.AR: return "Automatic rifleman";
   case ACR_EUARole.GL: return "Grenadier";
   case ACR_EUARole.LAT: return "Anti-tank";
   case ACR_EUARole.MARKSMAN: return "Marksman";
   case ACR_EUARole.CREW: return "Crewman";
   case ACR_EUARole.PILOT: return "Pilot";
   case ACR_EUARole.UNARMED: return "Unarmed";
   case ACR_EUARole.MG_ASSISTANT: return "MG assistant";
   case ACR_EUARole.AT_ASSISTANT: return "AT assistant";
   case ACR_EUARole.MEDIC: return "Medic";
   case ACR_EUARole.ENGINEER: return "Engineer";
   case ACR_EUARole.LIGHT_AT: return "Light anti-tank";
   case ACR_EUARole.SNIPER: return "Sniper";
   case ACR_EUARole.SPOTTER: return "Spotter";
  }
  return "Soldier";
 }

 // A vanilla soldier's role from its prefab name (Character_US_MG, Character_USSR_Sharpshooter ...).
 static int RoleFromPrefab(string prefab)
 {
  string leaf = FilePath.StripExtension(FilePath.StripPath(prefab));
  leaf.ToLower();
  leaf += "_";
  if (leaf.Contains("sniper")) return ACR_EUARole.SNIPER;
  if (leaf.Contains("spotter")) return ACR_EUARole.SPOTTER;
  if (leaf.Contains("sharpshooter") || leaf.Contains("marksman") || leaf.Contains("_dmr_")) return ACR_EUARole.MARKSMAN;
  if (leaf.Contains("medic")) return ACR_EUARole.MEDIC;
  if (leaf.Contains("engineer") || leaf.Contains("sapper")) return ACR_EUARole.ENGINEER;
  if (leaf.Contains("_amg_") || leaf.Contains("mgassistant") || leaf.Contains("_ammo")) return ACR_EUARole.MG_ASSISTANT;
  if (leaf.Contains("atassistant") || leaf.Contains("_aat_")) return ACR_EUARole.AT_ASSISTANT;
  if (leaf.Contains("_mg_") || leaf.Contains("machinegun")) return ACR_EUARole.MG;
  if (leaf.Contains("_ar_")) return ACR_EUARole.AR;
  if (leaf.Contains("_gl_") || leaf.Contains("grenadier")) return ACR_EUARole.GL;
  if (leaf.Contains("_lat_") || leaf.Contains("lightat")) return ACR_EUARole.LIGHT_AT;
  if (leaf.Contains("_at_") || leaf.Contains("antitank") || leaf.Contains("_rpg")) return ACR_EUARole.LAT;
  if (leaf.Contains("_sl_") || leaf.Contains("_tl_") || leaf.Contains("leader") || leaf.Contains("officer") || leaf.Contains("commander")) return ACR_EUARole.LEADER;
  if (leaf.Contains("crew")) return ACR_EUARole.CREW;
  if (leaf.Contains("pilot")) return ACR_EUARole.PILOT;
  if (leaf.Contains("unarmed")) return ACR_EUARole.UNARMED;
  return ACR_EUARole.RIFLEMAN;
 }

 // A custom team's formation from who is in it.
 static ACR_EFormation FormationOf(array<int> roles, bool sf)
 {
  if (sf) return ACR_EFormation.SF;
  int at, mg, eng, med, snipe;
  foreach (int role : roles)
  {
   if (role == ACR_EUARole.LAT || role == ACR_EUARole.LIGHT_AT) at++;
   if (role == ACR_EUARole.MG) mg++;
   if (role == ACR_EUARole.ENGINEER) eng++;
   if (role == ACR_EUARole.MEDIC) med++;
   if (role == ACR_EUARole.SNIPER || role == ACR_EUARole.SPOTTER || role == ACR_EUARole.MARKSMAN) snipe++;
  }
  int men = roles.Count();
  if (men <= 4 && snipe * 2 >= men) return ACR_EFormation.SNIPER;
  if (men <= 5 && at >= 2) return ACR_EFormation.AT;
  if (men <= 5 && mg >= 1) return ACR_EFormation.MG;
  if (eng >= 2 && eng * 2 >= men - 1) return ACR_EFormation.ENGINEER;
  if (med >= 2 && med * 2 >= men - 1) return ACR_EFormation.MEDIC;
  if (men >= 6) return ACR_EFormation.LINE;
  return ACR_EFormation.MANEUVER;
 }
}

// A player's own team: soldiers from the faction's own kitted (or, for vanilla factions, its own
// catalogue) soldiers, optionally riding a transport from the catalogue. The server checks every
// soldier and prices it (ACR_DeckCatalog.CustomEntry); the code only names them.
class ACR_CustomCard
{
 string m_sName;
 ref array<string> m_aSoldiers = {}; // soldier ids (ACR_DeckSoldier.m_sId), one per man
 string m_sTransport;               // a vehicle card's name, or ""

 string ToText()
 {
  return ACR_DeckRules.CleanName(m_sName) + "!" + string.Join(",", m_aSoldiers, true) + "!" + m_sTransport;
 }

 static ACR_CustomCard FromText(string text)
 {
  array<string> parts = {};
  text.Split("!", parts, false);
  if (parts.Count() < 2) return null;
  ACR_CustomCard card = new ACR_CustomCard();
  card.m_sName = ACR_DeckRules.CleanName(parts[0]);
  if (card.m_sName.IsEmpty()) return null;
  parts[1].Split(",", card.m_aSoldiers, true);
  if (parts.Count() >= 3) card.m_sTransport = parts[2].Trim();
  return card;
 }

 ACR_CustomCard Copy()
 {
  ACR_CustomCard copy = new ACR_CustomCard();
  copy.m_sName = m_sName;
  copy.m_aSoldiers.Copy(m_aSoldiers);
  copy.m_sTransport = m_sTransport;
  return copy;
 }
}

// One deck: a faction, a name, the catalogue cards it takes (by card name) and its custom cards.
// As text it is one line, the share code players copy and paste:
// ACADECK1|faction|name|card^card^...|custom~custom...
class ACR_DeckData
{
 string m_sFaction;
 string m_sName;
 ref array<string> m_aCards = {};
 ref array<ref ACR_CustomCard> m_aCustom = {};

 string ToText()
 {
  array<string> customs = {};
  foreach (ACR_CustomCard custom : m_aCustom) customs.Insert(custom.ToText());
  return ACR_DeckRules.FORMAT + "|" + m_sFaction + "|" + ACR_DeckRules.CleanName(m_sName) + "|" + string.Join("^", m_aCards, true) + "|" + string.Join("~", customs, true);
 }

 static ACR_DeckData FromText(string text)
 {
  string line = text.Trim();
  if (!line.StartsWith(ACR_DeckRules.FORMAT + "|")) return null;
  array<string> parts = {};
  line.Split("|", parts, false);
  if (parts.Count() < 4) return null;
  ACR_DeckData deck = new ACR_DeckData();
  deck.m_sFaction = parts[1].Trim();
  deck.m_sName = ACR_DeckRules.CleanName(parts[2]);
  if (deck.m_sFaction.IsEmpty()) return null;
  if (deck.m_sName.IsEmpty()) deck.m_sName = "Deck";
  array<string> cards = {};
  parts[3].Split("^", cards, true);
  foreach (string card : cards)
  {
   string name = card.Trim();
   if (!name.IsEmpty() && !deck.m_aCards.Contains(name) && deck.m_aCards.Count() < ACR_DeckRules.MAX_CARDS) deck.m_aCards.Insert(name);
  }
  if (parts.Count() >= 5)
  {
   array<string> customs = {};
   parts[4].Split("~", customs, true);
   foreach (string customText : customs)
   {
    if (deck.m_aCustom.Count() >= ACR_DeckRules.MAX_CUSTOM) break;
    ACR_CustomCard custom = ACR_CustomCard.FromText(customText);
    if (custom && !deck.FindCustom(custom.m_sName)) deck.m_aCustom.Insert(custom);
   }
  }
  return deck;
 }

 ACR_CustomCard FindCustom(string name)
 {
  foreach (ACR_CustomCard custom : m_aCustom) if (custom.m_sName == name) return custom;
  return null;
 }

 ACR_DeckData Copy()
 {
  return FromText(ToText());
 }
}
