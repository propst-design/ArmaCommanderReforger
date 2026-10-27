// Socket for an optional civilian-population mod (server). ACA never names such a mod's classes:
// a compat add-on that depends on both mods plugs in with `modded class ACR_CivilianSocket` and
// overrides the hooks below. Unplugged, every hook does nothing and ACA plays as before.
//  - ACA -> mod: AiHeadroom (room in the active AI limit), OnStrike (scripted blasts the mod's
//    civilians can't hear: ACA support projectiles are local copies on each client).
//  - mod -> ACA: CivilianKilled, a feed line to the killer's side ("civilian killed near ...").
//    The other side isn't told: that would hand it the killer's position for free.
class ACR_CivilianSocket : Managed
{
 protected static ref ACR_CivilianSocket s_Instance;
 static const int BATCH_MS = 5000;      // deaths at one place within this go out as one line
 static const float NEAR_BASE = 1500;   // m: named after the nearest base this close, else a grid

 protected ref map<string, int> m_Pending = new map<string, int>(); // "faction|place" -> deaths
 protected bool m_bFlushQueued;

 static ACR_CivilianSocket Get()
 {
  if (!s_Instance) s_Instance = new ACR_CivilianSocket();
  return s_Instance;
 }

 // ---------------------------------------------------------------- hooks (override in a compat add-on)

 // The plugged-in mod's name for logs; empty while nothing is plugged in.
 string Name() { return ""; }
 bool Plugged() { return !Name().IsEmpty(); }

 // Active AI slots the mod's own agents may take, added to ACA's raise of the engine limit.
 int AiHeadroom() { return 0; }

 // A scripted ACA blast (bomb run, battery round, cook-off) at this point.
 void OnStrike(vector impact) {}

 // ---------------------------------------------------------------- called by the mod

 // A civilian died. killer: the killing entity (soldier, vehicle) or null.
 void CivilianKilled(IEntity victim, IEntity killer, int killerPlayer)
 {
  if (!victim || !Replication.IsServer()) return;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  string side;
  if (killerPlayer > 0)
  {
   Faction playerFaction = SCR_FactionManager.SGetPlayerFaction(killerPlayer);
   if (playerFaction) side = playerFaction.GetFactionKey();
  }
  if (side.IsEmpty() && killer) side = ACR_Intel.FactionKeyOf(killer);
  if (side.IsEmpty() && killer) side = ACR_Vehicles.FactionOf(killer.GetRootParent());
  if (side.IsEmpty() || !manager.IntelFor(side)) return;
  string key = side + "|" + PlaceName(victim.GetOrigin());
  m_Pending.Set(key, m_Pending.Get(key) + 1);
  if (m_bFlushQueued) return;
  m_bFlushQueued = true;
  GetGame().GetCallqueue().CallLater(Flush, BATCH_MS, false);
 }

 // ---------------------------------------------------------------- internals

 protected void Flush()
 {
  m_bFlushQueued = false;
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (manager)
  {
   float now = GetGame().GetWorld().GetWorldTime();
   foreach (string key, int deaths : m_Pending)
   {
    int bar = key.IndexOf("|");
    ACR_Intel intel = manager.IntelFor(key.Substring(0, bar));
    if (!intel) continue;
    string place = key.Substring(bar + 1, key.Length() - bar - 1);
    string line = "Civilian killed by our fire " + place;
    if (deaths > 1) line = string.Format("%1 civilians killed by our fire %2", deaths, place);
    intel.AddFeed(now, line);
   }
  }
  m_Pending.Clear();
 }

 static string PlaceName(vector at)
 {
  ACR_Base nearest;
  float best = NEAR_BASE;
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (!base || base.m_bRetired) continue;
   float d = vector.DistanceXZ(base.GetOrigin(), at);
   if (d < best) { best = d; nearest = base; }
  }
  if (nearest) return "near " + nearest.m_sName;
  return "at " + ACR_Manager.Grid(at);
 }

 // Tests: the line batch waiting to go out, and sending it now.
 int PendingDeaths()
 {
  int n;
  foreach (string key, int deaths : m_Pending) n += deaths;
  return n;
 }
 void FlushNow()
 {
  GetGame().GetCallqueue().Remove(Flush);
  Flush();
 }
}
