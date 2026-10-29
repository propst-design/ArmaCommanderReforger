// After-action report (5 Oct; roadmap item 6). The server keeps a running tally per side: own
// soldiers and vehicles lost, enemy soldiers and vehicles killed (by whoever the game credits:
// soldiers, vehicle gunners, mortar crews; bombs and the battery have no shooter and count only
// as losses), bases won and lost, FOBs built and HQs lost, RP earned and spent, and a short
// timeline of the big moments (FOB events included).
// Each player sees his side's report in the map panel ("Report"), updated every few seconds.
// No victory conditions: it is a report, not a scoreboard that ends anything.
class ACR_SideStats : Managed
{
 int m_iLost;
 int m_iKills;
 int m_iVehiclesLost;
 int m_iVehicleKills;
 int m_iBasesWon;
 int m_iBasesLost;
 int m_iRPEarned;
 int m_iRPSpent;
 int m_iSquadsBought;
 int m_iSupports;
 int m_iFobsBuilt;
 int m_iFobsLost;     // FOB HQs lost (destroyed, demolished, dismantled)
 ref array<string> m_aTimeline = {};
}

class ACR_Report
{
 static const int TIMELINE = 12;
 protected static ref map<string, ref ACR_SideStats> s_Sides = new map<string, ref ACR_SideStats>();

 static void Reset() { s_Sides.Clear(); }

 static ACR_SideStats Of(string key)
 {
  if (key.IsEmpty()) return null;
  ACR_SideStats stats;
  if (!s_Sides.Find(key, stats))
  {
   stats = new ACR_SideStats();
   s_Sides.Insert(key, stats);
  }
  return stats;
 }

 protected static bool Running()
 {
  return Replication.IsServer() && ACR_Manager.GetInstance() != null;
 }

 static string Clock()
 {
  int seconds = GetGame().GetWorld().GetWorldTime() / 1000;
  int minutes = seconds / 60;
  int hours = minutes / 60;
  minutes = minutes - hours * 60;
  return string.Format("%1:%2", hours, minutes.ToString(2));
 }

 static void Event(string key, string text)
 {
  ACR_SideStats stats = Of(key);
  if (!stats) return;
  stats.m_aTimeline.Insert(Clock() + "  " + text);
  while (stats.m_aTimeline.Count() > TIMELINE) stats.m_aTimeline.RemoveOrdered(0);
 }

 static void OnSoldierDown(IEntity victim, Instigator instigator)
 {
  IEntity shooter;
  if (instigator) shooter = instigator.GetInstigatorEntity();
  if (victim) ACR_Insurgency.OnLoss(ACR_Intel.FactionKeyOf(victim), victim.GetOrigin(), ACR_Intel.FactionKeyOf(shooter));
  if (!Running()) return;
  string side = ACR_Intel.FactionKeyOf(victim);
  ACR_SideStats lost = Of(side);
  if (lost) lost.m_iLost++;
  IEntity killer;
  if (instigator) killer = instigator.GetInstigatorEntity();
  string by = ACR_Intel.FactionKeyOf(killer);
  if (!by.IsEmpty() && by != side) { ACR_SideStats won = Of(by); if (won) won.m_iKills++; }
 }

 static void OnVehicleDown(IEntity vehicle, Instigator instigator, string side)
 {
  IEntity shooter;
  if (instigator) shooter = instigator.GetInstigatorEntity();
  if (vehicle) ACR_Insurgency.OnLoss(side, vehicle.GetOrigin(), ACR_Intel.FactionKeyOf(shooter));
  if (!Running()) return;
  ACR_SideStats lost = Of(side);
  if (lost) lost.m_iVehiclesLost++;
  IEntity killer;
  if (instigator) killer = instigator.GetInstigatorEntity();
  string by = ACR_Intel.FactionKeyOf(killer);
  if (!by.IsEmpty() && by != side) { ACR_SideStats won = Of(by); if (won) won.m_iVehicleKills++; }
  string name = "vehicle";
  if (vehicle && vehicle.GetPrefabData()) name = FilePath.StripExtension(FilePath.StripPath(vehicle.GetPrefabData().GetPrefabName()));
  if (!side.IsEmpty()) Event(side, "lost a " + name);
 }

 static void OnCapture(string base, string by, string from)
 {
  ACR_SideStats won = Of(by);
  if (won) { won.m_iBasesWon++; Event(by, "took " + base); }
  ACR_SideStats lost = Of(from);
  if (lost) { lost.m_iBasesLost++; Event(from, "lost " + base); }
 }

 static void OnIncome(string key, int amount) { ACR_SideStats s = Of(key); if (s) s.m_iRPEarned += amount; }
 static void OnSpend(string key, int amount) { ACR_SideStats s = Of(key); if (s) s.m_iRPSpent += amount; }
 static void OnRefund(string key, int amount) { ACR_SideStats s = Of(key); if (s) s.m_iRPSpent -= amount; }

 // The panel text for one side.
 static string Text(string key)
 {
  ACR_SideStats s = Of(key);
  if (!s) return "";
  string text = ACR_FactionSelection.DisplayName(key) + " report\n" + string.Format("Time %1\nSoldiers lost %2, enemy killed %3\nVehicles lost %4, enemy vehicles destroyed %5\nBases won %6, lost %7\nRP earned %8, spent %9",
   Clock(), s.m_iLost, s.m_iKills, s.m_iVehiclesLost, s.m_iVehicleKills, s.m_iBasesWon, s.m_iBasesLost, s.m_iRPEarned, s.m_iRPSpent);
  if (s.m_iFobsBuilt > 0 || s.m_iFobsLost > 0) text += string.Format("\nFOBs built %1, HQs lost %2", s.m_iFobsBuilt, s.m_iFobsLost);
  string doctrines = ACR_DoctrineIntel.ReportLines(key);
  if (!doctrines.IsEmpty()) text += "\n" + doctrines;
  if (!s.m_aTimeline.IsEmpty())
  {
   text += "\n";
   for (int i = s.m_aTimeline.Count() - 1; i >= 0 && i >= s.m_aTimeline.Count() - 8; i--) text += "\n" + s.m_aTimeline[i];
  }
  return text;
 }

 // Written into the server log every 5 minutes (and by tests).
 static void LogAll()
 {
  foreach (string key, ACR_SideStats s : s_Sides)
   PrintFormat("[ACR] REPORT %1: lost %2 killed %3 | vehicles lost %4 destroyed %5 | bases +%6 -%7 | RP +%8 -%9", ACR_FactionSelection.DisplayName(key), s.m_iLost, s.m_iKills, s.m_iVehiclesLost, s.m_iVehicleKills, s.m_iBasesWon, s.m_iBasesLost, s.m_iRPEarned, s.m_iRPSpent);
 }
}

modded class SCR_CharacterDamageManagerComponent
{
 protected override void OnDamageStateChanged(EDamageState newState, EDamageState previousDamageState, bool isJIP)
 {
  super.OnDamageStateChanged(newState, previousDamageState, isJIP);
  if (newState == EDamageState.DESTROYED && previousDamageState != EDamageState.DESTROYED && !isJIP) ACR_Report.OnSoldierDown(GetOwner(), GetInstigator());
 }
}

modded class SCR_VehicleDamageManagerComponent
{
 override void OnDamageStateChanged(EDamageState newState, EDamageState previousDamageState, bool isJIP)
 {
  // The vehicle's side before it burns (an empty wreck reports none).
  string side = ACR_Intel.FactionKeyOf(GetOwner());
  super.OnDamageStateChanged(newState, previousDamageState, isJIP);
  if (newState == EDamageState.DESTROYED && previousDamageState != EDamageState.DESTROYED && !isJIP) ACR_Report.OnVehicleDown(GetOwner(), GetInstigator(), side);
 }
}
