// -acrDeckTest 1 (server-capable, ~20 s): the deck builder (#106, ACR_Deck / ACR_Decks). The deck
// code round-trips; the standard deck is today's list; a budget trims a built deck, locked trucks staying;
// the AI's deck fits its budget with its minimums and utilities, is the same for the same match
// seed and differs between seeds (the standard deck is never trimmed); custom cards are priced and checked from the faction's own
// soldiers; a handed-in deck becomes the faction's buy list with its custom card; decks off leaves
// today's list; only the host's hand-in decides a side's deck on auto (a player's is an offer the row
// picks), none after Start, and FIA doesn't follow Blue's row; a custom team riding a rare vehicle has
// no more than its stock and shares it. Logs every faction's standard deck in points (to calibrate the budget).
class ACR_TestDecks
{
 static const int CHECKS = 13;

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick != 5) return;
  string west = ACR_FactionSelection.West();
  if (west.IsEmpty()) west = "US";
  ACR_FactionSettings settings = ACR_FactionSettings.Find(west);
  bool stepped = ACR_Pricing.Stepped(settings);

  // Every faction's standard deck in points.
  array<string> factions = {"US", "USSR", "FIA", "RHS_USAF", "RHS_AFRF"};
  foreach (string faction : factions)
  {
   if (!GetGame().GetFactionManager().GetFactionByKey(faction)) continue;
   array<ref ACR_RequisitionEntry> today = {};
   ACR_FactionCatalog.BuildRequisitions(faction, ACR_FactionSettings.Find(faction), today);
   array<ref ACR_RequisitionEntry> extras = {};
   ACR_DeckCatalog.Extras(faction, extras, stepped);
   int counts;
   string byRole;
   for (int r = 0; r < ACR_DeckRules.RoleCount(); r++)
   {
    int rolePoints;
    int roleCards;
    foreach (ACR_RequisitionEntry e : today)
    {
     if (ACR_DeckRules.RoleOf(e) != r) continue;
     rolePoints += ACR_DeckRules.Points(e);
     roleCards++;
    }
    if (roleCards > 0) byRole += string.Format(" %1 %2/%3", ACR_DeckRules.RoleName(r), roleCards, rolePoints);
    counts += roleCards;
   }
   PrintFormat("[ACR] DECK TEST standard deck %1: %2 cards, %3 points (%4 extra cards) |%5", faction, counts, ACR_Decks.Points(today), extras.Count(), byRole);
  }

  // 1. The deck code.
  ACR_DeckData deck = new ACR_DeckData();
  deck.m_sFaction = west;
  deck.m_sName = "Test | deck ^ one";
  deck.m_aCards = {"Rifle Squad", "M998 (M2)"};
  ACR_CustomCard card = new ACR_CustomCard();
  card.m_sName = "Heavy Team";
  card.m_aSoldiers = {"a", "b", "b"};
  card.m_sTransport = "M923A1";
  deck.m_aCustom.Insert(card);
  ACR_DeckData back = ACR_DeckData.FromText(deck.ToText());
  bool round = back && back.m_sName == "Test  deck  one" && back.m_aCards.Count() == 2 && back.m_aCards[1] == "M998 (M2)" && back.m_aCustom.Count() == 1 && back.m_aCustom[0].m_aSoldiers.Count() == 3 && back.m_aCustom[0].m_sTransport == "M923A1" && back.ToText() == deck.Copy().ToText();
  ACR_TestRegress.Check("deck: the code round-trips, separators taken out of names", round, deck.ToText());

  // 2. The standard deck, unlimited: today's list.
  ACR_Options.Set("acrDeckPoints", "0");
  ACR_Options.Set(ACR_Decks.OptionFor(west), "standard");
  array<ref ACR_RequisitionEntry> plain = {};
  ACR_FactionCatalog.BuildRequisitions(west, settings, plain);
  array<ref ACR_RequisitionEntry> standard = {};
  ACR_FactionCatalog.BuildRequisitions(west, settings, standard);
  ACR_Decks.Apply(west, settings, standard);
  ACR_TestRegress.Check("deck: the standard deck with no budget is today's list", Names(plain) == Names(standard), string.Format("%1 vs %2 cards", plain.Count(), standard.Count()));

  // 3. The standard deck is never trimmed; a built deck over the budget is, locked trucks staying.
  ACR_Options.Set("acrDeckPoints", "40");
  array<ref ACR_RequisitionEntry> untrimmed = {};
  ACR_FactionCatalog.BuildRequisitions(west, settings, untrimmed);
  ACR_Decks.Apply(west, settings, untrimmed);
  array<bool> keepAll = {};
  foreach (ACR_RequisitionEntry k : plain) keepAll.Insert(true);
  ACR_Decks.Fit(plain, keepAll, 40);
  array<ref ACR_RequisitionEntry> trimmed = {};
  foreach (int ki, ACR_RequisitionEntry kk : plain) if (keepAll[ki]) trimmed.Insert(kk);
  bool locksKept = true;
  foreach (ACR_RequisitionEntry p : plain)
   if (ACR_DeckRules.Locked(p) && !ACR_DeckCatalog.Find(trimmed, p.m_sName)) locksKept = false;
  bool trimOk = Names(untrimmed) == Names(plain) && ACR_Decks.Points(trimmed) <= 40 && trimmed.Count() < plain.Count() && locksKept;
  ACR_TestRegress.Check("deck: at 40 points the standard deck stays whole; a built deck is trimmed to fit, locked trucks kept", trimOk, string.Format("standard %1 cards; built %2 cards, %3 points, locks kept %4", untrimmed.Count(), trimmed.Count(), ACR_Decks.Points(trimmed), locksKept));

  // 4. The AI's deck: budget, minimums, utilities; the same seed the same deck.
  ACR_Options.Set("acrDeckPoints", "100");
  // (100 rather than the default 150: a tighter budget is the harder case for the minimums.)
  ACR_Options.Set(ACR_Decks.OptionFor(west), "ai");
  string seedWas;
  bool hadSeed = ACR_Options.Get("acrSeed", seedWas);
  ACR_Options.Set("acrSeed", "4242");
  array<ref ACR_RequisitionEntry> ai = AIDeck(west, settings);
  array<ref ACR_RequisitionEntry> again = AIDeck(west, settings);
  int infantry, teams;
  bool mortar, observer, ride;
  foreach (ACR_RequisitionEntry a : ai)
  {
   if (ACR_DeckRules.RoleOf(a) == ACR_EDeckRole.INFANTRY) infantry++;
   if (ACR_DeckRules.RoleOf(a) == ACR_EDeckRole.TEAMS) teams++;
   if (a.m_bMortarTeam) mortar = true;
   if (a.m_bObserverTeam) observer = true;
   if (a.m_bTransport && !ACR_DeckRules.Locked(a)) ride = true;
  }
  bool anyTeams;
  foreach (ACR_RequisitionEntry t : plain) if (ACR_DeckRules.RoleOf(t) == ACR_EDeckRole.TEAMS) anyTeams = true;
  bool aiOk = ACR_Decks.Points(ai) <= 100 && infantry >= 2 && (teams >= 1 || !anyTeams) && mortar && observer && ride;
  ACR_TestRegress.Check("deck: the AI's deck fits 100 points with 2+ infantry, a weapons team, mortar, observers and a ride", aiOk, string.Format("%1 points, %2 infantry, %3 teams, mortar %4, observer %5, ride %6: %7", ACR_Decks.Points(ai), infantry, teams, mortar, observer, ride, Names(ai)));
  ACR_TestRegress.Check("deck: the same match seed builds the same AI deck", Names(ai) == Names(again), "");
  bool differs;
  for (int s = 1; s <= 6 && !differs; s++)
  {
   ACR_Options.Set("acrSeed", (4242 + s * 977).ToString());
   if (Names(AIDeck(west, settings)) != Names(ai)) differs = true;
  }
  ACR_TestRegress.Check("deck: another seed builds another AI deck (within 6 seeds)", differs, "");
  if (hadSeed) ACR_Options.Set("acrSeed", seedWas);
  else ACR_Options.Unset("acrSeed");

  // 5. Custom cards: priced from the faction's own soldiers; strangers, too many men refused.
  array<ref ACR_DeckSoldier> pool = {};
  ACR_DeckCatalog.Soldiers(west, plain, pool);
  ACR_CustomCard mine = new ACR_CustomCard();
  mine.m_sName = "Test Custom Team";
  int expected;
  for (int m = 0; m < 4 && m < pool.Count(); m++)
  {
   mine.m_aSoldiers.Insert(pool[m].m_sId);
   expected += pool[m].m_iRP;
  }
  if (expected < 6) expected = 6;
  string ride1;
  foreach (ACR_RequisitionEntry v : plain) if (ACR_DeckCatalog.IsTransport(v)) { ride1 = v.m_sName; break; }
  string error;
  ACR_RequisitionEntry made = ACR_DeckCatalog.CustomEntry(west, mine, pool, plain, stepped, error);
  int madeCost = -1;
  if (made) madeCost = made.m_iBaseCost;
  bool customOk = made && made.m_aSoldiers.Count() == 4 && madeCost == expected && made.m_bCustom && made.m_sVehicle.IsEmpty();
  ACR_TestRegress.Check("deck: a custom team of 4 of the faction's own soldiers is priced from its men", customOk, string.Format("%1 soldiers in the pool, RP %2 (expected %3), %4", pool.Count(), madeCost, expected, error));
  ACR_CustomCard stranger = mine.Copy();
  stranger.m_aSoldiers.Insert("Not_A_Soldier_Of_Ours");
  string strangerError, bigError, rideError;
  ACR_RequisitionEntry refused = ACR_DeckCatalog.CustomEntry(west, stranger, pool, plain, stepped, strangerError);
  ACR_CustomCard big = mine.Copy();
  for (int b = 0; b < 12; b++) big.m_aSoldiers.Insert(mine.m_aSoldiers[0]);
  ACR_RequisitionEntry tooBig = ACR_DeckCatalog.CustomEntry(west, big, pool, plain, stepped, bigError);
  ACR_RequisitionEntry riding;
  if (!ride1.IsEmpty())
  {
   ACR_CustomCard rider = mine.Copy();
   rider.m_sTransport = ride1;
   riding = ACR_DeckCatalog.CustomEntry(west, rider, pool, plain, stepped, rideError);
  }
  bool checks = !refused && !tooBig && (ride1.IsEmpty() || (riding && riding.m_bBoard && !riding.m_sVehicle.IsEmpty() && riding.m_iBaseCost > expected));
  ACR_TestRegress.Check("deck: a stranger and 16 men are refused; a ride adds its vehicle and price", checks, string.Format("stranger: %1 | big: %2 | ride %3: %4", strangerError, bigError, ride1, rideError));

  // 6. A handed-in deck becomes the buy list (the real one, from GetFaction), with its custom card.
  ACR_Options.Set("acrDeckPoints", "0");
  ACR_Options.Unset(ACR_Decks.OptionFor(west));
  ACR_DeckData handed = new ACR_DeckData();
  handed.m_sFaction = west;
  handed.m_sName = "Test Hand In";
  handed.m_aCards.Insert(plain[0].m_sName);
  handed.m_aCards.Insert("A Card No Mod Has");
  handed.m_aCustom.Insert(mine);
  string answer = ACR_Decks.Accept(0, handed.ToText(), true, false);
  ACR_Manager manager = ACR_Manager.GetInstance();
  manager.m_Bases.ForgetFactions();
  ACR_FactionData data = manager.m_Bases.GetFaction(west);
  bool hasCustom = ACR_DeckCatalog.Find(data.m_aRequisitions, "Test Custom Team") != null;
  bool hasFirst = ACR_DeckCatalog.Find(data.m_aRequisitions, plain[0].m_sName) != null;
  bool onlyDeck = true;
  foreach (ACR_RequisitionEntry kept : data.m_aRequisitions)
   if (!kept.m_bCustom && !ACR_DeckRules.Locked(kept) && kept.m_sName != plain[0].m_sName) onlyDeck = false;
  ACR_TestRegress.Check("deck: a handed-in deck is the buy list: its card, its custom team, the locked trucks", hasCustom && hasFirst && onlyDeck && answer.Contains("A Card No Mod Has"), string.Format("%1 | %2 | applied: %3", answer, Names(data.m_aRequisitions), ACR_Decks.Applied(west)));

  // 8. Ownership (review of #238): a player's deck is an offer. On auto the host's deck still
  // decides; the row picks the player's. Nothing is taken after Start.
  ACR_DeckData offered = new ACR_DeckData();
  offered.m_sFaction = west;
  offered.m_sName = "Test Offer";
  offered.m_aCards.Insert(plain[plain.Count() - 1].m_sName);
  string offerAnswer = ACR_Decks.Accept(77, offered.ToText(), false, false);
  ACR_DeckSubmission autoSub;
  string autoKind = ACR_Decks.Resolve(west, autoSub);
  bool hostStill = autoKind == "deck" && autoSub && autoSub.m_iPlayer == 0;
  ACR_DeckSubmission offerSub;
  foreach (ACR_DeckSubmission each : ACR_Decks.Submitted()) if (each.m_iPlayer == 77) offerSub = each;
  bool rowPicks;
  if (offerSub)
  {
   ACR_Options.Set(ACR_Decks.OptionFor(west), "sub:" + offerSub.m_iSerial.ToString());
   ACR_DeckSubmission picked;
   rowPicks = ACR_Decks.Resolve(west, picked) == "deck" && picked == offerSub;
   ACR_Options.Unset(ACR_Decks.OptionFor(west));
  }
  ACR_TestRegress.Check("deck: a player's hand-in is an offer: auto keeps the host's deck, the deck row picks the offer", hostStill && rowPicks && offerAnswer.Contains("Offered to the host"), string.Format("%1 | auto %2 | row %3", offerAnswer, autoKind, rowPicks));
  int countBefore = ACR_Decks.Submitted().Count();
  string lateAnswer = ACR_Decks.Accept(0, offered.ToText(), true, true);
  bool fiaOwn = ACR_Decks.OptionFor("FIA") == "" || ACR_FactionSelection.West() == "FIA" || ACR_FactionSelection.East() == "FIA";
  ACR_TestRegress.Check("deck: no hand-in after Start; FIA has no deck row of its own side's", ACR_Decks.Submitted().Count() == countBefore && lateAnswer.Contains("has started") && fiaOwn, lateAnswer);

  // 9. A custom team riding a rare vehicle (fixed stock) is rare with no more than the ride's stock,
  // and its purchases take the ride card's stock (ACR_Requisition.RideOf).
  ACR_RequisitionEntry rareRide;
  foreach (ACR_RequisitionEntry rv : plain) if (ACR_DeckCatalog.IsTransport(rv) && rv.m_bRare) { rareRide = rv; break; }
  if (!rareRide)
   ACR_TestRegress.Check("deck: a custom team on a rare ride keeps the ride's stock (no rare ride in this list)", true, "");
  else
  {
   ACR_CustomCard carried = mine.Copy();
   carried.m_sName = "Test Carried Team";
   carried.m_sTransport = rareRide.m_sName;
   string carriedError;
   ACR_RequisitionEntry carriedEntry = ACR_DeckCatalog.CustomEntry(west, carried, pool, plain, stepped, carriedError);
   ACR_FactionData rideData = new ACR_FactionData();
   rideData.m_aRequisitions.Insert(rareRide);
   if (carriedEntry) rideData.m_aRequisitions.Insert(carriedEntry);
   bool rareOk = carriedEntry && carriedEntry.m_bRare && !carriedEntry.m_bStepped && carriedEntry.m_iStock <= rareRide.m_iStock && carriedEntry.m_iRemaining <= rareRide.m_iRemaining && ACR_Requisition.RideOf(rideData, carriedEntry) == rareRide;
   ACR_TestRegress.Check("deck: a custom team on a rare ride is rare, within the ride's stock, and shares it", rareOk, string.Format("%1: %2", rareRide.m_sName, carriedError));
  }

  // 7. Decks off: today's list.
  ACR_Options.Set("acrDecks", "0");
  manager.m_Bases.ForgetFactions();
  data = manager.m_Bases.GetFaction(west);
  ACR_TestRegress.Check("deck: decks off leaves today's list", Names(data.m_aRequisitions) == Names(plain), string.Format("%1 vs %2 cards", data.m_aRequisitions.Count(), plain.Count()));

  ACR_Options.Unset("acrDecks");
  ACR_Options.Unset("acrDeckPoints");
  ACR_Options.Unset(ACR_Decks.OptionFor(west));
  ACR_Decks.Reset();
  manager.m_Bases.ForgetFactions();
  ACR_TestRegress.Complete("deck", CHECKS);
 }

 protected static array<ref ACR_RequisitionEntry> AIDeck(string faction, ACR_FactionSettings settings)
 {
  array<ref ACR_RequisitionEntry> list = {};
  ACR_FactionCatalog.BuildRequisitions(faction, settings, list);
  ACR_Decks.Apply(faction, settings, list);
  return list;
 }

 protected static string Names(array<ref ACR_RequisitionEntry> list)
 {
  string names;
  foreach (ACR_RequisitionEntry entry : list) names += entry.m_sName + ";";
  return names;
 }
}
