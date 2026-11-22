// srv:fiagrievance (Everon whole map, about 40 s): FIA grievance (ACR_FIAGrievance, Jacob 21 Nov).
// Checks, with leaning off: doing nothing raises no hideout's grievance or recruiting, and harm far
// from FIA feeds nobody; a real building near a hideout destroyed by a US soldier (the modded
// SCR_DestructibleBuildingComponent) and a civilian killed there (the Civil Population socket's
// report) raise that hideout's grievance and its recruits a minute, not a far hideout's; both
// sides hear it (ours / enemy); a strike on a town counts when it lands, one in the open doesn't;
// the cap holds against a massacre; it fades by half in 15 min; -acrFIAGrievance 0 turns it off.
class ACR_TestFIAGrievance
{
 static const ResourceName CIVILIAN = "{22E43956740A6794}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_Randomized.et";
 protected static ref ACR_DefenderPost s_Hideout;
 protected static ref ACR_DefenderPost s_Far;
 protected static IEntity s_Soldier;
 protected static IEntity s_Building;
 protected static vector s_vBuilding;   // the building is replaced by its ruin once destroyed
 protected static float s_fBaseFill;
 protected static float s_fBefore;
 protected static vector s_vTown;
 protected static bool s_bTown;
 protected static ref array<IEntity> s_aFound;

 static void Run(int second)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_FactionSettings settings : ACR_FactionSettings.s_All) settings.m_bAICommander = false;
  if (second == 10) Start(manager);
  if (!s_Hideout) return;
  if (second == 12) Control(manager);
  if (second == 14) Destroy(manager);
  if (second == 17) BuildingCounted(manager);
  if (second == 18) CivilianKilled(manager);
  if (second == 19) Heard(manager);
  if (second == 20) StrikeCalled(manager);
  if (second == 25) StrikeLanded(manager);
  if (second == 26) Capped(manager);
  if (second == 27) Fades(manager);
  if (second == 28) OptionOff(manager);
 }

 protected static void Start(ACR_Manager manager)
 {
  ACR_Options.Set("acrFIAGrievance", "1");
  ACR_FIAGrievance.Apply();
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  if (hideouts.IsEmpty())
  {
   ACR_TestRegress.Abort("fiagrievance", "no hideout placed");
   return;
  }
  // The hideout with a destructible building nearest its middle; the far one: furthest from it.
  float best = 250;
  foreach (ACR_DefenderPost hideout : hideouts)
  {
   hideout.m_Cell.m_fGrievance = 0;
   IEntity building = BuildingNear(hideout.m_vPos, 250);
   if (!building) continue;
   float d = vector.DistanceXZ(building.GetOrigin(), hideout.m_vPos);
   if (d >= best) continue;
   best = d;
   s_Hideout = hideout;
   s_Building = building;
   s_vBuilding = building.GetOrigin();
  }
  if (!s_Hideout)
  {
   ACR_TestRegress.Abort("fiagrievance", "no destructible building within 250 m of a hideout");
   return;
  }
  float far;
  foreach (ACR_DefenderPost other : hideouts)
  {
   float gap = vector.DistanceXZ(other.m_vPos, s_Hideout.m_vPos);
   if (other == s_Hideout || gap <= far) continue;
   far = gap;
   s_Far = other;
  }
  string us = ACR_FactionSelection.West();
  array<ResourceName> pool = manager.m_Bases.GetFaction(us).m_aGarrison;
  if (!pool.IsEmpty()) s_Soldier = ACR_Manager.SpawnPrefab(pool[0], ACR_Manager.Ground(s_Hideout.m_vPos + Vector(700, 0, 0)));
  s_fBaseFill = ACR_FIAGrowth.FillRate(s_Hideout);
  // A town spot nearest the hideout (within REACH), for the strike.
  for (int r = 0; r <= 2500 && !s_bTown; r += 125)
  {
   for (int a = 0; a < 16 && !s_bTown; a++)
   {
    vector spot = s_Hideout.m_vPos + Vector(Math.Cos(a * Math.PI / 8) * r, 0, Math.Sin(a * Math.PI / 8) * r);
    // Still a town after the building near the hideout comes down (a spot of exactly TOWN_BUILDINGS
    // houses next to it stopped being one before the strike landed: coin-srvfiagrievance-b).
    if (ACR_FIAGrievance.Houses(spot, ACR_FIAGrievance.TOWN_RADIUS) <= ACR_FIAGrievance.TOWN_BUILDINGS) continue;
    s_vTown = spot;
    s_bTown = true;
   }
  }
  PrintFormat("[ACR] GRIEVANCE test: %1 (%2 hideouts), building %3 m off, far %4 at %5 m, soldier %6, town %7, base fill %8", s_Hideout.m_sName, hideouts.Count(), (int)best, s_Far != null, (int)far, s_Soldier != null, s_bTown, s_fBaseFill);
  ACR_TestRegress.Check("fiagrievance: on by default with leaning off, no grievance at the start", ACR_FIAGrievance.On() && !ACR_Insurgency.Leaning() && Total() == 0, string.Format("on %1, leaning %2, total %3", ACR_FIAGrievance.On(), ACR_Insurgency.Leaning(), Total()));
 }

 // Five quiet minutes: nothing rises; harm far from every hideout feeds none.
 protected static void Control(ACR_Manager manager)
 {
  ACR_FIAGrowth.Grow(5);
  float fill = ACR_FIAGrowth.FillRate(s_Hideout);
  ACR_TestRegress.Check("fiagrievance: doing nothing raises no grievance and no recruiting", Total() == 0 && ACR_FIAGrievance.s_iEvents == 0 && Math.AbsFloat(fill - s_fBaseFill) < 0.0001, string.Format("total %1, events %2, fill %3 (base %4)", Total(), ACR_FIAGrievance.s_iEvents, fill, s_fBaseFill));
  vector away;
  bool found;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (found || base.m_bRetired) continue;
   if (NearestHideout(base.GetOrigin()) < ACR_FIAGrievance.REACH) continue;
   away = base.GetOrigin();
   found = true;
  }
  if (!found)
  {
   ACR_TestRegress.Skip("fiagrievance: harm far from FIA feeds no hideout", "no base 3 km from every hideout");
   return;
  }
  ACR_FIAGrievance.OnCivilianKilled(away, ACR_FactionSelection.West());
  ACR_TestRegress.Check("fiagrievance: harm far from FIA feeds no hideout", Total() == 0, string.Format("total %1 at %2", Total(), ACR_Manager.Grid(away)));
 }

 // A real building by the hideout destroyed by a US soldier, through the damage system.
 protected static void Destroy(ACR_Manager manager)
 {
  s_fBefore = s_Hideout.m_Cell.m_fGrievance;
  SCR_DestructibleBuildingComponent damage = SCR_DestructibleBuildingComponent.Cast(s_Building.FindComponent(SCR_DestructibleBuildingComponent));
  if (!damage || !s_Soldier) return;
  vector hit[3];
  hit[0] = s_Building.GetOrigin();
  hit[1] = "0 1 0";
  hit[2] = "0 -1 0";
  SCR_DamageContext context = new SCR_DamageContext(EDamageType.EXPLOSIVE, 1000000, hit, s_Building, damage.GetDefaultHitZone(), Instigator.CreateInstigator(s_Soldier), null, -1, -1);
  damage.HandleDamage(context);
  PrintFormat("[ACR] GRIEVANCE test: building state %1", damage.GetState());
 }

 protected static void BuildingCounted(ACR_Manager manager)
 {
  float gained = s_Hideout.m_Cell.m_fGrievance - s_fBefore;
  float fill = ACR_FIAGrowth.FillRate(s_Hideout);
  float farGrievance = -1;
  if (s_Far) farGrievance = s_Far.m_Cell.m_fGrievance;
  bool farOk = !s_Far || vector.DistanceXZ(s_Far.m_vPos, s_vBuilding) < ACR_FIAGrievance.REACH || farGrievance == 0;
  ACR_TestRegress.Check("fiagrievance: a building destroyed near a hideout raises its grievance and recruiting, not a far one's", Math.AbsFloat(gained - ACR_FIAGrievance.BUILDING) < 0.001 && fill > s_fBaseFill && farOk, string.Format("+%1, fill %2 (base %3), far %4", gained, fill, s_fBaseFill, farGrievance));
 }

 protected static void CivilianKilled(ACR_Manager manager)
 {
  s_fBefore = s_Hideout.m_Cell.m_fGrievance;
  IEntity civilian = ACR_Manager.SpawnPrefab(CIVILIAN, ACR_Manager.Ground(s_Hideout.m_vPos + Vector(0, 0, 80)));
  ACR_CivilianSocket socket = ACR_CivilianSocket.Get();
  if (civilian && s_Soldier) socket.CivilianKilled(civilian, s_Soldier, 0);
  socket.FlushNow();
  float gained = s_Hideout.m_Cell.m_fGrievance - s_fBefore;
  ACR_TestRegress.Check("fiagrievance: a civilian killed near a hideout raises its grievance", civilian && Math.AbsFloat(gained - ACR_FIAGrievance.CIVILIAN) < 0.001, string.Format("civilian %1, +%2", civilian != null, gained));
  if (civilian) SCR_EntityHelper.DeleteEntityAndChildren(civilian);
 }

 protected static void Heard(ACR_Manager manager)
 {
  bool ours = HasLine(manager.IntelFor(ACR_FactionSelection.West()), "FIA recruits after our");
  bool enemy = HasLine(manager.IntelFor(ACR_FactionSelection.East()), "FIA recruits after enemy");
  ACR_TestRegress.Check("fiagrievance: the side that did it and the other side both hear it", ours && enemy && ACR_FIAGrievance.s_iLines >= 1, string.Format("ours %1, enemy %2, lines %3", ours, enemy, ACR_FIAGrievance.s_iLines));
 }

 protected static void StrikeCalled(ACR_Manager manager)
 {
  if (!s_bTown) return;
  s_fBefore = s_Hideout.m_Cell.m_fGrievance;
  ACR_FIAGrievance.OnStrikeCalled(ACR_FactionSelection.West(), s_vTown, ACR_ESupportType.ARTILLERY_BATTERY, 3);
 }

 protected static void StrikeLanded(ACR_Manager manager)
 {
  if (!s_bTown)
  {
   ACR_TestRegress.Skip("fiagrievance: a strike on a town counts when it lands, one in the open doesn't", "no town spot near the hideout");
   return;
  }
  float landed = s_Hideout.m_Cell.m_fGrievance - s_fBefore;
  float expected = ACR_FIAGrievance.STRIKE_HEAVY * ACR_FIAGrievance.WeightAt(s_Hideout, s_vTown);
  // Open ground near the hideout: no buildings round it.
  vector open;
  bool foundOpen;
  for (int a = 0; a < 16 && !foundOpen; a++)
  {
   vector spot = s_Hideout.m_vPos + Vector(Math.Cos(a * Math.PI / 8) * 450, 0, Math.Sin(a * Math.PI / 8) * 450);
   if (ACR_FIAGrievance.Houses(spot, ACR_FIAGrievance.TOWN_RADIUS) > 0) continue;
   open = spot;
   foundOpen = true;
  }
  float before = s_Hideout.m_Cell.m_fGrievance;
  if (foundOpen) ACR_FIAGrievance.StrikeLanded(ACR_FactionSelection.West(), open, ACR_FIAGrievance.STRIKE_HEAVY, "shelling");
  float field = s_Hideout.m_Cell.m_fGrievance - before;
  ACR_TestRegress.Check("fiagrievance: a strike on a town counts when it lands, one in the open doesn't", expected > 0 && Math.AbsFloat(landed - expected) < 0.001 && foundOpen && field == 0, string.Format("town %1 m off +%2 (expected %3), open spot %4 +%5", (int)vector.DistanceXZ(s_vTown, s_Hideout.m_vPos), landed, expected, foundOpen, field));
 }

 protected static void Capped(ACR_Manager manager)
 {
  for (int i = 0; i < 40; i++) ACR_FIAGrievance.OnCivilianKilled(s_Hideout.m_vPos, ACR_FactionSelection.West());
  float fill = ACR_FIAGrowth.FillRate(s_Hideout);
  float most = s_fBaseFill + ACR_FIAGrievance.MAX * ACR_FIAGrievance.PER_POINT;
  ACR_TestRegress.Check("fiagrievance: forty civilians killed stay within the cap", Math.AbsFloat(s_Hideout.m_Cell.m_fGrievance - ACR_FIAGrievance.MAX) < 0.001 && fill <= most + 0.0001 && fill > s_fBaseFill, string.Format("grievance %1 of %2, fill %3 (at most %4)", s_Hideout.m_Cell.m_fGrievance, ACR_FIAGrievance.MAX, fill, most));
 }

 protected static void Fades(ACR_Manager manager)
 {
  float before = s_Hideout.m_Cell.m_fGrievance;
  ACR_FIAGrievance.Fade(ACR_FIAGrievance.HALF_LIFE_MIN);
  float after = s_Hideout.m_Cell.m_fGrievance;
  ACR_TestRegress.Check("fiagrievance: grievance halves in 15 minutes", Math.AbsFloat(after - before * 0.5) < 0.01, string.Format("%1 -> %2", before, after));
 }

 protected static void OptionOff(ACR_Manager manager)
 {
  ACR_Options.Set("acrFIAGrievance", "0");
  ACR_FIAGrievance.Apply();
  s_Hideout.m_Cell.m_fGrievance = 0;
  ACR_FIAGrievance.OnCivilianKilled(s_Hideout.m_vPos, ACR_FactionSelection.West());
  ACR_FIAGrievance.OnBuildingDown(s_Hideout.m_vPos, ACR_FactionSelection.West());
  s_Hideout.m_Cell.m_fGrievance = 3; // as if left from before: off, it recruits nobody
  float fill = ACR_FIAGrievance.Fill(s_Hideout, false);
  s_Hideout.m_Cell.m_fGrievance = 0;
  bool off = !ACR_FIAGrievance.On() && ACR_FIAGrievance.s_iEvents == 0 && fill == 0;
  ACR_Options.Set("acrFIAGrievance", "1");
  ACR_FIAGrievance.Apply();
  ACR_TestRegress.Check("fiagrievance: -acrFIAGrievance 0 turns it off", off, string.Format("fill %1", fill));
  ACR_TestRegress.Complete("fiagrievance", 10);
 }

 // ---------------------------------------------------------------- helpers

 protected static float Total()
 {
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  float total;
  foreach (ACR_DefenderPost hideout : hideouts) total += hideout.m_Cell.m_fGrievance;
  return total;
 }

 protected static float NearestHideout(vector pos)
 {
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  float nearest = 1000000;
  foreach (ACR_DefenderPost hideout : hideouts) nearest = Math.Min(nearest, vector.DistanceXZ(hideout.m_vPos, pos));
  return nearest;
 }

 protected static bool HasLine(ACR_Intel intel, string text)
 {
  if (!intel) return false;
  foreach (string line : intel.m_Feed) if (line.Contains(text)) return true;
  return false;
 }

 protected static IEntity BuildingNear(vector pos, float radius)
 {
  if (!s_aFound) s_aFound = {};
  s_aFound.Clear();
  GetGame().GetWorld().QueryEntitiesBySphere(pos, radius, CollectBuilding, null, EQueryEntitiesFlags.STATIC);
  IEntity best;
  float nearest = radius;
  foreach (IEntity e : s_aFound)
  {
   float d = vector.DistanceXZ(e.GetOrigin(), pos);
   if (d >= nearest) continue;
   nearest = d;
   best = e;
  }
  return best;
 }

 protected static bool CollectBuilding(IEntity e)
 {
  if (!SCR_DestructibleBuildingEntity.Cast(e)) return true;
  SCR_DestructibleBuildingComponent damage = SCR_DestructibleBuildingComponent.Cast(e.FindComponent(SCR_DestructibleBuildingComponent));
  if (damage && damage.GetState() != EDamageState.DESTROYED) s_aFound.Insert(e);
  return true;
 }
}
