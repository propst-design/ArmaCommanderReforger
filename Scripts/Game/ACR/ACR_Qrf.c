// QRF (map panel, a standing order): the squad stands by at a post and covers RADIUS round it.
// When its side knows of an enemy inside (ACR_Intel: its squads' eyes and its base sentries),
// it goes after it with the Hunt order, then walks back to the post and stands by again.
// Policy only: how the soldiers fight and search is vanilla and BBB's. On standby the squad
// is a defending squad to BBB, so BBB's radio contact relay (BBB_ContactRelay) still hands it
// other squads' sightings and vanilla may send a fireteam to look; once it moves out it is
// hunting, and the relay leaves it to ACA's plan (ACR_BBBHooks.AllowContactRelay).
class ACR_QrfPlan : Managed
{
 vector m_vPost;
 bool m_bOut;          // hunting
 bool m_bReturning;    // walking back to the post
 int m_iSorties;
 float m_fNextCheck;
 string m_sTarget;     // what it went after
}

class ACR_Qrf
{
 static const float RADIUS = 300;        // the area it covers round its post
 static const float LEASH = 1.5;         // a hunt that leads past RADIUS * LEASH is broken off
 static const float FRESH_MS = 30000;    // contacts seen this recently count
 static const float FRESH_WEIGHT = 0.25; // a target just seen scores this much more than one 30 s old
 static const float CHECK_MS = 4000;
 static const float HOME_RADIUS = 30;    // back this close: standing by again
 static const float MIN_STRENGTH = 0.5;  // weaker than this (or short of ammo) it stays put
 static const int MIN_AMMO = 25;

 static bool CanOrder(ACR_Squad squad, out string refusal)
 {
  refusal = "";
  if (!squad.IsCombatUnit())
  {
   refusal = "only a fighting squad can be a QRF";
   return false;
  }
  return true;
 }

 static ACR_QrfPlan Start(ACR_Squad squad, vector post)
 {
  ACR_QrfPlan plan = new ACR_QrfPlan();
  plan.m_vPost = post;
  PrintFormat("[ACR] QRF %1: standing by at %2, covering %3 m", squad.GetName(), post, RADIUS);
  return plan;
 }

 // Every tactics tick while the squad is a QRF (not while a player leads it).
 static void Tick(ACR_Squad squad, float now)
 {
  ACR_QrfPlan plan = squad.m_Qrf;
  if (!plan) return;
  IEntity anchor = squad.GetAnchor();
  if (!anchor) return;
  float fromPost = vector.DistanceXZ(anchor.GetOrigin(), plan.m_vPost);
  if (plan.m_bOut)
  {
   // The hunt is over (target destroyed or lost, or no longer fit), or it leads too far.
   string back;
   if (!squad.IsHunting()) back = "hunt over";
   else if (fromPost > RADIUS * LEASH) back = string.Format("%1 m out, past its leash", (int)fromPost);
   else if (!Fit(squad)) back = "no longer fit to fight";
   if (back.IsEmpty()) return;
   Return(squad, plan, back);
   return;
  }
  if (plan.m_bReturning && fromPost <= HOME_RADIUS)
  {
   plan.m_bReturning = false;
   squad.SetOrder("QRF: standing by", ACR_EOrderState.DEFEND);
   PrintFormat("[ACR] QRF %1: back at its post", squad.GetName());
  }
  // Standing by (or on the way back), not regrouping or set aside.
  if (!squad.m_Order.StartsWith("QRF") || now < plan.m_fNextCheck) return;
  plan.m_fNextCheck = now + CHECK_MS;
  if (!Fit(squad)) return;
  ACR_Contact contact = Target(squad, plan, now);
  if (!contact) return;
  string what = ACR_Intel.Describe(contact.m_iKind, contact.m_iCount);
  plan.m_bOut = true;
  plan.m_bReturning = false;
  plan.m_iSorties++;
  plan.m_sTarget = what;
  squad.QrfHunt(contact.m_Squad, contact.m_Body, contact.m_Pos, what);
  int distance = (int)vector.DistanceXZ(contact.m_Pos, plan.m_vPost);
  PrintFormat("[ACR] QRF %1: moving out after %2 %3 m from its post (sortie %4)", squad.GetName(), what, distance, plan.m_iSorties);
  Feed(squad, string.Format("%1 (QRF) moving out after %2, %3 m from its post", squad.GetName(), what, distance));
 }

 // An enemy the side knows of inside RADIUS (not aircraft), scored by how near the post it is and
 // how freshly it was seen; among those close to the best, a seeded weighted pick (ACR_Variety).
 static ACR_Contact Target(ACR_Squad squad, ACR_QrfPlan plan, float now)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !squad.GetFaction()) return null;
  ACR_Intel intel = manager.IntelFor(squad.GetFaction().GetFactionKey());
  if (!intel) return null;
  array<ACR_Contact> candidates = {};
  array<float> scores = {};
  foreach (ACR_Contact contact : intel.m_Contacts)
  {
   float age = now - contact.m_fSeen;
   if (contact.m_iKind == ACR_Intel.KIND_AIR || age > FRESH_MS) continue;
   float d = vector.DistanceXZ(contact.m_Pos, plan.m_vPost);
   if (d > RADIUS) continue;
   candidates.Insert(contact);
   scores.Insert(1 - d / RADIUS + FRESH_WEIGHT * (1 - age / FRESH_MS));
  }
  int pick = ACR_Variety.Pick(scores, string.Format("qrf %1 %2", squad.GetName(), plan.m_iSorties));
  if (pick < 0) return null;
  return candidates[pick];
 }

 static bool Fit(ACR_Squad squad)
 {
  return squad.IsCombatUnit() && squad.StrengthRatio() >= MIN_STRENGTH && (squad.m_iAmmoPct < 0 || squad.m_iAmmoPct >= MIN_AMMO);
 }

 static void Return(ACR_Squad squad, ACR_QrfPlan plan, string why)
 {
  plan.m_bOut = false;
  plan.m_bReturning = true;
  squad.QrfStandBy(plan.m_vPost, "QRF: returning");
  PrintFormat("[ACR] QRF %1: returning to its post (%2)", squad.GetName(), why);
 }

 // A new order (not one of its own) ends the QRF.
 static void End(ACR_Squad squad)
 {
  ACR_QrfPlan plan = squad.m_Qrf;
  if (!plan) return;
  squad.m_Qrf = null;
  PrintFormat("[ACR] QRF %1: stood down after %2 sorties", squad.GetName(), plan.m_iSorties);
 }

 protected static void Feed(ACR_Squad squad, string line)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager || !squad.GetFaction()) return;
  ACR_Intel intel = manager.IntelFor(squad.GetFaction().GetFactionKey());
  if (intel) intel.AddFeed(GetGame().GetWorld().GetWorldTime(), line);
 }

 // The side's own QRFs: a pale ring round each area covered ("~": not a danger area).
 static void Zones(string faction, notnull array<vector> positions, notnull array<float> radii, notnull array<string> labels)
 {
  ACR_Manager manager = ACR_Manager.GetInstance();
  if (!manager) return;
  foreach (ACR_Squad squad : manager.m_Squads)
  {
   if (!squad || !squad.m_Qrf || !squad.GetFaction() || squad.GetFaction().GetFactionKey() != faction) continue;
   positions.Insert(squad.m_Qrf.m_vPost);
   radii.Insert(RADIUS);
   labels.Insert("~QRF " + squad.GetName());
  }
 }
}
