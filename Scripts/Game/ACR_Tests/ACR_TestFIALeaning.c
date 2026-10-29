// srv:fialeaning (Everon whole map, about 1 min): Insurgency part 2, FIA leans by conduct
// (ACR_Insurgency, #89). Checks: both sides start wary with FIA hostile; the panel line shows it;
// a neutral side is spared by the cells (a team beside one doesn't bring it out) but still counts
// for the pockets; killing FIA lowers the standing, with a margin before the tier drops and a
// feed line when it does; taking a pocket and destroying a building cost (more near FIA);
// friendly means the game's factions are friendly, the pockets known and enemies near a cell
// tipped off; falling back to hostile makes the factions hostile again and doubles the side's
// traffic weight; restraint climbs back.
class ACR_TestFIALeaning
{
 static const ResourceName FIRE_TEAM = "{84E5BBAB25EA23E5}Prefabs/Groups/BLUFOR/Group_US_FireTeam.et";
 static const ResourceName USSR_TEAM = "{657590C1EC9E27D3}Prefabs/Groups/OPFOR/Group_USSR_LightFireTeam.et";
 protected static ACR_DefenderPost s_Cell;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 10) Start(manager);
  if (!s_Cell) return;
  if (second == 12) Spawn(manager, "Neutral Team", ACR_FactionSelection.West(), FIRE_TEAM, s_Cell.m_Cell.m_vKillZone, 150);
  if (second == 25) Spared(manager);
  if (second == 26) Offences(manager);
  if (second == 28) Friendly(manager);
  if (second == 29) Spawn(manager, "Tipped Team", ACR_FactionSelection.East(), USSR_TEAM, s_Cell.m_Cell.m_vKillZone, 200);
  if (second == 45) TipOff(manager);
  if (second == 46) Hostile(manager);
  if (second == 47) Restraint(manager);
 }

 protected static void Start(ACR_Manager manager)
 {
  ACR_Options.Set("acrFIAAllegiance", "hostile");
  ACR_Options.Set("acrFIALeaning", "1");
  ACR_Insurgency.Apply();
  string us = ACR_FactionSelection.West();
  string ussr = ACR_FactionSelection.East();
  bool wary = ACR_Insurgency.Leaning() && ACR_Insurgency.Tier(us) == ACR_EFIAStanding.WARY && ACR_Insurgency.Tier(ussr) == ACR_EFIAStanding.WARY;
  ACR_TestRegress.Check("fialeaning: with FIA hostile, both sides start wary and at war with it", wary && !ACR_Insurgency.Friendly(ACR_Pockets.FACTION, us) && !ACR_Insurgency.Friendly(ACR_Pockets.FACTION, ussr), string.Format("%1 / %2", ACR_Insurgency.Label(ACR_Insurgency.Tier(us)), ACR_Insurgency.Label(ACR_Insurgency.Tier(ussr))));
  string tag = ACR_Insurgency.StandingTag(us);
  ACR_TestRegress.Check("fialeaning: the panel's command line shows the standing", tag.Contains("FIA: wary"), tag);
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts) if (post.m_Cell && !post.m_Cell.m_bHouse && !post.m_Group && !s_Cell) s_Cell = post;
  if (!s_Cell)
  {
   ACR_TestRegress.Abort("fialeaning", "no guerrilla road cell");
   return;
  }
  // Blue to neutral.
  ACR_Insurgency.Change(us, 20, "test", false);
 }

 protected static void Spared(ACR_Manager manager)
 {
  string us = ACR_FactionSelection.West();
  bool neutral = ACR_Insurgency.Tier(us) == ACR_EFIAStanding.NEUTRAL && ACR_Insurgency.Spared(us) && !ACR_Insurgency.Spared(ACR_FactionSelection.East());
  bool cellSees = ACR_Defenders.EnemyWithin(manager, ACR_Pockets.FACTION, s_Cell.m_vPos, 400, true);
  bool pocketSees = ACR_Defenders.EnemyWithin(manager, ACR_Pockets.FACTION, s_Cell.m_vPos, 400);
  ACR_TestRegress.Check("fialeaning: a neutral team beside a cell doesn't bring it out (pockets still see it)", neutral && !s_Cell.m_Group && !cellSees && pocketSees, string.Format("tier %1, deployed %2, cell sees %3, pocket sees %4", ACR_Insurgency.Label(ACR_Insurgency.Tier(us)), s_Cell.m_Group != null, cellSees, pocketSees));
  ACR_TestRegress.Check("fialeaning: a spared side's traffic draws no ambush", ACR_Insurgency.TrafficWeight(us) == 0 && ACR_Insurgency.TrafficWeight(ACR_FactionSelection.East()) == 1, "");
 }

 protected static void Offences(ACR_Manager manager)
 {
  string us = ACR_FactionSelection.West();
  ACR_FIASide side = ACR_Insurgency.SideOf(us);
  ACR_Intel intel = manager.IntelFor(us);
  // Any entity away from a sprung cell stands in for the dead man: a full-price kill.
  IEntity victim;
  ACR_Squad team = Find(manager, "Neutral Team");
  if (team) victim = team.GetAnchor();
  float before = side.m_fStanding;
  ACR_Insurgency.OnFIAKilled(victim, us);
  ACR_Insurgency.OnFIAKilled(victim, us);
  bool held = ACR_Insurgency.Tier(us) == ACR_EFIAStanding.NEUTRAL;
  ACR_Insurgency.OnFIAKilled(victim, us);
  bool dropped = ACR_Insurgency.Tier(us) == ACR_EFIAStanding.WARY;
  bool told = intel && !intel.m_Feed.IsEmpty() && intel.m_Feed[0].Contains("FIA is now wary");
  ACR_TestRegress.Check("fialeaning: killing FIA lowers the standing, the tier drops past a margin, with a feed line", victim && held && dropped && told && Math.AbsFloat(before - side.m_fStanding - 3 * ACR_Insurgency.KILL) < 0.01, string.Format("%1 -> %2, held %3, dropped %4, told %5", before, side.m_fStanding, held, dropped, told));
  before = side.m_fStanding;
  ACR_Insurgency.OnCapture(us, ACR_Pockets.FACTION);
  bool pocket = Math.AbsFloat(before - side.m_fStanding - ACR_Insurgency.POCKET_TAKEN) < 0.01;
  ACR_Base fiaBase;
  foreach (ACR_Base base : ACR_Base.s_All) if (base.m_bPocket && !fiaBase) fiaBase = base;
  before = side.m_fStanding;
  ACR_Insurgency.OnBuildingDown(Vector(-100000, 0, -100000), us);
  float plain = before - side.m_fStanding;
  before = side.m_fStanding;
  if (fiaBase) ACR_Insurgency.OnBuildingDown(fiaBase.GetOrigin(), us);
  float nearFIA = before - side.m_fStanding;
  ACR_TestRegress.Check("fialeaning: taking a pocket and destroying buildings cost, more near FIA", pocket && Math.AbsFloat(plain - ACR_Insurgency.BUILDING) < 0.01 && Math.AbsFloat(nearFIA - ACR_Insurgency.BUILDING_NEAR) < 0.01, string.Format("pocket %1, building %2, near FIA %3", pocket, plain, nearFIA));
 }

 protected static void Friendly(ACR_Manager manager)
 {
  string us = ACR_FactionSelection.West();
  ACR_Insurgency.Change(us, 100, "test", false);
  SCR_Faction fia = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(ACR_Pockets.FACTION));
  SCR_Faction west = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(us));
  bool game = fia && west && fia.IsFactionFriendly(west) && west.IsFactionFriendly(fia);
  int pockets, known;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.m_bPocket) continue;
   pockets++;
   if (base.OwnerSeenBy(us) == ACR_Pockets.FACTION) known++;
  }
  ACR_TestRegress.Check("fialeaning: friendly: no fire either way and the pockets known", ACR_Insurgency.Tier(us) == ACR_EFIAStanding.FRIENDLY && ACR_Insurgency.Friendly(ACR_Pockets.FACTION, us) && game && pockets > 0 && known == pockets, string.Format("game factions friendly %1, %2 of %3 pockets known", game, known, pockets));
 }

 protected static void TipOff(ACR_Manager manager)
 {
  ACR_Intel intel = manager.IntelFor(ACR_FactionSelection.West());
  ACR_Insurgency.TickLeaning();
  bool tipped;
  if (intel)
  {
   foreach (string line : intel.m_Feed) if (line.Contains("FIA tip-off")) tipped = true;
  }
  ACR_Squad team = Find(manager, "Tipped Team");
  string detail = "no enemy team";
  if (team && team.GetAnchor()) detail = string.Format("enemy team %1 m from the cell", (int)vector.DistanceXZ(team.GetAnchor().GetOrigin(), s_Cell.m_Cell.m_vKillZone));
  ACR_TestRegress.Check("fialeaning: friendly: FIA tips it off about enemies near a cell", tipped, detail);
 }

 protected static void Hostile(ACR_Manager manager)
 {
  string us = ACR_FactionSelection.West();
  ACR_Insurgency.Change(us, -100, "test", true);
  SCR_Faction fia = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(ACR_Pockets.FACTION));
  SCR_Faction west = SCR_Faction.Cast(GetGame().GetFactionManager().GetFactionByKey(us));
  bool hostile = fia && west && !fia.IsFactionFriendly(west) && !west.IsFactionFriendly(fia);
  ACR_TestRegress.Check("fialeaning: fallen to hostile: at war again, its traffic draws the ambushes twice as hard", ACR_Insurgency.Tier(us) == ACR_EFIAStanding.HOSTILE && hostile && !ACR_Insurgency.Friendly(ACR_Pockets.FACTION, us) && ACR_Insurgency.TrafficWeight(us) == 2, string.Format("game factions hostile %1, weight %2", hostile, ACR_Insurgency.TrafficWeight(us)));
 }

 protected static void Restraint(ACR_Manager manager)
 {
  ACR_FIASide blue = ACR_Insurgency.SideOf(ACR_FactionSelection.West());
  ACR_FIASide red = ACR_Insurgency.SideOf(ACR_FactionSelection.East());
  red.m_fLastOffence = GetGame().GetWorld().GetWorldTime() - ACR_Insurgency.RECOVER_AFTER_MS - 1000;
  float blueBefore = blue.m_fStanding;
  float redBefore = red.m_fStanding;
  ACR_Insurgency.TickLeaning();
  float land = ACR_Insurgency.LAND_PER_MIN * NearPockets(red.m_sSide);
  float landBlue = ACR_Insurgency.LAND_PER_MIN * NearPockets(blue.m_sSide);
  bool climbed = Math.AbsFloat(red.m_fStanding - redBefore - (ACR_Insurgency.RECOVER_PER_MIN - land)) < 0.01;
  bool held = Math.AbsFloat(blue.m_fStanding - Math.Max(0, blueBefore - landBlue)) < 0.01;
  ACR_TestRegress.Check("fialeaning: restraint climbs back, a recent offence doesn't", climbed && held, string.Format("red %1 -> %2 (land %3), blue %4 -> %5", redBefore, red.m_fStanding, land, blueBefore, blue.m_fStanding));
  ACR_TestRegress.Complete("fialeaning", 10);
 }

 protected static int NearPockets(string side)
 {
  int near;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bPocket || base.m_bRetired || base.m_sOwner != side) continue;
   foreach (ACR_Base pocket : ACR_Base.s_All)
   {
    if (!pocket.m_bPocket || vector.DistanceXZ(base.GetOrigin(), pocket.GetOrigin()) > ACR_Insurgency.LAND_REACH) continue;
    near++;
    break;
   }
  }
  return near;
 }

 protected static ACR_Squad Find(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == name) return squad;
  return null;
 }

 protected static void Spawn(ACR_Manager manager, string name, string side, ResourceName prefab, vector at, float distance)
 {
  vector spot = ACR_Manager.Ground(at + Vector(distance, 0, 0));
  manager.m_sSpawningFaction = side;
  manager.SpawnUnit(name, prefab, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }
}
