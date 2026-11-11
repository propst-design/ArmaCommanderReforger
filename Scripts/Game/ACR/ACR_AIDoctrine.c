// AI commander doctrines (Jacob, 24 Oct): each AI side fights to one of a few styles. By default it
// follows the side's company (#38, the setup screen's Blue/Red company): Combat Engineers fight as
// Fortress, Mechanized as Blitz, Fire Support as Firepower, Recon as Raider, a Rifle Company (none
// picked) as Balanced. The setup screen's Blue/Red AI doctrine row
// (-acrDoctrineWest/-acrDoctrineEast) overrides it, "random" included; with no company picked, the
// faction's m_eAIDoctrine comes before Balanced. A doctrine is not a second
// commander: it reweights levers ACR_AICommander already has (attack and defence odds, empty
// base grabs, fronts, initiative, reserve, retreat, what it buys, when it calls supports,
// rides and air lifts, fire missions, mines, FOBs, refits, pockets).
// The squad cap, RP and stock are the same for every doctrine, so none fields a bigger army
// (Raider's 2-3 man teams take half a cap slot each, so it fields about as many men).
// Pass 2 (Jacob, 24 Oct): the enemy can work out a doctrine from intel (ACR_DoctrineIntel),
// and a commander whose doctrine isn't working changes it (ACR_AICommander.ReviewDoctrine,
// -acrDoctrineAdapt 0 keeps the first one).
enum ACR_EAIDoctrineChoice
{
 RANDOM,
 BALANCED,
 BLITZ,
 FIREPOWER,
 FORTRESS,
 RAIDER
}

class ACR_AIDoctrine : Managed
{
 string m_sKey;
 string m_sName;
 string m_sSummary;
 // The line under the setup screen's Command rows ("Blue: Fortress: holds in depth, counterattacks
 // lost bases", at most ACR_Setup.ROW_CHARS).
 string m_sBrief;

 // Planning (ACR_AICommander.AssignAttacks / AssignDefenders / UpdateAttacks)
 float m_fAttackRatio = 1;      // x the force it wants against a defended base
 float m_fDefenseRatio = 1;     // x the defence it sends against a threat
 float m_fConcedeRatio = 1;     // x the odds at which it gives a base up
 int m_iGrabs;                  // + empty bases it goes for at once (3..8 before this)
 int m_iExtraFronts = 3;        // most extra attacks with idle troops (6 on maps over 24 bases)
 int m_iSquadsPerFront = 3;     // idle squads per extra attack
 float m_fWeakTarget = 1;       // x score of weakly held or contested bases (lower = sooner)
 float m_fFobTarget = 1;        // x score of enemy FOBs (lower = sooner)
 float m_fCounterNeed = 0.7;    // force it accepts for a counterattack on a base it just lost
 float m_fCounterMinutes = 5;   // how long a counterattack stays due
 float m_fQuietMinutes = 4;     // minutes without an attack before it accepts worse odds
 float m_fPressurePerMinute = 0.05;
 float m_fMinPressure = 0.65;
 int m_iReserveSquads;          // + squads it needs before it keeps a reserve back
 float m_fRetreatRatio = 0.4;   // an attacking squad in contact below this falls back
 float m_fHoldRatio = 0.4;      // a defender of an overrun base below this falls back
 float m_fForwardShare = 0.5;   // share of a threatened base's defenders that go out to a forward line
 int m_iDefensiveAirThreat = 8; // threat at a losing base that earns a gun run or rockets
 float m_fAbortRatio = 0.35;    // an attack below this share of its strength is called off
 int m_iHeavyPrep = 12;         // known defenders that earn air or rocket preparation fires
 int m_iRoadblocks = 2;         // most roadblocks it holds
 int m_iMinefields = 2;         // most minefields it keeps laid
 int m_iFortifyPositions = 2;   // most fortified positions it digs (it upgrades them first)
 float m_fBuildShare = 1;       // x the build budget's share of income and its trickle
 int m_iHolders = 2;            // most builders that stay on what they built
 bool m_bHoldersStay;           // its holders defend a base only when outnumbered there (else also as a last resort)
 float m_fFobFront = 1500;      // m from the nearest deploy base before a front base wants a FOB
 float m_fFireBaseSeconds = 240; // s a base of fire supports before it joins the assault
 float m_fPocketScore = 2.5;    // x attack score of FIA pockets in the way (lower = sooner)
 // Strategy: an axis of advance (ACR_AILanes, only with -acrAIStrategy 1)
 bool m_bLanes = true;          // fights down a lane after the land-rush (Raider keeps raiding everywhere)
 float m_fRushMinutes = 6;      // min of opening land-rush (grabs anywhere) before it picks a lane
 float m_fLaneWidth = 700;      // m either side of the lane's axis
 int m_iLaneFronts = 1;         // supporting attacks beside the main one while it has a lane
 float m_fFlankWorks = 2;       // base works weight of a tactical point on the lane's flank

 // Moving (ACR_AICommander.Travel / TryAirLift)
 float m_fRideDistance = 900;   // m on foot before it asks for a truck
 float m_fAirLiftDistance = 2000; // m before it calls an air lift
 float m_fRefitRatio = 0.5;     // idle squads below this strength walk back to refill
 float m_fAmmoLow = 45;        // squads under this share (pct) of their starting magazines go for ammo (ACR_AmmoRuns)

 // Fire missions (ACR_FireMissions, AI-led sides)
 float m_fFireMissionGap = 1;   // x the wait between observer calls and mortar free fire

 // Buying (ACR_AICommander.Requisition)
 float m_fVehicleWeight = 1;    // x the pick weight of armed vehicles
 float m_fSmallTeamWeight = 1;  // x the pick weight of 2-3 man teams
 bool m_bSmallTeamsFree;        // small teams aren't held back by the line-squad ratio
 int m_iTransports;             // + trucks it keeps for rides
 bool m_bSupportsFirst;         // buys mortars and gunships even while short of troops
 float m_fObserverWeight = 1;  // x the pick weight of forward observer teams
 int m_iSupportMissions = 1;    // fire missions it keeps the RP for (dearest first)
 bool m_bAirBudget;             // ... counting air and rocket strikes, not only mortars
 int m_iSupportBudgetMax = 60;  // most RP it holds back for fire missions

 static const string RANDOM = "random";
 protected static ref array<ref ACR_AIDoctrine> s_All;

 static array<ref ACR_AIDoctrine> All()
 {
  if (s_All) return s_All;
  s_All = {};

  ACR_AIDoctrine balanced = new ACR_AIDoctrine();
  balanced.m_sKey = "balanced";
  balanced.m_sName = "Balanced";
  balanced.m_sSummary = "the standard commander";
  balanced.m_sBrief = "the standard commander";
  s_All.Insert(balanced);

  // Mechanised shock: vehicles, speed and many fronts; accepts thinner odds and losses.
  ACR_AIDoctrine blitz = new ACR_AIDoctrine();
  blitz.m_sKey = "blitz";
  blitz.m_sName = "Blitz";
  blitz.m_sSummary = "mechanised, fast, attacks early on thin odds";
  blitz.m_sBrief = "armour, attacks early on thin odds";
  blitz.m_fAttackRatio = 0.85;
  blitz.m_iGrabs = 2;
  blitz.m_iExtraFronts = 4;
  blitz.m_fQuietMinutes = 2;
  blitz.m_fPressurePerMinute = 0.08;
  blitz.m_fMinPressure = 0.6;
  blitz.m_iReserveSquads = 2;
  blitz.m_fRetreatRatio = 0.3;
  blitz.m_fHoldRatio = 0.35;
  blitz.m_fForwardShare = 0.34;
  blitz.m_iDefensiveAirThreat = 10;
  blitz.m_fAbortRatio = 0.25;
  blitz.m_iHeavyPrep = 16;
  blitz.m_iMinefields = 1;
  blitz.m_fFobFront = 1100;
  blitz.m_fFireBaseSeconds = 150;
  blitz.m_fRideDistance = 600;
  blitz.m_fAirLiftDistance = 1700;
  blitz.m_fRefitRatio = 0.35;
  blitz.m_fAmmoLow = 35;
  blitz.m_fFireMissionGap = 1.25;
  blitz.m_fVehicleWeight = 2.2;
  blitz.m_iTransports = 1;
  blitz.m_iSupportBudgetMax = 30;
  blitz.m_fRushMinutes = 4;
  blitz.m_fLaneWidth = 550;
  s_All.Insert(blitz);

  // Attrition: mortars and air first, one front at a time, every assault prepared by fire.
  ACR_AIDoctrine firepower = new ACR_AIDoctrine();
  firepower.m_sKey = "firepower";
  firepower.m_sName = "Firepower";
  firepower.m_sSummary = "artillery and air first, patient, one front at a time";
  firepower.m_sBrief = "artillery and air first, patient";
  firepower.m_fAttackRatio = 1.15;
  firepower.m_iExtraFronts = 1;
  firepower.m_iSquadsPerFront = 4;
  firepower.m_fQuietMinutes = 6;
  firepower.m_fPressurePerMinute = 0.04;
  firepower.m_fMinPressure = 0.75;
  firepower.m_fRetreatRatio = 0.45;
  firepower.m_fHoldRatio = 0.45;
  firepower.m_fForwardShare = 0.34;
  firepower.m_iDefensiveAirThreat = 5;
  firepower.m_iHeavyPrep = 6;
  firepower.m_fFireBaseSeconds = 360;
  firepower.m_fPocketScore = 2;
  firepower.m_fRefitRatio = 0.55;
  firepower.m_fAmmoLow = 50;
  firepower.m_fFireMissionGap = 0.6;
  firepower.m_fObserverWeight = 3;
  firepower.m_bSupportsFirst = true;
  firepower.m_iSupportMissions = 2;
  firepower.m_bAirBudget = true;
  firepower.m_iSupportBudgetMax = 100;
  firepower.m_iLaneFronts = 0;
  s_All.Insert(firepower);

  // Defence in depth: holds what it has, hits back hard at what it loses.
  ACR_AIDoctrine fortress = new ACR_AIDoctrine();
  fortress.m_sKey = "fortress";
  fortress.m_sName = "Fortress";
  fortress.m_sSummary = "defends in depth behind manned roadblocks, minefields and positions, counterattacks lost bases at once";
  fortress.m_sBrief = "holds in depth, counterattacks lost bases";
  fortress.m_fAttackRatio = 1.1;
  fortress.m_fDefenseRatio = 1.35;
  fortress.m_fConcedeRatio = 1.5;
  fortress.m_iGrabs = -1;
  fortress.m_iExtraFronts = 1;
  fortress.m_iSquadsPerFront = 4;
  fortress.m_fCounterNeed = 0.55;
  fortress.m_fCounterMinutes = 10;
  fortress.m_fQuietMinutes = 6;
  fortress.m_fMinPressure = 0.75;
  fortress.m_iReserveSquads = -1;
  fortress.m_fHoldRatio = 0.25;
  fortress.m_fForwardShare = 0.75;
  fortress.m_iDefensiveAirThreat = 6;
  fortress.m_iRoadblocks = 3;
  fortress.m_iMinefields = 4;
  // Builds and mans more (user, 25 Oct): twice the build share, up to four positions, and the
  // builders stay on what they built.
  fortress.m_fBuildShare = 2;
  fortress.m_iFortifyPositions = 4;
  fortress.m_iHolders = 4;
  fortress.m_bHoldersStay = true;
  fortress.m_fPocketScore = 1.5;
  fortress.m_fRideDistance = 1100;
  fortress.m_fAirLiftDistance = 2400;
  fortress.m_fRefitRatio = 0.6;
  fortress.m_fAmmoLow = 50;
  fortress.m_fFireMissionGap = 0.8;
  fortress.m_fObserverWeight = 1.5;
  fortress.m_fRushMinutes = 5;
  fortress.m_fLaneWidth = 800;
  fortress.m_iLaneFronts = 0;
  fortress.m_fFlankWorks = 4;
  s_All.Insert(fortress);

  // Raids: many small teams going for weak spots and FOBs, quick to break off.
  ACR_AIDoctrine raider = new ACR_AIDoctrine();
  raider.m_sKey = "raider";
  raider.m_sName = "Raider";
  raider.m_sSummary = "small teams raid weak bases and FOBs, break off early";
  raider.m_sBrief = "small teams raid weak bases and FOBs";
  raider.m_fAttackRatio = 0.9;
  raider.m_iGrabs = 1;
  raider.m_iExtraFronts = 5;
  raider.m_iSquadsPerFront = 2;
  raider.m_fWeakTarget = 0.5;
  raider.m_fFobTarget = 0.6;
  raider.m_fQuietMinutes = 3;
  raider.m_fRetreatRatio = 0.5;
  raider.m_fHoldRatio = 0.5;
  raider.m_fAbortRatio = 0.5;
  raider.m_iMinefields = 1;
  raider.m_fFobFront = 1300;
  raider.m_fFireBaseSeconds = 180;
  raider.m_fPocketScore = 3.5;
  raider.m_fRideDistance = 800;
  raider.m_fAirLiftDistance = 1500;
  raider.m_fAmmoLow = 40;
  raider.m_fFireMissionGap = 1.25;
  raider.m_fVehicleWeight = 0.7;
  raider.m_fSmallTeamWeight = 2.5;
  raider.m_bSmallTeamsFree = true;
  raider.m_bLanes = false;
  raider.m_fRushMinutes = 8;
  s_All.Insert(raider);
  return s_All;
 }

 static ACR_AIDoctrine Find(string key)
 {
  string lower = key;
  lower.ToLower();
  foreach (ACR_AIDoctrine doctrine : All())
   if (doctrine.m_sKey == lower) return doctrine;
  return null;
 }

 static string NameOf(string key)
 {
  ACR_AIDoctrine doctrine = Find(key);
  if (doctrine) return doctrine.m_sName;
  return "Random";
 }

 // The host option for the side this faction plays: "acrDoctrineWest" or "acrDoctrineEast".
 static string OptionFor(string factionKey)
 {
  if (factionKey == ACR_FactionSelection.East()) return "acrDoctrineEast";
  return "acrDoctrineWest";
 }

 // The doctrine this faction's AI commander fights to this match, and where it came from.
 static ACR_AIDoctrine Choose(string factionKey, out string source)
 {
  string value;
  bool picked = ACR_Options.Get(OptionFor(factionKey), value);
  value.ToLower();
  if (picked && Find(value))
  {
   source = ACR_Options.Source(OptionFor(factionKey));
   return Find(value);
  }
  if (picked && value == RANDOM) return Random(source);
  // The doctrine row left at "follows company": the company's style.
  string company = ACR_DoctrineKits.CompanyOf(factionKey);
  if (Find(ACR_DoctrineKits.KitOf(company)))
  {
   source = "follows its company (" + ACR_DoctrineKits.CompanyName(company) + ")";
   return Find(ACR_DoctrineKits.KitOf(company));
  }
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  if (settings && Find(settings.AIDoctrineKey()))
  {
   source = "faction settings";
   return Find(settings.AIDoctrineKey());
  }
  // Tests keep the standard commander unless they ask for a doctrine.
  string forTest = ACR_TestHooks.Get().AIDoctrine(factionKey);
  if (Find(forTest))
  {
   source = "test";
   return Find(forTest);
  }
  // A Rifle Company (none picked) fights as the standard commander.
  source = "follows its company (" + ACR_DoctrineKits.CompanyName(company) + ")";
  return Find("balanced");
 }

 // The doctrine the row's "follows company" gives this faction (tests aside): its company's, the
 // scenario's with no company picked, else Balanced.
 static string FollowedKey(string factionKey)
 {
  string kit = ACR_DoctrineKits.KitOf(ACR_DoctrineKits.CompanyOf(factionKey));
  if (Find(kit)) return kit;
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  if (settings && Find(settings.AIDoctrineKey())) return Find(settings.AIDoctrineKey()).m_sKey;
  return "balanced";
 }

 // The doctrine row's "follows company" as the setup screen shows it: "follows company (Fortress)",
 // or "scenario's choice (Raider)" when no company is picked and the scenario sets one.
 static string FollowsRow(string factionKey)
 {
  string followed = NameOf(FollowedKey(factionKey));
  if (Find(ACR_DoctrineKits.KitOf(ACR_DoctrineKits.CompanyOf(factionKey))) || followed == "Balanced") return "follows company (" + followed + ")";
  return "scenario's choice (" + followed + ")";
 }

 // Changing doctrine mid-match (on unless the host sets -acrDoctrineAdapt 0 / off).
 static bool AdaptAllowed()
 {
  string value;
  if (!ACR_Options.Get("acrDoctrineAdapt", value)) return true;
  value.ToLower();
  return value != "0" && value != "false" && value != "off";
 }

 // The doctrine that beats this one (a heuristic from how they fight): Fortress's prepared
 // defence breaks Blitz's thin-odds attacks; Firepower's preparation fires crack a Fortress;
 // Raider's many fronts outrun Firepower's one at a time; Blitz's armour runs down raiders.
 // Balanced has no clear answer.
 static string Counter(string key)
 {
  if (key == "blitz") return "fortress";
  if (key == "fortress") return "firepower";
  if (key == "firepower") return "raider";
  if (key == "raider") return "blitz";
  return "";
 }

 // The doctrine that looks most like this one from the outside (the other half of an intel
 // hint): the two aggressive ones, the two patient ones, and Balanced with Fortress.
 static string LookAlike(string key)
 {
  if (key == "blitz") return "raider";
  if (key == "raider") return "blitz";
  if (key == "firepower") return "fortress";
  if (key == "fortress") return "firepower";
  return "fortress";
 }

 protected static ACR_AIDoctrine Random(out string source)
 {
  source = RANDOM;
  array<ref ACR_AIDoctrine> all = All();
  return all[Math.RandomInt(0, all.Count())];
 }
}
