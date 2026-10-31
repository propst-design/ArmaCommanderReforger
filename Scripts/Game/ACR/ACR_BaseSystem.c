// Server-side bases, capture, garrisons, hidden-base discovery and requisition
// income. Numbers follow the Arma 3 mod (defines/commonDefines.inc, economy.sqf).
class ACR_FactionData
{
 int m_iRP;
 ref array<ref ACR_RequisitionEntry> m_aRequisitions = {};
 ref array<ResourceName> m_aGarrison = {};
 ref array<ref ACR_SupportEntry> m_aSupports = {};
}

class ACR_BaseSystem : Managed
{
 static const float CAPTURE_RATE = 0.4;     // score per second per soldier of difference
 static const float CAPTURE_MAX_STEP = 6;   // cap per second (original mod: 2; raised so more troops cap faster)
 static const float SPAWN_RANGE = 400;      // enemies this close deploy the garrison
 static const float DESPAWN_RANGE = 450;    // no enemies this close: garrison packs up
 static const int POINTS_CAP = 1000;

 int m_iIncomeInterval = 60;                // seconds
 int m_iBasicIncome = 5;
 int m_iStartingRP = 50;
 protected float m_fNextIncome = -1;

 static const ResourceName GROUP_BASE = "{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et";
 protected ref map<string, ref ACR_FactionData> m_Factions = new map<string, ref ACR_FactionData>();
 ref array<SCR_AIGroup> m_GarrisonGroups = {};

 // per-tick census
 protected ref array<IEntity> m_Bodies = {};
 protected ref array<string> m_BodyFactions = {};

 // Buy list, garrison soldiers and RP for a faction, built from its
 // ACR_FactionSettings entity (or catalog defaults) the first time it's needed.
 // Setup phase ended: build buy lists and RP again with the chosen options.
 void ForgetFactions() { m_Factions.Clear(); ACR_FactionCatalog.ForgetResolved(); }

 ACR_FactionData GetFaction(string factionKey)
 {
  ACR_FactionData data;
  if (m_Factions.Find(factionKey, data)) return data;
  data = new ACR_FactionData();
  ACR_FactionSettings settings = ACR_FactionSettings.Find(factionKey);
  data.m_iRP = m_iStartingRP;
  if (settings) data.m_iRP = settings.m_iStartingRP;
  if (ACR_Manager.StartRPOption() > 0) data.m_iRP = ACR_Manager.StartRPOption();
  ACR_FactionCatalog.BuildRequisitions(factionKey, settings, data.m_aRequisitions);
  ACR_FactionCatalog.BuildGarrisonPool(factionKey, settings, data.m_aGarrison);
  if (!settings || settings.m_bSupports)
  {
   if (settings && settings.m_aSupports && !settings.m_aSupports.IsEmpty())
   {
    bool listsAirLift = false;
    foreach (ACR_SupportEntry configured : settings.m_aSupports)
    {
     if (configured && configured.m_eType == ACR_ESupportType.AIR_LIFT) listsAirLift = true;
     if (!configured || !configured.m_bEnabled) continue;
     data.m_aSupports.Insert(configured.Copy());
    }
    // Lists made before the air lift existed (14 Oct) get it too; a row for it (on or off) decides.
    if (!listsAirLift)
    {
     ACR_SupportEntry lift = new ACR_SupportEntry();
     lift.m_eType = ACR_ESupportType.AIR_LIFT;
     lift.m_iCost = -1;
     lift.m_iCooldown = -1;
     lift.m_iAICost = -1;
     lift.Resolve();
     data.m_aSupports.Insert(lift);
    }
   }
   else ACR_Supports.BuildDefaults(data.m_aSupports);
  }
  m_Factions.Insert(factionKey, data);
  return data;
 }

 bool Spend(string factionKey, int amount)
 {
  ACR_FactionData data = GetFaction(factionKey);
  if (data.m_iRP < amount) return false;
  data.m_iRP -= amount;
  ACR_Report.OnSpend(factionKey, amount);
  return true;
 }

 void Refund(string factionKey, int amount)
 {
  ACR_FactionData data = GetFaction(factionKey);
  if (!data) return;
  data.m_iRP = Math.Min(data.m_iRP + amount, POINTS_CAP);
  ACR_Report.OnRefund(factionKey, amount);
 }

 bool IsGarrison(SCR_AIGroup group)
 {
  return m_GarrisonGroups.Contains(group);
 }

 // ================================================================ economy

 int GetRP(string factionKey)
 {
  return GetFaction(factionKey).m_iRP;
 }

 int GetIncome(string factionKey)
 {
  int income = m_iBasicIncome;
  foreach (ACR_Base base : ACR_Base.s_All)
   if (base.m_sOwner == factionKey && base.m_bIncome) income += base.m_iValue;
  return income;
 }

 static bool s_bNoIncomeForTest;

 int SecondsToIncome()
 {
  if (m_fNextIncome < 0) return m_iIncomeInterval;
  return Math.Max(0, Math.Ceil((m_fNextIncome - GetGame().GetWorld().GetWorldTime()) / 1000));
 }

 protected void TickIncome(ACR_Manager manager)
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (m_fNextIncome < 0) m_fNextIncome = now + m_iIncomeInterval * 1000;
  if (now < m_fNextIncome) return;
  m_fNextIncome = now + m_iIncomeInterval * 1000;
  if (s_bNoIncomeForTest) return;
  array<Faction> factions = {};
  GetGame().GetFactionManager().GetFactionsList(factions);
  foreach (Faction faction : factions)
  {
   string key = faction.GetFactionKey();
   int added = Math.Round(GetIncome(key) * ACR_Manager.IncomeOption());
   ACR_FactionData data = GetFaction(key);
   int before = data.m_iRP;
   data.m_iRP = Math.Min(data.m_iRP + added, POINTS_CAP);
   ACR_Report.OnIncome(key, data.m_iRP - before);
  }
  PrintFormat("[ACR] Income paid");
 }

 // ================================================================ bases

 void Tick(ACR_Manager manager)
 {
  CollectBodies(manager);
  foreach (ACR_Base base : ACR_Base.s_All)
   UpdateBase(manager, base);
  TickIncome(manager);
 }

 // Every living, conscious character with a faction: AI agents plus player bodies. The
 // unconscious don't hold, contest or spot (Jacob 19 Oct: nobody comes to finish them off).
 protected void CollectBodies(ACR_Manager manager)
 {
  m_Bodies.Clear();
  m_BodyFactions.Clear();
  array<AIAgent> agents = {};
  GetGame().GetAIWorld().GetAIAgents(agents);
  foreach (AIAgent agent : agents)
   AddBody(agent.GetControlledEntity(), manager);
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
   AddBody(GetGame().GetPlayerManager().GetPlayerControlledEntity(id), manager);
 }

 protected void AddBody(IEntity entity, ACR_Manager manager)
 {
  ChimeraCharacter character = ChimeraCharacter.Cast(entity);
  if (!character || m_Bodies.Contains(character) || character.GetCharacterController().IsDead() || character.GetCharacterController().IsUnconscious()) return;
  // Stationed support helicopter crews are not capture troops.
  AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
  if (control && control.GetControlAIAgent() && manager.m_Supports)
  {
   SCR_AIGroup group = SCR_AIGroup.Cast(control.GetControlAIAgent().GetParentGroup());
   if (group && manager.m_Supports.IsSupportGroup(group)) return;
  }
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
  if (!affiliation || !affiliation.GetAffiliatedFaction()) return;
  m_Bodies.Insert(character);
  m_BodyFactions.Insert(affiliation.GetAffiliatedFaction().GetFactionKey());
 }

 protected void UpdateBase(ACR_Manager manager, ACR_Base base)
 {
  vector center = base.GetOrigin();
  map<string, int> inside = new map<string, int>();
  bool enemyNear, enemyInRange, enemyApproaching;
  foreach (int i, IEntity body : m_Bodies)
  {
   if (!body) continue; // deleted this tick (e.g. a garrison packed up after a capture)
   string key = m_BodyFactions[i];
   float distance = vector.DistanceXZ(body.GetOrigin(), center);
   // FIA's ally neither fights nor takes its pockets (ACR_Insurgency).
   bool friendOfOwner = base.m_bPocket && key != base.m_sOwner && ACR_Insurgency.Friendly(key, base.m_sOwner);
   if (distance <= base.m_fRadius && !friendOfOwner)
   {
    int count;
    inside.Find(key, count);
    inside.Set(key, count + 1);
    if (!base.m_aDetectedBy.Contains(key))
    {
     ACR_Intel finder;
     if (base.m_bBuilt && base.m_bHidden && key != base.m_sOwner) finder = manager.IntelFor(key);
     if (finder) finder.FobFound(base, "", GetGame().GetWorld().GetWorldTime());
     else
     {
      base.m_aDetectedBy.Insert(key);
      if (base.m_bHidden && key != base.m_sOwner) manager.NotifyFaction(key, string.Format("Enemy base discovered: %1", base.m_sName));
     }
    }
   }
   if (!base.m_sOwner.IsEmpty() && key != base.m_sOwner && !friendOfOwner)
   {
    // An FIA pocket is found once its garrison would deploy against you.
    // Hidden FIA (-acrFIAHidden): only by entering it (above), and nobody is told.
    if (base.m_bPocket && distance <= SPAWN_RANGE && !base.m_aDetectedBy.Contains(key) && !ACR_Guerrillas.Hidden())
    {
     base.m_aDetectedBy.Insert(key);
     manager.NotifyFaction(key, string.Format("FIA resistance at %1", base.m_sName));
     PrintFormat("[ACR] POCKETS %1 found by %2", base.m_sName, key);
    }
    if (distance <= SPAWN_RANGE) enemyNear = true;
    if (distance <= SPAWN_RANGE + 300) enemyApproaching = true;
    if (distance <= DESPAWN_RANGE) enemyInRange = true;
   }
  }
  UpdateCapture(manager, base, inside);
  if (enemyApproaching && !base.m_GarrisonGroup && base.m_iGarrisonLeft > 0) ACR_Garrison.Prepare(center, GarrisonArea(base));
  UpdateGarrison(manager, base, enemyNear, enemyInRange);
  UpdatePatrol(base);
 }

 // Buildings this far from a base's centre are garrisoned by its own soldiers.
 static float GarrisonArea(ACR_Base base)
 {
  return Math.Min(base.m_fRadius, 80);
 }

 // Strongest faction inside other than 'exclude'.
 protected string Strongest(map<string, int> inside, string exclude, out int best)
 {
  string winner;
  best = 0;
  foreach (string key, int count : inside)
  {
   if (key == exclude || count <= best) continue;
   best = count;
   winner = key;
  }
  return winner;
 }

 protected void UpdateCapture(ACR_Manager manager, ACR_Base base, map<string, int> inside)
 {
  int attackers;
  if (!base.m_sOwner.IsEmpty())
  {
   int defenders;
   inside.Find(base.m_sOwner, defenders);
   string attacker = Strongest(inside, base.m_sOwner, attackers);
   bool wasContested = base.m_bContested;
   base.m_bContested = attackers > 0;
   if (base.m_bContested && !wasContested)
    manager.NotifyFaction(base.m_sOwner, string.Format("%1 is under attack", base.m_sName));
   float change = 1; // secure again over time when no attackers are inside
   if (attackers > 0) change = Math.Clamp((defenders - attackers) * CAPTURE_RATE, -CAPTURE_MAX_STEP, CAPTURE_MAX_STEP);
   base.m_fScore = Math.Clamp(base.m_fScore + change, -100, 100);
   if (base.m_fScore <= -100) Capture(manager, base, attacker);
   return;
  }
  // Neutral base: whoever holds it longest takes it.
  int holders;
  if (!base.m_sCapturer.IsEmpty()) inside.Find(base.m_sCapturer, holders);
  string challenger = Strongest(inside, base.m_sCapturer, attackers);
  if (holders == 0 && attackers > 0 && base.m_fScore <= 0)
  {
   base.m_sCapturer = challenger;
   holders = attackers;
   Strongest(inside, challenger, attackers);
  }
  base.m_bContested = holders > 0 && attackers > 0;
  float step = -1;
  if (holders > 0 || attackers > 0) step = Math.Clamp((holders - attackers) * CAPTURE_RATE, -CAPTURE_MAX_STEP, CAPTURE_MAX_STEP);
  base.m_fScore = Math.Clamp(base.m_fScore + step, 0, 100);
  if (base.m_fScore <= 0) base.m_sCapturer = "";
  if (base.m_fScore >= 100 && !base.m_sCapturer.IsEmpty()) Capture(manager, base, base.m_sCapturer);
 }

 // Debug menu: hand a base over at once.
 void DebugCapture(ACR_Manager manager, ACR_Base base, string factionKey) { Capture(manager, base, factionKey); }

 protected void Capture(ACR_Manager manager, ACR_Base base, string factionKey)
 {
  string previous = base.m_sOwner;
  DespawnGarrison(base, false);
  DespawnPatrol(base, false);
  base.m_sOwner = factionKey;
  base.m_bPocket = false;
  base.m_sCapturer = "";
  base.m_fScore = 100;
  base.m_bContested = false;
  // A captured FOB's buildings change hands with it (HQ, upgrades, free-form compositions).
  if (base.m_bBuilt && manager.m_Structures) manager.m_Structures.TransferFob(base, factionKey);
  if (manager.m_Structures) manager.m_Structures.UpdateHousing(base);
  // Like the original: the new owner gets half the original garrison.
  base.m_iGarrisonLeft = Math.Ceil(base.m_iGarrison * 0.5);
  if (!base.m_aDetectedBy.Contains(factionKey)) base.m_aDetectedBy.Insert(factionKey);
  PrintFormat("[ACR] Base captured: %1 by %2 (was %3)", base.m_sName, factionKey, previous);
  ACR_Report.OnCapture(base.m_sName, factionKey, previous);
  ACR_DoctrineIntel.OnCapture(manager, factionKey, previous);
  manager.NotifyFaction(factionKey, string.Format("%1 captured", base.m_sName));
  if (!previous.IsEmpty()) manager.NotifyFaction(previous, string.Format("%1 lost", base.m_sName));
 }

 // ================================================================ garrisons

 // Housing grants defenders once on completion, including during an attack.
 // Losing housing reduces capacity and recalls only excess surviving AI guards.
 void ResizeGarrison(ACR_Base base, int change)
 {
  base.m_iGarrison = Math.Max(0, base.m_iGarrison + change);
  int alive = base.m_iGarrisonLeft;
  if (base.m_GarrisonGroup) alive = CountAlive(base.m_GarrisonGroup);
  int wanted = Math.Min(base.m_iGarrison, alive + Math.Max(0, change));
  base.m_iGarrisonLeft = wanted;
  SCR_AIGroup group = base.m_GarrisonGroup;
  if (!group) return;
  if (wanted > alive)
  {
   array<ResourceName> pool = GetFaction(base.m_sOwner).m_aGarrison;
   if (pool.IsEmpty()) return;
   for (int i = alive; i < wanted; i++) AddGarrisonSoldier(base, group, pool);
  }
  else if (wanted < alive)
  {
   array<AIAgent> agents = {};
   group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity soldier = agent.GetControlledEntity();
    if (alive <= wanted) break;
    if (!soldier || !ACR_Respawn.IsAlive(soldier) || ACR_Squad.FindController(soldier)) continue;
    SCR_EntityHelper.DeleteEntityAndChildren(soldier);
    alive--;
   }
  }
 }

 // #73: where a garrison, patrol or guard soldier appears near center: clear of walls (a 0.6 m
 // wide, 2 m tall cylinder), not under a roof, and on the soldiers' navmesh. Before, a soldier
 // whose random spot had no room within 5 m spawned there anyway, and a spot with room inside a
 // house was dropped to the terrain under its floor: FIA in walls, every village. Each try reaches
 // further out; with none good, the navmesh point nearest the centre (walkable, maybe indoors),
 // else the clear ground nearest it.
 // Where the navmesh isn't loaded (no AI near on a server) room and roof alone decide.
 // Soldiers with window posts are moved onto them afterwards (ACR_Garrison), as before.
 static const int SPOT_TRIES = 10;
 static vector SoldierSpot(vector center, float spread, IEntity navAgent)
 {
  AIPathfindingComponent pathing;
  if (navAgent) pathing = AIPathfindingComponent.Cast(navAgent.FindComponent(AIPathfindingComponent));
  NavmeshWorldComponent mesh;
  if (pathing) mesh = pathing.GetNavmeshComponent();
  if (mesh && !mesh.IsTileLoaded(center)) mesh.LoadTileIn(center);
  vector onMesh;
  for (int attempt = 0; attempt < SPOT_TRIES; attempt++)
  {
   float reach = spread + attempt * 4;
   vector candidate = ACR_Manager.Ground(center + Vector(Math.RandomFloat(-reach, reach), 0, Math.RandomFloat(-reach, reach)));
   vector free;
   if (!SCR_WorldTools.FindEmptyTerrainPosition(free, candidate, 6, 0.6, 2)) continue;
   free = ACR_Manager.Ground(free);
   // The finder's spot may sit on something; on the terrain it must still have room.
   if (!RoomAt(free) || Covered(free)) continue;
   if (!pathing || (mesh && !mesh.IsTileLoaded(free))) return free;
   if (!pathing.GetClosestPositionOnNavmesh(free, Vector(1, 1.5, 1), onMesh)) continue;
   if (vector.DistanceXZ(onMesh, free) > 1) continue;
   return free;
  }
  vector near = ACR_Manager.Ground(center);
  if (pathing && pathing.GetClosestPositionOnNavmesh(near, Vector(30, 10, 30), onMesh)) return onMesh + Vector(0, 0.15, 0);
  vector wide;
  if (SCR_WorldTools.FindEmptyTerrainPosition(wide, near, 40, 0.6, 2)) return ACR_Manager.Ground(wide);
  return near;
 }

 // Room for a soldier standing right here (only the spot itself is tried).
 static bool RoomAt(vector spot)
 {
  vector free;
  return SCR_WorldTools.FindEmptyTerrainPosition(free, spot, 0.7, 0.6, 2) && vector.DistanceXZ(free, spot) < 0.1;
 }

 // Something other than a tree or a person overhead within 25 m: a roof, a floor, a bridge.
 static bool Roofed(vector spot)
 {
  TraceParam trace = new TraceParam();
  trace.Start = spot + Vector(0, 0.3, 0);
  trace.End = spot + Vector(0, 25, 0);
  trace.Flags = TraceFlags.ENTS;
  float hit = GetGame().GetWorld().TraceMove(trace, null);
  if (hit >= 1 || !trace.TraceEnt) return false;
  return !Tree.Cast(trace.TraceEnt) && !ChimeraCharacter.Cast(trace.TraceEnt);
 }

 // #87: inside a heap, seen from above: a sand or gravel pile, rubble or a low roof over the spot.
 // The game's empty-spot finder hands out spots inside piles (10 of 20 round two piles in
 // srv:regress), and squads, bought units and spawn points took its spots unchecked: squads stood
 // buried in industrial yards. Walks down from 30 m, past trees, bushes and people (the soldier
 // standing there included).
 static bool Buried(vector spot)
 {
  TraceParam trace = new TraceParam();
  trace.Flags = TraceFlags.ENTS;
  vector top = spot + Vector(0, 30, 0);
  vector bottom = spot + Vector(0, 0.4, 0);
  for (int pass = 0; pass < 6; pass++)
  {
   trace.Start = top;
   trace.End = bottom;
   float hit = GetGame().GetWorld().TraceMove(trace, null);
   if (hit >= 1 || !trace.TraceEnt) return false;
   IEntity root = trace.TraceEnt.GetRootParent();
   if (!Tree.Cast(trace.TraceEnt) && !ChimeraCharacter.Cast(root)) return true;
   top = top + (bottom - top) * hit - Vector(0, 0.05, 0);
   if (top[1] <= bottom[1]) return false;
  }
  return false;
 }

 static bool Covered(vector spot) { return Roofed(spot) || Buried(spot); }

 // Clear ground for something clearance wide near center: the game's empty spots, minus those
 // under a heap (#87). False when there's none within radius.
 static bool ClearGround(out vector result, vector center, float radius, float clearance, float height = 2.5)
 {
  array<vector> spots = {};
  SCR_WorldTools.FindAllEmptyTerrainPositions(spots, center, radius, clearance, height, 12);
  foreach (vector spot : spots)
  {
   vector ground = ACR_Manager.Ground(spot);
   if (Buried(ground)) continue;
   result = ground;
   return true;
  }
  return false;
 }

 // #87: a squad the game spawned in its formation round one spot: anyone who came out inside a
 // heap or under a roof steps to clear ground near where he stands. The game queues its members
 // over a few frames, so it looks twice.
 static void SettleSoon(SCR_AIGroup group)
 {
  if (!group) return;
  GetGame().GetCallqueue().CallLater(Settle, 1500, false, group, 0);
 }

 protected static void Settle(SCR_AIGroup group, int pass)
 {
  if (!group) return;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity body = agent.GetControlledEntity();
   if (!body || !ACR_Respawn.IsAlive(body) || EntityUtils.IsPlayer(body) || ACR_Transport.VehicleOf(body)) continue;
   vector at = body.GetOrigin();
   if (!Covered(at)) continue;
   vector spot = SoldierSpot(at, 3, agent);
   if (Covered(spot)) continue;
   body.SetOrigin(spot);
   PrintFormat("[ACR] SPAWN %1: a soldier who came out inside something stepped %2 m clear", group.GetCustomName(), (int)vector.DistanceXZ(at, spot));
  }
  if (pass < 1) GetGame().GetCallqueue().CallLater(Settle, 4000, false, group, pass + 1);
 }

 protected void AddGarrisonSoldier(ACR_Base base, SCR_AIGroup group, array<ResourceName> pool)
 {
  float spread = Math.Min(base.m_fRadius * 0.6, 25);
  vector spot = SoldierSpot(base.GetOrigin(), spread, ACR_Garrison.AnyAgent());
  IEntity soldier = ACR_Manager.SpawnPrefab(pool.GetRandomElement(), spot);
  if (!soldier) return;
  if (!group.AddAIEntityToGroup(soldier))
  {
   SCR_EntityHelper.DeleteEntityAndChildren(soldier);
   return;
  }
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(soldier.FindComponent(FactionAffiliationComponent));
  if (affiliation) affiliation.SetAffiliatedFactionByKey(base.m_sOwner);
 }

 protected void UpdateGarrison(ACR_Manager manager, ACR_Base base, bool enemyNear, bool enemyInRange)
 {
  if (base.m_GarrisonGroup)
  {
   ACR_Squad.KeepSimulated(base.m_GarrisonGroup);
   base.m_iGarrisonLeft = CountAlive(base.m_GarrisonGroup);
   if (!enemyInRange && !base.m_bContested) DespawnGarrison(base, true);
   return;
  }
  if (enemyNear && base.m_iGarrisonLeft > 0 && !base.m_sOwner.IsEmpty()) SpawnGarrison(base);
 }

 protected int CountAlive(SCR_AIGroup group)
 {
  int alive;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (character && !character.GetCharacterController().IsDead()) alive++;
  }
  return alive;
 }

 protected void SpawnGarrison(ACR_Base base)
 {
  array<ResourceName> pool = GetFaction(base.m_sOwner).m_aGarrison;
  if (pool.IsEmpty())
  {
   PrintFormat("[ACR] No garrison soldiers for faction %1", base.m_sOwner);
   base.m_iGarrisonLeft = 0;
   return;
  }
  vector center = base.GetOrigin();
  // The faction group prefab would otherwise spawn its own six default members on top of ours.
  SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(GROUP_BASE, center));
  SCR_AIGroup.IgnoreSpawning(false);
  if (group) group.SetMaxUnitsToSpawn(0);
  if (!group) return;
  m_GarrisonGroups.Insert(group); // before any soldier joins, so it is never registered as a squad
  group.SetDeleteWhenEmpty(false); // we delete it ourselves; otherwise it vanishes mid-fight and respawns
  group.SetCustomName(base.m_sName + " garrison", 0);
  for (int i = 0; i < base.m_iGarrisonLeft; i++)
   AddGarrisonSoldier(base, group, pool);
  ACR_Squad.KeepSimulated(group);
  base.m_GarrisonGroup = group;
  // The base's buildings get posts at their windows; the soldiers start on them (the
  // game's fast init) and the rest hold the base round its centre as before.
  IEntity navAgent = ACR_Garrison.AnyAgent();
  int posts = ACR_Garrison.AssignArea(group, navAgent, center, GarrisonArea(base), base.m_iGarrisonLeft);
  ResourceName order = ACR_Squad.WP_DEFEND;
  if (posts > 0) order = ACR_Squad.WP_GARRISON;
  AIWaypoint defend = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(Resource.Load(order), null, SpawnParams(center)));
  SCR_DefendWaypoint garrison = SCR_DefendWaypoint.Cast(defend);
  if (garrison && posts > 0) garrison.SetFastInit(true);
  if (defend)
  {
   defend.SetCompletionRadius(base.m_fRadius);
   group.AddWaypoint(defend);
  }
  PrintFormat("[ACR] Garrison deployed at %1: %2 soldiers, %3 in group", base.m_sName, base.m_iGarrisonLeft, group.GetAgentsCount());
 }

 protected EntitySpawnParams SpawnParams(vector position)
 {
  EntitySpawnParams params = new EntitySpawnParams();
  params.TransformMode = ETransformMode.WORLD;
  params.Transform[3] = position;
  return params;
 }

 // keepSurvivors: pack up (survivors return to the reserve) vs wiped on capture.
 protected void DespawnGarrison(ACR_Base base, bool keepSurvivors)
 {
  SCR_AIGroup group = base.m_GarrisonGroup;
  if (!group) return;
  if (keepSurvivors) base.m_iGarrisonLeft = CountAlive(group);
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   // Never delete a soldier a player is controlling.
   if (soldier && !ACR_Squad.FindController(soldier)) SCR_EntityHelper.DeleteEntityAndChildren(soldier);
  }
  m_GarrisonGroups.RemoveItem(group);
  SCR_EntityHelper.DeleteEntityAndChildren(group);
  base.m_GarrisonGroup = null;
  PrintFormat("[ACR] Garrison packed up at %1: %2 in reserve", base.m_sName, base.m_iGarrisonLeft);
 }

 // ================================================================ defenders (ACR_Defenders)

 // Defenders of a built position outside any base: count soldiers from the faction's garrison
 // pool round center, defending it within radius. Like a garrison, never a squad.
 SCR_AIGroup SpawnGuards(string faction, vector center, int count, string name, float radius)
 {
  ACR_FactionData data = GetFaction(faction);
  if (!data || data.m_aGarrison.IsEmpty() || count <= 0) return null;
  // The group prefab would otherwise spawn its own default members on top of ours.
  SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(GROUP_BASE, center));
  SCR_AIGroup.IgnoreSpawning(false);
  if (!group) return null;
  group.SetMaxUnitsToSpawn(0);
  m_GarrisonGroups.Insert(group); // before any soldier joins, so it is never registered as a squad
  group.SetDeleteWhenEmpty(false);
  group.SetCustomName(name, 0);
  IEntity navAgent = ACR_Garrison.AnyAgent();
  for (int i = 0; i < count; i++)
  {
   vector spot = SoldierSpot(center, 8, navAgent);
   IEntity soldier = ACR_Manager.SpawnPrefab(data.m_aGarrison.GetRandomElement(), spot);
   if (!soldier) continue;
   if (!group.AddAIEntityToGroup(soldier))
   {
    SCR_EntityHelper.DeleteEntityAndChildren(soldier);
    continue;
   }
   FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(soldier.FindComponent(FactionAffiliationComponent));
   if (affiliation) affiliation.SetAffiliatedFactionByKey(faction);
  }
  ACR_Squad.KeepSimulated(group);
  AIWaypoint defend = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(Resource.Load(ACR_Squad.WP_DEFEND), null, SpawnParams(center)));
  if (defend)
  {
   defend.SetCompletionRadius(radius);
   group.AddWaypoint(defend);
  }
  return group;
 }

 int GuardsAlive(SCR_AIGroup group)
 {
  if (!group) return 0;
  return CountAlive(group);
 }

 // Packs defenders up (never one a player controls); returns how many were alive.
 int PackUpGuards(SCR_AIGroup group)
 {
  if (!group) return 0;
  int alive = CountAlive(group);
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (soldier && !ACR_Squad.FindController(soldier)) SCR_EntityHelper.DeleteEntityAndChildren(soldier);
  }
  m_GarrisonGroups.RemoveItem(group);
  SCR_EntityHelper.DeleteEntityAndChildren(group);
  return alive;
 }

 // ================================================================ FOB patrols

 // A FOB with barracks keeps 2 (small) or 4 (large) of its garrison walking the perimeter
 // (Jacob, 20 Oct); the rest stay in reserve until enemies come near, as at every base.
 // Patrol soldiers leave the reserve while out and return to it when the patrol stands down.
 static const float PATROL_RING = 45;
 static const int PATROL_POINTS = 6;
 static const float PATROL_REFILL_MS = 60000; // one replacement from the reserve this often
 static bool s_bNoFobPatrols;                  // tests whose fixtures need an empty FOB

 static int PatrolSize(ACR_Base base)
 {
  // FIA pockets at military sites walk a small patrol (ACR_Pockets).
  if (base && base.m_bPocket && !base.m_bRetired)
  {
   if (base.m_iValue >= 3) return 2;
   return 0;
  }
  if (s_bNoFobPatrols || !base || !base.FobOnline() || base.m_sOwner.IsEmpty() || base.m_iHousingGarrison <= 0) return 0;
  if (base.m_iHousingGarrison >= 19) return 4;
  return 2;
 }

 void UpdatePatrol(ACR_Base base)
 {
  int size = PatrolSize(base);
  SCR_AIGroup group = base.m_PatrolGroup;
  if (group)
  {
   int alive = CountAlive(group);
   if (size == 0 || alive > size) { DespawnPatrol(base, true); return; }
   ACR_Squad.KeepSimulated(group);
   float now = GetGame().GetWorld().GetWorldTime();
   // Losses are replaced one at a time from the reserve, never mid-fight.
   if (alive < size && base.m_iGarrisonLeft > 0 && !base.m_GarrisonGroup && now >= base.m_fPatrolRefill)
   {
    array<ResourceName> pool = GetFaction(base.m_sOwner).m_aGarrison;
    if (!pool.IsEmpty())
    {
     AddGarrisonSoldier(base, group, pool);
     base.m_iGarrisonLeft--;
    }
    base.m_fPatrolRefill = now + PATROL_REFILL_MS;
   }
   if (!group.GetCurrentWaypoint()) NextPatrolLeg(base, group);
   return;
  }
  if (size == 0 || base.m_iGarrisonLeft <= 0 || base.m_GarrisonGroup) return;
  SpawnPatrol(base, Math.Min(size, base.m_iGarrisonLeft));
 }

 protected void SpawnPatrol(ACR_Base base, int count)
 {
  array<ResourceName> pool = GetFaction(base.m_sOwner).m_aGarrison;
  if (pool.IsEmpty()) return;
  SCR_AIGroup.IgnoreSpawning(true);
  SCR_AIGroup group = SCR_AIGroup.Cast(ACR_Manager.SpawnPrefab(GROUP_BASE, base.GetOrigin()));
  SCR_AIGroup.IgnoreSpawning(false);
  if (!group) return;
  group.SetMaxUnitsToSpawn(0);
  m_GarrisonGroups.Insert(group); // before any soldier joins, so it is never registered as a squad
  group.SetDeleteWhenEmpty(false);
  group.SetCustomName(base.m_sName + " patrol", 0);
  for (int i = 0; i < count; i++) AddGarrisonSoldier(base, group, pool);
  base.m_iGarrisonLeft -= count;
  ACR_Squad.KeepSimulated(group);
  base.m_PatrolGroup = group;
  base.m_fPatrolRefill = GetGame().GetWorld().GetWorldTime() + PATROL_REFILL_MS;
  NextPatrolLeg(base, group);
  PrintFormat("[ACR] Patrol out at %1: %2 soldiers, %3 in reserve", base.m_sName, count, base.m_iGarrisonLeft);
 }

 // Round the FOB, one point at a time (PATROL_POINTS on a PATROL_RING circle).
 protected void NextPatrolLeg(ACR_Base base, SCR_AIGroup group)
 {
  base.m_iPatrolLeg = (base.m_iPatrolLeg + 1) % PATROL_POINTS;
  float rad = 360.0 / PATROL_POINTS * base.m_iPatrolLeg * Math.DEG2RAD;
  vector point = ACR_Manager.Ground(base.GetOrigin() + Vector(Math.Sin(rad) * PATROL_RING, 0, Math.Cos(rad) * PATROL_RING));
  AIWaypoint leg = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(Resource.Load(ACR_Squad.WP_MOVE), null, SpawnParams(point)));
  if (!leg) return;
  leg.SetCompletionRadius(8);
  group.AddWaypoint(leg);
 }

 // keepSurvivors: they return to the reserve (stand down) vs lost with the base (capture).
 void DespawnPatrol(ACR_Base base, bool keepSurvivors)
 {
  SCR_AIGroup group = base.m_PatrolGroup;
  if (!group) return;
  if (keepSurvivors) base.m_iGarrisonLeft = Math.Min(base.m_iGarrison, base.m_iGarrisonLeft + CountAlive(group));
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity soldier = agent.GetControlledEntity();
   if (soldier && !ACR_Squad.FindController(soldier)) SCR_EntityHelper.DeleteEntityAndChildren(soldier);
  }
  m_GarrisonGroups.RemoveItem(group);
  SCR_EntityHelper.DeleteEntityAndChildren(group);
  base.m_PatrolGroup = null;
  PrintFormat("[ACR] Patrol stood down at %1: %2 in reserve", base.m_sName, base.m_iGarrisonLeft);
 }

 // One line for the command panel.
 // viewer: the faction reading it (an unfound FIA pocket reads as unclaimed); "" = the truth.
 string Describe(ACR_Base base, string viewer = "")
 {
  string owner = base.m_sOwner;
  int garrison = base.m_iGarrisonLeft;
  if (viewer != "" && owner != "" && base.OwnerSeenBy(viewer) == "")
  {
   owner = "";
   garrison = 0;
  }
  if (owner.IsEmpty()) owner = "Unclaimed";
  string text = string.Format("%1 | garrison %2", owner, garrison);
  if (ACR_LogisticsRules.CanDeploy(base)) text += " | deploy new units";
  if (ACR_LogisticsRules.CanReinforce(base)) text += " | infantry replacements";
  if (base.m_bBuilt && base.m_bHqLost) text += " | HQ DESTROYED: FOB offline until rebuilt";
  else if (ACR_LogisticsRules.RelayBlocked(base)) text += " | RADIO RELAY REQUIRED: deployment / RP reinforcement offline";
  if (base.m_bIncome) text += string.Format(" | +%1 RP", base.m_iValue);
  else text += " | no income";
  if (base.m_bContested) text += " | CONTESTED";
  if (base.m_bHidden) text += " | Hidden";
  if (base.m_sOwner.IsEmpty() && !base.m_sCapturer.IsEmpty())
   text += string.Format(" | %1 capturing %2/100", base.m_sCapturer, Math.Round(base.m_fScore));
  else if (!base.m_sOwner.IsEmpty() && base.m_fScore < 100)
   text += string.Format(" | held %1/100", Math.Round((base.m_fScore + 100) / 2));
  return text;
 }
}
