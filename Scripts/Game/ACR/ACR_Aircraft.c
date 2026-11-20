// AC: Actual's own period jets: on by default without RHS since 19 Nov (out of Experimental; Jacob, 5 Nov:
// ship them switchable by the host). They are the RF-4C and Su-17M4R over the recon corridor, the A-10A and Su-25 over a
// bomb run with the bombs hanging on their pylons and falling (ACR_FallingBomb), their synthesised engine
// loops and pass-by one-shots (ACR_LocalFlyover), and the FLYOVER bomb run they give a side with no support
// list (ACR_Supports.BuildDefaults).
//
// Host option -acrPeriodJets 0 (or the setup row through ACR_Options.Set) turns them off; with RHS loaded
// they are off unless -acrPeriodJets 1 asks for them (its FA-18 flies, the row's "no RHS"). Off, the recon flight
// and the bomb run are as they were before the jets: RHS's FA-18 when RHS is loaded, else an unseen recon
// flight and a VIRTUAL bomb run for a side with no list; no bombs drawn, no blast delayed for a falling bomb,
// no new sound. A mission's explicit m_sBombRunPrefab / m_sReconPrefab / m_eBombRunDelivery still work,
// except that naming one of these jets needs the option on (off, it isn't flown).
//
// Every gate asks here. The server decides; each flyover's RPC carries its answer to the clients
// (ACR_RpcFlyover), which can't read the host's options.
class ACR_Aircraft
{
 // Server: the host's choice; unset, on without RHS and off with it.
 static bool PeriodJetsOn()
 {
  string fallback = "1";
  if (RhsJet()) fallback = "0";
  return ACR_LogisticsRules.Choice("acrPeriodJets", fallback) != "0";
 }

 // RHS's FA-18 is loaded: looked up once a session (the setup screen asks on every refresh; the loaded
 // mods don't change in a running game). Bare static: 0 = not looked yet, 1 = no, 2 = yes.
 protected static int s_iRhsJet;
 static bool RhsJet()
 {
  if (s_iRhsJet == 0)
  {
   s_iRhsJet = 1;
   if (ACR_Supports.HasPrefab(ACR_Supports.FA18_FLYOVER)) s_iRhsJet = 2;
  }
  return s_iRhsJet == 2;
 }

 // One of AC: Actual's own jets.
 static bool IsPeriodJet(ResourceName prefab)
 {
  return prefab == ACR_Supports.BOMB_JET_WEST || prefab == ACR_Supports.BOMB_JET_EAST || prefab == ACR_ReconRun.RECON_JET_WEST || prefab == ACR_ReconRun.RECON_JET_EAST;
 }

 // Server: a flyover prefab may be flown (anything but AC: Actual's own jets while they are off).
 static bool MayFly(ResourceName prefab)
 {
  return !IsPeriodJet(prefab) || PeriodJetsOn();
 }
}
