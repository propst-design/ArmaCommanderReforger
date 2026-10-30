// Insurgency (#89, scope agreed with Jacob 28 Oct): FIA as a presence in the world, not a side
// (no commander, no RP). Its pieces: the pockets (ACR_Pockets), the guerrilla cells
// (ACR_Guerrillas), and here what the two sides make of it:
//  - Allegiance (-acrFIAAllegiance hostile|west|east, setup row "FIA sides with"): FIA is hostile
//    to both sides, or allied to one. Allied: the game's factions are friendly (the AI on both
//    sides holds its fire), cells and pockets never deploy against that side, it can't take an
//    FIA pocket and its AI doesn't attack or shell one, and it knows where the pockets are.
//  - Dangerous roads: where a side loses men or vehicles to FIA on a road, the spot is marked on
//    that side's map ("FIA ambush", DANGER_MS). Its AI commander sends a free combat squad to clear
//    it, and holds supply runs whose way passes it until it is cleared or the mark runs out.
//  - Leaning by conduct (part 2, -acrFIALeaning 1, setup row "FIA leans by conduct"): each main
//    side has a standing with FIA (0-100), and FIA treats it by its tier:
//     hostile (<25): ambushes go after its traffic first (twice the weight)
//     wary (25-49):  as part 1's hostile FIA
//     neutral (50-74): the guerrilla cells leave it alone (don't deploy, spring or follow its
//                    traffic); the pockets still hold their ground
//     friendly (75+): as part 1's ally (game factions friendly, free passage through pockets, it
//                    knows them), plus tip-offs: its enemies near an FIA cell or pocket show on
//                    its map
//    It falls when the side kills FIA (less when a cell sprang on it), takes a pocket, holds a
//    base near a pocket, or destroys buildings (more near FIA); it climbs back slowly after 5 min
//    with no offence, up to 80. The allegiance sets the start (its ally 85, a hostile side 35).
//    Commanders see their tier in the panel's command line and get a feed line when it changes.
//    Civilian harm is meant to count too once civilians exist (#49).
//  - Hideouts (ACR_Guerrillas): a side that fights one has found it: a feed line, and it is
//    marked like a dangerous road (no time limit, supply runs don't wait for it) so the side's AI
//    sends a squad to clear it. Destroying one costs standing (HIDEOUT_TAKEN). FIA's friends
//    know where its hideouts are.
class ACR_DangerRoad : Managed
{
 string m_sSide;
 vector m_vPos;
 float m_fUntil;
 int m_iLosses;
 ACR_Squad m_Sweeper;   // sent to clear it (ACR_AICommander.ManageDangerRoads)
 float m_fQuietSince;   // the sweeper there with no contact since (0: not yet)
 int m_iSweeps;         // sweepers sent
 ACR_DefenderPost m_Hideout; // a found FIA hideout to clear, not a road (no time limit)
}

enum ACR_EFIAStanding
{
 HOSTILE,
 WARY,
 NEUTRAL,
 FRIENDLY
}

// A main side's standing with FIA (leaning by conduct).
class ACR_FIASide : Managed
{
 string m_sSide;
 float m_fStanding;
 int m_iTier;
 float m_fLastOffence;
 ref map<string, float> m_mTipped = new map<string, float>(); // squad name: tipped off about until
}

class ACR_Insurgency
{
 static const float DANGER_MS = 1200000;  // a dangerous road is marked this long after the last loss
 static const float DANGER_RADIUS = 250;
 static const float ROAD_REACH = 120;     // a loss this close to a road counts (not one inside a pocket's yard)
 static const float CLEAR_QUIET_MS = 60000; // the sweeper there this long without contact: cleared
 static ref array<ref ACR_DangerRoad> s_aRoads = {};

 // ---------------------------------------------------------------- allegiance

 // The main side FIA is allied to ("" = hostile to both), read once at Apply (Friendly runs
 // for every sighting).
 protected static string s_sAlly;
 static string Ally() { return s_sAlly; }
 protected static string AllyOption()
 {
  string value;
  if (!ACR_Options.Get("acrFIAAllegiance", value)) return "";
  value.ToLower();
  if (value == "west") return ACR_FactionSelection.West();
  if (value == "east") return ACR_FactionSelection.East();
  return "";
 }

 // Two factions that don't fight: the same one, or FIA and a side it is friendly with (its ally,
 // or a side whose standing is friendly).
 static bool Friendly(string a, string b)
 {
  if (a == b) return true;
  if (a == ACR_Pockets.FACTION) return s_aFriends.Contains(b);
  if (b == ACR_Pockets.FACTION) return s_aFriends.Contains(a);
  return false;
 }

 // Server, once the layout is applied (ACR_Skirmish.ApplyOnce): the game's factions, and the ally
 // knows the pockets; with leaning, each side's starting standing.
 static void Apply()
 {
  s_aRoads.Clear();
  // A replay of Apply (tests): friendships from the last one end first.
  array<string> old = {};
  old.Copy(s_aFriends);
  foreach (string was : old) Unfriend(was);
  s_aFriends.Clear();
  s_aSides.Clear();
  s_iSeconds = 0;
  s_sAlly = AllyOption();
  s_bLeaning = LeaningOption() && FIAInWorld();
  if (s_bLeaning)
  {
   array<string> sides = {ACR_FactionSelection.West(), ACR_FactionSelection.East()};
   foreach (string side : sides)
   {
    ACR_FIASide standing = new ACR_FIASide();
    standing.m_sSide = side;
    standing.m_fStanding = START;
    if (side == s_sAlly) standing.m_fStanding = START_ALLY;
    standing.m_iTier = TierOf(standing.m_fStanding, ACR_EFIAStanding.HOSTILE);
    standing.m_fLastOffence = -RECOVER_AFTER_MS;
    s_aSides.Insert(standing);
    if (standing.m_iTier == ACR_EFIAStanding.FRIENDLY) Befriend(side);
    PrintFormat("[ACR] INSURGENCY FIA leaning: %1 starts %2 (%3)", side, Label(standing.m_iTier), (int)standing.m_fStanding);
   }
   return;
  }
  if (s_sAlly.IsEmpty()) return;
  int known = Befriend(s_sAlly);
  PrintFormat("[ACR] INSURGENCY FIA sides with %1 (%2 pockets known to it)", s_sAlly, known);
 }

 protected static bool FIAInWorld()
 {
  string fia = ACR_Pockets.FACTION;
  return ACR_FactionSelection.West() != fia && ACR_FactionSelection.East() != fia && GetGame().GetFactionManager() && GetGame().GetFactionManager().GetFactionByKey(fia);
 }

 // FIA and 'side' at peace: the game's factions friendly, and the side knows the pockets.
 // Returns the pockets it learnt of.
 protected static int Befriend(string side)
 {
  if (!s_aFriends.Contains(side)) s_aFriends.Insert(side);
  SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
  if (!factions) return 0;
  SCR_Faction fia = SCR_Faction.Cast(factions.GetFactionByKey(ACR_Pockets.FACTION));
  SCR_Faction friend = SCR_Faction.Cast(factions.GetFactionByKey(side));
  if (!fia || !friend || fia == friend) return 0;
  factions.SetFactionsFriendly(fia, friend);
  int known;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.m_bPocket || base.m_aDetectedBy.Contains(side)) continue;
   base.m_aDetectedBy.Insert(side);
   known++;
  }
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  foreach (ACR_DefenderPost hideout : hideouts) ACR_Guerrillas.Know(hideout, side);
  return known;
 }

 // No longer friends: the game's factions hostile again (what the side learnt, it keeps).
 protected static void Unfriend(string side)
 {
  s_aFriends.RemoveItem(side);
  SCR_FactionManager factions = SCR_FactionManager.Cast(GetGame().GetFactionManager());
  if (!factions) return;
  SCR_Faction fia = SCR_Faction.Cast(factions.GetFactionByKey(ACR_Pockets.FACTION));
  SCR_Faction friend = SCR_Faction.Cast(factions.GetFactionByKey(side));
  if (fia && friend && fia != friend) factions.SetFactionsHostile(fia, friend);
 }

 // ---------------------------------------------------------------- leaning by conduct

 static const float START = 35;            // a side FIA isn't allied to (wary)
 static const float START_ALLY = 85;       // the side FIA is allied to (friendly)
 static const float RECOVER_CAP = 80;      // restraint alone brings it this far
 static const float RECOVER_PER_MIN = 0.5;
 static const float RECOVER_AFTER_MS = 300000; // no offence for this long before it climbs
 static const float MARGIN = 3;            // a tier is only left this far below its bound
 static const float KILL = 3;              // an FIA man killed
 static const float KILL_SPRUNG = 1;       // ... by a side a cell sprang on (self-defence)
 static const float SPRUNG_REACH = 400;    // m from a sprung cell: its fight
 static const float POCKET_TAKEN = 15;
 static const float BUILDING = 2;          // a building destroyed
 static const float BUILDING_NEAR = 4;     // ... within NEAR_FIA of a pocket or cell
 static const float NEAR_FIA = 1000;
 static const float LAND_REACH = 1500;     // a base this close to an FIA pocket ...
 static const float LAND_PER_MIN = 0.25;   // ... costs this a minute
 static const float TIP_REACH = 500;       // enemies this close to a cell or pocket are tipped off
 static const float TIP_AGAIN_MS = 300000; // about the same squad
 static const float HIDEOUT_TAKEN = 10;    // a hideout destroyed, for each side with men there
 static const float HIDEOUT_TAKEN_REACH = 600;
 static ref array<ref ACR_FIASide> s_aSides = {};
 protected static ref array<string> s_aFriends = {};
 protected static bool s_bLeaning;
 protected static int s_iSeconds;

 static bool Leaning() { return s_bLeaning; }

 protected static bool LeaningOption()
 {
  string value;
  if (!ACR_Options.Get("acrFIALeaning", value)) return false;
  value.ToLower();
  return value == "1" || value == "on" || value == "true";
 }

 static ACR_FIASide SideOf(string side)
 {
  foreach (ACR_FIASide standing : s_aSides) if (standing.m_sSide == side) return standing;
  return null;
 }

 // The side's tier (wary without leaning, or for a side not tracked).
 static int Tier(string side)
 {
  ACR_FIASide standing = SideOf(side);
  if (!standing) return ACR_EFIAStanding.WARY;
  return standing.m_iTier;
 }

 // The guerrilla cells leave this side alone (neutral or better, or FIA's friend).
 static bool Spared(string side)
 {
  if (s_aFriends.Contains(side)) return true;
  return s_bLeaning && Tier(side) >= ACR_EFIAStanding.NEUTRAL;
 }

 // How much this side's traffic draws the ambushes (ACR_Guerrillas.SampleTraffic).
 static float TrafficWeight(string side)
 {
  if (Spared(side)) return 0;
  if (s_bLeaning && Tier(side) == ACR_EFIAStanding.HOSTILE) return 2;
  return 1;
 }

 static string Label(int tier)
 {
  if (tier == ACR_EFIAStanding.HOSTILE) return "hostile";
  if (tier == ACR_EFIAStanding.NEUTRAL) return "neutral";
  if (tier == ACR_EFIAStanding.FRIENDLY) return "friendly";
  return "wary";
 }

 // The panel's command line (ACR_MapFeed): "" without leaning.
 static string StandingTag(string side)
 {
  ACR_FIASide standing = SideOf(side);
  if (!s_bLeaning || !standing) return "";
  return "  |  FIA: " + Label(standing.m_iTier);
 }

 // The tier for 'value', leaving 'current' downwards only MARGIN below its bound.
 static int TierOf(float value, int current)
 {
  int tier = ACR_EFIAStanding.HOSTILE;
  if (value >= 25) tier = ACR_EFIAStanding.WARY;
  if (value >= 50) tier = ACR_EFIAStanding.NEUTRAL;
  if (value >= 75) tier = ACR_EFIAStanding.FRIENDLY;
  if (tier >= current) return tier;
  float bound = 25 * current;
  if (value < bound - MARGIN) return tier;
  return current;
 }

 // The standing moves by 'delta' (an offence when 'offence'); 'why' goes in the feed line if the
 // tier changes.
 static void Change(string side, float delta, string why, bool offence)
 {
  ACR_FIASide standing = SideOf(side);
  if (!s_bLeaning || !standing || delta == 0 || !Replication.IsServer()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (offence) standing.m_fLastOffence = now;
  standing.m_fStanding = Math.Clamp(standing.m_fStanding + delta, 0, 100);
  int tier = TierOf(standing.m_fStanding, standing.m_iTier);
  if (tier == standing.m_iTier) return;
  if (standing.m_iTier == ACR_EFIAStanding.FRIENDLY) Unfriend(side);
  standing.m_iTier = tier;
  if (tier == ACR_EFIAStanding.FRIENDLY) Befriend(side);
  string line = string.Format("FIA is now %1 towards us (%2)", Label(tier), why);
  PrintFormat("[ACR] INSURGENCY FIA standing of %1: %2 (%3, %4)", side, Label(tier), (int)standing.m_fStanding, why);
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(side);
  if (intel) intel.AddFeed(now, line);
 }

 // An FIA man killed by 'by' (ACR_Report): less where a cell sprang an ambush on them.
 static void OnFIAKilled(IEntity victim, string by)
 {
  if (!s_bLeaning || !victim || !SideOf(by)) return;
  float cost = KILL;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || !post.m_Cell.m_bSprung) continue;
   if (vector.DistanceXZ(post.m_vPos, victim.GetOrigin()) < SPRUNG_REACH || vector.DistanceXZ(post.m_Cell.m_vKillZone, victim.GetOrigin()) < SPRUNG_REACH) cost = KILL_SPRUNG;
  }
  Change(by, -cost, "FIA fighters killed", true);
 }

 // A base taken by 'by' from 'from' (ACR_Report).
 static void OnCapture(string by, string from)
 {
  if (from == ACR_Pockets.FACTION) Change(by, -POCKET_TAKEN, "an FIA pocket taken", true);
 }

 // A building destroyed at 'pos' by someone of 'by' (the modded SCR_DestructibleBuildingComponent).
 static void OnBuildingDown(vector pos, string by)
 {
  if (!s_bLeaning || !SideOf(by)) return;
  float cost = BUILDING;
  if (NearFIA(pos, NEAR_FIA)) cost = BUILDING_NEAR;
  PrintFormat("[ACR] INSURGENCY %1 destroyed a building at %2 (-%3)", by, ACR_Manager.Grid(pos), cost);
  Change(by, -cost, "buildings destroyed", true);
 }

 // ---------------------------------------------------------------- hideouts

 // The main sides with a living man (a squad's or a player) within range of pos.
 static void SidesNear(vector pos, float range, notnull array<string> sides)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction()) continue;
   string key = squad.GetFaction().GetFactionKey();
   if (key == ACR_Pockets.FACTION || sides.Contains(key)) continue;
   IEntity anchor = squad.GetAnchor();
   if (!anchor || vector.DistanceXZ(anchor.GetOrigin(), pos) > range + 300) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body || !ACR_Respawn.IsAlive(body) || vector.DistanceXZ(body.GetOrigin(), pos) > range) continue;
    sides.Insert(key);
    break;
   }
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (!player || !ACR_Respawn.IsAlive(player)) continue;
   string side = ACR_Intel.FactionKeyOf(player);
   if (side.IsEmpty() || side == ACR_Pockets.FACTION || sides.Contains(side)) continue;
   if (vector.DistanceXZ(player.GetOrigin(), pos) <= range) sides.Insert(side);
  }
 }

 // A hideout opened fire (ACR_Guerrillas.Spring): the sides with men near have found it.
 static void HideoutFound(ACR_DefenderPost post)
 {
  array<string> sides = {};
  SidesNear(post.m_vPos, ACR_Guerrillas.HIDEOUT_FOUND, sides);
  ACR_Manager manager = ACR_Manager.GetInstance();
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (string side : sides)
  {
   if (Friendly(side, ACR_Pockets.FACTION) || !ACR_Guerrillas.Know(post, side)) continue;
   ACR_DangerRoad mark = new ACR_DangerRoad();
   mark.m_sSide = side;
   mark.m_vPos = post.m_vPos;
   mark.m_fUntil = now;
   mark.m_Hideout = post;
   s_aRoads.Insert(mark);
   PrintFormat("[ACR] INSURGENCY %1 found %2", side, post.m_sName);
   ACR_Intel intel;
   if (manager) intel = manager.IntelFor(side);
   if (intel) intel.AddFeed(now, string.Format("FIA hideout found at %1", ACR_Manager.Grid(post.m_vPos)));
  }
 }

 // A hideout wiped out (ACR_Guerrillas.PackedUp): its marks go, the sides that knew it hear,
 // and the sides with men there pay for it in standing.
 static void HideoutDestroyed(ACR_DefenderPost post)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = s_aRoads.Count() - 1; i >= 0; i--)
  {
   if (s_aRoads[i].m_Hideout != post) continue;
   Release(s_aRoads[i]);
   s_aRoads.RemoveOrdered(i);
  }
  foreach (string knew : post.m_Cell.m_aKnownBy)
  {
   ACR_Intel intel;
   if (manager) intel = manager.IntelFor(knew);
   if (intel) intel.AddFeed(now, string.Format("FIA hideout at %1 destroyed", ACR_Manager.Grid(post.m_vPos)));
  }
  array<string> sides = {};
  SidesNear(post.m_vPos, HIDEOUT_TAKEN_REACH, sides);
  foreach (string side : sides) Change(side, -HIDEOUT_TAKEN, "an FIA hideout destroyed", true);
  PrintFormat("[ACR] INSURGENCY %1 destroyed (%2 sides there)", post.m_sName, sides.Count());
 }

 // An FIA pocket or guerrilla cell within 'range' of pos.
 static bool NearFIA(vector pos, float range)
 {
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_bPocket && vector.DistanceXZ(base.GetOrigin(), pos) < range) return true;
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts) if (post.m_Cell && !post.m_bGone && vector.DistanceXZ(post.m_Cell.m_vKillZone, pos) < range) return true;
  return false;
 }

 // Once a minute: bases near pockets cost, restraint pays; friendly sides get tip-offs.
 static void TickLeaning()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_FIASide standing : s_aSides)
  {
   int near;
   foreach (ACR_Base base : ACR_Base.s_All)
   {
    if (base.m_bPocket || base.m_bRetired || base.m_sOwner != standing.m_sSide) continue;
    foreach (ACR_Base pocket : ACR_Base.s_All)
    {
     if (!pocket.m_bPocket || vector.DistanceXZ(base.GetOrigin(), pocket.GetOrigin()) > LAND_REACH) continue;
     near++;
     break;
    }
   }
   float delta = -LAND_PER_MIN * near;
   string why = "our bases by FIA land";
   if (now - standing.m_fLastOffence >= RECOVER_AFTER_MS && standing.m_fStanding < RECOVER_CAP)
   {
    float climb = Math.Min(RECOVER_PER_MIN, RECOVER_CAP - standing.m_fStanding);
    delta += climb;
    if (delta > 0) why = "we left FIA alone";
   }
   Change(standing.m_sSide, delta, why, false);
   if (standing.m_iTier == ACR_EFIAStanding.FRIENDLY) TipOff(standing, now);
  }
 }

 // FIA tells its friend about the friend's enemies near its cells and pockets.
 protected static void TipOff(ACR_FIASide standing, float now)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Intel intel = manager.IntelFor(standing.m_sSide);
  if (!intel) return;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction()) continue;
   string key = squad.GetFaction().GetFactionKey();
   if (key == standing.m_sSide || key == ACR_Pockets.FACTION || Friendly(key, standing.m_sSide)) continue;
   IEntity anchor = squad.GetAnchor();
   if (!anchor || !NearFIA(anchor.GetOrigin(), TIP_REACH)) continue;
   float until;
   if (standing.m_mTipped.Find(squad.GetName(), until) && now < until) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   int men;
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (!body || !ACR_Respawn.IsAlive(body)) continue;
    intel.Tip(body, now, "FIA");
    men++;
   }
   if (men == 0) continue;
   standing.m_mTipped.Set(squad.GetName(), now + TIP_AGAIN_MS);
   intel.AddFeed(now, string.Format("FIA tip-off: %1 enemy soldiers near %2", men, ACR_Manager.Grid(anchor.GetOrigin())));
  }
 }

 // ---------------------------------------------------------------- dangerous roads

 // A soldier or vehicle of 'side' lost at 'pos' to FIA (ACR_Report).
 static void OnLoss(string side, vector pos, string by)
 {
  if (by != ACR_Pockets.FACTION || side.IsEmpty() || side == by || !Replication.IsServer()) return;
  if (ACR_Minefields.RoadClearance(pos) > ROAD_REACH) return;
  // A fight for a pocket is a base fight, not an ambush on the road.
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_bPocket && vector.DistanceXZ(base.GetOrigin(), pos) < base.m_fRadius + 100) return;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_DangerRoad known : s_aRoads)
  {
   if (known.m_sSide != side || vector.DistanceXZ(known.m_vPos, pos) > DANGER_RADIUS) continue;
   known.m_fUntil = now + DANGER_MS;
   known.m_iLosses++;
   return;
  }
  ACR_DangerRoad road = new ACR_DangerRoad();
  road.m_sSide = side;
  road.m_vPos = pos;
  road.m_fUntil = now + DANGER_MS;
  road.m_iLosses = 1;
  s_aRoads.Insert(road);
  PrintFormat("[ACR] INSURGENCY %1 lost a man to FIA at %2: a dangerous road for %3 min", side, ACR_Manager.Grid(pos), (int)(DANGER_MS / 60000));
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  ACR_Intel intel = manager.IntelFor(side);
  if (intel) intel.AddFeed(now, string.Format("FIA ambush at %1: the road there is dangerous", ACR_Manager.Grid(pos)));
 }

 // Every second (ACR_Manager.Tick): marks run out; once a minute, the leaning.
 static void Tick()
 {
  if (s_bLeaning && Replication.IsServer())
  {
   s_iSeconds++;
   if (s_iSeconds % 60 == 0) TickLeaning();
  }
  if (s_aRoads.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  for (int i = s_aRoads.Count() - 1; i >= 0; i--)
  {
   if (s_aRoads[i].m_Hideout && !s_aRoads[i].m_Hideout.m_bGone) continue;
   if (!s_aRoads[i].m_Hideout && now < s_aRoads[i].m_fUntil) continue;
   PrintFormat("[ACR] INSURGENCY the dangerous road at %1 for %2 ran out", ACR_Manager.Grid(s_aRoads[i].m_vPos), s_aRoads[i].m_sSide);
   Release(s_aRoads[i]);
   s_aRoads.RemoveOrdered(i);
  }
 }

 // Cleared by a sweeper (ACR_AICommander.ManageDangerRoads).
 static void Clear(ACR_DangerRoad road, string why)
 {
  PrintFormat("[ACR] INSURGENCY the dangerous road at %1 for %2 cleared (%3)", ACR_Manager.Grid(road.m_vPos), road.m_sSide, why);
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(road.m_sSide);
  if (intel) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), string.Format("The road at %1 is clear again", ACR_Manager.Grid(road.m_vPos)));
  Release(road);
  s_aRoads.RemoveItem(road);
 }

 protected static void Release(ACR_DangerRoad road)
 {
  if (road.m_Sweeper && road.m_Sweeper.m_iAITask == ACR_EAITask.SWEEP) road.m_Sweeper.m_iAITask = ACR_EAITask.FREE;
  road.m_Sweeper = null;
 }

 // The first of side's dangerous roads within reach of the way from a to b (null: none).
 static ACR_DangerRoad OnTheWay(string side, vector a, vector b)
 {
  foreach (ACR_DangerRoad road : s_aRoads)
   if (!road.m_Hideout && road.m_sSide == side && ACR_AICommander.DistanceToSegment(road.m_vPos, a, b) < DANGER_RADIUS) return road;
  return null;
 }

 // The side's command map: a danger ring per dangerous road.
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  foreach (ACR_DangerRoad road : s_aRoads)
  {
   if (road.m_sSide != faction || road.m_Hideout) continue; // drawn by ACR_Guerrillas.Zones
   positions.Insert(road.m_vPos);
   radii.Insert(DANGER_RADIUS);
   labels.Insert("FIA ambush");
  }
 }
}
