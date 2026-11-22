// AI commander, the turn and the 2IC interface: Tick and Turn (they call every manager, so they live
// in the last layer), squads handed to the AI, the 2IC's map UI and requests, Build a FOB here, and the
// hand-over and release.
//
// Last layer of ACR_AICommander (see ACR_AICommanderBuild.c for how the layers load).
modded class ACR_AICommander
{
 // Called every second by the manager.
 void Tick(bool noPlayerCommander)
 {
  if (m_bActive != noPlayerCommander)
  {
   m_bActive = noPlayerCommander;
   Say(string.Format("[ACR] AI commander %1: %2", m_sFaction, m_bActive));
   // With the 2IC on its plans carry on for the squads nobody claims (ACR_SquadAI.Adopt).
   if (!m_bActive && !ACR_SquadAI.TwoIC()) ReleaseAll();
   else if (!m_bActive && !ACR_SquadAI.ImThe2IC()) HandOverToPlainTwoIC();
   // Back in command: judge its doctrine on what it does from now, not on the player's spell, and
   // its initiative clock starts again (the spell counted as quiet: it came back at minimum pressure).
   m_fNextDoctrineReview = -1;
   if (m_bActive) m_fLastOffensive = GetGame().GetWorld().GetWorldTime();
  }
  if (!m_bActive) return;
  Turn();
 }

 // The AI commander's turn, every second: in command (Tick), or as the side's 2IC while a player
 // commands (TickHanded(true), Jacob 6 Nov: "I'm the 2IC"), the same turn with the squads nobody
 // has claimed (m_bHandedOnly: MySquads and the pickers see only those).
 protected void Turn()
 {
  GatherIntel();
  TickSapperDemos();
  m_iSeconds++;
  if (m_iSeconds % TICK_SECONDS != 1) return;
  int t = System.GetTickCount();
  m_iTickStarted = t;
  AccrueBuildBudget();
  TrickleBuildBudget();
  ReleaseRoadblockBuilders();
  Assess();
  ManageRecon();
  t = ACR_Perf.Lap("ai.assess", t);
  UpdateForecast();
  t = ACR_Perf.Lap("ai.forecast", t);
  UpdateAttacks();
  t = ACR_Perf.Lap("ai.attacks", t);
  AssignDefenders();
  ReturnToPosts();
  // COIN: towns and leads before grabs and attacks take the free squads (ACR_AICommanderSupport).
  ManageCoinPosts();
  t = ACR_Perf.Lap("ai.defend", t);
  UpdateLane();
  UpdateFrontGap();
  if (AllocatorControls()) RunAllocation(true);
  else AssignAttacks();
  t = ACR_Perf.Lap("ai.plan", t);
  PostIdle(); // with the allocator in control: only the squads it left with nothing to do
  MoveUp();
  ManageScreens();
  CatchRides();
  PositionMortars();
  Recrew();
  t = ACR_Perf.Lap("ai.post", t);
  ManageLogistics();
  ManageDangerRoads();
  ManageEvacs();
  ManageCoin();
  ManagePallets();
  ManageTruce();
  ManageFobBuild();
  ManageFobUpgrades();
  int beforeBuild = m_Manager.m_Bases.GetRP(m_sFaction);
  ManagePendingBuilds();
  ManageRoadblocks();
  SpendBuildBudget(beforeBuild - m_Manager.m_Bases.GetRP(m_sFaction));
  beforeBuild = m_Manager.m_Bases.GetRP(m_sFaction);
  ManageMinefields();
  SpendBuildBudget(beforeBuild - m_Manager.m_Bases.GetRP(m_sFaction));
  beforeBuild = m_Manager.m_Bases.GetRP(m_sFaction);
  ManageFortify();
  SpendBuildBudget(beforeBuild - m_Manager.m_Bases.GetRP(m_sFaction));
  t = ACR_Perf.Lap("ai.logistics", t);
  Requisition();
  t = ACR_Perf.Lap("ai.buy", t);
  ReviewDoctrine();
  LogStatus();
  int allocStart = System.GetTickCount();
  if (!AllocatorControls()) RunAllocation(false);
  ACR_Perf.Lap("ai.alloc", allocStart);
  m_iBuildLastRP = m_Manager.m_Bases.GetRP(m_sFaction);
 }

 void TickHanded(bool all = false)
 {
  m_bHandedOnly = true;
  m_bTwoICAll = all;
  if (all != m_bTwoICAllShown)
  {
   m_bTwoICAllShown = all;
   if (all) TwoICReport("Commanding the side as your 2IC: buying, supplies, FOBs and fire support. Your squads and orders stay yours");
  }
  if (all)
  {
   // "I'm the 2IC": the AI commander's whole turn, as if nobody commanded, with the squads nobody
   // has claimed; the player keeps every command power and whatever squads they lead or order.
   Turn();
   if (m_iSeconds % TICK_SECONDS == 1) PrintFormat("[ACR] AI %1: commanding the side as the player's 2IC, %2 squad(s) handed to it, %3 attack(s)", m_sFaction, ACR_SquadAI.CountFor(m_sFaction), m_Attacks.Count());
   m_bTwoICAll = false;
   m_bHandedOnly = false;
   return;
  }
  GatherIntel();
  TickSapperDemos(); // its FOB raids' charges, as in Tick (AssignAttacks starts them)
  m_iHandedSeconds++;
  if (m_iHandedSeconds % TICK_SECONDS == 1)
  {
   m_iTickStarted = System.GetTickCount();
   // Its engineering is paid out of the build budget only (CanBuild), set aside as in Tick.
   AccrueBuildBudget();
   TrickleBuildBudget();
   ReleaseRoadblockBuilders();
   Assess();
   UpdateAttacks();
   AssignDefenders();
   UpdateFrontGap();
   AssignAttacks();
   PostIdle();
   CatchRides();
   int beforeBuild = m_Manager.m_Bases.GetRP(m_sFaction);
   ManageRoadblocks();
   SpendBuildBudget(beforeBuild - m_Manager.m_Bases.GetRP(m_sFaction));
   beforeBuild = m_Manager.m_Bases.GetRP(m_sFaction);
   ManageMinefields();
   SpendBuildBudget(beforeBuild - m_Manager.m_Bases.GetRP(m_sFaction));
   // Its far builders ride out (RideToBuild) here too: without this the job was never given or
   // dropped, the squad sat on the task and no other roadblock or field was built (audit 31 Oct).
   beforeBuild = m_Manager.m_Bases.GetRP(m_sFaction);
   ManagePendingBuilds();
   SpendBuildBudget(beforeBuild - m_Manager.m_Bases.GetRP(m_sFaction));
   PrintFormat("[ACR] AI %1: commanding %2 squad(s) handed to it, %3 attack(s)", m_sFaction, ACR_SquadAI.CountFor(m_sFaction), m_Attacks.Count());
  }
  m_bTwoICAll = false;
  m_bHandedOnly = false;
 }

 // The RP this commander may spend: all of the side's, but while it is the lazy commander's 2IC
 // (ACR_SquadAI.RunsAll) the player's keep stays theirs. Every purchase check reads this, not m_iRP.
 int PurseForTest(bool all)
 {
  bool was = m_bTwoICAll;
  m_bTwoICAll = all;
  int purse = Purse();
  m_bTwoICAll = was;
  return purse;
 }

 // What the 2IC is doing, for its card: its grabs and attacks, then how many of its squads
 // hold, build, refit and wait. The first three, then "+N more".
 string TwoICDoing()
 {
  array<string> parts = {};
  if (LaneActive()) parts.Insert(string.Format("main effort toward %1", m_Lane.m_Objective.m_sName));
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (!attack.m_Target) continue;
   array<ACR_Squad> mine = {};
   foreach (ACR_Squad member : attack.m_Squads)
    if (member && member.m_bAICommanded) mine.Insert(member);
   if (mine.IsEmpty()) continue;
   string kind = "attacking";
   if (attack.m_bGrab) kind = "taking";
   else if (attack.m_bFobRaid) kind = "raiding";
   else if (attack.m_bCounter) kind = "counterattacking";
   parts.Insert(string.Format("%1 %2 (%3)", kind, attack.m_Target.m_sName, SquadList(mine)));
  }
  int holding, building, refitting, waiting;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad || !squad.m_bAICommanded || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   int task = squad.m_iAITask;
   if (task == ACR_EAITask.DEFEND) holding++;
   else if (task == ACR_EAITask.BUILD_FOB || task == ACR_EAITask.ROADBLOCK || task == ACR_EAITask.MINES || task == ACR_EAITask.FORTIFY) building++;
   else if (task == ACR_EAITask.RESUPPLY || m_Refitting.Contains(squad)) refitting++;
   else if (task == ACR_EAITask.FREE || task == ACR_EAITask.POSTED) waiting++;
  }
  if (holding > 0) parts.Insert(string.Format("%1 holding", holding));
  if (building > 0) parts.Insert(string.Format("%1 building", building));
  if (refitting > 0) parts.Insert(string.Format("%1 refitting", refitting));
  if (waiting > 0) parts.Insert(string.Format("%1 in reserve", waiting));
  if (parts.IsEmpty()) return "nothing yet";
  string doing = parts[0];
  for (int i = 1; i < parts.Count() && i < 3; i++) doing += ", " + parts[i];
  if (parts.Count() > 3) doing += string.Format(", +%1 more", parts.Count() - 3);
  return doing;
 }

 bool SmallForceForTest()
 {
  bool handed = m_bHandedOnly;
  m_bHandedOnly = true;
  bool small = SmallForce();
  m_bHandedOnly = handed;
  return small;
 }

 int HoldersCapForTest()
 {
  bool handed = m_bHandedOnly;
  m_bHandedOnly = true;
  int cap = HoldersCap();
  m_bHandedOnly = handed;
  return cap;
 }

 // What a squad of the 2IC's is doing, for its panel: "taking Morton", "holding Lamentin", ...
 string TaskText(ACR_Squad squad)
 {
  if (!squad) return "";
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (!attack.m_Target || !attack.m_Squads.Contains(squad)) continue;
   if (attack.m_bGrab) return "taking " + attack.m_Target.m_sName;
   if (attack.m_bFobRaid) return "raiding " + attack.m_Target.m_sName;
   if (attack.m_bCounter) return "counterattacking " + attack.m_Target.m_sName;
   return "attacking " + attack.m_Target.m_sName;
  }
  string at = "";
  if (squad.m_AITarget) at = " " + squad.m_AITarget.m_sName;
  int task = squad.m_iAITask;
  if (task == ACR_EAITask.DEFEND) return "holding" + at;
  if (task == ACR_EAITask.POSTED) return "posted" + at;
  if (m_AmmoRuns.Contains(squad)) return "rearming";
  if (task == ACR_EAITask.RESUPPLY || m_Refitting.Contains(squad)) return "refitting";
  if (task == ACR_EAITask.BUILD_FOB) return "building a FOB";
  if (task == ACR_EAITask.ROADBLOCK) return "on a roadblock";
  if (task == ACR_EAITask.MINES) return "laying mines";
  if (task == ACR_EAITask.FORTIFY) return "digging in";
  if (task == ACR_EAITask.SWEEP) return "clearing a road";
  if (task == ACR_EAITask.RESCUE) return "running an evac";
  if (task == ACR_EAITask.PALLET) return "recovering a supply pallet";
  if (task == ACR_EAITask.SUPPLY_RUN) return "on a supply run";
  if (task == ACR_EAITask.TOWN_POST) return "holding a town" + at;
  if (task == ACR_EAITask.SEARCH) return "searching for a hideout";
  if (task == ACR_EAITask.AID) return "taking aid to a town";
  if (task == ACR_EAITask.ATTACK) return "attacking" + at;
  return "in reserve";
 }

 // Answered as the 2IC it is: running the whole side ("I'm the 2IC"), what it buys for a player's
 // ask (a construction truck for "Build a FOB here") comes out of Purse too, above the player's
 // -acr2ICKeep, as everything else it buys (review 0.5.13 #14: the ask used to spend below the keep).
 void AnswerTwoIC(ACR_TwoICRequest r)
 {
  bool handed = m_bHandedOnly;
  bool all = m_bTwoICAll;
  m_bHandedOnly = true;
  m_bTwoICAll = ACR_SquadAI.RunsAll(m_Manager, m_sFaction);
  AnswerRequest(r);
  m_bTwoICAll = all;
  m_bHandedOnly = handed;
 }

 protected void AnswerRequest(ACR_TwoICRequest r)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  // Its squad is gone (lost, or a player took it back): another one, or why not.
  if (!r.m_sSquad.IsEmpty() && (!r.m_Squad || !m_Manager.m_Squads.Contains(r.m_Squad) || !r.m_Squad.m_bAICommanded || r.m_Squad.CountAlive() == 0))
  {
   string lost = r.m_sSquad;
   EndTwoICRequest(r);
   r.Set(ACR_ETwoICState.WAITING, lost + " can't any more: finding another");
  }
  int state = r.m_iState;
  if (state == ACR_ETwoICState.WAITING || (state == ACR_ETwoICState.CANT && r.m_bQueued && now >= r.m_fNextTry))
  {
   TryRequest(r);
   if (r.m_iState == ACR_ETwoICState.CANT)
   {
    r.m_fNextTry = now + ACR_TwoICRequests.RETRY_MS;
    if (!r.m_bQueued && r.m_fUntil <= 0) r.m_fUntil = now + ACR_TwoICRequests.DROP_MS;
   }
   return;
  }
  if (r.m_iKind == ACR_ETwoICAsk.FOB)
  {
   FobRequestProgress(r);
   return;
  }
  if (r.m_iKind == ACR_ETwoICAsk.TAKE)
  {
   if (!r.m_Base) return;
   if (r.m_Base.m_sOwner == m_sFaction)
   {
    EndTwoICRequest(r);
    r.Set(ACR_ETwoICState.DONE, "took " + r.m_Base.m_sName);
    return;
   }
   // The grab or the attack ended without it.
   if (state == ACR_ETwoICState.COMING && !IsAttacking(r.m_Base) && RequestedTarget() != r.m_Base)
   {
    EndTwoICRequest(r);
    r.Set(ACR_ETwoICState.CANT, "the attack on " + r.m_Base.m_sName + " didn't take it");
    r.m_fNextTry = now + ACR_TwoICRequests.RETRY_MS;
    if (!r.m_bQueued) r.m_fUntil = now + ACR_TwoICRequests.DROP_MS;
   }
   return;
  }
  if (state == ACR_ETwoICState.COMING && r.m_Squad && vector.DistanceXZ(PositionOf(r.m_Squad), r.m_vPos) <= ARRIVED)
  {
   if (r.m_iKind == ACR_ETwoICAsk.HOLD) r.Set(ACR_ETwoICState.HOLDING, string.Format("%1 holds %2 until you release it", r.m_sSquad, r.m_sPlace));
   else
   {
    r.m_fUntil = now + SUPPORT_MS;
    r.Set(ACR_ETwoICState.SUPPORTING, string.Format("%1 supports you at %2 for 10 min", r.m_sSquad, r.m_sPlace));
   }
   return;
  }
  if (state == ACR_ETwoICState.SUPPORTING && now >= r.m_fUntil)
  {
   string done = r.m_sSquad;
   EndTwoICRequest(r);
   r.Set(ACR_ETwoICState.DONE, done + " is done supporting and free again");
  }
 }

 protected void TryRequest(ACR_TwoICRequest r)
 {
  if (r.m_iKind == ACR_ETwoICAsk.FOB)
  {
   TryFob(r);
   return;
  }
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  if (mine.IsEmpty())
  {
   r.Set(ACR_ETwoICState.CANT, "I have no squads: every squad of ours is a player's");
   return;
  }
  if (r.m_iKind == ACR_ETwoICAsk.TAKE)
  {
   TryTake(r, mine);
   return;
  }
  ACR_Squad squad = RequestSquad(mine, r.m_vPos);
  if (!squad)
  {
   r.Set(ACR_ETwoICState.CANT, string.Format("none of my %1 squads is free (%2)", mine.Count(), BusyList(mine)));
   return;
  }
  int dist = (int)vector.DistanceXZ(PositionOf(squad), r.m_vPos);
  if (dist > REQUEST_REACH)
  {
   r.Set(ACR_ETwoICState.CANT, string.Format("my nearest free squad, %1, is %2 m away", squad.GetName(), dist));
   return;
  }
  string from = ACR_TwoIC.Where(squad);
  Forget(squad);
  squad.SetAITask(ACR_EAITask.DEFEND);
  squad.m_AITarget = null;
  if (r.m_iKind == ACR_ETwoICAsk.HOLD) squad.SetMode(ACR_EMoveMode.COMBAT);
  else squad.SetMode(ACR_EMoveMode.FAST);
  Travel(squad, r.m_vPos, true, 0);
  m_OnRequest.Set(squad, r.m_iId);
  r.m_Squad = squad;
  r.m_sSquad = squad.GetName();
  r.m_fUntil = 0;
  r.Set(ACR_ETwoICState.COMING, string.Format("%1, %2", squad.GetName(), from));
  PrintFormat("[ACR] 2IC %1: %2 answers request %3 (%4 m)", m_sFaction, squad.GetName(), r.m_iId, dist);
 }

 protected void TryTake(ACR_TwoICRequest r, array<ACR_Squad> mine)
 {
  ACR_Base base = r.m_Base;
  if (!base || base.m_bRetired)
  {
   r.Set(ACR_ETwoICState.DONE, "there's no base there any more");
   return;
  }
  if (base.m_sOwner == m_sFaction)
  {
   r.Set(ACR_ETwoICState.DONE, base.m_sName + " is ours");
   return;
  }
  // A truce made after the ask: the partner's base is off limits (no fire, no capture).
  if (ACR_Insurgency.Friendly(base.m_sOwner, m_sFaction))
  {
   r.Set(ACR_ETwoICState.DONE, base.m_sName + " is held by a side we have a truce with");
   return;
  }
  if (IsAttacking(base))
  {
   r.Set(ACR_ETwoICState.COMING, "already attacking " + base.m_sName);
   return;
  }
  if (base.m_sOwner.IsEmpty())
  {
   ACR_Squad runner = RequestSquad(mine, base.GetOrigin());
   if (!runner)
   {
    r.Set(ACR_ETwoICState.CANT, string.Format("none of my %1 squads is free to take %2 (%3)", mine.Count(), base.m_sName, BusyList(mine)));
    return;
   }
   int dist = (int)vector.DistanceXZ(PositionOf(runner), base.GetOrigin());
   RequestGrab(runner, base);
   r.m_Squad = runner;
   r.m_sSquad = runner.GetName();
   r.m_fUntil = 0;
   r.Set(ACR_ETwoICState.COMING, string.Format("%1 is going to take %2 (%3 m away)", runner.GetName(), base.m_sName, dist));
   return;
  }
  if (SmallForce())
  {
   r.Set(ACR_ETwoICState.CANT, string.Format("with %1 squads I only take empty bases, not %2", ACR_SquadAI.CountFor(m_sFaction), base.m_sName));
   return;
  }
  float need = AttackNeed(base) * Pressure();
  array<ACR_Squad> free = {};
  MySquads(free, true);
  int available;
  foreach (ACR_Squad counted : free) available += counted.Strength();
  m_RequestedTarget = base;
  m_iRequestedBy = r.m_iId;
  r.m_fUntil = 0;
  if (need <= available) r.Set(ACR_ETwoICState.COMING, string.Format("going for %1 with my free squads (strength %2 of the %3 it needs)", base.m_sName, available, (int)need));
  else r.Set(ACR_ETwoICState.CANT, string.Format("not yet: I have %1 of the %2 strength it needs free", available, (int)need));
 }

 // A request ends (done, cancelled, released, its squad lost): its squad goes back to the 2IC's
 // own plans; a grab made for it is called off; an attack target it set is dropped.
 void EndTwoICRequest(ACR_TwoICRequest r)
 {
  if (m_iRequestedBy == r.m_iId)
  {
   m_RequestedTarget = null;
   m_iRequestedBy = 0;
  }
  ACR_Squad squad = r.m_Squad;
  r.m_Squad = null;
  r.m_sSquad = "";
  if (!squad) return;
  int id;
  if (m_OnRequest.Find(squad, id) && id == r.m_iId)
  {
   m_OnRequest.Remove(squad);
   if (squad.m_iAITask == ACR_EAITask.DEFEND && !squad.m_AITarget) squad.SetAITask(ACR_EAITask.FREE);
  }
  if (r.m_iKind == ACR_ETwoICAsk.TAKE && r.m_Base && r.m_Base.m_sOwner != m_sFaction && OnGrab(squad))
  {
   Forget(squad);
   squad.SetAITask(ACR_EAITask.FREE);
  }
  // A FOB asked for and called off before it stands: the truck stops, its loads go back on it, and
  // the site isn't avoided (nobody gave up on it).
  if (r.m_iKind == ACR_ETwoICAsk.FOB && squad == m_FobBuilder && !FobStandsNear(r.m_vPos) && !squad.BuiltFobAt(r.m_vPos))
  {
   if (squad.OrderState() == ACR_EOrderState.BUILD_FOB || squad.m_iAITask == ACR_EAITask.BUILD_FOB) squad.CancelEstablishFob();
   m_FobBuilder = null;
   m_bFobProject = false;
   if (m_FobEscort && m_FobEscort.m_iAITask == ACR_EAITask.DEFEND && !m_FobEscort.m_AITarget) m_FobEscort.SetAITask(ACR_EAITask.FREE);
   m_FobEscort = null;
  }
 }

 // "Build a FOB here" (ACR_TwoICRequests, the commander's): a checked site, then one of my loaded
 // construction trucks with an escort, as my own FOB builds go (ManageFobBuild: one project at a
 // time, given up after 3 min without progress); with none, one bought at the base nearest the site.
 // What waits on time or RP is kept queued by itself, so the line says Queued and the 2IC tries
 // again every 20 s until the commander cancels it.
 protected void TryFob(ACR_TwoICRequest r)
 {
  if (FobStandsNear(r.m_vPos))
  {
   r.Set(ACR_ETwoICState.DONE, "a FOB of ours stands there already");
   return;
  }
  if (!r.m_bSited)
  {
   vector site;
   string why;
   if (!ACR_FobBuild.ValidateSite(r.m_vPos, site, why, m_sFaction))
   {
    r.Set(ACR_ETwoICState.CANT, "a FOB can't go there: " + why);
    return;
   }
   r.m_vPos = site;
   r.m_bSited = true;
  }
  if (HasActiveFobProject())
  {
   r.m_bQueued = true;
   r.Set(ACR_ETwoICState.CANT, "a FOB is being built already: yours is next");
   return;
  }
  ACR_Squad builder = FindConstructionSquad();
  if (builder)
  {
   vector cleared;
   string blocked;
   if (!ACR_FobBuild.ValidateSite(r.m_vPos, cleared, blocked, m_sFaction))
   {
    r.m_bSited = false;
    r.Set(ACR_ETwoICState.CANT, "the site isn't clear any more: " + blocked);
    return;
   }
   builder.SetAITask(ACR_EAITask.BUILD_FOB);
   if (!builder.EstablishFob(cleared))
   {
    builder.SetAITask(ACR_EAITask.FREE);
    r.Set(ACR_ETwoICState.CANT, builder.GetName() + " can't take the order");
    return;
   }
   m_FobBuilder = builder;
   m_bFobProject = true;
   m_vFobTargetSite = cleared;
   r.m_vPos = cleared;
   string with = "";
   ACR_Squad escort = FindEscortSquad(builder);
   if (escort)
   {
    m_FobEscort = escort;
    escort.SetAITask(ACR_EAITask.DEFEND);
    escort.m_AITarget = null;
    escort.SetMode(ACR_EMoveMode.FAST);
    Travel(escort, cleared, true, 0);
    with = ", " + escort.GetName() + " escorting it";
   }
   r.m_Squad = builder;
   r.m_sSquad = builder.GetName();
   r.m_fUntil = 0;
   r.m_bQueued = false;
   r.Set(ACR_ETwoICState.COMING, string.Format("%1, %2%3", builder.GetName(), ACR_TwoIC.Where(builder), with));
   PrintFormat("[ACR] 2IC %1: %2 builds the FOB asked for at %3 (request %4)", m_sFaction, builder.GetName(), cleared, r.m_iId);
   return;
  }
  // No loaded truck of mine. One bought a moment ago (by me, for this or my own FOB) is waited for.
  float now = GetGame().GetWorld().GetWorldTime();
  if (now - m_fFobTruckBoughtAt < FOB_TRUCK_RETRY_MS)
  {
   r.Set(ACR_ETwoICState.WAITING, "a construction truck I bought is getting ready");
   return;
  }
  // A loaded one that isn't mine to send (a player's, giving a ride): not a second one bought.
  if (HasLoadedConstructionTruck())
  {
   r.m_bQueued = true;
   r.Set(ACR_ETwoICState.CANT, "our loaded construction truck isn't mine to send (a player's, or busy): I'll send one once one is free");
   return;
  }
  int entry, cost;
  ACR_Base base;
  string whyNot = ACR_TwoICSuggest.TruckOffer(m_Manager, m_sFaction, r.m_vPos, entry, cost, base);
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (whyNot.IsEmpty() && (!data || Purse(data) < cost)) whyNot = string.Format("not enough RP (%1 needed, %2 mine to spend)", cost, Purse(data));
  if (!whyNot.IsEmpty())
  {
   // Only a side with no construction truck on its list never gets one; the rest is time or RP.
   r.m_bQueued = !whyNot.Contains("no construction truck") && !whyNot.Contains("requisition list");
   r.Set(ACR_ETwoICState.CANT, "I have no loaded construction truck and can't buy one: " + whyNot);
   return;
  }
  int before = m_Manager.m_Bases.GetRP(m_sFaction);
  string bought = m_Manager.PurchaseAt(m_sFaction, entry, base);
  if (m_Manager.m_Bases.GetRP(m_sFaction) >= before)
  {
   r.m_bQueued = true;
   r.Set(ACR_ETwoICState.CANT, string.Format("buying a construction truck at %1 failed: %2", base.m_sName, bought));
   return;
  }
  m_fFobTruckBoughtAt = now;
  r.Set(ACR_ETwoICState.WAITING, string.Format("buying a construction truck at %1 (%2 RP)", base.m_sName, cost));
  PrintFormat("[ACR] AI %1 requisition (FOB asked for, request %2): %3", m_sFaction, r.m_iId, bought);
 }

 // The truck on a FOB request: Done once the FOB stands; a build that ended without one (given up,
 // or wiped out) is "can't yet" again, as any request whose answer fell through.
 protected void FobRequestProgress(ACR_TwoICRequest r)
 {
  if (r.m_iState != ACR_ETwoICState.COMING) return;
  if (FobStandsNear(r.m_vPos))
  {
   string builder = r.m_sSquad;
   EndTwoICRequest(r);
   r.Set(ACR_ETwoICState.DONE, string.Format("%1 built it: a FOB of ours stands at %2", builder, r.m_sPlace));
   return;
  }
  ACR_Squad truck = r.m_Squad;
  if (!truck) return;
  // Finished just now: the FOB appears on the next structures tick.
  if (truck.BuiltFobAt(r.m_vPos)) return;
  int order = truck.OrderState();
  if (order == ACR_EOrderState.BUILD_FOB || (order == ACR_EOrderState.REGROUP && truck.m_iAITask == ACR_EAITask.BUILD_FOB)) return;
  string gaveUp = r.m_sSquad;
  EndTwoICRequest(r);
  r.m_bSited = false;
  float now = GetGame().GetWorld().GetWorldTime();
  r.Set(ACR_ETwoICState.CANT, string.Format("%1 gave up the build at %2", gaveUp, r.m_sPlace));
  r.m_fNextTry = now + ACR_TwoICRequests.RETRY_MS;
  if (!r.m_bQueued) r.m_fUntil = now + ACR_TwoICRequests.DROP_MS;
 }

 protected bool FobStandsNear(vector pos)
 {
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base && base.m_bBuilt && !base.m_bRetired && base.m_sOwner == m_sFaction && vector.DistanceXZ(base.GetOrigin(), pos) <= FOB_ASK_NEAR) return true;
  }
  return false;
 }

 // What the squads are busy with, for a "none free" answer.
 protected string BusyList(array<ACR_Squad> mine)
 {
  int attacking, defending, holding, requests, support, other;
  foreach (ACR_Squad squad : mine)
  {
   if (OnRequest(squad)) requests++;
   else if (IsHolder(squad)) holding++;
   else if (IsSupportTeam(squad) || !squad.IsCombatUnit()) support++;
   else if (squad.m_iAITask == ACR_EAITask.ATTACK) attacking++;
   else if (squad.m_iAITask == ACR_EAITask.DEFEND) defending++;
   else other++;
  }
  string list = "";
  if (attacking > 0) list += string.Format(", %1 attacking", attacking);
  if (defending > 0) list += string.Format(", %1 defending", defending);
  if (holding > 0) list += string.Format(", %1 holding what they built", holding);
  if (requests > 0) list += string.Format(", %1 on requests", requests);
  if (support > 0) list += string.Format(", %1 support teams", support);
  if (other > 0) list += string.Format(", %1 busy otherwise", other);
  if (list.IsEmpty()) return "all busy";
  return list.Substring(2, list.Length() - 2);
 }

 // A grab of an empty base by this squad, as AssignAttacks makes one.
 protected void RequestGrab(ACR_Squad runner, ACR_Base empty)
 {
  Forget(runner);
  StartGrab(runner, empty);
  PrintFormat("[ACR] 2IC %1: %2 grabs %3 as asked", m_sFaction, runner.GetName(), empty.m_sName);
 }

 // The scored FOB sites behind 'front' (2IC suggestions and Establish FOB, ACR_TwoICSuggest), as
 // FindFobSiteStep searches them: 'most' more validated; true when the search is complete.
 bool TwoICFobSites(ACR_Base front, int most, notnull array<ref ACR_FobSite> sites)
 {
  if (!front) return true;
  ACR_Base deployBase = NearestDeployBase(front);
  vector dir = Vector(0, 0, -1);
  if (deployBase)
  {
   vector toDeploy = deployBase.GetOrigin() - front.GetOrigin();
   toDeploy[1] = 0;
   if (toDeploy.LengthSq() > 0.001)
   {
    toDeploy.Normalize();
    dir = toDeploy;
   }
  }
  string none;
  return ACR_FobSiting.Step(m_sFaction, front, dir, NearestEnemyBaseTo(front), most, sites, none);
 }

 // A squad given back to its player: out of every plan of ours.
 // A player took command with the plain 2IC (-acr2IC 1, kept apart from "I'm the 2IC" by ruling):
 // TickHanded runs only part of the AI's managers, so a squad on a task of the other part (refit walks
 // and ammo runs, screens, road sweeps, evacs, pallets, its own FOB build or fortifying) kept that task
 // all spell with nothing to end it, and its ammo trucks stayed booked. Those come off here; the jobs
 // themselves (a build under way) carry on as the squad's (review, 12 Nov). "I'm the 2IC" runs the
 // whole turn and keeps every plan.
 protected void HandOverToPlainTwoIC()
 {
  EndAllAmmoRuns("");
  int freed;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   ACR_EAITask task = squad.m_iAITask;
   bool stranded = task == ACR_EAITask.RESUPPLY || task == ACR_EAITask.SWEEP || task == ACR_EAITask.RESCUE || task == ACR_EAITask.PALLET;
   if (!stranded) stranded = task == ACR_EAITask.BUILD_FOB || task == ACR_EAITask.FORTIFY || IsScreen(squad);
   if (!stranded) continue;
   squad.SetAITask(ACR_EAITask.FREE);
   freed++;
  }
  m_Refitting.Clear();
  m_RefitWalkTo.Clear();
  m_Screens.Clear();
  m_EvacTaxi = null;
  m_PalletSquad = null;
  m_FobBuilder = null;
  m_bFobProject = false;
  m_FobEscort = null;
  Say(string.Format("[ACR] AI %1: a player took command (plain 2IC): %2 squad(s) freed from tasks it won't run", m_sFaction, freed));
 }

 void Forget(ACR_Squad squad)
 {
  m_OnRequest.Remove(squad);
  for (int i = m_Screens.Count() - 1; i >= 0; i--)
   if (!m_Screens[i] || m_Screens[i].m_Squad == squad) m_Screens.Remove(i);
  foreach (ACR_AIAttack attack : m_Attacks) LeaveAttack(attack, squad);
  m_RetreatingUntil.Remove(squad);
  m_ReinforceRetryAt.Remove(squad);
  m_Refitting.Remove(squad);
  ForgetRefit(squad);
  ForgetAmmo(squad);
  m_ForwardAt.Remove(squad);
  m_Trips.Remove(squad);
  if (m_ReserveSquad == squad) m_ReserveSquad = null;
  if (m_FobBuilder == squad)
  {
   m_FobBuilder = null;
   m_bFobProject = false;
  }
  if (m_FobEscort == squad) m_FobEscort = null;
 }

 bool PlansFor(ACR_Squad squad)
 {
  foreach (ACR_AIAttack attack : m_Attacks)
   if (attack.m_Squads.Contains(squad)) return true;
  return squad == m_ReserveSquad || IsScreen(squad) || m_Refitting.Contains(squad) || m_AmmoRuns.Contains(squad);
 }

 protected void ReleaseAll()
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
   if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == m_sFaction) squad.SetAITask(ACR_EAITask.FREE);
  m_Attacks.Clear();
  m_RetreatingUntil.Clear();
  m_ReinforceRetryAt.Clear();
  m_Refitting.Clear();
  m_RefitWalkTo.Clear();
  m_RefitGaveUpOn.Clear();
  m_RefitGaveUpAt.Clear();
  m_RefitGiveUps.Clear();
  // Ended, not just forgotten: their trucks are released too (m_ServiceJobs kept them booked).
  EndAllAmmoRuns("");
  m_AmmoLookAt.Clear();
  m_AmmoNoTruckUntil.Clear();
  m_AmmoSkips.Clear();
  m_Screens.Clear();
  m_Trips.Clear();
  m_ReserveSquad = null;
  m_iSaveForEntry = -1;
  m_iLastSavedLog = -1;
  m_FobBuilder = null;
  m_bFobProject = false;
  m_FobEscort = null;
  m_aPendingBuilds.Clear();
 }
}
