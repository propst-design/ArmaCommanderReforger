// FIA pockets of resistance (Jacob, 24 Oct): on whole- and half-map battles, a few neutral
// bases away from both HQs start held by FIA. No FIA commander: the base system's garrison
// deploys when an enemy comes within its spawn range and packs up when they leave, military
// sites keep a small patrol, and a capture hands the base to whoever clears it.
// Hidden until found: other sides see the base as empty until one of their units comes within
// the garrison's spawn range (ACR_Base.OwnerSeenBy), so a squad grabbing an "empty" village
// runs into them. Option -acrPockets off|few|many (setup row "FIA resistance"): default few
// in games with the setup screen (every hosted/played match), off otherwise (test servers).
class ACR_Pockets
{
 static const string FACTION = "FIA";
 static const float MIN_HQ_DISTANCE = 1500; // m from any side's starting base (whole map)
 static const float MIN_SPACING = 800;      // m between two pockets

 static int CountOption(bool half)
 {
  string value = ACR_TestHooks.Get().Pockets();
  if (value == "" && !ACR_Options.Get("acrPockets", value)) value = "";
  if (value == "")
  {
   if (!ACR_Setup.Wanted()) return 0;
   value = "few";
  }
  value.ToLower();
  if (value == "off" || value == "0") return 0;
  if (value == "many")
  {
   if (half) return 6;
   return 12;
  }
  if (half) return 3;
  return 7;
 }

 // Garrison by kind of site: villages 4-6, industry 6-8, military 8-10.
 static int GarrisonFor(ACR_Base base)
 {
  if (base.m_iValue >= 3) return Math.RandomIntInclusive(8, 10);
  if (base.m_iValue == 2) return Math.RandomIntInclusive(6, 8);
  return Math.RandomIntInclusive(4, 6);
 }

 // Server, once the battlefield layout is applied (ACR_Skirmish.ApplyOnce).
 static void Apply()
 {
  // A named layout carries its own size; otherwise the size option (generated layouts too).
  int size = ACR_Skirmish.SizeOption();
  if (ACR_Skirmish.s_Active) size = ACR_Skirmish.s_Active.m_eSize;
  if (size == ACR_ESkirmishSize.FLASHPOINT) return;
  int wanted = CountOption(size == ACR_ESkirmishSize.HALF);
  if (wanted <= 0) return;
  if (ACR_FactionSelection.West() == FACTION || ACR_FactionSelection.East() == FACTION)
  {
   PrintFormat("[ACR] POCKETS none: %1 is playing as a main side", FACTION);
   return;
  }
  if (!GetGame().GetFactionManager().GetFactionByKey(FACTION))
  {
   PrintFormat("[ACR] POCKETS none: this world has no %1 faction", FACTION);
   return;
  }
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !manager.m_Bases || manager.m_Bases.GetFaction(FACTION).m_aGarrison.IsEmpty())
  {
   PrintFormat("[ACR] POCKETS none: no %1 garrison soldiers", FACTION);
   return;
  }
  array<ACR_Base> held = {};
  array<ACR_Base> open = {};
  foreach (ACR_Base base : ACR_Base.s_All)
  {
   if (base.m_bRetired) continue;
   // Bases a quick start handed out aren't starting bases: pockets keep their distance from HQs.
   if (!base.m_sOwner.IsEmpty() && !ACR_QuickStart.Assigned(base)) held.Insert(base);
   else if (!base.m_sOwner.IsEmpty()) continue;
   else if (!base.m_bBuilt && !base.m_bHidden) open.Insert(base);
  }
  if (held.IsEmpty()) return;
  // On a half map the HQs are closer: keep pockets a quarter of the HQ gap away, at least 1 km.
  float gap = 0;
  foreach (ACR_Base a : held)
   foreach (ACR_Base b : held)
    if (a.m_sOwner != b.m_sOwner) gap = Math.Max(gap, vector.DistanceXZ(a.GetOrigin(), b.GetOrigin()));
  float clearance = Math.Min(MIN_HQ_DISTANCE, Math.Max(1000, gap * 0.25));
  array<ACR_Base> candidates = {};
  foreach (ACR_Base site : open)
  {
   bool nearStart = false;
   foreach (ACR_Base start : held)
    if (vector.DistanceXZ(site.GetOrigin(), start.GetOrigin()) < clearance) nearStart = true;
   // Nor right beside a quick start's front: its garrisons would be at each other from the start.
   foreach (ACR_Base given : ACR_QuickStart.s_Assigned)
    if (vector.DistanceXZ(site.GetOrigin(), given.GetOrigin()) < ACR_QuickStart.POCKET_CLEARANCE) nearStart = true;
   if (!nearStart) candidates.Insert(site);
  }
  array<ACR_Base> picked = {};
  while (picked.Count() < wanted && !candidates.IsEmpty())
  {
   int i = Math.RandomInt(0, candidates.Count());
   ACR_Base pick = candidates[i];
   candidates.Remove(i);
   bool crowded = false;
   foreach (ACR_Base other : picked)
    if (vector.DistanceXZ(pick.GetOrigin(), other.GetOrigin()) < MIN_SPACING) crowded = true;
   if (crowded) continue;
   picked.Insert(pick);
  }
  string names = "";
  foreach (ACR_Base pocket : picked)
  {
   pocket.m_sOwner = FACTION;
   pocket.m_bPocket = true;
   pocket.m_fScore = 100;
   pocket.m_sCapturer = "";
   pocket.m_iGarrison = GarrisonFor(pocket);
   pocket.m_iGarrisonLeft = pocket.m_iGarrison;
   names += string.Format(" %1(%2)", pocket.m_sName, pocket.m_iGarrison);
  }
  PrintFormat("[ACR] POCKETS %1 of %2 wanted, %3 m from starting bases:%4", picked.Count(), wanted, (int)clearance, names);
 }
}
