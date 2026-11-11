// Plugs Shadow Company Civil Population (SCCP, BCE0D49568A9E6BF) into ACA's civilian socket
// (ACR_CivilianSocket). Optional: load this add-on with both mods; either mod runs without it.
// Server only, like both sides. Touches only SCCP's public methods so its updates break less.
//  - AI slots: SCCP's walking civilians (maxAgents) are added to ACA's raise of the active AI
//    limit, so they don't push ACA groups over it (the engine deletes groups above the limit).
//  - Scripted ACA blasts are reported to SCCP as explosions: they are local projectiles, which
//    SCCP's ammo listener never hears.
//  - Every civilian death SCCP records goes to ACA's feed of the killer's side.
//  - SCCP's walkers feed ACA's civilian rings: each side sees roughly where townsfolk are.
modded class ACR_CivilianSocket
{
 protected SCCP_Director Director()
 {
  SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
  if (!mode) return null;
  return mode.SCCP_GetDirector();
 }

 override string Name() { return "Civil Population"; }

 override int AiHeadroom()
 {
  SCCP_Director director = Director();
  if (!director || !director.GetSettings() || !director.GetSettings().m_bEnabled) return 0;
  return director.GetSettings().m_iMaxAgents;
 }

 override void OnStrike(vector impact)
 {
  SCCP_Director director = Director();
  if (director && director.GetThreat()) director.GetThreat().Report(impact, SCCP_Threat.KIND_EXPLOSION);
 }

 // SCCP's walkers, for the vague civilian rings on each side's map (ACR_Intel.CivilianActivity).
 // Each zone's civilians (m_aCivs); its parked and moving cars (m_aVehs) are left out.
 override void Civilians(notnull array<IEntity> into)
 {
  super.Civilians(into);
  SCCP_Director director = Director();
  if (!director || !director.GetZones()) return;
  foreach (SCCP_Zone zone : director.GetZones())
  {
   if (!zone) continue;
   foreach (SCCP_Civ civ : zone.m_aCivs)
    if (civ && civ.m_Ent && ACR_Respawn.IsAlive(civ.m_Ent)) into.Insert(civ.m_Ent);
  }
 }
}

modded class SCCP_Collateral
{
 override void Record(SCR_InstigatorContextData ctx, SCCP_Director d)
 {
  int before = Count();
  super.Record(ctx, d);
  if (Count() > before) ACR_CivilianSocket.Get().CivilianKilled(ctx.GetVictimEntity(), ctx.GetKillerEntity(), ctx.GetKillerPlayerID());
 }
}
