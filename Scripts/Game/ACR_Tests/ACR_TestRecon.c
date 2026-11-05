// -acrReconTest 1 (server-capable, ~1 min): recon flight and radio intercept (ACR_Recon). A Soviet
// squad, lined up where it stands, is photographed by a US recon flight: in the US intel, kept 5 min,
// fuzzed by one offset per group; the Soviets are told a jet went over; a summary line for the US.
// Radio intercept: a Soviet transmission gives the US a ring that holds the transmitter off its
// centre, and the same transmitter again moves the ring. The AI commander drops its blind guess at a
// photographed base and flies its recon over a fresh intercept near its bases first. ACA's own jets
// (Prefabs/Vehicles/Aircraft) load as models of their size, and each side flies its own. Their bombs
// load too, and where a player watches, a bomber's bomb hangs on its pylon, falls and lands on time.
class ACR_TestRecon
{
 static const int CHECKS = 14;
 static const int CALL_TICK = 6;
 static const int CHECK_TICK = 50;
 static ACR_Squad s_Enemy;
 static ref array<IEntity> s_Bodies;
 static ref array<vector> s_TrueAt;
 static int s_iRP;
 static float s_fCallAt;
 static ACR_LocalFlyover s_Bomber;
 static string s_sDrop;

 static void Run(ACR_TestHarness h, int tick)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !h.m_Range) return;
  ACR_Squad alpha = manager.FindSquadByGroup(h.m_Range.m_Group);
  if (tick == 1)
  {
   ACR_FactionSettings soviets = ACR_FactionSettings.Find("USSR");
   if (soviets) soviets.m_bAICommander = false;
   ACR_FactionSettings us = ACR_FactionSettings.Find("US");
   if (us) us.m_bAICommander = false;
   // Deterministic: everything under the track is seen, every transmission heard.
   ACR_ReconRun.SEE_OPEN = 1;
   ACR_ReconRun.SEE_COVERED = 1;
   ACR_ReconRun.SEE_VEHICLE = 1;
   ACR_RadioIntercept.INTERCEPT_CHANCE = 1;
  }
  if (!alpha || !alpha.GetAnchor()) return;
  if (tick == 4) { Pure(manager); Aircraft(); }
  if (tick == 4 || tick == 14 || tick == 22) Drop(tick);
  if (tick == CALL_TICK) Call(h, manager, alpha);
  // Held where they were put (a halted squad still walks back into formation), so the spots the
  // flight reports can be measured against where they really stood.
  if (tick > CALL_TICK && tick < CHECK_TICK && s_Bodies)
   foreach (int i, IEntity body : s_Bodies) if (body) body.SetOrigin(s_TrueAt[i]);
  if (tick == CHECK_TICK) Results(manager);
 }

 // ACA's blockout jets: each prefab spawns a model as long as the real aircraft (nose along the model's
 // +X, as ACR_LocalFlyover assumes), and the defaults hand each side its own recon and bomb-run jet.
 static void Aircraft()
 {
  array<ResourceName> jets = {ACR_Supports.BOMB_JET_WEST, ACR_Supports.BOMB_JET_EAST, ACR_ReconRun.RECON_JET_WEST, ACR_ReconRun.RECON_JET_EAST};
  array<float> lengths = {16.85, 14.62, 19.15, 19.05}; // the Su-25 without its nose probes (3-view: ~14.5 m body)
  bool client = GetGame().GetPlayerController() != null;
  int good = 0;
  string sizes = "";
  foreach (int i, ResourceName jet : jets)
  {
   IEntity plane = ACR_Supports.SpawnLocal(jet, ACR_Manager.Ground(Vector(500 + i * 40, 0, 500)) + "0 300 0", vector.Zero);
   vector mins, maxs;
   if (plane) plane.GetBounds(mins, maxs);
   float length = maxs[0] - mins[0];
   // a dedicated server builds no sound components: the engine is checked where a player hears it
   bool engine = plane && (plane.FindComponent(SoundComponent) || !client);
   if (plane && plane.GetVObject() && engine && Math.AbsFloat(length - lengths[i]) < 0.5 && maxs[1] - mins[1] < 5.5) good++;
   sizes += string.Format(" %1 %2x%3x%4 sound %5", FilePath.StripPath(jet), length, maxs[1] - mins[1], maxs[2] - mins[2], engine);
   if (plane) SCR_EntityHelper.DeleteEntityAndChildren(plane);
  }
  // where a player listens, a flyover 1 km off starts the engine loop
  bool sounding = !client;
  vector listener;
  if (client && ACR_LocalFlyover.Listener(listener))
  {
   vector from = listener + "1000 400 0";
   ACR_LocalFlyover.Start(ACR_ReconRun.RECON_JET_EAST, from, "0 0 1", 200, from[1], 2);
   ACR_LocalFlyover flight = ACR_LocalFlyover.Latest();
   // ...and, a turbojet, its scream (heard within 1.2 km)
   sounding = flight && flight.m_bSounding && flight.m_hEngine != AudioHandle.Invalid && flight.m_hEngineFar != AudioHandle.Invalid && flight.m_hScream != AudioHandle.Invalid;
  }
  ACR_TestRegress.Check("aircraft: ACA's four jets load as models of their real length, nose along +X, with the engine loops the flyover starts (near, far, a turbojet's scream)", good == 4 && sounding, sizes + string.Format(", client %1, flyover sounding %2", client, sounding));

  array<ref ACR_SupportEntry> west = {};
  array<ref ACR_SupportEntry> east = {};
  ACR_Supports.BuildDefaults(west, "US");
  ACR_Supports.BuildDefaults(east, "USSR");
  ResourceName westBomb = BombJet(west);
  ResourceName eastBomb = BombJet(east);
  bool rhs = ACR_Supports.HasPrefab(ACR_Supports.FA18_FLYOVER);
  bool bombs = westBomb == ACR_Supports.BOMB_JET_WEST && eastBomb == ACR_Supports.BOMB_JET_EAST;
  if (rhs) bombs = westBomb == ACR_Supports.FA18_FLYOVER && eastBomb == ACR_Supports.FA18_FLYOVER;
  ResourceName westRecon = ACR_ReconRun.ReconPrefab(null, "US");
  ResourceName eastRecon = ACR_ReconRun.ReconPrefab(null, "USSR");
  bool recon = westRecon == ACR_ReconRun.RECON_JET_WEST && eastRecon == ACR_ReconRun.RECON_JET_EAST;
  ACR_TestRegress.Check("aircraft: each side flies its own recon and bomb-run jet", bombs && recon, string.Format("bomb %1 / %2, recon %3 / %4, rhs %5", FilePath.StripPath(westBomb), FilePath.StripPath(eastBomb), FilePath.StripPath(westRecon), FilePath.StripPath(eastRecon), rhs));
 }

 // An A-10 1 km off with one Snakeye landing 16 s in (a fall of ~9 s with its petals out): hung on its pylon
 // at first (tick 4), let go and falling below it, nose down, short of the ground (tick 14, ~10 s in), gone
 // once it has landed (tick 22).
 // A dedicated server draws no bombs: checked where a player watches.
 static void Drop(int tick)
 {
  if (tick == 4)
  {
   // stowed and opened: the Mk 82 Snakeye (petals out), the OFAB-250ShN (brake chute streaming 3.9 m behind),
   // and the FAB-250 M-62
   array<ResourceName> names = {ACR_LocalFlyover.BOMB_MK82, ACR_FallingBomb.MK82_OPEN, ACR_LocalFlyover.BOMB_OFAB, ACR_FallingBomb.OFAB_OPEN, ACR_LocalFlyover.BOMB_FAB250};
   array<float> lengths = {2.18, 2.1, 1.99, 5.84, 1.96};
   int good = 0;
   string sizes = "";
   foreach (int i, ResourceName name : names)
   {
    IEntity bomb = ACR_Supports.SpawnLocal(name, ACR_Manager.Ground(Vector(700 + i * 40, 0, 500)) + "0 300 0", vector.Zero);
    vector mins, maxs;
    if (bomb) bomb.GetBounds(mins, maxs);
    if (bomb && bomb.GetVObject() && Math.AbsFloat(maxs[0] - mins[0] - lengths[i]) < 0.1) good++;
    sizes += string.Format(" %1 %2 m", FilePath.StripPath(name), maxs[0] - mins[0]);
    if (bomb) SCR_EntityHelper.DeleteEntityAndChildren(bomb);
   }
   ACR_TestRegress.Check("aircraft: the bombs load as models of their real length, shut and opened", good == names.Count(), sizes);
  }
  vector listener;
  if (!GetGame().GetPlayerController() || !ACR_LocalFlyover.Listener(listener))
  {
   if (tick == 22) ACR_TestRegress.Check("bomb run: a bomb hangs on the jet, falls and lands on time", true, "no player here: nothing drawn");
   return;
  }
  if (tick == 4)
  {
   vector from = listener + "1000 300 -1000";
   vector dir = "0 0 1";
   ACR_LocalFlyover.Start(ACR_Supports.BOMB_JET_WEST, from, dir, 200, from[1], 30);
   s_Bomber = ACR_LocalFlyover.Latest();
   if (!s_Bomber) return;
   s_Bomber.AddBomb(from + dir * 3200, 16);
   ACR_FallingBomb hung = s_Bomber.m_aBombs[0];
   float off = -1;
   if (hung.m_Body) off = vector.Distance(hung.m_Body.GetOrigin(), s_Bomber.m_Plane.GetOrigin());
   s_sDrop = string.Format("hung %1 m from the jet's origin (on its pylon), falling %2; ", off, hung.m_bFalling);
   if (!hung.m_Body || off < 1 || off > 6 || hung.m_bFalling) s_sDrop += "BAD ";
   return;
  }
  if (!s_Bomber || s_Bomber.m_aBombs.IsEmpty())
  {
   if (tick == 22) ACR_TestRegress.Check("bomb run: a bomb hangs on the jet, falls and lands on time", false, s_sDrop + "the bomber went");
   return;
  }
  ACR_FallingBomb bomb = s_Bomber.m_aBombs[0];
  if (tick == 14)
  {
   float below = -1;
   float above = -1;
   float pitch = 0;
   if (bomb.m_Body && s_Bomber.m_Plane)
   {
    vector at = bomb.m_Body.GetOrigin();
    vector jet = s_Bomber.m_Plane.GetOrigin();
    below = jet[1] - at[1];
    above = at[1] - bomb.m_vImpact[1];
    vector m[4];
    bomb.m_Body.GetTransform(m);
    vector nose = m[0];
    pitch = nose[1];
   }
   s_sDrop += string.Format("at ~10 s falling %1, %2 m below the jet, %3 m up, nose %4; ", bomb.m_bFalling, below, above, pitch);
   if (!bomb.m_bFalling || below < 20 || above < 1 || pitch > -0.05) s_sDrop += "BAD ";
   return;
  }
  s_sDrop += string.Format("at ~18 s landed %1", bomb.m_bDone && !bomb.m_Body);
  ACR_TestRegress.Check("bomb run: a bomb hangs on the jet, falls and lands on time", !s_sDrop.Contains("BAD") && bomb.m_bDone && !bomb.m_Body, s_sDrop);
 }

 static ResourceName BombJet(array<ref ACR_SupportEntry> entries)
 {
  foreach (ACR_SupportEntry entry : entries)
   if (entry.m_eType == ACR_ESupportType.BOMB_RUN && entry.m_eBombRunDelivery == ACR_EBombRunDelivery.FLYOVER) return entry.m_sBombRunPrefab;
  return "";
 }

 // Geometry, names, intercepts and the AI's use of them: no flight needed.
 static void Pure(ACR_Manager manager)
 {
  vector start = Vector(1000, 0, 1000);
  vector north = Vector(0, 0, 1);
  bool inside = ACR_ReconRun.InCorridor(Vector(1250, 0, 2000), start, north, 2400, 300);
  bool wide = ACR_ReconRun.InCorridor(Vector(1350, 0, 2000), start, north, 2400, 300);
  bool behind = ACR_ReconRun.InCorridor(Vector(1000, 0, 950), start, north, 2400, 300);
  bool beyond = ACR_ReconRun.InCorridor(Vector(1000, 0, 3500), start, north, 2400, 300);
  ACR_TestRegress.Check("recon: the corridor is the track's length and width", inside && !wide && !behind && !beyond, string.Format("inside %1 wide %2 behind %3 beyond %4", inside, wide, behind, beyond));

  ACR_Intel us = manager.IntelFor("US");
  us.m_Intercepts.Clear();
  vector from = ACR_Manager.Ground(Vector(4000, 0, 6000));
  ACR_RadioIntercept.Transmitted("USSR", from, "Test transmitter");
  float offset = -1;
  if (us.m_Intercepts.Count() == 1) offset = vector.DistanceXZ(us.m_Intercepts[0].m_vCentre, from);
  float r = ACR_RadioIntercept.INTERCEPT_RADIUS;
  array<vector> zonePos = {};
  array<float> zoneRadii = {};
  array<string> zoneLabels = {};
  ACR_RadioIntercept.Zones("US", zonePos, zoneRadii, zoneLabels);
  bool ring = offset >= r * 0.25 - 1 && offset <= r * 0.66 + 1 && zoneLabels.Contains("~Radio intercept");
  ACR_TestRegress.Check("radio intercept: a ring that holds the transmitter, off its centre, on the map", ring, string.Format("rings %1 offset %2 labels %3", us.m_Intercepts.Count(), offset, zoneLabels.Count()));
  ACR_RadioIntercept.Transmitted("USSR", from + Vector(30, 0, 0), "Test transmitter");
  ACR_TestRegress.Check("radio intercept: the same transmitter again moves its ring", us.m_Intercepts.Count() == 1, string.Format("rings %1", us.m_Intercepts.Count()));

  ACR_AICommander ai = manager.EnsureAICommander("US");
  ACR_Base enemyBase;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base && !base.m_bRetired && base.m_sOwner == "USSR" && base.IsVisibleTo("US")) { enemyBase = base; break; }
  int before = -1;
  int after = -1;
  if (ai && enemyBase)
  {
   ai.AssessForTest();
   before = ai.DefenceOfForTest(enemyBase);
   us.Photographed(enemyBase, GetGame().GetWorld().GetWorldTime());
   ai.AssessForTest();
   after = ai.DefenceOfForTest(enemyBase);
  }
  ACR_TestRegress.Check("AI: a photographed enemy base loses the blind guess at its defence", before - after == ACR_AICommander.UNKNOWN_DEFENCE, string.Format("%1: %2 -> %3", enemyBase, before, after));

  ACR_Base ownBase;
  foreach (ACR_Base mine : ACR_Base.s_All)
   if (mine && !mine.m_bRetired && mine.m_sOwner == "US") { ownBase = mine; break; }
  us.m_Intercepts.Clear();
  string why;
  vector picked;
  vector ringAt;
  if (ai && ownBase)
  {
   ACR_RadioIntercept.Heard(us, ownBase.GetOrigin() + Vector(300, 0, 0), "USSR:near");
   ringAt = us.m_Intercepts[0].m_vCentre;
   picked = ai.ReconTarget(why);
  }
  ACR_TestRegress.Check("AI: its recon goes over a fresh radio intercept near its bases first", ringAt != vector.Zero && vector.DistanceXZ(picked, ringAt) < 1, string.Format("ring %1 picked %2 (%3)", ringAt, picked, why));
  us.m_Intercepts.Clear();
 }

 static void Call(ACR_TestHarness h, ACR_Manager manager, ACR_Squad alpha)
 {
  if (!manager.m_Supports) manager.m_Supports = new ACR_Supports(manager);
  foreach (ACR_Squad squad : manager.m_Squads)
   if (squad.GetFaction() && squad.GetFaction().GetFactionKey() == "USSR" && squad.CountAlive() >= 3 && !squad.m_bMortar && !squad.Aircraft() && squad.GetAnchor() && !ACR_Transport.VehicleOf(squad.GetAnchor())) { s_Enemy = squad; break; }
  if (!s_Enemy)
  {
   ACR_TestRegress.Abort("recon", "no Soviet squad to photograph");
   return;
  }
  // Where it stands (on land, away from Alpha): its men in a line there, halted.
  vector target = ACR_Manager.Ground(s_Enemy.GetAnchor().GetOrigin());
  s_Bodies = {};
  s_TrueAt = {};
  array<AIAgent> agents = {};
  s_Enemy.GetAgents(agents);
  foreach (int i, AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || ACR_Transport.VehicleOf(body)) continue;
   vector at = ACR_Manager.Ground(target + Vector(i * 3, 0, 0));
   body.SetOrigin(at);
   s_Bodies.Insert(body);
   s_TrueAt.Insert(at);
  }
  s_Enemy.Halt("Test");
  ACR_FactionData us = manager.m_Bases.GetFaction("US");
  us.m_iRP = 500;
  int index = h.SupportIndex(manager, ACR_ESupportType.RECON_FLIGHT);
  // The AI commanders may have flown one before they were switched off: cooldown cleared.
  if (index >= 0) us.m_aSupports[index].m_fReadyAt = 0;
  s_fCallAt = GetGame().GetWorld().GetWorldTime();
  string result = manager.m_Supports.Call("US", index, target);
  s_iRP = us.m_iRP;
  PrintFormat("[ACR] RECONTEST call (entry %1) over %2 with %3 men of %4: %5", index, target, s_Bodies.Count(), s_Enemy.GetName(), result);
  ACR_TestRegress.Check("recon: the call is accepted and paid", index >= 0 && result.Contains("overhead") && s_iRP == 500 - us.m_aSupports[index].m_iCost, string.Format("entry %1 RP %2: %3", index, s_iRP, result));
 }

 static void Results(ACR_Manager manager)
 {
  if (!s_Enemy || !s_Bodies) return;
  ACR_Intel us = manager.IntelFor("US");
  float now = GetGame().GetWorld().GetWorldTime();
  int kept;
  bool within = true;
  bool together = true;
  bool moved;
  vector firstOffset;
  bool haveFirst;
  foreach (int i, IEntity body : s_Bodies)
  {
   if (!body || us.KeepUntil(body) < s_fCallAt + ACR_ReconRun.RECON_MEMORY_MS) continue; // photographed by this flight
   kept++;
   vector offset = us.m_SeenAt.Get(body) - s_TrueAt[i];
   offset[1] = 0;
   if (offset.Length() > ACR_ReconRun.FUZZ_INFANTRY + 5) within = false;
   if (offset.Length() > 1) moved = true;
   if (!haveFirst) { firstOffset = offset; haveFirst = true; }
   else if (vector.DistanceXZ(offset, firstOffset) > 4) together = false;
  }
  PrintFormat("[ACR] RECONTEST %1 of %2 photographed (recon sightings total %3), offset %4", kept, s_Bodies.Count(), ACR_ReconRun.s_iSeen, firstOffset);
  ACR_TestRegress.Check("recon: the squad under the track is in the side's intel, kept 5 min", s_Bodies.Count() > 0 && kept * 2 >= s_Bodies.Count(), string.Format("%1 of %2", kept, s_Bodies.Count()));
  ACR_TestRegress.Check("recon: spots fuzzed within the limit, one offset for the group", kept > 0 && within && together && moved, string.Format("within %1 together %2 moved %3", within, together, moved));
  bool warned;
  foreach (string line : manager.IntelFor("USSR").m_Feed) if (line.Contains("reconnaissance aircraft")) warned = true;
  ACR_TestRegress.Check("recon: the side flown over hears the jet", warned, string.Format("USSR feed %1", manager.IntelFor("USSR").m_Feed.Count()));
  bool summary;
  foreach (string usLine : us.m_Feed) if (usLine.Contains(ACR_ReconRun.SPOTTER + " over")) summary = true;
  ACR_TestRegress.Check("recon: a summary line in the caller's feed", summary, string.Format("US feed %1", us.m_Feed.Count()));
  ACR_TestRegress.Complete("recon", CHECKS);
 }
}
