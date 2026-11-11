// One purchase between paying for a unit and the unit being usable (R01, 5 Oct review).
// ACR_Requisition.PurchaseAt fills it in, removes what it made and refunds it on failure;
// purchases whose crew or members arrive later wait in ACR_Requisition.m_aPending.
class ACR_PendingPurchase : Managed
{
 string m_sFaction;
 string m_sName;
 int m_iPaid;
 ACR_RequisitionEntry m_Entry;
 ACR_RequisitionEntry m_Ride; // a custom team's ride card: one of its stock went with this purchase
 string m_sError;
 bool m_bRefunded;
 float m_fDeadline;
 // What this purchase made, and nothing else (a failure deletes only these).
 IEntity m_Vehicle;
 bool m_bCrewed;
 SCR_AIGroup m_Group;
 SCR_AIGroup m_Riders;

 bool NeedsWatch() { return (m_Vehicle && m_bCrewed) || m_Group || m_Riders; }

 // Usable: a crewed vehicle has somebody aboard, each group has members. A vehicle
 // destroyed in the meantime ends the wait (it was delivered; losing it isn't a refund).
 bool Ready()
 {
  if (m_bCrewed)
  {
   if (!m_Vehicle) return true;
   array<IEntity> crew = {};
   Occupants(m_Vehicle, crew);
   if (crew.IsEmpty()) return false;
  }
  if (m_Group && m_Group.GetAgentsCount() == 0) return false;
  if (m_Riders && m_Riders.GetAgentsCount() == 0) return false;
  return true;
 }

 // A player who has taken the bought vehicle makes it impossible to roll that part
 // back safely. The caller keeps the charge if the rest of the purchase fails.
 bool HasPlayerOccupant()
 {
  if (!m_Vehicle) return false;
  array<IEntity> occupants = {};
  Occupants(m_Vehicle, occupants);
  foreach (IEntity occupant : occupants) if (EntityUtils.IsPlayer(occupant)) return true;
  return false;
 }

 // Its soldiers the squad registry doesn't count yet (for the soldier limit).
 int Unregistered(ACR_Manager manager)
 {
  int count;
  if (m_Group && !manager.FindSquadByGroup(m_Group)) count += m_Group.GetAgentsCount();
  if (m_Riders && !manager.FindSquadByGroup(m_Riders)) count += m_Riders.GetAgentsCount();
  if (m_bCrewed && m_Vehicle && manager.m_PendingCrewNames.Contains(m_Vehicle))
  {
   array<IEntity> crew = {};
   Occupants(m_Vehicle, crew);
   foreach (IEntity man : crew) if (!manager.FindSquadBySoldier(man)) count++;
  }
  return count;
 }

 static void Occupants(IEntity vehicle, notnull array<IEntity> result)
 {
  result.Clear();
  if (!vehicle) return;
  SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
  if (!compartments) return;
  array<BaseCompartmentSlot> seats = {};
  compartments.GetCompartments(seats);
  foreach (BaseCompartmentSlot seat : seats) if (seat.GetOccupant()) result.Insert(seat.GetOccupant());
 }
}
