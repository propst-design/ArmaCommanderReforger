// FIA spoils (insurgency part 4, Jacob 19 Nov: "The FIA should loot gear, steal vehicles into a
// pool, and attempt to take prisoners"). After a fight, FIA go over the ground they hold:
//  - Who: a fighting cell or hideout (ACR_Guerrillas, sprung) or an FIA pocket's garrison that is
//    out (ACR_BaseSystem). It sends a spoils team: TEAM men split off the group (never the last
//    of it), on foot, while the ground round the spoil is clear: no awake man of a side FIA fights
//    within FIELD_CLEAR of it (the knocked out and those who gave up don't count; BBB prisoners
//    carry the engine's disarmed flag).
//  - What (within SWEEP_REACH of where the group fights, nearest first):
//     the dead of a side FIA fights: their weapons and magazines (Loot);
//     the knocked out and those who surrendered: looted, then taken prisoner (ACR_FIAPrisoners);
//     an empty, sound vehicle nobody uses (ACR_Depots.Stealable): the team drives it to a depot or
//     a hideout's stash and it joins FIA's vehicles for raids (ACR_Depots.Steal).
//  - Loot arms recruits: each weapon (heavier ones count double) and some magazines add to the
//    pool of the hideout that feeds the area (ACR_FIAIntel.SupplierOf), so the hideout raises and
//    tops up cells sooner (ACR_FIAGrowth). Each body is looted once; FIA never loot their own.
//  - Back: the team walks back and rejoins its group (or its reserve, if the group packed up).
//    Contact on the way (an awake enemy within FIELD_CLEAR of a team man) sends it back at once.
// Option: -acrFIASpoils 0 turns off loot, vehicle theft and prisoners (default on with FIA).
class ACR_SpoilsTeam : Managed
{
 SCR_AIGroup m_Group;
 ACR_DefenderPost m_Post;   // the cell or hideout it came from (null: a pocket's garrison)
 ACR_Base m_Base;           // the pocket it came from
 vector m_vHome;            // where it walks back to
 IEntity m_Target;
 bool m_bHome;
 float m_fSince;            // sent out
 float m_fHomeSince;
 int m_iTaken;
 string m_sName;
 float m_fBestGap;         // nearest it has been to its target
 float m_fProgressAt;      // when it last got nearer
 int m_iStalls;            // re-orders while it got no nearer
}

// A recruit's rifle swapped for a looted one (ACR_FIASpoils.Arm): once he holds the new gun, the old
// one and its magazines go and the new one's magazines come; not taken in time, the new gun goes.
class ACR_LootSwap : Managed
{
 IEntity m_Soldier;
 IEntity m_Old;
 IEntity m_New;
 int m_iTries;
 ref array<typename> m_OldWells = {};

 void Confirm()
 {
  if (!m_New)
  {
   ACR_FIASpoils.s_aSwaps.RemoveItem(this);
   return;
  }
  if (!ACR_Respawn.IsAlive(m_Soldier))
  {
   if (ACR_SquadMarksmen.Loose(m_New)) SCR_EntityHelper.DeleteEntityAndChildren(m_New);
   ACR_FIASpoils.s_aSwaps.RemoveItem(this);
   return;
  }
  ChimeraCharacter man = ChimeraCharacter.Cast(m_Soldier);
  CharacterControllerComponent controller = man.GetCharacterController();
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(man.FindComponent(SCR_InventoryStorageManagerComponent));
  BaseWeaponManagerComponent weapons = controller.GetWeaponManagerComponent();
  array<IEntity> carried = {};
  if (weapons) weapons.GetWeaponsList(carried);
  bool done = inventory && carried.Contains(m_New) && !controller.IsChangingItem();
  if (done && controller.GetCurrentItemInHands() == m_Old)
  {
   controller.TryEquipRightHandItem(m_New, EEquipItemType.EEquipTypeWeapon, false);
   done = false;
  }
  if (!done)
  {
   m_iTries++;
   if (m_iTries < 6)
   {
    GetGame().GetCallqueue().CallLater(Confirm, 500);
    return;
   }
   if (ACR_SquadMarksmen.Loose(m_New)) SCR_EntityHelper.DeleteEntityAndChildren(m_New);
   PrintFormat("[ACR] SPOILS %1: looted %2 not taken, keeps his own", ACR_UALoadout.Leaf(m_Soldier), ACR_UALoadout.Leaf(m_New));
   ACR_FIASpoils.s_aSwaps.RemoveItem(this);
   return;
  }
  array<IEntity> items = {};
  inventory.GetItems(items);
  foreach (IEntity item : items)
  {
   if (!item) continue;
   BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
   if (!magazine || !magazine.GetMagazineWell()) continue;
   bool old;
   foreach (typename well : m_OldWells) if (magazine.GetMagazineWell().Type().IsInherited(well)) old = true;
   if (old) inventory.TryDeleteItem(item);
  }
  if (m_Old && (inventory.Contains(m_Old) || ACR_SquadMarksmen.Loose(m_Old))) SCR_EntityHelper.DeleteEntityAndChildren(m_Old);
  BaseWeaponComponent weapon = BaseWeaponComponent.Cast(m_New.FindComponent(BaseWeaponComponent));
  array<BaseMuzzleComponent> muzzles = {};
  if (weapon) weapon.GetMuzzlesList(muzzles);
  if (!muzzles.IsEmpty())
  {
   ResourceName ammo = muzzles[0].GetDefaultMagazineOrProjectileName();
   for (int n = ACR_UALoadout.CountPrefab(inventory, ammo); n < ACR_FIASpoils.ARMED_MAGAZINES; n++)
    if (!ACR_UALoadout.SpawnCarried(inventory, ammo)) break;
  }
  ACR_FIASpoils.s_iArmed++;
  ACR_FIASpoils.s_aSwaps.RemoveItem(this);
  PrintFormat("[ACR] SPOILS %1 carries looted %2", ACR_UALoadout.Leaf(m_Soldier), ACR_UALoadout.Leaf(m_New));
 }
}

class ACR_FIASpoils
{
 static const int TEAM = 2;
 static const int LOOK_EVERY_S = 3;
 static const float SWEEP_REACH = 150;     // m from where the group fights: what it goes over
 static const float FIELD_CLEAR = 150;     // no awake enemy this close to a spoil (or to the team)
 static const float TAKE_REACH = 4;        // a team man this close takes it
 static const float VEHICLE_REACH = 8;     // ... a vehicle
 static const float LOOSE_REACH = 3;       // weapons on the ground this close to a body go with it
 static const float SWEEP_MAX_MS = 180000; // out this long: back
 static const float HOME_REACH = 20;
 static const float STALL_MS = 20000;      // no nearer to its target this long: ordered again ...
 static const int STALLS = 3;              // ... this often, then the spoil is given up
 static const float HOME_MAX_MS = 300000;  // walking back this long without getting there: they rejoin all the same, out of contact
 static const float ARMS_RECRUITS = 0.25;  // recruits a hideout gains for each arms point
 static const float HEAVY_POINTS = 2;      // a machine gun or launcher; a rifle 1, a handgun 0.5
 static const float MAGAZINE_POINTS = 0.1;
 static const float BODY_POINTS_MAX = 3;   // one body's gear counts at most this much
 protected static bool s_bOn;
 protected static int s_iSeconds;
 static ref array<ref ACR_SpoilsTeam> s_aTeams;
 protected static ref array<IEntity> s_aFound;
 protected static ref array<IEntity> s_aTaken;     // looted or taken: never twice
 protected static ref array<vector> s_aAwakePos;   // awake men of the war this second (AwakeNear)
 protected static ref array<string> s_aAwakeKey;
 protected static float s_fAwakeAt;
 static int s_iBodies;                    // bodies looted this match
 static int s_iWeapons;                   // weapons taken
 static float s_fArms;                    // arms points
 static int s_iRejoined;                  // teams back with their group or reserve
 static bool s_bLookForTest;              // tests: look every second

 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_aTeams = {};
  s_aSwaps = {};
  s_aFound = {};
  s_aTaken = {};
  s_aAwakePos = {};
  s_aAwakeKey = {};
 }

 // Per match (ACR_Manager.Stop) and at ACR_Guerrillas.Apply.
 static void Reset()
 {
  s_aTeams.Clear();
  s_aFound.Clear();
  s_aTaken.Clear();
  s_aAwakePos.Clear();
  s_aAwakeKey.Clear();
  s_fAwakeAt = -1;
  s_iSeconds = 0;
  s_iBodies = 0;
  s_iWeapons = 0;
  s_fArms = 0;
  s_iRejoined = 0;
  s_iArmed = 0;
  if (GetGame() && GetGame().GetCallqueue())
   foreach (ACR_LootSwap pending : s_aSwaps) if (pending) GetGame().GetCallqueue().Remove(pending.Confirm);
  s_aSwaps.Clear();
  s_bOn = false;
  s_bLookForTest = false;
 }

 // ACR_Guerrillas.Apply, once FIA is in the world.
 static void Apply()
 {
  Reset();
  s_bOn = OptionOn();
  ACR_FIAPrisoners.Reset();
  PrintFormat("[ACR] SPOILS %1 (-acrFIASpoils)", s_bOn);
 }

 static bool On() { return s_bOn; }
 static void SetOnForTest(bool on) { s_bOn = on; }

 protected static bool OptionOn()
 {
  string value;
  if (!ACR_Options.Get("acrFIASpoils", value)) return true;
  value.ToLower();
  return !(value == "0" || value == "off" || value == "false");
 }

 // Every second (ACR_Guerrillas.Tick).
 static void Tick()
 {
  if (!s_bOn) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases) return;
  s_iSeconds++;
  float now = GetGame().GetWorld().GetWorldTime();
  PruneTaken();
  for (int i = s_aTeams.Count() - 1; i >= 0; i--)
  {
   if (!Step(manager, s_aTeams[i], now)) s_aTeams.RemoveOrdered(i);
  }
  if (s_bLookForTest || s_iSeconds % LOOK_EVERY_S == 0) Look(manager, now);
  ACR_FIAPrisoners.Tick(manager, now);
 }

 // ---------------------------------------------------------------- who goes

 protected static void Look(ACR_Manager manager, float now)
 {
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Cell || post.m_bGone || !post.m_Group || !post.m_Cell.m_bSprung || TeamOf(post, null)) continue;
   vector anchor = post.m_Cell.m_vKillZone;
   if (post.m_Cell.m_bHideout) anchor = post.m_vPos;
   Send(manager, post.m_Group, anchor, post, null, post.m_sName, now);
  }
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base.m_bPocket || base.m_bRetired || base.m_sOwner != ACR_Pockets.FACTION || !base.m_GarrisonGroup || TeamOf(null, base)) continue;
   Send(manager, base.m_GarrisonGroup, base.GetOrigin(), null, base, "FIA " + base.m_sName, now);
  }
 }

 static ACR_SpoilsTeam TeamOf(ACR_DefenderPost post, ACR_Base base)
 {
  foreach (ACR_SpoilsTeam team : s_aTeams)
  {
   if (post && team.m_Post == post) return team;
   if (base && team.m_Base == base) return team;
  }
  return null;
 }

 // A cell, hideout or pocket stays out for its spoils (ACR_Guerrillas.StayOut, ACR_BaseSystem.HoldOut):
 // its team is out, or something lies there to take and it has men to send. Each spoil is gone for
 // good once taken or given up (a team that couldn't reach it), so this ends.
 static bool Holds(ACR_DefenderPost post, ACR_Base base)
 {
  if (!s_bOn) return false;
  if (TeamOf(post, base)) return true;
  SCR_AIGroup group;
  vector anchor;
  if (post)
  {
   if (!post.m_Cell || !post.m_Cell.m_bSprung) return false;
   group = post.m_Group;
   anchor = post.m_Cell.m_vKillZone;
   if (post.m_Cell.m_bHideout) anchor = post.m_vPos;
  }
  else if (base)
  {
   if (base.m_sOwner != ACR_Pockets.FACTION) return false;
   group = base.m_GarrisonGroup;
   anchor = base.GetOrigin();
  }
  array<ChimeraCharacter> men = {};
  Awake(group, men);
  if (men.Count() <= TEAM) return false;
  int dismounted;
  foreach (ChimeraCharacter man : men) if (!man.IsInVehicle()) dismounted++;
  return dismounted >= TEAM && NextSpoil(anchor, anchor, null) != null;
 }

 // Living men of a cell's spoils team out (still the cell's: ACR_Guerrillas.Tick).
 static int TeamAlive(ACR_DefenderPost post)
 {
  ACR_SpoilsTeam team = TeamOf(post, null);
  if (!team || !team.m_Group) return 0;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases) return 0;
  return manager.m_Bases.GuardsAlive(team.m_Group);
 }

 // A spoils team out of 'group' for the nearest spoil round 'anchor' (false: nothing to take, the
 // ground not clear, or too few men to spare).
 static bool Send(ACR_Manager manager, SCR_AIGroup group, vector anchor, ACR_DefenderPost post, ACR_Base base, string name, float now)
 {
  IEntity target = NextSpoil(anchor, anchor, null);
  if (!target) return false;
  array<ChimeraCharacter> men = {};
  Awake(group, men);
  if (men.Count() <= TEAM) return false;
  // The men nearest it (none on a gun or in a vehicle).
  array<ChimeraCharacter> picked = {};
  for (int n = 0; n < TEAM; n++)
  {
   ChimeraCharacter best;
   float nearest = 100000;
   foreach (ChimeraCharacter man : men)
   {
    if (picked.Contains(man) || man.IsInVehicle()) continue;
    float d = vector.DistanceXZ(man.GetOrigin(), target.GetOrigin());
    if (d >= nearest) continue;
    nearest = d;
    best = man;
   }
   if (best) picked.Insert(best);
  }
  if (picked.Count() < TEAM) return false;
  SCR_AIGroup split = manager.m_Bases.SplitGuards(group, picked, name + " spoils team");
  if (!split) return false;
  ACR_SpoilsTeam team = new ACR_SpoilsTeam();
  team.m_Group = split;
  team.m_Post = post;
  team.m_Base = base;
  team.m_vHome = anchor;
  team.m_fSince = now;
  team.m_sName = name;
  s_aTeams.Insert(team);
  ACR_Guerrillas.SetCombatMode(split, EAIGroupCombatMode.RETURN_FIRE);
  GoFor(team, target);
  PrintFormat("[ACR] SPOILS %1 sends %2 men for %3 at %4 (%5 m)", name, picked.Count(), What(target), ACR_Manager.Grid(target.GetOrigin()), (int)vector.DistanceXZ(anchor, target.GetOrigin()));
  return true;
 }

 protected static void GoFor(ACR_SpoilsTeam team, IEntity target)
 {
  team.m_Target = target;
  team.m_fProgressAt = 0;
  team.m_iStalls = 0;
  float radius = 2;
  if (Vehicle.Cast(target)) radius = 5;
  ACR_Guerrillas.Order(team.m_Group, ACR_Squad.WP_MOVE, target.GetOrigin(), radius, false);
 }

 // The conscious, armed men of a group.
 static void Awake(SCR_AIGroup group, notnull array<ChimeraCharacter> men)
 {
  if (!group) return;
  array<AIAgent> agents = {};
  group.GetAgents(agents);
  foreach (AIAgent agent : agents)
  {
   ChimeraCharacter man = ChimeraCharacter.Cast(agent.GetControlledEntity());
   if (man && ACR_Squad.IsConscious(man) && !ACR_Squad.FindController(man)) men.Insert(man);
  }
 }

 // ---------------------------------------------------------------- what

 protected static bool CollectSpoil(IEntity e)
 {
  if ((ChimeraCharacter.Cast(e) || Vehicle.Cast(e)) && !s_aFound.Contains(e)) s_aFound.Insert(e);
  return true;
 }

 // The nearest spoil to 'from' within SWEEP_REACH of 'anchor' whose ground is clear (null: none).
 static IEntity NextSpoil(vector anchor, vector from, ACR_SpoilsTeam team)
 {
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(anchor, SWEEP_REACH, CollectSpoil, null, EQueryEntitiesFlags.DYNAMIC);
  IEntity best;
  float nearest = 100000;
  foreach (IEntity e : s_aFound)
  {
   if (!e || s_aTaken.Contains(e) || !Spoil(e)) continue;
   // Another team already after it.
   bool claimed;
   foreach (ACR_SpoilsTeam other : s_aTeams) if (other != team && other.m_Target == e) claimed = true;
   if (claimed) continue;
   float d = vector.DistanceXZ(from, e.GetOrigin());
   if (d >= nearest || AwakeNear(e.GetOrigin(), FIELD_CLEAR, true)) continue;
   nearest = d;
   best = e;
  }
  s_aFound.Clear();
  return best;
 }

 // Worth taking: a body of a side FIA fights (dead with gear, or alive and out of the fight: knocked
 // out or surrendered), or a vehicle FIA can steal.
 static bool Spoil(IEntity e)
 {
  if (Vehicle.Cast(e)) return ACR_Depots.Stealable(e);
  ChimeraCharacter man = ChimeraCharacter.Cast(e);
  if (!man || ACR_FIAPrisoners.Held(man) || !ACR_IED.Target(ACR_Pockets.FACTION, ACR_Intel.FactionKeyOf(man))) return false;
  // In a vehicle: only one that has stopped (a crew knocked out in their seats, Jacob 19 Nov).
  if (man.IsInVehicle() && Moving(ACR_Transport.VehicleOf(man))) return false;
  if (!ACR_Respawn.IsAlive(man)) return HasGear(man);
  return OutOfFight(man);
 }

 static bool Moving(IEntity vehicle)
 {
  if (!vehicle || !vehicle.GetPhysics()) return false;
  return vehicle.GetPhysics().GetVelocity().Length() > 1.0;
 }

 // Alive but no longer fighting: knocked out, or given up (BBB's prisoners: the disarmed flag).
 static bool OutOfFight(ChimeraCharacter man)
 {
  if (!man || !ACR_Respawn.IsAlive(man)) return false;
  if (man.GetCharacterController().IsUnconscious()) return true;
  PerceivableComponent perceivable = PerceivableComponent.Cast(man.FindComponent(PerceivableComponent));
  return perceivable && perceivable.IsDisarmed();
 }

 protected static bool HasGear(ChimeraCharacter body)
 {
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(body.FindComponent(SCR_InventoryStorageManagerComponent));
  if (inventory)
  {
   array<IEntity> items = {};
   inventory.GetItems(items);
   foreach (IEntity item : items) if (item && (item.FindComponent(WeaponComponent) || item.FindComponent(BaseMagazineComponent))) return true;
  }
  return LooseWeapon(body.GetOrigin()) != null;
 }

 protected static string What(IEntity e)
 {
  if (Vehicle.Cast(e)) return ACR_Vehicles.NameOf(e);
  if (!ACR_Respawn.IsAlive(e)) return "the gear of the dead";
  return "a prisoner";
 }

 // ---------------------------------------------------------------- awake men

 // An awake man of the war (a squad's, a garrison's or a player; not knocked out, not given up)
 // within range: of a side FIA fights (hostile), else of 'side'.
 static bool AwakeNear(vector pos, float range, bool hostile, string side = "")
 {
  float now = GetGame().GetWorld().GetWorldTime();
  if (now != s_fAwakeAt) Census(now);
  foreach (int i, vector at : s_aAwakePos)
  {
   if (vector.DistanceXZ(at, pos) > range) continue;
   string key = s_aAwakeKey[i];
   if (hostile && ACR_IED.Target(ACR_Pockets.FACTION, key)) return true;
   if (!hostile && key == side) return true;
  }
  return false;
 }

 // Once per world time: every awake man's place and side (AI agents and players).
 protected static void Census(float now)
 {
  s_fAwakeAt = now;
  s_aAwakePos.Clear();
  s_aAwakeKey.Clear();
  array<AIAgent> agents = {};
  GetGame().GetAIWorld().GetAIAgents(agents);
  foreach (AIAgent agent : agents) AddAwake(agent.GetControlledEntity());
  array<int> players = {};
  GetGame().GetPlayerManager().GetPlayers(players);
  foreach (int id : players) AddAwake(GetGame().GetPlayerManager().GetPlayerControlledEntity(id));
 }

 protected static void AddAwake(IEntity entity)
 {
  ChimeraCharacter man = ChimeraCharacter.Cast(entity);
  if (!man || !ACR_Squad.IsConscious(man)) return;
  PerceivableComponent perceivable = PerceivableComponent.Cast(man.FindComponent(PerceivableComponent));
  if (perceivable && perceivable.IsDisarmed()) return;
  string key = ACR_Intel.FactionKeyOf(man);
  if (key.IsEmpty()) return;
  s_aAwakePos.Insert(man.GetOrigin());
  s_aAwakeKey.Insert(key);
 }

 // ---------------------------------------------------------------- the team out

 // One team's second; false: it is done (rejoined, gone, or now a vehicle's crew).
 protected static bool Step(ACR_Manager manager, ACR_SpoilsTeam team, float now)
 {
  array<ChimeraCharacter> men = {};
  Awake(team.m_Group, men);
  if (men.IsEmpty())
  {
   // Knocked out: they lie there a while (their group holds them), then go back to it as any team.
   if (team.m_Group && manager.m_Bases.GuardsAlive(team.m_Group) > 0 && now - team.m_fSince < SWEEP_MAX_MS + HOME_MAX_MS) return true;
   if (team.m_Group && manager.m_Bases.GuardsAlive(team.m_Group) > 0)
   {
    Rejoin(manager, team);
    return false;
   }
   PrintFormat("[ACR] SPOILS %1: the spoils team is gone (%2 taken)", team.m_sName, team.m_iTaken);
   if (team.m_Group) manager.m_Bases.PackUpGuards(team.m_Group);
   return false;
  }
  bool contact;
  foreach (ChimeraCharacter man : men) if (AwakeNear(man.GetOrigin(), FIELD_CLEAR, true)) contact = true;
  if (team.m_bHome) return WalkHome(manager, team, men, contact, now);
  if (contact)
  {
   PrintFormat("[ACR] SPOILS %1: the spoils team runs into the enemy, back", team.m_sName);
   ACR_Guerrillas.SetCombatMode(team.m_Group, EAIGroupCombatMode.FIRE_AT_WILL);
   GoHome(team, now);
   return true;
  }
  // Whatever lies by a team man is taken; a vehicle reached becomes theirs to drive.
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(men[0].GetOrigin(), VEHICLE_REACH + 20, CollectSpoil, null, EQueryEntitiesFlags.DYNAMIC);
  array<IEntity> near = {};
  near.InsertAll(s_aFound);
  s_aFound.Clear();
  // The men first (a crew in its seats too), then the vehicle they lay in.
  foreach (IEntity body : near)
  {
   if (!body || Vehicle.Cast(body) || s_aTaken.Contains(body) || !Spoil(body)) continue;
   bool byBody;
   foreach (ChimeraCharacter looter : men) if (vector.DistanceXZ(looter.GetOrigin(), body.GetOrigin()) <= TAKE_REACH + 2) byBody = true;
   if (byBody) Take(team, ChimeraCharacter.Cast(body));
  }
  foreach (IEntity e : near)
  {
   if (!e || !Vehicle.Cast(e) || s_aTaken.Contains(e)) continue;
   bool by;
   foreach (ChimeraCharacter taker : men) if (vector.DistanceXZ(taker.GetOrigin(), e.GetOrigin()) <= VEHICLE_REACH) by = true;
   // Only the one they went for; it becomes the depot's, its crew drives it home and joins there.
   bool crewTaken;
   if (!by || e != team.m_Target || !ACR_Depots.Steal(e, team.m_Group, team.m_sName, crewTaken)) continue;
   s_aTaken.Insert(e);
   team.m_iTaken++;
   if (crewTaken) return false;
   // Parked where it stood (by the yard): the team carries on.
   team.m_Target = null;
  }
  if (now - team.m_fSince >= SWEEP_MAX_MS)
  {
   // What they couldn't get to is given up (or the group would stay out for it for good).
   if (team.m_Target) s_aTaken.Insert(team.m_Target);
   GoHome(team, now);
   return true;
  }
  if (team.m_Target && !s_aTaken.Contains(team.m_Target) && Spoil(team.m_Target))
  {
   if (Progress(team, men, now)) return true;
   // Stuck on the way (no route, a wall): given up, on to the next.
   PrintFormat("[ACR] SPOILS %1: the spoils team can't get to %2, gives it up", team.m_sName, What(team.m_Target));
   s_aTaken.Insert(team.m_Target);
  }
  IEntity next = NextSpoil(team.m_vHome, men[0].GetOrigin(), team);
  if (next) GoFor(team, next);
  else GoHome(team, now);
  return true;
 }

 // Getting closer to its target (true), or ordered again while stalled; false once STALLS re-orders
 // haven't moved it.
 protected static bool Progress(ACR_SpoilsTeam team, array<ChimeraCharacter> men, float now)
 {
  float gap = 100000;
  foreach (ChimeraCharacter man : men) gap = Math.Min(gap, vector.DistanceXZ(man.GetOrigin(), team.m_Target.GetOrigin()));
  if (team.m_fProgressAt <= 0 || gap < team.m_fBestGap - 2)
  {
   team.m_fBestGap = gap;
   team.m_fProgressAt = now;
   return true;
  }
  if (now - team.m_fProgressAt < STALL_MS) return true;
  if (team.m_iStalls >= STALLS) return false;
  team.m_iStalls++;
  team.m_fProgressAt = now;
  float radius = 2;
  if (Vehicle.Cast(team.m_Target)) radius = 5;
  ACR_Guerrillas.Order(team.m_Group, ACR_Squad.WP_MOVE, team.m_Target.GetOrigin(), radius, false);
  return true;
 }

 protected static void GoHome(ACR_SpoilsTeam team, float now)
 {
  team.m_bHome = true;
  team.m_fHomeSince = now;
  team.m_Target = null;
  vector home = HomeOf(team);
  ACR_Guerrillas.Order(team.m_Group, ACR_Squad.WP_MOVE, home, 10, false);
  PrintFormat("[ACR] SPOILS %1: the spoils team heads back (%2 taken)", team.m_sName, team.m_iTaken);
 }

 // Where its group is now (its leader), else where it set out from.
 protected static vector HomeOf(ACR_SpoilsTeam team)
 {
  SCR_AIGroup group;
  if (team.m_Post) group = team.m_Post.m_Group;
  if (team.m_Base) group = team.m_Base.m_GarrisonGroup;
  if (group && group.GetLeaderAgent() && group.GetLeaderAgent().GetControlledEntity()) return group.GetLeaderAgent().GetControlledEntity().GetOrigin();
  return team.m_vHome;
 }

 protected static bool WalkHome(ACR_Manager manager, ACR_SpoilsTeam team, array<ChimeraCharacter> men, bool contact, float now)
 {
  vector home = HomeOf(team);
  bool there;
  foreach (ChimeraCharacter man : men) if (vector.DistanceXZ(man.GetOrigin(), home) <= HOME_REACH) there = true;
  if (!there && (contact || now - team.m_fHomeSince < HOME_MAX_MS)) return true;
  Rejoin(manager, team);
  return false;
 }

 // Back with its group (into the same group if it is out, else its reserve).
 protected static void Rejoin(ACR_Manager manager, ACR_SpoilsTeam team)
 {
  int back;
  string to = "its reserve";
  ACR_DefenderPost post = team.m_Post;
  if (post && !post.m_bGone && post.m_Group)
  {
   array<AIAgent> agents = {};
   team.m_Group.GetAgents(agents);
   foreach (AIAgent agent : agents)
   {
    if (!agent || !ACR_Respawn.IsAlive(agent.GetControlledEntity())) continue;
    team.m_Group.RemoveAgent(agent);
    post.m_Group.AddAgent(agent);
    back++;
   }
   manager.m_Bases.PackUpGuards(team.m_Group);
   to = "its group";
  }
  else if (post && !post.m_bGone) post.m_iReserve += manager.m_Bases.PackUpGuards(team.m_Group);
  else if (team.m_Base && team.m_Base.m_bPocket && team.m_Base.m_sOwner == ACR_Pockets.FACTION)
  {
   if (team.m_Base.m_GarrisonGroup) to = "its garrison";
   back = manager.m_Bases.TakeIntoReserve(team.m_Base, team.m_Group, ACR_Pockets.FACTION, team.m_sName + " spoils team");
  }
  else
  {
   back = manager.m_Bases.PackUpGuards(team.m_Group);
   to = "nobody (its post is gone)";
  }
  s_iRejoined++;
  PrintFormat("[ACR] SPOILS %1: the spoils team is back with %2 (%3 taken)", team.m_sName, to, team.m_iTaken);
 }

 // ---------------------------------------------------------------- taking

 // A body taken: its gear (to the hideout feeding the area), and a living man as a prisoner.
 static void Take(ACR_SpoilsTeam team, ChimeraCharacter man)
 {
  if (!man) return;
  s_aTaken.Insert(man);
  if (team) team.m_iTaken++;
  ACR_DefenderPost hideout = ACR_FIAIntel.SupplierOf(man.GetOrigin());
  Loot(man, hideout);
  if (OutOfFight(man)) ACR_FIAPrisoners.Capture(man, hideout);
 }

 // Weapons and magazines off a body (and weapons on the ground by it): gone from the field, arms
 // points to 'hideout' (none standing: they're only denied). Returns the points.
 static float Loot(ChimeraCharacter body, ACR_DefenderPost hideout)
 {
  float points;
  int weapons, magazines;
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(body.FindComponent(SCR_InventoryStorageManagerComponent));
  if (inventory)
  {
   array<IEntity> items = {};
   inventory.GetItems(items);
   foreach (IEntity item : items)
   {
    if (!item) continue;
    WeaponComponent weapon = WeaponComponent.Cast(item.FindComponent(WeaponComponent));
    if (weapon)
    {
     points += WeaponPoints(weapon);
     weapons++;
     Tally(hideout, item, weapon);
     inventory.TryDeleteItem(item);
     continue;
    }
    if (item.FindComponent(BaseMagazineComponent))
    {
     magazines++;
     inventory.TryDeleteItem(item);
    }
   }
  }
  IEntity loose = LooseWeapon(body.GetOrigin());
  while (loose)
  {
   WeaponComponent dropped = WeaponComponent.Cast(loose.FindComponent(WeaponComponent));
   if (dropped)
   {
    points += WeaponPoints(dropped);
    Tally(hideout, loose, dropped);
   }
   weapons++;
   SCR_EntityHelper.DeleteEntityAndChildren(loose);
   loose = LooseWeapon(body.GetOrigin());
  }
  points = Math.Min(points + magazines * MAGAZINE_POINTS, BODY_POINTS_MAX);
  if (weapons + magazines <= 0) return 0;
  s_iBodies++;
  s_iWeapons += weapons;
  s_fArms += points;
  string to = "nobody (no hideout in reach)";
  if (hideout && hideout.m_Cell && !hideout.m_bGone)
  {
   ACR_GuerrillaCell cell = hideout.m_Cell;
   cell.m_iArms += weapons;
   cell.m_fPool = Math.Min(ACR_FIAGrowth.PoolCap(), cell.m_fPool + points * ARMS_RECRUITS);
   to = string.Format("%1 (pool %2)", hideout.m_sName, cell.m_fPool);
  }
  PrintFormat("[ACR] SPOILS looted %1 weapons and %2 magazines at %3 (%4 arms points) for %5", weapons, magazines, ACR_Manager.Grid(body.GetOrigin()), points, to);
  return points;
 }

 // ---------------------------------------------------------------- looted arms in recruits' hands
 // (Jacob 19 Nov: a looted AK or M16 shows up in FIA hands.) The hideout keeps a tally of the rifle
 // and machine gun types its spoils teams took (TALLY_MAX, newest kept); when a cell it feeds (or the
 // hideout itself) deploys, up to half its men swap their rifle for one from the tally, with its own
 // magazines. Nothing looted: the normal kit. The swap is confirmed before the old gun goes (as the
 // squads' marksman swap); refused, he keeps his own. Gone with the match (the cells are).
 static const int TALLY_MAX = 12;
 static const float ARMED_SHARE = 0.5;
 static const int ARMED_MAGAZINES = 4;
 static int s_iArmed;                     // recruits who took a looted weapon this match
 static ref array<ref ACR_LootSwap> s_aSwaps; // swaps waiting to be confirmed (held here: the call queue doesn't keep them)

 protected static void Tally(ACR_DefenderPost hideout, IEntity item, WeaponComponent weapon)
 {
  if (!hideout || !hideout.m_Cell || !item || !item.GetPrefabData()) return;
  EWeaponType type = weapon.GetWeaponType();
  if (type != EWeaponType.WT_RIFLE && type != EWeaponType.WT_MACHINEGUN) return;
  array<ResourceName> tally = hideout.m_Cell.m_aLootedArms;
  tally.Insert(item.GetPrefabData().GetPrefabName());
  while (tally.Count() > TALLY_MAX) tally.RemoveOrdered(0);
 }

 // The hideout whose tally a cell's men draw on: itself, or the one feeding it.
 static ACR_DefenderPost ArsenalOf(ACR_DefenderPost post)
 {
  if (!post || !post.m_Cell) return null;
  if (post.m_Cell.m_bHideout) return post;
  return ACR_FIAIntel.SupplierOf(post.m_Cell.m_vKillZone);
 }

 // Just deployed (ACR_Guerrillas.Deployed): some of its men take looted arms.
 static void ArmRecruits(ACR_DefenderPost post)
 {
  if (!post) return;
  ArmGroup(post.m_Group, post.m_vPos, post.m_sName, ArsenalOf(post));
 }

 // Arm a freshly spawned FIA group (cell, hideout, pocket garrison, or mustered/bought band)
 // from its supplying hideout's looted-arms tally, scaling the share with how full the cache is
 // (1 weapon -> 1 fighter; full TALLY_MAX cache -> up to ARMED_SHARE of the group).
 static void ArmGroup(SCR_AIGroup group, vector at, string label, ACR_DefenderPost hideout = null)
 {
  if (!s_bOn || !group) return;
  if (!hideout) hideout = ACR_FIAIntel.SupplierOf(at);
  if (!hideout || !hideout.m_Cell || hideout.m_Cell.m_aLootedArms.IsEmpty()) return;
  array<ChimeraCharacter> men = {};
  Awake(group, men);
  if (men.IsEmpty()) return;
  int cached = hideout.m_Cell.m_aLootedArms.Count();
  float share = Math.Min(ARMED_SHARE, ARMED_SHARE * cached / TALLY_MAX);
  int most = Math.Max(1, (int)Math.Round(men.Count() * share));
  int asked;
  foreach (ChimeraCharacter man : men)
  {
   if (asked >= most) break;
   if (Arm(man, hideout.m_Cell.m_aLootedArms.GetRandomElement())) asked++;
  }
  if (asked > 0) PrintFormat("[ACR] SPOILS %1: %2 of %3 men take looted arms from %4 (%5/%6 in its tally)", label, asked, men.Count(), hideout.m_sName, cached, TALLY_MAX);
 }

 // Delayed arming for purchased/mustered foot bands whose members may finish spawning on a later frame.
 static void ArmGroupSoon(SCR_AIGroup group, vector at, string label, ACR_DefenderPost hideout = null)
 {
  if (!s_bOn || !group || !GetGame() || !GetGame().GetCallqueue()) return;
  GetGame().GetCallqueue().CallLater(ArmGroup, 1500, false, group, at, label, hideout);
 }

 // His rifle replaced by 'prefab' (confirmed by ACR_LootSwap a moment later). False: not started.
 static bool Arm(ChimeraCharacter man, ResourceName prefab)
 {
  IEntity original = ACR_SquadMarksmen.Primary(man);
  if (!original || man.IsInVehicle()) return false;
  if (original.GetPrefabData() && original.GetPrefabData().GetPrefabName() == prefab) return false;
  CharacterControllerComponent control = man.GetCharacterController();
  if (!control || control.IsChangingItem()) return false;
  if (control.GetCurrentItemInHands() && control.GetCurrentItemInHands() != original) return false;
  Resource resource = Resource.Load(prefab);
  if (!resource || !resource.IsValid()) return false;
  EntitySpawnParams parameters = new EntitySpawnParams();
  parameters.TransformMode = ETransformMode.WORLD;
  man.GetWorldTransform(parameters.Transform);
  IEntity replacement = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), parameters);
  if (!replacement) return false;
  InventoryItemComponent item = InventoryItemComponent.Cast(original.FindComponent(InventoryItemComponent));
  InventoryStorageSlot slot;
  if (item) slot = item.GetParentSlot();
  SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(man.FindComponent(SCR_InventoryStorageManagerComponent));
  bool accepted;
  if (slot && inventory)
  {
   if (control.GetCurrentItemInHands() == original)
   {
    control.ReplaceEquippedItem(replacement);
    accepted = true;
   }
   else accepted = inventory.TryReplaceItem(replacement, slot.GetStorage(), slot.GetID());
  }
  if (!accepted)
  {
   SCR_EntityHelper.DeleteEntityAndChildren(replacement);
   return false;
  }
  ACR_LootSwap swap = new ACR_LootSwap();
  swap.m_Soldier = man;
  swap.m_Old = original;
  swap.m_New = replacement;
  BaseWeaponComponent oldWeapon = BaseWeaponComponent.Cast(original.FindComponent(BaseWeaponComponent));
  array<BaseMuzzleComponent> oldMuzzles = {};
  if (oldWeapon) oldWeapon.GetMuzzlesList(oldMuzzles);
  foreach (BaseMuzzleComponent oldMuzzle : oldMuzzles)
   if (oldMuzzle.GetMagazineWell()) swap.m_OldWells.Insert(oldMuzzle.GetMagazineWell().Type());
  s_aSwaps.Insert(swap);
  GetGame().GetCallqueue().CallLater(swap.Confirm, 1500);
  return true;
 }

 static float WeaponPoints(WeaponComponent weapon)
 {
  EWeaponType type = weapon.GetWeaponType();
  if (type == EWeaponType.WT_MACHINEGUN || type == EWeaponType.WT_ROCKETLAUNCHER || type == EWeaponType.WT_GRENADELAUNCHER) return HEAVY_POINTS;
  if (type == EWeaponType.WT_HANDGUN) return 0.5;
  if (type == EWeaponType.WT_RIFLE || type == EWeaponType.WT_SNIPERRIFLE) return 1;
  return 0;
 }

 protected static bool CollectLoose(IEntity e)
 {
  if (e.GetParent() || !e.FindComponent(WeaponComponent) || s_aFound.Contains(e)) return true;
  // Not a mounted gun (a tripod or a vehicle's): those are vehicles to the engine.
  if (Vehicle.Cast(e) || e.FindComponent(BaseCompartmentManagerComponent)) return true;
  s_aFound.Insert(e);
  return true;
 }

 // A gun lying on the ground by 'pos' (null: none).
 protected static IEntity LooseWeapon(vector pos)
 {
  array<IEntity> kept = {};
  kept.InsertAll(s_aFound);
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(pos, LOOSE_REACH, CollectLoose, null, EQueryEntitiesFlags.DYNAMIC);
  IEntity found;
  if (!s_aFound.IsEmpty()) found = s_aFound[0];
  s_aFound.Clear();
  s_aFound.InsertAll(kept);
  return found;
 }

 protected static void PruneTaken()
 {
  for (int i = s_aTaken.Count() - 1; i >= 0; i--) if (!s_aTaken[i]) s_aTaken.Remove(i);
 }

 // A body taken outside a team's own step (a stolen vehicle's crew, tests).
 static void TakeBody(ChimeraCharacter man) { Take(null, man); }
}
