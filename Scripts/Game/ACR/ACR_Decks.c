// Deck builder (#106), server side: the catalogue a deck is picked from (ACR_DeckCatalog), the
// decks players hand in, which deck each side plays and the buy list it leaves (ACR_Decks.Apply,
// from ACR_BaseSystem.GetFaction right after ACR_FactionCatalog.BuildRequisitions).

// One soldier a custom card can take.
class ACR_DeckSoldier
{
 string m_sId;           // the prefab's file name: the same in every match and mod set
 ResourceName m_sPrefab;
 string m_sName;
 int m_iRole;            // ACR_EUARole
 int m_iRP;
 bool m_bSF;
}

class ACR_DeckCatalog
{
 static const float SF_SOLDIER_RP = 2;   // x a soldier's RP in a special forces kit (SF teams: 8 RP a man)

 // ---------------------------------------------------------------- extra kitted cards
 // Teams of ACA's own kitted soldiers (#98: ACA kitted copies only) that today's list leaves out:
 // the variety lives in the builder. Only for factions with kitted soldiers.
 static void Extras(string faction, array<ref ACR_RequisitionEntry> result, bool stepped)
 {
  if (!ACR_InfantryCatalog.Active(faction)) return;
  if (faction == "RHS_USAF")
  {
   ACR_EInfantryKit usmc = ACR_EInfantryKit.USMC;
   AddExtra(result, "Weapons Squad", ACR_EFormation.MG, usmc, {1,2,10,2,10}, stepped);
   AddExtra(result, "Assault Section", ACR_EFormation.AT, usmc, {1,14,14,14,3}, stepped);
   AddExtra(result, "Scout Sniper Team", ACR_EFormation.SNIPER, usmc, {1,15,16,6}, stepped);
   AddExtra(result, "Combat Engineer Squad", ACR_EFormation.ENGINEER, usmc, {1,13,13,13,3,0}, stepped);
   AddExtra(result, "Reinforced Rifle Squad", ACR_EFormation.LINE, usmc, {1,1,6,3,3,4,4,14,14,12,2,10}, stepped);
   AddExtra(result, "MARSOC Recon Team", ACR_EFormation.SF, ACR_EInfantryKit.MARSOC, {1,6,12,0}, stepped);
   return;
  }
  bool ru = faction == "RHS_AFRF";
  ACR_EInfantryKit kit = ACR_EInfantryKit.UA;
  if (ru) kit = ACR_EInfantryKit.RU;
  AddExtra(result, "Assault Squad", ACR_EFormation.LINE, kit, {1,4,4,3,3,14,0}, stepped);
  AddExtra(result, "Weapons Squad", ACR_EFormation.MG, kit, {1,2,10,2,10}, stepped);
  AddExtra(result, "Anti-Tank Section", ACR_EFormation.AT, kit, {1,5,11,5,11}, stepped);
  AddExtra(result, "Recon Squad", ACR_EFormation.SCOUT, kit, {1,6,0,0,12,14}, stepped);
  if (ru) AddExtra(result, "SSO Recon Team", ACR_EFormation.SF, ACR_EInfantryKit.RU_SF, {1,6,12,0}, stepped);
  else AddExtra(result, "Western Weapons Team", ACR_EFormation.MG, ACR_EInfantryKit.WESTERN, {1,2,10,3}, stepped);
 }

 protected static void AddExtra(array<ref ACR_RequisitionEntry> result, string name, ACR_EFormation formation, ACR_EInfantryKit kit, array<int> roles, bool stepped)
 {
  if (Find(result, name)) return;
  int rp;
  foreach (int role : roles)
  {
   if (ACR_InfantryTemplates.Character(kit, role).IsEmpty()) return; // a role this kit lacks
   rp += ACR_DeckRules.SoldierRP(role);
  }
  if (formation == ACR_EFormation.SF) rp = (int)Math.Round(rp * SF_SOLDIER_RP);
  array<ref ACR_RequisitionEntry> made = {};
  ACR_RequisitionEntry entry = ACR_InfantryCatalog.Add(made, name, rp, formation, kit, roles);
  entry.m_bExtra = true;
  ACR_Pricing.Setup(entry, stepped);
  result.Insert(entry);
 }

 static ACR_RequisitionEntry Find(array<ref ACR_RequisitionEntry> list, string name)
 {
  foreach (ACR_RequisitionEntry entry : list) if (entry.m_sName == name) return entry;
  return null;
 }

 // ---------------------------------------------------------------- soldiers for custom cards
 // The soldiers this faction already fields: every man of its infantry cards (kitted factions: also
 // every role of their kits). Never another faction's or a mod's soldiers as shipped (#98).
 static void Soldiers(string faction, array<ref ACR_RequisitionEntry> catalogue, notnull array<ref ACR_DeckSoldier> result)
 {
  result.Clear();
  array<ResourceName> prefabs = {};
  array<bool> sf = {};
  foreach (ACR_RequisitionEntry entry : catalogue)
  {
   if (entry.m_sGroup.IsEmpty() || !entry.m_sVehicle.IsEmpty() || entry.m_bMortarTeam || entry.m_bObserverTeam) continue;
   array<ResourceName> men = {};
   if (entry.m_aSoldiers && !entry.m_aSoldiers.IsEmpty()) men.Copy(entry.m_aSoldiers);
   else GroupSlots(entry.m_sGroup, men);
   foreach (ResourceName man : men)
   {
    if (man.IsEmpty() || prefabs.Contains(man)) continue;
    prefabs.Insert(man);
    sf.Insert(entry.m_eFormation == ACR_EFormation.SF);
   }
  }
  array<ACR_EInfantryKit> kits = {};
  KitsOf(faction, kits);
  foreach (ACR_EInfantryKit ownKit : kits)
  {
   for (int kitRole = 0; kitRole <= ACR_EUARole.SPOTTER; kitRole++)
   {
    ResourceName kitMan = ACR_InfantryTemplates.Character(ownKit, kitRole);
    if (kitMan.IsEmpty() || prefabs.Contains(kitMan)) continue;
    prefabs.Insert(kitMan);
    sf.Insert(false);
   }
  }
  foreach (int i, ResourceName prefab : prefabs)
  {
   ACR_DeckSoldier soldier = new ACR_DeckSoldier();
   soldier.m_sPrefab = prefab;
   soldier.m_sId = FilePath.StripExtension(FilePath.StripPath(prefab));
   ACR_EInfantryKit kit;
   int role;
   if (KitRole(prefab, kit, role))
   {
    soldier.m_iRole = role;
    soldier.m_bSF = kit == ACR_EInfantryKit.RU_SF || kit == ACR_EInfantryKit.UA_SF || kit == ACR_EInfantryKit.MARSOC;
    soldier.m_sName = KitName(kit, kits.Count() > 1) + ACR_DeckRules.SoldierRoleName(role);
   }
   else
   {
    soldier.m_iRole = ACR_DeckRules.RoleFromPrefab(prefab);
    soldier.m_bSF = sf[i];
    soldier.m_sName = ACR_DeckRules.SoldierRoleName(soldier.m_iRole);
    if (soldier.m_bSF) soldier.m_sName = "SF " + soldier.m_sName;
   }
   if (soldier.m_iRole == ACR_EUARole.CREW || soldier.m_iRole == ACR_EUARole.PILOT || soldier.m_iRole == ACR_EUARole.UNARMED) continue;
   soldier.m_iRP = ACR_DeckRules.SoldierRP(soldier.m_iRole);
   if (soldier.m_bSF) soldier.m_iRP = (int)Math.Round(soldier.m_iRP * SF_SOLDIER_RP);
   result.Insert(soldier);
  }
  // Two soldiers of one name (a vanilla faction's riflemen of two groups): the second gets its file name.
  foreach (int a, ACR_DeckSoldier first : result)
  {
   for (int b = a + 1; b < result.Count(); b++)
    if (result[b].m_sName == first.m_sName) result[b].m_sName = result[b].m_sName + " (" + result[b].m_sId + ")";
  }
 }

 static void KitsOf(string faction, notnull array<ACR_EInfantryKit> kits)
 {
  kits.Clear();
  if (!ACR_InfantryCatalog.Active(faction)) return;
  if (faction == "RHS_AFRF") { kits.Insert(ACR_EInfantryKit.RU); kits.Insert(ACR_EInfantryKit.RU_SF); return; }
  if (faction == "RHS_USAF") { kits.Insert(ACR_EInfantryKit.USMC); kits.Insert(ACR_EInfantryKit.MARSOC); return; }
  kits.Insert(ACR_EInfantryKit.UA);
  kits.Insert(ACR_EInfantryKit.WESTERN);
  kits.Insert(ACR_EInfantryKit.UA_SF);
 }

 protected static string KitName(ACR_EInfantryKit kit, bool several)
 {
  if (!several) return "";
  switch (kit)
  {
   case ACR_EInfantryKit.RU: return "";
   case ACR_EInfantryKit.UA: return "";
   case ACR_EInfantryKit.USMC: return "";
   case ACR_EInfantryKit.WESTERN: return "Western ";
   case ACR_EInfantryKit.RU_SF: return "SSO ";
   case ACR_EInfantryKit.UA_SF: return "SF ";
   case ACR_EInfantryKit.MARSOC: return "MARSOC ";
  }
  return "";
 }

 // A kitted prefab's kit and role (ACR_InfantryTemplates is the list of them).
 static bool KitRole(ResourceName prefab, out ACR_EInfantryKit kit, out int role)
 {
  for (int k = ACR_EInfantryKit.RU; k <= ACR_EInfantryKit.MARSOC; k++)
  {
   for (int r = 0; r <= ACR_EUARole.SPOTTER; r++)
   {
    if (ACR_InfantryTemplates.Character(k, r) != prefab) continue;
    kit = k;
    role = r;
    return true;
   }
  }
  return false;
 }

 // A group prefab's members (m_aUnitPrefabSlots, inherited values included).
 static bool GroupSlots(ResourceName group, notnull array<ResourceName> result)
 {
  result.Clear();
  Resource resource = Resource.Load(group);
  if (!resource || !resource.IsValid()) return false;
  IEntitySource source = resource.GetResource().ToEntitySource();
  if (source) source.Get("m_aUnitPrefabSlots", result);
  return !result.IsEmpty();
 }

 static ACR_DeckSoldier FindSoldier(array<ref ACR_DeckSoldier> pool, string id)
 {
  foreach (ACR_DeckSoldier soldier : pool) if (soldier.m_sId == id) return soldier;
  return null;
 }

 // The group a custom team spawns in: the faction's own curated base group, else its side's.
 static ResourceName BaseGroup(string faction, array<ref ACR_RequisitionEntry> catalogue)
 {
  foreach (ACR_RequisitionEntry entry : catalogue)
   if (entry.m_aSoldiers && !entry.m_aSoldiers.IsEmpty() && !entry.m_sGroup.IsEmpty() && !entry.m_bMortarTeam && !entry.m_bObserverTeam) return entry.m_sGroup;
  if (faction.Contains("FIA")) return ACR_Mortars.GROUP_FIA;
  if (ACR_Supports.IsWestern(faction)) return ACR_Mortars.GROUP_WEST;
  return ACR_Mortars.GROUP_EAST;
 }

 // A vehicle card a custom team may ride in.
 static bool IsTransport(ACR_RequisitionEntry entry)
 {
  if (!entry || entry.m_sVehicle.IsEmpty() || !entry.m_sGroup.IsEmpty() || entry.m_bAircraft || entry.m_iSupportHeli > 0) return false;
  if (ACR_Logistics.ServiceOfName(entry.m_sVehicle) != 0) return false;
  return entry.m_bTransport || entry.m_bCarrier || entry.m_bSquadCarrier;
 }

 // A custom card's buy-list entry, or null with what's wrong. The price comes from its men here:
 // a card's code only names them.
 static ACR_RequisitionEntry CustomEntry(string faction, ACR_CustomCard card, array<ref ACR_DeckSoldier> pool, array<ref ACR_RequisitionEntry> catalogue, bool stepped, out string error)
 {
  if (!card || card.m_sName.IsEmpty()) { error = "a custom card has no name"; return null; }
  int men = card.m_aSoldiers.Count();
  if (men < ACR_DeckRules.CUSTOM_MIN_MEN || men > ACR_DeckRules.CUSTOM_MAX_MEN)
  {
   error = string.Format("%1 has %2 men (%3 to %4)", card.m_sName, men, ACR_DeckRules.CUSTOM_MIN_MEN, ACR_DeckRules.CUSTOM_MAX_MEN);
   return null;
  }
  ACR_RequisitionEntry same = Find(catalogue, card.m_sName);
  if (same && !same.m_bCustom) { error = card.m_sName + " has the name of a catalogue card"; return null; }
  array<ResourceName> soldiers = {};
  array<int> roles = {};
  bool sf;
  int rp;
  foreach (string id : card.m_aSoldiers)
  {
   ACR_DeckSoldier soldier = FindSoldier(pool, id);
   if (!soldier) { error = string.Format("%1: %2 isn't a soldier of this faction here", card.m_sName, id); return null; }
   soldiers.Insert(soldier.m_sPrefab);
   roles.Insert(soldier.m_iRole);
   rp += soldier.m_iRP;
   if (soldier.m_bSF) sf = true;
  }
  ACR_RequisitionEntry ride;
  if (!card.m_sTransport.IsEmpty())
  {
   ride = Find(catalogue, card.m_sTransport);
   if (!IsTransport(ride)) { error = string.Format("%1: %2 can't carry it", card.m_sName, card.m_sTransport); return null; }
   int ridePrice = ride.m_iBaseCost;
   if (ridePrice <= 0) ridePrice = ride.m_iCost;
   rp += ridePrice;
  }
  ACR_RequisitionEntry entry = new ACR_RequisitionEntry();
  int price = rp;
  if (price < 6) price = 6;
  entry.Set(card.m_sName, BaseGroup(faction, catalogue), "", price, ACR_DeckRules.CUSTOM_STOCK);
  entry.m_aSoldiers = soldiers;
  entry.m_eFormation = ACR_DeckRules.FormationOf(roles, sf);
  entry.m_bCustom = true;
  if (sf) { entry.m_bRare = true; entry.m_iStock = 2; }
  if (ride)
  {
   entry.m_sVehicle = ride.m_sVehicle;
   entry.m_bCrewed = true;
   entry.m_bBoard = true;
   entry.m_bArmed = ride.m_bArmed;
   // Never more of the vehicle than its own card gives: a rare ride (an APC or IFV with a fixed
   // stock) makes the card rare with the ride's stock, and each purchase also takes one of the ride
   // card's stock when the deck has it (ACR_Requisition.RideOf).
   entry.m_sRide = ride.m_sName;
   if (ride.m_bRare)
   {
    entry.m_bRare = true;
    entry.m_iStock = Math.Min(entry.m_iStock, ride.m_iStock);
   }
  }
  ACR_Pricing.Setup(entry, stepped);
  return entry;
 }

 // What a card is in a word or two, for the editor: "6 men", "vehicle".
 static string Info(ACR_RequisitionEntry entry)
 {
  if (ACR_DeckRules.Locked(entry)) return "always in";
  string info;
  if (!entry.m_sVehicle.IsEmpty() && entry.m_sGroup.IsEmpty()) info = "vehicle";
  else
  {
   int men;
   if (entry.m_aSoldiers && !entry.m_aSoldiers.IsEmpty()) men = entry.m_aSoldiers.Count();
   else
   {
    array<ResourceName> slots = {};
    GroupSlots(entry.m_sGroup, slots);
    men = slots.Count();
   }
   if (men > 0) info = men.ToString() + " men";
   else info = "team";
   if (!entry.m_sVehicle.IsEmpty()) info += " + ride";
  }
  int price = entry.m_iBaseCost;
  if (price <= 0) price = entry.m_iCost;
  info += ", " + price.ToString() + " RP";
  if (entry.m_bRare) info += ", rare";
  return info;
 }
}

// A deck a player handed in for the match (the editor's "Use").
class ACR_DeckSubmission
{
 int m_iSerial;
 int m_iPlayer;
 string m_sPlayer;
 bool m_bHost;   // handed in by the host or an admin: only these decide a side's deck on auto
 ref ACR_DeckData m_Deck;
}

class ACR_Decks
{
 protected static ref array<ref ACR_DeckSubmission> s_Submitted;
 protected static int s_iSerial;
 protected static ref map<string, string> s_Applied; // faction -> what its list was built from
 static const int MAX_SUBMITTED = 24;

 static array<ref ACR_DeckSubmission> Submitted()
 {
  if (!s_Submitted) s_Submitted = {};
  return s_Submitted;
 }

 static string Applied(string faction)
 {
  if (!s_Applied) return "";
  string text;
  s_Applied.Find(faction, text);
  return text;
 }

 static void Reset()
 {
  s_Submitted = null;
  s_Applied = null;
  s_iSerial = 0;
 }

 // The side's deck row; "" for a faction that isn't one of the two sides (FIA pockets and guerrillas
 // play the standard deck, not Blue's choice).
 static string OptionFor(string faction)
 {
  if (faction == ACR_FactionSelection.East()) return "acrDeckEast";
  if (faction == ACR_FactionSelection.West()) return "acrDeckWest";
  return "";
 }

 // The side's deck row: "" (auto), "standard", "ai" or "sub:<serial>".
 static string Chosen(string faction)
 {
  string option = OptionFor(faction);
  string value;
  if (option.IsEmpty() || !ACR_Options.Get(option, value)) return "";
  value.TrimInPlace();
  value.ToLower();
  return value;
 }

 // The latest deck the host or an admin handed in for this faction (auto). A player's own deck is an
 // offer: the host's Blue deck / Red deck row picks it. Sides are picked after match setup, so at
 // hand-in time nobody but the host can be trusted to speak for a side (a Red player could otherwise
 // hand Blue, or the AI enemy, a deck of two trucks).
 static ACR_DeckSubmission Latest(string faction)
 {
  ACR_DeckSubmission latest;
  foreach (ACR_DeckSubmission sub : Submitted())
   if (sub.m_bHost && sub.m_Deck.m_sFaction == faction && (!latest || sub.m_iSerial > latest.m_iSerial)) latest = sub;
  return latest;
 }

 static ACR_DeckSubmission BySerial(int serial)
 {
  foreach (ACR_DeckSubmission sub : Submitted()) if (sub.m_iSerial == serial) return sub;
  return null;
 }

 // What the side plays: "standard", "ai" or "deck" (the submission).
 static string Resolve(string faction, out ACR_DeckSubmission submission)
 {
  submission = null;
  string chosen = Chosen(faction);
  if (chosen.StartsWith("sub:"))
  {
   submission = BySerial(chosen.Substring(4, chosen.Length() - 4).ToInt());
   if (submission && submission.m_Deck.m_sFaction == faction) return "deck";
   submission = null;
  }
  if (chosen == "standard" || chosen == "ai") return chosen;
  // Auto: the latest deck the host or an admin handed in for this faction, else the standard deck. The AI's own
  // deck only when the row asks for it ("AI picks"): it changes what an AI side can buy, so it is a
  // choice, not the default.
  submission = Latest(faction);
  if (submission) return "deck";
  return "standard";
 }

 // ---------------------------------------------------------------- the buy list
 // From ACR_BaseSystem.GetFaction: 'list' is today's list (BuildRequisitions); it becomes the deck.
 static void Apply(string faction, ACR_FactionSettings settings, array<ref ACR_RequisitionEntry> list)
 {
  if (!s_Applied) s_Applied = new map<string, string>();
  if (!ACR_DeckRules.Enabled())
  {
   s_Applied.Set(faction, "decks off");
   return;
  }
  // Mission makers keep control: a list fixed in the world overrides any deck.
  if (settings && settings.m_aRequisitions && !settings.m_aRequisitions.IsEmpty())
  {
   s_Applied.Set(faction, "the mission's own list");
   PrintFormat("[ACR] DECK %1: the mission's own list, no deck", faction);
   return;
  }
  bool stepped = ACR_Pricing.Stepped(settings);
  int supportsAt = SupportsAt(list);
  array<ref ACR_RequisitionEntry> extras = {};
  ACR_DeckCatalog.Extras(faction, extras, stepped);
  foreach (int e, ACR_RequisitionEntry extra : extras) list.InsertAt(extra, supportsAt + e);
  int budget = ACR_DeckRules.Budget();
  ACR_DeckSubmission submission;
  string kind = Resolve(faction, submission);
  array<bool> keep = {};
  string note;
  string what;
  if (kind == "deck")
  {
   note = KeepDeck(faction, submission.m_Deck, list, stepped, keep);
   what = string.Format("%1's deck %2", submission.m_sPlayer, submission.m_Deck.m_sName);
  }
  else if (kind == "ai")
  {
   KeepAI(faction, list, budget, keep);
   what = "the AI's pick";
  }
  else
  {
   foreach (ACR_RequisitionEntry entry : list) keep.Insert(!entry.m_bExtra);
   what = "the standard deck";
  }
  // The standard deck is today's list, whatever its points (#106: nobody has to build a deck to play).
  int dropped;
  if (kind != "standard") dropped = Fit(list, keep, budget);
  if (dropped > 0) note += string.Format(" %1 dropped to fit %2 points.", dropped, budget);
  for (int i = list.Count() - 1; i >= 0; i--) if (!keep[i]) list.RemoveOrdered(i);
  int points = Points(list);
  string summary = string.Format("%1: %2 cards, %3/%4 points.%5", what, list.Count(), points, ACR_DeckRules.BudgetName(budget), note);
  s_Applied.Set(faction, summary);
  PrintFormat("[ACR] DECK %1: %2", faction, summary);
  string names;
  foreach (ACR_RequisitionEntry kept : list)
  {
   if (!names.IsEmpty()) names += ", ";
   names += string.Format("%1 (%2)", kept.m_sName, ACR_DeckRules.Points(kept));
  }
  PrintFormat("[ACR] DECK %1 cards: %2", faction, names);
 }

 // Where the support cards (mortar, observers, helicopters) start: extras and customs go before them.
 protected static int SupportsAt(array<ref ACR_RequisitionEntry> list)
 {
  foreach (int i, ACR_RequisitionEntry entry : list)
   if (entry.m_bMortarTeam || entry.m_bObserverTeam || entry.m_iSupportHeli > 0) return i;
  return list.Count();
 }

 static int Points(array<ref ACR_RequisitionEntry> list, array<bool> keep = null)
 {
  int total;
  foreach (int i, ACR_RequisitionEntry entry : list)
   if (!keep || keep[i]) total += ACR_DeckRules.Points(entry);
  return total;
 }

 // A player's deck: its catalogue cards that this match has, plus its custom cards (added to 'list').
 protected static string KeepDeck(string faction, ACR_DeckData deck, array<ref ACR_RequisitionEntry> list, bool stepped, notnull array<bool> keep)
 {
  keep.Clear();
  foreach (ACR_RequisitionEntry entry : list) keep.Insert(ACR_DeckRules.Locked(entry) || deck.m_aCards.Contains(entry.m_sName));
  array<string> missing = {};
  foreach (string card : deck.m_aCards)
   if (!ACR_DeckCatalog.Find(list, card)) missing.Insert(card);
  array<ref ACR_DeckSoldier> pool = {};
  ACR_DeckCatalog.Soldiers(faction, list, pool);
  int at = SupportsAt(list);
  array<string> refused = {};
  foreach (ACR_CustomCard custom : deck.m_aCustom)
  {
   string error;
   ACR_RequisitionEntry made = ACR_DeckCatalog.CustomEntry(faction, custom, pool, list, stepped, error);
   if (!made) { refused.Insert(error); continue; }
   list.InsertAt(made, at);
   keep.InsertAt(true, at);
   at++;
  }
  string note;
  if (!missing.IsEmpty()) note += string.Format(" Not in this match's mods: %1.", string.Join(", ", missing, true));
  if (!refused.IsEmpty()) note += string.Format(" Custom cards left out: %1.", string.Join("; ", refused, true));
  return note;
 }

 // Over the budget: drop the dearest card of the role with the most cards, until it fits (locked
 // cards are free and stay). The number of cards dropped.
 static int Fit(array<ref ACR_RequisitionEntry> list, notnull array<bool> keep, int budget)
 {
  if (budget <= 0) return 0;
  int dropped;
  while (Points(list, keep) > budget)
  {
   array<int> counts = {};
   for (int r = 0; r < ACR_DeckRules.RoleCount(); r++) counts.Insert(0);
   foreach (int i, ACR_RequisitionEntry entry : list)
    if (keep[i] && !ACR_DeckRules.Locked(entry)) counts[ACR_DeckRules.RoleOf(entry)] = counts[ACR_DeckRules.RoleOf(entry)] + 1;
   int victim = -1;
   int victimCount, victimPoints;
   foreach (int j, ACR_RequisitionEntry candidate : list)
   {
    if (!keep[j] || ACR_DeckRules.Locked(candidate)) continue;
    int count = counts[ACR_DeckRules.RoleOf(candidate)];
    int points = ACR_DeckRules.Points(candidate);
    if (victim < 0 || count > victimCount || (count == victimCount && points > victimPoints))
    {
     victim = j;
     victimCount = count;
     victimPoints = points;
    }
   }
   if (victim < 0) break;
   keep[victim] = false;
   dropped++;
  }
  return dropped;
 }

 // ---------------------------------------------------------------- the AI's deck
 // Built from its company and doctrine (#106): what its company kit and its buying need first, a
 // minimum per role, then the rest of the budget weighted by doctrine, a seeded pick among the
 // near-best (ACR_Variety: the same match makes the same deck, two matches differ).
 static void KeepAI(string faction, array<ref ACR_RequisitionEntry> list, int budget, notnull array<bool> keep)
 {
  keep.Clear();
  ACR_AIDoctrine doctrine = DoctrineOf(faction);
  string kit = ACR_DoctrineKits.KitOf(ACR_DoctrineKits.CompanyOf(faction));
  foreach (ACR_RequisitionEntry entry : list)
  {
   // Locked trucks, and the supports its fire missions and doctrines lean on.
   keep.Insert(ACR_DeckRules.Locked(entry) || entry.m_bMortarTeam || entry.m_bObserverTeam);
  }
  // Its rides: the cheapest transport.
  KeepCheapest(list, keep, 1);
  // Blitz's carrier, Raider's recon team.
  if (kit == "blitz") KeepCheapest(list, keep, 2);
  if (kit == "raider") KeepCheapest(list, keep, 3);
  int left = budget - Points(list, keep);
  // A minimum per role: two infantry cards, a weapons team, a recon card.
  for (int n = 0; n < 2; n++) left -= PickAI(faction, list, keep, doctrine, left, budget, ACR_EDeckRole.INFANTRY, "min inf " + n.ToString());
  left -= PickAI(faction, list, keep, doctrine, left, budget, ACR_EDeckRole.TEAMS, "min team");
  left -= PickAI(faction, list, keep, doctrine, left, budget, ACR_EDeckRole.RECON, "min recon");
  // The rest, by doctrine.
  for (int step = 0; step < ACR_DeckRules.MAX_CARDS; step++)
  {
   int spent = PickAI(faction, list, keep, doctrine, left, budget, -1, "fill " + step.ToString());
   if (spent < 0) break;
   left -= spent;
  }
 }

 static ACR_AIDoctrine DoctrineOf(string faction)
 {
  string value;
  if (ACR_Options.Get(ACR_AIDoctrine.OptionFor(faction), value))
  {
   value.ToLower();
   if (ACR_AIDoctrine.Find(value)) return ACR_AIDoctrine.Find(value);
  }
  ACR_AIDoctrine followed = ACR_AIDoctrine.Find(ACR_AIDoctrine.FollowedKey(faction));
  if (followed) return followed;
  return ACR_AIDoctrine.Find("balanced");
 }

 // what: 1 a transport, 2 a carrier (a squad carrier when there is one), 3 a small recon team.
 protected static void KeepCheapest(array<ref ACR_RequisitionEntry> list, array<bool> keep, int what)
 {
  bool squad;
  if (what == 2)
  {
   foreach (ACR_RequisitionEntry e : list)
   {
    if (e.m_bSquadCarrier) squad = true;
   }
  }
  int best = -1;
  foreach (int i, ACR_RequisitionEntry entry : list)
  {
   bool fits;
   if (what == 1) fits = entry.m_bTransport && !ACR_DeckRules.Locked(entry) && entry.m_sGroup.IsEmpty();
   else if (what == 2) fits = entry.m_bCarrier && entry.m_bSquadCarrier == squad;
   else fits = ACR_DoctrineKits.IsRecon(entry) && ACR_AICommander.IsSmallTeamEntry(entry) && !entry.m_bObserverTeam;
   if (!fits) continue;
   if (keep[i]) return;
   if (best < 0 || ACR_DeckRules.Points(entry) < ACR_DeckRules.Points(list[best])) best = i;
  }
  if (best >= 0) keep[best] = true;
 }

 // One more card for the AI, of 'role' (-1: any), that fits in 'left'. Its points, or -1 for none.
 protected static int PickAI(string faction, array<ref ACR_RequisitionEntry> list, array<bool> keep, ACR_AIDoctrine doctrine, int left, int budget, int role, string key)
 {
  array<int> counts = {};
  for (int r = 0; r < ACR_DeckRules.RoleCount(); r++) counts.Insert(0);
  foreach (int i, ACR_RequisitionEntry entry : list)
   if (keep[i]) counts[ACR_DeckRules.RoleOf(entry)] = counts[ACR_DeckRules.RoleOf(entry)] + 1;
  array<int> indices = {};
  array<float> scores = {};
  foreach (int j, ACR_RequisitionEntry candidate : list)
  {
   if (keep[j]) continue;
   int cardRole = ACR_DeckRules.RoleOf(candidate);
   if (role >= 0 && cardRole != role) continue;
   // Its AI can't fly aircraft (support helicopters it buys for a helipad).
   if (candidate.m_bAircraft && candidate.m_iSupportHeli <= 0) continue;
   if (cardRole == ACR_EDeckRole.SF && counts[cardRole] > 0) continue;
   int points = ACR_DeckRules.Points(candidate);
   if (budget > 0 && points > left) continue;
   float weight = RoleWeight(cardRole, doctrine);
   if (weight <= 0) continue;
   float score = weight * Math.Sqrt(points) / (1 + 0.7 * counts[cardRole]);
   indices.Insert(j);
   scores.Insert(score);
  }
  int pick = ACR_Variety.Pick(scores, "deck " + faction + " " + key);
  if (pick < 0) return -1;
  keep[indices[pick]] = true;
  return ACR_DeckRules.Points(list[indices[pick]]);
 }

 static float RoleWeight(int role, ACR_AIDoctrine doctrine)
 {
  switch (role)
  {
   case ACR_EDeckRole.INFANTRY: return 1.0;
   case ACR_EDeckRole.TEAMS: return 0.8 * doctrine.m_fSmallTeamWeight;
   case ACR_EDeckRole.RECON: return 0.6 * doctrine.m_fObserverWeight;
   case ACR_EDeckRole.SF: return 0.5;
   case ACR_EDeckRole.VEHICLES: return 0.6 * doctrine.m_fVehicleWeight;
   case ACR_EDeckRole.CARRIERS: return 0.8 * doctrine.m_fVehicleWeight;
   case ACR_EDeckRole.ARMOUR: return 0.7 * doctrine.m_fVehicleWeight;
   case ACR_EDeckRole.SUPPORT: return 0.5;
   case ACR_EDeckRole.LOGISTICS: return 0.15;
  }
  // Air: support helicopters.
  if (doctrine.m_bAirBudget) return 0.6;
  return 0.4;
 }

 // ---------------------------------------------------------------- the editor's requests
 // The faction's whole catalogue for the editor: today's list, the extra cards, the soldiers for
 // custom cards, the budget and the standard deck.
 static void Catalogue(string faction, notnull array<string> names, notnull array<int> roles, notnull array<int> points, notnull array<int> prices, notnull array<int> flags, notnull array<string> infos, notnull array<string> soldierIds, notnull array<string> soldierNames, notnull array<int> soldierRP, notnull array<int> soldierFlags)
 {
  array<ref ACR_RequisitionEntry> list = {};
  ACR_FactionSettings settings = ACR_FactionSettings.Find(faction);
  ACR_FactionCatalog.BuildRequisitions(faction, settings, list);
  bool stepped = ACR_Pricing.Stepped(settings);
  ACR_DeckCatalog.Extras(faction, list, stepped);
  foreach (ACR_RequisitionEntry entry : list)
  {
   names.Insert(entry.m_sName);
   roles.Insert(ACR_DeckRules.RoleOf(entry));
   points.Insert(ACR_DeckRules.Points(entry));
   int price = entry.m_iBaseCost;
   if (price <= 0) price = entry.m_iCost;
   prices.Insert(price);
   int flag;
   if (!entry.m_bExtra) flag |= ACR_DeckFlag.STANDARD;
   if (ACR_DeckRules.Locked(entry)) flag |= ACR_DeckFlag.LOCKED;
   if (entry.m_bExtra) flag |= ACR_DeckFlag.EXTRA;
   if (entry.m_bRare) flag |= ACR_DeckFlag.RARE;
   if (entry.m_eFormation == ACR_EFormation.SF) flag |= ACR_DeckFlag.SF;
   if (ACR_DeckCatalog.IsTransport(entry)) flag |= ACR_DeckFlag.TRANSPORT;
   flags.Insert(flag);
   infos.Insert(ACR_DeckCatalog.Info(entry));
  }
  array<ref ACR_DeckSoldier> pool = {};
  ACR_DeckCatalog.Soldiers(faction, list, pool);
  foreach (ACR_DeckSoldier soldier : pool)
  {
   soldierIds.Insert(soldier.m_sId);
   soldierNames.Insert(soldier.m_sName);
   soldierRP.Insert(soldier.m_iRP);
   int soldierFlag;
   if (soldier.m_bSF) soldierFlag |= ACR_DeckFlag.SF;
   soldierFlags.Insert(soldierFlag);
  }
 }

 // A player hands in a deck for the match. Checked against this match's catalogue now, so the
 // player hears at once what won't come with it.
 static string Submit(int playerId, string text)
 {
  return Accept(playerId, text, ACR_Manager.IsHostOrAdmin(playerId), ACR_Setup.Started());
 }

 // host: the host or an admin handed it in; started: match setup is over.
 static string Accept(int playerId, string text, bool host, bool started)
 {
  ACR_DeckData deck = ACR_DeckData.FromText(text);
  if (!deck) return "That isn't a deck code";
  if (!ACR_DeckRules.Enabled()) return "Decks are off for this match";
  // Hand-ins are cleared when a match ends (ACR_Manager.Stop), so one made after Start would never apply.
  if (started) return "The match has started: hand a deck in during match setup";
  if (deck.m_sFaction != ACR_FactionSelection.West() && deck.m_sFaction != ACR_FactionSelection.East())
   return string.Format("%1 doesn't play in this match", deck.m_sFaction);
  ACR_FactionSettings settings = ACR_FactionSettings.Find(deck.m_sFaction);
  if (settings && settings.m_aRequisitions && !settings.m_aRequisitions.IsEmpty()) return "This mission fixes its buy list: decks don't apply";
  array<ref ACR_RequisitionEntry> list = {};
  ACR_FactionCatalog.BuildRequisitions(deck.m_sFaction, settings, list);
  bool stepped = ACR_Pricing.Stepped(settings);
  ACR_DeckCatalog.Extras(deck.m_sFaction, list, stepped);
  array<bool> keep = {};
  string note = KeepDeck(deck.m_sFaction, deck, list, stepped, keep);
  int budget = ACR_DeckRules.Budget();
  int points = Points(list, keep);
  int cards;
  foreach (bool kept : keep) if (kept) cards++;
  array<ref ACR_DeckSubmission> all = Submitted();
  for (int i = all.Count() - 1; i >= 0; i--)
   if (all[i].m_iPlayer == playerId && all[i].m_Deck.m_sFaction == deck.m_sFaction) all.RemoveOrdered(i);
  while (all.Count() >= MAX_SUBMITTED) all.RemoveOrdered(0);
  ACR_DeckSubmission before = Latest(deck.m_sFaction);
  ACR_DeckSubmission sub = new ACR_DeckSubmission();
  s_iSerial++;
  sub.m_iSerial = s_iSerial;
  sub.m_iPlayer = playerId;
  sub.m_bHost = host;
  sub.m_sPlayer = GetGame().GetPlayerManager().GetPlayerName(playerId);
  if (sub.m_sPlayer.IsEmpty()) sub.m_sPlayer = "Player " + playerId.ToString();
  sub.m_Deck = deck;
  all.Insert(sub);
  PrintFormat("[ACR] DECK handed in by %1 for %2: %3 (%4 cards, %5 points)", sub.m_sPlayer, deck.m_sFaction, deck.m_sName, cards, points);
  string result = string.Format("%1 handed in for %2: %3 cards, %4/%5 points.", deck.m_sName, deck.m_sFaction, cards, points, ACR_DeckRules.BudgetName(budget));
  if (budget > 0 && points > budget) result += " Over budget: the dearest cards are dropped.";
  if (!host) result += " Offered to the host: the setup screen's deck row picks which deck the side plays.";
  else if (Chosen(deck.m_sFaction) != "") result += " The deck row picks which deck the side plays.";
  else if (before && before.m_iPlayer != playerId) result += string.Format(" It replaces %1's deck for %2.", before.m_sPlayer, deck.m_sFaction);
  return result + note;
 }

 // ---------------------------------------------------------------- setup rows
 static array<string> RowValues(string faction)
 {
  array<string> values = {"", "standard", "ai"};
  foreach (ACR_DeckSubmission sub : Submitted())
   if (sub.m_Deck.m_sFaction == faction) values.Insert("sub:" + sub.m_iSerial.ToString());
  return values;
 }

 static string RowName(string faction, string value)
 {
  if (value == "standard") return "standard";
  if (value == "ai") return "AI picks";
  if (value.StartsWith("sub:"))
  {
   ACR_DeckSubmission sub = BySerial(value.Substring(4, value.Length() - 4).ToInt());
   if (!sub) return "a deck no longer here";
   return Short(sub.m_sPlayer, 12) + ": " + sub.m_Deck.m_sName;
  }
  ACR_DeckSubmission latest;
  string kind = Resolve(faction, latest);
  if (kind == "deck" && latest) return "auto (" + Short(latest.m_sPlayer, 10) + ": " + Short(latest.m_Deck.m_sName, 14) + ")";
  if (kind == "ai") return "auto (AI picks)";
  return "auto (standard)";
 }

 protected static string Short(string text, int chars)
 {
  if (text.Length() <= chars) return text;
  return text.Substring(0, chars - 1) + ".";
 }
}
