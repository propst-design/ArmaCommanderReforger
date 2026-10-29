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
 ref ACR_GuerrillaCell m_Cell;    // an FIA guerrilla cell (ACR_Guerrillas): its own spring and pull-back
}

class ACR_Defenders
{
 static ref array<ref ACR_DefenderPost> s_Posts = {};
 static const float SPAWN_RANGE = 400;   // as ACR_BaseSystem's garrisons
 static const float DESPAWN_RANGE = 450;

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
   if (post.m_Group)
   {
    ACR_Squad.KeepSimulated(post.m_Group);
    bool stay;
    if (post.m_Cell) stay = ACR_Guerrillas.StayOut(manager, post);
    else stay = EnemyWithin(manager, post.m_sFaction, post.m_vPos, post.m_fSpawnRange + DESPAWN_RANGE - SPAWN_RANGE);
    if (manager.m_Bases.GuardsAlive(post.m_Group) > 0 && stay) continue;
    int back = manager.m_Bases.PackUpGuards(post.m_Group);
    post.m_Group = null;
    if (!post.m_bGone) post.m_iReserve += back;
    PrintFormat("[ACR] DEFENDERS %1 packed up: %2 came back, %3 in reserve", post.m_sName, back, post.m_iReserve);
    if (post.m_Cell && !post.m_bGone) ACR_Guerrillas.PackedUp(post);
   }
   if (post.m_bGone)
   {
    s_Posts.RemoveOrdered(i);
    continue;
   }
   float range = post.m_fSpawnRange;
   if (post.m_Cell)
   {
    if (!ACR_Guerrillas.Ready(post)) continue;
    range = ACR_Guerrillas.Range(post);
   }
   if (post.m_iReserve <= 0 || !EnemyWithin(manager, post.m_sFaction, post.m_vPos, range)) continue;
   post.m_Group = manager.m_Bases.SpawnGuards(post.m_sFaction, post.m_vPos, post.m_iReserve, post.m_sName + " defenders", post.m_fRadius);
   if (!post.m_Group) continue;
   PrintFormat("[ACR] DEFENDERS %1 deployed: %2 soldiers", post.m_sName, post.m_iReserve);
   post.m_iReserve = 0;
   if (post.m_Cell) ACR_Guerrillas.Deployed(manager, post);
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
  PrintFormat("[ACR] DEFENDERS %1: its squad left for another job, %2 gun(s) to man from the reserve (%3 in reserve)", post.m_sName, guns.Count(), post.m_iReserve);
 }

 // A squad holds it again: the gun crew goes back into the reserve.
 static void StandDownCrew(ACR_DefenderPost post)
 {
  if (post) post.m_bCrewWanted = false;
 }

 // The guns a gun crew holds (ACR_BBBHooks.GetHeldGuns: BBB keeps them manned).
 static bool CrewGunsOf(SCR_AIGroup group, notnull array<IEntity> guns)
 {
  if (!group) return false;
  foreach (ACR_DefenderPost post : s_Posts)
  {
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

 // A gun's empty gunner seat (null: none, or the gun is gone).
 static BaseCompartmentSlot FreeTurret(IEntity gun)
 {
  if (!gun || ACR_Logistics.Health(gun) <= 0) return null;
  SCR_BaseCompartmentManagerComponent seats = SCR_BaseCompartmentManagerComponent.Cast(gun.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!seats) return null;
  array<BaseCompartmentSlot> turrets = {};
  seats.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
  foreach (BaseCompartmentSlot slot : turrets) if (slot && !slot.GetOccupant()) return slot;
  return null;
 }

 // The crew's men on foot into its guns' empty seats; how many sit in one.
 protected static int SeatCrew(ACR_DefenderPost post)
 {
  int seated;
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
    if (!slot) continue;
    if (access.GetInVehicle(gun, slot, true, -1, ECloseDoorAfterActions.LEAVE_OPEN, false)) seated++;
    break;
   }
  }
  return seated;
 }

 // A living enemy soldier (a squad's or a player) within range of pos.
 static bool EnemyWithin(ACR_Manager manager, string faction, vector pos, float range)
 {
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad.GetFaction() || ACR_Insurgency.Friendly(squad.GetFaction().GetFactionKey(), faction)) continue;
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
   if (!key.IsEmpty() && !ACR_Insurgency.Friendly(key, faction) && vector.DistanceXZ(player.GetOrigin(), pos) <= range) return true;
  }
  return false;
 }

 static void Reset()
 {
  s_Posts.Clear();
 }
}
