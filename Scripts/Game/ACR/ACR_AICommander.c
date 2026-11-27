enum ACR_EAITask
{
 FREE,      // available for orders
 DEFEND,    // holding a threatened base
 ATTACK,    // part of a base attack
 POSTED,    // idle, moved up to a frontline base
 RESUPPLY,  // on its way to an ammo / repair / fuel / medical vehicle
 BUILD_FOB, // establishing a forward base
 ROADBLOCK, // establishing or holding a forward roadblock
 MINES,     // laying a minefield on an approach (-acrMinesAI)
 FORTIFY,   // digging in or upgrading a position on an approach (-acrFortifyAI)
 SWEEP,     // clearing a road where FIA ambushed the side (ACR_Insurgency, #89)
 RESCUE,    // driving an evac: to the XP, loading, to a base (ACR_Evac, #107)
 PALLET,    // going for a supply pallet (ACR_Airdrop, #107)
 SUPPLY_RUN, // a construction truck crew driving a supply run (ACR_SupplyRuns, #242)
 TOWN_POST,  // COIN: holding a town to win it over (ACR_AICommanderSupport, #244)
 SEARCH,     // COIN: searching an FIA intel ring for its hideout
 AID         // COIN: a supply truck taking aid to a town
}

// Strategic AI for one faction, adapted from the Arma 3 mod's ai_strategic.sqf
// and ai_attack.sqf. Every 15 seconds it values each base, defends the ones
// under threat, attacks the weakest enemy or empty border base, posts idle
// squads to the front, and spends RP on reinforcements. It stands down while a
// player of its faction is in command.
class ACR_AICommander : Managed
{
 static const int TICK_SECONDS = 15;          // STRATEGY_TICK
 static const float DEFENSE_DISTANCE = 450;   // squads this close count as defending a base
 static const float WORKS_FRONT = 1500;       // m: an enemy base this near a base with lines standing makes it a front base (KeepsWorksManned)
 static const float THREAT_DISTANCE = 400;    // enemies this far past the ring threaten it
 static const float STAGING_DISTANCE = 210;   // staging point outside the target ring
 static const float STAGING_CLEARANCE = 350;  // m kept between a staging point and any other enemy base ring
 static const float ATTACK_RATIO = 1.8;       // attackers want 1.8x the defence
 static const float DEFENSE_RATIO = 1.1;      // defenders want 1.1x the threat
 static const float CONCEDE_RATIO = 4;        // give a base up against 4x the defence
 static const int PLAYER_STRENGTH = 5;
 static float STAGING_TIMEOUT = 300;          // seconds
 // An assault that hasn't taken the base by now is called off (it would
 // otherwise hold the attack slot forever). A foothold inside the base ring
 // buys it more time. Grabs of empty bases get less.
 static float ASSAULT_TIMEOUT = 600;          // s, nobody of ours inside the ring
 static float ASSAULT_HARD_TIMEOUT = 900;     // s, even with a foothold
 static bool s_bGrabByDistance = true;          // off in tests that shorten GRAB_TIMEOUT
 static float GRAB_TIMEOUT = 480;             // s  (the test harness shortens these)
 static const float STALLED_COOLDOWN = 300;   // s before the same base is tried again
 static const float PUSH_INTERVAL = 120;      // s between re-issuing assault orders
 static const float FIRE_BASE_RANGE = 500;    // m: the base of fire shoots only this close to a seen enemy
 static const float FIRE_BASE_STANDOFF = 300; // m: how far short of them it moves up when too far or blind
 // (s of supporting fire before the base of fire joins the assault anyway: the doctrine's m_fFireBaseSeconds, 240 standard)
 static const float FIRE_BASE_MOVE_S = 120;   // s to reach a firing spot before it joins instead
 static const float RETREAT_COOLDOWN_MS = 60000;
 // A reserve riding or regrouping is on its way: re-posting it would cancel that (20 Oct
 // live: a regrouping reserve was re-posted every 15 s for 15 min and never arrived).
 static bool ReserveMoving(ACR_EOrderState state) { return state == ACR_EOrderState.RIDE || state == ACR_EOrderState.REGROUP; }
 // An attack is called off below its abort share of the strength it started with. Float on the
 // left: int-first compares in ints (start 10 x 0.35 read as 3, so 3 men of 10 kept attacking).
 static bool AttackBroken(int strength, int startStrength, float abortRatio) { return startStrength > 0 && startStrength * abortRatio > strength; }
 static const float REINFORCE_ATTACK_DISTANCE = 1400;
 // How far a squad can get to an attack (#27, long attacks stalled: squads 2-3 km off were sent
 // on foot because an air lift was ready or rides existed, and the attack went in before they
 // came): on foot WALK_REACH (1.5 times REINFORCE_ATTACK_DISTANCE, as before); further only in its
 // own vehicle, a taxi it can get, or a ready air lift. The forming up waits for the slowest.
 static const float WALK_REACH = 2100;
 static const float WALK_SPEED = 1.2;        // m/s on foot, as a grab's time limit counts it
 static const float RIDE_SPEED = 4;          // m/s by truck door to door (ACR_Rides.Request's estimate)
 static const float MOUNTED_SPEED = 6;
 static const float AIR_LIFT_SECONDS = 240;
 static const float STAGE_TRAVEL_MAX = 1200; // s: the most the forming up waits
 static const float REINFORCE_ATTACK_MOUNTED = 2200;
 static const float ASSAULT_JOIN_EXTRA = 400; // m past the staging ring a squad may be to join an assault already going in
 static const float STAGE_STRETCH = 0.5;      // late joiners may stretch the forming up by half of STAGING_TIMEOUT
 // Initiative: with no attack launched for a while, the commander accepts
 // worse odds (down to 65% of the usual strength) and commits its reserve,
 // so two AI sides don't sit facing each other forever. How long and how far: the doctrine.
 float m_fLastOffensive = -1;
 float m_fNextWeakLog;
 ACR_Squad m_RefillWaiting;         // a squad at a base that could refill now but for the RP (Requisition holds purchases)
 float m_fNextRefillLog;

 float Pressure()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (m_fLastOffensive < 0) m_fLastOffensive = now;
  float quiet = (now - m_fLastOffensive) / 60000 - m_Doctrine.m_fQuietMinutes;
  if (quiet <= 0) return 1;
  return Math.Max(m_Doctrine.m_fMinPressure, 1 - quiet * m_Doctrine.m_fPressurePerMinute);
 }

 ACR_Manager m_Manager;
 string m_sFaction;
 string Faction() { return m_sFaction; }
 bool m_bActive;
 protected int m_iSeconds;
 ref array<ref ACR_AIAttack> m_Attacks = {};
 ref map<ACR_Base, int> m_Defence = new map<ACR_Base, int>();
 ref map<ACR_Base, int> m_Threat = new map<ACR_Base, int>();
 ref map<ACR_Base, string> m_LastOwner = new map<ACR_Base, string>();
 ACR_Base m_CounterTarget;
 ref map<ACR_Base, float> m_StalledUntil = new map<ACR_Base, float>();
 ref map<ACR_Squad, float> m_RetreatingUntil = new map<ACR_Squad, float>();
 ref map<ACR_Squad, float> m_ReinforceRetryAt = new map<ACR_Squad, float>();
 ref map<ACR_Squad, float> m_Refitting = new map<ACR_Squad, float>(); // squad walking back to refill -> give up at
 // The base a squad walks back to for refills, the last one it gave up walking to and when, and
 // how many walks in a row it gave up (RefitGaveUpOn, NoteRefitGaveUp).
 ref map<ACR_Squad, ACR_Base> m_RefitWalkTo = new map<ACR_Squad, ACR_Base>();
 ref map<ACR_Squad, ACR_Base> m_RefitGaveUpOn = new map<ACR_Squad, ACR_Base>();
 ref map<ACR_Squad, float> m_RefitGaveUpAt = new map<ACR_Squad, float>();
 ref map<ACR_Squad, int> m_RefitGiveUps = new map<ACR_Squad, int>();
 int m_iSaveForEntry = -1;
 int m_iLastSavedLog = -1;
 protected float m_fNextStatus;
 float m_fCounterUntil;
 // Retreat (a squad in contact) and abort (a whole attack) ratios: the doctrine's, 0.4 / 0.35 standard.
 static const int SUPPORT_RESERVE = 15;    // RP the AI keeps back when calling supports

 // Engineering budget (-acrAIBuildShare N, 0 = off by default): N% (at most 30, times the
 // doctrine's m_fBuildShare) of what the side's RP gains between strategy ticks is set aside for
 // roadblocks and minefields (and Fortify positions where that order exists), and at least
 // BUILD_TRICKLE each strategy tick; those orders are paid out of it (CanBuild). At most
 // BUILD_BUDGET_MAX, never more than the side holds. Unit purchases leave only BUILD_FENCE of it
 // alone (BuildKeep): the next order's worth, not the whole budget. (6 Nov Chernarus: USSR held
 // 150-190 RP for 20 min, 100 of it a full budget no squad was free to spend, so units saw ~20.)
 static const int BUILD_BUDGET_MAX = 100;
 static const int BUILD_FENCE = 30;        // the dearest single AI engineering order (minefield 25-28, fortify 25)
 static const float BUILD_TRICKLE = 2;
 float m_fBuildBudget;
 protected int m_iBuildLastRP = -1;
 static int BuildShare() { return Math.ClampInt(ACR_LogisticsRules.Choice("acrAIBuildShare", "0").ToInt(), 0, 30); }
 int BuildBudget() { return (int)m_fBuildBudget; }
 // This side's share: the 2IC panel's for its side (ACR_TwoIC.ShareFor), else the setting.
 int SideShare()
 {
  int share;
  if (ACR_TwoIC.ShareFor(m_sFaction, share)) return share;
  return BuildShare();
 }
 float EffectiveShare() { return ShareAt(m_Manager.m_Bases.GetRP(m_sFaction)); }

 // A rich side digs in (#28): over SURPLUS_FROM banked RP the share rises with the surplus, to twice
 // the share at the RP cap (ACR_BaseSystem.POINTS_CAP), still at most 50%; and the budget is kept at
 // least at that share of what is banked over SURPLUS_FROM, still at most BUILD_BUDGET_MAX. A side
 // at the cap gains no RP to take a share of (overnight Everon games: the leading side sat at the
 // cap from about 105 minutes with nothing left to buy). At or below SURPLUS_FROM nothing changes.
 static const int SURPLUS_FROM = 600;
 // SURPLUS_FROM varied by up to 15% per side (ACR_Variety, seeded from the match seed): a rich side
 // starts digging in at a slightly different bank each match.
 int SurplusFrom() { return (int)ACR_Variety.Jitter(SURPLUS_FROM, 0.15, "surplus " + m_sFaction); }

 // x1 at or below SURPLUS_FROM, x2 at the RP cap.
 float SurplusFactor(int rp)
 {
  float over = (rp - SurplusFrom()) / (float)(ACR_BaseSystem.POINTS_CAP - SurplusFrom());
  return 1 + Math.Clamp(over, 0, 1);
 }

 // A rich side fires more (#28 step 2), by the same x1..x2 surplus factor: the RP held for fire
 // missions and the missions it counts rise, while the threat that earns defensive fire or air
 // strikes and the defenders that earn heavy preparation fall, and its observers call sooner.
 float FireFactor() { return SurplusFactor(m_Manager.m_Bases.GetRP(m_sFaction)); }
 static int Lowered(int threshold, float factor)
 {
  float lowered = (1.0 / factor) * threshold;
  return Math.Max(1, (int)Math.Round(lowered));
 }

 // The share (percent of RP gained) with 'rp' banked.
 float ShareAt(int rp) { return Math.Min(SideShare() * m_Doctrine.m_fBuildShare * SurplusFactor(rp), 50); }

 // The least the budget holds with 'rp' banked: that share of the surplus (0 at or below SURPLUS_FROM).
 float SurplusFloor(int rp)
 {
  if (SideShare() <= 0 || rp <= SurplusFrom()) return 0;
  return Math.Min((rp - SurplusFrom()) * ShareAt(rp) / 100.0, BUILD_BUDGET_MAX);
 }

 // At the start of a strategy tick: the share of the RP gained since the last one, topped up to
 // the surplus floor.
 void AccrueBuildBudget()
 {
  int rp = m_Manager.m_Bases.GetRP(m_sFaction);
  float share = ShareAt(rp);
  if (share <= 0) m_fBuildBudget = 0;
  else if (m_iBuildLastRP >= 0 && rp > m_iBuildLastRP) m_fBuildBudget += (rp - m_iBuildLastRP) * share / 100.0;
  m_fBuildBudget = Math.Max(m_fBuildBudget, SurplusFloor(rp));
  m_fBuildBudget = Math.Clamp(m_fBuildBudget, 0, Math.Min(BUILD_BUDGET_MAX, rp));
  m_iBuildLastRP = rp;
 }

 // Each strategy tick while the share is on: at least a trickle goes in, so a side with a small
 // income still builds (playtest 25 Oct: 15% of a small income held 0-32 RP, and the AI built
 // one roadblock and one hasty position all game).
 void TrickleBuildBudget()
 {
  if (SideShare() <= 0) return;
  int rp = m_Manager.m_Bases.GetRP(m_sFaction);
  m_fBuildBudget = Math.Clamp(m_fBuildBudget + BUILD_TRICKLE * m_Doctrine.m_fBuildShare, 0, Math.Min(BUILD_BUDGET_MAX, rp));
 }

 void SpendBuildBudget(int spent)
 {
  if (spent > 0) m_fBuildBudget = Math.Max(0, m_fBuildBudget - spent);
 }

 // What unit purchases keep back for it: the next order's worth. The rest of the budget is how much
 // engineering may spend, not RP locked away: units may spend it, and the budget follows the side's
 // RP down at the next strategy tick (AccrueBuildBudget).
 int BuildKeep()
 {
  if (SideShare() <= 0) return 0;
  return Math.Min(BuildBudget(), BUILD_FENCE);
 }

 string BuildStatus()
 {
  if (SideShare() <= 0) return "";
  int holders;
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.GetFaction() && squad.GetFaction().GetFactionKey() == m_sFaction && IsHolder(squad)) holders++;
  }
  string surplus = "";
  int rp = m_Manager.m_Bases.GetRP(m_sFaction);
  if (rp > SurplusFrom()) surplus = string.Format(", raised by %1 RP banked over %2", rp - SurplusFrom(), SurplusFrom());
  return string.Format(" | build budget %1 RP (%2%% of income%3), holding %4 of %5", BuildBudget(), (int)EffectiveShare(), surplus, holders, HoldersCap());
 }
 static const int MAX_GRABS = 3;           // empty bases it goes for at the same time (at least)
 static const int MAX_GRABS_BIG = 8;       // ... at most, on maps with many empty bases
 // Planning work one strategic tick may do before further grabs and attacks wait for the next
 // (Everon soak 23 Oct: an air lift search, a ride request and a water route in one tick froze
 // the server for 200 ms; each costs up to ~100 ms on its own).
 static const int PLAN_BUDGET_MS = 50;
 int m_iTickStarted;

 // The style this commander fights to this match (ACR_AIDoctrine), chosen when it is made.
 ref ACR_AIDoctrine m_Doctrine;
 string m_sDoctrineSource;
 // Its home style: the doctrine it started with (its company's, unless the setup screen overrode
 // it). A changed doctrine goes back to it once the trouble that changed it is over (PickDoctrine),
 // never inside #29's hold, and only after a varied wait past it (HomeDriftMin).
 string m_sHomeDoctrine;
 ACR_AIDoctrine Doctrine() { return m_Doctrine; }
 string HomeDoctrine() { return m_sHomeDoctrine; }
 void SetDoctrineForTest(ACR_AIDoctrine doctrine) { m_Doctrine = doctrine; m_sDoctrineSource = "test"; }

 void ACR_AICommander(ACR_Manager manager, string factionKey)
 {
  m_Manager = manager;
  m_sFaction = factionKey;
  m_Doctrine = ACR_AIDoctrine.Choose(factionKey, m_sDoctrineSource);
  m_sHomeDoctrine = m_Doctrine.m_sKey;
  Say(string.Format("[ACR] AI commander %1 doctrine: %2 (%3; %4)", m_sFaction, m_Doctrine.m_sName, m_sDoctrineSource, m_Doctrine.m_sSummary));
 }
 float RetreatRatio() { return m_Doctrine.m_fRetreatRatio; }
 // Defenders of an overrun base hold to their own ratio (Fortress stands, Raider slips away).
 float HoldRatio() { return m_Doctrine.m_fHoldRatio; }

 // ---------------------------------------------------------------- job allocator
 // -acrAIAllocator 1 (allocator phase 1, 3 Nov 2026): after the managers' tick the allocator
 // (ACR_AIAllocator) works out its own assignment of the combat jobs from the same picture and logs
 // where it differs ("ALLOC <side>: would move ..."), with a summary line and a Think section;
 // nothing is ordered. Phase 2 lets it assign them. In it: free, posted, attacking and defending
 // combat squads. Left out: works, refits, sweeps, FOB building, 2IC requests, the FOB escort, recon
 // teams, squads falling back (later phases), and as always players', mortar and aircraft crews.
 static const float ALLOC_ATTACK_VALUE = 10;
 static const float ALLOC_GRAB_VALUE = 3;   // observe soak 3 Nov: at 6 every reserve and attacker 'should' grab
 static const float ALLOC_DEFEND_VALUE = 12;
 static const float ALLOC_HOLD_VALUE = 2;
 static const float ALLOC_RESERVE_VALUE = 20; // a reserve with threatened bases in reach: only a clearly better job takes it
 static const float ALLOC_RESERVE_QUIET = 6;  // one on a quiet front (soak 4 Nov: a flat 20 outbid three-squad attacks)
 static const float ALLOC_RESERVE_THREAT = 12; // threat strength in reach at which a reserve is worth the full value
 static const float ALLOC_GRACE = 120;      // s a launched attack keeps its squads before the allocator may call it off
 static const int ALLOC_ATTACK_TARGETS = 6; // enemy bases weighed as attack jobs per tick
 static const int ALLOC_GRAB_TARGETS = 8;   // empty bases weighed as grabs per tick
 static const int ALLOC_LOG_LINES = 6;      // "would move" lines per tick at most
 static const int ALLOC_FULL_REACH = 3;     // full Reach checks per new attack (estimates rank, Reach confirms)
 protected ref array<ref ACR_AIJob> m_aAllocJobs = {};
 protected ref array<ref ACR_AIJobSquad> m_aAllocSquads = {};
 protected ref array<string> m_aAllocThink = {};
 protected int m_iAllocAgree, m_iAllocDiffer, m_iAllocTicks;

 static const float ALLOC_DROP_COOLDOWN = 120; // s before an attack the allocator called off is weighed again

 // Assault squads stay on their attack (Jacob, 19 Nov; Kamenoe, 11:55: mid-assault the allocator sent
 // two of its three squads to bases on a forecast, the Reservist Squad went in alone, the attack was
 // called off for losses and the USSR ended with no combat-ready squad). Once its attack is assaulting,
 // or it is within ALLOC_ASSAULT_COMMIT of its staging point, a squad of a running attack is offered a
 // defence only for a real threat there (AttackerMayDefend): the enemies at the base outnumber what holds
 // it, or an HQ or FOB is under attack. A forecast alone never pulls it; free squads still answer one,
 // and a called-off attack frees its squads as before.
 static const float ALLOC_ASSAULT_COMMIT = 600;  // m from its staging point: closing in, committed
 static const float KEY_BASE_THREAT_SHARE = 0.5; // an HQ or FOB is under attack past this share of its defence

 // Pure: may a squad committed to an assault be pulled to defend a base with 'threat' enemies at it now
 // (contacts, not the forecast) against 'defence' there? 'keyBase': an HQ or a FOB (where the side deploys).
 static bool AttackerMayDefend(int threat, int defence, bool keyBase)
 {
  if (threat <= 0) return false;
  if (keyBase) return defence * KEY_BASE_THREAT_SHARE <= threat;
  return threat > defence;
 }

 // Its attack holds it (see above): assaulting, or this squad closing on its staging point.
 protected bool HeldByAssault(ACR_Squad squad, ACR_AIAttack attack, vector pos)
 {
  if (!attack || attack.m_bGrab || !attack.m_Target) return false;
  if (attack.m_bAssaulting) return true;
  return vector.DistanceXZ(pos, attack.StagingOf(squad)) <= ALLOC_ASSAULT_COMMIT;
 }
 // 2 by default (#232), 0 with the classic commander (ACR_AIRework).
 static int AllocatorMode() { return Math.ClampInt(ACR_LogisticsRules.Choice("acrAIAllocator", ACR_AIRework.Default("2")).ToInt(), 0, 2); }
 // -acrAIAllocator 2: the allocator assigns the combat jobs (in place of AssignAttacks and PostIdle).
 bool AllocatorControls() { return AllocatorMode() == 2 && !m_bHandedOnly; }

 // What a base is worth to take or keep, before the job's own weight.
 protected float AllocWorth(ACR_Base base)
 {
  float worth = 2 + Math.Max(0, base.m_iValue) * 2;
  if (base.m_bBuilt) worth += 2;
  if (base == m_CounterTarget && CounterDue()) worth *= 1.5;
  if (base == RequestedTarget()) worth *= 2;
  return worth;
 }

 // A reserve is worth what it can answer: the threat to own bases within its reach.
 protected float AllocReserveValue(ACR_Base reserveBase)
 {
  float threat;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_sOwner != m_sFaction) continue;
   if (vector.DistanceXZ(base.GetOrigin(), reserveBase.GetOrigin()) > RESERVE_MAX_PULL) continue;
   threat += ThreatTo(base);
  }
  float share = Math.Min(1, threat / ALLOC_RESERVE_THREAT);
  return ALLOC_RESERVE_QUIET + (ALLOC_RESERVE_VALUE - ALLOC_RESERVE_QUIET) * share;
 }

 protected ACR_AIJob AllocJob(int kind, ACR_Base target, string label, float value)
 {
  ACR_AIJob job = new ACR_AIJob();
  job.m_iKind = kind;
  job.m_Target = target;
  job.m_sName = label;
  job.m_fValue = value;
  job.m_sKey = typename.EnumToString(ACR_EAITask, kind) + " " + label;
  m_aAllocJobs.Insert(job);
  return job;
 }

 protected ACR_AIJob AllocAttack(ACR_Base base)
 {
  ACR_AIJob job = AllocJob(ACR_EAITask.ATTACK, base, base.m_sName, ALLOC_ATTACK_VALUE * AllocWorth(base) * EnvelopBonus(base) * IncomeGrabBonus(base));
  job.m_bTeam = true;
  job.m_bNeedsTaker = true;
  job.m_iMaxSquads = 8;
  job.m_fPatience = 600;
  float need = 1;
  if (!base.m_sOwner.IsEmpty()) need = AttackNeed(base);
  if (InLaneOrFlank(base)) need *= LANE_MASS; // the main effort is filled up further
  if (base == m_CounterTarget && CounterDue()) need = need * m_Doctrine.m_fCounterNeed;
  job.m_fNeed = need;
  // The planner's own bar: the doctrine's pressure after a quiet spell, lower on High threat.
  float bar = Pressure();
  if (ACR_Manager.ThreatOption() == 2) bar = bar * 0.85;
  job.m_fBar = bar;
  return job;
 }

 // A counterattack on a base just lost is still on (AssignAttacks' counterDue).
 protected bool CounterDue()
 {
  return m_CounterTarget && m_CounterTarget.m_sOwner != m_sFaction && GetGame().GetWorld().GetWorldTime() < m_fCounterUntil;
 }

 // Takes a squad out of every attack it is listed in (review 3 Nov: a squad the allocator moved to
 // another attack or grab stayed listed in its old one, which kept ordering it back).
 // Its strength leaves with it: the attack's start strength drops by what it took (as for a walker
 // dropped at launch), and an attack left empty ends as handed off, not lost (A/B soak 7 Nov: a
 // grab's only squad moved to the Vernon attack, and Etoupe was "called off (too many losses)" 15 s
 // later, put on the stalled cooldown and counted as a lane failure; 45 such call-offs in 2 h).
 void LeaveAttacks(ACR_Squad squad)
 {
  foreach (ACR_AIAttack attack : m_Attacks) LeaveAttack(attack, squad);
 }

 // Takes one squad out of one attack without counting it as a loss (LeaveAttacks, Forget).
 static void LeaveAttack(ACR_AIAttack attack, ACR_Squad squad)
 {
  int at = attack.m_Squads.Find(squad);
  if (at < 0) return;
  attack.m_iStartStrength = Math.Max(0, attack.m_iStartStrength - squad.FieldStrength());
  attack.Remove(at);
  if (attack.m_Squads.IsEmpty()) attack.m_bGivenAway = true;
 }

 protected int AllocIndexOf(ACR_AIJob job)
 {
  foreach (int i, ACR_AIJob each : m_aAllocJobs) if (each == job) return i;
  return -1;
 }

 // The cheap travel estimate the allocator ranks with (design: two tiers; full Reach only for
 // the squads a new attack actually takes). 'dist' is to the job's point. Beyond a walk only with a
 // free truck in fetch range ('truckNear', AllocTruckNear; 'byTruck' says it counts on one, and the
 // allocator gives each truck to one squad), as Reach. Soak 6 Nov: it counted a ride for every far
 // squad while the side had one truck, and Reach dropped the attack it planned, every 2 minutes.
 // The air lift likewise: only one TryAirLift would call for this trip ('to': where it ends, 'task':
 // the task it goes with; AirLiftCountable), and 'byAir' marks it so the allocator gives the one lift
 // to one squad. Match 6 Nov (Chernarus): every far squad counted a 240 s lift, Travel refused it,
 // squads walked 6 km and the attacks were called off "never formed up" six minutes later.
 protected float AllocEta(ACR_Squad squad, float dist, bool truckNear, vector to, int task, out bool byTruck, out bool byAir)
 {
  byTruck = false;
  byAir = false;
  if (squad.InMobileVehicle() || ACR_Transport.CrewedVehicle(squad)) return dist / MOUNTED_SPEED;
  if (dist <= WALK_REACH) return dist / WALK_SPEED;
  if (truckNear)
  {
   byTruck = true;
   return dist / RIDE_SPEED + 120;
  }
  if (AirLiftCountable(squad, to, task))
  {
   byAir = true;
   return AIR_LIFT_SECONDS;
  }
  return -1;
 }

 // 'ring' metres out of 'target' on the side facing 'from' (where an attack from there stages).
 static vector RingPoint(vector target, vector from, float ring)
 {
  vector dir = from - target;
  dir[1] = 0;
  if (ring <= 0 || dir.Length() <= ring) return target;
  dir.Normalize();
  return target + dir * ring;
 }

 // A free truck Reach would fetch for this squad on a long trip (the widest AI pickup).
 bool AllocTruckNear(ACR_Squad squad)
 {
  if (!m_Manager.m_Rides || squad.InMobileVehicle() || ACR_Transport.CrewedVehicle(squad)) return false;
  float fetch;
  return m_Manager.m_Rides.FindTransport(m_sFaction, PositionOf(squad), float.MAX, squad.CountAlive(), fetch, true, null, false, m_bHandedOnly) != null;
 }

 protected string AllocJobName(int j)
 {
  if (j < 0 || j >= m_aAllocJobs.Count()) return "nothing";
  ACR_AIJob job = m_aAllocJobs[j];
  string kind = typename.EnumToString(ACR_EAITask, job.m_iKind);
  if (job.m_iKind == ACR_EAITask.ATTACK && !job.m_bTeam) kind = "GRAB";
  return kind + " " + job.m_sName;
 }

 protected void RunAllocation(bool control)
 {
  if (AllocatorMode() < 1 || m_bHandedOnly) return;
  int started = System.GetTickCount();
  m_aAllocJobs.Clear();
  m_aAllocSquads.Clear();
  m_aAllocThink.Clear();

  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  array<ACR_Squad> pool = {};
  foreach (ACR_Squad candidate : mine)
  {
   int task = candidate.m_iAITask;
   if (task != ACR_EAITask.FREE && task != ACR_EAITask.POSTED && task != ACR_EAITask.ATTACK && task != ACR_EAITask.DEFEND) continue;
   if (task == ACR_EAITask.DEFEND && (OnRequest(candidate) || candidate == m_FobEscort || !candidate.m_AITarget)) continue;
   // Sent to defuse a charge (RespondToCharge, called once per charge): kept while it is primed, as
   // AssignDefenders does. With no known threat there is no defence job, and it was re-tasked away.
   if (task == ACR_EAITask.DEFEND && m_Manager.HasPrimedDemolition(candidate.m_AITarget)) continue;
   if (!candidate.IsCombatUnit() || IsRetreating(candidate) || IsReconTeam(candidate) || IsScreen(candidate)) continue;
   if ((task == ACR_EAITask.FREE || task == ACR_EAITask.POSTED) && AmmoShortForAttack(candidate)) continue; // rearms first (ManageAmmoRuns)
   pool.Insert(candidate);
  }
  // Squads flown to their attack (MySquads leaves air-lifted squads out): in, and locked below, or
  // their attack looks short of them and is called off while they are in the air.
  foreach (ACR_AIAttack lifted : m_Attacks)
  {
   foreach (ACR_Squad flown : lifted.m_Squads)
    if (flown && ACR_AirLift.Busy(flown) && !pool.Contains(flown) && flown.GetFaction() && flown.GetFaction().GetFactionKey() == m_sFaction) pool.Insert(flown);
  }
  if (pool.IsEmpty()) return;

  // Jobs. Running attacks and grabs first: the jobs their squads have now.
  map<ACR_Squad, ACR_AIAttack> attackOf = new map<ACR_Squad, ACR_AIAttack>();
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (!attack.m_Target) continue;
   ACR_AIJob running;
   if (attack.m_bGrab)
   {
    running = AllocJob(ACR_EAITask.ATTACK, attack.m_Target, attack.m_Target.m_sName, ALLOC_GRAB_VALUE * AllocWorth(attack.m_Target) * IncomeGrabBonus(attack.m_Target));
    running.m_fPatience = 480;
   }
   else
   {
    running = AllocAttack(attack.m_Target);
    // Judged as its own abort rule judges it (AttackBroken): against the strength it set out with,
    // worth nothing below the doctrine's abort share. Not against today's bar: Pressure() is back
    // at 1 once it has launched, so one sent at x0.87 odds would count as unfunded at once.
    ACR_AIAllocator.JudgeRunning(running, attack.m_iStartStrength, m_Doctrine.m_fAbortRatio);
   }
   running.m_bRunning = true;
   running.m_Attack = attack;
   foreach (ACR_Squad attacker : attack.m_Squads) if (attacker) attackOf.Set(attacker, attack);
  }
  // New attacks: the planner's best-scored enemy bases (cheapest x nearest).
  bool laneActive = LaneActive();
  bool laneOpen = LaneOpenToOthers();
  array<ACR_Base> targets = {};
  array<float> scores = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner == m_sFaction || !HostileOwner(base) || !Visible(base) || IsAttacking(base) || CoolingDown(base)) continue;
   if (base.m_bPocket && NearestOwnDistance(base.GetOrigin()) > POCKET_IN_THE_WAY) continue;
   // An enemy HQ waits its turn, unless it is the counterattack or the player's "take this base".
   bool priorityTarget = base == RequestedTarget() || (base == m_CounterTarget && CounterDue());
   if (!priorityTarget && !HqTargetable(base)) continue;
   // The lane, as AssignAttacks weighs it: in it sooner, its flanks later, elsewhere not while it has work.
   float laneFactor = LaneTargetFactor(base);
   if (laneFactor < 0 && !laneOpen) continue;
   float score = Math.Max(10, DefenceOf(base) * ATTACK_RATIO * m_Doctrine.m_fAttackRatio) * (NearestFreeDistance(pool, base.GetOrigin()) / 1000 + 0.1);
   if (laneFactor > 0) score *= laneFactor;
   score /= EnvelopBonus(base) * IncomeGrabBonus(base); // going round a strongpoint, cheap income (ACR_AIEnvelopment, ACR_AIIncome)
   int at = 0;
   while (at < scores.Count() && scores[at] <= score) at++;
   targets.InsertAt(base, at);
   scores.InsertAt(score, at);
  }
  if (CounterDue() && targets.Contains(m_CounterTarget))
  {
   targets.RemoveItem(m_CounterTarget);
   targets.InsertAt(m_CounterTarget, 0);
  }
  for (int t = 0; t < targets.Count() && t < ALLOC_ATTACK_TARGETS; t++) AllocAttack(targets[t]);
  // Grabs: the nearest empty bases in sight.
  int rearGrabs = 0;
  foreach (ACR_AIAttack rear : m_Attacks)
   if (laneActive && rear.m_bGrab && !InLaneOrFlank(rear.m_Target)) rearGrabs++;
  array<ACR_Base> empties = {};
  array<float> gaps = {};
  foreach (ACR_Base open : ACR_Base.s_All)
  {
   if (!open || open.m_bRetired || !SeenOwner(open).IsEmpty() || !Visible(open) || IsAttacking(open) || CoolingDown(open)) continue;
   // With a lane: grabs in it and on its flanks, and one at a time close behind our own (as AssignAttacks).
   if (laneActive && !InLaneOrFlank(open) && !OffLaneGrabAllowed(open, rearGrabs)) continue;
   if (laneActive && !InLaneOrFlank(open)) rearGrabs++;
   float gap = NearestFreeDistance(pool, open.GetOrigin()) / IncomeGrabBonus(open); // income first while short of it
   int place = 0;
   while (place < gaps.Count() && gaps[place] <= gap) place++;
   empties.InsertAt(open, place);
   gaps.InsertAt(gap, place);
  }
  // As many grabs as AssignAttacks allows (one per 6 empty bases in sight, 3 to 8), running ones included.
  int runningGrabs;
  foreach (ACR_AIAttack counted : m_Attacks) if (counted.m_bGrab) runningGrabs++;
  int maxGrabs = Math.Clamp(empties.Count() / 6, Math.Max(1, MAX_GRABS + m_Doctrine.m_iGrabs), Math.Max(1, MAX_GRABS_BIG + m_Doctrine.m_iGrabs));
  int newGrabs = Math.Min(ALLOC_GRAB_TARGETS, maxGrabs - runningGrabs);
  for (int g = 0; g < empties.Count() && g < newGrabs; g++)
  {
   ACR_AIJob grab = AllocJob(ACR_EAITask.ATTACK, empties[g], empties[g].m_sName, ALLOC_GRAB_VALUE * AllocWorth(empties[g]) * IncomeGrabBonus(empties[g]));
   grab.m_fPatience = 480;
  }
  // Defence: our bases under threat, not given up (threat past CONCEDE_RATIO x defence).
  foreach (ACR_Base held : ACR_Base.s_All)
  {
   if (!held || held.m_bRetired || held.m_sOwner != m_sFaction) continue;
   int threat = DefenceThreat(held);
   int defence = DefenceOf(held);
   // Given up only for the enemies there now: a forecast on a thinly held base is the time to reinforce it.
   if (threat <= 0 || (defence > 0 && ThreatTo(held) > defence * CONCEDE_RATIO * m_Doctrine.m_fConcedeRatio)) continue;
   ACR_AIJob defend = AllocJob(ACR_EAITask.DEFEND, held, held.m_sName, ALLOC_DEFEND_VALUE * AllocWorth(held));
   defend.m_fNeed = threat * DEFENSE_RATIO * m_Doctrine.m_fDefenseRatio;
   defend.m_iMaxSquads = 4;
   defend.m_fPatience = 240;
  }
  // Holding: the front line, and the reserve.
  array<ACR_Base> front = {};
  FrontlineBases(front);
  RearOut(front);
  foreach (ACR_Base post : front)
  {
   // A fallback for squads with nothing better: barely more for a richer base (Arland control
   // run 3 Nov: at 2x worth, holding the HQ beat every grab of the opening).
   ACR_AIJob hold = AllocJob(ACR_EAITask.POSTED, post, post.m_sName, ALLOC_HOLD_VALUE + 0.3 * AllocWorth(post));
   hold.m_iMaxSquads = 2;
   hold.m_fPatience = 1200;
  }
  // The reserve, when AssignAttacks would keep one (force big enough, two free, not after a quiet spell).
  ACR_Base reserveBase;
  int minAllForReserve = 5;
  if (ACR_Manager.ThreatOption() == 0) minAllForReserve = 6;
  else if (ACR_Manager.ThreatOption() == 2) minAllForReserve = 4;
  minAllForReserve = (int)Math.Max(3, minAllForReserve + m_Doctrine.m_iReserveSquads);
  array<ACR_Squad> freeNow = {};
  MySquads(freeNow, true);
  if (freeNow.Count() >= 2 && (KeepsReserve() || (mine.Count() >= minAllForReserve && Pressure() >= 0.9))) reserveBase = ReserveBase();
  int reserveJob = -1;
  if (reserveBase)
  {
   ACR_AIJob reserve = AllocJob(ACR_EAITask.POSTED, reserveBase, "reserve at " + reserveBase.m_sName, AllocReserveValue(reserveBase));
   reserve.m_fPatience = 1200;
   reserveJob = m_aAllocJobs.Count() - 1;
  }

  // Defences a committed assault squad may still be pulled to (AttackerMayDefend): real threats only.
  // Also precompute fog-of-war enemy armor presence per job so AT/armor squads get a capability fit bonus.
  array<bool> realThreat = {};
  array<bool> jobEnemyArmor = {};
  foreach (ACR_AIJob defJob : m_aAllocJobs)
  {
   bool pullable = true;
   if (defJob.m_iKind == ACR_EAITask.DEFEND) pullable = AttackerMayDefend(ThreatTo(defJob.m_Target), DefenceOf(defJob.m_Target), defJob.m_Target.m_bBuilt || IsHQ(defJob.m_Target));
   realThreat.Insert(pullable);
   bool hasArmor = false;
   if (defJob.m_Target && (defJob.m_iKind == ACR_EAITask.ATTACK || defJob.m_iKind == ACR_EAITask.DEFEND))
    hasArmor = ACR_AIIntel.EnemyArmorNear(this, defJob.m_Target.GetOrigin(), THREAT_DISTANCE + defJob.m_Target.m_fRadius + 150);
   jobEnemyArmor.Insert(hasArmor);
  }
  int keptOnAssault;
  bool night = ACR_Raid.IsNight();

  // Squads: strength, what they do now, a cheap estimate and fit for every job.
  foreach (ACR_Squad squad : pool)
  {
   ACR_AIJobSquad entry = new ACR_AIJobSquad();
   entry.m_Squad = squad;
   entry.m_sName = squad.GetName();
   entry.m_fStrength = squad.Strength();
   entry.m_bTaker = CanTakeBase(squad, false);
   vector pos = PositionOf(squad);
   ACR_AIAttack ownAttack;
   attackOf.Find(squad, ownAttack);
   bool support = IsSupportTeam(squad);
   bool atCapable = ACR_AIRequisition.IsATSquad(squad);
   bool truckNear = AllocTruckNear(squad);
   bool heldByAssault = HeldByAssault(squad, ownAttack, pos);
   bool keptHere = false;
   foreach (int j, ACR_AIJob job : m_aAllocJobs)
   {
    ACR_Base where = job.m_Target;
    float dist = Math.Max(0, vector.DistanceXZ(pos, where.GetOrigin()) - where.m_fRadius);
    if (job.m_bTeam) dist = Math.Max(0, dist - STAGING_DISTANCE);
    float fit = 1;
    if (job.m_iKind == ACR_EAITask.ATTACK && support) fit = 0;
    if (job.m_iKind == ACR_EAITask.ATTACK && !job.m_bTeam && job.m_bRunning && ownAttack != job.m_Attack) fit = 0;
    // A grab as AssignAttacks sends them: on foot within GRAB_WALK, farther by a free truck (GrabTime).
    bool grabByTruck = false;
    if (fit > 0 && job.m_iKind == ACR_EAITask.ATTACK && !job.m_bTeam && !job.m_bRunning && GrabTimeOf(squad, where.GetOrigin(), float.MAX, grabByTruck) < 0) fit = 0;
    if (job.m_iKind == ACR_EAITask.ATTACK && !job.m_bTeam && !job.m_bRunning && GrabBarred(squad, where)) fit = 0; // stalled there lately
    if (job.m_iKind == ACR_EAITask.DEFEND && support) fit = 0.3;
    if (fit > 0 && atCapable && jobEnemyArmor[j]) fit *= 1.3;
    if (fit > 0 && night && job.m_iKind == ACR_EAITask.ATTACK && job.m_bTeam)
    {
     if (ACR_Transport.CrewedVehicle(squad)) fit *= 0.85;
     else fit *= 1.15;
    }
    // Committed to an assault: a defence only for a real threat there, never for a forecast alone.
    if (job.m_iKind == ACR_EAITask.DEFEND && heldByAssault && !realThreat[j])
    {
     fit = 0;
     keptHere = true;
    }
    // A posted squad is only taken for real work, never walked to another post (as PostIdle leaves
    // it). Soak 4 Nov: with a front's hold slots shuffled, squads walked 2-3 km between two posts
    // worth 2 and back 30 s later.
    bool holdJob = job.m_iKind == ACR_EAITask.POSTED && j != reserveJob;
    if (holdJob && squad.m_iAITask == ACR_EAITask.POSTED && squad != m_ReserveSquad && vector.DistanceXZ(squad.m_MoveTarget, where.GetOrigin()) > where.m_fRadius + DEFENSE_DISTANCE) fit = 0;
    // COIN: the reaction force only reacts (a defence) or stays the reserve; no grabs (KeepsReserve).
    if (KeepsReserve() && squad == m_ReserveSquad && job.m_iKind != ACR_EAITask.DEFEND && j != reserveJob) fit = 0;
    bool byTruck, byAir;
    vector endsAt = where.GetOrigin();
    if (job.m_bTeam) endsAt = RingPoint(endsAt, pos, AttackRing(where));
    float eta = AllocEta(squad, dist, truckNear, endsAt, job.m_iKind, byTruck, byAir);
    if (grabByTruck && fit > 0)
    {
     eta = dist / RIDE_SPEED + 120; // as AllocEta's ride, short of its walk
     byTruck = true;
     byAir = false;
    }
    // Its own attack: the way to its staging point, or none once going in (its ride, if any, is under way).
    if (ownAttack && job.m_bRunning && ownAttack.m_Target == where)
    {
     eta = 0;
     bool ownRide, ownLift;
     if (!ownAttack.m_bAssaulting && job.m_bTeam) eta = AllocEta(squad, vector.DistanceXZ(pos, ownAttack.StagingOf(squad)), truckNear, ownAttack.StagingOf(squad), ACR_EAITask.ATTACK, ownRide, ownLift);
     if (eta < 0) eta = 0;
     byTruck = false;
     byAir = false;
    }
    entry.m_aEta.Insert(eta);
    entry.m_aByTruck.Insert(byTruck && fit > 0);
    entry.m_aByAir.Insert(byAir && fit > 0);
    entry.m_aFit.Insert(fit);
    // What it has now.
    if (entry.m_iCurrent >= 0) continue;
    int has = squad.m_iAITask;
    if (has == ACR_EAITask.ATTACK && job.m_iKind == ACR_EAITask.ATTACK && job.m_bRunning && squad.m_AITarget == where) entry.m_iCurrent = j;
    else if (has == ACR_EAITask.DEFEND && job.m_iKind == ACR_EAITask.DEFEND && squad.m_AITarget == where) entry.m_iCurrent = j;
    else if (has == ACR_EAITask.POSTED && squad == m_ReserveSquad && j == reserveJob) entry.m_iCurrent = j;
    else if (has == ACR_EAITask.POSTED && squad != m_ReserveSquad && job.m_iKind == ACR_EAITask.POSTED && j != reserveJob && vector.DistanceXZ(squad.m_MoveTarget, where.GetOrigin()) <= where.m_fRadius + DEFENSE_DISTANCE) entry.m_iCurrent = j;
   }
   if (keptHere) keptOnAssault++;
   // Locked: going in under fire, in a sapper raid with its charge under way, or on its way to a
   // posting (see the trips note; one bound nowhere the allocator knows stays out of it). In
   // control, defence stays AssignDefenders' (it runs first): the two never pull a defender to and fro.
   if (ownAttack && (ownAttack.m_bPrep || (ownAttack.m_bAssaulting && squad.KnownEnemies(300) > 0))) entry.m_bLocked = true;
   if (ownAttack && ownAttack.m_SapperDemo && ownAttack.m_SapperDemo.InProgress()) entry.m_bLocked = true;
   if (ACR_AirLift.Busy(squad)) entry.m_bLocked = true;
   // A fresh attack keeps its squads for a grace period: it only ends by its own rules (losses,
   // stall, won). Soak 4 Nov: a base won in the launch tick added a reserve and posts next tick,
   // and most allocator call-offs came 15-30 s after launch.
   if (ownAttack && !ownAttack.m_bGrab && entry.m_iCurrent >= 0 && GetGame().GetWorld().GetWorldTime() - ownAttack.m_fLaunchedAt < ALLOC_GRACE * 1000) entry.m_bLocked = true;
   if (control && squad.m_iAITask == ACR_EAITask.DEFEND && entry.m_iCurrent >= 0) entry.m_bLocked = true;
   if (squad.m_iAITask == ACR_EAITask.POSTED && Committed(squad))
   {
    if (entry.m_iCurrent < 0) continue;
    entry.m_bLocked = true;
   }
   // A crew getting back into its vehicle (Recrew) keeps the job it has, and gets none while out of
   // it: its orders go by vehicle once aboard.
   if (Remounting(squad))
   {
    if (entry.m_iCurrent < 0) continue;
    entry.m_bLocked = true;
   }
   m_aAllocSquads.Insert(entry);
  }

  int maxTeams = 1;
  ACR_FactionSettings settings = Settings();
  if (settings) maxTeams = settings.m_iAIAttacks;
  int extra = m_Doctrine.m_iExtraFronts;
  if (ACR_Base.s_All.Count() > 24) extra = extra * 2;
  if (laneActive) extra = m_Doctrine.m_iLaneFronts; // a lane concentrates the force (AssignAttacks)
  maxTeams += Math.Min(extra, freeNow.Count() / Math.Max(1, m_Doctrine.m_iSquadsPerFront)); // free squads, as AssignAttacks counts
  // The free trucks, one squad each (AllocEta only counts on one in fetch range; this says how many).
  int trucks = -1;
  if (m_Manager.m_Rides) trucks = m_Manager.m_Rides.CountIdleTransports(m_sFaction, m_bHandedOnly);
  // One air lift a plan, as Reach claims it (AllocEta marks only the squads it would fly).
  ACR_AIAllocator.Allocate(m_aAllocJobs, m_aAllocSquads, maxTeams, trucks, 1);
  // In control the lane learns from the allocation: an attack in it funded (running or new) keeps
  // it fed; only unfunded ones in it count toward a starved lane (as AssignAttacks' musters).
  if (control && laneActive)
  {
   bool laneFed, laneWanted;
   foreach (ACR_AIJob laneJob : m_aAllocJobs)
   {
    if (!laneJob.m_bTeam || !InLaneOrFlank(laneJob.m_Target)) continue;
    if (laneJob.m_fScore > 0) laneFed = true;
    else laneWanted = true;
   }
   if (laneFed) NoteLaneMuster(true);
   else if (laneWanted) NoteLaneMuster(false);
  }

  // Two tiers: the estimates ranked the squads; Reach checks the new attacks' picks for real
  // (taxis and the air lift claimed in order), and says how often the estimate was wrong.
  int reachCalls, reachWrong;
  foreach (int k, ACR_AIJob funded : m_aAllocJobs)
  {
   if (control) break; // in control the launch checks them (ApplyAllocation)
   if (!funded.m_bTeam || funded.m_bRunning || funded.m_aMembers.IsEmpty()) continue;
   array<ACR_Squad> taxis = {};
   bool liftTaken = false;
   foreach (int n, int memberIndex : funded.m_aMembers)
   {
    if (n >= ALLOC_FULL_REACH) break;
    float seconds;
    int how = Reach(m_aAllocSquads[memberIndex].m_Squad, funded.m_Target.GetOrigin(), taxis, liftTaken, seconds, AttackRing(funded.m_Target));
    reachCalls++;
    if (how == REACH_AIR_LIFT) liftTaken = true;
    if (how == REACH_NONE) reachWrong++;
   }
  }

  // Compare with what the managers did.
  int agree, differ, logged;
  foreach (ACR_AIJobSquad picked : m_aAllocSquads)
  {
   if (picked.m_iPicked == picked.m_iCurrent)
   {
    agree++;
    continue;
   }
   differ++;
   string line = string.Format("%1: %2 -> %3 (worth %4 there, %5 on its own)", picked.m_sName, AllocJobName(picked.m_iCurrent), AllocJobName(picked.m_iPicked), (int)(picked.m_fScore * 10) / 10.0, (int)(picked.m_fAlt * 10) / 10.0);
   m_aAllocThink.Insert("  would move " + line);
   if (logged < ALLOC_LOG_LINES)
   {
    logged++;
    PrintFormat("[ACR] ALLOC %1: would move %2", m_sFaction, line);
   }
  }
  m_iAllocAgree += agree;
  m_iAllocDiffer += differ;
  m_iAllocTicks++;
  int teams;
  foreach (ACR_AIJob teamJob : m_aAllocJobs)
  {
   if (teamJob.m_aMembers.IsEmpty()) continue;
   if (teamJob.m_bTeam) teams++;
   string names;
   foreach (int member2 : teamJob.m_aMembers) names += " " + m_aAllocSquads[member2].m_sName;
   string strength = "";
   if (teamJob.m_fNeed > 0) strength = string.Format(" %1/%2", (int)teamJob.m_fStrength, (int)teamJob.m_fNeed);
   m_aAllocThink.InsertAt(string.Format("  %1 worth %2%3, latest %4 s:%5", AllocJobName(AllocIndexOf(teamJob)), (int)teamJob.m_fScore, strength, (int)teamJob.m_fLatest, names), 0);
  }
  int ms = System.GetTickCount() - started;
  string summary = string.Format("ALLOC %1: %2 squads, %3 jobs, agree %4, differ %5, teams %6/%7, reach %8", m_sFaction, m_aAllocSquads.Count(), m_aAllocJobs.Count(), agree, differ, teams, maxTeams, reachCalls);
  summary += string.Format(" (%1 wrong), %2 ms | match agree %3 differ %4 over %5 ticks", reachWrong, ms, m_iAllocAgree, m_iAllocDiffer, m_iAllocTicks);
  PrintFormat("[ACR] %1", summary);
  string role = "observing";
  if (control) role = "in control";
  m_aAllocThink.InsertAt(string.Format("Allocator (%1): %2", role, summary.Substring(6, summary.Length() - 6)), 0);
  if (keptOnAssault > 0) m_aAllocThink.Insert(string.Format("  %1 assault squad(s) kept on their attack: no defence offered without a real threat there", keptOnAssault));
  foreach (ACR_AIJob roundJob : m_aAllocJobs)
  {
   if (!roundJob.m_bTeam || !EnvelopOf(roundJob.m_Target)) continue;
   m_aAllocThink.Insert(string.Format("  %1: %2", roundJob.m_sName, EnvelopText(roundJob.m_Target)));
  }
  // Once a minute the whole picture, for reading soaks afterwards.
  if (m_iAllocTicks % 4 == 1)
  {
   foreach (string jobLine : m_aAllocThink) PrintFormat("[ACR] ALLOC %1 |%2", m_sFaction, jobLine);
  }
  if (control) ApplyAllocation();
 }

 void ThinkAllocator(notnull array<string> lines) { lines.Copy(m_aAllocThink); }

 // -acrAIAllocator 2 (allocator phase 2): carries the allocation out through the managers' own
 // paths (LaunchAttack, JoinAttack, StartGrab, PostTo, HoldReserve). Locked squads and squads whose
 // job didn't change are left alone; a squad moved off an attack leaves it at the next
 // UpdateAttacks (its task is no longer ATTACK).
 protected void ApplyAllocation()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  // Running attacks it didn't fund: called off, their squads freed for the jobs it gave them.
  foreach (int u, ACR_AIJob unfunded : m_aAllocJobs)
  {
   if (!unfunded.m_bTeam || !unfunded.m_bRunning || !unfunded.m_Attack || !unfunded.m_aMembers.IsEmpty()) continue;
   ACR_AIAttack dropped = unfunded.m_Attack;
   if (dropped.m_SapperDemo && dropped.m_SapperDemo.InProgress()) continue; // a charge is fused: UpdateAttacks sees it through
   // Why, for reading soaks: what its own squads made it worth against what they'd do elsewhere.
   array<int> own = {};
   float elsewhere, ownStrength, ownLatest;
   string went;
   foreach (int o, ACR_AIJobSquad member : m_aAllocSquads)
   {
    if (member.m_iCurrent != u) continue;
    own.Insert(o);
    elsewhere += member.m_fAlt;
    went += string.Format(" %1 -> %2 (%3);", member.m_sName, AllocJobName(member.m_iPicked), (int)(member.m_fScore * 10) / 10.0);
   }
   float ownValue = ACR_AIAllocator.TeamValue(unfunded, u, m_aAllocSquads, own, ownStrength, ownLatest);
   Say(string.Format("[ACR] AI %1: attack on %2 called off (its squads are worth more elsewhere: %3 here, strength %4/%5, latest %6 s; %7 elsewhere)", m_sFaction, unfunded.m_sName, (int)(ownValue * 10) / 10.0, (int)ownStrength, (int)(unfunded.m_fNeed * unfunded.m_fBar), (int)ownLatest, (int)(elsewhere * 10) / 10.0));
   PrintFormat("[ACR] ALLOC %1: %2 call-off, its squads went:%3", m_sFaction, unfunded.m_sName, went);
   foreach (ACR_Squad left : dropped.m_Squads)
    if (left && left.m_iAITask == ACR_EAITask.ATTACK) left.SetAITask(ACR_EAITask.FREE);
   m_StalledUntil.Set(dropped.m_Target, now + ALLOC_DROP_COOLDOWN * 1000);
   m_Attacks.RemoveItem(dropped);
  }
  // One-squad jobs.
  foreach (ACR_AIJobSquad entry : m_aAllocSquads)
  {
   ACR_Squad squad = entry.m_Squad;
   if (!squad || entry.m_bLocked || entry.m_iPicked == entry.m_iCurrent) continue;
   if (entry.m_iPicked < 0)
   {
    // Nothing worth doing: off its attack or defence; it stays where it is.
    if (squad.m_iAITask == ACR_EAITask.ATTACK || squad.m_iAITask == ACR_EAITask.DEFEND) squad.SetAITask(ACR_EAITask.FREE);
    continue;
   }
   ACR_AIJob job = m_aAllocJobs[entry.m_iPicked];
   if (job.m_bTeam) continue; // attacks below, as a whole
   if (job.m_iKind == ACR_EAITask.ATTACK && job.m_bRunning) continue; // a running grab keeps its runner
   if (squad == m_ReserveSquad) m_ReserveSquad = null;
   string from = AllocJobName(entry.m_iCurrent);
   LeaveAttacks(squad);
   if (job.m_iKind == ACR_EAITask.ATTACK)
   {
    StartGrab(squad, job.m_Target);
    Say(string.Format("[ACR] AI %1: %2 grabs empty %3 (allocator; was %4)", m_sFaction, squad.GetName(), job.m_sName, from));
   }
   else if (job.m_iKind == ACR_EAITask.DEFEND)
   {
    squad.SetMode(ACR_EMoveMode.FAST);
    Travel(squad, job.m_Target.GetOrigin(), true, 0);
    squad.SetAITask(ACR_EAITask.DEFEND);
    squad.m_AITarget = job.m_Target;
    Say(string.Format("[ACR] AI %1: %2 sent to defend %3 (allocator; was %4)", m_sFaction, squad.GetName(), job.m_sName, from));
   }
   else if (job.m_iKind == ACR_EAITask.POSTED && job.m_sName.StartsWith("reserve at ")) HoldReserve(squad, job.m_Target);
   else if (job.m_iKind == ACR_EAITask.POSTED) PostTo(squad, job.m_Target);
  }
  // Attacks, as a whole: squads joining one under way, or a new one launched.
  foreach (int j, ACR_AIJob team : m_aAllocJobs)
  {
   if (!team.m_bTeam || team.m_aMembers.IsEmpty()) continue;
   if (team.m_bRunning && team.m_Attack)
   {
    foreach (int joinAt : team.m_aMembers)
    {
     ACR_AIJobSquad joiner = m_aAllocSquads[joinAt];
     if (joiner.m_iCurrent == j || !joiner.m_Squad) continue;
     if (joiner.m_Squad == m_ReserveSquad) m_ReserveSquad = null;
     LeaveAttacks(joiner.m_Squad);
     vector stage = JoinAttack(team.m_Attack, joiner.m_Squad, now);
     // Counted in what it set out with, as LeaveAttacks takes it off again if it moves on.
     team.m_Attack.m_iStartStrength += joiner.m_Squad.FieldStrength();
     Say(string.Format("[ACR] AI %1: %2 joins the attack on %3 (allocator), staging at grid %4", m_sFaction, joiner.m_Squad.GetName(), team.m_sName, ACR_Manager.Grid(stage)));
    }
    continue;
   }
   // New: the allocator's squads, each checked with Reach (taxis and the air lift claimed in order).
   array<ACR_Squad> taxis = {};
   bool liftTaken = false;
   int strength;
   bool taker = team.m_Target.m_sOwner.IsEmpty();
   float slowest;
   array<ACR_Squad> goers = {};
   foreach (int goAt : team.m_aMembers)
   {
    ACR_Squad goer = m_aAllocSquads[goAt].m_Squad;
    if (!goer) continue;
    float travel;
    int how = Reach(goer, team.m_Target.GetOrigin(), taxis, liftTaken, travel, AttackRing(team.m_Target));
    if (how == REACH_NONE) continue;
    if (how == REACH_AIR_LIFT) liftTaken = true;
    slowest = Math.Max(slowest, travel);
    goers.Insert(goer);
    strength += goer.Strength();
    if (CanTakeBase(goer, team.m_Target.m_bBuilt)) taker = true;
   }
   float required = team.m_fNeed * team.m_fBar;
   if (goers.IsEmpty() || required > strength || !taker)
   {
    // The estimate was too kind: not weighed again for a while, or the same team wins every tick.
    Say(string.Format("[ACR] AI %1: not attacking %2: of the allocator's squads only %3 strength can get there (%4 wanted)", m_sFaction, team.m_sName, strength, (int)required));
    m_StalledUntil.Set(team.m_Target, now + ALLOC_DROP_COOLDOWN * 1000);
    continue;
   }
   ACR_AIAttack attack = NewAttack(team.m_Target, team.m_Target == m_CounterTarget && CounterDue());
   attack.m_fTravelSeconds = slowest;
   foreach (ACR_Squad launched : goers)
   {
    if (launched == m_ReserveSquad) m_ReserveSquad = null;
    LeaveAttacks(launched);
    attack.m_Squads.Insert(launched);
   }
   Say(string.Format("[ACR] AI %1: the allocator launches an attack on %2 (worth %3, latest arrival %4 s)", m_sFaction, team.m_sName, (int)team.m_fScore, (int)team.m_fLatest));
   LaunchAttack(attack, PositionOf(attack.m_Squads[0]), strength, required, Pressure());
  }
 }

 // ---------------------------------------------------------------- doctrine reviews
 // Doctrines can change (Jacob, 24 Oct). Every DOCTRINE_REVIEW_MIN minutes, the first after
 // DOCTRINE_FIRST_REVIEW_MIN, the commander reads its own side's report (ACR_Report: bases won
 // and lost, soldiers lost and enemies killed since the last review) and changes doctrine when
 // the current one isn't working. With the enemy's doctrine worked out from intel
 // (ACR_DoctrineIntel: the same picture its players get, never the truth) it picks the counter.
 // At most one change per review; -acrDoctrineAdapt 0 keeps the first doctrine.
 static const float DOCTRINE_FIRST_REVIEW_MIN = 15;
 static const float DOCTRINE_REVIEW_MIN = 10;
 float m_fNextDoctrineReview = -1;
 int m_iReviewLost, m_iReviewKills, m_iReviewWon, m_iReviewBasesLost;
 int m_iDoctrineChanges;
 bool m_bDoctrineSettling; // changed at the last review: the next one only snapshots
 static const float DOCTRINE_HOLD_MIN = 30;
 float m_fDoctrineSince = -1;
 string m_sDoctrineLeft, m_sDoctrineLeftWhy;
 string m_sDoctrinePending;
 static const int DOCTRINE_AHEAD_BY = 5;
 static const string WHY_HOME = "the trouble is over, back to its home style";
 static const float HOME_DRIFT_EXTRA_MIN = 10;

 // Doctrine review and switching live in ACR_AIStrategy.c; thin forwarders preserve existing callers/tests.
 protected void SnapshotReview() { ACR_AIStrategy.SnapshotReview(this); }
 protected void ReviewDoctrine() { ACR_AIStrategy.ReviewDoctrine(this); }
 static bool NeedsConfirming(string why) { return ACR_AIStrategy.NeedsConfirming(why); }
 static bool Ahead(int ours, int theirs) { return ACR_AIStrategy.Ahead(ours, theirs); }
 protected void BasesHeld(out int ours, out int theirs) { ACR_AIStrategy.BasesHeld(this, ours, theirs); }
 static string DoctrineReasonKind(string why) { return ACR_AIStrategy.DoctrineReasonKind(why); }
 static string DoctrineChangeBlocked(string pick, string why, float heldMin, string left, string leftWhy) { return ACR_AIStrategy.DoctrineChangeBlocked(pick, why, heldMin, left, leftWhy); }
 float HomeDriftMin() { return ACR_AIStrategy.HomeDriftMin(this); }
 static string HomeDriftBlocked(float heldMin, float waitMin) { return ACR_AIStrategy.HomeDriftBlocked(heldMin, waitMin); }
 static string PickDoctrine(string current, string enemyKnown, int won, int basesLost, int lost, int kills, out string why, string home = "", bool ahead = false) { return ACR_AIStrategy.PickDoctrine(current, enemyKnown, won, basesLost, lost, kills, why, home, ahead); }
 void ChangeDoctrine(ACR_AIDoctrine doctrine, string why) { ACR_AIStrategy.ChangeDoctrine(this, doctrine, why); }

 protected void TickSapperDemos()
 {
  if (m_Attacks.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (attack && attack.m_bAssaulting && attack.m_bFobRaid && attack.m_SapperDemo)
   {
    attack.m_SapperDemo.Tick(now);
   }
  }
 }

 ACR_FactionSettings Settings() { return ACR_FactionSettings.Find(m_sFaction); }

 // ---------------------------------------------------------------- helpers

 protected bool IsRetreating(ACR_Squad squad)
 {
  float until;
  if (!m_RetreatingUntil.Find(squad, until)) return false;
  if (GetGame().GetWorld().GetWorldTime() >= until)
  {
   m_RetreatingUntil.Remove(squad);
   return false;
  }
  ACR_Base safe = NearestOwnBase(PositionOf(squad));
  if (safe && ThreatTo(safe) == 0 && vector.DistanceXZ(PositionOf(squad), safe.GetOrigin()) <= safe.m_fRadius && squad.KnownEnemies(300) == 0 && squad.StrengthRatio() >= RetreatRatio() && (squad.m_iAmmoPct < 0 || squad.m_iAmmoPct >= OUT_OF_AMMO))
  {
   m_RetreatingUntil.Remove(squad);
   return false;
  }
  return true;
 }

 // committedToo: with the posted squads still on their way somewhere (Committed), for a base under threat.
 void MySquads(array<ACR_Squad> result, bool freeOnly, bool committedToo = false)
 {
  result.Clear();
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (m_bHandedOnly && !squad.m_bAICommanded) continue; // a player commands the side: only squads handed to us
   if (squad.PlayerSteers() || !squad.GetAnchor()) continue; // players lead themselves
   if (squad.m_bMortar) continue; // mortar teams: PositionMortars
   if (squad.Aircraft()) continue; // helicopter crews: AI can't fly
   if (ACR_AirLift.Busy(squad)) continue; // flown somewhere: leave it until it has landed
   if (freeOnly && squad.m_iAITask != ACR_EAITask.FREE && squad.m_iAITask != ACR_EAITask.POSTED) continue;
   if (freeOnly && !squad.IsCombatUnit()) continue; // lone drivers, unarmed crews: kept out of the fighting
   if (freeOnly && IsRetreating(squad)) continue; // just broke contact: let it fall back instead of re-tasking at once
   if (freeOnly && IsScreen(squad)) continue; // out front watching a way in: ManageScreens
   if (freeOnly && !committedToo && Committed(squad)) continue; // on its way to a posting: see the trips note
   if (freeOnly && Remounting(squad)) continue; // a crew getting back into its vehicle: Recrew
   result.Insert(squad);
  }
 }

 vector PositionOf(ACR_Squad squad)
 {
  IEntity anchor = squad.GetAnchor();
  if (anchor) return anchor.GetOrigin();
  return vector.Zero;
 }

 void SortByDistance(array<ACR_Squad> squads, vector point)
 {
  // simple insertion sort; squad counts are small
  for (int i = 1; i < squads.Count(); i++)
  {
   ACR_Squad current = squads[i];
   float d = vector.DistanceXZ(PositionOf(current), point);
   int j = i - 1;
   while (j >= 0 && vector.DistanceXZ(PositionOf(squads[j]), point) > d)
   {
    squads[j + 1] = squads[j];
    j--;
   }
   squads[j + 1] = current;
  }
 }

 // COIN (#244 item 5): the army always keeps a reaction force once it has two free squads; the
 // insurgency gives it few fights, so the pressure rule would let every squad go grabbing.
 bool KeepsReserve()
 {
  return ACR_TownSupport.On() && ACR_Coin.IsArmy(m_sFaction);
 }

 bool CoolingDown(ACR_Base base)
 {
  float until;
  return m_StalledUntil.Find(base, until) && GetGame().GetWorld().GetWorldTime() < until;
 }

 bool Visible(ACR_Base base)
 {
  return base.IsVisibleTo(m_sFaction);
 }

 // The owner as this commander knows it: an unfound FIA pocket looks empty (ACR_Pockets).
 string SeenOwner(ACR_Base base)
 {
  return base.OwnerSeenBy(m_sFaction);
 }

 // Held, as this commander knows it, by someone it fights: an FIA pocket of an FIA allied to
 // this side is not (ACR_Insurgency).
 bool HostileOwner(ACR_Base base)
 {
  string seen = SeenOwner(base);
  return !seen.IsEmpty() && !ACR_Insurgency.Friendly(seen, m_sFaction);
 }

 bool HasKnownEnemyBase()
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (HostileOwner(base) && Visible(base)) return true;
  return false;
 }

 bool FriendlyBaseThreatened()
 {
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner == m_sFaction && ThreatTo(base) > DefenceOf(base)) return true;
  return false;
 }

 ACR_Base NearestEnemyBase(vector point, ACR_Base except = null)
 {
  ACR_Base nearest;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == m_sFaction || !HostileOwner(base) || !Visible(base) || base == except) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), point);
   if (d < best) { best = d; nearest = base; }
  }
  return nearest;
 }

 static bool IsReconTeam(ACR_Squad squad)
 {
  if (!squad) return false;
  string name = squad.GetName();
  name.ToLower();
  return name.Contains("recon") || name.Contains("scout") || name.Contains("sniper") || name.Contains("observer") || name.Contains("spotter");
 }

 // Soldiers who can walk in and take a base: alive and on foot or riding as passengers
 // (not a vehicle's driver or gunner).
 static int Dismounts(ACR_Squad squad)
 {
  if (!squad) return 0;
  array<AIAgent> agents = {};
  squad.GetAgents(agents);
  int count = 0;
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   if (ACR_Transport.VehicleOf(body) && !ACR_Transport.Passenger(body)) continue;
   count++;
  }
  return count;
 }

 // A squad that can take a held base: fighting infantry, not recon, support, a crew without
 // dismounts, aircraft or mortar (sappers count on a FOB raid). Everon 26 Oct: a counterattack
 // on Meaux was one recon BRDM from 2.5 km out; its infantry partner died on the way, and the
 // BRDM overwatched 400 m short for 14 min until the attack stalled out.
 static bool CanTakeBase(ACR_Squad squad, bool fobRaid)
 {
  if (!squad || squad.Aircraft() || squad.m_bMortar || Dismounts(squad) == 0) return false;
  if (fobRaid && ACR_SapperDemo.IsSapperSquad(squad)) return true;
  return !IsReconTeam(squad) && !IsSupportTeam(squad);
 }

 static int TakersIn(ACR_AIAttack attack, ACR_Squad except = null)
 {
  int count = 0;
  foreach (ACR_Squad squad : attack.m_Squads)
   if (squad && squad != except && CanTakeBase(squad, attack.m_bFobRaid)) count++;
  return count;
 }

 // Own bases nearest to anything not ours: where fights happen.
 protected void FrontlineBases(array<ACR_Base> result)
 {
  result.Clear();
  array<ACR_Base> mine = {};
  array<ACR_Base> others = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == m_sFaction) mine.Insert(base);
   else if (Visible(base)) others.Insert(base);
  }
  if (others.IsEmpty()) { result.Copy(mine); return; }
  array<float> nearest = {};
  foreach (ACR_Base base : mine)
  {
   float gap = float.MAX;
   foreach (ACR_Base other : others) gap = Math.Min(gap, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   nearest.Insert(gap);
  }
  array<int> picked = {};
  FrontlinePick(nearest, picked);
  foreach (int i : picked) result.Insert(mine[i]);
 }

 static const float FRONTLINE_SLACK = 150; // m: bases this much farther than the closest still count

 // The indexes of every base within FRONTLINE_SLACK of the smallest gap. Two passes, so
 // the answer doesn't depend on the order the bases come in (R07, 5 Oct review: the old
 // single pass dropped bases seen before a closer one).
 static void FrontlinePick(notnull array<float> nearest, notnull array<int> result)
 {
  result.Clear();
  float best = float.MAX;
  foreach (float gap : nearest) best = Math.Min(best, gap);
  foreach (int i, float gap : nearest) if (gap <= best + FRONTLINE_SLACK) result.Insert(i);
 }

 // ---------------------------------------------------------------- lane (axis of advance, ACR_AILanes)
 static const float LANE_START_SLACK = 1000; // m: own bases this much farther from the enemy than the closest can start a lane
 static const float LANE_MASS = 1.4;         // x the force an attack in the lane is filled up to
 ref ACR_AILane m_Lane;
 protected float m_fNextLaneReview = -1;
 protected ACR_Base m_LaneStalledAt;         // objective of a lane given up as stalled...
 protected float m_fLaneStalledUntil;        // ...whose lanes score less until then (world ms)
 protected bool m_bLaneStarveCounted;
 protected bool m_bRushOver;
 ACR_AILane Lane() { return m_Lane; }
 void SetLaneForTest(ACR_AILane lane) { m_Lane = lane; }

 protected bool LanesUsed() { return ACR_AILanes.On() && m_Doctrine && m_Doctrine.m_bLanes; }

 // The lane steers the plan once the land-rush is over.
 bool LaneActive()
 {
  return m_Lane && LanesUsed() && !Rushing() && m_Lane.Valid(m_sFaction);
 }

 // The opening land-rush: the first m_fRushMinutes of the match, while empty bases are in sight.
 // Once over it stays over (it is asked many times a tick).
 bool Rushing()
 {
  if (m_bRushOver) return false;
  if (GetGame().GetWorld().GetWorldTime() < m_Doctrine.m_fRushMinutes * 60000)
  {
   foreach (ACR_Base base : ACR_Base.s_All)
    if (!base.m_bRetired && Visible(base) && SeenOwner(base).IsEmpty()) return true;
  }
  m_bRushOver = true;
  Say(string.Format("[ACR] AI %1: land-rush over", m_sFaction));
  return false;
 }
 void SetRushOverForTest(bool over) { m_bRushOver = over; }

 // Starved: it hasn't mustered for anything in the lane for STARVE_S. Other targets open meanwhile.
 protected bool LaneStarved()
 {
  if (!m_Lane || m_Lane.m_fStarvedSince < 0) return false;
  return GetGame().GetWorld().GetWorldTime() - m_Lane.m_fStarvedSince > ACR_AILanes.STARVE_S * 1000;
 }

 // Bases off the lane open as targets when it is starved, or when nothing in it or on its flanks
 // can be attacked at all: a lane toward empty bases (soak 4 Nov) never starves, as starving counts
 // only targets in it that couldn't be mustered for, and held every attack back for the whole match.
 bool LaneOpenToOthers()
 {
  return LaneStarved() || !LaneHasTarget();
 }
 bool LaneOpenForTest() { return LaneOpenToOthers(); }

 // An enemy base in the lane or on its flanks that the planners would attack (or already do).
 protected bool LaneHasTarget()
 {
  if (!LaneActive()) return false;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_sOwner == m_sFaction || !HostileOwner(base) || !Visible(base) || CoolingDown(base)) continue;
   if (base.m_bPocket && NearestOwnDistance(base.GetOrigin()) > POCKET_IN_THE_WAY) continue;
   if (!HqTargetable(base)) continue;
   if (InLaneOrFlank(base)) return true;
  }
  return false;
 }

 // An attack target's score multiplier from the lane: 1 without one, -1 for a base it leaves alone.
 // A base off the lane that goes round a strongpoint in the lane or on its flanks is a flank target,
 // sooner the more it cuts the strongpoint off (ACR_AIEnvelopment).
 float LaneTargetFactor(ACR_Base base)
 {
  if (!LaneActive()) return 1;
  vector at = base.GetOrigin();
  float factor = ACR_AILanes.TargetFactor(m_Lane.Contains(at), m_Lane.Flanks(at), base.m_bIncome, base.m_iValue);
  if (factor >= 0) return factor;
  ACR_Base strong = EnvelopOf(base);
  if (strong && InLaneOrFlank(strong)) return ACR_AILanes.FLANK_TARGET / EnvelopBonus(base);
  return factor;
 }

 // ---------------------------------------------------------------- going round strongpoints
 // ACR_AIEnvelopment (Jacob, 19 Nov). Refreshed at most every ENVELOP_REFRESH_MS from this side's
 // intel (DefenceOf, Visible, SeenOwner): per weak enemy base, its bonus and the strongpoint it goes
 // round; per base, when it last beat an attack of ours (NoteRepulsed).
 static const float ENVELOP_REFRESH_MS = 10000;
 protected ref map<ACR_Base, float> m_EnvelopBonus = new map<ACR_Base, float>();
 protected ref map<ACR_Base, ACR_Base> m_EnvelopOf = new map<ACR_Base, ACR_Base>();
 protected ref map<ACR_Base, string> m_EnvelopWhy = new map<ACR_Base, string>();
 protected ref map<ACR_Base, float> m_RepulsedAt = new map<ACR_Base, float>();
 protected float m_fEnvelopAt = -1;

 // The score bonus for attacking 'base' to go round a strongpoint (1: none).
 float EnvelopBonus(ACR_Base base)
 {
  UpdateEnvelopment();
  float bonus;
  if (!base || !m_EnvelopBonus.Find(base, bonus)) return 1;
  return bonus;
 }

 // The strongpoint 'base' goes round, or null.
 ACR_Base EnvelopOf(ACR_Base base)
 {
  UpdateEnvelopment();
  ACR_Base strong;
  if (!base || !m_EnvelopOf.Find(base, strong)) return null;
  return strong;
 }

 // An attack of ours on 'base' was beaten (called off for losses or stalled).
 void NoteRepulsed(ACR_Base base)
 {
  if (!base) return;
  m_RepulsedAt.Set(base, GetGame().GetWorld().GetWorldTime());
  m_fEnvelopAt = -1;
 }

 protected bool Repulsed(ACR_Base base, float now)
 {
  float at;
  return m_RepulsedAt.Find(base, at) && now - at < ACR_AIEnvelopment.REPULSED_MIN * 60000;
 }

 protected void UpdateEnvelopment(bool force = false)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (!force && m_fEnvelopAt >= 0 && now - m_fEnvelopAt < ENVELOP_REFRESH_MS) return;
  int started = System.GetTickCount();
  m_fEnvelopAt = now;
  m_EnvelopBonus.Clear();
  m_EnvelopOf.Clear();
  m_EnvelopWhy.Clear();
  for (int k = m_RepulsedAt.Count() - 1; k >= 0; k--) if (!m_RepulsedAt.GetKey(k)) m_RepulsedAt.RemoveElement(k);
  float weight = m_Doctrine.m_fEnvelop;
  if (weight <= 0) return;
  // What we could field against it: every combat squad's strength.
  float strength;
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine) if (squad.IsCombatUnit()) strength += squad.Strength();
  // Strongpoints, the strongest first.
  array<ACR_Base> strongs = {};
  array<int> strongDefence = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_bPocket || base.m_sOwner == m_sFaction || !Visible(base) || !HostileOwner(base)) continue;
   int defence = DefenceOf(base);
   if (!ACR_AIEnvelopment.Strongpoint(AttackNeed(base), strength, defence, Repulsed(base, now))) continue;
   int at = 0;
   while (at < strongDefence.Count() && strongDefence[at] >= defence) at++;
   strongs.InsertAt(base, at);
   strongDefence.InsertAt(defence, at);
  }
  for (int s = 0; s < strongs.Count() && s < ACR_AIEnvelopment.MAX_STRONG; s++)
  {
   ACR_Base strong = strongs[s];
   vector strongAt = strong.GetOrigin();
   string owner = SeenOwner(strong);
   // The bases we know its side holds (its network), and ours nearest it (where we come from).
   array<ACR_Base> side = {};
   foreach (ACR_Base other : ACR_Base.s_All)
   {
    if (!other || other == strong || other.m_bRetired || !Visible(other) || SeenOwner(other) != owner) continue;
    side.Insert(other);
   }
   ACR_Base front = NearestOwnBase(strongAt);
   foreach (ACR_Base cand : side)
   {
    if (cand.m_bPocket || vector.DistanceXZ(cand.GetOrigin(), strongAt) > ACR_AIEnvelopment.REACH) continue;
    if (!ACR_AIEnvelopment.Weak(DefenceOf(cand), strongDefence[s])) continue;
    array<vector> rest = {};
    foreach (ACR_Base linked : side) if (linked != cand) rest.Insert(linked.GetOrigin());
    float cut = Math.Max(ACR_AIEnvelopment.Cut(strongAt, cand.GetOrigin(), rest), ACR_AIEnvelopment.Astride(strongAt, cand.GetOrigin(), rest));
    float rear = 0;
    if (front) rear = ACR_AIEnvelopment.Rear(front.GetOrigin(), strongAt, cand.GetOrigin());
    float bonus = ACR_AIEnvelopment.Bonus(cut, rear, weight);
    float had;
    if (m_EnvelopBonus.Find(cand, had) && had >= bonus) continue;
    m_EnvelopBonus.Set(cand, bonus);
    m_EnvelopOf.Set(cand, strong);
    string why = "beside it";
    if (cut >= 0.5 && cut >= rear) why = string.Format("cutting it off from %1 percent of its side's bases near it", Math.Round(cut * 100));
    else if (rear >= 0.5) why = "into its rear";
    else if (cut > 0) why = string.Format("beside it, across %1 percent of its links", Math.Round(cut * 100));
    if (Repulsed(strong, now)) why += ", after it beat our attack";
    m_EnvelopWhy.Set(cand, why);
   }
  }
  ACR_Perf.Lap("ai.envelop", started);
 }

 // Its envelopment in a line for Think and tests: "going round <strongpoint> (<why>, x<bonus>)".
 string EnvelopText(ACR_Base base)
 {
  ACR_Base strong = EnvelopOf(base);
  if (!strong) return "";
  string why;
  m_EnvelopWhy.Find(base, why);
  return string.Format("going round %1 (%2, x%3)", strong.m_sName, why, Math.Round(EnvelopBonus(base) * 100) / 100);
 }
 float EnvelopBonusForTest(ACR_Base base) { UpdateEnvelopment(true); return EnvelopBonus(base); }
 ACR_Base EnvelopOfForTest(ACR_Base base) { return EnvelopOf(base); }
 void NoteRepulsedForTest(ACR_Base base) { NoteRepulsed(base); }

 // ---------------------------------------------------------------- income grabs when behind
 // ACR_AIIncome (Jacob, 19 Nov: Pisy and Flikonya MB "just for extra income"). Urgency from our income
 // against the enemy income we can see, and our RP; refreshed with the envelopment picture.
 protected float m_fIncomeUrgency;
 protected float m_fIncomeAt = -1;

 protected float IncomeUrgency()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (m_fIncomeAt >= 0 && now - m_fIncomeAt < ENVELOP_REFRESH_MS) return m_fIncomeUrgency;
  m_fIncomeAt = now;
  int own, enemy;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || !base.m_bIncome || base.m_iValue <= 0) continue;
   if (base.m_sOwner == m_sFaction) own += base.m_iValue;
   else if (Visible(base) && HostileOwner(base)) enemy += base.m_iValue;
  }
  m_fIncomeUrgency = ACR_AIIncome.Urgency(own, enemy, m_Manager.m_Bases.GetRP(m_sFaction));
  return m_fIncomeUrgency;
 }

 // The worth multiplier for taking 'base' for its income (1: none): an income base, empty or lightly
 // held by an enemy, within reach of ours and nearer ours than the enemy's, while we need the money.
 float IncomeGrabBonus(ACR_Base base)
 {
  if (!base || base.m_bRetired || !base.m_bIncome || base.m_iValue <= 0 || base.m_sOwner == m_sFaction) return 1;
  if (!SeenOwner(base).IsEmpty() && (!HostileOwner(base) || DefenceOf(base) > ACR_AIIncome.LIGHT_DEFENCE)) return 1;
  float urgency = IncomeUrgency();
  if (urgency <= 0) return 1;
  float enemyGap = float.MAX;
  ACR_Base enemyBase = NearestEnemyBase(base.GetOrigin(), base);
  if (enemyBase) enemyGap = vector.DistanceXZ(enemyBase.GetOrigin(), base.GetOrigin());
  if (!ACR_AIIncome.Reachable(NearestOwnDistance(base.GetOrigin()), enemyGap)) return 1;
  return ACR_AIIncome.Bonus(urgency, m_Doctrine.m_fIncomeGrab);
 }

 // With a lane, an empty base off it: one at a time close behind our own (RearGrab), and while we need
 // income one more for its income (IncomeGrabBonus).
 bool OffLaneGrabAllowed(ACR_Base open, int rearGrabs)
 {
  if (rearGrabs == 0 && RearGrab(open)) return true;
  int cap = 1;
  if (IncomeUrgency() > 0) cap = 2;
  return rearGrabs < cap && IncomeGrabBonus(open) > 1;
 }
 float IncomeGrabBonusForTest(ACR_Base base) { m_fIncomeAt = -1; return IncomeGrabBonus(base); }

 // ---------------------------------------------------------------- stalled grabs
 // A grab that stalls goes to another squad, not to the same one again (Jacob's 19 Nov game: a gun UAZ
 // parked 98 m short of Flikonya MB's 60 m ring for ten minutes, its crew staying aboard, and the base
 // was not tried again for an hour). The stalled squad is barred from that base for GRAB_BAR_MS; when
 // only crewed vehicles were on it, every crewed vehicle is. The base cools off GRAB_RETRY_S only.
 static const float GRAB_BAR_MS = 900000;
 static const float GRAB_RETRY_S = 90;
 static const float GRAB_PARK_NEAR = 250;  // m outside the ring a vehicle grab may stop short
 static const float GRAB_PARK_S = 90;      // s without closing 10 m before it counts as parked short
 protected ref map<ACR_Squad, ACR_Base> m_GrabStalledOn = new map<ACR_Squad, ACR_Base>();
 protected ref map<ACR_Squad, float> m_GrabStalledAt = new map<ACR_Squad, float>();
 protected ref map<ACR_Base, float> m_VehicleGrabFailedAt = new map<ACR_Base, float>();

 bool GrabBarred(ACR_Squad squad, ACR_Base base)
 {
  if (!squad || !base) return false;
  float now = GetGame().GetWorld().GetWorldTime();
  ACR_Base on;
  float at;
  if (m_GrabStalledOn.Find(squad, on) && on == base && m_GrabStalledAt.Find(squad, at) && now - at < GRAB_BAR_MS) return true;
  return ACR_Transport.CrewedVehicle(squad) != null && m_VehicleGrabFailedAt.Find(base, at) && now - at < GRAB_BAR_MS;
 }

 // Only crewed vehicles on it (their crews stay aboard), stopped within GRAB_PARK_NEAR outside the ring
 // and not closing in for GRAB_PARK_S.
 protected bool GrabParkedShort(ACR_AIAttack grab)
 {
  if (!grab.m_bGrab || !grab.m_Target) return false;
  float gap = float.MAX;
  int live;
  foreach (ACR_Squad member : grab.m_Squads)
  {
   if (!member || member.Strength() <= 0) continue;
   if (!ACR_Transport.CrewedVehicle(member)) return false;
   live++;
   gap = Math.Min(gap, vector.DistanceXZ(PositionOf(member), grab.m_Target.GetOrigin()) - grab.m_Target.m_fRadius);
  }
  float now = GetGame().GetWorld().GetWorldTime();
  if (live == 0 || gap <= 0 || gap > GRAB_PARK_NEAR)
  {
   grab.m_fParkedGap = -1;
   return false;
  }
  if (grab.m_fParkedGap < 0 || gap < grab.m_fParkedGap - 10)
  {
   grab.m_fParkedGap = gap;
   grab.m_fParkedSince = now;
   return false;
  }
  return now - grab.m_fParkedSince > GRAB_PARK_S * 1000;
 }

 // A grab stalled: bar its squads from it (and crewed vehicles, when only they were on it).
 void NoteGrabStalled(ACR_AIAttack grab)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  for (int k = m_GrabStalledOn.Count() - 1; k >= 0; k--)
  {
   ACR_Squad old = m_GrabStalledOn.GetKey(k);
   float since;
   if (!old || !m_GrabStalledAt.Find(old, since) || now - since >= GRAB_BAR_MS)
   {
    if (old) m_GrabStalledAt.Remove(old);
    m_GrabStalledOn.RemoveElement(k);
   }
  }
  for (int n = m_GrabStalledAt.Count() - 1; n >= 0; n--) if (!m_GrabStalledAt.GetKey(n)) m_GrabStalledAt.RemoveElement(n);
  bool vehiclesOnly = !grab.m_Squads.IsEmpty();
  foreach (ACR_Squad member : grab.m_Squads)
  {
   if (!member) continue;
   m_GrabStalledOn.Set(member, grab.m_Target);
   m_GrabStalledAt.Set(member, now);
   if (!ACR_Transport.CrewedVehicle(member)) vehiclesOnly = false;
  }
  if (vehiclesOnly && grab.m_Target) m_VehicleGrabFailedAt.Set(grab.m_Target, now);
 }
 bool GrabParkedShortForTest(ACR_AIAttack grab) { return GrabParkedShort(grab); }
 bool GrabBarredForTest(ACR_Squad squad, ACR_Base base) { return GrabBarred(squad, base); }
 void NoteGrabStalledForTest(ACR_AIAttack grab) { NoteGrabStalled(grab); }

 bool InLaneOrFlank(ACR_Base base)
 {
  if (!LaneActive() || !base) return false;
  return m_Lane.Contains(base.GetOrigin()) || m_Lane.Flanks(base.GetOrigin());
 }

 // An empty base off the lane is still grabbed when it is close behind our own bases, nearer them
 // than any enemy base we know of.
 protected bool RearGrab(ACR_Base base)
 {
  float own = NearestOwnDistance(base.GetOrigin());
  if (own > ACR_AILanes.REAR_GRAB) return false;
  ACR_Base enemy = NearestEnemyBase(base.GetOrigin());
  return !enemy || vector.DistanceXZ(enemy.GetOrigin(), base.GetOrigin()) > own;
 }

 // An enemy HQ: a base with the owner's player spawn point (as IsHQ for our own).
 protected bool EnemyHQ(ACR_Base base)
 {
  string owner = SeenOwner(base);
  if (owner.IsEmpty() || owner == m_sFaction) return false;
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
   if (point && !ACR_SpawnPoints.IsBasePoint(point) && point.GetFactionKey() == owner && vector.DistanceXZ(point.GetOrigin(), base.GetOrigin()) <= base.m_fRadius + 60) return true;
  return false;
 }

 // An enemy HQ is the last objective, not the first (Jacob, 6 Nov: at the start it is often the only
 // enemy base in sight, so every squad left over after the grabs went for it, into the enemy's
 // spawn and its whole starting army). It is a target once it is next in line from one of our bases
 // (no other base still to take is clearly nearer that base), or once nothing else is left to take.
 // A counterattack or a player's "take this base" goes for it all the same (AssignAttacks' priority).
 static const float HQ_NEXT_SLACK = 1.25;     // next in line: within 1.25x of that base's nearest other target
 static const float HQ_LOG_INTERVAL = 120000; // ms between "leaves the HQ for later" lines
 protected float m_fNextHqLog;

 // Pure: is 'hq' next in line from any of 'own', given the other bases still to take?
 static bool HqNextInLine(notnull array<vector> own, vector hq, notnull array<vector> others)
 {
  if (others.IsEmpty()) return true;
  foreach (vector at : own)
  {
   float nearest = float.MAX;
   foreach (vector other : others) nearest = Math.Min(nearest, vector.DistanceXZ(at, other));
   if (vector.DistanceXZ(at, hq) <= nearest * HQ_NEXT_SLACK) return true;
  }
  return false;
 }

 // Whether the attack planners may pick this base: any base but an enemy HQ not yet next in line.
 bool HqTargetable(ACR_Base base)
 {
  if (!EnemyHQ(base)) return true;
  array<vector> own = {};
  array<vector> others = {};
  foreach (ACR_Base other : ACR_Base.s_All)
  {
   if (!other || other.m_bRetired || other == base) continue;
   if (other.m_sOwner == m_sFaction) { own.Insert(other.GetOrigin()); continue; }
   if (!Visible(other)) continue;
   // Still to take: an empty base, or one held by an enemy (a found FIA pocket only when it is in
   // the way, as AssignAttacks weighs it). An allied FIA's base is not.
   string seen = SeenOwner(other);
   if (!seen.IsEmpty() && !HostileOwner(other)) continue;
   if (!seen.IsEmpty() && other.m_bPocket && NearestOwnDistance(other.GetOrigin()) > POCKET_IN_THE_WAY) continue;
   others.Insert(other.GetOrigin());
  }
  if (HqNextInLine(own, base.GetOrigin(), others)) return true;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now >= m_fNextHqLog)
  {
   m_fNextHqLog = now + HQ_LOG_INTERVAL;
   Say(string.Format("[ACR] AI %1: leaves the enemy HQ %2 for later (%3 other bases to take first)", m_sFaction, base.m_sName, others.Count()));
  }
  return false;
 }

 // What a lane from 'from' to 'to' wins (income of the bases in it we don't hold, an enemy HQ at
 // its end) against the defence we know of in it and its length. Only what this side can see.
 protected float LaneScore(ACR_Base from, ACR_Base to)
 {
  vector a = from.GetOrigin();
  vector b = to.GetOrigin();
  float half = m_Doctrine.m_fLaneWidth;
  float gain = 0;
  float cost = 0;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired || base.m_sOwner == m_sFaction || !Visible(base) || ACR_AILane.PartnerHeld(base, m_sFaction)) continue;
   if (!ACR_AILanes.InLane(a, b, half, base.GetOrigin())) continue;
   gain += ACR_AILanes.Gain(base.m_bIncome, base.m_iValue);
   if (HostileOwner(base)) cost += DefenceOf(base);
  }
  if (EnemyHQ(to)) gain += ACR_AILanes.HQ_GAIN;
  float score = ACR_AILanes.Score(gain, cost, vector.DistanceXZ(a, b));
  // A main effort that goes round a strongpoint, cutting it off or into its rear (ACR_AIEnvelopment).
  if (HostileOwner(to)) score *= EnvelopBonus(to);
  if (to == m_LaneStalledAt && GetGame().GetWorld().GetWorldTime() < m_fLaneStalledUntil) score *= ACR_AILanes.FAILED_FACTOR;
  return score;
 }

 // The best lane now: from one of our front bases (within LANE_START_SLACK of the closest to the
 // enemy) to a base we can see and don't hold, at least MIN_LENGTH away. Near-equal lanes are
 // picked at random (ACR_Variety), so the same map doesn't always see the same push.
 protected ACR_AILane BestLane()
 {
  array<ACR_Base> starts = {};
  array<float> fronts = {};
  float closest = float.MAX;
  foreach (ACR_Base own : ACR_Base.s_All)
  {
   if (own.m_bRetired || own.m_sOwner != m_sFaction || own.m_bContested) continue;
   float front = FrontDistance(own);
   if (front == float.MAX) continue;
   starts.Insert(own);
   fronts.Insert(front);
   closest = Math.Min(closest, front);
  }
  array<ACR_Base> froms = {};
  array<ACR_Base> tos = {};
  array<float> scores = {};
  foreach (int i, ACR_Base from : starts)
  {
   if (fronts[i] > closest + LANE_START_SLACK) continue;
   foreach (ACR_Base to : ACR_Base.s_All)
   {
    // Not toward a base the truce partner (or a befriended side) holds: no attack is ever sent
    // there (UpdateAttacks calls them off), so the lane would hold the AI's front on it for good.
    if (to.m_bRetired || to.m_bPocket || to.m_sOwner == m_sFaction || !Visible(to) || ACR_AILane.PartnerHeld(to, m_sFaction)) continue;

    if (vector.DistanceXZ(from.GetOrigin(), to.GetOrigin()) < ACR_AILanes.MIN_LENGTH) continue;
    froms.Insert(from);
    tos.Insert(to);
    scores.Insert(LaneScore(from, to));
   }
  }
  if (scores.IsEmpty()) return null;
  int pick = ACR_Variety.Pick(scores, string.Format("lane %1 %2", m_sFaction, (int)GetGame().GetWorld().GetWorldTime()));
  if (pick < 0 || scores[pick] <= 0) return null;
  ACR_AILane lane = new ACR_AILane();
  lane.m_From = froms[pick];
  lane.m_Objective = tos[pick];
  lane.m_fScore = scores[pick];
  lane.m_fHalfWidth = m_Doctrine.m_fLaneWidth;
  return lane;
 }

 // Every REVIEW_S (at once when the lane breaks or stalls): keep the lane or change it, and tell
 // the side's 2IC channel. Never the enemy: they work it out from their intel.
 protected void UpdateLane()
 {
  if (!LanesUsed())
  {
   m_Lane = null;
   return;
  }
  if (Rushing()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  bool valid = m_Lane && m_Lane.Valid(m_sFaction);
  int setbacks = 0;
  if (m_Lane) setbacks = m_Lane.m_iSetbacks;
  if (valid && setbacks < ACR_AILanes.SETBACKS && now < m_fNextLaneReview) return;
  m_fNextLaneReview = now + ACR_AILanes.REVIEW_S * 1000;
  string why = "the best way in";
  if (m_Lane && m_Lane.m_Objective && m_Lane.m_Objective.m_sOwner == m_sFaction) why = string.Format("took %1", m_Lane.m_Objective.m_sName);
  else if (m_Lane && !valid && m_Lane.m_From) why = string.Format("lost %1", m_Lane.m_From.m_sName);
  else if (valid && setbacks >= ACR_AILanes.SETBACKS)
  {
   why = string.Format("stalled short of %1", m_Lane.m_Objective.m_sName);
   m_LaneStalledAt = m_Lane.m_Objective;
   m_fLaneStalledUntil = now + ACR_AILanes.FAILED_MIN * 60000;
  }
  else if (valid) why = "the enemy is thinner there";
  ACR_AILane best = BestLane();
  float current = 0;
  float held = 0;
  if (valid)
  {
   m_Lane.m_fScore = LaneScore(m_Lane.m_From, m_Lane.m_Objective);
   current = m_Lane.m_fScore;
   held = (now - m_Lane.m_fSince) / 60000;
  }
  float bestScore = 0;
  if (best) bestScore = best.m_fScore;
  if (!ACR_AILanes.ShouldSwitch(valid, setbacks, held, current, bestScore)) return;
  if (!best || (valid && best.Same(m_Lane)))
  {
   // Nothing better: carry on where it is (or, with no lane left, plan base by base).
   if (valid)
   {
    m_Lane.m_iSetbacks = 0;
    m_Lane.m_fStarvedSince = -1;
    m_bLaneStarveCounted = false;
   }
   else if (m_Lane)
   {
    m_Lane = null;
    Say(string.Format("[ACR] AI %1: no lane to push down (%2): planning base by base", m_sFaction, why));
   }
   return;
  }
  best.m_fSince = now;
  m_Lane = best;
  m_bLaneStarveCounted = false;
  Say(string.Format("[ACR] AI %1: main effort from %2 toward %3, %4 m wide, score %5 (%6)", m_sFaction, best.m_From.m_sName, best.m_Objective.m_sName, (int)(best.m_fHalfWidth * 2), best.m_fScore, why));
  TwoICReport(string.Format("Main effort: from %1 toward %2 (%3)", best.m_From.m_sName, best.m_Objective.m_sName, why));
 }
 void UpdateLaneForTest() { m_fNextLaneReview = -1; UpdateLane(); }

 // An attack ended: one called off in the lane or on its flank is a setback, one won there takes one back.
 void NoteLaneOutcome(ACR_AIAttack attack, bool won)
 {
  if (!attack || attack.m_bGrab || !InLaneOrFlank(attack.m_Target)) return;
  if (won) m_Lane.m_iSetbacks = Math.Max(0, m_Lane.m_iSetbacks - 1);
  else m_Lane.m_iSetbacks++;
 }

 // The plan found nothing in the lane it could muster for: after STARVE_S that is a setback (once),
 // and other targets open until it launches in the lane again.
 void NoteLaneMuster(bool launchedInLane)
 {
  if (!LaneActive()) return;
  if (launchedInLane)
  {
   m_Lane.m_fStarvedSince = -1;
   m_bLaneStarveCounted = false;
   return;
  }
  if (m_Lane.m_fStarvedSince < 0) m_Lane.m_fStarvedSince = GetGame().GetWorld().GetWorldTime();
  if (LaneStarved() && !m_bLaneStarveCounted)
  {
   m_bLaneStarveCounted = true;
   m_Lane.m_iSetbacks++;
   Say(string.Format("[ACR] AI %1: nothing in the lane toward %2 it can muster for: other targets open", m_sFaction, m_Lane.m_Objective.m_sName));
  }
 }

 // ---------------------------------------------------------------- supports

 protected int SupportIndex(ACR_ESupportType type) { return ACR_AIIntel.SupportIndex(this, type); }

 int SupportBudget() { return ACR_AIIntel.SupportBudget(this); }
 int SupportBudgetForTest() { return ACR_AIIntel.SupportBudgetForTest(this); }

 bool UrgentTroops(int combatUnits, int cap) { return ACR_AIIntel.UrgentTroops(this, combatUnits, cap); }

 bool SupportReady(ACR_ESupportType type) { return ACR_AIIntel.SupportReady(this, type); }

 protected bool FriendliesNear(vector point, float radius) { return ACR_AIIntel.FriendliesNear(this, point, radius); }

 bool CallSupport(ACR_ESupportType type, vector point, string why) { return ACR_AIIntel.CallSupport(this, type, point, why); }

 // ---------------------------------------------------------------- 0. intelligence
 // No map hacks: the commander only knows about enemies its own soldiers have
 // actually spotted (their perception), or that walk into one of its bases
 // (sentries). Each sighting is remembered at the last seen position for
 // INTEL_MEMORY_MS, then forgotten. Threat estimates, defensive fires and
 // attack planning all work from this picture, not from true positions.
 static const float INTEL_MEMORY_MS = 90000;
 static const float SENTRY_RANGE = 60;
 static const int UNKNOWN_DEFENCE = 4;        // assumed extra defenders at an enemy base we can't see into
 ref map<IEntity, vector> m_SeenAt = new map<IEntity, vector>();
 ref map<IEntity, float> m_SeenTime = new map<IEntity, float>();

 void Note(IEntity enemy, float now) { ACR_AIIntel.Note(this, enemy, now); }
 void NoteAtForTest(IEntity enemy, vector at, float now) { ACR_AIIntel.NoteAtForTest(this, enemy, at, now); }
 void ForgetForTest(IEntity enemy) { ACR_AIIntel.ForgetForTest(this, enemy); }
 void AssessForTest() { ACR_AIIntel.AssessForTest(this); }

 protected string FactionKeyOf(IEntity body) { return ACR_AIIntel.FactionKeyOf(this, body); }

 void GatherIntel() { ACR_AIIntel.GatherIntel(this); }

 bool KnownPosition(ACR_Squad squad, out vector position) { return ACR_AIIntel.KnownPosition(this, squad, position); }

 ACR_Squad EnemyNear(vector point, float radius, out vector seenAt) { return ACR_AIIntel.EnemyNear(this, point, radius, seenAt); }

 // ---------------------------------------------------------------- 1. assessment

 void Assess() { ACR_AIIntel.Assess(this); }

 protected bool Photographed(ACR_Base base) { return ACR_AIIntel.Photographed(this, base); }

 // ---------------------------------------------------------------- recon flights
 // The commander buys its own photographs (ACR_ReconRun): over a fresh radio intercept near one
 // of its bases (something is out there, find out what), else over the enemy base nearest its
 // own, the likeliest next fight, once its photographs of it are out of date. At most one flight
 // per RECON_GAP_MS, sooner under a doctrine that values observers (m_fObserverWeight); the
 // support's own cooldown and the RP reserve (SupportReady) still apply.
 static const float RECON_GAP_MS = 240000;
 static const float RECON_FIRST_MS = 120000;
 static const float INTERCEPT_FRESH_MS = 90000;
 static const float INTERCEPT_NEAR = 1500;
 float m_fNextRecon;

 protected void ManageRecon() { ACR_AIIntel.ManageRecon(this); }

 vector ReconTarget(out string why) { return ACR_AIIntel.ReconTarget(this, why); }

 int DefenceOf(ACR_Base base) { return ACR_AIIntel.DefenceOf(this, base); }

 // Where the next attack on us will come from (-acrDefenceForecast, ACR_DefenceForecast; defence
 // design step 1): from this side's intel only, after the threat count. Nothing acts on it yet: it
 // is logged, checked against the attacks that come, and shown in Think.
 ref ACR_DefenceForecast m_Forecast;
 ACR_DefenceForecast Forecast() { return ACR_AIIntel.Forecast(this); }

 void UpdateForecast() { ACR_AIIntel.UpdateForecast(this); }

 void ThinkForecast(notnull array<string> lines) { ACR_AIIntel.ThinkForecast(this, lines); }
 int ThreatTo(ACR_Base base) { return ACR_AIIntel.ThreatTo(this, base); }

 int DefenceThreat(ACR_Base base) { return ACR_AIIntel.DefenceThreat(this, base); }

 bool ForecastForTwoIC() { return ACR_AIIntel.ForecastForTwoIC(this); }

 ACR_Squad TwoICDefender(vector pos) { return ACR_AIIntel.TwoICDefender(this, pos); }

 int EnRouteTo(ACR_Base base) { return ACR_AIIntel.EnRouteTo(this, base); }

 // "threat 6" or "an attack forecast from the NE in ~3 min (~6)", for the defence lines.
 string ThreatText(ACR_Base base)
 {
  int threat = ThreatTo(base);
  if (threat > 0 || !m_Forecast) return string.Format("threat %1", threat);
  ACR_BaseForecast entry = m_Forecast.Of(base);
  if (!entry) return string.Format("threat %1", threat);
  string from = "";
  ACR_ForecastWay lead = entry.LeadWay();
  if (lead) from = " from the " + ACR_DefenceForecast.CompassOf(lead.m_fBearing);
  return string.Format("an attack forecast%1 in ~%2 min (~%3)", from, Math.Max(1, Math.Round(entry.m_fEta / 60)), Math.Round(entry.m_fExpected));
 }
 int DefenceOfForTest(ACR_Base base) { return DefenceOf(base); }

 // A manned tier II or III position within DEFENSE_DISTANCE of a base counts its holders again,
 // half (tier II) or whole (tier III), on top of the squad itself (and +5 a gun with a gunner in it,
 // as for any armed vehicle): dug in, they hold more than a squad posted at the base (Jacob, #25).
 // An enemy's counts only where we know its holders are. Threat is left alone: a position holds
 // ground, it doesn't take a base.
 static const float MANNED_REACH = 60; // m: its holders this close to it count as manning it
 int FortifiedCredit(ACR_Base base)
 {
  int credit;
  bool ours = base.m_sOwner == m_sFaction;
  foreach (ACR_Fortification fort : ACR_Fortify.s_Positions)
  {
   if (fort.m_sFaction != base.m_sOwner || fort.m_iTier < 2 || vector.DistanceXZ(fort.m_vPos, base.GetOrigin()) > DEFENSE_DISTANCE) continue;
   ACR_Squad holder = ManningSquad(fort);
   if (!holder) continue;
   vector where = PositionOf(holder);
   if (!ours && !KnownPosition(holder, where)) continue;
   if (vector.DistanceXZ(where, fort.m_vPos) > MANNED_REACH) continue;
   int men = holder.CountAlive();
   if (fort.m_iTier >= 3) credit += men;
   else credit += men / 2;
  }
  return credit;
 }

 // The squad (AI or player) holding a position, on its way back to one included.
 protected ACR_Squad ManningSquad(ACR_Fortification fort)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (squad && squad.m_Fortification == fort && squad.CountAlive() > 0) return squad;
  }
  return null;
 }
 void SetThreatForTest(ACR_Base base, int threat, int defence) { m_Threat.Set(base, threat); m_Defence.Set(base, defence); }
 void AssignDefendersForTest() { AssignDefenders(); }

 // The AI-led squads of ours holding a roadblock or position within 'reach' of 'pos' (added to 'into').
 // A squad at one of our bases with lines standing and an enemy base within WORKS_FRONT: it stays behind them
 // rather than go and defend another base, unless that one is outnumbered already; and the last squad there
 // stays whatever the call. The user, 2 Nov: Chotain Base's holding squad was sent to Villeneuve as the QRF 25
 // minutes before the attack on Chotain Base, and nobody manned its works when it came; 3 Nov (pn30): a threat
 // of 5 at a sawmill, 'outnumbered' by one man, took both of Chotain Base's squads. Said once per squad and base.
 bool KeepsWorksManned(ACR_Squad squad, ACR_Base calling, bool outnumbered)
 {
  if (!squad) return false;
  vector at = PositionOf(squad);
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base == calling || base.m_sOwner != m_sFaction || base.m_bRetired || !ACR_BaseWorks.LinesStand(base)) continue;
   if (vector.DistanceXZ(at, base.GetOrigin()) > DEFENSE_DISTANCE) continue;
   ACR_Base enemy = NearestEnemyBase(base.GetOrigin());
   if (!enemy || vector.DistanceXZ(enemy.GetOrigin(), base.GetOrigin()) > WORKS_FRONT) continue;
   bool last = LastSquadAt(squad, base);
   if (outnumbered && !last) continue;
   ACR_Base said;
   if (!m_KeptBehindLines.Find(squad, said) || said != calling)
   {
    m_KeptBehindLines.Set(squad, calling);
    string why = "rather than defend";
    if (last) why = "as its last squad, rather than defend";
    Say(string.Format("[ACR] AI %1: %2 stays behind the lines at %3 (%4 is %5 m off) %6 %7 (threat %8, defence %9)", m_sFaction, squad.GetName(), base.m_sName, enemy.m_sName, (int)vector.DistanceXZ(enemy.GetOrigin(), base.GetOrigin()), why, calling.m_sName, ThreatTo(calling), DefenceOf(calling)));
   }
   return true;
  }
  return false;
 }

 // No other squad of ours with men alive stands within DEFENSE_DISTANCE of the base (a player's counts).
 protected bool LastSquadAt(ACR_Squad squad, ACR_Base base)
 {
  foreach (ACR_Squad other : m_Manager.m_Squads)
  {
   if (!other || other == squad || !other.GetFaction() || other.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (other.CountAlive() <= 0 || !other.GetAnchor() || other.m_bMortar || other.Aircraft()) continue;
   if (vector.DistanceXZ(PositionOf(other), base.GetOrigin()) <= DEFENSE_DISTANCE) return false;
  }
  return true;
 }

 void HoldersNear(vector pos, float reach, notnull array<ACR_Squad> into)
 {
  foreach (ACR_Squad holder : m_Manager.m_Squads)
  {
   if (!IsHolder(holder) || !holder.GetFaction() || holder.GetFaction().GetFactionKey() != m_sFaction || holder.PlayerSteers()) continue;
   if (vector.DistanceXZ(PositionOf(holder), pos) > reach) continue;
   into.Insert(holder);
  }
 }

 // ---------------------------------------------------------------- 2. defence

 void AssignDefenders() { ACR_AIDefensePlanner.AssignDefenders(this); }

 protected void CounterBatterySupport(float fireFactor) { ACR_AIDefensePlanner.CounterBatterySupport(this, fireFactor); }
 void CounterBatterySupportForTest(float fireFactor) { ACR_AIDefensePlanner.CounterBatterySupportForTest(this, fireFactor); }

 // Defence in depth (Jacob 20 Oct: defenders only ever held the point itself). When an
 // attack on a base is seen coming (intel), part of its defenders move out to a forward
 // line 100-300 m towards it (Travel garrisons a building there when there is one) and the
 // rest stay at the point as the reserve (the base garrison counts towards it). The forward
 // squads fall back to the point when the enemy gets too close to hold out there, when they
 // are badly hurt, or when the threat is gone.
 static const float FORWARD_MIN = 100;
 static const float FORWARD_MAX = 300;
 static const float FORWARD_ENEMY_GAP = 80;   // m a forward position stays short of the enemy
 static const float FORWARD_REISSUE = 100;    // m the line must move before squads are moved again
 static const float FORWARD_SPREAD = 60;      // m between forward squads along the line
 ref map<ACR_Squad, vector> m_ForwardAt = new map<ACR_Squad, vector>();

 bool ThreatFrom(ACR_Base base, out vector at) { return ACR_AIDefensePlanner.ThreatFrom(this, base, at); }

 bool ForwardSpotOf(ACR_Squad squad, out vector spot) { return ACR_AIDefensePlanner.ForwardSpotOf(this, squad, spot); }

 protected void RecallForward(ACR_Squad squad, ACR_Base base, string why) { ACR_AIDefensePlanner.RecallForward(this, squad, base, why); }

 void DefendInDepth() { ACR_AIDefensePlanner.DefendInDepth(this); }

 // ---------------------------------------------------------------- 3. attacks

 static const int REACH_NONE = 0;
 static const int REACH_WALK = 1;
 static const int REACH_OWN = 2;
 static const int REACH_TAXI = 3;
 static const int REACH_AIR_LIFT = 4;

 // ---- grabs of empty bases: how far, and how
 static const float GRAB_WALK = 1500; // m: farthest a grab goes on foot (longer walks timed out, Everon soak 5 Oct)

 // Pure: a grab's time in the planners' units (on foot 3, driving 9, as AssignAttacks has always
 // ranked them): its own vehicle anywhere; on foot within GRAB_WALK; farther only by a free truck
 // 'fetch' m away (Travel asks ACR_Rides for the same one), else -1. Before 6 Nov there was no truck
 // case: a generated Chernarus put the US HQ 2.2 km from every other base, and with no base in walking
 // range the US AI grabbed nothing all match while its truck stood at the HQ.
 static float GrabTime(bool rolling, float dist, float fetch)
 {
  if (rolling) return dist / 9;
  if (dist <= GRAB_WALK) return dist / 3;
  if (fetch < 0) return -1;
  return (fetch + dist) / 9 + 45; // the truck's way to them and the boarding
 }

 // GrabTime for this squad to 'target' (-1: out of its reach). 'beat': a time it has to beat to
 // matter, so the transport search runs only for a squad that could. 'byTruck': it needs one.
 float GrabTimeOf(ACR_Squad squad, vector target, float beat, out bool byTruck)
 {
  vector at = PositionOf(squad);
  float dist = vector.DistanceXZ(at, target);
  bool rolling = squad.InMobileVehicle() || ACR_Transport.CrewedVehicle(squad) != null;
  byTruck = !rolling && dist > GRAB_WALK;
  float fetch = -1;
  if (byTruck && m_Manager.m_Rides && GrabTime(false, dist, 0) < beat)
  {
   float found;
   if (m_Manager.m_Rides.FindTransport(m_sFaction, at, dist, squad.CountAlive(), found, true, null, false, m_bHandedOnly)) fetch = found;
  }
  return GrabTime(rolling, dist, fetch);
 }

 // How far out of a base an attack on it forms up (LaunchAttack's staging ring).
 static float AttackRing(ACR_Base target)
 {
  if (!target) return 0;
  return target.m_fRadius + STAGING_DISTANCE;
 }

 // On foot with 'walk' metres to go, more than a walk (WALK_REACH): no vehicle of its own, no air
 // lift, no truck ride under way or coming.
 bool OnFootFar(ACR_Squad squad, float walk)
 {
  if (walk <= WALK_REACH) return false;
  if (squad.IsMounted() || squad.InMobileVehicle() || ACR_Transport.CrewedVehicle(squad) || ACR_AirLift.Busy(squad)) return false;
  if (m_Manager.m_Rides && m_Manager.m_Rides.RideOf(squad)) return false;
  return true;
 }

 // How a free squad would get to 'target' for an attack, and in how many seconds: in its own
 // vehicle; on foot within the doctrine's ride distance; by a taxi it can get (a free transport
 // within the pickup range ACR_Rides.Request fetches from, not counted for another squad of this
 // attack: added to 'taxis'); on foot within WALK_REACH; by a ready air lift (one squad an attack,
 // 'liftTaken'); else REACH_NONE. 'ring': how far out of 'target' the trip ends (an attack stages
 // its radius + STAGING_DISTANCE out, AttackRing). Soak 6 Nov: measured to the centre, a squad at
 // Zaprudny (2.2 km from the USSR HQ, staging 1.9 km off) "couldn't get there" while the allocator,
 // which counts the ring, kept picking it: the HQ attack was planned and dropped 23 times in an hour.
 int Reach(ACR_Squad squad, vector target, notnull array<ACR_Squad> taxis, bool liftTaken, out float seconds, float ring = 0)
 {
  float dist = Math.Max(0, vector.DistanceXZ(PositionOf(squad), target) - ring);
  seconds = 0;
  if (squad.InMobileVehicle() || ACR_Transport.CrewedVehicle(squad))
  {
   seconds = dist / MOUNTED_SPEED;
   return REACH_OWN;
  }
  if (dist <= m_Doctrine.m_fRideDistance || !m_Manager.m_Rides)
  {
   if (dist > WALK_REACH) return REACH_NONE;
   seconds = dist / WALK_SPEED;
   return REACH_WALK;
  }
  float fetch;
  ACR_Squad taxi = m_Manager.m_Rides.FindTransport(m_sFaction, PositionOf(squad), dist, squad.CountAlive(), fetch, true, taxis, false, m_bHandedOnly);
  if (taxi)
  {
   taxis.Insert(taxi);
   seconds = fetch / RIDE_SPEED + 45 + dist / RIDE_SPEED;
   return REACH_TAXI;
  }
  if (dist <= WALK_REACH)
  {
   seconds = dist / WALK_SPEED;
   return REACH_WALK;
  }
  // Only a lift TryAirLift would call for this trip (attacks: the squad goes as ATTACK).
  if (!liftTaken && AirLiftCountable(squad, RingPoint(target, PositionOf(squad), ring), ACR_EAITask.ATTACK))
  {
   seconds = AIR_LIFT_SECONDS;
   return REACH_AIR_LIFT;
  }
  return REACH_NONE;
 }

 protected int ReachableStrength(array<ACR_Squad> free, vector target, float ring = 0) { return ACR_AIAttackPlanner.ReachableStrength(this, free, target, ring); }

 static float StageSeconds(float travel) { return ACR_AIAttackPlanner.StageSeconds(travel); }

 int ReachForTest(ACR_Squad squad, vector target, notnull array<ACR_Squad> taxis, bool liftTaken, out float seconds) { return ACR_AIAttackPlanner.ReachForTest(this, squad, target, taxis, liftTaken, seconds); }
 int ReachableStrengthForTest(array<ACR_Squad> free, vector target) { return ACR_AIAttackPlanner.ReachableStrengthForTest(this, free, target); }

 static bool IsSupportTeam(ACR_Squad squad) { return ACR_AIAttackPlanner.IsSupportTeam(squad); }

 protected float NearestFreeDistance(array<ACR_Squad> free, vector point) { return ACR_AIAttackPlanner.NearestFreeDistance(this, free, point); }

 protected float AttackNeed(ACR_Base target) { return ACR_AIAttackPlanner.AttackNeed(this, target); }

 protected void ReinforceAttacks(array<ACR_Squad> free) { ACR_AIAttackPlanner.ReinforceAttacks(this, free); }

 protected vector JoinAttack(ACR_AIAttack attack, ACR_Squad squad, float now) { return ACR_AIAttackPlanner.JoinAttack(this, attack, squad, now); }

 protected bool PlanBudgetSpent() { return ACR_AIAttackPlanner.PlanBudgetSpent(this); }

 protected void AssignAttacks() { ACR_AIAttackPlanner.AssignAttacks(this); }

 protected ACR_AIAttack NewAttack(ACR_Base target, bool counter) { return ACR_AIAttackPlanner.NewAttack(this, target, counter); }

 protected void LaunchAttack(ACR_AIAttack attack, vector from, int strength, float required, float pressure, bool dropWalkers = true) { ACR_AIAttackPlanner.LaunchAttack(this, attack, from, strength, required, pressure, dropWalkers); }

 // Each armed carrier with room (gun manned, 4+ free seats) picks the nearest infantry squad of the
 // attack that fits and is within 400 m, and rides it to that squad's staging point (5 Oct;
 // Jacob: APCs for motorized assaults). The carrier then forms up there instead of its own point.
 static float CARRIER_PICKUP = 400;
 static int s_iCarrierTries; // tests
 protected void PairCarriers(ACR_AIAttack attack, array<ACR_Squad> carried) { ACR_AIAttackPlanner.PairCarriers(this, attack, carried); }

 string ForceAttack(ACR_Base target) { return ACR_AIAttackPlanner.ForceAttack(this, target); }

 // Where the reserve waits: an own base that is safe (not contested, no known threat, at
 // least RESERVE_MIN_FRONT from every enemy base we know of), bigger first (value, radius),
 // not much farther back than RESERVE_IDEAL_FRONT, and not an HQ (IsHQ). The current
 // reserve base gets a bonus so the reserve doesn't shuttle between two near-equal bases.
 // Falls back to HomeBase (the HQ) when nothing else qualifies.
 static const float RESERVE_MIN_FRONT = 600;
 static const float RESERVE_IDEAL_FRONT = 1200;
 static const float RESERVE_MAX_PULL = 1800;
 static const int SAVE_MIN_UNITS = 4;            // save RP for a pricier unit only with this many fighting units out
 static const float FOB_DEPLOY_MIN_FRONT = 500;  // m: a built FOB closer to an enemy/neutral base than this isn't a deploy point
 static const float REINFORCE_RETRY_MS = 120000; // a squad whose AI reinforce failed waits this long before another try
 static const float POCKET_IN_THE_WAY = 1500; // m: a found FIA pocket this close to one of our bases is a target
 // (A pocket's attack score is multiplied by the doctrine's m_fPocketScore, 2.5 standard; idle
 // squads below the doctrine's m_fRefitRatio, 0.5 standard, walk back to a quiet base to refill.)
 static const float REFIT_GIVE_UP_MS = 900000;   // a walk back to refill that hasn't arrived by then is dropped
 // A walk back to refill that was given up (the base unreachable from where the squad is) isn't
 // sent to that base again for REFIT_SKIP_MS: the next try goes to another quiet base, and each
 // walk given up in a row waits REINFORCE_RETRY_MS longer before the next, up to REFIT_RETRY_MAX
 // times. AI soak 7 Nov (Everon, 2 h): Soviet Weapons Team was sent "to Lancre to reinforce"
 // 29 times, every 3 min, giving up 75 s after each.
 static const float REFIT_SKIP_MS = 1200000;
 static const int REFIT_RETRY_MAX = 5;
 ACR_Base m_ReserveBase;
 protected ref map<ACR_Squad, ACR_Base> m_KeptBehindLines = new map<ACR_Squad, ACR_Base>(); // KeepsWorksManned: said once per base called
 ACR_Squad m_ReserveSquad;
 // COIN jobs (ACR_AICommanderSupport.c, #244 item 5): fields live here (layer rule).
 protected ref map<ACR_Squad, ACR_Base> m_TownPosts = new map<ACR_Squad, ACR_Base>();
 protected ACR_Squad m_Searcher;
 protected vector m_vSearchAt;
 protected float m_fSearchSince;
 protected float m_fSearchQuietSince;
 protected ref map<string, float> m_SearchedAt = new map<string, float>(); // lead centre (grid) -> last searched
 protected ACR_Squad m_AidRun;
 protected ACR_Base m_AidTown;
 protected float m_fAidSince;

 bool ReserveFits(ACR_Base base)
 {
  if (!base || base.m_sOwner != m_sFaction || IsHQ(base) || base.m_bContested || ThreatTo(base) > 0) return false;
  return FrontDistance(base) >= RESERVE_MIN_FRONT;
 }

 // Distance to the nearest enemy-held base we know of (any other base if none is known).
 float FrontDistance(ACR_Base base)
 {
  float enemy = float.MAX;
  float other = float.MAX;
  foreach (ACR_Base candidate : ACR_Base.s_All)
  {
   if (candidate.m_sOwner == m_sFaction || !Visible(candidate)) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), candidate.GetOrigin());
   if (!candidate.m_sOwner.IsEmpty()) enemy = Math.Min(enemy, d);
   other = Math.Min(other, d);
  }
  if (enemy < float.MAX) return enemy;
  return other;
 }

 // An HQ = a base with one of this side's player spawn points (most bases can deploy bought
 // units, so m_bAllowSpawn doesn't tell; the skirmish layout moves the spawn points to its HQs).
 bool IsHQ(ACR_Base base)
 {
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
   if (point && !ACR_SpawnPoints.IsBasePoint(point) && point.GetFactionKey() == m_sFaction && vector.DistanceXZ(point.GetOrigin(), base.GetOrigin()) <= base.m_fRadius + 60) return true;
  return false;
 }

 ACR_Base ReserveBase()
 {
  ACR_Base hedged = HedgedReserveBase();
  if (hedged)
  {
   m_ReserveBase = hedged;
   return hedged;
  }
  ACR_Base best;
  float bestScore = -float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!ReserveFits(base)) continue;
   float front = FrontDistance(base);
   float score = base.m_iValue * 150 + base.m_fRadius - Math.Max(0, front - RESERVE_IDEAL_FRONT) * 0.25;
   if (base == m_ReserveBase) score += 250;
   // Behind the main effort, to feed it.
   if (LaneActive() && m_Lane.Contains(base.GetOrigin())) score += ACR_AILanes.RESERVE_PULL;
   if (score > bestScore) { bestScore = score; best = base; }
  }
  if (!best) best = HomeBase();
  m_ReserveBase = best;
  return best;
 }

 // Hedging (-acrDefenceForecast 2, defence step 2): with attacks forecast on some of our bases, the
 // reserve waits at the base (one that fits a reserve, not a target itself) whose farthest likely
 // target is nearest, rather than at front distance; it is committed to one of them only when that
 // one's forecast firms up (AssignDefenders). Null: nothing forecast, the usual choice.
 static const float HEDGE_MARGIN = 300; // m another base must be better by before the reserve moves
 protected ACR_Base HedgedReserveBase()
 {
  if (!m_Forecast || !ACR_DefenceForecast.Acts() || m_bHandedOnly) return null;
  array<ACR_Base> watched = {};
  m_Forecast.Watched(watched, ACR_DefenceForecast.HEDGE_TARGETS);
  if (watched.IsEmpty()) return null;
  array<vector> targets = {};
  foreach (ACR_Base target : watched) targets.Insert(target.GetOrigin());
  array<ACR_Base> fits = {};
  array<vector> spots = {};
  int keep = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || watched.Contains(base)) continue;
   if (base.m_sOwner != m_sFaction || base.m_bContested || ThreatTo(base) > 0) continue;
   if (base == m_ReserveBase) keep = fits.Count();
   fits.Insert(base);
   spots.Insert(base.GetOrigin());
  }
  int pick = ACR_DefenceForecast.Hedge(spots, targets, keep, HEDGE_MARGIN);
  if (pick < 0) return null;
  if (fits[pick] != m_ReserveBase)
   Say(string.Format("[ACR] AI %1: the reserve hedges at %2 (attacks forecast on %3 targets, the farthest %4 m off)", m_sFaction, fits[pick].m_sName, watched.Count(), Math.Round(FarthestOf(fits[pick].GetOrigin(), targets))));
  return fits[pick];
 }

 protected static float FarthestOf(vector from, notnull array<vector> points)
 {
  float worst;
  foreach (vector point : points) worst = Math.Max(worst, vector.DistanceXZ(from, point));
  return worst;
 }

 // The own spawn base furthest from the fighting.
 protected ACR_Base HomeBase()
 {
  ACR_Base home;
  float best = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || !base.m_bAllowSpawn) continue;
   float nearestFront = 100000;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (other.m_sOwner != m_sFaction) nearestFront = Math.Min(nearestFront, vector.DistanceXZ(base.GetOrigin(), other.GetOrigin()));
   if (nearestFront > best) { best = nearestFront; home = base; }
  }
  return home;
 }

 ACR_Base NearestOwnBase(vector point, ACR_Base except = null)
 {
  ACR_Base nearest;
  float best = float.MAX;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || base == except) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), point);
   if (d < best) { best = d; nearest = base; }
  }
  return nearest;
 }

 float NearestOwnDistance(vector point)
 {
  ACR_Base nearest = NearestOwnBase(point);
  if (!nearest) return float.MAX;
  return vector.DistanceXZ(nearest.GetOrigin(), point);
 }

 static bool IsServiceVehicle(ACR_RequisitionEntry entry)
 {
  if (entry.m_sVehicle.IsEmpty()) return false;
  string text = entry.m_sName + " " + entry.m_sVehicle;
  text.ToLower();
  array<string> words = {"fuel", "repair", "ammo", "supply", "medical", "ambulance", "command", "radio", "tanker"};
  foreach (string word : words)
   if (text.Contains(word)) return true;
  // The rides' own test (ACR_Logistics.ServiceOfName: arsenal, engineer...): a truck they won't take
  // as a taxi is no transport. 6 Nov: Arsenal and Construction trucks passed as transports, rides
  // refused them ("service vehicle"), the shortage stayed and the AI bought 29 trucks in 3 h.
  return ACR_Logistics.ServiceOfName(entry.m_sVehicle) != 0;
 }

 // Long trips on foot become rides or air lifts:
 // Air lift (aca-f22 part 2): long moves (~2 km+ or long road detours) when the flight path is clear of known enemies.
 // (How far on foot before a ride, and before an air lift: the doctrine's m_fRideDistance and
 // m_fAirLiftDistance, 900 and 2000 m standard.)
 static const float AIRLIFT_RETRY_MS = 60000;   // after a refused AI air lift
 protected float m_fNextAirLiftTry;
 static const float AIRLIFT_ENEMY_CLEARANCE = 350;

 static float DistanceToSegment(vector p, vector a, vector b)
 {
  vector ab = b - a;
  ab[1] = 0;
  float l2 = ab[0] * ab[0] + ab[2] * ab[2];
  if (l2 < 1.0) return vector.DistanceXZ(p, a);
  vector ap = p - a;
  ap[1] = 0;
  float t = Math.Clamp((ap[0] * ab[0] + ap[2] * ab[2]) / l2, 0.0, 1.0);
  vector closest = Vector(a[0] + ab[0] * t, 0, a[2] + ab[2] * t);
  return vector.DistanceXZ(p, closest);
 }

 bool FlightPathClear(vector from, vector to)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (IEntity enemy, vector seenPos : m_SeenAt)
  {
   if (!enemy || !ACR_Respawn.IsAlive(enemy)) continue;
   float seenTime;
   if (!m_SeenTime.Find(enemy, seenTime) || (now - seenTime) > INTEL_MEMORY_MS) continue;
   if (DistanceToSegment(seenPos, from, to) < AIRLIFT_ENEMY_CLEARANCE) return false;
  }
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == m_sFaction) continue;
   vector known;
   if (!KnownPosition(squad, known)) continue;
   if (DistanceToSegment(known, from, to) < AIRLIFT_ENEMY_CLEARANCE) return false;
  }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == m_sFaction || base.m_sOwner.IsEmpty() || !Visible(base)) continue;
   float dDrop = vector.DistanceXZ(to, base.GetOrigin());
   if (dDrop < base.m_fRadius + 150) return false;
   float dClosest = DistanceToSegment(base.GetOrigin(), from, to);
   if (dClosest < base.m_fRadius + AIRLIFT_ENEMY_CLEARANCE && dClosest < dDrop - 20) return false;
  }
  return true;
 }

 // TryAirLift's cheap checks for 'squad' (at 'from') flying to 'destination' and going as 'task'
 // (the idle-shuffle gate). The plans ask them too (AirLiftCountable), so they only count on a
 // lift Travel would call. 'why': the check it fails ("" when an air lift was never on: none in
 // this mission, a player's squad, a vehicle crew).
 protected bool AirLiftOpen(ACR_Squad squad, vector from, vector destination, int task, out string why)
 {
  why = "";
  if (!m_Manager || !m_Manager.m_Supports || (m_bHandedOnly && !m_bTwoICAll)) return false; // paid: not for handed squads (CallSupport), but the lazy commander's 2IC may
  if (!squad || !squad.m_Group || squad.PlayerSteers()) return false;
  if (squad.IsMounted() || ACR_Transport.CrewedVehicle(squad) || squad.m_bMortar || squad.Aircraft()) return false;
  if (ACR_AirLift.Busy(squad)) return false;
  if (!ACR_Manager.OnMap(destination)) return false;
  if (vector.DistanceXZ(from, destination) < ACR_AirLift.MIN_DISTANCE) return false;
  int supportIdx = SupportIndex(ACR_ESupportType.AIR_LIFT);
  if (supportIdx < 0) return false;
  ACR_FactionData data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data) return false;
  ACR_SupportEntry entry = data.m_aSupports[supportIdx];
  if (!entry || !entry.m_bEnabled) return false;
  float now = GetGame().GetWorld().GetWorldTime();
  bool waveSlot = ACR_AirLift.WaveSlotAvailable(m_sFaction, now);
  if (now < entry.m_fReadyAt && !waveSlot)
  {
   why = string.Format("helicopter not ready for %1 s", (int)((entry.m_fReadyAt - now) / 1000));
   return false;
  }
  if (now < m_fNextAirLiftTry && !waveSlot)
  {
   why = "waiting after a refused lift";
   return false;
  }
  int cost = entry.CostFor(true);
  if (Purse(data) < cost + SUPPORT_RESERVE)
  {
   why = string.Format("short of RP (%1 for %2)", Purse(data), cost + SUPPORT_RESERVE);
   return false;
  }
  if (squad.KnownEnemies(300) > 0)
  {
   why = "enemies within 300 m";
   return false;
  }
  // AI priority gate: when known enemy bases exist, preserve air lifts for combat attacks
  // and threatened base defense rather than idle squad shuffling or cheap grabs.
  if ((task == ACR_EAITask.FREE || task == ACR_EAITask.POSTED) && HasKnownEnemyBase())
  {
   if (FriendlyBaseThreatened() || !m_Attacks.IsEmpty() || Purse(data) < cost * 2 + SUPPORT_RESERVE)
   {
    why = "kept for attacks and threatened bases";
    return false;
   }
  }
  return true;
 }

 // A lift the plans may count on for 'squad' to 'to', going as 'task': TryAirLift's cheap checks,
 // a long trip (a short one is flown only round a detour by road, too dear to route for every
 // estimate) and a clear flight path.
 protected bool AirLiftCountable(ACR_Squad squad, vector to, int task)
 {
  if (!squad) return false;
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return false;
  vector from = anchor.GetOrigin();
  string why;
  if (!AirLiftOpen(squad, from, to, task, why)) return false;
  if (vector.DistanceXZ(from, to) < m_Doctrine.m_fAirLiftDistance) return false;
  return FlightPathClear(from, to);
 }

 bool TryAirLift(ACR_Squad squad, vector destination, ResourceName prefab, float radius, string order, int kind, float dropShort = 0)
 {
  if (!squad) return false;
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return false;
  vector from = anchor.GetOrigin();
  float distance = vector.DistanceXZ(from, destination);
  // Cheap checks before the road route and landing zone searches (Jacob 18 Oct: freezes).
  string why;
  if (!AirLiftOpen(squad, from, destination, squad.m_iAITask, why))
  {
   NoAirLift(squad, distance, why);
   return false;
  }
  int supportIdx = SupportIndex(ACR_ESupportType.AIR_LIFT);
  float now = GetGame().GetWorld().GetWorldTime();

  bool isLong = distance >= m_Doctrine.m_fAirLiftDistance;
  bool isDetour = false;
  if (!isLong && distance >= m_Doctrine.m_fRideDistance)
  {
   if (BBB_Terrain.WaterAlong(from, destination) > BBB_Terrain.WET_ALLOWED)
   {
    isDetour = true;
   }
   else
   {
    array<vector> roadLegs = {};
    int roadNodes, roadMillis;
    if (!BBB_Roads.Route(from, destination, roadLegs, roadNodes, roadMillis) || BBB_Roads.s_bLastDetour)
    {
     isDetour = true;
    }
    else if (BBB_Roads.s_fLastLength > distance * 1.6)
    {
     isDetour = true;
    }
   }
  }
  if (!isLong && !isDetour) return false;

  vector targetLZ = destination;
  if (dropShort > 0)
  {
   vector dir = destination - from;
   dir[1] = 0;
   if (dir.Length() > 1)
   {
    dir.Normalize();
    targetLZ = destination - dir * Math.Min(dropShort, distance * 0.5);
   }
  }

  if (!FlightPathClear(from, targetLZ))
  {
   NoAirLift(squad, distance, "enemies seen along the flight path");
   return false;
  }

  string result = m_Manager.m_Supports.Call(m_sFaction, supportIdx, targetLZ, true, -1, squad, prefab, destination, radius, order, kind);
  if (!ACR_AirLift.Busy(squad))
  {
   // Refused (usually no landing spot): don't search again for every other long move.
   m_fNextAirLiftTry = now + AIRLIFT_RETRY_MS;
   Say(string.Format("[ACR] AI %1: no air lift for %2 (%3), next try in %4 s", m_sFaction, squad.GetName(), result, (int)(AIRLIFT_RETRY_MS / 1000)));
   return false;
  }
  Say(string.Format("[ACR] AI %1: air lift for %2 to %3 (%4 m): %5", m_sFaction, squad.GetName(), ACR_Manager.Grid(destination), (int)distance, result));
  return true;
 }

 // Why a long trip got no air lift, for attacks only (other long moves ask often; match 6 Nov:
 // attacks counted on lifts that were refused without a word).
 protected void NoAirLift(ACR_Squad squad, float distance, string why)
 {
  if (why.IsEmpty() || distance < m_Doctrine.m_fAirLiftDistance || squad.m_iAITask != ACR_EAITask.ATTACK) return;
  Say(string.Format("[ACR] AI %1: no air lift for %2 (%3 m): %4", m_sFaction, squad.GetName(), (int)distance, why));
 }

 // ---------------------------------------------------------------- trips
 // A posted squad on its way somewhere is committed to that trip (Jacob 1 Nov, Chernarus: squads
 // posted across the map were pulled out of their trucks into attacks, FOB escorts and new
 // postings, and walked for half an hour). Travel notes every trip; while one holds, a POSTED
 // squad isn't free for other plans (MySquads), as an air-lifted one isn't. It stops holding when
 // the squad gets there, stops, meets the enemy, is given something else to do, runs well over
 // the time the walk would take, or the reason goes: the base it was posted to is lost. A
 // threatened base still calls committed squads (AssignDefenders), and the admin's attack order
 // takes them too. Squads still on foot on a trip keep looking for a ride (CatchRides).
 static const float TRIP_COMMIT_MIN = 500;   // m: a shorter move commits nobody
 static const float TRIP_ARRIVED = 200;      // m from the destination
 static const float TRIP_SAME_GOAL = 150;    // m: the squad's order still ends this near the trip's end
 static const float TRIP_PACE = 1.0;         // m/s: the walk the trip is given time for...
 static const float TRIP_SLACK_S = 300;      // ...plus this
 static const float RIDE_RETRY_MS = 45000;   // a trip on foot asks for a ride again after this, doubling
 static const float RIDE_RETRY_MAX_MS = 360000; // ...up to this (a road that goes far round is refused every time)
 protected ref map<ACR_Squad, ref ACR_AITrip> m_Trips = new map<ACR_Squad, ref ACR_AITrip>();

 protected void NoteTrip(ACR_Squad squad, vector to, ResourceName prefab, float radius, string order, int kind, float dropShort)
 {
  float distance = vector.DistanceXZ(PositionOf(squad), to);
  if (distance < TRIP_COMMIT_MIN)
  {
   m_Trips.Remove(squad);
   return;
  }
  ACR_AITrip trip = new ACR_AITrip();
  trip.m_vTo = to;
  trip.m_sPrefab = prefab;
  trip.m_fRadius = radius;
  trip.m_sOrder = order;
  trip.m_iKind = kind;
  trip.m_fDropShort = dropShort;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner == m_sFaction && vector.DistanceXZ(base.GetOrigin(), to) <= base.m_fRadius + 150) { trip.m_Base = base; break; }
  }
  float now = GetGame().GetWorld().GetWorldTime();
  trip.m_fUntil = now + (distance / TRIP_PACE + TRIP_SLACK_S) * 1000;
  trip.m_fNextRide = now + RIDE_RETRY_MS;
  m_Trips.Set(squad, trip);
 }

 // "" while the trip still holds the squad, else why not.
 protected string TripOver(ACR_Squad squad, ACR_AITrip trip, float now)
 {
  if (now > trip.m_fUntil) return "out of time";
  // (A retired FOB keeps its owner: its HQ is gone, so it is lost all the same.)
  if (trip.m_Base && (trip.m_Base.m_bRetired || trip.m_Base.m_sOwner != m_sFaction)) return "base lost";
  if (vector.DistanceXZ(PositionOf(squad), trip.m_vTo) <= TRIP_ARRIVED) return "arrived";
  if (squad.PlayerSteers()) return "a player leads it";
  if (squad.KnownEnemies(300) > 0) return "contact";
  if (ACR_AirLift.Busy(squad)) return "";
  ACR_Ride ride;
  if (m_Manager.m_Rides) ride = m_Manager.m_Rides.RideOf(squad);
  if (ride && ride.m_Rider == squad)
  {
   if (vector.DistanceXZ(ride.m_Final, trip.m_vTo) <= TRIP_SAME_GOAL) return "";
   return "rides elsewhere";
  }
  // Pauses on the way (regrouping, getting out of a vehicle) keep the trip.
  ACR_EOrderState state = squad.OrderState();
  if (state == ACR_EOrderState.REGROUP || state == ACR_EOrderState.DISMOUNT || state == ACR_EOrderState.GET_OUT) return "";
  if (vector.DistanceXZ(squad.Heading(), trip.m_vTo) > TRIP_SAME_GOAL) return "given another order";
  if (!squad.m_Group || !squad.m_Group.GetCurrentWaypoint()) return "stopped";
  return "";
 }

 // True while a posted squad's trip still holds it (it isn't free for other plans).
 bool Committed(ACR_Squad squad)
 {
  if (!squad || squad.m_iAITask != ACR_EAITask.POSTED) return false;
  ACR_AITrip trip = m_Trips.Get(squad);
  if (!trip) return false;
  string over = TripOver(squad, trip, GetGame().GetWorld().GetWorldTime());
  if (over.IsEmpty()) return true;
  m_Trips.Remove(squad);
  PrintFormat("[ACR] TRIP %1 to %2: over (%3)", squad.GetName(), ACR_Manager.Grid(trip.m_vTo), over);
  return false;
 }

 // A squad's trip still holding and ending within 'reach' of 'point'.
 bool CommittedTo(ACR_Squad squad, vector point, float reach)
 {
  if (!Committed(squad)) return false;
  return vector.DistanceXZ(m_Trips.Get(squad).m_vTo, point) <= reach;
 }

 // Squads still on foot on a long trip (any task) look for a ride on the way: a truck freed or
 // bought since they set off (1 Nov live: one refused ask at the start meant a 30-minute walk;
 // the ask also tells Requisition the side is short of transport). One ask a tick, the farthest
 // from its end first; a refused trip waits RIDE_RETRY_MS, doubling each time, before asking again.
 protected void CatchRides()
 {
  if (!m_Manager.m_Rides) return;
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_Squad> over = {};
  ACR_Squad pick;
  float pickLeft;
  foreach (ACR_Squad squad, ACR_AITrip trip : m_Trips)
  {
   if (!squad || !m_Manager.m_Squads.Contains(squad) || !TripOver(squad, trip, now).IsEmpty())
   {
    over.Insert(squad);
    continue;
   }
   if (now < trip.m_fNextRide || squad.IsMounted() || ACR_Transport.CrewedVehicle(squad) || squad.m_bMortar || squad.Aircraft()) continue;
   if (m_Manager.m_Rides.RideOf(squad) || ACR_AirLift.Busy(squad)) continue;
   float left = vector.DistanceXZ(PositionOf(squad), trip.m_vTo);
   if (left <= m_Doctrine.m_fRideDistance || left <= pickLeft) continue;
   pick = squad;
   pickLeft = left;
  }
  foreach (ACR_Squad done : over) m_Trips.Remove(done);
  if (!pick || PlanBudgetSpent()) return;
  ACR_AITrip go = m_Trips.Get(pick);
  string why;
  if (!m_Manager.m_Rides.Request(pick, go.m_vTo, go.m_fDropShort, go.m_sPrefab, go.m_fRadius, go.m_sOrder, go.m_iKind, why, true, false, m_bHandedOnly))
  {
   go.m_iRideAsks++;
   go.m_fNextRide = now + Math.Min(RIDE_RETRY_MAX_MS, RIDE_RETRY_MS * Math.Pow(2, go.m_iRideAsks - 1));
   return;
  }
  Say(string.Format("[ACR] AI %1: %2 catches a ride on the way to %3 (%4 m to go)", m_sFaction, pick.GetName(), ACR_Manager.Grid(go.m_vTo), (int)pickLeft));
 }

 string TripOverForTest(ACR_Squad squad, ACR_Base base)
 {
  ACR_AITrip trip = new ACR_AITrip();
  trip.m_Base = base;
  trip.m_vTo = base.GetOrigin();
  trip.m_fUntil = GetGame().GetWorld().GetWorldTime() + 600000;
  return TripOver(squad, trip, GetGame().GetWorld().GetWorldTime());
 }
 void CatchRidesForTest()
 {
  foreach (ACR_Squad squad, ACR_AITrip trip : m_Trips) trip.m_fNextRide = 0;
  m_iTickStarted = System.GetTickCount();
  CatchRides();
 }
 void TravelForTest(ACR_Squad squad, vector destination, bool defend) { Travel(squad, destination, defend, 0); }
 void FreeSquadsForTest(array<ACR_Squad> result) { MySquads(result, true); }

 // 'approach': the enemy base this is a staging move for, to walk a covered way there on foot.
 void Travel(ACR_Squad squad, vector destination, bool defend, float dropShort, ACR_Base approach = null)
 {
  m_fCoverDetour = 0;
  // Defending a base: infantry garrison its buildings (the one nearest the centre,
  // spilling into its neighbours); vehicle crews, and bases without buildings, defend.
  bool garrison;
  vector building;
  // A base with works standing: behind the bags on the enemy's side, which ACR_Garrison takes for what's meant.
  // Never hijack an explicit 2IC Hold request onto a nearby base's works line.
  if (defend && !OnRequest(squad) && !ACR_Transport.CrewedVehicle(squad) && ACR_BaseWorks.LinePoint(destination, m_sFaction, building))
  {
   garrison = true;
   destination = building;
  }
  else if (defend && !ACR_Transport.CrewedVehicle(squad) && ACR_Garrison.AnchorNear(destination, 80, building, squad.m_Group))
  {
   garrison = true;
   destination = building;
  }
  ResourceName prefab = ACR_Squad.WP_MOVE;
  float radius = -1;
  string order = "Moving";
  int kind = 0;
  if (defend)
  {
   prefab = ACR_Squad.WP_DEFEND;
   radius = 15;
   order = "Defending";
   kind = 1;
  }
  if (garrison)
  {
   prefab = ACR_Squad.WP_GARRISON;
   order = "Garrisoning";
  }
  NoteTrip(squad, destination, prefab, radius, order, kind, dropShort);
  // Riders given somewhere else to go on the way: the same ride takes them there.
  if (m_Manager.m_Rides && m_Manager.m_Rides.Redirect(squad, destination, dropShort, prefab, radius, order, kind)) return;
  if (!squad.IsMounted())
  {
   if (TryAirLift(squad, destination, prefab, radius, order, kind, dropShort)) return;
   float distance = vector.DistanceXZ(PositionOf(squad), destination);
   if (m_Manager.m_Rides && distance > m_Doctrine.m_fRideDistance)
   {
    string why;
    if (m_Manager.m_Rides.Request(squad, destination, dropShort, prefab, radius, order, kind, why, true, false, m_bHandedOnly)) return;
    // (CatchRides keeps looking for one on the way.)
    Say(string.Format("[ACR] AI %1: %2 sets off on foot for %3 m to %4: %5", m_sFaction, squad.GetName(), (int)distance, ACR_Manager.Grid(destination), why));
   }
  }
  if (approach && !defend && !squad.IsMounted() && !ACR_Transport.CrewedVehicle(squad) && CoveredApproach(squad, approach, destination)) return;
  // On foot (or its own vehicle): round any water rather than across it.
  if (garrison) squad.RouteTo(destination, ACR_Squad.WP_GARRISON, 15, "Garrisoning", 1);
  else if (defend) squad.RouteTo(destination, ACR_Squad.WP_DEFEND, 15, "Defending", 1);
  else squad.RouteTo(destination, ACR_Squad.WP_MOVE, -1, "Moving", 0);
 }

 // A beaten squad breaks contact and falls back to the nearest own base; one out of ammo (forAmmo,
 // ammo runs on) to the nearest that can restock it, unless that is FALLBACK_REARM_EXTRA farther
 // (ManageAmmoRuns takes it on from wherever it gets to).
 static const float FALLBACK_REARM_EXTRA = 1000;
 void FallBack(ACR_Squad squad, string why, ACR_Base except = null, bool forAmmo = false)
 {
  ACR_Base refuge = NearestOwnBase(PositionOf(squad), except);
  if (forAmmo && ACR_AmmoRuns.On())
  {
   ACR_Base rearm = NearestRearmBase(PositionOf(squad), except);
   if (rearm && (!refuge || vector.DistanceXZ(PositionOf(squad), rearm.GetOrigin()) <= vector.DistanceXZ(PositionOf(squad), refuge.GetOrigin()) + FALLBACK_REARM_EXTRA)) refuge = rearm;
  }
  squad.SetAITask(ACR_EAITask.FREE);
  if (m_ReserveSquad == squad) m_ReserveSquad = null;
  m_RetreatingUntil.Set(squad, GetGame().GetWorld().GetWorldTime() + RETREAT_COOLDOWN_MS);
  squad.SetMode(ACR_EMoveMode.FAST);
  // On foot, round any water; no waiting for a truck under fire.
  if (refuge)
  {
   vector refugeAt = ACR_Manager.Ground(refuge.GetOrigin());
   squad.RouteTo(refugeAt, ACR_Squad.WP_DEFEND, 15, "Falling back", 1);
   // Covered fall-back (experimental, -acrCoveredFallBack 1): BBB bounds it back in covered halves.
   if (ACR_Squad.CoveredFallBackOn()) squad.MarkFallBack(refugeAt);
  }
  string where = "its position";
  if (refuge) where = refuge.m_sName;
  Say(string.Format("[ACR] AI %1: %2 falls back to %3 (%4)", m_sFaction, squad.GetName(), where, why));
 }

 bool IsAttacking(ACR_Base base)
 {
  foreach (ACR_AIAttack attack : m_Attacks)
   if (attack.m_Target == base) return true;
  return false;
 }

 // Base of fire keeps covering until the assault squads are on the objective, then lifts
 // fire and joins in.
 // The ground picture of an attack (-acrAttackGround): read out to the farthest squad of the attack
 // (the area the attack crosses), toward the main approach, with the enemies this side has seen
 // there as eyes. Nothing acts on it yet: it is logged and shown in Think so the reading of the
 // ground can be judged first (design step 1).
 void StartGround(ACR_AIAttack attack)
 {
  ACR_Base target = attack.m_Target;
  float extent = 0;
  foreach (ACR_Squad squad : attack.m_Squads)
   if (squad) extent = Math.Max(extent, vector.DistanceXZ(PositionOf(squad), target.GetOrigin()));
  attack.m_Ground = new ACR_AttackGround(target, m_sFaction, attack.m_vApproach, extent);
  array<vector> enemies = {};
  SeenEnemiesNear(target.GetOrigin(), attack.m_Ground.m_fExtent, enemies);
  attack.m_Ground.SetEyes(enemies);
 }

 void TickGround(ACR_AIAttack attack)
 {
  if (!attack.m_Ground || !attack.m_Target) return;
  ACR_AttackGround ground = attack.m_Ground;
  if (ground.IsDone() && ground.m_iBuilds != attack.m_iGroundLogged)
  {
   attack.m_iGroundLogged = ground.m_iBuilds;
   Say(string.Format("[ACR] AI %1: %2", m_sFaction, ground.Summary(attack.m_aStaging)));
  }
  // An enemy seen where no eye was: read the ground again (only once the last reading is done).
  if (!ground.IsDone()) return;
  array<vector> enemies = {};
  SeenEnemiesNear(attack.m_Target.GetOrigin(), ground.m_fExtent, enemies);
  ground.SetEyes(enemies);
 }

 // Where this side last saw each enemy squad within 'radius' of 'point' (intel only).
 protected void SeenEnemiesNear(vector point, float radius, notnull array<vector> seen)
 {
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad.GetFaction() || squad.GetFaction().GetFactionKey() == m_sFaction) continue;
   vector known;
   if (!KnownPosition(squad, known)) continue;
   if (vector.DistanceXZ(known, point) <= radius) seen.Insert(known);
  }
 }

 void TickFireBase(ACR_AIAttack attack)
 {
  ACR_Base target = attack.m_Target;
  if (!attack.m_Suppressor || !target) return;
  bool closeIn = false;
  foreach (ACR_Squad assaulter : attack.m_Squads)
   if (assaulter && assaulter != attack.m_Suppressor && vector.DistanceXZ(PositionOf(assaulter), target.GetOrigin()) < target.m_fRadius + 60) closeIn = true;
  if (closeIn) LiftFire(attack, "assault on the objective");
  else UpdateFireBase(attack);
 }

 // Base of fire (Jacob, 20 Oct: a squad stood suppressing an empty base centre for 10 min).
 // It fires only at an enemy this side has seen at the objective (intel, 90 s memory), from
 // within FIRE_BASE_RANGE with clear terrain between. Too far or blind: it moves up to a
 // spot short of the enemy. With nobody seen there yet it holds an overwatch position facing
 // the objective (in range, clear fire line) and engages only what its own men see (Jacob
 // 20 Oct, Everon: 18 of 27 bases of fire lifted "no enemy seen" the moment the assault
 // started). It joins the assault when the assault reaches the objective, nobody is left to
 // cover, it can't reach a firing spot in time, or after the doctrine's m_fFireBaseSeconds.
 void UpdateFireBase(ACR_AIAttack attack)
 {
  ACR_Squad fire = attack.m_Suppressor;
  ACR_Base target = attack.m_Target;
  if (!fire || !target) return;
  float now = GetGame().GetWorld().GetWorldTime();
  bool covering = false;
  foreach (ACR_Squad other : attack.m_Squads)
   if (other && other != fire && other.Strength() > 0) covering = true;
  if (!covering) { LiftFire(attack, "nobody left to cover"); return; }
  if (attack.m_fAssaultStart > 0 && now - attack.m_fAssaultStart > m_Doctrine.m_fFireBaseSeconds * 1000) { LiftFire(attack, "time up"); return; }
  vector seenAt;
  ACR_Squad enemy = EnemyNear(target.GetOrigin(), target.m_fRadius + 40, seenAt);
  if (!enemy) { Overwatch(attack); return; }
  vector from = PositionOf(fire);
  float distance = vector.DistanceXZ(from, seenAt);
  if (distance <= FIRE_BASE_RANGE && FireLine(from, seenAt))
  {
   attack.m_fFireBaseMoveSince = 0;
   if (fire.OrderState() == ACR_EOrderState.SUPPRESS && vector.DistanceXZ(fire.m_MoveTarget, seenAt) < 10) return;
   fire.Suppress(seenAt);
   Say(string.Format("[ACR] AI %1: %2 (base of fire) suppresses the enemy seen at %3 (%4 m)", m_sFaction, fire.GetName(), target.m_sName, Math.Round(distance)));
   return;
  }
  float stand = Math.Min(FIRE_BASE_STANDOFF, distance - 100);
  if (stand < 120) { LiftFire(attack, "no firing spot"); return; }
  if (attack.m_fFireBaseMoveSince <= 0) attack.m_fFireBaseMoveSince = now;
  if (now - attack.m_fFireBaseMoveSince > FIRE_BASE_MOVE_S * 1000) { LiftFire(attack, "no firing spot found in time"); return; }
  vector back = from - seenAt;
  back[1] = 0;
  back.Normalize();
  vector spot = seenAt + back * stand;
  spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
  if (fire.OrderState() == ACR_EOrderState.MOVE && vector.DistanceXZ(fire.m_MoveTarget, spot) < 40) return;
  if (vector.DistanceXZ(from, spot) > 80 && fire.KnownEnemies(250) == 0) fire.SetMode(ACR_EMoveMode.FAST);
  else fire.SetMode(ACR_EMoveMode.COMBAT);
  fire.MoveTo(spot);
  Say(string.Format("[ACR] AI %1: %2 (base of fire) moves up to %3 m from the enemy at %4: %5", m_sFaction, fire.GetName(), Math.Round(stand), target.m_sName, spot));
 }

 // No enemy seen at the objective: hold (Defend) an overwatch spot facing it, so the squad
 // shoots only at what it sees itself. Suppression of a spot nobody is seen at any more ends.
 protected void Overwatch(ACR_AIAttack attack)
 {
  ACR_Squad fire = attack.m_Suppressor;
  ACR_Base target = attack.m_Target;
  float now = GetGame().GetWorld().GetWorldTime();
  if (!attack.m_bOverwatch)
  {
   vector found;
   bool gotCrest = attack.m_Ground && attack.m_Ground.IsDone() && attack.m_Ground.BestFiringToward(PositionOf(fire), target.m_fRadius + OVERWATCH_MIN_GAP, FIRE_BASE_RANGE, found);
   if (!gotCrest && !OverwatchSpot(target, PositionOf(fire), found)) { LiftFire(attack, "no firing spot"); return; }
   attack.m_vOverwatch = found;
   attack.m_bOverwatch = true;
   attack.m_fFireBaseMoveSince = 0;
  }
  else if (fire.OrderState() != ACR_EOrderState.DEFEND)
  {
   // Back from suppressing or moving up: where it stands will do if it still has a fire line.
   vector here = PositionOf(fire);
   float range = vector.DistanceXZ(here, target.GetOrigin());
   if (range >= target.m_fRadius + OVERWATCH_MIN_GAP && range <= FIRE_BASE_RANGE && FireLine(here, target.GetOrigin())) attack.m_vOverwatch = here;
  }
  vector spot = attack.m_vOverwatch;
  float away = vector.DistanceXZ(PositionOf(fire), spot);
  if (away > OVERWATCH_REACHED)
  {
   if (attack.m_fFireBaseMoveSince <= 0) attack.m_fFireBaseMoveSince = now;
   if (now - attack.m_fFireBaseMoveSince > FIRE_BASE_MOVE_S * 1000) { LiftFire(attack, "no firing spot found in time"); return; }
  }
  else attack.m_fFireBaseMoveSince = 0;
  if (fire.OrderState() == ACR_EOrderState.DEFEND && vector.DistanceXZ(fire.m_MoveTarget, spot) < 20) return;
  fire.SetMode(ACR_EMoveMode.COMBAT);
  fire.Defend(spot);
  Say(string.Format("[ACR] AI %1: %2 (base of fire) holds overwatch on %3 from %4 m (no enemy seen there yet): %5", m_sFaction, fire.GetName(), target.m_sName, Math.Round(vector.DistanceXZ(spot, target.GetOrigin())), spot));
 }

 // A spot within FIRE_BASE_RANGE of the objective with a clear fire line to it: where the
 // squad stands if that will do, otherwise on rings round the objective on the squad's side.
 static const float OVERWATCH_REACHED = 40;
 static const float OVERWATCH_MIN_GAP = 90;   // m outside the base ring (not part of the assault)
 bool OverwatchSpot(ACR_Base target, vector from, out vector spot)
 {
  if (!target) return false;
  vector center = target.GetOrigin();
  float minRange = target.m_fRadius + OVERWATCH_MIN_GAP;
  float here = vector.DistanceXZ(from, center);
  if (here >= minRange && here <= FIRE_BASE_RANGE && !BBB_Terrain.IsWaterAt(from) && FireLine(from, center))
  {
   spot = from;
   return true;
  }
  vector side = from - center;
  side[1] = 0;
  if (side.LengthSq() < 1) side = Vector(0, 0, 1);
  side.Normalize();
  // The nearest one to the squad (it is walking there while the assault goes in).
  array<float> rings = {200, 250, 300, 350, 400};
  array<float> turns = {0, 10, -10, 20, -20, 30, -30, 45, -45, 60, -60};
  float best = float.MAX;
  foreach (float ring : rings)
  {
   float d = Math.Clamp(ring, minRange, FIRE_BASE_RANGE - 20);
   foreach (float turn : turns)
   {
    vector candidate = ACR_Manager.Ground(center + Turned(side, turn) * d);
    float walk = vector.DistanceXZ(candidate, from);
    if (BBB_Terrain.IsWaterAt(candidate) || NearOtherEnemyBase(candidate, target)) continue;
    if (!FireLine(candidate, center)) continue;
    float heightAdvantage = candidate[1] - center[1];
    float score = walk - Math.Max(0.0, heightAdvantage * 2.5);
    if (score < best)
    {
     spot = candidate;
     best = score;
    }
   }
  }
  return best < float.MAX;
 }

 // Aim point for supporting gun vehicles: nearest known enemy around the objective, else the base centre.
 vector SupportAimFor(ACR_Base target)
 {
  if (!target) return vector.Zero;
  vector seenAt;
  if (EnemyNear(target.GetOrigin(), target.m_fRadius + 250, seenAt)) return seenAt;
  return target.GetOrigin();
 }

 // Real turret-height line of sight (terrain AND trees/buildings/walls) so gun vehicles don't park behind woods.
 static bool VehicleFireLine(vector from, vector aim)
 {
  BaseWorld world = GetGame().GetWorld();
  if (!world) return false;
  TraceParam trace = new TraceParam();
  trace.Start = from + Vector(0, 2.2, 0);
  trace.End = aim + Vector(0, 1.5, 0);
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = world.TraceMove(trace, null);
  float length = vector.Distance(trace.Start, trace.End);
  return fraction >= 0.85 || (1 - fraction) * length <= 35;
 }

 // Firing spot for a crewed gun vehicle supporting an assault: tighter standoff and real WORLD|ENTS LOS.
 // minShift > 0 forces repositioning away from a current blind spot.
 bool VehicleOverwatchSpot(ACR_Base target, vector from, vector aimPoint, out vector spot, float minShift = 0)
 {
  if (!target) return false;
  vector center = target.GetOrigin();
  float minRange = Math.Max(110.0, target.m_fRadius + 60.0);
  float maxRange = 340.0;
  float here = vector.DistanceXZ(from, aimPoint);
  if (minShift <= 0 && here >= minRange && here <= 320.0 && !BBB_Terrain.IsWaterAt(from) && VehicleFireLine(from, aimPoint))
  {
   spot = from;
   return true;
  }
  vector side = from - aimPoint;
  side[1] = 0;
  if (side.LengthSq() < 1) side = Vector(0, 0, 1);
  side.Normalize();
  array<float> rings = {150, 200, 250, 300};
  array<float> turns = {0, 15, -15, 30, -30, 45, -45, 60, -60, 75, -75};
  float best = float.MAX;
  foreach (float ring : rings)
  {
   float d = Math.Clamp(ring, minRange, maxRange);
   foreach (float turn : turns)
   {
    vector candidate = ACR_Manager.Ground(aimPoint + Turned(side, turn) * d);
    float walk = vector.DistanceXZ(candidate, from);
    if (walk < minShift || BBB_Terrain.IsWaterAt(candidate) || NearOtherEnemyBase(candidate, target)) continue;
    if (!VehicleFireLine(candidate, aimPoint)) continue;
    float heightAdvantage = candidate[1] - aimPoint[1];
    float score = walk - Math.Max(0.0, heightAdvantage * 3.0);
    if (score < best)
    {
     spot = candidate;
     best = score;
    }
   }
  }
  return best < float.MAX;
 }

 protected void LiftFire(ACR_AIAttack attack, string reason)
 {
  ACR_Squad fire = attack.m_Suppressor;
  if (!fire || !attack.m_Target) return;
  fire.SetMode(ACR_EMoveMode.COMBAT);
  IEntity vFire = ACR_Transport.CrewedVehicle(fire);
  if (vFire && fire.HasGunner(vFire) && reason != "nobody left to cover")
  {
   vector dir = PositionOf(fire) - attack.m_Target.GetOrigin();
   dir[1] = 0;
   if (dir.LengthSq() < 1) dir = Vector(0, 0, 1);
   dir.Normalize();
   vector perimeterSpot = ACR_Manager.Ground(attack.m_Target.GetOrigin() + dir * (attack.m_Target.m_fRadius + 80));
   fire.Defend(perimeterSpot);
   Say(string.Format("[ACR] AI %1: %2 (armed vehicle) holds perimeter standoff at %3 (%4)", m_sFaction, fire.GetName(), attack.m_Target.m_sName, reason));
  }
  else
  {
   fire.Assault(attack.m_Target.GetOrigin());
   Say(string.Format("[ACR] AI %1: %2 lifts fire and joins the assault on %3 (%4)", m_sFaction, fire.GetName(), attack.m_Target.m_sName, reason));
  }
  attack.m_Suppressor = null;
 }

 // Eye height to just above the aim point with only terrain in the way (trees and
 // houses are what suppressive fire is for). The last 30 m don't count: the enemy
 // may sit in a dip or behind a crest at the objective itself.
 static bool FireLine(vector from, vector aim)
 {
  BaseWorld world = GetGame().GetWorld();
  if (!world) return false;
  TraceParam trace = new TraceParam();
  trace.Start = from + Vector(0, 1.6, 0);
  trace.End = aim + Vector(0, 1.5, 0);
  trace.Flags = TraceFlags.WORLD;
  float fraction = world.TraceMove(trace, null);
  float length = vector.Distance(trace.Start, trace.End);
  return fraction >= 1 || (1 - fraction) * length <= 30;
 }

 // True when an assault or grab has gone on too long without taking the base.
 bool Stalled(ACR_AIAttack attack)
 {
  // Every squad left gave up on a spot it can't reach: don't wait out the time limit
  // (Everon night soak 26 Oct: a grab's only squad held "unreachable" for 17 min).
  int live = 0;
  bool allGaveUp = true;
  foreach (ACR_Squad member : attack.m_Squads)
  {
   if (!member || member.Strength() <= 0) continue;
   live++;
   if (!member.GaveUpUnreachable()) allGaveUp = false;
  }
  if (live > 0 && allGaveUp)
  {
   Say(string.Format("[ACR] AI %1: every squad on %2 found it unreachable", m_sFaction, attack.m_Target.m_sName));
   return true;
  }
  if (GrabParkedShort(attack))
  {
   Say(string.Format("[ACR] AI %1: the vehicles grabbing %2 stopped short of it and are not getting closer", m_sFaction, attack.m_Target.m_sName));
   return true;
  }
  if (!attack.m_bAssaulting || attack.m_fAssaultStart <= 0) return false;
  float seconds = (GetGame().GetWorld().GetWorldTime() - attack.m_fAssaultStart) / 1000;
  if (attack.m_bGrab && !s_bGrabByDistance) return seconds > GRAB_TIMEOUT;
  if (attack.m_bGrab) return seconds > Math.Max(GRAB_TIMEOUT, attack.m_fGrabSeconds);
  if (seconds > ASSAULT_HARD_TIMEOUT) return true;
  // The soft limits don't count time BBB's squads spent fighting for it (#101); the hard one does.
  seconds -= attack.m_fBBBFightS;
  if (seconds > ASSAULT_TIMEOUT * 0.5 && attack.m_Target && !attack.m_Target.m_bContested)
  {
   bool nearPerimeter = false;
   int currentStrength = 0;
   foreach (ACR_Squad member : attack.m_Squads)
   {
    currentStrength += member.Strength();
    if (vector.DistanceXZ(PositionOf(member), attack.m_Target.GetOrigin()) < attack.m_Target.m_fRadius + 100) nearPerimeter = true;
   }
   if (!nearPerimeter && currentStrength < DefenceOf(attack.m_Target)) return true;
  }
  if (seconds <= ASSAULT_TIMEOUT) return false;
  foreach (ACR_Squad squad : attack.m_Squads)
   if (vector.DistanceXZ(PositionOf(squad), attack.m_Target.GetOrigin()) < attack.m_Target.m_fRadius) return false;
  return true;
 }

 // #101: what BBB's squad tactics say about this attack's squads (BBB_Tactic.Report). A squad
 // assaulting or flanking that really gains ground, or one holding because it is outnumbered or
 // flanked, is fighting, not stalled: that time comes off the soft time limits (at most
 // BBB_FIGHT_MAX_S; ASSAULT_HARD_TIMEOUT still counts everything). A squad that holds outnumbered
 // or flanked asks for support: mortar fire on where its own men last saw the threat, through
 // the same checks as any other call (ready, in range, no friendlies near the point).
 static const float BBB_FIGHT_MAX_S = 300;
 static const float BBB_SUPPORT_EVERY_S = 90;
 static const float BBB_THREAT_FRESH_S = 20;
 protected void NoteBBBFight(ACR_AIAttack attack) { ACR_AIAttackPlanner.NoteBBBFight(this, attack); }

 protected void PushAttackers(ACR_AIAttack attack) { ACR_AIAttackPlanner.PushAttackers(this, attack); }

 // A line in the server log every 5 minutes: what this commander is doing.
 protected void LogStatus()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextStatus) return;
  m_fNextStatus = now + 300000;
  Print(StatusText());
 }

 // The status line (LogStatus; the AC:A Think view shows it live).
 string StatusText()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_Squad> all = {};
  MySquads(all, false);
  int nFree = 0;
  int nDefend = 0;
  int nAttack = 0;
  int nOther = 0;
  foreach (ACR_Squad squad : all)
  {
   if (squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED) nFree++;
   else if (squad.m_iAITask == ACR_EAITask.DEFEND) nDefend++;
   else if (squad.m_iAITask == ACR_EAITask.ATTACK) nAttack++;
   else nOther++;
  }
  string attacks;
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (!attack.m_Target) continue;
   string phase = "staging";
   if (attack.m_bGrab) phase = "grab";
   else if (attack.m_bAssaulting) phase = "assault";
   else if (attack.m_bPrep) phase = "prep";
   int minutes = 0;
   if (attack.m_fAssaultStart > 0) minutes = (now - attack.m_fAssaultStart) / 60000;
   attacks += string.Format(" %1(%2 %3m)", attack.m_Target.m_sName, phase, minutes);
  }
  array<ACR_Squad> ready = {};
  MySquads(ready, true);
  attacks += string.Format(" | combat-ready idle %1", ready.Count());
  map<string, int> idleAt = new map<string, int>();
  foreach (ACR_Squad idle : ready)
  {
   ACR_Base near = NearestOwnBase(PositionOf(idle));
   string where = "field";
   if (near && vector.DistanceXZ(PositionOf(idle), near.GetOrigin()) < near.m_fRadius + 150) where = near.m_sName;
   int had;
   idleAt.Find(where, had);
   idleAt.Set(where, had + 1);
  }
  foreach (string place, int n : idleAt) attacks += string.Format(" %1:%2", place, n);
  // What the rest of the force is: the AI squad cap counts fighting units only.
  int line, depleted, vehiclesNoGunner, remnants;
  foreach (ACR_Squad member : all)
  {
   if (member.IsCombatUnit())
   {
    if (!IsSpecialistSquad(member)) line++;
    if (!ACR_Transport.CrewedVehicle(member) && member.StrengthRatio() < m_Doctrine.m_fRefitRatio) depleted++;
   }
   else if (ACR_Transport.CrewedVehicle(member)) vehiclesNoGunner++;
   else remnants++;
  }
  attacks += string.Format(" | line %1, depleted %2, refitting %3, remnants %4, unarmed vehicles %5, ammo runs %6", line, depleted, m_Refitting.Count(), remnants, vehiclesNoGunner, m_AmmoRuns.Count());
  int visible;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner != m_sFaction && !base.m_sOwner.IsEmpty() && Visible(base)) visible++;
  attacks += string.Format(" | support budget %1 | doctrine %2", SupportBudget(), m_Doctrine.m_sName) + BuildStatus();
  return string.Format("[ACR] AI %1 status: %2 squads (free %3, defending %4, attacking %5, other %6), attacks:%7, enemy bases known %8, RP %9",
   m_sFaction, all.Count(), nFree, nDefend, nAttack, nOther, attacks, visible, m_Manager.m_Bases.GetRP(m_sFaction));
 }

 // ---------------------------------------------------------------- AC:A Think (ACR_Think)
 // Every "[ACR] AI <side>:" decision goes through Say: printed as before, and the newest
 // THINK_LINES kept (with the match clock) for the host's Think view.
 static const int THINK_LINES = 20;
 protected ref array<string> m_aThink = {};
 void Say(string line)
 {
  // PrintFormat, not Print: Print of a variable logs it as "string line = '...'".
  PrintFormat("%1", line);
  string text = line;
  if (text.StartsWith("[ACR] AI ")) text = text.Substring(9, text.Length() - 9);
  if (text.StartsWith(m_sFaction + ": ")) text = text.Substring(m_sFaction.Length() + 2, text.Length() - m_sFaction.Length() - 2);
  m_aThink.Insert(ACR_Think.Clock() + " " + text);
  if (m_aThink.Count() > THINK_LINES) m_aThink.RemoveOrdered(0);
 }
 void ThinkDecisions(notnull array<string> lines) { lines.Copy(m_aThink); }

 // One line per attack: phase, squads, time in that phase and where it forms up.
 void ThinkAttacks(notnull array<string> lines)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (!attack.m_Target) continue;
   string kind = "attack";
   if (attack.m_bGrab) kind = "grab";
   else if (attack.m_bCounter) kind = "counter";
   else if (attack.m_bFobRaid) kind = "FOB raid";
   string phase;
   if (attack.m_bAssaulting || attack.m_bGrab) phase = string.Format("assault %1 min", (int)((now - attack.m_fAssaultStart) / 60000));
   else if (attack.m_bPrep) phase = string.Format("prep, %1 s left", (int)Math.Max(0, (attack.m_fPrepUntil - now) / 1000));
   else phase = string.Format("staging, %1 s left", (int)Math.Max(0, (attack.m_fStageDeadline - now) / 1000));
   string grids;
   foreach (vector stage : attack.m_aStaging)
   {
    string grid = ACR_Manager.Grid(stage);
    if (!grids.Contains(grid)) grids += " " + grid;
   }
   if (grids.IsEmpty()) grids = " " + ACR_Manager.Grid(attack.m_Staging);
   string fire = "none";
   if (attack.m_Suppressor) fire = attack.m_Suppressor.GetName();
   lines.Insert(string.Format("%1 %2: %3, %4 squad(s), staging%5, base of fire %6", kind, attack.m_Target.m_sName, phase, attack.m_Squads.Count(), grids, fire));
   if (attack.m_Ground) lines.Insert("  " + attack.m_Ground.Summary(attack.m_aStaging));
  }
  if (m_Refitting.Count() > 0)
  {
   string refit;
   foreach (ACR_Squad squad, float until : m_Refitting)
    if (squad) refit += string.Format(" %1 (%2 s)", squad.GetName(), (int)Math.Max(0, (until - now) / 1000));
   lines.Insert("refitting:" + refit);
  }
  ThinkAmmoRuns(lines);
 }

 protected void UpdateAttacks() { ACR_AIAttackPlanner.UpdateAttacks(this); }

 // ---------------------------------------------------------------- 4. idle squads

 // Posts a squad at a front base (PostIdle, the allocator).
 protected void PostTo(ACR_Squad squad, ACR_Base base)
 {
  squad.SetAITask(ACR_EAITask.POSTED);
  squad.SetMode(ACR_EMoveMode.FAST);
  Travel(squad, base.GetOrigin(), true, 0);
  Say(string.Format("[ACR] AI %1: %2 posted to %3", m_sFaction, squad.GetName(), base.m_sName));
 }

 // Makes a squad the reserve at 'home' (AssignAttacks, the allocator).
 void HoldReserve(ACR_Squad reserve, ACR_Base home)
 {
  m_ReserveSquad = reserve;
  reserve.SetAITask(ACR_EAITask.POSTED);
  reserve.SetMode(ACR_EMoveMode.COMBAT);
  Travel(reserve, home.GetOrigin(), true, 0);
  Say(string.Format("[ACR] AI %1: %2 held in reserve at %3", m_sFaction, reserve.GetName(), home.m_sName));
 }

 protected void PostIdle()
 {
  array<ACR_Base> front = {};
  FrontlineBases(front);
  RearOut(front);
  if (front.IsEmpty()) return;
  array<ACR_Squad> free = {};
  MySquads(free, true);
  foreach (ACR_Squad squad : free)
  {
   if (squad.m_iAITask == ACR_EAITask.POSTED) continue;
   if (IsReconTeam(squad))
   {
    ACR_Base enemyBase = NearestEnemyBase(PositionOf(squad));
    if (enemyBase)
    {
     vector vSpot;
     if (OverwatchSpot(enemyBase, PositionOf(squad), vSpot))
     {
      squad.SetAITask(ACR_EAITask.POSTED);
      squad.SetMode(ACR_EMoveMode.SILENT);
      squad.Defend(vSpot);
      Say(string.Format("[ACR] AI %1: recon team %2 posted to overwatch at %3 (%4 m)", m_sFaction, squad.GetName(), enemyBase.m_sName, (int)vector.DistanceXZ(vSpot, enemyBase.GetOrigin())));
      continue;
     }
    }
   }
   // At a rear base (new squads at the HQ): up to the front as MoveUp takes posted ones, by truck or
   // base to base on foot, never a 13 km walk (soak 6 Nov: a US team set off on foot for 13196 m).
   ACR_Base standing = NearestOwnBase(PositionOf(squad));
   if (standing && vector.DistanceXZ(PositionOf(squad), standing.GetOrigin()) <= standing.m_fRadius + DEFENSE_DISTANCE && RearBase(standing))
   {
    bool upByTruck;
    ACR_Base upTo = MoveUpTo(squad, EnemyGap(standing), upByTruck);
    if (upTo) PostTo(squad, upTo);
    else squad.SetAITask(ACR_EAITask.POSTED); // waits for a truck or a nearer base (MoveUp)
    continue;
   }
   // Nearest frontline base, but spread out: each squad already posted
   // there counts as 400 m farther, so idle troops don't pile up in one place.
   ACR_Base nearest;
   float best = float.MAX;
   float bestScore = float.MAX;
   foreach (ACR_Base base : front)
   {
    float d = vector.DistanceXZ(PositionOf(squad), base.GetOrigin());
    float score = d + PostedAt(base) * 400;
    // The lane's front bases draw idle troops: they mass behind the next attack.
    if (LaneActive() && m_Lane.Contains(base.GetOrigin())) score -= ACR_AILanes.POST_PULL;
    if (score < bestScore) { bestScore = score; best = d; nearest = base; }
   }
   if (!nearest || best < 150)
   {
    squad.SetAITask(ACR_EAITask.POSTED);
    continue;
   }
   PostTo(squad, nearest);
  }
  PositionTransports();
 }

 // ---------------------------------------------------------------- 4a. moving up
 // Squads posted at a quiet base far behind the front move up toward it (Jacob, 6 Nov: "squads
 // weren't really getting around"). Soak 6 Nov (Chernarus, US AI): it held 21 bases to 4 and had 9
 // line squads idle at bases 6-9 km back (Pulkovo, Vybor), posted there while those faced empty
 // bases, as the attacks on the last enemy bases went short of strength in reach. A posted squad is
 // never walked to another post (4 Nov: squads paced 2-3 km between two posts and back); this goes
 // one way only: from a rear base with no threat to a base at least MOVEUP_GAIN nearer the enemy, so
 // it can't swing back while the enemy's bases stand. Far moves go by truck; without one, in hops of
 // at most MOVEUP_WALK on foot from base to base (CatchRides still picks them up on the way).
 // "Rear" is judged against the enemy bases this side knows of, never empty ones (FrontlineBases
 // counts those too): PostIdle and the allocator's hold jobs leave rear bases out as well.
 static const float MOVEUP_INTERVAL_MS = 30000; // one squad moves up at a time, this often
 static const float MOVEUP_DEPTH = 2500;        // m: a base this much farther from the enemy than our nearest is the rear
 static const float MOVEUP_GAIN = 1500;         // m: a move up gets at least this much nearer the enemy
 static const float MOVEUP_WALK = 3000;         // m: farthest a move up goes on foot in one hop
 static const float MOVEUP_CALM = 500;          // m: a squad with an enemy it knows of this close stays put
 float m_fNextMoveUp = -1;
 float m_fFrontGap = -1;              // m from our base nearest the enemy to it, this tick (-1: no enemy base known)

 protected float EnemyGap(ACR_Base base) { return ACR_AIScreens.EnemyGap(this, base); }

 protected void UpdateFrontGap() { ACR_AIScreens.UpdateFrontGap(this); }

 protected bool RearBase(ACR_Base base) { return ACR_AIScreens.RearBase(this, base); }

 protected void RearOut(array<ACR_Base> posts) { ACR_AIScreens.RearOut(this, posts); }

 protected void MoveUp() { ACR_AIScreens.MoveUp(this); }

 protected ACR_Base MoveUpTo(ACR_Squad squad, float fromGap, out bool byTruck) { return ACR_AIScreens.MoveUpTo(this, squad, fromGap, byTruck); }

 static int MoveUpPick(notnull array<float> gaps, notnull array<float> dists, notnull array<float> scores, float fromGap, float frontGap, bool truck, out bool atFront) { return ACR_AIScreens.MoveUpPick(gaps, dists, scores, fromGap, frontGap, truck, atFront); }

 // ---------------------------------------------------------------- 4b. the screen
 // -acrDefenceForecast 2 (defence step 2; Jacob, 4 Nov: the forecast is blind without eyes forward).
 // Our front bases an attack is likeliest on (the forecast's expected strength, and the enemy's lanes
 // through them before anything is seen) get a screen: a recon team, else the smallest free squad,
 // posted outside the base where it sees most of the way the attack would come (ACR_DefenceScreen).
 // It watches and reports (its sightings are the side's intel like any squad's); with an enemy close or
 // men lost it pulls back to the base and defends it, and the base isn't screened again for a while.
 // A screen is out of the free pool (MySquads), so attacks, posts and the allocator leave it be.
 static const int SCREEN_SQUADS_PER = 5;      // one screen per this many combat squads ...
 static const int SCREEN_MOST = 3;            // ... at most
 static const float SCREEN_FRONT = 2500;      // m: only bases this near the enemy are screened
 static const float SCREEN_LANE_WEIGHT = 4;   // a base on the enemy's likeliest lane counts as 4 expected
 static const float SCREEN_LEAST = 0.5;       // weight below which a base isn't screened
 static const float SCREEN_REACH = 2000;      // m a squad is sent to a screen from at most
 static const float SCREEN_CONTACT = 300;     // m: an enemy it knows of this close and it pulls back
 static const float SCREEN_HURT = 0.7;        // ... or below this share of its men
 static const float SCREEN_RESITE_ANGLE = 60; // degrees the way must swing before the screen moves
 static const float SCREEN_RESITE_MS = 120000;
 static const float SCREEN_QUIET_MS = 180000; // a base no longer wanted this long: its screen is released
 static const float SCREEN_AGAIN_MS = 120000; // a base whose screen pulled back isn't screened again for this long
 ref array<ref ACR_ScreenPost> m_Screens = {};
 ref array<ref ACR_ScreenPost> m_ReconPosts = {};
 ref map<ACR_Base, float> m_ReconAgain = new map<ACR_Base, float>();
 ref map<ACR_Base, float> m_ScreenAgain = new map<ACR_Base, float>();

 bool IsScreen(ACR_Squad squad) { return ACR_AIScreens.IsScreen(this, squad); }

 ACR_ScreenPost ScreenOf(ACR_Base base) { return ACR_AIScreens.ScreenOf(this, base); }

 protected void ManageScreens() { ACR_AIScreens.ManageScreens(this); }

 protected void ScreensWanted(notnull array<ACR_Base> enemyBases, float now, notnull array<ACR_Base> wanted) { ACR_AIScreens.ScreensWanted(this, enemyBases, now, wanted); }

 protected float ScreenBearing(ACR_Base base, notnull array<ACR_Base> enemyBases) { return ACR_AIScreens.ScreenBearing(this, base, enemyBases); }

 protected ACR_Squad ScreenSquad(vector spot) { return ACR_AIScreens.ScreenSquad(this, spot); }

 protected void SendScreen(ACR_ScreenPost post, ACR_Squad squad, vector spot, float bearing, int sees, float now, string what) { ACR_AIScreens.SendScreen(this, post, squad, spot, bearing, sees, now, what); }

 protected void PullBackScreen(ACR_ScreenPost post, string why, float now) { ACR_AIScreens.PullBackScreen(this, post, why, now); }

 void DefenceTickForTest() { ACR_AIScreens.DefenceTickForTest(this); }

 void ThinkScreens(notnull array<string> lines) { ACR_AIScreens.ThinkScreens(this, lines); }

 // Keep idle unarmed transport trucks near safe bases where foot infantry spawn or stage,
 // while leaving their order state as RETURN so IsFreeTransport can claim them en route.
 // At most MOTOR_POOL_PER_HUB park at one base; the rest go to the next nearest hub with room
 // (Sly's match, Nov: a dozen trucks in rows at the HQ, one rocket run took them all).
 static const int MOTOR_POOL_PER_HUB = 2;
 static const float MOTOR_POOL_PARKED = 450; // m from a hub's centre: parked there
 protected void PositionTransports() { ACR_AIScreens.PositionTransports(this); }

 protected static int HubAt(notnull array<ACR_Base> hubs, vector pos) { return ACR_AIScreens.HubAt(hubs, pos); }

 // A staging point around the target, as close to the wanted direction as
 // possible, that is dry, has a dry run in, and stays clear of every other
 // enemy base we know of (forming up next to one drags the force into a fight
 // it didn't plan).
 bool StagingPoint(ACR_Base target, vector dir, float ring, out vector point)
 {
  array<float> turns = {0, 30, -30, 60, -60, 90, -90, 120, -120, 150, -150, 180};
  foreach (float turn : turns)
  {
   float angle = turn * Math.DEG2RAD;
   vector rotated = Vector(dir[0] * Math.Cos(angle) - dir[2] * Math.Sin(angle), 0, dir[0] * Math.Sin(angle) + dir[2] * Math.Cos(angle));
   vector candidate;
   if (!BBB_Terrain.DryApproach(target.GetOrigin(), rotated, ring, candidate)) continue;
   if (NearOtherEnemyBase(candidate, target)) continue;
   point = candidate;
   return true;
  }
  return BBB_Terrain.DryApproach(target.GetOrigin(), dir, ring, point);
 }

 // ---------------------------------------------------------------- covered approaches
 // Jacob 26 Oct (Provins): a squad walking to its flank staging point took the shortest path,
 // across the front of the objective in full view, while a route round behind the rise was
 // hidden. A foot squad's way to its staging point is now chosen from the direct line and an arc
 // round the objective either way, by length plus the stretch the objective's ground can see
 // (terrain only: dead ground; houses and trees aren't counted). Rides and air lifts keep
 // their own routes; contact on the way is BBB's (combat manoeuvre).
 static const float COVER_ARC_STEP = 25;       // degrees between arc waypoints
 static const float COVER_SAMPLE = 40;         // m between sight checks along a path
 static const float COVER_RANGE = 800;         // m from the objective beyond which nothing counts as seen
 static const float COVER_EXPOSED_WEIGHT = 4;  // an exposed metre costs this many extra metres
 static const float COVER_SPIRAL_OUT = 250;    // m past the staging ring an arc may start
 static const float COVER_MAX_STRETCH = 2.5;   // an arc longer than this times the direct way (+300 m) isn't walked
 static const float COVER_MIN_GAIN = 0.15;     // the arc must cut the cost by this share
 static const float COVER_WALK_SPEED = 2;      // m/s on foot, for the forming-up wait
 float m_fCoverDetour;                         // extra metres of the last covered route Travel chose (0: direct)
 ACR_AIAttack m_LaunchingAttack;               // set while LaunchAttack sends its squads off
 static ref array<float> s_aApproachTurns;
 static void ACR_InitStatics()
 {
  s_aApproachTurns = {0, 60, -60, 115, -115};
 }
 static const float APPROACH_SEPARATION = 40;

 // Covered approach & approach-line geometry lives in ACR_AIApproach.c; thin forwarders preserve existing callers/tests.
 static void ObjectiveEyes(vector center, float radius, notnull array<vector> eyes) { ACR_AIApproach.ObjectiveEyes(center, radius, eyes); }
 static vector EyeAt(vector point) { return ACR_AIApproach.EyeAt(point); }
 static bool GroundSeen(notnull array<vector> eyes, vector point) { return ACR_AIApproach.GroundSeen(eyes, point); }
 static float PathExposure(notnull array<vector> eyes, vector from, notnull array<vector> legs, out float exposed) { return ACR_AIApproach.PathExposure(eyes, from, legs, exposed); }
 static void ArcLegs(vector center, vector from, vector stage, float ring, int sense, notnull array<vector> legs) { ACR_AIApproach.ArcLegs(center, from, stage, ring, sense, legs); }
 protected ACR_Base BypassFor(ACR_Base target) { return ACR_AIApproach.BypassFor(this, target); }
 protected bool CoveredApproach(ACR_Squad squad, ACR_Base target, vector stage) { return ACR_AIApproach.CoveredApproach(this, squad, target, stage); }
 protected vector HiddenStaging(ACR_Base target, vector point, array<vector> others = null) { return ACR_AIApproach.HiddenStaging(this, target, point, others); }
 static vector Turned(vector dir, float degrees) { return ACR_AIApproach.Turned(dir, degrees); }
 static float TurnBetween(vector a, vector b) { return ACR_AIApproach.TurnBetween(a, b); }
 int PickApproach(ACR_AIAttack attack, vector from, out vector stage) { return ACR_AIApproach.PickApproach(this, attack, from, stage); }
 static bool ApartFrom(vector center, vector point, notnull array<vector> others) { return ACR_AIApproach.ApartFrom(center, point, others); }
 static int ApproachLines(ACR_AIAttack attack) { return ACR_AIApproach.ApproachLines(attack); }
 protected bool NearOtherEnemyBase(vector point, ACR_Base except) { return ACR_AIApproach.NearOtherEnemyBase(this, point, except); }

 // Crews that ended up on foot next to their own intact, empty vehicle get
 // back in (the game's AI sometimes bails out and leaves a BTR standing).
 protected ref map<ACR_Squad, float> m_NextRecrew = new map<ACR_Squad, float>();
 // Tries since it was last aboard. A crew that can't get in isn't told again every 90 s for good:
 // each try is a new order, which ends what it was doing (Everon long match 31 Oct: a Humvee's crew,
 // 14 times in 20 min, a base grab cut short on the way). It stays on foot for a while instead.
 protected ref map<ACR_Squad, int> m_RecrewTries = new map<ACR_Squad, int>();
 static const int RECREW_TRIES = 3;
 static const float RECREW_REST_MS = 600000;

 // Recrew would put this crew back into 'vehicle' (its own): intact, nobody else aboard, fuel left,
 // within 400 m, no enemy within 120 m ('nearEnemy' leaves that out: it gets back in once they are
 // gone), not stuck in the last 10 minutes (the crew bailed, got back in, got stuck again).
 protected bool RecrewFits(ACR_Squad squad, IEntity vehicle, bool driverless, float now, bool nearEnemy = false)
 {
  if (ACR_Logistics.Health(vehicle) < 0.25 || (!driverless && !ACR_Vehicles.IsEmpty(vehicle))) return false;
  // On its side or roof: nobody gets in and out of a wreck it can't drive (nothing rights one yet).
  if (SCR_AIVehicleUsability.IsVehicleFlipped(vehicle)) return false;
  // Run dry (the game's AI gets out of a car with no fuel): wait for the fuel truck, not in and
  // out again (Jacob's 27 Oct game: a Humvee crew left, got back in and left again three times).
  float fuel = ACR_Logistics.Fuel(vehicle);
  if (fuel >= 0 && fuel < 0.02) return false;
  if (vector.DistanceXZ(PositionOf(squad), vehicle.GetOrigin()) > 400) return false;
  if (!nearEnemy && squad.KnownEnemies(120) > 0) return false;
  return now - squad.m_fStuckAt >= 600000;
 }

 // A crew off its own vehicle that Recrew is putting back in (between its tries too, and while an
 // enemy close by holds it off; not while it rests after RECREW_TRIES): not free for other orders
 // until it is aboard, when its orders go by vehicle. Match 6 Nov (Chernarus): the FV510 Warrior's crew, out of its 98% IFV, was in one tick
 // posted to a town 2.9 km off on foot, put in a house there, given a truck ride to the front and
 // sent back to the IFV; it lost the IFV and died on foot.
 protected bool Remounting(ACR_Squad squad)
 {
  IEntity vehicle = squad.m_OwnVehicle;
  if (!vehicle || squad.m_iAITask == ACR_EAITask.RESUPPLY) return false;
  bool driverless = squad.m_bDriverBailed && squad.IsMounted() && !ACR_Squad.DriverOf(vehicle);
  if (squad.IsMounted() && !driverless) return false;
  float now = GetGame().GetWorld().GetWorldTime();
  float next;
  if (m_NextRecrew.Find(squad, next) && next - now > 90000) return false; // resting on foot
  return RecrewFits(squad, vehicle, driverless, now, true);
 }

 protected void Recrew()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  array<ACR_Squad> mine = {};
  MySquads(mine, false);
  foreach (ACR_Squad squad : mine)
  {
   IEntity vehicle = squad.m_OwnVehicle;
   if (!vehicle || squad.m_iAITask == ACR_EAITask.RESUPPLY) continue;
   // Or its driver bailed and only part of the crew is back aboard: nobody drives, so every
   // move fails where it stands (ACR_Squad.m_bDriverBailed).
   bool driverless = squad.m_bDriverBailed && squad.IsMounted() && !ACR_Squad.DriverOf(vehicle);
   if (squad.IsMounted() && !driverless)
   {
    m_RecrewTries.Remove(squad);
    continue;
   }
   if (!RecrewFits(squad, vehicle, driverless, now)) continue;
   float next;
   if (m_NextRecrew.Find(squad, next) && now < next) continue;
   // Driverless: one man for the driver's seat (the group's Get in seats nobody while part of
   // the crew is aboard); its order stands. Tried again in 30 s if he isn't in by then.
   if (driverless)
   {
    m_NextRecrew.Set(squad, now + 30000);
    if (squad.ReseatDriver(vehicle)) Say(string.Format("[ACR] AI %1: %2's driver gets back into its vehicle", m_sFaction, squad.GetName()));
    continue;
   }
   int tries;
   m_RecrewTries.Find(squad, tries);
   if (tries >= RECREW_TRIES)
   {
    m_RecrewTries.Remove(squad);
    m_NextRecrew.Set(squad, now + RECREW_REST_MS);
    Say(string.Format("[ACR] AI %1: %2 can't get back into its vehicle (%3 tries), on foot for %4 min", m_sFaction, squad.GetName(), tries, (int)(RECREW_REST_MS / 60000)));
    continue;
   }
   m_RecrewTries.Set(squad, tries + 1);
   m_NextRecrew.Set(squad, now + 90000);
   if (squad.GetIn(vehicle.GetOrigin())) Say(string.Format("[ACR] AI %1: %2 gets back into its vehicle", m_sFaction, squad.GetName()));
  }
 }

 void RecrewForTest() { Recrew(); }
 void RecrewSoonForTest(ACR_Squad squad) { m_NextRecrew.Remove(squad); }
 float RecrewNextForTest(ACR_Squad squad)
 {
  float next;
  m_NextRecrew.Find(squad, next);
  return next;
 }

 int PostedAt(ACR_Base base)
 {
  int count;
  array<ACR_Squad> mine = {};
  MySquads(mine, true, true);
  // (and those on their way there: postings spread out as they are made, not once squads arrive)
  foreach (ACR_Squad squad : mine)
   if (squad.m_iAITask == ACR_EAITask.POSTED && (vector.DistanceXZ(PositionOf(squad), base.GetOrigin()) < base.m_fRadius + 150 || CommittedTo(squad, base.GetOrigin(), base.m_fRadius + 150))) count++;
  return count;
 }

 // ---------------------------------------------------------------- mortar teams
 // One working team at a time, bought once troops aren't short. While no base that isn't
 // ours is in reach it goes to the own base nearest the front (setting up by itself on
 // arrival), so it follows the front as it moves.
 static const float MORTAR_MOVE_MS = 120000;
 static const float MORTAR_MIN_STANDOFF = 400; // don't set up closer than this to an enemy base
 protected float m_fNextMortarMove;

 bool BuyMortarTeam(ACR_Base spawn, ACR_FactionData data)
 {
  array<ACR_MortarTeam> teams = {};
  m_Manager.Mortars().TeamsOf(m_sFaction, teams);
  foreach (ACR_MortarTeam team : teams) if (!team.m_bLost) return false;
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (!entry.m_bMortarTeam || entry.m_iRemaining <= 0) continue;
   if (Purse(data) < entry.m_iCost + SUPPORT_RESERVE) return false;
   Say(string.Format("[ACR] AI %1 requisition (mortar team): %2", m_sFaction, m_Manager.PurchaseAt(m_sFaction, i, spawn)));
   return true;
  }
  return false;
 }

 // One support gunship on a helipad (worlds with pads), bought again when lost. With a content
 // pack's attack helicopter on the buy list, that one instead (still one helicopter).
 bool BuyHelicopter(ACR_FactionData data)
 {
  if (!ACR_HeliStation.UsesPads(m_sFaction) || !m_Manager.m_Supports) return false;
  ACR_HeliStation station = m_Manager.m_Supports.Station();
  if (station.CountOf(m_sFaction, true) > 0 || station.CountOf(m_sFaction, true, true) > 0) return false;
  ACR_Helipad pad = station.FreePad(m_sFaction, null);
  if (!pad) return false;
  ACR_Base base = pad.Base();
  if (!ACR_LogisticsRules.CanDeploy(base) || base.m_bContested) return false;
  int pick = -1;
  foreach (int i, ACR_RequisitionEntry entry : data.m_aRequisitions)
  {
   if (!ACR_Requisition.Available(data, entry)) continue;
   if (entry.m_iSupportHeli == 3) { pick = i; break; }
   if (entry.m_iSupportHeli == 1 && pick < 0) pick = i;
  }
  if (pick < 0 || Purse(data) < data.m_aRequisitions[pick].m_iCost + SUPPORT_RESERVE) return false;
  string kind = "support gunship";
  if (data.m_aRequisitions[pick].m_iSupportHeli == 3) kind = "attack helicopter";
  Say(string.Format("[ACR] AI %1 requisition (%2): %3", m_sFaction, kind, m_Manager.PurchaseAt(m_sFaction, pick, base)));
  return true;
 }

 // What a mortar is there to cover: a base it knows the enemy (or a pocket) holds, or one this
 // side is attacking. Unclaimed sites don't count (Everon night soak 26 Oct: hidden unclaimed
 // sites next to each HQ kept both teams there all game; one fire mission in an hour).
 protected bool MortarTarget(ACR_Base base)
 {
  if (base.m_sOwner == m_sFaction) return false;
  foreach (ACR_AIAttack attack : m_Attacks) if (attack.m_Target == base) return true;
  return HostileOwner(base);
 }

 // The own, safe base closest to what the mortar covers (but not right on top of any base
 // that isn't ours).
 protected ACR_Base MortarBase()
 {
  ACR_Base best;
  float bestFront = float.MAX;
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_sOwner != m_sFaction || base.m_bContested || ThreatTo(base) > 0) continue;
   float blockedUntil;
   if (m_MortarUnreachable.Find(base, blockedUntil) && now < blockedUntil) continue;
   float front = float.MAX;
   float nearest = float.MAX;
   foreach (ACR_Base other : ACR_Base.s_All)
   {
    if (other.m_sOwner == m_sFaction) continue;
    float d = vector.DistanceXZ(base.GetOrigin(), other.GetOrigin());
    nearest = Math.Min(nearest, d);
    if (MortarTarget(other)) front = Math.Min(front, d);
   }
   if (nearest < MORTAR_MIN_STANDOFF || front >= bestFront) continue;
   bestFront = front;
   best = base;
  }
  return best;
 }

 // Where a team was last sent (MortarBase), that order's serial and when. A team that gives up on
 // the way (unreachable) leaves that base out for MORTAR_UNREACHABLE_MS: Jacob's 27 Oct game sent
 // Red's mortar to Bovemont eight times in 30 min, and each time it packed up, failed to path
 // there within a minute and set up again at home. Packing up halts and restores the order, so
 // the serial only bounds it from below (ACR_Squad.GaveUpSince).
 static const float MORTAR_UNREACHABLE_MS = 900000;
 static const float MORTAR_SENT_MS = 600000;
 protected ref map<ACR_Squad, ACR_Base> m_MortarSentTo = new map<ACR_Squad, ACR_Base>();
 protected ref map<ACR_Squad, int> m_MortarSentSerial = new map<ACR_Squad, int>();
 protected ref map<ACR_Squad, float> m_MortarSentAt = new map<ACR_Squad, float>();
 protected ref map<ACR_Base, float> m_MortarUnreachable = new map<ACR_Base, float>();

 // Every AI tick, before PositionMortars' time gate: a team that gives up sets up where it is
 // within the next two minutes.
 protected void NoteMortarGiveUps(float now)
 {
  array<ACR_Squad> sent = {};
  foreach (ACR_Squad sentSquad, ACR_Base sentBase : m_MortarSentTo) sent.Insert(sentSquad);
  foreach (ACR_Squad squad : sent)
  {
   ACR_Base base = m_MortarSentTo.Get(squad);
   bool gaveUp = squad && base && squad.GaveUpSince(m_MortarSentSerial.Get(squad));
   if (gaveUp)
   {
    m_MortarUnreachable.Set(base, now + MORTAR_UNREACHABLE_MS);
    Say(string.Format("[ACR] AI %1 mortar: %2 can't reach %3, not sending a mortar there for %4 min", m_sFaction, squad.GetName(), base.m_sName, (int)(MORTAR_UNREACHABLE_MS / 60000)));
   }
   // Done with it: given up, gone, set up at the base, or long enough ago.
   bool done = gaveUp || !squad || !base || now - m_MortarSentAt.Get(squad) > MORTAR_SENT_MS;
   if (!done)
   {
    ACR_MortarTeam team = m_Manager.Mortars().TeamOf(squad);
    done = !team || (team.m_eState == ACR_EMortarState.DEPLOYED && vector.DistanceXZ(team.m_Spot, base.GetOrigin()) < 80); // sent within 20 m (and only from radius + 80 out)
   }
   if (!done) continue;
   m_MortarSentTo.Remove(squad);
   m_MortarSentSerial.Remove(squad);
   m_MortarSentAt.Remove(squad);
  }
 }

 // srv:mortargiveup: a send as PositionMortars makes it, the give-up check, and the block.
 void SendMortarForTest(ACR_Squad squad, ACR_Base base)
 {
  squad.MoveTo(ACR_Manager.Ground(base.GetOrigin()));
  m_MortarSentTo.Set(squad, base);
  m_MortarSentSerial.Set(squad, squad.m_iOrderSerial);
  m_MortarSentAt.Set(squad, GetGame().GetWorld().GetWorldTime());
 }
 void NoteMortarGiveUpsForTest() { NoteMortarGiveUps(GetGame().GetWorld().GetWorldTime()); }
 bool MortarBlockedForTest(ACR_Base base)
 {
  float until;
  return m_MortarUnreachable.Find(base, until) && GetGame().GetWorld().GetWorldTime() < until;
 }

 // A team under counter-battery fire (ACR_CounterBattery.UnderFire) packs up and moves well away
 // from where the rounds came from, so the estimate the enemy built goes stale. Not held back by
 // MORTAR_MOVE_MS: the rounds are landing now. A team that is still firing finishes its mission first.
 protected void DisplaceUnderFire(float now)
 {
  array<ACR_MortarTeam> teams = {};
  m_Manager.Mortars().TeamsOf(m_sFaction, teams);
  foreach (ACR_MortarTeam team : teams)
  {
   ACR_Squad squad = team.m_Squad;
   if (!squad || squad.PlayerSteers() || team.m_bLost || team.m_bFiring || team.m_eState != ACR_EMortarState.DEPLOYED) continue;
   if (m_bHandedOnly && (squad.m_bClaimed || squad.m_Commander != 0)) continue; // a team the player ordered is theirs
   vector from;
   if (!ACR_CounterBattery.UnderFire(team, now, from)) continue;
   vector away = ACR_CounterBattery.DisplaceSpot(team, from);
   team.m_fIncomingAt = 0;
   team.m_iFired = 0;
   team.m_fLastFinished = 0;
   squad.MoveTo(away);
   m_fNextMortarMove = now + MORTAR_MOVE_MS;
   Say(string.Format("[ACR] AI %1 mortar: %2 under counter-battery fire, displacing %3 m", m_sFaction, squad.GetName(), Math.Round(vector.DistanceXZ(team.m_Spot, away))));
  }
 }

 void DisplaceUnderFireForTest(float now) { DisplaceUnderFire(now); }

 protected void PositionMortars()
 {
  float now = GetGame().GetWorld().GetWorldTime();
  NoteMortarGiveUps(now);
  DisplaceUnderFire(now);
  if (now < m_fNextMortarMove) return;
  array<ACR_MortarTeam> teams = {};
  m_Manager.Mortars().TeamsOf(m_sFaction, teams);
  foreach (ACR_MortarTeam team : teams)
  {
   ACR_Squad squad = team.m_Squad;
   if (!squad || squad.PlayerSteers() || team.m_bLost || team.m_bFiring) continue;
   if (m_bHandedOnly && (squad.m_bClaimed || squad.m_Commander != 0)) continue; // the lazy commander's 2IC: a team the player ordered is theirs
   if (team.m_eState == ACR_EMortarState.PACKING || team.m_eState == ACR_EMortarState.DEPLOYING) continue;
   // Shoot-and-scoot: after firing a mission, displace slightly so counter-battery misses
   if (team.m_eState == ACR_EMortarState.DEPLOYED && team.m_iFired > 0 && !team.m_bFiring && team.m_fLastFinished > 0 && (now / 1000) > team.m_fLastFinished + 15.0)
   {
    team.m_iFired = 0;
    team.m_fLastFinished = 0;
    vector scoot = ACR_Manager.Ground(team.m_Spot + Vector(Math.RandomFloat(-40, 40), 0, Math.RandomFloat(-40, 40)));
    squad.MoveTo(scoot);
    m_fNextMortarMove = now + MORTAR_MOVE_MS;
    Say(string.Format("[ACR] AI %1 mortar: %2 shoot-and-scoot displacement", m_sFaction, squad.GetName()));
    continue;
   }
   vector here = PositionOf(squad);
   if (team.m_eState == ACR_EMortarState.DEPLOYED) here = team.m_Spot;
   else if (ACR_Mortars.IsMoving(squad, here)) continue; // on its way
   // Stays while it can reach something it covers (with some margin).
   float reach = ACR_Mortars.MaxRange(team.m_sTube) * 0.85;
   bool inReach = false;
   foreach (ACR_Base other : ACR_Base.s_All)
    if (MortarTarget(other) && vector.DistanceXZ(here, other.GetOrigin()) < reach) inReach = true;
   if (inReach) continue;
   ACR_Base front = MortarBase();
   if (!front || vector.DistanceXZ(here, front.GetOrigin()) < front.m_fRadius + 80) continue;
   vector spot = ACR_Manager.Ground(front.GetOrigin() + Vector(Math.RandomFloat(-20, 20), 0, Math.RandomFloat(-20, 20)));
   squad.MoveTo(spot);
   m_MortarSentTo.Set(squad, front);
   m_MortarSentSerial.Set(squad, squad.m_iOrderSerial);
   m_MortarSentAt.Set(squad, now);
   m_fNextMortarMove = now + MORTAR_MOVE_MS;
   Say(string.Format("[ACR] AI %1 mortar: %2 moving to %3", m_sFaction, squad.GetName(), front.m_sName));
  }
 }

 // ---------------------------------------------------------------- 5. requisition

 protected int FullSizeOf(ACR_Squad squad) { return ACR_AIRequisition.FullSizeOf(this, squad); }

 static bool IsSmallTeamEntry(ACR_RequisitionEntry entry) { return ACR_AIRequisition.IsSmallTeamEntry(entry); }

 float NoteRefitGaveUp(ACR_Squad squad, ACR_Base base, float now) { return ACR_AIRequisition.NoteRefitGaveUp(this, squad, base, now); }
 int RefitGiveUps(ACR_Squad squad) { return ACR_AIRequisition.RefitGiveUps(this, squad); }
 bool RefitGaveUpOn(ACR_Squad squad, ACR_Base base, float now) { return ACR_AIRequisition.RefitGaveUpOn(this, squad, base, now); }
 void ForgetRefit(ACR_Squad squad) { ACR_AIRequisition.ForgetRefit(this, squad); }

 protected bool ReinforceDepleted(ACR_FactionData data, int keep) { return ACR_AIRequisition.ReinforceDepleted(this, data, keep); }

 protected void Requisition() { ACR_AIRequisition.Requisition(this); }

 static float QualityWeight(ACR_RequisitionEntry entry, float factor) { return ACR_AIRequisition.QualityWeight(entry, factor); }

 // Share of the AI squad cap a fighting unit takes: one, or half for a 2-3 man team of a
 // doctrine built on them (Raider), so it fields about as many men as the others
 // (the soldier cap still bounds the total).
 // Light rides (20 Nov, Jacob: "I don't see the AI buying jeeps or HMMWVs for rides at all"). Before
 // this the AI bought only trucks (m_bTransport: the catalog's VEHICLE_TRUCK), and the unit pick skips
 // unarmed cars, so an unarmed UAZ, M151 or HMMWV was never bought. Small parties now get one; trucks
 // still take squads. At most MAX_LIGHT_RIDES (one more at high threat); they don't count against
 // the squad cap (no gun: ACR_Squad.IsCombatUnit) nor the truck quota (under 4 seats).
 static const int MAX_LIGHT_RIDES = 2;

 static bool IsLightRide(ACR_RequisitionEntry entry) { return ACR_AIRequisition.IsLightRide(entry); }

 int CountLightRides(ACR_FactionData data) { return ACR_AIRequisition.CountLightRides(this, data); }

 protected bool BuyLightRide(ACR_FactionData data, ACR_Base spawn, bool urgent) { return ACR_AIRequisition.BuyLightRide(this, data, spawn, urgent); }

 protected float CapSlots(ACR_Squad squad) { return ACR_AIRequisition.CapSlots(this, squad); }
 void RequisitionForTest(int needs) { ACR_AIRequisition.RequisitionForTest(this, needs); }

 protected bool IsRUArmorEntry(ACR_RequisitionEntry entry) { return ACR_AIRequisition.IsRUArmorEntry(this, entry); }

 static bool IsSapperEntry(ACR_RequisitionEntry entry) { return ACR_AIRequisition.IsSapperEntry(entry); }

 bool HasEnemyFob() { return ACR_AIRequisition.HasEnemyFob(this); }

 static bool IsSpecialistSquad(ACR_Squad squad) { return ACR_AIRequisition.IsSpecialistSquad(squad); }

 static bool IsSpecialistTeam(ACR_RequisitionEntry entry) { return ACR_AIRequisition.IsSpecialistTeam(entry); }

 protected bool HasSpecialForces() { return ACR_AIRequisition.HasSpecialForces(this); }

 protected bool OwnsLike(ACR_RequisitionEntry entry) { return ACR_AIRequisition.OwnsLike(this, entry); }

 // ---------------------------------------------------------------- 6. logistics
 // Squads low on ammo, hurt, or with a damaged / thirsty vehicle get the matching
 // service vehicle: it drives out to squads holding a position, while free
 // squads go back to it. Idle service vehicles wait at the safest base nearest
 // the front, and go home to restock when they run low. Lose the truck and the
 // front runs dry.
 static const int LOW_AMMO = 45;
 static const int OUT_OF_AMMO = 15;           // below this a squad in a fight pulls back
 static const int LOW_HEALTH = 60;
 static const int LOW_FUEL = 25;
 static const float SERVICE_REACH = 2000;     // farthest a service vehicle is sent or sought
 int m_iNeeds;                      // services wanted this tick (for requisition)
 protected ref map<ACR_Squad, ACR_Squad> m_ServiceJobs = new map<ACR_Squad, ACR_Squad>(); // service crew -> squad it is serving
 // A squad sent back to a service truck, and when it gives up: a truck destroyed, emptied or
 // gone home left a squad that still needed the service tasked RESUPPLY (out of every pool) for good.
 protected ref map<ACR_Squad, float> m_ResupplyGiveUp = new map<ACR_Squad, float>();
 // #242 audit (10 Nov): a drive-out job ended only when the squad's need cleared, so a truck that
 // ran dry, or stopped at the road point up to 250 m from a squad holding its positions (served
 // only within 25 m), kept the job for the rest of the match: never parked, never restocked, and
 // requisition never replaced it (the side still had one). A job now also ends when the truck is
 // gone or empty, has stood still SERVICE_JOB_STILL_MS without the need clearing, or has run
 // SERVICE_JOB_MAX_MS; that squad then gets no drive-out for SERVICE_SNUB_MS (it may still walk).
 static const float SERVICE_JOB_STILL_MS = 180000;
 static const float SERVICE_JOB_MAX_MS = 900000;
 static const float SERVICE_SNUB_MS = 600000;
 static const int JOB_ON = 0;
 static const int JOB_NO_TRUCK = 1;
 static const int JOB_EMPTY = 2;
 static const int JOB_STILL = 3;
 static const int JOB_LONG = 4;
 protected ref map<ACR_Squad, float> m_ServiceJobSince = new map<ACR_Squad, float>();  // crew -> sent at
 protected ref map<ACR_Squad, float> m_ServiceJobMoved = new map<ACR_Squad, float>();  // crew -> last seen driving
 protected ref map<ACR_Squad, float> m_ServiceSnub = new map<ACR_Squad, float>();      // client -> no drive-out until

 bool InAssault(ACR_Squad squad)
 {
  foreach (ACR_AIAttack attack : m_Attacks)
   if (attack.m_bAssaulting && !attack.m_bGrab && attack.m_Squads.Contains(squad)) return true;
  return false;
 }

 // ---------------------------------------------------------------- ammo runs
 // (ACR_AmmoRuns.) A squad low on ammo has the ammo truck meet it where that is safe, walks back to a
 // quiet base of ours that can restock it, or carries on and looks again later: each costed, picked
 // with ACR_Variety. One truck serves one squad at a time (m_ServiceJobs); at most a share of the
 // combat squads are away at once, except the shot dry. A holder's post waits for it (NoteLeftPost,
 // ReturnToPosts); others are free for their next job when the run is over.
 // Safe for a truck, by the side's own picture: the squad knows of no enemy within AMMO_CONTACT_M,
 // the side has seen none within AMMO_SEEN_M of the meeting point, no base of ours there is contested
 // or under attack, no enemy base within AMMO_FRONT_M, and no FIA ambush on the way (DangerOnTheWay).
 // Checked again every tick while the truck drives: a run that turns unsafe lets the truck go.
 static const float AMMO_CONTACT_M = 400;
 static const float AMMO_SEEN_M = 500;
 static const float AMMO_FRONT_M = 400;
 static const float AMMO_MEET_ROAD_M = 150;   // the truck meets it on a road this close, else where it stands
 static const float AMMO_MEET_M = 45;         // the truck counts as there within this of the meeting point
 static const float AMMO_SERVE_M = 40;        // and hands out to every man of the squad within this of it
 static const float AMMO_BASE_REACH = 5000;
 static const float AMMO_TRUCK_MS = 420000;   // a truck that hasn't got there by then is let go
 static const float AMMO_SERVE_MS = 60000;    // at the truck this long: what could be handed out has been
 static const float AMMO_GATHER_MS = 30000;   // inside the base: the stragglers get this long to come in
 static const float AMMO_SETTLE_MS = 10000;   // the magazines are handed out over a few seconds
 static const float AMMO_CONTACT_MS = 90000;  // inside the base with enemies near: it waits this long, then fights
 static const float AMMO_LOOK_MS = 120000;    // carrying on, or a truck that failed it: it looks again after this
 static const float AMMO_SKIP_MS = 900000;    // a base it couldn't get to is left out this long
 ref map<ACR_Squad, ref ACR_AmmoRun> m_AmmoRuns = new map<ACR_Squad, ref ACR_AmmoRun>();
 protected ref map<ACR_Squad, float> m_AmmoLookAt = new map<ACR_Squad, float>();
 protected ref map<ACR_Squad, float> m_AmmoNoTruckUntil = new map<ACR_Squad, float>();
 protected ref map<ACR_Squad, float> m_AmmoToppedAt = new map<ACR_Squad, float>();
 protected ref map<ACR_Squad, int> m_AmmoToppedPct = new map<ACR_Squad, int>();
 // Every base a squad couldn't get to, and when (one remembered base let a run ping-pong between
 // two unreachable bases all match: review 0.5.13 #13).
 protected ref map<ACR_Squad, ref map<ACR_Base, float>> m_AmmoSkips = new map<ACR_Squad, ref map<ACR_Base, float>>();

 // Shot nearly dry: not sent on a new attack (ammo runs on), as ACR_Qrf leaves it off a QRF.
 protected bool AmmoShortForAttack(ACR_Squad squad)
 {
  return squad && squad.m_iAmmoPct >= 0 && squad.m_iAmmoPct < ACR_AmmoRuns.ATTACK_MIN && ACR_AmmoRuns.On();
 }

 void DropAmmoShort(array<ACR_Squad> squads)
 {
  for (int i = squads.Count() - 1; i >= 0; i--)
   if (AmmoShortForAttack(squads[i])) squads.RemoveOrdered(i);
 }

 // A base of ours that can restock a squad now: replacements allowed there (its relay standing),
 // not contested, nobody attacking it.
 protected bool RearmBase(ACR_Base base)
 {
  return base && !base.m_bRetired && base.m_sOwner == m_sFaction && ACR_LogisticsRules.CanReinforce(base) && !base.m_bContested && ThreatTo(base) == 0;
 }

 // The nearest one (FallBack "out of ammo": no weighing under fire).
 protected ACR_Base NearestRearmBase(vector point, ACR_Base except = null)
 {
  ACR_Base best;
  float bestDistance = AMMO_BASE_REACH;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base == except || !RearmBase(base)) continue;
   float d = vector.DistanceXZ(point, base.GetOrigin());
   if (d < bestDistance) { bestDistance = d; best = base; }
  }
  return best;
 }

 protected void ThinkAmmoRuns(notnull array<string> lines)
 {
  if (m_AmmoRuns.IsEmpty()) return;
  string text;
  foreach (ACR_Squad squad, ACR_AmmoRun run : m_AmmoRuns)
  {
   if (!squad || !run) continue;
   if (run.m_Crew) text += string.Format(" %1 (truck %2)", squad.GetName(), run.m_Crew.GetName());
   else if (run.m_Base) text += string.Format(" %1 (to %2)", squad.GetName(), run.m_Base.m_sName);
  }
  lines.Insert("ammo runs:" + text);
 }

 bool ServiceJobForTest(ACR_Squad crew, ACR_Squad client) { ACR_Squad serving; return m_ServiceJobs.Find(crew, serving) && serving == client; }
 // The supply point's truck park (ACR_Logistics.ParkSpot), worked out once per supply point: a
 // road point trucks can reach, on the side away from the nearest enemy base we know of. The
 // base centre when no road is near.
 static const float PARK_ARRIVE = 40;          // m from the park that counts as parked
 static const float PARK_RETRY_MS = 300000;    // a truck that stopped short is sent again after this
 protected ACR_Base m_ParkBase;
 protected vector m_vPark;
 protected ref map<ACR_Squad, float> m_ParkSentAt = new map<ACR_Squad, float>();
 protected ref map<ACR_Squad, vector> m_ParkSentTo = new map<ACR_Squad, vector>();

 // ---------------------------------------------------------------- dangerous roads (#89)
 // Where FIA cost this side men on a road (ACR_Insurgency): a free combat squad goes to clear it
 // (escorting the road), and supply runs whose way passes it wait until it is cleared or the mark
 // runs out. A road gets two sweepers at most (one that can't get there is replaced once).
 protected float m_fDangerSaidAt = -1000000;
 static const float SWEEP_REACH = 3000;
 // A sweep that never gets there, or stays in contact by a hideout it can't take, ends: the squad
 // was kept on SWEEP for the rest of the match (audit 31 Oct). Counts as one of the road's sweeps.
 static const float SWEEP_MAX_MS = 600000;
 // A found hideout holding our men (ACR_FIAPrisoners, insurgency part 4) comes first, gets a sweep
 // more for each of them held (RESCUE_SWEEPS at most) and a squad from further off.
 static const int RESCUE_SWEEPS = 3;
 static const float RESCUE_REACH = 1.5;
 // Containment evac (ACR_Evac): one evac at a time. A free transport (ACR_Rides.IsFreeTransport)
 // near the town with the most people holding out sets an XP by the town and drives there (an XP
 // of ours with people still on foot comes first). Loaded (no seat left, nobody left on foot, or
 // EVAC_LOAD_MS with someone aboard), it drives to our nearest base with no zombie in it; they are
 // delivered there (ACR_Evac pays on delivery) and it is free again. Under "I'm the 2IC" the
 // commander can keep evacs to themselves (ACR_Evac.TwoICExtracts). In TvTvZ both AIs race for them.
 static const float EVAC_REACH = 3000;     // m: a transport this close to the town goes
 static const float EVAC_AT_XP = 40;       // m from the XP: arrived, loading
 static const float EVAC_LOAD_MS = 240000; // leaves with whoever is aboard after this long
 static const float EVAC_MAX_MS = 1500000; // the whole job, then it is given up
 protected ACR_EvacPoint m_EvacPoint;
 protected ACR_Squad m_EvacTaxi;
 protected int m_iEvacPhase;               // 0 to the XP, 1 loading, 2 to a base
 protected float m_fEvacSince;
 protected float m_fEvacPhaseAt;

 // Containment airdrops (ACR_Airdrop): one runner at a time goes for the nearest pallet it may take
 // (ours, or the enemy's while no truce holds: ACR_Airdrop.MayTake); freed once the pallet is gone, or
 // as a sweeper would be. Under "I'm the 2IC" the commander can keep pallets with evacs
 // (ACR_Evac.TwoICExtracts). The runner (PalletRunner) is a free combat squad within SWEEP_REACH; else
 // one on a grab of an empty base within PALLET_DIVERT and not yet near it (a grab is the cheapest job:
 // defenders and real attacks are never taken); else, with no zombie near the pallet, a free truck
 // within EVAC_REACH that drives up to it. Jacob, 12 Nov: the AI never went for a pallet, as every
 // combat squad was out grabbing bases all match and only a free one was looked for.
 static const float PALLET_DIVERT = 1500;      // m: a grab squad this close to the pallet may be diverted
 static const float PALLET_GRAB_NEAR = 300;    // m: one this close to its grab finishes it first
 static const float PALLET_TRUCK_ZOMBIES = 150; // m: no truck goes while the dead are this close to it
 protected ACR_Pallet m_PalletTarget;
 protected ACR_Squad m_PalletSquad;
 protected float m_fPalletSince;
 void StartGrabForTest(ACR_Squad runner, ACR_Base empty) { StartGrab(runner, empty); }

 ACR_Squad PalletSquadForTest() { return m_PalletSquad; }
 ACR_Squad EvacTaxiForTest() { return m_EvacTaxi; }

 // A service vehicle that gave up stuck (ACR_Squad.m_fStuckAt: holding where it is) waits before
 // it is sent anywhere again, as Recrew does. Jacob's 26 Oct game: an arsenal truck stuck by its
 // spawn at Saint-Philippe Factory was sent to Tyrone 59 times in 2.5 h, 353 stuck lines.
 // Giving up a move as unreachable counts too (RHS soak 28 Oct: a fuel truck that couldn't
 // path off its spot was sent again every 45 s for 22 min, 31 give-ups).
 static const float STUCK_SERVICE_WAIT_MS = 600000;
 protected ACR_Base m_SupplyPoint;

 // ---------------------------------------------------------------- FOB establishment (Stage 3)
 // (Front gap before a FOB: the doctrine's m_fFobFront, 1.5 km standard.)
 static const float FOB_EXISTING_PROXIMITY = 1000;   // don't build within 1 km of existing friendly FOB
 static const int FOB_RP_COMBAT_RESERVE = 30;        // RP reserve required when buying construction truck
 static const float FOB_EVAL_INTERVAL_MS = 30000;    // evaluate every 30 seconds

 static const float FOB_TRUCK_RETRY_MS = 300000;    // at most one construction truck purchase per 5 min
 static const float FOB_SITE_AVOID_MS = 600000;     // an abandoned site is skipped for 10 min
 static const float FOB_SITE_AVOID_RADIUS = 150;
 static const float FOB_SITE_RETRY_MS = 300000;     // after a failed site search
 static const float FOB_SITE_STEP_MS = 5000;        // between the steps of a site search (ACR_FobSiting)

 protected ACR_Squad m_FobBuilder;
 protected bool m_bFobProject; // m_FobBuilder was set: a wiped builder (weak ref gone null) still ends the project
 ACR_Squad m_FobEscort;
 protected vector m_vFobTargetSite;
 protected float m_fNextFobEval;
 protected float m_fNextRoadblockEval;
 protected float m_fNextMinefieldEval;
 protected float m_fNextFortifyEval;
 protected float m_fFobTruckBoughtAt = -FOB_TRUCK_RETRY_MS;
 protected ref array<vector> m_aAvoidSites = {};
 protected ref array<float> m_aAvoidUntil = {};

 bool HasActiveFobProject()
 {
  // A wiped-out builder squad is disposed and its weak reference goes null: the project still ends
  // here (site avoided, escort freed), or the AI bought truck after truck for the same deadly site.
  if (m_FobBuilder || m_bFobProject)
  {
   if (m_FobBuilder && m_FobBuilder.CountAlive() > 0 && m_FobBuilder.OrderState() == ACR_EOrderState.BUILD_FOB) return true;
   // A regroup on the way (up to a minute) goes back to the build: not given up.
   if (m_FobBuilder && m_FobBuilder.CountAlive() > 0 && m_FobBuilder.OrderState() == ACR_EOrderState.REGROUP && m_FobBuilder.m_iAITask == ACR_EAITask.BUILD_FOB) return true;
   // Given up (3 min without progress) or wiped out short of completion: avoid that site for a while.
   // An HQ rebuild targets the HQ-lost FOB's own origin: that FOB stands there still built, so it
   // counts as finished only once its HQ is back (or the AI re-sent and rebought trucks for it forever).
   bool finished = false;
   foreach (ACR_Base built : ACR_Base.s_All)
    if (built && built.m_bBuilt && !built.m_bRetired && !built.m_bHqLost && vector.DistanceXZ(built.GetOrigin(), m_vFobTargetSite) < 20) finished = true;
   // Finished just now: the FOB appears on the next structures tick, after its HQ (a check in
   // that gap used to avoid the new FOB's own site).
   if (!finished && m_FobBuilder && m_FobBuilder.BuiltFobAt(m_vFobTargetSite)) finished = true;
   if (!finished) AvoidSite(m_vFobTargetSite);
   m_FobBuilder = null;
   m_bFobProject = false;
   if (m_FobEscort && m_FobEscort.m_iAITask == ACR_EAITask.DEFEND && !m_FobEscort.m_AITarget)
   {
    m_FobEscort.SetAITask(ACR_EAITask.FREE);
   }
   m_FobEscort = null;
  }
  foreach (ACR_Squad squad : m_Manager.m_Squads)
  {
   if (!squad || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != m_sFaction) continue;
   if (squad.OrderState() == ACR_EOrderState.BUILD_FOB || squad.m_iAITask == ACR_EAITask.BUILD_FOB) return true;
  }
  return false;
 }

 // The scored search, 'most' more offsets validated (-1: all that are left); 'pending' while some
 // are still to be validated (ManageFobBuild spreads a search over a few ticks).
 protected string m_sFobSiteNone;
 void AvoidSite(vector site)
 {
  m_aAvoidSites.Insert(site);
  m_aAvoidUntil.Insert(GetGame().GetWorld().GetWorldTime() + FOB_SITE_AVOID_MS);
  Say(string.Format("[ACR] AI %1: FOB site %2 abandoned, avoiding it for 10 minutes", m_sFaction, site));
 }

 static const float FOB_UPGRADE_EVAL_INTERVAL_MS = 15000;
 protected float m_fNextFobUpgradeEval;

 void SetFobUpgradeEvalTimeForTest(float time) { m_fNextFobUpgradeEval = time; }

 void SetFobEvalTimeForTest(float time) { m_fNextFobEval = time; }
 void StartTickForTest() { m_iTickStarted = System.GetTickCount(); }
 void AssignAttacksForTest() { m_iTickStarted = System.GetTickCount(); AssignAttacks(); }
 void UpdateAttacksForTest() { UpdateAttacks(); }
 void RunAllocationForTest(bool control) { m_iTickStarted = System.GetTickCount(); RunAllocation(control); }
 // Holding a roadblock or a position it built (not building one).
 static bool IsHolder(ACR_Squad squad)
 {
  if (!squad) return false;
  if (squad.m_iAITask == ACR_EAITask.ROADBLOCK) return squad.m_ActiveRoadblock && squad.OrderState() != ACR_EOrderState.BUILD_ROADBLOCK;
  if (squad.m_iAITask == ACR_EAITask.FORTIFY) return squad.m_Fortification && !squad.m_FortifyPlan;
  return false;
 }

 // Our uncontested bases nearest to an enemy base, nearest first (at most 'most'), each with
 // that enemy base's position: where roadblocks, minefields and fortified positions go. (The one
 // base nearest the enemy only, before: a side held one roadblock all game, playtest 25 Oct.)
 static const int FRONT_BASES = 3;
 // What held a roadblock, minefield or position up, at most once per NOTE_S per kind (a playtest
 // log shows whether siting ever stopped one, or the RP, the planning budget or a free squad did;
 // 25 Oct, pn5: no AI minefield or USSR position all game, and nothing in the log said why).
 static const float NOTE_S = 300;
 protected ref map<string, float> m_mBuildNotes = new map<string, float>();
 // With no free squad near a front base, a roadblock, minefield or position takes the nearest
 // within FAR_BUILDER, on foot (the job's walk deadline grows with the distance). pn5, 25 Oct:
 // 11 notes of a roadblock site waiting for a free squad within 450 m, and no AI field or USSR
 // position all game. The reserve and the squads attacking or defending stay out of it as before.
 static const float FAR_BUILDER = 1500;
 // A far builder (more than the doctrine's m_fRideDistance from 'site') rides out in a truck when
 // one is free: the job is given, and paid, once the squad is out at the drop (ManagePendingBuilds;
 // a job given aboard would end the ride at once). False: no ride needed or none to be had, the
 // job is given now and the squad walks.
 protected ref array<ref ACR_PendingBuild> m_aPendingBuilds = {};
 // Our defences keep apart (playtest 25 Oct: twice an AI minefield 10 m from one of its own
 // positions, laid or dug in either order; the AI is spared its own mines, its players are not):
 // a field's centre DEFENCE_STANDOFF m from our positions and roadblocks (also those on their way)
 // and clear of our bases' edges by 40 m, a position as far from our fields.
 static const float DEFENCE_STANDOFF = 60;

 void ResetBuildTimersForTest()
 {
  m_fNextRoadblockEval = 0;
  m_fNextMinefieldEval = 0;
  m_fNextFortifyEval = 0;
 }

 // A front base of ours gets its next tier of works (ACR_BaseWorks) once the side has the RP banked
 // for it: more the higher the tier, as a rich side digs in (#28). The most threatened ones are
 // likeliest, picked among the near-best; a doctrine that builds no positions builds no works.
 static const int WORKS_BANK_STEP = 150;
 // With no free squad near, works still get a squad (pn23 playtest: one town started in 36 minutes, the
 // log saying 'wait for a free squad' 14 times, with free squads 5 km off and 8 attacking): the nearest
 // free one wherever it is, as it walks there; else one of an attack with squads to spare, on its way
 // and out of contact, within FAR_BUILDER.
 static const float WORKS_FAR = 7000;
 static const float WORKS_WALK = 2500;
 static const int WORKS_ATTACK_SPARE = 4;
 // A position of ours at a front base that stands empty (its holder killed, gone to defend or
 // posted) gets the nearest free squad within FAR_BUILDER sent back to hold it, while fewer than
 // the doctrine's m_iHolders squads hold what we built (roadblocks included; more would be posted
 // again at once). Before, nothing sent anyone back (Jacob, #25). True if a squad was sent.
 static const float FRONT_POSITION_REACH = 600; // m from a front base: a position there is part of its defence
 // The nearest free infantry squad within FAR_BUILDER of 'pos' that can dig in (not off a
 // roadblock or position it holds); within FORTIFY_NEAR only, before.
 static const float FORTIFY_NEAR = 500;
 // ---------------------------------------------------------------- holders back to their posts
 // A holder that leaves its position to defend a base (outnumbered there, or as a last resort)
 // goes back to it once that defence is over, before new attacks take it: the pn9 playtest
 // (26 Oct) had two Military Hill holders go to defend it, then onto an attack, and the positions
 // stood empty for 15 minutes. Only while the position is still ours, standing and unmanned, the
 // doctrine's holders allow it, and within RETURN_MS of leaving. A roadblock's holder the same
 // (#64 review): it has m_ActiveRoadblock, not m_Fortification.
 static const float RETURN_MS = 1200000;
 protected ref map<ACR_Squad, ACR_Fortification> m_LeftPost = new map<ACR_Squad, ACR_Fortification>();
 protected ref map<ACR_Squad, ACR_Roadblock> m_LeftRoadblock = new map<ACR_Squad, ACR_Roadblock>();
 protected ref map<ACR_Squad, float> m_LeftPostAt = new map<ACR_Squad, float>();

 void NoteLeftPost(ACR_Squad squad)
 {
  if (!squad) return;
  if (squad.m_Fortification) m_LeftPost.Set(squad, squad.m_Fortification);
  else if (squad.m_ActiveRoadblock) m_LeftRoadblock.Set(squad, squad.m_ActiveRoadblock);
  else return;
  m_LeftPostAt.Set(squad, GetGame().GetWorld().GetWorldTime());
 }

 int LeftPostsForTest() { return m_LeftPost.Count() + m_LeftRoadblock.Count(); }
 // ---------------------------------------------------------------- why nobody builds
 static const int BUILDER_FORTIFY = 0;
 static const int BUILDER_ROADBLOCK = 1;
 static const int BUILDER_MINES = 2;

 // A squad walking back to refill, or weak enough that it soon will (ManageReinforcements), isn't
 // sent to build: the refit would take it off the job. Soak, 26 Oct: a 3-man squad, still on its
 // way to refill, was posted, rode out to dig in and was called back a minute later.
 bool DueForRefit(ACR_Squad squad)
 {
  return m_Refitting.Contains(squad) || squad.StrengthRatio() < m_Doctrine.m_fRefitRatio;
 }

 void SetRefittingForTest(ACR_Squad squad, bool on)
 {
  if (on) m_Refitting.Set(squad, GetGame().GetWorld().GetWorldTime() + REFIT_GIVE_UP_MS);
  else m_Refitting.Remove(squad);
 }

 // ---------------------------------------------------------------- hand-over

 // A player took command: leave every squad as it is, but forget AI tasks.
 // ---------------------------------------------------------------- squads handed to the AI
 // A player commands the side (or it has no AI commander): the squad logic runs for the squads
 // handed to the AI (squad panel, ACR_SquadAI) and only them: MySquads and DefendInDepth see
 // no other squad. No purchases, logistics, FOB building, mortars or crews for the side.
 // all (the 2IC while a player commands: ACR_SquadAI.RunsAll): the whole of Tick's turn
 // as well (purchases, logistics, FOBs, mortars, crews, supports, fortifying), still only with
 // the squads handed to it, and spending the side's RP down to the player's keep (Purse).
 bool m_bHandedOnly;
 bool m_bTwoICAll;
 protected bool m_bTwoICAllShown;
 protected int m_iHandedSeconds;
 // A squad this commander may order: any of its side's, but as the 2IC only those handed to it.
 bool Ours(ACR_Squad squad) { return squad && (!m_bHandedOnly || squad.m_bAICommanded); }

 int Purse(ACR_FactionData data = null)
 {
  if (!data && m_Manager && m_Manager.m_Bases) data = m_Manager.m_Bases.GetFaction(m_sFaction);
  if (!data) return 0;
  if (!m_bTwoICAll) return data.m_iRP;
  return Math.Max(0, data.m_iRP - ACR_SquadAI.TwoICKeep());
 }

 // ---------------------------------------------------------------- the 2IC's map UI (#14 v1)
 // A line in the side's 2IC channel (ACR_TwoIC), while this runs the 2IC's squads.
 void TwoICReport(string text)
 {
  if (m_bHandedOnly) ACR_TwoIC.Report(m_sFaction, text);
 }

 // "Alpha 1", "Alpha 1 and Bravo 1", "3 squads".
 static string SquadList(array<ACR_Squad> squads)
 {
  array<string> names = {};
  foreach (ACR_Squad squad : squads)
   if (squad) names.Insert(squad.GetName());
  if (names.Count() == 1) return names[0];
  if (names.Count() == 2) return names[0] + " and " + names[1];
  return string.Format("%1 squads", names.Count());
 }

 // A 2IC with fewer than ACR_TwoIC.SMALL_FORCE squads: it only grabs empty bases, and holds with at
 // most half its force.
 float m_fNextSmallLog;
 bool SmallForce()
 {
  return m_bHandedOnly && !m_bTwoICAll && ACR_TwoIC.On() && ACR_TwoIC.SmallForce(m_sFaction); // commanding the side, it is no small force
 }

 // The most squads that stay on what they built: the doctrine's m_iHolders, a small force's half.
 protected int HoldersCap()
 {
  if (!SmallForce()) return m_Doctrine.m_iHolders;
  return Math.Min(m_Doctrine.m_iHolders, ACR_SquadAI.CountFor(m_sFaction) / 2);
 }

 // ---------------------------------------------------------------- the 2IC's requests (#14 v1)
 // A player's "Ask the 2IC" (ACR_TwoICRequests), answered each second ahead of the 2IC's own
 // plans: Support me here and Hold this send the nearest free squad (a small force takes one off a
 // grab: requests first), which stays on it (AssignDefenders leaves it) until the support runs out
 // or the hold is released; Take this base grabs an empty base at once and makes an enemy one the
 // next attack's target.
 static const float REQUEST_REACH = 2500;   // m: the farthest a squad is sent to support or hold
 static const float SUPPORT_MS = 600000;    // a support stays 10 min once there
 static const float ARRIVED = 60;           // m from the spot: there
 protected ref map<ACR_Squad, int> m_OnRequest = new map<ACR_Squad, int>(); // squad -> request id
 ACR_Base m_RequestedTarget;
 int m_iRequestedBy;

 bool OnRequest(ACR_Squad squad)
 {
  return squad && m_OnRequest.Contains(squad);
 }

 // "Take this base" for an enemy base, while it stands.
 ACR_Base RequestedTarget()
 {
  if (m_RequestedTarget && (m_RequestedTarget.m_bRetired || m_RequestedTarget.m_sOwner == m_sFaction)) m_RequestedTarget = null;
  return m_RequestedTarget;
 }

 // ---------------------------------------------------------------- Build a FOB here
 static const float FOB_ASK_NEAR = 150; // m: a FOB of ours this close to the asked spot answers it

 // Tests: as if no construction truck had been bought lately.
 void ResetFobTruckBoughtForTest()
 {
  m_fFobTruckBoughtAt = -FOB_TRUCK_RETRY_MS;
 }

 // The nearest squad of ours free to answer: a fighting squad that isn't holding what it built,
 // on another request or falling back; a small force takes one off a grab too (requests first).
 ACR_Squad RequestSquad(array<ACR_Squad> mine, vector pos)
 {
  bool small = SmallForce();
  ACR_Squad best;
  float bestDistance = float.MAX;
  foreach (ACR_Squad squad : mine)
  {
   if (!squad.IsCombatUnit() || IsSupportTeam(squad) || OnRequest(squad) || IsHolder(squad) || IsRetreating(squad)) continue;
   bool free = squad.m_iAITask == ACR_EAITask.FREE || squad.m_iAITask == ACR_EAITask.POSTED;
   if (!free && !(small && squad.m_iAITask == ACR_EAITask.ATTACK && OnGrab(squad))) continue;
   float d = vector.DistanceXZ(PositionOf(squad), pos);
   if (d >= bestDistance) continue;
   bestDistance = d;
   best = squad;
  }
  return best;
 }

 protected bool OnGrab(ACR_Squad squad)
 {
  foreach (ACR_AIAttack attack : m_Attacks)
  {
   if (attack.m_bGrab && attack.m_Squads.Contains(squad)) return true;
  }
  return false;
 }

 // A grab of an empty base by this squad (AssignAttacks, a 2IC request, the allocator).
 void StartGrab(ACR_Squad runner, ACR_Base empty)
 {
  ACR_AIAttack grab = new ACR_AIAttack();
  grab.m_Target = empty;
  grab.m_bGrab = true;
  grab.m_bAssaulting = true;
  grab.m_fAssaultStart = GetGame().GetWorld().GetWorldTime();
  grab.Add(runner, empty.GetOrigin(), -1);
  grab.m_iStartStrength = runner.FieldStrength(); // a roadblock's gun stays behind (FieldStrength)
  // Everon soak 5 Oct: most grabs of bases 2-3 km away timed out on the way at 480 s.
  // Allow the walk (or drive) there at about 1.2 m/s, plus 3 minutes. Foot grabs are already
  // limited to GRAB_WALK; farther ones go by truck and keep the walking allowance (the truck may
  // be late or fall through, CatchRides then finds another on the way); only squads in their own
  // vehicle, which can be far but fast, cap at 10 min.
  grab.m_fGrabSeconds = vector.DistanceXZ(PositionOf(runner), empty.GetOrigin()) / 1.2 + 180;
  if (runner.InMobileVehicle()) grab.m_fGrabSeconds = Math.Min(600, grab.m_fGrabSeconds);
  runner.SetAITask(ACR_EAITask.ATTACK);
  runner.m_AITarget = empty;
  runner.SetMode(ACR_EMoveMode.FAST);
  Travel(runner, empty.GetOrigin(), false, 100);
  m_Attacks.Insert(grab);
 }

 bool IsAttackingForTest(ACR_Base base)
 {
  return IsAttacking(base);
 }
 void LeaveAttacksForTest(ACR_Squad squad) { LeaveAttacks(squad); }
 bool CoolingDownForTest(ACR_Base base) { return CoolingDown(base); }

}
