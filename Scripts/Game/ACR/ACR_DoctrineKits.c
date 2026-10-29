// Company starting kits (#38): each side picks its company on the setup screen (Blue company / Red
// company, -acrCompanyWest / -acrCompanyEast), and every side gets its company's kit at Start,
// whoever ends up playing it (Jacob, #38). The kit is a swap, not an extra: it is bought at the start
// at its list price, so it comes off the side's starting RP, and every company starts with the same
// value (its units at list price and its RP). The kits are the doctrine kits of the first prototype:
// - Rifle Company: the standard start (the default);
// - Mechanized (Blitz's kit): an APC or IFV in place of the starting truck (the truck's list
//   price back, the carrier's off);
// - Fire Support (Firepower's; key "weapons"): a mortar team and a forward observer team;
// - Combat Engineers (Fortress's): POSITIONS tier I Fortify positions at its front bases, built at
//   the start on the best scored spots facing the enemy (ACR_PositionSiting); an AI side mans them
//   with -acrFortifyAI 1, a player side as it likes;
// - Recon (Raider's): RECON_TEAMS 2-3 man recon teams, and its intel on the enemy commander's
//   doctrine starts at a hint (level 1) once an enemy AI commander is in command
//   (ACR_DoctrineIntel.m_fHeadStart).
// A kit item the side's list lacks, or that it can't afford, is left out (said in the log), so the
// value stays equal. An AI side fights to its company's doctrine unless the setup screen's doctrine
// row overrides it (ACR_AIDoctrine.Choose). Server only.
class ACR_KitTruck : Managed
{
 string m_sName;
 ResourceName m_sGroup;
 ResourceName m_sVehicle;
 bool m_bCrewed;
 ResourceName m_sPassengers;
 vector m_vVehiclePos;
 vector m_vGroupPos;
 int m_iPrice;
}

// What a side's kit gave: the units bought (named as they spawn, and the RP paid), the positions
// built, and the truck's price given back (Blitz).
class ACR_KitRecord : Managed
{
 string m_sKit;
 ref array<string> m_aUnits = {};
 ref array<int> m_aPaid = {};
 int m_iPositions;
 int m_iPositionRP;
 int m_iCredit;
 string m_sNotes;

 // The RP the kit took, net of the truck's price given back.
 int Spent()
 {
  int spent = m_iPositionRP - m_iCredit;
  foreach (int paid : m_aPaid) spent += paid;
  return spent;
 }

 string Describe()
 {
  string units = "none";
  if (!m_aUnits.IsEmpty())
  {
   units = "";
   foreach (int i, string name : m_aUnits)
   {
    if (i > 0) units += ", ";
    units += string.Format("%1 (%2 RP)", name, m_aPaid[i]);
   }
  }
  string line = string.Format("%1: units %2; positions %3 (%4 RP); truck's price back %5 RP; net %6 RP", ACR_DoctrineKits.CompanyName(ACR_DoctrineKits.CompanyOfKit(m_sKit)), units, m_iPositions, m_iPositionRP, m_iCredit, Spent());
  if (!m_sNotes.IsEmpty()) line += "; " + m_sNotes;
  return line;
 }
}

class ACR_DoctrineKits
{
 static const int RECON_TEAMS = 2;
 static const int POSITIONS = 2;
 // After the starting units spawn, a side's kit waits until its starting squads are in (a Fortress
 // sites its positions from one): checked every KIT_WAIT_STEP_MS, given anyway after KIT_WAIT_MAX_MS.
 // The pn12d playtest (27 Oct, Everon) had its squads register 9 s after spawning, and the fixed 5 s
 // wait gave Combat Engineers no positions ("no infantry squad of ours to site it from").
 static const int KIT_WAIT_STEP_MS = 1000;
 static const int KIT_WAIT_MAX_MS = 60000;
 static const int KIT_WAIT_LOG_MS = 5000;
 static const float CARRIER_NEAR = 0.15; // Blitz: carriers within 15% of the cheapest's price are as good (ACR_Variety)

 protected static ref map<string, string> s_Sides = new map<string, string>(); // faction -> the kit due (a doctrine key, as the first prototype's)
 protected static ref map<string, ref ACR_KitTruck> s_Trucks = new map<string, ref ACR_KitTruck>(); // Blitz: the truck left out
 static ref map<string, ref ACR_KitRecord> s_Given = new map<string, ref ACR_KitRecord>();
 protected static ref map<string, int> s_Starting = new map<string, int>(); // faction -> the squads its starting units make
 protected static float s_fWaitSince = -1;
 protected static float s_fWaitLogged;

 // ---------------------------------------------------------------- companies
 // The company keys and the kit each gives (rifle: none).
 static string KitOf(string company)
 {
  if (company == "mechanized") return "blitz";
  if (company == "weapons") return "firepower";
  if (company == "engineers") return "fortress";
  if (company == "recon") return "raider";
  return "";
 }

 static string CompanyOfKit(string kit)
 {
  if (kit == "blitz") return "mechanized";
  if (kit == "firepower") return "weapons";
  if (kit == "fortress") return "engineers";
  if (kit == "raider") return "recon";
  return "rifle";
 }

 // #38's names: the setup screen's company rows show only these.
 static string CompanyName(string company)
 {
  if (company == "mechanized") return "Mechanized";
  if (company == "weapons") return "Fire Support";
  if (company == "engineers") return "Combat Engineers";
  if (company == "recon") return "Recon";
  return "Rifle Company";
 }

 // The line under the setup screen's Economy rows: "Combat Engineers: two tier I positions at its
 // front" (with "Blue: ", at most ACR_Setup.ROW_CHARS).
 static string CompanySummary(string company)
 {
  string what = "the standard start";
  if (company == "mechanized") what = "an APC or IFV in place of its truck";
  else if (company == "weapons") what = "a mortar team and an observer team";
  else if (company == "engineers") what = "two tier I positions at its front";
  else if (company == "recon") what = "two recon teams and early enemy intel";
  return CompanyName(company) + ": " + what;
 }

 // The host option for the side this faction plays: "acrCompanyWest" or "acrCompanyEast".
 static string OptionFor(string factionKey)
 {
  if (factionKey == ACR_FactionSelection.East()) return "acrCompanyEast";
  return "acrCompanyWest";
 }

 // The company this faction picked ("rifle" when none or an unknown one).
 static string CompanyOf(string factionKey)
 {
  string value;
  if (!ACR_Options.Get(OptionFor(factionKey), value)) return "rifle";
  value.ToLower();
  // Fire Support's key is "weapons" (its first name).
  if (value == "firesupport") value = "weapons";
  if (KitOf(value).IsEmpty()) return "rifle";
  return value;
 }

 // Some side picked a company with a kit.
 static bool Enabled()
 {
  return !KitOf(CompanyOf(ACR_FactionSelection.West())).IsEmpty() || !KitOf(CompanyOf(ACR_FactionSelection.East())).IsEmpty();
 }

 // The kit due for this side ("" none), for tests.
 static string DueKit(string faction)
 {
  string kit;
  s_Sides.Find(faction, kit);
  return kit;
 }

 // A kit is due for the side and not given yet (its AI commander buys nothing until it is).
 static bool Pending(string faction) { return s_Sides.Contains(faction) && !s_Given.Contains(faction); }

 // At the start, before the starting units spawn: every side's company kit, whoever plays it.
 static void Plan(ACR_Manager manager)
 {
  s_Sides.Clear();
  s_Trucks.Clear();
  s_Given.Clear();
  s_Starting.Clear();
  s_fWaitSince = -1;
  if (!Enabled() || !manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All)
  {
   if (!settings || !ACR_FactionSelection.IsSelected(settings.m_sFaction)) continue;
   string company = CompanyOf(settings.m_sFaction);
   string kit = KitOf(company);
   if (kit.IsEmpty())
   {
    PrintFormat("[ACR] COMPANY KIT %1: none (%2)", settings.m_sFaction, CompanyName(company));
    continue;
   }
   s_Sides.Set(settings.m_sFaction, kit);
   PrintFormat("[ACR] COMPANY KIT %1: the %2 kit due", settings.m_sFaction, CompanyName(company));
  }
 }

 // Blitz: this starting unit is the side's truck, left out for a carrier (bought by Give; if that
 // fails, the truck is spawned after all). Once per side, and only when its list has a carrier it
 // can pay for with the truck's price back.
 static bool SwapsTruck(ACR_Manager manager, string faction, string name, ResourceName group, ResourceName vehicle, bool crewed, ResourceName passengers, vector vehiclePos, vector groupPos)
 {
  string doctrine;
  if (!s_Sides.Find(faction, doctrine) || doctrine != "blitz" || s_Trucks.Contains(faction)) return false;
  if (vehicle.IsEmpty() || !group.IsEmpty() || !passengers.IsEmpty()) return false;
  ACR_FactionData data = manager.m_Bases.GetFaction(faction);
  int truck = TruckPrice(data, faction, vehicle);
  if (truck < 0) return false;
  int index = CarrierIndex(data, faction);
  if (index < 0)
  {
   PrintFormat("[ACR] COMPANY KIT %1: no APC or IFV in its list, so it keeps %2", faction, name);
   return false;
  }
  ACR_RequisitionEntry carrier = data.m_aRequisitions[index];
  if (data.m_iRP + truck < carrier.m_iCost)
  {
   PrintFormat("[ACR] COMPANY KIT %1: %2 (%3 RP) is more than its %4 RP and %5's %6, so it keeps the truck", faction, carrier.m_sName, carrier.m_iCost, data.m_iRP, name, truck);
   return false;
  }
  ACR_KitTruck kept = new ACR_KitTruck();
  kept.m_sName = name;
  kept.m_sGroup = group;
  kept.m_sVehicle = vehicle;
  kept.m_bCrewed = crewed;
  kept.m_sPassengers = passengers;
  kept.m_vVehiclePos = vehiclePos;
  kept.m_vGroupPos = groupPos;
  kept.m_iPrice = truck;
  s_Trucks.Set(faction, kept);
  PrintFormat("[ACR] COMPANY KIT %1: %2 (%3 RP) left out for %4 (%5 RP)", faction, name, truck, carrier.m_sName, carrier.m_iCost);
  return true;
 }

 // The list price of 'vehicle' as one of the side's transport trucks (an unarmed truck it can
 // buy); a transport truck its list doesn't have (FIA starts with the USSR's Ural, srv:doctrinekits)
 // counts at its cheapest transport truck's price; else -1 (a jeep, a gun truck: never swapped).
 static int TruckPrice(ACR_FactionData data, string faction, ResourceName vehicle)
 {
  int cheapest = -1;
  foreach (ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (!entry.m_bTransport || entry.m_sVehicle.IsEmpty()) continue;
   if (entry.m_sVehicle == vehicle || ACR_FactionCatalog.ResolvePrefab(faction, entry.m_sVehicle, EEntityCatalogType.VEHICLE) == vehicle) return entry.m_iCost;
   string leaf = FilePath.StripPath(entry.m_sVehicle);
   leaf.ToLower();
   if (leaf.Contains("_transport") && (cheapest < 0 || entry.m_iCost < cheapest)) cheapest = entry.m_iCost;
  }
  string own = FilePath.StripPath(vehicle);
  own.ToLower();
  if (own.Contains("_transport")) return cheapest;
  return -1;
 }

 // A starting unit spawned for this side (ACR_Mode.SpawnStartingUnits): the squads it makes, its
 // group, its vehicle's crew and its passengers each register as one.
 static void CountStarting(string faction, ResourceName group, ResourceName vehicle, bool crewed, ResourceName passengers)
 {
  int squads = StartingOf(faction);
  if (!group.IsEmpty()) squads++;
  if (crewed && !vehicle.IsEmpty()) squads++;
  if (!passengers.IsEmpty()) squads++;
  s_Starting.Set(faction, squads);
 }

 static int StartingOf(string faction)
 {
  int squads;
  s_Starting.Find(faction, squads);
  return squads;
 }

 static void SetStartingForTest(string faction, int squads) { s_Starting.Set(faction, squads); }

 // The squads of this side registered so far.
 static int SquadsOf(ACR_Manager manager, string faction)
 {
  int squads;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad.GetFaction() && squad.GetFaction().GetFactionKey() == faction) squads++;
  return squads;
 }

 // After the starting units spawned: every due kit once its side's starting squads are in.
 static void GiveAll()
 {
  s_fWaitSince = GetGame().GetWorld().GetWorldTime();
  s_fWaitLogged = s_fWaitSince;
  if (WaitStep(s_fWaitSince)) GetGame().GetCallqueue().CallLater(WaitTick, KIT_WAIT_STEP_MS, true);
 }

 protected static void WaitTick()
 {
  if (!WaitStep(GetGame().GetWorld().GetWorldTime())) GetGame().GetCallqueue().Remove(WaitTick);
 }

 // Gives every due kit whose side's starting squads are in (or that waited KIT_WAIT_MAX_MS). True
 // while one is still waiting.
 static bool WaitStep(float now)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  bool waiting;
  bool log = now - s_fWaitLogged >= KIT_WAIT_LOG_MS;
  foreach (string faction, string kit : s_Sides)
  {
   if (s_Given.Contains(faction)) continue;
   int want = StartingOf(faction);
   int have = SquadsOf(manager, faction);
   float waited = now - s_fWaitSince;
   if (have < want && waited < KIT_WAIT_MAX_MS)
   {
    waiting = true;
    if (log) PrintFormat("[ACR] COMPANY KIT %1: waiting for its starting squads (%2 of %3 in, %4 s)", faction, have, want, (int)(waited / 1000));
    continue;
   }
   if (have < want) PrintFormat("[ACR] COMPANY KIT %1: %2 of %3 starting squads in after %4 s, given anyway", faction, have, want, KIT_WAIT_MAX_MS / 1000);
   ACR_KitRecord record = Give(manager, faction, kit);
   PrintFormat("[ACR] COMPANY KIT %1 given: %2 (%3 RP left; %4 s after the start, %5 of %6 squads in)", faction, record.Describe(), manager.m_Bases.GetRP(faction), (int)(waited / 1000), have, want);
  }
  if (log) s_fWaitLogged = now;
  return waiting;
 }

 // kit: a doctrine key, as KitOf gives ("balanced" or "": the standard start).
 static ACR_KitRecord Give(ACR_Manager manager, string faction, string kit)
 {
  ACR_KitRecord record = new ACR_KitRecord();
  s_Given.Set(faction, record);
  record.m_sKit = kit;
  ACR_Base hq = HQOf(faction);
  ACR_FactionData data = manager.m_Bases.GetFaction(faction);
  if (kit == "blitz") GiveCarrier(manager, faction, data, hq, record);
  else if (kit == "firepower")
  {
   Buy(manager, faction, data, hq, MortarIndex(data), "mortar team", record);
   Buy(manager, faction, data, hq, ObserverIndex(data), "observer team", record);
  }
  else if (kit == "fortress") DigIn(manager, faction, record);
  else if (kit == "raider")
  {
   for (int i = 0; i < RECON_TEAMS; i++) Buy(manager, faction, data, hq, ReconIndex(data), "recon team", record);
   ACR_Intel intel = manager.IntelFor(faction);
   if (intel)
   {
    intel.Doctrine().m_fHeadStart = ACR_DoctrineIntel.HINT;
    Note(record, "its intel on the enemy doctrine starts at a hint");
   }
  }
  return record;
 }

 // Blitz: the truck's price back and the carrier bought; without the carrier after all, the truck
 // as it was.
 protected static void GiveCarrier(ACR_Manager manager, string faction, ACR_FactionData data, ACR_Base hq, ACR_KitRecord record)
 {
  ACR_KitTruck truck;
  if (!s_Trucks.Find(faction, truck))
  {
   // No starting truck to swap (Red's Arland start is two rifle squads: pn10, 26 Oct, the kit came
   // out empty): the carrier at its list price, out of the starting RP like any kit.
   Note(record, "no starting truck to swap: the carrier at its list price");
   Buy(manager, faction, data, hq, CarrierIndex(data, faction), "APC or IFV", record);
   return;
  }
  data.m_iRP += truck.m_iPrice;
  record.m_iCredit = truck.m_iPrice;
  if (Buy(manager, faction, data, hq, CarrierIndex(data, faction), "APC or IFV", record)) return;
  data.m_iRP -= truck.m_iPrice;
  if (data.m_iRP < 0) data.m_iRP = 0;
  record.m_iCredit = 0;
  manager.m_sSpawningFaction = faction;
  manager.SpawnUnit(truck.m_sName, truck.m_sGroup, truck.m_sVehicle, truck.m_bCrewed, truck.m_sPassengers, truck.m_vVehiclePos, truck.m_vGroupPos);
  manager.m_sSpawningFaction = "";
  Note(record, truck.m_sName + " spawned after all");
 }

 // Buys entry 'index' at 'hq' as a commander would (ACR_Requisition.PurchaseAt: its price off the
 // RP, its stock and price step taken), noting it in 'record'.
 protected static bool Buy(ACR_Manager manager, string faction, ACR_FactionData data, ACR_Base hq, int index, string what, ACR_KitRecord record)
 {
  if (index < 0)
  {
   Note(record, "no " + what + " in its list");
   return false;
  }
  if (!hq)
  {
   Note(record, "no base to deploy the " + what + " at");
   return false;
  }
  ACR_RequisitionEntry entry = data.m_aRequisitions[index];
  int before = data.m_iRP;
  string result = manager.m_Requisition.PurchaseAt(faction, index, hq);
  int paid = before - data.m_iRP;
  if (paid <= 0)
  {
   Note(record, what + ": " + result);
   return false;
  }
  record.m_aUnits.Insert(string.Format("%1 %2", entry.m_sName, entry.m_iNamed));
  record.m_aPaid.Insert(paid);
  return true;
 }

 // Fortress: POSITIONS tier I positions at our front bases (one at each, or all at the one there
 // is), each on the best scored spot facing the enemy that takes one, built now at Cost(1) each.
 protected static void DigIn(ACR_Manager manager, string faction, ACR_KitRecord record)
 {
  array<ACR_Base> fronts = {};
  array<vector> enemies = {};
  // The AI commander's view of the front (on a side without one, an instance that is never ticked).
  manager.EnsureAICommander(faction).FrontBasesOf(fronts, enemies, POSITIONS);
  if (fronts.IsEmpty())
  {
   Note(record, "no front base (no enemy base known)");
   return;
  }
  for (int i = 0; i < POSITIONS; i++)
  {
   int f = i % fronts.Count();
   string why = DigInAt(manager, faction, fronts[f], enemies[f], record);
   if (!why.IsEmpty()) Note(record, string.Format("no position at %1: %2", fronts[f].m_sName, why));
  }
 }

 protected static string DigInAt(ACR_Manager manager, string faction, ACR_Base front, vector enemyPos, ACR_KitRecord record)
 {
  // The plan sites it as this squad would dig it (facing, its upgrades' fit); nobody walks there.
  ACR_Squad squad = InfantryNear(manager, faction, front.GetOrigin());
  if (!squad) return "no infantry squad of ours to site it from";
  int cost = ACR_Fortify.Cost(1);
  array<ref ACR_PositionSite> sites = {};
  int nearFields;
  ACR_PositionSiting.Candidates(squad, faction, front.GetOrigin(), enemyPos, sites, nearFields);
  string refusal = "no spot";
  foreach (ACR_PositionSite site : sites)
  {
   ACR_FortifyPlan plan = ACR_Fortify.Plan(squad, site.m_vPos, refusal, enemyPos);
   if (!plan) continue;
   // A new position only: by the one just built the plan is its upgrade (srv:doctrinekits, FIA:
   // the second position upgraded the first).
   if (plan.m_Target)
   {
    refusal = "next to the position just built";
    continue;
   }
   if (!manager.m_Bases.Spend(faction, cost)) return string.Format("not enough RP (%1 needed)", cost);
   string reason;
   ACR_Fortification fort = ACR_Fortify.Build(plan, reason);
   if (!fort)
   {
    manager.m_Bases.Refund(faction, cost);
    refusal = reason;
    continue;
   }
   record.m_iPositions++;
   record.m_iPositionRP += cost;
   PrintFormat("[ACR] COMPANY KIT %1: a position on the approach to %2 at %3 (%4)", faction, front.m_sName, fort.m_vPos, ACR_PositionSiting.Breakdown(site));
   return "";
  }
  return string.Format("%1 spots, none taken (last: %2)", sites.Count(), refusal);
 }

 static ACR_Squad InfantryNear(ACR_Manager manager, string faction, vector pos)
 {
  ACR_Squad best;
  float bestDistance = float.MAX;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction || squad.CountAlive() <= 0 || !squad.GetAnchor()) continue;
   if (!ACR_Fortify.Infantry(squad)) continue;
   float d = vector.DistanceXZ(squad.GetAnchor().GetOrigin(), pos);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = squad;
  }
  return best;
 }

 // The side's HQ, else another of its bases that can deploy units.
 static ACR_Base HQOf(string faction)
 {
  ACR_Base any;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner != faction || !ACR_LogisticsRules.CanDeploy(base)) continue;
   if (ACR_LogisticsRules.IsHQ(base)) return base;
   if (!any) any = base;
  }
  return any;
 }

 // An APC or IFV in stock that seats a squad (a BTR-70 over the BRDM-2 scout car: pn10, 26 Oct),
 // else any carrier there is (-1: none): the cheapest, or with variety on (ACR_Variety) one of
 // those within CARRIER_NEAR of its price, seeded by the side, so every call in a match agrees.
 static int CarrierIndex(ACR_FactionData data, string faction = "")
 {
  bool squadCarrier;
  foreach (ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (entry.m_bCarrier && entry.m_iRemaining > 0 && entry.m_bSquadCarrier) squadCarrier = true;
  }
  array<int> indices = {};
  array<float> scores = {};
  foreach (int i, ACR_RequisitionEntry candidate : data.m_aRequisitions)
  {
   if (!candidate.m_bCarrier || candidate.m_iRemaining <= 0 || candidate.m_bSquadCarrier != squadCarrier) continue;
   indices.Insert(i);
   scores.Insert(-candidate.m_iCost);
  }
  int pick = ACR_Variety.Pick(scores, "kit carrier " + faction, CARRIER_NEAR);
  if (pick < 0) return -1;
  return indices[pick];
 }

 static int MortarIndex(ACR_FactionData data)
 {
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (entry.m_bMortarTeam && entry.m_iRemaining > 0) return i;
  }
  return -1;
 }

 static int ObserverIndex(ACR_FactionData data)
 {
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (entry.m_bObserverTeam && entry.m_iRemaining > 0) return i;
  }
  return -1;
 }

 // The cheapest 2-3 man recon team in stock: a scout or sniper team (by formation or name).
 static int ReconIndex(ACR_FactionData data)
 {
  int best = -1;
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (entry.m_iRemaining <= 0 || entry.m_bObserverTeam || !ACR_AICommander.IsSmallTeamEntry(entry)) continue;
   if (!IsRecon(entry)) continue;
   if (best < 0 || entry.m_iCost < data.m_aRequisitions[best].m_iCost) best = i;
  }
  return best;
 }

 static bool IsRecon(ACR_RequisitionEntry entry)
 {
  if (entry.m_eFormation == ACR_EFormation.SCOUT || entry.m_eFormation == ACR_EFormation.SNIPER) return true;
  string name = entry.m_sName;
  name.ToLower();
  return name.Contains("recon") || name.Contains("scout") || name.Contains("sniper");
 }

 protected static void Note(ACR_KitRecord record, string note)
 {
  if (!record.m_sNotes.IsEmpty()) record.m_sNotes += "; ";
  record.m_sNotes += note;
 }

 // Tests: a kit due for 'faction' from 'doctrine', as Plan would set it.
 static void DueForTest(string faction, string doctrine)
 {
  s_Sides.Set(faction, doctrine);
  s_Trucks.Remove(faction);
  s_Given.Remove(faction);
 }

 static void ResetForTest()
 {
  s_Sides.Clear();
  s_Trucks.Clear();
  s_Given.Clear();
  s_Starting.Clear();
  s_fWaitSince = -1;
 }

 static void BeginWaitForTest(float now)
 {
  s_fWaitSince = now;
  s_fWaitLogged = now;
 }
}
