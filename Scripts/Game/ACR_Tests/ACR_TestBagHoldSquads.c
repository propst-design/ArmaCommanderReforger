// srv:bagholdsquads (about 2.5 min, Everon): who goes behind the bags. A town with all its works up and two US rifle
// squads: a defend order at bags, a garrison order at the same bags (no two men at one place), a move order (the
// places go), a garrison order on a house away from them (it stays a house), a squad told to hold the works, the
// point the AI commander sends a squad to, -acrBagHold 0, and bags shot away from under a place.
class ACR_TestBagHoldSquads
{
 static const ResourceName SQUAD = "{DDF3799FA1387848}Prefabs/Groups/BLUFOR/Group_US_RifleSquad.et";
 static const string ONE = "Bag Squad 1";
 static const string TWO = "Bag Squad 2";
 static const int CHECKS = 12;
 protected static ACR_Base s_Base;
 protected static ACR_BaseWork s_Work;
 protected static string s_sSide;
 protected static vector s_vPoint;
 protected static bool s_bDone;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 8) Setup(manager);
  if (!s_Base || s_bDone) return;
  if (second == 12) Squads(manager);
  if (second == 25) DefendAtBags(manager);
  // They walk to their posts, 150-195 m from the middle of a town: healthy walks took 85-115 s (4 Nov 2026), so two minutes.
  if (second == 145) OnPlaces(manager);
  if (second == 146) SecondSquad(manager);
  if (second == 148) MoveAway(manager);
  if (second == 150) House(manager);
  if (second == 156) Hold(manager);
  if (second == 158) Commander();
  if (second == 160) Off(manager);
  if (second == 162) BagsShotAway(manager);
  if (second == 166) Finish();
 }

 protected static void Setup(ACR_Manager manager)
 {
  s_sSide = ACR_FactionSelection.West();
  string east = ACR_FactionSelection.East();
  int most = -1;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired || base.m_bHidden || base.m_bBuilt || base.m_sOwner == east) continue;
   array<IEntity> buildings = {};
   ACR_BaseDefences.Buildings(base.GetOrigin(), ACR_BaseDefences.SearchRadius(base), buildings);
   if (buildings.Count() <= most) continue;
   most = buildings.Count();
   s_Base = base;
  }
  if (!s_Base)
  {
   ACR_TestRegress.Abort("bagholdsquads", "no town");
   return;
  }
  s_Base.m_sOwner = s_sSide;
  int built = ACR_BaseWorks.BuildAllForTest(s_Base, s_sSide);
  s_Work = ACR_BaseWorks.Find(s_Base);
  ACR_BaseWorks.TiersDoneForTest(s_Work);
  PrintFormat("[ACR] BAGHOLDSQUADS %1: %2 sites, %3 lines", s_Base.m_sName, built, s_Work.m_aLines.Count());
 }

 protected static void Squads(ACR_Manager manager)
 {
  vector at = ACR_Manager.Ground(s_Base.GetOrigin() + Vector(10, 0, 10));
  manager.m_sSpawningFaction = s_sSide;
  manager.SpawnUnit(ONE, SQUAD, "", false, "", at, at);
  vector beside = ACR_Manager.Ground(at + Vector(12, 0, 0));
  manager.SpawnUnit(TWO, SQUAD, "", false, "", beside, beside);
  manager.m_sSpawningFaction = "";
 }

 protected static ACR_Squad Find(ACR_Manager manager, string name)
 {
  foreach (ACR_Squad squad : manager.m_Squads) if (squad.GetName() == name) return squad;
  return null;
 }

 protected static void DefendAtBags(ACR_Manager manager)
 {
  bool found = ACR_BaseWorks.LinePoint(s_Base.GetOrigin(), s_sSide, s_vPoint);
  ACR_TestRegress.Check("bagholdsquads: the works give a point to defend", found, string.Format("%1", s_vPoint));
  ACR_Squad squad = Find(manager, ONE);
  if (!found || !squad) return;
  squad.Defend(s_vPoint);
  ACR_TestRegress.Check("bagholdsquads: a defend order at bags takes places behind them", ACR_BagHold.HasPlaces(squad.m_Group), string.Format("%1 places", ACR_BagHold.PlaceCount()));
  ACR_TestRegress.Check("bagholdsquads: it's a garrison waypoint, its label Defending", squad.m_sOrderPrefab == ACR_Squad.WP_GARRISON && squad.OrderState() == ACR_EOrderState.DEFEND, squad.m_sOrderPrefab);
 }

 protected static void OnPlaces(ACR_Manager manager)
 {
  ACR_Squad squad = Find(manager, ONE);
  if (!squad) return;
  array<IEntity> posts = {};
  ACR_Garrison.PostEntities(squad.m_Group, posts);
  array<IEntity> men = {};
  ACR_BagSampler.Alive(squad.m_Group, men);
  int on;
  foreach (IEntity man : men)
  {
   foreach (IEntity post : posts)
   {
    if (post && vector.DistanceXZ(post.GetOrigin(), man.GetOrigin()) < ACR_BagSampler.ON_POST * 1.5)
    {
     on++;
     break;
    }
   }
  }
  // Half its places taken: a short line has fewer places than men (5 for 9 by the roadblock, 4 Nov 2026).
  ACR_TestRegress.Check("bagholdsquads: the squad walks to its places", Math.Min(men.Count(), posts.Count()) <= on * 2, string.Format("%1 of %2 men, %3 places", on, men.Count(), posts.Count()));
 }

 protected static void SecondSquad(ACR_Manager manager)
 {
  ACR_Squad one = Find(manager, ONE);
  ACR_Squad two = Find(manager, TWO);
  if (!one || !two) return;
  two.Garrison(s_vPoint);
  // The line may be full by now: then it does what it would with no bags there, it isn't left without an order.
  ACR_TestRegress.Check("bagholdsquads: a garrison order at the same bags takes what's left of them", ACR_BagHold.HasPlaces(two.m_Group) || two.m_Group.GetCurrentWaypoint() != null, string.Format("places %1", ACR_BagHold.HasPlaces(two.m_Group)));
  array<IEntity> ones = {};
  array<IEntity> twos = {};
  ACR_Garrison.PostEntities(one.m_Group, ones);
  ACR_Garrison.PostEntities(two.m_Group, twos);
  int clashes;
  foreach (IEntity a : ones)
   foreach (IEntity b : twos)
    if (a && b && vector.DistanceXZ(a.GetOrigin(), b.GetOrigin()) < 1.5) clashes++;
  ACR_TestRegress.Check("bagholdsquads: no two men at one place", clashes == 0, string.Format("%1 and %2 places, %3 clashes", ones.Count(), twos.Count(), clashes));
 }

 protected static void MoveAway(ACR_Manager manager)
 {
  ACR_Squad squad = Find(manager, TWO);
  if (!squad) return;
  squad.MoveTo(ACR_Manager.Ground(s_Base.GetOrigin()));
  ACR_TestRegress.Check("bagholdsquads: a move order gives up the places", !ACR_BagHold.HasPlaces(squad.m_Group) && !ACR_Garrison.HasPosts(squad.m_Group), "");
 }

 protected static void House(ACR_Manager manager)
 {
  ACR_Squad squad = Find(manager, TWO);
  if (!squad) return;
  array<IEntity> buildings = {};
  ACR_Garrison.BuildingsWithin(s_Base.GetOrigin(), 60, buildings);
  IEntity house;
  foreach (IEntity building : buildings)
  {
   if (vector.DistanceXZ(building.GetOrigin(), s_vPoint) < 25) continue;
   house = building;
   break;
  }
  if (!house)
  {
   ACR_TestRegress.Check("bagholdsquads: a house to garrison", false, "");
   return;
  }
  squad.Garrison(house.GetOrigin());
  ACR_TestRegress.Check("bagholdsquads: a garrison order on a house stays a house", !ACR_BagHold.HasPlaces(squad.m_Group), string.Format("posts %1", ACR_Garrison.HasPosts(squad.m_Group)));
 }

 protected static void Hold(ACR_Manager manager)
 {
  ACR_Squad squad = Find(manager, ONE);
  if (!squad) return;
  squad.MoveTo(ACR_Manager.Ground(s_Base.GetOrigin()));
  squad.HoldBaseWorks(s_Work);
  ACR_TestRegress.Check("bagholdsquads: a squad told to hold the works goes behind their bags", ACR_BagHold.HasPlaces(squad.m_Group), string.Format("%1 places", ACR_BagHold.PlaceCount()));
 }

 protected static void Commander()
 {
  vector at;
  bool found = ACR_BaseWorks.LinePoint(s_Base.GetOrigin() + Vector(30, 0, 30), s_sSide, at);
  string enemy = ACR_FactionSelection.East();
  vector none;
  bool theirs = ACR_BaseWorks.LinePoint(s_Base.GetOrigin(), enemy, none);
  ACR_TestRegress.Check("bagholdsquads: the AI commander's point is its own side's works only", found && !theirs, "");
 }

 protected static void Off(ACR_Manager manager)
 {
  ACR_Squad squad = Find(manager, TWO);
  if (!squad) return;
  ACR_Options.Set("acrBagHold", "0");
  vector at;
  bool point = ACR_BaseWorks.LinePoint(s_Base.GetOrigin(), s_sSide, at);
  squad.Defend(s_vPoint);
  bool places = ACR_BagHold.HasPlaces(squad.m_Group);
  ACR_Options.Unset("acrBagHold");
  ACR_TestRegress.Check("bagholdsquads: -acrBagHold 0 leaves defend orders plain", !point && !places, "");
 }

 protected static void BagsShotAway(ACR_Manager manager)
 {
  ACR_Squad squad = Find(manager, ONE);
  if (!squad) return;
  IEntity bags = ACR_BagHold.BagsForTest(squad.m_Group);
  int before = ACR_BagHold.PlaceCount();
  int lost = ACR_BagHold.s_iLost;
  if (bags) SCR_EntityHelper.DeleteEntityAndChildren(bags);
  GetGame().GetCallqueue().CallLater(CheckShotAway, 2500, false, before, lost);
 }

 protected static void CheckShotAway(int before, int lost)
 {
  ACR_TestRegress.Check("bagholdsquads: a place goes with its bags", ACR_BagHold.PlaceCount() < before && ACR_BagHold.s_iLost > lost, string.Format("%1 places now, %2 before", ACR_BagHold.PlaceCount(), before));
 }

 protected static void Finish()
 {
  s_bDone = true;
  ACR_TestRegress.Complete("bagholdsquads", CHECKS);
 }
}
