// A base (control/resource point) placed by the mission maker in World Editor.
// Mirrors Arma Commander's base module: owner, garrison, RP value, income and
// hidden flags, and a capture radius. All runtime logic lives in ACR_BaseSystem.
class ACR_BaseClass : GenericEntityClass {}
class ACR_Base : GenericEntity
{
 [Attribute("", UIWidgets.EditBox, "Base name shown on the map")]
 string m_sName;
 [Attribute("", UIWidgets.EditBox, "Starting owner faction key (e.g. US, USSR). Empty = nobody")]
 string m_sOwner;
 [Attribute("10", UIWidgets.EditBox, "Garrison: AI soldiers guarding the base")]
 int m_iGarrison;
 [Attribute("1", UIWidgets.EditBox, "Requisition points this base adds to its owner's income")]
 int m_iValue;
 [Attribute("1", UIWidgets.CheckBox, "Produces income. Off = a purely tactical control point")]
 bool m_bIncome;
 [Attribute("0", UIWidgets.CheckBox, "Hidden from other factions until one of their units enters it")]
 bool m_bHidden;
 [Attribute("1", UIWidgets.CheckBox, "Purchased units and reinforcements can deploy here")]
 bool m_bAllowSpawn;
 [Attribute("1", UIWidgets.CheckBox, "Allows RP infantry replacements; separate from new-unit deployment")]
 bool m_bAllowReinforce;
 [Attribute("0", UIWidgets.ComboBox, "Logistics role: Authored preserves existing deployment flags", enums: ParamEnumArray.FromEnum(ACR_EBaseRole))]
 ACR_EBaseRole m_eLogisticsRole;
 [Attribute("50", UIWidgets.EditBox, "Capture radius in metres")]
 float m_fRadius;

 static ref array<ACR_Base> s_All = {};

 // ---- server runtime state
 float m_fScore = 100;              // owned: 100 = secure, -100 = lost. Neutral: 0..100 progress for m_sCapturer
 string m_sCapturer;                // neutral base: faction currently taking it
 bool m_bContested;
 int m_iGarrisonLeft;               // soldiers still in reserve (virtual while not deployed)
 int m_iHousingGarrison;            // capacity supplied by completed friendly FOB barracks
 SCR_AIGroup m_GarrisonGroup;       // spawned only while enemies are near
 SCR_AIGroup m_PatrolGroup;         // FOB with barracks: a few of the garrison walk the perimeter
 int m_iPatrolLeg;
 float m_fPatrolRefill;
 ref array<string> m_aDetectedBy = {};
 bool m_bBuilt;                     // a forward base made by a built HQ (ACR_Structures)
 bool m_bRetired;                   // its HQ is gone: no longer a base
 IEntity m_FobSupply;               // replicated building provider; supplies are paid, never generated
 int m_iSupplyDelivery;
 float m_fSupplyDeliveryLeft;
 float m_fSupplyDeliveryUpdated;
 string m_sSupplyDeliveryFaction;
 ref array<ref ACR_FobSlot> m_aFobSlots;
 ref ACR_FobSlot m_FobDefences;     // the defences upgrade (ACR_FobDefences)
 float m_fFobSlotsLastTick;
 bool m_bHqLost;                    // built FOB whose HQ was destroyed: offline, capturable, rebuildable
 bool m_bAIManaged;                 // handed to the AI (FOB panel, ACR_FobAI)...
 string m_sAIManagedFor;            // ...by this side
 bool m_bPocket;                    // FIA pocket of resistance (ACR_Pockets) until someone captures it

 // A built FOB keeps its upgrades when its HQ is destroyed (Jacob, 18 Oct): no deployment,
 // reinforcement, services, deliveries or upgrades until an Establish FOB order rebuilds the HQ.
 void MarkHqLost(string why = "its HQ was destroyed")
 {
  if (m_bRetired || !m_bBuilt || m_bHqLost) return;
  m_bHqLost = true;
  m_iSupplyDelivery = 0;
  m_fSupplyDeliveryLeft = 0;
  PrintFormat("[ACR] BASE %1 offline (%2)", m_sName, why);
  ACR_SideStats stats = ACR_Report.Of(m_sOwner);
  if (stats) stats.m_iFobsLost++;
  ACR_Intel.FobEvent(this, m_sName + " offline: HQ lost, an Establish FOB order rebuilds it", m_sName + " HQ lost");
 }

 // A built FOB that can deploy, serve and build (not retired, HQ standing).
 bool FobOnline()
 {
  return m_bBuilt && !m_bRetired && !m_bHqLost;
 }

 // A built forward base whose HQ is gone: off the map and out of every base list.
 // Also used for bases left out of a skirmish layout (ACR_Skirmish).
 void Retire(string why = "its HQ is gone")
 {
  if (m_bRetired) return;
  m_bRetired = true;
  m_iSupplyDelivery = 0;
  m_fSupplyDeliveryLeft = 0;
  ACR_FobSlots.Cleanup(this);
  if (m_FobSupply) SCR_EntityHelper.DeleteEntityAndChildren(m_FobSupply);
  m_FobSupply = null;
  if (s_All) s_All.RemoveItem(this);
  if (m_GarrisonGroup) SCR_EntityHelper.DeleteEntityAndChildren(m_GarrisonGroup);
  if (m_PatrolGroup) SCR_EntityHelper.DeleteEntityAndChildren(m_PatrolGroup);
  PrintFormat("[ACR] BASE %1 retired (%2)", m_sName, why);
  if (m_bBuilt) ACR_Intel.FobEvent(this, m_sName + " is gone: nothing of it is left standing", m_sName + " gone");
 }

 void ACR_Base(IEntitySource src, IEntity parent)
 {
  SetEventMask(EntityEvent.INIT);
 }

 override void EOnInit(IEntity owner)
 {
  if (!GetGame().InPlayMode()) return;
  if (m_sName.IsEmpty()) m_sName = string.Format("Base %1", s_All.Count() + 1);
  // An authored radius of 0 or less could never be captured; a negative value drained income.
  if (m_fRadius <= 0)
  {
   PrintFormat("[ACR] BASE %1: capture radius %2 m is not positive, using 50 m", m_sName, m_fRadius);
   m_fRadius = 50;
  }
  if (m_iValue < 0)
  {
   PrintFormat("[ACR] BASE %1: value %2 is negative, using 0", m_sName, m_iValue);
   m_iValue = 0;
  }
  m_iGarrisonLeft = m_iGarrison;
  if (m_sOwner.IsEmpty()) m_fScore = 0;
  s_All.Insert(this);
 }

 Faction GetOwnerFaction()
 {
  if (m_sOwner.IsEmpty()) return null;
  return GetGame().GetFactionManager().GetFactionByKey(m_sOwner);
 }

 // Name as shown on the map, e.g. "Farm [1]".
 string GetLabel()
 {
  if (m_bIncome) return string.Format("%1 [%2]", m_sName, m_iValue);
  return m_sName;
 }

 // The owner as this faction knows it: an FIA pocket looks unclaimed until found.
 string OwnerSeenBy(string factionKey)
 {
  if (m_bPocket && m_sOwner != factionKey && !m_aDetectedBy.Contains(factionKey)) return "";
  return m_sOwner;
 }

 bool IsVisibleTo(string factionKey)
 {
  return !m_bHidden || m_sOwner == factionKey || m_aDetectedBy.Contains(factionKey);
 }

 void ~ACR_Base()
 {
  if (s_All) s_All.RemoveItem(this);
 }
}
