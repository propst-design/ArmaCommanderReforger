// TvZ (#107): one side against the zombies, with no second side. The default zombie mode (TvTvZ)
// keeps both sides fighting each other while Project Outbreak's undead threaten both (ACR_Outbreak).
// In TvZ the players' side ("living") plays alone and the other side ("absent") never shows up:
//  - its bases are open towns from the start, its starting units and spawn points are dropped, it
//    has no AI commander and isn't offered on the deploy screen; every player joins the living side.
//  - its HQ becomes the hive: the biggest nest on the map, as far from the living side as it gets.
//  - zombies are the only enemy: more nests and a higher cap (ACR_Outbreak.Apply asks Share,
//    Country, Cap), hordes more often (HORDE_SCALE), and night raids hit HQs too.
//  - overrun: with no second economy to protect, zombies may take a base back. A held base (not
//    the home HQ, a FOB or a pocket) with zombies inside and none of its side for OVERRUN_MS falls
//    open and a nest settles in it (ACR_BaseSystem.UpdateCapture). A night raid on such a base
//    stays out in it until it falls (ACR_Outbreak.StayOut), then settles there itself.
// Option -acrZombieMode tvtvz|tvz (setup row "Zombie mode", shown only with Outbreak loaded). TvZ
// needs zombies: with -acrZombies off it plays as "few". Never on flashpoints (no zombies there).
class ACR_TvZ
{
 static const float OVERRUN_MS = 60000;   // zombies alone inside a held base this long: it falls
 static const int SETTLE_MIN = 4;         // a nest settling in an overrun base: at least this many
 static const int SETTLE_MAX = 16;        // ...and at most
 static const float HORDE_SCALE = 0.6;    // hordes leave nests this much sooner than in TvTvZ
 protected static bool s_bApplied;
 protected static ACR_Base s_Home;         // the living side's HQ: never overrun
 protected static ACR_Base s_Hive;         // the absent side's HQ: the biggest nest

 static void Reset()
 {
  s_bApplied = false;
  s_Home = null;
  s_Hive = null;
 }

 static string ModeOption()
 {
  string value = ACR_TestHooks.Get().ZombieMode();
  if (value == "" && !ACR_Options.Get("acrZombieMode", value)) value = "";
  value.ToLower();
  if (value == "tvz") return value;
  return "tvtvz";
 }

 // TvZ is being played: asked for, Outbreak loaded, not a flashpoint, not a COIN match (COIN has
 // its own absent side; zombies there are the two-side kind).
 static bool On()
 {
  if (ModeOption() != "tvz" || !ACR_Outbreak.Loaded() || ACR_Coin.On()) return false;
  int size = ACR_Skirmish.SizeOption();
  if (ACR_Skirmish.s_Active) size = ACR_Skirmish.s_Active.m_eSize;
  return size != ACR_ESkirmishSize.FLASHPOINT;
 }

 // Who plays and who sits out: one answer for every one-side mode (ACR_OneSide).
 static string Living() { return ACR_OneSide.Living(); }
 static string Absent() { return ACR_OneSide.Absent(); }
 static bool IsAbsent(string factionKey) { return On() && ACR_OneSide.IsAbsent(factionKey); }

 static ACR_Base Hive() { return s_Hive; }

 // Server, right after ACR_OneSide.Apply opened the absent side's ground (ACR_Skirmish.ApplyOnce).
 static void Apply()
 {
  if (s_bApplied || !On()) return;
  s_bApplied = true;
  s_Home = ACR_OneSide.Home();
  s_Hive = ACR_OneSide.AbsentHQ();
  string hive = "none";
  if (s_Hive) hive = s_Hive.m_sName;
  PrintFormat("[ACR] TVZ %1 against the zombies (%2 absent): hive %3", Living(), Absent(), hive);
 }

 // ---------------------------------------------------------------- nest density (ACR_Outbreak.Apply)

 // Zombies are the only enemy: half the open bases on few, most of them on many.
 static float Share(string level)
 {
  if (level == "many") return 0.8;
  return 0.5;
 }

 static int Country(int country)
 {
  return Math.Ceil(country * 1.5);
 }

 // How many zombies may be on the map at once (nests, hordes and the risen dead).
 static int Cap(int cap)
 {
  return Math.Round(cap * 1.5);
 }

 // ---------------------------------------------------------------- overrun (ACR_BaseSystem.UpdateCapture)

 static bool CanFall(ACR_Base base)
 {
  if (!On() || base.m_sOwner != Living() || base == s_Home) return false;
  return !base.m_bBuilt && !base.m_bPocket && !base.m_bRetired && base.m_eLogisticsRole != ACR_EBaseRole.HQ;
 }

 // Every base tick for a held base: zombies inside, its side not. True: it falls now.
 static bool Overrun(ACR_Base base, int zombies, int holders)
 {
  if (zombies <= 0 || holders > 0 || !CanFall(base))
  {
   base.m_fZombiesSince = 0;
   return false;
  }
  float now = GetGame().GetWorld().GetWorldTime();
  if (base.m_fZombiesSince <= 0) base.m_fZombiesSince = now;
  return now - base.m_fZombiesSince >= OVERRUN_MS;
 }

 // The base has fallen open (ACR_BaseSystem.Overrun): the zombies that took it settle in. A raid
 // aimed at it settles there itself when it packs up; a nest already there grows.
 static void Fallen(ACR_Base base, int zombies)
 {
  base.m_fZombiesSince = 0;
  int size = Math.ClampInt(zombies, SETTLE_MIN, SETTLE_MAX);
  string how = "";
  foreach (ACR_DefenderPost post : ACR_Defenders.s_Posts)
  {
   if (!post.m_Nest || post.m_bGone) continue;
   if (post.m_Nest.m_Raid == base) { how = post.m_sName + " settles there"; break; }
   if (!post.m_Nest.m_bRoaming && post.m_Nest.m_sBase == base.m_sName)
   {
    post.m_Nest.m_iSize = Math.Min(ACR_Outbreak.NEST_MAX, Math.Max(post.m_Nest.m_iSize, size));
    if (!post.m_Group) post.m_iReserve = Math.Max(post.m_iReserve, size);
    how = post.m_sName + " grows";
    break;
   }
  }
  if (how.IsEmpty())
  {
   ACR_DefenderPost nest = ACR_Outbreak.AddNest(base.GetOrigin(), size, base.m_sName);
   how = string.Format("%1 (%2) settles", nest.m_sName, nest.m_iReserve);
  }
  PrintFormat("[ACR] TVZ %1 overrun by %2 zombies: open, %3", base.m_sName, zombies, how);
 }
}
