enum ACR_EBaseRole
{
 AUTHORED,
 HQ,
 MILITARY,
 SETTLEMENT,
 OBJECTIVE
}

// Explicit site roles separate buying formations from replacing squad casualties.
// AUTHORED preserves existing mission-maker flags until a role is assigned.
class ACR_LogisticsRules
{
 static string Choice(string option, string fallback)
 {
  string value;
  if (!ACR_Options.Get(option, value) || value.IsEmpty()) return fallback;
  value.TrimInPlace();
  value.ToLower();
  // On/off options test "0": "off", "false" or "no" left them on.
  if (value == "off" || value == "false" || value == "no") return "0";
  if (value == "on" || value == "true" || value == "yes") return "1";
  return value;
 }

 static int Number(string option, int fallback, int maximum)
 {
  string value;
  if (!ACR_Options.Get(option, value) || value.IsEmpty()) return fallback;
  return Math.Clamp(value.ToInt(), 0, maximum);
 }

 static bool CanDeploy(ACR_Base base)
 {
  if (!base || base.m_bRetired || !base.m_bAllowSpawn) return false;
  if (RelayBlocked(base)) return false;
  if (!IsHQ(base) && ACR_SupplyLines.IsIsolated(base)) return false;
  string policy = Choice("acrDeployment", "military");
  if (policy == "all") return true;
  if (base.m_bBuilt || IsHQ(base)) return true;
  if (policy == "hq") return false;
  if (base.m_eLogisticsRole == ACR_EBaseRole.AUTHORED) return true;
  return base.m_eLogisticsRole == ACR_EBaseRole.MILITARY;
 }

 static bool IsHQ(ACR_Base base)
 {
  if (!base) return false;
  if (base.m_eLogisticsRole == ACR_EBaseRole.HQ) return true;
  if (base.m_eLogisticsRole != ACR_EBaseRole.AUTHORED) return false;
  foreach (SCR_SpawnPoint point : SCR_SpawnPoint.GetSpawnPoints())
   if (point && !ACR_SpawnPoints.IsBasePoint(point) && point.GetFactionKey() == base.m_sOwner && vector.DistanceXZ(point.GetOrigin(), base.GetOrigin()) <= base.m_fRadius + 60) return true;
  return false;
 }

 static bool CanReinforce(ACR_Base base)
 {
  if (!base || base.m_bRetired || !base.m_bAllowReinforce) return false;
  if (RelayBlocked(base)) return false;
  if (ACR_SupplyLines.IsIsolated(base)) return false;
  string policy = Choice("acrReinforcement", "settlements");
  if (policy == "all") return true;
  if (policy == "deployment") return CanDeploy(base);
  return CanDeploy(base) || base.m_eLogisticsRole == ACR_EBaseRole.SETTLEMENT;
 }

 static bool SuppliedServices() { return Choice("acrFobServices", "supplies") != "free"; }
 static bool RequireRelay() { return Choice("acrFobRelay", "1") != "0"; }
 static bool RelayBlocked(ACR_Base base)
 {
  if (base && base.m_bBuilt && base.m_bHqLost) return true;
  if (!base || !base.m_bBuilt || !RequireRelay()) return false;
  ACR_Manager manager = ACR_Manager.GetInstance();
  return !manager || !manager.m_Structures || !manager.m_Structures.HasRelay(base);
 }
 static int DepotBonus() { return Number("acrDepotCapacity", 2000, 10000); }
 static int ServiceCost() { return Number("acrServiceSupplyCost", 10, 100); }
 static int InstantServiceFee() { return Number("acrFobRPServiceCost", 3, 100); }
}
