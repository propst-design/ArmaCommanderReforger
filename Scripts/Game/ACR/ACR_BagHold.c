// Men behind bags (the user, 1 Nov 2026: what's the point of the works if the AI won't use them). ACR_Garrison's posts
// bring a man to his place, but once a fight starts the game's combat movement has him: it only ever lowers his
// stance, so behind the bags he crouches or lies down and sees nothing, and it moves him on towards the enemy a few
// seconds later. The game has a whole way of fighting from cover, though: standing to fire and crouching to hide by the
// cover's height, hiding to reload, hide-and-peek when pinned. So each man gets a cover of his own that no navmesh
// cover point stands behind (SCR_AICoverLock.m_bACRBag). The combat move nodes at the end of this file keep him on his
// place while the enemy is in front of the bags and send him back when he's off it (a grenade, a step back); an
// enemy behind him, or on top of him, is the game's business. How long he shows and hides stays the game's own
// random timing. -acrBagHold 0: as before.
class ACR_BagPlace : Managed
{
 vector m_vPos;       // where he stands
 vector m_vFacing;    // the bags' firing side
 float m_fHeight;     // the bags' top over his feet
 IEntity m_Bags;      // they go, so does the place
 IEntity m_Post;      // its post (ACR_Garrison)
 SCR_AIUtilityComponent m_Man;
 int m_iFails;        // spells of going there that got him no nearer
 float m_fCheckedAt;
 float m_fCheckedGap;
 float m_fSentUntil;  // his last try at getting there, if it was the last thing he did
 float m_fBarrelUntil; // MayFire's last look down his barrel, good till then
 bool m_bBarrelClear;
}

class ACR_BagHold
{
 static const float ARRIVE = 1.6;        // m from his place: he's on it
 // Of his facing towards the enemy: less, and the bags are no use to him, the game takes over. Holding on past
 // square-on (standing, or low along the line) cost 35 men of 56 for 13 attackers of 84 in srv:worksfight; at 0, 18 of 32 for 23 of 48.
 static const float FRONT = 0;
 static const float NO_RUN_GAP = 8;      // m from his place he doesn't run for it ...
 static const float NO_RUN_ENEMY = 15;   // ... with an enemy this close
 static const float HAND_TO_HAND = 6;    // m: an enemy this close is fought from wherever he stands
 static const int MOST_FAILS = 3;        // of PROGRESS_MS with no headway towards his place: he gives it up
 static const float PROGRESS_MS = 6000;
 static const float PROGRESS_M = 1;
 static const float HEIGHT_DEFAULT = 1.3; // m, the long high piece's real top; its bounds say 1.46
 static const float BARREL_MS = 200;
 static const float BARREL_LOOK = 3;     // m along his barrel
 static const vector MESH_LOOK = "1.5 1.5 1.5"; // round his place, for the nearest navmesh to it
 // Nothing to shoot at: down, and up now and then for a look (srv:worksfight: on his feet all the while, a man was
 // shot from 150 m by attackers lying in the grass he never saw). Up for good with someone in his sights or close.
 static const float SEEN_S = 4;          // s since he saw his target: he has someone to shoot
 static const float WATCH_NEAR = 160;    // m: an enemy this close, he stays up. 40 before: he ducked for 3-6 s whenever he hadn't seen
                                         // his target for 4 s, and down behind high bags he sees nothing, so he never saw it again
                                         // (srv:worksfightbase, 2-3 Nov: the lost fights were the ones 'blind behind bags' 54-82%)
 static const float LOOK_MIN_S = 2.5;    // how long he looks
 static const float LOOK_MAX_S = 4;
 static const float DOWN_MIN_S = 3;      // how long he stays down
 static const float DOWN_MAX_S = 6;
 protected static ref map<SCR_AIGroup, ref array<ref ACR_BagPlace>> s_Places;
 static void ACR_InitStatics()
 {
  s_Places = new map<SCR_AIGroup, ref array<ref ACR_BagPlace>>();
 }
 static int s_iEntered, s_iSentBack, s_iHolds, s_iLost, s_iMoveFails; // tests

 static bool On()
 {
  return ACR_LogisticsRules.Choice("acrBagHold", "1") != "0";
 }

 static bool HasPlaces(SCR_AIGroup group)
 {
  return group && !s_Places.IsEmpty() && s_Places.Contains(group);
 }

 static IEntity BagsForTest(SCR_AIGroup group)
 {
  array<ref ACR_BagPlace> places = s_Places.Get(group);
  if (!places || places.IsEmpty()) return null;
  return places[0].m_Bags;
 }

 static int PlaceCount()
 {
  int n;
  foreach (SCR_AIGroup group, array<ref ACR_BagPlace> places : s_Places) n += places.Count();
  return n;
 }

 // A man's place behind a piece of bags, for ACR_Garrison.PostsAt.
 static ACR_GarrisonSpot SpotBehind(vector piece, vector facing, ResourceName prefab, IEntity bags)
 {
  ACR_GarrisonSpot spot = new ACR_GarrisonSpot();
  spot.m_Pos = piece - facing * ACR_BaseWorks.MAN_BEHIND;
  // A net hangs down behind the bags: he stands clear of it.
  if (ACR_BaseDefences.HalfSize(prefab)[2] > 1) spot.m_Pos = spot.m_Pos - facing * 0.5;
  spot.m_Pos[1] = ACR_BaseLiner.Ground(spot.m_Pos);
  spot.m_Look = facing;
  spot.m_bStand = true;
  spot.m_Bags = bags;
  return spot;
 }

 // A post has just been put up for 'group' (ACR_Garrison.PostsAt): behind bags, it's a place to hold.
 static void AddPlace(SCR_AIGroup group, IEntity post, ACR_GarrisonSpot spot)
 {
  if (!group || !post || !spot.m_Bags || !On()) return;
  array<ref ACR_BagPlace> places = s_Places.Get(group);
  if (!places)
  {
   places = {};
   s_Places.Set(group, places);
  }
  ACR_BagPlace place = new ACR_BagPlace();
  place.m_vPos = spot.m_Pos;
  place.m_vFacing = spot.m_Look;
  place.m_Bags = spot.m_Bags;
  place.m_Post = post;
  place.m_fHeight = TopOver(spot.m_Bags, spot.m_Pos);
  places.Insert(place);
 }

 // The bags' top over the ground at his feet, looked at from above.
 protected static float TopOver(IEntity bags, vector feet)
 {
  vector middle = bags.GetOrigin();
  TraceParam trace = new TraceParam();
  trace.Start = middle + Vector(0, 3, 0);
  trace.End = middle - Vector(0, 1, 0);
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  float fraction = GetGame().GetWorld().TraceMove(trace, ACR_Garrison.SolidFilter);
  float height = trace.Start[1] - 4 * fraction - feet[1];
  if (fraction >= 1 || height < 0.3 || height > 2.2) return HEIGHT_DEFAULT;
  return height;
 }

 // The group's posts went (ACR_Garrison.Release): nobody holds them any more.
 static void Forget(SCR_AIGroup group)
 {
  if (!group || s_Places.IsEmpty()) return;
  array<ref ACR_BagPlace> places = s_Places.Get(group);
  if (!places) return;
  foreach (ACR_BagPlace place : places) Free(place);
  s_Places.Remove(group);
 }

 protected static void Free(ACR_BagPlace place)
 {
  SCR_AIUtilityComponent man = place.m_Man;
  place.m_Man = null;
  if (!man) return;
  man.m_ACRBagPlace = null;
  if (!man.m_CombatMoveState) return;
  SCR_AICoverLock lock = man.m_CombatMoveState.GetAssignedCover();
  if (lock && lock.m_bACRBag) man.m_CombatMoveState.ReleaseCover();
 }

 protected static void Take(SCR_AIUtilityComponent man, ACR_BagPlace place)
 {
  if (man.m_ACRBagPlace == place) return;
  if (man.m_ACRBagPlace) man.m_ACRBagPlace.m_Man = null;
  if (place.m_Man) Free(place);
  man.m_ACRBagPlace = place;
  place.m_Man = man;
  place.m_iFails = 0;
  place.m_fCheckedAt = 0;
 }

 // Every second from ACR_Garrison.Sweep: bags gone, men gone, and each man on the place the game sent him to.
 static void Sweep()
 {
  for (int g = s_Places.Count() - 1; g >= 0; g--)
  {
   SCR_AIGroup group = s_Places.GetKey(g);
   array<ref ACR_BagPlace> places = s_Places.GetElement(g);
   if (!group)
   {
    foreach (ACR_BagPlace orphan : places) Free(orphan);
    s_Places.RemoveElement(g);
    continue;
   }
   for (int p = places.Count() - 1; p >= 0; p--)
   {
    ACR_BagPlace place = places[p];
    if (!place.m_Bags || place.m_Bags.IsDeleted())
    {
     Free(place);
     places.RemoveOrdered(p);
     s_iLost++;
     continue;
    }
    if (place.m_Man && !ACR_Respawn.IsAlive(place.m_Man.m_OwnerEntity)) Free(place);
    if (place.m_Man) BackOnWatch(place);
    if (!place.m_Post) continue;
    AISmartActionComponent action = AISmartActionComponent.Cast(place.m_Post.FindComponent(AISmartActionComponent));
    if (!action) continue;
    AIAgent user = action.GetUser();
    if (!user) continue;
    SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(user.FindComponent(SCR_AIUtilityComponent));
    if (utility && utility != place.m_Man) Take(utility, place);
   }
  }
 }

 // The fight over, he's back at his post in whatever stance it left him: crouched behind the bags he'd never
 // see the next one coming.
 protected static void BackOnWatch(ACR_BagPlace place)
 {
  SCR_AIUtilityComponent man = place.m_Man;
  if (!SCR_AIPerformActionBehavior.Cast(man.GetCurrentBehavior())) return;
  ChimeraCharacter body = ChimeraCharacter.Cast(man.m_OwnerEntity);
  if (!body || !body.GetCharacterController()) return;
  if (body.GetCharacterController().GetStance() != ECharacterStance.STAND) SCR_AIStanceHandling.SetStance(body.GetCharacterController(), ECharacterStance.STAND);
 }

 // His place, or the nearest free one of his group's now he's in a fight and the game gave him none.
 protected static ACR_BagPlace PlaceFor(SCR_AIUtilityComponent utility, vector here)
 {
  if (utility.m_ACRBagPlace) return utility.m_ACRBagPlace;
  if (!utility.GetAIAgent()) return null;
  SCR_AIGroup group = SCR_AIGroup.Cast(utility.GetAIAgent().GetParentGroup());
  if (!group) return null;
  array<ref ACR_BagPlace> places = s_Places.Get(group);
  if (!places) return null;
  ACR_BagPlace best;
  float nearest = float.MAX;
  foreach (ACR_BagPlace place : places)
  {
   if (place.m_Man || place.m_iFails >= MOST_FAILS) continue;
   float d = vector.DistanceXZ(place.m_vPos, here);
   if (d < nearest)
   {
    nearest = d;
    best = place;
   }
  }
  if (best) Take(utility, best);
  return best;
 }

 // From the combat move nodes, before the game moves a man on. True: he has a place and the fight is one for it,
 // so the game leaves him be (he's on it, or on his way back). 'target' is where the enemy is; 'distance' how far;
 // 'sinceSeen' how long since he saw him (below 0: never).
 static bool Holds(SCR_AIUtilityComponent utility, SCR_AICombatMoveState state, vector target, float distance, float sinceSeen = 0)
 {
  if (s_Places.IsEmpty() || !utility || !state || !utility.m_OwnerEntity) return false;
  // On a gun (the works' nest, from the bags beside it): the game runs the gun, his place waits.
  ChimeraCharacter rider = ChimeraCharacter.Cast(utility.m_OwnerEntity);
  if (rider && rider.IsInVehicle()) return false;
  vector here = utility.m_OwnerEntity.GetOrigin();
  ACR_BagPlace place = PlaceFor(utility, here);
  if (!place) return false;
  SCR_AIGroup group = SCR_AIGroup.Cast(utility.GetAIAgent().GetParentGroup());
  if (!group || BBB.HasPlayer(group) || BBB_Morale.IsWithdrawing(group) || utility.m_BBBFireJob) return false;
  vector toward = target - place.m_vPos;
  toward[1] = 0;
  float faces = 1;
  if (toward.Length() > 0.1) faces = vector.Dot(toward.Normalized(), place.m_vFacing);
  if (faces < FRONT)
  {
   // Past the bags: they're no cover from there.
   SCR_AICoverLock held = state.GetAssignedCover();
   if (held && held.m_bACRBag && !state.IsExecutingRequest()) state.ReleaseCover();
   return false;
  }
  float gap = vector.DistanceXZ(here, place.m_vPos);
  if (gap <= ARRIVE)
  {
   EnterCover(utility, state, place, here);
   KeepShowing(utility, state, place.m_fHeight);
   if (distance >= WATCH_NEAR && (sinceSeen < 0 || sinceSeen >= SEEN_S)) LowWatch(state);
   else state.m_fACRDownUntil = 0;
   s_iHolds++;
   return true;
  }
  if (distance < HAND_TO_HAND || (gap > NO_RUN_GAP && distance < NO_RUN_ENEMY)) return false;
  if (state.IsExecutingRequest()) return true;
  SendBack(utility, state, place, target, gap);
  return true;
 }

 // Vanilla sets these when a man reaches a cover it found him. Hidden first: the node then stands him up, which
 // is what makes the stance right whatever he was doing (he may be lying down).
 protected static void EnterCover(SCR_AIUtilityComponent utility, SCR_AICombatMoveState state, ACR_BagPlace place, vector here)
 {
  SCR_AICoverLock lock = state.GetAssignedCover();
  if (state.m_bInCover && lock && lock.m_bACRBag) return;
  if (state.IsMoving()) return;
  lock = new SCR_AICoverLock(0, 0, 0, vector.Zero, vector.Zero);
  lock.m_bACRBag = true;
  lock.m_vCoverPos = here;
  lock.m_vCoverTallestPos = here + place.m_vFacing * 0.6 + Vector(0, place.m_fHeight, 0);
  lock.m_vCoverDirXZ = place.m_vFacing;
  state.AssignCover(lock);
  state.m_bInCover = true;
  state.m_bExposedInCover = false;
  s_iEntered++;
 }

 // The stance he shows himself in over these bags, as the game's SCR_AIGetCoverParameters works it out.
 protected static ECharacterStance ShowStance(float height)
 {
  if (height > 0.85) return ECharacterStance.STAND;
  return ECharacterStance.CROUCH;
 }

 // Nothing to shoot at just now: once he's had his look, down for a while. The game stands a man up as soon as it finds
 // him hidden, so SCR_AICombatMoveState holds that off until he's due up again.
 protected static void LowWatch(SCR_AICombatMoveState state)
 {
  if (!state.m_bExposedInCover || state.IsExecutingRequest()) return;
  if (state.m_fTimerRequest_s < state.m_fACRLookS) return;
  state.m_fACRLookS = Math.RandomFloat(LOOK_MIN_S, LOOK_MAX_S);
  state.m_fACRDownUntil = GetGame().GetWorld().GetWorldTime() + Math.RandomFloat(DOWN_MIN_S, DOWN_MAX_S) * 1000;
  state.ApplyRequestChangeStanceInCover(false);
 }

 // The game thinks he's showing, but something else (his post, a stance order while he was elsewhere) has him lower:
 // it only sets a stance when that flag changes, so he'd stay blind.
 protected static void KeepShowing(SCR_AIUtilityComponent utility, SCR_AICombatMoveState state, float height)
 {
  if (!state.m_bExposedInCover || state.IsExecutingRequest()) return;
  ChimeraCharacter body = ChimeraCharacter.Cast(utility.m_OwnerEntity);
  if (!body || !body.GetCharacterController()) return;
  if (body.GetCharacterController().GetStance() != ShowStance(height)) state.ApplyRequestChangeStanceInCover(true);
 }

 protected static void SendBack(SCR_AIUtilityComponent utility, SCR_AICombatMoveState state, ACR_BagPlace place, vector target, float gap)
 {
  // Not getting any nearer (bags, a house, a fence in the way): another place, or none.
  float now = GetGame().GetWorld().GetWorldTime();
  // Headway counts only while he keeps at it: not from before the game had him somewhere else.
  if (place.m_fCheckedAt == 0 || now > place.m_fSentUntil)
  {
   place.m_fCheckedAt = now;
   place.m_fCheckedGap = gap;
  }
  else if (now - place.m_fCheckedAt > PROGRESS_MS)
  {
   if (gap > place.m_fCheckedGap - PROGRESS_M) place.m_iFails++;
   else place.m_iFails = 0;
   place.m_fCheckedAt = now;
   place.m_fCheckedGap = gap;
   if (place.m_iFails >= MOST_FAILS)
   {
    PrintFormat("[ACR] BAGHOLD %1 m short of a place, giving it up", gap.ToString(-1, 1));
    Free(place);
    return;
   }
  }
  SCR_AICombatMoveRequest_Move rq = new SCR_AICombatMoveRequest_Move();
  rq.m_eReason = SCR_EAICombatMoveReason.STANDARD;
  rq.m_vTargetPos = target;
  // A place hard up against the bags can be off the navmesh, and a move to it fails wherever he starts from (139 of
  // 290 in one srv:bagholdlow): the nearest point on it.
  rq.m_vMovePos = place.m_vPos;
  AIAgent agent = utility.GetAIAgent();
  AIPathfindingComponent pathing;
  if (agent) pathing = AIPathfindingComponent.Cast(agent.FindComponent(AIPathfindingComponent));
  vector onMesh;
  if (pathing && pathing.GetClosestPositionOnNavmesh(place.m_vPos, MESH_LOOK, onMesh)) rq.m_vMovePos = onMesh;
  rq.m_eDirection = SCR_EAICombatMoveDirection.CUSTOM_POS;
  rq.m_bTryFindCover = false;
  rq.m_bFailIfNoCover = false;
  rq.m_eStanceMoving = ECharacterStance.STAND;
  rq.m_eStanceEnd = ECharacterStance.CROUCH;
  rq.m_eMovementType = EMovementType.RUN;
  float speed = SCR_AICombatMoveUtils.CHARACTER_SPEED_STAND_RUN;
  if (gap > NO_RUN_GAP)
  {
   rq.m_eMovementType = EMovementType.SPRINT;
   speed = SCR_AICombatMoveUtils.CHARACTER_SPEED_STAND_SPRINT;
  }
  rq.m_fMoveDuration_s = gap / speed + 2;
  place.m_fSentUntil = now + (rq.m_fMoveDuration_s + 2) * 1000;
  rq.GetOnFailed().Insert(MoveFailed);
  rq.m_bAimAtTarget = SCR_AICombatMoveUtils.IsAimingAndMovementPossible(rq.m_eStanceMoving, rq.m_eMovementType);
  rq.m_bAimAtTargetEnd = true;
  state.ApplyNewRequest(rq);
  s_iSentBack++;
 }

 protected static void MoveFailed(SCR_AIUtilityComponent utility, SCR_AICombatMoveRequestBase request, SCR_EAICombatMoveRequestFailReason reason)
 {
  s_iMoveFails++;
 }

 // Down behind his bags, or not yet up over them, his shots can go into them (172 rounds of 434 in one srv:worksfight).
 // Asked by the game's fire nodes as they fire, so looked at only so often. Only the barrel decides: stopping every
 // man who's down from firing cut the clear shots to a quarter and the kills by half (covering fire over the top).
 static bool MayFire(SCR_AIUtilityComponent utility)
 {
  if (!utility || !utility.m_ACRBagPlace || !utility.m_CombatMoveState) return true;
  SCR_AICombatMoveState state = utility.m_CombatMoveState;
  SCR_AICoverLock lock = state.GetAssignedCover();
  if (!state.m_bInCover || !lock || !lock.m_bACRBag) return true;
  ChimeraCharacter body = ChimeraCharacter.Cast(utility.m_OwnerEntity);
  if (!body || !body.GetCharacterController()) return true;
  if (state.m_bExposedInCover && body.GetCharacterController().GetStance() <= ShowStance(utility.m_ACRBagPlace.m_fHeight)) return true;
  ACR_BagPlace place = utility.m_ACRBagPlace;
  float now = GetGame().GetWorld().GetWorldTime();
  if (now < place.m_fBarrelUntil) return place.m_bBarrelClear;
  place.m_fBarrelUntil = now + BARREL_MS;
  place.m_bBarrelClear = true;
  BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(body.FindComponent(BaseWeaponManagerComponent));
  vector mat[4];
  if (!weapons || !weapons.GetCurrentMuzzleTransform(mat)) return true;
  TraceParam trace = new TraceParam();
  trace.Start = mat[3];
  trace.End = mat[3] + mat[2] * BARREL_LOOK;
  trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
  place.m_bBarrelClear = GetGame().GetWorld().TraceMove(trace, ACR_Garrison.SolidFilter) >= 1;
  return place.m_bBarrelClear;
 }

 // Shot at with nothing to shoot back at: pinned, hide-and-peek as SuppressedInCoverLogic does it (the game's own
 // times); not pinned, up, or he'd never see who it was.
 static void Peek(SCR_AIUtilityComponent utility, SCR_AICombatMoveState state)
 {
  if (!state.IsInValidCover() || state.IsExecutingRequest()) return;
  bool exposed = state.m_bExposedInCover;
  if (!utility.m_ThreatSystem || utility.m_ThreatSystem.GetState() != EAIThreatState.THREATENED)
  {
   if (exposed) LowWatch(state);
   else state.ApplyRequestChangeStanceInCover(true);
   return;
  }
  float wait = 3.0;
  if (exposed) wait = 1.7;
  SCR_AICombatMoveRequest_ChangeStanceInCover last = SCR_AICombatMoveRequest_ChangeStanceInCover.Cast(state.GetRequest());
  if (last && last.m_f_UserTimer_s > 0) wait = last.m_f_UserTimer_s;
  if (state.m_fTimerRequest_s <= wait) return;
  SCR_AICombatMoveRequest_ChangeStanceInCover rq = new SCR_AICombatMoveRequest_ChangeStanceInCover();
  rq.m_bExposedInCover = !exposed;
  rq.m_bAimAtTarget = true;
  rq.m_bAimAtTargetEnd = true;
  rq.m_eReason = SCR_EAICombatMoveReason.SUPPRESSED_IN_COVER;
  if (exposed) rq.m_f_UserTimer_s = Math.RandomFloat(3.5, 7.0);
  else rq.m_f_UserTimer_s = Math.RandomFloat(1.7, 2.5);
  state.ApplyNewRequest(rq);
 }

}

modded class SCR_AIUtilityComponent
{
 ACR_BagPlace m_ACRBagPlace;
}

// A cover of a man's own: no navmesh cover point behind it to mark taken or free, and the bags cover the whole of
// their front, not the 60 degrees either side the game gives a cover point before it calls it useless.
modded class SCR_AICoverLock
{
 bool m_bACRBag;

 override void Release()
 {
  if (m_bACRBag)
  {
   m_vCoverPos = vector.Zero;
   return;
  }
  super.Release();
 }

 override float CosAngleToThreat(vector threatPos)
 {
  float cosAngle = super.CosAngleToThreat(threatPos);
  if (m_bACRBag && cosAngle >= ACR_BagHold.FRONT) return 1;
  return cosAngle;
 }
}

modded class SCR_AICombatMoveState
{
 float m_fACRDownUntil;     // world time: he stays down behind his bags till then (ACR_BagHold.LowWatch)
 float m_fACRLookS = 3;     // how long his next look lasts

 override void ApplyRequestChangeStanceInCover(bool exposed)
 {
  if (exposed && m_CoverLock && m_CoverLock.m_bACRBag && GetGame().GetWorld().GetWorldTime() < m_fACRDownUntil) return;
  super.ApplyRequestChangeStanceInCover(exposed);
 }

 // The game's stance handling outside cover also runs for a man in cover when he's being shot at from far off.
 // He doesn't lie down behind his bags, where he can't see: he hides by his cover's own stance, or shows.
 override void ApplyRequestChangeStanceOutsideCover(ECharacterStance stance)
 {
  if (m_bInCover && m_CoverLock && m_CoverLock.m_bACRBag)
  {
   bool up = stance == ECharacterStance.STAND;
   if (m_bExposedInCover != up) ApplyRequestChangeStanceInCover(up);
   return;
  }
  super.ApplyRequestChangeStanceOutsideCover(stance);
 }
}

// The hold sits on the game's "move to the next position" decision only: danger, grenades, friendly fire, a cover
// that has become useless and reloading are decided before it.
modded class SCR_AICombatMoveLogic_Attack
{
 protected override bool MoveToNextPosCondition()
 {
  if (m_Target && ACR_BagHold.Holds(m_Utility, m_State, m_Target.GetLastSeenPosition(), m_fTargetDist, m_Target.GetTimeSinceSeen())) return false;
  return super.MoveToNextPosCondition();
 }
}

modded class SCR_AICombatMoveLogic_Suppressive
{
 protected override bool MoveToNextPosCondition()
 {
  if (m_SuppressionVolume && ACR_BagHold.Holds(m_Utility, m_State, m_SuppressionVolume.GetCenterPosition(), m_fTargetDist)) return false;
  return super.MoveToNextPosCondition();
 }
}

// Shot at from far off with no target: the game sprints him up to 25 m in any direction to find cover. He has it.
modded class SCR_AICombatMoveLogic_HideFromThreatSystem
{
 override void Update()
 {
  if (m_iCurrentSector != -1 && m_MyEntity && m_Utility.m_SectorThreatFilter.IsSectorActive(m_iCurrentSector) && !m_bPushedRequest)
  {
   vector threat = m_Utility.m_SectorThreatFilter.GetSectorPos(m_iCurrentSector);
   if (ACR_BagHold.Holds(m_Utility, m_State, threat, vector.Distance(m_MyEntity.GetOrigin(), threat)))
   {
    if (!m_bReachedSafety)
    {
     m_bReachedSafety = true;
     m_ParentBehavior.OnMovementCompleted(m_State.IsInValidCover());
    }
    ACR_BagHold.Peek(m_Utility, m_State);
    return;
   }
  }
  super.Update();
 }
}

modded class SCR_AICombatMoveLogic_HideFromUnknownFire
{
 override void CombatMoveLogic(vector threatPos, float distToThreat)
 {
  if (m_State && ACR_BagHold.Holds(m_Utility, m_State, threatPos, distToThreat))
  {
   ACR_BagHold.Peek(m_Utility, m_State);
   return;
  }
  super.CombatMoveLogic(threatPos, distToThreat);
 }
}

modded class SCR_AIDecoWeaponUnobstructed
{
 protected SCR_AIUtilityComponent m_ACRUtility;

 override void OnInit(AIAgent owner)
 {
  super.OnInit(owner);
  m_ACRUtility = SCR_AIUtilityComponent.Cast(owner.FindComponent(SCR_AIUtilityComponent));
 }

 override bool TestFunction(AIAgent owner)
 {
  return super.TestFunction(owner) && ACR_BagHold.MayFire(m_ACRUtility);
 }
}

// A man with a place doesn't go off to look at where the shots came from: the group's investigate orders are for
// the men without one.
modded class SCR_AIMoveAndInvestigateBehavior
{
 override float CustomEvaluate()
 {
  if (m_Utility && m_Utility.m_ACRBagPlace)
  {
   Fail();
   return 0;
  }
  return super.CustomEvaluate();
 }
}
