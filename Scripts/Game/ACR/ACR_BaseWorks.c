// Base works (#105): Fortify on the middle of a base we hold fortifies the whole town, one tier per
// order, with FOB defences' tier names. A squad works it site by site, paying as it goes, so it can
// stop at any point and pick up where it left off: its own, or another squad's, next order on the
// base goes on with what is left, a lost site included.
//   Defences I:   sandbag lines across the worst approaches (ACR_BaseLines), a stretch at a time.
//   Defences II:  MG nests built into the lines, more line, wire in front, a checkpoint in a line's
//                 lane on the road from each of the two nearest enemy bases; defenders in reserve, who
//                 man the nests' guns and deploy when an enemy comes near (ACR_Defenders).
//   Defences III: bunkers and guard towers tied to the lines, a line behind the first if the town has
//                 the depth, the command post in a building; more defenders, who go into the buildings
//                 round the post when they deploy.
// Where they go starts from a survey of the base at the first order (ACR_BaseSurvey). Each choice and
// which site comes next are weighted among the near-best (ACR_Variety), so no two towns go up alike. The
// works stay when the base falls: they are the new owner's, who can go on from there. A site whose
// walls are all gone can be rebuilt.
// The AI commander (-acrFortifyAI, ACR_AICommander.ManageBaseWorks) uses it at a threatened front
// base once it has the RP banked, and counts it in the base's defence (Credit).

enum ACR_EBaseSite
{
 PLANNED,  // a stretch of line, its wire, or the command post
 NEST,
 BUNKER,   // a bunker or guard tower
 ROADBLOCK
}

class ACR_BaseSite : Managed
{
 ACR_EBaseSite m_eKind;
 int m_iTier;                       // 1-3
 vector m_vPos;
 float m_fYaw;
 float m_fScore;                    // threat and view
 ref ACR_BaseDefencePos m_Plan;     // PLANNED: its pieces
 ResourceName m_sPrefab;            // NEST, BUNKER
 ref array<IEntity> m_aWalls = {};  // what holds it up: gone when all of these are
 ref array<IEntity> m_aDressing = {};
 ACR_Roadblock m_Roadblock;
 bool m_bBuilt;
 bool m_bDamaged;                   // PLANNED: it stands with pieces gone: open again, for those alone
 bool m_bNoFit;                     // nothing could be placed: not tried again
 bool m_bSkipped;                   // a squad gave up on it (a truck parked there, no way in): the next order tries again
 int m_iSpot;                       // NEST, BUNKER: its spot in ACR_FobDefences.PrefabFor
 int m_iSector;                     // the sector of the survey it looks into
 int m_iLine = -1;                  // its line in the work, if any
 int m_iOrder;                      // a stretch: how far along its line
 ref array<vector> m_aCuts = {};    // NEST: the line's sandbag pieces that make way for it
 bool m_bAllCut;                    // a stretch all of whose pieces made way for a nest: it still counts
 float m_fReach;                    // NEST, BUNKER: clear m along its facing (the log, tests)
}

class ACR_BaseWork : Managed
{
 ACR_Base m_Base;
 string m_sFaction;
 int m_iTier;                       // the highest tier finished (0: none yet)
 ref array<ref ACR_BaseSite> m_aSites = {};
 ref array<ref ACR_BaseLine> m_aLines = {};
 ACR_DefenderPost m_Defenders;
 int m_iMannedTier;                 // the tier the reserve has been raised for (under this owner)
 int m_iPicks;                      // sites handed out, for the weighted pick's key
}

class ACR_BaseWorks
{
 static const float CLICK_RADIUS = 60;  // m from a base's centre: Fortify there means the whole base
 static const int MAX_NESTS = 3;
 static const int MAX_BUNKERS = 3;
 static const int MAX_ROADBLOCKS = 2;
 static const float SITE_RADIUS = 30;   // squad members this close to a site work
 static const float GUN_APART = 35;     // m between nests and bunkers
 static const float SAME_WAY = 60;      // m between guns looking the same way (within 25 degrees)
 static const float COVER_CHECKPOINT = 0.4; // how much a site counts in its sector (ACR_BaseSurvey.AddCover)
 static const float COVER_NEST = 0.6;
 static const float COVER_BUNKER = 0.5;
 static const float POST_AREA = 40;     // m round the command post its defenders garrison
 static const int REACH_GRACE_MS = 90000; // ms past a 1 m/s walk to a site before the squad gives up on it
 static const int AI_RP_RESERVE = 30;   // an AI commander keeps this much back (as ManageBaseWorks)
 static ref array<ref ACR_BaseWork> s_aWorks = {};
 static int s_iMaxSitesForTest;         // at most this many sites of each kind (0: no limit)
 static int s_iLastPlanMs;              // the last plan's cost (tests, log)
 static ref ACR_BaseSurvey s_LastSurvey; // the last plan's survey (tests)

 // -acrBaseWorks 0 turns it off; Fortify on a base is then refused, as before.
 static bool Enabled() { return ACR_Fortify.Enabled() && ACR_LogisticsRules.Choice("acrBaseWorks", "1") != "0"; }

 static int Defenders(int tier)
 {
  if (tier < 2) return 0;
  return ACR_Fortify.Defenders(tier) * 2;
 }

 static int Cost(ACR_BaseSite site)
 {
  switch (site.m_eKind)
  {
   case ACR_EBaseSite.NEST: return 10;
   case ACR_EBaseSite.BUNKER: return 15;
   case ACR_EBaseSite.ROADBLOCK: return ACR_Roadblocks.Cost(ACR_ERoadblockType.MEDIUM);
  }
  if (!site.m_Plan) return 5;
  switch (site.m_Plan.m_eKind)
  {
   case ACR_EBaseDefenceKind.LINE: return 8;
   case ACR_EBaseDefenceKind.WIRE: return 4;
   case ACR_EBaseDefenceKind.POST: return 20;
  }
  return 5;
 }

 // Seconds of work.
 static float Duration(ACR_BaseSite site)
 {
  switch (site.m_eKind)
  {
   case ACR_EBaseSite.NEST: return 45;
   case ACR_EBaseSite.BUNKER: return 60;
   case ACR_EBaseSite.ROADBLOCK: return 40;
  }
  if (!site.m_Plan) return 15;
  switch (site.m_Plan.m_eKind)
  {
   case ACR_EBaseDefenceKind.LINE: return 20;
   case ACR_EBaseDefenceKind.WIRE: return 15;
   case ACR_EBaseDefenceKind.POST: return 60;
  }
  return 15;
 }

 static string SiteWords(ACR_BaseSite site)
 {
  switch (site.m_eKind)
  {
   case ACR_EBaseSite.NEST: return "MG nest";
   case ACR_EBaseSite.BUNKER: return "bunker";
   case ACR_EBaseSite.ROADBLOCK: return "checkpoint";
  }
  if (!site.m_Plan) return "sandbags";
  switch (site.m_Plan.m_eKind)
  {
   case ACR_EBaseDefenceKind.LINE: return "sandbag line";
   case ACR_EBaseDefenceKind.WIRE: return "wire";
   case ACR_EBaseDefenceKind.POST: return "command post";
  }
  return "sandbags";
 }

 // ---------------------------------------------------------------- the order

 // The base of ours a Fortify click at 'point' means, if any (a FOB's is refused in Order).
 static ACR_Base BaseAt(vector point, string faction)
 {
  if (!Enabled()) return null;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_bHidden || base.m_sOwner != faction) continue;
   if (vector.DistanceXZ(point, base.GetOrigin()) < CLICK_RADIUS) return base;
  }
  return null;
 }

 // The server side of the order: the next site of the tier that is open. A new order replaces the
 // squad's current job, like any other. false + refusal, nothing charged, when it can't.
 static bool Order(ACR_Squad squad, ACR_Base base, out string refusal)
 {
  refusal = "";
  if (!Enabled())
  {
   refusal = "fortifying a base is turned off for this match";
   return false;
  }
  if (!ACR_Fortify.CanFortify(squad) || !squad.GetFaction())
  {
   refusal = "only infantry can fortify";
   return false;
  }
  string faction = squad.GetFaction().GetFactionKey();
  if (!base || base.m_bRetired || base.m_sOwner != faction)
  {
   refusal = "that base isn't ours";
   return false;
  }
  if (base.m_bBuilt)
  {
   refusal = "a FOB builds its defences from its own panel";
   return false;
  }
  ACR_BaseWork work = WorkFor(base, faction);
  foreach (ACR_BaseSite skipped : work.m_aSites) skipped.m_bSkipped = false;
  int tier = OpenTier(work);
  if (tier == 0)
  {
   if (work.m_aSites.IsEmpty()) refusal = "no place to fortify here";
   else refusal = "this base is fortified as far as it goes";
   return false;
  }
  ACR_FortifyPlan plan = NextPlan(work, squad, tier, refusal);
  if (!plan) return false;
  if (!squad.Fortify(plan))
  {
   refusal = "unable to start the work";
   return false;
  }
  PrintFormat("[ACR] BASEWORKS %1 %2: %3 starts %4 (%5 sites open)", faction, base.m_sName, squad.GetName(), ACR_FobDefences.TierName(tier - 1), OpenSites(work, tier));
  return true;
 }

 static string Accepted(ACR_Base base)
 {
  ACR_BaseWork work = Find(base);
  if (!work) return "fortifying the base";
  return string.Format("fortifying %1: %2", base.m_sName, ACR_FobDefences.TierName(OpenTier(work) - 1));
 }

 static ACR_BaseWork Find(ACR_Base base)
 {
  foreach (ACR_BaseWork work : s_aWorks) if (work.m_Base == base) return work;
  return null;
 }

 static int TierOf(ACR_Base base)
 {
  ACR_BaseWork work = Find(base);
  if (!work) return 0;
  return work.m_iTier;
 }

 static ACR_BaseWork WorkFor(ACR_Base base, string faction)
 {
  ACR_BaseWork work = Find(base);
  if (work)
  {
   if (work.m_sFaction != faction) Transfer(work, faction);
   return work;
  }
  work = new ACR_BaseWork();
  work.m_Base = base;
  work.m_sFaction = faction;
  int started = System.GetTickCount();
  PlanSites(work);
  s_iLastPlanMs = System.GetTickCount() - started;
  PrintFormat("[ACR] BASEWORKS %1 %2: planned %3 sites in %4 ms", faction, base.m_sName, work.m_aSites.Count(), s_iLastPlanMs);
  s_aWorks.Insert(work);
  return work;
 }

 // The lowest tier with a site still to build: a lost site comes before the next tier.
 static int OpenTier(ACR_BaseWork work)
 {
  for (int tier = 1; tier <= ACR_Fortify.MAX_TIER; tier++)
   if (OpenSites(work, tier) > 0) return tier;
  return 0;
 }

 static int OpenSites(ACR_BaseWork work, int tier)
 {
  int open;
  foreach (ACR_BaseSite site : work.m_aSites)
   if (site.m_iTier == tier && (!site.m_bBuilt || site.m_bDamaged) && !site.m_bNoFit && !site.m_bSkipped) open++;
  return open;
 }

 static int BuiltSites(ACR_BaseWork work, int tier = 0)
 {
  int built;
  foreach (ACR_BaseSite site : work.m_aSites)
   if (site.m_bBuilt && (tier == 0 || site.m_iTier == tier)) built++;
  return built;
 }

 // Another squad is at work on it.
 protected static bool Claimed(ACR_BaseSite site, ACR_Squad except)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad != except && squad.m_FortifyPlan && squad.m_FortifyPlan.m_Site == site) return true;
  return false;
 }

 // The squad's next site of this tier, near it and facing the threat.
 protected static ACR_FortifyPlan NextPlan(ACR_BaseWork work, ACR_Squad squad, int tier, out string refusal)
 {
  refusal = "";
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !squad.GetAnchor()) return null;
  vector from = squad.GetAnchor().GetOrigin();
  array<ACR_BaseSite> open = {};
  array<float> scores = {};
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (site.m_iTier != tier || (site.m_bBuilt && !site.m_bDamaged) || site.m_bNoFit || site.m_bSkipped || Claimed(site, squad)) continue;
   open.Insert(site);
   float score = site.m_fScore - vector.DistanceXZ(from, site.m_vPos) / 200;
   // A checkpoint is out of the way: everything else in the town first, then the walk.
   if (site.m_eKind == ACR_EBaseSite.ROADBLOCK) score -= 5;
   // A line grows from what's up: the next stretch along it before anywhere else.
   if (NextToBuilt(work, site)) score += 0.6;
   scores.Insert(score);
  }
  if (open.IsEmpty())
  {
   refusal = "another squad is on what is left to build";
   return null;
  }
  work.m_iPicks++;
  int picked = ACR_Variety.Pick(scores, string.Format("base works %1 %2 %3", work.m_Base.m_sName, tier, work.m_iPicks));
  ACR_BaseSite site = open[picked];
  ACR_FortifyPlan plan = new ACR_FortifyPlan();
  plan.m_sFaction = work.m_sFaction;
  plan.m_vSite = site.m_vPos;
  plan.m_fYaw = site.m_fYaw;
  plan.m_iTier = tier;
  plan.m_Work = work;
  plan.m_Site = site;
  plan.m_iCost = Cost(site);
  float share = 1;
  if (site.m_bDamaged)
  {
   share = MissingShare(work, site);
   plan.m_iCost = Math.Max(1, Math.Round(plan.m_iCost * share));
  }
  if (manager.m_Bases.GetRP(work.m_sFaction) + squad.PendingRP() < plan.m_iCost)
  {
   refusal = string.Format("not enough RP (%1 needed)", plan.m_iCost);
   return null;
  }
  // From the base side of the site.
  vector back = work.m_Base.GetOrigin() - site.m_vPos;
  back[1] = 0;
  float length = back.Length();
  if (length > 1) back = back * (Math.Min(8, length) / length);
  plan.m_vStage = ACR_Manager.Ground(site.m_vPos + back);
  if (BBB_Terrain.IsWaterAt(plan.m_vStage)) plan.m_vStage = site.m_vPos;
  // Somewhere a man can stand: a stretch can run beside a fence or a house.
  vector walkable;
  if (squad.ReachableNear(plan.m_vStage, 12, walkable)) plan.m_vStage = walkable;
  plan.m_fRadius = SITE_RADIUS;
  plan.m_fDuration = Math.Max(5, Duration(site) * share) * ACR_Fortify.WORK_SCALE;
  float now = GetGame().GetWorld().GetWorldTime();
  plan.m_fLastTick = now;
  plan.m_fLastProgress = now;
  // A site it can't get to (no way into a yard) shouldn't hold up the rest of the town for long.
  plan.m_fDeadline = now + vector.DistanceXZ(from, plan.m_vStage) / ACR_Minefields.TRAVEL_SPEED * 1000 + REACH_GRACE_MS;
  return plan;
 }

 protected static bool NextToBuilt(ACR_BaseWork work, ACR_BaseSite site)
 {
  if (site.m_iLine < 0 || !site.m_Plan || site.m_Plan.m_eKind != ACR_EBaseDefenceKind.LINE) return false;
  foreach (ACR_BaseSite other : work.m_aSites)
  {
   if (other.m_bBuilt && other.m_iLine == site.m_iLine && other.m_Plan && other.m_Plan.m_eKind == ACR_EBaseDefenceKind.LINE && Math.AbsInt(other.m_iOrder - site.m_iOrder) == 1) return true;
  }
  return false;
 }

 // "Bay Town: sandbag line"
 static string Words(ACR_FortifyPlan plan)
 {
  return string.Format("%1: %2", plan.m_Work.m_Base.m_sName, SiteWords(plan.m_Site));
 }

 static bool VehicleOnSite(ACR_FortifyPlan plan)
 {
  // A stretch is long: every piece of it.
  if (plan.m_Site.m_Plan && plan.m_Site.m_Plan.m_eKind == ACR_EBaseDefenceKind.LINE)
  {
   foreach (vector at : plan.m_Site.m_Plan.m_aPiecePos) if (ACR_FobSlots.VehicleOnSpot(at, 3)) return true;
   return false;
  }
  return ACR_FobSlots.VehicleOnSpot(plan.m_vSite, 8);
 }

 // ---------------------------------------------------------------- the squad's work

 // A site's work is done (ACR_Squad.CompleteFortify): its pieces go in, and the squad goes on to
 // the next of the tier, or holds the base when there is none it can do.
 static void SiteDone(ACR_Squad squad, ACR_FortifyPlan plan)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_BaseWork work = plan.m_Work;
  ACR_BaseSite site = plan.m_Site;
  int orderedBy = squad.m_iOrderedBy;
  if (!s_aWorks.Contains(work) || work.m_sFaction != plan.m_sFaction || !Build(work, site))
  {
   if (manager) manager.m_Bases.Refund(plan.m_sFaction, plan.m_iCost);
   site.m_bNoFit = s_aWorks.Contains(work) && work.m_sFaction == plan.m_sFaction;
   PrintFormat("[ACR] BASEWORKS %1: no %2 at %3, refunded %4 RP", plan.m_sFaction, SiteWords(site), site.m_vPos, plan.m_iCost);
  }
  if (!s_aWorks.Contains(work) || work.m_sFaction != plan.m_sFaction)
  {
   squad.HoldBaseWorks(work);
   return;
  }
  GoOn(squad, plan, orderedBy);
 }

 // The squad gave up on a site (ACR_Squad.TickFortify): refunded, the rest of the tier goes on, and
 // the next order on the base tries it again. One parked truck doesn't cost the tier.
 static void SiteSkipped(ACR_Squad squad, ACR_FortifyPlan plan)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) manager.m_Bases.Refund(plan.m_sFaction, plan.m_iCost);
  plan.m_Site.m_bSkipped = true;
  PrintFormat("[ACR] BASEWORKS %1: %2 gave up on the %3 at %4, refunded %5 RP", plan.m_sFaction, squad.GetName(), SiteWords(plan.m_Site), plan.m_vSite, plan.m_iCost);
  // Where it got to, for a site it never reached: stuck, or just far.
  IEntity anchor = squad.GetAnchor();
  if (anchor) PrintFormat("[ACR] BASEWORKS %1: arrived %2, stage %3, the squad at %4, %5 m off (%6 men, %7)", plan.m_sFaction, plan.m_bArrived, plan.m_vStage, anchor.GetOrigin(), (int)vector.DistanceXZ(anchor.GetOrigin(), plan.m_vSite), squad.CountAlive(), squad.ThinkMovement());
  if (!s_aWorks.Contains(plan.m_Work) || plan.m_Work.m_sFaction != plan.m_sFaction)
  {
   squad.HoldBaseWorks(plan.m_Work);
   return;
  }
  GoOn(squad, plan, squad.m_iOrderedBy);
 }

 protected static void GoOn(ACR_Squad squad, ACR_FortifyPlan plan, int orderedBy)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_BaseWork work = plan.m_Work;
  int tier = plan.m_iTier;
  if (OpenSites(work, tier) == 0 && !WorkedOn(work, tier, squad)) TierDone(work, tier);
  string why;
  ACR_FortifyPlan next;
  if (OpenSites(work, tier) > 0) next = NextPlan(work, squad, tier, why);
  // An AI commander's squad goes on only as far as its build budget does, as its first site did
  // (ACR_AICommander.ManageBaseWorks); it can pick the work up again later.
  ACR_AICommander ai;
  if (manager && squad.m_iAITask == ACR_EAITask.FORTIFY) ai = manager.AICommanderFor(plan.m_sFaction);
  if (next && ai && !AIMayPay(ai, next.m_iCost))
  {
   next = null;
   why = "";
  }
  if (next && squad.Fortify(next))
  {
   squad.m_iOrderedBy = orderedBy;
   if (ai) ai.SpendBuildBudget(next.m_iCost);
   return;
  }
  if (!why.IsEmpty() && OpenSites(work, tier) > 0 && manager)
  {
   string paused = string.Format("%1 stopped work on %2: %3", squad.GetName(), work.m_Base.m_sName, why);
   manager.Command().Feed(plan.m_sFaction, paused);
   if (orderedBy != 0) manager.Notify(orderedBy, paused);
  }
  squad.HoldBaseWorks(work);
 }

 static bool AIMayPay(ACR_AICommander ai, int cost)
 {
  return ai.CanBuild(cost, AI_RP_RESERVE);
 }

 // A squad other than 'except' has a site of this tier in hand.
 protected static bool WorkedOn(ACR_BaseWork work, int tier, ACR_Squad except)
 {
  foreach (ACR_BaseSite site : work.m_aSites)
   if (site.m_iTier == tier && Claimed(site, except)) return true;
  return false;
 }

 protected static void TierDone(ACR_BaseWork work, int tier)
 {
  int built = BuiltSites(work, tier);
  if (built == 0) return;
  work.m_iTier = Math.Max(work.m_iTier, tier);
  RaiseDefenders(work);
  ManWorks(work);
  string done = string.Format("%1 fortified: %2, %3 sites", work.m_Base.m_sName, ACR_FobDefences.TierName(tier - 1), built);
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) manager.Command().Feed(work.m_sFaction, done);
  ACR_Report.Event(work.m_sFaction, done);
  PrintFormat("[ACR] BASEWORKS %1 %2", work.m_sFaction, done);
 }

 protected static void RaiseDefenders(ACR_BaseWork work)
 {
  int more = Defenders(work.m_iTier) - Defenders(work.m_iMannedTier);
  if (more <= 0) return;
  work.m_iMannedTier = work.m_iTier;
  if (!work.m_Defenders) work.m_Defenders = ACR_Defenders.Add(work.m_sFaction, work.m_Base.GetOrigin(), work.m_Base.m_sName + " works", ACR_BaseDefences.SearchRadius(work.m_Base));
  work.m_Defenders.m_Work = work;
  work.m_Defenders.m_iReserve += more;
 }

 // The reserve on the nests' guns, and based at the command post once there is one.
 protected static void ManWorks(ACR_BaseWork work)
 {
  ACR_DefenderPost post = work.m_Defenders;
  if (!post) return;
  // A town with no command post site: the buildings round its middle.
  if (work.m_iTier >= 3)
  {
   post.m_fRadius = POST_AREA;
   post.m_fGarrisonArea = POST_AREA;
  }
  array<IEntity> guns = {};
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (!site.m_bBuilt) continue;
   if (work.m_iTier >= 3 && site.m_Plan && site.m_Plan.m_eKind == ACR_EBaseDefenceKind.POST) post.m_vPos = site.m_vPos;
   if (site.m_eKind != ACR_EBaseSite.NEST) continue;
   foreach (IEntity piece : site.m_aWalls)
   {
    IEntity gun = ACR_Roadblocks.FindTurretInHierarchy(piece);
    if (gun) guns.Insert(gun);
   }
  }
  if (!guns.IsEmpty()) ACR_Defenders.CrewGuns(post, guns);
 }

 protected static bool Build(ACR_BaseWork work, ACR_BaseSite site)
 {
  switch (site.m_eKind)
  {
   case ACR_EBaseSite.ROADBLOCK:
    site.m_Roadblock = ACR_Roadblocks.SpawnRoadblock(work.m_sFaction, site.m_vPos, site.m_fYaw, work.m_Base.m_sName + " works");
    if (!site.m_Roadblock) return false;
    site.m_aWalls.Insert(site.m_Roadblock.m_Entity);
    GateWay(work, site);
    break;
   case ACR_EBaseSite.PLANNED:
    BuildPlanned(work, site);
    if (site.m_aWalls.IsEmpty())
    {
     foreach (IEntity prop : site.m_aDressing) if (prop) SCR_EntityHelper.DeleteEntityAndChildren(prop);
     site.m_aDressing.Clear();
     return false;
    }
    break;
   default:
    ACR_FobBuild.FellTrees(site.m_vPos, ACR_FobDefences.PIECE_RADIUS);
    IEntity piece = ACR_Fortify.SpawnPiece(site.m_sPrefab, site.m_vPos, site.m_fYaw, work.m_sFaction);
    if (!piece) return false;
    site.m_aWalls.Insert(piece);
    MakeWay(work, site);
  }
  site.m_bBuilt = true;
  site.m_bDamaged = false;
  PrintFormat("[ACR] BASEWORKS %1 %2: %3 at %4 (%5 pieces)", work.m_sFaction, work.m_Base.m_sName, SiteWords(site), site.m_vPos, site.m_aWalls.Count() + site.m_aDressing.Count());
  return true;
 }

 // A nest built into a line: the sandbags and dressing where it goes come out.
 protected static void MakeWay(ACR_BaseWork work, ACR_BaseSite nest)
 {
  if (nest.m_aCuts.IsEmpty()) return;
  foreach (ACR_BaseSite stretch : work.m_aSites)
  {
   if (!stretch.m_Plan || stretch.m_Plan.m_eKind != ACR_EBaseDefenceKind.LINE) continue;
   bool had = !stretch.m_aWalls.IsEmpty();
   for (int i = stretch.m_aWalls.Count() - 1; i >= 0; i--)
   {
    IEntity piece = stretch.m_aWalls[i];
    if (!piece) continue;
    foreach (vector cut : nest.m_aCuts)
    {
     if (vector.DistanceXZ(piece.GetOrigin(), cut) > 0.6) continue;
     stretch.m_aWalls.Remove(i);
     ACR_Navmesh.DeleteStructure(piece);
     break;
    }
   }
   if (had && stretch.m_aWalls.IsEmpty()) stretch.m_bAllCut = true;
   for (int d = stretch.m_aDressing.Count() - 1; d >= 0; d--)
   {
    IEntity prop = stretch.m_aDressing[d];
    if (!prop || vector.DistanceXZ(prop.GetOrigin(), nest.m_vPos) > 7) continue;
    stretch.m_aDressing.Remove(d);
    SCR_EntityHelper.DeleteEntityAndChildren(prop);
   }
  }
 }

 // A checkpoint just built in a line's lane: its real pieces, as the lane left them, say how wide it
 // stands; the line's bags inside that make way, so the two meet at the road as one gate.
 protected static void GateWay(ACR_BaseWork work, ACR_BaseSite site)
 {
  IEntity root = site.m_Roadblock.m_Entity;
  if (!root) return;
  float left = 0;
  float right = 0;
  float back = 0;
  float front = 0;
  GateExtent(root, root, left, right, back, front);
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   foreach (ACR_LinePiece piece : line.m_aPieces)
   {
    vector local = root.CoordToLocal(piece.m_vPos);
    if (local[0] < left - 0.4 || local[0] > right + 0.4 || local[2] < back - 0.4 || local[2] > front + 0.4) continue;
    site.m_aCuts.Insert(piece.m_vPos);
   }
  }
  PrintFormat("[ACR] BASEWORKS checkpoint at %1 stands x %2 .. %3, z %4 .. %5: %6 line pieces make way", site.m_vPos, left.ToString(-1, 1), right.ToString(-1, 1), back.ToString(-1, 1), front.ToString(-1, 1), site.m_aCuts.Count());
  MakeWay(work, site);
 }

 protected static void GateExtent(IEntity root, IEntity parent, inout float left, inout float right, inout float back, inout float front)
 {
  IEntity child = parent.GetChildren();
  while (child)
  {
   // Solid things, not bushes, decals or the odd box.
   vector mins, maxs;
   child.GetBounds(mins, maxs);
   if (child.GetPhysics() && !Tree.Cast(child) && maxs[0] - mins[0] > 0.9)
   {
    vector at = root.CoordToLocal(child.GetOrigin());
    left = Math.Min(left, at[0]);
    right = Math.Max(right, at[0]);
    back = Math.Min(back, at[2]);
    front = Math.Max(front, at[2]);
   }
   GateExtent(root, child, left, right, back, front);
   child = child.GetSibling();
  }
 }

 protected static bool CutForNest(ACR_BaseWork work, vector at)
 {
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (!site.m_bBuilt || site.m_aCuts.IsEmpty()) continue;
   foreach (vector cut : site.m_aCuts) if (vector.DistanceXZ(cut, at) < 0.6) return true;
  }
  return false;
 }

 // The site's pieces. Only sandbags and such ask for a navmesh rebuild; a chair doesn't. A site with no
 // sandbags (wire) is held up by whatever it has. A line's pieces lie on the ground; on a rebuild,
 // those that made way for a nest stay out.
 protected static void BuildPlanned(ACR_BaseWork work, ACR_BaseSite site)
 {
  ACR_BaseDefencePos pos = site.m_Plan;
  bool anyWall = pos.Walls() > 0;
  bool line = pos.m_eKind == ACR_EBaseDefenceKind.LINE;
  foreach (int i, vector at : pos.m_aPiecePos)
  {
   // A repair: what was shot away, no more.
   if (site.m_bDamaged && ((anyWall && !pos.m_aPieceWall[i]) || StandsAt(site, at))) continue;
   IEntity piece;
   if (line && pos.m_aPieceWall[i])
   {
    if (CutForNest(work, at)) continue;
    piece = ACR_BaseDefences.SpawnOnGround(pos.m_aPiecePrefab[i], at, pos.m_aPieceYaw[i]);
   }
   else piece = ACR_BaseDefences.SpawnProp(pos.m_aPiecePrefab[i], at, pos.m_aPieceYaw[i]);
   if (!piece) continue;
   if (pos.m_aPieceWall[i] || !anyWall)
   {
    site.m_aWalls.Insert(piece);
    ACR_Navmesh.RebuildAfterSpawn(piece);
   }
   else site.m_aDressing.Insert(piece);
  }
 }

 protected static bool StandsAt(ACR_BaseSite site, vector at)
 {
  foreach (IEntity wall : site.m_aWalls)
   if (wall && vector.DistanceXZ(wall.GetOrigin(), at) < 0.6) return true;
  return false;
 }

 // Of what should stand on this site, the share that doesn't.
 static float MissingShare(ACR_BaseWork work, ACR_BaseSite site)
 {
  ACR_BaseDefencePos pos = site.m_Plan;
  if (!pos) return 1;
  bool anyWall = pos.Walls() > 0;
  int should, missing;
  foreach (int i, vector at : pos.m_aPiecePos)
  {
   if ((anyWall && !pos.m_aPieceWall[i]) || CutForNest(work, at)) continue;
   should++;
   if (!StandsAt(site, at)) missing++;
  }
  if (should == 0) return 1;
  return missing / (float)should;
 }

 // A nest's or a checkpoint's gun of this side's works. A squad's man on one is still infantry: a stretch's
 // repair was refused as 'only infantry can fortify' with the last man of the squad on a nest's gun.
 static bool OwnGun(string faction, IEntity gun)
 {
  foreach (ACR_BaseWork work : s_aWorks)
  {
   if (work.m_sFaction != faction) continue;
   foreach (ACR_BaseSite site : work.m_aSites)
   {
    if (site.m_eKind != ACR_EBaseSite.NEST && site.m_eKind != ACR_EBaseSite.ROADBLOCK) continue;
    foreach (IEntity piece : site.m_aWalls)
     if (piece && ACR_Roadblocks.FindTurretInHierarchy(piece) == gun) return true;
   }
  }
  return false;
 }

 // Something built here has been lost, whole or in part: a tier already up wants it back.
 static bool NeedsRepair(ACR_Base base)
 {
  ACR_BaseWork work = Find(base);
  if (!work) return false;
  for (int tier = 1; tier <= work.m_iTier; tier++)
   if (OpenSites(work, tier) > 0) return true;
  return false;
 }

 // ---------------------------------------------------------------- the men behind the bags

 static bool s_bNoLineMenForTest; // the reserve as it was before: at the post, or in its buildings
 static const float MAN_BEHIND = 1.0;       // m a man stands behind a piece's middle
 static const float REMAN_MS = 60000;       // how often the line in use is looked at again
 static const float FACES_ENOUGH = 0.2;     // of a line's facing towards the enemy, to man it
 static const float STAY_CLOSE = 200;       // m from the line: they stay where they are
 static const float THREAT_RANGE = 900;     // m: a man further off doesn't turn the defence round

 // The nearest enemy man's side of the base: false with none in range.
 protected static bool ThreatAt(ACR_DefenderPost post, out vector at)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return false;
  float best = THREAT_RANGE;
  bool found = false;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction() || ACR_Insurgency.Friendly(squad.GetFaction().GetFactionKey(), post.m_sFaction)) continue;
   IEntity anchor = squad.GetAnchor();
   if (!anchor) continue;
   float d = vector.DistanceXZ(anchor.GetOrigin(), post.m_vPos);
   if (d >= best) continue;
   best = d;
   at = anchor.GetOrigin();
   found = true;
  }
  return found;
 }

 // Pieces of this line a man can stand behind: long, up, not made way for.
 protected static int MannablePieces(ACR_BaseWork work, ACR_BaseLine line, notnull array<ACR_LinePiece> result)
 {
  result.Clear();
  foreach (ACR_LinePiece piece : line.m_aPieces)
  {
   if (!piece.m_bLong || CutForNest(work, piece.m_vPos)) continue;
   foreach (ACR_BaseSite site : work.m_aSites)
   {
    if (site.m_iLine != work.m_aLines.Find(line) || !site.m_bBuilt || !StandsAt(site, piece.m_vPos)) continue;
    result.Insert(piece);
    break;
   }
  }
  return result.Count();
 }

 // The built lines a man could go to for an enemy at 'threat', the best first: those that face him, near
 // him; the first is picked among the near-best. None at all with a threat only behind them.
 protected static void LinesFor(ACR_BaseWork work, vector threat, bool hasThreat, notnull array<int> result)
 {
  result.Clear();
  array<float> scores = {};
  array<int> lines = {};
  array<ACR_LinePiece> pieces = {};
  foreach (int l, ACR_BaseLine line : work.m_aLines)
  {
   if (MannablePieces(work, line, pieces) < 2) continue;
   float score = 1 - l * 0.05; // the worst approach first
   if (hasThreat)
   {
    float faces = ACR_BaseDefences.DotXZ(line.m_vFacing, ACR_BaseDefences.FlatDir(line.m_vMiddle, threat));
    if (faces < FACES_ENOUGH) continue;
    score = faces * 2 - vector.DistanceXZ(line.m_vMiddle, threat) / 400;
   }
   scores.Insert(score);
   lines.Insert(l);
  }
  if (lines.IsEmpty()) return;
  work.m_iPicks++;
  int first = ACR_Variety.Pick(scores, string.Format("base man %1 %2", work.m_Base.m_sName, work.m_iPicks));
  result.Insert(lines[first]);
  lines.Remove(first);
  scores.Remove(first);
  while (!lines.IsEmpty())
  {
   int best = 0;
   foreach (int s, float value : scores) if (value > scores[best]) best = s;
   result.Insert(lines[best]);
   lines.Remove(best);
   scores.Remove(best);
  }
 }

 // The reserve out: each man to his own place behind the bags of the lines that face the enemy, nearest him
 // first, standing to fire over them. Before, they stood in the town's middle or in its buildings and
 // the works did nothing for them (pn23 playtest). False: no line for it, the caller does what it did.
 static bool ManLine(ACR_DefenderPost post, int men)
 {
  ACR_BaseWork work = post.m_Work;
  if (!work || !post.m_Group || s_bNoLineMenForTest || men <= 0) return false;
  ACR_Squad.FightLikeSquad(post.m_Group);
  vector threat;
  bool hasThreat = ThreatAt(post, threat);
  array<int> lines = {};
  LinesFor(work, threat, hasThreat, lines);
  if (lines.IsEmpty()) return false;
  array<ref ACR_GarrisonSpot> spots = {};
  array<ACR_LinePiece> pieces = {};
  foreach (int index : lines)
  {
   MannablePieces(work, work.m_aLines[index], pieces);
   vector aim = work.m_aLines[index].m_vMiddle;
   if (hasThreat) aim = threat;
   while (spots.Count() < men && !pieces.IsEmpty())
   {
    int nearest = 0;
    foreach (int p, ACR_LinePiece candidate : pieces)
     if (vector.DistanceXZ(candidate.m_vPos, aim) < vector.DistanceXZ(pieces[nearest].m_vPos, aim)) nearest = p;
    ACR_LinePiece piece = pieces[nearest];
    pieces.Remove(nearest);
    ACR_GarrisonSpot spot = new ACR_GarrisonSpot();
    spot.m_Pos = piece.m_vPos - piece.m_vFacing * MAN_BEHIND;
    spot.m_Pos[1] = ACR_BaseLiner.Ground(spot.m_Pos);
    spot.m_Look = piece.m_vFacing;
    spot.m_bStand = true;
    spots.Insert(spot);
   }
   if (spots.Count() >= men) break;
  }
  if (spots.IsEmpty()) return false;
  vector center = spots[0].m_Pos;
  float radius;
  if (ACR_Garrison.PostsAt(post.m_Group, spots, center, radius) <= 0) return false;
  ACR_Guerrillas.Order(post.m_Group, ACR_Squad.WP_GARRISON, center, radius, true);
  post.m_iMannedLine = lines[0];
  post.m_fMannedAt = GetGame().GetWorld().GetWorldTime();
  PrintFormat("[ACR] BASEWORKS %1 %2: %3 men to line %4 and on (%5 places)", work.m_sFaction, work.m_Base.m_sName, men, lines[0], spots.Count());
  return true;
 }

 // While they're out: the enemy has gone round to a side the line in use doesn't face. Not with him close:
 // up and running across the town under fire is worse than the wrong line.
 static void KeepManned(ACR_DefenderPost post)
 {
  if (!post.m_Work || post.m_iMannedLine < 0 || post.m_iMannedLine >= post.m_Work.m_aLines.Count()) return;
  ACR_Squad.FightLikeSquad(post.m_Group);
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - post.m_fMannedAt < REMAN_MS) return;
  post.m_fMannedAt = now;
  vector threat;
  if (!ThreatAt(post, threat)) return;
  ACR_BaseLine line = post.m_Work.m_aLines[post.m_iMannedLine];
  if (vector.DistanceXZ(line.m_vMiddle, threat) < STAY_CLOSE) return;
  if (ACR_BaseDefences.DotXZ(line.m_vFacing, ACR_BaseDefences.FlatDir(line.m_vMiddle, threat)) > -FACES_ENOUGH) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager) ManLine(post, manager.m_Bases.GuardsAlive(post.m_Group));
 }

 // Where a squad holds the base when a tier ends: behind the worst approach's line, not in the town's
 // middle. False: no line up.
 static bool HoldSpot(ACR_BaseWork work, out vector at, out float radius)
 {
  array<ACR_LinePiece> pieces = {};
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   if (MannablePieces(work, line, pieces) < 2) continue;
   at = ACR_Manager.Ground(line.m_vMiddle - line.m_vFacing * 4);
   radius = Math.Clamp(line.m_fLength * 0.6, 20, 45);
   return true;
  }
  return false;
 }

 // Tier I up at once and free, nobody walking (Quick start's front towns). The sites built.
 static int BuildNow(ACR_Base base, string faction)
 {
  if (!Enabled() || !base || base.m_bBuilt || base.m_sOwner != faction) return 0;
  ACR_BaseWork work = WorkFor(base, faction);
  int built;
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (site.m_iTier != 1 || site.m_bBuilt || site.m_bNoFit) continue;
   if (Build(work, site)) built++;
   else site.m_bNoFit = true;
  }
  if (built > 0) work.m_iTier = Math.Max(work.m_iTier, 1);
  return built;
 }

 // Tests: every tier up at once, for a look at it (ACR_TestWorksShots).
 static int BuildAllForTest(ACR_Base base, string faction)
 {
  ACR_BaseWork work = WorkFor(base, faction);
  int built;
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (site.m_bBuilt || site.m_bNoFit) continue;
   if (Build(work, site)) built++;
   else site.m_bNoFit = true;
  }
  return built;
 }

 static void TiersDoneForTest(ACR_BaseWork work)
 {
  for (int tier = 1; tier <= ACR_Fortify.MAX_TIER; tier++) TierDone(work, tier);
 }

 // ---------------------------------------------------------------- where the sites go

 protected static void PlanSites(ACR_BaseWork work)
 {
  ACR_Base base = work.m_Base;
  ACR_BaseSurvey survey = ACR_BaseSurvey.Take(base, work.m_sFaction);
  s_LastSurvey = survey;
  PrintFormat("[ACR] BASEWORKS survey %1 (%2 ms)", survey.Words(), survey.m_iMs);
  ACR_BaseLines.Plan(work, survey);
  // Nests first: gaps for men on foot keep clear of them, and stretches are cut after the gaps.
  ACR_BaseLines.Nests(work, survey, MAX_NESTS);
  ACR_BaseLines.Stretches(work, survey);
  ACR_BaseLines.Wire(work, survey);
  ACR_BaseLines.Checkpoints(work, survey, MAX_ROADBLOCKS);
  ACR_BaseLines.Posts(work, survey, MAX_BUNKERS);
  ACR_BaseLines.CommandPost(work, survey);
  string refused = survey.RefusedWords();
  if (!refused.IsEmpty()) PrintFormat("[ACR] BASEWORKS %1: command post pieces left out: %2", base.m_sName, refused);
  float seconds = 0;
  int pieces = 0;
  // Compositions by their measured part counts: a nest about 30, a bunker 45, a tower 20, a checkpoint 50.
  int entities = 0;
  int ends = 0;
  int anchored = 0;
  foreach (ACR_BaseLine line : work.m_aLines)
  {
   ends += 2;
   anchored += line.Anchors();
  }
  foreach (ACR_BaseSite made : work.m_aSites)
  {
   PrintFormat("[ACR] BASEWORKS site %1 %2, tier %3, at %4 %5 facing %6, line %7, its sector now %8", base.m_sName, SiteWords(made), made.m_iTier, (int)made.m_vPos[0], (int)made.m_vPos[2], (int)made.m_fYaw, made.m_iLine, survey.Weak(made.m_iSector).ToString(-1, 2));
   if (made.m_iTier == 1) seconds += Duration(made);
   if (made.m_Plan)
   {
    entities += made.m_Plan.m_aPiecePos.Count();
    pieces += made.m_Plan.Walls();
   }
   else if (made.m_eKind == ACR_EBaseSite.NEST) entities += 30;
   else if (made.m_eKind == ACR_EBaseSite.BUNKER) entities += 45;
   else entities += 50;
  }
  float ours, existing, either;
  survey.Coverage(work, ours, existing, either);
  PrintFormat("[ACR] BASEWORKS %1: covers %2%% of the threatened arc with lines, %3%% with those and what stands there (%4%% that alone); anchored ends %5 of %6", base.m_sName, (int)(ours * 100), (int)(either * 100), (int)(existing * 100), anchored, ends);
  PrintFormat("[ACR] BASEWORKS %1: %2 sandbag pieces, about %3 entities in all; tier I is %4 s of work, walking aside", base.m_sName, pieces, entities, (int)seconds);
  if (s_iMaxSitesForTest > 0) Trim(work);
 }

 protected static void Trim(ACR_BaseWork work)
 {
  map<string, int> seen = new map<string, int>();
  for (int i = 0; i < work.m_aSites.Count(); i++)
  {
   string kind = SiteWords(work.m_aSites[i]);
   int count;
   seen.Find(kind, count);
   seen.Set(kind, count + 1);
   // A line needs a stretch or two to be one, and a squad something to go on to.
   int keep = s_iMaxSitesForTest;
   if (work.m_aSites[i].m_Plan && work.m_aSites[i].m_Plan.m_eKind == ACR_EBaseDefenceKind.LINE) keep = s_iMaxSitesForTest * 3;
   if (count < keep) continue;
   work.m_aSites.RemoveOrdered(i);
   i--;
  }
 }

 // What goes up here: a tower where only its height clears the fan, or where there is a long view
 // or a wall to see over; a bunker at a road in or a shorter view. Among those that fit, by turns
 // as near as the place allows (a weighted pick, seeded).
 static bool TowerThere(ACR_BaseWork work, ACR_BaseSpot spot, int towersMade, int bunkersMade, int made)
 {
  if (!spot.m_bBunker) return true;
  if (!spot.m_bTower) return false;
  float tower = 0.5 - 0.25 * towersMade;
  float bunker = 0.5 - 0.25 * bunkersMade;
  if (spot.m_fReach >= 100) tower += 0.3;
  else bunker += 0.15;
  if (spot.m_bGate) bunker += 0.25;
  array<float> scores = {bunker, tower};
  return ACR_Variety.Pick(scores, string.Format("base works post %1 %2", work.m_Base.m_sName, made), 0.3) == 1;
 }

 // A gun already too close to this place, or side by side looking the same way.
 static bool GunsNear(ACR_BaseWork work, vector at, float facing)
 {
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   if (site.m_eKind != ACR_EBaseSite.NEST && site.m_eKind != ACR_EBaseSite.BUNKER) continue;
   float apart = vector.DistanceXZ(site.m_vPos, at);
   if (apart < GUN_APART) return true;
   if (apart < SAME_WAY && ACR_BaseDefences.DotXZ(ACR_BaseSurvey.Dir(site.m_fYaw), ACR_BaseSurvey.Dir(facing)) > 0.906) return true;
  }
  return false;
 }

 static bool GunWithin(ACR_BaseWork work, vector at, float within)
 {
  foreach (ACR_BaseSite site : work.m_aSites)
   if ((site.m_eKind == ACR_EBaseSite.NEST || site.m_eKind == ACR_EBaseSite.BUNKER) && vector.DistanceXZ(site.m_vPos, at) < within) return true;
  return false;
 }

 static bool CheckpointNear(ACR_BaseWork work, vector pos, float apart)
 {
  foreach (ACR_BaseSite site : work.m_aSites)
   if (site.m_eKind == ACR_EBaseSite.ROADBLOCK && vector.DistanceXZ(site.m_vPos, pos) < apart) return true;
  return false;
 }

 // ---------------------------------------------------------------- upkeep

 // From ACR_Fortify.Tick every 5 s: lost sites and a base that changed hands.
 static void Tick()
 {
  for (int w = s_aWorks.Count() - 1; w >= 0; w--)
  {
   ACR_BaseWork work = s_aWorks[w];
   if (!work.m_Base || work.m_Base.m_bRetired)
   {
    CallOff(work, "its base is gone");
    ACR_Defenders.Remove(work.m_Defenders);
    s_aWorks.RemoveOrdered(w);
    continue;
   }
   string owner = work.m_Base.m_sOwner;
   if (!owner.IsEmpty() && owner != work.m_sFaction) Transfer(work, owner);
   foreach (ACR_BaseSite site : work.m_aSites)
    if (site.m_bBuilt) PruneSite(site);
   // The tier stands while any site of it does.
   while (work.m_iTier > 0 && BuiltSites(work, work.m_iTier) == 0)
   {
    PrintFormat("[ACR] BASEWORKS %1 %2: %3 is gone", work.m_sFaction, work.m_Base.m_sName, ACR_FobDefences.TierName(work.m_iTier - 1));
    work.m_iTier--;
   }
  }
 }

 protected static void PruneSite(ACR_BaseSite site)
 {
  for (int i = site.m_aWalls.Count() - 1; i >= 0; i--)
  {
   IEntity piece = site.m_aWalls[i];
   if (piece && ACR_Logistics.Health(piece) > 0) continue;
   site.m_aWalls.Remove(i);
   if (piece) ACR_Navmesh.DeleteStructure(piece);
   site.m_bDamaged = site.m_Plan != null;
  }
  if (!site.m_aWalls.IsEmpty() || site.m_bAllCut) return;
  site.m_bDamaged = false;
  // The dressing goes with it; the site can be built again.
  foreach (IEntity prop : site.m_aDressing) if (prop) SCR_EntityHelper.DeleteEntityAndChildren(prop);
  site.m_aDressing.Clear();
  site.m_Roadblock = null;
  site.m_bBuilt = false;
  PrintFormat("[ACR] BASEWORKS: the %1 at %2 is gone", SiteWords(site), site.m_vPos);
 }

 protected static void Transfer(ACR_BaseWork work, string owner)
 {
  CallOff(work, "the base has fallen");
  PrintFormat("[ACR] BASEWORKS %1: %2 passes to %3 (%4 sites standing)", work.m_Base.m_sName, work.m_sFaction, owner, BuiltSites(work));
  work.m_sFaction = owner;
  // The sandbags change hands; the men in reserve don't.
  ACR_Defenders.Remove(work.m_Defenders);
  work.m_Defenders = null;
  work.m_iMannedTier = 0;
  foreach (ACR_BaseSite site : work.m_aSites)
  {
   // Nests and bunkers still to build are the new owner's kind.
   if (!site.m_sPrefab.IsEmpty()) site.m_sPrefab = ACR_FobDefences.PrefabFor(site.m_iSpot, owner, site.m_iTier - 1);
   if (site.m_Roadblock)
   {
    site.m_Roadblock.m_sFaction = owner;
    // Its reserve (-acrRoadblockDefenders) doesn't change hands either.
    ACR_Defenders.Remove(site.m_Roadblock.m_Defenders);
    site.m_Roadblock.m_Defenders = null;
   }
   foreach (IEntity piece : site.m_aWalls)
   {
    if (!piece) continue;
    SCR_CampaignBuildingCompositionComponent comp = SCR_CampaignBuildingCompositionComponent.Cast(piece.FindComponent(SCR_CampaignBuildingCompositionComponent));
    if (comp) comp.ACR_SetFaction(owner);
   }
  }
 }

 protected static void CallOff(ACR_BaseWork work, string why)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad && squad.m_FortifyPlan && squad.m_FortifyPlan.m_Work == work) squad.CancelFortify(why);
 }

 // The owner's defence credit in the AI's sums: dug in all round, its men hold longer than they
 // would in the open (as ACR_AICommander.FortifiedCredit counts a position's).
 static int Credit(ACR_Base base)
 {
  ACR_BaseWork work = Find(base);
  if (!work) return 0;
  switch (work.m_iTier)
  {
   case 1: return 2;
   case 2:
    if (work.m_Defenders) return 5;
    return 2;
   case 3:
    if (work.m_Defenders) return 10;
    return 3;
  }
  return 0;
 }

 // The owning side's command map: a pale ring round the town ("~": not a danger area).
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  foreach (ACR_BaseWork work : s_aWorks)
  {
   if (work.m_sFaction != faction || !work.m_Base) continue;
   int built = BuiltSites(work);
   if (built == 0) continue;
   positions.Insert(work.m_Base.GetOrigin());
   radii.Insert(ACR_BaseDefences.SearchRadius(work.m_Base));
   string tier = "Works";
   if (work.m_iTier > 0) tier = ACR_FobDefences.TierName(work.m_iTier - 1);
   labels.Insert(string.Format("~%1: %2 sites", tier, built));
  }
 }

 static void Reset()
 {
  s_aWorks.Clear();
  s_iMaxSitesForTest = 0;
  s_LastSurvey = null;
 }
}
