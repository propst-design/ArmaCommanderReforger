// Defenders for built positions (Fortify tier II and III, ACR_Fortify.Defenders; checkpoints with
// -acrRoadblockDefenders, ACR_Roadblocks.Defenders): soldiers from
// the side's garrison pool held in reserve at the position, like a base's garrison
// (ACR_BaseSystem). They deploy when an enemy comes within SPAWN_RANGE and defend the position,
// pack up (survivors back to the reserve) once no enemy is within DESPAWN_RANGE, and are used up
// as they die. A position that is gone keeps the defenders who are out until they can pack up;
// its reserve goes with it. Their group is never a squad. While the squad holding a Fortify position
// is away on another job, men from the reserve man its guns (a gun crew, CrewGuns), back into the
// reserve once a squad holds it again (the user, 26 Oct: holders sent to lay mines in front left the
// nest empty).
class ACR_DefenderPost : Managed
{
 string m_sFaction;
 vector m_vPos;
 string m_sName;
 float m_fRadius;      // their Defend round the position
 int m_iReserve;       // soldiers not out (used up as they die)
 SCR_AIGroup m_Group;  // out now
 bool m_bGone;         // its position is gone
 bool m_bCrewWanted;   // its holding squad is away: its guns are manned from the reserve
 ref array<IEntity> m_aCrewGuns = {};
 SCR_AIGroup m_GunCrew; // on its guns now
 float m_fSpawnRange = 400;       // an enemy this close deploys them (ACR_Defenders.SPAWN_RANGE)
 float m_fGarrisonArea;           // > 0: they go into the buildings this close (ACR_Garrison)
 ACR_BaseWork m_Work;             // a whole base's works: its men go behind their bags (ACR_BaseWorks.ManLine)
 ACR_Fortification m_Fort;        // a Fortify position's: its men go behind its wings if it has them (ACR_Wings.ManWings)
 bool m_bOnWings;
 int m_iMannedLine = -1;
 bool m_bLinesFull;     // ManLine: the lines facing him had no place left (others' men on them all)
 float m_fMannedAt;
 ref ACR_GuerrillaCell m_Cell;    // an FIA guerrilla cell (ACR_Guerrillas): its own spring and pull-back
 ref ACR_ZombieNest m_Nest;       // a zombie nest (ACR_Outbreak): out while anyone is near them, grows back
 ref ACR_IED m_IED;               // an IED's team (ACR_IED, #96): the triggerman out while a target is near
 ref ACR_Raid m_Raid;             // a sabotage team (ACR_Raid, #96): never deploys for an enemy, walks to its target
}

class ACR_Defenders
{
 static ref array<ref ACR_DefenderPost> s_Posts;
 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_Posts = {};
  s_mReseatAt = new map<IEntity, float>();
 }
 static const float SPAWN_RANGE = 400;   // as ACR_BaseSystem's garrisons
 static const float DESPAWN_RANGE = 450;
 static const float REMAN_REACH = 8;     // m from an empty gun of a base's works a man of its reserve leaves the bags for it: the nest's own wing
 static const float REMAN_WAIT_MS = 45000; // ... and how long the gun then stays empty before the next man is asked
 protected static ref map<IEntity, float> s_mReseatAt;

 static ACR_DefenderPost Add(string faction, vector pos, string name, float radius)
 {
  ACR_DefenderPost post = new ACR_DefenderPost();
  post.m_sFaction = faction;
  post.m_vPos = pos;
  post.m_sName = name;
  post.m_fRadius = radius;
  s_Posts.Insert(post);
  return post;
 }

 // Its position is gone: no more from the reserve.
 static void Remove(ACR_DefenderPost post)
 {
  if (!post) return;
  post.m_bGone = true;
  post.m_iReserve = 0;
 }

 // From ACR_Fortify.Tick every 5 s.
 static void Tick()
 {
  if (s_Posts.IsEmpty()) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases) return;
  for (int i = s_Posts.Count() - 1; i >= 0; i--)
  {
   ACR_DefenderPost post = s_Posts[i];
   TickGunCrew(manager, post);
   if (post.m_Work && !ACR_BaseWorks.s_bNoLineMenForTest) ACR_Squad.FightLikeSquad(post.m_GunCrew);
   if (post.m_Group)
   {
    ACR_Squad.KeepSimulated(post.m_Group);
    bool stay;
    if (post.m_Cell) stay = ACR_Guerrillas.StayOut(manager, post);
    else if (post.m_Nest) stay = ACR_Outbreak.StayOut(manager, post);
    else if (post.m_IED) stay = ACR_IED.StayOut(manager, post);
    else if (post.m_Raid) stay = ACR_Raid.StayOut(manager, post);
    else stay = EnemyWithin(manager, post.m_sFaction, post.m_vPos, post.m_fSpawnRange + DESPAWN_RANGE - SPAWN_RANGE);
    if (manager.m_Bases.GuardsAlive(post.m_Group) > 0 && stay)
    {
     ACR_BaseWorks.KeepManned(post);
     if (post.m_bOnWings) ACR_Squad.FightLikeSquad(post.m_Group);
     if (post.m_Work) SeatLineMen(post);
     continue;
    }
    vector where = vector.Zero;
    if (post.m_Nest) where = ACR_Outbreak.Centre(manager, post.m_Group);
    int back = manager.m_Bases.PackUpGuards(post.m_Group);
    post.m_Group = null;
    if (!post.m_bGone) post.m_iReserve += back;
    PrintFormat("[ACR] DEFENDERS %1 packed up: %2 came back, %3 in reserve", post.m_sName, back, post.m_iReserve);
    if (post.m_Cell && !post.m_bGone) ACR_Guerrillas.PackedUp(post);
    if (post.m_Nest && !post.m_bGone) ACR_Outbreak.PackedUp(manager, post, where);
    if (post.m_IED && !post.m_bGone) ACR_IED.PackedUp(post);
    if (post.m_Raid && !post.m_bGone) ACR_Raid.PackedUp(post);
   }
   if (post.m_bGone)
   {
    s_Posts.RemoveOrdered(i);
    continue;
   }
   if (post.m_Raid) continue; // ACR_Raid brings its team out itself
   float range = post.m_fSpawnRange;
   bool comeOut;
   if (post.m_Nest)
   {
    comeOut = ACR_Outbreak.TickPost(manager, post);
    if (post.m_bGone) continue; // joined another nest: removed next tick
    range = ACR_Outbreak.Range();
   }
   if (post.m_Cell)
   {
    if (!ACR_Guerrillas.Ready(post)) continue;
    range = ACR_Guerrillas.Range(post);
   }
   if (post.m_IED)
   {
    if (!ACR_IED.Ready(post)) continue;
    range = ACR_IED.RANGE;
   }
   if (post.m_iReserve <= 0 || (!comeOut && !EnemyWithin(manager, post.m_sFaction, post.m_vPos, range, post.m_Cell != null || post.m_IED != null))) continue;
   float spread = 8;
   array<ResourceName> picks = null;
   if (post.m_Nest)
   {
    spread = ACR_Outbreak.SPREAD;
    picks = new array<ResourceName>();
    ACR_Outbreak.Picks(post.m_iReserve, picks);
   }
   post.m_Group = manager.m_Bases.SpawnGuards(post.m_sFaction, post.m_vPos, post.m_iReserve, post.m_sName + " defenders", post.m_fRadius, spread, picks);
   if (!post.m_Group) continue;
   if (post.m_Nest) ACR_Outbreak.NoOrders(post.m_Group);
   PrintFormat("[ACR] DEFENDERS %1 deployed: %2 soldiers", post.m_sName, post.m_iReserve);
   post.m_bOnWings = false;
   post.m_iMannedLine = -1;
   if (ACR_BaseWorks.ManLine(post, post.m_iReserve)) {}
   else if (ACR_Wings.ManWings(post, post.m_iReserve)) {}
   else if (ACR_Fortify.ManReserve(post, post.m_iReserve)) {}
   else if (post.m_fGarrisonArea > 0 && ACR_Garrison.AssignArea(post.m_Group, post.m_Group.GetLeaderAgent(), post.m_vPos, post.m_fGarrisonArea, post.m_iReserve) > 0) ACR_Guerrillas.Order(post.m_Group, ACR_Squad.WP_GARRISON, post.m_vPos, post.m_fGarrisonArea, true);
   post.m_iReserve = 0;
   if (post.m_Cell) ACR_Guerrillas.Deployed(manager, post);
   if (post.m_IED) ACR_IED.Deployed(manager, post);
  }
 }

 // ---------------------------------------------------------------- the gun crew
 // The squad holding its position left it for another job: its guns are manned from the reserve.
 static void CrewGuns(ACR_DefenderPost post, notnull array<IEntity> guns)
 {
  if (!post || guns.IsEmpty()) return;
  post.m_bCrewWanted = true;
  post.m_aCrewGuns.Clear();
  post.m_aCrewGuns.InsertAll(guns);
  if (post.m_Work) PrintFormat("[ACR] DEFENDERS %1: %2 gun(s) to man from the reserve (%3 in reserve)", post.m_sName, guns.Count(), post.m_iReserve);
  else PrintFormat("[ACR] DEFENDERS %1: its squad left for another job, %2 gun(s) to man from the reserve (%3 in reserve)", post.m_sName, guns.Count(), post.m_iReserve);
 }

 // A squad holds it again: the gun crew goes back into the reserve.
 static void StandDownCrew(ACR_DefenderPost post)
 {
  if (post) post.m_bCrewWanted = false;
 }

 // The guns a gun crew holds (ACR_BBBHooks.GetHeldGuns: BBB keeps them manned). Not a base's works reserve out
 // on its bags: listed here too, BBB's remount (every 8 s, from 40 m) pulled every man off the bags onto guns
 // the attackers were shooting at (srv:worksfightbase, 2 Nov: on post 8%, 7 of 8 lost). SeatLineMen does it
 // for the reserve, from beside the gun and seldom. An FIA ambush's tripod gun is (ACR_Guerrillas.HeldGun): BBB keeps
 // its gunner on it while he has no enemy close (BBB_HeldGuns.Keep; its remount walks squads only: ACR_Guerrillas.Reman).
 static bool CrewGunsOf(SCR_AIGroup group, notnull array<IEntity> guns)
 {
  if (!group) return false;
  foreach (ACR_DefenderPost post : s_Posts)
  {
   if (post && post.m_Cell && post.m_Group == group)
   {
    IEntity held = ACR_Guerrillas.HeldGun(post);
    if (held) guns.Insert(held);
    return true;
   }
   if (!post || post.m_GunCrew != group) continue;
   foreach (IEntity gun : post.m_aCrewGuns) if (gun) guns.Insert(gun);
   return true;
  }
  return false;
 }

 // Every Tick: a crew onto the guns once their seats are free (the squad's gunners out), kept on
 // them, and packed up (survivors back into the reserve) once a squad holds the position again or
 // it is gone.
 protected static void TickGunCrew(ACR_Manager manager, ACR_DefenderPost post)
 {
  if (post.m_GunCrew)
  {
   ACR_Squad.KeepSimulated(post.m_GunCrew);
   if (post.m_bCrewWanted && !post.m_bGone && manager.m_Bases.GuardsAlive(post.m_GunCrew) > 0)
   {
    SeatCrew(post);
    foreach (IEntity crewGun : post.m_aCrewGuns) ACR_GunHold.Restock(crewGun);
    return;
   }
   int back = manager.m_Bases.PackUpGuards(post.m_GunCrew);
   post.m_GunCrew = null;
   if (!post.m_bGone) post.m_iReserve += back;
   PrintFormat("[ACR] DEFENDERS %1: the gun crew back in reserve (%2 came back, %3 in reserve)", post.m_sName, back, post.m_iReserve);
   return;
  }
  if (!post.m_bCrewWanted || post.m_bGone || post.m_iReserve <= 0) return;
  int free;
  vector at = post.m_vPos;
  foreach (IEntity gun : post.m_aCrewGuns)
  {
   if (!FreeTurret(gun)) continue;
   if (free == 0) at = gun.GetOrigin();
   free++;
  }
  if (free <= 0) return;
  int count = Math.Min(free, post.m_iReserve);
  post.m_GunCrew = manager.m_Bases.SpawnGuards(post.m_sFaction, at, count, post.m_sName + " gun crew", 10);
  if (!post.m_GunCrew) return;
  post.m_iReserve -= count;
  int seated = SeatCrew(post);
  PrintFormat("[ACR] DEFENDERS %1: %2 from the reserve man its guns while its squad is away (%3 seated, %4 left in reserve)", post.m_sName, count, seated, post.m_iReserve);
 }

 // A base's works reserve out on its bags: a gun of the works standing empty (its gunner shot, or never crewed)
 // is manned by the man on the bags beside it (within REMAN_REACH), at most once per REMAN_WAIT_MS per gun. The
 // gunner is who the attackers shoot first: from further off, or oftener, this emptied the bags into the guns
 // (srv:worksfightbase, 2 Nov: 10 re-mans, 7 of 8 lost; the men on the bags were doing the killing).
 static void SeatLineMen(ACR_DefenderPost post)
 {
  if (!post.m_Group || post.m_aCrewGuns.IsEmpty()) return;
  array<AIAgent> agents = {};
  post.m_Group.GetAgents(agents);
  float now = GetGame().GetWorld().GetWorldTime();
  foreach (IEntity gun : post.m_aCrewGuns)
  {
   BaseCompartmentSlot slot = FreeTurret(gun);
   if (!slot) continue;
   float again;
   if (s_mReseatAt.Find(gun, again) && now < again) continue;
   s_mReseatAt.Set(gun, now + REMAN_WAIT_MS);
   ChimeraCharacter best;
   float nearest = REMAN_REACH;
   foreach (AIAgent agent : agents)
   {
    ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
    if (!body || !ACR_Respawn.IsAlive(body) || body.IsInVehicle()) continue;
    CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
    if (!access || access.IsGettingIn() || access.IsGettingOut()) continue;
    float d = vector.DistanceXZ(body.GetOrigin(), gun.GetOrigin());
    if (d >= nearest) continue;
    nearest = d;
    best = body;
   }
   if (!best) continue;
   if (best.GetCompartmentAccessComponent().GetInVehicle(gun, slot, true, -1, ECloseDoorAfterActions.LEAVE_OPEN, false))
    PrintFormat("[ACR] DEFENDERS %1: a man from the bags %2 m off mans the works' gun again", post.m_sName, (int)nearest);
  }
 }

 // A gun's empty gunner seat (null: none, the gun is gone, or it is out of ammo: BBB gets a
 // gunner off a dry gun, #154, so nobody is put back on it until it has ammo again).
 static BaseCompartmentSlot FreeTurret(IEntity gun)
 {
  if (!gun || ACR_Logistics.Health(gun) <= 0) return null;
  SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(gun.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!seats) return null;
  array<BaseCompartmentSlot> turrets = {};
  seats.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
  foreach (BaseCompartmentSlot slot : turrets) if (slot && !slot.GetOccupant() && BBB_VehicleHelpers.TurretHasAmmo(slot)) return slot;
  return null;
 }

 // A gun's gunner seat, taken or not (null: none).
 static BaseCompartmentSlot TurretSeat(IEntity gun)
 {
  if (!gun) return null;
  SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(gun.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!seats) return null;
  array<BaseCompartmentSlot> turrets = {};
  seats.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
  if (turrets.IsEmpty()) return null;
  return turrets[0];
 }

 // The crew's men on foot into its guns' empty seats, each to a gun of his own (a seat only shows
 // as taken once he sits in it, so they all went for the first free gun); how many sit in one.
 protected static int SeatCrew(ACR_DefenderPost post)
 {
  int seated;
  array<IEntity> taken = {};
  array<AIAgent> agents = {};
  post.m_GunCrew.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter body = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (!body || !ACR_Respawn.IsAlive(body)) continue;
   if (body.IsInVehicle())
   {
    seated++;
    continue;
   }
   CompartmentAccessComponent access = body.GetCompartmentAccessComponent();
   if (!access || access.IsGettingIn() || access.IsGettingOut()) continue;
   foreach (IEntity gun : post.m_aCrewGuns)
   {
    BaseCompartmentSlot slot = FreeTurret(gun);
    if (!slot || taken.Contains(gun)) continue;
    taken.Insert(gun);
    if (access.GetInVehicle(gun, slot, true, -1, ECloseDoorAfterActions.LEAVE_OPEN, false)) seated++;
    break;
   }
  }
  return seated;
 }

 // A living enemy soldier (a squad's or a player) within range of pos. For a guerrilla cell
 // ('cell'), sides FIA spares (ACR_Insurgency.Spared) don't count.
 static bool EnemyWithin(ACR_Manager manager, string faction, vector pos, float range, bool cell = false)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction() || ACR_Insurgency.Friendly(squad.GetFaction().GetFactionKey(), faction)) continue;
   if (cell && ACR_Insurgency.Spared(squad.GetFaction().GetFactionKey())) continue;
   IEntity anchor = squad.GetAnchor();
   if (!anchor || vector.DistanceXZ(anchor.GetOrigin(), pos) > range + 300) continue;
   array<AIAgent> agents = {};
   squad.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    IEntity body = agent.GetControlledEntity();
    if (body && ACR_Respawn.IsAlive(body) && vector.DistanceXZ(body.GetOrigin(), pos) <= range) return true;
   }
  }
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players)
  {
   IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
   if (!player || !ACR_Respawn.IsAlive(player)) continue;
   string key = ACR_Intel.FactionKeyOf(player);
   if (key.IsEmpty() || ACR_Insurgency.Friendly(key, faction) || (cell && ACR_Insurgency.Spared(key))) continue;
   if (vector.DistanceXZ(player.GetOrigin(), pos) <= range) return true;
  }
  // FIA saboteurs (#96): built positions' defenders deploy against them.
  return ACR_Raid.RaiderWithin(faction, pos, range);
 }

 static void Reset()
 {
  s_mReseatAt.Clear();
  s_Posts.Clear();
 }
}
