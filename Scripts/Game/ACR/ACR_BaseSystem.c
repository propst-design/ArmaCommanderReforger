// Server-side bases, capture, garrisons, hidden-base discovery and requisition
// income. Numbers follow the Arma 3 mod (defines/commonDefines.inc, economy.sqf).

// Every man the game itself makes for a group (SpawnGroupMember: bought squads, starting units,
// riders, and any group the engine wakes from dormancy) goes on the navmesh point nearest his
// formation slot within 10 m, up and down too: beside a house, a sealed attic, a cellar lifted to
// the terrain or the hollow of a castle wall (Jacob, 6 Nov; srv:voidspawn: 40 of 64 men on houses).
// The game fills a group from a queue, one man a tick, often after SettleSoon's two looks, so each
// man is looked at the moment he is made (ACR_BaseSystem.SettleMan), before anyone sees him.
modded class SCR_AIGroup
{
 override protected bool SpawnGroupMember(bool snapToTerrain, int index, ResourceName res, bool editMode, bool isLast)
 {
  if (editMode || !ACR_Manager.GetInstance() || ACR_BaseSystem.s_bTestNoSettle) return super.SpawnGroupMember(snapToTerrain, index, res, editMode, isLast);
  array<AIAgent> before = {};
  GetAgents(before);
  bool spawned = super.SpawnGroupMember(snapToTerrain, index, res, editMode, isLast);
  array<AIAgent> after = {};
  GetAgents(after);
  foreach (AIAgent agent : after) if (!before.Contains(agent)) ACR_BaseSystem.SettleMan(this, agent);
  return spawned;
 }
}

class ACR_FactionData
{
 int m_iRP;
 ref array<ref ACR_RequisitionEntry> m_aRequisitions = {};
 ref array<ResourceName> m_aGarrison = {};
 ref array<ref ACR_SupportEntry> m_aSupports = {};
}

// The survivors of a base's garrison or patrol after it fell (ACR_BaseSystem.FallBack).
class ACR_FallBack : Managed
{
 SCR_AIGroup m_Group;
 string m_sFaction;
 string m_sName;
 ACR_Base m_From;  // the base they lost: they don't count towards taking it back
 ACR_Base m_Dest;  // their side's base they make for, or null: a point FALLBACK_AWAY from the enemy
}

class ACR_BaseSystem : Managed
{
 // Soldiers of 'key' at this base neither capture nor contest it: FIA's ally at an FIA pocket
 // (ACR_Insurgency), or either living side while a TvTvZ truce holds (ACR_Truce, #107).
 static bool FriendOfOwner(ACR_Base base, string key)
 {
  if (key == base.m_sOwner || base.m_sOwner.IsEmpty()) return false;
  if (!base.m_bPocket && !ACR_Truce.Between(key, base.m_sOwner)) return false;
  return ACR_Insurgency.Friendly(key, base.m_sOwner);
 }

 static const float CAPTURE_RATE = 0.4;     // score per second per soldier of difference
 static const float CAPTURE_MAX_STEP = 6;   // cap per second (original mod: 2; raised so more troops cap faster)
 static const float SPAWN_RANGE = 400;      // enemies this close deploy the garrison
 static const float DESPAWN_RANGE = 450;    // no enemies this close: garrison packs up
 static const float WORKS_SPAWN_RANGE = 600; // ... a base with works standing: the AI stages its attack about 320 m out, so its
                                            // garrison is behind the bags before the assault steps off (the user, 2 Nov, Chotain Base)
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
 protected ref array<ACR_Base> m_BodyFled = {}; // the base a body's group is falling back from (FallBack), else null
 protected ref array<bool> m_BodyInWar = {}; // a body of the war's own factions (InWar)

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
  // Deck builder (#106): the side's deck decides which of them it can buy.
  ACR_Decks.Apply(factionKey, settings, data.m_aRequisitions);
  ACR_FactionCatalog.BuildGarrisonPool(factionKey, settings, data.m_aGarrison);
  // Only prefabs that load: an empty row, a typo or a mod that isn't loaded spawned nothing, and
  // the next tick counted the missing men as lost for good (audit 31 Oct).
  for (int g = data.m_aGarrison.Count() - 1; g >= 0; g--)
  {
   ResourceName soldier = data.m_aGarrison[g];
   if (!soldier.IsEmpty() && Resource.Load(soldier).IsValid()) continue;
   PrintFormat("[ACR] Garrison soldier prefab for %1 doesn't load, skipped: '%2'", factionKey, soldier);
   data.m_aGarrison.Remove(g);
  }
  if (!settings || settings.m_bSupports)
  {
   bool listsAttack = false;
   if (settings && settings.m_aSupports && !settings.m_aSupports.IsEmpty())
   {
    bool listsAirLift = false;
    bool listsRecon = false;
    foreach (ACR_SupportEntry configured : settings.m_aSupports)
    {
     if (configured && configured.m_eType == ACR_ESupportType.AIR_LIFT) listsAirLift = true;
     if (configured && configured.m_eType == ACR_ESupportType.ATTACK_HELI) listsAttack = true;
     if (configured && configured.m_eType == ACR_ESupportType.RECON_FLIGHT) listsRecon = true;
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
    // ...and the recon flight (4 Nov), the same way.
    if (!listsRecon) data.m_aSupports.Insert(ACR_Supports.NewEntry(ACR_ESupportType.RECON_FLIGHT));
   }
   else ACR_Supports.BuildDefaults(data.m_aSupports, factionKey);
   // A content pack's attack helicopter for the side adds its support.
   ACR_Supports.FitAttackHelicopter(factionKey, data.m_aSupports, listsAttack);
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

 // Gives back what Spend took, in full: the cap limits income, not a refund.
 void Refund(string factionKey, int amount)
 {
  ACR_FactionData data = GetFaction(factionKey);
  if (!data) return;
  data.m_iRP += amount;
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
   // Only the two sides at war earn (broad review, refactor 10): CIV, Outbreak's Z, FIA and modded
   // factions got a full record (requisitions built) and RP nobody spends.
   if (!ACR_FactionSelection.IsSelected(key)) continue;
   int added = Math.Round(GetIncome(key) * ACR_Manager.IncomeOption());
   ACR_FactionData data = GetFaction(key);
   int before = data.m_iRP;
   // Income stops at the cap but never cuts a side above it (starting RP over 1000).
   if (data.m_iRP < POINTS_CAP) data.m_iRP = Math.Min(data.m_iRP + added, POINTS_CAP);
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
  TickFallBacks(manager);
  TickIncome(manager);
 }

 // Every living, conscious character with a faction: AI agents plus player bodies. The
 // unconscious don't hold, contest or spot (Jacob 19 Oct: nobody comes to finish them off),
 // nor do BBB prisoners (unarmed, kneeling, out of their squads).
 protected void CollectBodies(ACR_Manager manager)
 {
  m_Bodies.Clear();
  m_BodyFactions.Clear();
  m_BodyFled.Clear();
  m_BodyInWar.Clear();
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
  // A BBB prisoner (BBB 0.1.11+) kneels unarmed with the engine's disarmed flag, which vanilla
  // otherwise sets only for the downed (skipped above). Read through vanilla, not BBB_Morale.IsPrisoner,
  // so ACA builds against BBB pins with or without surrender.
  PerceivableComponent perceivable = PerceivableComponent.Cast(character.FindComponent(PerceivableComponent));
  if (perceivable && perceivable.IsDisarmed()) return;
  // Stationed support helicopter crews are not capture troops.
  ACR_Base fled;
  AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
  if (control && control.GetControlAIAgent())
  {
   SCR_AIGroup group = SCR_AIGroup.Cast(control.GetControlAIAgent().GetParentGroup());
   if (group && manager.m_Supports && manager.m_Supports.IsSupportGroup(group)) return;
   if (group) fled = FledFrom(group);
  }
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
  if (!affiliation || !affiliation.GetAffiliatedFaction()) return;
  m_Bodies.Insert(character);
  string key = affiliation.GetAffiliatedFaction().GetFactionKey();
  m_BodyFactions.Insert(key);
  m_BodyFled.Insert(fled);
  m_BodyInWar.Insert(InWar(key));
 }

 // The war's own factions: the two sides and FIA. Only they (and a base's owner) hold, contest,
 // take or find a base. Another mod's faction doesn't: Project Outbreak's zombies are a faction
 // hostile to everyone that roams and nests everywhere, so they kept bases "under attack" for good,
 // could even take them, and every spawn point of a side closed (6 Nov, Discord: a commander killed
 // in his command unit sat on a deploy screen with nothing to pick). They still turn a garrison out.
 static bool InWar(string key)
 {
  return ACR_FactionSelection.IsSelected(key) || key == ACR_Pockets.FACTION;
 }

 protected void UpdateBase(ACR_Manager manager, ACR_Base base)
 {
  vector center = base.GetOrigin();
  map<string, int> inside = new map<string, int>();
  int zombies; // inside: they block the base, never hold, take or find it (ACR_Outbreak)
  bool enemyNear, enemyInRange, enemyApproaching;
  float spawnRange = SPAWN_RANGE;
  float despawnRange = DESPAWN_RANGE;
  if (ACR_BaseWorks.LinesStand(base))
  {
   spawnRange = WORKS_SPAWN_RANGE;
   despawnRange = WORKS_SPAWN_RANGE + DESPAWN_RANGE - SPAWN_RANGE;
  }
  foreach (int i, IEntity body : m_Bodies)
  {
   if (!body) continue; // deleted this tick (e.g. a garrison packed up after a capture)
   string key = m_BodyFactions[i];
   float distance = vector.DistanceXZ(body.GetOrigin(), center);
   bool friendOfOwner = FriendOfOwner(base, key);
   bool inWar = m_BodyInWar[i] || key == base.m_sOwner;
   bool zombie = ACR_Outbreak.IsZombies(key);
   if (zombie && distance <= base.m_fRadius) zombies++;
   // Survivors leaving a base they lost don't contest it (FallBack); at another base they count.
   if (distance <= base.m_fRadius && !friendOfOwner && inWar && m_BodyFled[i] != base)
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
    if (base.m_bPocket && inWar && distance <= SPAWN_RANGE && !base.m_aDetectedBy.Contains(key) && !ACR_Guerrillas.Hidden())
    {
     base.m_aDetectedBy.Insert(key);
     manager.NotifyFaction(key, string.Format("FIA resistance at %1", base.m_sName));
     PrintFormat("[ACR] POCKETS %1 found by %2", base.m_sName, key);
    }
    // Only an enemy at war (or the dead) turns the garrison out: civilians and evacuees passing
    // through kept towns' garrisons deployed all match (broad review #16).
    if (inWar || zombie)
    {
     if (distance <= spawnRange) enemyNear = true;
     if (distance <= spawnRange + 300) enemyApproaching = true;
     if (distance <= despawnRange) enemyInRange = true;
    }
   }
  }
  UpdateCapture(manager, base, inside, zombies);
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

 // Zombies inside ('zombies', ACR_Outbreak) block a base: a held one is under attack and can't
 // secure itself, an open one's capture stops; they never take it.
 protected void UpdateCapture(ACR_Manager manager, ACR_Base base, map<string, int> inside, int zombies)
 {
  int attackers;
  if (!base.m_sOwner.IsEmpty())
  {
   int defenders;
   inside.Find(base.m_sOwner, defenders);
   string attacker = Strongest(inside, base.m_sOwner, attackers);
   bool wasContested = base.m_bContested;
   base.m_bContested = attackers > 0 || zombies > 0;
   if (base.m_bContested && !wasContested)
    manager.NotifyFaction(base.m_sOwner, string.Format("%1 is under attack", base.m_sName));
   float change = 1; // secure again over time when no attackers are inside
   if (attackers > 0) change = Math.Clamp((defenders - attackers) * CAPTURE_RATE, -CAPTURE_MAX_STEP, CAPTURE_MAX_STEP);
   else if (zombies > 0) change = 0;
   base.m_fScore = Math.Clamp(base.m_fScore + change, -100, 100);
   if (base.m_fScore <= -100) Capture(manager, base, attacker);
   else if (ACR_TvZ.Overrun(base, zombies, defenders + attackers)) Overrun(manager, base, zombies);
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
  base.m_bContested = holders > 0 && (attackers > 0 || zombies > 0);
  float step = -1;
  if (holders > 0 || attackers > 0) step = Math.Clamp((holders - attackers) * CAPTURE_RATE, -CAPTURE_MAX_STEP, CAPTURE_MAX_STEP);
  if (zombies > 0 && step > 0) step = 0;
  base.m_fScore = Math.Clamp(base.m_fScore + step, 0, 100);
  if (base.m_fScore <= 0) base.m_sCapturer = "";
  if (base.m_fScore >= 100 && !base.m_sCapturer.IsEmpty()) Capture(manager, base, base.m_sCapturer);
 }

 // Debug menu: hand a base over at once.
 void DebugCapture(ACR_Manager manager, ACR_Base base, string factionKey) { Capture(manager, base, factionKey); }

 protected void Capture(ACR_Manager manager, ACR_Base base, string factionKey)
 {
  string previous = base.m_sOwner;
  // Survivors fall back instead of vanishing (Jacob, 6 Nov: an FIA team was simply gone once its town fell).
  SCR_AIGroup garrison = base.m_GarrisonGroup;
  SCR_AIGroup patrol = base.m_PatrolGroup;
  base.m_GarrisonGroup = null;
  base.m_PatrolGroup = null;
  FallBack(base, garrison, previous, "garrison");
  FallBack(base, patrol, previous, "patrol");
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
  // Zombies near a town just taken come for it (ACR_Outbreak.CounterAttack).
  ACR_Outbreak.CounterAttack(base.GetOrigin(), base.m_sName, base, factionKey);
 }

 // TvZ (ACR_TvZ): zombies alone inside a held base long enough take it back. It goes open (no
 // garrison left: they were killed), and the zombies that took it settle in as a nest.
 protected void Overrun(ACR_Manager manager, ACR_Base base, int zombies)
 {
  string previous = base.m_sOwner;
  DespawnGarrison(base, false);
  DespawnPatrol(base, false);
  base.m_sOwner = "";
  base.m_sCapturer = "";
  base.m_fScore = 0;
  base.m_bContested = false;
  base.m_iGarrisonLeft = 0;
  if (manager.m_Structures) manager.m_Structures.UpdateHousing(base);
  PrintFormat("[ACR] Base overrun: %1 by zombies (was %2)", base.m_sName, previous);
  ACR_Report.OnCapture(base.m_sName, "", previous);
  manager.NotifyFaction(previous, string.Format("%1 overrun by zombies", base.m_sName));
  ACR_TvZ.Fallen(base, zombies);
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
 // wide, 2 m tall cylinder), not under a roof, on the soldiers' navmesh, and able to walk out from
 // there (ReachesOpen). Before #73, a soldier whose random spot had no room within 5 m spawned there
 // anyway, and a spot with room inside a house was dropped to the terrain under its floor: FIA in
 // walls, every village. Each try reaches further out, the last SPOT_WIDE_TRIES well beyond the
 // spread. With none good: clear open ground nearest the centre, else the centre's ground. Never the
 // navmesh point nearest the centre, as before 6 Nov: in a castle or a crowded town that was a sealed
 // attic, a cellar or the hollow inside of a thick wall (Jacob, Chernarus: Zub, Devil's Castle).
 // Where the navmesh isn't loaded (no AI near on a server) room and roof alone decide.
 // Soldiers with window posts are moved onto them afterwards (ACR_Garrison), as before.
 static const int SPOT_TRIES = 10;
 static const int SPOT_WIDE_TRIES = 10;
 static int s_iSpotSealed;     // tests: spots refused because they lead nowhere
 static int s_iSpotLastResort; // tests: calls that found no good spot at all
 static vector SoldierSpot(vector center, float spread, IEntity navAgent)
 {
  AIPathfindingComponent pathing = Pathing(navAgent);
  NavmeshWorldComponent mesh;
  if (pathing) mesh = pathing.GetNavmeshComponent();
  if (mesh && !mesh.IsTileLoaded(center)) mesh.LoadTileIn(center);
  vector onMesh;
  for (int attempt = 0; attempt < SPOT_TRIES + SPOT_WIDE_TRIES; attempt++)
  {
   float reach = spread + attempt * 4;
   if (attempt >= SPOT_TRIES) reach = spread + 40 + (attempt - SPOT_TRIES) * 8;
   vector candidate = ACR_Manager.Ground(center + Vector(Math.RandomFloat(-reach, reach), 0, Math.RandomFloat(-reach, reach)));
   vector free;
   if (!SCR_WorldTools.FindEmptyTerrainPosition(free, candidate, 6, 0.6, 2)) continue;
   free = ACR_Manager.Ground(free);
   // The finder's spot may sit on something; on the terrain it must still have room.
   if (!RoomAt(free) || Covered(free)) continue;
   if (!pathing || (mesh && !mesh.IsTileLoaded(free))) return free;
   if (!pathing.GetClosestPositionOnNavmesh(free, Vector(1, 1.5, 1), onMesh)) continue;
   if (vector.DistanceXZ(onMesh, free) > 1) continue;
   if (!ReachesOpen(onMesh, pathing)) { s_iSpotSealed++; continue; }
   return free;
  }
  s_iSpotLastResort++;
  vector near = ACR_Manager.Ground(center);
  array<vector> spots = {};
  SCR_WorldTools.FindAllEmptyTerrainPositions(spots, near, 60, 0.6, 2, 24);
  foreach (vector spot : spots)
  {
   vector ground = ACR_Manager.Ground(spot);
   if (RoomAt(ground) && !Covered(ground)) return ground;
  }
  PrintFormat("[ACR] SPAWN no open ground near %1: a soldier stands at its centre", near);
  return near;
 }

 // Where a soldier made at 'at' stands: right there if he has room, isn't under a roof or in a
 // heap and can walk out (ReachesOpen); else the nearest such spot (SoldierSpot).
 static vector OpenSpot(vector at, IEntity navAgent = null)
 {
  if (RoomAt(at) && !Covered(at) && ReachesOpen(at, Pathing(navAgent))) return at;
  return SoldierSpot(at, 2, navAgent);
 }

 // The soldiers' pathfinding (the navmesh they walk): navAgent's, else any squad's.
 static AIPathfindingComponent Pathing(IEntity navAgent)
 {
  if (!navAgent) navAgent = ACR_Garrison.AnyAgent();
  if (!navAgent) return null;
  return AIPathfindingComponent.Cast(navAgent.FindComponent(AIPathfindingComponent));
 }

 // A soldier here can walk out onto open ground (Jacob, 6 Nov: men spawned in sealed attics and
 // cellars and inside thick castle walls). The navmesh is built in those spaces too, so "on the
 // navmesh" never told them apart. The engine hands out a point reachable from a given one
 // (GetReachablePoint): a short random hop, about 1-15 m (probe, Everon). Hopping on from the
 // farthest point found at ground level, a spot that connects to the outside soon lands OPEN_REACH
 // m away on uncovered ground (OPEN_GROUND m above the terrain at most: not a roof or a floor); one
 // in a sealed space never does. Three hops in a row that get no further restart from another point
 // found (the walk can stall on a step or a porch: Lancre, probe). OPEN_HOPS queries at most:
 // open ground takes 3-6 (srv:voidspawn).
 // Nothing to judge (no navmesh, its tile not loaded, no navmesh within a metre, a query the engine
 // can't answer: the walk reached a tile not loaded): true.
 static int OPEN_HOPS = 48; // (tests may raise it)
 static const float OPEN_REACH = 10;
 static const float OPEN_STEP = 20;
 static const float OPEN_GROUND = 2;
 static int s_iOpenHops; // queries made, all calls (tests, perf)
 static bool s_bOpenTrace; // tests: print every hop
 static string s_sOpenWhy; // tests: how the last walk ended
 static bool ReachesOpen(vector spot, AIPathfindingComponent pathing)
 {
  if (!pathing) return true;
  NavmeshWorldComponent mesh = pathing.GetNavmeshComponent();
  if (!mesh || !mesh.IsTileLoaded(spot)) return true;
  vector start;
  s_sOpenWhy = "not judged";
  if (!pathing.GetClosestPositionOnNavmesh(spot, Vector(1, 1.5, 1), start)) return true;
  BaseWorld world = GetGame().GetWorld();
  array<vector> found = {start};
  vector from = start;
  float farthest = 0;
  int stalled;
  for (int hop = 0; hop < OPEN_HOPS; hop++)
  {
   vector next = start;
   s_iOpenHops++;
   bool got = mesh.GetReachablePoint(from, OPEN_STEP, next);
   float away = vector.DistanceXZ(next, start);
   float above = next[1] - world.GetSurfaceY(next[0], next[2]);
   if (s_bOpenTrace) PrintFormat("[ACR] OPENHOP %1 from %2 got %3 -> %4 away %5 above ground %6", hop, from, got, next, away, above);
   if (!got) { s_sOpenWhy = "no answer"; return true; } // the walk reached a tile not loaded, or the engine had no answer: nothing to judge
   if (above > OPEN_GROUND || away <= farthest)
   {
    stalled++;
    if (stalled >= 3 && found.Count() > 1)
    {
     from = found.GetRandomElement();
     stalled = 0;
    }
    continue;
   }
   stalled = 0;
   farthest = away;
   from = next;
   found.Insert(next);
   if (away >= OPEN_REACH && !Covered(next)) { s_sOpenWhy = "open"; return true; }
  }
  s_sOpenWhy = string.Format("shut in (farthest %1 m)", farthest);
  return false;
 }

 // Room for a soldier standing right here (only the spot itself is tried): nothing solid where
 // his body would be, other soldiers included ('ignore': the one standing there now).
 // The game's finder (FindEmptyTerrainPosition) tests four diagonal lines through its cylinder,
 // which miss a wall crossing it off-centre below head height, and a line starting inside a thick
 // wall or a rock hits nothing at all: FIA went on spawning in walls after #73 (Jacob, 3 Nov).
 // A box overlap catches both; the box starts above the ankles so a kerb or a step doesn't count.
 static bool RoomAt(vector spot, IEntity ignore = null)
 {
  // The finder can't be told to skip the man standing here: for him, the box alone.
  vector free;
  if (!ignore && (!SCR_WorldTools.FindEmptyTerrainPosition(free, spot, 0.7, 0.6, 2) || vector.DistanceXZ(free, spot) >= 0.1)) return false;
  TraceBox body = new TraceBox();
  body.Mins = Vector(-0.3, 0.35, -0.3);
  body.Maxs = Vector(0.3, 1.75, 0.3);
  body.Start = spot;
  body.Flags = TraceFlags.ENTS;
  array<IEntity> skip = {};
  if (ignore)
  {
   Hierarchy(ignore, skip); // his rifle and kit are entities of their own
   body.ExcludeArray = skip;
  }
  return GetGame().GetWorld().TracePosition(body, null) >= 0;
 }

 protected static void Hierarchy(IEntity root, notnull array<IEntity> into)
 {
  into.Insert(root);
  IEntity child = root.GetChildren();
  while (child)
  {
   Hierarchy(child, into);
   child = child.GetSibling();
  }
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

 // Clear ground near center (as ClearGround) that a vehicle can drive out of (DrivesOut).
 // anyClear/firstClear: the first clear spot, drivable or not. Stops searching once
 // s_iExitSweeps reaches sweepLimit (a cramped town costs a bounded number of sweeps).
 static bool DrivableGround(out vector result, out bool anyClear, out vector firstClear, vector center, float radius, float clearance, set<int> enclosed, int sweepLimit)
 {
  anyClear = false;
  array<vector> spots = {};
  SCR_WorldTools.FindAllEmptyTerrainPositions(spots, center, radius, clearance, 2.5, 12);
  foreach (vector spot : spots)
  {
   vector ground = ACR_Manager.Ground(spot);
   if (Buried(ground)) continue;
   if (!anyClear) { firstClear = ground; anyClear = true; }
   if (s_iExitSweeps >= sweepLimit) return false;
   if (!DrivesOut(ground, enclosed)) continue;
   result = ground;
   return true;
  }
  return false;
 }

 // A vehicle on this spot can drive away from it (Jacob, 6 Nov: the AI's vehicles came out in
 // courtyards they couldn't leave; the finders only asked for room to stand). The way out is a
 // lane EXIT_HALF_WIDTH * 2 wide, clear of what the vehicle navmesh is built round (colliders on
 // the Navmesh/NavmeshVehicle layers: buildings, walls, fences, trees, rocks; not bushes, vehicles
 // or soldiers, which give way or move), on slopes a vehicle climbs and off water, from the spot to
 // ground EXIT_REACH m from it. Best-first over a world grid of EXIT_STEP cells, farthest from the
 // spot first, each step swept only when it is taken: open ground costs about ten sweeps, an
 // enclosed yard is searched out (at most EXIT_SWEEPS) and refused.
 // enclosed: cells a failed search reached (kept a while per base): a spot in one of them is in
 // the same yard and is refused without a search. A search that ran out of sweeps marks its cells
 // too: a maze too big to call is no place for a bought vehicle either.
 static const float EXIT_STEP = 4;
 static const float EXIT_REACH = 40;
 static const float EXIT_HALF_WIDTH = 1.4;
 static const float EXIT_CLIMB = 0.6; // metres of rise per metre (31 degrees)
 static const int EXIT_SWEEPS = 160;
 static int s_iExitSweeps; // sweeps made by DrivesOut since the game started (tests, perf)
 // The last refused search (tests): cells reached, steps too steep, into water, into a wall.
 static int s_iExitReached;
 static int s_iExitSteep;
 static int s_iExitWet;
 static int s_iExitWalled;

 static bool DrivesOut(vector spot, set<int> enclosed = null)
 {
  int startX = (int)Math.Round(spot[0] / EXIT_STEP);
  int startZ = (int)Math.Round(spot[2] / EXIT_STEP);
  if (enclosed && enclosed.Contains(ExitCell(startX, startZ))) return false;
  TraceBox lane = new TraceBox();
  lane.Mins = Vector(-EXIT_HALF_WIDTH, 0.5, -EXIT_HALF_WIDTH);
  lane.Maxs = Vector(EXIT_HALF_WIDTH, 2.4, EXIT_HALF_WIDTH);
  lane.Flags = TraceFlags.ENTS;
  lane.LayerMask = EPhysicsLayerDefs.Navmesh | EPhysicsLayerDefs.NavmeshVehicle;
  BaseWorld world = GetGame().GetWorld();
  set<int> reached = new set<int>();
  reached.Insert(ExitCell(startX, startZ));
  // Steps not taken yet: the cell, where it starts and where it ends.
  array<int> stepX = {};
  array<int> stepZ = {};
  array<vector> stepFrom = {};
  array<vector> stepTo = {};
  ExitSteps(spot, startX, startZ, reached, stepX, stepZ, stepFrom, stepTo);
  int sweeps, steep, wet;
  bool escaped;
  while (!stepTo.IsEmpty() && sweeps < EXIT_SWEEPS)
  {
   int pick;
   float farthest = -1;
   foreach (int i, vector to : stepTo)
   {
    float away = vector.DistanceSqXZ(to, spot);
    if (away <= farthest) continue;
    farthest = away;
    pick = i;
   }
   int x = stepX[pick];
   int z = stepZ[pick];
   vector from = stepFrom[pick];
   vector at = stepTo[pick];
   stepX.Remove(pick);
   stepZ.Remove(pick);
   stepFrom.Remove(pick);
   stepTo.Remove(pick);
   if (reached.Contains(ExitCell(x, z))) continue;
   if (Math.AbsFloat(at[1] - from[1]) > EXIT_CLIMB * vector.DistanceXZ(from, at)) { steep++; continue; }
   if (BBB_Terrain.IsWaterAt(at)) { wet++; continue; }
   sweeps++;
   lane.Start = from;
   lane.End = at;
   if (world.TraceMove(lane, null) < 1) continue;
   reached.Insert(ExitCell(x, z));
   if (vector.DistanceXZ(at, spot) >= EXIT_REACH) { escaped = true; break; }
   ExitSteps(at, x, z, reached, stepX, stepZ, stepFrom, stepTo);
  }
  s_iExitSweeps += sweeps;
  if (escaped) return true;
  s_iExitReached = reached.Count();
  s_iExitSteep = steep;
  s_iExitWet = wet;
  s_iExitWalled = sweeps - reached.Count() + 1;
  if (enclosed) foreach (int cell : reached) enclosed.Insert(cell);
  return false;
 }

 protected static int ExitCell(int x, int z) { return x * 8192 + z; }

 protected static void ExitSteps(vector from, int x, int z, set<int> reached, array<int> stepX, array<int> stepZ, array<vector> stepFrom, array<vector> stepTo)
 {
  for (int dx = -1; dx <= 1; dx++)
  {
   for (int dz = -1; dz <= 1; dz++)
   {
    if ((dx == 0 && dz == 0) || reached.Contains(ExitCell(x + dx, z + dz))) continue;
    stepX.Insert(x + dx);
    stepZ.Insert(z + dz);
    stepFrom.Insert(from);
    stepTo.Insert(ACR_Manager.Ground(Vector((x + dx) * EXIT_STEP, 0, (z + dz) * EXIT_STEP)));
   }
  }
 }

 // #87: a squad the game spawned in its formation round one spot: anyone who came out inside a
 // wall or a heap or under a roof, or shut in where he can't walk out (ReachesOpen), steps to clear
 // ground near where he stands. The game queues its members over a few frames, so it looks twice.
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
  foreach (AIAgent agent : agents) SettleMan(group, agent);
  if (pass < 1) GetGame().GetCallqueue().CallLater(Settle, 4000, false, group, pass + 1);
 }

 // One man of the group: inside a wall or a heap, under a roof or shut in: to clear ground near him.
 static int s_iSettled; // tests
 static bool s_bTestNoSettle; // tests: the game's own placement, left alone
 static void SettleMan(SCR_AIGroup group, AIAgent agent)
 {
  if (!agent) return;
  IEntity body = agent.GetControlledEntity();
  if (!body || !ACR_Respawn.IsAlive(body) || EntityUtils.IsPlayer(body) || ACR_Transport.VehicleOf(body)) return;
  vector at = body.GetOrigin();
  if (!Covered(at) && RoomAt(at, body) && ReachesOpen(at, Pathing(agent))) return;
  vector spot = SoldierSpot(at, 3, agent);
  if (Covered(spot) || !RoomAt(spot, body)) return;
  body.SetOrigin(spot);
  s_iSettled++;
  PrintFormat("[ACR] SPAWN %1: a soldier who came out inside or shut in something stepped %2 m clear", group.GetCustomName(), (int)vector.DistanceXZ(at, spot));
 }

 // One soldier from a side's garrison pool with a content pack's swap (ACR_ContentSocket.Soldier)
 // rolled now, so a varied swap varies per soldier rather than once per match. A swap that doesn't
 // load keeps the pool's own soldier.
 static ResourceName PickGarrisonSoldier(string faction, notnull array<ResourceName> pool)
 {
  if (pool.IsEmpty()) return "";
  ResourceName picked = pool.GetRandomElement();
  ResourceName swapped = ACR_ContentSocket.Get().Soldier(faction, picked);
  if (swapped.IsEmpty() || swapped == picked) return picked;
  Resource resource = Resource.Load(swapped);
  if (!resource || !resource.IsValid()) return picked;
  return swapped;
 }

 protected void AddGarrisonSoldier(ACR_Base base, SCR_AIGroup group, array<ResourceName> pool)
 {
  float spread = Math.Min(base.m_fRadius * 0.6, 25);
  vector spot = SoldierSpot(base.GetOrigin(), spread, ACR_Garrison.AnyAgent());
  IEntity soldier = ACR_Manager.SpawnPrefab(PickGarrisonSoldier(base.m_sOwner, pool), spot);
  if (!soldier) return;
  if (!group.AddAIEntityToGroup(soldier))
  {
   SCR_EntityHelper.DeleteEntityAndChildren(soldier);
   return;
  }
  FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(soldier.FindComponent(FactionAffiliationComponent));
  if (affiliation) affiliation.SetAffiliatedFactionByKey(base.m_sOwner);
  // Garrisons get their side's and profiles' skill too (no tactics tick sets it again).
  ACR_Manager manager = ACR_Manager.GetInstance();
  SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(soldier.FindComponent(SCR_AICombatComponent));
  if (manager && combat) ACR_AISkill.Apply(combat, group, manager.m_iAISkill, manager.m_fAIPerception, base.m_sOwner);
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
  // A base with works standing: the garrison mans the lines that face the enemy before it takes to the houses
  // (the user, 2 Nov, Chotain Base: the attack came, nobody was on the bags, the garrison sat in the offices).
  ACR_BaseWork work = ACR_BaseWorks.Find(base);
  vector lineCenter;
  float lineRadius;
  int line;
  if (work && work.m_sFaction == base.m_sOwner && ACR_BaseWorks.ManSquad(work, group, base.m_iGarrisonLeft, lineCenter, lineRadius, line))
  {
   ACR_Guerrillas.Order(group, ACR_Squad.WP_GARRISON, lineCenter, lineRadius, true);
   // ... and kept there as the works' reserve is: the line it faces looked at again, a gun beside it re-manned.
   ACR_BaseWorks.WatchGarrison(work, group, line);
   PrintFormat("[ACR] Garrison deployed at %1: %2 soldiers, %3 in group, behind its bags", base.m_sName, base.m_iGarrisonLeft, group.GetAgentsCount());
   return;
  }
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

 // keepSurvivors: pack up (survivors return to the reserve); a capture hands them to FallBack instead.
 // A base that goes for good (ACR_Base.Retire): its garrison and patrol soldiers with their groups
 // (deleting a group entity leaves its members standing: broad review #22).
 void DespawnForces(ACR_Base base)
 {
  if (!base) return;
  DespawnGarrison(base, false);
  DespawnPatrol(base, false);
 }

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
 // pool round center (within spread), defending it within radius. Like a garrison, never a squad.
 // 'picks': who exactly, by index (zombie nests, ACR_Outbreak.Picks); otherwise random from the pool.
 SCR_AIGroup SpawnGuards(string faction, vector center, int count, string name, float radius, float spread = 8, array<ResourceName> picks = null)
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
   vector spot = SoldierSpot(center, spread, navAgent);
   ResourceName prefab = PickGarrisonSoldier(faction, data.m_aGarrison);
   if (picks && i < picks.Count()) prefab = picks[i];
   IEntity soldier = ACR_Manager.SpawnPrefab(prefab, spot);
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

 // ================================================================ falling back

 // A base taken: its garrison's and patrol's survivors don't vanish with it (Jacob, 6 Nov: an FIA
 // team was simply gone once the town it held fell). They leave it, returning fire, for their side's
 // nearest base within FALLBACK_REACH (else FALLBACK_AWAY m from the enemy round them) and hold it
 // if it is under attack. Once no enemy is within DESPAWN_RANGE of any of them they pack up into the
 // reserve of their side's nearest base that isn't fighting, as a garrison packs up (dispersed if
 // their side holds none). They never count towards the base they lost (no instant recapture by the
 // men just beaten out of it), do count at any other, and stay garrison groups: never squads.
 static const float FALLBACK_REACH = 2500;
 static const float FALLBACK_AWAY = 350;
 static const float FALLBACK_TICK_MS = 5000;
 protected ref array<ref ACR_FallBack> m_FallBacks = {};
 protected float m_fNextFallBack;

 protected void FallBack(ACR_Base base, SCR_AIGroup group, string faction, string kind)
 {
  if (!group) return;
  int alive = CountAlive(group);
  if (alive <= 0 || faction.IsEmpty())
  {
   PackUpGuards(group);
   return;
  }
  // Off the bags and out of the windows first: the works and the posts would hold them there.
  ACR_BaseWork work = ACR_BaseWorks.Find(base);
  if (work && work.m_GarrisonPost && work.m_GarrisonPost.m_Group == group) work.m_GarrisonPost = null;
  ACR_Garrison.Release(group);
  ACR_FallBack retreat = new ACR_FallBack();
  retreat.m_Group = group;
  retreat.m_sFaction = faction;
  retreat.m_From = base;
  retreat.m_sName = base.m_sName + " " + kind;
  group.SetCustomName(retreat.m_sName + " (falling back)", 0);
  ACR_Guerrillas.SetCombatMode(group, EAIGroupCombatMode.RETURN_FIRE);
  m_FallBacks.Insert(retreat);
  vector to = Head(retreat);
  PrintFormat("[ACR] FALLBACK %1: %2 left, %3 for %4", retreat.m_sName, alive, faction, Where(retreat, to));
 }

 // The group's way out: its side's nearest base in reach, else away from the enemy round it.
 protected vector Head(ACR_FallBack retreat)
 {
  vector from = MenAt(retreat.m_Group, retreat.m_From.GetOrigin());
  retreat.m_Dest = NearestHeld(retreat.m_sFaction, from, FALLBACK_REACH, retreat.m_From, false);
  if (retreat.m_Dest)
  {
   vector home = retreat.m_Dest.GetOrigin();
   ACR_Guerrillas.Order(retreat.m_Group, ACR_Squad.WP_MOVE, home, Math.Max(retreat.m_Dest.m_fRadius * 0.5, 20), false);
   return home;
  }
  // Away from the enemies within 300 m of them (else from the base's middle).
  vector sum = vector.Zero;
  int count;
  foreach (int i, IEntity body : m_Bodies)
  {
   if (!body || ACR_Insurgency.Friendly(m_BodyFactions[i], retreat.m_sFaction)) continue;
   if (vector.DistanceXZ(body.GetOrigin(), from) > 300) continue;
   sum += body.GetOrigin();
   count++;
  }
  vector away = from - retreat.m_From.GetOrigin();
  if (count > 0) away = from - sum * (1.0 / count);
  away[1] = 0;
  if (away.LengthSq() < 1) away = Vector(Math.RandomFloat(-1, 1), 0, Math.RandomFloat(-1, 1));
  away.Normalize();
  vector point = ACR_Manager.Ground(from + away * FALLBACK_AWAY);
  // Not into the sea or a lake: a quarter turn at a time.
  for (int turn = 0; turn < 3 && BBB_Terrain.IsWaterAt(point); turn++)
  {
   away = Vector(away[2], 0, -away[0]);
   point = ACR_Manager.Ground(from + away * FALLBACK_AWAY);
  }
  ACR_Guerrillas.Order(retreat.m_Group, ACR_Squad.WP_MOVE, point, 20, false);
  return point;
 }

 protected string Where(ACR_FallBack retreat, vector to)
 {
  if (retreat.m_Dest) return retreat.m_Dest.m_sName;
  return ACR_Manager.Grid(to);
 }

 // The first living man's position, else 'fallback'.
 protected vector MenAt(SCR_AIGroup group, vector fallback)
 {
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man && ACR_Respawn.IsAlive(man)) return man.GetOrigin();
  }
  return fallback;
 }

 // The faction's nearest standing base to 'pos' within 'reach' (not 'except'); quiet: only one
 // whose garrison isn't out (a deployed garrison counts its own men, ours would be lost).
 protected ACR_Base NearestHeld(string faction, vector pos, float reach, ACR_Base except, bool quiet)
 {
  ACR_Base best;
  float bestDistance = reach;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base == except || base.m_bRetired || base.m_sOwner != faction) continue;
   if (quiet && base.m_GarrisonGroup) continue;
   float distance = vector.DistanceXZ(pos, base.GetOrigin());
   if (distance > bestDistance) continue;
   bestDistance = distance;
   best = base;
  }
  return best;
 }

 // The base a garrison group is falling back from, if it is.
 protected ACR_Base FledFrom(SCR_AIGroup group)
 {
  foreach (ACR_FallBack retreat : m_FallBacks) if (retreat.m_Group == group) return retreat.m_From;
  return null;
 }

 // Groups falling back, now and then (FALLBACK_TICK_MS).
 protected void TickFallBacks(ACR_Manager manager)
 {
  if (m_FallBacks.IsEmpty()) return;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < m_fNextFallBack) return;
  m_fNextFallBack = now + FALLBACK_TICK_MS;
  for (int i = m_FallBacks.Count() - 1; i >= 0; i--)
  {
   ACR_FallBack retreat = m_FallBacks[i];
   SCR_AIGroup group = retreat.m_Group;
   int alive;
   if (group) alive = CountAlive(group);
   if (alive <= 0)
   {
    if (group) PackUpGuards(group);
    m_FallBacks.Remove(i);
    PrintFormat("[ACR] FALLBACK %1: none left", retreat.m_sName);
    continue;
   }
   ACR_Squad.KeepSimulated(group);
   // The base they made for fell too: the next one.
   if (retreat.m_Dest && (retreat.m_Dest.m_bRetired || retreat.m_Dest.m_sOwner != retreat.m_sFaction))
   {
    vector to = Head(retreat);
    PrintFormat("[ACR] FALLBACK %1: now for %2", retreat.m_sName, Where(retreat, to));
   }
   if (StillFighting(manager, retreat))
   {
    // There and still pressed: they hold it with its garrison.
    if (retreat.m_Dest && !group.GetCurrentWaypoint())
     ACR_Guerrillas.Order(group, ACR_Squad.WP_DEFEND, retreat.m_Dest.GetOrigin(), retreat.m_Dest.m_fRadius, false);
    continue;
   }
   // Clear of the enemy: into the reserve of a base of theirs that isn't fighting (offscreen, as a garrison packs up).
   ACR_Base home = NearestHeld(retreat.m_sFaction, MenAt(group, retreat.m_From.GetOrigin()), float.MAX, null, true);
   if (!home && NearestHeld(retreat.m_sFaction, vector.Zero, float.MAX, null, false)) continue; // every base of theirs is fighting: later
   int men = PackUpGuards(group);
   m_FallBacks.Remove(i);
   if (home)
   {
    home.m_iGarrisonLeft += men;
    PrintFormat("[ACR] FALLBACK %1: %2 back in the reserve at %3 (%4 there)", retreat.m_sName, men, home.m_sName, home.m_iGarrisonLeft);
   }
   else PrintFormat("[ACR] FALLBACK %1: %2 dispersed, %3 holds no base", retreat.m_sName, men, retreat.m_sFaction);
  }
 }

 // An enemy (squad or player) within DESPAWN_RANGE of one of them.
 protected bool StillFighting(ACR_Manager manager, ACR_FallBack retreat)
 {
  array<AIAgent> agents = {};
  retreat.m_Group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   IEntity man = agent.GetControlledEntity();
   if (man && ACR_Respawn.IsAlive(man) && ACR_Defenders.EnemyWithin(manager, retreat.m_sFaction, man.GetOrigin(), DESPAWN_RANGE)) return true;
  }
  return false;
 }

 // How many are falling back (tests).
 int FallingBack(string faction)
 {
  int men;
  foreach (ACR_FallBack retreat : m_FallBacks) if (retreat.m_Group && retreat.m_sFaction == faction) men += CountAlive(retreat.m_Group);
  return men;
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
