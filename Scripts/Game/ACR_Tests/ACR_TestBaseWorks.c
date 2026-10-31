// srv:baseworks, srv:baseworksarland (about 6 min): Fortify on the middle of a base (ACR_BaseWorks,
// #105). A squad at the biggest town we can give the West. Checks: the option, the click's reach and
// what is refused, charging nothing; the first site paid alone; a new order refunds the site in hand
// and leaves the rest open; tier I built site by site and on the side's map; tier II with its
// defenders and a nest whose real gun can fire, tier III with more, and nothing past that; a lost
// site opens again and only it is rebuilt; the base falling hands the works over standing.
// srv:baseworksai: AI commanders with RP banked fortify a town of their own. srv:baseworksplan: where
// the sites go, on every base of a map, against the hard rules (RunPlan).
class ACR_TestBaseWorks
{
 static const ResourceName SQUAD = "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
 static const ResourceName ENEMY = "{E552DABF3636C2AD}Prefabs/Groups/OPFOR/Group_USSR_RifleSquad.et";
 static const string NAME = "Works Squad";
 static const int STEP_LIMIT = 420; // s a tier may take (a site it can't reach is given up on after its walk + 90 s)
 static const float ENEMY_CLEAR = 700; // m from the other side's men for the town the test picks
 static const int CHECKS = 50;
 protected static int s_iStep;
 protected static int s_iStepAt;
 protected static int s_iRP;
 protected static int s_iQueued;
 protected static ACR_Base s_Base;
 protected static ACR_BaseWork s_Work;
 protected static ACR_BaseSite s_Lost;
 protected static ACR_BaseSite s_Skipped;
 protected static ACR_BaseSite s_Chipped;
 protected static int s_iChipWalls;
 protected static int s_iRepairCost;
 protected static int s_iBuiltAtGive;
 protected static int s_iMen;
 protected static string s_sSide;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  if (System.IsCLIParam("acrBaseWorksPlanTest"))
  {
   foreach (ACR_FactionSettings aside : ACR_FactionSettings.s_All) aside.m_bAICommander = false;
   RunPlan(manager, second);
   return;
  }
  if (System.IsCLIParam("acrBaseWorksAITest"))
  {
   RunAI(manager, second);
   return;
  }
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 8) Setup(manager);
  if (second < 15 || !s_Base) return;
  ACR_Squad squad = Find(manager);
  if (!squad)
  {
   if (second == 40) ACR_TestRegress.Abort("baseworks", "no squad");
   return;
  }
  ACR_FactionData us = manager.m_Bases.GetFaction(s_sSide);
  int waited = second - s_iStepAt;
  switch (s_iStep)
  {
   case 0: Refusals(squad, us); Next(second); break;
   case 1: First(squad, us); Next(second); break;
   case 2: if (waited >= 3) { Replaced(squad, us); Give(squad); Next(second); } break;
   case 3: if (waited >= 2) { Gave(squad, us); Next(second); } break;
   case 4: if (Tier(1, waited)) { TierOne(squad, us, manager); OrderNext(squad); Next(second); } break;
   case 5: if (Retried(waited)) { OrderNext(squad); Next(second); } break;
   case 6: if (Tier(2, waited)) { TierTwo(squad); OrderNext(squad); Next(second); } break;
   case 7: if (Tier(3, waited)) { TierThree(squad); Lose(); Next(second); } break;
   case 8: if (waited >= 7) { Lost(squad, us); Next(second); } break;
   case 9: if (Rebuilt(waited)) { RebuiltChecks(us); Chip(); Next(second); } break;
   case 10: if (waited >= 7) { Chipped(squad, us); Next(second); } break;
   case 11: if (Repaired(waited)) { RepairedChecks(us); EnemyNear(manager); Next(second); } break;
   case 12: if (Deployed(waited)) { DeployedChecks(); s_Base.m_sOwner = ACR_FactionSelection.East(); Next(second); } break;
   case 13: if (waited >= 7) { Fallen(); s_iStep = 99; } break;
  }
 }

 protected static void Next(int second)
 {
  s_iStep++;
  s_iStepAt = second;
 }

 protected static bool Tier(int tier, int waited)
 {
  if (s_Work && s_Work.m_iTier >= tier) return true;
  if (waited % 30 == 29) PrintFormat("[ACR] TEST baseworks tier %1, %2 s: %3", tier, waited, Where());
  if (waited > STEP_LIMIT) ACR_TestRegress.Abort("baseworks", string.Format("tier %1 not done in %2 s: %3 built, %4 open; %5", tier, STEP_LIMIT, ACR_BaseWorks.BuiltSites(s_Work, tier), ACR_BaseWorks.OpenSites(s_Work, tier), Where()));
  return false;
 }

 // Where the squad is and what it says it's doing, when it's taking too long.
 protected static string Where()
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return "";
  ACR_Squad squad = Find(manager);
  if (!squad || !squad.GetAnchor()) return "no squad";
  string plan = "no plan";
  if (squad.m_FortifyPlan) plan = string.Format("site %1, %2 m off, stage %3", squad.m_FortifyPlan.m_vSite, (int)vector.DistanceXZ(squad.GetAnchor().GetOrigin(), squad.m_FortifyPlan.m_vSite), squad.m_FortifyPlan.m_vStage);
  bool waypoint = squad.m_Group && squad.m_Group.GetCurrentWaypoint();
  return string.Format("%1 at %2, order %3, %4, waypoint %5, %6 men, enemies %7, [%8], seed %9", squad.GetName(), squad.GetAnchor().GetOrigin(), squad.OrderState(), plan, waypoint, squad.CountAlive(), squad.KnownEnemies(60, true), squad.ThinkMovement(), ACR_Generator.Seed());
 }

 protected static ACR_Squad Find(ACR_Manager manager)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == NAME) return squad;
  return null;
 }

 // Far enough from the other side's men that the squad works in peace: Saint-Pierre is 200 m from
 // where the Soviets start, and a fight there held the squad up or thinned the reserve.
 protected static bool EnemyClose(ACR_Manager manager, ACR_Base base)
 {
  string east = ACR_FactionSelection.East();
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.GetAnchor() || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != east) continue;
   if (vector.DistanceXZ(squad.GetAnchor().GetOrigin(), base.GetOrigin()) < ENEMY_CLEAR) return true;
  }
  foreach (ACR_Base other : ACR_Base.s_All)
  {
   if (other && !other.m_bRetired && other.m_sOwner == east && vector.DistanceXZ(other.GetOrigin(), base.GetOrigin()) < ENEMY_CLEAR) return true;
  }
  return false;
 }

 // The base with the most buildings, ours for the test.
 protected static void Setup(ACR_Manager manager)
 {
  s_sSide = ACR_FactionSelection.West();
  ACR_Fortify.WORK_SCALE = 0.05;
  ACR_BaseSystem.s_bNoIncomeForTest = true; // the RP sums below are the sites' alone
  ACR_BaseWorks.s_iMaxSitesForTest = 1;
  int most = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_bHidden || base.m_bBuilt || base.m_sOwner == ACR_FactionSelection.East() || EnemyClose(manager, base)) continue;
   array<IEntity> buildings = {};
   ACR_BaseDefences.Buildings(base.GetOrigin(), ACR_BaseDefences.SearchRadius(base), buildings);
   if (buildings.Count() <= most) continue;
   // A town with a nest built into a line and something on tier III: some have nowhere with the view
   // for them, and the test goes through every tier.
   ACR_BaseWork plan = ACR_BaseWorks.WorkFor(base, s_sSide);
   bool nest = false;
   bool third = false;
   foreach (ACR_BaseSite site : plan.m_aSites)
   {
    if (site.m_eKind == ACR_EBaseSite.NEST && !site.m_aCuts.IsEmpty()) nest = true;
    if (site.m_iTier == 3) third = true;
   }
   if (!nest || !third)
   {
    PrintFormat("[ACR] TEST baseworks: %1 (%2 buildings) has no nest in a line or no tier III, on to the next", base.m_sName, buildings.Count());
    ACR_BaseWorks.s_aWorks.RemoveItem(plan);
    continue;
   }
   if (s_Base) ACR_BaseWorks.s_aWorks.RemoveItem(ACR_BaseWorks.Find(s_Base));
   most = buildings.Count();
   s_Base = base;
  }
  if (!s_Base)
  {
   ACR_TestRegress.Abort("baseworks", "no base");
   return;
  }
  s_Base.m_sOwner = s_sSide;
  manager.m_Bases.GetFaction(s_sSide).m_iRP = 1000;
  PrintFormat("[ACR] TEST baseworks at %1: %2 buildings", s_Base.m_sName, most);
  vector spot = ACR_Manager.Ground(s_Base.GetOrigin() + Vector(25, 0, 25));
  manager.m_sSpawningFaction = s_sSide;
  manager.SpawnUnit(NAME, SQUAD, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }

 protected static void Refusals(ACR_Squad squad, ACR_FactionData us)
 {
  s_iRP = us.m_iRP;
  vector center = s_Base.GetOrigin();
  ACR_TestRegress.Check("baseworks: on by default, a click on the middle means the base", ACR_BaseWorks.BaseAt(center + Vector(30, 0, 0), s_sSide) == s_Base, "");
  ACR_TestRegress.Check("baseworks: a click further out is a plain Fortify", ACR_BaseWorks.BaseAt(center + Vector(ACR_BaseWorks.CLICK_RADIUS + 5, 0, 0), s_sSide) != s_Base, "");
  ACR_Options.Set("acrBaseWorks", "0");
  ACR_TestRegress.Check("baseworks: -acrBaseWorks 0 turns it off", !ACR_BaseWorks.Enabled() && !ACR_BaseWorks.BaseAt(center, s_sSide), "");
  ACR_Options.Unset("acrBaseWorks");
  ACR_Base enemy;
  foreach (ACR_Base other : ACR_Base.s_All)
   if (other && !other.m_bRetired && other.m_sOwner == ACR_FactionSelection.East()) enemy = other;
  string why;
  bool refused = true;
  if (enemy) refused = !ACR_BaseWorks.Order(squad, enemy, why);
  ACR_TestRegress.Check("baseworks: an enemy base is refused", refused && us.m_iRP == s_iRP, why);
  s_Base.m_bBuilt = true;
  ACR_TestRegress.Check("baseworks: a FOB is pointed to its own panel", ACR_BaseWorks.BaseAt(center, s_sSide) == s_Base && !ACR_BaseWorks.Order(squad, s_Base, why) && why.Contains("FOB") && us.m_iRP == s_iRP, why);
  s_Base.m_bBuilt = false;
 }

 protected static void First(ACR_Squad squad, ACR_FactionData us)
 {
  string why;
  bool ok = ACR_BaseWorks.Order(squad, s_Base, why);
  s_Work = ACR_BaseWorks.Find(s_Base);
  ACR_TestRegress.Check("baseworks: the order takes", ok && s_Work != null, why);
  if (!ok || !s_Work)
  {
   ACR_TestRegress.Abort("baseworks", "no works: " + why);
   return;
  }
  int t1 = ACR_BaseWorks.OpenSites(s_Work, 1);
  int t2 = ACR_BaseWorks.OpenSites(s_Work, 2);
  int t3 = ACR_BaseWorks.OpenSites(s_Work, 3);
  ACR_TestRegress.Check("baseworks: sites planned on all three tiers", t1 > 0 && t2 > 0 && t3 > 0, string.Format("%1 / %2 / %3 (one of each kind in the test)", t1, t2, t3));
  ACR_TestRegress.Check("baseworks: planning stays under 2 s", ACR_BaseWorks.s_iLastPlanMs < 2000, string.Format("%1 ms", ACR_BaseWorks.s_iLastPlanMs));
  ACR_TestRegress.Check("baseworks: only the first site is paid", squad.m_FortifyPlan && us.m_iRP == s_iRP - squad.m_FortifyPlan.m_iCost, string.Format("%1 -> %2", s_iRP, us.m_iRP));
  s_iQueued = ACR_Navmesh.s_iQueued;
  // Not done before the new order comes: a stretch can stand right beside where the squad spawned.
  if (squad.m_FortifyPlan) squad.m_FortifyPlan.m_fDuration = 600;
 }

 protected static void Replaced(ACR_Squad squad, ACR_FactionData us)
 {
  int open = ACR_BaseWorks.OpenSites(s_Work, 1);
  ACR_OrderSnapshot aside = squad.SnapshotOrder();
  squad.MoveTo(ACR_Manager.Ground(s_Base.GetOrigin() + Vector(20, 0, 0)));
  ACR_TestRegress.Check("baseworks: a new order refunds the site in hand", !squad.m_FortifyPlan && us.m_iRP == s_iRP, string.Format("%1 of %2", us.m_iRP, s_iRP));
  ACR_TestRegress.Check("baseworks: ... and the sites stay open", ACR_BaseWorks.OpenSites(s_Work, 1) == open && ACR_BaseWorks.BuiltSites(s_Work) == 0, "");
  // A Fortify order called off while it was set aside isn't put back (a regroup, a player's turn).
  squad.RestoreOrder(aside);
  ACR_TestRegress.Check("baseworks: a called-off order isn't restored", squad.OrderState() != ACR_EOrderState.FORTIFY && !squad.m_FortifyPlan, squad.OrderState().ToString());
  string why;
  bool again = ACR_BaseWorks.Order(squad, s_Base, why);
  ACR_TestRegress.Check("baseworks: ordered again it goes on", again, why);
  if (squad.m_FortifyPlan) squad.m_FortifyPlan.m_fDuration = 600;
 }

 // Three minutes with no progress on this site, as with a truck parked on it.
 protected static void Give(ACR_Squad squad)
 {
  if (!squad.m_FortifyPlan) return;
  s_Skipped = squad.m_FortifyPlan.m_Site;
  s_iBuiltAtGive = ACR_BaseWorks.BuiltSites(s_Work);
  squad.m_FortifyPlan.m_bArrived = true;
  squad.m_FortifyPlan.m_fLastProgress = GetGame().GetWorld().GetWorldTime() - ACR_Squad.FOB_GIVE_UP_MS - 1000;
 }

 protected static void Gave(ACR_Squad squad, ACR_FactionData us)
 {
  bool onward = squad.m_FortifyPlan && s_Skipped && squad.m_FortifyPlan.m_Site != s_Skipped && s_Skipped.m_bSkipped;
  int paid;
  if (squad.m_FortifyPlan) paid = squad.m_FortifyPlan.m_iCost;
  // A next site right beside the squad can be up in the seconds this waits, and stays paid for.
  int builtSince = ACR_BaseWorks.BuiltSites(s_Work) - s_iBuiltAtGive;
  ACR_TestRegress.Check("baseworks: a site it can't finish is refunded and it goes on", onward && us.m_iRP == s_iRP - paid * (1 + builtSince), string.Format("%1 -> %2, %3 built since", s_iRP, us.m_iRP, builtSince));
  if (squad.m_FortifyPlan) Stopped(squad, squad.m_FortifyPlan);
 }

 // A man walking is left alone; stopped dead with his waypoint still there, he is sent on.
 protected static void Stopped(ACR_Squad squad, ACR_FortifyPlan plan)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  vector at = squad.GetAnchor().GetOrigin();
  plan.m_vWalkFrom = at + Vector(ACR_Squad.WALK_MOVED_M + 3, 0, 0);
  plan.m_fWalkAt = now - ACR_Squad.WALK_STOPPED_MS - 1000;
  squad.KeepWalking(plan, now);
  ACR_TestRegress.Check("baseworks: a squad on the move is left to walk", plan.m_iWalkRetries == 0 && plan.m_fWalkAt == now, string.Format("%1 tries", plan.m_iWalkRetries));
  plan.m_vWalkFrom = at;
  plan.m_fWalkAt = now - ACR_Squad.WALK_STOPPED_MS - 1000;
  squad.KeepWalking(plan, now);
  bool waypoint = squad.m_Group && squad.m_Group.GetCurrentWaypoint();
  ACR_TestRegress.Check("baseworks: a squad stopped on its walk is sent on", plan.m_iWalkRetries == 1 && waypoint, string.Format("%1 tries, waypoint %2", plan.m_iWalkRetries, waypoint));
  // Still there on the third try: its leader goes ahead to walkable ground, nearer the site.
  plan.m_iWalkRetries = 2;
  plan.m_vWalkFrom = at;
  plan.m_fWalkAt = now - ACR_Squad.WALK_STOPPED_MS - 1000;
  float before = vector.DistanceXZ(at, plan.m_vStage);
  squad.KeepWalking(plan, now);
  vector stepped = squad.GetAnchor().GetOrigin();
  float after = vector.DistanceXZ(stepped, plan.m_vStage);
  ACR_TestRegress.Check("baseworks: a leader stuck for good is stepped ahead", before < 3 || after < before - 1, string.Format("%1 m to go, then %2", (int)before, (int)after));
  plan.m_iWalkRetries = 0;
 }

 protected static bool Retried(int waited)
 {
  if (s_Skipped && s_Skipped.m_bBuilt)
  {
   ACR_TestRegress.Check("baseworks: the next order tries the skipped site again", true, "");
   return true;
  }
  if (waited > STEP_LIMIT) ACR_TestRegress.Abort("baseworks", "the skipped site wasn't tried again");
  return false;
 }

 protected static void TierOne(ACR_Squad squad, ACR_FactionData us, ACR_Manager manager)
 {
  int built = ACR_BaseWorks.BuiltSites(s_Work, 1);
  bool standing = true;
  bool damage = false;
  int paid;
  foreach (ACR_BaseSite site : s_Work.m_aSites)
  {
   if (site.m_iTier != 1 || !site.m_bBuilt) continue;
   paid += ACR_BaseWorks.Cost(site);
   if (site.m_aWalls.IsEmpty()) standing = false;
   foreach (IEntity piece : site.m_aWalls)
   {
    if (!piece) standing = false;
    else if (piece.FindComponent(DamageManagerComponent)) damage = true;
   }
  }
  ACR_TestRegress.Check("baseworks: tier I built, every site standing", built > 0 && standing && ACR_BaseWorks.OpenSites(s_Work, 1) == 0, string.Format("%1 sites", built));
  ACR_TestRegress.Check("baseworks: paid per site", us.m_iRP == s_iRP - paid, string.Format("%1 -> %2, %3 for the sites", s_iRP, us.m_iRP, paid));
  ACR_TestRegress.Check("baseworks: the navmesh is told", ACR_Navmesh.s_iQueued > s_iQueued, string.Format("%1 more", ACR_Navmesh.s_iQueued - s_iQueued));
  ACR_TestRegress.Check("baseworks: the sandbags have a damage manager", damage, "");
  LineChecks();
  array<vector> positions = {};
  array<float> radii = {};
  array<string> labels = {};
  ACR_Fortify.Zones(s_sSide, positions, radii, labels);
  string label = "";
  foreach (string line : labels) if (line.Contains("Defences I")) label = line;
  ACR_TestRegress.Check("baseworks: on the side's map", !label.IsEmpty(), label);
  ACR_TestRegress.Check("baseworks: the squad holds the base after the tier", !squad.m_FortifyPlan && squad.OrderState() == ACR_EOrderState.DEFEND, squad.OrderState().ToString());
 }

 // Tier I as it stands in the world: stretches of line whose real pieces meet end to end.
 protected static void LineChecks()
 {
  int stretches, loose, pieces;
  string worst = "";
  foreach (ACR_BaseSite site : s_Work.m_aSites)
  {
   if (site.m_iTier != 1 || !site.m_bBuilt || !site.m_Plan || site.m_Plan.m_eKind != ACR_EBaseDefenceKind.LINE) continue;
   stretches++;
   foreach (int i, IEntity piece : site.m_aWalls)
   {
    pieces++;
    if (i == 0) continue;
    vector a = piece.GetOrigin();
    vector b = site.m_aWalls[i - 1].GetOrigin();
    float halves = LengthOf(piece) * 0.5 + LengthOf(site.m_aWalls[i - 1]) * 0.5;
    float gap = vector.DistanceXZ(a, b) - halves;
    if (gap < 0.6) continue;
    loose++;
    worst = string.Format("%1 m at %2", gap.ToString(-1, 2), a);
   }
  }
  ACR_TestRegress.Check("baseworks: tier I is a sandbag line, in stretches", stretches > 0 && pieces >= stretches * 2, string.Format("%1 stretches, %2 pieces", stretches, pieces));
  ACR_TestRegress.Check("baseworks: its real pieces meet end to end", stretches > 0 && loose == 0, string.Format("%1 joints open %2", loose, worst));
 }

 // Along the piece: its model's size across its own X.
 protected static float LengthOf(IEntity piece)
 {
  vector mins, maxs;
  piece.GetBounds(mins, maxs);
  return maxs[0] - mins[0];
 }

 protected static void OrderNext(ACR_Squad squad)
 {
  string why;
  if (!ACR_BaseWorks.Order(squad, s_Base, why)) ACR_TestRegress.Abort("baseworks", "next tier refused: " + why);
 }

 protected static void TierTwo(ACR_Squad squad)
 {
  bool real = true;
  bool nest = false;
  string kinds = "";
  foreach (ACR_BaseSite site : s_Work.m_aSites)
  {
   if (site.m_iTier != 2 || !site.m_bBuilt) continue;
   kinds += ACR_BaseWorks.SiteWords(site) + ", ";
   if (site.m_eKind == ACR_EBaseSite.NEST) nest = true;
   if (site.m_eKind == ACR_EBaseSite.NEST && !site.m_aWalls[0].FindComponent(SCR_CampaignBuildingCompositionComponent)) real = false;
   if (site.m_eKind == ACR_EBaseSite.ROADBLOCK && !site.m_Roadblock) real = false;
  }
  ACR_TestRegress.Check("baseworks: tier II built", ACR_BaseWorks.BuiltSites(s_Work, 2) > 0 && real, kinds);
  ACR_TestRegress.Check("baseworks: tier II has an MG nest", nest, kinds);
  NestFires();
  NestInLine();
  int men = Men();
  bool crewOut = s_Work.m_Defenders && !s_Work.m_Defenders.m_aCrewGuns.IsEmpty();
  ACR_TestRegress.Check("baseworks: tier II has defenders in reserve", men == ACR_BaseWorks.Defenders(2), string.Format("%1 men", men));
  ACR_TestRegress.Check("baseworks: the reserve is told to man the nest's gun", crewOut, "");
 }

 // The nest as built: the game's line of fire check on its real gun, and the gun looks the way the
 // planner looked. (The planner only ever saw the place and a bearing.)
 protected static void NestFires()
 {
  foreach (ACR_BaseSite site : s_Work.m_aSites)
  {
   if (site.m_iTier != 2 || !site.m_bBuilt || site.m_eKind != ACR_EBaseSite.NEST) continue;
   string detail;
   ACR_TestRegress.Check("baseworks: the built nest's gun has a clear line of fire", ACR_Fortify.LineOfFire(site.m_aWalls[0], detail), detail);
   IEntity gun = ACR_Roadblocks.FindTurretInHierarchy(site.m_aWalls[0]);
   float turned = 180;
   if (gun)
   {
    vector transform[4];
    gun.GetWorldTransform(transform);
    turned = Math.AbsFloat(ACR_BaseSurvey.Normal(ACR_BaseDefences.Bearing(transform[2]) - site.m_fYaw + 180) - 180);
   }
   ACR_TestRegress.Check("baseworks: a man on its gun still counts as infantry", gun && ACR_BaseWorks.OwnGun(s_sSide, gun), "");
   ACR_TestRegress.Check("baseworks: ... and looks the way the planner checked", gun && turned < 45, string.Format("%1 deg off facing %2", (int)turned, (int)site.m_fYaw));
   return;
  }
  ACR_TestRegress.Check("baseworks: the built nest's gun has a clear line of fire", false, "no nest built");
  ACR_TestRegress.Check("baseworks: a man on its gun still counts as infantry", false, "no nest built");
  ACR_TestRegress.Check("baseworks: ... and looks the way the planner checked", false, "no nest built");
 }

 // The pieces where a nest goes came out when it went in.
 protected static void NestInLine()
 {
  int left = 0;
  int cuts = 0;
  foreach (ACR_BaseSite nest : s_Work.m_aSites)
  {
   if (nest.m_eKind != ACR_EBaseSite.NEST || !nest.m_bBuilt) continue;
   cuts += nest.m_aCuts.Count();
   foreach (ACR_BaseSite site : s_Work.m_aSites)
   {
    if (!site.m_Plan) continue;
    foreach (IEntity piece : site.m_aWalls)
    {
     if (!piece) continue;
     foreach (vector cut : nest.m_aCuts) if (vector.DistanceXZ(piece.GetOrigin(), cut) < 0.6) left++;
    }
   }
  }
  ACR_TestRegress.Check("baseworks: a nest takes the place of the line's pieces", cuts > 0 && left == 0, string.Format("%1 places, %2 pieces still there", cuts, left));
 }

 // The works' men: in reserve, or on the guns.
 protected static int Men()
 {
  if (!s_Work.m_Defenders) return 0;
  int men = s_Work.m_Defenders.m_iReserve;
  if (s_Work.m_Defenders.m_GunCrew)
  {
   array<AIAgent> crew = {};
   s_Work.m_Defenders.m_GunCrew.GetAgents(crew);
   men += crew.Count();
  }
  return men;
 }

 protected static void EnemyNear(ACR_Manager manager)
 {
  // In front of a line: from a side no line faces the reserve takes the buildings, as it should.
  vector spot = ACR_Manager.Ground(s_Work.m_Defenders.m_vPos + Vector(250, 0, 0));
  if (!s_Work.m_aLines.IsEmpty()) spot = ACR_Manager.Ground(s_Work.m_aLines[0].m_vMiddle + s_Work.m_aLines[0].m_vFacing * 150);
  manager.m_sSpawningFaction = ACR_FactionSelection.East();
  manager.SpawnUnit("Works Enemy", ENEMY, "", false, "", spot, spot);
  manager.m_sSpawningFaction = "";
 }

 protected static bool Deployed(int waited)
 {
  if (s_Work.m_Defenders && s_Work.m_Defenders.m_Group) return true;
  if (waited > 40) ACR_TestRegress.Abort("baseworks", "no defenders deployed with an enemy near");
  return false;
 }

 protected static void DeployedChecks()
 {
  SCR_AIGroup group = s_Work.m_Defenders.m_Group;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  ACR_TestRegress.Check("baseworks: an enemy near deploys the reserve", agents.Count() > 0, string.Format("%1 men", agents.Count()));
  ACR_TestRegress.Check("baseworks: ... each to his own place behind a line's bags", s_Work.m_Defenders.m_iMannedLine >= 0, string.Format("line %1", s_Work.m_Defenders.m_iMannedLine));
  ACR_TestRegress.Check("baseworks: ... on posts of their own", ACR_Garrison.HasPosts(group), string.Format("at %1", s_Work.m_Defenders.m_vPos));
 }

 protected static void TierThree(ACR_Squad squad)
 {
  int men = Men();
  array<IEntity> buildings = {};
  ACR_BaseDefences.Buildings(s_Base.GetOrigin(), ACR_BaseDefences.SearchRadius(s_Base) + 60, buildings);
  float nearest = 1000;
  foreach (ACR_BaseSite site : s_Work.m_aSites)
  {
   if (site.m_iTier == 3 && site.m_bBuilt && site.m_eKind == ACR_EBaseSite.BUNKER) nearest = Math.Min(nearest, ACR_BaseDefences.BuildingGap(buildings, site.m_vPos, 8));
  }
  // (From its origin, at the back of it: a tower is narrow there. Its bags are checked in plan.)
  ACR_TestRegress.Check("baseworks: the built bunker stands clear of the buildings", nearest >= 2, string.Format("%1 m", nearest.ToString(-1, 1)));
  ACR_TestRegress.Check("baseworks: tier III built, more defenders", ACR_BaseWorks.BuiltSites(s_Work, 3) > 0 && men == ACR_BaseWorks.Defenders(3), string.Format("%1 men", men));
  string why;
  ACR_TestRegress.Check("baseworks: nothing past tier III", !ACR_BaseWorks.Order(squad, s_Base, why), why);
 }

 protected static void Lose()
 {
  foreach (ACR_BaseSite site : s_Work.m_aSites)
  {
   if (site.m_iTier != 1 || !site.m_bBuilt) continue;
   s_Lost = site;
   foreach (IEntity piece : site.m_aWalls) if (piece) SCR_EntityHelper.DeleteEntityAndChildren(piece);
   return;
  }
 }

 protected static void Lost(ACR_Squad squad, ACR_FactionData us)
 {
  ACR_TestRegress.Check("baseworks: a site with no walls left opens again", s_Lost && !s_Lost.m_bBuilt && ACR_BaseWorks.OpenTier(s_Work) == 1 && s_Lost.m_aDressing.IsEmpty(), "");
  ACR_TestRegress.Check("baseworks: the tiers still stand", s_Work.m_iTier == 3, "");
  s_iMen = Men();
  s_iRP = us.m_iRP;
  OrderNext(squad);
 }

 protected static bool Rebuilt(int waited)
 {
  if (s_Lost && s_Lost.m_bBuilt) return true;
  if (waited % 30 == 29) PrintFormat("[ACR] TEST baseworks rebuild, %1 s: %2", waited, Where());
  if (waited > STEP_LIMIT) ACR_TestRegress.Abort("baseworks", "the lost site wasn't rebuilt: " + Where());
  return false;
 }

 protected static void RebuiltChecks(ACR_FactionData us)
 {
  ACR_TestRegress.Check("baseworks: only the lost site is rebuilt and paid", us.m_iRP == s_iRP - ACR_BaseWorks.Cost(s_Lost) && ACR_BaseWorks.OpenTier(s_Work) == 0, string.Format("%1 -> %2", s_iRP, us.m_iRP));
  ACR_TestRegress.Check("baseworks: a rebuild adds no men", Men() == s_iMen, string.Format("%1 then %2", s_iMen, Men()));
 }

 // Two pieces of a stretch shot away, the rest up.
 protected static void Chip()
 {
  foreach (ACR_BaseSite site : s_Work.m_aSites)
  {
   if (site.m_iTier != 1 || !site.m_bBuilt || !site.m_Plan || site.m_Plan.m_eKind != ACR_EBaseDefenceKind.LINE || site.m_aWalls.Count() < 4) continue;
   s_Chipped = site;
   s_iChipWalls = site.m_aWalls.Count();
   SCR_EntityHelper.DeleteEntityAndChildren(site.m_aWalls[0]);
   SCR_EntityHelper.DeleteEntityAndChildren(site.m_aWalls[1]);
   return;
  }
 }

 protected static void Chipped(ACR_Squad squad, ACR_FactionData us)
 {
  ACR_TestRegress.Check("baseworks: a stretch with pieces shot away stands, and opens for a repair", s_Chipped && s_Chipped.m_bBuilt && s_Chipped.m_bDamaged && ACR_BaseWorks.OpenTier(s_Work) == 1 && s_Work.m_iTier == 3 && ACR_BaseWorks.NeedsRepair(s_Base), string.Format("%1 walls of %2", s_Chipped.m_aWalls.Count(), s_iChipWalls));
  s_iRP = us.m_iRP;
  s_iRepairCost = Math.Max(1, Math.Round(ACR_BaseWorks.Cost(s_Chipped) * ACR_BaseWorks.MissingShare(s_Work, s_Chipped)));
  OrderNext(squad);
 }

 protected static bool Repaired(int waited)
 {
  if (s_Chipped && !s_Chipped.m_bDamaged) return true;
  if (waited % 30 == 29) PrintFormat("[ACR] TEST baseworks repair, %1 s: %2", waited, Where());
  if (waited > STEP_LIMIT) ACR_TestRegress.Abort("baseworks", "the stretch wasn't repaired: " + Where());
  return false;
 }

 protected static void RepairedChecks(ACR_FactionData us)
 {
  ACR_TestRegress.Check("baseworks: the repair puts back what was lost, no more", s_Chipped.m_aWalls.Count() == s_iChipWalls, string.Format("%1, was %2", s_Chipped.m_aWalls.Count(), s_iChipWalls));
  ACR_TestRegress.Check("baseworks: and costs its share of the stretch", s_iRepairCost < ACR_BaseWorks.Cost(s_Chipped) && us.m_iRP == s_iRP - s_iRepairCost, string.Format("%1 -> %2, share %3", s_iRP, us.m_iRP, s_iRepairCost));
  ACR_TestRegress.Check("baseworks: nothing else is open after", ACR_BaseWorks.OpenTier(s_Work) == 0, "");
 }

 protected static void Fallen()
 {
  string east = ACR_FactionSelection.East();
  ACR_TestRegress.Check("baseworks: the works pass to whoever takes the base", s_Work.m_sFaction == east && ACR_BaseWorks.BuiltSites(s_Work) > 0 && s_Work.m_iTier == 3, s_Work.m_sFaction);
  ACR_TestRegress.Check("baseworks: the men in reserve don't change hands", !s_Work.m_Defenders, "");
  ACR_TestRegress.Complete("baseworks", CHECKS);
 }

 // ---------------------------------------------------------------- the AI

 protected static void RunAI(ACR_Manager manager, int second)
 {
  if (second == 5) ACR_Options.Set("acrFortifyAI", "1");
  if (second >= 5 && second % 30 == 5)
  {
   array<string> sides = {ACR_FactionSelection.West(), ACR_FactionSelection.East()};
   foreach (string side : sides)
   {
    ACR_FactionData data = manager.m_Bases.GetFaction(side);
    if (data) data.m_iRP = 1000;
   }
  }
  if (second < 60 || second % 10 != 0) return;
  int built, tier;
  string where = "";
  foreach (ACR_BaseWork work : ACR_BaseWorks.s_aWorks)
  {
   built += ACR_BaseWorks.BuiltSites(work);
   tier = Math.Max(tier, work.m_iTier);
   where += string.Format(" %1 %2:%3", work.m_sFaction, work.m_Base.m_sName, ACR_BaseWorks.BuiltSites(work));
  }
  if (tier < 1 && second < 500) return;
  ACR_TestRegress.Check("baseworksai: an AI commander fortifies a town", tier >= 1, string.Format("%1 sites built, by second %2:%3", built, second, where));
  // Past the first site it pays from the build budget too.
  ACR_AICommander ai = manager.AICommanderFor(ACR_FactionSelection.East());
  bool stops = false;
  if (ai)
  {
   ACR_Options.Set("acrAIBuildShare", "15");
   ai.m_fBuildBudget = 25;
   bool enough = ACR_BaseWorks.AIMayPay(ai, 20);
   ai.m_fBuildBudget = 15;
   stops = enough && !ACR_BaseWorks.AIMayPay(ai, 20);
   ACR_Options.Unset("acrAIBuildShare");
  }
  ACR_TestRegress.Check("baseworksai: the next site waits for the build budget", stops, "");
  ACR_TestRegress.Complete("baseworksai", 2);
 }

 // ---------------------------------------------------------------- the placement, every town

 // srv:baseworksplan[arland|kolguyev] (about 2 min, no squad): the works' plan of every base on the
 // map, checked against the hard rules with traces and queries of its own, not the planner's (so a
 // plan from the old ring, Strong, fails them: tried on aea7d35). Prints each plan's geometry for
 // drawing (WORKSGEO lines). On Kolguyev also the spot of the 30 Oct playtest's Bovemont Base.
 protected static int s_iBases, s_iNests, s_iBunkers, s_iWalls, s_iChecks, s_iPosts, s_iLineTowns, s_iLongTowns, s_iLines, s_iPieces, s_iMostPieces;
 protected static int s_iLoose, s_iShortRuns, s_iGunAlone, s_iEnds, s_iLongRuns, s_iLongStretch, s_iLongTier, s_iNestOff, s_iNoCheckLane, s_iWorstTier;
 protected static ref set<int> s_aLengths = new set<int>();
 protected static int s_iSeeing, s_iLooked, s_iGapByNest, s_iFragments, s_iLoneCheck, s_iAnchored, s_iEnds2, s_iBehind, s_iCramped;
 protected static float s_fOurs, s_fEither;
 static const float COVER_FLOOR = 0.5;   // of a threatened arc, by lines, on average (the maps reach 55-68%)
 static const float ANCHOR_FLOOR = 0.5;  // the maps reach 55-78%
 protected static int s_iLongLine, s_iLongNoNest, s_iOnApron;
 protected static int s_iFire, s_iClose, s_iFacing, s_iPlannedFacing, s_iBuilding, s_iDoor, s_iRoad, s_iDressRoad, s_iApart, s_iDoubled, s_iFarCheck, s_iWorstMs, s_iNestTowns;
 protected static string s_sWorst;
 protected static ref array<IEntity> s_aFound = {};

 protected static void RunPlan(ACR_Manager manager, int second)
 {
  if (second != 12) return;
  s_sSide = ACR_FactionSelection.West();
  int most = -1;
  ACR_Base biggest;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_bBuilt) continue;
   array<IEntity> buildings = {};
   ACR_BaseDefences.Buildings(base.GetOrigin(), ACR_BaseDefences.SearchRadius(base), buildings);
   if (buildings.Count() > most)
   {
    most = buildings.Count();
    biggest = base;
   }
   PlanChecks(base);
  }
  ACR_TestRegress.Check("baseworks plan: every base planned", s_iBases >= 5, string.Format("%1 bases: %2 lines, %3 sandbag pieces, %4 stretches and wire, %5 nests in %6 towns, %7 bunkers and towers, %8 checkpoints, %9 command posts", s_iBases, s_iLines, s_iPieces, s_iWalls, s_iNests, s_iNestTowns, s_iBunkers, s_iChecks, s_iPosts));
  ACR_TestRegress.Check("baseworks plan: most towns have a line", s_iLineTowns * 3 >= s_iBases * 2, string.Format("%1 of %2", s_iLineTowns, s_iBases));
  ACR_TestRegress.Check("baseworks plan: most of those have one of 30 m or more", s_iLongTowns * 2 >= s_iBases, string.Format("%1 of %2 towns", s_iLongTowns, s_iBases));
  ACR_TestRegress.Check("baseworks plan: pieces meet end to end", s_iLoose == 0, string.Format("%1 joints open", s_iLoose));
  // An end piece may close right on a wall; a run of bags with a fence at its back can't be manned.
  ACR_TestRegress.Check("baseworks plan: a man has room behind the bags", s_iCramped * 20 <= s_iBehind, string.Format("%1 of %2 long pieces", s_iCramped, s_iBehind));
  ACR_TestRegress.Check("baseworks plan: no sandbag piece or stub on its own", s_iShortRuns == 0, string.Format("%1 runs under 5 m or a single piece", s_iShortRuns));
  ACR_TestRegress.Check("baseworks plan: line ends stand on something or curve back", s_iEnds == 0, string.Format("%1 ends", s_iEnds));
  ACR_TestRegress.Check("baseworks plan: every gun touches a line", s_iGunAlone == 0, string.Format("%1 of %2 over 14 m from a sandbag", s_iGunAlone, s_iNests + s_iBunkers));
  ACR_TestRegress.Check("baseworks plan: a town with a long line gets a nest", s_iLongNoNest * 4 <= s_iLongLine, string.Format("%1 of %2 towns with a line of 60 m or more have none", s_iLongNoNest, s_iLongLine));
  ACR_TestRegress.Check("baseworks plan: no sandbags across a runway or apron", s_iOnApron * 100 <= s_iPieces, string.Format("%1 of %2 pieces", s_iOnApron, s_iPieces));
  ACR_TestRegress.Check("baseworks plan: every nest is built into a line, 6 m of bags each side", s_iNestOff == 0, string.Format("%1 of %2 not", s_iNestOff, s_iNests));
  ACR_TestRegress.Check("baseworks plan: no walk gap beside a nest", s_iGapByNest == 0, string.Format("%1", s_iGapByNest));
  ACR_TestRegress.Check("baseworks plan: no line is short of 30 m unless it closes a gap", s_iFragments == 0, string.Format("%1 of %2", s_iFragments, s_iLines));
  ACR_TestRegress.Check("baseworks plan: checkpoints stand in a line's gate", s_iLoneCheck == 0, string.Format("%1 of %2 over 16 m from the line", s_iLoneCheck, s_iChecks));
  float oursShare = s_fOurs / Math.Max(s_iLineTowns, 1);
  float eitherShare = s_fEither / Math.Max(s_iBases, 1);
  float anchorShare = s_iAnchored * 1.0 / Math.Max(s_iEnds2, 1);
  PrintFormat("[ACR] TEST baseworks cover all: lines %1%%, with what stands there %2%%, anchored ends %3%%", (int)(oursShare * 100), (int)(eitherShare * 100), (int)(anchorShare * 100));
  ACR_TestRegress.Check("baseworks plan: lines cover the threatened arc", oursShare >= COVER_FLOOR, string.Format("%1%% in towns with a line, floor %2%%", (int)(oursShare * 100), (int)(COVER_FLOOR * 100)));
  ACR_TestRegress.Check("baseworks plan: most ends stand on something", anchorShare >= ANCHOR_FLOOR, string.Format("%1 of %2 ends, floor %3%%", s_iAnchored, s_iEnds2, (int)(ANCHOR_FLOOR * 100)));
  ACR_TestRegress.Check("baseworks plan: a way through on foot every so often", s_iLongRuns == 0, string.Format("%1 runs over 120 m unbroken", s_iLongRuns));
  ACR_TestRegress.Check("baseworks plan: stretches stay a stretch", s_iLongStretch == 0, string.Format("%1 over 40 m", s_iLongStretch));
  ACR_TestRegress.Check("baseworks plan: pieces per base stay under the cap", s_iMostPieces <= ACR_BaseLines.MOST_PIECES, string.Format("most %1 (cap %2)", s_iMostPieces, ACR_BaseLines.MOST_PIECES));
  ACR_TestRegress.Check("baseworks plan: tier I stays a reasonable job", s_iWorstTier <= 330, string.Format("worst %1 s of work", s_iWorstTier));
  ACR_TestRegress.Check("baseworks plan: towns don't get one template", s_aLengths.Count() * 4 >= s_iLines, string.Format("%1 different line lengths in %2 lines", s_aLengths.Count(), s_iLines));
  ACR_TestRegress.Check("baseworks plan: every gun has a clear line of fire (the game's own check)", s_iFire == 0, string.Format("%1 of %2 blocked", s_iFire, s_iNests + s_iBunkers));
  ACR_TestRegress.Check("baseworks plan: no gun faces a wall, fence or building close in front", s_iClose == 0, string.Format("%1 of %2", s_iClose, s_iNests + s_iBunkers));
  ACR_TestRegress.Check("baseworks plan: a gun sees 40 m along its middle", s_iFacing == 0, string.Format("%1 of %2", s_iFacing, s_iNests + s_iBunkers));
  ACR_TestRegress.Check("baseworks plan: no stretch of line stands blind", s_iPlannedFacing * 100 <= s_iWalls, string.Format("%1 of %2 stretches with most pieces seeing under 10 m (1 in 100 allowed: a tree line or a low wall can do that)", s_iPlannedFacing, s_iWalls));
  ACR_TestRegress.Check("baseworks plan: the lines' pieces see out", s_iSeeing * 100 >= s_iLooked * 85, string.Format("%1 of %2 long pieces", s_iSeeing, s_iLooked));
  ACR_TestRegress.Check("baseworks plan: nothing stands close to a building", s_iBuilding == 0, string.Format("%1 sites", s_iBuilding));
  ACR_TestRegress.Check("baseworks plan: nothing at a door", s_iDoor == 0, string.Format("%1 sites", s_iDoor));
  ACR_TestRegress.Check("baseworks plan: no gun or sandbag on a road", s_iRoad == 0, string.Format("%1 sites", s_iRoad));
  ACR_TestRegress.Check("baseworks plan: no dressing on a road", s_iDressRoad == 0, string.Format("%1 pieces", s_iDressRoad));
  ACR_TestRegress.Check("baseworks plan: guns stand well apart", s_iApart == 0, string.Format("%1 pairs under 30 m", s_iApart));
  ACR_TestRegress.Check("baseworks plan: no two nests side by side look the same way", s_iDoubled == 0, string.Format("%1 pairs under 50 m, within 20 degrees", s_iDoubled));
  ACR_TestRegress.Check("baseworks plan: checkpoints stay near the town", s_iFarCheck == 0, string.Format("%1 of %2", s_iFarCheck, s_iChecks));
  ACR_TestRegress.Check("baseworks plan: planning cost", s_iWorstMs < 800, string.Format("worst %1 ms (%2)", s_iWorstMs, s_sWorst));
  CrateChecks();
  NestWidth();
  SeedChecks(biggest);
  Draw(biggest);
  Replay();
  ACR_TestRegress.Complete("baseworksplan", 39);
 }

 protected static void PlanChecks(ACR_Base base)
 {
  string faction = base.m_sOwner;
  if (faction.IsEmpty()) faction = s_sSide;
  ACR_BaseWork work = ACR_BaseWorks.WorkFor(base, faction);
  s_iBases++;
  if (ACR_BaseWorks.s_iLastPlanMs > s_iWorstMs)
  {
   s_iWorstMs = ACR_BaseWorks.s_iLastPlanMs;
   s_sWorst = base.m_sName;
  }
  float radius = ACR_BaseDefences.SearchRadius(base);
  vector center = base.GetOrigin();
  array<IEntity> buildings = {};
  ACR_BaseDefences.Buildings(center, radius + 100, buildings);
  array<ref ACR_BaseDefenceSeg> segs = {};
  ACR_BaseDefences.Roads(center, radius + 100, segs);
  array<vector> doors = {};
  Doors(buildings, doors);
  array<vector> guns = {};
  array<float> yaws = {};
  array<int> kinds = {};
  int nests;
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (site.m_eKind == ACR_EBaseSite.ROADBLOCK)
   {
    s_iChecks++;
    if (vector.DistanceXZ(site.m_vPos, center) > radius + 90)
    {
     s_iFarCheck++;
     PrintFormat("[ACR] TEST baseworks plan %1: checkpoint %2 m out", base.m_sName, (int)vector.DistanceXZ(site.m_vPos, center));
    }
    continue;
   }
   if (site.m_eKind == ACR_EBaseSite.PLANNED)
   {
    PlannedChecks(base, site, buildings, segs, doors);
    continue;
   }
   if (site.m_eKind == ACR_EBaseSite.NEST)
   {
    s_iNests++;
    nests++;
   }
   else s_iBunkers++;
   GunChecks(base, site, buildings, segs, doors);
   if (site.m_eKind == ACR_EBaseSite.NEST && !Winged(work, site)) s_iNestOff++;
   if (!NearBags(work, site.m_vPos, 14))
   {
    s_iGunAlone++;
    PrintFormat("[ACR] TEST baseworks plan %1: %2 at %3: no sandbags within 14 m", base.m_sName, ACR_BaseWorks.SiteWords(site), site.m_vPos);
   }
   foreach (int g, vector other : guns)
   {
    if (vector.DistanceXZ(other, site.m_vPos) < 30) s_iApart++;
    if (site.m_eKind == ACR_EBaseSite.NEST && kinds[g] == ACR_EBaseSite.NEST && vector.DistanceXZ(other, site.m_vPos) < 50 && ACR_BaseDefences.DotXZ(ACR_BaseSurvey.Dir(yaws[g]), ACR_BaseSurvey.Dir(site.m_fYaw)) > 0.94) s_iDoubled++;
   }
   guns.Insert(site.m_vPos);
   yaws.Insert(site.m_fYaw);
   kinds.Insert(site.m_eKind);
  }
  if (nests > 0) s_iNestTowns++;
  bool longLine = false;
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   if (line.m_fLength >= 60) longLine = true;
   foreach (ACR_LinePiece bag : line.m_aPieces)
   {
    vector along = ACR_BaseDefences.FlatDir(bag.m_vFrom, bag.m_vTo);
    if (ACR_BaseLiner.WidePaved(bag.m_vPos, along) && ACR_BaseLiner.WidePaved(bag.m_vPos, along * -1)) s_iOnApron++;
   }
  }
  if (longLine)
  {
   s_iLongLine++;
   if (nests == 0)
   {
    s_iLongNoNest++;
    PrintFormat("[ACR] TEST baseworks plan %1: a line of 60 m or more and no nest", base.m_sName);
   }
  }
  JoinChecks(base, work, buildings, segs);
  CoverRow(base, faction, work, buildings);
  foreach (ACR_BaseSite check : work.m_aSites)
   if (check.m_eKind == ACR_EBaseSite.ROADBLOCK && !NearBags(work, check.m_vPos, 16)) s_iLoneCheck++;
 }

 // The nest stands among its line's bags: 12 m of unbroken pieces each side of where it cut.
 protected static bool Winged(ACR_BaseWork work, ACR_BaseSite nest)
 {
  if (nest.m_aCuts.IsEmpty() || nest.m_iLine < 0 || nest.m_iLine >= work.m_aLines.Count()) return false;
  array<ref ACR_LinePiece> pieces = work.m_aLines[nest.m_iLine].m_aPieces;
  int first = -1;
  int last = -1;
  foreach (int i, ACR_LinePiece piece : pieces)
  {
   foreach (vector cut : nest.m_aCuts)
   {
    if (vector.DistanceXZ(cut, piece.m_vPos) > 0.3) continue;
    if (first < 0) first = i;
    last = i;
   }
  }
  if (first < 0) return false;
  float before = 0;
  for (int b = first - 1; b >= 0 && pieces[b].m_iBreak == 0; b--) before += vector.DistanceXZ(pieces[b].m_vFrom, pieces[b].m_vTo);
  float after = 0;
  for (int a = last; a < pieces.Count() - 1 && pieces[a].m_iBreak == 0; a++) after += vector.DistanceXZ(pieces[a + 1].m_vFrom, pieces[a + 1].m_vTo);
  // Walk gaps and nests.
  foreach (int k, ACR_LinePiece near : pieces)
   if (near.m_iBreak == ACR_BaseLines.BREAK_WALK && vector.DistanceXZ(near.m_vTo, nest.m_vPos) < 10) s_iGapByNest++;
  return before >= 6 && after >= 6;
 }

 // A town's coverage row, from its pieces and its own query of walls and buildings: the threatened arc from
 // the town's middle, in 5 degree bins. Works on any plan, the old planner's too.
 protected static void CoverRow(ACR_Base base, string faction, ACR_BaseWork work, array<IEntity> buildings)
 {
  vector center = base.GetOrigin();
  array<int> lines = {};
  array<int> stands = {};
  for (int i = 0; i < 72; i++)
  {
   lines.Insert(0);
   stands.Insert(0);
  }
  int pieces = 0;
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (!site.m_Plan || site.m_Plan.m_eKind == ACR_EBaseDefenceKind.POST) continue;
   foreach (int k, vector at : site.m_Plan.m_aPiecePos)
   {
    if (!site.m_Plan.m_aPieceWall[k]) continue;
    pieces++;
    vector a, b;
    PieceEnds(site.m_Plan.m_aPiecePrefab[k], at, site.m_Plan.m_aPieceYaw[k], a, b);
    Mark(lines, ACR_BaseDefences.Bearing(a - center), ACR_BaseDefences.Bearing(b - center));
   }
  }
  foreach (IEntity house : buildings)
  {
   if (vector.DistanceXZ(house.GetOrigin(), center) > ACR_BaseDefences.SearchRadius(base)) continue;
   vector mins, maxs;
   house.GetBounds(mins, maxs);
   Mark(stands, ACR_BaseDefences.Bearing(house.CoordToParent(Vector(mins[0], 0, mins[2])) - center), ACR_BaseDefences.Bearing(house.CoordToParent(Vector(maxs[0], 0, maxs[2])) - center));
   Mark(stands, ACR_BaseDefences.Bearing(house.CoordToParent(Vector(mins[0], 0, maxs[2])) - center), ACR_BaseDefences.Bearing(house.CoordToParent(Vector(maxs[0], 0, mins[2])) - center));
  }
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(center, ACR_BaseDefences.SearchRadius(base) + 30, CollectWalls, null, EQueryEntitiesFlags.STATIC);
  foreach (IEntity wall : s_aFound)
  {
   vector wMins, wMaxs;
   wall.GetBounds(wMins, wMaxs);
   if (wMaxs[1] - wMins[1] < 1) continue;
   Mark(stands, ACR_BaseDefences.Bearing(wall.CoordToParent(wMins) - center), ACR_BaseDefences.Bearing(wall.CoordToParent(wMaxs) - center));
  }
  s_aFound.Clear();
  array<vector> dirs = {};
  array<float> weights = {};
  ACR_BaseDefences.Threats(base, faction, dirs, weights);
  int arc, ours, either;
  for (int bin = 0; bin < 72; bin++)
  {
   float rad = (bin + 0.5) * 5 * Math.DEG2RAD;
   if (ACR_BaseDefences.ThreatDot(dirs, weights, Vector(Math.Sin(rad), 0, Math.Cos(rad))) < 0.2) continue;
   arc++;
   if (lines[bin] > 0) ours++;
   if (lines[bin] > 0 || stands[bin] > 0) either++;
  }
  arc = Math.Max(arc, 1);
  int anchored, ends;
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   ends += 2;
   anchored += line.Anchors();
  }
  s_iAnchored += anchored;
  s_iEnds2 += ends;
  if (pieces > 0) s_fOurs += ours * 1.0 / arc;
  s_fEither += either * 1.0 / arc;
  PrintFormat("[ACR] TEST baseworks cover %1 | lines %2 | pieces %3 | lines cover %4%% | with what stands %5%% | anchored %6 of %7", base.m_sName, work.m_aLines.Count(), pieces, ours * 100 / arc, either * 100 / arc, anchored, ends);
 }

 protected static void Mark(array<int> bins, float from, float to)
 {
  from = ACR_BaseSurvey.Normal(from);
  to = ACR_BaseSurvey.Normal(to);
  float span = ACR_BaseSurvey.Normal(to - from);
  if (span > 180)
  {
   float swap = from;
   from = to;
   to = swap;
   span = 360 - span;
  }
  for (float at = from; at <= from + span + 0.01; at += 5) bins[(int)(ACR_BaseSurvey.Normal(at) / 5) % 72] = 1;
  bins[(int)(to / 5) % 72] = 1;
 }

 protected static bool NearBags(ACR_BaseWork work, vector at, float within)
 {
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (!site.m_Plan || site.m_Plan.m_eKind == ACR_EBaseDefenceKind.POST) continue;
   foreach (int i, vector piece : site.m_Plan.m_aPiecePos)
    if (site.m_Plan.m_aPieceWall[i] && vector.DistanceXZ(piece, at) < within) return true;
  }
  return false;
 }

 // The ends of a sandbag piece from its place and its model's length alone (not what the planner
 // recorded): along its local X, a round piece's half a metre behind its middle.
 protected static void PieceEnds(ResourceName prefab, vector at, float yaw, out vector a, out vector b)
 {
  vector half = ACR_BaseDefences.HalfSize(prefab);
  float rad = yaw * Math.DEG2RAD;
  vector facing = Vector(Math.Sin(rad), 0, Math.Cos(rad));
  vector right = Vector(facing[2], 0, -facing[0]);
  vector middle = at;
  if (prefab == ACR_BaseDefences.ROUND_PLASTIC || prefab == ACR_BaseDefences.ROUND_BURLAP) middle = at - facing * 0.5;
  a = middle - right * half[0];
  b = middle + right * half[0];
 }

 // Every sandbag piece of a town (not the command post's), by how they actually touch: runs of pieces
 // whose ends meet. Works on any plan, the old planner's too. Then the lines' own order and ends.
 protected static void JoinChecks(ACR_Base base, ACR_BaseWork work, array<IEntity> buildings, array<ref ACR_BaseDefenceSeg> segs)
 {
  array<vector> as = {};
  array<vector> bs = {};
  array<float> lengths = {};
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (!site.m_Plan || site.m_Plan.m_eKind == ACR_EBaseDefenceKind.POST) continue;
   foreach (int i, vector at : site.m_Plan.m_aPiecePos)
   {
    if (!site.m_Plan.m_aPieceWall[i]) continue;
    vector a, b;
    PieceEnds(site.m_Plan.m_aPiecePrefab[i], at, site.m_Plan.m_aPieceYaw[i], a, b);
    as.Insert(a);
    bs.Insert(b);
    lengths.Insert(vector.DistanceXZ(a, b));
   }
  }
  int n = as.Count();
  s_iPieces += n;
  s_iMostPieces = Math.Max(s_iMostPieces, n);
  // Runs: pieces whose ends are within 3 m (a walk-through gap doesn't split a run).
  array<int> run = {};
  for (int r = 0; r < n; r++) run.Insert(r);
  for (int x = 0; x < n; x++)
  {
   for (int y = x + 1; y < n; y++)
   {
    if (run[x] == run[y]) continue;
    if (EndsApart(as[x], bs[x], as[y], bs[y]) > 3) continue;
    int from = run[y];
    int to = run[x];
    for (int z = 0; z < n; z++) if (run[z] == from) run[z] = to;
   }
  }
  float longest = 0;
  for (int c = 0; c < n; c++)
  {
   float length = 0;
   int count = 0;
   for (int d = 0; d < n; d++)
   {
    if (run[d] != c) continue;
    length += lengths[d];
    count++;
   }
   if (count == 0) continue;
   longest = Math.Max(longest, length);
   // (Two pieces can close a gap in a wall, 5 m of it.)
   if (count >= 2 && length >= 5) continue;
   s_iShortRuns++;
   PrintFormat("[ACR] TEST baseworks plan %1: %2 sandbags, %3 m, standing alone at %4", base.m_sName, count, (int)length, as[c]);
  }
  if (n > 0) s_iLineTowns++;
  if (longest >= 30) s_iLongTowns++;
  LineOrder(base, work, buildings, segs);
  // Stretches and the tier's work.
  int seconds = 0;
  foreach (ACR_BaseSite stretch : work.m_aSites)
  {
   if (stretch.m_iTier == 1) seconds += ACR_BaseWorks.Duration(stretch);
   if (!stretch.m_Plan || stretch.m_Plan.m_eKind != ACR_EBaseDefenceKind.LINE) continue;
   float span = 0;
   foreach (int s, vector piece : stretch.m_Plan.m_aPiecePos)
    if (stretch.m_Plan.m_aPieceWall[s]) span += 2 * ACR_BaseDefences.HalfSize(stretch.m_Plan.m_aPiecePrefab[s])[0];
   if (span > 40) s_iLongStretch++;
  }
  s_iWorstTier = Math.Max(s_iWorstTier, seconds);
 }

 protected static float EndsApart(vector a1, vector b1, vector a2, vector b2)
 {
  float least = vector.DistanceXZ(a1, a2);
  least = Math.Min(least, vector.DistanceXZ(a1, b2));
  least = Math.Min(least, vector.DistanceXZ(b1, a2));
  return Math.Min(least, vector.DistanceXZ(b1, b2));
 }

 // A line's own chain: neighbours meet, gaps are gaps; its ends; a way through often enough.
 protected static void LineOrder(ACR_Base base, ACR_BaseWork work, array<IEntity> buildings, array<ref ACR_BaseDefenceSeg> segs)
 {
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   s_iLines++;
   s_aLengths.Insert((int)line.m_fLength);
   if (line.m_fLength < 29.5 && !line.Closes())
   {
    s_iFragments++;
    PrintFormat("[ACR] TEST baseworks plan %1: a line of %2 m, ends %3 and %4", base.m_sName, (int)line.m_fLength, line.m_sEndA, line.m_sEndB);
   }
   int count = line.m_aPieces.Count();
   float unbroken = 0;
   foreach (int i, ACR_LinePiece piece : line.m_aPieces)
   {
    unbroken += vector.DistanceXZ(piece.m_vFrom, piece.m_vTo);
    if (piece.m_bLong)
    {
     s_iBehind++;
     if (!ACR_BaseLiner.RoomBehind(piece.m_vPos, piece.m_vFacing))
     {
      s_iCramped++;
      PrintFormat("[ACR] TEST baseworks plan %1: no room behind the bags at %2", base.m_sName, piece.m_vPos);
     }
    }
    if (i + 1 >= count) continue;
    if (piece.m_iBreak != 0)
    {
     if (unbroken > 120)
     {
      s_iLongRuns++;
      PrintFormat("[ACR] TEST baseworks plan %1: %2 m unbroken to %3", base.m_sName, (int)unbroken, piece.m_vTo);
     }
     unbroken = 0;
     continue;
    }
    ACR_LinePiece next = line.m_aPieces[i + 1];
    vector a1, b1, a2, b2;
    PieceEnds(piece.m_sPrefab, piece.m_vPos, piece.m_fYaw, a1, b1);
    PieceEnds(next.m_sPrefab, next.m_vPos, next.m_fYaw, a2, b2);
    float gap = EndsApart(a1, b1, a2, b2);
    if (gap > 0.6)
    {
     s_iLoose++;
     PrintFormat("[ACR] TEST baseworks plan %1: a joint %2 m open at %3", base.m_sName, gap.ToString(-1, 2), piece.m_vTo);
    }
   }
   if (unbroken > 120)
   {
    s_iLongRuns++;
    PrintFormat("[ACR] TEST baseworks plan %1: %2 m unbroken to its end", base.m_sName, (int)unbroken);
   }
   CheckEnd(base, line, line.m_sEndA, line.m_aPieces, 0, 1, buildings, segs);
   CheckEnd(base, line, line.m_sEndB, line.m_aPieces, count - 1, -1, buildings, segs);
  }
 }

 // An end stands on something near it or turns back from the line's run; a word alone doesn't pass.
 protected static void CheckEnd(ACR_Base base, ACR_BaseLine line, string word, array<ref ACR_LinePiece> pieces, int last, int inward, array<IEntity> buildings, array<ref ACR_BaseDefenceSeg> segs)
 {
  if (pieces.IsEmpty()) return;
  ACR_LinePiece end = pieces[last];
  vector tip = end.m_vFrom;
  if (inward < 0) tip = end.m_vTo;
  bool fine = false;
  if (word == "curve")
  {
   // The end piece, against the last long piece: turned at least 30 degrees, away from the firing side.
   int back = last + inward;
   while (back >= 0 && back < pieces.Count() && !pieces[back].m_bLong) back += inward;
   if (back >= 0 && back < pieces.Count())
   {
    // Both outward along the chain: a first piece's chain runs into the line, a last one's out of it.
    float outer = -1.0 * inward;
    vector endWay = ACR_BaseDefences.FlatDir(end.m_vFrom, end.m_vTo) * outer;
    vector lineWay = ACR_BaseDefences.FlatDir(pieces[back].m_vFrom, pieces[back].m_vTo) * outer;
    fine = ACR_BaseDefences.DotXZ(endWay, lineWay) < 0.87 && ACR_BaseDefences.DotXZ(endWay, pieces[back].m_vFacing) < 0;
   }
  }
  else if (word == "building") fine = ACR_BaseDefences.BuildingGap(buildings, tip, 8) < 8;
  else if (word == "road") fine = ACR_BaseDefences.RoadEdgeGap(segs, tip) < 6;
  else if (word == "wall" || word == "gap" || word == "work" || word == "slope" || word == "rock" || word == "line") fine = true;
  if (fine) return;
  s_iEnds++;
  PrintFormat("[ACR] TEST baseworks plan %1: a line of %2 m ends \"%3\" at %4 on nothing", base.m_sName, (int)line.m_fLength, word, tip);
 }

 protected static void GunChecks(ACR_Base base, ACR_BaseSite site, array<IEntity> buildings, array<ref ACR_BaseDefenceSeg> segs, array<vector> doors)
 {
  string what = ACR_BaseWorks.SiteWords(site);
  float up = 1.1;
  if (site.m_eKind == ACR_EBaseSite.BUNKER)
  {
   up = 1.3;
   if (site.m_iSpot == 1) up = 4.5;
  }
  bool tower = site.m_eKind == ACR_EBaseSite.BUNKER && site.m_iSpot == 1;
  string detail;
  // Where it fires from, forward of its origin (measured): nest 1.1 m, bunker 4 m, tower 3 m.
  float ahead = 1.1;
  if (site.m_eKind == ACR_EBaseSite.BUNKER) ahead = 4;
  if (tower) ahead = 3;
  vector gunAt = site.m_vPos + ACR_BaseSurvey.Dir(site.m_fYaw) * ahead;
  if (!tower && !ACR_Fortify.LineOfFireAt(gunAt, site.m_fYaw, null, detail))
  {
   s_iFire++;
   PrintFormat("[ACR] TEST baseworks plan %1: %2 at %3: %4", base.m_sName, what, site.m_vPos, detail);
  }
  // A wall, fence or building close on any ray is a fail; a tree or rock on two of the five.
  array<float> turns = {-20, -10, 0, 10, 20};
  int blocked = 0;
  bool structure = false;
  string blocker = "";
  foreach (float turn : turns)
  {
   string hit;
   float clear = ClearAhead(gunAt, up, site.m_fYaw + turn, 40, hit);
   if (clear < 20)
   {
    blocked++;
    blocker = string.Format("%1 %2 m ahead (%3 deg)", hit, (int)clear, turn);
    if (s_bHitStructure) structure = true;
   }
   float wanted = 39;
   if (site.m_eKind == ACR_EBaseSite.NEST && site.m_fReach > 0) wanted = site.m_fReach - 6;
   if (turn == 0 && clear < wanted)
   {
    s_iFacing++;
    PrintFormat("[ACR] TEST baseworks plan %1: %2 at %3 facing %4: only %5 m clear (%6)", base.m_sName, what, site.m_vPos, (int)site.m_fYaw, (int)clear, hit);
   }
  }
  if (structure || blocked >= 2)
  {
   s_iClose++;
   PrintFormat("[ACR] TEST baseworks plan %1: %2 at %3 facing %4: %5 (%6 rays)", base.m_sName, what, site.m_vPos, (int)site.m_fYaw, blocker, blocked);
  }
  // (A tower's narrow: its origin can stand closer. Its bags aside, 5 m is what we ask of all.)
  float gap = ACR_BaseDefences.BuildingGap(buildings, gunAt, 5);
  if (gap < 5)
  {
   s_iBuilding++;
   PrintFormat("[ACR] TEST baseworks plan %1: %2 at %3: %4 m from a building", base.m_sName, what, site.m_vPos, gap.ToString(-1, 1));
  }
  if (NearDoor(doors, gunAt, 6))
  {
   s_iDoor++;
   PrintFormat("[ACR] TEST baseworks plan %1: %2 at %3: at a door", base.m_sName, what, site.m_vPos);
  }
  if (ACR_BaseDefences.RoadEdgeGap(segs, site.m_vPos) < 4)
  {
   s_iRoad++;
   PrintFormat("[ACR] TEST baseworks plan %1: %2 at %3: on a road", base.m_sName, what, site.m_vPos);
  }
 }

 protected static void PlannedChecks(ACR_Base base, ACR_BaseSite site, array<IEntity> buildings, array<ref ACR_BaseDefenceSeg> segs, array<vector> doors)
 {
  ACR_BaseDefencePos pos = site.m_Plan;
  string what = ACR_BaseWorks.SiteWords(site);
  if (pos.m_eKind == ACR_EBaseDefenceKind.POST)
  {
   s_iPosts++;
   return;
  }
  s_iWalls++;
  if (pos.m_eKind == ACR_EBaseDefenceKind.WIRE) return;
  bool apart = pos.m_eKind != ACR_EBaseDefenceKind.CORNER && pos.m_eKind != ACR_EBaseDefenceKind.EDGE;
  vector from = vector.Zero;
  int bags = 0;
  bool closed, door, road;
  foreach (int i, vector at : pos.m_aPiecePos)
  {
   if (!pos.m_aPieceWall[i])
   {
    if (ACR_BaseDefences.RoadEdgeGap(segs, at) < 0) s_iDressRoad++;
    continue;
   }
   if (apart && ACR_BaseDefences.BuildingGap(buildings, at, 1) < 0.25) closed = true;
   if (NearDoor(doors, at, 2.5)) door = true;
   if (ACR_BaseDefences.RoadEdgeGap(segs, at) < 0) road = true;
   vector size = ACR_BaseDefences.HalfSize(pos.m_aPiecePrefab[i]);
   if (size[0] < 1.5) continue;
   from = from + at;
   bags++;
  }
  if (closed)
  {
   s_iBuilding++;
   PrintFormat("[ACR] TEST baseworks plan %1: %2 at %3: its bags against a building", base.m_sName, what, pos.m_vPos);
  }
  if (door)
  {
   s_iDoor++;
   PrintFormat("[ACR] TEST baseworks plan %1: %2 at %3: its bags at a door", base.m_sName, what, pos.m_vPos);
  }
  if (road)
  {
   s_iRoad++;
   PrintFormat("[ACR] TEST baseworks plan %1: %2 at %3: its bags on a road", base.m_sName, what, pos.m_vPos);
  }
  if (bags == 0) return;
  // Each long piece's own firing side, 10 m clear (its yaw is the way it faces): most of a stretch's. A
  // tree in front of a piece or two is allowed for (ACR_BaseLiner.MAX_BLIND).
  int sees = 0;
  int looked = 0;
  string blocker = "";
  foreach (int k, vector place : pos.m_aPiecePos)
  {
   if (!ACR_BaseDefences.IsLong(pos.m_aPiecePrefab[k]) || !pos.m_aPieceWall[k]) continue;
   float look = pos.m_aPieceYaw[k];
   string hit;
   float clear = ClearAhead(place + ACR_BaseSurvey.Dir(look) * 0.8, 1.45, look, 15, hit);
   looked++;
   string aside;
   if (clear >= 10 || ClearAhead(place + ACR_BaseSurvey.Dir(look) * 0.8, 1.45, look - 18, 15, aside) >= 10 || ClearAhead(place + ACR_BaseSurvey.Dir(look) * 0.8, 1.45, look + 18, 15, aside) >= 10) sees++;
   else blocker = string.Format("%1 %2 m ahead", hit, (int)clear);
  }
  s_iSeeing += sees;
  s_iLooked += looked;
  if (looked >= 5 && sees * 2 < looked)
  {
   s_iPlannedFacing++;
   PrintFormat("[ACR] TEST baseworks plan %1: %2 at %3: %4 of %5 pieces see out; %6", base.m_sName, what, pos.m_vPos, sees, looked, blocker);
  }
 }

 protected static bool SolidOnly(notnull IEntity e)
 {
  if (ChimeraCharacter.Cast(e) || Vehicle.Cast(e)) return false;
  if (e.GetPrefabData() && e.GetPrefabData().GetPrefabName().Contains("Barbed")) return false;
  if (Tree.Cast(e))
  {
   vector mins, maxs;
   e.GetBounds(mins, maxs);
   if (maxs[1] - mins[1] < 2.5) return false;
  }
  return true;
 }

 // m clear ahead at a height over the ground, up to range. Rising ground is LineOfFireAt's to judge:
 // past it, it counts as clear (a level trace goes on under the ground into whatever stands there).
 protected static bool s_bHitStructure;
 protected static float ClearAhead(vector place, float up, float bearing, float range, out string hit)
 {
  hit = "";
  s_bHitStructure = false;
  BaseWorld world = GetGame().GetWorld();
  float rad = bearing * Math.DEG2RAD;
  vector dir = Vector(Math.Sin(rad), 0, Math.Cos(rad));
  vector muzzle = place;
  muzzle[1] = world.GetSurfaceY(place[0], place[2]) + up;
  TraceParam trace = new TraceParam();
  trace.Start = muzzle + dir * 1.2;
  trace.End = muzzle + dir * range;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = world.TraceMove(trace, SolidOnly);
  if (fraction >= 1 || !trace.TraceEnt || GenericTerrainEntity.Cast(trace.TraceEnt) || trace.TraceEnt.ClassName() == "RoadEntity") return range;
  hit = ACR_BaseDefences.ModelName(trace.TraceEnt);
  IEntity root = trace.TraceEnt.GetRootParent();
  s_bHitStructure = SCR_DestructibleBuildingEntity.Cast(root) || BaseBuilding.Cast(root) || hit.Contains("Fence") || hit.Contains("Wall");
  return 1.2 + fraction * (range - 1.2);
 }

 protected static void Doors(array<IEntity> buildings, notnull array<vector> doors)
 {
  foreach (IEntity b : buildings)
  {
   IEntity child = b.GetChildren();
   while (child)
   {
    string part = "";
    if (child.GetPrefabData()) part = child.GetPrefabData().GetPrefabName();
    if (part.Contains("/Doors/") && !part.Contains("_INT")) doors.Insert(child.GetOrigin());
    child = child.GetSibling();
   }
  }
 }

 protected static bool NearDoor(array<vector> doors, vector p, float within)
 {
  foreach (vector door : doors) if (vector.DistanceXZ(door, p) < within) return true;
  return false;
 }

 // The real nest's bags against what the planner cuts for it: the line's ends land within 1 m of its wings.
 protected static void NestWidth()
 {
  array<string> sides = {"US", "USSR", "FIA"};
  string words = "";
  bool fine = true;
  foreach (string side : sides)
  {
   Resource res = Resource.Load(ACR_FobDefences.PrefabFor(ACR_FobDefences.POST_COUNT, side, 1));
   if (!res.IsValid()) continue;
   EntitySpawnParams params = new EntitySpawnParams();
   params.TransformMode = ETransformMode.WORLD;
   params.Transform[3] = ACR_Base.s_All[0].GetOrigin() + Vector(0, 300, 0);
   IEntity e = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
   if (!e) continue;
   float left = 0;
   float right = 0;
   BagExtent(e, e, left, right);
   SCR_EntityHelper.DeleteEntityAndChildren(e);
   float cut = ACR_BaseLines.NestPieces(side) * ACR_BaseLines.LONG_STEP * 0.5;
   // (The planner centres its bags on the cut: ACR_BaseLines.NestShift.)
   if (Math.AbsFloat(cut - (right - left) * 0.5) > 1) fine = false;
   words += string.Format("%1: bags %2 .. %3, cut +-%4; ", side, left.ToString(-1, 1), right.ToString(-1, 1), cut.ToString(-1, 1));
  }
  ACR_TestRegress.Check("baseworks plan: the line's ends meet a nest's wings", fine, words);
 }

 protected static void BagExtent(IEntity root, IEntity parent, inout float left, inout float right)
 {
  IEntity child = parent.GetChildren();
  while (child)
  {
   if (ACR_BaseDefences.ModelName(child).Contains("Sandbag_01"))
   {
    vector mins, maxs;
    child.GetBounds(mins, maxs);
    vector side = root.VectorToLocal(child.VectorToParent(Vector(1, 0, 0)));
    vector at = root.CoordToLocal(child.GetOrigin());
    float halfX = (maxs[0] - mins[0]) * 0.5 * Math.AbsFloat(side[0]) + (maxs[2] - mins[2]) * 0.5 * Math.AbsFloat(side[2]);
    if (maxs[0] - mins[0] > 0.9)
    {
     left = Math.Min(left, at[0] - halfX);
     right = Math.Max(right, at[0] + halfX);
    }
   }
   BagExtent(root, child, left, right);
   child = child.GetSibling();
  }
 }

 // The kit's crates read as kit: no tall tower of them, in every side's kit.
 protected static void CrateChecks()
 {
  array<string> sides = {"US", "USSR", "FIA"};
  float tallest = 0;
  string name = "";
  foreach (string side : sides)
  {
   ACR_BaseDefenceKit kit = ACR_BaseDefences.Kit(side);
   foreach (ResourceName prefab : kit.m_aClutter)
   {
    float height = HeightOf(prefab);
    if (height <= tallest) continue;
    tallest = height;
    name = FilePath.StripPath(prefab);
   }
  }
  ACR_TestRegress.Check("baseworks plan: nothing in a kit's clutter stands over 2 m", tallest > 0 && tallest < 2, string.Format("tallest %1 m, %2", tallest.ToString(-1, 2), name));
  ResourceName cratesPrefab = ACR_BaseDefences.PostPrefab(ACR_BaseDefences.Kit("US"), "crates");
  float crates = HeightOf(cratesPrefab);
  vector half = ACR_BaseDefences.HalfSize(cratesPrefab);
  ACR_TestRegress.Check("baseworks plan: the command post's crates are knee to chest high", crates > 0.3 && crates < 1.4 && half[0] > 0.3, string.Format("%1 m, half %2", crates.ToString(-1, 2), half));
 }

 protected static float HeightOf(ResourceName prefab)
 {
  vector size = SizeOf(prefab);
  return size[1];
 }

 // A prefab's size with its children, spawned high up for a moment.
 protected static vector SizeOf(ResourceName prefab)
 {
  Resource res = Resource.Load(prefab);
  if (!res.IsValid()) return vector.Zero;
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = ACR_Base.s_All[0].GetOrigin() + Vector(0, 300, 0);
  IEntity e = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
  if (!e) return vector.Zero;
  vector mins, maxs;
  e.GetWorldBounds(mins, maxs);
  GrowBounds(e, mins, maxs);
  SCR_EntityHelper.DeleteEntityAndChildren(e);
  return maxs - mins;
 }

 protected static void GrowBounds(IEntity e, inout vector mins, inout vector maxs)
 {
  IEntity child = e.GetChildren();
  while (child)
  {
   vector a, b;
   child.GetWorldBounds(a, b);
   for (int i = 0; i < 3; i++)
   {
    if (a[i] < mins[i]) mins[i] = a[i];
    if (b[i] > maxs[i]) maxs[i] = b[i];
   }
   GrowBounds(child, mins, maxs);
   child = child.GetSibling();
  }
 }

 // The same seed plans a town the same; another seed differently.
 protected static void SeedChecks(ACR_Base base)
 {
  string first = PlanText(base);
  string again = PlanText(base);
  ACR_TestRegress.Check("baseworks plan: the same seed plans the same", first == again && !first.IsEmpty(), base.m_sName);
  string seedWas;
  ACR_Options.Get("acrSeed", seedWas);
  int differs = 0;
  array<string> seeds = {"11", "222", "3333"};
  foreach (string seed : seeds)
  {
   ACR_Options.Set("acrSeed", seed);
   if (PlanText(base) != first) differs++;
  }
  if (seedWas.IsEmpty()) ACR_Options.Unset("acrSeed");
  else ACR_Options.Set("acrSeed", seedWas);
  ACR_TestRegress.Check("baseworks plan: another seed plans otherwise", differs > 0, string.Format("%1 of %2 seeds differ at %3", differs, seeds.Count(), base.m_sName));
 }

 protected static string PlanText(ACR_Base base)
 {
  string faction = base.m_sOwner;
  if (faction.IsEmpty()) faction = s_sSide;
  ACR_BaseWorks.Reset();
  ACR_BaseWork work = ACR_BaseWorks.WorkFor(base, faction);
  string text = "";
  foreach (ACR_BaseSite site : work.m_aSites) text += string.Format("%1 %2 %3 %4 %5;", site.m_eKind, site.m_iTier, (int)site.m_vPos[0], (int)site.m_vPos[2], (int)site.m_fYaw);
  return text;
 }

 // The geometry of a town's plan, for a picture.
 protected static void Draw(ACR_Base base)
 {
  string faction = base.m_sOwner;
  if (faction.IsEmpty()) faction = s_sSide;
  ACR_BaseWorks.Reset();
  ACR_BaseWork work = ACR_BaseWorks.WorkFor(base, faction);
  vector c = base.GetOrigin();
  float radius = ACR_BaseDefences.SearchRadius(base);
  PrintFormat("[ACR] WORKSGEO TOWN %1 %2 %3 %4", base.m_sName, (int)c[0], (int)c[2], (int)radius);
  array<vector> dirs = {};
  array<float> weights = {};
  ACR_BaseDefences.Threats(base, faction, dirs, weights);
  foreach (int d, vector dir : dirs) PrintFormat("[ACR] WORKSGEO T %1 %2 %3", dir[0].ToString(-1, 2), dir[2].ToString(-1, 2), weights[d].ToString(-1, 2));
  array<IEntity> buildings = {};
  ACR_BaseDefences.Buildings(c, radius + 80, buildings);
  foreach (IEntity b : buildings)
  {
   vector mins, maxs;
   b.GetBounds(mins, maxs);
   vector p0 = b.CoordToParent(Vector(mins[0], 0, mins[2]));
   vector p1 = b.CoordToParent(Vector(maxs[0], 0, mins[2]));
   vector p2 = b.CoordToParent(Vector(maxs[0], 0, maxs[2]));
   vector p3 = b.CoordToParent(Vector(mins[0], 0, maxs[2]));
   PrintFormat("[ACR] WORKSGEO B %1 %2 %3 %4 %5 %6 %7 %8", p0[0].ToString(-1, 1), p0[2].ToString(-1, 1), p1[0].ToString(-1, 1), p1[2].ToString(-1, 1), p2[0].ToString(-1, 1), p2[2].ToString(-1, 1), p3[0].ToString(-1, 1), p3[2].ToString(-1, 1));
  }
  array<vector> doors = {};
  Doors(buildings, doors);
  foreach (vector door : doors) PrintFormat("[ACR] WORKSGEO D %1 %2", door[0].ToString(-1, 1), door[2].ToString(-1, 1));
  array<ref ACR_BaseDefenceSeg> segs = {};
  ACR_BaseDefences.Roads(c, radius + 80, segs);
  foreach (ACR_BaseDefenceSeg s : segs) PrintFormat("[ACR] WORKSGEO R %1 %2 %3 %4 %5", s.m_vA[0].ToString(-1, 1), s.m_vA[2].ToString(-1, 1), s.m_vB[0].ToString(-1, 1), s.m_vB[2].ToString(-1, 1), s.m_fWidth.ToString(-1, 1));
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(c, radius + 80, CollectWalls, null, EQueryEntitiesFlags.STATIC);
  foreach (IEntity wall : s_aFound)
  {
   vector wMins, wMaxs;
   wall.GetBounds(wMins, wMaxs);
   vector a = Vector(wMins[0], 0, 0.5 * (wMins[2] + wMaxs[2]));
   vector z = Vector(wMaxs[0], 0, 0.5 * (wMins[2] + wMaxs[2]));
   if (wMaxs[2] - wMins[2] > wMaxs[0] - wMins[0])
   {
    a = Vector(0.5 * (wMins[0] + wMaxs[0]), 0, wMins[2]);
    z = Vector(0.5 * (wMins[0] + wMaxs[0]), 0, wMaxs[2]);
   }
   a = wall.CoordToParent(a);
   z = wall.CoordToParent(z);
   PrintFormat("[ACR] WORKSGEO W %1 %2 %3 %4 %5", a[0].ToString(-1, 1), a[2].ToString(-1, 1), z[0].ToString(-1, 1), z[2].ToString(-1, 1), (int)(wMaxs[1] - wMins[1] + 0.5));
  }
  s_aFound.Clear();
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   string words = ACR_BaseWorks.SiteWords(site);
   words.Replace(" ", "_");
   PrintFormat("[ACR] WORKSGEO S %1 %2 %3 %4 %5 %6", words, site.m_iTier, site.m_vPos[0].ToString(-1, 1), site.m_vPos[2].ToString(-1, 1), (int)site.m_fYaw, site.m_iSpot);
   if (!site.m_Plan) continue;
   foreach (int i, vector at : site.m_Plan.m_aPiecePos)
    PrintFormat("[ACR] WORKSGEO P %1 %2 %3 %4 %5", at[0].ToString(-1, 1), at[2].ToString(-1, 1), (int)site.m_Plan.m_aPieceYaw[i], FilePath.StripExtension(FilePath.StripPath(site.m_Plan.m_aPiecePrefab[i])), site.m_Plan.m_aPieceWall[i]);
  }
  PrintFormat("[ACR] WORKSGEO END %1", base.m_sName);
 }

 protected static bool CollectWalls(IEntity e)
 {
  if (!e || !e.GetPhysics()) return true;
  string name = "";
  if (e.GetPrefabData()) name = e.GetPrefabData().GetPrefabName();
  if (name.IsEmpty() && e.GetVObject()) name = e.GetVObject().GetResourceName();
  if (name.Contains("/Walls/") || name.Contains("Fence")) s_aFound.Insert(e);
  return true;
 }

 // Kolguyev: the base of the 30 Oct playtest.
 protected static void Replay()
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && base.m_sName == "Bovemont Base") Draw(base);
 }
}
