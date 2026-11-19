// -acrAllocatorTest 1 (server-capable, ~15 s): the AI job allocator (ACR_AIAllocator) on made-up
// fronts, no squads in the world: an attack takes its nearest squads and leaves the far one out,
// an attack that can't reach its bar takes no one, a far member is swapped for a near free squad,
// the job a squad has is kept unless another is clearly better, locked squads stay, an attack
// needs a squad that can take the base, a running attack is fully ready at the strength it set
// out with, worth nothing once broken, keeps its slot under the cap, its own squads first and
// them unless elsewhere is clearly better, and the value curves; trucks are counted (one squad a
// truck), and where a squad at a quiet rear base moves up to (ACR_AICommander.MoveUpPick).
// Then live on the range with -acrAIAllocator 2: free Alpha 200 m from an empty base is sent to
// grab it by the allocator, and not when it is a medic team.
class ACR_TestAllocator
{
 static const int CHECKS = 31;

 static ACR_AIJobSquad Squad(string name, float strength, int current, array<float> etas, array<float> fits = null)
 {
  ACR_AIJobSquad squad = new ACR_AIJobSquad();
  squad.m_sName = name;
  squad.m_fStrength = strength;
  squad.m_iCurrent = current;
  foreach (int i, float eta : etas)
  {
   squad.m_aEta.Insert(eta);
   float fit = 1;
   if (fits && i < fits.Count()) fit = fits[i];
   squad.m_aFit.Insert(fit);
  }
  return squad;
 }

 static array<float> E(float a, float b)
 {
  array<float> pair = {};
  pair.Insert(a);
  pair.Insert(b);
  return pair;
 }

 static ACR_AIJob Attack(string name, float value, float need, float bar = 1)
 {
  ACR_AIJob job = new ACR_AIJob();
  job.m_sName = name;
  job.m_iKind = ACR_EAITask.ATTACK;
  job.m_fValue = value;
  job.m_fNeed = need;
  job.m_bTeam = true;
  job.m_bNeedsTaker = true;
  job.m_fBar = bar;
  job.m_iMaxSquads = 6;
  return job;
 }

 static ACR_AIJob Hold(string name, float value, int most = 4)
 {
  ACR_AIJob job = new ACR_AIJob();
  job.m_sName = name;
  job.m_iKind = ACR_EAITask.POSTED;
  job.m_fValue = value;
  job.m_iMaxSquads = most;
  job.m_fPatience = 1200;
  return job;
 }

 // Its way to the first job (an attack) is a truck ride, to the second not.
 static ACR_AIJobSquad Ride(ACR_AIJobSquad squad)
 {
  squad.m_aByTruck.Insert(true);
  squad.m_aByTruck.Insert(false);
  return squad;
 }

 // Its way to the first job (an attack) is the air lift, to the second not.
 static ACR_AIJobSquad Fly(ACR_AIJobSquad squad)
 {
  squad.m_aByAir.Insert(true);
  squad.m_aByAir.Insert(false);
  return squad;
 }

 static string Picks(array<ref ACR_AIJobSquad> squads, array<ref ACR_AIJob> jobs)
 {
  string text;
  foreach (ACR_AIJobSquad squad : squads)
  {
   string where = "-";
   if (squad.m_iPicked >= 0) where = jobs[squad.m_iPicked].m_sName;
   text += string.Format(" %1->%2", squad.m_sName, where);
  }
  return text;
 }

 static void Run(ACR_TestHarness h, int tick)
 {
  if (tick == 8) Live(h);
  if (tick != 3) return;

  // Value curves.
  bool curves = Math.AbsFloat(ACR_AIAllocator.TimeFactor(600, 600) - 0.5) < 0.001 && ACR_AIAllocator.TimeFactor(0, 600) == 1;
  ACR_TestRegress.Check("allocator: value halves at the job's patience", curves, string.Format("%1", ACR_AIAllocator.TimeFactor(600, 600)));
  bool ready = ACR_AIAllocator.Readiness(9, 10, 1) == 0 && ACR_AIAllocator.Readiness(12.5, 10, 1) == 1 && ACR_AIAllocator.Readiness(10, 10, 1) > 0.7;
  ACR_TestRegress.Check("allocator: a team below its bar is worth nothing, past 1.25x need no more", ready, string.Format("%1 %2 %3", ACR_AIAllocator.Readiness(9, 10, 1), ACR_AIAllocator.Readiness(10, 10, 1), ACR_AIAllocator.Readiness(12.5, 10, 1)));

  // 1. Nearest first, the far squad left out (it would hold the whole assault back).
  array<ref ACR_AIJob> jobs = {};
  jobs.Insert(Attack("Ajvan", 40, 10));
  jobs.Insert(Hold("Szilis", 2));
  array<ref ACR_AIJobSquad> squads = {};
  squads.Insert(Squad("A", 6, -1, E(60, 300)));
  squads.Insert(Squad("B", 6, -1, E(90, 300)));
  squads.Insert(Squad("Far", 6, -1, E(3000, 100)));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool near = squads[0].m_iPicked == 0 && squads[1].m_iPicked == 0 && squads[2].m_iPicked == 1;
  ACR_TestRegress.Check("allocator: an attack takes its nearest squads and leaves the far one to hold", near, Picks(squads, jobs));

  // 2. Not enough for the bar: the attack takes no one.
  jobs[0].m_fNeed = 20;
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool unfunded = squads[0].m_iPicked == 1 && squads[1].m_iPicked == 1 && jobs[0].m_aMembers.IsEmpty();
  ACR_TestRegress.Check("allocator: an attack short of its bar takes no one", unfunded, Picks(squads, jobs));

  // 3. The 3 Nov Ajvan case with a near free squad: the far member goes, the near one joins.
  jobs.Clear();
  ACR_AIJob running = Attack("Ajvan", 40, 7.4, 0.87);
  running.m_bRunning = true;
  jobs.Insert(running);
  jobs.Insert(Hold("Szilis", 2));
  squads.Clear();
  squads.Insert(Squad("Naval", 2, 0, E(0, 2000)));
  squads.Insert(Squad("Squad2", 6, 0, E(2860, 1500)));
  squads.Insert(Squad("Near", 6, -1, E(120, 900)));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool swap = squads[0].m_iPicked == 0 && squads[2].m_iPicked == 0 && squads[1].m_iPicked == 1;
  ACR_TestRegress.Check("allocator: a member 48 min out is swapped for a free squad 2 min out", swap, Picks(squads, jobs));
  // Without the near squad the attack keeps both: nothing better to do with them.
  squads.Remove(2);
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool keeps = squads[0].m_iPicked == 0 && squads[1].m_iPicked == 0;
  ACR_TestRegress.Check("allocator: with no one nearer the slow team is kept rather than sent in at 2 men", keeps, Picks(squads, jobs) + string.Format(" value %1", jobs[0].m_fScore));

  // 4. Incumbency: a slightly better job doesn't move a squad, a clearly better one does.
  jobs.Clear();
  jobs.Insert(Hold("Here", 10));
  jobs.Insert(Hold("There", 11.5));
  squads.Clear();
  squads.Insert(Squad("S", 6, 0, E(0, 0)));
  ACR_AIAllocator.Allocate(jobs, squads, 0);
  bool stays = squads[0].m_iPicked == 0;
  jobs[1].m_fValue = 14;
  ACR_AIAllocator.Allocate(jobs, squads, 0);
  bool moves = squads[0].m_iPicked == 1;
  ACR_TestRegress.Check("allocator: a squad keeps its job for a 15% better one", stays, Picks(squads, jobs));
  ACR_TestRegress.Check("allocator: a squad moves for a 40% better one", moves, Picks(squads, jobs));

  // 5. A locked squad stays even for a much better job.
  jobs[1].m_fValue = 100;
  squads[0].m_bLocked = true;
  ACR_AIAllocator.Allocate(jobs, squads, 0);
  bool locked = squads[0].m_iPicked == 0 && jobs[0].m_aMembers.Count() == 1;
  ACR_TestRegress.Check("allocator: a locked squad stays on its job", locked, Picks(squads, jobs));

  // 5b. A locked squad counts towards its job: a defence it already covers gets no one else.
  jobs.Clear();
  ACR_AIJob defend = Hold("Szilis", 12);
  defend.m_iKind = ACR_EAITask.DEFEND;
  defend.m_fNeed = 6;
  jobs.Insert(defend);
  squads.Clear();
  ACR_AIJobSquad holder = Squad("Holder", 8, 0, E(0, 0));
  holder.m_bLocked = true;
  squads.Insert(holder);
  squads.Insert(Squad("Spare", 6, -1, E(60, 0)));
  ACR_AIAllocator.Allocate(jobs, squads, 0);
  bool covered = squads[0].m_iPicked == 0 && squads[1].m_iPicked == -1;
  ACR_TestRegress.Check("allocator: a defence its locked squad already covers gets no one else", covered, Picks(squads, jobs));

  // 6. An attack needs a squad that can take the base; a squad that can't do a job isn't given it.
  jobs.Clear();
  jobs.Insert(Attack("Ajvan", 40, 6));
  jobs.Insert(Hold("Szilis", 2));
  squads.Clear();
  ACR_AIJobSquad mortar = Squad("Medics", 8, -1, E(60, 60));
  mortar.m_bTaker = false;
  squads.Insert(mortar);
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool taker = squads[0].m_iPicked == 1;
  ACR_TestRegress.Check("allocator: an attack with no squad able to take the base isn't made", taker, Picks(squads, jobs));
  squads.Clear();
  squads.Insert(Squad("Rifles", 8, -1, E(60, 60), E(0, 1)));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool unfit = squads[0].m_iPicked == 1;
  ACR_TestRegress.Check("allocator: a squad with fit 0 for a job never gets it", unfit, Picks(squads, jobs));

  // 7. The team cap: two attacks worth doing, one allowed.
  jobs.Clear();
  jobs.Insert(Attack("North", 40, 6));
  jobs.Insert(Attack("South", 30, 6));
  jobs[0].m_iMaxSquads = 1;
  jobs[1].m_iMaxSquads = 1;
  squads.Clear();
  squads.Insert(Squad("A", 6, -1, E(60, 60)));
  squads.Insert(Squad("B", 6, -1, E(60, 60)));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool capped = !jobs[0].m_aMembers.IsEmpty() && jobs[1].m_aMembers.IsEmpty();
  ACR_TestRegress.Check("allocator: no more team jobs than allowed, the most valuable first", capped, Picks(squads, jobs));
  ACR_AIAllocator.Allocate(jobs, squads, 2);
  bool both = jobs[0].m_aMembers.Count() == 1 && jobs[1].m_aMembers.Count() == 1;
  ACR_TestRegress.Check("allocator: with two allowed, both attacks are made", both, Picks(squads, jobs));

  // 8. The 4 Nov Quarry case: an attack just launched at full strength isn't called off for grabs
  // worth a little more than it was at launch; once broken (below the abort share) it is.
  jobs.Clear();
  ACR_AIJob quarry = Attack("Quarry", 30, 0);
  quarry.m_fPatience = 600;
  quarry.m_bRunning = true;
  ACR_AIAllocator.JudgeRunning(quarry, 10, 0.5);
  jobs.Insert(quarry);
  jobs.Insert(Hold("GrabA", 7, 1));
  jobs.Insert(Hold("GrabB", 9, 1));
  jobs[1].m_fPatience = 0;
  jobs[2].m_fPatience = 0;
  squads.Clear();
  array<float> etaA = E(600, 0);
  etaA.Insert(0);
  array<float> etaB = E(600, 0);
  etaB.Insert(0);
  squads.Insert(Squad("A", 5, 0, etaA));
  squads.Insert(Squad("B", 5, 0, etaB));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool committed = squads[0].m_iPicked == 0 && squads[1].m_iPicked == 0;
  ACR_TestRegress.Check("allocator: an attack at the strength it set out with is fully ready (not called off at once)", committed, Picks(squads, jobs) + string.Format(" value %1", jobs[0].m_fScore));
  squads[0].m_fStrength = 2;
  squads[1].m_fStrength = 2;
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool broken = squads[0].m_iPicked != 0 && squads[1].m_iPicked != 0;
  ACR_TestRegress.Check("allocator: an attack below its abort share is worth nothing", broken, Picks(squads, jobs));

  // 9. Under the cap a running attack keeps its slot against a new one worth more.
  jobs.Clear();
  ACR_AIJob old = Attack("Old", 20, 6);
  old.m_bRunning = true;
  jobs.Insert(old);
  jobs.Insert(Attack("New", 40, 6));
  jobs.Insert(Hold("Post", 1));
  array<float> etaR = E(60, 60);
  etaR.Insert(0);
  array<float> etaF = E(60, 60);
  etaF.Insert(0);
  squads.Clear();
  squads.Insert(Squad("A", 8, 0, etaR));
  squads.Insert(Squad("B", 8, -1, etaF));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool kept = squads[0].m_iPicked == 0 && jobs[1].m_aMembers.IsEmpty();
  ACR_TestRegress.Check("allocator: a running attack keeps its slot under the cap against a new one worth more", kept, Picks(squads, jobs));

  // 10. The 4 Nov Gorey case: a running attack (26 with incumbency) keeps its squads against posts
  // worth a little more in all (30), and lets them go when those are clearly better (40).
  jobs.Clear();
  ACR_AIJob gorey = Attack("Gorey", 20, 0);
  gorey.m_bRunning = true;
  ACR_AIAllocator.JudgeRunning(gorey, 10, 0.5);
  jobs.Insert(gorey);
  jobs.Insert(Hold("PostA", 15, 1));
  jobs.Insert(Hold("PostB", 15, 1));
  array<float> etaG = E(0, 0);
  etaG.Insert(0);
  squads.Clear();
  squads.Insert(Squad("A", 5, 0, etaG));
  squads.Insert(Squad("B", 5, 0, etaG));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool margin = squads[0].m_iPicked == 0 && squads[1].m_iPicked == 0;
  ACR_TestRegress.Check("allocator: a running attack isn't called off for jobs only a little better", margin, Picks(squads, jobs));
  jobs[1].m_fValue = 20;
  jobs[2].m_fValue = 20;
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool released = squads[0].m_iPicked != 0 && squads[1].m_iPicked != 0;
  ACR_TestRegress.Check("allocator: a running attack lets its squads go for clearly better jobs", released, Picks(squads, jobs));

  // 10b. The 19 Nov Kamenoe case: three squads assaulting, a defence worth more on a forecast alone.
  // The commander offers it to committed assault squads only for a real threat there
  // (AttackerMayDefend: outnumbered now, or an HQ or FOB under attack), never for a forecast.
  bool rule = !ACR_AICommander.AttackerMayDefend(0, 0, true) && !ACR_AICommander.AttackerMayDefend(3, 6, false) && !ACR_AICommander.AttackerMayDefend(2, 6, true);
  rule = rule && ACR_AICommander.AttackerMayDefend(7, 6, false) && ACR_AICommander.AttackerMayDefend(3, 6, true);
  ACR_TestRegress.Check("keep assault: a forecast alone never pulls an assault squad; outnumbered, or an HQ or FOB under attack, may", rule, "");
  array<float> kamenoeFits = {};
  kamenoeFits.Insert(1);
  kamenoeFits.Insert(0); // the forecast-only defence: AttackerMayDefend(0, ...) is false
  bool forecastHolds;
  bool threatPulls;
  for (int pass = 0; pass < 2; pass++)
  {
   jobs.Clear();
   ACR_AIJob kamenoe = Attack("Kamenoe", 30, 0);
   kamenoe.m_bRunning = true;
   ACR_AIAllocator.JudgeRunning(kamenoe, 16, 0.35);
   jobs.Insert(kamenoe);
   ACR_AIJob fob = Hold("FOB 2", 50, 4);
   fob.m_iKind = ACR_EAITask.DEFEND;
   fob.m_fNeed = 5;
   fob.m_fPatience = 240;
   jobs.Insert(fob);
   if (pass == 1) kamenoeFits[1] = 1; // a real threat there: offered
   squads.Clear();
   squads.Insert(Squad("NILFT", 5, 0, E(0, 120), kamenoeFits));
   squads.Insert(Squad("BoF", 6, 0, E(0, 120), kamenoeFits));
   squads.Insert(Squad("Reservists", 5, 0, E(0, 120), kamenoeFits));
   ACR_AIAllocator.Allocate(jobs, squads, 1);
   if (pass == 0) forecastHolds = squads[0].m_iPicked == 0 && squads[1].m_iPicked == 0 && squads[2].m_iPicked == 0;
   else threatPulls = squads[0].m_iPicked == 1 || squads[1].m_iPicked == 1 || squads[2].m_iPicked == 1;
  }
  ACR_TestRegress.Check("keep assault: the allocator leaves an assaulting team whole against a forecast-only defence worth more", forecastHolds && threatPulls, Picks(squads, jobs) + string.Format(" (forecast held %1, real threat pulled %2)", forecastHolds, threatPulls));

  // 11. A running attack tries its own squads before a nearer outsider (with room for two, the
  // outsider used to take a member's place and the member was walked off).
  jobs.Clear();
  ACR_AIJob own = Attack("Own", 25, 0);
  own.m_bRunning = true;
  own.m_iMaxSquads = 2;
  ACR_AIAllocator.JudgeRunning(own, 10, 0.5);
  jobs.Insert(own);
  jobs.Insert(Hold("Post", 15, 3));
  squads.Clear();
  squads.Insert(Squad("A", 5, 0, E(100, 0)));
  squads.Insert(Squad("B", 5, 0, E(100, 0)));
  squads.Insert(Squad("C", 5, -1, E(10, 0)));
  ACR_AIAllocator.Allocate(jobs, squads, 1);
  bool ownFirst = squads[0].m_iPicked == 0 && squads[1].m_iPicked == 0 && squads[2].m_iPicked == 1;
  ACR_TestRegress.Check("allocator: a running attack keeps its own squads before a nearer outsider", ownFirst, Picks(squads, jobs));

  // 12. The 6 Nov Soapy Yar case: trucks are counted, one squad a truck. An attack that needs two
  // squads riding in isn't planned with one truck free (Reach would drop it at launch, every 2 min).
  jobs.Clear();
  jobs.Insert(Attack("SoapyYar", 40, 16));
  jobs.Insert(Hold("Pulkovo", 2));
  squads.Clear();
  squads.Insert(Squad("Walker", 6, -1, E(300, 0)));
  squads.Insert(Ride(Squad("RiderA", 6, -1, E(600, 0))));
  squads.Insert(Ride(Squad("RiderB", 6, -1, E(650, 0))));
  ACR_AIAllocator.Allocate(jobs, squads, 1, 2);
  bool twoTrucks = jobs[0].m_aMembers.Count() == 3;
  ACR_AIAllocator.Allocate(jobs, squads, 1, 1);
  bool oneTruck = jobs[0].m_aMembers.IsEmpty();
  ACR_TestRegress.Check("allocator: an attack needing two rides is planned with two trucks free, not with one", twoTrucks && oneTruck, Picks(squads, jobs) + string.Format(" (with two trucks: %1)", twoTrucks));
  // A one-squad job by truck with none left goes to no one; with one free it does.
  jobs.Clear();
  jobs.Insert(Hold("Grab", 10, 1));
  squads.Clear();
  ACR_AIJobSquad farSquad = Squad("Far", 6, -1, E(600, 0));
  farSquad.m_aByTruck.Insert(true);
  squads.Insert(farSquad);
  ACR_AIAllocator.Allocate(jobs, squads, 0, 0);
  bool noTruck = squads[0].m_iPicked == -1;
  ACR_AIAllocator.Allocate(jobs, squads, 0, 1);
  ACR_TestRegress.Check("allocator: a job by truck goes to no one with no truck free", noTruck && squads[0].m_iPicked == 0, Picks(squads, jobs));
  // The 6 Nov Topolinovo case: the air lift is counted the same way, one squad a lift. Two far
  // squads that each count on a 240 s lift don't make an attack with one lift; the walker and one
  // flyer don't reach its bar either, so it waits.
  jobs.Clear();
  jobs.Insert(Attack("Topolinovo", 40, 16));
  jobs.Insert(Hold("Svergovka", 2));
  squads.Clear();
  squads.Insert(Squad("Walker", 6, -1, E(300, 0)));
  squads.Insert(Fly(Squad("FlyerA", 6, -1, E(240, 0))));
  squads.Insert(Fly(Squad("FlyerB", 6, -1, E(240, 0))));
  ACR_AIAllocator.Allocate(jobs, squads, 1, -1, 2);
  bool twoLifts = jobs[0].m_aMembers.Count() == 3;
  ACR_AIAllocator.Allocate(jobs, squads, 1, -1, 1);
  bool oneLift = jobs[0].m_aMembers.IsEmpty() && squads[1].m_iPicked != 0 && squads[2].m_iPicked != 0;
  ACR_TestRegress.Check("allocator: an attack needing two air lifts is planned with two, not with the one there is", twoLifts && oneLift, Picks(squads, jobs) + string.Format(" (with two lifts: %1)", twoLifts));

  // 13. Moving up (ACR_AICommander.MoveUpPick), the 6 Nov Pulkovo case: a squad 9.5 km from the enemy,
  // our front 2.2 km. Bases: Zaprudny (front, 7 km off), Vybor (6.3 km from the enemy, 2.8 km off),
  // a neighbour 1 km off barely nearer the enemy.
  array<float> gaps = {2200.0, 6300.0, 9000.0};
  array<float> dists = {7000.0, 2800.0, 1000.0};
  array<float> scores = {7000.0, 2800.0, 1000.0};
  bool atFront;
  int truckPick = ACR_AICommander.MoveUpPick(gaps, dists, scores, 9500, 2200, true, atFront);
  bool frontByTruck = truckPick == 0 && atFront;
  int onFoot = ACR_AICommander.MoveUpPick(gaps, dists, scores, 9500, 2200, false, atFront);
  bool hops = onFoot == 1 && !atFront;
  ACR_TestRegress.Check("move up: by truck to the front, on foot one hop of at most 3 km toward it", frontByTruck && hops, string.Format("truck %1, foot %2", truckPick, onFoot));
  int stuck = ACR_AICommander.MoveUpPick(gaps, dists, scores, 6800, 2200, false, atFront);
  ACR_TestRegress.Check("move up: never to a base less than 1.5 km nearer the enemy (waits instead)", stuck == -1, string.Format("from 6.8 km on foot: %1", stuck));
  array<float> frontGaps = {2200.0, 2400.0};
  array<float> frontDists = {7000.0, 7500.0};
  array<float> frontScores = {7000.0, 5500.0};
  int lane = ACR_AICommander.MoveUpPick(frontGaps, frontDists, frontScores, 9500, 2200, true, atFront);
  ACR_TestRegress.Check("move up: of the front bases, the best posting score (the lane's pull) wins", lane == 1, string.Format("%1", lane));

  // The live checks follow at tick 8.
 }

 static void Live(ACR_TestHarness h)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) { ACR_TestRegress.Abort("allocator", "no range"); return; }
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  ACR_Base empty;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base && !base.m_bRetired && base.m_sOwner.IsEmpty() && base.IsVisibleTo("US")) { empty = base; break; }
  }
  if (!alpha || !alpha.GetAnchor() || !empty) { ACR_TestRegress.Abort("allocator", "no Alpha squad or no empty base the US side can see"); return; }
  // Every other US squad busy (base works: outside the allocator); Alpha free, 200 m from the base.
  array<ACR_Squad> others = {};
  array<int> tasks = {};
  foreach (ACR_Squad other : manager.m_Squads)
  {
   if (!other || other == alpha || !other.GetFaction() || other.GetFaction().GetFactionKey() != "US") continue;
   others.Insert(other);
   tasks.Insert(other.m_iAITask);
   other.m_iAITask = ACR_EAITask.FORTIFY;
  }
  IEntity anchor = alpha.GetAnchor();
  vector home = anchor.GetOrigin();
  anchor.SetOrigin(ACR_Manager.Ground(empty.GetOrigin() + Vector(200, 0, 0)));
  string name = alpha.m_Name;
  ACR_Options.Set("acrAIAllocator", "2");

  alpha.m_iAITask = ACR_EAITask.FREE;
  ACR_AICommander ai = new ACR_AICommander(manager, "US");
  ai.RunAllocationForTest(true);
  bool grabs = alpha.m_iAITask == ACR_EAITask.ATTACK && alpha.m_AITarget == empty && ai.IsAttackingForTest(empty);
  ACR_TestRegress.Check("allocator live: in control, a free squad 200 m from an empty base is sent to grab it", grabs, string.Format("%1: task %2, target %3", alpha.m_Name, typename.EnumToString(ACR_EAITask, alpha.m_iAITask), alpha.m_AITarget == empty));

  // A/B soak 7 Nov: the allocator moved a grab's only squad to another job, and the grab was then
  // "called off (too many losses)" and its base put on the stalled cooldown. Moved off, it ends
  // handed off: no cooldown on the base.
  ai.LeaveAttacksForTest(alpha);
  alpha.m_iAITask = ACR_EAITask.DEFEND;
  ai.UpdateAttacksForTest();
  bool handedOff = !ai.IsAttackingForTest(empty) && !ai.CoolingDownForTest(empty);
  ACR_TestRegress.Check("allocator live: a grab whose squad the allocator moves ends without a loss cooldown", handedOff, string.Format("still attacking %1, cooling down %2", ai.IsAttackingForTest(empty), ai.CoolingDownForTest(empty)));

  alpha.Halt("Holding");
  alpha.m_iAITask = ACR_EAITask.FREE;
  alpha.m_AITarget = null;
  alpha.m_Name = "Medic Team 1";
  ACR_AICommander medicAI = new ACR_AICommander(manager, "US");
  medicAI.RunAllocationForTest(true);
  bool stays = alpha.m_iAITask != ACR_EAITask.ATTACK;
  ACR_TestRegress.Check("allocator live: a medic team is never sent to grab", stays, string.Format("task %1", typename.EnumToString(ACR_EAITask, alpha.m_iAITask)));

  ACR_Options.Unset("acrAIAllocator");
  alpha.m_Name = name;
  alpha.Halt("Holding");
  alpha.m_iAITask = ACR_EAITask.FREE;
  alpha.m_AITarget = null;
  anchor.SetOrigin(home);
  foreach (int i, ACR_Squad squad : others) if (squad) squad.m_iAITask = tasks[i];
  ACR_TestRegress.Complete("allocator", CHECKS);
 }
}
