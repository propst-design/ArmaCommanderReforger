// Town support (COIN, 0.8.0 #244): every town leans towards the army or the insurgency.
//  - A town is a base that pays income (not a built FOB). Its support runs 0 (backs the army) to 100
//    (backs the insurgency) in four tiers, as the conduct standing's: Loyal < 25, Quiet < 50,
//    Restless < 75, Hostile; a tier is left only HYSTERESIS past its bound. Start: army-held
//    START_HELD, open START_OPEN, FIA pocket START_POCKET, the stronghold START_STRONGHOLD (+-5).
//  - What moves it, slowly (every TICK_S, rates per minute; a tier takes tens of minutes):
//    towards the army: PRESENCE_MIN+ army men in the town (its radius, at least RADIUS_MIN) with no
//    FIA there and no harm or fight there for CALM_MS (STATIONED); FIA harm (below).
//    towards the insurgency: no army man there and not held by the army (EMPTY, only up to
//    EMPTY_CAP: absence alone never makes a town Hostile); FIA men there unopposed or an FIA pocket
//    (FIA_WORKS); prisoners held at a hideout within PRISON_REACH (PRISONERS); army harm (below).
//    An army-held town with nobody in it holds steady.
//  - Harm, at once, to every town within HARM_REACH of it: the events FIA grievance already hears
//    (ACR_FIAGrievance: a civilian killed, a building destroyed, a strike support landing in a town)
//    times HARM_SCALE, towards the insurgency when the army did it, towards the army when FIA did.
//    It also calms the town for CALM_MS: a fight's ruins win nobody over.
//  - Effects: the army earns HostileIncome() (host option, default half) of a Hostile town's value
//    (ACR_BaseSystem.GetIncome). Informants: FIA within INFORM_REACH of a Loyal town shows on the
//    army's map (ACR_Intel.Tip). The insurgency's side of the tiers (recruits, income) comes with
//    item 3 (#244 stage 1) and the insurgent seat (#97).
//  - What the army sees: the tier of towns it holds or has men in, and the last tier it saw
//    elsewhere ("?" if never); a tier change it sees gets a feed line with the reason.
//  - Only in COIN (ACR_Coin): an ordinary two-army match keeps the conduct standing as it is.
class ACR_TownSupport
{
 static const int LOYAL = 0;
 static const int QUIET = 1;
 static const int RESTLESS = 2;
 static const int HOSTILE = 3;
 static const float HYSTERESIS = 3;
 static const int TICK_S = 10;
 static const float RADIUS_MIN = 150;      // m: a town's ground, at least
 static const int PRESENCE_MIN = 2;        // army men in it that count as a presence
 static const float STATIONED = 0.6;       // per minute towards the army: troops in a calm town
 static const float EMPTY = 0.25;          // per minute towards the insurgency: no army there
 static const float EMPTY_CAP = 70;        // ...up to this (Restless at most)
 static const float FIA_WORKS = 0.4;       // per minute: FIA in it unopposed, or an FIA pocket
 static const float PRISONERS = 0.2;       // per minute: prisoners held at a hideout near it
 static const float PRISON_REACH = 1500;
 static const float HARM_SCALE = 4;        // support points per grievance point (a building 2, a civilian 4, a barrage 6)
 static const float HARM_REACH = 400;      // m from the town's middle
 static const float CALM_MS = 300000;      // after harm or a fight: troops win nobody over for 5 min
 // Starts (+-5): open towns just under the middle (mostly Quiet), so a match opens Simmering and the
 // army's absence pushes it up; at 55 most towns began Restless and the province opened at Uprising.
 static const float START_HELD = 35;
 static const float START_OPEN = 45;
 static const float START_POCKET = 65;
 static const float START_STRONGHOLD = 85;
 static const float INFORM_REACH = 500;
 static const float INFORM_GAP_MS = 300000;  // an informants' feed line per town at most this often
 protected static bool s_bOn;
 protected static int s_iSeconds;
 static float s_fRateScaleForTest = 1;     // tests speed time up
 static int s_iChanges;                    // tests: tier changes
 static int s_iHarm;                       // tests: harm events that moved a town
 static int s_iAid;                        // tests: aid deliveries

 static void Reset()
 {
  s_bOn = false;
  s_iSeconds = 0;
  s_fRateScaleForTest = 1;
  s_iChanges = 0;
  s_iHarm = 0;
  s_iAid = 0;
 }

 static bool On() { return s_bOn; }

 static bool IsTown(ACR_Base base)
 {
  return base && base.m_bIncome && !base.m_bBuilt && !base.m_bRetired;
 }

 // Server, once FIA is placed (ACR_Skirmish.ApplyOnce).
 static void Apply()
 {
  Reset();
  ACR_Province.Reset();
  if (!ACR_Coin.On()) return;
  s_bOn = true;
  string army = ACR_Coin.Army();
  int towns;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!IsTown(base)) continue;
   float start = START_OPEN;
   if (base == ACR_Coin.Stronghold()) start = START_STRONGHOLD;
   else if (base.m_bPocket) start = START_POCKET;
   else if (base.m_sOwner == army) start = START_HELD;
   base.m_fSupport = Math.Clamp(start + Math.RandomFloat(-5, 5), 0, 100);
   base.m_iSupportTier = TierOf(base.m_fSupport, -1);
   base.m_iSupportKnown = -1;
   if (base.m_sOwner == army) base.m_iSupportKnown = base.m_iSupportTier;
   base.m_sSupportWhy = "";
   base.m_fSupportCalmUntil = 0;
   base.m_fInformedAt = -INFORM_GAP_MS;
   base.m_fAidAt = 0;
   base.m_fHarmLineAt = 0;
   towns++;
  }
  PrintFormat("[ACR] SUPPORT %1 towns lean between %2 and the insurgency", towns, army);
 }

 // ---------------------------------------------------------------- tiers

 static int TierOf(float support, int previous)
 {
  int tier = HOSTILE;
  if (support < 25) tier = LOYAL;
  else if (support < 50) tier = QUIET;
  else if (support < 75) tier = RESTLESS;
  if (previous < 0 || tier == previous) return tier;
  // Leave the previous tier only HYSTERESIS past its bound.
  float low = previous * 25;
  float high = low + 25;
  if (tier < previous && support > low - HYSTERESIS) return previous;
  if (tier > previous && support < high + HYSTERESIS) return previous;
  return tier;
 }

 static string TierName(int tier)
 {
  if (tier == LOYAL) return "Loyal";
  if (tier == QUIET) return "Quiet";
  if (tier == RESTLESS) return "Restless";
  if (tier == HOSTILE) return "Hostile";
  return "unknown";
 }

 // The tier 'viewer' knows (-1: unknown). The army sees what it holds or has men in, else what it saw
 // last; anyone else (FIA, a spectator's "") the truth.
 static int KnownTier(ACR_Base base, string viewer)
 {
  if (!s_bOn || !IsTown(base) || base.m_iSupportTier < 0) return -1;
  if (viewer.IsEmpty() || !ACR_Coin.IsArmy(viewer) || base.m_sOwner == viewer) return base.m_iSupportTier;
  return base.m_iSupportKnown;
 }

 // The army's share of the town's income (ACR_BaseSystem.GetIncome): 1, or HostileIncome when Hostile.
 static float IncomeShare(ACR_Base base, string factionKey)
 {
  if (!s_bOn || !IsTown(base) || base.m_iSupportTier != HOSTILE || !ACR_Coin.IsArmy(factionKey)) return 1;
  return ACR_Coin.HostileIncome();
 }

 // " | support: Loyal" for the base's panel line and map label (ACR_BaseSystem.Describe).
 static string Detail(ACR_Base base, string viewer)
 {
  if (!s_bOn || !IsTown(base)) return "";
  int tier = KnownTier(base, viewer);
  if (tier < 0) return " | support: unknown";
  string text = " | support: " + TierName(tier);
  if (tier == HOSTILE && ACR_Coin.IsArmy(viewer)) text += string.Format(" (income x%1)", ACR_Coin.HostileIncome());
  return text;
 }

 // Client side, from a base's detail line: the tier word for its map caption ("Loyal", "?" when
 // unknown), "" when the detail has no support part (not COIN, not a town).
 static string CaptionTier(string detail)
 {
  int at = detail.IndexOf(" | support: ");
  if (at < 0) return "";
  string rest = detail.Substring(at + 12, detail.Length() - at - 12);
  int end = rest.IndexOf(" ");
  if (end >= 0) rest = rest.Substring(0, end);
  if (rest == "unknown") return "?";
  return rest;
 }

 // ---------------------------------------------------------------- the tick

 // Server, every second (ACR_Manager.Tick).
 static void Tick()
 {
  if (!s_bOn || !Replication.IsServer()) return;
  s_iSeconds++;
  if (s_iSeconds % TICK_S != 0) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases) return;
  string army = ACR_Coin.Army();
  string fia = ACR_Pockets.FACTION;
  float now = GetGame().GetWorld().GetWorldTime();
  float minutes = TICK_S / 60.0 * s_fRateScaleForTest;
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!IsTown(base) || base.m_fSupport < 0) continue;
   vector middle = base.GetOrigin();
   float radius = Math.Max(base.m_fRadius, RADIUS_MIN);
   int soldiers = manager.m_Bases.MenWithin(middle, radius, army);
   int insurgents = manager.m_Bases.MenWithin(middle, radius, fia);
   bool held = base.m_sOwner == army;
   if (soldiers > 0 || held) base.m_iSupportKnown = base.m_iSupportTier;
   // A fight in the town: nobody is won over while it lasts and for a while after.
   if (soldiers > 0 && insurgents > 0) base.m_fSupportCalmUntil = now + CALM_MS;
   float delta = 0;
   string why = "";
   if (soldiers >= PRESENCE_MIN && insurgents == 0 && now >= base.m_fSupportCalmUntil)
   {
    delta -= STATIONED * minutes;
    why = "our troops there";
   }
   if (soldiers == 0 && !held && base.m_fSupport < EMPTY_CAP)
   {
    delta += Math.Min(EMPTY * minutes, EMPTY_CAP - base.m_fSupport);
    why = "no troops of ours there";
   }
   if (soldiers == 0 && (insurgents > 0 || base.m_sOwner == fia))
   {
    delta += FIA_WORKS * minutes;
    why = "FIA works it unopposed";
   }
   if (PrisonersNear(middle, hideouts))
   {
    delta += PRISONERS * minutes;
    if (why.IsEmpty()) why = "prisoners held nearby";
   }
   if (delta != 0) Move(base, delta, why, now);
   if (base.m_iSupportTier == LOYAL && insurgents > 0) Inform(manager, base, army, now);
  }
  ACR_Province.Tick(now);
 }

 // ---------------------------------------------------------------- recruits (item 3)

 // Recruits a minute for a hideout (ACR_FIAGrowth.FillRate in COIN): the towns within
 // ACR_Guerrillas.SUPPLY_REACH, each by its tier (RecruitWeight) and nearness (full within
 // RECRUIT_NEAR, none at the reach), times ACR_FIAGrowth.FILL_PER_MIN, at most RECRUIT_MAX of that.
 // A hideout among Hostile towns fills fastest; among Loyal ones it gets nobody.
 static const float RECRUIT_NEAR = 1000;
 static const float RECRUIT_MAX = 2;
 static float Recruits(ACR_DefenderPost hideout)
 {
  if (!hideout) return 0;
  float reach = ACR_Guerrillas.SUPPLY_REACH;
  float sum = 0;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!IsTown(base) || base.m_iSupportTier < 0) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), hideout.m_vPos);
   if (d >= reach) continue;
   float near = 1;
   if (d > RECRUIT_NEAR) near = 1 - (d - RECRUIT_NEAR) / (reach - RECRUIT_NEAR);
   sum += RecruitWeight(base.m_iSupportTier) * near;
  }
  return ACR_FIAGrowth.FILL_PER_MIN * Math.Min(RECRUIT_MAX, sum);
 }

 // "No recruits / a trickle / recruits / most recruits" (#244's table).
 static float RecruitWeight(int tier)
 {
  if (tier == QUIET) return 0.1;
  if (tier == RESTLESS) return 0.5;
  if (tier == HOSTILE) return 1;
  return 0;
 }

 protected static bool PrisonersNear(vector middle, notnull array<ACR_DefenderPost> hideouts)
 {
  foreach (ACR_DefenderPost hideout : hideouts)
   if (vector.DistanceXZ(hideout.m_vPos, middle) <= PRISON_REACH && ACR_FIAPrisoners.HeldAt(hideout, "", true) > 0) return true;
  return false;
 }

 // Informants in a Loyal town tell the army of FIA near it.
 protected static void Inform(ACR_Manager manager, ACR_Base base, string army, float now)
 {
  ACR_Intel intel = manager.IntelFor(army);
  if (!intel) return;
  // Within INFORM_REACH, which may reach past the town's own ground.
  array<IEntity> near = {};
  manager.m_Bases.MenWithin(base.GetOrigin(), INFORM_REACH, ACR_Pockets.FACTION, near);
  foreach (IEntity body : near) intel.Tip(body, now, "informants");
  if (near.IsEmpty() || now - base.m_fInformedAt < INFORM_GAP_MS) return;
  base.m_fInformedAt = now;
  intel.AddFeed(now, string.Format("Informants in %1: %2 FIA seen nearby", base.m_sName, near.Count()), base.GetOrigin());
 }

 // 'delta' support (up: towards the insurgency) for 'why'; a tier change the army sees gets a line.
 static void Move(ACR_Base base, float delta, string why, float now)
 {
  float before = base.m_fSupport;
  base.m_fSupport = Math.Clamp(base.m_fSupport + delta, 0, 100);
  if (base.m_fSupport == before) return;
  if (!why.IsEmpty()) base.m_sSupportWhy = why;
  int tier = TierOf(base.m_fSupport, base.m_iSupportTier);
  if (tier == base.m_iSupportTier) return;
  int old = base.m_iSupportTier;
  base.m_iSupportTier = tier;
  s_iChanges++;
  PrintFormat("[ACR] SUPPORT %1 %2 -> %3 (%4, %5)", base.m_sName, TierName(old), TierName(tier), base.m_sSupportWhy, Math.Round(base.m_fSupport));
  ACR_Manager manager = ACR_Manager.GetInstance();
  string army = ACR_Coin.Army();
  if (!manager || base.m_iSupportKnown < 0 || (base.m_sOwner != army && manager.m_Bases.MenWithin(base.GetOrigin(), Math.Max(base.m_fRadius, RADIUS_MIN), army) == 0)) return;
  base.m_iSupportKnown = tier;
  ACR_Intel intel = manager.IntelFor(army);
  if (!intel) return;
  string turn = "turns";
  if (tier < old) turn = "comes round to";
  string line = string.Format("%1 %2 %3 (%4)", base.m_sName, turn, TierName(tier), base.m_sSupportWhy);
  if (tier == HOSTILE && base.m_sOwner == army) line += string.Format(": income x%1", ACR_Coin.HostileIncome());
  intel.AddFeed(now, line, base.GetOrigin());
 }

 // ---------------------------------------------------------------- aid deliveries (item 4)
 // The army's supply truck (a construction truck bought with RP: its paid load, ACR_Logistics) told to
 // Unload while stopped in a town the army holds or patrols hands out AID_SUPPLIES of its load: the
 // town moves AID_STEP towards the army. One delivery per town per AID_GAP_MS; every refusal says
 // why and spends nothing. Players give it with the panel's Unload order (ACR_Requests), the AI
 // commander the same way (item 5).
 static const float AID_SUPPLIES = 250;     // a quarter of a full truck: four towns a load
 static const float AID_STEP = 10;          // support points: a step (a tier is 25)
 static const float AID_GAP_MS = 1200000;   // 20 min a town
 static const float AID_MAX_SPEED = 2;      // m/s: stopped

 // The squad's supply truck (null: none).
 static IEntity AidTruck(ACR_Squad squad)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!squad || !manager || !manager.m_Logistics) return null;
  array<IEntity> vehicles = {};
  squad.GetVehicles(vehicles);
  foreach (IEntity vehicle : vehicles)
   if (vehicle && (ACR_Logistics.ServiceOf(vehicle) & ACR_EService.CONSTRUCTION) != 0) return vehicle;
  return null;
 }

 // The town the point is in (null: none).
 static ACR_Base TownAt(vector at)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (IsTown(base) && base.m_fSupport >= 0 && vector.DistanceXZ(base.GetOrigin(), at) <= Math.Max(base.m_fRadius, RADIUS_MIN)) return base;
  return null;
 }

 // Why the army's truck can't hand out aid in this town now ("" = it can). Shared by players and the AI.
 static string AidRefusal(ACR_Base town, IEntity truck, int crew)
 {
  if (!town) return "aid goes to a town: drive the supply truck into one first";
  ACR_Manager manager = ACR_Manager.GetInstance();
  string army = ACR_Coin.Army();
  float radius = Math.Max(town.m_fRadius, RADIUS_MIN);
  if (manager.m_Bases.MenWithin(town.GetOrigin(), radius, ACR_Pockets.FACTION) > 0) return string.Format("FIA are in %1: clear them out first", town.m_sName);
  if (town.m_sOwner != army && manager.m_Bases.MenWithin(town.GetOrigin(), radius, army) < crew + PRESENCE_MIN) return string.Format("%1 isn't ours: hold it or have soldiers there to hand out aid", town.m_sName);
  float now = GetGame().GetWorld().GetWorldTime();
  if (town.m_fAidAt > 0 && now - town.m_fAidAt < AID_GAP_MS)
  {
   int ago = Math.Floor((now - town.m_fAidAt) / 60000);
   int next = Math.Ceil((AID_GAP_MS - (now - town.m_fAidAt)) / 60000);
   return string.Format("%1 had aid %2 min ago: the next in %3 min", town.m_sName, ago, next);
  }
  Physics physics = truck.GetPhysics();
  if (physics && physics.GetVelocity().Length() > AID_MAX_SPEED) return "stop the truck first";
  if (manager.m_Logistics.PaidSupplies(truck) < AID_SUPPLIES) return string.Format("the truck needs %1 paid supplies for aid (it has %2)", AID_SUPPLIES, Math.Floor(manager.m_Logistics.PaidSupplies(truck)));
  return "";
 }

 // An Unload order for 'squad' (ACR_Requests). "" when this isn't an aid matter (not COIN, not the
 // army, no supply truck): the plain Unload goes ahead. Else what happened; 'accepted' if delivered.
 static string Aid(ACR_Squad squad, out bool accepted)
 {
  accepted = false;
  if (!s_bOn || !squad || !squad.GetFaction() || !ACR_Coin.IsArmy(squad.GetFaction().GetFactionKey())) return "";
  IEntity truck = AidTruck(squad);
  if (!truck) return "";
  ACR_Base town = TownAt(truck.GetOrigin());
  array<AIAgent> crew = {};
  squad.GetAgents(crew);
  string why = AidRefusal(town, truck, crew.Count());
  if (!why.IsEmpty())
  {
   PrintFormat("[ACR] SUPPORT aid refused %1: %2", squad.GetName(), why);
   return squad.GetName() + ": no aid: " + why;
  }
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager.m_Logistics.SpendPaid(truck, AID_SUPPLIES)) return squad.GetName() + ": no aid: the truck's load couldn't be handed out";
  float now = GetGame().GetWorld().GetWorldTime();
  town.m_fAidAt = now;
  float before = town.m_fSupport;
  Move(town, -AID_STEP, "aid delivered", now);
  s_iAid++;
  accepted = true;
  PrintFormat("[ACR] SUPPORT aid %1 in %2: %3 -> %4", squad.GetName(), town.m_sName, Math.Round(before), Math.Round(town.m_fSupport));
  ACR_Intel intel = manager.IntelFor(ACR_Coin.Army());
  if (intel) intel.AddFeed(now, string.Format("Aid delivered to %1 (%2 now)", town.m_sName, TierName(town.m_iSupportTier)), town.GetOrigin());
  return string.Format("%1: aid handed out in %2 (%3 supplies)", squad.GetName(), town.m_sName, AID_SUPPLIES);
 }

 // ---------------------------------------------------------------- harm (ACR_FIAGrievance's events)

 // 'points' of harm (grievance points) at 'pos' by 'by': every town within HARM_REACH leans away
 // from whoever did it. Only the army's and FIA's harm counts.
 static void Harm(vector pos, string by, float points, string what)
 {
  if (!s_bOn || !Replication.IsServer() || by.IsEmpty()) return;
  bool fia = by == ACR_Pockets.FACTION;
  if (!fia && !ACR_Coin.IsArmy(by)) return;
  float now = GetGame().GetWorld().GetWorldTime();
  float shift = points * HARM_SCALE;
  string why = "our " + what;
  if (fia) { shift = -shift; why = "FIA " + what; }
  bool moved;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!IsTown(base) || base.m_fSupport < 0 || vector.DistanceXZ(base.GetOrigin(), pos) > Math.Max(HARM_REACH, base.m_fRadius)) continue;
   base.m_fSupportCalmUntil = now + CALM_MS;
   PrintFormat("[ACR] SUPPORT %1 %2 by %3 at %4: %5", base.m_sName, what, by, ACR_Manager.Grid(pos), shift);
   Move(base, shift, why, now);
   HarmLine(base, fia, what, now);
   moved = true;
  }
  if (moved) s_iHarm++;
 }

 // The feed says what harm cost (#244: "the feed says what it cost"), once a HARM_LINE_GAP_MS a town.
 // This is the line FIA grievance gave outside COIN ("FIA recruits after our shelling near X").
 static const float HARM_LINE_GAP_MS = 300000;
 protected static void HarmLine(ACR_Base base, bool fia, string what, float now)
 {
  if (now - base.m_fHarmLineAt < HARM_LINE_GAP_MS && base.m_fHarmLineAt > 0) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Intel intel = manager.IntelFor(ACR_Coin.Army());
  if (!intel) return;
  base.m_fHarmLineAt = now;
  string line = string.Format("Our %1 in %2 turns people against us", what, base.m_sName);
  if (fia) line = string.Format("FIA %1 in %2 turns people against FIA", what, base.m_sName);
  intel.AddFeed(now, line, base.GetOrigin());
 }
}
