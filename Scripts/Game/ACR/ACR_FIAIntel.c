// Intel from the fight (#96 part 4 of the plan, "Intel rings"): fighting FIA pays off in what you
// learn. The side that wipes out a cell (or kills an IED team or a raid, ACR_IED / ACR_Raid) gets
// a rough "FIA activity" ring where the hideout that fed it lies: RADIUS_FIRST m, its middle off
// the hideout by up to OFFSET_FIRST of that. Each further lead from the same hideout tightens it
// (RADIUS_NEXT, RADIUS_LAST). The ring always holds the hideout; finding the hideout itself still
// takes men there (ACR_Insurgency.HideoutFound), which replaces the ring with the hideout's own.
// Allowed with "Hidden FIA": it's intel earned by contact. Any lead can feed it (#97's heat too).
class ACR_FIALead : Managed
{
 string m_sSide;
 ACR_DefenderPost m_Hideout;
 vector m_vCentre;
 float m_fRadius;
 int m_iLeads;
}

class ACR_FIAIntel
{
 static const float RADIUS_FIRST = 1000;
 static const float RADIUS_NEXT = 600;
 static const float RADIUS_LAST = 400;
 static const float OFFSET_SHARE = 0.6;    // the middle is off the hideout by up to this share of the radius
 static const float WITNESS_REACH = 500;   // sides with men this close to a wiped cell get the lead
 static ref array<ref ACR_FIALead> s_aLeads;

 // Called by ACR_StaticInit at game start (static initializer budget: HANDBOOK Pitfalls).
 static void ACR_InitStatics()
 {
  s_aLeads = {};
 }

 static void Reset()
 {
  s_aLeads.Clear();
 }

 // The standing hideout that feeds a cell at 'pos' (the nearest in ACR_Guerrillas.SUPPLY_REACH).
 static ACR_DefenderPost SupplierOf(vector pos)
 {
  array<ACR_DefenderPost> hideouts = {};
  ACR_Guerrillas.Hideouts(hideouts);
  ACR_DefenderPost best;
  float nearest = ACR_Guerrillas.SUPPLY_REACH;
  foreach (ACR_DefenderPost hideout : hideouts)
  {
   float d = vector.DistanceXZ(hideout.m_vPos, pos);
   if (d > nearest) continue;
   nearest = d;
   best = hideout;
  }
  return best;
 }

 // A cell (or team) of FIA's wiped out at 'pos': each main side with men near that FIA doesn't
 // count as a friend gets a lead on 'hideout'.
 static void Wiped(vector pos, ACR_DefenderPost hideout, string what)
 {
  if (!hideout || hideout.m_bGone) return;
  array<string> sides = {};
  ACR_Insurgency.SidesNear(pos, WITNESS_REACH, sides);
  foreach (string side : sides)
   if (!ACR_Insurgency.Friendly(side, ACR_Pockets.FACTION)) Lead(side, hideout, what);
 }

 // 'side' learns roughly where 'hideout' is: a ring, or a tighter one.
 static ACR_FIALead Lead(string side, ACR_DefenderPost hideout, string what)
 {
  if (side.IsEmpty() || !hideout || hideout.m_bGone || !hideout.m_Cell || hideout.m_Cell.m_aKnownBy.Contains(side)) return null;
  ACR_FIALead lead;
  foreach (ACR_FIALead known : s_aLeads) if (known.m_sSide == side && known.m_Hideout == hideout) lead = known;
  if (!lead)
  {
   lead = new ACR_FIALead();
   lead.m_sSide = side;
   lead.m_Hideout = hideout;
   s_aLeads.Insert(lead);
  }
  lead.m_iLeads++;
  lead.m_fRadius = RADIUS_FIRST;
  if (lead.m_iLeads == 2) lead.m_fRadius = RADIUS_NEXT;
  if (lead.m_iLeads >= 3) lead.m_fRadius = RADIUS_LAST;
  float angle = Math.RandomFloat(0, Math.PI2);
  float off = Math.RandomFloat(0.2, OFFSET_SHARE) * lead.m_fRadius;
  lead.m_vCentre = hideout.m_vPos + Vector(Math.Cos(angle) * off, 0, Math.Sin(angle) * off);
  PrintFormat("[ACR] INSURGENCY %1 has a lead on %2 (%3): a %4 m ring, %5 leads", side, hideout.m_sName, what, (int)lead.m_fRadius, lead.m_iLeads);
  ACR_Manager manager = ACR_Manager.GetInstance();
  ACR_Intel intel;
  if (manager) intel = manager.IntelFor(side);
  if (intel) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), string.Format("FIA activity: %1 points to a hideout within %2 m of %3", what, (int)lead.m_fRadius, ACR_Manager.Grid(lead.m_vCentre)));
  return lead;
 }

 // The side's command map (ACR_Guerrillas.Zones): a ring per lead whose hideout stands and that the
 // side hasn't found yet.
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  for (int i = s_aLeads.Count() - 1; i >= 0; i--)
  {
   ACR_FIALead lead = s_aLeads[i];
   if (!lead.m_Hideout || lead.m_Hideout.m_bGone || lead.m_Hideout.m_Cell.m_aKnownBy.Contains(lead.m_sSide))
   {
    s_aLeads.RemoveOrdered(i);
    continue;
   }
   if (lead.m_sSide != faction) continue;
   positions.Insert(lead.m_vCentre);
   radii.Insert(lead.m_fRadius);
   labels.Insert("FIA activity");
  }
 }

 static ACR_FIALead LeadOf(string side, ACR_DefenderPost hideout)
 {
  foreach (ACR_FIALead lead : s_aLeads) if (lead.m_sSide == side && lead.m_Hideout == hideout) return lead;
  return null;
 }
}
