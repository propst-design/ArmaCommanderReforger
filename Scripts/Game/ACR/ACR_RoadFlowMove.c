// #86 (20 Nov 2026): a crewed vehicle's road hops flow into each other again on Reforger 1.9. The
// game's move activity sends every agent of the group a cancel when it completes, fails or is
// deselected; between two waypoints that stopped the driver dead at each hop. When the move that
// ends is an ACA squad's order waypoint and the next waypoint is the hop ACA queued behind it
// (ACR_Squad.FlowsOnFrom), the cancel is skipped and the next hop's move takes the driver over.
modded class SCR_AIMoveActivity
{
 override protected void SendCancelMessagesToAllAgents()
 {
  if (m_Utility && ACR_Squad.KeepsDriving(m_Utility.m_Owner, m_RelatedWaypoint, m_vPosition.m_Value)) return;
  super.SendCancelMessagesToAllAgents();
 }
}
